/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __BL40_MODULE_H__
#define __BL40_MODULE_H__

void *bl40_rx_msg(void *msg, uint32_t size);
void *bl40_rx_data_callback(void *data, uint32_t size);

#endif /*__BL40_MODULE_H__*/
