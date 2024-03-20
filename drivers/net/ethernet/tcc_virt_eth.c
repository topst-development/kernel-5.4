/* dummy.c: a dummy net driver

	The purpose of this driver is to provide a device to point a
	route through, but not to actually transmit packets.

	Why?  If you have a machine whose only connection is an occasional
	PPP/SLIP/PLIP link, you can only connect to your own hostname
	when the link is up.  Otherwise you have to use localhost.
	This isn't very consistent.

	One solution is to set up a dummy link using PPP/SLIP/PLIP,
	but this seems (to me) too much overhead for too little gain.
	This driver provides a small alternative. Thus you can do

	[when not running slip]
		ifconfig dummy slip.addr.ess.here up
	[to go to slip]
		ifconfig dummy down
		dip whatever

	This was written by looking at Donald Becker's skeleton driver
	and the loopback driver.  I then threw away anything that didn't
	apply!	Thanks to Alan Cox for the key clue on what to do with
	misguided packets.

			Nick Holloway, 27th May 1994
	[I tweaked this explanation a little but that's all]
			Alan Cox, 30th May 1994
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/rtnetlink.h>
#include <linux/net_tstamp.h>
#include <net/rtnetlink.h>
#include <net/sock.h>
#include <linux/u64_stats_sync.h>

#include <linux/ip.h>
#include <linux/tcc_shmem.h>

#define DRV_NAME	"tcc_virt_eth"
#define DRV_VERSION	"1.0"

#undef pr_fmt
#define pr_fmt(fmt) DRV_NAME ": " fmt

static int numdummies = 1;
static int num_vfs = 1;

static int shmem_port = 0;

struct tcc_shm_callback virt_eth_callback;
int32_t virt_eth_rx(unsigned long data, char* received_data, uint32_t received_num);

struct vf_data_storage {
	u8	vf_mac[ETH_ALEN];
	u16	pf_vlan; /* When set, guest VLAN config not allowed. */
	u16	pf_qos;
	__be16	vlan_proto;
	u16	min_tx_rate;
	u16	max_tx_rate;
	u8	spoofchk_enabled;
	bool	rss_query_enabled;
	u8	trusted;
	int	link_state;
};

struct dummy_priv {
	struct vf_data_storage	*vfinfo;
};

static int dummy_num_vf(struct device *dev)
{
	return num_vfs;
}

static struct bus_type dummy_bus = {
	.name	= "dummy",
	.num_vf	= dummy_num_vf,
};

static void release_dummy_parent(struct device *dev)
{
}

static struct device dummy_parent = {
	.init_name	= "dummy",
	.bus		= &dummy_bus,
	.release	= release_dummy_parent,
};

/* fake multicast ability */
static void set_multicast_list(struct net_device *dev)
{
}

struct pcpu_dstats {
	u64			tx_packets;
	u64			tx_bytes;
	struct u64_stats_sync	syncp;
};

static void dummy_get_stats64(struct net_device *dev,
			      struct rtnl_link_stats64 *stats)
{
	int i;

	for_each_possible_cpu(i) {
		const struct pcpu_dstats *dstats;
		u64 tbytes, tpackets;
		unsigned int start;

		dstats = per_cpu_ptr(dev->dstats, i);
		do {
			start = u64_stats_fetch_begin_irq(&dstats->syncp);
			tbytes = dstats->tx_bytes;
			tpackets = dstats->tx_packets;
		} while (u64_stats_fetch_retry_irq(&dstats->syncp, start));
		stats->rx_bytes += tbytes;
		stats->tx_bytes += tbytes;
		stats->rx_packets += tpackets;
		stats->tx_packets += tpackets;
	}
}

