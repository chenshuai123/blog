阿里云docker云服务的网络模拟
============================

我尝试使用阿里云的docker服务部署两套不同业务的容器，而阿里云的docker是部署在ECS上，ECS所在网络是VPC，为了考虑可靠性，两套容器需要部署在同VPC下的不同ECS上。

经过一番摸索，把两套docker在两个ECS上跑起来了，并且打通网络，
查看了一下容器内的网络

容器1

    root@ccaa7f11d461-suki-ubuntu-suki-ubuntu-1:/# ifconfig
    eth0      Link encap:Ethernet  HWaddr 02:42:ac:12:03:04
              inet addr:172.18.3.4  Bcast:0.0.0.0  Mask:255.255.255.0
              inet6 addr: fe80::42:acff:fe12:304/64 Scope:Link
              UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
              RX packets:181175 errors:0 dropped:0 overruns:0 frame:0
              TX packets:181166 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:1000
              RX bytes:12812246 (12.8 MB)  TX bytes:9560136 (9.5 MB)
    
    eth1      Link encap:Ethernet  HWaddr 02:42:ac:11:00:02
              inet addr:172.17.0.2  Bcast:0.0.0.0  Mask:255.255.0.0
              inet6 addr: fe80::42:acff:fe11:2/64 Scope:Link
              UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
              RX packets:55 errors:0 dropped:0 overruns:0 frame:0
              TX packets:8 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:0
              RX bytes:3398 (3.3 KB)  TX bytes:648 (648.0 B)
    
    lo        Link encap:Local Loopback
              inet addr:127.0.0.1  Mask:255.0.0.0
              inet6 addr: ::1/128 Scope:Host
              UP LOOPBACK RUNNING  MTU:65536  Metric:1
              RX packets:0 errors:0 dropped:0 overruns:0 frame:0
              TX packets:0 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:1
              RX bytes:0 (0.0 B)  TX bytes:0 (0.0 B)
    
    root@ccaa7f11d461-suki-ubuntu-suki-ubuntu-1:/# ip route
    default via 172.17.0.1 dev eth1
    172.17.0.0/16 dev eth1  proto kernel  scope link  src 172.17.0.2
    172.18.0.0/16 via 172.18.3.1 dev eth0
    172.18.3.0/24 dev eth0  proto kernel  scope link  src 172.18.3.4
    root@ccaa7f11d461-suki-ubuntu-suki-ubuntu-1:/#


容器2

    root@91e840d740b6-suki-ubuntu2-suki-ubuntu2-1:/# ifconfig
    eth0      Link encap:Ethernet  HWaddr 02:42:ac:12:01:03
              inet addr:172.18.1.3  Bcast:0.0.0.0  Mask:255.255.255.0
              inet6 addr: fe80::42:acff:fe12:103/64 Scope:Link
              UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
              RX packets:8555 errors:0 dropped:0 overruns:0 frame:0
              TX packets:8563 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:1000
              RX bytes:812126 (812.1 KB)  TX bytes:812774 (812.7 KB)
    
    eth1      Link encap:Ethernet  HWaddr 02:42:ac:11:00:03
              inet addr:172.17.0.3  Bcast:0.0.0.0  Mask:255.255.0.0
              inet6 addr: fe80::42:acff:fe11:3/64 Scope:Link
              UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
              RX packets:153 errors:0 dropped:0 overruns:0 frame:0
              TX packets:141 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:0
              RX bytes:358185 (358.1 KB)  TX bytes:9063 (9.0 KB)
    
    lo        Link encap:Local Loopback
              inet addr:127.0.0.1  Mask:255.0.0.0
              inet6 addr: ::1/128 Scope:Host
              UP LOOPBACK RUNNING  MTU:65536  Metric:1
              RX packets:8 errors:0 dropped:0 overruns:0 frame:0
              TX packets:8 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:1
              RX bytes:1464 (1.4 KB)  TX bytes:1464 (1.4 KB)
    
    root@91e840d740b6-suki-ubuntu2-suki-ubuntu2-1:/# ip route
    default via 172.17.0.1 dev eth1
    172.17.0.0/16 dev eth1  proto kernel  scope link  src 172.17.0.3
    172.18.0.0/16 via 172.18.1.1 dev eth0
    172.18.1.0/24 dev eth0  proto kernel  scope link  src 172.18.1.3
    root@91e840d740b6-suki-ubuntu2-suki-ubuntu2-1:/#



