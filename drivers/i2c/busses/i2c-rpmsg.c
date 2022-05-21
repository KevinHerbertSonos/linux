/*
 * Copyright (c) 2019, Sonos, Inc.
 *
 * SPDX-License-Identifier:     GPL-2.0
 *
 */

/* The i2c-rpmsg transfer protocol:
 *
 *   +---------------+-------------------------------+
 *   |  Byte Offset  |            Content            |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       0       |           Category            |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |     1 ~ 2     |           Version             |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       3       |             Type              |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       4       |           Command             |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       5       |           Reserved1           |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       6       |           Reserved2           |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       7       |           Reserved3           |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       8       |           Reserved4           |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       9       |           Reserved5           |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       10      |         Return Value          |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       11      |           Bus    ID           |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       12      |           Device ID           |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       13      |           Flags               |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |    14 ~ 17    |           Reg Address         |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       18      |           Reg Address Len     |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |       19      |           Data Len            |
 *   +---------------+---+---+---+---+---+---+---+---+
 *   |    20 ~ 35    |        16 Bytes Data          |
 *   +---------------+---+---+---+---+---+---+---+---+
 *
 * The definition of Return Value:
 * 0x00 = Success
 * 0x01 = Failed
 * 0x02 = Invalid parameter
 * 0x03 = Invalid message
 * 0x04 = Operate in invalid state
 * 0x05 = Memory allocation failed
 * 0x06 = Timeout when waiting for an event
 * 0x07 = Cannot add to list as node already in another list
 * 0x08 = Cannot remove from list as node not in list
 * 0x09 = Transfer timeout
 * 0x0A = Transfer failed due to peer core not ready
 * 0x0B = Transfer failed due to communication failure
 * 0x0C = Cannot find service for a request/notification
 * 0x0D = Service version cannot support the request/notification
 *
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/imx_rpmsg.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/rpmsg.h>
#include <linux/i2c.h>

#define I2C_RPMSG_MAX_BUF_SIZE			16
#define RPMSG_TIMEOUT				1000

#define I2C_RPMSG_CATEGORY			0x09
#define I2C_RPMSG_VERSION			0x0001
#define I2C_RPMSG_TYPE_REQUEST			0x00
#define I2C_RPMSG_TYPE_RESPONSE			0x01
#define I2C_RPMSG_COMMAND_READ			0x00
#define I2C_RPMSG_COMMAND_WRITE			0x01
#define I2C_RPMSG_PRIORITY			0x01

#define I2C_RPMSG_M_STOP			0x0200

struct i2c_rpmsg_msg {
	struct imx_rpmsg_head header;
	u8 ret_val;
	u8 bus;
	u8 addr;
	u32 subAddress;
	u8 subaddressSize;
	u8 len;
	u8 buf[I2C_RPMSG_MAX_BUF_SIZE];
} __attribute__ ((packed));

struct i2c_rpmsg_info {
	struct rpmsg_device *rpdev;
	struct device *dev;
	struct i2c_rpmsg_msg *msg;
	struct pm_qos_request pm_qos_req;
	struct completion cmd_complete;
	struct mutex lock;
};

static struct i2c_rpmsg_info i2c_rpmsg_info;

static int i2c_send_message(struct i2c_rpmsg_msg *msg,
			       struct i2c_rpmsg_info *info)
{
	int err;

	if (!info->rpdev) {
		dev_dbg(info->dev,
			 "rpmsg channel not ready, m4 image ready?\n");
		return -EINVAL;
	}

	mutex_lock(&info->lock);
	pm_qos_add_request(&info->pm_qos_req, PM_QOS_CPU_DMA_LATENCY, 0);

	msg->header.cate = IMX_RPMSG_I2C;
	msg->header.major = IMX_RMPSG_MAJOR;
	msg->header.minor = IMX_RMPSG_MINOR;
	msg->header.type = 0;

	/* wait response from rpmsg */
	reinit_completion(&info->cmd_complete);

	err = rpmsg_send(info->rpdev->ept, (void *)msg,
			    sizeof(struct i2c_rpmsg_msg));
	if (err) {
		dev_err(&info->rpdev->dev, "rpmsg_send failed: %d\n", err);
		goto err_out;
	}

	err = wait_for_completion_timeout(&info->cmd_complete,
					  msecs_to_jiffies(RPMSG_TIMEOUT));

	if (!err) {
		dev_err(&info->rpdev->dev, "rpmsg_send timeout!\n");
		err = -ETIMEDOUT;
		goto err_out;
	}

	err = 0;

