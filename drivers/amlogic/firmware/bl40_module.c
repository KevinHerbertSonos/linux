// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/reset.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <asm/cacheflush.h>
#include <linux/firmware.h>
#include <linux/arm-smccc.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/spinlock.h>
#include "bl40_module.h"

#define AMLOGIC_BL40_BOOTUP     0x8200004E
#define BL40_GET_CODE           0x5A5A8D9E
#define BL40_GET_TYPE_COLOR     1
#define BL40_GET_TYPE_PATTERN   2
#define ACK_OK                  1
#define ACK_FAIL                2

struct bl40_info {
	int cpuid;
	char name[30];
};

struct bl40_msg {
	struct list_head list;
	struct bl40_msg_buf msg_buf;
	struct completion complete;
};

struct mbox_buf {
	char buf[64];
	u32 sum_data;
};

/*for listen list*/
spinlock_t lock;
struct device *device;
static LIST_HEAD(bl40_list);
struct bl40_msg_buf g_bl40_buf;
#define BL40_IOC_MAGIC  'H'
#define BL40_FIRMWARE_LOAD      _IOWR(BL40_IOC_MAGIC, 1, struct bl40_info)
#define BL40_CMD_SEND           _IOWR(BL40_IOC_MAGIC, 2, struct bl40_msg_buf)
#define BL40_CMD_LISTEN         _IOWR(BL40_IOC_MAGIC, 3, struct bl40_msg_buf)
#define BL40_CMD_GET            _IOWR(BL40_IOC_MAGIC, 4, struct bl40_msg_buf)

void *bl40_rx_msg(void *msg, uint32_t size)
{
	struct bl40_msg_buf *msg_buf;
	u32 data_size = 0;

	msg_buf = &g_bl40_buf;
	if (size > BL40_BUF_SIZE)
		data_size = BL40_BUF_SIZE;
	else
		data_size = size;

	msg_buf->size = data_size;
	memcpy(msg_buf->buf, msg, data_size);
	pr_info("%s: data = 0x%x\n", __func__, *(unsigned int *)msg_buf->buf);

	return NULL;
}

