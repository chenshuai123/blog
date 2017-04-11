/*
 * $Id: hello.c,v 1.5 2004/10/26 03:32:21 corbet Exp $
 */
#define pr_fmt(fmt) "TCP: " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/kprobes.h>
#include <asm/traps.h>
#include <net/inet_connection_sock.h>
#include <uapi/linux/tcp.h>
#include <net/tcp.h>
#include <net/ip_fib.h>
#include <linux/inetdevice.h>
#include <linux/slab_def.h>
#include <net/protocol.h>
#include <net/route.h>
#include <uapi/linux/netfilter.h>
#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_nat_core.h>
#include <net/netfilter/nf_nat_l3proto.h>
#include <net/netfilter/nf_nat_l4proto.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_seqadj.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_extend.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <net/netfilter/nf_conntrack_ecache.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <net/netfilter/nf_conntrack_timestamp.h>
#include <net/netfilter/nf_conntrack_timeout.h>
#include <net/netfilter/nf_conntrack_labels.h>
#include <net/netfilter/nf_conntrack_synproxy.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_core.h>
#include <net/netfilter/nf_nat_helper.h>

MODULE_LICENSE("GPL");

//unsigned long filterIp = ntohl(0xd46fa8c0);
//unsigned long filterIp = 0xd26fa8c0; // host1
//unsigned long filterIp = 0xd36fa8c0; // host2
unsigned long filterIp = 0xd46fa8c0; // host3
//unsigned long filterIp = 0xd56fa8c0; // host4


//////////////////////////////////////////////////////////////////////////////////
// 日志模块，记录发包

int g_index = 0;

#define CHENSHUAI_ETHNET_HEAD (18)
#define CHENSHUAI_RECORD (100)
struct chenshuai_record {
    int index;
    unsigned long srcIp ;
    unsigned long destIp ;
    short srcPort ;
    short destPort ;
    char tcpflag ;
    struct net_device * pdevice;
    struct sock * sk;
    unsigned long _skb_refdst;
    char ethnet_head[CHENSHUAI_ETHNET_HEAD];
};

struct chenshuai_log {
       struct chenshuai_record record[CHENSHUAI_RECORD];
};

struct chenshuai_log * cslog = NULL;
unsigned long csIndex = 0;

static void chenshuai_recordlog(unsigned long srcIp, unsigned long destIp, short srcPort, short destPort, char tcpflag, struct sk_buff *skb)
{
    int recordIndex = csIndex;
    if (csIndex < (CHENSHUAI_RECORD-1))
    {
        csIndex ++;
        cslog->record[recordIndex].index       = g_index ++;
        cslog->record[recordIndex].srcIp       = srcIp;
        cslog->record[recordIndex].destIp      = destIp;
        cslog->record[recordIndex].srcPort     = srcPort;
        cslog->record[recordIndex].destPort    = destPort;
        cslog->record[recordIndex].tcpflag     = tcpflag;
        cslog->record[recordIndex].pdevice     = skb->dev;
        cslog->record[recordIndex].sk          = skb->sk;
        cslog->record[recordIndex]._skb_refdst = skb->_skb_refdst;
        memcpy(cslog->record[recordIndex].ethnet_head, (char *)skb->data, CHENSHUAI_ETHNET_HEAD);
    }
}

static void chenshuai_showlog(void)
{
    int i;
    printk ("chenshuai_showlog \n");
    for (i = 0; i < csIndex; i ++)
    {
        printk ("\nchenshuai idx:%d sIp:%x dIp:%x sPo:%x dPo:%x tflg:%x sock:%x Eth:[%2x %2x %2x %2x %2x %2x] [%2x %2x %2x %2x %2x %2x] [%2x %2x %2x %2x %2x %2x] dev:%x ops:%x nd_net:%x net:%x _skb_refdst:%x devname:%s \n",
                cslog->record[i].index,
                cslog->record[i].srcIp,
                cslog->record[i].destIp,
                cslog->record[i].srcPort,
                cslog->record[i].destPort,
                cslog->record[i].tcpflag,
                cslog->record[i].sk,
                cslog->record[i].ethnet_head[0],
                cslog->record[i].ethnet_head[1],
                cslog->record[i].ethnet_head[2],
                cslog->record[i].ethnet_head[3],
                cslog->record[i].ethnet_head[4],
                cslog->record[i].ethnet_head[5],
                cslog->record[i].ethnet_head[6],
                cslog->record[i].ethnet_head[7],
                cslog->record[i].ethnet_head[8],
                cslog->record[i].ethnet_head[9],
                cslog->record[i].ethnet_head[10],
                cslog->record[i].ethnet_head[11],
                cslog->record[i].ethnet_head[12],
                cslog->record[i].ethnet_head[13],
                cslog->record[i].ethnet_head[14],
                cslog->record[i].ethnet_head[15],
                cslog->record[i].ethnet_head[16],
                cslog->record[i].ethnet_head[17],
                cslog->record[i].pdevice,
                cslog->record[i].pdevice->netdev_ops,
                &cslog->record[i].pdevice->nd_net,
                read_pnet(&cslog->record[i].pdevice->nd_net),
                cslog->record[i]._skb_refdst,
                cslog->record[i].pdevice->name);
    }
}

//////////////////////////////////////////////////////////////////////////////////
// 日志模块，记录收报
struct chenshuai_R_record {
    int index;
    unsigned long srcIp ;
    unsigned long destIp ;
    short srcPort ;
    short destPort ;
    char tcpflag ;
    struct net_device * skb_dev;
    struct net_device * dev;
    struct net_device * orig_dev;
    struct packet_type *pt;
    struct sock * sk;
    int skb_headlen;
    int res_type;
    int skb_dst; //skb_dst(skb)
    unsigned long _skb_refdst;
    char ethnet_head[CHENSHUAI_ETHNET_HEAD];
};

struct chenshuai_R_log {
       struct chenshuai_R_record record[CHENSHUAI_RECORD];
};

struct chenshuai_R_log * csRlog = NULL;
unsigned long csRIndex = 0;

static void chenshuai_R_recordlog(unsigned long srcIp, unsigned long destIp, short srcPort, short destPort, char tcpflag, 
       struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev, int res_type)
{
    int recordIndex = csRIndex;
    if (csRIndex < (CHENSHUAI_RECORD-1))
    {
        csRIndex ++;
        csRlog->record[recordIndex].index  = g_index ++;
        csRlog->record[recordIndex].srcIp  = srcIp;
        csRlog->record[recordIndex].destIp = destIp;
        csRlog->record[recordIndex].srcPort = srcPort;
        csRlog->record[recordIndex].destPort = destPort;
        csRlog->record[recordIndex].tcpflag  = tcpflag;
        csRlog->record[recordIndex].skb_dev  = skb->dev;
        csRlog->record[recordIndex].dev      = dev;
        csRlog->record[recordIndex].orig_dev = orig_dev;
        csRlog->record[recordIndex].pt       = pt;
        csRlog->record[recordIndex].skb_headlen  = skb_headlen(skb);
        csRlog->record[recordIndex].res_type = res_type;
        csRlog->record[recordIndex].skb_dst  = skb_dst(skb);
        csRlog->record[recordIndex]._skb_refdst = skb->_skb_refdst;

        char * pbuf = (char *)skb->data;
        pbuf = pbuf - 14;
        memcpy(csRlog->record[recordIndex].ethnet_head, pbuf, CHENSHUAI_ETHNET_HEAD);
    }
}

static void chenshuai_R_showlog(void)
{
    int i;
    printk ("\nchenshuai_R_showlog iphdr len:%x\n", sizeof(struct iphdr));
    for (i = 0; i < csRIndex; i ++)
    {
        printk ("\nchenshuai idx:%d sIp:%x dIp:%x sPo:%x dPo:%x tflg:%x Eth:[%2x %2x %2x %2x %2x %2x] [%2x %2x %2x %2x %2x %2x] [%2x %2x %2x %2x %2x %2x] skb_dev:%x  dev:%x orig_dev:%x pt:%x skb_headlen:%x res_type:%x skb_dst:%x _skb_refdst:%x name:%s\n",
                csRlog->record[i].index,
                csRlog->record[i].srcIp,
                csRlog->record[i].destIp,
                csRlog->record[i].srcPort,
                csRlog->record[i].destPort,
                csRlog->record[i].tcpflag,
                csRlog->record[i].ethnet_head[0],
                csRlog->record[i].ethnet_head[1],
                csRlog->record[i].ethnet_head[2],
                csRlog->record[i].ethnet_head[3],
                csRlog->record[i].ethnet_head[4],
                csRlog->record[i].ethnet_head[5],
                csRlog->record[i].ethnet_head[6],
                csRlog->record[i].ethnet_head[7],
                csRlog->record[i].ethnet_head[8],
                csRlog->record[i].ethnet_head[9],
                csRlog->record[i].ethnet_head[10],
                csRlog->record[i].ethnet_head[11],
                csRlog->record[i].ethnet_head[12],
                csRlog->record[i].ethnet_head[13],
                csRlog->record[i].ethnet_head[14],
                csRlog->record[i].ethnet_head[15],
                csRlog->record[i].ethnet_head[16],
                csRlog->record[i].ethnet_head[17],
                csRlog->record[i].skb_dev,
                csRlog->record[i].dev,
                csRlog->record[i].orig_dev,
                csRlog->record[i].pt,
                csRlog->record[i].skb_headlen,
                csRlog->record[i].res_type,
                csRlog->record[i].skb_dst,
                csRlog->record[i]._skb_refdst,
                csRlog->record[i].dev->name);
    }
}


//////////////////////////////////////////////////////////////////////////////////

extern void replace_netdev_ops(void);
extern void recover_netdev_ops(void);

extern void replace_ip_rcv(void);
extern void recover_ip_rcv(void);

extern void replace_ipv4_dst_ops(void);
extern void recover_ipv4_dst_ops(void);

extern void replace_ip_protocol(void);
extern void recover_ip_protocol(void);

static int __init hello_init(void)
{
        cslog = kzalloc(sizeof(struct chenshuai_log), GFP_KERNEL | __GFP_NOWARN | __GFP_REPEAT);
        csRlog = kzalloc(sizeof(struct chenshuai_R_log), GFP_KERNEL | __GFP_NOWARN | __GFP_REPEAT);
/////////////////////////////////////////////////////////////////////////
        printk("Hello net_namespace_list:%x init_net:%x init_net.list:%x\n", 
                &net_namespace_list, &init_net, &init_net.list);
/////////////////////////////////////////////////////////////////////////
        //replace_ip_protocol();
/////////////////////////////////////////////////////////////////////////
        replace_ipv4_dst_ops();
/////////////////////////////////////////////////////////////////////////
        replace_netdev_ops();
/////////////////////////////////////////////////////////////////////////
        replace_ip_rcv();
/////////////////////////////////////////////////////////////////////////
        return 0;
}


