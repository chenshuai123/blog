kvm qemu vhost-user 
===================

一. QEMU 和 KVM 之间
QEMU code
vhost-backend.c   kernel_ops   调用 ioctl
vhost-user.c      user_ops     调用 vring接口

可以看出来一个是内核态用的，一个是给用户态用的（vhost-user，ovs+dpdk）

    static const VhostOps kernel_ops = {
        .backend_type = VHOST_BACKEND_TYPE_KERNEL,
        ....
        .vhost_set_mem_table = vhost_kernel_set_mem_table,
        .vhost_set_vring_addr = vhost_kernel_set_vring_addr,
        ....
    }
    
    const VhostOps user_ops = {
        .backend_type = VHOST_BACKEND_TYPE_USER,
        ...
        .vhost_set_mem_table = vhost_user_set_mem_table,
        .vhost_set_vring_addr = vhost_user_set_vring_addr,
        ...
    }


Linux code  kvm_types.h 中 有关于对各个地址的描述

    /*
     * Address types:
     *
     *  gva - guest virtual address
     *  gpa - guest physical address
     *  gfn - guest frame number
     *  hva - host virtual address
     *  hpa - host physical address
     *  hfn - host frame number
     */

先看一下QEMU初始化中涉及内存那块

QEMU（用户态）                对应                            KVM（内核态）

1. kvm_init -> ioctl(KVM_CREATE_VM)           
   对应           
   kvm_create_vm (申请内存)  
   kvm->memslots[] = kvm_alloc_memslots()

2. kvm_region_add -> kvm_set_phys_mem
   -> kvm_set_user_memory_region
   -> kvm_vm_ioctl(KVM_SET_USER_MEMORY_REGION)   
   对应        
   __kvm_set_memory_region 设置memslot的base_gfn, npages, userspace_addr


附上代码

    /* for KVM_SET_USER_MEMORY_REGION */
    struct kvm_userspace_memory_region {
            __u32 slot;
            __u32 flags;
            __u64 guest_phys_addr;
            __u64 memory_size; /* bytes */
            __u64 userspace_addr; /* start of the userspace allocated memory */
    };


    int __kvm_set_memory_region(struct kvm *kvm,
                    const struct kvm_userspace_memory_region *mem)
    {   
        ...
        slot = id_to_memslot(__kvm_memslots(kvm, as_id), id);
        base_gfn = mem->guest_phys_addr >> PAGE_SHIFT;
        npages = mem->memory_size >> PAGE_SHIFT;
    
        if (npages > KVM_MEM_MAX_NR_PAGES)
            goto out;
    
        new = old = *slot;
    
        new.id = id;
        new.base_gfn = base_gfn;
        new.npages = npages;
        new.flags = mem->flags;
        ...
        if (change == KVM_MR_CREATE) {
            new.userspace_addr = mem->userspace_addr;
    
            if (kvm_arch_create_memslot(kvm, &new, npages))
                goto out_free;
        }
        ...
    }

其中 userspace_addr 是 【虚拟机对应的QEMU进程中的虚拟地址】。

update_memslots 把新的 kvm_memory_slot 插入到 kvm_memslots 中去，而install_new_memslots 重新装一下kvm_memslots

    update_memslots(slots, &new);
    old_memslots = install_new_memslots(kvm, as_id, slots);    

做了 gpa(gfn) 和QEMU 进程的虚拟内存的映射

新的slot需要进行map

    /*
     * IOMMU mapping:  New slots need to be mapped.  Old slots need to be
     * un-mapped and re-mapped if their base changes.  Since base change
     * unmapping is handled above with slot deletion, mapping alone is
     * needed here.  Anything else the iommu might care about for existing
     * slots (size changes, userspace addr changes and read-only flag
     * changes) is disallowed above, so any other attribute changes getting
     * here can be skipped.
     */
    if ((change == KVM_MR_CREATE) || (change == KVM_MR_MOVE)) {
        r = kvm_iommu_map_pages(kvm, &new);
        return r;
    }

        while (gfn < end_gfn) {
                unsigned long page_size;

                /* Check if already mapped */
                if (iommu_iova_to_phys(domain, gfn_to_gpa(gfn))) {
                        gfn += 1;
                        continue;
                }

                /* Get the page size we could use to map */
                page_size = kvm_host_page_size(kvm, gfn);

                /* Make sure the page_size does not exceed the memslot */
                while ((gfn + (page_size >> PAGE_SHIFT)) > end_gfn)
                        page_size >>= 1;

                /* Make sure gfn is aligned to the page size we want to map */
                while ((gfn << PAGE_SHIFT) & (page_size - 1))
                        page_size >>= 1;

                /* Make sure hva is aligned to the page size we want to map */
                while (__gfn_to_hva_memslot(slot, gfn) & (page_size - 1))
                        page_size >>= 1;

                /*
                 * Pin all pages we are about to map in memory. This is
                 * important because we unmap and unpin in 4kb steps later.
                 */
                pfn = kvm_pin_pages(slot, gfn, page_size >> PAGE_SHIFT);
                if (is_error_noslot_pfn(pfn)) {
                        gfn += 1;
                        continue;
                }

                /* Map into IO address space */
                r = iommu_map(domain, gfn_to_gpa(gfn), pfn_to_hpa(pfn),
                              page_size, flags);
                if (r) {
                        printk(KERN_ERR "kvm_iommu_map_address:"
                               "iommu failed to map pfn=%llx\n", pfn);
                        kvm_unpin_pages(kvm, pfn, page_size >> PAGE_SHIFT);
                        goto unmap_pages;
                }

                gfn += page_size >> PAGE_SHIFT;

                cond_resched();
        }

