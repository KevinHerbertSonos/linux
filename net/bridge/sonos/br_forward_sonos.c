/* br_forward_sonos.c - Sonos Forwarding Extensions
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <linux/version.h>
#include <linux/netfilter_bridge.h>

#include "br_private.h"

#include "br_forward_sonos.h"
#include "br_direct.h"
#include "br_fdb_sonos.h"
#include "br_proxy.h"
#include "br_mcast.h"
#include "br_uplink.h"
#include "br_sonos.h"
#include "br_stp_sonos.h"

// #define DEBUG_BR_FLOOD 1

const unsigned char bridge_ula[6] = { 0x01, 0x80, 0xc2, 0x00, 0x00, 0x00 };

unsigned char broadcast_addr[]  = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
unsigned char upnp_addr[]       = {0x01, 0x00, 0x5e, 0x7f, 0xff, 0xfa};
unsigned char mdns_addr[]       = {0x01, 0x00, 0x5e, 0x00, 0x00, 0xfb};
unsigned char igmp_ah_addr[]    = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x01};
unsigned char igmp_ar_addr[]    = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x02};
unsigned char igmp_amr_addr[]   = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x16};
unsigned char rincon_gmp_addr[] = {0x01, 0x0e, 0x58, 0xdd, 0xdd, 0xdd};
unsigned char ipv6_mcast_pref[] = {0x33, 0x33};

#ifdef CONFIG_SONOS_BRIDGE_PROXY
static inline int
should_proxy_up(const struct net_bridge_port *p_arriving,
                const struct net_bridge_port *p_outgoing) {
        if (!p_outgoing->br->proxy_mode || !p_arriving)
		return 0;
	return (p_outgoing->is_uplink || (p_outgoing->direct_enabled && !p_outgoing->is_satellite)) && p_arriving->is_satellite;
}
#endif

int should_proxy(const struct net_bridge_port *p_arriving,
		 const struct net_bridge_port *p_outgoing) {
#ifdef CONFIG_SONOS_BRIDGE_PROXY
	/* If we're proxying, assume here that we'll deliver it if it's
	 * sourced or destined from the uplink port and the other port
	 * is a satellite, or if both ports are satellites.
	 *
	 * This code has been written to explicitly allow early return;
	 * the optimizer didn't appear to be generating code that
	 * allowed this when we were assigning each logical condition
	 * to a variable and then ORing them.
	 *
	 * We can't assume that p_arriving is non-zero because this
	 * function is called when a packet is generated on this machine
	 * and doesn't have a "port". However, in such a case, we're
	 * not doing any proxying, either.
	 */

	unsigned char proxy_up, proxy_down, both_sat;

	if (!p_outgoing->br->proxy_mode || !p_arriving)
		return 0;

	proxy_up = should_proxy_up(p_arriving, p_outgoing);
	if (proxy_up)
		return 1;

	proxy_down = p_arriving->is_uplink && p_outgoing->is_satellite;
	if (proxy_down)
		return 1;

	both_sat = p_arriving->is_satellite && p_outgoing->is_satellite;
	if (both_sat)
		return 1;

	/* if (proxy_up || proxy_down || both_sat) return 1; */

