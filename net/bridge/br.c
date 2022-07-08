/*
 *	Generic parts
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/llc.h>
#include <net/llc.h>
#include <net/stp.h>
#include <net/switchdev.h>

#if defined(CONFIG_SONOS_BRIDGE_PROXY) /* SONOS SWPBL-70338 */
#include <linux/inetdevice.h>
#endif

#include "br_private.h"
#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
#include "br_forward_sonos.h"
#include "br_proxy.h"
#include "br_sonos.h"
#endif

/*
 * Handle changes in state of network devices enslaved to a bridge.
 *
 * Note: don't care about up/down if bridge itself is down, because
 *     port state is checked when bridge is brought up.
 */
static int br_device_event(struct notifier_block *unused, unsigned long event, void *ptr)
{
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);
	struct net_bridge_port *p;
	struct net_bridge *br;
	bool changed_addr;
#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	struct net_bridge_port_list_node *pl = dev->br_port_list;

	if (NULL == pl) {
		return NOTIFY_DONE;
	}

	p = pl->port;
#else
	int err;

	/* register of bridge completed, add sysfs entries */
	if ((dev->priv_flags & IFF_EBRIDGE) && event == NETDEV_REGISTER) {
		br_sysfs_addbr(dev);
		return NOTIFY_DONE;
	}

	/* not a port of a bridge */
	p = br_port_get_rtnl(dev);
#endif
	if (!p)
		return NOTIFY_DONE;

	br = p->br;

	switch (event) {
	case NETDEV_CHANGEMTU:
#if defined(CONFIG_SONOS)
		spin_lock_bh(&br->lock);
		dev_set_mtu(br->dev, br_min_mtu(br));
		spin_unlock_bh(&br->lock);
#else
		dev_set_mtu(br->dev, br_min_mtu(br));
#endif
		break;

	case NETDEV_CHANGEADDR:
		spin_lock_bh(&br->lock);
#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
		br_fdb_changeaddr(pl, dev->dev_addr);
#else
		br_fdb_changeaddr(p, dev->dev_addr);
#endif
		changed_addr = br_stp_recalculate_bridge_id(br);
		spin_unlock_bh(&br->lock);

#if !defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
		if (changed_addr)
			call_netdevice_notifiers(NETDEV_CHANGEADDR, br->dev);
#endif
		break;

	case NETDEV_CHANGE:
#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
		spin_lock_bh(&br->lock);
		sonos_netdev_change(br, dev, pl);
		spin_unlock_bh(&br->lock);
#else
		br_port_carrier_check(p);
#endif
		break;

	case NETDEV_FEAT_CHANGE:
#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
		spin_lock_bh(&br->lock);
		if (br->dev->flags & IFF_UP) {
			br_features_recompute(br);
		}
		spin_unlock_bh(&br->lock);
		/* could do recursive feature change notification
		 * but who would care??
		 */
#else
		netdev_update_features(br->dev);
#endif
		break;

	case NETDEV_DOWN:
		spin_lock_bh(&br->lock);
		if (br->dev->flags & IFF_UP)
#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
			sonos_netdev_down(pl);
#else
			br_stp_disable_port(p);
#endif
		spin_unlock_bh(&br->lock);
		break;

	case NETDEV_UP:
#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
		/* bring up all ports on this interface */
		spin_lock_bh(&br->lock);
		if (br->dev->flags & IFF_UP) {
			sonos_netdev_up(pl);
		}
		spin_unlock_bh(&br->lock);
#else
		if (netif_running(br->dev) && netif_oper_up(dev)) {
			spin_lock_bh(&br->lock);
			br_stp_enable_port(p);
			spin_unlock_bh(&br->lock);
		}
#endif
		break;

	case NETDEV_UNREGISTER:
		br_del_if(br, dev);
		break;

	case NETDEV_CHANGENAME:
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
		err = br_sysfs_renameif(p);
		if (err)
			return notifier_from_errno(err);
#endif
		break;

	case NETDEV_PRE_TYPE_CHANGE:
		/* Forbid underlaying device to change its type. */
		return NOTIFY_BAD;

	case NETDEV_RESEND_IGMP:
		/* Propagate to master device */
		call_netdevice_notifiers(event, br->dev);
		break;
	}

	/* Events that may cause spanning tree to refresh */
	if (event == NETDEV_CHANGEADDR || event == NETDEV_UP ||
	    event == NETDEV_CHANGE || event == NETDEV_DOWN)
		br_ifinfo_notify(RTM_NEWLINK, p);

	return NOTIFY_DONE;
}

static struct notifier_block br_device_notifier = {
	.notifier_call = br_device_event
};

