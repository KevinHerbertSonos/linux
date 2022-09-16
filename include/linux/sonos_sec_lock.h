/*
 * Copyright (c) 2018, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef SONOS_SEC_LOCK_H
#define SONOS_SEC_LOCK_H

#include <linux/types.h>

extern bool sonos_allow_insmod(void);
extern bool sonos_allow_mount_dev(void);
extern bool sonos_allow_mount_exec(void);

#endif
