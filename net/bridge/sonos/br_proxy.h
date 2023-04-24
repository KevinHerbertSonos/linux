/* br_proxy.h - Sonos Bridge Proxy
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _BR_PROXY_H
#define _BR_PROXY_H
#include <linux/version.h>

/* br_proxy.c */
extern void br_dupip_check(struct net_bridge *br);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
extern void br_dupip_timer_expired(struct timer_list *t);
#else
extern void br_dupip_timer_expired(unsigned long arg);
#endif

extern int br_dump_ifinfo(struct sk_buff *skb, struct netlink_callback *cb);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
extern int br_rtm_setlink(struct sk_buff *skb,  struct nlmsghdr *nlh, struct netlink_ext_ack *extack);
#else
extern int br_rtm_setlink(struct sk_buff *skb,  struct nlmsghdr *nlh);
#endif

extern int br_inetaddr_event(struct notifier_block *unused, unsigned long event, void *ptr);

#endif
