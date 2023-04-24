/* br_stp_sonos.c - Sonos Spanning Tree Extensions
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <linux/version.h>
#include "br_private.h"

#include "br_stp_sonos.h"
#include "br_priority.h"
#include "br_forward_sonos.h"
#include "br_mcast.h"
#include "br_proxy.h"
#include "br_sonos.h"

void sonos_stp_timer_init(struct net_bridge *br)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	timer_setup(&br->mcast_timer, br_mcast_timer_expired, 0);

	INIT_DELAYED_WORK(&br->gc_work, _br_gc_timer_expired);

#ifdef CONFIG_SONOS_BRIDGE_PROXY /* SWPBL-25027 */
	timer_setup(&br->dupip_timer, br_dupip_timer_expired, 0);
#endif /* CONFIG_SONOS_BRIDGE_PROXY */
#else
	setup_timer(&br->mcast_timer, br_mcast_timer_expired,
		      (unsigned long) br);

	setup_timer(&br->gc_timer, _br_gc_timer_expired, (unsigned long) br);

#ifdef CONFIG_SONOS_BRIDGE_PROXY /* SWPBL-25027 */
	setup_timer(&br->dupip_timer, br_dupip_timer_expired,
		    (unsigned long) br);
#endif /* CONFIG_SONOS_BRIDGE_PROXY */
#endif
}

void sonos_update_message_age(struct net_bridge *br,
			      struct net_bridge_port *root,
			      struct br_config_bpdu *bpdu,
			      int message_age_incr)
{
	bpdu->message_age = br->max_age
		- (root->message_age_timer.expires - jiffies)
		+ message_age_incr;

	/* Yes, this does happen.  message_age_timer.expires is
	 * generally set a little further in the future than we'd like
	 * (it rounds to 4 jiffies), so it is possible for the above
	 * calculation to go negative.  That makes something that
	 * arrived really early appear to have arrived really late,
	 * which eventually ends up causing the interface to flap.
	 */
	if (bpdu->message_age <= 0) {

		//printk("br: %d, expired (root=%d)?\n",
		//p->port_no, root->port_no);

		if (timer_pending(&root->message_age_timer)) {
			//printk("br: %d, fixed\n", p->port_no);
			bpdu->message_age = message_age_incr;
		} else {
			bpdu->message_age = br->max_age;
		}
	}

	//printk("br: %d, age: %d, %d\n",
	//p->port_no, bpdu.message_age, br->max_age);
}

void sonos_check_max_age_changed(struct net_bridge *br,
				 struct net_bridge_port *p,
				 const struct br_config_bpdu *bpdu,
				 unsigned long old_max_age)
{
	/* SONOS: If max_age changed we need to bump up the
	 *        timer again.
	 */
	if (br->max_age != old_max_age) {
		printk("br: max age %ld -> %ld\n",
		       old_max_age, br->max_age);
		mod_timer(&p->message_age_timer, jiffies
			  + (p->br->max_age - bpdu->message_age));
	}
}

/* called under bridge lock */
int br_stp_mod_port_addr(struct net_bridge *br,
                         unsigned char* oldaddr,
                         unsigned char* newaddr)
{
	struct net_bridge_port *p;

	/*
	 * Find the port w/ the old mac address.  This should be unique.
	 *
	 * This could all get entertaining if we also find a p2p port w/ the
	 * new MAC.  If so, I don't think we want to do anything since we're
	 * already going to end up with a topology change.
	 */
	list_for_each_entry(p, &br->port_list, list) {
		if (p->is_p2p && !memcmp(&p->p2p_dest_addr, oldaddr, ETH_ALEN)) {
			printk("br: Moving %d from "
			       "%02x:%02x:%02x:%02x:%02x:%02x "
			       "to %02x:%02x:%02x:%02x:%02x:%02x\n",
			       p->port_no,
			       oldaddr[0], oldaddr[1], oldaddr[2],
			       oldaddr[3], oldaddr[4], oldaddr[5],
			       newaddr[0], newaddr[1], newaddr[2],
			       newaddr[3], newaddr[4], newaddr[5]);
			memcpy(&p->p2p_dest_addr, newaddr, ETH_ALEN);
			return 0;
		}
	}

	return -EINVAL;
}

int _br_stp_mod_port_dev(struct net_bridge_port *p,
                         unsigned char* oldaddr,
                         struct net_device* dev)
{
	if (p->is_p2p && !memcmp(&p->p2p_dest_addr, oldaddr, ETH_ALEN)) {
		struct net_device *old_dev = p->dev;
		struct net_bridge_port_list_node* pl_curr  = p->dev->br_port_list;
		struct net_bridge_port_list_node* pl_trail = NULL;

		if (p->dev == dev)
			return 1;

		dev_hold(dev);
		dev_hold(old_dev);

		printk("br: Moving %d from %s to %s\n",
			   p->port_no,
			   old_dev->name,
			   dev->name);

		/* find the entry in the original dev's list */
		while (pl_curr) {
			if (pl_curr->port == p) {
				break;
			}
			pl_trail = pl_curr;
			pl_curr  = pl_curr->next;
		}

		if (pl_curr) {
			/* Remove from one dev list */
			if (pl_trail) {
				pl_trail->next = pl_curr->next;
			} else {
				old_dev->br_port_list = pl_curr->next;
			}

			/* Add to the other */
			pl_curr->next      = dev->br_port_list;
			dev->br_port_list  = pl_curr;

			/* Update the actual port to point to the dev */
			p->dev = dev;

			/* MCS-1172: _new_p2p_tunnel has
			 * done a dev_hold on the old
			 * device, and _del_p2p_tunnel
			 * will do a dev_put on the new
			 * device if the p2p link is
			 * deleted. To account for this,
			 * we must do a dev_put on the
			 * old_dev and a dev_hold on the
			 * new dev.
			 */
			dev_put(old_dev);
			dev_hold(dev);
		}

		dev_put(old_dev);
		dev_put(dev);

		return 1;
	}
	return 0;
}
/* called under bridge lock */
int br_stp_mod_port_dev(struct net_bridge *br,
			unsigned char* oldaddr,
			struct net_device* dev)
{
	struct net_bridge_port *p;

