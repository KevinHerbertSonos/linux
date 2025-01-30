/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019-2024, Sonos, Inc.
 *
 * Custom ALSA data types and values for Sonos.
 *
 */

#ifndef _UAPI_SONOS_ASOUND_H
#define _UAPI_SONOS_ASOUND_H

/**
 * Enum definitions for the "SONOS HDMI eARC Link" mixer control.
 *
 * @HDMI_EARC_LINK_DISABLED
 *	eARC discovery is disabled.  eARC peripheral is configured to
 *	passthrough legacy ARC data.
 * @HDMI_EARC_LINK_OFFLINE
 *	eARC discovery is pending.
 * @HDMI_EARC_LINK_ONLINE
 *	eARC link has been established.
 * @HDMI_EARC_LINK_TIMEOUT
 *	eARC discovery has timed out.
 * @HDMI_EARC_LINK_MAX
 *	Number of possible values.
 *
 */
enum sonos_hdmi_earc_link {
	HDMI_EARC_LINK_DISABLED = 0,
	HDMI_EARC_LINK_OFFLINE,
	HDMI_EARC_LINK_ONLINE,
	HDMI_EARC_LINK_TIMEOUT,
	HDMI_EARC_LINK_MAX,
};

#endif /* _UAPI_SONOS_ASOUND_H */