static void hello_exit(void)
{
        int i;
        printk("\nGoodbye filterIp:%x\n", filterIp);

        //recover_ip_protocol();
        recover_netdev_ops();
        recover_ip_rcv();
        recover_ipv4_dst_ops();
        

        chenshuai_showlog();
        chenshuai_R_showlog();
        kvfree(cslog);
        kvfree(csRlog);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// 通过修改 arptable 来抓包，有些时候抓不到包
/*
struct neigh_table *gp_tbl;
typedef int (*chenshuai_output)(struct neighbour *neigh, struct sk_buff *skb);
chenshuai_output pfn_chenshuai_neigh_resolve_output = 0;
chenshuai_output pfn_chenshuai_neigh_connected_output = 0;

unsigned long g_neigh_resolve_output = 0;
unsigned long g_neigh_connected_output = 0;

int chenshuai_neigh_resolve_output(struct neighbour *neigh, struct sk_buff *skb)
{
    unsigned long srcIp = 0;
    unsigned long destIp = 0;
    short srcPort = 0;
    short destPort = 0;
    char tcpflag = 0;
    struct iphdr *iph;
    struct tcphdr *th;
    char * pbuf;

    iph = ip_hdr(skb);

    if (IPPROTO_TCP == iph->protocol)
    {
            // 跳过ip头
        pbuf = (char *)skb->data;
        pbuf += 20;

        th = (struct tcphdr *)pbuf;
        tcpflag = tcp_flag_byte(th);

        if ((srcIp == filterIp) || (destIp == filterIp))
//        if (((srcIp & filterIp) == filterIp) || ((destIp & filterIp) == filterIp)) 
//        if (TCPHDR_RST & tcpflag)
        {
//            char * pIpHead = (char *)iph;
            
            srcIp = iph->saddr;
            destIp = iph->daddr;

            srcPort = th->source;
            srcPort = ntohs(srcPort);

            destPort = th->dest;
            destPort = ntohs(destPort);

            if ((destPort == 22) || (srcPort == 22))
            {
                fin_count1 ++;
            //pdevice = skb->dev;
            printk ("chenshuai neigh_resolve sIp:%x dIp:%x sPo:%x dPo:%x tflg:%x sock:%x _skb_refdst:%x dev:%x %s \n",
                    srcIp, destIp, srcPort, destPort, tcpflag, skb->sk, skb->_skb_refdst, skb->dev, skb->dev->name);
//            dump_stack();
//                chenshuai_recordlog(srcIp, destIp, srcPort, destPort, tcpflag, skb);
            }
        }
    }
    return pfn_chenshuai_neigh_resolve_output(neigh, skb);
}

int chenshuai_neigh_connected_output(struct neighbour *neigh, struct sk_buff *skb)
{
    g_count2 ++;

    unsigned long srcIp = 0;
    unsigned long destIp = 0;
    short srcPort = 0;
    short destPort = 0;
    char tcpflag = 0;
    struct iphdr *iph;
    struct tcphdr *th;
    char * pbuf;

    iph = ip_hdr(skb);

    if (IPPROTO_TCP == iph->protocol)
    {
        char * pIpHead = (char *)iph;

        // 跳过ip头
        pbuf = (char *)skb->data;
        pbuf += 20;

        th = (struct tcphdr *)pbuf;
        tcpflag = tcp_flag_byte(th);

        srcIp = iph->saddr;
        destIp = iph->daddr;
        if ((srcIp == filterIp) || (destIp == filterIp))
//        if (TCPHDR_RST & tcpflag)
        {

            srcPort = th->source;
            srcPort = ntohs(srcPort);

            destPort = th->dest;
            destPort = ntohs(destPort);

            if ((destPort == 22) || (srcPort == 22))
            {
                fin_count2 ++;
            //pdevice = skb->dev;
//            printk ("chenshuai sIp:%x dIp:%x sPo:%x dPo:%x tflg:%x sock:%x dev:%x %s \n",
//                    srcIp, destIp, srcPort, destPort, tcpflag, skb->sk, skb->dev, skb->dev->name);
//            dump_stack();
//                chenshuai_recordlog(srcIp, destIp, srcPort, destPort, tcpflag, skb);
            }
        }
    }

    return pfn_chenshuai_neigh_connected_output(neigh, skb);
}

void replace_arp_table(void)
{
/////////////////////////////////////////////////////////////////////////
    g_neigh_resolve_output = kallsyms_lookup_name("neigh_resolve_output");
    g_neigh_connected_output = kallsyms_lookup_name("neigh_connected_output");
    gp_tbl = (struct neigh_table *)kallsyms_lookup_name("arp_tbl");
    struct neigh_hash_table *nht = gp_tbl->nht;

    int i;
    for (i = 0; i < 8; i++)
    {
        struct neighbour *n = nht->hash_buckets[i];
        if (0 == n)
        {
            continue;
        }
        printk ("neighbour=[%x]\n", (int)n);
        for (;n!=0; n=n->next)
        {
            printk("%x\n", (int)n->output);
            if ((int)n->output == (int)g_neigh_resolve_output)
            {        
                pfn_chenshuai_neigh_resolve_output = n->output;
                n->output = chenshuai_neigh_resolve_output;
                printk("g_neigh_resolve_output %x\n", (int)n->output);
            }
            if ((int)n->output == (int)g_neigh_connected_output)
            {
                pfn_chenshuai_neigh_connected_output = n->output;
                n->output = chenshuai_neigh_connected_output;
                printk("g_neigh_connected_output %x\n", (int)n->output);
            }
        }
    }
}

void recover_arp_table(void)
{
    struct neigh_hash_table *nht = gp_tbl->nht;
    int i;
    for (i = 0; i < 8; i++)
    {
        struct neighbour *n = nht->hash_buckets[i];
        if (0 == n)
        {
            continue;
        }
        printk ("neighbour=[%x]\n", (int)n);
        for (;n!=0; n=n->next)
        {
            if ((int)n->output == (int)chenshuai_neigh_resolve_output)
            {
                printk ("chenshuai_neigh_resolve_output %x\n", n->output);
                n->output = pfn_chenshuai_neigh_resolve_output;
            }
            if ((int)n->output == (int)chenshuai_neigh_connected_output)
            {
                printk ("chenshuai_neigh_connected_output %x\n", n->output);
                n->output = pfn_chenshuai_neigh_connected_output;
            }
            printk("Recover[%x]\n", (int)n->output);
        }
    }
}
*/

//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
// 通过接管网络设备的 ops 变量来抓包(发送)，可行
struct net_device_ops chenshuai_internal_dev_netdev_ops = {
#ifdef HAVE_DEV_TSTATS
        .ndo_init = internal_dev_init,
        .ndo_uninit = internal_dev_uninit,
        .ndo_get_stats64 = ip_tunnel_get_stats64,
#endif
        .ndo_open = 0,
        .ndo_stop = 0,
        .ndo_start_xmit = 0,
        .ndo_set_mac_address = 0,
        .ndo_change_mtu = 0,
/*
        .ndo_open = internal_dev_open,
        .ndo_stop = internal_dev_stop,
        .ndo_start_xmit = internal_dev_xmit,
        .ndo_set_mac_address = eth_mac_addr,
        .ndo_change_mtu = internal_dev_change_mtu,
*/
};

bool g_gcCap = false;

typedef int (*fn_internal_dev_xmit)(struct sk_buff *skb, struct net_device *netdev);
fn_internal_dev_xmit gfn_internal_dev_xmit;
int chenshuai_internal_dev_xmit(struct sk_buff *skb, struct net_device *netdev)
{
    unsigned long srcIp = 0;
    unsigned long destIp = 0;
    short srcPort = 0;
    short destPort = 0;
    char tcpflag = 0;
    struct iphdr *iph;
    struct tcphdr *th;
    char * pbuf;

    iph = ip_hdr(skb);

    if (IPPROTO_TCP == iph->protocol)
    {
        // 跳过ip头
        pbuf = (char *)skb->data;
        pbuf += 34;

        th = (struct tcphdr *)pbuf;
        tcpflag = tcp_flag_byte(th);



//        if ((srcIp == filterIp) || (destIp == filterIp))
//        if (((srcIp & filterIp) == filterIp) || ((destIp & filterIp) == filterIp))
//        if (TCPHDR_RST & tcpflag)
        {
//            char * pIpHead = (char *)iph;

            srcIp = iph->saddr;
            destIp = iph->daddr;

            srcPort = th->source;
            srcPort = ntohs(srcPort);

            destPort = th->dest;
            destPort = ntohs(destPort);

//            if ((destPort == 22) || (srcPort == 22))
            {
            //pdevice = skb->dev;
//            printk ("chenshuai sIp:%x dIp:%x sPo:%x dPo:%x tflg:%x sock:%x dev:%x %s \n",
//                    srcIp, destIp, srcPort, destPort, tcpflag, skb->sk, skb->dev, skb->dev->name);
//            dump_stack();
                //chenshuai_recordlog(srcIp, destIp, srcPort, destPort, tcpflag, skb);
            
            g_gcCap = false;

            char mac[14];
            memcpy(mac, skb->data, 14);

            printk ("\nchenshuai S sIp:%x dIp:%x sPo:%x dPo:%x tcpflag:%x sock:%x "
                    "Eth:[%2x %2x %2x %2x %2x %2x] [%2x %2x %2x %2x %2x %2x] [%2x %2x] "
                    "dev:%x ops:%x nd_net:%x net:%x _skb_refdst:%x skb:%x nfct:%x devname:%s \n", 
                    srcIp, destIp, srcPort, destPort, tcpflag, skb->sk, 
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mac[6], 
                    mac[7], mac[8], mac[9], mac[10], mac[11], mac[12], mac[13], 
                    skb->dev, skb->dev->netdev_ops, skb->dev->nd_net, 
                    read_pnet(skb->dev->nd_net), skb->_skb_refdst, skb, skb->nfct,
                    skb->dev->name);
/*
            if (g_dst1)
            {
                printk ("chenshuai S g_dst1:%x input:%x output:%x\n", g_dst1, g_dst1->input, g_dst1->output);
            }

            if (g_dst2)
            {
                printk ("chenshuai S g_dst2:%x input:%x output:%x\n", g_dst2, g_dst2->input, g_dst2->output);
            }
*/
            if (TCPHDR_SYN == tcpflag)
            {
                // 看ip tables 怎么转的
                //dump_stack();
            }
            if (TCPHDR_RST == tcpflag)
            {
                // 看ip tables 怎么转的
                //dump_stack();
            }
            if (TCPHDR_ACK == tcpflag)
            {
                // 看ip tables 怎么转的
                //dump_stack();
//                g_skb2 = skb;
            }
            }
        }
    }

    int ret = (*gfn_internal_dev_xmit)(skb, netdev);
    return ret;
}

void replace_netdev_ops(void)
{
    int i = 0;

    gfn_internal_dev_xmit = kallsyms_lookup_name("internal_dev_xmit");
    chenshuai_internal_dev_netdev_ops.ndo_open = kallsyms_lookup_name("internal_dev_open");
    chenshuai_internal_dev_netdev_ops.ndo_stop = kallsyms_lookup_name("internal_dev_stop");
    chenshuai_internal_dev_netdev_ops.ndo_start_xmit = chenshuai_internal_dev_xmit;
    chenshuai_internal_dev_netdev_ops.ndo_set_mac_address = kallsyms_lookup_name("eth_mac_addr");
    chenshuai_internal_dev_netdev_ops.ndo_change_mtu = kallsyms_lookup_name("internal_dev_change_mtu");

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
/*
        struct net_device *dev;
        for (i=0; i < 10000; i ++)
        {
            dev = dev_get_by_index(&init_net, i);
            if (dev)
            {
                printk ("chenshuai %d dev:%x ops:%x devname %s\n", i, dev, dev->netdev_ops, dev->name);
            }
        }
*/
}

void recover_netdev_ops(void)
{
    int i = 0;
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
                    dev->netdev_ops = (const struct net_device_ops *)kallsyms_lookup_name("internal_dev_netdev_ops");
                    printk ("chenshuai recover ops %x\n", dev->netdev_ops);
                }
            }
        }
    }
}




//////////////////////////////////////////////////////////////////////////////////////////////////////
// 抓收报

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
struct ip_rt_acct __percpu *p_ip_rt_acct;
struct net_protocol ** pp_inet_protos = NULL;
struct net_protocol * p_tcp_protocol;
int * p_sysctl_ip_early_demux ;
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
static inline bool ip_rcv_options(struct sk_buff *skb)
{
    struct ip_options *opt;
    const struct iphdr *iph;
    struct net_device *dev = skb->dev;

    /* It looks as overkill, because not all
       IP options require packet mangling.
       But it is the easiest for now, especially taking
       into account that combination of IP options
       and running sniffer is extremely rare condition.
                          --ANK (980813)
    */
    if (skb_cow(skb, skb_headroom(skb))) {
        IP_INC_STATS_BH(dev_net(dev), IPSTATS_MIB_INDISCARDS);
        goto drop;
    }

    iph = ip_hdr(skb);
    opt = &(IPCB(skb)->opt);
    opt->optlen = iph->ihl*4 - sizeof(struct iphdr);

    if (ip_options_compile(dev_net(dev), opt, skb)) {
        IP_INC_STATS_BH(dev_net(dev), IPSTATS_MIB_INHDRERRORS);
        goto drop;
    }

    if (unlikely(opt->srr)) {
        struct in_device *in_dev = __in_dev_get_rcu(dev);

        if (in_dev) {
            if (!IN_DEV_SOURCE_ROUTE(in_dev)) {
                if (IN_DEV_LOG_MARTIANS(in_dev))
                                {
                                }
                goto drop;
            }
        }

        if (ip_options_rcv_srr(skb))
            goto drop;
    }

    return false;
drop:
    return true;
}

void ip_handle_martian_source(struct net_device *dev,
                     struct in_device *in_dev,
                     struct sk_buff *skb,
                     __be32 daddr,
                     __be32 saddr)
{
}


bool chenshuai_rt_is_expired(const struct rtable *rth)
{
    return rth->rt_genid != rt_genid_ipv4(dev_net(rth->dst.dev));
}

bool chenshuai_rt_cache_valid(const struct rtable *rt)
{
        return  rt &&
                rt->dst.obsolete == DST_OBSOLETE_FORCE_CHK &&
                !chenshuai_rt_is_expired(rt);
}


#ifdef CONFIG_IP_ROUTE_CLASSID
void chenshuai_set_class_tag(struct rtable *rt, u32 tag)
{
    if (!(rt->dst.tclassid & 0xFFFF))
        rt->dst.tclassid |= tag & 0xFFFF;
    if (!(rt->dst.tclassid & 0xFFFF0000))
        rt->dst.tclassid |= tag & 0xFFFF0000;
}
#endif

void chenshuai_fill_route_from_fnhe(struct rtable *rt, struct fib_nh_exception *fnhe)
{
    rt->rt_pmtu = fnhe->fnhe_pmtu;
    rt->dst.expires = fnhe->fnhe_expires;

    if (fnhe->fnhe_gw) {
        rt->rt_flags |= RTCF_REDIRECTED;
        rt->rt_gateway = fnhe->fnhe_gw;
        rt->rt_uses_gateway = 1;
    }
}

void chenshuai_rt_free(struct rtable *rt)
{
    call_rcu(&rt->dst.rcu_head, dst_rcu_free);
}

void chenshuai_fnhe_flush_routes(struct fib_nh_exception *fnhe)
{
    struct rtable *rt;

    rt = rcu_dereference(fnhe->fnhe_rth_input);
    if (rt) {
        RCU_INIT_POINTER(fnhe->fnhe_rth_input, NULL);
        chenshuai_rt_free(rt);
    }
    rt = rcu_dereference(fnhe->fnhe_rth_output);
    if (rt) {
        RCU_INIT_POINTER(fnhe->fnhe_rth_output, NULL);
        chenshuai_rt_free(rt);
    }
}

typedef int (*fn_ip_route_input_noref)(struct sk_buff *skb, __be32 dst, __be32 src, u8 tos, struct net_device *devin);
fn_ip_route_input_noref pfn_ip_route_input_noref;

typedef void (*fn_fib_select_multipath)(struct fib_result *res);
fn_fib_select_multipath pfn_fib_select_multipath;

typedef int (*fn_fib_validate_source)(struct sk_buff *skb, __be32 src, __be32 dst,
                                      u8 tos, int oif, struct net_device *dev,
                                      struct in_device *idev, u32 *itag);
fn_fib_validate_source pfn_fib_validate_source;

typedef struct rtable *(*fn_rt_dst_alloc)(struct net_device *dev,
                                          bool nopolicy, bool noxfrm, bool will_cache);
fn_rt_dst_alloc pfn_rt_dst_alloc;

typedef bool (*fn_rt_cache_route)(struct fib_nh *nh, struct rtable *rt);
fn_rt_cache_route pfn_rt_cache_route;

