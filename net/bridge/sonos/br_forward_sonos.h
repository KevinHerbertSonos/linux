/* br_forward_sonos.h - Sonos Forwarding Extensions
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _BR_FORWARD_SONOS_H
#define _BR_FORWARD_SONOS_H

#define MAC_ADDR_FMT    "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ADDR_VAR(x) (x)[0], (x)[1], (x)[2], (x)[3], (x)[4], (x)[5]

#include <linux/version.h>

#include "br_private.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,24)

#define br_sonos_vlan_tag_present(skb) \
	skb_vlan_tag_present(skb)

#define br_sonos_vlan_insert_tag(skb, vlan_proto, vlan_tx_tag) \
	vlan_insert_tag_set_proto(skb, vlan_proto, vlan_tx_tag)

#define br_sonos_vlan_tag_get(skb) \
	skb_vlan_tag_get(skb)

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)

#define br_sonos_vlan_tag_present(skb) \
	vlan_tx_tag_present(skb)

#define br_sonos_vlan_insert_tag(skb, vlan_proto, vlan_tx_tag) \
	__vlan_put_tag(skb, vlan_proto, vlan_tx_tag)

#define br_sonos_vlan_tag_get(skb) \
	vlan_tx_tag_get(skb)

#endif

/* SONOS: Globals */
extern unsigned char rincon_gmp_addr[6];
extern unsigned char broadcast_addr[6];
extern unsigned char igmp_ah_addr[6];
extern unsigned char igmp_ar_addr[6];
extern unsigned char igmp_amr_addr[6];
extern unsigned char upnp_addr[6];
extern unsigned char mdns_addr[6];
extern unsigned char ipv6_mcast_pref[2];

/* Filter functions */
static inline int br_mcast_dest_is_allowed(const unsigned char *addr)
{
    return (0 == memcmp(addr, broadcast_addr,  ETH_ALEN) ||
            0 == memcmp(addr, upnp_addr,       ETH_ALEN) ||
            0 == memcmp(addr, mdns_addr,       ETH_ALEN) ||
            0 == memcmp(addr, igmp_ah_addr,    ETH_ALEN) ||
            0 == memcmp(addr, igmp_ar_addr,    ETH_ALEN) ||
            0 == memcmp(addr, igmp_amr_addr,   ETH_ALEN) ||
            0 == memcmp(addr, ipv6_mcast_pref, sizeof(ipv6_mcast_pref)));
}

static inline int br_mcast_dest_is_allowed_from_local(const unsigned char *addr)
{
    return (0 == memcmp(addr, rincon_gmp_addr,  ETH_ALEN));
}

/* br_forward_sonos.c */
extern void br_deliver_direct(const struct net_bridge_port *from,
                              const struct net_bridge_port *to,
                              struct sk_buff *skb);

extern void br_deliver_bpdu(const struct net_bridge_port *to,
                            struct sk_buff *skb);

extern void br_forward_direct(const struct net_bridge_port *from,
			      const struct net_bridge_port *to,
			      struct sk_buff *skb);

extern unsigned char br_is_dhcp(struct sk_buff *skb,
				struct iphdr **oiph,
				struct udphdr **oudph,
				unsigned char **odhcph);

extern void br_log_dhcp(struct udphdr *udph,
			unsigned char *dhcph);

extern int _br_forward_finish(const struct net_bridge_port *to,
			      struct sk_buff *skb);

extern void br_true_flood(struct net_bridge *br, struct net_bridge_port *from,
			  struct sk_buff *skb, int clone, int p2p_flood_deliver,
			  void (*__packet_hook)(const struct net_bridge_port *p,
						struct sk_buff *skb));

extern void br_mcast(struct net_bridge *br, struct net_bridge_port *from,
		     struct sk_buff *skb, int clone,
		     void (*__packet_hook)(const struct net_bridge_port *p, struct sk_buff *skb),
		     struct net_bridge_mcast_entry *me);

extern void br_mcast_direct(struct net_bridge *br, struct net_bridge_port *from,
			    struct sk_buff *skb, int clone,
			    void (*__packet_hook)(const struct net_bridge_port *p,
						  struct sk_buff *skb),
			    struct net_bridge_mcast_entry *me);


extern struct sk_buff * __br_encap_skb(struct sk_buff *skb,
				       const unsigned char *dest,
				       const unsigned char *src,
				       unsigned short proto);

extern int should_proxy(const struct net_bridge_port *p_arriving,
			const struct net_bridge_port *p_outgoing);

