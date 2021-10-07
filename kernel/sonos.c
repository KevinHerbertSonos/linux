/*
 * Copyright (c) 2014-2018, Sonos, Inc.
 *
 * SPDX-License-Identifier:     GPL-2.0
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>

#include "mdp.h"

/* In memory copy of the MDP */
struct manufacturing_data_page sys_mdp;
EXPORT_SYMBOL(sys_mdp);