typedef struct fib_nh_exception *(*fn_find_exception)(struct fib_nh *nh, __be32 daddr);
fn_find_exception pfn_find_exception;

typedef void (*fn_rt_add_uncached_list)(struct rtable *rt);
fn_rt_add_uncached_list pfn_rt_add_uncached_list;

typedef int (*fn_inet_addr_onlink)(struct in_device *in_dev, __be32 a, __be32 b);
fn_inet_addr_onlink pfn_inet_addr_onlink;

typedef int (*fn_input)(struct sk_buff *);
typedef int (*fn_output)(struct sock *sk, struct sk_buff *skb);

fn_input pfn_ip_forward;
fn_output pfn_ip_output;

fn_input pfn_ip_local_deliver;
fn_output pfn_ip_rt_bug;

fn_input pfn_ip_error;

spinlock_t * p_fnhe_lock = NULL;

bool chenshuai_mock_rt_bind_exception(struct rtable *rt, struct fib_nh_exception *fnhe,
                  __be32 daddr)
{
    bool ret = false;

    //spin_lock_bh(&fnhe_lock);
    spin_lock_bh(p_fnhe_lock);

    if (daddr == fnhe->fnhe_daddr) {
        struct rtable __rcu **porig;
        struct rtable *orig;
        int genid = fnhe_genid(dev_net(rt->dst.dev));

        if (rt_is_input_route(rt))
            porig = &fnhe->fnhe_rth_input;
        else
            porig = &fnhe->fnhe_rth_output;
        orig = rcu_dereference(*porig);

        if (fnhe->fnhe_genid != genid) {
            fnhe->fnhe_genid = genid;
            fnhe->fnhe_gw = 0;
            fnhe->fnhe_pmtu = 0;
            fnhe->fnhe_expires = 0;
            //fnhe_flush_routes(fnhe);
            chenshuai_fnhe_flush_routes(fnhe);
            orig = NULL;
        }
        //fill_route_from_fnhe(rt, fnhe);
        chenshuai_fill_route_from_fnhe(rt, fnhe);
        if (!rt->rt_gateway)
            rt->rt_gateway = daddr;

        if (!(rt->dst.flags & DST_NOCACHE)) {
            rcu_assign_pointer(*porig, rt);
            if (orig)
                chenshuai_rt_free(orig);
            ret = true;
        }

        fnhe->fnhe_stamp = jiffies;
    }
    //spin_unlock_bh(&fnhe_lock);
    spin_unlock_bh(p_fnhe_lock);

    return ret;
}

void chenshuai_mock_rt_set_nexthop(struct rtable *rt, __be32 daddr,
               const struct fib_result *res,
               struct fib_nh_exception *fnhe,
               struct fib_info *fi, u16 type, u32 itag)
{
    bool cached = false;

    if (fi) {
        struct fib_nh *nh = &FIB_RES_NH(*res);

        if (nh->nh_gw && nh->nh_scope == RT_SCOPE_LINK) {
            rt->rt_gateway = nh->nh_gw;
            rt->rt_uses_gateway = 1;
        }
        dst_init_metrics(&rt->dst, fi->fib_metrics, true);
#ifdef CONFIG_IP_ROUTE_CLASSID
        rt->dst.tclassid = nh->nh_tclassid;
#endif
        if (unlikely(fnhe))
        {
            //cached = rt_bind_exception(rt, fnhe, daddr);
            cached = chenshuai_mock_rt_bind_exception(rt, fnhe, daddr);
        }
        else if (!(rt->dst.flags & DST_NOCACHE))
        {
            //cached = rt_cache_route(nh, rt);
            cached = (*pfn_rt_cache_route)(nh, rt);
        }
        if (unlikely(!cached)) {
            /* Routes we intend to cache in nexthop exception or
             * FIB nexthop have the DST_NOCACHE bit clear.
             * However, if we are unsuccessful at storing this
             * route into the cache we really need to set it.
             */
            rt->dst.flags |= DST_NOCACHE;
            if (!rt->rt_gateway)
                rt->rt_gateway = daddr;
            //rt_add_uncached_list(rt);
            (*pfn_rt_add_uncached_list)(rt);
        }
    } else
    {
        //rt_add_uncached_list(rt);
        (*pfn_rt_add_uncached_list)(rt);
    }

#ifdef CONFIG_IP_ROUTE_CLASSID
#ifdef CONFIG_IP_MULTIPLE_TABLES
    chenshuai_set_class_tag(rt, res->tclassid);
#endif
    chenshuai_set_class_tag(rt, itag);
#endif
}

bool g_bRcvCap = false;
int g_count6 = 0;

int chenshuai_mock__mkroute_input(struct sk_buff *skb,
               const struct fib_result *res,
               struct in_device *in_dev,
               __be32 daddr, __be32 saddr, u32 tos)
{
    struct fib_nh_exception *fnhe;
    struct rtable *rth;
    int err;
    struct in_device *out_dev;
    unsigned int flags = 0;
    bool do_cache;
    u32 itag = 0;

    if (g_bRcvCap)
    {
        g_count6 ++;
    }

    /* get a working reference to the output device */
    out_dev = __in_dev_get_rcu(FIB_RES_DEV(*res));
    if (out_dev == NULL) {
        return -EINVAL;
    }

    //err = fib_validate_source(skb, saddr, daddr, tos, FIB_RES_OIF(*res),
    err = (*pfn_fib_validate_source)(skb, saddr, daddr, tos, FIB_RES_OIF(*res),
                  in_dev->dev, in_dev, &itag);
    if (err < 0) {
        ip_handle_martian_source(in_dev->dev, in_dev, skb, daddr,
                     saddr);

        goto cleanup;
    }

    do_cache = res->fi && !itag;
    if (out_dev == in_dev && err && IN_DEV_TX_REDIRECTS(out_dev) &&
        skb->protocol == htons(ETH_P_IP) &&
        (IN_DEV_SHARED_MEDIA(out_dev) ||
         (*pfn_inet_addr_onlink)(out_dev, saddr, FIB_RES_GW(*res))))
        IPCB(skb)->flags |= IPSKB_DOREDIRECT;

    if (skb->protocol != htons(ETH_P_IP)) {
        /* Not IP (i.e. ARP). Do not create route, if it is
         * invalid for proxy arp. DNAT routes are always valid.
         *
         * Proxy arp feature have been extended to allow, ARP
         * replies back to the same interface, to support
         * Private VLAN switch technologies. See arp.c.
         */
        if (out_dev == in_dev &&
            IN_DEV_PROXY_ARP_PVLAN(in_dev) == 0) {
            err = -EINVAL;
            goto cleanup;
        }
    }

//    fnhe = find_exception(&FIB_RES_NH(*res), daddr);
    fnhe = (*pfn_find_exception)(&FIB_RES_NH(*res), daddr);
    if (do_cache) {
        if (fnhe != NULL)
            rth = rcu_dereference(fnhe->fnhe_rth_input);
        else
            rth = rcu_dereference(FIB_RES_NH(*res).nh_rth_input);

//        if (rt_cache_valid(rth)) {
        if ((*chenshuai_rt_cache_valid)(rth)) {
            skb_dst_set_noref(skb, &rth->dst);
            goto out;
        }
    }

//    rth = rt_dst_alloc(out_dev->dev,
    rth = (*pfn_rt_dst_alloc)(out_dev->dev,
               IN_DEV_CONF_GET(in_dev, NOPOLICY),
               IN_DEV_CONF_GET(out_dev, NOXFRM), do_cache);
    if (!rth) {
        err = -ENOBUFS;
        goto cleanup;
    }

    rth->rt_genid = rt_genid_ipv4(dev_net(rth->dst.dev));
    rth->rt_flags = flags;
    rth->rt_type = res->type;
    rth->rt_is_input = 1;
    rth->rt_iif     = 0;
    rth->rt_pmtu    = 0;
    rth->rt_gateway = 0;
    rth->rt_uses_gateway = 0;
    INIT_LIST_HEAD(&rth->rt_uncached);

    //rth->dst.input = ip_forward;
    //rth->dst.output = ip_output;

    rth->dst.input = pfn_ip_forward;
    rth->dst.output = pfn_ip_output;

    chenshuai_mock_rt_set_nexthop(rth, daddr, res, fnhe, res->fi, res->type, itag);
    skb_dst_set(skb, &rth->dst);
out:
    err = 0;
 cleanup:
    return err;
}

int chenshuai_mock_ip_mkroute_input(struct sk_buff *skb,
                struct fib_result *res,
                const struct flowi4 *fl4,
                struct in_device *in_dev,
                __be32 daddr, __be32 saddr, u32 tos)
{
#ifdef CONFIG_IP_ROUTE_MULTIPATH
    if (res->fi && res->fi->fib_nhs > 1)
        (*pfn_fib_select_multipath)(res);
#endif

    /* create a routing cache entry */
    return chenshuai_mock__mkroute_input(skb, res, in_dev, daddr, saddr, tos);
}

int g_count5 = 0;
int chenshuai_mock_ip_route_input_slow(struct sk_buff *skb, __be32 daddr, __be32 saddr,
                   u8 tos, struct net_device *dev)
{
    struct fib_result res;
    struct in_device *in_dev = __in_dev_get_rcu(dev);
    struct flowi4   fl4;
    unsigned int    flags = 0;
    u32     itag = 0;
    struct rtable   *rth;
    int     err = -EINVAL;
    struct net    *net = dev_net(dev);
    bool do_cache;

    if (g_bRcvCap)
    {
        g_count5 ++;
    }

    /* IP on this device is disabled. */

    if (!in_dev)
        goto out;

    /* Check for the most weird martians, which can be not detected
       by fib_lookup.
     */

    if (ipv4_is_multicast(saddr) || ipv4_is_lbcast(saddr))
        goto martian_source;

    res.fi = NULL;
    if (ipv4_is_lbcast(daddr) || (saddr == 0 && daddr == 0))
        goto brd_input;

    /* Accept zero addresses only to limited broadcast;
     * I even do not know to fix it or not. Waiting for complains :-)
     */
    if (ipv4_is_zeronet(saddr))
        goto martian_source;

    if (ipv4_is_zeronet(daddr))
        goto martian_destination;

    /* Following code try to avoid calling IN_DEV_NET_ROUTE_LOCALNET(),
     * and call it once if daddr or/and saddr are loopback addresses
     */
    if (ipv4_is_loopback(daddr)) {
        if (!IN_DEV_NET_ROUTE_LOCALNET(in_dev, net))
            goto martian_destination;
    } else if (ipv4_is_loopback(saddr)) {
        if (!IN_DEV_NET_ROUTE_LOCALNET(in_dev, net))
            goto martian_source;
    }

    /*
     *  Now we are ready to route packet.
     */
    fl4.flowi4_oif = 0;
    fl4.flowi4_iif = dev->ifindex;
    fl4.flowi4_mark = skb->mark;
    fl4.flowi4_tos = tos;
    fl4.flowi4_scope = RT_SCOPE_UNIVERSE;
    fl4.daddr = daddr;
    fl4.saddr = saddr;
    err = fib_lookup(net, &fl4, &res);
    if (g_bRcvCap)
    {
        printk ("fib_lookup sIp:%x dIp:%x iif:%x tos:%x mark:%x err:%x type:%x\n", 
                saddr, daddr, dev->ifindex, tos, skb->mark, err, res.type);
    }
    if (err != 0) {
        if (!IN_DEV_FORWARD(in_dev))
            err = -EHOSTUNREACH;
        goto no_route;
    }

    if (res.type == RTN_BROADCAST)
        goto brd_input;

    if (res.type == RTN_LOCAL) {
        err = (*pfn_fib_validate_source)(skb, saddr, daddr, tos,
                      0, dev, in_dev, &itag);
        if (err < 0)
            goto martian_source_keep_err;
        goto local_input;
    }

    if (!IN_DEV_FORWARD(in_dev)) {
        err = -EHOSTUNREACH;
        goto no_route;
    }
    if (res.type != RTN_UNICAST)
        goto martian_destination;

    err = chenshuai_mock_ip_mkroute_input(skb, &res, &fl4, in_dev, daddr, saddr, tos);
    if (g_bRcvCap)
    {
        printk("chenshuai_mock_ip_mkroute_input skb:%x dst:%x\n", skb, skb_dst(skb));
    }
out:    return err;

brd_input:
    if (skb->protocol != htons(ETH_P_IP))
        goto e_inval;

    if (!ipv4_is_zeronet(saddr)) {
        err = (*pfn_fib_validate_source)(skb, saddr, 0, tos, 0, dev,
                      in_dev, &itag);
        if (err < 0)
            goto martian_source_keep_err;
    }
    flags |= RTCF_BROADCAST;
    res.type = RTN_BROADCAST;
//    RT_CACHE_STAT_INC(in_brd);

local_input:
    do_cache = false;
    if (res.fi) {
        if (!itag) {
            rth = rcu_dereference(FIB_RES_NH(res).nh_rth_input);
            if (chenshuai_rt_cache_valid(rth)) {
                skb_dst_set_noref(skb, &rth->dst);
                err = 0;
                goto out;
            }
            do_cache = true;
        }
    }

    rth = (*pfn_rt_dst_alloc)(net->loopback_dev,
               IN_DEV_CONF_GET(in_dev, NOPOLICY), false, do_cache);
    if (g_bRcvCap)
    {
        printk ("chenshuai_mock_ip_route_input_slow rt_dst_alloc %x\n", rth);
    }
    if (!rth)
        goto e_nobufs;

    //rth->dst.input= ip_local_deliver;
    //rth->dst.output= ip_rt_bug;

    rth->dst.input= pfn_ip_local_deliver;
    rth->dst.output= pfn_ip_rt_bug;
#ifdef CONFIG_IP_ROUTE_CLASSID
    rth->dst.tclassid = itag;
#endif

    rth->rt_genid = rt_genid_ipv4(net);
    rth->rt_flags   = flags|RTCF_LOCAL;
    rth->rt_type    = res.type;
    rth->rt_is_input = 1;
    rth->rt_iif = 0;
    rth->rt_pmtu    = 0;
    rth->rt_gateway = 0;
    rth->rt_uses_gateway = 0;
    INIT_LIST_HEAD(&rth->rt_uncached);
//    RT_CACHE_STAT_INC(in_slow_tot);
    if (res.type == RTN_UNREACHABLE) {
        //rth->dst.input= ip_error;
        rth->dst.input= pfn_ip_error;
        rth->dst.error= -err;
        rth->rt_flags   &= ~RTCF_LOCAL;
    }
    if (do_cache) {
        if (unlikely(!(*pfn_rt_cache_route)(&FIB_RES_NH(res), rth))) {
            rth->dst.flags |= DST_NOCACHE;
            //rt_add_uncached_list(rth);
            (*pfn_rt_add_uncached_list)(rth);
        }
    }
    skb_dst_set(skb, &rth->dst);
    err = 0;
    goto out;

no_route:
//    RT_CACHE_STAT_INC(in_no_route);
    res.type = RTN_UNREACHABLE;
    res.fi = NULL;
    goto local_input;

    /*
     *  Do not cache martian addresses: they should be logged (RFC1812)
     */
martian_destination:
//    RT_CACHE_STAT_INC(in_martian_dst);
#ifdef CONFIG_IP_ROUTE_VERBOSE
    if (IN_DEV_LOG_MARTIANS(in_dev))
    {
//        net_warn_ratelimited("martian destination %pI4 from %pI4, dev %s\n",
//                     &daddr, &saddr, dev->name);
    }
#endif

e_inval:
    err = -EINVAL;
    goto out;

e_nobufs:
    err = -ENOBUFS;
    goto out;

martian_source:
    err = -EINVAL;
martian_source_keep_err:
    ip_handle_martian_source(dev, in_dev, skb, daddr, saddr);
    goto out;
}