#endif
        return 0;
}
static inline int should_deliver(const struct net_bridge_port *p_arriving,
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

struct sk_buff * __br_encap_skb(struct sk_buff *skb,
				const unsigned char *dest,
				const unsigned char *src,
				unsigned short proto)
{
	struct ethhdr *ether;

	/* Get some space for another header */
	if ((skb_headroom(skb) < ETH_HLEN) &&
	    (pskb_expand_head(skb, ETH_HLEN - skb_headroom(skb), 0,
			      GFP_ATOMIC))) {
		dev_kfree_skb(skb);
		return NULL;
	}

	/* Find current ethernet header */
	ether = eth_hdr(skb);

	skb_push(skb, ETH_HLEN);
	skb->mac_header -= ETH_HLEN;
	ether = eth_hdr(skb);

	/* Set up the addresses and proto */
	memcpy(ether->h_dest, dest, ETH_ALEN);
	memcpy(ether->h_source, src, ETH_ALEN);
	ether->h_proto = htons(proto);

	return skb;
}

static void __dev_queue_wrapper_xmit(const struct net_bridge_port *p, struct sk_buff *skb)
{
	/* 3.10 kernel will remove vlan header and store it in vlan_tci of skb.
	 * We need to recover vlan header here to avoid kernel inserting vlan header
	 * before p2p tunneling header.
	 */
	if (br_sonos_vlan_tag_present(skb)) {
		skb = br_sonos_vlan_insert_tag(skb, skb->vlan_proto,
					       br_sonos_vlan_tag_get(skb));
		if (!skb) {
			printk("Returned null skb during vlan header recovering before sending on %s\n",
				p->dev->name);
			return;
		}
		skb->vlan_tci = 0;
	}
	/* Encapsulate tunneled p2p traffic (with some exceptions) */
	if (p->is_p2p) {
		if (!p->is_unencap)  {

			/* Encapsulate the original ethernet frame inside our own frame
			 * addressed directly to the point-to-point link
			 *
			 * destination is point-to-point endpoint
			 * source is MAC address of outgoing interface
			 * protocol number for tunneled frames (direct is
			 * unlearned)
			 */
			skb = __br_encap_skb(skb,
					     p->p2p_dest_addr,
					     skb->dev->dev_addr,
					     __br_skb_marked_direct(skb) ?
					     BR_TUNNEL2_PROTOCOLNUM :
					     BR_TUNNEL_PROTOCOLNUM);

		} else if (p->is_unicast) {
			/* destination is point-to-point endpoint */
			memcpy(eth_hdr(skb)->h_dest, p->p2p_dest_addr, ETH_ALEN);
		}

	}

	/* Encapsulate multicast management traffic going onto the network */
	if ((!p->is_p2p || p->is_uplink) && skb &&
	    br_mcast_is_management_header(eth_hdr(skb))) {

		skb = __br_encap_skb(skb,
				     broadcast_addr,
				     eth_hdr(skb)->h_source,
				     BR_MCAST_GL_PROTOCOLNUM);
	}

	/* If we still have something to transmit, go for it */
	if (skb) {
		dev_queue_xmit(skb);
	}
}

void __dev_queue_push_xmit(const struct net_bridge_port *to,
			   struct sk_buff *skb)
{
	skb_push(skb, ETH_HLEN);
        __dev_queue_wrapper_xmit(to, skb);
}

#ifdef CONFIG_SONOS_BRIDGE_PROXY
static void _set_sat_ip(struct net_bridge_port *sat_port, u32 new_ip)
{
	unsigned char is_changing_to_current;
	unsigned char is_changing_from_current;

	int run_check = 0;
	u32 current_sat_ip = sat_port->sat_ip;
	u32 current_br_ip = sat_port->br->current_ipv4_addr;

	/* Check if I even want to set it */
	if (current_sat_ip == new_ip || !new_ip) {
		return;
	}

	/* Check if we may be transitioning in or out of conflict. */
	is_changing_to_current = (new_ip == current_br_ip);
	is_changing_from_current = (current_sat_ip == current_br_ip);

	if (is_changing_to_current || is_changing_from_current) {
		run_check = 1;
	}

	sat_port->sat_ip = new_ip;

	if (run_check) {
		br_dupip_check(sat_port->br);
	}
}

/*
 * Check whether a frame is a DHCP datagram.
 */
unsigned char br_is_dhcp(struct sk_buff *skb,
			  struct iphdr **oiph,
			  struct udphdr **oudph,
			  unsigned char **odhcph)
{

	struct iphdr *iph;
	struct udphdr *udph;

	int total_offset = 0;

	skb_reset_network_header(skb);

	total_offset += sizeof(struct iphdr);
	if (!pskb_may_pull(skb, total_offset))
		return 0;

	iph = ip_hdr(skb);
	*oiph = iph;

	if (iph->protocol != IPPROTO_UDP)
		return 0;

        /*
         * From RFC-791, iph->ihl is the length of the internet header
         * in 32-bit words. The minimum value for a correct header is 5.
         */
	skb_set_transport_header(skb, (iph->ihl * 4));

	/* subtract size of iphdr because iph->ihl includes it */
	total_offset += (iph->ihl * 4 - sizeof(struct iphdr) + sizeof(struct udphdr));
	if (!pskb_may_pull(skb, total_offset))
		return 0;

	udph = udp_hdr(skb);
	*oudph = udph;

	if (!(
		(udph->source == htons(67) && udph->dest == htons(68)) ||
		(udph->source == htons(68) && udph->dest == htons(67))
	     ))
		return 0;

	total_offset += (ntohs(udph->len) - sizeof(struct udphdr));

	if (!pskb_may_pull(skb, total_offset))
		return 0;

	*odhcph = (unsigned char *)(udph + 1);

        return 1;
}

void br_log_dhcp(struct udphdr *udph, unsigned char *dhcph)
{
	const unsigned int tid_offset = 4;
	const unsigned int cip_offset = 12;
	const unsigned int chaddr_offset = 28;
	const unsigned int first_option_offset = 240;
	const unsigned char message_type_code = 0x35;

	const char *msg_names[] = { "???", "DIS", "OFR", "REQ", "DEC", "ACK", "NAK", };

	unsigned int idx = 0;
	const int dhcp_len = ntohs(udph->len) - sizeof(struct udphdr);
	if (dhcp_len <= first_option_offset + 2) {
		return;
	}

	if (dhcph[first_option_offset] == message_type_code) {
		if (dhcph[first_option_offset + 2] < sizeof(msg_names)/sizeof(msg_names[0])) {
			idx = dhcph[first_option_offset + 2];
		}
	}

	printk("br proxy dhcp: %s 0x%x %pM %pI4\n",
		msg_names[idx],
		ntohl(*(__u32 *)&dhcph[tid_offset]),
		&dhcph[chaddr_offset],
		(__u32 *)&dhcph[cip_offset]);
}

/* Check for a DHCP frame. Set the broadcast bit and recalculate the
 * checksum if we find it.
 */
static void _br_proxy_mangle_for_ip(struct sk_buff *skb)
{
	struct iphdr *iph = NULL;
	struct udphdr *udph = NULL;
	unsigned char *dhcph = NULL;

	int dhcp_flag_offset = 10;
	__u16 *flags;
	__u16 dhcp_bcast_flag = htons(0x8000);

	__wsum csum;

	unsigned char is_dhcp = br_is_dhcp(skb, &iph, &udph, &dhcph);

	if (iph) {
		_set_sat_ip(BR_SKB_CB(skb)->source_port, iph->saddr);
	}

	/* If it's not a DHCP frame destined for the server, do nothing.
	 */
	if (!is_dhcp || !iph || !udph || !dhcph || (udph->source != htons(68) ||
						    udph->dest != htons(67))) {
		return;
	}

	br_log_dhcp(udph, dhcph);

	/* found it, set the bit */
	flags = (__u16 *)&dhcph[dhcp_flag_offset];
	*flags |= dhcp_bcast_flag;

	/* recalculate the UDP checksum */
	udph->check = 0;
	csum = csum_partial(skb_transport_header(skb), ntohs(udph->len), 0);
	udph->check = csum_tcpudp_magic(iph->saddr, iph->daddr, ntohs(udph->len),
					IPPROTO_UDP, csum);

}

static void _br_proxy_mangle_for_arp(struct net_bridge *br, struct sk_buff *skb)
{
	unsigned char *arp_ptr = (unsigned char *)(eth_hdr(skb) + 1) + sizeof(struct arphdr);
	unsigned char *arp_sender_mac;
	__u32* arp_sender_ip;

	if (!pskb_may_pull(skb, sizeof(struct arphdr) + ETH_ALEN + sizeof(__u32)))
		return;

	skb_reset_network_header(skb);
	arp_ptr = (unsigned char *)(arp_hdr(skb) + 1);

	arp_sender_mac = arp_ptr;

	arp_ptr += ETH_ALEN;

	arp_sender_ip = (__u32 *)arp_ptr;

	/* Record the sender IP. We'll use it to forward frames
	 * from the uplink to this satellite. */
	_set_sat_ip(BR_SKB_CB(skb)->source_port, *arp_sender_ip);

	/* mess with arp header */
	memcpy(arp_sender_mac, br->static_mac, ETH_ALEN);
}
#endif
static void br_mangle_if_proxying_up(const struct net_bridge_port *to,
				     struct sk_buff *skb)
{
#ifdef CONFIG_SONOS_BRIDGE_PROXY
        if (BR_SKB_CB(skb)->should_proxy_up &&
	    to->br->use_static_mac &&
	    should_proxy_up(BR_SKB_CB(skb)->source_port, to)) {
		/* We're a proxy bridge forwarding a frame from
		 * a satellite to the uplink or a direct-route port, so
		 * let's do our naughty proxy stuff.
		 */
		__be16 proto = eth_hdr(skb)->h_proto;

		if (ntohs(proto) == ETH_P_ARP) {
			_br_proxy_mangle_for_arp(to->br, skb);
		} else if (ntohs(proto) == ETH_P_IP) {
			_br_proxy_mangle_for_ip(skb);
		}

		memcpy(eth_hdr(skb)->h_source, to->br->static_mac, ETH_ALEN);
        }
#endif
}

int _br_forward_finish(const struct net_bridge_port *to,
			      struct sk_buff *skb)
{
	br_mangle_if_proxying_up(to, skb);
	__dev_queue_push_xmit(to, skb);
	return 0;
}

/* SONOS: Only called with bpdus.  It doesn't really matter how we tunnel them,
 *        but sending it the same way we used to send it before direct routing
 *        makes me feel better.
 */
void br_deliver_bpdu(const struct net_bridge_port *p,
                     struct sk_buff *skb)
{
	__br_skb_mark_direct(skb, 0);
	__dev_queue_wrapper_xmit(p, skb);
}

/* called under bridge lock */
void br_true_flood(struct net_bridge *br, struct net_bridge_port *from,
		   struct sk_buff *skb, int clone, int p2p_flood_deliver,
		   void (*__packet_hook)(const struct net_bridge_port *p,
					 struct sk_buff *skb))
{
	struct net_bridge_port *p;
	struct net_bridge_port *prev;

	/* Send in the clones */
	if (clone) {
		struct sk_buff *skb2;

		if ((skb2 = skb_copy(skb, GFP_ATOMIC)) == NULL) {
			br->statistics.tx_dropped++;
			return;
		}

		skb = skb2;
	}

	prev = NULL;

        /* handle deliveries to ports running STP */
	list_for_each_entry_rcu(p, &br->port_list, list) {
            if (should_deliver(from, p)) {
                if (prev != NULL) {
                    struct sk_buff *skb2;

                    if ((skb2 = skb_copy(skb, GFP_ATOMIC)) == NULL) {
                        br->statistics.tx_dropped++;
                        kfree_skb(skb);
                        return;
                    }
                    __packet_hook(prev, skb2);
                }

                prev = p;
            }
	}

        /* handle deliveries to p2p leaf stations */
	if (p2p_flood_deliver) {
		list_for_each_entry_rcu(p, &br->leaf_list, list) {
			if (should_deliver(from, p)) {
				if (prev != NULL) {
					struct sk_buff *skb2;
					if ((skb2 = skb_copy(skb, GFP_ATOMIC)) == NULL) {
						br->statistics.tx_dropped++;
						kfree_skb(skb);
						return;
					}
					__packet_hook(prev, skb2);
				}
				prev = p;
			}
		}
	}

	if (prev != NULL) {
		__packet_hook(prev, skb);
		return;
	}

	kfree_skb(skb);
}

/* called under bridge lock */
void br_mcast(struct net_bridge *br, struct net_bridge_port *from,
	      struct sk_buff *skb, int clone,
	      void (*__packet_hook)(const struct net_bridge_port *p, struct sk_buff *skb),
	      struct net_bridge_mcast_entry *me)
{
	struct net_bridge_port *p;
	struct net_bridge_port *prev;
        struct net_bridge_mcast_rx_port* mrxp;

	if (clone) {
		struct sk_buff *skb2;

		if ((skb2 = skb_copy(skb, GFP_ATOMIC)) == NULL) {
			br->statistics.tx_dropped++;
			return;
		}

		skb = skb2;
	}

	prev = NULL;

        for (mrxp = me->rx_port_list; mrxp != NULL; mrxp = mrxp->next) {
		p = mrxp->dst;
		/* don't forward to the local device */
		if (!p)
			continue;

		if (p->is_p2p) {
			if (sonos_should_deliver(from, p)) {
				if (prev != NULL) {
					struct sk_buff *skb2;

					if ((skb2 = skb_copy(skb, GFP_ATOMIC)) == NULL) {
						br->statistics.tx_dropped++;
						kfree_skb(skb);
						return;
					}

					__packet_hook(prev, skb2);
				}

				prev = p;
			}
		} else {
			/* when multicasting onto a normal network segment, 'expand'
			   the multicast into n unicasts to each of the MAC addresses
			   that have expressed interest in receiving the data.  This
			   avoids flooding the customer's home network with multicast
			   data. */
			if (sonos_should_deliver(from, p)) {
				struct net_bridge_mcast_rx_mac* mrxm;
				for (mrxm = mrxp->rx_mac_list;
				     mrxm != NULL;
				     mrxm = mrxm->next) {
					struct sk_buff *skb2;

					if ((skb2 = skb_copy(skb, GFP_ATOMIC))) {
						memcpy((char *)(eth_hdr(skb2)->h_dest),
						       mrxm->addr,
						       ETH_ALEN);
						if (mrxm->ip != 0) {
							br_udp_overwrite_ip(skb2, mrxm->ip);
						}
						__packet_hook(p, skb2);
					} else
						br->statistics.tx_dropped++;
				}
			}
		}
	}

	if (prev != NULL) {
		__packet_hook(prev, skb);
		return;
	}

	kfree_skb(skb);
}

void
br_mcast_direct(struct net_bridge *br, struct net_bridge_port *from,
		struct sk_buff *skb, int clone,
		void (*__packet_hook)(const struct net_bridge_port *p,
				      struct sk_buff *skb),
		struct net_bridge_mcast_entry *me)
{
#define BR_MAX_DIRECT_DESTS 32

	struct {
		struct net_bridge_mcast_rx_mac* mrxm;
		struct net_bridge_port*         port;
		int                             dr;
	} destinations[32];

	int num = 0, bad = 0;
	struct net_bridge_mcast_rx_mac* mrxm;
	struct net_bridge_mcast_rx_port* mrxp;
        struct net_bridge_port* port;
	unsigned int i;
	int dr;

#ifdef BR_DEBUG_DIRECT
    int log = 0;
	{
		static int log_counter = 0;
		if (++log_counter == 512) {
			log_counter = 0;
			log = 1;
		}
	}
#endif

	/* REVIEW: This is a prototype.  Spinning through all of this for every
	 *         packet is not the best idea, so we should figure out how to
	 *         update the multicast entries themselves so that they always
	 *         contain the correct information.
	 *
	 *         OK, so it isn't *that* bad.  If we are all wired or capable
	 *         of direct routing it isn't any less efficient than the old
	 *         code.  If we are not, we spin through a couple of lists a
	 *         second time.  We're not doing hash table lookups, so I think
	 *         we'll be OK.
	 */
        for (mrxp = me->rx_port_list; mrxp != NULL; mrxp = mrxp->next) {

		struct net_bridge_port* p = mrxp->dst;

		/* Skip local ports.  We know this originated locally, so we
		 * don't care about sonos_should_deliver() at all.  If we can
		 * see all of the destinations, we're going for it.  This is a
		 * little sketchy for ethernet ports, but I think we're all good
		 * since we're converting everything to unicast and should not
		 * be able to generate a loop.
		 *
		 * Also skip ports that are not at least learning.
		 */
		if (!p) {
			continue;
		}

		/* Figure out how to send each packet, aborting once we find out
		 * we can't direct route.
		 */
		for (mrxm = mrxp->rx_mac_list;
		     mrxm != NULL;
		     mrxm = mrxm->next) {

			port = p;
			dr = 0;

			/* If the STP port is wireless, see if there is a
			 * better way to get to the DA (i.e. a direct route).
			 */
			if (p->is_p2p) {
				/* Do we actually know the neighbor and is the signal strength good
				 * enough? Does it want direct routed multicast traffic?
				 *
				 * If the destination does not support going directly, we can send
				 * exactly one packet down the STP path (for the short term).
				 */
				if (!mrxm->direct_dst || !(mrxm->direct_dst->direct_enabled & 2)) {
					if (++bad == 2) {
						goto abort;
					}
				}

				/* Only direct route when known and enabled */
				if (mrxm->direct_dst && mrxm->direct_dst->direct_enabled) {
					port = mrxm->direct_dst;
					dr = 1;
				}

				/* If port is not forwarding or blocked, drop */
				if (port->state <= BR_STATE_LEARNING)
					continue;

			} else {

				/* If the port is not enabled, drop */
				if (port->state == BR_STATE_DISABLED) {
					continue;
				}
			}

			/* Create another entry in the array */
			destinations[num].mrxm = mrxm;
			destinations[num].port = port;
			destinations[num].dr   = dr;

			if (++num == BR_MAX_DIRECT_DESTS)
				goto abort;
		}
	}

#ifdef BR_DEBUG_DIRECT
	if (log) {
		printk("mc: rock! (%d, %d)\n", num, bad);
	}
#endif

	/*
	 * Unicast to all destinations
	 */

	/* Clone if required.  This *must* be a copy, not a clone. */
	if (clone) {
		struct sk_buff *skb2;

		if ((skb2 = skb_copy(skb, GFP_ATOMIC)) == NULL) {
			br->statistics.tx_dropped++;
			return;
		}

		skb = skb2;
	}

	/* Unicast to all of the destinations
	 *
	 * REVIEW: Should port the "prev" trickery from below to save a copy if
	 *         possible.
	 */
	for (i = 0; i < num; i++) {
		struct net_bridge_port* port;
		struct net_bridge_mcast_rx_mac* mrxm;
		struct sk_buff *skb2;

		port = destinations[i].port;
		mrxm = destinations[i].mrxm;

		if ((skb2 = skb_copy(skb, GFP_ATOMIC))) {

			/* Mark it direct if we're not following the STP path */
			__br_skb_mark_direct(skb2, destinations[i].dr);

			memcpy((char *)(eth_hdr(skb2)->h_dest),
			       mrxm->addr,
			       ETH_ALEN);

			if (mrxm->ip != 0 && !destinations[i].dr) {
#ifdef BR_DEBUG_DIRECT
				if (log) {
					printk("mc ip\n");
				}
#endif
				br_udp_overwrite_ip(skb2, mrxm->ip);
			}
			__packet_hook(port, skb2);

#ifdef BR_DEBUG_DIRECT
			if (log)
			{
				printk("mc dest: %02x:%02x:%02x:%02x:%02x:%02x (%d:%d)\n",
				       mrxm->addr[0],
				       mrxm->addr[1],
				       mrxm->addr[2],
				       mrxm->addr[3],
				       mrxm->addr[4],
				       mrxm->addr[5],
				       port->port_no,
				       destinations[i].dr);
			}
#endif
		} else {
			br->statistics.tx_dropped++;
			break;
		}
	}

	/*
	 * Free the original since we made copies of everything.  We can save a
	 * copy here (and the free) with some work.
	 */
	kfree_skb(skb);
	return;
abort:

#ifdef BR_DEBUG_DIRECT
	if (log) {
		printk("mc: abort!\n");
		i = 0;
	}
#endif

	/* Can't do it directly, go old school */
	br_mcast(br, from, skb, clone, __packet_hook, me);
}

struct sk_buff *br_handle_frame_std(struct net_bridge_port *p,
				    struct sk_buff *skb,
				    const unsigned char *src,
				    const unsigned char *dest,
				    int direct)
{
	/* Learning */
	if (p->state == BR_STATE_LEARNING ||
	    p->state == BR_STATE_FORWARDING ||
	    direct) {

		/* Only learn if the packet was not routed directly */
		if (!direct) {
			struct net_bridge_fdb_entry *fdb;
			fdb = br_sonos_fdb_update(p->br, p, src);
			if (fdb && fdb->priority == 0) {
				br_stats_update(p->br, src, dest);
			}
		}

		/* Always check for table updates regardless of source */
		br_mcast_check(skb, p->br, p);
	}

	/* STP maintenance */
	if (p->br->stp_enabled &&
	    !memcmp(dest, bridge_ula, 5) &&
	    !(dest[5] & 0xF0)) {
		if (!dest[5]) {
			br_stp_handle_bpdu(p, skb);
			return NULL;
		}
	}

#ifdef CONFIG_SONOS_BRIDGE_PROXY
	/* Forwarding
	 * Ignore the forwarding state when it's a leaf satellite and STP is disabled.
	 */
	else if ((p->state == BR_STATE_FORWARDING &&
		 !(p->is_leaf && p->is_satellite && !p->br->stp_enabled)) || direct) {
#else
	else if (p->state == BR_STATE_FORWARDING || direct) {
#endif

		if (ether_addr_equal(p->br->dev->dev_addr, dest))
			skb->pkt_type = PACKET_HOST;

		br_handle_frame_finish(p, skb);
		return NULL;
	}
#ifdef CONFIG_SONOS_BRIDGE_PROXY
	else if (BR_SKB_CB(skb)->should_proxy_up) {
		br_uplink_proxy(p->br, p, skb, dest);
		return NULL;
	}
#endif

	kfree_skb(skb);
	return NULL;
}

void __sonos_br_deliver(const struct net_bridge_port *to, struct sk_buff *skb)
{
	skb->dev = to->dev;

	_br_forward_finish(to,skb);
}

void sonos_br_deliver(const struct net_bridge_port *from,
		      const struct net_bridge_port *to,
		      struct sk_buff *skb)
{
	if (sonos_should_deliver(from, to)) {
		__br_skb_mark_direct(skb, 0);
		__sonos_br_deliver(to, skb);
		return;
	}

	kfree_skb(skb);
}

/* called with rcu_read_lock */
void sonos_flood_deliver(struct net_bridge *br,
			 struct net_bridge_port *from,
			 struct sk_buff *skb, int clone)
{
	sonos_br_flood(br, from, skb, clone, __sonos_br_deliver);
}

/* called with rcu_read_lock */
void br_deliver_direct(const struct net_bridge_port *from,
		       const struct net_bridge_port *to,
		       struct sk_buff *skb)
{
	/*
         * REVIEW: This is different since we don't care about STP state.
         *
         *         Also note that this traffic will be sent down a tunnel that
         *         tells the device on the other end not to learn.  Sending all
         *         traffic to a ZP down this path is a *bad* idea, since it
         *         will never learn the return path and end up constantly
         *         flooding the replies.
         */
	if (from != to) {
		__br_skb_mark_direct(skb, 1);
		__sonos_br_deliver(to, skb);
                return;
        }

	kfree_skb(skb);
}

void __sonos_br_forward(const struct net_bridge_port *to, struct sk_buff *skb)
{
	skb->dev = to->dev;
	skb->ip_summed = CHECKSUM_NONE;
        _br_forward_finish(to, skb);
}

void sonos_br_forward(const struct net_bridge_port *from,
		      const struct net_bridge_port *to,
		      struct sk_buff *skb)
{
	if (sonos_should_deliver(from, to)) {
		__br_skb_mark_direct(skb, 0);
		__sonos_br_forward(to, skb);
		return;
	}

	kfree_skb(skb);
}

/* called with rcu_read_lock */
void sonos_flood_forward(struct net_bridge *br,
			 struct net_bridge_port *from,
			 struct sk_buff *skb, int clone)
{
	sonos_br_flood(br, from, skb, clone, __sonos_br_forward);
}

/* called with rcu_read_lock */
void br_forward_direct(const struct net_bridge_port *from,
		       const struct net_bridge_port *to,
		       struct sk_buff *skb)
{
	/*
         * REVIEW: This is different since we don't care about STP state.
         */
	if (from != to) {
		__br_skb_mark_direct(skb, 1);
		__sonos_br_forward(to, skb);
                return;
        }

	kfree_skb(skb);
}

/* called under bridge lock */
void sonos_br_flood(struct net_bridge *br,
		    struct net_bridge_port *from,
		    struct sk_buff *skb, int clone,
		    void (*__packet_hook)(const struct net_bridge_port *p,
					  struct sk_buff *skb))
{
	const unsigned char *dest  = (unsigned char *)(eth_hdr(skb)->h_dest);
	int p2p_flood_deliver = 0;

	/* Make sure we do not direct route anything by accident */
	__br_skb_mark_direct(skb, 0);

	/* The following is a slight hack, but is very convenient for our
	   purposes.  The p2p leaf concept is used by the Rincon handheld
	   remote controls, which only need to receive unicasts destined
	   for themselves, MAC broadcasts, and MAC multicasts to the
	   UPnP-reserved multicast group.  We block all other
	   multicasts. */
	if (dest[0] & 1) {
		if (br_mcast_dest_is_allowed(dest)) {
			p2p_flood_deliver = 1;
		}
	} else {
		p2p_flood_deliver = 1;
	}

	if (p2p_flood_deliver) {
		br_true_flood(br, from, skb, clone, 1, __packet_hook);
	} else {
		/* see if we can limit the delivery of this packet to just the subset
		   of the network that is a member of the MAC multicast group */
		struct net_bridge_mcast_entry *me = br_mcast_get(br, dest);
		if (me) {
			/* See if we can direct route the wireless bits */
			if (__packet_hook == __sonos_br_deliver)
				br_mcast_direct(br, from, skb, clone, __packet_hook, me);
			else
				br_mcast(br, from, skb, clone, __packet_hook, me);
			br_mcast_put(me);
		} else {
			/* if a packet originates locally, and it is destined for a
			   MAC multicast group that is 'unknown' to us, then drop
			   the packet. */
			if (__packet_hook == __sonos_br_deliver) {
				if (br_mcast_dest_is_allowed_from_local(dest)) {
					br_true_flood(br, from, skb, clone, 0, __packet_hook);
				} else {
					kfree_skb(skb);
				}
			} else {
				br_true_flood(br, from, skb, clone, 0, __packet_hook);
			}
		}
	}
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,24)
static int br_pass_frame_up_finish(struct net *net, struct sock *sk,
				   struct sk_buff *skb)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static int br_pass_frame_up_finish(struct sk_buff *skb)
#endif
{
	netif_receive_skb(skb);
	return 0;
}

void sonos_pass_frame_up(struct net_bridge *br, struct sk_buff *skb)
{
	struct net_device *indev;

	br->statistics.rx_packets++;
	br->statistics.rx_bytes += skb->len;

	indev = skb->dev;
	skb->dev = br->dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,24)
	NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_IN, NULL, NULL, skb, indev, NULL,
			br_pass_frame_up_finish);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_IN, skb, indev, NULL,
			br_pass_frame_up_finish);
#endif
}

int sonos_handle_frame_finish(struct net_bridge_port *p, struct sk_buff *skb)
{
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	struct net_bridge *br = p->br;
	struct net_bridge_fdb_entry *dst;

	int passedup = 0;
	if (br->dev->flags & IFF_PROMISC) {
		struct sk_buff *skb2;

		skb2 = skb_clone(skb, GFP_ATOMIC);
		if (skb2 != NULL) {
			passedup = 1;
			br_pass_frame_up(br, skb2);
		}
	}

	if (dest[0] & 1) {
		br_flood_forward(br, p, skb, !passedup);
		if (!passedup)
			br_pass_frame_up(br, skb);
		goto out;
	}

	dst = __br_fdb_get(br, dest, 0);

	if ((dst != NULL && dst->is_local) || skb->pkt_type == PACKET_HOST) {
		if (!passedup)
			br_pass_frame_up(br, skb);
		else
			kfree_skb(skb);
		goto out;
	}

	if (dst != NULL) {

		if (0 == skb->priority) {
			skb->priority = dst->priority;
		}

		/* NOTE: This was not sourced on this device, but we may still
		 *       want to direct route it.  Just make sure we forward
		 *       instead of deliver.
		 */
                br_direct_unicast(p, dst, skb,
				  br_forward,
				  br_forward_direct);
		goto out;

	}

	br_flood_forward(br, p, skb, 0);

out:
	return 0;
}

