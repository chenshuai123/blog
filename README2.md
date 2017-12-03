kvm qemu vhost-user 
===================

一. QEMU 和 KVM 之间

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

至此，kvm->memslots 初始化完了，每个虚拟机都有唯一一个kvm

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





二. QEMU 和 vhost-user 之间

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


我们这里只看 用户态的 , 即 user_ops , 为什么我这里只列出 .vhost_set_mem_table 和 .vhost_set_vring_addr，一般而言，涉及到内存和地址，永远都是关键，也是最头疼的。

vhost-user 的基础是 vhost-user进程和QEMU进程之间是通过共享内存的。

    static int
    vhost_user_set_mem_table(struct virtio_net *dev, struct VhostUserMsg *pmsg)
    {
        ...
        dev->nr_guest_pages = 0;
        if (!dev->guest_pages) {
                dev->max_guest_pages = 8;
                dev->guest_pages = malloc(dev->max_guest_pages *
                                                sizeof(struct guest_page));
        }
 
        dev->mem = rte_zmalloc("vhost-mem-table", sizeof(struct virtio_memory) +
                sizeof(struct virtio_memory_region) * memory.nregions, 0);
        if (dev->mem == NULL) {
                RTE_LOG(ERR, VHOST_CONFIG,
                        "(%d) failed to allocate memory for dev->mem\n",
                        dev->vid);
                return -1;
        }
        dev->mem->nregions = memory.nregions;
        ...
        for (i = 0; i < memory.nregions; i++) {
            fd  = pmsg->fds[i];
            reg = &dev->mem->regions[i];
        
            reg->guest_phys_addr = memory.regions[i].guest_phys_addr;
            reg->guest_user_addr = memory.regions[i].userspace_addr;
            reg->size            = memory.regions[i].memory_size;
            reg->fd              = fd;
        
            mmap_offset = memory.regions[i].mmap_offset;
            mmap_size   = reg->size + mmap_offset;
            ...
            mmap_size = RTE_ALIGN_CEIL(mmap_size, alignment);
        
            mmap_addr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_POPULATE, fd, 0);
            reg->mmap_addr = mmap_addr;
            reg->mmap_size = mmap_size;
            reg->host_user_addr = (uint64_t)(uintptr_t)mmap_addr +
                                  mmap_offset;
        
            if (dev->dequeue_zero_copy)
                    add_guest_pages(dev, reg, alignment);
        }
        ...
    }

这里申请了两块内存 dev->guest_pages 和 dev->mem ，

然后赋值

    reg = &dev->mem->regions[i];
    reg->guest_phys_addr = memory.regions[i].guest_phys_addr;
    reg->guest_user_addr = memory.regions[i].userspace_addr;
    mmap_addr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
    reg->host_user_addr = (uint64_t)(uintptr_t)mmap_addr +  mmap_offset;

guest_phys_addr 是虚拟机的物理地址

guest_user_addr（.userspace_addr）是 虚拟机对应的QEMU进程的虚拟地址

mmap_addr 是 vhost-user 进程共享了 QEMU 进程的内存，所以这个就是vhost-user进程的虚拟内存，

host_user_addr 是 mmap_addr 加了偏移，因为mmap是通过传过来的句柄fd进行的，所以还存在一定的偏移。

所以这里就建立了一张 虚拟机的物理地址， 虚拟机对应的QEMU进程的虚拟地址，vhost-user进程的虚拟内存 这三个地址的一一对应的表，其实这三个地址都指向一个地址，只是从不同的角度来看。


这里还有一个

    if (dev->dequeue_zero_copy)
         add_guest_pages(dev, reg, alignment);

顾名思义，零拷贝，看一下怎么处理的

    add_guest_pages(struct virtio_net *dev, struct virtio_memory_region *reg,
                    uint64_t page_size)
    {
            uint64_t reg_size = reg->size;
            uint64_t host_user_addr  = reg->host_user_addr;
            uint64_t guest_phys_addr = reg->guest_phys_addr;
            uint64_t host_phys_addr;
            uint64_t size;
    
            host_phys_addr = rte_mem_virt2phy((void *)(uintptr_t)host_user_addr);
            size = page_size - (guest_phys_addr & (page_size - 1));
            size = RTE_MIN(size, reg_size);
    
            add_one_guest_page(dev, guest_phys_addr, host_phys_addr, size);
            host_user_addr  += size;
            guest_phys_addr += size;
            reg_size -= size;
            ...
    }


    host_phys_addr = rte_mem_virt2phy((void *)(uintptr_t)host_user_addr);

