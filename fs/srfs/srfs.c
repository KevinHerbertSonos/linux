/*
 * Copyright (c) 2020, Sonos, Inc.
 *
 * All rights reserved.
 */

#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/namei.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/mpage.h>
#include <linux/highmem.h>
#include <linux/exportfs.h>
#include <linux/buffer_head.h>
#include <linux/uio.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/magic.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include "srfs_limits.h"
#include "srfs_client.h"
#include "srfs_internal.h"
#include "srfs_util.h"



struct srfs_info_s  srfs_info;

int srfs_send_rpmsg(srfs_rpmsg_t *msg, u8 cmd)
{
        int err = 0;

        if (!srfs_info.rpdev) {
                srfs_err("srfs rpmsg channel not ready?\n");
                return -EINVAL;
        }
	pm_qos_add_request(&srfs_info.pm_qos_req, PM_QOS_CPU_DMA_LATENCY, 0);

        msg->header.cate = 15; //IMX_RPMSG_SRFS;
        msg->header.major = IMX_RMPSG_MAJOR;
        msg->header.minor = IMX_RMPSG_MINOR;
        msg->header.type = 0 ;
        msg->header.cmd = cmd;

        /* wait response from rpmsg */
        reinit_completion(&srfs_info.cmd_complete);

        err = rpmsg_send(srfs_info.rpdev->ept, (void *)msg,
                                     sizeof(srfs_rpmsg_t));
        if (err) {
                srfs_err("srfs rpmsg_send failed: %d\n", err);
                goto err_out;
        }

        err = wait_for_completion_timeout(&srfs_info.cmd_complete,
                                       msecs_to_jiffies(RPMSG_TIMEOUT));
        if (!err) {
                srfs_err("srfs rpmsg_send timeout!\n");
                err = -ETIMEDOUT;
                goto err_out;
        }

        err = 0;

err_out:
         pm_qos_remove_request(&srfs_info.pm_qos_req);

         return err;

}

static int rpmsg_srfs_cb(struct rpmsg_device *rpdev, void *data, int len,
                                                     void *priv, u32 src)
{
	srfs_rpmsg_t * msg = (srfs_rpmsg_t *)data;
	srfs_info.msg = msg;
	//memcpy( msg, data, len);

        complete(&srfs_info.cmd_complete);

        return 0;
}


static int rpmsg_srfs_probe(struct rpmsg_device *rpdev)
{

        srfs_info.rpdev = rpdev;
        mutex_init(&srfs_info.lock);
        init_completion(&srfs_info.cmd_complete);

        dev_info(&rpdev->dev, "new channel: 0x%x -> 0x%x!\n",
                                        rpdev->src, rpdev->dst);
	init_openf();

        srfs_info.init_done = true;
	srfs_info.serverOp = true;
        return 0;
}

static void rpmsg_srfs_remove(struct rpmsg_device *rpdev)
{
        dev_info(&rpdev->dev, "rpmsg srfs driver is removed\n");
}


static struct rpmsg_device_id rpmsg_srfs_id_table[] = {
        { .name = "rpmsg-srfs-channel" },
        { },
};

static struct rpmsg_driver rpmsg_srfs_driver = {
        .drv.name       = "rpmsg_srfs",
        .drv.owner      = THIS_MODULE,
        .id_table       = rpmsg_srfs_id_table,
        .probe          = rpmsg_srfs_probe,
        .callback       = rpmsg_srfs_cb,
        .remove         = rpmsg_srfs_remove,
};

int register_rpmsg(void)
{
        int rc;
        memset(&srfs_info, 0, sizeof(srfs_info));
        rc =  register_rpmsg_driver(&rpmsg_srfs_driver);
        if (rc==0) {
                srfs_info.init_done = 1;
                printk("SRFS register rpmsg driver ok\n");
        }
        else printk("SRFS register rpmsg driver err\n");

        return rc;
}


struct kernfs_node *srfs_root_kn;
static struct kernfs_root *srfs_root;
static struct vfsmount *srfs_mnt;

static uint32_t read_count = 0;
static uint32_t write_count = 0;
static uint32_t seek_count = 0;


static const struct inode_operations srfs_file_inode_operations;

static const struct super_operations srfs_ops;
static const struct inode_operations srfs_dir_inode_operations;

