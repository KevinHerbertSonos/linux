/*
 * Copyright (c) 2019, Sonos, Inc.
 *
 * SPDX-License-Identifier:     GPL-2.0
 *
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP
 *
 * derived from the omap-rpmsg implementation.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#define  DEVICE_NAME "spirpmsp"
#define  CLASS_NAME  "spirpmsg_class"

#define SPI_MODE_MASK		(SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
				| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
				| SPI_NO_CS | SPI_READY | SPI_TX_DUAL \
				| SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD)

struct spidev_data {
	int			major;
	int			irq;
	struct completion	done;
	int			wait_for_completion;
	int			tx_busy_gpio;
	struct class*		spidev_class;
	struct device*		spidev_device;
	dev_t			devt;
	spinlock_t		spi_lock;
	struct spi_device	*spi;
	struct spi_rpmsg_vproc	*rpdev;
	struct list_head	device_entry;
	struct cdev		chr_dev;

	/* TX/RX buffers are NULL unless this device is open (users > 0) */
	struct mutex		buf_lock;
	unsigned		users;
	u32			buffer_max_size;
	u8			*tx_buffer;
	u8			*rx_buffer;
	u32			speed_hz;
};

struct spi_virdev {
	struct virtio_device vdev;
	unsigned int vring[2];
	struct virtqueue *vq[2];
	int base_vq_id;
	int num_of_vqs;
	u32 vproc_id;
	struct notifier_block nb;
};

struct spi_rpmsg_vproc {
	char *rproc_name;
	struct mutex lock;
	int vdev_nums;
	int first_notify;
	struct spi_virdev *ivdev;
	struct delayed_work rpmsg_work;
	struct blocking_notifier_head notifier;
	u32 in_idx;
	u32 out_idx;
	u32 core_id;
	spinlock_t mu_lock;
	struct spidev_data *spidev;
};

/*
 * For now, allocate 256 buffers of 512 bytes for each side. each buffer
 * will then have 16B for the msg header and 496B for the payload.
 * This will require a total space of 256KB for the buffers themselves, and
 * 3 pages for every vring (the size of the vring depends on the number of
 * buffers it supports).
 */
#define RPMSG_NUM_BUFS		(512)
#define RPMSG_BUF_SIZE		(512)
#define RPMSG_BUFS_SPACE	(RPMSG_NUM_BUFS * RPMSG_BUF_SIZE)

/*
 * The alignment between the consumer and producer parts of the vring.
 * Note: this is part of the "wire" protocol. If you change this, you need
 * to update your BIOS image as well
 */
#define RPMSG_VRING_ALIGN	(4096)

/* With 256 buffers, our vring will occupy 3 pages */
#define RPMSG_RING_SIZE	((DIV_ROUND_UP(vring_size(RPMSG_NUM_BUFS / 2, \
				RPMSG_VRING_ALIGN), PAGE_SIZE)) * PAGE_SIZE)

#define to_spi_virdev(vd) container_of(vd, struct spi_virdev, vdev)

struct spi_rpmsg_vq_info {
	__u16 num;	/* number of entries in the virtio_ring */
	__u16 vq_id;	/* a globaly unique index of this virtqueue */
	void *addr;	/* address where we mapped the virtio ring */
	struct spi_rpmsg_vproc *rpdev;
};

static struct spi_rpmsg_vproc spi_rpmsg_vprocs = {
		.rproc_name	= "spi",
};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static unsigned bufsiz = 4096;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

#define POLLING_INTERVAL  10	/* in msec */
static int spidev_ready(struct spidev_data *spidev, int timeout)
{
	int loopcnt = 0;
	int max_loop;

	max_loop = (timeout + POLLING_INTERVAL - 1) / POLLING_INTERVAL;
	while (loopcnt++ <= max_loop ) {
		if ( !gpio_get_value(spidev->tx_busy_gpio)){
			return 0;
		}
		msleep(POLLING_INTERVAL);
	}
	printk(KERN_INFO "SPI is not ready for read/write in %d\n", timeout);
	return -EFAULT;
}

static ssize_t
spidev_sync(struct spidev_data *spidev, struct spi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status;
	struct spi_device *spi;

	spin_lock_irq(&spidev->spi_lock);
	spi = spidev->spi;
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		status = -ESHUTDOWN;
	else
		status = spi_sync(spi, message);

	if (status == 0)
		status = message->actual_length;

	return status;
}

