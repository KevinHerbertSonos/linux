/*
 *	Spanning tree protocol; interface code
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
#include <linux/kmod.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>

#include "br_private.h"
#include "br_private_stp.h"

#if defined(CONFIG_SONOS)
#include "br_sonos.h"
#include "br_mcast.h"
#endif

/* Port id is composed of priority and port number.
 * NB: some bits of priority are dropped to
 *     make room for more ports.
 */
static inline port_id br_make_port_id(__u8 priority, __u16 port_no)
{
	return ((u16)priority << BR_PORT_BITS)
		| (port_no & ((1<<BR_PORT_BITS)-1));
}

#define BR_MAX_PORT_PRIORITY ((u16)~0 >> BR_PORT_BITS)

/* called under bridge lock */
void br_init_port(struct net_bridge_port *p)
{
	p->port_id = br_make_port_id(p->priority, p->port_no);
	br_become_designated_port(p);
	p->state = BR_STATE_BLOCKING;
	p->topology_change_ack = 0;
	p->config_pending = 0;
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	sonos_init_port(p);
#endif
}

/* NO locks held */
void br_stp_enable_bridge(struct net_bridge *br)
{
	struct net_bridge_port *p;

	spin_lock_bh(&br->lock);
	mod_timer(&br->hello_timer, jiffies + br->hello_time);
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	mod_timer(&br->gc_timer, jiffies + HZ * 4);
	mod_timer(&br->mcast_timer, jiffies + br->mcast_advertise_time);
#else
	mod_timer(&br->gc_timer, jiffies + HZ/10);
#endif

	br_config_bpdu_generation(br);

	/* bring up STP-aware ports */
	list_for_each_entry(p, &br->port_list, list) {
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
		int bUp      = (p->dev->flags & IFF_UP) ? 1 : 0;
		int bCarrier = netif_carrier_ok(p->dev);

		if (bUp && bCarrier)
			br_stp_enable_port(p);
#else
		if (netif_running(p->dev) && netif_oper_up(p->dev))
			br_stp_enable_port(p);
#endif

	}
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	sonos_enable_leaf_ports(br, p);
#endif
	spin_unlock_bh(&br->lock);
}

/* NO locks held */
void br_stp_disable_bridge(struct net_bridge *br)
{
	struct net_bridge_port *p;

	spin_lock_bh(&br->lock);

	/* STP-aware ports */
	list_for_each_entry(p, &br->port_list, list) {
		if (p->state != BR_STATE_DISABLED)
			br_stp_disable_port(p);

	}

#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	sonos_disable_leaf_ports(br, p);
	br_mcast_destroy_list(br);
#endif

	br->topology_change = 0;
	br->topology_change_detected = 0;
	spin_unlock_bh(&br->lock);

	del_timer_sync(&br->hello_timer);
	del_timer_sync(&br->topology_change_timer);
	del_timer_sync(&br->tcn_timer);
	del_timer_sync(&br->gc_timer);
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	del_timer_sync(&br->mcast_timer);
#endif
}

/* called under bridge lock */
void br_stp_enable_port(struct net_bridge_port *p)
{
	br_init_port(p);
	br_port_state_selection(p->br);
	br_log_state(p);
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	br_ifinfo_notify(RTM_NEWLINK, p);
#endif
}

/* called under bridge lock */
void br_stp_disable_port(struct net_bridge_port *p)
{
	struct net_bridge *br = p->br;
	int wasroot;

#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	printk(KERN_INFO "%s: port %i(%s) entering %s state\n",
	       br->dev->name, p->port_no, p->dev->name, "disabled");
#endif

	wasroot = br_is_root_bridge(br);
	br_become_designated_port(p);
	p->state = BR_STATE_DISABLED;
	p->topology_change_ack = 0;
	p->config_pending = 0;

	br_log_state(p);
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	br_ifinfo_notify(RTM_NEWLINK, p);
#endif

	del_timer(&p->message_age_timer);
	del_timer(&p->forward_delay_timer);
	del_timer(&p->hold_timer);

#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	br_fdb_delete_by_port(br, p, 0);
	br_multicast_disable_port(p);
#endif

	br_configuration_update(br);

	br_port_state_selection(br);

	if (br_is_root_bridge(br) && !wasroot)
		br_become_root_bridge(br);
}

#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
static void br_stp_start(struct net_bridge *br)
{
	int r;
	char *argv[] = { BR_STP_PROG, br->dev->name, "start", NULL };
	char *envp[] = { NULL };

	r = call_usermodehelper(BR_STP_PROG, argv, envp, UMH_WAIT_PROC);

	spin_lock_bh(&br->lock);

	if (br->bridge_forward_delay < BR_MIN_FORWARD_DELAY)
		__br_set_forward_delay(br, BR_MIN_FORWARD_DELAY);
	else if (br->bridge_forward_delay < BR_MAX_FORWARD_DELAY)
		__br_set_forward_delay(br, BR_MAX_FORWARD_DELAY);

	if (r == 0) {
		br->stp_enabled = BR_USER_STP;
		br_debug(br, "userspace STP started\n");
	} else {
		br->stp_enabled = BR_KERNEL_STP;
		br_debug(br, "using kernel STP\n");

		/* To start timers on any ports left in blocking */
		br_port_state_selection(br);
	}

	spin_unlock_bh(&br->lock);
}

