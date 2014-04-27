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

#include "athread.h"

#ifdef HAVE_LIBPTHREAD
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>


void
set_thread_priority(pthread_t thread, char * name, int realtime, int priority) {

	struct sched_param param;
	int policy;
	int x;

	if (pthread_getschedparam(thread, &policy, &param) != 0) {
		return;
	}

	if (realtime) {
		policy = SCHED_FIFO;
#ifndef __OpenBSD__
		if (priority == -1) {
			priority = 1;
		}
#endif /* !__OpenBSD__ */
	}
	if (priority != -1) {
#ifdef PTHREAD_MIN_PRIORITY
#ifdef PTHREAD_MAX_PRIORITY
		if (priority < PTHREAD_MIN_PRIORITY) {
			param.sched_priority = PTHREAD_MIN_PRIORITY;
			fprintf(stderr, "Warning: %s thread priority (%d) too low, set to %d",
				name, priority, PTHREAD_MIN_PRIORITY);
		} else if (priority > PTHREAD_MAX_PRIORITY) {
			param.sched_priority = PTHREAD_MAX_PRIORITY;
			fprintf(stderr, "Warning: %s thread priority (%d) too high, set to %d",
				name, priority, PTHREAD_MAX_PRIORITY);
		} else
#endif /* PTHREAD_MAX_PRIORITY */
#endif /* PTHREAD_MIN_PRIORITY */
		{
			param.sched_priority = priority;
		}
	}

	if ((x = pthread_setschedparam(thread, policy, &param)) != 0) {
		fprintf(stderr,
			"Warning: cannot use real-time scheduling for %s thread (%s/%d) "
			"(%d: %s)\n", name, policy == SCHED_FIFO ? "FIFO" : "RR", param.sched_priority,
			x, strerror(x));
	}

}


#else /* !HAVE_LIBPTHREAD */
#include <glib.h>


void
set_thread_priority(GThread * thread, char * name, int realtime, int priority) {

	if (realtime)
		g_thread_set_priority(thread, G_THREAD_PRIORITY_URGENT);

}


#endif /* HAVE_LIBPTHREAD */

// vim: shiftwidth=8:tabstop=8:softtabstop=8:
