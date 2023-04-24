/* br_tunnel.h - Sonos Point-to-Point Packet Tunneling
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _BR_TUNNEL_H
#define _BR_TUNNEL_H

/* br_tunnel.c */
extern int br_add_p2p_tunnel(struct net_bridge *br,
                             struct net_device *dev,
                             unsigned char* daddr,
                             struct __add_p2p_entry* ape);

extern int br_set_p2p_tunnel_path_cost(struct net_bridge *br,
                                          struct net_device *dev,
                                          unsigned char* daddr,
                                          int path_cost);

extern int br_set_p2p_direct_enabled(struct net_bridge *br,
				     struct net_device *dev,
				     unsigned char* daddr,
				     int enabled);

extern int br_set_p2p_direct_addr(struct net_bridge *br,
				  struct net_device *dev,
				  unsigned char* daddr,
				  void __user *userbuf);

extern int br_set_p2p_tunnel_remote_stp_state(struct net_bridge *br,
                                          struct net_device *dev,
                                          unsigned char* daddr,
                                          int remote_stp_state);

extern int sonos_get_p2p_tunnel_states(struct net_bridge *br,
				       int val,
				       void __user *userbuf1,
				       void __user *userbuf2);

extern int br_del_p2p_tunnel(struct net_bridge *br,
                             struct net_device *dev,
                             unsigned char* daddr);

extern int br_add_p2p_tunnel_leaf(struct net_bridge *br,
                                  struct net_device *dev,
                                  unsigned char* daddr,
                                  struct __add_p2p_leaf_entry* ape);

extern int br_get_p2p_tunnel_states(struct net_bridge *br,
                                    struct net_device *dev,
                                    unsigned int max_records,
                                    unsigned char* state_data,
                                    unsigned int* state_data_len);

extern int br_add_uplink(struct net_bridge *br,
			 struct net_device *dev,
			 unsigned char* daddr);

#endif
