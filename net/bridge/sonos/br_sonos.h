/* br_sonos.h - Sonos Bridge Extensions
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _BR_SONOS_H
#define _BR_SONOS_H

#include <linux/version.h>
#include "br_private.h"

/* br_sonos.c */
extern int sonos_initial_port_cost(struct net_device *dev);

extern struct
net_bridge_port_list_node *sonos_alloc_port_list(struct net_bridge_port *p);

extern void sonos_del_br(struct net_bridge *br);

extern struct net_bridge_port* br_find_port(const unsigned char *h_source,
					    struct net_bridge_port_list_node *pl);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
extern void _br_gc_timer_expired(unsigned long data);
#else
extern void _br_gc_timer_expired(struct work_struct *work);
#endif

extern int sonos_get_routing_capabilities(struct net_bridge *br, void __user *userbuf);

extern netdev_tx_t sonos_br_dev_xmit(struct net_bridge *br, struct sk_buff *skb,
				     const unsigned char *dest);

extern void sonos_br_get_stats64(struct net_bridge *dev,
				 struct rtnl_link_stats64 *stats);

extern void sonos_init_port(struct net_bridge_port *p);

extern void sonos_enable_leaf_ports(struct net_bridge *br,
				    struct net_bridge_port *p);

extern void sonos_disable_leaf_ports(struct net_bridge *br,
				     struct net_bridge_port *p);

extern bool change_bridge_id_static_mac(struct net_bridge *br);

extern const unsigned char *sonos_change_bridge_id(struct net_bridge *br,
						   const unsigned char *addr,
						   const unsigned char *br_mac_zero);

extern void sonos_del_nbp(struct net_bridge_port *p, struct net_bridge *br,
		struct net_device *dev, struct net_bridge_port_list_node *pl);

extern int sonos_add_bridge(struct net *net, const char *name,
			    struct net_device *dev);

extern void sonos_br_features_recompute(struct net_bridge *br);

extern int sonos_br_add_if(struct net_bridge *br, struct net_device *dev);

extern int sonos_br_del_if(struct net_bridge *br, struct net_device *dev);

extern unsigned char sonos_get_any_forwarding(struct net_bridge *br, void __user *userbuf);

extern int sonos_br_set_static_mac(struct net_bridge *br, void __user *userbuf);

extern void sonos_get_port_info(struct __port_info *p,
				struct net_bridge_port *pt);

extern int sonos_mod_port_addr(struct net_bridge *br, void __user *userbuf1, void __user *userbuf2);

extern int sonos_mod_port_dev(struct net_bridge *br, struct net_device *dev,
			      void __user *userbuf, int val);

extern int sonos_brctl_wrapper(struct net_bridge *br, unsigned long args[]);

extern void sonos_netdev_change(struct net_bridge *br, struct net_device *dev,
				struct net_bridge_port_list_node *pl);

extern void sonos_netdev_down(struct net_bridge_port_list_node *pl);

extern void sonos_netdev_up(struct net_bridge_port_list_node *pl);

#endif
