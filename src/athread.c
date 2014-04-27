/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <config.h>

#include <glib.h>

#include "athread.h"

#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
#include <sched.h>


void
set_thread_priority(pthread_t thread, const gchar * name, gboolean realtime,
		    gint priority) {

	struct sched_param param;
	int policy, priority_min, priority_max;
	int err;

	if ((err = pthread_getschedparam(thread, &policy, &param)) != 0) {
		g_debug("cannot get scheduling policy for %s thread: %s",
			name, g_strerror(err));
		return;
	}

	if (realtime) {
		policy = SCHED_FIFO;
	}
	priority_min = sched_get_priority_min(policy);
	priority_max = sched_get_priority_max(policy);
	if (priority != -1) {
		if (priority < priority_min) {
			g_warning("%s thread priority (%d) too low, set to %d",
				  name, priority, priority_min);
			priority = priority_min;
		} else if (priority > priority_max) {
			g_warning("%s thread priority (%d) too high, set to %d",
				  name, priority, priority_max);
			priority = priority_max;
		}
		param.sched_priority = priority;
	} else {
		if (param.sched_priority < priority_min) {
			param.sched_priority = priority_min;
		} else if (param.sched_priority > priority_max) {
			param.sched_priority = priority_max;
		}
	}

	if ((err = pthread_setschedparam(thread, policy, &param)) != 0) {
		g_warning("cannot set scheduling policy for %s thread: %s",
			  name, g_strerror(err));
	}

}


#else /* !HAVE_LIBPTHREAD */


void
set_thread_priority(GThread * thread, const gchar * name, gboolean realtime,
		    gint priority) {

	if (realtime)
		g_thread_set_priority(thread, G_THREAD_PRIORITY_URGENT);

}


#endif /* HAVE_LIBPTHREAD */

// vim: shiftwidth=8:tabstop=8:softtabstop=8:
