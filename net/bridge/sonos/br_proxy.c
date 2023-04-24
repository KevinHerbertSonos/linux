/* br_proxy.c - Sonos Bridge Proxy
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <linux/inetdevice.h>
#include <linux/version.h>
#include "br_private.h"

#include "br_proxy.h"
#include "br_sonos.h"

#ifdef CONFIG_SONOS_BRIDGE_PROXY
static unsigned int sats_match_bridge(const struct net_bridge *br) {

	struct net_bridge_port *p;
	unsigned int num_sats_checked = 0;

	if (!br->current_ipv4_addr) {
		return 0;
	}

	list_for_each_entry_rcu(p, &br->port_list, list) {
		if (p->is_satellite) {
			num_sats_checked++;
			if (!p->sat_ip || p->sat_ip != br->current_ipv4_addr) {
				/* We're very strict here on purpose. If
				 * there's even a single satellite that
				 * either has no IP set yet, or has an
				 * IP that doesn't match the bridge,
				 * we'll return "no match". */
				return 0;
			}
		}
	}

	/* If we checked at least one sat and it matched, return true. */
	return num_sats_checked;
}

static void br_dupip_notify(struct net_bridge *br, int time)
{
	struct net *net = dev_net(br->dev);
	struct sk_buff *skb;
	int err = -ENOBUFS;
	struct nlmsghdr *nlh;

	skb = nlmsg_new(RTA_LENGTH(sizeof(time)), GFP_ATOMIC);

	if (skb == NULL) {
		goto errout;
	}

	nlh = nlmsg_put(skb, 0, 0, RWM_DUPE_IP, 0, 0);

	if (nlh == NULL) {
		goto errout;
	}

	if (nla_put_le32(skb, RWA_DUPE_TIME, time) < 0) {
		goto failure;
	}

	nlmsg_end(skb, nlh);
	if (skb->len < 0) {
		goto failure;
	}

	rtnl_notify(skb, net, 0, RTMGRP_Rincon, NULL, GFP_ATOMIC);
	return;

failure:
	nlmsg_cancel(skb, nlh);

errout:
	kfree_skb(skb);
	rtnl_set_sk_err(net, RTMGRP_Rincon, err);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
void br_dupip_timer_expired(unsigned long arg)
{
	struct net_bridge *br = (struct net_bridge *)arg;
#else
void br_dupip_timer_expired(struct timer_list *t)
{
        struct net_bridge *br = from_timer(br, t, dupip_timer);
#endif
	static const int dupip_notify_interval_secs = 30;

	spin_lock_bh(&br->lock);

	if (sats_match_bridge(br)) {
		br_dupip_notify(br, ((long)jiffies - (long)br->dupip_start) / HZ);
		mod_timer(&br->dupip_timer,
			  jiffies + dupip_notify_interval_secs*HZ);
	} else {
		printk("br proxy: IP conflict event ends %d\n", __LINE__);
		br_dupip_notify(br, -1);
	}

	spin_unlock_bh(&br->lock);
}

struct notifier_block br_inetaddr_notifier = {
	.notifier_call = br_inetaddr_event,
};

int br_inetaddr_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct in_ifaddr *ifa = ptr;
	struct net_device *event_dev = ifa->ifa_dev->dev;
	struct net_device *dev;

	for_each_netdev(&init_net, dev) {
		if (event_dev == dev && (dev->priv_flags & IFF_EBRIDGE)) {
			struct net_bridge *br = netdev_priv(dev);

			switch (event) {
			case NETDEV_UP:
				br->current_ipv4_addr = ifa->ifa_local;
				br_dupip_check(br);
				return NOTIFY_OK;
			case NETDEV_DOWN:
				br->current_ipv4_addr = 0;
				br_dupip_check(br);
				return NOTIFY_OK;
			default:
				return NOTIFY_DONE;
			}
		}
	}
        return NOTIFY_DONE;
}
#endif

void br_dupip_check(struct net_bridge *br)
{
#ifdef CONFIG_SONOS_BRIDGE_PROXY
	if (!sats_match_bridge(br) && timer_pending(&br->dupip_timer)) {
		printk("br proxy: IP conflict event ends %d\n", __LINE__);
		br_dupip_notify(br, -1);
		del_timer(&br->dupip_timer);
	} else if (sats_match_bridge(br) && !timer_pending(&br->dupip_timer)) {
		printk("br proxy: IP conflict event begins\n");
		br->dupip_start = jiffies;
		mod_timer(&br->dupip_timer, jiffies);
	}
#endif
}

/*
 * Dump information about all ports, in response to GETLINK
 */
int br_dump_ifinfo(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct net *net = sock_net(skb->sk);
	struct net_device *dev;
	const unsigned char *src  = eth_hdr(skb)->h_source;
	struct net_bridge_port *p ;
	int idx;

	idx = 0;
	for_each_netdev(net, dev) {
		/* not a bridge port */
		if (dev->br_port_list == NULL || idx < cb->args[0])
			goto skip;

		p = br_find_port(src, dev->br_port_list);

		if (br_sonos_fill_ifinfo(skb, p, NETLINK_CB(cb->skb).portid,
					 cb->nlh->nlmsg_seq, RTM_NEWLINK,
					 NLM_F_MULTI) < 0)
			break;
skip:
		++idx;
	}

	cb->args[0] = idx;

	return skb->len;
}

/*
 * Change state of port (ie from forwarding to blocking etc)
 * Used by spanning tree in user space.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
int br_rtm_setlink(struct sk_buff *skb,  struct nlmsghdr *nlh, struct netlink_ext_ack *extack)
#else
int br_rtm_setlink(struct sk_buff *skb,  struct nlmsghdr *nlh)
#endif
{
	struct net *net = sock_net(skb->sk);
	const unsigned char *src  = eth_hdr(skb)->h_source;
	struct ifinfomsg *ifm;
	struct nlattr *protinfo;
	struct net_device *dev;
	struct net_bridge_port *p;
	u8 new_state;

	if (nlmsg_len(nlh) < sizeof(*ifm))
		return -EINVAL;

	ifm = nlmsg_data(nlh);
	if (ifm->ifi_family != AF_BRIDGE)
		return -EPFNOSUPPORT;

	protinfo = nlmsg_find_attr(nlh, sizeof(*ifm), IFLA_PROTINFO);
	if (!protinfo || nla_len(protinfo) < sizeof(u8))
		return -EINVAL;

	new_state = nla_get_u8(protinfo);
	if (new_state > BR_STATE_BLOCKING)
		return -EINVAL;

	dev = __dev_get_by_index(net, ifm->ifi_index);
	if (!dev)
		return -ENODEV;

	if (!dev->br_port_list)
		return -EINVAL;

	p = br_find_port(src, dev->br_port_list);

	/* if kernel STP is running, don't allow changes */
	if (p->br->stp_enabled)
		return -EBUSY;

	if (!netif_running(dev) ||
	    (!netif_carrier_ok(dev) && new_state != BR_STATE_DISABLED))
		return -ENETDOWN;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
	br_set_state(p, new_state);
#else
	p->state = new_state;
	br_log_state(p);
#endif
	return 0;
}
