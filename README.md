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
 <br />
root@host1:~# ip netns exec qrouter-5a367e1d-9162-4b90-af5a-ff4655c7571f ip a <br />
17: qr-dcedd015-a5: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1450 qdisc noqueue state UNKNOWN group default
link/ether fa:16:3e:91:5f:a0 brd ff:ff:ff:ff:ff:ff <br />
inet 88.88.88.1/24 brd 88.88.88.255 scope global qr-dcedd015-a5 <br />
 <br />
18: qg-5d88ef18-33: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UNKNOWN group default
link/ether fa:16:3e:e3:66:3c brd ff:ff:ff:ff:ff:ff <br />
inet 192.168.111.223/32 brd 192.168.111.223 scope global qg-5d88ef18-33 <br />

我从网络节点(host1)上ssh 实例（192.168.111.223），同时在网络节点和计算节点(host5)上开启抓包 <br />
 <br />
网络节点上的抓包，注意这条高亮的报文，这个［R］就是 tcp 中的 rest 报文（复位报文，用来关闭socket，具体含义请自行谷歌百度）<br />
root@host1:~# tcpdump -ni any tcp port 22 and host not 10.1.0.12 <br />
19:25:18.052823 IP 192.168.111.210.17350 > 192.168.111.223.22: Flags [S], <br />
19:25:18.056965 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
19:25:18.057018 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
19:25:18.057049 IP 192.168.111.210.17350 > 192.168.111.223.22: Flags [.], ack 1, <br />
`19:25:18.057117 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [R],` <br />
19:25:19.253970 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
19:25:19.253970 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
19:25:19.254004 IP 192.168.111.210.17350 > 192.168.111.223.22: Flags [R], <br />


计算节点的抓包，注意没有［R］报文，但是时间戳不对，一个是18秒，一个是19秒，而且网络节点上是收到，不是发送<br />
root@host5:~# tcpdump -ni any tcp port 22 and host not 10.1.0.12 <br />
 9:25:18.028119 IP 192.168.111.210.17350 > 88.88.88.4.22: Flags [S], <br />
19:25:18.031503 IP 88.88.88.4.22 > 192.168.111.210.17350: Flags [S.], <br />
19:25:18.031598 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
19:25:18.031596 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
19:25:19.228699 IP 88.88.88.4.22 > 192.168.111.210.17350: Flags [S.], <br />
19:25:19.228721 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
19:25:19.228699 IP 192.168.111.223.22 > 192.168.111.210.17350: Flags [S.], <br />
19:25:19.228963 IP 192.168.111.210.17350 > 88.88.88.4.22: Flags [R], <br />


这里［S］［S.］是什么？这两个都是tcp三次握手的报文（ssh走的也是tcp协议），S是SYN报文，S. (带一个点的)是SYN＋ACK报文，如下图，就是三次握手的第一个和第二个报文。<br />

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
