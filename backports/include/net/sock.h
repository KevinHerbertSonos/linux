#ifndef __BACKPORT_NET_SOCK_H
#define __BACKPORT_NET_SOCK_H
#include_next <net/sock.h>

/* no reclassification while locks are held */
static inline bool sock_allow_reclassification(const struct sock *csk)
{
	struct sock *sk = (struct sock *)csk;

	return !sk->sk_lock.owned && !spin_is_locked(&sk->sk_lock.slock);
}

#ifndef sock_skb_cb_check_size
#define sock_skb_cb_check_size(size) \
	BUILD_BUG_ON((size) > FIELD_SIZEOF(struct sk_buff, cb))
#endif

#define sk_alloc(net, family, priority, prot, kern) sk_alloc(net, family, priority, prot)

static inline void sk_set_bit(int nr, struct sock *sk)
{
	set_bit(nr, &sk->sk_socket->flags);
}

static inline void sk_clear_bit(int nr, struct sock *sk)
{
	clear_bit(nr, &sk->sk_socket->flags);
}


#endif /* __BACKPORT_NET_SOCK_H */