很有趣，两个容器网络能ping通的IP都在eth0上，路由也看的出来，172.18.0.0/16 的网络都走eth0，可是这里勾起我的兴趣来了，容器1是172.18.3.4/24，而容器2是172.18.1.3/24，不同网段啊，有意思。

再看一下ECS上的配置

ECS1

    eth0      Link encap:Ethernet  HWaddr 00:16:3e:1a:ad:25
              inet addr:172.19.143.161  Bcast:172.19.143.255  Mask:255.255.240.0
              UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
              RX packets:3771834 errors:0 dropped:0 overruns:0 frame:0
              TX packets:55800701 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:1000
              RX bytes:1482258559 (1.4 GB)  TX bytes:48703340198 (48.7 GB)
    
    vpc-600e7 Link encap:Ethernet  HWaddr 8a:19:6b:8e:44:1c
              inet addr:172.18.3.1  Bcast:0.0.0.0  Mask:255.255.255.0
              UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
              RX packets:58716 errors:0 dropped:0 overruns:0 frame:0
              TX packets:58685 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:1000
              RX bytes:2639504 (2.6 MB)  TX bytes:4361778 (4.3 MB)

    root@c17fff8e9667a47969f84740350890d75-node1:/# ip route
    default via 172.19.143.253 dev eth0
    172.17.0.0/16 dev docker_gwbridge  proto kernel  scope link  src 172.17.0.1
    172.18.3.0/24 dev vpc-600e7  proto kernel  scope link  src 172.18.3.1
    172.19.128.0/20 dev eth0  proto kernel  scope link  src 172.19.143.161
    172.31.254.0/24 dev docker0  proto kernel  scope link  src 172.31.254.1
    
    5: vpc-600e7: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP mode DEFAULT group default qlen 1000
        link/ether 8a:19:6b:8e:44:1c brd ff:ff:ff:ff:ff:ff promiscuity 0
        bridge


ECS2

    eth0      Link encap:Ethernet  HWaddr 00:16:3e:1a:b7:dc
              inet addr:172.19.143.173  Bcast:172.19.143.255  Mask:255.255.240.0
              UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
              RX packets:1495792 errors:0 dropped:0 overruns:0 frame:0
              TX packets:1375238 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:1000
              RX bytes:792970150 (792.9 MB)  TX bytes:835897702 (835.8 MB)
    
    vpc-600e7 Link encap:Ethernet  HWaddr 0e:81:be:d8:4e:75
              inet addr:172.18.1.1  Bcast:0.0.0.0  Mask:255.255.255.0
              UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
              RX packets:59049 errors:0 dropped:0 overruns:0 frame:0
              TX packets:59033 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:1000
              RX bytes:3553180 (3.5 MB)  TX bytes:3474590 (3.4 MB)
    
    root@c17fff8e9667a47969f84740350890d75-node3:/# ip route
    default via 172.19.143.253 dev eth0
    172.17.0.0/16 dev docker_gwbridge  proto kernel  scope link  src 172.17.0.1
    172.18.1.0/24 dev vpc-600e7  proto kernel  scope link  src 172.18.1.1
    172.19.128.0/20 dev eth0  proto kernel  scope link  src 172.19.143.173
    172.31.254.0/24 dev docker0  proto kernel  scope link  src 172.31.254.1
    
    4: vpc-600e7: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP mode DEFAULT group default qlen 1000
        link/ether 0e:81:be:d8:4e:75 brd ff:ff:ff:ff:ff:ff promiscuity 0
        bridge




