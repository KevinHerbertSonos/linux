/*
 * Copyright (c) 2018, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * linux/include/sonos_corrupt.h
 *
 * Functions for detecting memory corruption.
 */

#ifdef CONFIG_SONOS_DEBUG_CORRUPT
extern void sonos_corrupt_check(void);
#else
#define sonos_corrupt_check()
#endif
