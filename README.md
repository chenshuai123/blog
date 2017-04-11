neutron l3 agent 和 opendaylight l3 同时开启的问题
=================================================

一个测试用例， 在neutron l3 agent 和 opendaylight l3 同时打开下，ssh 登陆 创建的虚拟机，结果ssh直接被拒绝。<br />
 <br />
虚拟机配置，租户网络是 88.88.88.0/24， vxlan类型，floating ip 给虚拟机分配的是192.168.111.223，这台虚拟机分>配在 host5上 <br />
 <br />
2d386740-3bb4-4754-9c06-36a6bc0831ea | test51 | ACTIVE | - | Running | net1=88.88.88.4, 192.168.111.223 <br />
 <br />
neutron l3 agent 的机制请参考 http://lingxiankong.github.io/blog/2013/11/19/iptables-in-neutron/ <br />
 <br />
neutron l3 agent 会在控制节点上创建一个名字空间，一般都是qrouter开头 <br />

    root@host1:~# ip netns exec qrouter-5a367e1d-9162-4b90-af5a-ff4655c7571f ip a 
    17: qr-dcedd015-a5: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1450 qdisc noqueue state UNKNOWN group default
    link/ether fa:16:3e:91:5f:a0 brd ff:ff:ff:ff:ff:ff 
    inet 88.88.88.1/24 brd 88.88.88.255 scope global qr-dcedd015-a5 

    18: qg-5d88ef18-33: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UNKNOWN group default
    link/ether fa:16:3e:e3:66:3c brd ff:ff:ff:ff:ff:ff
    inet 192.168.111.223/32 brd 192.168.111.223 scope global qg-5d88ef18-33 

我从网络节点(host1)上ssh 实例（192.168.111.223），同时在网络节点(host1)和计算节点(host5)上开启抓包 <br />
 <br />
网络节点上的抓包，注意第5条报文，这个［R］就是 tcp 中的 rest 报文（复位报文，用来关闭socket，具体含义请自行谷歌百度）<br />

    root@host1:~# tcpdump -ni any tcp port 22 and host not 10.1.0.12 <br />
    19:25:18.052823 IP 192.168.111.210.17350 > 192.168.111.223.22: Flags [S], <br />
    19:25:18.056965 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
    19:25:18.057018 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
    19:25:18.057049 IP 192.168.111.210.17350 > 192.168.111.223.22: Flags [.], ack 1, <br />
    19:25:18.057117 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [R], <br />
    19:25:19.253970 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
    19:25:19.253970 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
    19:25:19.254004 IP 192.168.111.210.17350 > 192.168.111.223.22: Flags [R], <br />


计算节点的抓包，注意［R］报文，但是时间戳不对，网络节点有两个REST，一个是18秒，一个是19秒，而计算节点上只有一个，是19秒，而且计算节点上是收到，不是发送<br />
    root@host5:~# tcpdump -ni any tcp port 22 and host not 10.1.0.12 <br />
    19:25:18.028119 IP 192.168.111.210.17350 > 88.88.88.4.22: Flags [S], <br />
    19:25:18.031503 IP 88.88.88.4.22 > 192.168.111.210.17350: Flags [S.], <br />
    19:25:18.031598 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
    19:25:18.031596 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
    19:25:19.228699 IP 88.88.88.4.22 > 192.168.111.210.17350: Flags [S.], <br />
    19:25:19.228721 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
    19:25:19.228699 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
    19:25:19.228963 IP 192.168.111.210.17350 > 88.88.88.4.22: Flags [R], <br />


这里［S］［S.］是什么？这两个都是tcp三次握手的报文（ssh走的也是tcp协议），S是SYN报文，S. (带一个点的)是SYN＋ACK报文，如下图，就是三次握手的第一个和第二个报文。<br />
![github-01.jpg](/images/01.jpg "github-01.jpg")


而且注意，在计算节点上的抓包，竟然是 192.168.111.223 报文，这个是虚拟实例的floating ip，这个IP是在网络节点上qrouter的名字空间里面，为什么在host5上会有？<br />

再看一下 网络节点上名字空间里面的抓包 <br />

名字空间中抓qr的报文, REST报文是19秒<br />

    root@host1:~# ip netns exec qrouter-5a367e1d-9162-4b90-af5a-ff4655c7571f tcpdump -ni qr-dcedd015-a5 tcp port 22
    19:25:18.052888 IP 192.168.111.210.17350 > 88.88.88.4.22: Flags [S],
    19:25:19.254026 IP 192.168.111.210.17350 > 88.88.88.4.22: Flags [R], 