很有意思，首先根据IP地址，vpc-600e7 是对接到容器中的eth0的，实际抓包来看也是的确如此，其次vpc-600e7是一个linux bridge, (这里吐槽一下，阿里的ECS上竟然没有装brctl)，最后，ECS上eth0 是有ip的，租户网络的ip。

阿里云使用VPC的ECS只有一个网卡，那两套容器间的通信走的还是ECS上的eth0，而eth0上有ip，说明vpc-600e7到eth0走的是路由，嗯，缺省路由就是。

那我好奇的是容器报文出了ECS的网卡后，怎么到的另一个ECS？这在阿里云上看不到了，我就自己模拟。


我创建了两个虚拟机，虚拟机外部的网络承载在openvswitch上，

物理机上的信息

        Bridge br-test
            Port "vnet3"
                Interface "vnet3"
            Port "vnet1"
                Interface "vnet1"
            Port br-test
                Interface br-test
                    type: internal
        ovs_version: "2.0.2"
    root@ubuntu:~# ifconfig vnet3
    vnet3     Link encap:Ethernet  HWaddr fe:58:00:98:c0:92
              inet6 addr: fe80::fc58:ff:fe98:c092/64 Scope:Link
              UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
              RX packets:0 errors:0 dropped:0 overruns:0 frame:0
              TX packets:8 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:500
              RX bytes:0 (0.0 B)  TX bytes:648 (648.0 B)
    
    root@ubuntu:~# ifconfig vnet1
    vnet1     Link encap:Ethernet  HWaddr fe:58:00:98:c0:81
              inet6 addr: fe80::fc58:ff:fe98:c081/64 Scope:Link
              UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
              RX packets:0 errors:0 dropped:0 overruns:0 frame:0
              TX packets:8 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:500
              RX bytes:0 (0.0 B)  TX bytes:648 (648.0 B)
    
    root@ubuntu:~#

虚拟机1

    root@test1:~# ifconfig ens8
    ens8      Link encap:Ethernet  HWaddr 52:58:00:98:c0:81
              inet addr:10.10.10.11  Bcast:10.10.10.255  Mask:255.255.255.0
              inet6 addr: fe80::5058:ff:fe98:c081/64 Scope:Link
              UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
              RX packets:16 errors:0 dropped:0 overruns:0 frame:0
              TX packets:7 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:1000
              RX bytes:1296 (1.2 KB)  TX bytes:578 (578.0 B)
    
    root@test1:~# ip route
    default via 10.10.10.1 dev ens8
    10.10.10.0/24 dev ens8  proto kernel  scope link  src 10.10.10.11


虚拟机2

    root@test2:~# ifconfig ens8
    ens8      Link encap:Ethernet  HWaddr 52:58:00:98:c0:92
              inet addr:10.10.10.12  Bcast:10.10.10.255  Mask:255.255.255.0
              inet6 addr: fe80::5058:ff:fe98:c092/64 Scope:Link
              UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
              RX packets:8 errors:0 dropped:0 overruns:0 frame:0
              TX packets:7 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:1000
              RX bytes:648 (648.0 B)  TX bytes:578 (578.0 B)
    
    root@test2:~# ip route
    default via 10.10.10.1 dev ens8
    10.10.10.0/24 dev ens8  proto kernel  scope link  src 10.10.10.12

互通

    root@test1:~# ping 10.10.10.12
    PING 10.10.10.12 (10.10.10.12) 56(84) bytes of data.
    64 bytes from 10.10.10.12: icmp_seq=1 ttl=64 time=0.665 ms
    64 bytes from 10.10.10.12: icmp_seq=2 ttl=64 time=0.183 ms
    
    root@test2:~# ping 10.10.10.11
    PING 10.10.10.11 (10.10.10.11) 56(84) bytes of data.
    64 bytes from 10.10.10.11: icmp_seq=1 ttl=64 time=0.325 ms
    64 bytes from 10.10.10.11: icmp_seq=2 ttl=64 time=0.104 ms


