/*
 * Copyright (c) 2015-2021, Sonos, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 * linux/fs/proc/sonos_lock.c
 *
 * /proc entry support to allow control over unlock authorization
 * functionality on secured Sonos products.
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include "mdp.h"
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include "sonos_rollback.h"
// sonos_lock.h needs to know whether this is
// is the source file...
#define SONOS_LOCK_C
#include "sonos_lock.h"
#ifdef CONFIG_SONOS_SECBOOT
#include <sonos/firmware_allowlist.h>
#include <linux/sonos_sec_lock.h>
#endif

/*
 * Proc files to view and modify three toggles that control whether we are
 * still allowed to:
 *	- load modules (insmod)?
 *	- mount a file system without nodev (mount_dev)?
 *	- mount a file system without noexec (mount_exec)?
 *
 * These are one-way variables: once false they will never go back to
 * true (proc file representation is 0 and 1 respectively).
 *
 * We don't currently expose an in-kernel way to flip these (not needed);
 * just the proc file interface (write "0" to the file to flip it).
 * Units with the corresponding DevUnlock permission will not flip these,
 * even if you write "0" to the proc file. This greatly simplifies the
 * shell scripts in charge of flipping these since they don't need to know
 * whether or not units have real certs or DevUnlock permissions (they just
 * write "0" to the proc files at the right time during boot regardless).
 */

static int proc_show_allow(struct seq_file *m, void *v);
static ssize_t proc_write_allow(struct file *file, const char __user * buf,
				size_t length, loff_t *offset);

#define IMPLEMENT_ALLOW_PROCFILE(s)					\
									\
static bool allow_##s = true;						\
									\
bool sonos_allow_##s(void)						\
{									\
	return allow_##s;						\
}									\
EXPORT_SYMBOL(sonos_allow_##s);						\
									\
static int proc_open_allow_##s(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, proc_show_allow, &allow_##s);		\
}									\
									\
static const struct file_operations allow_##s##_proc_fops = {		\
	.open		= proc_open_allow_##s,				\
	.read		= seq_read,					\
	.write		= proc_write_allow,				\
	.llseek		= seq_lseek,					\
	.release	= single_release,				\
};

IMPLEMENT_ALLOW_PROCFILE(insmod)
IMPLEMENT_ALLOW_PROCFILE(mount_dev)
IMPLEMENT_ALLOW_PROCFILE(mount_exec)

static int proc_show_allow(struct seq_file *m, void *v)
{
	bool *allow_flag = m->private;
	seq_printf(m, "%d\n", *allow_flag ? 1 : 0);
	return 0;
}

struct procfile_devunlock_map_entry {
	/* 'fops' is used to figure out which proc file is calling us */
	const struct file_operations *fops;
	const char *file_name;
	bool *allow_flag;
	u32 devunlock_perm;
};

#define MAP_ENTRY(s, perm) { &allow_##s##_proc_fops, "allow_" #s, &allow_##s, perm }
static const struct procfile_devunlock_map_entry devunlock_map[] =
{
	MAP_ENTRY(insmod, MDP_AUTH_FLAG_INSMOD_CTRL),
	MAP_ENTRY(mount_dev, MDP_AUTH_FLAG_NODEV_CTRL),
	MAP_ENTRY(mount_exec, MDP_AUTH_FLAG_EXEC_ENABLE),
};

/*
 * Writing "0" to the file flips the bit unless DevUnlock is set to
 * avoid the restriction in question. We ignore any other writes.
 *
 * We register this one function for all of the proc files so we have to
 * figure out which one is calling us. I had hoped to just look at file->f_ops
 * and compare it to the fops that we registered but this did not work (I think
 * that files in proc have some other proc-related fops from the kernel and
 * keep the one we pass in in some other data structure). Plan B used here is
 * to store our fops as custom data as the final argument of proc_create_data
 * which comes back to us in PDE_DATA(file->f_inode).
 */
