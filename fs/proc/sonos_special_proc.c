#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>

#ifdef CONFIG_PROC_FS
#include "linux/sonos_kernel.h"
#include "sonos_custom.h"

extern char uboot_version_str[120];
int uboot_boot_index = -1;
int tee_boot_index = -1;
const char * lk_version;
const char * tee_version;

static int uboot_revision_proc_show(struct seq_file *m, void *v)
{
	if ( uboot_version_str[0] == 'U' )
		seq_printf(m, "%s\n", uboot_version_str);
	else
		seq_printf(m, "ASSERT: no MDP from U-Boot\n");
	return 0;
}

static int uboot_revision_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, uboot_revision_proc_show, NULL);
}

static const struct file_operations uboot_revision_proc_fops = {
	.open		= uboot_revision_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int lk_revision_proc_show(struct seq_file *m, void *v)
{
	char * ver_start, *ver_end;
	char lk_version_string[SONOS_LK_VERSION_STRING_LEN];
	if ( lk_version ) {
		snprintf(lk_version_string, sizeof(lk_version_string),
			"%s", lk_version);
		ver_start = strstr(lk_version_string, "Rev");
		ver_end = strstr(lk_version_string, "Flash");
		if ( ver_start == NULL || ver_end == NULL ) {
			return 0;
		}
		ver_start += 3; /* skip Rev */
		ver_end[0] = 0; /* NULL end */
		seq_printf(m, "%s\n", ver_start);
	}
	return 0;
}

static int lk_revision_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, lk_revision_proc_show, NULL);
}

static const struct file_operations lk_revision_proc_fops = {
	.open		= lk_revision_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int tee_revision_proc_show(struct seq_file *m, void *v)
{
	if ( tee_version ) {
		seq_printf(m, "%s\n", tee_version);
	}
	return 0;
}

static int tee_revision_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, tee_revision_proc_show, NULL);
}

static const struct file_operations tee_revision_proc_fops = {
	.open		= tee_revision_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct ctl_table sonos_tee_ctl_table[] = {
	{
		.procname       = "teesection",
		.data		= &tee_boot_index,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler   = &proc_dointvec,
	},
	{ }
};

static struct ctl_table sonos_tee_sys_table[] = {
	{
	  .procname = "kernel",
	  .mode = 0555,
	  .child = sonos_tee_ctl_table,
	},
	{ }
};

static struct ctl_table sonos_uboot_ctl_table[] = {
	{
		.procname       = "ubootsection",
		.data		= &uboot_boot_index,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler   = &proc_dointvec,
	},
	{ }
};

static struct ctl_table sonos_uboot_sys_table[] = {
	{
	  .procname = "kernel",
	  .mode = 0555,
	  .child = sonos_uboot_ctl_table,
	},
	{ }
};

static struct ctl_table sonos_ctl_table[] = {
	{
		.procname       = "bootgeneration",
		.data		= &bootgeneration,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler   = &proc_dointvec,
	},
	{
		.procname       = "bootsection",
		.data		= &bootsection,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler   = &proc_dointvec,
	},
	{ }
};

static struct ctl_table sonos_sys_table[] = {
	{
	  .procname = "kernel",
	  .mode = 0555,
	  .child = sonos_ctl_table,
	},
	{ }
};

static int __init sonos_special_proc_init(void)
{
	struct device_node *node = NULL;
	int err;
	proc_create("uboot_revision", 0, NULL, &uboot_revision_proc_fops);
#ifdef CONFIG_SYSCTL
	register_sysctl_table(sonos_sys_table);
#endif
	node = of_find_compatible_node(NULL, NULL, "sonos,feature");
	if (node) {
		err = of_property_read_string(node, "lk-version",
			&lk_version);
		if (err == 0) {
			proc_create("lk_revision", 0, NULL, &lk_revision_proc_fops);
		} else {
			lk_version = NULL;
		}
		err = of_property_read_string(node, "tee-version",
			&tee_version);
		if (err == 0) {
			proc_create("tee_revision", 0, NULL, &tee_revision_proc_fops);
		} else {
			tee_version = NULL;
		}
		err = of_property_read_u32(node, "uboot-boot-index",
			&uboot_boot_index);
		if (err == 0) {
			if ( uboot_boot_index == 1 ) {
				uboot_boot_index = 0;
			} else {
				uboot_boot_index = 1;
			}
#ifdef CONFIG_SYSCTL
			register_sysctl_table(sonos_uboot_sys_table);
#endif
		}
		err = of_property_read_u32(node, "tee-boot-index",
			&tee_boot_index);
		if (err == 0) {
			if ( tee_boot_index == 1 ) {
				tee_boot_index = 0;
			} else {
				tee_boot_index = 1;
			}
#ifdef CONFIG_SYSCTL
			register_sysctl_table(sonos_tee_sys_table);
#endif
		}
	}

	return 0;
}
module_init(sonos_special_proc_init);
#endif
