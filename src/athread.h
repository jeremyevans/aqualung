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

#ifndef AQUALUNG_ATHREAD_H
#define AQUALUNG_ATHREAD_H

#include <config.h>

#include <stddef.h>


#ifdef HAVE_LIBPTHREAD

#include <pthread.h>

#define AQUALUNG_THREAD_DECLARE(thread_id) pthread_t thread_id;
#define AQUALUNG_THREAD_CREATE(id, attr, func, args) \
	pthread_create(&(id), attr, func, args);
#define AQUALUNG_THREAD_JOIN(thread_id) pthread_join(thread_id, NULL);
#define AQUALUNG_THREAD_DETACH() pthread_detach(pthread_self());

#define AQUALUNG_MUTEX_DECLARE(mutex) pthread_mutex_t mutex;
#define AQUALUNG_MUTEX_DECLARE_INIT(mutex) \
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#define AQUALUNG_MUTEX_LOCK(mutex) pthread_mutex_lock(&(mutex));
#define AQUALUNG_MUTEX_UNLOCK(mutex) pthread_mutex_unlock(&(mutex));
#define AQUALUNG_MUTEX_TRYLOCK(mutex) pthread_mutex_trylock(&(mutex)) == 0

#define AQUALUNG_COND_DECLARE(cond) pthread_cond_t cond;
#define AQUALUNG_COND_DECLARE_INIT(cond) \
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
#define AQUALUNG_COND_INIT(cond) pthread_cond_init(&(cond), NULL);
#define AQUALUNG_COND_SIGNAL(cond) pthread_cond_signal(&(cond));
#define AQUALUNG_COND_TIMEDWAIT(cond, mutex, timeout) \
	pthread_cond_timedwait(&(cond), &(mutex), &(timeout));
#define AQUALUNG_COND_WAIT(cond, mutex) pthread_cond_wait(&(cond), &(mutex));

#else /* !HAVE_LIBPTHREAD */

#include <glib.h>

#define AQUALUNG_THREAD_DECLARE(thread_id) GThread * thread_id;
#define AQUALUNG_THREAD_CREATE(id, attr, func, args) \
	id = g_thread_create(func, args, TRUE, NULL);
#define AQUALUNG_THREAD_JOIN(thread_id) g_thread_join(thread_id);
#define AQUALUNG_THREAD_DETACH() ;

#define AQUALUNG_MUTEX_DECLARE(mutex) GMutex * mutex;
#define AQUALUNG_MUTEX_DECLARE_INIT(mutex) GMutex * mutex = NULL;
#define AQUALUNG_MUTEX_LOCK(mutex) g_mutex_lock(mutex);
#define AQUALUNG_MUTEX_UNLOCK(mutex) g_mutex_unlock(mutex);
#define AQUALUNG_MUTEX_TRYLOCK(mutex) g_mutex_trylock(mutex) == TRUE

#define AQUALUNG_COND_DECLARE(cond) GCond * cond;
#define AQUALUNG_COND_DECLARE_INIT(cond) GCond * cond = NULL;
#define AQUALUNG_COND_INIT(cond) cond = NULL;
#define AQUALUNG_COND_SIGNAL(cond) g_cond_signal(cond);
#define AQUALUNG_COND_TIMEDWAIT(cond, mutex, timeout) \
	g_cond_timed_wait(cond, mutex, timeout);
#define AQUALUNG_COND_WAIT(cond, mutex) g_cond_wait(cond, mutex);

#endif /* !HAVE_LIBPTHREAD */


#endif /* AQUALUNG_ATHREAD_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8:
