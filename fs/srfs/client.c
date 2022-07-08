/*
 * Copyright (c) 2020, Sonos, Inc.
 *
 * All rights reserved.a
 *
 * SOnos Remote File System Client
 *
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/syscalls.h>
#include "srfs_limits.h"
#include "srfs_client.h"
#include "srfs_internal.h"
#include "srfs_util.h"

void processFlist(struct inode *dir, struct dentry *dentry,
		char * flist, size_t len)
{
	char name[32]= {0};
	char * pname;
	int name_len;
	uint8_t type;
	char * p = flist;
	char * ppos ;
	int err = 0;

        /* flist contains a list seperated by '\n' */
	/* even the last file/dir in the list, must have '\n' appended to it */
        /* the first byte indicate this is a regular file or a directory */
	/* file or directory name starts from second byte                */

	ppos = strchr(p, '\n');

	while (ppos) {
		if ( *p=='1' )  {
			type = SRFS_NODE_DIR;
		}
		else {
			type = SRFS_NODE_FILE;
		}
		name_len = ppos-p-1 ;
		memcpy( name, p+1, name_len);
		name[ppos-p] = '\0';
		pname = (char *)kzalloc(name_len, GFP_KERNEL);
		memcpy(pname, name, name_len);
		pname[name_len]='\0';
		//create a vfs node
		//ignore any error-file could already exist
		err = srfst_create_node(pname, dir, dentry, type);

		p = ppos+1;
		ppos = strchr(p, '\n');
		memset(name, 0, 32);
	}
	return;
}

/* return the number of bytes read or negtive number indicating an error */
ssize_t srfsc_read(struct file *filp,  char * buf, uint8_t kern, size_t count, loff_t *pos)
{
	srfs_rpmsg_t msg;
	int rc;
	ssize_t bytes=0;
	ssize_t mypos = *pos;
	size_t offset= 0;
	struct srfs_fileP_t * cInfo = filp->private_data;

	mm_segment_t old_fs = get_fs();

	if (!srfs_info.serverOp || count==0 ) return 0;

	srfs_dbg("enter srfsc_read buf=%p \n", buf);

	if(cInfo == NULL) {
		srfs_err("srfsc_read: file not opened?\n");
		return -1;
	}

	memset(&msg, 0, sizeof(msg));

	rc = srfsc_seek( filp, mypos, 0) ;
	if ( rc <0 ) return rc;

	mutex_lock(&srfs_info.lock);

	do {
	        msg.len = 0;
	        msg.bytes = count-offset;
	        if (msg.bytes > SRFS_MSG_MAX_LEN)
	                msg.bytes = SRFS_MSG_MAX_LEN;

	        msg.pos = mypos;
	        msg.fd = cInfo->cid;
	        msg.lfs_id = cInfo->sid;
	        msg.len = 0;

		rc = srfs_send_rpmsg(&msg, SRFS_FILE_READ);

		if (rc !=0 ) {
			srfs_err("%s rpmsg error %d\n", __func__, rc);
			break;
		}
		memcpy(&msg, srfs_info.msg, sizeof(srfs_rpmsg_t));

		if ( msg.status != 0 ) {
			rc = -msg.status;
			srfs_dbg("%s rpmsg bad status\n", __func__ );
			break;
	        }
		if (msg.header.cmd != SRFS_FILE_READ) {
			srfs_err("srfsc_read: wrong cmd\n");
			rc = -SRFS_UNKNOWN_CMD;
			break;
		}
		if ((srfs_info.msg->fd != msg.fd) || (srfs_info.msg->lfs_id != msg.lfs_id)) {
			srfs_err("srfsc_read: fd mismatch\n");
			rc = -SRFS_INVALID_FD;
			break;
		}

		if (msg.len ==0 ) {
			srfs_err("%s LFS error\n", __func__);
			rc = -SRFS_LFS_ERR;
			break;
		}
		srfs_dbg("srfsc_read:data: %s len=%d\n", msg.data, msg.len);

		if ( old_fs==KERNEL_DS || kern ) {
			srfs_err("srfsc_read:data: kern buf\n");
			memcpy(buf+offset, msg.data, msg.len );
		}
		else {
			bytes = copy_to_user(buf+offset, msg.data, msg.len);
			if (bytes) {
				srfs_err("srfsc_read: %d bytes not copied to user\n", bytes);
				rc = SRFS_BUF_ERR;
				break;
			}
		}

		if (msg.bytes > msg.len ) {
			/* we have reached EOF */
			break;
		}
		offset += msg.len;
		mypos +=  msg.len;

	} while ( count >offset );

	mutex_unlock(&srfs_info.lock);
	set_openf_writeback(cInfo->cid, true);
	srfs_dbg("rfsc_read: readbytes=%d\n", offset);

	return (rc==0) ? offset : rc ;
}

