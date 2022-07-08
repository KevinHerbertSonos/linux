 /*
 * Copyright (C) 2020, Sonos, Inc.
 *
 */

#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/mount.h>
#include <linux/device.h>
#include <linux/genhd.h>
#include <linux/namei.h>
#include <linux/fs.h>
#include <linux/shmem_fs.h>
#include <linux/ramfs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dcache.h>
#include <linux/kthread.h>
#include "srfs_internal.h"

static struct task_struct *thread;

static DEFINE_SPINLOCK(req_lock);

static struct req {
	struct req *next;
	struct completion done;
	int err;
	const char *name;
	struct inode *inode;
	struct dentry *dentry;
	u8 type;
	u8 oper;
	umode_t mode;
} *requests;



int srfst_create_node(char *pname, struct inode *dir, struct dentry * dentry, u8 type )
{
	struct req *req;

	if (!thread)
		return 0;

	if (!pname)
		return SRFS_NULL_PTR;


	req = (struct req *)kzalloc(sizeof(struct req), GFP_KERNEL);
	if (req==NULL) {
		printk(KERN_ERR "%s kzalloc error\n", __func__);
		return SRFS_NULL_PTR;
	}
	req->dentry = dentry;
	req->inode = dir;
	req->type = type;
	req->oper = SRFS_OP_ADD;
	req->name = pname;

	if (type==SRFS_NODE_DIR)
		req->mode = SRFS_DEFAULT_MODE_DIR|S_IFDIR;
	else
		req->mode = SRFS_DEFAULT_MODE|S_IFREG;

	init_completion(&req->done);

	spin_lock(&req_lock);
	req->next = requests;
	requests = req;
	spin_unlock(&req_lock);

	wake_up_process(thread);
	wait_for_completion(&req->done);

	return req->err;
}

int srfst_delete_node(char *pname, struct inode *dir, struct dentry *dentry, u8 type)
{
	struct req *req;

	if (!thread)
		return 0;
	if (!pname)
		return SRFS_NULL_PTR;

	req = (struct req *)kzalloc(sizeof(struct req), GFP_KERNEL);
	if (req==NULL) {
                printk(KERN_ERR "%s kzalloc error\n", __func__);
                return SRFS_NULL_PTR;
        }

	req->mode = 0;
	req->inode = dir;
	req->dentry = dentry;
	req->type = type;
        req->oper = SRFS_OP_DELETE;
	req->name = pname;

	init_completion(&req->done);

	spin_lock(&req_lock);
	req->next = requests;
	requests = req;
	spin_unlock(&req_lock);

	wake_up_process(thread);
	wait_for_completion(&req->done);

	return req->err;
}


int srfst_create(const char *fname, struct inode *dir,
		struct dentry *dentry, umode_t mode, u8 type)
{
	struct dentry *dchild;
	struct qstr dname;
	int err = 0;


	dname.name = fname;
        dname.len = strlen(fname);

        dchild = d_hash_and_lookup(dentry, &dname);
	printk(KERN_DEBUG "%s d_lookup on parent=%p  got %p\n",
	                  __func__, dentry, dchild);

        if (dchild) {
                /* dentry with this name exist, nothing to do */
                dput(dchild);
                return  0;
        }
        dchild = d_alloc(dentry, &dname);
        printk(KERN_DEBUG "%s d_alloc %p dname='%d %s' = %p\n", __func__, dentry,
                           dname.len, dname.name, dchild);
        if (dchild == NULL) {
                dput(dentry);
		printk(KERN_ERR "%s unable to create dentry for %s\n",
				__func__, dname.name);
                return -ENOMEM;
        }
	if (dchild->d_parent != dentry) {
		printk(KERN_ERR "%s dchild parent mismatch\n", __func__);
		printk(KERN_ERR "%s dchild->d_name=%s\n", __func__, dchild->d_name.name);
		return -ECHILD;
	}

	if (type == SRFS_NODE_DIR)
		err = vfs_mkdir( dir, dchild, mode );
	else
		err = srfs_create( dir, dchild, mode, 0 );

	return err;
}


int srfst_remove(const char *fname, struct inode *dir, struct dentry *dentry, u8 type)
{
	struct dentry *dchild;
	int err = 0;
	struct qstr dname;

        dname.name = fname;
        dname.len = strlen(fname);

	dchild = d_hash_and_lookup(dentry, &dname);

	if (dchild) {
		dput(dchild);
		if (type== SRFS_NODE_DIR)
			err = simple_rmdir( dir, dchild );
		else
			err = simple_unlink(dir, dchild );
	}
	else {
		printk(KERN_DEBUG "%s dentry with name %s not found\n",
				__func__, fname);
		err = 0;
	}
	return err;
}

static DECLARE_COMPLETION(setup_done);

int handle(const char *name, struct inode * dir, struct dentry * dentry,
		umode_t mode, u8 type, u8 oper)
{

	if (oper==SRFS_OP_ADD)
		return srfst_create(name, dir, dentry, mode, type);
	else if (oper == SRFS_OP_DELETE )
		return srfst_remove(name, dir, dentry, type);
	else /* should never happen */
		return 0;
}

int srfstd(void *unused)
{

	sys_chdir("/ramdisk/srfs/");
	sys_chroot("/ramdisk/srfs/");
	complete(&setup_done);

	while (1) {
		spin_lock(&req_lock);
		while (requests) {
			struct req *req = requests;
			requests = NULL;
			spin_unlock(&req_lock);
			while (req) {
				struct req *next = req->next;
				req->err = handle(req->name,
						req->inode,
						req->dentry,
						req->mode,
						req->type,
						req->oper);
				complete(&req->done);
				// free current req
				kfree( req );
				// move req ptr to next request
				req = next;
			}
			spin_lock(&req_lock);
		}
		__set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock(&req_lock);
		schedule();
	}
	return 0;
}

/*
 * Create srfst kthread
 */
int srfst_init(void)
{
	int err=0;

	thread = kthread_run(srfstd, NULL, "srfs_thread");
	if (!IS_ERR(thread)) {
		wait_for_completion(&setup_done);
	} else {
		err = PTR_ERR(thread);
		thread = NULL;
	}

	if (err) {
		printk(KERN_ERR "srfst: unable to create srfst %i\n", err);
		return err;
	}

	printk(KERN_INFO "srfst: initialized\n");
	return 0;
}
