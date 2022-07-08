/*
 * Copyright (c) 2018, Sonos, Inc.
 *
 * SPDX-License-Identifier:     GPL-2.0
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#include <linux/module.h>
#include <linux/version.h>
#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/rtc/sonos_rtc.h>

static LIST_HEAD(sonos_rtc_list);

struct sonos_rtc_device_entry {
	struct list_head list;
	struct rtc_hw_ops sonos_rtc_hw_ops;
};

static int __init sonos_rtc_driver_init(void);
static void __exit sonos_rtc_driver_exit(void);

static void lookup_ops(struct rtc_hw_ops **sonos_rtc_hw_ops)
{
	struct sonos_rtc_device_entry *dev_entry;
	*sonos_rtc_hw_ops = NULL;
	list_for_each_entry(dev_entry, &sonos_rtc_list, list) {
		*sonos_rtc_hw_ops = &dev_entry->sonos_rtc_hw_ops;
	}
}

int sonos_rtc_register_ops(struct rtc_hw_ops *sonos_rtc_hw_ops)
{
	struct sonos_rtc_device_entry *dev_entry;
	dev_entry = kzalloc(sizeof(struct sonos_rtc_device_entry), GFP_KERNEL);
	if (!dev_entry) {
		printk(KERN_ERR "sonos-rtc: RTC Driver kzalloc failed\n");
		return -ENOMEM;
	}
	memcpy(&dev_entry->sonos_rtc_hw_ops, sonos_rtc_hw_ops, sizeof(struct rtc_hw_ops));

	list_add_tail(&dev_entry->list, &sonos_rtc_list);
	return 0;
}
EXPORT_SYMBOL(sonos_rtc_register_ops);

void sonos_rtc_unregister_ops(void)
{
	struct sonos_rtc_device_entry *dev_entry, *tmp;

	list_for_each_entry_safe(dev_entry, tmp, &sonos_rtc_list, list)
			list_del(&dev_entry->list);
}
EXPORT_SYMBOL(sonos_rtc_unregister_ops);

/* Alarm read/write functions */
static int sonos_rtc_getalarm(struct device *dev, struct rtc_wkalrm *alm)
{
	int ret;
	struct rtc_hw_ops *sonos_rtc_hw_ops;
	time64_t time;
	lookup_ops(&sonos_rtc_hw_ops);
	if ((sonos_rtc_hw_ops != NULL) && (sonos_rtc_hw_ops->sonos_read_alarm != NULL)) {
		ret = sonos_rtc_hw_ops->sonos_read_alarm(&time);
	} else {
		dev_err(dev, "Failed to call RTC get alarm function\n");
		return -ENOTSUPP;
	}

	rtc_time64_to_tm(time, &alm->time);
	return ret;
}

static int sonos_rtc_setalarm(struct device *dev, struct rtc_wkalrm *alm)
{
	int ret;
	struct rtc_hw_ops *sonos_rtc_hw_ops;
	time64_t time;
	time = rtc_tm_to_time64(&alm->time);
	lookup_ops(&sonos_rtc_hw_ops);
	if ((sonos_rtc_hw_ops != NULL) && (sonos_rtc_hw_ops->sonos_set_alarm != NULL)) {
		ret = sonos_rtc_hw_ops->sonos_set_alarm(time);
	} else {
		dev_err(dev, "Failed to call RTC set alarm function\n");
		return -ENOTSUPP;
	}
	return ret;
}

static int sonos_rtc_irqalarm(struct device *dev, unsigned int enabled)
{
	int ret = 0;
	struct rtc_hw_ops *sonos_rtc_hw_ops;

	/* We use this function only to _disable_ the RTC alarm inteerupt.
	   We do not need to do anything special to _enable_ the RTC alarm interrupt.
	   RTC alarm interrupt is automatically enabled when a non-zero value is written to it. */
	if (!enabled) {
		lookup_ops(&sonos_rtc_hw_ops);
		if ((sonos_rtc_hw_ops != NULL) && (sonos_rtc_hw_ops->sonos_set_alarm != NULL)) {
			/* Writing 0 to the RTC alarm clears the interrupt */
			ret = sonos_rtc_hw_ops->sonos_set_alarm(0);
		} else {
			dev_err(dev, "Failed to clear RTC alarm interrupt\n");
			return -EFAULT;
		}
	}
	return ret;
}

