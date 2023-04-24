/* br_priority.h - Sonos Priority Extensions
 * Copyright (c) 2016-2023, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _BR_PRIORITY_H
#define _BR_PRIORITY_H

#define BR_BPDU_PRIORITY 2

/* br_priority.c */
extern int br_priority_for_addr(const unsigned char *addr);

#endif