#if !defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
/* called with RTNL */
static int br_switchdev_event(struct notifier_block *unused,
			      unsigned long event, void *ptr)
{
	struct net_device *dev = switchdev_notifier_info_to_dev(ptr);
	struct net_bridge_port *p;
	struct net_bridge *br;
	struct switchdev_notifier_fdb_info *fdb_info;
	int err = NOTIFY_DONE;

	p = br_port_get_rtnl(dev);
	if (!p)
		goto out;

	br = p->br;

	switch (event) {
	case SWITCHDEV_FDB_ADD:
		fdb_info = ptr;
		err = br_fdb_external_learn_add(br, p, fdb_info->addr,
						fdb_info->vid);
		if (err)
			err = notifier_from_errno(err);
		break;
	case SWITCHDEV_FDB_DEL:
		fdb_info = ptr;
		err = br_fdb_external_learn_del(br, p, fdb_info->addr,
						fdb_info->vid);
		if (err)
			err = notifier_from_errno(err);
		break;
	}

out:
	return err;
}

static struct notifier_block br_switchdev_notifier = {
	.notifier_call = br_switchdev_event,
};
#endif

static void __net_exit br_net_exit(struct net *net)
{
	struct net_device *dev;
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	LIST_HEAD(list);
#endif

	rtnl_lock();
#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
restart:
	for_each_netdev(net, dev)
		if (dev->priv_flags & IFF_EBRIDGE)
		{
			br_dev_delete(dev, NULL);
			goto restart;
		}
#else
	for_each_netdev(net, dev)
		if (dev->priv_flags & IFF_EBRIDGE)
			br_dev_delete(dev, &list);

	unregister_netdevice_many(&list);
#endif
	rtnl_unlock();

}

static struct pernet_operations br_net_ops = {
	.exit	= br_net_exit,
};

#if !defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
static const struct stp_proto br_stp_proto = {
	.rcv	= br_stp_rcv,
};
#endif

static int __init br_init(void)
{
	int err;

	BUILD_BUG_ON(sizeof(struct br_input_skb_cb) > FIELD_SIZEOF(struct sk_buff, cb));

#if !defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	err = stp_proto_register(&br_stp_proto);
	if (err < 0) {
		pr_err("bridge: can't register sap for STP\n");
		return err;
	}
#endif

	err = br_fdb_init();
	if (err)
		goto err_out;

	err = register_pernet_subsys(&br_net_ops);
	if (err)
		goto err_out1;

	err = br_nf_core_init();
	if (err)
		goto err_out2;

	err = register_netdevice_notifier(&br_device_notifier);
	if (err)
		goto err_out3;

#if defined(CONFIG_SONOS_BRIDGE_PROXY) /* SONOS SWPBL-70338 */
	err = register_inetaddr_notifier(&br_inetaddr_notifier);
	if (err)
		goto err_out_proxy;
#endif
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	err = register_switchdev_notifier(&br_switchdev_notifier);
	if (err)
		goto err_out4;
#endif

	err = br_netlink_init();
	if (err)
		goto err_out5;

	brioctl_set(br_ioctl_deviceless_stub);

#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	br_handle_frame_hook = sonos_br_handle_frame;
#elif IS_ENABLED(CONFIG_ATM_LANE)
	br_fdb_test_addr_hook = br_fdb_test_addr;
#endif

#if IS_MODULE(CONFIG_BRIDGE_NETFILTER)
	pr_info("bridge: filtering via arp/ip/ip6tables is no longer available "
		"by default. Update your scripts to load br_netfilter if you "
		"need this.\n");
#endif

	return 0;

err_out5:
#if defined(CONFIG_SONOS_BRIDGE_PROXY) /* SONOS SWPBL-70338 */
	unregister_inetaddr_notifier(&br_inetaddr_notifier);
err_out_proxy:
#endif
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	unregister_switchdev_notifier(&br_switchdev_notifier);
err_out4:
#endif
	unregister_netdevice_notifier(&br_device_notifier);
err_out3:
	br_nf_core_fini();
err_out2:
	unregister_pernet_subsys(&br_net_ops);
err_out1:
	br_fdb_fini();
err_out:
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	stp_proto_unregister(&br_stp_proto);
#endif
	return err;
}

static void __exit br_deinit(void)
{
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	stp_proto_unregister(&br_stp_proto);
#endif
	br_netlink_fini();
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	unregister_switchdev_notifier(&br_switchdev_notifier);
#endif
	unregister_netdevice_notifier(&br_device_notifier);
#if defined(CONFIG_SONOS_BRIDGE_PROXY) /* SONOS SWPBL-70338 */
	unregister_inetaddr_notifier(&br_inetaddr_notifier);
#endif
	brioctl_set(NULL);
	unregister_pernet_subsys(&br_net_ops);

	rcu_barrier(); /* Wait for completion of call_rcu()'s */

	br_nf_core_fini();
#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	br_handle_frame_hook = NULL;
#elif IS_ENABLED(CONFIG_ATM_LANE)
	br_fdb_test_addr_hook = NULL;
#endif
	br_fdb_fini();
}

#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
int (*br_should_route_hook)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(br_should_route_hook);
#endif

module_init(br_init)
module_exit(br_deinit)
MODULE_LICENSE("GPL");
MODULE_VERSION(BR_VERSION);
MODULE_ALIAS_RTNL_LINK("bridge");