rte_mem_virt2phy 看函数名字就是通过虚拟内存地址获取物理内存地址，host_user_addr 是vhost_user进程的虚拟地址，host_phys_addr 就是对应的物理内存地址。rte_mem_virt2phy 里面用的是 /proc/self/pagemap 。

再看一下 add_one_guest_page(dev, guest_phys_addr, host_phys_addr, size); 看参数应该就明白了一半了吧
 
    static void
    add_one_guest_page(struct virtio_net *dev, uint64_t guest_phys_addr,
                       uint64_t host_phys_addr, uint64_t size)
    {
        ...
        page = &dev->guest_pages[dev->nr_guest_pages++];
        page->guest_phys_addr = guest_phys_addr;
        page->host_phys_addr  = host_phys_addr;
        page->size = size;
        ...
    }

这里用到了之前申请的内存 guest_pages

总结：

vhost_user_set_mem_table 函数干了两件事：

1. 建立起第一张表，包含了 虚拟机的物理地址， 虚拟机对应的QEMU进程的虚拟地址，vhost-user进程的虚拟内存 这3个地址的一一对应的表。

2. 建立起第二张表，包含了 虚拟机的物理地址（guest_phys_addr）和 vhost-user进程的物理地址（其实也是host的物理地址）的一一对应的表。




再看一下 vhost_user_set_vring_addr

    static int
    vhost_user_set_vring_addr(struct virtio_net *dev, struct vhost_vring_addr *addr)
    {
        struct vhost_virtqueue *vq;
        ...
        /* addr->index refers to the queue index. The txq 1, rxq is 0. */
        vq = dev->virtqueue[addr->index];
    
        /* The addresses are converted from QEMU virtual to Vhost virtual. */
        vq->desc = (struct vring_desc *)(uintptr_t)qva_to_vva(dev,
                        addr->desc_user_addr);
        ...
        vq->avail = (struct vring_avail *)(uintptr_t)qva_to_vva(dev,
                            addr->avail_user_addr);
        ...
        vq->used = (struct vring_used *)(uintptr_t)qva_to_vva(dev,
                            addr->used_user_addr);
        ...
    }

再看一下 qva_to_vva 是干嘛的？

    static uint64_t
    qva_to_vva(struct virtio_net *dev, uint64_t qva)
    {
            struct virtio_memory_region *reg;
            uint32_t i;
    
            /* Find the region where the address lives. */
            for (i = 0; i < dev->mem->nregions; i++) {
                    reg = &dev->mem->regions[i];
    
                    if (qva >= reg->guest_user_addr &&
                        qva <  reg->guest_user_addr + reg->size) {
                            return qva - reg->guest_user_addr +
                                   reg->host_user_addr;
                    }
            }
    
            return 0;
    }

qva_to_vva用到了之前的 vhost_user_set_mem_table 中初始化的 dev->mem ，所以就是通过 QEMU 进程的虚拟地址 算出 vhost_user进程的虚拟地址，那 vq->desc, va->used, vq->avail 就是vhost_user进程的虚拟地址，那为什么是它呢？因为vhost_virtqueue 或者说 virtqueue 是virtio的关键，也是前后端virtio驱动通信的关键。



