/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFUSE_H
#define __EFUSE_H

/* #define EFUSE_READ_ONLY */

#ifndef EFUSE_READ_ONLY
#define EFUSE_CLASS_ATTR CLASS_ATTR_RW
#else
#define EFUSE_CLASS_ATTR CLASS_ATTR_RO
#endif

#define EFUSE_CHECK_NAME_LEN   32

struct aml_efuse_key {
	int                   num;
	struct efusekey_info *infos;
};

#define EFUSE_INFO_GET		_IO('f', 0x40)

#define EFUSE_HAL_API_READ	0
#define EFUSE_HAL_API_WRITE 1
#define EFUSE_HAL_API_USER_MAX 3

#define AML_DATA_PROCESS            (0x820000FF)
#define AML_D_P_W_EFUSE_AMLOGIC     (0x20)
#define EFUSE_PATTERN_SIZE      (0x400)
#define EFUSE_OBJ_READ          (0x82000038)
#define EFUSE_OBJ_WRITE         (0x82000039)

enum efuse_obj_status_e {
	EFUSE_OBJ_SUCCESS		= 0,

	EFUSE_OBJ_ERR_OTHER_INTERNAL	= 1,

	EFUSE_OBJ_ERR_INVALID_DATA	= 100,
	EFUSE_OBJ_ERR_NOT_FOUND,
	EFUSE_OBJ_ERR_DEPENDENCY,
	EFUSE_OBJ_ERR_SIZE,
	EFUSE_OBJ_ERR_NOT_SUPPORT,

	EFUSE_OBJ_ERR_ACCESS		= 200,
	EFUSE_OBJ_ERR_WRITE_PROTECTED = 201,

	EFUSE_OBJ_ERR_UNKNOWN		= 300,
	EFUSE_OBJ_ERR_INTERNAL,
};

enum efuse_obj_info_e {
	EFUSE_OBJ_EFUSE_DATA	= 0,
	EFUSE_OBJ_LOCK_STATUS,
};

enum efuse_obj_type_e {
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
};

struct efuse_obj_field_t {
	char name[48];
	unsigned char data[32];
	unsigned int size;
};

/* efuse HAL_API arg */
struct efuse_hal_api_arg {
	unsigned int cmd;
	unsigned int offset;
	unsigned int size;
	unsigned long buffer;
	unsigned long retcnt;
};

struct aml_efuse_cmd {
	unsigned int read_cmd;
	unsigned int write_cmd;
	unsigned int read_obj_cmd;
	unsigned int write_obj_cmd;
	unsigned int get_max_cmd;
	unsigned int mem_in_base_cmd;
	unsigned int mem_out_base_cmd;
};

struct lockable_info {
	char itemname[EFUSE_CHECK_NAME_LEN];
	unsigned int subcmd;
};

struct aml_efuse_lockable_check {
	unsigned int main_cmd;
	unsigned int item_num;
	struct lockable_info *infos;
};

extern struct aml_efuse_cmd efuse_cmd;
extern void __iomem *sharemem_input_base;
extern void __iomem *sharemem_output_base;
extern unsigned int efuse_obj_cmd_status;

ssize_t efuse_get_max(void);
ssize_t efuse_read_usr(char *buf, size_t count, loff_t *ppos);
ssize_t efuse_write_usr(char *buf, size_t count, loff_t *ppos);
unsigned long efuse_amlogic_set(char *buf, size_t count);
u32 efuse_obj_write(u32 obj_id, char *name, u8 *buff, u32 size);
u32 efuse_obj_read(u32 obj_id, char *name, u8 *buff, u32 *size);

/*return: 0:is configurated, -1: don't cfg*/
int efuse_burn_lockable_is_cfg(char *itemname);
/*
 * return: 1:burned(written), 0: not write, -1: fail
 */
int efuse_burn_check_burned(char *itemname);

#ifdef CONFIG_AMLOGIC_EFUSE
int __init aml_efuse_init(void);
void aml_efuse_exit(void);
#else
static int __init aml_efuse_init(void)
{
	return 0;
}

static void aml_efuse_exit(void)
{
}
#endif
#endif
