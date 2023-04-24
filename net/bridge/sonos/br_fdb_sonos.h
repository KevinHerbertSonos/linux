/* br_fdb_sonos.h - Sonos Forwarding Database Extensions
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _BR_FDB_SONOS_H
#define _BR_FDB_SONOS_H

#include <linux/jhash.h>
#include <linux/version.h>

#include "br_private.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,24)

#define br_sonos_fdb_update(br, port, source) \
	br_fdb_update(br, port, source, 0, false)

#define br_sonos_fdb_delete_by_port(br, port) \
	br_fdb_delete_by_port(br, port, 0, 0)

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)

#define br_sonos_fdb_update(br, port, source) \
	br_fdb_update(br, port, source, 0)

#define br_sonos_fdb_delete_by_port(br, port) \
	br_fdb_delete_by_port(br, port, 0)

#endif

/* br_fdb_sonos.c */

extern void br_fdb_delete_non_local(struct net_bridge *br);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
extern struct net_bridge_fdb_entry *br_fdb_get(struct net_bridge *br,
					       unsigned char *addr);
#endif

extern void br_fdb_put(struct net_bridge_fdb_entry *ent);

extern struct net_bridge_port *_get_direct_port(struct net_bridge *br,
					      const unsigned char *addr);

extern void br_fdb_update_dst_direct(struct net_bridge *br,
				     struct net_bridge_port *p);

extern void sonos_fdb_changeaddr(struct net_bridge_port_list_node *pl,
				 const unsigned char *newaddr);

extern void sonos_fdb_create(struct net_bridge_fdb_entry *fdb,
			     struct hlist_head *head,
			     struct net_bridge_port *source,
			     struct net_bridge_port *direct_dest,
			     const unsigned char *addr,
			     int is_local);

static inline int sonos_mac_hash(const unsigned char *mac)
{
	return jhash(mac, ETH_ALEN, 0) & (BR_HASH_SIZE - 1);
}

#endif