我比较懒，虚拟机地址就改了最后一个数字，虚拟机1的mac地址是81，虚拟机2的mac地址是92，

这里还缺一个 10.10.10.1 ，那我就在物理机上ovs网桥上再加一个虚拟端口就当作虚拟机的网关用，

    root@ubuntu:~# ovs-vsctl add-port br-test testgw -- set Interface testgw type=internal
    root@ubuntu:~# ifconfig testgw 10.10.10.1/24 up
    
        Bridge br-test
            Port "vnet3"
                Interface "vnet3"
            Port "vnet1"
                Interface "vnet1"
            Port br-test
                Interface br-test
                    type: internal
            Port testgw
                Interface testgw
                    type: internal
        ovs_version: "2.0.2"

    root@ubuntu:~# ifconfig testgw
    testgw    Link encap:Ethernet  HWaddr 46:f8:56:ca:c1:4f
              inet addr:10.10.10.1  Bcast:10.10.10.255  Mask:255.255.255.0
              inet6 addr: fe80::44f8:56ff:feca:c14f/64 Scope:Link
              UP BROADCAST RUNNING  MTU:1500  Metric:1
              RX packets:58 errors:0 dropped:0 overruns:0 frame:0
              TX packets:69 errors:0 dropped:0 overruns:0 carrier:0
              collisions:0 txqueuelen:0
              RX bytes:4852 (4.8 KB)  TX bytes:8829 (8.8 KB)



两套虚拟机ping通网关

    root@test1:~# ping 10.10.10.1
    PING 10.10.10.1 (10.10.10.1) 56(84) bytes of data.
    64 bytes from 10.10.10.1: icmp_seq=1 ttl=64 time=0.184 ms
    64 bytes from 10.10.10.1: icmp_seq=2 ttl=64 time=0.061 ms
    
    root@test2:~# ping 10.10.10.1
    PING 10.10.10.1 (10.10.10.1) 56(84) bytes of data.
    64 bytes from 10.10.10.1: icmp_seq=1 ttl=64 time=0.409 ms
    64 bytes from 10.10.10.1: icmp_seq=2 ttl=64 time=0.064 ms


然后各在虚拟机上创建一个linux bridge，一个网络名字空间和一对veth pair，把一个veth放到名字空间，另一个挂在linux bridge上，以及打开ip转发，


    root@test1:~# brctl addbr vpc1
    root@test1:~# ip netns add testns1
    root@test1:~# ip link add veth0 type veth peer name veth1
    root@test1:~# ip link set veth1 netns testns1
    root@test1:~# ip netns exec testns1 ifconfig veth1 172.18.1.3/24 up
    root@test1:~# ifconfig veth0 up
    root@test1:~# ifconfig vpc1 172.18.1.1/24 up
    root@test1:~# ip netns exec testns1 ip route add default dev veth1 via 172.18.1.1
    root@test1:~# brctl addif vpc1 veth0
    root@test1:/# echo 1 > /proc/sys/net/ipv4/ip_forward
    root@test1:~# ping 172.18.1.3
    PING 172.18.1.3 (172.18.1.3) 56(84) bytes of data.
    64 bytes from 172.18.1.3: icmp_seq=1 ttl=64 time=0.111 ms
    64 bytes from 172.18.1.3: icmp_seq=2 ttl=64 time=0.016 ms


    root@test2:~# brctl addbr vpc2
    root@test2:~# ip netns add testns2
    root@test2:~# ip link add veth0 type veth peer name veth1
    root@test2:~# ip link set veth1 netns testns2
    root@test2:~# ip netns exec testns2 ifconfig veth1 172.18.3.4/24 up
    root@test2:~# ifconfig veth0 up
    root@test2:~# ifconfig vpc2 172.18.3.1/24 up
    root@test2:~# ip netns exec testns2 ip route add default dev veth1 via 172.18.3.1
    root@test2:~# brctl addif vpc2 veth0
    root@test2:~# echo 1 > /proc/sys/net/ipv4/ip_forward
    root@test2:~# ping 172.18.3.4
    PING 172.18.3.4 (172.18.3.4) 56(84) bytes of data.
    64 bytes from 172.18.3.4: icmp_seq=1 ttl=64 time=0.022 ms
    64 bytes from 172.18.3.4: icmp_seq=2 ttl=64 time=0.018 ms


