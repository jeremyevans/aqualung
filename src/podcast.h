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

#ifndef _PODCAST_H
#define _PODCAST_H

#include <config.h>

#ifdef HAVE_PODCAST

#include <glib.h>

typedef struct {

	char * file;
	char * title;
	char * desc;
	char * url;

	int new;
	float duration; /* sec */
	unsigned size;  /* byte */
	unsigned date;  /* sec */

} podcast_item_t;

enum {
	PODCAST_AUTO_CHECK  = (1 << 0),
	PODCAST_COUNT_LIMIT = (1 << 1),
	PODCAST_SIZE_LIMIT  = (1 << 2),
	PODCAST_DATE_LIMIT  = (1 << 3)
};

enum {
	PODCAST_STATE_IDLE = 0,
	PODCAST_STATE_UPDATE,
	PODCAST_STATE_ABORTED
};

typedef struct {

	char * dir;
	char * title;
	char * author;
	char * desc;
	char * url;

	unsigned check_interval; /* sec */
	unsigned last_checked;   /* sec */

	/* limits: zero means no limit */
	unsigned count_limit;
	unsigned size_limit;     /* MB */
	unsigned date_limit;     /* sec */

	int flags;
	int state;
	GSList * items;

} podcast_t;

podcast_t * podcast_new(void);
podcast_item_t * podcast_item_new(void);

void podcast_free(podcast_t * podcast);
void podcast_item_free(podcast_item_t * item);

void podcast_update(podcast_t * podcast);

#endif /* HAVE_PODCAST */

#endif /* _PODCAST_H */