static const struct address_space_operations srfs_aops = {
	.readpage	= simple_readpage,
	.write_begin	= simple_write_begin,
	.write_end	= simple_write_end,
	.set_page_dirty	= __set_page_dirty_no_writeback,
};

static ssize_t sync_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos);

static loff_t srfs_file_llseek(struct file *file, loff_t offset, int whence);

ssize_t kern_write(struct file *filp, const char *buf, size_t len, loff_t *ppos)
{
	mm_segment_t old_fs;
        ssize_t res;

        old_fs = get_fs();
        set_fs(get_ds());

	file_start_write(filp);
         /* The cast to a user pointer is valid due to the set_fs() */
        res = sync_write(filp, (__force const char __user *)buf, len, ppos);
	file_end_write(filp);
        set_fs(old_fs);

        return res;
}

static ssize_t sync_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
        struct iovec iov ;
        struct kiocb kiocb;
        struct iov_iter iter;
        ssize_t ret;

	iov.iov_base = (void __user *)buf;
	iov.iov_len = len;

        init_sync_kiocb(&kiocb, filp);
        kiocb.ki_pos = *ppos;
        iov_iter_init(&iter, WRITE, &iov, 1, len);

        ret = filp->f_op->write_iter(&kiocb, &iter);
        BUG_ON(ret == -EIOCBQUEUED);
        if (ret > 0)
                *ppos = kiocb.ki_pos;
        return ret;
}

ssize_t sync_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
        struct iovec iov = { .iov_base = buf, .iov_len = len };
        struct kiocb kiocb;
        struct iov_iter iter;
        ssize_t ret;

	srfs_dbg("sync_read\n");

        init_sync_kiocb(&kiocb, filp);
        kiocb.ki_pos = *ppos;
        iov_iter_init(&iter, READ, &iov, 1, len);

        ret = filp->f_op->read_iter(&kiocb, &iter);
        BUG_ON(ret == -EIOCBQUEUED);
        *ppos = kiocb.ki_pos;
        return ret;
}

ssize_t
srfs_file_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	ssize_t len = 0;

	read_count ++ ;
	srfs_dbg("srfs_file_read_iter-count=%d\n", read_count);
	srfs_info.serverOp = false;
	len = generic_file_read_iter(iocb, iter);
	srfs_info.serverOp = true;
	return len;
}


ssize_t
srfs_file_write(struct file *file, const char __user * buf, size_t count, loff_t *ppos)
{
	ssize_t ret=0;

	srfs_dbg("srfs_file_write: count=%u, ppos=%lld\n", count, *ppos);

	if (srfs_info.serverOp) {

		ret = srfsc_write(file, buf, 0, count, ppos);
		srfs_dbg("srfsc_write returned %d\n", ret);
	}
	else {
		file_start_write(file);
		ret = sync_write(file, buf, count, ppos);
		file_end_write(file);
		srfs_dbg("srfs_file_write: write to local\n");
	}
	return ret;
}

