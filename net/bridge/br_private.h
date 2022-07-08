/*
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

#ifndef _BR_PRIVATE_H
#define _BR_PRIVATE_H

#include <linux/netdevice.h>
#include <linux/if_bridge.h>
#include <linux/netpoll.h>
#include <linux/u64_stats_sync.h>
#include <net/route.h>
#include <linux/if_vlan.h>

/*
 * SONOS SWPBL-66817: As part of the porting effort from Linux-2.6.35 to
 * Linux-3.10.17 (SWPBL-19651), most references to spin_(un)lock(...) in the
 * bridge were replaced by spin_(un)lock_bh(...). These SONOS_SPIN(UN)LOCK(...)
 * macros highlight the places where this change was made, without changing the
 * functionality of bridge locking.
 */
#if defined(CONFIG_SONOS)

#define SONOS_SPINLOCK(_plock) \
	(spin_lock_bh(_plock))
#define SONOS_SPINUNLOCK(_plock) \
	(spin_unlock_bh(_plock))

#else

#define SONOS_SPINLOCK(_plock) \
	(spin_lock(_plock))
#define SONOS_SPINUNLOCK(_plock) \
	(spin_unlock(_plock))

#endif /* SONOS SWPBL-66817 */

#define BR_HASH_BITS 8
#define BR_HASH_SIZE (1 << BR_HASH_BITS)

#define BR_HOLD_TIME (1*HZ)

/*
 * SONOS: Make BR_PORT_BITS match other products (8 instead of 10).  We don't
 *        need more than this, and it makes it easier to play "what's
 *        different" when debugging Casbah bridge code
 */
#if defined(CONFIG_SONOS)
#define BR_PORT_BITS	8
#else /* !defined(CONFIG_SONOS) */
#define BR_PORT_BITS	10
#endif

#define BR_MAX_PORTS	(1<<BR_PORT_BITS)
#define BR_VLAN_BITMAP_LEN	BITS_TO_LONGS(VLAN_N_VID)

#if defined(CONFIG_SONOS)
#define BR_VERSION	"6.9"
#else /* !defined(CONFIG_SONOS) */
#define BR_VERSION	"2.3"
#endif

/* Control of forwarding link local multicast */
#define BR_GROUPFWD_DEFAULT	0
/* Don't allow forwarding control protocols like STP and LLDP */
#define BR_GROUPFWD_RESTRICTED	0x4007u

#if defined(CONFIG_SONOS)

#define RX_STATS_CHECK_INTERVAL 10
/*
 * Frames per second to detect MC/BC stream.
 * This number is chosen because voice stream like VOIP may be sent by 33.3
 * or 50 frames per second, and it should be a safe threshold to detect video
 * stream.
 */
#define BCMC_REPORT_FPS_THRESHOLD 64
#define BCMC_REPORT_TOTAL_PACKETS (BCMC_REPORT_FPS_THRESHOLD * RX_STATS_CHECK_INTERVAL)

#define BR_BCMC_HIST_SIZE         10
#define MCAST_TYPE		  1
#define BCAST_TYPE		  2

#define BR_MAX_MCAST_GROUPS 16
#else /* !defined(CONFIG_SONOS) */
/* Path to usermode spanning tree program */
#define BR_STP_PROG	"/sbin/bridge-stp"
#endif

typedef struct bridge_id bridge_id;
typedef struct mac_addr mac_addr;
typedef __u16 port_id;

struct bridge_id
{
	unsigned char	prio[2];
	unsigned char	addr[6];
};

struct mac_addr
{
	unsigned char	addr[6];
};

struct br_ip
{
	union {
		__be32	ip4;
#if IS_ENABLED(CONFIG_IPV6)
		struct in6_addr ip6;
#endif
	} u;
	__be16		proto;
	__u16		vid;
};

struct net_port_vlans {
	u16				port_idx;
	u16				pvid;
	union {
		struct net_bridge_port		*port;
		struct net_bridge		*br;
	}				parent;
	struct rcu_head			rcu;
	unsigned long			vlan_bitmap[BR_VLAN_BITMAP_LEN];
	unsigned long			untagged_bitmap[BR_VLAN_BITMAP_LEN];
	u16				num_vlans;
};

struct net_bridge_fdb_entry
{
	struct hlist_node		hlist;
	struct net_bridge_port		*dst;

	struct rcu_head			rcu;
	unsigned long			updated;
	unsigned long			used;
	mac_addr			addr;
	unsigned char			is_local;
	unsigned char			is_static;
	__u16				vlan_id;

#if defined(CONFIG_SONOS)
	struct net_bridge_port		*dst_direct;
	int                             priority;
	atomic_t			use_count;
	unsigned long			ageing_timer;
#endif
};

struct net_bridge_port_group {
	struct net_bridge_port		*port;
	struct net_bridge_port_group __rcu *next;
	struct hlist_node		mglist;
	struct rcu_head			rcu;
	struct timer_list		timer;
	struct br_ip			addr;
	unsigned char			state;
};

