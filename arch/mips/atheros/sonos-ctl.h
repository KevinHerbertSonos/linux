/*
 * Copyright (c) 2015-2021 Sonos, Inc.
 *
 * SPDX-License-Identifier:     GPL-2.0
 */

#ifndef SONOS_CTL_H
#define SONOS_CTL_H

struct syslib_irdata {
    ssize_t bytes_read; /* Bytes read by the driver. */
    size_t buf_length;  /* Length of userspace buffer. */
    char buf[0];        /* Beginning of data. */
};
#define SYSLIB_IRDATA_SIZE(n) (sizeof(struct syslib_irdata) + n - 1)

#endif // SONOS_CTL_H
