/* br_uplink.h - Sonos Uplink Detection
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _BR_UPLINK_H
#define _BR_UPLINK_H

#include "br_private_stp.h"

/* br_uplink.c */
extern void br_uplink_xmit(struct net_bridge *br,
			   struct sk_buff *skb,
			   const char *dest);

extern void br_uplink_proxy(struct net_bridge *br,
			    struct net_bridge_port *p,
			    struct sk_buff *skb,
			    const char *dest);

extern int br_set_uplink_mode(struct net_bridge *br, int enable);

extern struct sk_buff *br_uplink_handle_frame(struct net_bridge_port *p,
					      struct sk_buff *skb,
					      const unsigned char *src,
					      const unsigned char *dst);

#endif