好了，初始化完成了，看一下它们是怎么工作的。


    /*
     * The receive path for the vhost port is the TX path out from guest.
     */
    static int
    netdev_dpdk_vhost_rxq_recv(struct netdev_rxq *rxq_,
                               struct dp_packet **packets, int *c)
    {
        ...
        nb_rx = rte_vhost_dequeue_burst(virtio_dev, qid * VIRTIO_QNUM + VIRTIO_TXQ,
                                    vhost_dev->dpdk_mp->mp,
                                    (struct rte_mbuf **)packets,
                                    NETDEV_MAX_BURST);
        ...
    }


    uint16_t
    rte_vhost_dequeue_burst(int vid, uint16_t queue_id,
            struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count)
    {
        ...
        for (i = 0; i < count; i++) {
                struct vring_desc *desc;
                uint16_t sz, idx;
                int err;

                if (likely(i + 1 < count))
                        rte_prefetch0(&vq->desc[desc_indexes[i + 1]]);

                if (vq->desc[desc_indexes[i]].flags & VRING_DESC_F_INDIRECT) {
                        desc = (struct vring_desc *)(uintptr_t)gpa_to_vva(dev,
                                        vq->desc[desc_indexes[i]].addr);
                        if (unlikely(!desc))
                                break;

                        rte_prefetch0(desc);
                        sz = vq->desc[desc_indexes[i]].len / sizeof(*desc);
                        idx = 0;
                } else {
                        desc = vq->desc; // 这里用到了 vhost_user_set_vring_addr 中的
                        sz = vq->size;
                        idx = desc_indexes[i];
                }

                ...
                err = copy_desc_to_mbuf(dev, desc, sz, pkts[i], idx, mbuf_pool);
                ...
        }
    }



    static inline int __attribute__((always_inline))
    copy_desc_to_mbuf(struct virtio_net *dev, struct vring_desc *descs,
                      uint16_t max_desc, struct rte_mbuf *m, uint16_t desc_idx,
                      struct rte_mempool *mbuf_pool)
    {
        ...
        desc_addr = gpa_to_vva(dev, desc->addr); // 注意点1
        ...
        while (1) {
                uint64_t hpa;

                cpy_len = RTE_MIN(desc_avail, mbuf_avail);

                /*
                 * A desc buf might across two host physical pages that are
                 * not continuous. In such case (gpa_to_hpa returns 0), data
                 * will be copied even though zero copy is enabled.
                 */
                // 注意点2，零拷贝
                if (unlikely(dev->dequeue_zero_copy && (hpa = gpa_to_hpa(dev,
                                        desc->addr + desc_offset, cpy_len)))) {
                        cur->data_len = cpy_len;
                        cur->data_off = 0;
                        cur->buf_addr = (void *)(uintptr_t)desc_addr;
                        cur->buf_physaddr = hpa;

                        /*
                         * In zero copy mode, one mbuf can only reference data
                         * for one or partial of one desc buff.
                         */
                        mbuf_avail = cpy_len;
                } else { 
                        // 注意点3
                        rte_memcpy(rte_pktmbuf_mtod_offset(cur, void *,
                                                           mbuf_offset),
                                (void *)((uintptr_t)(desc_addr + desc_offset)),
                                cpy_len);
                }
        }
    }


我在上面这个函数标记了3个注意点，我们一个一个看

注意点1:

gpa_to_vva 顾名思义，虚拟机物理内存地址 to vhost_user 虚拟内存地址

    /* Convert guest physical Address to host virtual address */
    static inline uint64_t __attribute__((always_inline))
    gpa_to_vva(struct virtio_net *dev, uint64_t gpa)
    {
            struct virtio_memory_region *reg;
            uint32_t i;
    
            for (i = 0; i < dev->mem->nregions; i++) {
                    reg = &dev->mem->regions[i];
                    if (gpa >= reg->guest_phys_addr &&
                        gpa <  reg->guest_phys_addr + reg->size) {
                            return gpa - reg->guest_phys_addr +
                                   reg->host_user_addr;
                    }
            }
    
            return 0;
    }


注意点2

    hpa = gpa_to_hpa(dev, desc->addr + desc_offset, cpy_len)


    /* Convert guest physical address to host physical address */
    static inline phys_addr_t __attribute__((always_inline))
    gpa_to_hpa(struct virtio_net *dev, uint64_t gpa, uint64_t size)
    {
            uint32_t i;
            struct guest_page *page;
    
            for (i = 0; i < dev->nr_guest_pages; i++) {
                    page = &dev->guest_pages[i];
    
                    if (gpa >= page->guest_phys_addr &&
                        gpa + size < page->guest_phys_addr + page->size) {
                            return gpa - page->guest_phys_addr +
                                   page->host_phys_addr;
                    }
            }
    
            return 0;
    }


这里是 dev->guest_pages ，vhost_user_set_mem_table 中建立的第二张表，通过 gpa 获取 hpa。

再看一下 struct vring_desc *desc;

    /* VirtIO ring descriptors: 16 bytes.
     * These can chain together via "next". */
    struct vring_desc {
            uint64_t addr;  /*  Address (guest-physical). */
            uint32_t len;   /* Length. */
            uint16_t flags; /* The flags as indicated above. */
            uint16_t next;  /* We chain unused descriptors via this. */
    };



注意点3

                        rte_memcpy(rte_pktmbuf_mtod_offset(cur, void *,
                                                           mbuf_offset),
                                (void *)((uintptr_t)(desc_addr + desc_offset)),
                                cpy_len);

cur是目的内存，这里走的是 非零拷贝，所以就直接拷贝了。