err_out:
	pm_qos_remove_request(&info->pm_qos_req);
	mutex_unlock(&info->lock);

	dev_dbg(&info->rpdev->dev, "cmd:%d, resp:%d.\n",
		  msg->header.cmd, msg->ret_val);

	return err;
}

static struct rpmsg_device_id i2c_rpmsg_id_table[] = {
	{ .name	= "rpmsg-i2c-channel" },
	{ },
};

static int i2c_rpmsg_probe(struct rpmsg_device *rpdev)
{
	int ret = 0;

	dev_info(&rpdev->dev, "new channel: 0x%x -> 0x%x!\n",
		rpdev->src, rpdev->dst);

	i2c_rpmsg_info.rpdev = rpdev;
	mutex_init(&i2c_rpmsg_info.lock);

	init_completion(&i2c_rpmsg_info.cmd_complete);

	return ret;
}

static int i2c_rpmsg_cb(struct rpmsg_device *rpdev, void *data, int len,
			void *priv, u32 src)
{
	struct i2c_rpmsg_msg *msg = (struct i2c_rpmsg_msg *)data;

	dev_dbg(&rpdev->dev, "get from:%d: cmd:%d, resp:%d.\n",
			src, msg->header.cmd, msg->ret_val);

	i2c_rpmsg_info.msg = msg;

	complete(&i2c_rpmsg_info.cmd_complete);

	return 0;
}

static void i2c_rpmsg_remove(struct rpmsg_device *rpdev)
{
	dev_info(&rpdev->dev, "i2c rpmsg driver is removed\n");
}

static struct rpmsg_driver i2c_rpmsg_driver = {
	.drv.name	= "i2c_rpmsg",
	.drv.owner	= THIS_MODULE,
	.id_table	= i2c_rpmsg_id_table,
	.probe		= i2c_rpmsg_probe,
	.callback	= i2c_rpmsg_cb,
	.remove		= i2c_rpmsg_remove,
};

static int i2c_rpmsg_master_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
				int num)
{
	struct i2c_rpmsg_info *info = (struct i2c_rpmsg_info *)adap->algo_data;
	struct i2c_rpmsg_msg req;
	int rc;

	memset(&req, 0, sizeof(req));

	req.bus  = adap->nr;
	switch(num) {
	case 0:
		return 0;
	case 1:
		req.header.cmd = (msgs[0].flags & I2C_M_RD) ?
			I2C_SMBUS_READ: I2C_SMBUS_WRITE;
		req.addr = msgs[0].addr;
		req.len = msgs[0].len - 1;
		if ( req.len == 0 ) {
			return -EOPNOTSUPP;
		}
		req.subAddress = msgs[0].buf[0];
		req.subaddressSize = 1;
		if ( req.header.cmd == I2C_SMBUS_WRITE ) {
			memcpy(req.buf, &msgs[0].buf[1], req.len);
		}
		break;
	case 2:
		req.header.cmd = (msgs[1].flags & I2C_M_RD) ?
			I2C_SMBUS_READ: I2C_SMBUS_WRITE;
		req.addr = msgs[0].addr;
		if ( msgs[0].len < 1 || msgs[0].len > 2 ) {
			return -EOPNOTSUPP;
		}

		req.subAddress = msgs[0].buf[0];
		if ( msgs[0].len == 2 ) {
			req.subAddress |= msgs[0].buf[1] << 8;
		}
		req.subaddressSize = msgs[0].len;
		req.len = msgs[1].len;
		if ( req.header.cmd == I2C_SMBUS_WRITE ) {
			memcpy(req.buf, msgs[1].buf, req.len);
		}
		break;
	default:
		return -EOPNOTSUPP;
	}

	rc = i2c_send_message(&req, info);

	if ( rc || i2c_rpmsg_info.msg->ret_val ) {
		rc = -EINVAL;
	}

	if (!rc && req.header.cmd == I2C_SMBUS_READ ) {
		switch(num) {
		case 0:
			return 0;
		case 1:
			memcpy(msgs[0].buf, i2c_rpmsg_info.msg->buf,
					i2c_rpmsg_info.msg->len);
			break;
		case 2:
			memcpy(msgs[1].buf, i2c_rpmsg_info.msg->buf,
					i2c_rpmsg_info.msg->len);
			break;
		default:
			return -EOPNOTSUPP;
		}
	}
	return rc;
}

