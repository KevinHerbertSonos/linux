/*
 *	Device event handling
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

#include <linux/kernel.h>
#include <linux/rtnetlink.h>
#include <net/net_namespace.h>

#include "br_private.h"

#if defined(CONFIG_SONOS)
#include "br_proxy.h"
#include "br_sonos.h"
#endif

static int br_device_event(struct notifier_block *unused, unsigned long event, void *ptr);

struct notifier_block br_device_notifier = {
	.notifier_call = br_device_event
};

/*
 * Handle changes in state of network devices enslaved to a bridge.
 *
 * Note: don't care about up/down if bridge itself is down, because
 *     port state is checked when bridge is brought up.
 */
static int br_device_event(struct notifier_block *unused, unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;
	struct net_bridge_port *p;
	struct net_bridge *br;
	bool changed_addr;
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
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
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
		br_fdb_changeaddr(pl, dev->dev_addr);
#else
		br_fdb_changeaddr(p, dev->dev_addr);
#endif
		changed_addr = br_stp_recalculate_bridge_id(br);
		spin_unlock_bh(&br->lock);

#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
		if (changed_addr)
			call_netdevice_notifiers(NETDEV_CHANGEADDR, br->dev);
#endif
		break;

	case NETDEV_CHANGE:	/* device is up but carrier changed */
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651, SWPBL-67503 */
		spin_lock_bh(&br->lock);
		sonos_netdev_change(br, dev, pl);
		spin_unlock_bh(&br->lock);
#else
		br_port_carrier_check(p);
#endif
		break;

	case NETDEV_FEAT_CHANGE:
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
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
		/* shut down all ports on this interface */
		spin_lock_bh(&br->lock);
		if (br->dev->flags & IFF_UP)
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651, SWPBL-67503 */
			sonos_netdev_down(pl);
#else
			br_stp_disable_port(p);
#endif
		spin_unlock_bh(&br->lock);
		break;

	case NETDEV_UP:
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651, SWPBL-67503 */
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
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
		err = br_sysfs_renameif(p);
		if (err)
			return notifier_from_errno(err);
#endif
		break;

	case NETDEV_PRE_TYPE_CHANGE:
		/* Forbid underlaying device to change its type. */
		return NOTIFY_BAD;
	}

	/* Events that may cause spanning tree to refresh */
	if (event == NETDEV_CHANGEADDR || event == NETDEV_UP ||
	    event == NETDEV_CHANGE || event == NETDEV_DOWN)
		br_ifinfo_notify(RTM_NEWLINK, p);

	return NOTIFY_DONE;
}