ssize_t
srfs_file_read(struct file *filp,  char __user * buf, size_t count, loff_t *ppos )
{
        ssize_t readbytes = 0;
	loff_t pos = *ppos;
	struct srfs_fileP_t *cInfo ;

	cInfo =  filp->private_data;
	srfs_dbg("srfs_file_read user buf=%p,count=%d loff=%d\n", buf, count, (int)pos);

	if (cInfo && srfs_info.serverOp) {
                readbytes = srfsc_read(filp, buf, 0, count, ppos);
	}
	else readbytes = sync_read(filp, buf, readbytes, ppos);
	return readbytes;
}
ssize_t srfs_file_splice_read(struct file *in, loff_t *ppos,
	                      struct pipe_inode_info *pipe, size_t len,
	                      unsigned int flags)
{
	struct srfs_fileP_t *cInfo ;
	char *buf;
	ssize_t readbytes = 0;
	ssize_t size =0;
	struct inode *inode = file_inode(in);
	struct path path = in->f_path;
	loff_t write_pos = * ppos;

	srfs_dbg("srfs_file_splice_read: len=%d pos=%d\n", len, (int32_t)write_pos);
	/* validdate file size */
	size = i_size_read((const struct inode *)inode);
	if ( size==0 || size== SRFS_FILESIZE_MAX )
	{
		/* find out the file size from server */
		size = srfs_file_llseek(in, 0, SEEK_END);
		if (size<=0) return 0;
		/* write i_size */
		i_size_write(inode, size);
	}
	srfs_dbg("srfs_file_splice_read: file size=%d\n", size);

	if (len > size) len = size;
	if (*ppos>=size) {
		return 0;
	}
	cInfo =  in->private_data;

	buf = kzalloc(size, GFP_KERNEL);
        if ( !buf )  {
                goto k_error;
        }

	if (cInfo && srfs_info.serverOp) {

		readbytes = srfsc_read(in, buf, 1, size, ppos );
		if (readbytes< 0) goto s_error;

                set_openf_writeback(cInfo->cid, true);
		srfs_info.serverOp = false;

		vfs_truncate(&path, write_pos);

		kern_write(in, (const char *)buf, readbytes, &write_pos);

                set_openf_writeback(cInfo->cid, false);

	}
	readbytes = generic_file_splice_read( in, ppos, pipe, readbytes, flags);
	srfs_info.serverOp = true;
	srfs_dbg("srfs_file_splice_read %d bytes\n", readbytes);
s_error:
	kfree(buf);

k_error:

	return readbytes;
}

ssize_t
srfs_file_write_iter( struct kiocb *iocb, struct iov_iter *from )
{
	int rc;

	write_count++;
	srfs_dbg("srfs_file_write_iter count=%d\n", write_count);
	rc = generic_write_checks(iocb, from);
	if (rc <= 0)
                return rc;

	return generic_file_write_iter(iocb, from);
}

int
srfs_file_open(struct inode *inode, struct file *filp)
{
        int res =0;

        srfs_dbg("srfs_file_open filename:(%pD2)\n", filp);
	if ( srfs_info.serverOp )
		res = srfsc_open( filp );
	return res;
}

int
srfs_file_release(struct inode *inode, struct file *filp)
{
	int rc = 0;
        srfs_dbg("srfs: release file (%pD2)\n", filp);
	// send file close command to srfs server
	if (srfs_info.serverOp)
		rc = srfsc_close( filp );
	return rc;
}

/*
static int
srfs_file_flush(struct file *file, fl_owner_t id)
{
	struct inode *inode = file_inode(file);
	int rc =0;
	if (file->f_mode & FMODE_WRITE)
                rc = filemap_write_and_wait(inode->i_mapping);
        srfs_dbg("srfs_file_flush(%pD2)\n", file);
	srfs_dbg("f_mode: %x\n", file->f_mode);

	return rc;
}
*/

loff_t
srfs_file_llseek(struct file *file, loff_t offset, int whence)
{
	seek_count++;

	if (srfs_info.serverOp)
		return srfsc_seek( file, offset, whence );
	else
		return generic_file_llseek(file, offset, whence);
}

int srfs_dir_open(struct inode *inode, struct file *file)
{
	int rc=0;

        /* call client operation funcion to talk to server */
	if (srfs_info.serverOp) {
		rc = srfsc_opendir( inode, file );
		srfs_dbg("srfs_dir_open return  %d\n", rc);
	}
	return dcache_dir_open(inode, file);
}

int srfs_dir_read(struct file *file,  struct dir_context *ctx)
{
        int rc;

        srfs_dbg("enter srfs_dir_read\n");
        /* call client operation funcion to talk to server */
	if (srfs_info.serverOp) {
		srfs_info.serverOp = false;
		rc = srfsc_readdir( file );
		srfs_dbg("srfsc_readdir return %d\n", rc);

        }
        rc = dcache_readdir(file, ctx);
	srfs_info.serverOp = true ;
	return rc;
}

int srfs_dir_close(struct inode *inode, struct file *file)
{
	int rc;

	srfs_dbg("srfs_dir_close\n");
	if (srfs_info.serverOp) {
		rc = srfsc_closedir( inode, file );
	}
	return dcache_dir_close(inode, file);
}