struct net_bridge_mdb_entry
{
	struct hlist_node		hlist[2];
	struct net_bridge		*br;
	struct net_bridge_port_group __rcu *ports;
	struct rcu_head			rcu;
	struct timer_list		timer;
	struct br_ip			addr;
	bool				mglist;
};

struct net_bridge_mdb_htable
{
	struct hlist_head		*mhash;
	struct rcu_head			rcu;
	struct net_bridge_mdb_htable	*old;
	u32				size;
	u32				max;
	u32				secret;
	u32				ver;
};

#if defined(CONFIG_SONOS)
/* This is the list of members of the multicast group associated with a
   particular port on the bridge. */
struct net_bridge_mcast_rx_mac
{
	unsigned char                   addr[6];
	unsigned long                   ageing_timer;
	uint32_t                        ip;
	struct net_bridge_port		*direct_dst;
	struct net_bridge_mcast_rx_mac  *next;
};

/* This is the list of ports where members of the multicast group have been
   observed.  An entry with dst == 0 indicates the local bridge interface is
   a member of the multicast group. */
struct net_bridge_mcast_rx_port
{
	struct net_bridge_mcast_rx_port *next;
	struct net_bridge_port		*dst;
	struct net_bridge_mcast_rx_mac  *rx_mac_list;
};

/* This tracks each multicast group.
 */
struct net_bridge_mcast_entry
{
	struct net_bridge_mcast_entry  *next_hash;
	struct net_bridge_mcast_entry  *prev_hash;
	atomic_t			use_count;
	unsigned char                   addr[6];
	struct net_bridge_mcast_rx_port *rx_port_list;
};
#endif /* CONFIG_SONOS */

struct net_bridge_port
{
	struct net_bridge		*br;
	struct net_device		*dev;
	struct list_head		list;

#if defined(CONFIG_SONOS)
	/* Point-to-Point packet tunnelling */
	unsigned int                    is_p2p:1;
	unsigned int                    is_leaf:1;
	unsigned int                    is_unencap:1;
	unsigned int                    is_unicast:1;
	unsigned int                    is_uplink:1;
#ifdef CONFIG_SONOS_BRIDGE_PROXY
	unsigned int                    is_satellite:1;
	u32                             sat_ip;
#endif
	unsigned char                   p2p_dest_addr[6];
#endif /* CONFIG_SONOS */

	/* STP */
	u8				priority;
#if defined(CONFIG_SONOS)
	u16				state;
	u32				port_no;
	u16				remote_state;
#else /* !defined(CONFIG_SONOS) */
	u8				state;
	u16				port_no;
#endif
	unsigned char			topology_change_ack;
	unsigned char			config_pending;
	port_id				port_id;
	port_id				designated_port;
	bridge_id			designated_root;
	bridge_id			designated_bridge;
	u32				path_cost;
	u32				designated_cost;
#if defined(CONFIG_SONOS)
	/* direct routing */
	u8                              direct_enabled;
	unsigned char                   direct_addr[6];
	unsigned long                   direct_last_stp_time;
        /* Station mode direct routing priority */
        u8                              sonos_direct_skb_priority;
#else /* !defined(CONFIG_SONOS) */
	unsigned long			designated_age;
#endif

	struct timer_list		forward_delay_timer;
	struct timer_list		hold_timer;
	struct timer_list		message_age_timer;
	struct kobject			kobj;
	struct rcu_head			rcu;

	unsigned long 			flags;
#define BR_HAIRPIN_MODE		0x00000001
#define BR_BPDU_GUARD           0x00000002
#define BR_ROOT_BLOCK		0x00000004
#define BR_MULTICAST_FAST_LEAVE	0x00000008
#define BR_ADMIN_COST		0x00000010

#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
	u32				multicast_startup_queries_sent;
	unsigned char			multicast_router;
	struct timer_list		multicast_router_timer;
	struct timer_list		multicast_query_timer;
	struct hlist_head		mglist;
	struct hlist_node		rlist;
#endif

#ifdef CONFIG_SYSFS
	char				sysfs_name[IFNAMSIZ];
#endif

#ifdef CONFIG_NET_POLL_CONTROLLER
	struct netpoll			*np;
#endif
#ifdef CONFIG_BRIDGE_VLAN_FILTERING
	struct net_port_vlans __rcu	*vlan_info;
#endif
};

/* SONOS: Every port is on two lists.
 *
 *        The first is maintained using br_port_list, which is a struct
 *        net_bridge_port_list_node *, in struct net_device.
 *        This list contains all ports on the bridge, including leaf ports.
 *
 *        The second list is either port_list or leaf_list in struct
 *        net_bridge.  These contain the only the leaf ports and only the
 *        non-leaf ports, respectively.
 */
