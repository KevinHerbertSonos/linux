/*
 * drivers/amlogic/efuse/efuse.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __EFUSE_H
#define __EFUSE_H

/* #define EFUSE_DEBUG */
/*#define EFUSE_READ_ONLY			1*/

/* #define EFUSE_NONE_ID			0 */
#define EFUSE_VERSION_ID		1
#define EFUSE_LICENCE_ID		2

#define EFUSE_MAC_ID				3
#define EFUSE_MAC_WIFI_ID	4
#define EFUSE_MAC_BT_ID		5
/* #define EFUSE_HDCP_ID			6 */
#define EFUSE_USID_ID				7

/* #define EFUSE_RSA_KEY_ID		8 */
/* #define EFUSE_CUSTOMER_ID		9 */
/* #define EFUSE_MACHINEID_ID		10 */
#define EFUSE_NANDEXTCMD_ID		11

#define EFUSE_DWORDS            128  /* (EFUSE_BITS/32) */

#define EFUSE_BYTES            512  /* (EFUSE_BITS/8) */
#define AXG_EFUSE_BYTES        256

#define EFUSE_INFO_GET			_IO('f', 0x40)

#define EFUSE_HAL_API_READ	0
#define EFUSE_HAL_API_WRITE 1
#define EFUSE_HAL_API_USER_MAX 3

#define ASSIST_HW_REV 0x1f53

extern int efuseinfo_num;

extern void __iomem *sharemem_input_base;
extern void __iomem *sharemem_output_base;
extern unsigned int efuse_read_cmd;
extern unsigned int efuse_write_cmd;
extern unsigned int efuse_read_obj_cmd;
extern unsigned int efuse_write_obj_cmd;
extern unsigned int efuse_get_max_cmd;

struct efuseinfo_item_t {
	char title[40];
	unsigned int id;
	loff_t offset;    /* write offset */
	unsigned int data_len;
};

struct efuseinfo_t {
	struct efuseinfo_item_t *efuseinfo_version;
	int size;
	int version;
};

struct efuse_platform_data {
	loff_t pos;
	size_t count;
	bool (*data_verify)(const char *usid);
};

/* efuse HAL_API arg */
struct efuse_hal_api_arg {
	unsigned int cmd;		/* R/W */
	unsigned int offset;
	unsigned int size;
	unsigned long buffer;
	unsigned long retcnt;
};

typedef enum efuse_obj_status_s {
	EFUSE_OBJ_SUCCESS		= 0,

	EFUSE_OBJ_ERR_INVALID_DATA	= 100,
	EFUSE_OBJ_ERR_NOT_FOUND,
	EFUSE_OBJ_ERR_DEPENDENCY,
	EFUSE_OBJ_ERR_SIZE,
	EFUSE_OBJ_ERR_NOT_SUPPORT,

	EFUSE_OBJ_ERR_ACCESS		= 200,

	EFUSE_OBJ_ERR_UNKNOWN		= 300,
	EFUSE_OBJ_ERR_INTERNAL,
} efuse_obj_status_e;

typedef enum efuse_obj_type_s {
    /* Built-in object type */
    EFUSE_OBJ_LICENSE_ENABLE_SECURE_BOOT    = 0,
    EFUSE_OBJ_LICENSE_ENABLE_ENCRYPTION,
    EFUSE_OBJ_LICENSE_REVOKE_KPUB_0,
    EFUSE_OBJ_LICENSE_REVOKE_KPUB_1,
    EFUSE_OBJ_LICENSE_REVOKE_KPUB_2,
    EFUSE_OBJ_LICENSE_REVOKE_KPUB_3,
    EFUSE_OBJ_LICENSE_ENABLE_ANTIROLLBACK,
    EFUSE_OBJ_LICENSE_ENABLE_JTAG_PASSWORD,
    EFUSE_OBJ_LICENSE_ENABLE_SCAN_PASSWORD,
    EFUSE_OBJ_LICENSE_DISABLE_JTAG,
    EFUSE_OBJ_LICENSE_DISABLE_SCAN,
    EFUSE_OBJ_LICENSE_ENABLE_USB_BOOT_PASSWORD,
    EFUSE_OBJ_LICENSE_DISABLE_USB_BOOT,
#if defined(SONOS_ARCH_ATTR_SOC_IS_S767)
    EFUSE_OBJ_LICENSE_ENABLE_M3_SECURE_BOOT,
    EFUSE_OBJ_LICENSE_ENABLE_M3_ENCRYPTION,
    EFUSE_OBJ_LICENSE_ENABLE_M4_SECURE_BOOT,
    EFUSE_OBJ_LICENSE_ENABLE_M4_ENCRYPTION,
    EFUSE_OBJ_LICENSE_REVOKE_M4_KPUB_0,
    EFUSE_OBJ_LICENSE_REVOKE_M4_KPUB_1,
    EFUSE_OBJ_LICENSE_ENABLE_M4_JTAG_PASSWORD,
    EFUSE_OBJ_LICENSE_DISABLE_M3_JTAG,
    EFUSE_OBJ_LICENSE_DISABLE_M4_JTAG,
#endif

    EFUSE_OBJ_THERMAL           = 0x100,
    EFUSE_OBJ_SBOOT_KPUB_SHA,
    EFUSE_OBJ_SBOOT_AES256,
    EFUSE_OBJ_JTAG_PASSWD_SHA_SALT,
    EFUSE_OBJ_SCAN_PASSWD_SHA_SALT,
    EFUSE_OBJ_SBOOT_AES256_RAW,
    EFUSE_OBJ_SBOOT_AES256_ENCRYPT,
    EFUSE_OBJ_SBOOT_AES256_SHA2,
    EFUSE_OBJ_AUDIO_CUSTOMER_ID,
#if defined(SONOS_ARCH_ATTR_SOC_IS_S767)
    EFUSE_OBJ_M4_SBOOT_KPUB_SHA,
    EFUSE_OBJ_M4_SBOOT_AES256_RAW,
    EFUSE_OBJ_M4_SBOOT_AES256_SHA2,
#endif

    /* General Purpose (GP) REE object */
    EFUSE_OBJ_GP_REE    = 0x200,

    /* General Purpose (GP) TEE object */
    EFUSE_OBJ_GP_TEE    = 0x300,

    /* Raw object */
    EFUSE_OBJ_RAW       = 0x400,
} efuse_obj_type_e;


extern struct efuseinfo_t efuseinfo[];
#ifndef CONFIG_ARM64
int efuse_getinfo_byTitle(unsigned char *name, struct efuseinfo_item_t *info);
int check_if_efused(loff_t pos, size_t count);
int efuse_read_item(char *buf, size_t count, loff_t *ppos);
int efuse_write_item(char *buf, size_t count, loff_t *ppos);
extern int efuse_active_version;
extern struct clk *efuse_clk;
#else

ssize_t efuse_get_max(void);
ssize_t efuse_read_usr(char *buf, size_t count, loff_t *ppos);
ssize_t efuse_write_usr(char *buf, size_t count, loff_t *ppos);

#ifdef CONFIG_EFUSE_OBJ_API
ssize_t efuse_obj_read(int obj_id, char*buf, ssize_t len);
ssize_t efuse_obj_write(int obj_id, char*buf, ssize_t len);
#endif

#endif

#endif
