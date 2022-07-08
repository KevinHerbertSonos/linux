/*
 * Copyright (c) 2020, Sonos, Inc.
 *
 * All rights reserved.
 *
 */


#include "srfs_util.h"
#define SRFS_MAX_OPEN_FILES   256


struct mutex of_lock;

static struct srfs_fileP_t opfiles[SRFS_MAX_OPEN_FILES];


void init_openf(void)
{
	memset(opfiles, 0,  sizeof(opfiles));
	mutex_init(&of_lock);
}

/* Input: filp
 * outpuf: client id
 * return: ptr to the allocated struct srfs_fileP_t
 * Find the first available openf data struct in the preallocated array
 * populated with input and default paramaters
 */
struct srfs_fileP_t * get_openf_slot(struct file * filp, uint8_t *out)
{
	int i;
	struct srfs_fileP_t *ret=NULL;
	mutex_lock(&of_lock);

	for (i=0; i<256; i++) {
		if (opfiles[i].filp == NULL) {
			opfiles[i].filp = filp;
			opfiles[i].cid = i;
			opfiles[i].writeback=false;
			*out = i ;
			ret = &opfiles[i];
			break;
		}
	}
	mutex_unlock(&of_lock);
	return ret;
}

void free_openf_slot( uint8_t cid)
{
	mutex_lock(&of_lock);
	opfiles[cid].filp = NULL;
	opfiles[cid].cid = opfiles[cid].sid = 0;
	opfiles[cid].writeback = 0;
	mutex_unlock(&of_lock);
}

bool get_openf_writeback( uint8_t cid )
{
	bool ret;
	mutex_lock(&of_lock);
	ret = opfiles[cid].writeback;
	mutex_unlock(&of_lock);
	return ret;
}

void set_openf_writeback(uint8_t cid, bool val )
{
        mutex_lock(&of_lock);
        opfiles[cid].writeback = val;
        mutex_unlock(&of_lock);
}

uint8_t  get_openf_sid( uint8_t cid )
{
        uint8_t ret;
        mutex_lock(&of_lock);
        ret = opfiles[cid].sid;
        mutex_unlock(&of_lock);
        return ret;
}

void set_openf_sid( uint8_t cid , uint8_t sid  )
{
        mutex_lock(&of_lock);
        opfiles[cid].sid = sid;
        mutex_unlock(&of_lock);
}

void update_openf_slot(uint8_t i, struct file * filp, uint8_t sid)
{
	mutex_lock(&of_lock);
        opfiles[i].filp = filp;
	opfiles[i].sid = sid;
        mutex_unlock(&of_lock);
}