int g_count4 = 0;
int chenshuai_mock_ip_route_input_noref(struct sk_buff *skb, __be32 daddr, __be32 saddr,
             u8 tos, struct net_device *dev)
{
    int res;

    rcu_read_lock();

    /* Multicast recognition logic is moved from route cache to here.
       The problem was that too many Ethernet cards have broken/missing
       hardware multicast filters :-( As result the host on multicasting
       network acquires a lot of useless route cache entries, sort of
       SDR messages from all the world. Now we try to get rid of them.
       Really, provided software IP multicast filter is organized
       reasonably (at least, hashed), it does not result in a slowdown
       comparing with route cache reject entries.
       Note, that multicast routers are not affected, because
       route cache entry is created eventually.
     */
    if (ipv4_is_multicast(daddr)) {
        rcu_read_unlock();
        (*pfn_ip_route_input_noref)(skb, daddr, saddr, tos, dev);
        return -EINVAL;
    }
    
    if (g_bRcvCap)
    {
        g_count4 ++;
    }

    res = chenshuai_mock_ip_route_input_slow(skb, daddr, saddr, tos, dev);
    rcu_read_unlock();
    return res;
}

int g_count1 = 0;
int g_count2 = 0;
int g_count3 = 0;

int chenshuai_mock_ip_rcv_finish(struct sk_buff *skb)
{
    const struct iphdr *iph = ip_hdr(skb);
    struct rtable *rt;

////////////

    char * pbuf;
    struct tcphdr *th;
    char tcpflag = 0;
    unsigned long srcIp  = iph->saddr;
    unsigned long destIp = iph->daddr;
    short srcPort  = 0;
    short destPort = 0;
    
    if (IPPROTO_TCP == iph->protocol)
    {
        pbuf = (char *)skb->data;
        pbuf += 20;
        th = (struct tcphdr *)pbuf;

        if ((srcIp == filterIp) || (destIp == filterIp))
        {
            srcPort = th->source;
            srcPort = ntohs(srcPort);

            destPort = th->dest;
            destPort = ntohs(destPort);

            tcpflag = tcp_flag_byte(th);

            g_count1 ++;
        }
    }

    if (*p_sysctl_ip_early_demux && !skb_dst(skb) && skb->sk == NULL) {
        const struct net_protocol *ipprot;
        int protocol = iph->protocol;

        ipprot = rcu_dereference(pp_inet_protos[protocol]);
        if (ipprot && ipprot->early_demux) {
            ipprot->early_demux(skb);
            /* must reload iph, skb->head might have changed */
            iph = ip_hdr(skb);
            if (g_bRcvCap)
            {
                struct in_device *in_dev = __in_dev_get_rcu(skb->dev);
                g_count2 ++;
                printk ("\nchenshuai R1 sIp:%x dIp:%x sPo:%x dPo:%x tcpflag:%x dev:%x dev_net:%x "
                        "skb_iif:%x skb_dst:%x in_dev:%x in_dev->name:%s name:%s\n",
                        srcIp, destIp, srcPort, destPort, tcpflag, skb->dev, dev_net(skb->dev), 
                        skb->skb_iif, skb_dst(skb), in_dev, in_dev->dev->name, skb->dev->name);
            }
        }
    }

    /*
     *  Initialise the virtual path cache for the packet. It describes
     *  how the packet travels inside Linux networking.
     */
    if (!skb_dst(skb)) {
        int err = chenshuai_mock_ip_route_input_noref(skb, iph->daddr, iph->saddr,
                                                      iph->tos, skb->dev);
        if (unlikely(err)) {
            if (err == -EXDEV)
            {}
            goto drop;
        }
        if (g_bRcvCap)
        {
            struct in_device *in_dev = __in_dev_get_rcu(skb->dev);
            g_count3 ++;
            printk ("\nchenshuai R2 sIp:%x dIp:%x sPo:%x dPo:%x tcpflag:%x dev:%x dev_net:%x "
                    "skb_iif:%x skb_dst:%x name:%s\n",
                    srcIp, destIp, srcPort, destPort, tcpflag, skb->dev, dev_net(skb->dev),
                    skb->skb_iif, skb_dst(skb), skb->dev->name);
        }
    }

#ifdef CONFIG_IP_ROUTE_CLASSID
    if (unlikely(skb_dst(skb)->tclassid)) {
        struct ip_rt_acct *st = this_cpu_ptr(p_ip_rt_acct);
        u32 idx = skb_dst(skb)->tclassid;
        st[idx&0xFF].o_packets++;
        st[idx&0xFF].o_bytes += skb->len;
        st[(idx>>16)&0xFF].i_packets++;
        st[(idx>>16)&0xFF].i_bytes += skb->len;
    }
#endif

    if (iph->ihl > 5 && ip_rcv_options(skb))
        goto drop;

    rt = skb_rtable(skb);
    if (rt->rt_type == RTN_MULTICAST) {
        IP_UPD_PO_STATS_BH(dev_net(rt->dst.dev), IPSTATS_MIB_INMCAST,
                skb->len);
    } else if (rt->rt_type == RTN_BROADCAST)
        IP_UPD_PO_STATS_BH(dev_net(rt->dst.dev), IPSTATS_MIB_INBCAST,
                skb->len);

    if (g_bRcvCap)
    {
        struct in_device *in_dev = __in_dev_get_rcu(skb->dev);
        printk ("\nchenshuai R3 sIp:%x dIp:%x sPo:%x dPo:%x tcpflag:%x dev:%x dev_net:%x "
                "skb_iif:%x skb_dst:%x input:%x output:%x name:%s\n",
                srcIp, destIp, srcPort, destPort, tcpflag, skb->dev, dev_net(skb->dev),
                skb->skb_iif, skb_dst(skb), skb_dst(skb)->input, skb_dst(skb)->output, 
                skb->dev->name);
        g_bRcvCap = false;
    }
    return dst_input(skb);

drop:
    kfree_skb(skb);
    return NET_RX_DROP;
}

typedef int (*fn_ip_rcv_finish)(struct sk_buff *skb);
fn_ip_rcv_finish pfn_ip_rcv_finish;

int g_mock_ip_rcv_count = 0;


////////////////////////////////////
// netfilter
unsigned int chenshuai_nf_iterate(struct list_head *head,
            struct sk_buff *skb,
            unsigned int hook,
            const struct net_device *indev,
            const struct net_device *outdev,
            struct nf_hook_ops **elemp,
            int (*okfn)(struct sk_buff *),
            int hook_thresh)
{
    unsigned int verdict;

    /*
     * The caller must not block between calls to this
     * function because of risk of continuing from deleted element.
     */
    list_for_each_entry_continue_rcu((*elemp), head, list) {
        if (hook_thresh > (*elemp)->priority)
            continue;

        /* Optimization: we don't need to hold module
           reference here, since function can't sleep. --RR */
repeat:
        verdict = (*elemp)->hook(*elemp, skb, indev, outdev, okfn);
        if (g_bRcvCap)
        {
            struct iphdr *iph;
            iph = ip_hdr(skb);
            printk ("chenshuai_nf_iterate skb:%x sIp:%x dIp:%x hook:%x elemp:%x verdict:%x nfct:%x\n", 
                    skb, iph->saddr, iph->daddr, (*elemp)->hook, *elemp, verdict, skb->nfct);
        }
        if (verdict != NF_ACCEPT) {
#ifdef CONFIG_NETFILTER_DEBUG
            if (unlikely((verdict & NF_VERDICT_MASK)
                            > NF_MAX_VERDICT)) {
                NFDEBUG("Evil return from %p(%u).\n",
                    (*elemp)->hook, hook);
                continue;
            }
#endif
            if (verdict != NF_REPEAT)
                return verdict;
            goto repeat;
        }
    }
    return NF_ACCEPT;
}

typedef int (*fn_nf_queue)(struct sk_buff *skb,
                           struct nf_hook_ops *elem,
                           u_int8_t pf, unsigned int hook,
                           struct net_device *indev,
                           struct net_device *outdev,
                           int (*okfn)(struct sk_buff *),
                           unsigned int queuenum);
fn_nf_queue pfn_nf_queue;

int g_count7 = 0;

int chenshuai_nf_hook_slow(u_int8_t pf, unsigned int hook, struct sk_buff *skb,
         struct net_device *indev,
         struct net_device *outdev,
         int (*okfn)(struct sk_buff *),
         int hook_thresh)
{
    struct nf_hook_ops *elem;
    unsigned int verdict;
    int ret = 0;

    ///////
    struct iphdr *iph;
    if (g_bRcvCap)
    {
        iph = ip_hdr(skb);
        printk ("\nchenshuai_nf_hook_slow0 skb:%x sIp:%x dIp:%x hook:%x indev:%x outdev:%x "
                "okfn:%x hook_thresh:%x nfct:%x\n", 
                skb, iph->saddr, iph->daddr, hook, indev, outdev, 
                okfn, hook_thresh, skb->nfct);
    }

    /* We may already have this, but read-locks nest anyway */
    rcu_read_lock();

    elem = list_entry_rcu(&nf_hooks[pf][hook], struct nf_hook_ops, list);
next_hook:
    verdict = chenshuai_nf_iterate(&nf_hooks[pf][hook], skb, hook, indev,
                                   outdev, &elem, okfn, hook_thresh);
    if (verdict == NF_ACCEPT || verdict == NF_STOP) 
    {
        if (g_bRcvCap)
        {
            iph = ip_hdr(skb);
            printk ("\nchenshuai_nf_hook_slow1 verdict:%x elem:%x skb:%x sIp:%x dIp:%x nfct:%x\n", 
                    verdict, &elem, skb, iph->saddr, iph->daddr, skb->nfct);
        }
        ret = 1;
    } 
    else if ((verdict & NF_VERDICT_MASK) == NF_DROP) 
    {
        if (g_bRcvCap)
        {
            iph = ip_hdr(skb);
            printk ("\nchenshuai_nf_hook_slow2 verdict:%x elem:%x skb:%x sIp:%x dIp:%x nfct:%x\n", 
                    verdict, &elem, skb, iph->saddr, iph->daddr, skb->nfct);
        }

        kfree_skb(skb);
        ret = NF_DROP_GETERR(verdict);
        if (ret == 0)
            ret = -EPERM;
    } 
    else if ((verdict & NF_VERDICT_MASK) == NF_QUEUE) 
    {
        if (g_bRcvCap)
        {
            iph = ip_hdr(skb);
            printk ("\nchenshuai_nf_hook_slow3 verdict:%x elem:%x skb:%x sIp:%x dIp:%x nfct:%x\n", 
                    verdict, &elem, skb, iph->saddr, iph->daddr, skb->nfct);
        }

        //int err = nf_queue(skb, elem, pf, hook, indev, outdev, okfn,
        int err = (*pfn_nf_queue)(skb, elem, pf, hook, indev, outdev, okfn,
                                 verdict >> NF_VERDICT_QBITS);
        if (err < 0) 
        {
            if (err == -ECANCELED)
                goto next_hook;
            if (err == -ESRCH &&
               (verdict & NF_VERDICT_FLAG_QUEUE_BYPASS))
                goto next_hook;
            kfree_skb(skb);
        }
    }
    rcu_read_unlock();
    return ret;
}


typedef int (*fn_nf_hook_slow)(u_int8_t pf, unsigned int hook, struct sk_buff *skb,
                               struct net_device *indev,
                               struct net_device *outdev,
                               int (*okfn)(struct sk_buff *),
                               int hook_thresh);

fn_nf_hook_slow pfn_nf_hook_slow;


int chenshuai_nf_hook_thresh(u_int8_t pf, unsigned int hook,
                                 struct sk_buff *skb,
                                 struct net_device *indev,
                                 struct net_device *outdev,
                                 int (*okfn)(struct sk_buff *), int thresh)
{
        if (nf_hooks_active(pf, hook))
        {
            //return nf_hook_slow(pf, hook, skb, indev, outdev, okfn, thresh);
            //return (*pfn_nf_hook_slow)(pf, hook, skb, indev, outdev, okfn, thresh);
            return chenshuai_nf_hook_slow(pf, hook, skb, indev, outdev, okfn, thresh);
        }
        return 1;
}

