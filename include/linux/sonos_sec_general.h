/*
 * Copyright (c) 2014-2018, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef SONOS_SEC_GENERAL_H
#define SONOS_SEC_GENERAL_H

#include <linux/types.h>

extern bool sonos_get_cpuid(uint8_t *buf, size_t bufLen);
extern bool sonos_get_unlock_counter(uint32_t *pValue);

#endif
