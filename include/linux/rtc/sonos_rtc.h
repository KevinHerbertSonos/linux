/*
 * Copyright (c) 2018, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef __SONOS_RTC_H__
#define __SONOS_RTC_H__

struct rtc_hw_ops {
	int (*sonos_read_time)(time64_t *);
	int (*sonos_set_time)(time64_t);
	int (*sonos_read_alarm)(time64_t*);
	int (*sonos_set_alarm)(time64_t);
};

/* Exported Function */
int sonos_rtc_register_ops(struct rtc_hw_ops *sonos_rtc_hw_ops);
void sonos_rtc_unregister_ops(void);

#endif /* __SONOS_RTC_H__ */