int
CHENSHUAI_NF_HOOK_THRESH(uint8_t pf, unsigned int hook, struct sk_buff *skb,
               struct net_device *in, struct net_device *out,
               int (*okfn)(struct sk_buff *), int thresh)
{
        int ret = chenshuai_nf_hook_thresh(pf, hook, skb, in, out, okfn, thresh);
        if (ret == 1)
                ret = okfn(skb);
        return ret;
}

int
CHENSHUAI_NF_HOOK(uint8_t pf, unsigned int hook, struct sk_buff *skb,
        struct net_device *in, struct net_device *out,
        int (*okfn)(struct sk_buff *))
{
    return CHENSHUAI_NF_HOOK_THRESH(pf, hook, skb, in, out, okfn, INT_MIN);
}

/////////////////////////////

int chenshuai_mock_ip_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev)
{
    const struct iphdr *iph;
    u32 len;

/////////////////////////////
    char * pbuf;
    unsigned long srcIp = 0;
    unsigned long destIp = 0;
    short srcPort = 0;
    short destPort = 0;
    char tcpflag = 0;
    struct tcphdr *th;

    iph = ip_hdr(skb);
    if (IPPROTO_TCP == iph->protocol)
    {
        pbuf = (char *)skb->data;
        pbuf += 20;

        th = (struct tcphdr *)pbuf;
        tcpflag = tcp_flag_byte(th);

        srcIp = iph->saddr;
        destIp = iph->daddr;

        if ((srcIp == filterIp) || (destIp == filterIp))
        {
            srcPort = th->source;
            srcPort = ntohs(srcPort);

            destPort = th->dest;
            destPort = ntohs(destPort);

            g_bRcvCap = true;
            printk ("\nchenshuai_mock_ip_rcv1 sIP:%x dIp:%x sPo:%x dPo:%x tcpflag:%x skb:%x "
                    "skb_dev:%x nfct:%x\n",
                     srcIp, destIp, srcPort, destPort, tcpflag, skb, skb->dev, skb->nfct);
        }
    }

    /* When the interface is in promisc. mode, drop all the crap
     * that it receives, do not try to analyse it.
     */
    if (skb->pkt_type == PACKET_OTHERHOST)
        goto drop;


    IP_UPD_PO_STATS_BH(dev_net(dev), IPSTATS_MIB_IN, skb->len);

    if ((skb = skb_share_check(skb, GFP_ATOMIC)) == NULL) {
        goto out;
    }

    if (!pskb_may_pull(skb, sizeof(struct iphdr)))
        goto inhdr_error;

    iph = ip_hdr(skb);

    /*
     *  RFC1122: 3.2.1.2 MUST silently discard any IP frame that fails the checksum.
     *
     *  Is the datagram acceptable?
     *
     *  1.  Length at least the size of an ip header
     *  2.  Version of 4
     *  3.  Checksums correctly. [Speed optimisation for later, skip loopback checksums]
     *  4.  Doesn't have a bogus length
     */

    if (iph->ihl < 5 || iph->version != 4)
        goto inhdr_error;

    BUILD_BUG_ON(IPSTATS_MIB_ECT1PKTS != IPSTATS_MIB_NOECTPKTS + INET_ECN_ECT_1);
    BUILD_BUG_ON(IPSTATS_MIB_ECT0PKTS != IPSTATS_MIB_NOECTPKTS + INET_ECN_ECT_0);
    BUILD_BUG_ON(IPSTATS_MIB_CEPKTS != IPSTATS_MIB_NOECTPKTS + INET_ECN_CE);
    IP_ADD_STATS_BH(dev_net(dev),
            IPSTATS_MIB_NOECTPKTS + (iph->tos & INET_ECN_MASK),
            max_t(unsigned short, 1, skb_shinfo(skb)->gso_segs));

    if (!pskb_may_pull(skb, iph->ihl*4))
        goto inhdr_error;

    iph = ip_hdr(skb);

    if (unlikely(ip_fast_csum((u8 *)iph, iph->ihl)))
        goto csum_error;

    len = ntohs(iph->tot_len);
    if (skb->len < len) {
        goto drop;
    } else if (len < (iph->ihl*4))
        goto inhdr_error;

    /* Our transport medium may have padded the buffer out. Now we know it
     * is IP we can trim to the true length of the frame.
     * Note this now means skb->len holds ntohs(iph->tot_len).
     */
    if (pskb_trim_rcsum(skb, len)) {
        goto drop;
    }

    skb->transport_header = skb->network_header + iph->ihl*4;

    /* Remove any debris in the socket control block */
    memset(IPCB(skb), 0, sizeof(struct inet_skb_parm));

    /* Must drop socket now because of tproxy. */
    skb_orphan(skb);

//    g_mock_ip_rcv_count ++;
    if(g_bRcvCap)
    {
    printk ("chenshuai_mock_ip_rcv2 sIP:%x dIp:%x sPo:%x dPo:%x tcpflag:%x skb:%x "
            "skb_dev:%x dev:%x nfct:%x\n",
            srcIp, destIp, srcPort, destPort, tcpflag, skb, skb->dev, dev, skb->nfct);
    }

    return CHENSHUAI_NF_HOOK(NFPROTO_IPV4, NF_INET_PRE_ROUTING, skb, dev, NULL,
//                   pfn_ip_rcv_finish);
                   chenshuai_mock_ip_rcv_finish);

csum_error:
    IP_INC_STATS_BH(dev_net(dev), IPSTATS_MIB_CSUMERRORS);
inhdr_error:
    IP_INC_STATS_BH(dev_net(dev), IPSTATS_MIB_INHDRERRORS);
drop:
    kfree_skb(skb);
out:
    return NET_RX_DROP;
}

//////////////////////////////////////////////////////////////////

extern int chenshuai_fib_lookup(struct sk_buff *skb);

struct packet_type *p_ip_packet_type;

typedef int (*fn_ip_rcv)(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev);
fn_ip_rcv pfn_ip_rcv;

int g_ip_rcv_count = 0;


int chenshuai_ip_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev)
{
    char * pbuf;
    unsigned long srcIp = 0;
    unsigned long destIp = 0;
    short srcPort = 0;
    short destPort = 0;
    char tcpflag = 0;
    struct tcphdr *th;
    struct iphdr *iph;

    iph = ip_hdr(skb);
    if (IPPROTO_TCP == iph->protocol)
    {
        pbuf = (char *)skb->data;
        pbuf += 20;

        th = (struct tcphdr *)pbuf;
        tcpflag = tcp_flag_byte(th);

        srcIp = iph->saddr;
        destIp = iph->daddr;

        if ((srcIp == filterIp) || (destIp == filterIp))
        {
        srcPort = th->source;
        srcPort = ntohs(srcPort);

        destPort = th->dest;
        destPort = ntohs(destPort);

        int res_type = chenshuai_fib_lookup(skb);

        //chenshuai_R_recordlog(srcIp, destIp, srcPort, destPort, tcpflag, skb, dev, pt, orig_dev, res_type);
        char mac[14];
        char * pbuf = (char *)skb->data;
        pbuf = pbuf - 14;
        memcpy(mac, pbuf, 14);

        if (TCPHDR_SYN == tcpflag)
        {
//            g_skb1 = skb;
            g_gcCap = true;
        }

        if (TCPHDR_ACK == tcpflag)
        {
//            g_Cap = true;
        }

        struct in_device *in_dev = __in_dev_get_rcu(skb->dev);

        printk ("\nchenshuai R sIP:%x dIp:%x sPo:%x dPo:%x tcpflag:%x skb_dev:%x dev:%x orig_dev:%x "
                "pt:%x skb_headlen:%x res_type:%x skb_dst:%x _skb_refdst:%x "
                "Eth:[%2x %2x %2x %2x %2x %2x] [%2x %2x %2x %2x %2x %2x] [%2x %2x] "
                "in_dev:%x skb_mark:%x skb:%x dev_name:%s indev_name:%s\n",
                srcIp, destIp, srcPort, destPort, tcpflag, skb->dev, dev, orig_dev, 
                pt, skb_headlen(skb), res_type, skb_dst(skb), skb->_skb_refdst, 
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mac[6], 
                mac[7], mac[8], mac[9], mac[10], mac[11], mac[12], mac[13], 
                in_dev, skb->mark, skb, skb->dev->name, in_dev->dev->name);
        }
    }

    int ret = (*pfn_ip_rcv)(skb, dev, pt, orig_dev);

    return ret;
}



//////////////////////////
// nat
int g_count8 = 0;
typedef const struct nf_nat_l3proto *(*fn__nf_nat_l3proto_find)(u8 l3proto);
fn__nf_nat_l3proto_find pfn__nf_nat_l3proto_find;


typedef bool (*fn_nf_ct_invert_tuplepr)(struct nf_conntrack_tuple *inverse,
                                        const struct nf_conntrack_tuple *orig);
fn_nf_ct_invert_tuplepr pfn_nf_ct_invert_tuplepr;

unsigned int chenshuai_mock_nf_nat_packet(struct nf_conn *ct,
               enum ip_conntrack_info ctinfo,
               unsigned int hooknum,
               struct sk_buff *skb)
{
    const struct nf_nat_l3proto *l3proto;
    const struct nf_nat_l4proto *l4proto;
    enum ip_conntrack_dir dir = CTINFO2DIR(ctinfo);
    unsigned long statusbit;
    enum nf_nat_manip_type mtype = HOOK2MANIP(hooknum);

    if (mtype == NF_NAT_MANIP_SRC)
        statusbit = IPS_SRC_NAT;
    else
        statusbit = IPS_DST_NAT;

    /* Invert if this is reply dir. */
    if (dir == IP_CT_DIR_REPLY)
        statusbit ^= IPS_NAT_MASK;

    /* Non-atomic: these bits don't change. */
    if (ct->status & statusbit) {
        struct nf_conntrack_tuple target;

        /* We are aiming to look like inverse of other direction. */
        //nf_ct_invert_tuplepr(&target, &ct->tuplehash[!dir].tuple);
        (*pfn_nf_ct_invert_tuplepr)(&target, &ct->tuplehash[!dir].tuple);

        //l3proto = __nf_nat_l3proto_find(target.src.l3num);
        l3proto = (*pfn__nf_nat_l3proto_find)(target.src.l3num);
        l4proto = __nf_nat_l4proto_find(target.src.l3num,
                        target.dst.protonum);
        if (!l3proto->manip_pkt(skb, 0, l4proto, &target, mtype))
            return NF_DROP;
    }
    return NF_ACCEPT;
}

typedef struct nf_conn_nat *(*fn_nf_ct_nat_ext_add)(struct nf_conn *ct);
fn_nf_ct_nat_ext_add pfn_nf_ct_nat_ext_add;

typedef unsigned int (*fn_nf_nat_alloc_null_binding)(struct nf_conn *ct, unsigned int hooknum);
fn_nf_nat_alloc_null_binding pfn_nf_nat_alloc_null_binding;

unsigned int
chenshuai_mock_nf_nat_ipv4_fn(const struct nf_hook_ops *ops, struct sk_buff *skb,
                              const struct net_device *in, const struct net_device *out,
                              unsigned int (*do_chain)(const struct nf_hook_ops *ops,
                                                       struct sk_buff *skb,
                                                       const struct net_device *in,
                                                       const struct net_device *out,
                                                       struct nf_conn *ct))
{
        struct nf_conn *ct;
        enum ip_conntrack_info ctinfo;
        struct nf_conn_nat *nat;
        /* maniptype == SRC for postrouting. */
        enum nf_nat_manip_type maniptype = HOOK2MANIP(ops->hooknum);

        /* We never see fragments: conntrack defrags on pre-routing
         * and local-out, and nf_nat_out protects post-routing.
         */
        NF_CT_ASSERT(!ip_is_fragment(ip_hdr(skb)));

        ct = nf_ct_get(skb, &ctinfo);
        /* Can't track?  It's not due to stress, or conntrack would
         * have dropped it.  Hence it's the user's responsibilty to
         * packet filter it out, or implement conntrack/NAT for that
         * protocol. 8) --RR
         */

        if (g_bRcvCap)
        {
            struct iphdr *iph;
            iph = ip_hdr(skb);
            printk("\nchenshuai_mock_nf_nat_ipv4_fn before sIp:%x dIp:%x skb:%x ct:%x\n",
                   iph->saddr, iph->daddr, skb, ct);
            g_count8 ++;
        }

        if (!ct)
        {
            if (g_bRcvCap)
            {
                struct iphdr *iph;
                iph = ip_hdr(skb);
                printk("chenshuai_mock_nf_nat_ipv4_fn before1 sIp:%x dIp:%x skb:%x ct:%x\n",
                       iph->saddr, iph->daddr, skb, ct);
            }
            return NF_ACCEPT;
        }

        /* Don't try to NAT if this packet is not conntracked */
        if (nf_ct_is_untracked(ct)) // 头文件中
        {
            if (g_bRcvCap)
            {
                struct iphdr *iph;
                iph = ip_hdr(skb);
                printk("chenshuai_mock_nf_nat_ipv4_fn before2 sIp:%x dIp:%x skb:%x \n",
                       iph->saddr, iph->daddr, skb);
            }
            return NF_ACCEPT;
        }

        //nat = nf_ct_nat_ext_add(ct);
        nat = (*pfn_nf_ct_nat_ext_add)(ct);
        if (nat == NULL)
        {
            if (g_bRcvCap)
            {
                struct iphdr *iph;
                iph = ip_hdr(skb);
                printk("chenshuai_mock_nf_nat_ipv4_fn before sIp:%x dIp:%x skb:%x nat:%x\n",
                       iph->saddr, iph->daddr, skb, nat);
            }
            return NF_ACCEPT;
        }

