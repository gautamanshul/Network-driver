#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/skbuff.h>

#define RX_INTR 0x0001
#define TX_INTR 0X0002


struct net_device *net0,*net1;

struct os_packet {
	struct net_device *dev;
	int datalen;
	u8 data[ETH_DATA_LEN];
};

struct os_priv {
	struct net_device_stats stats;
	int status;
	struct os_packet *pkt;
	int rx_int_enabled;
	int tx_packetlen;
	u8 *tx_packetdata;
	struct skbuff *skb;
	spinlock_t lock;
	struct net_device *dev;
};

int os_open(struct net_device *dev) { return 0; }
int os_stop(struct net_device *dev) { return 0; }
static void os_interrupt(int irq, void *dev_id, struct pt_regs *regs) { }
static void os_hw_tx(char *buf, int len, struct net_device *dev) { }
int os_start_xmit(struct sk_buff *skb, struct net_device *dev) {
	return 0;
}

struct net_device_stats *os_stats(struct net_device *dev) {
	return &(((struct os_priv *)netdev_priv(dev))->stats);
} 

int os_header(struct sk_buff *skb, struct net_device *dev, unsigned short type, const void *daddr, const void *saddr, unsigned int len) {
	return 0;
}

static const struct header_ops os_header_ops = {
	.create = os_header,
};

static const struct net_device_ops os_device_ops = {
	.ndo_open = os_open,
	.ndo_stop = os_stop,
	.ndo_start_xmit = os_start_xmit,
	.ndo_get_stats = os_stats,
};

int os_init_mod(void) {
	struct os_priv *priv;
	int i;

	/*net0 and net1 point to net_device structs. Bypassing the PCI as we don't have a real device*/
	net0 = alloc_etherdev(sizeof(struct os_priv));
	net1 = alloc_etherdev(sizeof(struct os_priv));

	for(i = 0; i < 6; i++) net0->dev_addr[i] = (unsigned char)i;
	for(i = 0; i < 6; i++) net0->broadcast[i] = (unsigned char)15;
	net0->hard_header_len = 14;

	
	for(i = 0; i < 6; i++) net1->dev_addr[i] = (unsigned char)i;
	for(i = 0; i < 6; i++) net1->broadcast[i] = (unsigned char)15;
	net1->hard_header_len = 14;
	net1->dev_addr[5]++;

	memcpy(net0->name, "net0\0", 5); 
	memcpy(net0->name, "net1\0", 5);

	net0->netdev_ops = &os_device_ops;
	net0->header_ops = &os_header_ops;
	 
	net1->netdev_ops = &os_device_ops;
	net1->header_ops = &os_header_ops;
	
	net0->flags |= IFF_NOARP;
	net1->flags |= IFF_NOARP;

	priv = netdev_priv(net0);
	memset(priv, 0, sizeof(struct os_priv));
	priv->dev = net0;
	priv->rx_int_enabled = 1;
	priv->pkt = kmalloc(sizeof(struct os_packet), GFP_KERNEL);
	priv->pkt->dev = net0;

	
	priv = netdev_priv(net1);
	memset(priv, 0, sizeof(struct os_priv));
	priv->dev = net1;
	priv->rx_int_enabled = 1;
	priv->pkt = kmalloc(sizeof(struct os_packet), GFP_KERNEL);
	priv->pkt->dev = net1;

	if(register_netdev(net0)) {
		printk(KERN_INFO "net0 not registered\n");
	} else {
		printk(KERN_INFO "net0 registered\n");
	}

	if(register_netdev(net1)) {
		printk(KERN_INFO "net1 not registered\n");
	} else {
		printk(KERN_INFO "net1 registered\n");
	}

	return 0;
}

void os_exit_mod(void)
{
	struct os_priv *priv;
	
	if(net0) {
		priv = netdev_priv(net0);
		kfree(priv->pkt);
		unregister_netdev(net0);
	}
	
	if(net1) {
		priv = netdev_priv(net1);
		kfree(priv->pkt);
		unregister_netdev(net1);
	}
}




MODULE_LICENSE("GPL");
module_init(os_init_mod);
module_exit(os_exit_mod);
