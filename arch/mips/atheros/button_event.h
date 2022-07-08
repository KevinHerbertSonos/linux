/*
 * Copyright (c) 2014-2020, Sonos, Inc.
 *
 * SPDX-License-Identifier:     GPL-2.0
 *
 */

/* Kernel space only button event definitions. Shared user/kernel space
   definitions are in sonos-ctl.h */

#ifndef BUTTON_EVENT_H
#define BUTTON_EVENT_H

#include <linux/sched.h>
#include "hwevent_queue_user_api.h"

struct button_sim_match {
    const char *cmd;
    enum HWEVTQ_EventSource source;
};

struct button_event_queue {
    u32 head;
    u32 tail;
    u32 len;
    u32 num;
    u32 high_water;
    u32 overruns;
    int was_full;
    spinlock_t *lock;
    wait_queue_head_t *kwq;
    struct button_evt events[1];
};

static inline u32 button_event_queue_next(struct button_event_queue *beq, u32 current_index)
{
    return ((current_index + 1) % beq->len);
}

static inline u32 button_event_queue_next_head(struct button_event_queue *beq)
{
    return button_event_queue_next(beq, beq->head);
}

static inline u32 button_event_queue_next_tail(struct button_event_queue *beq)
{
    return button_event_queue_next(beq, beq->tail);
}

static inline int button_event_queue_empty(struct button_event_queue *beq)
{
    return (beq->head == beq->tail);
}

static inline int button_event_queue_full(struct button_event_queue *beq)
{
    return (button_event_queue_next_head(beq) == beq->tail);
}

extern  int button_event_send(struct button_event_queue *, enum HWEVTQ_EventSource, enum HWEVTQ_EventInfo);
extern  int button_event_receive(struct button_event_queue *beq, char *user_data);
extern struct button_evt * button_event_get(struct button_event_queue *beq);
extern struct button_event_queue * button_event_queue_create(u32 len, wait_queue_head_t *wq, spinlock_t *lock);
extern void button_event_queue_free(struct button_event_queue *beq);
extern void button_event_queue_flush(struct button_event_queue *);
extern void button_sim_init(struct button_event_queue *beq, struct button_sim_match *cmds);
extern int  button_sim_process_cmd(char *cmd);

#endif