ssize_t srfsc_write(struct file *file, const char *buf, uint8_t kern, size_t count, loff_t *pos)
{
	srfs_rpmsg_t msg;
        int len = 0;
	int rc;
	size_t bytes=0;
	struct srfs_fileP_t *cInfo;
	ssize_t mypos = *pos;
        size_t written = 0;

        srfs_dbg("srfsc_write buf=%p, kern=%u, count=%d, pos=%d\n", buf, kern, count, (int32_t)*pos);
	if (count==0 || !srfs_info.serverOp ) return 0;

	cInfo = file->private_data;
	if (cInfo==NULL) {
		srfs_dbg("srfsc_write null srfs_fileP_t. ptr \n");
		return SRFS_NULL_PTR;
	}

	if (cInfo->writeback)
		return 0;

        memset(&msg, 0, sizeof(msg));

	rc = srfsc_seek( file, mypos, 0);

	if (rc <0 ) return rc;

	mutex_lock(&srfs_info.lock);

	do  {
		len =  count - written;
		if ( len > SRFS_MSG_MAX_LEN )
			len = SRFS_MSG_MAX_LEN ;

		if ( kern ) {
			memcpy(msg.data, buf+written, len);
		}
		else {
			bytes = copy_from_user(msg.data, buf+written, len );
			if (bytes) {
				srfs_err("srfsc_write copy from user %d bytes not copied.\n", bytes);
				rc = SRFS_BUF_ERR;
				break;
			}
		}
		msg.fd = cInfo->cid;
		msg.lfs_id = cInfo->sid;
		msg.header.cmd = SRFS_FILE_WRITE ;
		msg.pos = mypos;
		msg.bytes = len ;
		msg.len = len;

		rc = srfs_send_rpmsg(&msg, SRFS_FILE_WRITE);

		if (rc !=0 ) {
			srfs_err("srfsc_write rpmsg return error\n");
			break;
		}
		memcpy(&msg, srfs_info.msg, sizeof(srfs_rpmsg_t));

		if ( msg.status != 0 ) {
			srfs_err("srfsc_write msg status error\n");
			rc =  -msg.status;
			break;
		}
		if ( msg.bytes ==0) {
			srfs_err("srfsc_write 0 byte written\n");
			rc = -SRFS_LFS_ERR;
			break;
		}
		if ((msg.header.cmd != SRFS_FILE_WRITE) || (msg.fd != cInfo->cid) || (msg.lfs_id != cInfo->sid)) {
			rc = -SRFS_RPMSG_ERR;
			break;
		}

		/* good packet */
		written += msg.bytes;
		mypos += msg.bytes;
		srfs_dbg("srfsc_write written=%d mypos=%d\n", written, mypos);

	}  while ( count > written );

	mutex_unlock(&srfs_info.lock);
	srfs_dbg("srfsc_write %d bytes\n", written);

	return ( rc==0 ) ? written: rc ;
};


