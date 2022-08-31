// SPDX-License-Identifier: GPL-2.0
/* Bluetooth HCI driver model support. */

#include <linux/module.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

static const struct class bt_class = {
	.name = "bluetooth",
};

static void bt_link_release(struct device *dev)
{
	struct hci_conn *conn = to_hci_conn(dev);
	kfree(conn);
}

static const struct device_type bt_link = {
	.name    = "link",
	.release = bt_link_release,
};

void hci_conn_init_sysfs(struct hci_conn *conn)
{
	struct hci_dev *hdev = conn->hdev;

	bt_dev_dbg(hdev, "conn %p", conn);

	conn->dev.type = &bt_link;
	conn->dev.class = &bt_class;
	conn->dev.parent = &hdev->dev;

	device_initialize(&conn->dev);
}

void hci_conn_add_sysfs(struct hci_conn *conn)
{
	struct hci_dev *hdev = conn->hdev;

	bt_dev_dbg(hdev, "conn %p", conn);

	if (device_is_registered(&conn->dev))
		return;

	dev_set_name(&conn->dev, "%s:%d", hdev->name, conn->handle);

	if (device_add(&conn->dev) < 0)
		bt_dev_err(hdev, "failed to register connection device");
}

void hci_conn_del_sysfs(struct hci_conn *conn)
{
	struct hci_dev *hdev = conn->hdev;

	bt_dev_dbg(hdev, "conn %p", conn);

	if (!device_is_registered(&conn->dev)) {
		/* If device_add() has *not* succeeded, use *only* put_device()
		 * to drop the reference count.
		 */
		put_device(&conn->dev);
		return;
	}

	/* If there are devices using the connection as parent reset it to NULL
	 * before unregistering the device.
	 */
	while (1) {
		struct device *dev;

		dev = device_find_any_child(&conn->dev);
		if (!dev)
			break;
		device_move(dev, NULL, DPM_ORDER_DEV_LAST);
		put_device(dev);
	}

	device_unregister(&conn->dev);
}

#ifdef CONFIG_SONOS
static ssize_t show_adv_min(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct hci_dev *hdev = to_hci_dev(dev);
	u64 val;
	hci_dev_lock(hdev);
	val = hdev->le_adv_min_interval;
	hci_dev_unlock(hdev);

	return sprintf(buf, "%llu\n", val);
}

static ssize_t show_adv_max(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct hci_dev *hdev = to_hci_dev(dev);
	u64 val;
	hci_dev_lock(hdev);
	val = hdev->le_adv_max_interval;
	hci_dev_unlock(hdev);

	return sprintf(buf, "%llu\n", val);
}

static ssize_t show_conn_min_interval(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	struct hci_dev *hdev = to_hci_dev(dev);
	u64 val;

	hci_dev_lock(hdev);
	val = hdev->le_conn_min_interval;
	hci_dev_unlock(hdev);

	return sprintf(buf, "%llu\n", val);
}

static ssize_t show_conn_max_interval(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	struct hci_dev *hdev = to_hci_dev(dev);
	u64 val;

	hci_dev_lock(hdev);
	val = hdev->le_conn_max_interval;
	hci_dev_unlock(hdev);

	return sprintf(buf, "%llu\n", val);
}

static ssize_t store_adv_min(struct device *dev,
			    struct device_attribute *attr, const char *buf, size_t count)
{
	struct hci_dev *hdev = to_hci_dev(dev);
	u64 val;

	sscanf(buf, "%llu\n", &val);

	if (val < 0x0020 || val > 0x4000 || val > hdev->le_adv_max_interval)
		return -EINVAL;

	hci_dev_lock(hdev);
	hdev->le_adv_min_interval = val;
	hci_dev_unlock(hdev);

	return count;
}

static ssize_t store_adv_max(struct device *dev,
			    struct device_attribute *attr, const char *buf, size_t count)
{
	struct hci_dev *hdev = to_hci_dev(dev);
	u64 val;

	sscanf(buf, "%llu\n", &val);

	if (val < 0x0020 || val > 0x4000 || val < hdev->le_adv_min_interval)
		return -EINVAL;

	hci_dev_lock(hdev);
	hdev->le_adv_max_interval = val;
	hci_dev_unlock(hdev);

	return count;
}

static ssize_t store_conn_min_interval(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	struct hci_dev *hdev = to_hci_dev(dev);
	u64 val;

	sscanf(buf, "%llu\n", &val);

	if (val < 0x0006 || val > 0x0c80 || val > hdev->le_conn_max_interval)
		return -EINVAL;

	hci_dev_lock(hdev);
	hdev->le_conn_min_interval = val;
	hci_dev_unlock(hdev);

	return count;
}


static ssize_t store_conn_max_interval(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	struct hci_dev *hdev = to_hci_dev(dev);
	u64 val;

	sscanf(buf, "%llu\n", &val);

	if (val < 0x0006 || val > 0x0c80 || val < hdev->le_conn_min_interval)
		return -EINVAL;

	hci_dev_lock(hdev);
	hdev->le_conn_max_interval = val;
	hci_dev_unlock(hdev);

	return count;
}

static DEVICE_ATTR(adv_min_interval, S_IRUGO | S_IWUSR, show_adv_min, store_adv_min);
static DEVICE_ATTR(adv_max_interval, S_IRUGO | S_IWUSR, show_adv_max, store_adv_max);
static DEVICE_ATTR(conn_min_interval, S_IRUGO | S_IWUSR, show_conn_min_interval, store_conn_min_interval);
static DEVICE_ATTR(conn_max_interval, S_IRUGO | S_IWUSR, show_conn_max_interval, store_conn_max_interval);

static struct attribute *bt_host_attrs[] = {
	&dev_attr_adv_min_interval.attr,
	&dev_attr_adv_max_interval.attr,
	&dev_attr_conn_min_interval.attr,
	&dev_attr_conn_max_interval.attr,
	NULL
};

ATTRIBUTE_GROUPS(bt_host);
#endif

static void bt_host_release(struct device *dev)
{
	struct hci_dev *hdev = to_hci_dev(dev);

	if (hci_dev_test_flag(hdev, HCI_UNREGISTER))
		hci_release_dev(hdev);
	else
		kfree(hdev);
	module_put(THIS_MODULE);
}

static const struct device_type bt_host = {
	.name    = "host",
#ifdef CONFIG_SONOS
	.groups = bt_host_groups,
#endif
	.release = bt_host_release,
};

void hci_init_sysfs(struct hci_dev *hdev)
{
	struct device *dev = &hdev->dev;

	dev->type = &bt_host;
	dev->class = &bt_class;

	__module_get(THIS_MODULE);
	device_initialize(dev);
}

int __init bt_sysfs_init(void)
{
	return class_register(&bt_class);
}

void bt_sysfs_cleanup(void)
{
	class_unregister(&bt_class);
}
