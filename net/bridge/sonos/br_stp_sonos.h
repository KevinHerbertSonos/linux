/* br_stp_sonos.h - Sonos Spanning Tree Extensions
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _BR_STP_SONOS_H
#define _BR_STP_SONOS_H

#include "br_private_stp.h"

/* br_stp_sonos.c */
void sonos_stp_timer_init(struct net_bridge *br);

extern void sonos_update_message_age(struct net_bridge *br,
				     struct net_bridge_port *root,
				     struct br_config_bpdu *bpdu,
				     int message_age_incr);

extern void sonos_check_max_age_changed(struct net_bridge *br,
					struct net_bridge_port *p,
					const struct br_config_bpdu *bpdu,
					unsigned long old_max_age);

extern int br_stp_mod_port_addr(struct net_bridge *br,
				unsigned char* oldaddr,
                                unsigned char* newaddr);

extern int br_stp_mod_port_dev(struct net_bridge *br,
			       unsigned char* oldaddr,
			       struct net_device *dev);

extern int br_stp_handle_bpdu(struct net_bridge_port *p, struct sk_buff *skb);

extern void sonos_send_bpdu(struct net_bridge_port *p, const unsigned char *data,
			    int length, int size, struct sk_buff *skb);

static inline int sonos_bpdu_size(int length)
{
	int size = length + 2*ETH_ALEN + 2;

	if (size < 60) {
		size = 60;
	}

	return size;
}

#endif