![github-03.jpg](/images/03.jpg "github-03.jpg")


开始尝试ping，不通

    root@test1:~# ip netns exec testns1 ping 172.18.3.4
    PING 172.18.3.4 (172.18.3.4) 56(84) bytes of data.
    ^C
    --- 172.18.3.4 ping statistics ---
    48 packets transmitted, 0 received, 100% packet loss, time 47377ms
    
    root@test1:~#

在物理机上的ovs网桥上抓报

    root@ubuntu:~# tcpdump -e -ni vnet1
    tcpdump: WARNING: vnet1: no IPv4 address assigned
    tcpdump: verbose output suppressed, use -v or -vv for full protocol decode
    listening on vnet1, link-type EN10MB (Ethernet), capture size 65535 bytes
    02:25:34.272625 52:58:00:98:c0:81 > 46:f8:56:ca:c1:4f, ethertype IPv4 (0x0800), length 98: 172.18.1.3 > 172.18.3.4: ICMP echo request, id 3304, seq 357, length 64
    02:25:35.280601 52:58:00:98:c0:81 > 46:f8:56:ca:c1:4f, ethertype IPv4 (0x0800), length 98: 172.18.1.3 > 172.18.3.4: ICMP echo request, id 3304, seq 358, length 64
    02:25:36.288584 52:58:00:98:c0:81 > 46:f8:56:ca:c1:4f, ethertype IPv4 (0x0800), length 98: 172.18.1.3 > 172.18.3.4: ICMP echo request, id 3304, seq 359, length 64

vnet1是虚拟机1的ens8网卡对应的端口， 目的mac address是我在ovs网桥上创建的网关的，那现在把它改成虚拟机2的ens8的mac地址，同样地，回来的报文也需要把目的mac address改成虚拟机1的ens8的mac地址

第一种法子(用openflow流表):

    root@ubuntu:~# ovs-ofctl add-flow br-test "table=0,in_port=3,priority=1,ip,nw_dst=172.18.3.0/24 actions=set_field:52:58:00:98:c0:92->eth_dst,output=4"
    root@ubuntu:~# ovs-ofctl add-flow br-test "table=0,in_port=4,priority=1,ip,nw_dst=172.18.1.0/24 actions=set_field:52:58:00:98:c0:81->eth_dst,output=3"

    root@ubuntu:~# ovs-ofctl dump-flows br-test
    NXST_FLOW reply (xid=0x4):
     cookie=0x0, duration=314.721s, table=0, n_packets=135, n_bytes=13230, idle_age=140, priority=1,ip,in_port=3,nw_dst=172.18.3.0/24 actions=load:0x52580098c092->NXM_OF_ETH_DST[],output:4
     cookie=0x0, duration=185.804s, table=0, n_packets=7, n_bytes=686, idle_age=140, priority=1,ip,in_port=4,nw_dst=172.18.1.0/24 actions=load:0x52580098c081->NXM_OF_ETH_DST[],output:3
     cookie=0x0, duration=16253.814s, table=0, n_packets=1134, n_bytes=108519, idle_age=103, priority=0 actions=NORMAL
    root@ubuntu:~#

    root@test1:~# ip netns exec testns1 ping 172.18.3.4
    PING 172.18.3.4 (172.18.3.4) 56(84) bytes of data.
    64 bytes from 172.18.3.4: icmp_seq=1 ttl=62 time=0.288 ms
    64 bytes from 172.18.3.4: icmp_seq=2 ttl=62 time=0.146 ms


