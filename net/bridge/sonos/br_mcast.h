/* br_mcast.h - Sonos Multicast Extensions
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _BR_MCAST_H
#define _BR_MCAST_H

#include <linux/version.h>

/* br_mcast.c */
extern void br_mcast_transmit_grouplist(struct net_bridge *br);

extern void br_mcast_handle_grouplist(struct net_bridge *br,
                                      struct net_bridge_port *source,
                                      struct sk_buff *skb);

extern void br_mcast_destroy_list(struct net_bridge *br);

extern void br_mcast_age_list(struct net_bridge *br);

extern void br_mcast_delete_by_port(struct net_bridge *br,
                                    struct net_bridge_port *p);

extern void br_mcast_put(struct net_bridge_mcast_entry *me);

extern struct net_bridge_mcast_entry *br_mcast_get(struct net_bridge *br,
                                                   const unsigned char *addr);

extern int br_mcast_fdb_get_entries(struct net_bridge *br,
                                    unsigned char *buf,
                                    unsigned int buf_len,
                                    int offset);

extern void br_mcast_update_dst_direct(struct net_bridge *br,
                                       struct net_bridge_port *p);

extern void br_mcast_check(struct sk_buff *skb,
			   struct net_bridge *br,
			   struct net_bridge_port *p);

extern int br_mcast_is_management_header(struct ethhdr *ether);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
extern void br_mcast_timer_expired(struct timer_list *t);
#else
extern void br_mcast_timer_expired(unsigned long arg);
#endif

extern void sonos_set_multicast_list(struct net_device *dev);

extern void br_stats_update(struct net_bridge *br,
			    const unsigned char *src,
			    const unsigned char *dest);

extern int sonos_get_stats(struct net_bridge *br, void __user *userbuf);

static inline void br_stats_init(struct net_bridge *br)
{
	memset(&br->br_stats, 0, sizeof(br->br_stats));
	br->br_stats.rx_start_time = jiffies;
}

/* The call to sonos_set_abs_mac_header(...) is counterintuitive. While
 * skb->mac_header is relative to skb->head in Linux-4.4.24-mtk, and is absolute
 * in Linux-3.10, it is set by skb_set_mac_header(...) using an offset relative
 * to skb->data in both kernel verions. Thus, the mac header should be set
 * relative to skb->data. The same applies to skb->network_header.
 */
static inline void sonos_set_abs_mac_header(struct sk_buff *skb,
					    unsigned char *abs_mac_header)
{
	skb_set_mac_header(skb, abs_mac_header - skb->data);
}

static inline void sonos_set_abs_network_header(struct sk_buff *skb,
						unsigned char *abs_net_header)
{
	skb_set_network_header(skb, abs_net_header - skb->data);
}

void br_udp_overwrite_ip(struct sk_buff *skb, uint32_t dst_ip);

#endif
