/* br_fdb_sonos.c - Sonos Forwarding Database Extensions
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <linux/version.h>
#include "br_private.h"

#include "br_fdb_sonos.h"
#include "br_priority.h"

void br_fdb_delete_non_local(struct net_bridge *br)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	struct net_bridge_fdb_entry *f;
	struct hlist_node *n;

	spin_lock_bh(&br->hash_lock);

	hlist_for_each_entry_safe(f, n, &br->fdb_list, fdb_node) {
		if (!f->is_static)
			br_sonos_fdb_delete(br, f);
	}
	spin_unlock_bh(&br->hash_lock);
#else
	int i;

	spin_lock_bh(&br->hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct net_bridge_fdb_entry *f;
		struct hlist_node *n;

		hlist_for_each_entry_safe(f, n, &br->hash[i], hlist) {
			if (!f->is_static)
				br_sonos_fdb_delete(br, f);
		}
	}
	spin_unlock_bh(&br->hash_lock);
#endif
}

void br_fdb_update_dst_direct(struct net_bridge *br, struct net_bridge_port *p)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	struct hlist_node *h, *g;
	spin_lock_bh(&br->hash_lock);
	hlist_for_each_safe(h, g, &br->fdb_list) {
		struct net_bridge_fdb_entry *f
			= hlist_entry(h, struct net_bridge_fdb_entry, fdb_node);
		if (0 == f->dst_direct &&
		    ether_addr_equal(f->key.addr.addr, p->direct_addr)) {
			f->dst_direct = p;
		}
	}
	spin_unlock_bh(&br->hash_lock);
#else
	int i;

	/* REVIEW: Could we just look up p->direct_addr in the FDB and update
	 *         that single entry?  Oh well, this works and is rarely
	 *         called.
	 */
	spin_lock_bh(&br->hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct hlist_node *h, *g;

		hlist_for_each_safe(h, g, &br->hash[i]) {
			struct net_bridge_fdb_entry *f
				= hlist_entry(h, struct net_bridge_fdb_entry, hlist);
			if (0 == f->dst_direct &&
			    ether_addr_equal(f->addr.addr, p->direct_addr)) {
				f->dst_direct = p;
			}
		}
	}
	spin_unlock_bh(&br->hash_lock);
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
/* Interface used by ATM hook that keeps a ref count */
struct net_bridge_fdb_entry *br_fdb_get(struct net_bridge *br,
                                        unsigned char *addr)
{
	struct net_bridge_fdb_entry *fdb;

	rcu_read_lock();
	fdb = __br_fdb_get(br, addr, 0);
	if (fdb)
		atomic_inc(&fdb->use_count);
	rcu_read_unlock();
	return fdb;
}
#endif

/* Set entry up for deletion with RCU  */
void br_fdb_put(struct net_bridge_fdb_entry *ent)
{
	if (atomic_dec_and_test(&ent->use_count))
		call_rcu(&ent->rcu, br_fdb_rcu_free);
}

struct net_bridge_port *_get_direct_port(struct net_bridge *br,
				       const unsigned char *addr)
{
	struct list_head *ports = &br->port_list;
	struct net_bridge_port *p;

	list_for_each_entry_rcu(p, ports, list) {
		if (0 == memcmp(addr, p->direct_addr, 6)) {
			return (p);
		}
	}
	return 0;
}

void sonos_fdb_changeaddr(struct net_bridge_port_list_node *pl, const unsigned char *newaddr)
{
	struct net_bridge *br = pl->port->br;
	struct net_bridge_port_list_node *pl_curr = pl;
	struct hlist_node *h;
	int i;

	spin_lock_bh(&br->hash_lock);

	/* Search all chains since old address/hash is unknown */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	for (i = 0; i < BR_HASH_SIZE; i++) {
		hlist_for_each(h, &br->hash[i]) {
#else
		hlist_for_each(h, &br->fdb_list) {
#endif
			struct net_bridge_fdb_entry *f;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
			f = hlist_entry(h, struct net_bridge_fdb_entry, hlist);
#else
			f = hlist_entry(h, struct net_bridge_fdb_entry, fdb_node);
#endif
			while (pl_curr != NULL) {
				if (f->dst == pl_curr->port && f->is_local) {
					br_sonos_fdb_delete(br, f);
					br_sonos_fdb_insert(br, pl_curr->port, newaddr);
					goto done;
				}
				pl_curr = pl_curr->next;
			}
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	} /* End of the for loop for kernels older than 5.4 */
#else
	(void)i;
#endif

done:
	spin_unlock_bh(&br->hash_lock);
}

void sonos_fdb_create(struct net_bridge_fdb_entry *fdb,
		      struct hlist_head *head,
		      struct net_bridge_port *source,
		      struct net_bridge_port *direct_dest,
		      const unsigned char *addr,
		      int is_local)
{
	atomic_set(&fdb->use_count, 1);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	/* br_fdb.c handles adding fdb entry to list from 5.4 */
	hlist_add_head_rcu(&fdb->hlist, head);
	fdb->vlan_id = 0;
#else
	fdb->key.vlan_id = 0;
#endif
	fdb->dst = source;
	fdb->dst_direct = direct_dest;
	fdb->is_local = is_local;
	fdb->is_static = is_local;
	fdb->ageing_timer = jiffies;
	fdb->priority = br_priority_for_addr(addr);
}