	/*
	 * Find the port w/ the old mac address.  This should be unique.
	 */
	list_for_each_entry(p, &br->port_list, list) {
		if(_br_stp_mod_port_dev(p, oldaddr, dev)) {
			return 0;
		}
	}

	list_for_each_entry(p, &br->leaf_list, list) {
		if(_br_stp_mod_port_dev(p, oldaddr, dev)) {
			return 0;
		}
	}

	return -EINVAL;
}

static const unsigned char header[6] = {0x42, 0x42, 0x03, 0x00, 0x00, 0x00};

/* NO locks */
int br_stp_handle_bpdu(struct net_bridge_port *p, struct sk_buff *skb)
{
	struct net_bridge *br = p->br;
	unsigned char *buf;

	/* need at least the 802 and STP headers */
	if (!pskb_may_pull(skb, sizeof(header)+1) ||
	    memcmp(skb->data, header, sizeof(header)))
		goto err;

	buf = skb_pull(skb, sizeof(header));

	spin_lock_bh(&br->lock);
	if (p->state == BR_STATE_DISABLED
	    || !(br->dev->flags & IFF_UP)
	    || !br->stp_enabled)
		goto out;

	if (buf[0] == BPDU_TYPE_CONFIG) {
		struct br_config_bpdu bpdu;

		if (!pskb_may_pull(skb, 32))
		    goto out;

		buf = skb->data;
		bpdu.topology_change = (buf[1] & 0x01) ? 1 : 0;
		bpdu.topology_change_ack = (buf[1] & 0x80) ? 1 : 0;

		bpdu.root.prio[0] = buf[2];
		bpdu.root.prio[1] = buf[3];
		bpdu.root.addr[0] = buf[4];
		bpdu.root.addr[1] = buf[5];
		bpdu.root.addr[2] = buf[6];
		bpdu.root.addr[3] = buf[7];
		bpdu.root.addr[4] = buf[8];
		bpdu.root.addr[5] = buf[9];
		bpdu.root_path_cost =
			(buf[10] << 24) |
			(buf[11] << 16) |
			(buf[12] << 8) |
			buf[13];
		bpdu.bridge_id.prio[0] = buf[14];
		bpdu.bridge_id.prio[1] = buf[15];
		bpdu.bridge_id.addr[0] = buf[16];
		bpdu.bridge_id.addr[1] = buf[17];
		bpdu.bridge_id.addr[2] = buf[18];
		bpdu.bridge_id.addr[3] = buf[19];
		bpdu.bridge_id.addr[4] = buf[20];
		bpdu.bridge_id.addr[5] = buf[21];
		bpdu.port_id = (buf[22] << 8) | buf[23];

		bpdu.message_age = br_sonos_get_ticks(buf+24);
		bpdu.max_age = br_sonos_get_ticks(buf+26);
		bpdu.hello_time = br_sonos_get_ticks(buf+28);
		bpdu.forward_delay = br_sonos_get_ticks(buf+30);

		br_received_config_bpdu(p, &bpdu);
	}

	else if (buf[0] == BPDU_TYPE_TCN) {
		br_received_tcn_bpdu(p);
	}
 out:
	spin_unlock_bh(&br->lock);
 err:
	kfree_skb(skb);
	return 0;
}

void sonos_send_bpdu(struct net_bridge_port *p, const unsigned char *data,
		     int length, int size, struct sk_buff *skb)
{
	skb->priority = BR_BPDU_PRIORITY;

	sonos_set_abs_mac_header(skb, skb_put(skb, size));
	memcpy(skb_mac_header(skb), bridge_ula, ETH_ALEN);
	memcpy(skb_mac_header(skb) + ETH_ALEN, skb->dev->dev_addr, ETH_ALEN);
	*(skb_mac_header(skb) + 2*ETH_ALEN) = 0;
	*(skb_mac_header(skb) + 2*ETH_ALEN + 1) = length;
	sonos_set_abs_network_header(skb, skb_mac_header(skb) + 2*ETH_ALEN + 2);
	memcpy(skb_network_header(skb), data, length);
	memset(skb_network_header(skb) + length, 0xa5, size - length - 2*ETH_ALEN - 2);

	/* SONOS: We need to tunnel p2p frames, so call our xmit */
	br_deliver_bpdu(p, skb);
}
