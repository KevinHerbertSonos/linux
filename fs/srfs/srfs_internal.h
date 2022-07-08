/*
 *   Copyright (c) 2020, Sonos, Inc.
 *
 *   All rights reserved.
 */

#ifndef SRFS_INTERNAL_H
#define SRFS_INTERNAL_H

#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "rpmsg_srfs.h"


//#define srfs_dbg_enable

#ifdef srfs_dbg_enable
#define srfs_dbg  printk
#define srfs_err  printk

#else
#define srfs_dbg  pr_debug
#define srfs_err  pr_err
#endif

#define SRFS_DEFAULT_MODE_DIR   0644
#define SRFS_DEFAULT_MODE       0766

#define SRFS_NODE_FILE    0
#define SRFS_NODE_DIR     1
#define SRFS_OP_ADD       0
#define SRFS_OP_DELETE    1

/* SRFS error code */
#define SRFS_BUF_ERR      -10
#define SRFS_NULL_PTR     -11
#define SRFS_INVLIAD_SID  -12
#define SRFS_INVALID_CID  -13

struct srfs_info_s {
        struct rpmsg_device             *rpdev;
        srfs_rpmsg_t                    *msg;
        struct completion               cmd_complete;
        struct pm_qos_request           pm_qos_req;
        struct work_struct              work_q;
        struct mutex                    lock;
        bool   init_done;
        bool   serverOp;

};

extern  struct srfs_info_s  srfs_info;

extern int register_rpmsg(void);
//extern void unregister_rpmsg(void);
extern int srfs_send_rpmsg(srfs_rpmsg_t *msg, u8 cmd);

extern int srfs_create(struct inode * dir, struct dentry * dentry, umode_t mode,bool  dev);

extern int srfst_init(void);
extern int srfst_create_node( char *pname, struct inode *dir, struct dentry *dentry, u8 type);
extern int srfst_delete_node( char *pname, struct inode *dir, struct dentry *dentry, u8 type);

#endif
