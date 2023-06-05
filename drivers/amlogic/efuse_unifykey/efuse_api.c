// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/amlogic/efuse.h>
#include <linux/kallsyms.h>
#include "efuse.h"
#include <linux/amlogic/secmon.h>

static ssize_t meson_efuse_fn_smc(struct efuse_hal_api_arg *arg)
{
	long ret;
	unsigned int cmd, offset, size;
	unsigned long *retcnt = (unsigned long *)(arg->retcnt);
	struct arm_smccc_res res;

	if (arg->cmd == EFUSE_HAL_API_READ)
		cmd = efuse_cmd.read_cmd;
	else if (arg->cmd == EFUSE_HAL_API_WRITE)
		cmd = efuse_cmd.write_cmd;
	else
		return -1;

	offset = arg->offset;
	size = arg->size;

	meson_sm_mutex_lock();

	if (arg->cmd == EFUSE_HAL_API_WRITE)
		memcpy((void *)sharemem_input_base,
		       (const void *)arg->buffer, size);

	asm __volatile__("" : : : "memory");

	arm_smccc_smc(cmd, offset, size, 0, 0, 0, 0, 0, &res);
	ret = res.a0;
	*retcnt = res.a0;

	if ((arg->cmd == EFUSE_HAL_API_READ) && (ret != 0))
		memcpy((void *)arg->buffer,
		       (const void *)sharemem_output_base, ret);

	meson_sm_mutex_unlock();

	if (!ret)
		return -1;

	return 0;
}

