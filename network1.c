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

int os_open(struct net_device *dev) { netif_start_queue(dev); return 0; } /*Allow upper layers to call the hard_start_xmit function*/
int os_stop(struct net_device *dev) { netif_stop_queue(dev); return 0; } /*Stop upper layers from calling the hard_start_xmit function to prevent data flow when transmit resources are unavailable*/
static void os_interrupt(int irq, void *dev_id, struct pt_regs *regs) { }
static void os_hw_tx(char *buf, int len, struct net_device *dev) { }

void os_tx_i_handler(struct net_device *dev)
{
	struct os_priv *priv;
	
	priv = netdev_priv(dev);
   	if (netif_queue_stopped(priv->pkt->dev)) netif_wake_queue(priv->pkt->dev);
}

void os_rx_i_handler(struct net_device *dev)
{
	struct os_priv *priv;
	struct sk_buff *skb;
	
	priv = netdev_priv(dev);
	int len = priv->skb->len;
	char *data = priv->skb->data;
	
	skb = dev_alloc_skb(len);
	memcpy(skb_put(skb, data, len);

	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);

	netif_rx(skb);

	if (netif_queue_stopped(priv->pkt->dev)) netif_wake_queue(priv->pkt->dev); 
}

/*Transmission of a packet is accomplished with os_start_xmit. This function is called by the OS after the header function above with the same struct sk_buff object as argument skb. Ordinarily, the next step would be to shove the packet out through the NIC. But, in this example, the packet is merely going to be looped back. The action of receiving the looped back packet through a NIC will be simulated by directly calling another function which would be the receive interrupt handler if a NIC had requested an interrupt. The looped back packet will be prepared from the one held by skb. The first step is to extract the packet and its length from skb like this*/
int os_start_xmit(struct sk_buff *skb, struct net_device *dev) {
	char *data = skb->data;
	int len = skb->len;
	struct os_priv *priv;
	
	/*The skb holds the packet data, so its needed until the handlers are called */
	priv = netdev_priv(dev);
	priv->skb = skb;

	/*The IP header needs to be changed: the source and desitnation networks need to be reversed (for example, 192.168.0.1 is reversed to 192.168.1.1 and vice versa. The IP header may be referenced like this*/
	struct iphdr *ih = (struct iphdr *)(data + sizeof(struct ethhdr));	

	/*Reversals are accomplished as follows*/
	u32 *saddr = &ih->saddr;
	u32 *daddr = &ih->daddr;

	((u8*)saddr)[2] ^= 1;
	((u8*)daddr)[2] ^= 1;
	
	/*As IP requires checking a checksum, a new one should be created like this*/
	ih->check = 0;
	ih->check = ip_fast_csum((unsigned char*)ih, ih->ihl);

	/*where ihl is the internet header lengh. In our example here, its not checked. Before calling the pseudo-receive interrupt handler, put the modified packet into the private area like this if the destination is net1*/
	if (dev->dev_addr[ETH_ALEN-1] == 5) {
		priv = netdev_priv(net1);
		priv->pkt->len = len;
		memcpy(priv->pkt->data, data, len);
		os_rx_i_handler(net1); /*We call receive interrupt handler if the destination is net1*/
		os_tx_i_handler(net0); /*Typically the NIC will generate a transmit interrupt and handling this can be simulated like this if the source is net1*/
		priv = netdev_priv(net0); 
		dev_kfree_skb(priv->skb);
	} else {
		priv = netdev_priv(net0);
		priv->pkt->len = len;
		memcpy(priv->pkt->data, data, len);
		os_rx_i_handler(net0);
		os_tx_i_handler(net1);
		priv = netdev_priv(net1);
		dev_kfree_skb(priv->skb);
	}


	return 0;
}

struct net_device_stats *os_stats(struct net_device *dev) {
	return &(((struct os_priv *)netdev_priv(dev))->stats);
} 

/*The function below receives sk_buffer from kernel when a packet is pushed onto the net0 interface*/
/*The header function places a data-link header (MAC Address) in front of the packet which currently begins with the IP header*/
int os_header(struct sk_buff *skb, struct net_device *dev, unsigned short type, const void *daddr, const void *saddr, unsigned int len) {
	struct ethhdr *eth = (struct ethhdr*)skb_push(skb, ETH_HLEN);
	
	/*The space is filled with source MAC address, destination MAC address, which come from the net_device object as argument dev, and the protocol which comes from the unsigned short object as argument type. The MAC addresses may be filled with this*/
	memcpy(eth->h_source, dev->dev_addr, dev->addr_len); 
	memcpy(eth->h_dest, eth->h_source, dev->addr_len);
	
	/*The above functions would set h_source and h_dest to the same MAC addres. Hence, we change the destination MAC address for net0 and net1*/
	eth->h_dest[ETH_ALEN-1] = (eth->h_dest[ETH_ALEN-1] == 5) ? 6 : 5;

	/*Add the protocol id using htons to meet the internet standard of data byte transmission*/
	eth->h_proto = htons(type);

	/*The return value of the os_header functions is the number of bytes in the header*/
	return dev->hard_header_len;
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