static ssize_t proc_write_allow(struct file *file,
				const char __user * buf,
				size_t length,
				loff_t *offset)
{
	char c;
	ssize_t result = -EINVAL;

	if (length == 0) {
		return 0;
	}

	if (copy_from_user(&c, buf, 1)) {
		return -EFAULT;
	}

	if (c == '0') {
		const struct procfile_devunlock_map_entry *map_entry;
		size_t i;
		for (i = 0; i < ARRAY_SIZE(devunlock_map); i++) {
			map_entry = &devunlock_map[i];
			if (PDE_DATA(file->f_inode) == map_entry->fops) {
				break;
			}
		}

		/*
		 * All proc files registered with this write must appear in
		 * devunlock_map (so we must break in the loop above).
		 */
		BUG_ON(i == ARRAY_SIZE(devunlock_map));

		if (is_mdp_authorized(map_entry->devunlock_perm)) {
			printk(KERN_INFO "%s not set to 0 due to DevUnlock\n",
			       map_entry->file_name);
		}
		else {
			printk(KERN_INFO "%s set to 0\n",
			       map_entry->file_name);
			*(map_entry->allow_flag) = false;
		}
		result = length;
	}

	return result;
}

/*
 * Proc files to view four (read-only) toggles that show whether or not
 * DevUnlock is configured to allow us to:
 * 	- enable the serial console (console_enable)
 * 	- enable telnetd (telnet_enable)
 * 	- execute files out of /jffs and other non-rootfs mounts (exec_enable)
 * 	- enable kernel debug features including sysrq (kernel_debug_enable)
 */

static int proc_show_enabled(struct seq_file *m, void *v);

#define IMPLEMENT_ENABLED_PROCFILE(s, perm)				\
									\
static const u32 devunlock_perm_enabled_##s = perm;			\
									\
static int proc_open_enabled_##s(struct inode *inode, struct file *file)\
{									\
	return single_open(file, proc_show_enabled,			\
			   (void *)&devunlock_perm_enabled_##s);	\
}									\
									\
static const struct file_operations s##_enable_proc_fops = {		\
	.open		= proc_open_enabled_##s,			\
	.read		= seq_read,					\
	.llseek		= seq_lseek,					\
	.release	= single_release,				\
};

IMPLEMENT_ENABLED_PROCFILE(console, MDP_AUTH_FLAG_CONSOLE_ENABLE)
IMPLEMENT_ENABLED_PROCFILE(telnet, MDP_AUTH_FLAG_TELNET_ENABLE)
IMPLEMENT_ENABLED_PROCFILE(exec, MDP_AUTH_FLAG_EXEC_ENABLE)
IMPLEMENT_ENABLED_PROCFILE(kernel_debug, MDP_AUTH_FLAG_KERNEL_DEBUG_ENABLE)

static int proc_show_enabled(struct seq_file *m, void *v)
{
	const u32 *devunlock_perm = m->private;
	seq_printf(m, "%d\n", is_mdp_authorized(*devunlock_perm));
	return 0;
}

#if defined(CONFIG_UBIFS_FS)
/*
 *	/proc/unlock-auth/ubifs_crypt
 *		Initialized by ubifs driver at load time
 *		ro
 */

static int current_ubifs_type = 0;
void sonos_set_proc_crypt(int type)
{
	current_ubifs_type = type;
}
EXPORT_SYMBOL(sonos_set_proc_crypt);

static int ubifs_crypt_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", current_ubifs_type);
	return 0;
}

static int ubifs_crypt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ubifs_crypt_proc_show, NULL);
}

static const struct file_operations ubifs_crypt_proc_fops = {
	.open		= ubifs_crypt_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

/*	/proc/sonos-lock/watchdog_service_state
 *		who (kernel vs. userspace app) is responsible for petting the 'dog?
 *		rw
 */
extern void wdt2_set_service_state(int);
extern int  wdt2_get_service_state(void);

// FIXME (future) Right now Anacapa doesn't touch the watchdog. If we decide we want to use it,
// then these functions will need to be fixed. - Liang

void __attribute__((weak)) wdt2_set_service_state(int new_state)
{
}

int __attribute__((weak)) wdt2_get_service_state(void)
{
	return KERNEL_DEFAULT;
}

static int wdog_state_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", wdt2_get_service_state());
	return 0;
}

