#ifndef __BACKPORT_SKBUFF_H
#define __BACKPORT_SKBUFF_H
#include_next <linux/skbuff.h>

#include <generated/utsrelease.h>

static inline struct sk_buff *__pskb_copy_fclone(struct sk_buff *skb,
						 int headroom, gfp_t gfp_mask,
						 bool fclone)
{
	return __pskb_copy(skb, headroom, gfp_mask);
}

static inline int skb_copy_datagram_msg(const struct sk_buff *from, int offset,
					struct msghdr *msg, int size)
{
	return skb_copy_datagram_iovec(from, offset, msg->msg_iov, size);
}

static inline int memcpy_from_msg(void *data, struct msghdr *msg, int len)
{
	return memcpy_fromiovec(data, msg->msg_iov, len);
}

#endif /* __BACKPORT_SKBUFF_H */