int srfsc_open(struct file *filp)
{
	struct srfs_fileP_t *cInfo;
	srfs_rpmsg_t msg;
	const uint8_t *name;
	uint16_t name_len;
	uint8_t i, sid;
	int rc= 0;

	/* get a free slot in openf[] */
	cInfo = get_openf_slot( filp, &i );
	if (cInfo==NULL || i==0xFF ) {
		srfs_err("srfsc_open: openf array full \r\n");
		return -1 ;
	}

	name = filp->f_path.dentry->d_name.name;
	name_len = filp->f_path.dentry->d_name.len;

	memset(&msg, 0, sizeof(msg));
	msg.header.cmd = SRFS_FILE_OPEN;
	msg.len = name_len;
	msg.fd = i;
	msg.flags = filp->f_flags;
	strncpy(msg.data, name, name_len);
	msg.data[msg.len] = '\0';

	mutex_lock(&srfs_info.lock);

        rc = srfs_send_rpmsg(&msg, SRFS_FILE_OPEN);

        if (rc !=0 ) {
		srfs_dbg("srfs_send_rpmsg returned %d \r\n", rc);
		goto open_err;
	}
	memcpy(&msg, srfs_info.msg, sizeof(srfs_rpmsg_t));

        if ( msg.status != 0 ) {
		srfs_err("srfs_send_rpmsg bad status \r\n");
                rc = -msg.status;
	        goto open_err;
        }
	sid = msg.lfs_id;
	if (sid ==0xFF) {
		srfs_err("received invalid lfs_id\n");
		rc = -SRFS_LFS_ERR;
		goto open_err;
	}
	srfs_dbg("received fd=%u %u\n", i, sid);
	/* save the lsf_id and file ptr to openf slot i */
	set_openf_sid(i, sid);
	filp->private_data = cInfo;
	mutex_unlock(&srfs_info.lock);
	srfs_dbg("srfsc_open successful\n");
	return 0;

open_err:
	mutex_unlock(&srfs_info.lock);
	free_openf_slot(i);
	srfs_err("srfsc_open err\n");

        return rc;
};

int srfsc_close(struct file *filp)
{
	int rc;
	uint8_t fd,  lfs_id;
	srfs_rpmsg_t msg;
	struct srfs_fileP_t *cInfo;

	/* find the file in openf */
	cInfo = filp->private_data;
	if (cInfo==NULL) {
		srfs_dbg("not an open file\r\n");
		return -1;
	}
	fd = cInfo->cid;
	lfs_id = cInfo->sid;

	memset(&msg, 0, sizeof(msg));

	msg.header.cmd = SRFS_FILE_CLOSE;
	msg.len = 0;
	msg.fd = fd;
	msg.lfs_id = lfs_id;

        mutex_lock(&srfs_info.lock);
        rc = srfs_send_rpmsg(&msg, SRFS_FILE_CLOSE);

        if ((rc !=0 ) || ( srfs_info.msg->status != 0 )) {
		srfs_err("srfsc_close rpmsg error\n");
		rc = -SRFS_LFS_ERR;
        }

	mutex_unlock(&srfs_info.lock);
	free_openf_slot( fd );

        return rc;

};

int srfsc_rm(const unsigned char * fname, int len)
{
        srfs_rpmsg_t msg;
        int rc;

        memset(&msg, 0, sizeof(msg));
        msg.len = len;
	strncpy(msg.data, fname, len);
	msg.data[len]='\0';

        mutex_lock(&srfs_info.lock);
        rc = srfs_send_rpmsg(&msg, SRFS_RM_FILE);

        if (rc !=0 )
                goto rm_err;
        if ( srfs_info.msg->status != 0 ) {
                rc = srfs_info.msg->status;
                goto rm_err;
        }
	srfs_dbg("srfsc_rm ok\n");

rm_err:
        mutex_unlock(&srfs_info.lock);

        return 0;

}

int srfsc_rmdir(const unsigned char * dir, int len)
{
	srfs_rpmsg_t msg;
	int rc;

        srfs_dbg("srfsc_rmdir\n");
        memset(&msg, 0, sizeof(msg));
	msg.len = len;
	strncpy(msg.data, dir, len);
	msg.data[len]='\0';

        mutex_lock(&srfs_info.lock);
        rc = srfs_send_rpmsg(&msg, SRFS_RM_DIR);

        if (rc !=0 )
		goto rmdir_err;
        if ( srfs_info.msg->status != 0 ) {
                rc = srfs_info.msg->status;
	        goto rmdir_err;
        }

rmdir_err:
	mutex_unlock(&srfs_info.lock);
        srfs_err("srfsc_rmdir ends rc=%d\n",rc);

        return rc;

};