static inline ssize_t
spidev_sync_write(struct spidev_data *spidev, size_t len)
{
	struct spi_transfer	t = {
			.tx_buf		= spidev->tx_buffer,
			.len		= len,
			.speed_hz	= spidev->speed_hz,
		};
	struct spi_message	m;

	if ( spidev_ready(spidev, 60) ){
		return -EFAULT;
	}

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

static inline ssize_t
spidev_sync_read(struct spidev_data *spidev, size_t len)
{
	struct spi_transfer	t = {
			.rx_buf		= spidev->rx_buffer,
			.len		= len,
			.speed_hz	= spidev->speed_hz,
		};
	struct spi_message	m;

	if ( spidev_ready(spidev, 60) ){
		return -EFAULT;
	}

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

/*-------------------------------------------------------------------------*/

/* Read-only message with current device setup */
static ssize_t
spidev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct spidev_data	*spidev;
	ssize_t			status = 0;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	spidev = filp->private_data;

	mutex_lock(&spidev->buf_lock);
	status = spidev_sync_read(spidev, count);
	if (status > 0) {
		unsigned long	missing;

		missing = copy_to_user(buf, spidev->rx_buffer, status);
		if (missing == status)
			status = -EFAULT;
		else
			status = status - missing;
	}
	mutex_unlock(&spidev->buf_lock);

	return status;
}

/* Write-only message with current device setup */
static ssize_t
spidev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct spidev_data	*spidev;
	ssize_t			status = 0;
	unsigned long		missing;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	spidev = filp->private_data;

	mutex_lock(&spidev->buf_lock);
	missing = copy_from_user(spidev->tx_buffer, buf, count);
	if (missing == 0)
		status = spidev_sync_write(spidev, count);
	else
		status = -EFAULT;
	mutex_unlock(&spidev->buf_lock);

	return status;
}

static int spidev_message(struct spidev_data *spidev,
		struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
	struct spi_message	msg;
	struct spi_transfer	*k_xfers;
	struct spi_transfer	*k_tmp;
	struct spi_ioc_transfer *u_tmp;
	unsigned		n, total, tx_total, rx_total;
	u8			*tx_buf, *rx_buf;
	int			status = -EFAULT;

	spi_message_init(&msg);
	k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
	if (k_xfers == NULL)
		return -ENOMEM;

	/* Construct spi_message, copying any tx data to bounce buffer.
	 * We walk the array of user-provided transfers, using each one
	 * to initialize a kernel version of the same transfer.
	 */
	tx_buf = spidev->tx_buffer;
	rx_buf = spidev->rx_buffer;
	total = 0;
	tx_total = 0;
	rx_total = 0;
	for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
			n;
			n--, k_tmp++, u_tmp++) {
		k_tmp->len = u_tmp->len;

		total += k_tmp->len;
		/* Since the function returns the total length of transfers
		 * on success, restrict the total to positive int values to
		 * avoid the return value looking like an error.  Also check
		 * each transfer length to avoid arithmetic overflow.
		 */
		if (total > INT_MAX || k_tmp->len > INT_MAX) {
			status = -EMSGSIZE;
			goto done;
		}

		if (u_tmp->rx_buf) {
			/* this transfer needs space in RX bounce buffer */
			rx_total += k_tmp->len;
			if (rx_total > bufsiz) {
				status = -EMSGSIZE;
				goto done;
			}
			k_tmp->rx_buf = rx_buf;
			if (!access_ok(VERIFY_WRITE, (u8 __user *)
						(uintptr_t) u_tmp->rx_buf,
						u_tmp->len))
				goto done;
			rx_buf += k_tmp->len;
		}
		if (u_tmp->tx_buf) {
			/* this transfer needs space in TX bounce buffer */
			tx_total += k_tmp->len;
			if (tx_total > bufsiz) {
				status = -EMSGSIZE;
				goto done;
			}
			k_tmp->tx_buf = tx_buf;
			if (copy_from_user(tx_buf, (const u8 __user *)
						(uintptr_t) u_tmp->tx_buf,
					u_tmp->len))
				goto done;
			tx_buf += k_tmp->len;
		}

		k_tmp->cs_change = !!u_tmp->cs_change;
		k_tmp->tx_nbits = u_tmp->tx_nbits;
		k_tmp->rx_nbits = u_tmp->rx_nbits;
		k_tmp->bits_per_word = u_tmp->bits_per_word;
		k_tmp->delay_usecs = u_tmp->delay_usecs;
		k_tmp->speed_hz = u_tmp->speed_hz;
		if (!k_tmp->speed_hz)
			k_tmp->speed_hz = spidev->speed_hz;
#ifdef VERBOSE
		dev_dbg(&spidev->spi->dev,
			"  xfer len %u %s%s%s%dbits %u usec %uHz\n",
			u_tmp->len,
			u_tmp->rx_buf ? "rx " : "",
			u_tmp->tx_buf ? "tx " : "",
			u_tmp->cs_change ? "cs " : "",
			u_tmp->bits_per_word ? : spidev->spi->bits_per_word,
			u_tmp->delay_usecs,
			u_tmp->speed_hz ? : spidev->spi->max_speed_hz);
#endif
		spi_message_add_tail(k_tmp, &msg);
	}

	status = spidev_sync(spidev, &msg);
	if (status < 0)
		goto done;

	/* copy any rx data out of bounce buffer */
	rx_buf = spidev->rx_buffer;
	for (n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++) {
		if (u_tmp->rx_buf) {
			if (__copy_to_user((u8 __user *)
					(uintptr_t) u_tmp->rx_buf, rx_buf,
					u_tmp->len)) {
				status = -EFAULT;
				goto done;
			}
			rx_buf += u_tmp->len;
		}
	}
	status = total;