名字空间抓qg的报，注意第三条报文，也是［R］，而且时间戳也是18秒 <br />
    root@host1:~# ip netns exec qrouter-5a367e1d-9162-4b90-af5a-ff4655c7571f tcpdump -ni qg-5d88ef18-33 tcp port 22
    19:25:18.052860 IP 192.168.111.210.17350 > 192.168.111.223.22: Flags [S], 
    19:25:18.057053 IP 192.168.111.210.17350 > 192.168.111.223.22: Flags [.], 
    19:25:18.057069 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [R],
    19:25:19.254007 IP 192.168.111.210.17350 > 192.168.111.223.22: Flags [R], 


好了，到这里可以看出来，这个RST报文不是虚拟实例发的，也不是host5（计算节点）发的，是host1(网络节点)上的名字空间中 qg 端口发的。 <br />

那为什么 host5 上为什么会有192.168.111.223 报文？因为 odl l3 的关系，neutron l3 agent 其实提供两个功能，一个是metadata的功能，另一个是提供虚拟实例的租户网络IP和floating ip之间的转换，通过nat来实现，而odl l3 也提供了虚拟机的租户网络IP和floating ip之间的转换，是通过ovs流表来实现，<br />

host5上的ovs流表 <br />

    root@host5:~# ovs-ofctl -O OpenFlow13 dump-flows br-int | grep 88.4
    cookie=0x0, duration=543.500s, table=20, n_packets=0, n_bytes=0, priority=1024,arp,tun_id=0x43e,arp_tpa=88.88.88.4,arp_op=1 actions=move:NXM_OF_ETH_SRC[]->NXM_OF_ETH_DST[],set_field:fa:16:3e:4b:eb:3c->eth_src,load:0x2->NXM_OF_ARP_OP[],move:NXM_NX_ARP_SHA[]->NXM_NX_ARP_THA[],move:NXM_OF_ARP_SPA[]->NXM_OF_ARP_TPA[],load:0xfa163e4beb3c->NXM_NX_ARP_SHA[],load:0x58585804->NXM_OF_ARP_SPA[],IN_PORT
    
    cookie=0x0, duration=517.302s, table=30, n_packets=0, n_bytes=0, priority=1024,ip,in_port=5,nw_dst=192.168.111.223 actions=set_field:88.88.88.4->ip_dst,load:0x43e->NXM_NX_REG3[],goto_table:40
    cookie=0x0, duration=542.353s, table=40, n_packets=0, n_bytes=0, priority=36001,ip,in_port=7,dl_src=fa:16:3e:4b:eb:3c,nw_src=88.88.88.4 actions=goto_table:50 (浮动ip与租户ip的转换)
    cookie=0x0, duration=543.363s, table=70, n_packets=70, n_bytes=7622, priority=1024,ip,tun_id=0x43e,nw_dst=88.88.88.4 actions=set_field:fa:16:3e:4b:eb:3c->eth_dst,goto_table:80
    cookie=0x0, duration=517.301s, table=100, n_packets=2, n_bytes=148, priority=512,ip,tun_id=0x43e,dl_dst=fa:16:3e:91:5f:a0,nw_src=88.88.88.4 actions=set_field:fa:16:3e:74:db:c7->eth_src,dec_ttl,set_field:2c:ab:00:9a:04:37->eth_dst,set_field:192.168.111.223->ip_src,output:5 (租户ip与浮动ip的转换)
    root@host5:~#

odl l3 的流表在 host5上对虚拟实例的租户网络IP和floating ip进行了转换，报文流程图就是如下 <br />
![github-02.jpg](/images/02.jpg "github-02.jpg")

而openstack only的环境是只走net namespace，也就是说第二条报文（SYN＋ACK）也是走qrouter的名字空间的，但是现在这种场景有什么问题呢？想不出来，咨询了一堆人，没人知道为什么。<br />

好吧，做驱动，抓包抓调用栈，抓包的关键是找到发报的总出口 <br />
所有的发包都会调用到 __netdev_start_xmit 这个函数，一个回调，看到回调就可以去替换它 <br />

    static inline netdev_tx_t __netdev_start_xmit(const struct net_device_ops *ops, 
                                                  struct sk_buff *skb, struct net_device *dev, 
                                                  bool more) 
    { 
          skb->xmit_more = more ? 1 : 0; 
          return ops->ndo_start_xmit(skb, dev); 
    } 