        switch (ctinfo) {
        case IP_CT_RELATED:
        case IP_CT_RELATED_REPLY:
                if (ip_hdr(skb)->protocol == IPPROTO_ICMP) {
                        if (!nf_nat_icmp_reply_translation(skb, ct, ctinfo,
                                                           ops->hooknum))
                                return NF_DROP;
                        else
                                return NF_ACCEPT;
                }
                /* Fall thru... (Only ICMPs can be IP_CT_IS_REPLY) */         
        case IP_CT_NEW:
                /* Seen it before?  This can happen for loopback, retrans,
                 * or local packets.
                 */
                if (!nf_nat_initialized(ct, maniptype)) {   // 头文件中
                        unsigned int ret;

                        ret = do_chain(ops, skb, in, out, ct);
                        if (ret != NF_ACCEPT)
                                return ret;

                        if (nf_nat_initialized(ct, HOOK2MANIP(ops->hooknum)))   // 头文件中
                                break;

                        //ret = nf_nat_alloc_null_binding(ct, ops->hooknum);
                        ret = (*pfn_nf_nat_alloc_null_binding)(ct, ops->hooknum);
                        if (ret != NF_ACCEPT)
                                return ret;
                } else {
//                        pr_debug("Already setup manip %s for ct %p\n",
//                                 maniptype == NF_NAT_MANIP_SRC ? "SRC" : "DST",
//                                 ct);
                        if (nf_nat_oif_changed(ops->hooknum, ctinfo, nat, out))  // 头文件中
                                goto oif_changed;
                }
                break;

        default:
                /* ESTABLISHED */
                NF_CT_ASSERT(ctinfo == IP_CT_ESTABLISHED ||
                             ctinfo == IP_CT_ESTABLISHED_REPLY);
                if (nf_nat_oif_changed(ops->hooknum, ctinfo, nat, out))  // 头文件中
                        goto oif_changed;
        }

        if (g_bRcvCap)
        {
            struct iphdr *iph;
            iph = ip_hdr(skb);
            printk("chenshuai_mock_nf_nat_ipv4_fn after1 sIp:%x dIp:%x skb:%x\n",
                   iph->saddr, iph->daddr, skb);
        }
        //return nf_nat_packet(ct, ctinfo, ops->hooknum, skb);
        unsigned int ret = chenshuai_mock_nf_nat_packet(ct, ctinfo, ops->hooknum, skb);
        if (g_bRcvCap)
        {
            struct iphdr *iph;
            iph = ip_hdr(skb);
            printk("chenshuai_mock_nf_nat_ipv4_fn after2 sIp:%x dIp:%x skb:%x\n",
                   iph->saddr, iph->daddr, skb);
        }

        return ret;

oif_changed:
        nf_ct_kill_acct(ct, ctinfo, skb);
        return NF_DROP;
}                   

unsigned int
chenshuai_mock_nf_nat_ipv4_in(const struct nf_hook_ops *ops, struct sk_buff *skb,
                              const struct net_device *in, const struct net_device *out,
                              unsigned int (*do_chain)(const struct nf_hook_ops *ops,
                                                       struct sk_buff *skb,
                                                       const struct net_device *in,
                                                       const struct net_device *out,
                                                       struct nf_conn *ct))
{
        unsigned int ret;
        __be32 daddr = ip_hdr(skb)->daddr;

        //ret = nf_nat_ipv4_fn(ops, skb, in, out, do_chain);
        ret = chenshuai_mock_nf_nat_ipv4_fn(ops, skb, in, out, do_chain);
        if (ret != NF_DROP && ret != NF_STOLEN &&
            daddr != ip_hdr(skb)->daddr)
                skb_dst_drop(skb);

        return ret;
}

typedef unsigned int
(*fn_nf_nat_ipv4_in)(const struct nf_hook_ops *ops, struct sk_buff *skb,
                              const struct net_device *in, const struct net_device *out,
                              unsigned int (*do_chain)(const struct nf_hook_ops *ops,
                                                       struct sk_buff *skb,
                                                       const struct net_device *in,
                                                       const struct net_device *out,
                                                       struct nf_conn *ct));
fn_nf_nat_ipv4_in pfn_nf_nat_ipv4_in;

typedef unsigned int (*fn_iptable_nat_do_chain)(const struct nf_hook_ops *ops,
                                                struct sk_buff *skb,
                                                const struct net_device *in,
                                                const struct net_device *out,
                                                struct nf_conn *ct);
fn_iptable_nat_do_chain pfn_iptable_nat_do_chain;

unsigned int chenshuai_mock_iptable_nat_ipv4_in(const struct nf_hook_ops *ops,
                                        struct sk_buff *skb,
                                        const struct net_device *in,
                                        const struct net_device *out,
                                        int (*okfn)(struct sk_buff *))
{
    //return nf_nat_ipv4_in(ops, skb, in, out, iptable_nat_do_chain);
    //return pfn_nf_nat_ipv4_in(ops, skb, in, out, pfn_iptable_nat_do_chain);
    return chenshuai_mock_nf_nat_ipv4_in(ops, skb, in, out, pfn_iptable_nat_do_chain);
}

typedef unsigned int (*fn_iptable_nat_ipv4_in)(const struct nf_hook_ops *ops,
                                               struct sk_buff *skb,
                                               const struct net_device *in,
                                               const struct net_device *out,
                                               int (*okfn)(struct sk_buff *));
fn_iptable_nat_ipv4_in pfn_iptable_nat_ipv4_in;


unsigned int chenshuai_iptable_nat_ipv4_in(const struct nf_hook_ops *ops,
                                           struct sk_buff *skb,
                                           const struct net_device *in,
                                           const struct net_device *out,
                                           int (*okfn)(struct sk_buff *))
{
    unsigned int ret;
    
    if (g_bRcvCap)
    {
        struct iphdr *iph;
        iph = ip_hdr(skb);
        printk ("chenshuai_iptable_nat_ipv4_in before skb:%x sIp:%x dIp:%x\n", skb, iph->saddr, iph->daddr);
        //g_count8 ++;
    }
    ret = (*pfn_iptable_nat_ipv4_in)(ops, skb, in, out, okfn);

    if (g_bRcvCap)
    {
        struct iphdr *iph;
        iph = ip_hdr(skb);
        printk ("chenshuai_iptable_nat_ipv4_in after skb:%x sIp:%x dIp:%x\n", skb, iph->saddr, iph->daddr);
    }

    return ret;
}

//////////////////////////////////
// conntrack 
int g_count9 = 0;
bool
chenshuai_mock_nf_ct_invert_tuple(struct nf_conntrack_tuple *inverse,
           const struct nf_conntrack_tuple *orig,
           const struct nf_conntrack_l3proto *l3proto,
           const struct nf_conntrack_l4proto *l4proto)
{
    memset(inverse, 0, sizeof(*inverse));

    inverse->src.l3num = orig->src.l3num;
    if (l3proto->invert_tuple(inverse, orig) == 0)
        return false;

    inverse->dst.dir = !orig->dst.dir;

    inverse->dst.protonum = orig->dst.protonum;
    return l4proto->invert_tuple(inverse, orig);
}

void chenshuai_mock_nf_ct_add_to_unconfirmed_list(struct nf_conn *ct)
{
    struct ct_pcpu *pcpu;

    /* add this conntrack to the (per cpu) unconfirmed list */
    ct->cpu = smp_processor_id();
    pcpu = per_cpu_ptr(nf_ct_net(ct)->ct.pcpu_lists, ct->cpu);

    spin_lock(&pcpu->lock);
    hlist_nulls_add_head(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode,
                 &pcpu->unconfirmed);
    spin_unlock(&pcpu->lock);
}

typedef void (*fn_nf_ct_expect_put)(struct nf_conntrack_expect *exp);
fn_nf_ct_expect_put pfn_nf_ct_expect_put;

typedef struct nf_conn *
(*fn__nf_conntrack_alloc)(struct net *net, u16 zone,
             const struct nf_conntrack_tuple *orig,
             const struct nf_conntrack_tuple *repl,
             gfp_t gfp, u32 hash);
fn__nf_conntrack_alloc pfn__nf_conntrack_alloc;


typedef void (*fn_nf_conntrack_free)(struct nf_conn *ct);
fn_nf_conntrack_free pfn_nf_conntrack_free;


typedef struct nf_conntrack_expect *
(*fn_nf_ct_find_expectation)(struct net *net, u16 zone,
               const struct nf_conntrack_tuple *tuple);
fn_nf_ct_find_expectation pfn_nf_ct_find_expectation;


typedef int (*fn__nf_ct_try_assign_helper)(struct nf_conn *ct, struct nf_conn *tmpl, gfp_t flags);
fn__nf_ct_try_assign_helper pfn__nf_ct_try_assign_helper;


struct nf_conntrack_tuple_hash *
chenshuai_mock_init_conntrack(struct net *net, struct nf_conn *tmpl,
           const struct nf_conntrack_tuple *tuple,
           struct nf_conntrack_l3proto *l3proto,
           struct nf_conntrack_l4proto *l4proto,
           struct sk_buff *skb,
           unsigned int dataoff, u32 hash)
{
    struct nf_conn *ct;
    struct nf_conn_help *help;
    struct nf_conntrack_tuple repl_tuple;
    struct nf_conntrack_ecache *ecache;
    struct nf_conntrack_expect *exp = NULL;
    u16 zone = tmpl ? nf_ct_zone(tmpl) : NF_CT_DEFAULT_ZONE;
    struct nf_conn_timeout *timeout_ext;
    unsigned int *timeouts;

    //if (!nf_ct_invert_tuple(&repl_tuple, tuple, l3proto, l4proto)) {
    if (!chenshuai_mock_nf_ct_invert_tuple(&repl_tuple, tuple, l3proto, l4proto)) {
        //pr_debug("Can't invert tuple.\n");
        return NULL;
    }

    //ct = __nf_conntrack_alloc(net, zone, tuple, &repl_tuple, GFP_ATOMIC,
    ct = (*pfn__nf_conntrack_alloc)(net, zone, tuple, &repl_tuple, GFP_ATOMIC,
                  hash);
    if (IS_ERR(ct))
        return (struct nf_conntrack_tuple_hash *)ct;

    if (tmpl && nfct_synproxy(tmpl)) {
        nfct_seqadj_ext_add(ct);
        nfct_synproxy_ext_add(ct);
    }

    timeout_ext = tmpl ? nf_ct_timeout_find(tmpl) : NULL;
    if (timeout_ext)
        timeouts = NF_CT_TIMEOUT_EXT_DATA(timeout_ext);
    else
        timeouts = l4proto->get_timeouts(net);

    if (!l4proto->new(ct, skb, dataoff, timeouts)) {
        //nf_conntrack_free(ct);
        (*pfn_nf_conntrack_free)(ct);
        //pr_debug("init conntrack: can't track with proto module\n");
        return NULL;
    }

    if (timeout_ext)
        nf_ct_timeout_ext_add(ct, timeout_ext->timeout, GFP_ATOMIC);

    nf_ct_acct_ext_add(ct, GFP_ATOMIC);
    nf_ct_tstamp_ext_add(ct, GFP_ATOMIC);
    nf_ct_labels_ext_add(ct);

    ecache = tmpl ? nf_ct_ecache_find(tmpl) : NULL;
    nf_ct_ecache_ext_add(ct, ecache ? ecache->ctmask : 0,
                 ecache ? ecache->expmask : 0,
                 GFP_ATOMIC);

    local_bh_disable();
    if (net->ct.expect_count) 
    {
        spin_lock(&nf_conntrack_expect_lock);
        //exp = nf_ct_find_expectation(net, zone, tuple);
        exp = (*pfn_nf_ct_find_expectation)(net, zone, tuple);
        if (exp) 
        {
            //pr_debug("conntrack: expectation arrives ct=%p exp=%p\n",
            //     ct, exp);
            /* Welcome, Mr. Bond.  We've been expecting you... */
            __set_bit(IPS_EXPECTED_BIT, &ct->status);
            /* exp->master safe, refcnt bumped in nf_ct_find_expectation */
            ct->master = exp->master;
            if (exp->helper) 
            {
                help = nf_ct_helper_ext_add(ct, exp->helper,
                                GFP_ATOMIC);
                if (help)
                    rcu_assign_pointer(help->helper, exp->helper);
            }

#ifdef CONFIG_NF_CONNTRACK_MARK
            ct->mark = exp->master->mark;
#endif
#ifdef CONFIG_NF_CONNTRACK_SECMARK
            ct->secmark = exp->master->secmark;
#endif
            NF_CT_STAT_INC(net, expect_new);
        }
        spin_unlock(&nf_conntrack_expect_lock);
    }
    
    if (!exp) 
    {
        //__nf_ct_try_assign_helper(ct, tmpl, GFP_ATOMIC);
        (*pfn__nf_ct_try_assign_helper)(ct, tmpl, GFP_ATOMIC);
        NF_CT_STAT_INC(net, new);
    }

    /* Now it is inserted into the unconfirmed list, bump refcount */
    nf_conntrack_get(&ct->ct_general); // 头文件
    //nf_ct_add_to_unconfirmed_list(ct); 
    chenshuai_mock_nf_ct_add_to_unconfirmed_list(ct); 

    local_bh_enable(); // 头文件

    if (exp) {
        if (exp->expectfn)
            exp->expectfn(ct, exp);
        //nf_ct_expect_put(exp);
        (*pfn_nf_ct_expect_put)(exp);
    }

    return &ct->tuplehash[IP_CT_DIR_ORIGINAL];
}


typedef u32 (*fn_hash_conntrack_raw)(const struct nf_conntrack_tuple *tuple, u16 zone);
fn_hash_conntrack_raw pfn_hash_conntrack_raw;

