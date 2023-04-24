/* br_sonos.c - Sonos Bridge Extensions
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <linux/version.h>
#include "br_private.h"

#include "br_sonos.h"
#include "br_direct.h"
#include "br_fdb_sonos.h"
#include "br_forward_sonos.h"
#include "br_mcast.h"
#include "br_stp_sonos.h"
#include "br_tunnel.h"
#include "br_uplink.h"

int sonos_initial_port_cost(struct net_device *dev)
{
	/*
	 * Sonos only supports eth/ath, and occasionally wlan.  "Silly"
	 * heuristics are fine for us, and will continue to be until we support
	 * links that are much faster. :-)
	 */
	if (!strncmp(dev->name, "eth", 3))
		return 10;

	if (!strncmp(dev->name, "wlan", 4) || !strncmp(dev->name, "ath", 3))
		return 150;

	return 100;
}

struct
net_bridge_port_list_node *sonos_alloc_port_list(struct net_bridge_port *p)
{
	struct net_bridge_port_list_node *pl;
	pl = kzalloc(sizeof(*pl), GFP_KERNEL);
	if (pl == NULL) {
		kfree(p);
		return NULL;
	} else {
		pl->port = p;
		pl->next = 0;
	}
	return pl;
}

/* called with RTNL */
void sonos_del_br(struct net_bridge *br)
{
	struct net_bridge_port *p, *n;

	list_for_each_entry_safe(p, n, &br->port_list, list) {
		br_sonos_del_nbp(p);
	}

	list_for_each_entry_safe(p, n, &br->leaf_list, list) {
		br_sonos_del_nbp(p);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	cancel_delayed_work_sync(&br->gc_work);
#else
	del_timer_sync(&br->gc_timer);
#endif
	del_timer_sync(&br->mcast_timer);

	br_sysfs_delbr(br->dev);
	unregister_netdevice(br->dev);
}

static void sonos_init_bridge_dev(struct net_device *dev)
{
	struct net_bridge *br;

	br = netdev_priv(dev);
	br->dev = dev;

	spin_lock_init(&br->lock);
	INIT_LIST_HEAD(&br->port_list);
	INIT_LIST_HEAD(&br->leaf_list);
	spin_lock_init(&br->hash_lock);
	spin_lock_init(&br->mcast_lock);

	br->bridge_id.prio[0] = 0x80;
	br->bridge_id.prio[1] = 0x00;
	memset(br->bridge_id.addr, 0, ETH_ALEN);

	/* SONOS: All STP all the time... */
	br->stp_enabled = 1;

	br->designated_root = br->bridge_id;
	br->root_path_cost = 0;
	br->root_port = 0;
	br->bridge_max_age = br->max_age = 20 * HZ;
	br->bridge_hello_time = br->hello_time = 2 * HZ;
	br->bridge_forward_delay = br->forward_delay = 3 * HZ;
	br->topology_change = 0;
	br->topology_change_detected = 0;
	br->num_mcast_groups = 0;
	br->mcast_advertise_time = 10 * HZ;
	br->ageing_time = 60 * HZ;
	br->mcast_ageing_time = 60 * HZ;
	INIT_LIST_HEAD(&br->age_list);

	br_stp_timer_init(br);
	br_stats_init(br);
}

/* scan the list of bridge ports associated with an interface to find the
   one on which a packet arrived. */
struct net_bridge_port* br_find_port(const unsigned char *h_source,
                                     struct net_bridge_port_list_node *pl)
{
	struct net_bridge_port *uplink = NULL;

        while (pl) {
		if (pl->port->is_p2p) {
			if (0 == memcmp(pl->port->p2p_dest_addr, h_source, ETH_ALEN))
				return pl->port;
			if (pl->port->is_uplink) {
				uplink = pl->port;
			}
		} else {
			/* this is a normal bridge port, not a point-to-point tunnel */
			return pl->port;
		}

		pl = pl->next;
        }

	/*
	 * REVIEW:  This is most unfortunate, but we need to assume that the
	 *          source of the packet is the uplink port if we didn't match
	 *          on any port but we did find an uplink.  There should be no
	 *          other way into the bridge, but I really can't verify that
	 *          this packet is good in any way that makes me happy.
	 */
	return uplink;
}
       
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
void _br_gc_timer_expired(unsigned long data)
{
	struct net_bridge *br = (struct net_bridge *)data;
	br_mcast_age_list(br);
	br_fdb_cleanup(data);
	mod_timer(&br->gc_timer, jiffies + 4*HZ);  // NOTE: Same as SH4
}
#else
void _br_gc_timer_expired(struct work_struct *work)
{
	struct net_bridge *br = container_of(work, struct net_bridge,
					     gc_work.work);
	br_mcast_age_list(br);
	br_fdb_cleanup((unsigned long)br);
	mod_delayed_work(system_long_wq, &br->gc_work, 4*HZ);
}
#endif

/* called with RTNL */
static unsigned char get_routing_capabilities(struct net_bridge *br)
{
	struct net_bridge_port *p;

	list_for_each_entry(p, &br->port_list, list) {
		if (!p->is_p2p && netif_carrier_ok(p->dev)) {
			return 0;
		}
	}

	return 1;
}

int sonos_get_routing_capabilities(struct net_bridge *br, void __user *userbuf)
{
	int ret = 0;

	/* Ask the bridge what it can do */
	unsigned char rc;
	rc = get_routing_capabilities(br);

	if (copy_to_user(userbuf, &rc, sizeof(rc))) {
		ret = -EFAULT;
	}

	return ret;
}

netdev_tx_t sonos_br_dev_xmit(struct net_bridge *br, struct sk_buff *skb,
			      const unsigned char *dest)
{
	struct net_bridge_fdb_entry *dst;
	br->statistics.tx_packets++;
	br->statistics.tx_bytes += skb->len;

	skb_reset_mac_header(skb);
	skb_pull(skb, ETH_HLEN);
	memset(BR_SKB_CB(skb), 0, sizeof(*(BR_SKB_CB(skb))));

	rcu_read_lock();

	if (br->uplink_mode) {
		/* Uplink */
		br_uplink_xmit(br, skb, dest);
	} else if (dest[0] & 1) {
		/* Multicast/broadcast */
		br_flood_deliver(br, 0, skb, 0);
	} else {
		/* Unicast */
		dst = __br_fdb_get(br, dest, 0);
		if (dst) {
			/* Known address */
			if (0 == skb->priority) {
				skb->priority = dst->priority;
			}

			br_direct_unicast(0, dst, skb,
					  br_deliver,
					  br_deliver_direct);
		} else {
			/* Unknown address */
			br_flood_deliver(br, 0, skb, 0);
		}
	}

	rcu_read_unlock();
	return NETDEV_TX_OK;
}


void sonos_br_get_stats64(struct net_bridge *br,
			  struct rtnl_link_stats64 *stats)
{
	stats->tx_bytes   = br->statistics.tx_bytes;
	stats->tx_packets = br->statistics.tx_packets;
	stats->rx_bytes   = br->statistics.rx_bytes;
	stats->rx_packets = br->statistics.rx_packets;
}

void sonos_init_port(struct net_bridge_port *p)
{
	p->direct_enabled = 0;
	memset(&p->direct_addr, 0, ETH_ALEN);
	p->direct_last_stp_time = jiffies - BR_DIRECT_STP_TIME;
}

void sonos_enable_leaf_ports(struct net_bridge *br,
			     struct net_bridge_port *p)
{
	/* bring up leaf ports */
	list_for_each_entry(p, &br->leaf_list, list) {
		if (p->dev->flags & IFF_UP)
			p->state = BR_STATE_FORWARDING;
	}
}

void sonos_disable_leaf_ports(struct net_bridge *br,
			      struct net_bridge_port *p)
{
	/* Leaf nodes */
	list_for_each_entry(p, &br->leaf_list, list) {
		if (p->dev->flags & IFF_UP)
			p->state = BR_STATE_DISABLED;
	}
}

bool change_bridge_id_static_mac(struct net_bridge *br)
{
	if (!ether_addr_equal(br->bridge_id.addr, br->static_mac)) {
		br_stp_change_bridge_id(br, br->static_mac);
		return true;
	}
	return false;
}

const unsigned char *sonos_change_bridge_id(struct net_bridge *br,
					    const unsigned char *addr,
					    const unsigned char *br_mac_zero)
{
	struct net_bridge_port *p;

	list_for_each_entry(p, &br->port_list, list) {
		/* prefer first added wired ethernet interface for
		   bridge ID */
		if (addr == br_mac_zero ||
		    0 == strncmp(p->dev->name, "eth", 3))
			addr = p->dev->dev_addr;

	}

	return addr;
}

void sonos_del_nbp(struct net_bridge_port *p, struct net_bridge *br,
		   struct net_device *dev, struct net_bridge_port_list_node *pl)
{
	struct net_bridge_port_list_node *pl_curr;

	/*
	 * None of the paths in here grab br->lock, and this is what protects
	 * dev->br_port_list.  I don't think holding it for the entire function
	 * is going to cause any undue pain.
	 */
	spin_lock_bh(&br->lock);

	/*
	 * Disable all ports hanging off of this interface.  Leaf ports just
	 * get marked disabled, all others need to be taken out the stp
	 * insanity.
	 */
	pl_curr = pl;
	while (pl_curr != NULL) {
		if (pl_curr->port->is_leaf)
			pl_curr->port->state = BR_STATE_DISABLED;
		else {
			br_stp_disable_port(pl_curr->port);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
			br_ifinfo_notify(RTM_DELLINK, br, pl_curr->port);
#else
			br_ifinfo_notify(RTM_DELLINK, pl_curr->port);
#endif
		}
		pl_curr = pl_curr->next;
	}

	/*
	 * if the bridge port is not a point-to-point tunnel, take it out
	 * of promiscuous mode.
	 */
	if (!pl->port->is_p2p)
		dev_set_promiscuity(dev, -1);

	/*
	 * Remove the pointer to the bridge from the device.  We have a copy of
	 * this in 'pl', and we'll free it later.
	 */
	dev->br_port_list = NULL;

	/*
	 * Walk the ports again, removing them from the list that the bridge
	 * maintains.  Leaf nodes are in br->leaf_list, everything else is on
	 * br->port_list.  This doesn't matter, though, since the lists are
	 * doubly-linked and list_del_rcu() deletes the entry from whatever
	 * list it is on.
	 */
	for (pl_curr = pl; pl_curr; pl_curr = pl_curr->next) {

		/* Grabs br->hash_lock */
		br_sonos_fdb_delete_by_port(br, pl_curr->port);  /* hash_lock */

		/* Grabs br->mcast_lock */
		br_mcast_delete_by_port(br, pl_curr->port);

		/* Remove from list (free comes later) */
		list_del_rcu(&pl_curr->port->list);
	}


	/*
	 * Walk one last time, freeing the bridge_port_list_node.  Could roll
	 * this into the previous loop, I suppose, but it is a wee bit
	 * awkward.
	 */
	while ((pl_curr = pl) != NULL) {
		pl = pl->next;
		kfree(pl_curr);
	}

	/*
	 * Release br->lock
	 */
	spin_unlock_bh(&br->lock);

	/*
	 * Kill timers. We must release the bridge lock beforehand so
	 * that a still-running timer handler on another CPU can get the
	 * lock.
	 *
	 * REVIEW: I think this is already handled in
	 *         br_stp_disable_port(pl_curr->port)
	 */
	del_timer_sync(&p->message_age_timer);
	del_timer_sync(&p->forward_delay_timer);
	del_timer_sync(&p->hold_timer);

	/* Free the port */
	if (!p->is_p2p) {
		kobject_uevent(&p->kobj, KOBJ_REMOVE);
		kobject_del(&p->kobj);
	}

}

int sonos_add_bridge(struct net *net, const char *name, struct net_device *dev)
{
	int ret;

	sonos_init_bridge_dev(dev);

	rtnl_lock();
	if (strchr(dev->name, '%')) {
		ret = dev_alloc_name(dev, dev->name);
		if (ret < 0)
			goto err1;
	}

	ret = register_netdevice(dev);
	if (ret)
		goto err2;

	/* network device kobject is not setup until
	 * after rtnl_unlock does it's hotplug magic.
	 * so hold reference to avoid race.
	 */
	dev_hold(dev);
	rtnl_unlock();

	ret = br_sysfs_addbr(dev);
	dev_put(dev);

	if (ret)
		unregister_netdev(dev);
 out:
	return ret;

 err2:
	free_netdev(dev);
 err1:
	rtnl_unlock();
	goto out;
}

void sonos_br_features_recompute(struct net_bridge *br)
{
	struct net_bridge_port *p;
	unsigned long features, checksum;

	features = NETIF_F_SG | NETIF_F_FRAGLIST | NETIF_F_HIGHDMA;
	checksum = NETIF_F_IP_CSUM;	/* least commmon subset */

	list_for_each_entry(p, &br->port_list, list) {
		if (!(p->dev->features
		      & (NETIF_F_IP_CSUM|NETIF_F_HW_CSUM)))
			checksum = 0;
		features &= p->dev->features;
	}

	br->dev->features = features | checksum | NETIF_F_LLTX;
}

int sonos_br_add_if(struct net_bridge *br, struct net_device *dev)
{
	struct net_bridge_port *p;
	int err = 0;

	if (dev->br_port_list != NULL)
		return -EBUSY;

	if (dev->flags & IFF_LOOPBACK || dev->type != ARPHRD_ETHER)
		return -EINVAL;

	if (dev->netdev_ops->ndo_start_xmit == br_dev_xmit)
		return -ELOOP;

	if (IS_ERR(p = br_sonos_new_nbp(br, dev)))
		return PTR_ERR(p);

	if ((err = br_fdb_insert(br, p, dev->dev_addr, 0)))
		br_sonos_destroy_nbp(p);

	else if ((err = br_sysfs_addif(p)))
		br_sonos_del_nbp(p);
	else {
		dev_set_promiscuity(dev, 1);

		list_add_rcu(&p->list, &br->port_list);

		spin_lock_bh(&br->lock);
		br_stp_recalculate_bridge_id(br);
		sonos_br_features_recompute(br);
		if ((br->dev->flags & IFF_UP)
		    && (dev->flags & IFF_UP) && netif_carrier_ok(dev)) {
			br_stp_enable_port(p);
			printk("br: new port w/ carrier: %s\n",
			       &(p->dev->name[0]));
		}
		spin_unlock_bh(&br->lock);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		br_ifinfo_notify(RTM_NEWLINK, br, p);
#else
		br_ifinfo_notify(RTM_NEWLINK, p);
#endif

		kobject_uevent(&p->kobj, KOBJ_ADD);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		dev_set_mtu(br->dev, br_mtu_min(br));
#else
		dev_set_mtu(br->dev, br_min_mtu(br));
#endif
	}
	return err;
}

int sonos_br_del_if(struct net_bridge *br, struct net_device *dev)
{
	struct net_bridge_port *p;

	if (NULL == dev->br_port_list || NULL == dev->br_port_list->port)
		return -EINVAL;

	p = dev->br_port_list->port;

	br_sonos_del_nbp(p);

	spin_lock_bh(&br->lock);
	br_stp_recalculate_bridge_id(br);
	sonos_br_features_recompute(br);
	spin_unlock_bh(&br->lock);

	return 0;
}

/* called with RTNL */
unsigned char sonos_get_any_forwarding(struct net_bridge *br, void __user *userbuf)
{
	struct net_bridge_port *p;
	int ret = 0;
	unsigned char rc = 0;

	/* Ask the bridge if anyone is happy */
	list_for_each_entry(p, &br->port_list, list) {
		if (p->state == BR_STATE_FORWARDING) {
			if (p->is_p2p || netif_carrier_ok(p->dev)) {
				rc = 1;
				break;
			}
		}
	}

	if (copy_to_user(userbuf, &rc, sizeof(rc))) {
		ret = -EFAULT;
	}

	return ret;
}

int sonos_br_set_static_mac(struct net_bridge *br, void __user *userbuf)
{
	unsigned char mac[6];
	int i, use_static_mac = 0;

	if (copy_from_user(mac, userbuf, ETH_ALEN)) {
		return -EFAULT;
	}

	for (i = 0; i < ETH_ALEN; i++) {
		if (mac[i] != 0) {
			use_static_mac = 1;
			break;
		}
	}

	spin_lock_bh(&br->lock);
	br->use_static_mac = use_static_mac;
	memcpy(&br->static_mac[0], mac, ETH_ALEN);
	br_stp_recalculate_bridge_id(br);
	spin_unlock_bh(&br->lock);

	return 0;
}

void sonos_get_port_info(struct __port_info *p, struct net_bridge_port *pt)
{
	p->is_p2p = pt->is_p2p;
	if (pt->is_p2p) {
		p->direct_enabled = pt->direct_enabled;
		p->is_uplink = pt->is_uplink;
		memcpy(p->p2p_dest_addr, pt->p2p_dest_addr, ETH_ALEN);
#ifdef CONFIG_SONOS_BRIDGE_PROXY
		p->sat_ip = pt->sat_ip;
#endif
	}
	p->remote_state = pt->remote_state;
}

int sonos_mod_port_addr(struct net_bridge *br,  void __user *userbuf1,  void __user *userbuf2)
{
	unsigned char oldaddr[ETH_ALEN];
	unsigned char newaddr[ETH_ALEN];

	int ret = 0;

	if (!capable(CAP_NET_ADMIN)) {
		return -EPERM;
	}

	if (copy_from_user(oldaddr, userbuf1, ETH_ALEN)) {
		return -EFAULT;
	}

	if (copy_from_user(newaddr, userbuf2, ETH_ALEN)) {
		return -EFAULT;
	}

	spin_lock_bh(&br->lock);
	if (br_stp_mod_port_addr(br, oldaddr, newaddr)) {
		ret = -EINVAL;
	}
	spin_unlock_bh(&br->lock);

	return ret;
}

int sonos_mod_port_dev(struct net_bridge *br, struct net_device *dev,
		       void __user *userbuf, int val)
{
	unsigned char oldaddr[ETH_ALEN];

	int ret = 0;

	if (!capable(CAP_NET_ADMIN)) {
		return -EPERM;
	}

	if (copy_from_user(oldaddr, userbuf, ETH_ALEN)) {
		return -EFAULT;
	}

	dev = dev_get_by_index(&init_net, val);
	if (dev == NULL) {
		return -EINVAL;
	}

	spin_lock_bh(&br->lock);
	if (br_stp_mod_port_dev(br, oldaddr, dev)) {
		ret = -EINVAL;
	}
	spin_unlock_bh(&br->lock);

	dev_put(dev);

	return ret;
}

int sonos_brctl_wrapper(struct net_bridge *br, unsigned long args[])
{

	struct net_device *dev;
	unsigned char daddr[ETH_ALEN];
	int ret;

	dev = dev_get_by_index(&init_net, args[1]);
	if (dev == NULL) {
		return -EINVAL;
	}

	if (copy_from_user(daddr, (void *)(args[2]), ETH_ALEN)) {
		ret = -EFAULT;
		goto exit;
	}

	switch(args[0]) {
	case BRCTL_ADD_P2P_TUNNEL:
		ret = br_add_p2p_tunnel(br, dev, &(daddr[0]),
					(struct __add_p2p_entry *)&args[3]);
		break;
	case BRCTL_SET_P2P_TUNNEL_PATH_COST:
		ret = br_set_p2p_tunnel_path_cost(br, dev, &(daddr[0]), args[3]);
		break;
	case BRCTL_ADD_P2P_TUNNEL_LEAF:
		ret = br_add_p2p_tunnel_leaf(br, dev, &(daddr[0]),
					     (struct __add_p2p_leaf_entry *)&args[3]);
		break;
	case BRCTL_DEL_P2P_TUNNEL:
		ret = br_del_p2p_tunnel(br, dev, &(daddr[0]));
		break;
	case BRCTL_SET_P2P_TUNNEL_STP_STATE:
		ret = br_set_p2p_tunnel_remote_stp_state(br, dev, &(daddr[0]), args[3]);
		break;
	case BRCTL_SET_P2P_DIRECT_ADDR:
		ret = br_set_p2p_direct_addr(br, dev, &(daddr[0]), (void __user *)args[3]);
		break;
	case BRCTL_SET_P2P_DIRECT_ENABLED:
		ret = br_set_p2p_direct_enabled(br, dev, &(daddr[0]), args[3]);
		break;
	case BRCTL_ADD_UPLINK:
		ret = br_add_uplink(br, dev, &(daddr[0]));
		break;
	default:
		ret = -EOPNOTSUPP;
	}

exit:
	dev_put(dev);

	return ret;
}

void sonos_netdev_change(struct net_bridge *br, struct net_device *dev,
			 struct net_bridge_port_list_node *pl)
{
	int ok = netif_carrier_ok(dev);

	if (!(br->dev->flags & IFF_UP))
		return;

	while (pl != NULL) {

		struct net_bridge_port *p = pl->port;

		/* No STP for leaf nodes, so no need to toggle */
		//if ((p != NULL) && (p->br != NULL) && !(p->is_leaf)) {
		if ((p != NULL) &&
				(p->br != NULL) &&
				(p->br->dev != NULL) &&
				(p->br->dev->name[0] == 'b') &&
				(p->br->dev->name[1] == 'r') &&
				!(p->is_leaf)) {
			if (ok) {
				if (p->state == BR_STATE_DISABLED)
					br_stp_enable_port(p);
			} else {
				if (p->state != BR_STATE_DISABLED)
					br_stp_disable_port(p);
			}

		}

		pl = pl->next;
	}
}

void sonos_netdev_down(struct net_bridge_port_list_node *pl)
{
	while (pl != NULL) {
		if (pl->port->is_leaf)
		    pl->port->state = BR_STATE_DISABLED;
		else
		    br_stp_disable_port(pl->port);

		pl = pl->next;
	}
}

void sonos_netdev_up(struct net_bridge_port_list_node *pl)
{
	while (pl != NULL) {
		if (pl->port->is_leaf)
		    pl->port->state = BR_STATE_FORWARDING;
		else
		    br_stp_enable_port(pl->port);

		pl = pl->next;
	}
}