static int wdog_state_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, wdog_state_proc_show, NULL);
}

static ssize_t wdog_state_write_data(struct file *file, const char __user * buf, size_t length, loff_t * offset)
{
	char	buffer[64];
	__s32	new_state;

        memset(buffer, 0, sizeof(buffer));
        if (length > sizeof(buffer) - 1)
                length = sizeof(buffer) - 1;
        if (copy_from_user(buffer, buf, length))
                return -EFAULT;

	sscanf(buffer, "%d", &new_state);

	if ( new_state < 0 || new_state > 2 ) {
		printk(KERN_INFO "illegal state - must be 0, 1 or 2\n");
	} else {
		wdt2_set_service_state(new_state);
	}
	return length;
}

static const struct file_operations wdog_state_proc_fops = {
	.open		= wdog_state_proc_open,
	.read		= seq_read,
	.write		= wdog_state_write_data,
	.llseek		= seq_lseek,
	.release	= single_release,
};


/*
 *	/proc/sonos-lock/allowlist_flags
 *  	Reports flag 0x1 if uboot is a allowlist image (not yet implemented).
 *  	Reports flag 0x2 if kernel is a allowlist image.
 *  	ro
 */

static int allowlist_flags_ctrl_proc_show(struct seq_file *m, void *v)
{
	uint32_t flags = 0;
#ifdef CONFIG_SONOS_SECBOOT
	if (__be32_to_cpu(SONOS_FIRMWARE_ALLOWLIST.header.numEntries) > 0) {
		flags |= 0x2;
	}
#endif
	seq_printf(m, "0x%x\n", flags);
	return 0;
}

static int allowlist_flags_ctrl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, allowlist_flags_ctrl_proc_show, NULL);
}

static const struct file_operations allowlist_flags_proc_fops = {
	.open		= allowlist_flags_ctrl_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*
 * unlock-auth module init
 */

static int __init proc_unlock_auth_init(void)
{
	/*
	 * proc_create_data is only used so that the one write handler can tell
	 * which one of the three is being called. There might be a cleaner way
	 * to do this but I could not find one.
	 */
	proc_create_data("sonos-lock/allow_insmod", 0, NULL,
			 &allow_insmod_proc_fops,
			 (void *)&allow_insmod_proc_fops);
	proc_create_data("sonos-lock/allow_mount_dev", 0, NULL,
			 &allow_mount_dev_proc_fops,
			 (void *)&allow_mount_dev_proc_fops);
	proc_create_data("sonos-lock/allow_mount_exec", 0, NULL,
			 &allow_mount_exec_proc_fops,
			 (void *)&allow_mount_exec_proc_fops);
	proc_create("sonos-lock/console_enable", 0, NULL,
		    &console_enable_proc_fops);
	proc_create("sonos-lock/telnet_enable", 0, NULL,
		    &telnet_enable_proc_fops);
	proc_create("sonos-lock/exec_enable", 0, NULL,
		    &exec_enable_proc_fops);
	proc_create("sonos-lock/kernel_debug_enable", 0, NULL,
		    &kernel_debug_enable_proc_fops);
#if defined(CONFIG_UBIFS_FS)
	proc_create("sonos-lock/ubifs_crypt", 0, NULL, &ubifs_crypt_proc_fops);
#endif
	proc_create("sonos-lock/watchdog_service_state", 0, NULL, &wdog_state_proc_fops);
	proc_create("sonos-lock/allowlist_flags", 0, NULL, &allowlist_flags_proc_fops);
	return 0;
}
module_init(proc_unlock_auth_init);
