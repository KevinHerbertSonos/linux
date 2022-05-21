/*
 * Copyright (c) 2019-2020, Sonos, Inc.
 *
 * All rights reserved.*
 *
 */

#ifndef _SRFS_UTIL_H_
#define _SRFS_UTIL_H_

#include <linux/mutex.h>
#include <linux/fs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct srfs_fileP_t {
        struct file *filp; // linux file ptr

        uint8_t cid;       // client id
        uint8_t sid;       // server id
        bool writeback;
} ;


void init_openf(void);

struct srfs_fileP_t * get_openf_slot(struct file * filp, uint8_t *out);

void free_openf_slot( uint8_t cid) ;

bool get_openf_writeback( uint8_t cid);

void set_openf_writeback(uint8_t cid, bool val );

uint8_t  get_openf_sid(uint8_t cid );

void set_openf_sid( uint8_t cid , uint8_t sid  );

void update_openf_slot(uint8_t i, struct file * filp, uint8_t sid);



#ifdef __cplusplus
} /* extern "C" */
#endif
#endif