#if defined(CONFIG_SONOS)
struct net_bridge_port_list_node
{
	struct net_bridge_port*                port;
	struct net_bridge_port_list_node*      next;
};
#endif

#define br_port_exists(dev) (dev->priv_flags & IFF_BRIDGE_PORT)

static inline struct net_bridge_port *br_port_get_rcu(const struct net_device *dev)
{
	return rcu_dereference(dev->rx_handler_data);
}

static inline struct net_bridge_port *br_port_get_rtnl(const struct net_device *dev)
{
	return br_port_exists(dev) ?
		rtnl_dereference(dev->rx_handler_data) : NULL;
}

struct br_cpu_netstats {
	u64			rx_packets;
	u64			rx_bytes;
	u64			tx_packets;
	u64			tx_bytes;
	struct u64_stats_sync	syncp;
};

#if defined(CONFIG_SONOS)
struct net_bridge_bcmc_hit
{
	unsigned char   src[ETH_ALEN];
	unsigned char   dest[ETH_ALEN];
	unsigned long   timestamp;
	unsigned long   packet_count;
	u8              packet_type;
};

struct net_bridge_stats
{
	unsigned long   rx_mc_count;
	unsigned long   rx_mc_count_peak;
	unsigned long   rx_mc_peak_ts;
	unsigned long   rx_mc_hit;
	unsigned char	rx_mc_hit_src[ETH_ALEN];
	unsigned char	rx_mc_hit_dest[ETH_ALEN];
	unsigned long   rx_bc_count;
	unsigned long   rx_bc_count_peak;
	unsigned long   rx_bc_peak_ts;
	unsigned long   rx_bc_hit;
	unsigned char	rx_bc_hit_src[ETH_ALEN];
	unsigned char	rx_bc_hit_dest[ETH_ALEN];
	unsigned long   rx_start_time;

	unsigned int    bcmc_index;
	struct net_bridge_bcmc_hit bcmc_history[BR_BCMC_HIST_SIZE];
};
#endif /* CONFIG_SONOS */

struct net_bridge
{
	spinlock_t			lock;
	struct list_head		port_list;
	struct net_device		*dev;
#if defined(CONFIG_SONOS)
	struct list_head		leaf_list;
	struct net_device_stats		statistics;
	struct list_head		age_list;
#endif

	struct br_cpu_netstats __percpu *stats;
	spinlock_t			hash_lock;
	struct hlist_head		hash[BR_HASH_SIZE];
#ifdef CONFIG_BRIDGE_NETFILTER
	struct rtable 			fake_rtable;
	bool				nf_call_iptables;
	bool				nf_call_ip6tables;
	bool				nf_call_arptables;
#endif
	u16				group_fwd_mask;

	/* STP */
	bridge_id			designated_root;
	bridge_id			bridge_id;
	u32				root_path_cost;
	unsigned long			max_age;
	unsigned long			hello_time;
	unsigned long			forward_delay;
	unsigned long			bridge_max_age;
	unsigned long			ageing_time;
	unsigned long			bridge_hello_time;
	unsigned long			bridge_forward_delay;
#if defined(CONFIG_SONOS)
	unsigned long			mcast_ageing_time;
#endif

	u8				group_addr[ETH_ALEN];
	u16				root_port;

	enum {
		BR_NO_STP, 		/* no spanning tree */
		BR_KERNEL_STP,		/* old STP in kernel */
		BR_USER_STP,		/* new RSTP in userspace */
	} stp_enabled;

	unsigned char			topology_change;
	unsigned char			topology_change_detected;

#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
	unsigned char			multicast_router;

	u8				multicast_disabled:1;
	u8				multicast_querier:1;

	u32				hash_elasticity;
	u32				hash_max;

	u32				multicast_last_member_count;
	u32				multicast_startup_queries_sent;
	u32				multicast_startup_query_count;

	unsigned long			multicast_last_member_interval;
	unsigned long			multicast_membership_interval;
	unsigned long			multicast_querier_interval;
	unsigned long			multicast_query_interval;
	unsigned long			multicast_query_response_interval;
	unsigned long			multicast_startup_query_interval;

	spinlock_t			multicast_lock;
	struct net_bridge_mdb_htable __rcu *mdb;
	struct hlist_head		router_list;

	struct timer_list		multicast_router_timer;
	struct timer_list		multicast_querier_timer;
	struct timer_list		multicast_query_timer;
#endif

	struct timer_list		hello_timer;
	struct timer_list		tcn_timer;
	struct timer_list		topology_change_timer;
	struct timer_list		gc_timer;
	struct kobject			*ifobj;
#ifdef CONFIG_BRIDGE_VLAN_FILTERING
	u8				vlan_enabled;
	struct net_port_vlans __rcu	*vlan_info;
#endif

#if defined(CONFIG_SONOS)
	/* SONOS: Multicast group management */
	unsigned                        num_mcast_groups;
	unsigned char                   mcast_groups[BR_MAX_MCAST_GROUPS][6];
	int                             mcast_advertise_time;
	spinlock_t                      mcast_lock;
	struct timer_list		mcast_timer;
	struct net_bridge_mcast_entry  *mcast_hash[BR_HASH_SIZE];

