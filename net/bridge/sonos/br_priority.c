/* br_priority.c - Sonos Priority Extensions
 * Copyright (c) 2016-2022, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include "br_priority.h"

int br_priority_for_addr(const unsigned char *addr)
{
	/* REVIEW: Currently hardcoded to Sonos OUIs.  We will extend this at
	 *         some point in the future (allowlist, different priorities,
	 *         etc).
	 */
	if ((0x00 == addr[0] && 0x0e == addr[1] && 0x58 == addr[2]) ||
	    (0x78 == addr[0] && 0x28 == addr[1] && 0xca == addr[2]) ||
	    (0x94 == addr[0] && 0x9f == addr[1] && 0x3e == addr[2]) ||
	    (0xb8 == addr[0] && 0xe9 == addr[1] && 0x37 == addr[2]) ||
	    (0x5c == addr[0] && 0xaa == addr[1] && 0xfd == addr[2]) ||
	    (0x34 == addr[0] && 0x7e == addr[1] && 0x5c == addr[2]) ||
	    (0x48 == addr[0] && 0xa6 == addr[1] && 0xb8 == addr[2]) ||
	    (0x54 == addr[0] && 0x2a == addr[1] && 0x1b == addr[2]) ||
	    (0xf0 == addr[0] && 0xf6 == addr[1] && 0xc1 == addr[2]) ||
	    (0x38 == addr[0] && 0x42 == addr[1] && 0x0b == addr[2]) ||
	    (0xc4 == addr[0] && 0x38 == addr[1] && 0x75 == addr[2]) ||
	    (0x80 == addr[0] && 0x4a == addr[1] && 0xf2 == addr[2])) {
		return 1;
	}

	return 0;
}