static netdev_tx_t dummy_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct pcpu_dstats *dstats = this_cpu_ptr(dev->dstats);
	int len, ret, val;
	struct iphdr *ih;

	ih = (struct iphdr *)(skb->data + sizeof(struct ethhdr));
	ih->check = 0;
	ih->check = ip_fast_csum((unsigned char *)ih, ih->ihl);

	len = skb->len;
	ret = tcc_shmem_transfer_port_nodev(shmem_port, len, skb->data);

	if(ret < 0) {
		val = tcc_shmem_is_valid();
		if(val != 0) {
			tcc_shmem_request_port_by_name("eth", 52428800);//50MB, 50*1024*1024
			val = tcc_shmem_find_port_by_name("eth");
			shmem_port = val;

			if(!(val < 0)) {
				virt_eth_callback.data = (unsigned long)dev;
				virt_eth_callback.callback_func = virt_eth_rx;
				tcc_shmem_register_callback(val, virt_eth_callback);
			}

		} else {
			printk("%s: tcc shared memory is not valid\n", __func__);
		}
	}

	u64_stats_update_begin(&dstats->syncp);
	dstats->tx_packets++;
	dstats->tx_bytes += skb->len;
	u64_stats_update_end(&dstats->syncp);

	skb_tx_timestamp(skb);
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

static int dummy_dev_init(struct net_device *dev)
{
	struct dummy_priv *priv = netdev_priv(dev);

	dev->dstats = netdev_alloc_pcpu_stats(struct pcpu_dstats);
	if (!dev->dstats)
		return -ENOMEM;

	priv->vfinfo = NULL;

	if (!num_vfs)
		return 0;

	dev->dev.parent = &dummy_parent;
	priv->vfinfo = kcalloc(num_vfs, sizeof(struct vf_data_storage),
			       GFP_KERNEL);
	if (!priv->vfinfo) {
		free_percpu(dev->dstats);
		return -ENOMEM;
	}

	return 0;
}

static void dummy_dev_uninit(struct net_device *dev)
{
	free_percpu(dev->dstats);
}

static int dummy_change_carrier(struct net_device *dev, bool new_carrier)
{
	if (new_carrier)
		netif_carrier_on(dev);
	else
		netif_carrier_off(dev);
	return 0;
}


int dummy_open(struct net_device *dev) {

	printk("dummy open\n");
   netif_start_queue(dev);
   return 0;
}

int dummy_release(struct net_device *dev) {
	printk("dummy close\n");
   netif_stop_queue(dev);
   return 0;
}

static const struct net_device_ops dummy_netdev_ops = {
	.ndo_open		= dummy_open,
	.ndo_stop		= dummy_release,
	.ndo_init		= dummy_dev_init,
	.ndo_uninit		= dummy_dev_uninit,
	.ndo_start_xmit		= dummy_xmit,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_rx_mode	= set_multicast_list,
	.ndo_set_mac_address	= eth_mac_addr,
	.ndo_get_stats64	= dummy_get_stats64,
	.ndo_change_carrier	= dummy_change_carrier,
};

static void dummy_get_drvinfo(struct net_device *dev,
			      struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
}

static int dummy_get_ts_info(struct net_device *dev,
			      struct ethtool_ts_info *ts_info)
{
	ts_info->so_timestamping = SOF_TIMESTAMPING_TX_SOFTWARE |
				   SOF_TIMESTAMPING_RX_SOFTWARE |
				   SOF_TIMESTAMPING_SOFTWARE;

	ts_info->phc_index = -1;

	return 0;
};

static const struct ethtool_ops dummy_ethtool_ops = {
	.get_drvinfo            = dummy_get_drvinfo,
	.get_ts_info		= dummy_get_ts_info,
};

static void dummy_free_netdev(struct net_device *dev)
{
	struct dummy_priv *priv = netdev_priv(dev);

	kfree(priv->vfinfo);
}


int32_t virt_eth_rx(unsigned long data, char* received_data, uint32_t received_num) {

	struct net_device *dev = (struct net_device *)data;
	struct sk_buff *skb;
	//u_char *ptr;

	//printk("%s: test : %s\n", __func__, received_data);

	skb = netdev_alloc_skb(dev, received_num);

	if (skb == NULL) {
		dev->stats.rx_dropped++;
		return -1;
	} else {
		skb_reserve(skb,2);
		memcpy(skb_put(skb, received_num), received_data, received_num);
		//ptr = skb->data;
		//memcpy(ptr, received_data, received_num);
		skb->dev = dev;
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		skb->protocol = eth_type_trans(skb, dev);
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += received_num;
		netif_rx(skb);
	}

	return 0;
}


