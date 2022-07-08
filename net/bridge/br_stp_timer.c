/*
 *	Spanning tree protocol; timer-related code
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
#include <linux/times.h>

#include "br_private.h"
#include "br_private_stp.h"

#if defined(CONFIG_SONOS)
#include "br_mcast.h"
#include "br_proxy.h"
#include "br_sonos.h"
#include "br_stp_sonos.h"
#endif

/* called under bridge lock */
static int br_is_designated_for_some_port(const struct net_bridge *br)
{
	struct net_bridge_port *p;

	list_for_each_entry(p, &br->port_list, list) {
		if (p->state != BR_STATE_DISABLED &&
		    !memcmp(&p->designated_bridge, &br->bridge_id, 8))
			return 1;
	}

	return 0;
}

static void br_hello_timer_expired(unsigned long arg)
{
	struct net_bridge *br = (struct net_bridge *)arg;

	br_debug(br, "hello timer expired\n");
	SONOS_SPINLOCK(&br->lock);
	if (br->dev->flags & IFF_UP) {
		br_config_bpdu_generation(br);

		mod_timer(&br->hello_timer, round_jiffies(jiffies + br->hello_time));
	}
	SONOS_SPINUNLOCK(&br->lock);
}

static void br_message_age_timer_expired(unsigned long arg)
{
	struct net_bridge_port *p = (struct net_bridge_port *) arg;
	struct net_bridge *br = p->br;
	const bridge_id *id = &p->designated_bridge;
	int was_root;

	if (p->state == BR_STATE_DISABLED)
		return;

#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	br_info(br, "neighbor %.2x%.2x.%pM lost on port %u(%s)\n",
		id->prio[0], id->prio[1], &id->addr,
		(unsigned int) p->port_no, p->dev->name);
#else
	br_info(br, "port %u(%s) neighbor %.2x%.2x.%pM lost\n",
		(unsigned int) p->port_no, p->dev->name,
		id->prio[0], id->prio[1], &id->addr);
#endif

	/*
	 * According to the spec, the message age timer cannot be
	 * running when we are the root bridge. So..  this was_root
	 * check is redundant. I'm leaving it in for now, though.
	 */
	SONOS_SPINLOCK(&br->lock);
	if (p->state == BR_STATE_DISABLED)
		goto unlock;
	was_root = br_is_root_bridge(br);

	br_become_designated_port(p);
	br_configuration_update(br);
	br_port_state_selection(br);
	if (br_is_root_bridge(br) && !was_root)
		br_become_root_bridge(br);
 unlock:
	SONOS_SPINUNLOCK(&br->lock);
}

static void br_forward_delay_timer_expired(unsigned long arg)
{
	struct net_bridge_port *p = (struct net_bridge_port *) arg;
	struct net_bridge *br = p->br;

	br_debug(br, "port %u(%s) forward delay timer\n",
		 (unsigned int) p->port_no, p->dev->name);
	SONOS_SPINLOCK(&br->lock);
	if (p->state == BR_STATE_LISTENING) {
		p->state = BR_STATE_LEARNING;
		mod_timer(&p->forward_delay_timer,
			  jiffies + br->forward_delay);
	} else if (p->state == BR_STATE_LEARNING) {
		p->state = BR_STATE_FORWARDING;
		if (br_is_designated_for_some_port(br))
			br_topology_change_detection(br);
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
		netif_carrier_on(br->dev);
#endif
	}
	br_log_state(p);
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	br_ifinfo_notify(RTM_NEWLINK, p);
#endif
	SONOS_SPINUNLOCK(&br->lock);
}

static void br_tcn_timer_expired(unsigned long arg)
{
	struct net_bridge *br = (struct net_bridge *) arg;

	br_debug(br, "tcn timer expired\n");
	SONOS_SPINLOCK(&br->lock);
#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	if (br->dev->flags & IFF_UP) {
#else
	if (!br_is_root_bridge(br) && (br->dev->flags & IFF_UP)) {
#endif
		br_transmit_tcn(br);

		mod_timer(&br->tcn_timer,jiffies + br->bridge_hello_time);
	}
	SONOS_SPINUNLOCK(&br->lock);
}

static void br_topology_change_timer_expired(unsigned long arg)
{
	struct net_bridge *br = (struct net_bridge *) arg;

	br_debug(br, "topo change timer expired\n");
	SONOS_SPINLOCK(&br->lock);
	br->topology_change_detected = 0;
	br->topology_change = 0;
	SONOS_SPINUNLOCK(&br->lock);
}

static void br_hold_timer_expired(unsigned long arg)
{
	struct net_bridge_port *p = (struct net_bridge_port *) arg;

	br_debug(p->br, "port %u(%s) hold timer expired\n",
		 (unsigned int) p->port_no, p->dev->name);

	SONOS_SPINLOCK(&p->br->lock);
	if (p->config_pending)
		br_transmit_config(p);
	SONOS_SPINUNLOCK(&p->br->lock);
}

void br_stp_timer_init(struct net_bridge *br)
{
	setup_timer(&br->hello_timer, br_hello_timer_expired,
		      (unsigned long) br);

	setup_timer(&br->tcn_timer, br_tcn_timer_expired,
		      (unsigned long) br);

	setup_timer(&br->topology_change_timer,
		      br_topology_change_timer_expired,
		      (unsigned long) br);

#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651, SWPBL-65582 */
	sonos_stp_timer_init(br);
#else
	setup_timer(&br->gc_timer, br_fdb_cleanup, (unsigned long) br);
#endif
}

void br_stp_port_timer_init(struct net_bridge_port *p)
{
	setup_timer(&p->message_age_timer, br_message_age_timer_expired,
		      (unsigned long) p);

	setup_timer(&p->forward_delay_timer, br_forward_delay_timer_expired,
		      (unsigned long) p);

	setup_timer(&p->hold_timer, br_hold_timer_expired,
		      (unsigned long) p);
}

/* Report ticks left (in USER_HZ) used for API */
unsigned long br_timer_value(const struct timer_list *timer)
{
	return timer_pending(timer)
		? jiffies_delta_to_clock_t(timer->expires - jiffies) : 0;
}
