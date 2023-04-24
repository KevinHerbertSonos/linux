// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *	Ioctl handler
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 */

#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/if_bridge.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/times.h>
#include <net/net_namespace.h>
#include <linux/uaccess.h>
#include "br_private.h"

#if defined(CONFIG_SONOS)
#include "br_mcast.h"
#include "br_sonos.h"
#include "br_stp_sonos.h"
#include "br_tunnel.h"
#include "br_uplink.h"
#endif

static int get_bridge_ifindices(struct net *net, int *indices, int num)
{
	struct net_device *dev;
	int i = 0;

	rcu_read_lock();
	for_each_netdev_rcu(net, dev) {
		if (i >= num)
			break;
		if (dev->priv_flags & IFF_EBRIDGE)
			indices[i++] = dev->ifindex;
	}
	rcu_read_unlock();

	return i;
}

/* called with RTNL */
static void get_port_ifindices(struct net_bridge *br, int *ifindices, int num)
{
	struct net_bridge_port *p;

	list_for_each_entry(p, &br->port_list, list) {
		if (p->port_no < num)
			ifindices[p->port_no] = p->dev->ifindex;
	}
#if defined(CONFIG_SONOS)
	list_for_each_entry(p, &br->leaf_list, list) {
		if (p->port_no < num)
			ifindices[p->port_no] = p->dev->ifindex;
	}
#endif
}

/*
 * Format up to a page worth of forwarding table entries
 * userbuf -- where to copy result
 * maxnum  -- maximum number of entries desired
 *            (limited to a page for sanity)
 * offset  -- number of records to skip
 */
static int get_fdb_entries(struct net_bridge *br, void __user *userbuf,
			   unsigned long maxnum, unsigned long offset)
{
	int num;
	void *buf;
	size_t size;

	/* Clamp size to PAGE_SIZE, test maxnum to avoid overflow */
	if (maxnum > PAGE_SIZE/sizeof(struct __fdb_entry))
		maxnum = PAGE_SIZE/sizeof(struct __fdb_entry);

	size = maxnum * sizeof(struct __fdb_entry);

	buf = kmalloc(size, GFP_USER);
	if (!buf)
		return -ENOMEM;

	num = br_fdb_fillbuf(br, buf, maxnum, offset);
	if (num > 0) {
		if (copy_to_user(userbuf, buf, num*sizeof(struct __fdb_entry)))
			num = -EFAULT;
	}
	kfree(buf);

	return num;
}

/* called with RTNL */
static int add_del_if(struct net_bridge *br, int ifindex, int isadd)
{
	struct net *net = dev_net(br->dev);
	struct net_device *dev;
	int ret;

	if (!ns_capable(net->user_ns, CAP_NET_ADMIN))
		return -EPERM;

#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	dev = dev_get_by_index(net, ifindex);
#else
	dev = __dev_get_by_index(net, ifindex);
#endif
	if (dev == NULL)
		return -EINVAL;

	if (isadd)
		ret = br_add_if(br, dev, NULL);
	else
		ret = br_del_if(br, dev);

#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	dev_put(dev);
#endif
	return ret;
}

/*
 * Legacy ioctl's through SIOCDEVPRIVATE
 * This interface is deprecated because it was too difficult to
 * to do the translation for 32/64bit ioctl compatibility.
 */
