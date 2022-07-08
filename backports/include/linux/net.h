#ifndef __BACKPORT_LINUX_NET_H
#define __BACKPORT_LINUX_NET_H
#include_next <linux/net.h>
#include <linux/static_key.h>

#ifndef ___NET_RANDOM_STATIC_KEY_INIT
#define __BACKPORT_NET_GET_RANDOM_ONCE 1
#endif /* ___NET_RANDOM_STATIC_KEY_INIT */

#ifdef __BACKPORT_NET_GET_RANDOM_ONCE
#define __net_get_random_once LINUX_BACKPORT(__net_get_random_once)
bool __net_get_random_once(void *buf, int nbytes, bool *done,
			   struct static_key *done_key);

#ifdef HAVE_JUMP_LABEL
#define ___NET_RANDOM_STATIC_KEY_INIT ((struct static_key) \
		{ .enabled = ATOMIC_INIT(0), .entries = (void *)1 })
#else /* !HAVE_JUMP_LABEL */
#define ___NET_RANDOM_STATIC_KEY_INIT STATIC_KEY_INIT_FALSE
#endif /* HAVE_JUMP_LABEL */

#define net_get_random_once(buf, nbytes)				\
	({								\
		bool ___ret = false;					\
		static bool ___done = false;				\
		static struct static_key ___done_key =			\
			___NET_RANDOM_STATIC_KEY_INIT;			\
		if (!static_key_true(&___done_key))			\
			___ret = __net_get_random_once(buf,		\
						       nbytes,		\
						       &___done,	\
						       &___done_key);	\
		___ret;							\
	})

#endif /* __BACKPORT_NET_GET_RANDOM_ONCE */


#define sock_create_kern(net, family, type, proto, res) \
	__sock_create(net, family, type, proto, res, 1)

#ifndef SOCKWQ_ASYNC_NOSPACE
#define SOCKWQ_ASYNC_NOSPACE   SOCK_ASYNC_NOSPACE
#endif
#ifndef SOCKWQ_ASYNC_WAITDATA
#define SOCKWQ_ASYNC_WAITDATA   SOCK_ASYNC_WAITDATA
#endif

#endif /* __BACKPORT_LINUX_NET_H */