done:
	kfree(k_xfers);
	return status;
}

static struct spi_ioc_transfer *
spidev_get_ioc_message(unsigned int cmd, struct spi_ioc_transfer __user *u_ioc,
		unsigned *n_ioc)
{
	struct spi_ioc_transfer	*ioc;
	u32	tmp;

	/* Check type, command number and direction */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC
			|| _IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0))
			|| _IOC_DIR(cmd) != _IOC_WRITE)
		return ERR_PTR(-ENOTTY);

	tmp = _IOC_SIZE(cmd);
	if ((tmp % sizeof(struct spi_ioc_transfer)) != 0)
		return ERR_PTR(-EINVAL);
	*n_ioc = tmp / sizeof(struct spi_ioc_transfer);
	if (*n_ioc == 0)
		return NULL;

	/* copy into scratch area */
	ioc = kmalloc(tmp, GFP_KERNEL);
	if (!ioc)
		return ERR_PTR(-ENOMEM);
	if (__copy_from_user(ioc, u_ioc, tmp)) {
		kfree(ioc);
		return ERR_PTR(-EFAULT);
	}
	return ioc;
}

static long
spidev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int			err = 0;
	int			retval = 0;
	struct spidev_data	*spidev;
	struct spi_device	*spi;
	u32			tmp;
	unsigned		n_ioc;
	struct spi_ioc_transfer	*ioc;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
		return -ENOTTY;

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	spidev = filp->private_data;
	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;

	/* use the buffer lock here for triple duty:
	 *  - prevent I/O (from us) so calling spi_setup() is safe;
	 *  - prevent concurrent SPI_IOC_WR_* from morphing
	 *    data fields while SPI_IOC_RD_* reads them;
	 *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
	 */
	mutex_lock(&spidev->buf_lock);

	switch (cmd) {
	/* read requests */
	case SPI_IOC_RD_MODE:
		retval = __put_user(spi->mode & SPI_MODE_MASK,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_MODE32:
		retval = __put_user(spi->mode & SPI_MODE_MASK,
					(__u32 __user *)arg);
		break;
	case SPI_IOC_RD_LSB_FIRST:
		retval = __put_user((spi->mode & SPI_LSB_FIRST) ?  1 : 0,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_BITS_PER_WORD:
		retval = __put_user(spi->bits_per_word, (__u8 __user *)arg);
		break;
	case SPI_IOC_RD_MAX_SPEED_HZ:
		retval = __put_user(spidev->speed_hz, (__u32 __user *)arg);
		break;

	/* write requests */
	case SPI_IOC_WR_MODE:
	case SPI_IOC_WR_MODE32:
		if (cmd == SPI_IOC_WR_MODE)
			retval = __get_user(tmp, (u8 __user *)arg);
		else
			retval = __get_user(tmp, (u32 __user *)arg);
		if (retval == 0) {
			u32	save = spi->mode;

			if (tmp & ~SPI_MODE_MASK) {
				retval = -EINVAL;
				break;
			}

			tmp |= spi->mode & ~SPI_MODE_MASK;
			spi->mode = (u16)tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "spi mode %x\n", tmp);
		}
		break;
	case SPI_IOC_WR_LSB_FIRST:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u32	save = spi->mode;

			if (tmp)
				spi->mode |= SPI_LSB_FIRST;
			else
				spi->mode &= ~SPI_LSB_FIRST;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "%csb first\n",
						tmp ? 'l' : 'm');
		}
		break;
	case SPI_IOC_WR_BITS_PER_WORD:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->bits_per_word;

			spi->bits_per_word = tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->bits_per_word = save;
			else
				dev_dbg(&spi->dev, "%d bits per word\n", tmp);
		}
		break;
	case SPI_IOC_WR_MAX_SPEED_HZ:
		retval = __get_user(tmp, (__u32 __user *)arg);
		if (retval == 0) {
			u32	save = spi->max_speed_hz;

			spi->max_speed_hz = tmp;
			retval = spi_setup(spi);
			if (retval >= 0)
				spidev->speed_hz = tmp;
			else
				dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
			spi->max_speed_hz = save;
		}
		break;

	default:
		/* segmented and/or full-duplex I/O request */
		/* Check message and copy into scratch area */
		ioc = spidev_get_ioc_message(cmd,
				(struct spi_ioc_transfer __user *)arg, &n_ioc);
		if (IS_ERR(ioc)) {
			retval = PTR_ERR(ioc);
			break;
		}
		if (!ioc)
			break;	/* n_ioc is also 0 */

		/* translate to spi_message, execute */
		retval = spidev_message(spidev, ioc, n_ioc);
		kfree(ioc);
		break;
	}

	mutex_unlock(&spidev->buf_lock);
	spi_dev_put(spi);
	return retval;
}