static long bl40_miscdev_ioctl(struct file *fp, unsigned int cmd,
			       unsigned long arg)
{
	int ret = 0;
	const struct firmware *firmware = NULL;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case BL40_FIRMWARE_LOAD: {
		struct bl40_info bl40_info = {0};
		unsigned long phy_addr;
		void *virt_addr = NULL;
		struct arm_smccc_res res = {0};
		size_t size;

		ret = copy_from_user((void *)&bl40_info,
				     argp, sizeof(bl40_info));
		if (ret < 0)
			return ret;
		ret = request_firmware(&firmware, bl40_info.name, device);
		if (ret < 0) {
			pr_err("req firmware fail %d\n", ret);
			return ret;
		}

		size = firmware->size;
		virt_addr = devm_kzalloc(device, size, GFP_KERNEL);
		if (!virt_addr) {
			release_firmware(firmware);
			pr_err("memory request fail\n");
			return -ENOMEM;
		}
		memcpy(virt_addr, firmware->data, size);
		release_firmware(firmware);
		dma_map_single(device, virt_addr, size, DMA_FROM_DEVICE);
		phy_addr = virt_to_phys(virt_addr);

		/* unlock bl40 */
		scpi_unlock_bl40();
		arm_smccc_smc(AMLOGIC_BL40_BOOTUP, phy_addr,
			      size, 0, 0, 0, 0, 0, &res);
		pr_info("free memory\n");
		devm_kfree(device, virt_addr);
		ret = res.a0;
	}
	break;
	case BL40_CMD_SEND: {
		struct bl40_msg_buf bl40_buf = {0};

		ret = copy_from_user((void *)&bl40_buf,
				     argp, sizeof(bl40_buf));
		pr_debug("Enter BL40_CMD_SEND\n");
		scpi_send_bl40(SCPI_CMD_BL4_SEND, bl40_buf.buf, bl40_buf.size);
		ret = copy_to_user(argp, &bl40_buf, sizeof(bl40_buf));
	}
	break;
	case BL40_CMD_LISTEN: {
#ifdef NOTDEF
		struct bl40_msg bl40_msg = {0};
		unsigned long flags;

		init_completion(&bl40_msg.complete);
		spin_lock_irqsave(&lock, flags);
		list_add_tail(&bl40_msg.list, &bl40_list);
		spin_unlock_irqrestore(&lock, flags);
		wait_for_completion(&bl40_msg.complete);
		ret = copy_to_user(argp, &bl40_msg.msg_buf,
				   sizeof(struct bl40_msg_buf));
		spin_lock_irqsave(&lock, flags);
		list_del(&bl40_msg.list);
		spin_unlock_irqrestore(&lock, flags);
#endif /* NOTDEF */
	}
	break;
	case BL40_CMD_GET: {
		void *buf;
		unsigned int size;
		unsigned int type;
		struct bl40_msg_buf bl40_buf = {0};

		ret = copy_from_user((void *)&bl40_buf,	argp, sizeof(bl40_buf));
		buf = (void *)bl40_buf.buf;
		type = *(unsigned int *)buf;
		if (bl40_buf.size > BL40_BUF_SIZE)
			size = BL40_BUF_SIZE;
		else
			size = bl40_buf.size;

		pr_info("Enter BL40_CMD_GET\n");
		scpi_send_bl40(SCPI_CMD_BL4_GET, buf, size);
		mdelay(20);
		ret = copy_to_user(argp, &g_bl40_buf, sizeof(g_bl40_buf));
	}
	break;
	default:
		pr_info("Not have this cmd %x\n", cmd);
	break;
	};
	pr_debug("bl40 ioctl\n");
	return ret;
}

static const struct file_operations bl40_miscdev_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.unlocked_ioctl = bl40_miscdev_ioctl,
	.compat_ioctl = bl40_miscdev_ioctl,
};

static struct miscdevice bl40_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "bl40",
	.fops	= &bl40_miscdev_fops,
};

void *bl40_rx_data_callback(void *data, uint32_t size)
{
	struct mbox_buf *mbox_buf = (struct mbox_buf *)data;
	u8 idx = 0;
	u32 sum_data = 0;

	for (idx = 0; idx < size - 4; idx++)
		sum_data += mbox_buf->buf[idx];

	if (sum_data != mbox_buf->sum_data)
		pr_err("%s err %x != %x\n", __func__,
			sum_data, mbox_buf->sum_data);
	return data;
}

static int bl40_platform_probe(struct platform_device *pdev)
{
	int ret;

	device = &pdev->dev;
	platform_set_drvdata(pdev, NULL);
	ret = misc_register(&bl40_miscdev);
	//aml_mbox_receive_client_register(SCPI_CMD_BL4_LISTEN,
	//				 bl40_rx_msg);
	aml_mbox_receive_client_register(SCPI_CMD_BL4_GET, bl40_rx_msg);

	pr_info("bl40 probe\n");
	return ret;
}

static int bl40_platform_remove(struct platform_device *pdev)
{
	misc_deregister(&bl40_miscdev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id bl40_device_id[] = {
	{
		.compatible = "amlogic, bl40-bootup",
	},
	{}
};

static struct platform_driver bl40_platform_driver = {
	.driver = {
		.name  = "bl40",
		.owner = THIS_MODULE,
		.of_match_table = bl40_device_id,
	},
	.probe  = bl40_platform_probe,
	.remove = bl40_platform_remove,
};
module_platform_driver(bl40_platform_driver);

MODULE_AUTHOR("Amlogic Bl40");
MODULE_DESCRIPTION("BL40 Boot Module Driver");
MODULE_LICENSE("GPL");
