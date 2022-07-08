#ifndef __BACKPORT_LINUX_ERR_H
#define __BACKPORT_LINUX_ERR_H
#include_next <linux/err.h>

static inline int __must_check PTR_ERR_OR_ZERO(__force const void *ptr)
{
	if (IS_ERR(ptr))
		return PTR_ERR(ptr);
	else
		return 0;
}

#endif /* __BACKPORT_LINUX_ERR_H */
