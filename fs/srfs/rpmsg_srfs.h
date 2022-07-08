/*
 *   Copyright (c) 2020, Sonos, Inc.
 *
 *   All rights reserved.
 */

#ifndef SRFS_RPMSG_H
#define SRFS_RPMSG_H

#include <linux/rpmsg.h>
#include <linux/imx_rpmsg.h>

#define SRFS_MSG_MAX_LEN        470
#define RPMSG_TIMEOUT		1000

enum srfs_rpmsg_cmd {
	SRFS_FILE_OPEN,
	SRFS_FILE_READ,
	SRFS_FILE_WRITE,
	SRFS_FILE_CLOSE,
	SRFS_FILE_SEEK,
	SRFS_OPEN_DIR,
	SRFS_READ_DIR,
	SRFS_CLOSE_DIR,
	SRFS_MK_DIR,
	SRFS_RM_DIR,
	SRFS_RM_FILE,
	SRFS_RENAME,
	SRFS_CMD_MAX,
};

enum srfs_rpmsg_status {
	SRFS_RPMSG_OK = 0,
	SRFS_RPMSG_ERR,
	SRFS_LFS_ERR,
	SRFS_RPMSG_NO_PNAME,
	SRFS_INVALID_FD,
	SRFS_ZERO_DLEN,
	SRFS_UNKNOWN_CMD,
};

typedef struct {
	struct imx_rpmsg_head header;
	uint8_t status;
	uint8_t fd;     /* identifies an open file or dir on linux side */
			/* server must return it unchanged */
	uint8_t lfs_id; /* srfs server side file identifier */
	uint16_t flags; /* same meaning as in linux sys_open() */
	uint32_t bytes; /* number of bytes requested in read op */
			/* or number of bytes written to server for write op */
	int32_t pos;   /* this is the byte offset for read and write */
	uint16_t len;   /* length of *data */
	char data[SRFS_MSG_MAX_LEN+1];

}__attribute__ ((packed)) srfs_rpmsg_t;

#endif