#ifdef CONFIG_COMPAT
static long
spidev_compat_ioc_message(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct spi_ioc_transfer __user	*u_ioc;
	int				retval = 0;
	struct spidev_data		*spidev;
	struct spi_device		*spi;
	unsigned			n_ioc, n;
	struct spi_ioc_transfer		*ioc;

	u_ioc = (struct spi_ioc_transfer __user *) compat_ptr(arg);
	if (!access_ok(VERIFY_READ, u_ioc, _IOC_SIZE(cmd)))
		return -EFAULT;

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	spidev = filp->private_data;
	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;

	/* SPI_IOC_MESSAGE needs the buffer locked "normally" */
	mutex_lock(&spidev->buf_lock);

	/* Check message and copy into scratch area */
	ioc = spidev_get_ioc_message(cmd, u_ioc, &n_ioc);
	if (IS_ERR(ioc)) {
		retval = PTR_ERR(ioc);
		goto done;
	}
	if (!ioc)
		goto done;	/* n_ioc is also 0 */

	/* Convert buffer pointers */
	for (n = 0; n < n_ioc; n++) {
		ioc[n].rx_buf = (uintptr_t) compat_ptr(ioc[n].rx_buf);
		ioc[n].tx_buf = (uintptr_t) compat_ptr(ioc[n].tx_buf);
	}

	/* translate to spi_message, execute */
	retval = spidev_message(spidev, ioc, n_ioc);
	kfree(ioc);

done:
	mutex_unlock(&spidev->buf_lock);
	spi_dev_put(spi);
	return retval;
}

