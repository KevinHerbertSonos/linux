/*
 * Driver for Mediatek Hardware Random Number Generator
 *
 * Copyright (C) 2018 Greta Zhang <greta.zhang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __TRUSTZONE_TA_TRNG__
#define __TRUSTZONE_TA_TRNG__

#define TZ_TA_TRNG_UUID   "9c4df224-fe06-37f1-ceeb-bd1324cb9c6f"

/* Data Structure for TRNG TA */
/* You should define data structure used both in REE/TEE here
 * N/A for Test TA
 */

/* Command for TRNG TA */
#define TZCMD_TRNG_RD    0

#endif				/* __TRUSTZONE_TA_TRNG__ */