static int i2c_rpmsg_smbus_xfer(struct i2c_adapter *adap, u16 addr,
	unsigned short flags, char read_write,
	u8 command, int size, union i2c_smbus_data *data)
{
	struct i2c_rpmsg_info *info = (struct i2c_rpmsg_info *)adap->algo_data;
	struct i2c_rpmsg_msg req;
	int rc;

	memset(&req, 0, sizeof(req));

	req.header.cmd = read_write;
	req.addr = addr;
	req.bus  = adap->nr;
	req.subAddress = command;
	req.subaddressSize = 1;
	switch (size) {
	case I2C_SMBUS_BYTE:
		req.buf[0] = data->byte;
		req.len = 1;
	case I2C_SMBUS_QUICK:
		req.buf[0] = 1;
		req.len = 1;
		break;
	case I2C_SMBUS_BYTE_DATA:
		req.buf[0] = data->byte;
		req.len = 1;
		break;
	case I2C_SMBUS_WORD_DATA:
		if (!read_write) {
			req.buf[0] = data->word & 0xff;
			req.buf[1] = (data->word >> 8) & 0xff;
		}
		req.len = 2;
		break;
	case I2C_SMBUS_I2C_BLOCK_DATA:
		req.len = data->block[0];
		memcpy(req.buf, &data->block[1], req.len);
		break;
	default:
		return -EINVAL;
	}

	rc = i2c_send_message(&req, info);

	if (rc || info->msg->ret_val) {
		rc = -EINVAL;
	}
	if (!rc && read_write ) {
		switch (size) {
			case I2C_SMBUS_BYTE:
				data->byte = info->msg->buf[0];
				break;
			case I2C_SMBUS_QUICK:
				break;
			case I2C_SMBUS_BYTE_DATA:
				data->byte = info->msg->buf[0];
				break;
			case I2C_SMBUS_WORD_DATA:
				data->word = info->msg->buf[1] << 8;
				data->word |= info->msg->buf[0];
				break;
			case I2C_SMBUS_I2C_BLOCK_DATA:
				data->block[0] = info->msg->len;
				memcpy(&data->block[1],
					info->msg->buf,
					info->msg->len);
				break;
			default:
				return -EINVAL;
		}
	}

	return rc;
}

static u32 i2c_rpmsg_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE |
	       I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA |
	       I2C_FUNC_SMBUS_BLOCK_DATA |
	       I2C_FUNC_SMBUS_I2C_BLOCK;
}

static const struct i2c_algorithm i2c_rpmsg_algo = {
	.master_xfer	= i2c_rpmsg_master_xfer,
	.smbus_xfer	= i2c_rpmsg_smbus_xfer,
	.functionality	= i2c_rpmsg_func,
};