typedef struct nf_conntrack_tuple_hash * (*fn__nf_conntrack_find_get)(struct net *net, u16 zone,
                                          const struct nf_conntrack_tuple *tuple, u32 hash);
fn__nf_conntrack_find_get pfn__nf_conntrack_find_get;

struct nf_conn *
chenshuai_mock_resolve_normal_ct(struct net *net, struct nf_conn *tmpl,
          struct sk_buff *skb,
          unsigned int dataoff,
          u_int16_t l3num,
          u_int8_t protonum,
          struct nf_conntrack_l3proto *l3proto,
          struct nf_conntrack_l4proto *l4proto,
          int *set_reply,
          enum ip_conntrack_info *ctinfo)
{
    struct nf_conntrack_tuple tuple;
    struct nf_conntrack_tuple_hash *h;
    struct nf_conn *ct;
    u16 zone = tmpl ? nf_ct_zone(tmpl) : NF_CT_DEFAULT_ZONE;
    u32 hash;

    if (!nf_ct_get_tuple(skb, skb_network_offset(skb),
                 dataoff, l3num, protonum, &tuple, l3proto,
                 l4proto)) {
        pr_debug("resolve_normal_ct: Can't get tuple\n");
        return NULL;
    }

    /* look for tuple match */
    //hash = hash_conntrack_raw(&tuple, zone);
    hash = (*pfn_hash_conntrack_raw)(&tuple, zone);
    //h = __nf_conntrack_find_get(net, zone, &tuple, hash);
    h = (*pfn__nf_conntrack_find_get)(net, zone, &tuple, hash);
    if (!h) 
    {
        //h = init_conntrack(net, tmpl, &tuple, l3proto, l4proto,
        h = chenshuai_mock_init_conntrack(net, tmpl, &tuple, l3proto, l4proto,
                                          skb, dataoff, hash);
        if (!h)
            return NULL;
        if (IS_ERR(h))
            return (void *)h;
    }
    ct = nf_ct_tuplehash_to_ctrack(h);

    /* It exists; we have (non-exclusive) reference. */
    if (NF_CT_DIRECTION(h) == IP_CT_DIR_REPLY) 
    {
        *ctinfo = IP_CT_ESTABLISHED_REPLY;
        /* Please set reply bit if this packet OK */
        *set_reply = 1;
    } 
    else 
    {
        /* Once we've had two way comms, always ESTABLISHED. */
        if (test_bit(IPS_SEEN_REPLY_BIT, &ct->status)) 
        {
            //pr_debug("nf_conntrack_in: normal packet for %p\n", ct);
            *ctinfo = IP_CT_ESTABLISHED;
        }
        else if (test_bit(IPS_EXPECTED_BIT, &ct->status)) 
        {
            //pr_debug("nf_conntrack_in: related packet for %p\n",
            //     ct);
            *ctinfo = IP_CT_RELATED;
        } 
        else 
        {
            //pr_debug("nf_conntrack_in: new packet for %p\n", ct);
            *ctinfo = IP_CT_NEW;
        }
        *set_reply = 0;
    }
    skb->nfct = &ct->ct_general;
    skb->nfctinfo = *ctinfo;
    return ct;
}

enum tcp_bit_set {
    TCP_SYN_SET,
    TCP_SYNACK_SET,
    TCP_FIN_SET,
    TCP_ACK_SET,
    TCP_RST_SET,
    TCP_NONE_SET,
};

unsigned int get_conntrack_index(const struct tcphdr *tcph)
{
    if (tcph->rst) return TCP_RST_SET;
    else if (tcph->syn) return (tcph->ack ? TCP_SYNACK_SET : TCP_SYN_SET);
    else if (tcph->fin) return TCP_FIN_SET;
    else if (tcph->ack) return TCP_ACK_SET;
    else return TCP_NONE_SET;
}

unsigned int
chenshuai_mock_nf_conntrack_in(struct net *net, u_int8_t pf, unsigned int hooknum,
                               struct sk_buff *skb)
{
    struct nf_conn *ct, *tmpl = NULL;
    enum ip_conntrack_info ctinfo;
    struct nf_conntrack_l3proto *l3proto;
    struct nf_conntrack_l4proto *l4proto;
    unsigned int *timeouts;
    unsigned int dataoff;
    u_int8_t protonum;
    int set_reply = 0;
    int ret;

/////////////////////
    struct iphdr *iph;

    if (g_bRcvCap)
    {
        iph = ip_hdr(skb);
        printk ("\nchenshuai_mock_nf_conntrack_in before sIp:%x dIp:%x skb:%x skb_dev:%x nfct:%x\n",
                iph->saddr, iph->daddr, skb, skb->dev, skb->nfct);
        
    }

    if (skb->nfct) 
    {
        /* Previously seen (loopback or untracked)?  Ignore. */
        tmpl = (struct nf_conn *)skb->nfct;
        if (!nf_ct_is_template(tmpl)) {
            //NF_CT_STAT_INC_ATOMIC(net, ignore);
            return NF_ACCEPT;
        }
        skb->nfct = NULL;
    }

    /* rcu_read_lock()ed by nf_hook_slow */
    l3proto = __nf_ct_l3proto_find(pf);
    ret = l3proto->get_l4proto(skb, skb_network_offset(skb),
                   &dataoff, &protonum);
    if (ret <= 0) 
    {
        //pr_debug("not prepared to track yet or error occurred\n");
        //NF_CT_STAT_INC_ATOMIC(net, error);
        //NF_CT_STAT_INC_ATOMIC(net, invalid);
        ret = -ret;
        goto out;
    }

    if (g_bRcvCap)
    {
        iph = ip_hdr(skb);
        printk ("chenshuai_mock_nf_conntrack_in after1 sIp:%x dIp:%x skb:%x skb_dev:%x nfct:%x\n",
                iph->saddr, iph->daddr, skb, skb->dev, skb->nfct);
    }

    l4proto = __nf_ct_l4proto_find(pf, protonum);

    /* It may be an special packet, error, unclean...
     * inverse of the return code tells to the netfilter
     * core what to do with the packet. */
    if (l4proto->error != NULL) 
    {
        ret = l4proto->error(net, tmpl, skb, dataoff, &ctinfo,
                     pf, hooknum);
        if (ret <= 0) 
        {
            //NF_CT_STAT_INC_ATOMIC(net, error);
            //NF_CT_STAT_INC_ATOMIC(net, invalid);
            ret = -ret;
            goto out;
        }
        /* ICMP[v6] protocol trackers may assign one conntrack. */
        if (skb->nfct)
            goto out;
    }

    ct = chenshuai_mock_resolve_normal_ct(net, tmpl, skb, dataoff, pf, protonum,
                                          l3proto, l4proto, &set_reply, &ctinfo);
    if (!ct) 
    {
        /* Not valid part of a connection */
        //NF_CT_STAT_INC_ATOMIC(net, invalid);
        printk("chenshuai_mock_nf_conntrack_in !ct:%x\n", ct);
        ret = NF_ACCEPT;
        goto out;
    }

    if (IS_ERR(ct)) 
    {
        /* Too stressed to deal. */
        //NF_CT_STAT_INC_ATOMIC(net, drop);
        printk("chenshuai_mock_nf_conntrack_in ct err:%x\n", IS_ERR(ct));
        ret = NF_DROP;
        goto out;
    }

    NF_CT_ASSERT(skb->nfct);

    /* Decide what timeout policy we want to apply to this flow. */
    timeouts = nf_ct_timeout_lookup(net, ct, l4proto);

    if (g_bRcvCap)
    {
        struct tcphdr _tcph;
        enum ip_conntrack_dir dir = CTINFO2DIR(ctinfo);
        struct tcphdr *th = skb_header_pointer(skb, dataoff, sizeof(_tcph), &_tcph);
        unsigned int index = get_conntrack_index(th);
        iph = ip_hdr(skb);
        printk ("chenshuai_mock_nf_conntrack_in l4proto->packet:%x sIp:%x dIp:%x skb:%x old_state:%x dir:%x index:%x "
                "nfct_synproxy:%x last_dir:%x td_end:%x seq:%x\n",
                l4proto->packet, iph->saddr, iph->daddr, skb, ct->proto.tcp.state, dir, index,
                nfct_synproxy(ct), ct->proto.tcp.last_dir, ct->proto.tcp.seen[dir].td_end, ntohl(th->seq));
    }

    ret = l4proto->packet(ct, skb, dataoff, ctinfo, pf, hooknum, timeouts);
    
    if (ret <= 0) 
    {
        /* Invalid: inverse of the return code tells
         * the netfilter core what to do */
        //pr_debug("nf_conntrack_in: Can't track with proto module\n");
        nf_conntrack_put(skb->nfct);
        skb->nfct = NULL;
        //NF_CT_STAT_INC_ATOMIC(net, invalid);
        if (ret == -NF_DROP)
        {
            //NF_CT_STAT_INC_ATOMIC(net, drop);
        }

        ret = -ret;
        
        if (g_bRcvCap)
        {
            iph = ip_hdr(skb);
            printk ("chenshuai_mock_nf_conntrack_in l4proto->packet:%x error ret:%x sIp:%x dIp:%x "
                    "skb:%x skb_dev:%x nfct:%x ct:%x tmpl:%x\n",
                    l4proto->packet, ret, iph->saddr, iph->daddr, skb, skb->dev, skb->nfct, ct, tmpl);
        }

        goto out;
    }

    if (g_bRcvCap)
    {
        iph = ip_hdr(skb);
        printk ("chenshuai_mock_nf_conntrack_in after4 sIp:%x dIp:%x skb:%x skb_dev:%x nfct:%x ct:%x tmpl:%x\n",
                iph->saddr, iph->daddr, skb, skb->dev, skb->nfct, ct, tmpl);
    }

    if (set_reply && !test_and_set_bit(IPS_SEEN_REPLY_BIT, &ct->status))
        nf_conntrack_event_cache(IPCT_REPLY, ct);
out:
    if (tmpl) 
    {
        /* Special case: we have to repeat this hook, assign the
         * template again to this packet. We assume that this packet
         * has no conntrack assigned. This is used by nf_ct_tcp. */
        if (ret == NF_REPEAT)
            skb->nfct = (struct nf_conntrack *)tmpl;
        else
            nf_ct_put(tmpl);
    }
    if (g_bRcvCap)
    {
        iph = ip_hdr(skb);
        printk ("chenshuai_mock_nf_conntrack_in after sIp:%x dIp:%x skb:%x skb_dev:%x nfct:%x\n\n",
                iph->saddr, iph->daddr, skb, skb->dev, skb->nfct);
    }

    return ret;
}

unsigned int chenshuai_mock_ipv4_conntrack_in(const struct nf_hook_ops *ops,
                                              struct sk_buff *skb,
                                              const struct net_device *in,
                                              const struct net_device *out,
                                              int (*okfn)(struct sk_buff *))
{
    return chenshuai_mock_nf_conntrack_in(dev_net(in), PF_INET, ops->hooknum, skb);
}


struct nf_hook_ops * p_nf_nat_ipv4_ops;
struct nf_hook_ops * p_ipv4_conntrack_ops;