static long
spidev_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) == SPI_IOC_MAGIC
			&& _IOC_NR(cmd) == _IOC_NR(SPI_IOC_MESSAGE(0))
			&& _IOC_DIR(cmd) == _IOC_WRITE)
		return spidev_compat_ioc_message(filp, cmd, arg);

	return spidev_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define spidev_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int spidev_open(struct inode *inode, struct file *filp)
{
	struct spidev_data *spidev;
	int status = 0;

	mutex_lock(&device_list_lock);

	list_for_each_entry(spidev, &device_list, device_entry) {
		if (spidev->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}

	if (status) {
		pr_debug("spidev: nothing for minor %d\n", iminor(inode));
		goto err_find_dev;
	}

	spidev->users++;
	filp->private_data = spidev;
	nonseekable_open(inode, filp);
	mutex_unlock(&device_list_lock);
	return status;

err_find_dev:
	mutex_unlock(&device_list_lock);
	return status;
}

static int spidev_release(struct inode *inode, struct file *filp)
{
	struct spidev_data	*spidev;

	mutex_lock(&device_list_lock);
	spidev = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	spidev->users--;
	if (!spidev->users) {
		spin_lock_irq(&spidev->spi_lock);
		if (spidev->spi)
			spidev->speed_hz = spidev->spi->max_speed_hz;

		/* ... after we unbound from the underlying device? */
		spin_unlock_irq(&spidev->spi_lock);

	}
	mutex_unlock(&device_list_lock);
	return 0;
}

static struct file_operations spidev_fops =
{
	.owner =	THIS_MODULE,
	.write =	spidev_write,
	.read =		spidev_read,
	.unlocked_ioctl = spidev_ioctl,
	.compat_ioctl = spidev_compat_ioctl,
	.open =		spidev_open,
	.release =	spidev_release,
	.llseek =	no_llseek,
};

void get_xmit_rpmsg(u8 *data);
#define MAX_SPI_BUFFER_LEN	64

static void spi_first_message(struct spi_rpmsg_vproc *rpdev, uint32_t type)
{
	struct spidev_data *spidev = rpdev->spidev;
	memset(spidev->tx_buffer, 0, spidev->buffer_max_size);
	get_xmit_rpmsg(spidev->tx_buffer);
	spidev_sync_write(spidev, spidev->buffer_max_size);
}

void *get_a_tx_buf(struct virtqueue *vq)
{
	struct spi_rpmsg_vq_info *rpvq = vq->priv;
	struct spi_rpmsg_vproc *rpdev = rpvq->rpdev;
	struct spidev_data *spidev = rpdev->spidev;
	memset(spidev->tx_buffer, 0, spidev->buffer_max_size);
	return spidev->tx_buffer;
}

static irqreturn_t spi_mu_rpmsg_isr(int irq, void *param)
{
	struct spi_rpmsg_vproc *rpdev;
	struct spidev_data *spidev = (struct spidev_data *)param;
	unsigned long flags;
	rpdev = spidev->rpdev;

	if ( rpdev && spidev->irq == irq ) {
		spin_lock_irqsave(&rpdev->mu_lock, flags);
		rpdev->in_idx++;
		if (rpdev->in_idx == rpdev->out_idx) {
			spin_unlock_irqrestore(&rpdev->mu_lock, flags);
			pr_err("SPI overflow!\n");
			return IRQ_HANDLED;
		}
		spin_unlock_irqrestore(&rpdev->mu_lock, flags);
		schedule_delayed_work(&(rpdev->rpmsg_work), 0);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

void *debug_rx_packet(struct virtqueue *vq)
{
	struct spi_rpmsg_vq_info *rpvq = vq->priv;
	struct spi_rpmsg_vproc *rpdev = rpvq->rpdev;
	struct spidev_data *spidev = rpdev->spidev;
	int i;
	int valid_data = 0;
	char *slaveRxData = (char *)spidev->rx_buffer;

	for (i = 0; i < spidev->buffer_max_size; i++) {
		if( slaveRxData[i] ) {
			valid_data = 1;
			break;
		}
	}
	if ( valid_data == 0 )
		return NULL;

	return spidev->rx_buffer;
}

void send_spi_spmsg(struct virtqueue *vq)
{
	struct spi_rpmsg_vq_info *rpvq = vq->priv;
	struct spi_rpmsg_vproc *rpdev = rpvq->rpdev;
	struct spidev_data *spidev = rpdev->spidev;
	ssize_t	status = 0;

	reinit_completion(&spidev->done);
	spidev->wait_for_completion = 1;
	status = spidev_sync_write(spidev, spidev->buffer_max_size);
	status = wait_for_completion_timeout(&spidev->done, msecs_to_jiffies(300));
	spidev->wait_for_completion = 0;
}

void *spi_get_message(struct virtqueue *vq, int *len)
{
	struct spi_rpmsg_vq_info *rpvq = vq->priv;
	struct spi_rpmsg_vproc *rpdev = rpvq->rpdev;
	struct spidev_data *spidev = rpdev->spidev;
	ssize_t	status = 0;
	void *ret;
	status = spidev_sync_read(spidev, spidev->buffer_max_size);
	*len = spidev->buffer_max_size;
	if (spidev->wait_for_completion) {
		complete(&spidev->done);
	}
	ret = debug_rx_packet(vq);
	return ret;
}

static u64 spi_rpmsg_get_features(struct virtio_device *vdev)
{
	/* VIRTIO_RPMSG_F_NS has been made private */
	return 1 << 0;
}

static int spi_rpmsg_finalize_features(struct virtio_device *vdev)
{
	/* Give virtio_ring a chance to accept features */
	vring_transport_features(vdev);
	return 0;
}

/* kick the remote processor, and let it know which virtqueue to poke at */
static bool spi_rpmsg_notify(struct virtqueue *vq)
{
	unsigned int mu_rpmsg = 0;
	struct spi_rpmsg_vq_info *rpvq = vq->priv;

	mu_rpmsg = rpvq->vq_id << 16;
	mutex_lock(&rpvq->rpdev->lock);
	if (unlikely(rpvq->rpdev->first_notify > 0)) {
		rpvq->rpdev->first_notify--;
		spi_first_message(rpvq->rpdev, mu_rpmsg);
		/* wait for all announcement from M33 */
		mdelay(300);
	} else {
		struct rpmsg_hdr *msg;
		void *ret = NULL;
		unsigned int len;
		uint16_t idx;
		ret = virtqueue_get_tx_buf_spi(vq, &idx, &len);
		while (ret != NULL) {
			msg = get_a_tx_buf(vq);
			memcpy(msg, ret, len);
			send_spi_spmsg(vq);
			virtqueue_add_consumed_tx_buffer(vq, idx, len);
			ret = virtqueue_get_tx_buf_spi(vq, &idx, &len);
		}
	}
	mutex_unlock(&rpvq->rpdev->lock);

	return true;
}

static int spi_mu_rpmsg_callback(struct notifier_block *this,
					unsigned long index, void *data)
{
	struct spi_virdev *virdev;

	virdev = container_of(this, struct spi_virdev, nb);

	update_last_used_idx(virdev->vq[0]);

	vring_interrupt(0, virdev->vq[0]);

	return NOTIFY_DONE;
}

static int spi_mu_rpmsg_register_nb(struct spi_rpmsg_vproc *rpdev,
		struct notifier_block *nb)
{
	if ((rpdev == NULL) || (nb == NULL))
		return -EINVAL;

	blocking_notifier_chain_register(&(rpdev->notifier), nb);

	return 0;
}

static int spi_mu_rpmsg_unregister_nb(struct spi_rpmsg_vproc *rpdev,
		struct notifier_block *nb)
{
	if ((rpdev == NULL) || (nb == NULL))
		return -EINVAL;

	blocking_notifier_chain_unregister(&(rpdev->notifier), nb);

	return 0;
}

static struct virtqueue *rp_find_vq(struct virtio_device *vdev,
				    unsigned int index,
				    void (*callback)(struct virtqueue *vq),
				    const char *name)
{
	struct spi_virdev *virdev = to_spi_virdev(vdev);
	struct spi_rpmsg_vproc *rpdev = &spi_rpmsg_vprocs;
	struct spi_rpmsg_vq_info *rpvq;
	struct virtqueue *vq;
	int err;

	rpvq = kmalloc(sizeof(*rpvq), GFP_KERNEL);
	if (!rpvq)
		return ERR_PTR(-ENOMEM);

	/* ioremap'ing normal memory, so we cast away sparse's complaints */
	rpvq->addr = (__force void *)virdev->vring[index];
	if (!rpvq->addr) {
		err = -ENOMEM;
		goto free_rpvq;
	}

	memset(rpvq->addr, 0, RPMSG_RING_SIZE);

	pr_debug("vring%d: phys 0x%x, virt 0x%p\n", index, virdev->vring[index],
					rpvq->addr);

	vq = vring_new_virtqueue(index, RPMSG_NUM_BUFS / 2, RPMSG_VRING_ALIGN,
			vdev, true, rpvq->addr, spi_rpmsg_notify, callback,
			name);
	if (!vq) {
		pr_err("vring_new_virtqueue failed\n");
		err = -ENOMEM;
		goto unmap_vring;
	}

	virdev->vq[index] = vq;
	vq->priv = rpvq;
	/* system-wide unique id for this virtqueue */
	rpvq->vq_id = virdev->base_vq_id + index;
	rpvq->rpdev = rpdev;
	mutex_init(&rpdev->lock);

	return vq;

unmap_vring:
free_rpvq:
	kfree(rpvq);
	return ERR_PTR(err);
}

static void spi_rpmsg_del_vqs(struct virtio_device *vdev)
{
	struct virtqueue *vq, *n;
	struct spi_virdev *virdev = to_spi_virdev(vdev);
	struct spi_rpmsg_vproc *rpdev = &spi_rpmsg_vprocs;

	list_for_each_entry_safe(vq, n, &vdev->vqs, list) {
		struct spi_rpmsg_vq_info *rpvq = vq->priv;
		vring_del_virtqueue(vq);
		kfree(rpvq);
	}

	if (&virdev->nb)
		spi_mu_rpmsg_unregister_nb(rpdev, &virdev->nb);
}

static int spi_rpmsg_find_vqs(struct virtio_device *vdev, unsigned int nvqs,
		       struct virtqueue *vqs[],
		       vq_callback_t *callbacks[],
		       const char * const names[])
{
	struct spi_virdev *virdev = to_spi_virdev(vdev);
	struct spi_rpmsg_vproc *rpdev = &spi_rpmsg_vprocs; 
	int i, err;

	if (nvqs != 2)
		return -EINVAL;

	for (i = 0; i < nvqs; ++i) {
		vqs[i] = rp_find_vq(vdev, i, callbacks[i], names[i]);
		if (IS_ERR(vqs[i])) {
			err = PTR_ERR(vqs[i]);
			goto error;
		}
	}

	virdev->num_of_vqs = nvqs;

	virdev->nb.notifier_call = spi_mu_rpmsg_callback;
	spi_mu_rpmsg_register_nb(rpdev, &virdev->nb);

	return 0;

error:
	spi_rpmsg_del_vqs(vdev);
	return err;
}

static void spi_rpmsg_reset(struct virtio_device *vdev)
{
	dev_dbg(&vdev->dev, "reset !\n");
}

static u8 spi_rpmsg_get_status(struct virtio_device *vdev)
{
	return 0;
}

static void spi_rpmsg_set_status(struct virtio_device *vdev, u8 status)
{
	dev_dbg(&vdev->dev, "%s new status: %d\n", __func__, status);
}

static void spi_rpmsg_vproc_release(struct device *dev)
{
	/* this handler is provided so driver core doesn't yell at us */
}

static struct virtio_config_ops spi_rpmsg_config_ops = {
	.get_features	= spi_rpmsg_get_features,
	.finalize_features = spi_rpmsg_finalize_features,
	.find_vqs	= spi_rpmsg_find_vqs,
	.del_vqs	= spi_rpmsg_del_vqs,
	.reset		= spi_rpmsg_reset,
	.set_status	= spi_rpmsg_set_status,
	.get_status	= spi_rpmsg_get_status,
};

static int set_vring_phy_buf(struct spi_rpmsg_vproc *rpdev, int vdev_nums)
{
	struct resource res1;
	struct resource *res = &res1;
	resource_size_t size;
	unsigned int start, end;
	int ret = 0;

	size = 0x10000;
	res->start = (unsigned int)kmalloc(size, GFP_KERNEL);
	if (!res->start) {
		return -ENOMEM;
	}
	rpdev->ivdev = kzalloc(sizeof(struct spi_virdev),
						GFP_KERNEL);
	if (!rpdev->ivdev) {
		kfree((void *)res->start);
		return -ENOMEM;
	}

	start = res->start;
	end = res->start + size;

	rpdev->ivdev->vring[0] = start;
	rpdev->ivdev->vring[1] = start + 0x8000;
	start += 0x10000;
	if (start > end) {
		pr_err("Too small memory size %x!\n",
				(u32)size);
		ret = -EINVAL;
	}

	return ret;
}

static void rpmsg_work_handler(struct work_struct *work)
{
	unsigned long flags;
	struct delayed_work *dwork = to_delayed_work(work);
	struct spi_rpmsg_vproc *rpdev = container_of(dwork,
			struct spi_rpmsg_vproc, rpmsg_work);

	spin_lock_irqsave(&rpdev->mu_lock, flags);
	if (rpdev->in_idx != rpdev->out_idx) {
		spin_unlock_irqrestore(&rpdev->mu_lock, flags);
		blocking_notifier_call_chain(&(rpdev->notifier), 4,
			(void *)(phys_addr_t)0);
		spin_lock_irqsave(&rpdev->mu_lock, flags);
		rpdev->out_idx++;
	} else {
		pr_err("SPI overflow!\n");
	}
	spin_unlock_irqrestore(&rpdev->mu_lock, flags);
}

static int spi_rpmsg_probe(struct spidev_data *spidev)
{
	int core_id, ret = 0;
	struct spi_rpmsg_vproc *rpdev;

	core_id = 0;
	rpdev = &spi_rpmsg_vprocs;
	rpdev->core_id = core_id;
	spidev->rpdev = rpdev;

	/* Initialize the mu unit used by rpmsg */
	spin_lock_init(&rpdev->mu_lock);

	INIT_DELAYED_WORK(&(rpdev->rpmsg_work), rpmsg_work_handler);
	BLOCKING_INIT_NOTIFIER_HEAD(&(rpdev->notifier));

	pr_info("SPI is ready for cross core communication!\n");

	rpdev->vdev_nums = 1;
	rpdev->first_notify = rpdev->vdev_nums;

	if (!spidev->tx_buffer) {
		spidev->tx_buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!spidev->tx_buffer) {
			dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
			ret = -ENOMEM;
			goto err_find_dev;
		}
	}

	if (!spidev->rx_buffer) {
		spidev->rx_buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!spidev->rx_buffer) {
			dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
			ret = -ENOMEM;
			goto err_alloc_rx_buf;
		}
	}

	if (!strcmp(rpdev->rproc_name, "spi")) {
		ret = set_vring_phy_buf(rpdev,
					rpdev->vdev_nums);
		if (ret) {
			pr_err("No vring buffer.\n");
			ret = -ENOMEM;
			goto vdev_err_out;
		}
	} else {
		pr_err("No remote m4 processor.\n");
		ret = -ENODEV;
		goto vdev_err_out;
	}

	pr_debug("%s rpdev%d vdev%d: vring0 0x%x, vring1 0x%x\n",
			 __func__, rpdev->core_id, rpdev->vdev_nums,
			 rpdev->ivdev->vring[0],
			 rpdev->ivdev->vring[1]);
	rpdev->ivdev->vdev.id.device = VIRTIO_ID_RPMSG_SPI;
	rpdev->ivdev->vdev.config = &spi_rpmsg_config_ops;
	rpdev->spidev = spidev;
	rpdev->ivdev->vdev.dev.parent = NULL;
	rpdev->ivdev->vdev.dev.release = spi_rpmsg_vproc_release;
	rpdev->ivdev->base_vq_id = 0;
	rpdev->ivdev->vproc_id = rpdev->core_id;

	ret = register_virtio_device(&rpdev->ivdev->vdev);
	if (ret) {
		pr_err("%s failed to register rpdev: %d\n",
					__func__, ret);
		goto err_out;
	}
	return ret;

err_out:
vdev_err_out:
	kfree(spidev->rx_buffer);
	spidev->rx_buffer = NULL;
err_alloc_rx_buf:
	kfree(spidev->tx_buffer);
	spidev->tx_buffer = NULL;
err_find_dev:
	return ret;
}

static void spi_rpmsg_exit(struct spidev_data *spidev)
{
}

#ifdef CONFIG_OF
static const struct of_device_id spidev_dt_ids[] = {
	{ .compatible = "sonos,spi-rpmsg" },
	{},
};
MODULE_DEVICE_TABLE(of, spidev_dt_ids);
#endif

static int spidev_probe(struct spi_device *spi)
{
	struct spidev_data	*spidev;
	int			status = 0;
	struct device_node *np = spi->dev.of_node;
	u32 value;

	spidev = kzalloc(sizeof(*spidev), GFP_KERNEL);
	if (!spidev)
		return -ENOMEM;

	spidev->spi = spi;
	spin_lock_init(&spidev->spi_lock);
	mutex_init(&spidev->buf_lock);

	INIT_LIST_HEAD(&spidev->device_entry);

	spidev->irq = gpio_to_irq(of_get_named_gpio(np, "interrupts", 0));
	status = request_irq(spidev->irq, spi_mu_rpmsg_isr,
			IRQF_TRIGGER_FALLING,  "spi-mu-rpmsg", spidev);

	if (status) {
		pr_err("%s: register interrupt %d failed, rc %d\n",
				__func__, spidev->irq, status);
			return status;
	}

	spidev->tx_busy_gpio = of_get_named_gpio(np, "tx-busy-gpio", 0);
	if (gpio_is_valid(spidev->tx_busy_gpio)) {
		int ret = gpio_request(spidev->tx_busy_gpio, "tx-busy");
		if (ret) {
			dev_err(&spi->dev, "failed to request gpio %d\n",
					spidev->tx_busy_gpio);
			return ret;
		}
	}

	spidev->buffer_max_size = MAX_SPI_BUFFER_LEN;
	if ( !of_property_read_u32(np, "buffer-max-size", &value)) {
		spidev->buffer_max_size = value;
	}

	spidev->major = register_chrdev(0, DEVICE_NAME, &spidev_fops);
	if ( spidev->major < 0 ){
		printk(KERN_ALERT "%s failed to register a major number\n", DEVICE_NAME);
		status = spidev->major;
		goto major_err;
	}
	spidev->devt = MKDEV(spidev->major, 0);

	spidev->spidev_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(spidev->spidev_class)){
		pr_err("Failed to register device class\n");
		status = PTR_ERR(spidev->spidev_class);
		goto class_err;
	}

	spidev->spidev_device = device_create(spidev->spidev_class, NULL,
		spidev->devt, NULL, DEVICE_NAME);
	if (IS_ERR(spidev->spidev_device)){
		pr_err("Failed to create the device\n");
		status = PTR_ERR(spidev->spidev_device);
		goto device_err;
	}

	mutex_lock(&device_list_lock);
	list_add(&spidev->device_entry, &device_list);
	mutex_unlock(&device_list_lock);
	spidev->speed_hz = spi->max_speed_hz;

	if (status == 0)
		spi_set_drvdata(spi, spidev);
	else
		kfree(spidev);

	status = spi_rpmsg_probe(spidev);

	if ( status ) {
		goto rpmsg_err;
	}

	init_completion(&spidev->done);
	spidev->wait_for_completion = 0 ;
	return status;

rpmsg_err:
	spin_lock_irq(&spidev->spi_lock);
	spidev->spi = NULL;
	spin_unlock_irq(&spidev->spi_lock);

	mutex_lock(&device_list_lock);
	list_del(&spidev->device_entry);
	mutex_unlock(&device_list_lock);
	device_destroy(spidev->spidev_class, spidev->devt);

device_err:
	class_destroy(spidev->spidev_class);
	class_unregister(spidev->spidev_class);
class_err:
	unregister_chrdev(spidev->major, DEVICE_NAME);
major_err:
	kfree(spidev);
	return status;
}