static const struct file_operations srfs_file_operations = {
        .open           = srfs_file_open,
        .read           = srfs_file_read,
        //.flush          = srfs_file_flush,
        .read_iter      = srfs_file_read_iter,
        .write          = srfs_file_write,
        .write_iter     = srfs_file_write_iter,
	.mmap           = generic_file_mmap,
        .fsync          = noop_fsync,
        .llseek         = srfs_file_llseek,
        .release        = srfs_file_release,
        .splice_read    = srfs_file_splice_read, //generic_file_splice_read,
        .splice_write   = iter_file_splice_write,
};

static const struct file_operations srfs_dir_operations = {
        .iterate_shared = srfs_dir_read,
	.open           = srfs_dir_open,
        .release        = srfs_dir_close,
        .read           = generic_read_dir,
        .llseek		= dcache_dir_lseek,
	.fsync		= noop_fsync,
};


struct inode *srfs_get_inode(struct super_block *sb,
				const struct inode *dir, umode_t mode, dev_t dev)
{
	struct inode * inode = new_inode(sb);

	if (inode) {
		inode->i_fop = &srfs_file_operations;
		inode->i_ino = get_next_ino();
		inode_init_owner(inode, dir, mode);
		inode->i_mapping->a_ops = &srfs_aops;
		mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
		mapping_set_unevictable(inode->i_mapping);
		inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
		switch (mode & S_IFMT) {

		case S_IFREG:
			inode->i_op = &srfs_file_inode_operations;
			inode->i_fop = &srfs_file_operations;
			break;
		case S_IFDIR:
			inode->i_op = &srfs_dir_inode_operations;
			inode->i_fop = &srfs_dir_operations;

			inc_nlink(inode);
			break;
		default:
			break;
		}
	}
	return inode;
}


static int
srfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
	struct inode * inode = srfs_get_inode(dir->i_sb, dir, mode, dev);
	int error = -ENOSPC;

	srfs_dbg("srfs_mknod %pd2\n", dentry);

	if (inode) {
		d_add(dentry, inode);
		dget(dentry);
		error = 0;
		dir->i_mtime = dir->i_ctime = current_time(dir);
	}
	return error;
}

int srfs_mkdir(struct inode * dir, struct dentry * dentry, umode_t mode)
{
	int retval ;

	if (srfs_info.serverOp) {
		retval = srfsc_mkdir(dentry->d_name.name, dentry->d_name.len);
		if ( retval < 0 )
			srfs_err("srfsc_mkdir return error code %d\n", retval);

	}
	retval = srfs_mknod(dir, dentry, mode | S_IFDIR, 0);

	return retval;
}

int srfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
	return srfs_mknod(dir, dentry, mode | S_IFREG, 0);
};

static int srfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int res;

	srfs_dbg("srfs_rmdir (%s/%lu), %pd\n",
                        dir->i_sb->s_id, dir->i_ino, dentry);
	if (srfs_info.serverOp) {
		res = srfsc_rmdir( dentry->d_name.name, dentry->d_name.len);

		if ( res < 0 )  {
			srfs_err("srfsc_rmdir error code=%d \n", res);
			// if server rmdir failed, we don't remove local dir
			return res;
		}
	}
	return simple_rmdir(dir, dentry);
}

int srfs_rename(struct inode *old_dir, struct dentry *old_dentry,
               struct inode *new_dir, struct dentry *new_dentry,
               unsigned int flags)
{
	int res;
	int old_len, new_len;
	const uint8_t * old;
	const uint8_t * new;

	old = old_dentry->d_name.name;
	old_len = old_dentry->d_name.len;
	new  = new_dentry->d_name.name;
        new_len = new_dentry->d_name.len;

	res = srfsc_rename( old, old_len, new, new_len );
	if ( res < 0 ) {
		srfs_err("srfsc_rename error code=%d \n", res);
                return res;
	}

	return simple_rename(old_dir, old_dentry, new_dir, new_dentry, flags);
}

int srfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int res;
	srfs_dbg("srfs_unlink %pd2 \n", dentry);

	if (srfs_info.serverOp) {
		res = srfsc_rm(dentry->d_name.name, dentry->d_name.len);
		if ( res < 0 ) {
			srfs_dbg("srfsc_rm error code=%d \n", res);
			return res;
		}
        }

	return simple_unlink(dir, dentry);
}