static ssize_t meson_trustzone_efuse(struct efuse_hal_api_arg *arg)
{
	ssize_t ret;
	struct cpumask task_cpumask;

	if (!arg)
		return -1;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	ret = meson_efuse_fn_smc(arg);
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

/* BEGIN added by Sonos */

static int meson_efuse_obj_rw_fn(u32 cmd, u32 obj_id, uint8_t *buff, u32 *size)
{
	u32 rc = EFUSE_OBJ_ERR_UNKNOWN;
	u32 len = 0;
	struct arm_smccc_res res;

	if (!sharemem_input_base || !sharemem_output_base || !buff || !size) {
		return -1;
	}

	/* pr_info("%s cmd:0x%x, obj_id:0x%x[%d] size:%d\n",
	 * __func__, cmd, obj_id, obj_id, *size);
	 */

	meson_sm_mutex_lock();

	if (cmd == efuse_cmd.write_obj_cmd) {
		if (obj_id == EFUSE_OBJ_GP_REE && buff && *size == 16) {
			memcpy((void *)sharemem_input_base, buff, *size);
		} else {
			rc = -EFUSE_OBJ_ERR_INVALID_DATA;
			goto out;
		}
	}

	asm __volatile__("" : : : "memory");
	arm_smccc_smc(cmd, obj_id, *size, 0, 0, 0, 0, 0, &res);
	rc = res.a0;

	if (cmd == efuse_cmd.read_obj_cmd) {
		len = res.a1;
		/* pr_info("%s read rc:%d len:%d\n", __func__, rc, len); */
		if (rc == EFUSE_OBJ_SUCCESS) {
			if (*size >= len) {
				memcpy(buff, (const void *)sharemem_output_base, len);
				*size = len;
			} else {
				rc = -EFUSE_OBJ_ERR_SIZE;
			}
		} else {
			rc = -rc;
		}
	} else {
		/* pr_info("%s write rc:%d\n", __func__, rc); */
		if (rc != EFUSE_OBJ_SUCCESS) {
			rc = -rc;
		}
	}

out:
	meson_sm_mutex_unlock();

	return rc;
}

int meson_efuse_obj_read_sonos(u32 obj_id, u8 *buff, u32 *size)
{
	struct cpumask task_cpumask;
	int rc;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	rc = meson_efuse_obj_rw_fn(efuse_cmd.read_obj_cmd, obj_id, buff, size);
	set_cpus_allowed_ptr(current, &task_cpumask);

	if (rc != EFUSE_OBJ_SUCCESS) {
		pr_err("%s failed:%d\n", __func__, rc);
	}
	return rc;
}

int meson_efuse_obj_write_sonos(u32 obj_id, u8 *buff, u32 *size)
{
	struct cpumask task_cpumask;
	int rc;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	rc = meson_efuse_obj_rw_fn(efuse_cmd.write_obj_cmd, obj_id, buff, size);
	set_cpus_allowed_ptr(current, &task_cpumask);

	if (rc != EFUSE_OBJ_SUCCESS) {
		pr_err("%s failed:%d\n", __func__, rc);
	}
	return rc;
}

/* END added by Sonos */

static u32 meson_efuse_obj_read(u32 obj_id, u8 *buff, u32 *size)
{
	u32 rc = EFUSE_OBJ_ERR_UNKNOWN;
	u32 len = 0;
	struct arm_smccc_res res;

	meson_sm_mutex_lock();

	memcpy((void *)sharemem_input_base, buff, *size);

	do {
		arm_smccc_smc((unsigned long)EFUSE_OBJ_READ,
			      (unsigned long)obj_id,
			      (unsigned long)*size,
			      0, 0, 0, 0, 0, &res);

	} while (0);

	rc = res.a0;
	len = res.a1;

	if (rc == EFUSE_OBJ_SUCCESS) {
		if (*size >= len) {
			memcpy(buff, (const void *)sharemem_output_base, len);
			*size = len;
		} else {
			rc = EFUSE_OBJ_ERR_SIZE;
		}
	}

	meson_sm_mutex_unlock();

	return rc;
}

static u32 meson_efuse_obj_write(u32 obj_id, u8 *buff, u32 size)
{
	struct arm_smccc_res res;

	meson_sm_mutex_lock();

	memcpy((void *)sharemem_input_base, buff, size);

	do {
		arm_smccc_smc((unsigned long)EFUSE_OBJ_WRITE,
			      (unsigned long)obj_id,
			      (unsigned long)size,
			      0, 0, 0, 0, 0, &res);

	} while (0);

	meson_sm_mutex_unlock();

	return res.a0;
}

u32 efuse_obj_write(u32 obj_id, char *name, u8 *buff, u32 size)
{
	u32 ret;
	struct efuse_obj_field_t efuseinfo;
	struct cpumask task_cpumask;

	if (efuse_obj_cmd_status != 1)
		return EFUSE_OBJ_ERR_NOT_FOUND;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	memset(&efuseinfo, 0, sizeof(efuseinfo));
	strncpy(efuseinfo.name, name, sizeof(efuseinfo.name) - 1);
	if (size > sizeof(efuseinfo.data))
		return EFUSE_OBJ_ERR_SIZE;
	efuseinfo.size = size;
	memcpy(efuseinfo.data, buff, efuseinfo.size);
	ret = meson_efuse_obj_write(obj_id, (uint8_t *)&efuseinfo, sizeof(efuseinfo));
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

u32 efuse_obj_read(u32 obj_id, char *name, u8 *buff, u32 *size)
{
	u32 ret;
	struct efuse_obj_field_t efuseinfo;
	struct cpumask task_cpumask;

	if (efuse_obj_cmd_status != 1)
		return EFUSE_OBJ_ERR_NOT_FOUND;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	memset(&efuseinfo, 0, sizeof(efuseinfo));
	strncpy(efuseinfo.name, name, sizeof(efuseinfo.name) - 1);
	*size = sizeof(efuseinfo);
	ret = meson_efuse_obj_read(obj_id, (uint8_t *)&efuseinfo, size);
	memcpy(buff, efuseinfo.data, efuseinfo.size);
	*size = efuseinfo.size;
	set_cpus_allowed_ptr(current, &task_cpumask);

	return ret;
}

static ssize_t _efuse_read(char *buf, size_t count, loff_t *ppos)
{
	unsigned int pos = *ppos;

	struct efuse_hal_api_arg arg;
	unsigned long retcnt;
	ssize_t ret;

	arg.cmd = EFUSE_HAL_API_READ;
	arg.offset = pos;
	arg.size = count;
	arg.buffer = (unsigned long)buf;
	arg.retcnt = (unsigned long)&retcnt;
	ret = meson_trustzone_efuse(&arg);
	if (ret == 0) {
		*ppos += retcnt;
		return retcnt;
	}

	return ret;
}

static ssize_t _efuse_write(const char *buf, size_t count, loff_t *ppos)
{
	unsigned int pos = *ppos;

	struct efuse_hal_api_arg arg;
	unsigned long retcnt;
	ssize_t ret;

	arg.cmd = EFUSE_HAL_API_WRITE;
	arg.offset = pos;
	arg.size = count;
	arg.buffer = (unsigned long)buf;
	arg.retcnt = (unsigned long)&retcnt;

	ret = meson_trustzone_efuse(&arg);
	if (ret == 0) {
		*ppos = retcnt;
		return retcnt;
	}

	return ret;
}

ssize_t efuse_read_usr(char *buf, size_t count, loff_t *ppos)
{
	char *pdata = NULL;
	ssize_t ret;
	loff_t pos;

	pdata = kmalloc(count, GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pos = *ppos;

	ret = _efuse_read(pdata, count, (loff_t *)&pos);

	memcpy(buf, pdata, count);
	kfree(pdata);

	return ret;
}

ssize_t efuse_write_usr(char *buf, size_t count, loff_t *ppos)
{
	char *pdata = NULL;
	ssize_t ret;
	loff_t pos;

	pdata = kmalloc(count, GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	memcpy(pdata, buf, count);
	pos = *ppos;

	ret = _efuse_write(pdata, count, (loff_t *)&pos);
	kfree(pdata);

	return ret;
}
