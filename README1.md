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


