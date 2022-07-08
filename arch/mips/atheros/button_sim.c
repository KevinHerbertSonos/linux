/*
 * Button simulation
 * Copyright (c) 2014-2022 Sonos Inc.
 * All rights reserved.
 */

#include <linux/stddef.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include "mdp.h"
#include <linux/string.h>
#include <linux/slab.h>
#include <stdbool.h>
#include "button_event.h"
#ifdef SONOS_ARCH_ATTR_SUPPORTS_HWEVTQ
#include "hwevent_queue_api.h"
#endif


static struct button_sim_match *button_match;
static struct button_event_queue *button_queue;
static struct list_head  current_simulations;
static spinlock_t current_simulations_lock;

#include "button_simulator.h"

static int
button_sim_cmd_match(char *cmd, struct button_sim_match *bsm)
{
	return (strncmp(cmd, bsm->cmd, strlen(bsm->cmd)) == 0);
}


static void
button_sim_timer_handler(ButtonFollowUp* datap)
{
	struct button_sim_match *bsm; //struct button_sim_inst *
	enum HWEVTQ_EventInfo info;
	unsigned int msec;


	if (!datap){
		return;
	}

	datap->msec_remaining -= datap->msec_this_interval;

	// If no more time left the we release the button
	info = (datap->msec_remaining <= 0) ? HWEVTQINFO_RELEASED : HWEVTQINFO_REPEATED;

	bsm = button_match;
	while (bsm->source != HWEVTQSOURCE_NO_SOURCE) {
		if (button_sim_cmd_match(datap->simulated_cmd, bsm)) {
			button_event_send(button_queue, bsm->source, info);
#ifdef SONOS_ARCH_ATTR_SUPPORTS_HWEVTQ
			hwevtq_send_event_defer(bsm->source, info);
#endif
			break; // we found it so only button command
		}
		bsm++;
	}

	if (datap->msec_remaining > 0) {
	        msec = min(datap->msec_remaining,(unsigned int)REPEAT_TIME);
		datap->msec_this_interval = msec;
		mod_timer(&datap->sim_timer, jiffies + MS_TO_JIFFIES(msec));
	}
	else {
		buttonFollowUp_destroy(datap);
	}
}



/* called from proc write to /proc/driver/audioctl or equivalent file
   expected format is "keyword=duration"
   keywords are names for buttons or actions: volup,voldn,join,etc.
   the duration is the number of msec the press is active, duration is 0 if not present
   return TRUE if cmd processed */
int
button_sim_process_cmd(char *cmd)
{
	struct button_sim_match *bsm, *match_bsm;
	unsigned int duration = 0;
	unsigned int msec;
	ButtonFollowUp* cursor;
	ButtonFollowUp* tmpstore;
	int status = false;

	match_bsm = NULL;
	bsm = button_match;
	while (bsm->source != HWEVTQSOURCE_NO_SOURCE) {
		if (button_sim_cmd_match(cmd, bsm)) {
			match_bsm = bsm;
			// check if there is already a timer for this running - if so cancel it 
			list_for_each_entry_safe(cursor, tmpstore, &current_simulations, list){ // search active timers
				if ( strncmp(cursor->simulated_cmd,match_bsm->cmd,strlen(match_bsm->cmd)) == 0 ){
		  			printk("Canceled piror %s button press\n", match_bsm->cmd);
					buttonFollowUp_destroy(cursor); // found one that matches the command so stop it
					break; // there should only be one  - so we can stop searching
				}
			}
	    
			button_event_send(button_queue, bsm->source, HWEVTQINFO_PRESSED);
#ifdef SONOS_ARCH_ATTR_SUPPORTS_HWEVTQ
			hwevtq_send_event(bsm->source, HWEVTQINFO_PRESSED);
#endif
		}
		bsm++;
	}
	if (match_bsm != NULL) {
		// create a new object to keep track of button release and or repeate
		ButtonFollowUp* datap = buttonFollowUp_create(match_bsm->cmd);

		if (datap) {
			// Note: duration can be zero. This means follow the release in quick succession
			// which is what happens because the timer will expire right away with the timeout
			// value of zero. This is good because we can always handle the release in the same
			// code area, the timeout handler 
			duration = button_sim_get_duration(cmd+strlen(match_bsm->cmd));
		        msec = min(duration,(unsigned int)REPEAT_TIME_FIRST);
			datap->msec_remaining = duration;
			datap->msec_this_interval = msec;
			datap->sim_timer.function = (void (*)(unsigned long))button_sim_timer_handler;
			datap->sim_timer.data = (long)datap; 
			init_timer(&datap->sim_timer);		
			printk("Simulating %s button press for %dms\n", match_bsm->cmd, duration);
			mod_timer(&datap->sim_timer, jiffies + MS_TO_JIFFIES(msec));
			status = true;
		}
		else {
			printk("Failed to allocate memory for button info\n");
		}
	}
	return status;
}

void
button_sim_init(struct button_event_queue *beq, struct button_sim_match *button_match_in)
{
	button_queue = beq;
	button_match = button_match_in;
	spin_lock_init(&current_simulations_lock);
	INIT_LIST_HEAD(&current_simulations);
}