把虚拟机的物理地址和主机的物理地址做了地址映射(mmap),

附上 kvm_memory_slot 注释

    struct kvm_memory_slot {
        gfn_t base_gfn; // 该slot对应虚拟机页框的起点
        unsigned long npages; // 该slot中有多少个页
        unsigned long *dirty_bitmap; // 脏页的bitmap
        struct kvm_arch_memory_slot arch; // 体系结构相关的结构
        unsigned long userspace_addr; // 对应HVA的地址
        u32 flags; // slot的flag
        short id; // slot识别id
    };


3. KVM MMU 创建和初始化

创建调用栈:

    vmx_create_vcpu => kvm_vcpu_init => kvm_arch_vcpu_init => kvm_mmu_create

    int kvm_mmu_create(struct kvm_vcpu *vcpu)
    {
        ASSERT(vcpu);
        
        vcpu->arch.walk_mmu = &vcpu->arch.mmu;
        vcpu->arch.mmu.root_hpa = INVALID_PAGE;
        vcpu->arch.mmu.translate_gpa = translate_gpa;
        vcpu->arch.nested_mmu.translate_gpa = translate_nested_gpa;
        
        return alloc_mmu_pages(vcpu);
    }

注意 root_hpa 等于 INVALID_PAGE


初始化调用栈：

    kvm_vm_ioctl_create_vcpu => kvm_arch_vcpu_setup => kvm_mmu_setup => init_kvm_mmu => init_kvm_tdp_mmu

init_kvm_tdp_mmu中唯一做的事情就是初始化了arch.mmu

4. EPT页表建立流程

在虚拟机的入口函数 vcpu_enter_guest 中调用kvm_mmu_reload来完成EPT根页表的初始化。

    static int vcpu_enter_guest(struct kvm_vcpu *vcpu)
    {
    ...
        r = kvm_mmu_reload(vcpu);
    ...
    }


    static inline int kvm_mmu_reload(struct kvm_vcpu *vcpu)
    {
    	if (likely(vcpu->arch.mmu.root_hpa != INVALID_PAGE))
    		return 0;
    
    	return kvm_mmu_load(vcpu);
    }


    int kvm_mmu_load(struct kvm_vcpu *vcpu)
    {
    	int r;
    
    	r = mmu_topup_memory_caches(vcpu);
    	if (r)
    		goto out;
    	r = mmu_alloc_roots(vcpu);
    	kvm_mmu_sync_roots(vcpu);
    	if (r)
    		goto out;
    	/* set_cr3() should ensure TLB has been flushed */
    	vcpu->arch.mmu.set_cr3(vcpu, vcpu->arch.mmu.root_hpa);
    out:
    	return r;
    }

Intel EPT相关的VMEXIT有两个：
EPT Misconfiguration：EPT pte配置错误，具体情况参考Intel Manual 3C, 28.2.3.1 EPT Misconfigurations
EPT Violation：当guest VM访存出发到EPT相关的部分，在不产生EPT Misconfiguration的前提下，可能会产生EPT Violation，具体情况参考Intel Manual 3C, 28.2.3.2 EPT Violations
当Guest第一次访问某页面时，首先触发的是Guest OS的page fault，Guest OS会修复好自己mmu的页结构，并且访问对应的GPA，此时由于对应的EPT结构还没有建立，会触发EPT Violation。对于Intel EPT，EPT缺页的处理在函数tdp_page_fault中。

![github-05.jpg](/images/05.jpg "github-05.jpg")