rx_handler_result_t sonos_br_handle_frame(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct net_bridge_port_list_node *pl = skb->dev->br_port_list;

	if (unlikely(skb->pkt_type == PACKET_LOOPBACK)) {
		return RX_HANDLER_PASS;
	}

	if ( pl == NULL ) {
		return RX_HANDLER_PASS;
	}

	skb = br_handle_frame(pl, skb);
	if ( skb == NULL ) {
		return RX_HANDLER_CONSUMED;
	}
	return RX_HANDLER_PASS;
}

struct sk_buff *sonos_handle_frame(struct net_bridge_port_list_node *pl,
				      struct sk_buff *skb)
{
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	const unsigned char *src  = eth_hdr(skb)->h_source;
	struct net_bridge_port *p ;
        int direct = 0;

#ifdef DEBUG_BR_INPUT
	printk("hf: p=%04x, s=" MAC_ADDR_FMT ", d=" MAC_ADDR_FMT "\n",
	       eth_hdr(skb)->h_proto,
	       MAC_ADDR_VAR(src),
	       MAC_ADDR_VAR(dest));
#endif

	/*
	 * (in)sanity checks
	 */
        if (NULL == pl || NULL == pl->port) {
		goto drop;
        }

	if (!is_valid_ether_addr(eth_hdr(skb)->h_source)) {
		goto drop;
	}

