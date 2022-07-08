/*
 * Copyright (c) 2014-2020, Sonos, Inc.
 *
 * SPDX-License-Identifier:     GPL-2.0
 *
 */

#include <linux/slab.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include "button_event.h"

#ifdef CONFIG_SONOS_EVENT_QUEUE_MODULE
#include "blackbox.h"
#include "event_queue_api.h"
#endif

struct button_event_queue * button_event_queue_create(u32 len, wait_queue_head_t *wq, spinlock_t *lock)
{
    size_t size;
    struct button_event_queue *beq;

    size = sizeof(*beq) + (len * sizeof(beq->events)); 
    beq = kmalloc(size, GFP_KERNEL);
    if (beq == NULL) {
#ifdef CONFIG_SONOS_EVENT_QUEUE_MODULE
        bb_log(BB_MOD_EVENT, BB_LVL_ERR, "Event queue allocation failed!");
#else
        printk("%s: failed\n", __func__);
#endif
    } else {
        memset(beq, 0, sizeof(*beq));
        beq->len = len;
        beq->kwq = wq;
        beq->lock = lock;
    }
    return beq;
}

void button_event_queue_free(struct button_event_queue *beq)
{
    if (beq != NULL) {
        kfree(beq);
    }
}

int button_event_send(struct button_event_queue *beq, enum HWEVTQ_EventSource in_source, enum HWEVTQ_EventInfo in_info)
{
    struct button_evt *event;
    enum HWEVTQ_EventSource source = in_source;
    enum HWEVTQ_EventInfo info = in_info;
    struct timeval timestamp;
    unsigned long flags;
    int ret = 0;
    struct timespec from_timestamp;


    if (beq == NULL) {
        return -EIO;
    }

#ifdef CONFIG_SONOS_EVENT_QUEUE_MODULE
    /* Do not log noisy UPDATE events unless blackbox debug is enabled */
    if (in_info != HWEVTQINFO_UPDATE) {
        bb_log(BB_MOD_EVENT, BB_LVL_DEBUG, "Sent event %s from %s.", event_queue_get_event_string(in_info),
                                                                     event_queue_get_source_string(in_source));
    } else {
        bb_log_dbg(BB_MOD_EVENT, "Sent event %s from %s.", event_queue_get_event_string(in_info),
                                                           event_queue_get_source_string(in_source));
    }
#endif

#if defined(SONOS_ARCH_ATTR_LACKS_NTP_DAEMON)
#error "CLOCK_MONOTONIC and CLOCK_REALTIME both in use!"
#endif
    ktime_get_ts(&from_timestamp);
    timestamp.tv_sec = from_timestamp.tv_sec;
    timestamp.tv_usec = ((from_timestamp.tv_nsec)/1000);
    spin_lock_irqsave(beq->lock, flags);
    event = &beq->events[beq->head];
    if (button_event_queue_full(beq)) {
        beq->overruns++;
        if (!beq->was_full) {
            beq->was_full = 1;
            printk("event dropped, button queue full\n");
        }
        button_event_queue_flush(beq);
        event = &beq->events[beq->head];
        source = HWEVTQSOURCE_NO_SOURCE;
        info = HWEVTQINFO_QUEUE_OVERRUN;
        ret = -EIO;
    }
    event->m_lSource   = source;
    event->m_lInfo     = info;
    event->m_timestamp = timestamp;
    beq->head = button_event_queue_next_head(beq);
    beq->num++;
    if (beq->num > beq->high_water) {
        beq->high_water = beq->num;
    }
    spin_unlock_irqrestore(beq->lock, flags);
    wake_up_interruptible(beq->kwq);
    return ret;
}

struct button_evt * button_event_get(struct button_event_queue *beq)
{
    struct button_evt *event = &beq->events[beq->tail];
    beq->tail = button_event_queue_next_tail(beq);
    beq->num--;
    return event;
}

int button_event_receive(struct button_event_queue *beq, char *user_data)
{
    struct button_evt *event;
    int ret = 0;
    wait_queue_t wq;

    if (beq == NULL) {
        return -EIO;
    }
    init_waitqueue_entry(&wq, current);
    spin_lock_irq(beq->lock);
    while (button_event_queue_empty(beq)) {
        add_wait_queue(beq->kwq, &wq);
        spin_unlock_irq(beq->lock);
        current->state = TASK_INTERRUPTIBLE;
        schedule();
        current->state = TASK_RUNNING;
        spin_lock_irq(beq->lock);
        remove_wait_queue(beq->kwq, &wq);
        if (signal_pending(current)) {
            ret = -ERESTARTSYS;
            goto out_unlock;
        }
    }

    event = button_event_get(beq);
    if (copy_to_user(user_data, (char *)event, sizeof(*event))) {
#ifdef CONFIG_SONOS_EVENT_QUEUE_MODULE
        bb_log(BB_MOD_EVENT, BB_LVL_ERR, "%s: copy_to_user failed", __func__);
#else
        printk("%s: copy_to_user failed\n", __func__);
#endif
        ret = -EFAULT;
    } else {
        ret = sizeof(*event);
        beq->was_full = 0;
    }

out_unlock:
    spin_unlock_irq(beq->lock);
    return ret;
}

void button_event_queue_flush(struct button_event_queue *beq)
{
    if (beq != NULL) {
        beq->head = beq->tail;
        beq->num = 0;
    }
}