static const struct inode_operations srfs_file_inode_operations = {
        .setattr        = simple_setattr,
        .getattr        = simple_getattr,
};
static const struct inode_operations srfs_dir_inode_operations = {
	.create		= srfs_create,
	.lookup		= simple_lookup,
	.link		= simple_link,
	.unlink		= srfs_unlink,
	.mkdir		= srfs_mkdir,
	.rmdir		= srfs_rmdir,
	.mknod		= srfs_mknod,
	.rename		= srfs_rename,
};

static const struct super_operations srfs_ops = {
	.statfs		= simple_statfs,
	.drop_inode	= generic_delete_inode,
	.show_options	= generic_show_options,
};

struct srfs_mount_opts {
	umode_t mode;
};

enum {
	Opt_mode,
	Opt_err
};

static const match_table_t tokens = {
	{Opt_mode, "mode=%o"},
	{Opt_err, NULL}
};

struct srfs_fs_info {
	struct srfs_mount_opts mount_opts;
};

static int srfs_parse_options(char *data, struct srfs_mount_opts *opts)
{
	substring_t args[MAX_OPT_ARGS];
	int option;
	int token;
	char *p;

	opts->mode = SRFS_DEFAULT_MODE;

	while ((p = strsep(&data, ",")) != NULL) {
		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch (token) {
		case Opt_mode:
			if (match_octal(&args[0], &option))
				return -EINVAL;
			opts->mode = option & S_IALLUGO;
			break;
		}
	}

	return 0;
}

int srfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct srfs_fs_info *fsi;
	struct inode *inode;
	int err;

	save_mount_options(sb, data);

	fsi = kzalloc(sizeof(struct srfs_fs_info), GFP_KERNEL);
	sb->s_fs_info = fsi;
	if (!fsi)
		return -ENOMEM;

	err = srfs_parse_options(data, &fsi->mount_opts);
	if (err)
		return err;

	sb->s_maxbytes		= SRFS_FILESIZE_MAX;
	sb->s_blocksize		= PAGE_SIZE;
	sb->s_blocksize_bits	= PAGE_SHIFT;
	sb->s_magic		= 0x87654321;
	sb->s_op		= &srfs_ops;
	sb->s_time_gran		= 1;

	inode = srfs_get_inode(sb, NULL, S_IFDIR | fsi->mount_opts.mode, 0);
	sb->s_root = d_make_root(inode);

	if (!sb->s_root)
		return -ENOMEM;
	return 0;
}

struct dentry *srfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_nodev(fs_type, flags, data, srfs_fill_super);
}

static void srfs_kill_sb(struct super_block *sb)
{
	kfree(sb->s_fs_info);
	kill_litter_super(sb);
}

static struct file_system_type srfs_fs_type = {
	//.owner          = THIS_MODULE,
	.name		= "srfs",
	.mount		= srfs_mount,
	.kill_sb	= srfs_kill_sb,
	.fs_flags	= FS_USERNS_MOUNT,
};

int __init srfs_init(void)
{
	int err;

	srfs_root = kernfs_create_root(NULL, KERNFS_ROOT_EXTRA_OPEN_PERM_CHECK, NULL);
	if (IS_ERR(srfs_root))
		return PTR_ERR(srfs_root);

	srfs_root_kn = srfs_root->kn;
        err = register_filesystem(&srfs_fs_type);
        if (err) {
		pr_err("Could not register srfs\n");
                kernfs_destroy_root(srfs_root);
                return err;
        }

	srfs_mnt = kern_mount(&srfs_fs_type);
        if (IS_ERR(srfs_mnt)) {
                err = PTR_ERR(srfs_mnt);
                pr_err("Could not kern_mount srfs\n");
                unregister_filesystem(&srfs_fs_type);
		kernfs_destroy_root(srfs_root);
                return err;
        }
	err = srfst_init();
	if (err) {
		unregister_filesystem(&srfs_fs_type);
                kernfs_destroy_root(srfs_root);
                return err;
	}
        err = register_rpmsg();

        if(err) {
                srfs_err("Could not register srfs rpmsg driver\n");
	        unregister_filesystem(&srfs_fs_type);
                kernfs_destroy_root(srfs_root);
	}
	else
		srfs_dbg("srfs_init ok\n");

	return err;
}
module_init(srfs_init);
