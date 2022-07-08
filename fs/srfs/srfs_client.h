/*
 *  Copyright (c) 2020, Sonos, Inc.
 *
 **   All rights reserved.
 **/


/*
 * This file defines SRFS client file and directory operation functions.
 */

#ifndef _SRFS_CLIENT_H_
#define _SRFS_CLIENT_H_

#include <linux/fs.h>

#ifdef __cplusplus
extern "C" {
#endif


/* return the number of bytes read or written */
ssize_t srfsc_read(struct file *filp,  char * buf, uint8_t kern,  size_t count, loff_t *pos);

ssize_t srfsc_write(struct file *file, const char *buf, uint8_t kern, size_t count, loff_t *pos);

int srfsc_seek(struct file *file, loff_t offset, int whence);

int srfsc_open(struct file *filp);

int srfsc_close(struct file *filp);

int srfsc_rmdir(const unsigned char * dir, int namelen);

int srfsc_mkdir(const unsigned char * dir, int namelen);

int srfsc_rm(const unsigned char * fname, int namelen);

int srfsc_rename(const unsigned char * oldname, int oldname_len,
		 const unsigned char * newname, int newname_len);

int srfsc_opendir(struct inode *inode, struct file *file);

int srfsc_closedir(struct inode *inode, struct file *file);

int srfsc_readdir(struct file *file);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif

