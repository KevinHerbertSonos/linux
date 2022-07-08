/*
 *	Generic parts
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/llc.h>
#include <net/llc.h>
#include <net/stp.h>

#if defined(CONFIG_SONOS_BRIDGE_PROXY) /* SONOS SWPBL-25027 */
#include <linux/inetdevice.h>
#endif

#include "br_private.h"

#if defined(CONFIG_SONOS)
#include "br_forward_sonos.h"
#else /* SONOS SWPBL-19651 */
static const struct stp_proto br_stp_proto = {
	.rcv	= br_stp_rcv,
};
#endif

static struct pernet_operations br_net_ops = {
	.exit	= br_net_exit,
};

static int __init br_init(void)
{
	int err;

#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	err = stp_proto_register(&br_stp_proto);
	if (err < 0) {
		pr_err("bridge: can't register sap for STP\n");
		return err;
	}
#endif

	err = br_fdb_init();
	if (err)
		goto err_out;

	err = register_pernet_subsys(&br_net_ops);
	if (err)
		goto err_out1;

	err = br_netfilter_init();
	if (err)
		goto err_out2;

	err = register_netdevice_notifier(&br_device_notifier);
	if (err)
		goto err_out3;

#if defined(CONFIG_SONOS_BRIDGE_PROXY) /* SONOS SWPBL-25027 */
	err = register_inetaddr_notifier(&br_inetaddr_notifier);
	if (err)
		goto err_out_proxy;
#endif

	err = br_netlink_init();
	if (err)
		goto err_out4;

	brioctl_set(br_ioctl_deviceless_stub);

#if defined(CONFIG_SONOS) /* SONOS SWPBL-25829 */
	br_handle_frame_hook = sonos_br_handle_frame;
#elif IS_ENABLED(CONFIG_ATM_LANE) /* SONOS SWPBL-19651 */
	br_fdb_test_addr_hook = br_fdb_test_addr;
#endif

	return 0;

err_out4:
#if defined(CONFIG_SONOS_BRIDGE_PROXY) /* SONOS SWPBL-25027 */
	unregister_inetaddr_notifier(&br_inetaddr_notifier);
err_out_proxy:
#endif
	unregister_netdevice_notifier(&br_device_notifier);
err_out3:
	br_netfilter_fini();
err_out2:
	unregister_pernet_subsys(&br_net_ops);
err_out1:
	br_fdb_fini();
err_out:
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	stp_proto_unregister(&br_stp_proto);
#endif
	return err;
}

static void __exit br_deinit(void)
{
#if !defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
	stp_proto_unregister(&br_stp_proto);
#endif

	br_netlink_fini();
	unregister_netdevice_notifier(&br_device_notifier);
#if defined(CONFIG_SONOS_BRIDGE_PROXY) /* SONOS SWPBL-25027 */
	unregister_inetaddr_notifier(&br_inetaddr_notifier);
#endif
	brioctl_set(NULL);

	unregister_pernet_subsys(&br_net_ops);

	rcu_barrier(); /* Wait for completion of call_rcu()'s */

	br_netfilter_fini();
#if defined(CONFIG_SONOS) /* SONOS SWPBL-25829 */
	br_handle_frame_hook = NULL;
#elif IS_ENABLED(CONFIG_ATM_LANE) /* SONOS SWPBL-19651 */
	br_fdb_test_addr_hook = NULL;
#endif

	br_fdb_fini();
}

#if defined(CONFIG_SONOS) /* SONOS SWPBL-19651 */
int (*br_should_route_hook)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(br_should_route_hook);
#endif

module_init(br_init)
module_exit(br_deinit)
MODULE_LICENSE("GPL");
MODULE_VERSION(BR_VERSION);
MODULE_ALIAS_RTNL_LINK("bridge");
