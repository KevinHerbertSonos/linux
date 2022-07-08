#include <linux/kthread.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/debugfs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/export.h>
#include <linux/ktime.h>

int bit_wait(void *word)
{
	schedule();
	return 0;
}
EXPORT_SYMBOL_GPL(bit_wait);

int bit_wait_io(void *word)
{
	io_schedule();
	return 0;
}
EXPORT_SYMBOL_GPL(bit_wait_io);

/**
 * ktime_get_raw - Returns the raw monotonic time in ktime_t format
 */
ktime_t ktime_get_raw(void)
{
	struct timespec ts;

	getrawmonotonic(&ts);
	return timespec_to_ktime(ts);
}
EXPORT_SYMBOL_GPL(ktime_get_raw);


static inline bool is_kthread_should_stop(void)
{
	return (current->flags & PF_KTHREAD) && kthread_should_stop();
}

/*
 * DEFINE_WAIT_FUNC(wait, woken_wake_func);
 *
 * add_wait_queue(&wq, &wait);
 * for (;;) {
 *     if (condition)
 *         break;
 *
 *     p->state = mode;				condition = true;
 *     smp_mb(); // A				smp_wmb(); // C
 *     if (!wait->flags & WQ_FLAG_WOKEN)	wait->flags |= WQ_FLAG_WOKEN;
 *         schedule()				try_to_wake_up();
 *     p->state = TASK_RUNNING;		    ~~~~~~~~~~~~~~~~~~
 *     wait->flags &= ~WQ_FLAG_WOKEN;		condition = true;
 *     smp_mb() // B				smp_wmb(); // C
 *						wait->flags |= WQ_FLAG_WOKEN;
 * }
 * remove_wait_queue(&wq, &wait);
 *
 */
long wait_woken(wait_queue_t *wait, unsigned mode, long timeout)
{
	set_current_state(mode); /* A */
	/*
	 * The above implies an smp_mb(), which matches with the smp_wmb() from
	 * woken_wake_function() such that if we observe WQ_FLAG_WOKEN we must
	 * also observe all state before the wakeup.
	 */
	if (!(wait->flags & WQ_FLAG_WOKEN) && !is_kthread_should_stop())
		timeout = schedule_timeout(timeout);
	__set_current_state(TASK_RUNNING);

	/*
	 * The below implies an smp_mb(), it too pairs with the smp_wmb() from
	 * woken_wake_function() such that we must either observe the wait
	 * condition being true _OR_ WQ_FLAG_WOKEN such that we will not miss
	 * an event.
	 */
	set_mb(wait->flags, wait->flags & ~WQ_FLAG_WOKEN); /* B */

	return timeout;
}
EXPORT_SYMBOL(wait_woken);

int woken_wake_function(wait_queue_t *wait, unsigned mode, int sync, void *key)
{
	/*
	 * Although this function is called under waitqueue lock, LOCK
	 * doesn't imply write barrier and the users expects write
	 * barrier semantics on wakeup functions.  The following
	 * smp_wmb() is equivalent to smp_wmb() in try_to_wake_up()
	 * and is paired with set_mb() in wait_woken().
	 */
	smp_wmb(); /* C */
	wait->flags |= WQ_FLAG_WOKEN;

	return default_wake_function(wait, mode, sync, key);
}
EXPORT_SYMBOL(woken_wake_function);

#if defined(CONFIG_DEBUG_FS)
struct debugfs_devm_entry {
	int (*read)(struct seq_file *seq, void *data);
	struct device *dev;
};

static int debugfs_devm_entry_open(struct inode *inode, struct file *f)
{
	struct debugfs_devm_entry *entry = inode->i_private;

	return single_open(f, entry->read, entry->dev);
}

static const struct file_operations debugfs_devm_entry_ops = {
	.owner = THIS_MODULE,
	.open = debugfs_devm_entry_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek
};

#endif /* CONFIG_DEBUG_FS */
