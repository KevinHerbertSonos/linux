/*
 * Copyright (c) 2017, Sonos, Inc.
 *
 * SPDX-License-Identifier:     GPL-2.0
 *
 * The following file is used for prototypes and data structures for
 * sonos APIs embedded in the Linux kernel.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef SONOS_IR_H
#define SONOS_IR_H

#define MTK_IR_SCALING          111598
#define MTK_IR_END              0x5b

/* Function to call to pass along IR data */
struct sonos_ir {
        void (*read_ir)(uint32_t *ir_data, int num_regs);
};

#endif /* SONOS_IR_H */
