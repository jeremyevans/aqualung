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


#ifndef _STORE_PODCAST_H
#define _STORE_PODCAST_H

#include <config.h>

#include <gtk/gtk.h>

#include "podcast.h"


int store_podcast_iter_is_track(GtkTreeIter * iter);
void store_podcast_iter_addlist_defmode(GtkTreeIter * ms_iter, GtkTreeIter * pl_iter, int new_tab);
void store_podcast_selection_changed(GtkTreeIter * iter, GtkTextBuffer * buffer, GtkLabel * statusbar);
gboolean store_podcast_event_cb(GdkEvent * event, GtkTreeIter * iter, GtkTreePath * path);
void store_podcast_load_icons(void);
void store_podcast_create_popup_menu(void);
void store_podcast_set_toolbar_sensitivity(GtkTreeIter * iter, GtkWidget * edit,
					   GtkWidget * add, GtkWidget * remove);
void store_podcast_toolbar__edit_cb(gpointer data);
void store_podcast_toolbar__add_cb(gpointer data);
void store_podcast_toolbar__remove_cb(gpointer data);

void create_podcast_node(void);
void store_podcast_updater_start(void);

typedef struct {

	podcast_t * podcast;
	int ncurrent;
	int ndownloads;
	int percent;

} podcast_download_t;

podcast_download_t * podcast_download_new(podcast_t * podcast);

void store_podcast_update_podcast(podcast_download_t * pd);
void store_podcast_update_podcast_download(podcast_download_t * pd);
void store_podcast_add_item(podcast_t * podcast, podcast_item_t * item);
void store_podcast_remove_item(podcast_t * podcast, podcast_item_t * item);

void store_podcast_save(void);
void store_podcast_load(void);


#endif /* _STORE_PODCAST_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