/* Time read/write functions */
static int sonos_rtc_gettime(struct device *dev, struct rtc_time *tm)
{
	int ret;
	struct rtc_hw_ops *sonos_rtc_hw_ops;
	time64_t time;
	lookup_ops(&sonos_rtc_hw_ops);
	if ((sonos_rtc_hw_ops != NULL) && (sonos_rtc_hw_ops->sonos_read_time != NULL)) {
		ret = sonos_rtc_hw_ops->sonos_read_time(&time);
		if (ret) {
			return ret;
		}
	} else {
		dev_err(dev, "Failed to call RTC get time function\n");
		return -ENOTSUPP;
	}

	rtc_time64_to_tm(time, tm);
	return rtc_valid_tm(tm);
}

static int sonos_rtc_settime(struct device *dev, struct rtc_time *tm)
{
	int ret;
	struct rtc_hw_ops *sonos_rtc_hw_ops;
	time64_t time;
	time = rtc_tm_to_time64(tm);
	lookup_ops(&sonos_rtc_hw_ops);
	if ((sonos_rtc_hw_ops != NULL) && (sonos_rtc_hw_ops->sonos_set_time != NULL)) {
		ret = sonos_rtc_hw_ops->sonos_set_time(time);
		if (ret) {
			return ret;
		}
	} else {
		dev_err(dev, "Failed to call RTC set time function\n");
		return -ENOTSUPP;
	}
	return 0;
}

static const struct rtc_class_ops rtc_ops = {
	.read_time	= sonos_rtc_gettime,
	.set_time	= sonos_rtc_settime,
	.read_alarm	= sonos_rtc_getalarm,
	.set_alarm	= sonos_rtc_setalarm,
	.alarm_irq_enable = sonos_rtc_irqalarm,
};


static int sonos_rtc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct rtc_device *rtc = NULL;

	device_init_wakeup(&pdev->dev, 1);
	device_set_wakeup_capable(&pdev->dev, true);
	rtc = rtc_device_register(pdev->name, &pdev->dev, &rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		dev_err(&pdev->dev, "RTC device register failed\n");
		ret = PTR_ERR(&pdev->dev);
		return ret;
	}
	/* Use the Linux UIE (1 Hz periodic interrupt generator) emulator by
	 * indicating we don't support a hardware update interrupt generator. */
	rtc->uie_unsupported = 1;

	/* Initiallize resume counter to 0 */
	rtc->resume_cntr = 0;
	platform_set_drvdata(pdev, rtc);
	return 0;
}


static int sonos_rtc_remove(struct platform_device *pdev)
{
	struct rtc_device *rtc = platform_get_drvdata(pdev);
	rtc_device_unregister(rtc);
	return 0;
}

#ifdef CONFIG_PM
static int sonos_rtc_pm_resume(struct device *dev)
{
	struct rtc_device *rtc = dev_get_drvdata(dev);
	rtc->resume_cntr++;
	return 0;
}

static int sonos_rtc_pm_suspend(struct device *dev)
{
	return 0;
}
#else /* !CONFIG_PM */

#define sonos_rtc_pm_suspend	NULL
#define sonos_rtc_pm_resume	NULL

#endif /* CONFIG_PM */

static const struct of_device_id rtc_dt_ids[] = {
	{ .compatible = "sonos,rtc" },
	{},
};
MODULE_DEVICE_TABLE(of, rtc_dt_ids);

static const struct dev_pm_ops rtc_dev_pm_ops = {
	.suspend = sonos_rtc_pm_suspend,
	.resume = sonos_rtc_pm_resume,
};

static struct platform_driver rtc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sonos-rtc",
		.pm = &rtc_dev_pm_ops,
		.of_match_table = rtc_dt_ids,
		},
	.probe = sonos_rtc_probe,
	.remove = sonos_rtc_remove,
};

static int __init sonos_rtc_driver_init(void)
{
	int ret = 0;
	/* Register RTC */
	ret = platform_driver_register(&rtc_driver);
	if (ret) {
		printk("sonos-rtc: RTC Driver Failed to register platform driver\n");
	}
	return ret;
}

static void __exit sonos_rtc_driver_exit(void)
{
	platform_driver_unregister(&rtc_driver);
}

module_init(sonos_rtc_driver_init);
module_exit(sonos_rtc_driver_exit);

MODULE_AUTHOR("Sonos, Inc.");
MODULE_DESCRIPTION("RTC driver");
MODULE_LICENSE("GPL v2");
