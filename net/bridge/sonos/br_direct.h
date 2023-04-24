/* br_direct.h - Sonos Direct Routing Extensions
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _BR_DIRECT_H
#define _BR_DIRECT_H

/* DEBUG */
/* #define BR_DEBUG_DIRECT 1 */

#define BR_DIRECT_STP_TIME (10*HZ)

/* br_direct.c */
extern int br_direct_unicast(struct net_bridge_port *src,
			     struct net_bridge_fdb_entry *fdbe,
			     struct sk_buff *skb,
			     void (*__stp_hook)(const struct net_bridge_port *src,
						const struct net_bridge_port *dst,
						struct sk_buff *skb),
			     void (*__direct_hook)(const struct net_bridge_port *src,
						   const struct net_bridge_port *dst,
						   struct sk_buff *skb));

#endif