void replace_ip_rcv(void)
{

    pfn__nf_ct_try_assign_helper = (fn__nf_ct_try_assign_helper)kallsyms_lookup_name("__nf_ct_try_assign_helper");
    pfn_nf_ct_find_expectation = (fn_nf_ct_find_expectation)kallsyms_lookup_name("nf_ct_find_expectation");
    pfn__nf_conntrack_alloc = (fn__nf_conntrack_alloc)kallsyms_lookup_name("__nf_conntrack_alloc");
    pfn_nf_ct_expect_put = (fn_nf_ct_expect_put)kallsyms_lookup_name("nf_ct_expect_put");
    pfn__nf_conntrack_find_get = (fn__nf_conntrack_find_get)kallsyms_lookup_name("__nf_conntrack_find_get");
    pfn_hash_conntrack_raw = (fn_hash_conntrack_raw)kallsyms_lookup_name("hash_conntrack_raw");
    p_ipv4_conntrack_ops = (struct nf_hook_ops *)kallsyms_lookup_name("ipv4_conntrack_ops");
    pfn__nf_nat_l3proto_find = (fn__nf_nat_l3proto_find)kallsyms_lookup_name("__nf_nat_l3proto_find");
    pfn_nf_ct_invert_tuplepr = (fn_nf_ct_invert_tuplepr)kallsyms_lookup_name("nf_ct_invert_tuplepr");
    pfn_nf_nat_alloc_null_binding = (fn_nf_nat_alloc_null_binding)kallsyms_lookup_name("nf_nat_alloc_null_binding");
    pfn_nf_ct_nat_ext_add = (fn_nf_ct_nat_ext_add)kallsyms_lookup_name("nf_ct_nat_ext_add");
    pfn_nf_nat_ipv4_in = (fn_nf_nat_ipv4_in)kallsyms_lookup_name("nf_nat_ipv4_in");
    pfn_iptable_nat_do_chain = (fn_iptable_nat_do_chain)kallsyms_lookup_name("iptable_nat_do_chain");
    p_nf_nat_ipv4_ops = (struct nf_hook_ops *)kallsyms_lookup_name("nf_nat_ipv4_ops");
    //printk ("chenshuai p_nf_nat_ipv4_ops:%x\n", p_nf_nat_ipv4_ops);
    pfn_iptable_nat_ipv4_in = (fn_iptable_nat_ipv4_in)kallsyms_lookup_name("iptable_nat_ipv4_in");
    pfn_nf_queue = (fn_nf_queue)kallsyms_lookup_name("nf_queue");
    pfn_nf_hook_slow = (fn_nf_hook_slow)kallsyms_lookup_name("nf_hook_slow");
    pfn_ip_forward = (fn_input)kallsyms_lookup_name("ip_forward");
    pfn_ip_output = (fn_output)kallsyms_lookup_name("ip_output");
    
    pfn_ip_local_deliver = (fn_input)kallsyms_lookup_name("ip_local_deliver");
    pfn_ip_rt_bug = (fn_output)kallsyms_lookup_name("ip_rt_bug");

    pfn_ip_error = (fn_input)kallsyms_lookup_name("ip_error");

    p_fnhe_lock = kallsyms_lookup_name("fnhe_lock");

    pfn_inet_addr_onlink = (fn_inet_addr_onlink)kallsyms_lookup_name("inet_addr_onlink");

    pfn_rt_add_uncached_list = (fn_rt_add_uncached_list)kallsyms_lookup_name("rt_add_uncached_list");
    pfn_find_exception = (fn_find_exception)kallsyms_lookup_name("find_exception");
    pfn_fib_select_multipath = (fn_fib_select_multipath)kallsyms_lookup_name("fib_select_multipath");
    pfn_rt_cache_route = (fn_rt_cache_route)kallsyms_lookup_name("rt_cache_route");
    pfn_rt_dst_alloc = (fn_rt_dst_alloc)kallsyms_lookup_name("rt_dst_alloc");
    pfn_fib_validate_source = (fn_fib_validate_source)kallsyms_lookup_name("fib_validate_source");
    pfn_ip_route_input_noref = (fn_ip_route_input_noref)kallsyms_lookup_name("ip_route_input_noref");
    pfn_ip_rcv_finish = (fn_ip_rcv_finish)kallsyms_lookup_name("ip_rcv_finish");
    p_ip_rt_acct = kallsyms_lookup_name("ip_rt_acct");
    p_sysctl_ip_early_demux = (int *)kallsyms_lookup_name("sysctl_ip_early_demux");
    pp_inet_protos = (struct net_protocol **)kallsyms_lookup_name("inet_protos");

    struct net_protocol * tcpipprot = rcu_dereference(pp_inet_protos[IPPROTO_TCP]);
    printk ("chenshuai pfn__nf_ct_try_assign_helper:%x\n"
            "          pfn_nf_ct_find_expectation:%x\n"
            "          pfn__nf_conntrack_alloc:%x\n"
            "          pfn_nf_ct_expect_put:%x\n"
            "          pfn__nf_conntrack_find_get:%x\n"
            "          pfn_hash_conntrack_raw:%x\n"
            "          p_ipv4_conntrack_ops:%x %x\n"
            "          pfn__nf_nat_l3proto_find:%x\n"
            "          pfn_nf_ct_invert_tuplepr:%x\n"
            "          pfn_nf_nat_alloc_null_binding:%x\n"
            "          pfn_nf_ct_nat_ext_add:%x\n"
            "          pfn_nf_nat_ipv4_in:%x\n"
            "          pfn_iptable_nat_do_chain:%x\n"
            "          p_nf_nat_ipv4_ops:%x hook:%x\n"
            "          pfn_iptable_nat_ipv4_in:%x\n"
            "          pfn_nf_queue:%x\n"
            "          pfn_nf_hook_slow:%x\n"
            "          pfn_ip_forward:%x\n"
            "          pfn_ip_output:%x\n"
            "          pfn_ip_local_deliver:%x\n"
            "          pfn_ip_rt_bug:%x\n"
            "          pfn_ip_error:%x\n"
            "          p_fnhe_lock:%x\n"
            "          pfn_inet_addr_onlink:%x\n"
            "          pfn_rt_add_uncached_list:%x\n"
            "          pfn_find_exception:%x\n"
            "          pfn_fib_select_multipath:%x\n"
            "          pfn_rt_cache_route:%x\n"
            "          pfn_rt_dst_alloc:%x\n"
            "          pfn_fib_validate_source:%x\n"
            "          pfn_ip_route_input_noref:%x\n"
            "          pfn_ip_rcv_finish:%x\n"
            "          p_ip_rt_acct:%x\n"
            "          sysctl_ip_early_demux:%x %x\n"
            "          inet_protos:%x tcp_prot:%x tcp_prot:early_demux:%x\n",
            pfn__nf_ct_try_assign_helper,
            pfn_nf_ct_find_expectation,
            pfn__nf_conntrack_alloc,
            pfn_nf_ct_expect_put,
            pfn__nf_conntrack_find_get,
            pfn_hash_conntrack_raw,
            p_ipv4_conntrack_ops, p_ipv4_conntrack_ops->hook,
            pfn__nf_nat_l3proto_find,
            pfn_nf_ct_invert_tuplepr,
            pfn_nf_nat_alloc_null_binding,
            pfn_nf_ct_nat_ext_add,
            pfn_nf_nat_ipv4_in,
            pfn_iptable_nat_do_chain,
            p_nf_nat_ipv4_ops, p_nf_nat_ipv4_ops->hook,
            pfn_iptable_nat_ipv4_in,
            pfn_nf_queue,
            pfn_nf_hook_slow,
            pfn_ip_forward,
            pfn_ip_output,
            pfn_ip_local_deliver,
            pfn_ip_rt_bug,
            pfn_ip_error,
            p_fnhe_lock,
            pfn_inet_addr_onlink,
            pfn_rt_add_uncached_list,
            pfn_find_exception,
            pfn_fib_select_multipath,
            pfn_rt_cache_route,
            pfn_rt_dst_alloc,
            pfn_fib_validate_source,
            pfn_ip_route_input_noref,
            pfn_ip_rcv_finish,
            p_ip_rt_acct,
            p_sysctl_ip_early_demux, *p_sysctl_ip_early_demux,
            pp_inet_protos, tcpipprot, tcpipprot->early_demux);

    //return;

/////////////////////////////////////////////////////////////////////////////////////////

    p_ip_packet_type = (struct packet_type *)kallsyms_lookup_name("ip_packet_type");
    printk ("chenshuai ip_packet_type:%x\n", p_ip_packet_type);
    pfn_ip_rcv = kallsyms_lookup_name("ip_rcv");

// 替换 ip_rcv
//    p_ip_packet_type->func = chenshuai_ip_rcv;
    p_ip_packet_type->func = chenshuai_mock_ip_rcv;

// 替换 iptable
    //p_nf_nat_ipv4_ops->hook = chenshuai_iptable_nat_ipv4_in;
    p_nf_nat_ipv4_ops->hook = chenshuai_mock_iptable_nat_ipv4_in;

// 替换 conntrack
    p_ipv4_conntrack_ops->hook = chenshuai_mock_ipv4_conntrack_in;
    printk ("chenshuai ip_rcv replace:%x\n", p_ip_packet_type->func);


/////////////////////////////////////////////////////////////////////////////////////////

}

void recover_ip_rcv(void)
{
    //return;
    p_ipv4_conntrack_ops->hook = kallsyms_lookup_name("ipv4_conntrack_in");
    p_nf_nat_ipv4_ops->hook = (fn_iptable_nat_ipv4_in)kallsyms_lookup_name("iptable_nat_ipv4_in");
    p_ip_packet_type->func = pfn_ip_rcv;
    printk ("chenshuai ip_rcv recover:%x count:1:%x 2:%x 3:%x 4:%x 5:%x 6:%x 7:%x 8:%x 9:%x\n", 
             p_ip_packet_type->func, 
             g_count1, g_count2, g_count3, g_count4, g_count5, g_count6, g_count7, g_count8, g_count9);
}

int chenshuai_fib_lookup(struct sk_buff *skb) 
{
    struct net_device *dev = skb->dev;
    struct net * net = dev_net(dev);
    struct iphdr *iph = ip_hdr(skb);

    struct fib_result res;
    struct flowi4   fl4;

    int             err = 0xff;

    res.fi = NULL;
    fl4.flowi4_oif = 0;
    fl4.flowi4_iif = dev->ifindex;
    fl4.flowi4_mark = skb->mark;
    fl4.flowi4_tos = iph->tos;
    fl4.flowi4_scope = 0;
    fl4.daddr = iph->daddr;
    fl4.saddr = iph->saddr;
    err = fib_lookup(net, &fl4, &res);
    if (err != 0)
    {
        return 0xff;
    }
    return res.type;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
typedef int  (*gc)(struct dst_ops *ops);
int g_gc = 0;
int chenshuai_gc(struct dst_ops *ops)
{
    if (g_gcCap)
    {
//        dump_stack();
        g_gc ++;
    }
    return 0;
}

struct dst_ops * p_ipv4_dst_ops ;

void replace_ipv4_dst_ops(void)
{
    p_ipv4_dst_ops = (struct dst_ops *)kallsyms_lookup_name("ipv4_dst_ops");
    printk ("chenshuai ipv4_dst_ops replace ops:%x gc:%x gc_thresh:%x\n", p_ipv4_dst_ops, p_ipv4_dst_ops->gc, p_ipv4_dst_ops->gc_thresh);
    p_ipv4_dst_ops->gc = chenshuai_gc;
    p_ipv4_dst_ops->gc_thresh = 0;
    int ret = dst_entries_get_fast(p_ipv4_dst_ops);
    printk ("chenshuai dst_entries_get_fast %d %d\n", ret, dst_entries_get_fast(p_ipv4_dst_ops) > p_ipv4_dst_ops->gc_thresh);
}

void recover_ipv4_dst_ops(void)
{
    p_ipv4_dst_ops->gc = 0;
    p_ipv4_dst_ops->gc_thresh = 0xffffffff;
    printk ("chenshuai ipv4_dst_ops recover ops:%x gc:%x gc_thresh:%x count:%x\n",
            p_ipv4_dst_ops, p_ipv4_dst_ops->gc, p_ipv4_dst_ops->gc_thresh, g_gc);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/*
typedef void (*fn_tcp_v4_early_demux)(struct sk_buff *skb);
fn_tcp_v4_early_demux pfn_tcp_v4_early_demux;

int g_tcp_v4_early_demux = 0;
void chenshuai_tcp_v4_early_demux(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct tcphdr *th;
    unsigned long srcIp = 0;
    unsigned long destIp = 0;
    short srcPort = 0;
    short destPort = 0;

    char tcpflag = 0;

    bool bCap = false;

    iph = ip_hdr(skb);
    srcIp = iph->saddr;
    destIp = iph->daddr;


    if ((srcIp == filterIp) || (destIp == filterIp))
    {    
        th = tcp_hdr(skb);

        srcPort = th->source;
        srcPort = ntohs(srcPort);

        destPort = th->dest;
        destPort = ntohs(destPort);

        tcpflag = tcp_flag_byte(th);

        printk ("\nchenshuai_tcp_v4_early_demux sIp:%x dIp:%x sPo:%x dPo:%x tcpflag:%x dev:%x dev_net:%x skb_iif:%x name:%s\n",
                 srcIp, destIp, srcPort, destPort, tcpflag, skb->dev, dev_net(skb->dev), skb->skb_iif, skb->dev->name);
        g_tcp_v4_early_demux ++;
        bCap = true;
    }
    (*pfn_tcp_v4_early_demux)(skb);

    if (bCap)
    {
        if (skb_dst(skb))
        {
            printk ("\nchenshuai_tcp_v4_early_demux after dst:%x input:%x output:%x\n", 
                     skb_dst(skb), skb_dst(skb)->input, skb_dst(skb)->output);
        }
        else
        {
           printk ("\nchenshuai_tcp_v4_early_demux after dst:%x \n", skb_dst(skb));
        }
    }
}


struct net_protocol ** pp_inet_protos = NULL;

struct net_protocol * p_tcp_protocol;

struct net_protocol chenshuai_tcp_protocol;

int * p_sysctl_ip_early_demux ; 
*/
// IPPROTO_TCP
void replace_ip_protocol(void)
{
/*
    p_sysctl_ip_early_demux = (int *)kallsyms_lookup_name("sysctl_ip_early_demux"); 
    pp_inet_protos = (struct net_protocol **)kallsyms_lookup_name("inet_protos");
    struct net_protocol * tcpipprot = rcu_dereference(pp_inet_protos[IPPROTO_TCP]);
    printk ("chenshuai sysctl_ip_early_demux:%x %x inet_protos:%x tcp_prot:%x tcp_prot:early_demux:%x\n",
            p_sysctl_ip_early_demux, *p_sysctl_ip_early_demux, 
            pp_inet_protos, tcpipprot, tcpipprot->early_demux);
//////////////////////////////////////////////////////////////////

    pfn_tcp_v4_early_demux = kallsyms_lookup_name("tcp_v4_early_demux");
    chenshuai_tcp_protocol.early_demux = chenshuai_tcp_v4_early_demux;
    chenshuai_tcp_protocol.handler     = kallsyms_lookup_name("tcp_v4_rcv");
    chenshuai_tcp_protocol.err_handler = kallsyms_lookup_name("tcp_v4_err");
    chenshuai_tcp_protocol.no_policy   = 1;
    chenshuai_tcp_protocol.netns_ok    = 1;
    chenshuai_tcp_protocol.icmp_strict_tag_validation = 1;

    p_tcp_protocol = kallsyms_lookup_name("tcp_protocol");
    
    inet_del_protocol((const struct net_protocol *)p_tcp_protocol, IPPROTO_TCP);
    inet_add_protocol((const struct net_protocol *)&chenshuai_tcp_protocol, IPPROTO_TCP);
*/
}

void recover_ip_protocol(void)
{
/*
    inet_del_protocol((const struct net_protocol *)&chenshuai_tcp_protocol, IPPROTO_TCP);
    inet_add_protocol((const struct net_protocol *)p_tcp_protocol, IPPROTO_TCP);

    struct net_protocol * tcpipprot = rcu_dereference(pp_inet_protos[IPPROTO_TCP]);
    printk ("chenshuai inet_protos:%x tcp_prot:%x tcp_prot:early_demux:%x %x\n",
            pp_inet_protos, tcpipprot, tcpipprot->early_demux, g_tcp_v4_early_demux);
*/
}


module_init(hello_init);
module_exit(hello_exit);