int srfsc_mkdir(const unsigned char * dir, int len)
{
	srfs_rpmsg_t msg;
	int rc;

        srfs_dbg("srfsc_mkdir\n");
        memset(&msg, 0, sizeof(msg));
        msg.len = len;
        strncpy(msg.data, dir, len);
	msg.data[len] = '\0';

        mutex_lock(&srfs_info.lock);
        rc = srfs_send_rpmsg(&msg, SRFS_MK_DIR);

        if (rc !=0 )
                goto mkdir_err;
        if ( srfs_info.msg->status != 0 ) {
                rc = srfs_info.msg->status;
                goto mkdir_err;
	}
	srfs_dbg("srfsc_mkdir ok\n");

mkdir_err:
	mutex_unlock(&srfs_info.lock);

        return rc;
};


int srfsc_rename( const unsigned char * oldname, int oldname_len,
		  const unsigned char * newname, int newname_len)
{
        srfs_rpmsg_t msg;
        int rc;
	int len;

	srfs_dbg("srfsc_rename\n");
        if (oldname_len + newname_len +2 > SRFS_MSG_MAX_LEN ) {
		srfs_dbg("filename too long for RPMSG\n");
		return -EFAULT;
	}

        memset(&msg, 0, sizeof(msg));

	strncpy(msg.data, oldname, oldname_len);
	msg.data[oldname_len] = '\n'; /* seperate the two str with \n  */
	len = oldname_len +1;
	strncpy( msg.data+len, newname, newname_len);
	len += newname_len;
	msg.data[len] = '\0';
	msg.len = len ;

        mutex_lock(&srfs_info.lock);
        rc = srfs_send_rpmsg(&msg, SRFS_RENAME);
	if ( rc < 0 )
                goto rename_err;
        if ( srfs_info.msg->status != 0 ) {
                rc = srfs_info.msg->status;
                goto rename_err;
        }

rename_err:
        mutex_unlock(&srfs_info.lock);
        srfs_err("srfsc_rename ends rc=%d\n", rc);

        return rc;

};


int srfsc_opendir(struct inode *inode, struct file *filp)
{
        srfs_rpmsg_t msg;
        uint16_t name_len ;
	struct srfs_fileP_t *cInfo;
	uint8_t i;
	int rc;


        /* get a free slot in openf[] */
        cInfo = get_openf_slot( filp, &i );
	if ((cInfo==NULL) || i==0xFF ){
		pr_err("srfsc_opendir: openf array full \n");
                return -1 ;
        }
	memset(&msg, 0, sizeof(msg));

	name_len = filp->f_path.dentry->d_name.len ;
	if ( name_len == 0 ) {
		strcpy(msg.data, "/");
		msg.data[1] = '\0';
	}
	else {
		msg.len = name_len;
                memcpy(msg.data, filp->f_path.dentry->d_name.name, msg.len);
                msg.data[msg.len] = '\0';
	}

	msg.fd = i;

        mutex_lock(&srfs_info.lock);

        rc = srfs_send_rpmsg(&msg, SRFS_OPEN_DIR);
        if (rc !=0 ) {
		srfs_err("srfsc_opendir srfs_send_rpmsg returned %d \r\n", rc);
                goto opendir_err;
        }
        if ( srfs_info.msg->status != 0 ) {
		srfs_err("msg->status is not OK\n");
                rc = srfs_info.msg->status;
                goto opendir_err;
        }
	if (srfs_info.msg->lfs_id==0xFF) {
		srfs_err("received invalid lfs_id\n");
		goto opendir_err;
	}
	mutex_unlock(&srfs_info.lock);
	/* save the lsf_id and file ptr to openf slot i */
	set_openf_sid(i, srfs_info.msg->lfs_id);
	//update_openf_slot(i,  filp, srfs_info.msg->lfs_id);
	srfs_dbg("opendir cInfo cid=%u,sid=%u\n", cInfo->cid, cInfo->sid);
        filp->private_data = cInfo;
	mutex_unlock(&srfs_info.lock);
        srfs_dbg("srfsc_opendir successful\n");
	return 0;

opendir_err:
        mutex_unlock(&srfs_info.lock);
	free_openf_slot(i);
        return rc;
}