extern struct sk_buff *br_handle_frame_std(struct net_bridge_port *p,
					   struct sk_buff *skb,
					   const unsigned char *src,
					   const unsigned char *dest,
					   int direct);

extern void __sonos_br_deliver(const struct net_bridge_port *to,
			       struct sk_buff *skb);

extern void sonos_br_deliver(const struct net_bridge_port *from,
			     const struct net_bridge_port *to,
			     struct sk_buff *skb);

extern void sonos_flood_deliver(struct net_bridge *br,
				struct net_bridge_port *from,
				struct sk_buff *skb, int clone);

extern void __sonos_br_forward(const struct net_bridge_port *to,
			       struct sk_buff *skb);

extern void sonos_br_forward(const struct net_bridge_port *from,
			     const struct net_bridge_port *to,
			     struct sk_buff *skb);

extern void sonos_flood_forward(struct net_bridge *br,
				struct net_bridge_port *from,
				struct sk_buff *skb, int clone);

extern void sonos_br_flood(struct net_bridge *br,
			   struct net_bridge_port *from,
			   struct sk_buff *skb, int clone,
			   void (*__packet_hook)(const struct net_bridge_port *p,
						 struct sk_buff *skb));

extern void sonos_pass_frame_up(struct net_bridge *br, struct sk_buff *skb);

extern int sonos_handle_frame_finish(struct net_bridge_port *p, struct sk_buff *skb);

extern rx_handler_result_t sonos_br_handle_frame(struct sk_buff **pskb);

extern struct sk_buff *sonos_handle_frame(struct net_bridge_port_list_node *pl,
					  struct sk_buff *skb);

#ifdef CONFIG_SONOS_BRIDGE_PROXY
/*
 * This is a clone of inc_mac_addr() in setmac.c. That function
 * is used to increment the lower three bytes (non-OUI portion)
 * of the MDP SERIAL base address when assigning unique MAC
 * addresses to Ethernet and WiFi interfaces.
 */
static inline void inc_mac_addr( unsigned char *mac_addr )
{
	int i;

	/* Only increment the lower 3 bytes */
	for (i = 5; i >= 3; i--) {
		if (0 != ++mac_addr[i]) {
			break;
		}
	}
}

/* Check whether a serial (bridge) address matches a SonosNet address.
 * If the bridge address is even, then it's simply checking that they
 * match with the last bit of the bridge address set. If the bridge
 * address is odd, it's checking if they match with the second bit of
 * the first byte of the bridge set (the 'local admin' bit), or if
 * they match against a true increment (+1) accounting for wraps.
 */
static inline int
br_check_sonosnet_serial_match(const unsigned char* sonosnet,
			       const unsigned char* serial)
{
	int ret;
	unsigned char tmp_addr[ETH_ALEN];

	if (serial[ETH_ALEN - 1] & 0x1) { /* odd */
		if (sonosnet[0] & 0x2) {
			ret = !memcmp(sonosnet + 1, serial + 1, ETH_ALEN - 1);
			ret = ret && ((serial[0] | 0x2) == sonosnet[0]);
		} else {
			memcpy(tmp_addr, serial, sizeof(tmp_addr));
			inc_mac_addr(tmp_addr);
			ret = !memcmp(sonosnet, tmp_addr, sizeof(tmp_addr));
		}
	} else { /* even */
		ret = !memcmp(sonosnet, serial, ETH_ALEN - 1);
		ret = ret && ((serial[ETH_ALEN - 1] | 0x1) == sonosnet[ETH_ALEN - 1]);
	}
	return ret;
}
#endif

/* REVIEW:
 *
 * __br_skb_*: This uses skb->cb to stash data in.  This conflicts with
 * netfilter, but we (Sonos) do not use netfilter (and we've already broken it
 * in other ways).  Looks like cb[4] is safe to use even w/ netfilter, but
 * whatever...
 */
static inline void __br_skb_mark_direct(struct sk_buff *skb, int direct)
{
	BR_SKB_CB(skb)->direct = direct;
}

static inline int __br_skb_marked_direct(struct sk_buff *skb)
{
	return BR_SKB_CB(skb)->direct;
}

static inline int sonos_should_deliver(const struct net_bridge_port *p_arriving,
				       const struct net_bridge_port *p_outgoing)
{
	/* Don't forward if the port is the same */
	if (p_arriving == p_outgoing) return 0;

	if (should_proxy(p_arriving, p_outgoing)) return 1;

	/* if the specified port is disabled, don't forward it */
	if (p_outgoing->state != BR_STATE_FORWARDING
	    || p_outgoing->remote_state == BR_STATE_BLOCKING)
		return 0;

	return 1;
}


#endif