	/* SONOS: Fixed MAC address */
	unsigned char                   use_static_mac;
	unsigned char                   static_mac[6];

	/* SONOS: Uplink port */
	unsigned char                   uplink_mode;
#ifdef CONFIG_SONOS_BRIDGE_PROXY
	unsigned char                   proxy_mode;
	u32                             current_ipv4_addr;
	struct timer_list               dupip_timer;
	unsigned long                   dupip_start;
#endif
	struct net_bridge_stats         br_stats;
#endif /* CONFIG_SONOS */
};

struct br_input_skb_cb {
	struct net_device *brdev;
#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
	int igmp;
	int mrouters_only;
#endif
};

#define BR_INPUT_SKB_CB(__skb)	((struct br_input_skb_cb *)(__skb)->cb)

#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
# define BR_INPUT_SKB_CB_MROUTERS_ONLY(__skb)	(BR_INPUT_SKB_CB(__skb)->mrouters_only)
#else
# define BR_INPUT_SKB_CB_MROUTERS_ONLY(__skb)	(0)
#endif

#define br_printk(level, br, format, args...)	\
	printk(level "%s: " format, (br)->dev->name, ##args)

#define br_err(__br, format, args...)			\
	br_printk(KERN_ERR, __br, format, ##args)
#define br_warn(__br, format, args...)			\
	br_printk(KERN_WARNING, __br, format, ##args)
#define br_notice(__br, format, args...)		\
	br_printk(KERN_NOTICE, __br, format, ##args)
#define br_info(__br, format, args...)			\
	br_printk(KERN_INFO, __br, format, ##args)

#define br_debug(br, format, args...)			\
	pr_debug("%s: " format,  (br)->dev->name, ##args)

extern struct notifier_block br_device_notifier;

#if defined(CONFIG_SONOS)
extern struct notifier_block br_inetaddr_notifier;

extern const unsigned char bridge_ula[6];
#define br_group_address bridge_ula

struct br_cb {
	unsigned char direct:1;
#ifdef CONFIG_SONOS_BRIDGE_PROXY
	unsigned char should_proxy_up:1;
	struct net_bridge_port *source_port;
#endif
};

#define BR_SKB_CB(skb)    ((struct br_cb *)(&skb->cb[0]))

#endif /* CONFIG_SONOS */

/* called under bridge lock */
static inline int br_is_root_bridge(const struct net_bridge *br)
{
	return !memcmp(&br->bridge_id, &br->designated_root, 8);
}

/* br_device.c */
extern void br_dev_setup(struct net_device *dev);
extern void br_dev_delete(struct net_device *dev, struct list_head *list);
extern netdev_tx_t br_dev_xmit(struct sk_buff *skb,
			       struct net_device *dev);
#ifdef CONFIG_NET_POLL_CONTROLLER
static inline struct netpoll_info *br_netpoll_info(struct net_bridge *br)
{
	return br->dev->npinfo;
}

static inline void br_netpoll_send_skb(const struct net_bridge_port *p,
				       struct sk_buff *skb)
{
	struct netpoll *np = p->np;

	if (np)
		netpoll_send_skb(np, skb);
}

extern int br_netpoll_enable(struct net_bridge_port *p, gfp_t gfp);
extern void br_netpoll_disable(struct net_bridge_port *p);
#else
static inline struct netpoll_info *br_netpoll_info(struct net_bridge *br)
{
	return NULL;
}

static inline void br_netpoll_send_skb(const struct net_bridge_port *p,
				       struct sk_buff *skb)
{
}

static inline int br_netpoll_enable(struct net_bridge_port *p, gfp_t gfp)
{
	return 0;
}

static inline void br_netpoll_disable(struct net_bridge_port *p)
{
}
#endif

/* br_fdb.c */
extern int br_fdb_init(void);
extern void br_fdb_fini(void);
extern void br_fdb_flush(struct net_bridge *br);
#if defined(CONFIG_SONOS)
extern void br_fdb_changeaddr(struct net_bridge_port_list_node *pl,
			      const unsigned char *newaddr);
extern void br_fdb_cleanup(unsigned long arg);
extern void br_fdb_delete_by_port(struct net_bridge *br,
				  const struct net_bridge_port *p, int do_all);
extern struct net_bridge_fdb_entry *__br_fdb_get(struct net_bridge *br,
						 const unsigned char *addr,
						 __u16 vid);
extern int br_fdb_fillbuf(struct net_bridge *br, void *buf,
			  unsigned long count, unsigned long off);
extern int br_fdb_insert(struct net_bridge *br,
			 struct net_bridge_port *source,
			 const unsigned char *addr,
			 u16 vid);
extern int br_sonos_fdb_insert(struct net_bridge *br,
			       struct net_bridge_port *source,
			       const unsigned char *addr);
extern struct net_bridge_fdb_entry *br_fdb_update(struct net_bridge *br,
						  struct net_bridge_port *source,
						  const unsigned char *addr,
						  u16 vid);
extern void br_fdb_rcu_free(struct rcu_head *head);
void br_sonos_fdb_delete(struct net_bridge *br, struct net_bridge_fdb_entry *f);
static inline int br_fdb_dump(struct sk_buff *skb,
			      struct netlink_callback *cb,
			      struct net_device *dev,
			      int idx)
{
	return 0;
}
static inline int br_fdb_delete(struct ndmsg *ndm, struct nlattr *tb[],
				struct net_device *dev,
				const unsigned char *addr)
{
	return 0;
}
static inline int br_fdb_add(struct ndmsg *nlh, struct nlattr *tb[],
			     struct net_device *dev,
			     const unsigned char *addr,
			     u16 nlh_flags)
{
	return 0;
}
#else /* !defined(CONFIG_SONOS) */
extern void br_fdb_changeaddr(struct net_bridge_port *p,
			      const unsigned char *newaddr);
extern void br_fdb_change_mac_address(struct net_bridge *br, const u8 *newaddr);
extern void br_fdb_cleanup(unsigned long arg);
extern void br_fdb_delete_by_port(struct net_bridge *br,
				  const struct net_bridge_port *p, int do_all);
extern struct net_bridge_fdb_entry *__br_fdb_get(struct net_bridge *br,
						 const unsigned char *addr,
						 __u16 vid);
extern int br_fdb_test_addr(struct net_device *dev, unsigned char *addr);
extern int br_fdb_fillbuf(struct net_bridge *br, void *buf,
			  unsigned long count, unsigned long off);
extern int br_fdb_insert(struct net_bridge *br,
			 struct net_bridge_port *source,
			 const unsigned char *addr,
			 u16 vid);
extern void br_fdb_update(struct net_bridge *br,
			  struct net_bridge_port *source,
			  const unsigned char *addr,
			  u16 vid);
extern int fdb_delete_by_addr(struct net_bridge *br, const u8 *addr, u16 vid);

extern int br_fdb_delete(struct ndmsg *ndm, struct nlattr *tb[],
			 struct net_device *dev,
			 const unsigned char *addr);
extern int br_fdb_add(struct ndmsg *nlh, struct nlattr *tb[],
		      struct net_device *dev,
		      const unsigned char *addr,
		      u16 nlh_flags);
extern int br_fdb_dump(struct sk_buff *skb,
		       struct netlink_callback *cb,
		       struct net_device *dev,
		       int idx);
#endif

/* br_forward.c */
#if defined(CONFIG_SONOS)
extern void br_deliver(const struct net_bridge_port *from,
		       const struct net_bridge_port *to,
		       struct sk_buff *skb);
extern void br_forward(const struct net_bridge_port *from,
		       const struct net_bridge_port *to,
		       struct sk_buff *skb);
extern void br_flood_deliver(struct net_bridge *br,
			     struct net_bridge_port *from,
			     struct sk_buff *skb,
			     int clone);
extern void br_flood_forward(struct net_bridge *br,
			     struct net_bridge_port *from,
			     struct sk_buff *skb,
			     int clone);
#else /* !defined(CONFIG_SONOS) */
extern void br_deliver(const struct net_bridge_port *to,
		struct sk_buff *skb);
extern int br_dev_queue_push_xmit(struct sk_buff *skb);
extern void br_forward(const struct net_bridge_port *to,
		struct sk_buff *skb, struct sk_buff *skb0);
extern int br_forward_finish(struct sk_buff *skb);
extern void br_flood_deliver(struct net_bridge *br, struct sk_buff *skb);
extern void br_flood_forward(struct net_bridge *br, struct sk_buff *skb,
			     struct sk_buff *skb2);
#endif

/* br_if.c */
extern int br_add_bridge(struct net *net, const char *name);
extern int br_del_bridge(struct net *net, const char *name);
extern void br_net_exit(struct net *net);
extern int br_add_if(struct net_bridge *br,
	      struct net_device *dev);
extern int br_del_if(struct net_bridge *br,
	      struct net_device *dev);
extern int br_min_mtu(const struct net_bridge *br);
#if defined(CONFIG_SONOS)
extern void br_sonos_destroy_nbp(struct net_bridge_port *p);
extern void br_features_recompute(struct net_bridge *br);
extern void br_sonos_del_nbp(struct net_bridge_port *p);
extern struct net_bridge_port *br_sonos_new_nbp(struct net_bridge *br,
						struct net_device *dev);
#else /* !defined(CONFIG_SONOS) */
extern void br_port_carrier_check(struct net_bridge_port *p);
extern netdev_features_t br_features_recompute(struct net_bridge *br,
	netdev_features_t features);
#endif

/* br_input.c */
#if defined(CONFIG_SONOS)
extern int br_handle_frame_finish(struct net_bridge_port *p, struct sk_buff *skb);
extern struct sk_buff *br_handle_frame(struct net_bridge_port_list_node *pl,
				       struct sk_buff *skb);
struct net_bridge_port* br_find_port(const unsigned char *h_source,
				     struct net_bridge_port_list_node *pl);
/* REVIEW: Used by br_uplink now, so these can no longer be static.  Ugh... */
extern void br_pass_frame_up(struct net_bridge *br, struct sk_buff *skb);
#else /* !defined(CONFIG_SONOS) */
extern int br_handle_frame_finish(struct sk_buff *skb);
extern rx_handler_result_t br_handle_frame(struct sk_buff **pskb);
#endif

/* br_ioctl.c */
extern int br_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
extern int br_ioctl_deviceless_stub(struct net *net, unsigned int cmd, void __user *arg);

/* br_multicast.c */
#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
extern unsigned int br_mdb_rehash_seq;
extern int br_multicast_rcv(struct net_bridge *br,
			    struct net_bridge_port *port,
			    struct sk_buff *skb);
extern struct net_bridge_mdb_entry *br_mdb_get(struct net_bridge *br,
					       struct sk_buff *skb, u16 vid);
extern void br_multicast_add_port(struct net_bridge_port *port);
extern void br_multicast_del_port(struct net_bridge_port *port);
extern void br_multicast_enable_port(struct net_bridge_port *port);
extern void br_multicast_disable_port(struct net_bridge_port *port);
extern void br_multicast_init(struct net_bridge *br);
extern void br_multicast_open(struct net_bridge *br);
extern void br_multicast_stop(struct net_bridge *br);
extern void br_multicast_deliver(struct net_bridge_mdb_entry *mdst,
				 struct sk_buff *skb);
extern void br_multicast_forward(struct net_bridge_mdb_entry *mdst,
				 struct sk_buff *skb, struct sk_buff *skb2);
extern int br_multicast_set_router(struct net_bridge *br, unsigned long val);
extern int br_multicast_set_port_router(struct net_bridge_port *p,
					unsigned long val);
extern int br_multicast_toggle(struct net_bridge *br, unsigned long val);
extern int br_multicast_set_querier(struct net_bridge *br, unsigned long val);
extern int br_multicast_set_hash_max(struct net_bridge *br, unsigned long val);
extern struct net_bridge_mdb_entry *br_mdb_ip_get(
				struct net_bridge_mdb_htable *mdb,
				struct br_ip *dst);
extern struct net_bridge_mdb_entry *br_multicast_new_group(struct net_bridge *br,
				struct net_bridge_port *port, struct br_ip *group);
extern void br_multicast_free_pg(struct rcu_head *head);
extern struct net_bridge_port_group *br_multicast_new_port_group(
				struct net_bridge_port *port,
				struct br_ip *group,
				struct net_bridge_port_group *next,
				unsigned char state);
extern void br_mdb_init(void);
extern void br_mdb_uninit(void);
extern void br_mdb_notify(struct net_device *dev, struct net_bridge_port *port,
			  struct br_ip *group, int type);

#define mlock_dereference(X, br) \
	rcu_dereference_protected(X, lockdep_is_held(&br->multicast_lock))

#if IS_ENABLED(CONFIG_IPV6)
#include <net/addrconf.h>
static inline int ipv6_is_transient_multicast(const struct in6_addr *addr)
{
	if (ipv6_addr_is_multicast(addr) && IPV6_ADDR_MC_FLAG_TRANSIENT(addr))
		return 1;
	return 0;
}
#endif

static inline bool br_multicast_is_router(struct net_bridge *br)
{
	return br->multicast_router == 2 ||
	       (br->multicast_router == 1 &&
		timer_pending(&br->multicast_router_timer));
}
#else
static inline int br_multicast_rcv(struct net_bridge *br,
				   struct net_bridge_port *port,
				   struct sk_buff *skb)
{
	return 0;
}

static inline struct net_bridge_mdb_entry *br_mdb_get(struct net_bridge *br,
						      struct sk_buff *skb, u16 vid)
{
	return NULL;
}

static inline void br_multicast_add_port(struct net_bridge_port *port)
{
}

static inline void br_multicast_del_port(struct net_bridge_port *port)
{
}

static inline void br_multicast_enable_port(struct net_bridge_port *port)
{
}

static inline void br_multicast_disable_port(struct net_bridge_port *port)
{
}

static inline void br_multicast_init(struct net_bridge *br)
{
}

static inline void br_multicast_open(struct net_bridge *br)
{
}

static inline void br_multicast_stop(struct net_bridge *br)
{
}

static inline void br_multicast_deliver(struct net_bridge_mdb_entry *mdst,
					struct sk_buff *skb)
{
}

static inline void br_multicast_forward(struct net_bridge_mdb_entry *mdst,
					struct sk_buff *skb,
					struct sk_buff *skb2)
{
}
static inline bool br_multicast_is_router(struct net_bridge *br)
{
	return 0;
}
static inline void br_mdb_init(void)
{
}
static inline void br_mdb_uninit(void)
{
}
#endif

/* br_vlan.c */
#ifdef CONFIG_BRIDGE_VLAN_FILTERING
extern bool br_allowed_ingress(struct net_bridge *br, struct net_port_vlans *v,
			       struct sk_buff *skb, u16 *vid);
extern bool br_allowed_egress(struct net_bridge *br,
			      const struct net_port_vlans *v,
			      const struct sk_buff *skb);
extern struct sk_buff *br_handle_vlan(struct net_bridge *br,
				      const struct net_port_vlans *v,
				      struct sk_buff *skb);
extern int br_vlan_add(struct net_bridge *br, u16 vid, u16 flags);
extern int br_vlan_delete(struct net_bridge *br, u16 vid);
extern void br_vlan_flush(struct net_bridge *br);
extern int br_vlan_filter_toggle(struct net_bridge *br, unsigned long val);
extern int nbp_vlan_add(struct net_bridge_port *port, u16 vid, u16 flags);
extern int nbp_vlan_delete(struct net_bridge_port *port, u16 vid);
extern void nbp_vlan_flush(struct net_bridge_port *port);
extern bool nbp_vlan_find(struct net_bridge_port *port, u16 vid);

static inline struct net_port_vlans *br_get_vlan_info(
						const struct net_bridge *br)
{
	return rcu_dereference_rtnl(br->vlan_info);
}

static inline struct net_port_vlans *nbp_get_vlan_info(
						const struct net_bridge_port *p)
{
	return rcu_dereference_rtnl(p->vlan_info);
}

/* Since bridge now depends on 8021Q module, but the time bridge sees the
 * skb, the vlan tag will always be present if the frame was tagged.
 */
static inline int br_vlan_get_tag(const struct sk_buff *skb, u16 *vid)
{
	int err = 0;

	if (vlan_tx_tag_present(skb))
		*vid = vlan_tx_tag_get(skb) & VLAN_VID_MASK;
	else {
		*vid = 0;
		err = -EINVAL;
	}

	return err;
}

static inline u16 br_get_pvid(const struct net_port_vlans *v)
{
	/* Return just the VID if it is set, or VLAN_N_VID (invalid vid) if
	 * vid wasn't set
	 */
	smp_rmb();
	return (v->pvid & VLAN_TAG_PRESENT) ?
			(v->pvid & ~VLAN_TAG_PRESENT) :
			VLAN_N_VID;
}

#else
static inline bool br_allowed_ingress(struct net_bridge *br,
				      struct net_port_vlans *v,
				      struct sk_buff *skb,
				      u16 *vid)
{
	return true;
}

static inline bool br_allowed_egress(struct net_bridge *br,
				     const struct net_port_vlans *v,
				     const struct sk_buff *skb)
{
	return true;
}

static inline struct sk_buff *br_handle_vlan(struct net_bridge *br,
					     const struct net_port_vlans *v,
					     struct sk_buff *skb)
{
	return skb;
}

static inline int br_vlan_add(struct net_bridge *br, u16 vid, u16 flags)
{
	return -EOPNOTSUPP;
}

static inline int br_vlan_delete(struct net_bridge *br, u16 vid)
{
	return -EOPNOTSUPP;
}

static inline void br_vlan_flush(struct net_bridge *br)
{
}

static inline int nbp_vlan_add(struct net_bridge_port *port, u16 vid, u16 flags)
{
	return -EOPNOTSUPP;
}

static inline int nbp_vlan_delete(struct net_bridge_port *port, u16 vid)
{
	return -EOPNOTSUPP;
}

static inline void nbp_vlan_flush(struct net_bridge_port *port)
{
}

static inline struct net_port_vlans *br_get_vlan_info(
						const struct net_bridge *br)
{
	return NULL;
}
static inline struct net_port_vlans *nbp_get_vlan_info(
						const struct net_bridge_port *p)
{
	return NULL;
}

static inline bool nbp_vlan_find(struct net_bridge_port *port, u16 vid)
{
	return false;
}

static inline u16 br_vlan_get_tag(const struct sk_buff *skb, u16 *tag)
{
	return 0;
}
static inline u16 br_get_pvid(const struct net_port_vlans *v)
{
	return VLAN_N_VID;	/* Returns invalid vid */
}
#endif

/* br_netfilter.c */
#ifdef CONFIG_BRIDGE_NETFILTER
extern int br_netfilter_init(void);
extern void br_netfilter_fini(void);
extern void br_netfilter_rtable_init(struct net_bridge *);
#else
#define br_netfilter_init()	(0)
#define br_netfilter_fini()	do { } while(0)
#define br_netfilter_rtable_init(x)
#endif

/* br_stp.c */
extern void br_log_state(const struct net_bridge_port *p);
extern struct net_bridge_port *br_get_port(struct net_bridge *br,
					   u16 port_no);
extern void br_init_port(struct net_bridge_port *p);
extern void br_become_designated_port(struct net_bridge_port *p);

extern void __br_set_forward_delay(struct net_bridge *br, unsigned long t);
extern int br_set_forward_delay(struct net_bridge *br, unsigned long x);
extern int br_set_hello_time(struct net_bridge *br, unsigned long x);
extern int br_set_max_age(struct net_bridge *br, unsigned long x);


/* br_stp_if.c */
extern void br_stp_enable_bridge(struct net_bridge *br);
extern void br_stp_disable_bridge(struct net_bridge *br);
extern void br_stp_set_enabled(struct net_bridge *br, unsigned long val);
extern void br_stp_enable_port(struct net_bridge_port *p);
extern void br_stp_disable_port(struct net_bridge_port *p);
extern bool br_stp_recalculate_bridge_id(struct net_bridge *br);
extern void br_stp_change_bridge_id(struct net_bridge *br, const unsigned char *a);
extern void br_stp_set_bridge_priority(struct net_bridge *br,
				       u16 newprio);
extern int br_stp_set_port_priority(struct net_bridge_port *p,
				    unsigned long newprio);
extern int br_stp_set_path_cost(struct net_bridge_port *p,
				unsigned long path_cost);
extern ssize_t br_show_bridge_id(char *buf, const struct bridge_id *id);

/* br_stp_bpdu.c */
#if defined(CONFIG_SONOS)
extern int br_stp_handle_bpdu(struct net_bridge_port *p,
			      struct sk_buff *skb);
extern int br_sonos_get_ticks(const unsigned char *src);
#else
struct stp_proto;
extern void br_stp_rcv(const struct stp_proto *proto, struct sk_buff *skb,
		       struct net_device *dev);
#endif

/* br_stp_timer.c */
extern void br_stp_timer_init(struct net_bridge *br);
extern void br_stp_port_timer_init(struct net_bridge_port *p);
extern unsigned long br_timer_value(const struct timer_list *timer);

/* br.c */
#if IS_ENABLED(CONFIG_ATM_LANE)
extern int (*br_fdb_test_addr_hook)(struct net_device *dev, unsigned char *addr);
#endif
#if defined(CONFIG_SONOS)
extern struct net_bridge_fdb_entry *(*br_fdb_get_hook)(struct net_bridge *br,
						       unsigned char *addr);
extern void (*br_fdb_put_hook)(struct net_bridge_fdb_entry *ent);
#endif

/* br_netlink.c */
extern struct rtnl_link_ops br_link_ops;
extern int br_netlink_init(void);
extern void br_netlink_fini(void);
extern void br_ifinfo_notify(int event, struct net_bridge_port *port);
#if defined(CONFIG_SONOS)
/* Needed by br_proxy.c */
int br_sonos_fill_ifinfo(struct sk_buff *skb, const struct net_bridge_port *port,
			 u32 pid, u32 seq, int event, unsigned int flags);

static inline int br_setlink(struct net_device *dev, struct nlmsghdr *nlmsg)
{
	return 0;
}
static inline int br_dellink(struct net_device *dev, struct nlmsghdr *nlmsg)
{
	return 0;
}
static inline int br_getlink(struct sk_buff *skb, u32 pid, u32 seq,
			     struct net_device *dev, u32 filter_mask)
{
	return 0;
}
#else
extern int br_setlink(struct net_device *dev, struct nlmsghdr *nlmsg);
extern int br_dellink(struct net_device *dev, struct nlmsghdr *nlmsg);
extern int br_getlink(struct sk_buff *skb, u32 pid, u32 seq,
		      struct net_device *dev, u32 filter_mask);
#endif

#ifdef CONFIG_SYSFS
/* br_sysfs_if.c */
extern const struct sysfs_ops brport_sysfs_ops;
extern int br_sysfs_addif(struct net_bridge_port *p);
extern int br_sysfs_renameif(struct net_bridge_port *p);

/* br_sysfs_br.c */
extern int br_sysfs_addbr(struct net_device *dev);
extern void br_sysfs_delbr(struct net_device *dev);

#else

static inline int br_sysfs_addif(struct net_bridge_port *p) { return 0; }
static inline int br_sysfs_renameif(struct net_bridge_port *p) { return 0; }
static inline int br_sysfs_addbr(struct net_device *dev) { return 0; }
static inline void br_sysfs_delbr(struct net_device *dev) { return; }
#endif /* CONFIG_SYSFS */

#endif
