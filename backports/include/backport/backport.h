#ifndef __BACKPORT_H
#define __BACKPORT_H
#include <generated/autoconf.h>
#include <linux/kconfig.h>

#define LINUX_BACKPORT(__sym) backport_ ##__sym

#endif /* __BACKPORT_H */