	p = br_find_port(src, pl);

	if (!p || (p->state == BR_STATE_DISABLED && !p->br->uplink_mode)) {
		goto drop;
	}

	memset(BR_SKB_CB(skb), 0, sizeof(*(BR_SKB_CB(skb))));
#ifdef CONFIG_SONOS_BRIDGE_PROXY
	BR_SKB_CB(skb)->source_port = p;
#endif

	/* If the frame arrived on a port that is the endpoint of a
	 * point-to-point packet tunnel, it should be using the tunnel
	 * protocol number.  If so, pull off the header to obtain the
         * tunneled ethernet frame; if not, ignore the packet.
	 *
	 * NOTE: We now have a learned and an unlearned tunnel.  This is a key
	 *       part of how direct routing works.
	 */
	if (p->is_p2p && !p->is_unencap) {

		if (eth_hdr(skb)->h_proto != htons(BR_TUNNEL_PROTOCOLNUM)) {

			if (eth_hdr(skb)->h_proto != htons(BR_TUNNEL2_PROTOCOLNUM)) {
				src  = eth_hdr(skb)->h_source;
                                print_hex_dump(KERN_INFO, "bad proto ethhdr ", DUMP_PREFIX_NONE, 32, 1,
                                               eth_hdr(skb), sizeof(struct ethhdr), false);
                                /* Log first 32 bytes of skb->data */
                                print_hex_dump(KERN_INFO, "skb->data ", DUMP_PREFIX_NONE, 32, 1,
                                               skb->data, min(skb->len, 32U), false);

				goto drop;
			}
			direct = 1;
		}

		/*
		 * Unencapsulate the tunneled ethernet frame.  Note that it
		 * looks like we need to update the protocol here (2.4
		 * calculates it *way* later on, but 2.6 doesn't). Use
		 * the eth_type_trans() function that drivers (including
		 * ours) normally use. This accounts for the fact that
		 * we're unencapsulating a frame, and will set
		 * skb->pkt_type correctly.
		 */
		skb->protocol = eth_type_trans(skb, p->br->dev);

#ifdef CONFIG_SONOS_BRIDGE_PROXY
		if (p->br->proxy_mode &&
		    p->is_satellite &&
		    br_check_sonosnet_serial_match(src, eth_hdr(skb)->h_source)) {
			/* We only want to proxy to the uplink when the
			 * wireless TA equals the actual (encapsulated)
			 * source address. This is to ensure that we only
			 * proxy for satellites that are a single hop
			 * away. We also want to proxy up only for frames
			 * originating from satellites. */
			BR_SKB_CB(skb)->should_proxy_up = 1;
		}
#endif

		dest = eth_hdr(skb)->h_dest;
		src  = eth_hdr(skb)->h_source;

#ifdef DEBUG_BR_INPUT
		printk("hf unencap: p=%04x, s=" MAC_ADDR_FMT ", d=" MAC_ADDR_FMT "\n",
		       eth_hdr(skb)->h_proto,
		       MAC_ADDR_VAR(src),
		       MAC_ADDR_VAR(dest));
		if (eth_hdr(skb)->h_proto == htons(ETH_P_IP)) {
			unsigned char *data = skb->data;
			printk("hf unencap IP: src=%pI4 dst=%pI4 proto=%x\n",
				data + 12, data + 16, *(data + 9));
		}
#endif
	}
#ifdef CONFIG_SONOS_BRIDGE_PROXY
	else if (p->br->proxy_mode &&
	         p->is_satellite &&
	         p->is_unencap) {
        /* We want to proxy to the uplink for satellites that are a single hop away. */
        BR_SKB_CB(skb)->should_proxy_up = 1;
    }
#endif

	if (p->br->uplink_mode) {
		/* Anything that didn't come from the AP needs to have come
		 * over the unlearned tunnel
		 */
		if (p->is_uplink || direct) {
			return br_uplink_handle_frame(p, skb, src, dest);
		}
#ifdef CONFIG_SONOS_BRIDGE_PROXY
                else if (BR_SKB_CB(skb)->should_proxy_up) {
			return br_handle_frame_std(p, skb, src, dest, direct);
                }
#endif
	} else {
		return br_handle_frame_std(p, skb, src, dest, direct);
	}

drop:
	kfree_skb(skb);
	return NULL;
}