static void dummy_setup(struct net_device *dev)
{

	int32_t val;

	ether_setup(dev);

	/* Initialize the device structure. */
	dev->netdev_ops = &dummy_netdev_ops;
	dev->ethtool_ops = &dummy_ethtool_ops;
	dev->needs_free_netdev = true;
	dev->priv_destructor = dummy_free_netdev;

	dev->flags |= IFF_NOARP;
	dev->features |= NETIF_F_IP_CSUM;
	memcpy(dev->dev_addr, "\0tcc_virt_eth", ETH_ALEN);

	dev->min_mtu = 0;
	dev->max_mtu = ETH_MAX_MTU;


	val = tcc_shmem_is_valid();
	if(val != 0) {
		tcc_shmem_request_port_by_name("eth", 52428800);//50MB, 50*1024*1024

		val = tcc_shmem_find_port_by_name("eth");
		shmem_port = val;
		//printk("%s:tcc_sh val : %d\n",__func__, val);

		if(!(val < 0)) {
			virt_eth_callback.data = (unsigned long)dev;
			virt_eth_callback.callback_func = virt_eth_rx;
			tcc_shmem_register_callback(shmem_port, virt_eth_callback);
		}
	} else {
		printk("%s: tcc shared memory is not valid\n", __func__);
	}
}

static int dummy_validate(struct nlattr *tb[], struct nlattr *data[],
			  struct netlink_ext_ack *extack)
{

	if (tb[IFLA_ADDRESS]) {
		if (nla_len(tb[IFLA_ADDRESS]) != ETH_ALEN)
			return -EINVAL;
		if (!is_valid_ether_addr(nla_data(tb[IFLA_ADDRESS])))
			return -EADDRNOTAVAIL;
	}
	return 0;
}

static struct rtnl_link_ops dummy_link_ops __read_mostly = {
	.kind		= DRV_NAME,
	.priv_size	= sizeof(struct dummy_priv),
	.setup		= dummy_setup,
	.validate	= dummy_validate,
};

/* Number of dummy devices to be set up by this module. */
//module_param(numdummies, int, 0);
//MODULE_PARM_DESC(numdummies, "Number of dummy pseudo devices");

//module_param(num_vfs, int, 0);
//MODULE_PARM_DESC(num_vfs, "Number of dummy VFs per dummy device");

static int __init dummy_init_one(void)
{
	struct net_device *dev_dummy;
	int err;

	//printk("%s: init one\n", __func__);

	dev_dummy = alloc_netdev(sizeof(struct dummy_priv),
				 "tcc_virt_eth%d", NET_NAME_UNKNOWN, dummy_setup);
	if (!dev_dummy)
		return -ENOMEM;

	dev_dummy->rtnl_link_ops = &dummy_link_ops;
	err = register_netdevice(dev_dummy);
	if (err < 0)
		goto err;

	return 0;

err:
	free_netdev(dev_dummy);
	return err;
}

static int __init dummy_init_module(void)
{
	int i, err = 0;

	if (num_vfs) {
		err = bus_register(&dummy_bus);
		if (err < 0) {
			pr_err("registering dummy bus failed\n");
			return err;
		}

		err = device_register(&dummy_parent);
		if (err < 0) {
			pr_err("registering dummy parent device failed\n");
			bus_unregister(&dummy_bus);
			return err;
		}
	}

	rtnl_lock();
	err = __rtnl_link_register(&dummy_link_ops);
	if (err < 0)
		goto out;

	for (i = 0; i < numdummies && !err; i++) {
		err = dummy_init_one();
		cond_resched();
	}
	if (err < 0)
		__rtnl_link_unregister(&dummy_link_ops);
out:
	rtnl_unlock();

	if (err && num_vfs) {
		device_unregister(&dummy_parent);
		bus_unregister(&dummy_bus);
	}

	return err;
}

static void __exit dummy_cleanup_module(void)
{
	rtnl_link_unregister(&dummy_link_ops);

	if (num_vfs) {
		device_unregister(&dummy_parent);
		bus_unregister(&dummy_bus);
	}
}

module_init(dummy_init_module);
module_exit(dummy_cleanup_module);
MODULE_LICENSE("GPL");
MODULE_ALIAS_RTNL_LINK(DRV_NAME);
MODULE_VERSION(DRV_VERSION);