两条流表的意思 <br />
第一条 <br />
in_port : 虚拟机1的ens8对应于ovs的端口号 <br />
priority: 比0高就行，否则被NORMAL流表先匹配掉 <br />
ip      : ip 报文<br />
nw_dst  : 虚拟机2上的vpc2的网络号<br />
上面四条是匹配<br />
actions 是开始动作了<br />
set_field -> eth_dst : 把报文的目的mac地址改成 虚拟机2的ens8的mac地址<br />
output  : 虚拟机2上的ens8对应于ovs的端口号<br />
<br />
第二条<br />
in_port : 虚拟机2的ens8对应于ovs的端口号<br />
nw_dst  : 虚拟机1上的vpc1的网络号<br />
上面四条是匹配<br />
actions 是开始动作了<br />
set_field -> eth_dst : 把报文的目的mac地址改成 虚拟机1的ens8的mac地址<br />
output  : 虚拟机1上的ens8对应于ovs的端口号 <br />


再回来看看阿里云的虚拟机上

    root@c17fff8e9667a47969f84740350890d75-node3:~# arp -n
    Address                  HWtype  HWaddress           Flags Mask            Iface
    172.18.1.2               ether   02:42:ac:12:01:02   C                     vpc-600e7
    172.19.143.161           ether   ee:ff:ff:ff:ff:ff   C                     eth0
    172.17.0.3               ether   02:42:ac:11:00:03   C                     docker_gwbridge
    172.19.143.253           ether   ee:ff:ff:ff:ff:ff   C                     eth0
    172.18.1.3               ether   02:42:ac:12:01:03   C                     vpc-600e7
    172.19.143.164           ether   ee:ff:ff:ff:ff:ff   C                     eth0
    172.17.0.4               ether   02:42:ac:11:00:04   C                     docker_gwbridge
    172.17.0.2               ether   02:42:ac:11:00:02   C                     docker_gwbridge
    root@c17fff8e9667a47969f84740350890d75-node3:~#
    
    root@c17fff8e9667a47969f84740350890d75-node1:~# arp -n
    Address                  HWtype  HWaddress           Flags Mask            Iface
    172.17.0.4               ether   02:42:ac:11:00:04   C                     docker_gwbridge
    172.18.3.2               ether   02:42:ac:12:03:02   C                     vpc-600e7
    172.18.3.4               ether   02:42:ac:12:03:04   C                     vpc-600e7
    172.19.143.253           ether   ee:ff:ff:ff:ff:ff   C                     eth0
    172.19.143.173           ether   ee:ff:ff:ff:ff:ff   C                     eth0
    172.17.0.3               ether   02:42:ac:11:00:03   C                     docker_gwbridge
    172.19.143.164           ether   ee:ff:ff:ff:ff:ff   C                     eth0
    root@c17fff8e9667a47969f84740350890d75-node1:~#

有意思，所有的172.19.143.0/24 ip 的mac地址都是 ee:ff:ff:ff:ff:ff ， 其中还包括 172.19.143.253 ，这个在缺省路由，参考了一下阿里的docker网络，这个是他们的vswitch，我猜的没错的话，阿里云的docker的ECS的VPC是由类似sdn的东西来做控制的。


删除流表命令：
    root@ubuntu:~# ovs-ofctl del-flows br-test "table=0,in_port=4"
    root@ubuntu:~# ovs-ofctl del-flows br-test "table=0,in_port=3"


第二种法子（用路由）:

在物理机上加两条路由
    root@ubuntu:~# ip route add 172.18.1.0/24 dev testgw via 10.10.10.11
    root@ubuntu:~# ip route add 172.18.3.0/24 dev testgw via 10.10.10.12

意思是 虚拟机1的VPC1的网段就让它走虚拟机1上的ens8的ip去，虚拟机2上的VPC2的网段就让它走虚拟机2上的ens8的ip去。