因为这些接口（qg，qr，br）都是openvswitch创建的，所以用的都是 internal_dev_netdev_ops ，而这个变量是const，>没法修改里面的变量，但是可以它的上一层，就是 net_device ，每一个接口（无论是物理的还是虚拟的）在内核里面都有一个net_device，而这些个net_device 刚开始都会保存在 inet_init 中，而后面有名字空间的话，就会放到对应的名字空间列表中去，名字空间是 net_namespace_list，是一个循环链表，代码如下 <br />

    struct list_head * plist = &net_namespace_list;
    while (plist)
    {
        printk ("list:%x prev:%x next:%x\n", plist, plist->prev, plist->next);
        plist = plist->next;
        if (plist == &net_namespace_list)
        {
            break;
        }

        // net.list 到 net 的偏移量是16字节
        char * p = (char *)plist;
        p = p - 16;
        struct net * pnet = (struct net *)p;

        struct net_device *dev;
        for (i=0; i < 100; i ++)
        {
            dev = dev_get_by_index(pnet, i);
            if (dev)
            {
                printk ("chenshuai %d dev:%x ops:%x devname %s\n", i, dev, dev->netdev_ops, dev->name);
                if ((0 == strncmp(dev->name, "br-", 3)) ||
                    (0 == strncmp(dev->name, "qr-", 3)) ||
                    (0 == strncmp(dev->name, "qg-", 3)))
                {
                    dev->netdev_ops = (const struct net_device_ops *)&chenshuai_internal_dev_netdev_ops;
                    printk ("chenshuai replace ops %x\n", dev->netdev_ops);
                }
            }
        }
    }