int srfsc_readdir(struct file *file )
{
	srfs_rpmsg_t msg;
        int rc;
        int len = 0;
	char flist[ SRFS_PATHNAME_MAX+1];
	struct srfs_fileP_t *cInfo = file->private_data;
	struct inode * dir = file_inode(file);
	struct dentry * pDentry = file->f_path.dentry;

        srfs_dbg("enter srfsc_readdir\n");

        if( cInfo == NULL) {
                srfs_dbg("dir not open? \n");
                return -1;
        }

	set_openf_writeback(cInfo->cid, true);
	srfs_info.serverOp = false;

        memset(&msg, 0, sizeof(msg));
        msg.len = 0;
	msg.lfs_id = cInfo->sid;
	msg.fd = cInfo->cid;
        mutex_lock(&srfs_info.lock);
        rc = srfs_send_rpmsg(&msg, SRFS_READ_DIR);

        if (rc !=0 ) {
		srfs_err("srfs_readdir rpmsg return err\n");
                goto readdir_err;
	}
        if ( srfs_info.msg->status != 0 ) {
		srfs_err("srfs_readdir rpmsg status not ok\n");
                rc = srfs_info.msg->status;
                goto readdir_err;
        }
        /* process the received msg */
        len = srfs_info.msg->len;
        srfs_dbg("srfsc_readdir:data len=%d\n", len);
	mutex_unlock(&srfs_info.lock);
        if (len) {
		memcpy( flist, srfs_info.msg->data, len );
		flist[len] = '\0';
		processFlist(dir, pDentry, flist, len);
        }

	srfs_info.serverOp = true;
	set_openf_writeback(cInfo->cid, false);
        return len;

readdir_err:
	mutex_unlock(&srfs_info.lock);
	set_openf_writeback(cInfo->cid, false);
	srfs_info.serverOp = true;
        srfs_err("srfsc_readdir error\n");
	return -1;
}

int srfsc_closedir(struct inode *inode, struct file *file)
{
        srfs_rpmsg_t msg;
        int rc;
	struct srfs_fileP_t *cInfo = file->private_data;

	if( cInfo == NULL) {
                srfs_err("srfsc_readdir: dir not open? \n");
                return -1;
        }

        memset(&msg, 0, sizeof(msg));
        msg.len = 0;
	msg.lfs_id = cInfo->sid;
	msg.fd = cInfo->cid;

        mutex_lock(&srfs_info.lock);
        rc = srfs_send_rpmsg(&msg, SRFS_CLOSE_DIR);

        if ( srfs_info.msg->status != 0 ) {
                rc = srfs_info.msg->status;
        }

        mutex_unlock(&srfs_info.lock);
	free_openf_slot(cInfo->cid);
        return rc;
}

int srfsc_seek(struct file *file, loff_t offset, int whence)
{
        srfs_rpmsg_t msg;
        int rc = 0;
        struct srfs_fileP_t *cInfo = file->private_data;

        if( cInfo == NULL) {
                srfs_err("srfsc_seek: file not open? \n");
                return -1;
        }

        memset(&msg, 0, sizeof(msg));
        msg.len = 0;
        msg.lfs_id = cInfo->sid;
        msg.fd = cInfo->cid;
	msg.pos = offset;
	msg.flags = (uint16_t) whence ;

        mutex_lock(&srfs_info.lock);
        rc = srfs_send_rpmsg(&msg, SRFS_FILE_SEEK);
        if ( srfs_info.msg->status != 0 ) {
                rc = srfs_info.msg->status;
        }
	else {
		rc = srfs_info.msg->bytes;
	}
        mutex_unlock(&srfs_info.lock);
	srfs_dbg("srfsc_seek rc=%d\n", rc);
	return rc;
}