static void br_stp_stop(struct net_bridge *br)
{
	int r;
	char *argv[] = { BR_STP_PROG, br->dev->name, "stop", NULL };
	char *envp[] = { NULL };

	if (br->stp_enabled == BR_USER_STP) {
		r = call_usermodehelper(BR_STP_PROG, argv, envp, UMH_WAIT_PROC);
		br_info(br, "userspace STP stopped, return code %d\n", r);

		/* To start timers on any ports left in blocking */
		spin_lock_bh(&br->lock);
		br_port_state_selection(br);
		spin_unlock_bh(&br->lock);
	}

	br->stp_enabled = BR_NO_STP;
}
#endif

void br_stp_set_enabled(struct net_bridge *br, unsigned long val)
{
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	br->stp_enabled = val?1:0;
#else
	ASSERT_RTNL();

	if (val) {
		if (br->stp_enabled == BR_NO_STP)
			br_stp_start(br);
	} else {
		if (br->stp_enabled != BR_NO_STP)
			br_stp_stop(br);
	}
#endif
}

/* called under bridge lock */
void br_stp_change_bridge_id(struct net_bridge *br, const unsigned char *addr)
{
	/* should be aligned on 2 bytes for ether_addr_equal() */
	unsigned short oldaddr_aligned[ETH_ALEN >> 1];
	unsigned char *oldaddr = (unsigned char *)oldaddr_aligned;
	struct net_bridge_port *p;
	int wasroot;

	wasroot = br_is_root_bridge(br);

	memcpy(oldaddr, br->bridge_id.addr, ETH_ALEN);
	memcpy(br->bridge_id.addr, addr, ETH_ALEN);
	memcpy(br->dev->dev_addr, addr, ETH_ALEN);

	list_for_each_entry(p, &br->port_list, list) {
		if (ether_addr_equal(p->designated_bridge.addr, oldaddr))
			memcpy(p->designated_bridge.addr, addr, ETH_ALEN);

		if (ether_addr_equal(p->designated_root.addr, oldaddr))
			memcpy(p->designated_root.addr, addr, ETH_ALEN);
	}

	br_configuration_update(br);
	br_port_state_selection(br);
	if (br_is_root_bridge(br) && !wasroot)
		br_become_root_bridge(br);
}

/* should be aligned on 2 bytes for ether_addr_equal() */
static const unsigned short br_mac_zero_aligned[ETH_ALEN >> 1];

/* called under bridge lock */
bool br_stp_recalculate_bridge_id(struct net_bridge *br)
{
	const unsigned char *br_mac_zero =
			(const unsigned char *)br_mac_zero_aligned;
	const unsigned char *addr = br_mac_zero;
	struct net_bridge_port *p;

#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	/* SONOS: Allow setting of bridge MAC without funky heuristics below.
	 *        We don't always have a valid port in the bridge to use for
	 *        the MAC, sadly.
	 */
	if (br->use_static_mac) {
		return change_bridge_id_static_mac(br);
	}
#else
	/* user has chosen a value so keep it */
	if (br->dev->addr_assign_type == NET_ADDR_SET)
		return false;
#endif

	list_for_each_entry(p, &br->port_list, list) {
		if (addr == br_mac_zero ||
		    memcmp(p->dev->dev_addr, addr, ETH_ALEN) < 0)
			addr = p->dev->dev_addr;

	}

	if (ether_addr_equal(br->bridge_id.addr, addr))
		return false;	/* no change */

	br_stp_change_bridge_id(br, addr);
	return true;
}

/* called under bridge lock */
void br_stp_set_bridge_priority(struct net_bridge *br, u16 newprio)
{
	struct net_bridge_port *p;
	int wasroot;

	wasroot = br_is_root_bridge(br);

	list_for_each_entry(p, &br->port_list, list) {
		if (p->state != BR_STATE_DISABLED &&
		    br_is_designated_port(p)) {
			p->designated_bridge.prio[0] = (newprio >> 8) & 0xFF;
			p->designated_bridge.prio[1] = newprio & 0xFF;
		}

	}

	br->bridge_id.prio[0] = (newprio >> 8) & 0xFF;
	br->bridge_id.prio[1] = newprio & 0xFF;
	br_configuration_update(br);
	br_port_state_selection(br);
	if (br_is_root_bridge(br) && !wasroot)
		br_become_root_bridge(br);
}

/* called under bridge lock */
int br_stp_set_port_priority(struct net_bridge_port *p, unsigned long newprio)
{
	port_id new_port_id;

#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	if (newprio > BR_MAX_PORT_PRIORITY)
		return -ERANGE;
#endif

	new_port_id = br_make_port_id(newprio, p->port_no);
	if (br_is_designated_port(p))
		p->designated_port = new_port_id;

	p->port_id = new_port_id;
	p->priority = newprio;
	if (!memcmp(&p->br->bridge_id, &p->designated_bridge, 8) &&
	    p->port_id < p->designated_port) {
		br_become_designated_port(p);
		br_port_state_selection(p->br);
	}

	return 0;
}

/* called under bridge lock */
int br_stp_set_path_cost(struct net_bridge_port *p, unsigned long path_cost)
{
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	if (path_cost < BR_MIN_PATH_COST ||
	    path_cost > BR_MAX_PATH_COST)
		return -ERANGE;

	p->flags |= BR_ADMIN_COST;
#endif
	p->path_cost = path_cost;
	br_configuration_update(p->br);
	br_port_state_selection(p->br);
	return 0;
}

ssize_t br_show_bridge_id(char *buf, const struct bridge_id *id)
{
	return sprintf(buf, "%.2x%.2x.%.2x%.2x%.2x%.2x%.2x%.2x\n",
	       id->prio[0], id->prio[1],
	       id->addr[0], id->addr[1], id->addr[2],
	       id->addr[3], id->addr[4], id->addr[5]);
}