static int spidev_remove(struct spi_device *spi)
{
	struct spidev_data *spidev = spi_get_drvdata(spi);

	spi_rpmsg_exit(spidev);

	spin_lock_irq(&spidev->spi_lock);
	spidev->spi = NULL;
	spin_unlock_irq(&spidev->spi_lock);

	mutex_lock(&device_list_lock);
	list_del(&spidev->device_entry);
	mutex_unlock(&device_list_lock);
	device_destroy(spidev->spidev_class, spidev->devt);
	class_unregister(spidev->spidev_class);
	class_destroy(spidev->spidev_class);
	unregister_chrdev(spidev->major, DEVICE_NAME);
	kfree(spidev);
	return 0;
}

static struct spi_driver spidev_spi_driver = {
	.driver = {
		.name =		"spidev",
		.of_match_table = of_match_ptr(spidev_dt_ids),
	},
	.probe =	spidev_probe,
	.remove =	spidev_remove,
};

int __init spidev_init(void)
{
	int status;

	status = spi_register_driver(&spidev_spi_driver);
	return status;
}

static void __exit spidev_exit(void)
{
	spi_unregister_driver(&spidev_spi_driver);
}
module_init(spidev_init);
module_exit(spidev_exit);

MODULE_AUTHOR("Andrea Paterniani, <a.paterniani@swapp-eng.it>");
MODULE_DESCRIPTION("User mode SPI device interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:spidev");