我重写了一个 chenshuai_internal_dev_netdev_ops 和原来的 internal_dev_netdev_ops 一摸一样，除了 ndo_start_xmit 我把它替换成我的函数，用于抓包，当然抓完报还会继续调用原来的函数  <br />

    typedef int (*fn_internal_dev_xmit)(struct sk_buff *skb, struct net_device *netdev);
    fn_internal_dev_xmit gfn_internal_dev_xmit;
    gfn_internal_dev_xmit = kallsyms_lookup_name("internal_dev_xmit”);
    
    int chenshuai_internal_dev_xmit(struct sk_buff *skb, struct net_device *netdev)
    {
          // 抓包，抓调用栈
          return (*gfn_internal_dev_xmit)(skb, netdev);
    }

在qg上抓的调用栈捕获发现 <br />
第一个 SYN 报文是如下调用栈 <br />

    [59039.551147]  [<ffffffffc069813e>] chenshuai_internal_dev_xmit+0x9e/0xb0 [hello]
    [59039.551150]  [<ffffffff816abfe9>] dev_hard_start_xmit+0x169/0x3d0
    [59039.551155]  [<ffffffff816aba70>] ? netif_skb_features+0xb0/0x1e0
    [59039.551157]  [<ffffffff816abbbe>] ? validate_xmit_skb.isra.95.part.96+0x1e/0x2e0
    [59039.551159]  [<ffffffff816ac6c0>] __dev_queue_xmit+0x470/0x580
    [59039.551161]  [<ffffffff816ac7e0>] dev_queue_xmit+0x10/0x20
    [59039.551163]  [<ffffffff816b508a>] neigh_resolve_output+0x12a/0x230
    [59039.551169]  [<ffffffff816e7b00>] ? ip_fragment+0x8a0/0x8a0
    [59039.551171]  [<ffffffff816e7cd8>] ip_finish_output+0x1d8/0x870
    [59039.551173]  [<ffffffff816e9738>] ip_output+0x68/0xa0
    [59039.551176]  [<ffffffff816e5149>] ip_forward_finish+0x69/0x80
    [59039.551178]  [<ffffffff816e54c9>] ip_forward+0x369/0x460
    [59039.551181]  [<ffffffff816e3261>] ip_rcv_finish+0x81/0x360
    [59039.551183]  [<ffffffff816e3bd2>] ip_rcv+0x2a2/0x3f0


而第三个ACK报文是如下调用栈 <br />

    [59768.419015]  [<ffffffff81707b46>] tcp_v4_send_reset+0x246/0x400
    [59768.419020]  [<ffffffff816e3540>] ? ip_rcv_finish+0x360/0x360
    [59768.419022]  [<ffffffff81709412>] tcp_v4_rcv+0x602/0x980
    [59768.419025]  [<ffffffff816dca24>] ? nf_hook_slow+0x74/0x130
    [59768.419028]  [<ffffffff816e3540>] ? ip_rcv_finish+0x360/0x360
    [59768.419031]  [<ffffffff816e35ec>] ip_local_deliver_finish+0xac/0x220
    [59768.419033]  [<ffffffff816e38f8>] ip_local_deliver+0x48/0x80
    [59768.419036]  [<ffffffff816e3261>] ip_rcv_finish+0x81/0x360
    [59768.419038]  [<ffffffff816e3bd2>] ip_rcv+0x2a2/0x3f0

看到了嘛，一个是转发ip_forward，一个是提交到上层ip_local_deliver，也就是提交到本地，本地当然没有这个socket（这个socket是在虚拟实例上的），所以被tcp关掉了。 <br />

那问题是为什么一个走的是转发，一个走的是本地提交，我花了很多时间在 ip_rcv_finish 内部，包括它的寻路机制，ip_forward 内核代码只有一处，结果根本不对，直到我一愤怒，直接在我的ko文件中重编了ip_rcv 和 ip_rcv_finish ，再进行打点才发现，ip_rcv_finish 中 目的端的IP地址已经进行完毕了IP地址的转发， <br />

    ip_rcv 是保存在 全局变量 ip_packet_type 中的，直接替换抓包。
    p_ip_packet_type = (struct packet_type *)kallsyms_lookup_name("ip_packet_type");
    pfn_ip_rcv = kallsyms_lookup_name("ip_rcv");
    p_ip_packet_type->func = chenshuai_mock_ip_rcv;

第一个SYN报文，ip_rcv_finish 中打点 目的IP已经变成了 88.88.88.4， 而第二个ACK报文却还是 192.168.111.223 ，好了，那问题就是为什么 ip_rcv 中没有对第二个ACK报文进行转换目的IP，  <br />

转换IP的代码如下  <br />

        return NF_HOOK(NFPROTO_IPV4, NF_INET_PRE_ROUTING, skb, dev, NULL,
                       ip_rcv_finish);


就是这个 NF_HOOK 。 <br />

里面内容较多，我就不详细介绍了， <br />

修改IP的函数是 iptable_nat_ipv4_in ， 是保存在 全局变量 nf_nat_ipv4_ops 中，直接替换， <br />
而判断是否需要替换IP的是函数 ipv4_conntrack_in ，是保存在 全局变量 ipv4_conntrack_ops 中，直接替换，其实看到这里基本上就能猜到大致情况了，conntrack 是connection track，就是链接跟踪，SYN＋ACK没有走网络名字空间，出于好奇心继续看看内部是怎么判断的， <br />
最后发现在 l4proto->packet(ct, skb, dataoff, ctinfo, pf, hooknum, timeouts); 处报错了  <br />
一看到 l4 我就乐了，l4 不就是 tcp 那一层嘛  <br />

这个函数 tcp_packet ，内部有对每条经过iptables的tcp报文进行判断，并且跟踪状态，而ACK报文在这个函数判断下为非法报文  <br />

        case TCP_CONNTRACK_MAX:
                /* Special case for SYN proxy: when the SYN to the server or
                 * the SYN/ACK from the server is lost, the client may transmit
                 * a keep-alive packet while in SYN_SENT state. This needs to
                 * be associated with the original conntrack entry in order to
                 * generate a new SYN with the correct sequence number.
                 */
                if (nfct_synproxy(ct) && old_state == TCP_CONNTRACK_SYN_SENT &&
                    index == TCP_ACK_SET && dir == IP_CT_DIR_ORIGINAL &&
                    ct->proto.tcp.last_dir == IP_CT_DIR_ORIGINAL &&
                    ct->proto.tcp.seen[dir].td_end - 1 == ntohl(th->seq)) {
                        pr_debug("nf_ct_tcp: SYN proxy client keep alive\n");
                        spin_unlock_bh(&ct->lock);
                        return NF_ACCEPT;
                }
                /* Invalid packet */
                pr_debug("nf_ct_tcp: Invalid dir=%i index=%u ostate=%u\n",
                         dir, get_conntrack_index(th), old_state);
                spin_unlock_bh(&ct->lock);
                if (LOG_INVALID(net, IPPROTO_TCP))
                        nf_log_packet(net, pf, 0, skb, NULL, NULL, NULL,
                                  "nf_ct_tcp: invalid state ");
                return -NF_ACCEPT;


问题基本上就是 第一条SYN报文走的是网络名字空间，所以会在iptables内创建一条记录用于跟踪状态，而SYN＋ACK报文没有走这个网络名字空间，导致这条记录没有做更新，而ACK报文又进入网络名字空间，由于之前记录的状态是SYN报文已发，它等待的状态是SYN＋ACK的报文，不是ACK，所以iptables就认为这条报文是非法，也就不做改目的IP地址的处理，这样就被直接提交到本地tcp层处理，tcp发现本地没有这个socket，就直接关闭。 <br />

关于conntrack 上资料不多，可以参考一下这个， <br />
http://linux.chinaunix.net/techdoc/net/2009/06/15/1118315.shtml