static struct i2c_adapter_quirks i2c_rpmsg_quirks = {
	.flags = I2C_AQ_COMB | I2C_AQ_COMB_WRITE_FIRST | I2C_AQ_COMB_SAME_ADDR,
	.max_comb_1st_msg_len = 4,
};

static int sonos_rpmsg_i2c_probe(struct platform_device *pdev)
{
	struct i2c_adapter *adapter;
	int rc = 0;

	adapter = devm_kzalloc(&pdev->dev, sizeof(*adapter), GFP_KERNEL);
	if (!adapter)
		return -ENOMEM;

	platform_set_drvdata(pdev, adapter);

	adapter->algo = &i2c_rpmsg_algo;
	adapter->algo_data = (void *)&i2c_rpmsg_info;
	adapter->quirks = &i2c_rpmsg_quirks;
	adapter->dev.parent = &pdev->dev;
	adapter->dev.of_node = of_node_get(pdev->dev.of_node);
	adapter->nr = of_alias_get_id(adapter->dev.of_node, "i2c");
	snprintf(adapter->name, sizeof(adapter->name), "i2c-rpmsg-%d", adapter->nr);

	rc = i2c_add_adapter(adapter);
	if (rc) {
		dev_err(&pdev->dev, "Failed to register the i2c adapter\n");
		goto error_i2c_device_register;
	}

	return rc;

error_i2c_device_register:
	devm_kfree(&(pdev->dev), adapter);
	return rc;
}

static int sonos_rpmsg_i2c_remove(struct platform_device *pdev)
{
	struct i2c_adapter *adapter = platform_get_drvdata(pdev);

	unregister_rpmsg_driver(&i2c_rpmsg_driver);
	i2c_del_adapter(adapter);
	devm_kfree(&(pdev->dev), adapter);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sonos_rpmsg_i2c_suspend(struct device *dev)
{
	return 0;
}

static int sonos_rpmsg_i2c_suspend_noirq(struct device *dev)
{
	return 0;
}

static int sonos_rpmsg_i2c_resume(struct device *dev)
{
	return 0;
}

static int sonos_rpmsg_i2c_resume_noirq(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops sonos_rpmsg_i2c_pm_ops = {
	.suspend = sonos_rpmsg_i2c_suspend,
	.suspend_noirq = sonos_rpmsg_i2c_suspend_noirq,
	.resume = sonos_rpmsg_i2c_resume,
	.resume_noirq = sonos_rpmsg_i2c_resume_noirq,
};

#define IMX_RPMSG_RTC_PM_OPS	(&sonos_rpmsg_i2c_pm_ops)

#else

#define IMX8_RPMSG_RTC_PM_OPS	NULL

#endif

static const struct of_device_id sonos_rpmsg_i2c_dt_ids[] = {
	{ .compatible = "sonos,imx7ulp-rpmsg-i2c", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sonos_rpmsg_i2c_dt_ids);

static struct platform_driver sonos_rpmsg_i2c_driver = {
	.driver = {
		.name	= "sonos_rpmsg_i2c",
		.pm	= IMX_RPMSG_RTC_PM_OPS,
		.of_match_table = sonos_rpmsg_i2c_dt_ids,
	},
	.probe		= sonos_rpmsg_i2c_probe,
	.remove		= sonos_rpmsg_i2c_remove,
};

static int __init sonos_rpmsg_i2c_driver_init(void)
{
	int ret = 0;

	ret = register_rpmsg_driver(&i2c_rpmsg_driver);
	if (ret < 0)
		return ret;

	ret = platform_driver_register(&sonos_rpmsg_i2c_driver);
	if (ret < 0)
		unregister_rpmsg_driver(&i2c_rpmsg_driver);

	return ret;
}

subsys_initcall(sonos_rpmsg_i2c_driver_init);

MODULE_DESCRIPTION("Sonos RPMSG I2C Driver");
MODULE_LICENSE("GPL");