static int old_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct net_bridge *br = netdev_priv(dev);
	struct net_bridge_port *p = NULL;
	unsigned long args[4];
	int ret = -EOPNOTSUPP;

	if (copy_from_user(args, rq->ifr_data, sizeof(args)))
		return -EFAULT;

	switch (args[0]) {
	case BRCTL_ADD_IF:
	case BRCTL_DEL_IF:
		return add_del_if(br, args[1], args[0] == BRCTL_ADD_IF);

	case BRCTL_GET_BRIDGE_INFO:
	{
		struct __bridge_info b;

		memset(&b, 0, sizeof(struct __bridge_info));
		rcu_read_lock();
		memcpy(&b.designated_root, &br->designated_root, 8);
		memcpy(&b.bridge_id, &br->bridge_id, 8);
		b.root_path_cost = br->root_path_cost;
		b.max_age = jiffies_to_clock_t(br->max_age);
		b.hello_time = jiffies_to_clock_t(br->hello_time);
#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
		b.forward_delay = jiffies_to_clock_t(br->forward_delay);
		b.bridge_max_age = jiffies_to_clock_t(br->bridge_max_age);
		b.bridge_hello_time = jiffies_to_clock_t(br->bridge_hello_time);
#else
		b.forward_delay = br->forward_delay;
		b.bridge_max_age = br->bridge_max_age;
		b.bridge_hello_time = br->bridge_hello_time;
#endif
		b.bridge_forward_delay = jiffies_to_clock_t(br->bridge_forward_delay);
		b.topology_change = br->topology_change;
		b.topology_change_detected = br->topology_change_detected;
		b.root_port = br->root_port;

		b.stp_enabled = (br->stp_enabled != BR_NO_STP);
		b.ageing_time = jiffies_to_clock_t(br->ageing_time);
		b.hello_timer_value = br_timer_value(&br->hello_timer);
		b.tcn_timer_value = br_timer_value(&br->tcn_timer);
		b.topology_change_timer_value = br_timer_value(&br->topology_change_timer);
		b.gc_timer_value = br_timer_value(&br->gc_work.timer);
		rcu_read_unlock();

		if (copy_to_user((void __user *)args[1], &b, sizeof(b)))
			return -EFAULT;

		return 0;
	}

	case BRCTL_GET_PORT_LIST:
	{
		int num, *indices;

		num = args[2];
		if (num < 0)
			return -EINVAL;
		if (num == 0)
			num = 256;
		if (num > BR_MAX_PORTS)
			num = BR_MAX_PORTS;

		indices = kcalloc(num, sizeof(int), GFP_KERNEL);
		if (indices == NULL)
			return -ENOMEM;

		get_port_ifindices(br, indices, num);
		if (copy_to_user((void __user *)args[1], indices, num*sizeof(int)))
			num =  -EFAULT;
		kfree(indices);
		return num;
	}

	case BRCTL_SET_BRIDGE_FORWARD_DELAY:
		if (!ns_capable(dev_net(dev)->user_ns, CAP_NET_ADMIN))
			return -EPERM;

		ret = br_set_forward_delay(br, args[1]);
		break;

	case BRCTL_SET_BRIDGE_HELLO_TIME:
		if (!ns_capable(dev_net(dev)->user_ns, CAP_NET_ADMIN))
			return -EPERM;

		ret = br_set_hello_time(br, args[1]);
		break;

	case BRCTL_SET_BRIDGE_MAX_AGE:
		if (!ns_capable(dev_net(dev)->user_ns, CAP_NET_ADMIN))
			return -EPERM;

		ret = br_set_max_age(br, args[1]);
		break;

	case BRCTL_SET_AGEING_TIME:
		if (!ns_capable(dev_net(dev)->user_ns, CAP_NET_ADMIN))
			return -EPERM;

		ret = br_set_ageing_time(br, args[1]);
		break;

	case BRCTL_GET_PORT_INFO:
	{
		struct __port_info p;
		struct net_bridge_port *pt;

		rcu_read_lock();
		if ((pt = br_get_port(br, args[2])) == NULL
#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
		    &&(pt = br_get_leaf(br, args[2])) == NULL
#endif
		) {
			rcu_read_unlock();
			return -EINVAL;
		}

		memset(&p, 0, sizeof(struct __port_info));
		memcpy(&p.designated_root, &pt->designated_root, 8);
		memcpy(&p.designated_bridge, &pt->designated_bridge, 8);
		p.port_id = pt->port_id;
		p.designated_port = pt->designated_port;
		p.path_cost = pt->path_cost;
		p.designated_cost = pt->designated_cost;
		p.state = pt->state;
		p.top_change_ack = pt->topology_change_ack;
		p.config_pending = pt->config_pending;
		p.message_age_timer_value = br_timer_value(&pt->message_age_timer);
		p.forward_delay_timer_value = br_timer_value(&pt->forward_delay_timer);
		p.hold_timer_value = br_timer_value(&pt->hold_timer);

#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
		sonos_get_port_info(&p, pt);
#endif
		rcu_read_unlock();

		if (copy_to_user((void __user *)args[1], &p, sizeof(p)))
			return -EFAULT;

		return 0;
	}

	case BRCTL_SET_BRIDGE_STP_STATE:
		if (!ns_capable(dev_net(dev)->user_ns, CAP_NET_ADMIN))
			return -EPERM;

		br_stp_set_enabled(br, args[1]);
		ret = 0;
		break;

	case BRCTL_SET_BRIDGE_PRIORITY:
		if (!ns_capable(dev_net(dev)->user_ns, CAP_NET_ADMIN))
			return -EPERM;

		br_stp_set_bridge_priority(br, args[1]);
		ret = 0;
		break;

	case BRCTL_SET_PORT_PRIORITY:
	{
		if (!ns_capable(dev_net(dev)->user_ns, CAP_NET_ADMIN))
			return -EPERM;

#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
		if (args[2] >= (1<<(16-BR_PORT_BITS)))
			return -ERANGE;
#endif
		spin_lock_bh(&br->lock);
		if ((p = br_get_port(br, args[1])) == NULL)
			ret = -EINVAL;
		else
			ret = br_stp_set_port_priority(p, args[2]);
		spin_unlock_bh(&br->lock);
		break;
	}

	case BRCTL_SET_PATH_COST:
	{
		if (!ns_capable(dev_net(dev)->user_ns, CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_bh(&br->lock);
		if ((p = br_get_port(br, args[1])) == NULL)
			ret = -EINVAL;
		else
			ret = br_stp_set_path_cost(p, args[2]);
		spin_unlock_bh(&br->lock);
		break;
	}

	case BRCTL_GET_FDB_ENTRIES:
		return get_fdb_entries(br, (void __user *)args[1],
				       args[2], args[3]);

#if defined(CONFIG_SONOS) /* SONOS SWPBL-70338 */
	case BRCTL_ADD_P2P_TUNNEL:
	case BRCTL_SET_P2P_TUNNEL_PATH_COST:
	case BRCTL_ADD_P2P_TUNNEL_LEAF:
	case BRCTL_DEL_P2P_TUNNEL:
	case BRCTL_SET_P2P_TUNNEL_STP_STATE:
	case BRCTL_SET_P2P_DIRECT_ADDR:
	case BRCTL_SET_P2P_DIRECT_ENABLED:
	case BRCTL_ADD_UPLINK:
		return sonos_brctl_wrapper(br, args);

	case BRCTL_MOD_PORT_ADDR:
		return sonos_mod_port_addr(br, (void __user *)args[1], (void __user *)args[2]);

	case BRCTL_MOD_PORT_DEV:
		return sonos_mod_port_dev(br, dev, (void __user *)args[1], args[2]);

	case BRCTL_MCAST_GET_FDB_ENTRIES:
		return br_mcast_fdb_get_entries(br, (unsigned char *)args[1],
						args[2], args[3]);

	case BRCTL_GET_P2P_TUNNEL_STATES:
		return sonos_get_p2p_tunnel_states(br, args[1], (void __user *)args[2], (void __user *)args[3]);

	case BRCTL_GET_ROUTING_CAPABILITIES:
		return sonos_get_routing_capabilities(br, (void __user *)args[1]);

	case BRCTL_GET_ANY_FORWARDING:
		return sonos_get_any_forwarding(br, (void __user *)args[1]);

	case BRCTL_SET_STATIC_MAC:
		return sonos_br_set_static_mac(br, (void __user *)args[1]);

	case BRCTL_SET_UPLINK_MODE:
		return br_set_uplink_mode(br, (args[1] != 0));

	case BRCTL_GET_STATS:
		return sonos_get_stats(br, (void __user *)args[1]);
#endif
	}

	if (!ret) {
		if (p)
			br_ifinfo_notify(RTM_NEWLINK, NULL, p);
		else
			netdev_state_change(br->dev);
	}

	return ret;
}

static int old_deviceless(struct net *net, void __user *uarg)
{
	unsigned long args[3];

	if (copy_from_user(args, uarg, sizeof(args)))
		return -EFAULT;

	switch (args[0]) {
	case BRCTL_GET_VERSION:
		return BRCTL_VERSION;

	case BRCTL_GET_BRIDGES:
	{
		int *indices;
		int ret = 0;

		if (args[2] >= 2048)
			return -ENOMEM;
		indices = kcalloc(args[2], sizeof(int), GFP_KERNEL);
		if (indices == NULL)
			return -ENOMEM;

		args[2] = get_bridge_ifindices(net, indices, args[2]);

		ret = copy_to_user((void __user *)args[1], indices, args[2]*sizeof(int))
			? -EFAULT : args[2];

		kfree(indices);
		return ret;
	}

	case BRCTL_ADD_BRIDGE:
	case BRCTL_DEL_BRIDGE:
	{
		char buf[IFNAMSIZ];

		if (!ns_capable(net->user_ns, CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, (void __user *)args[1], IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ-1] = 0;

		if (args[0] == BRCTL_ADD_BRIDGE)
			return br_add_bridge(net, buf);

		return br_del_bridge(net, buf);
	}
	}

	return -EOPNOTSUPP;
}

int br_ioctl_deviceless_stub(struct net *net, unsigned int cmd, void __user *uarg)
{
	switch (cmd) {
	case SIOCGIFBR:
	case SIOCSIFBR:
		return old_deviceless(net, uarg);

	case SIOCBRADDBR:
	case SIOCBRDELBR:
	{
		char buf[IFNAMSIZ];

		if (!ns_capable(net->user_ns, CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, uarg, IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ-1] = 0;
		if (cmd == SIOCBRADDBR)
			return br_add_bridge(net, buf);

		return br_del_bridge(net, buf);
	}
	}
	return -EOPNOTSUPP;
}

int br_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct net_bridge *br = netdev_priv(dev);

	switch (cmd) {
	case SIOCDEVPRIVATE:
		return old_dev_ioctl(dev, rq, cmd);

	case SIOCBRADDIF:
	case SIOCBRDELIF:
		return add_del_if(br, rq->ifr_ifindex, cmd == SIOCBRADDIF);

	}

	br_debug(br, "Bridge does not support ioctl 0x%x\n", cmd);
	return -EOPNOTSUPP;
}
