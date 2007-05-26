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


#ifndef _PLAYLIST_H
#define _PLAYLIST_H

#include <config.h>

#include "common.h"

#define PLAYLIST_LOAD     0
#define PLAYLIST_LOAD_TAB 1
#define PLAYLIST_ENQUEUE  2

typedef struct {

	AQUALUNG_THREAD_DECLARE(thread_id)
	AQUALUNG_MUTEX_DECLARE(thread_mutex)
	AQUALUNG_MUTEX_DECLARE(wait_mutex)
	AQUALUNG_COND_DECLARE(thread_wait)

	volatile int thread_stop;

	int progbar_semaphore;
	int ms_semaphore;

	char name[MAXLEN];
	int playing;

	GtkTreeStore * store;
	GtkTreeSelection * select;
	GtkWidget * view;
	GtkWidget * scroll;
	GtkWidget * label;

	GtkTreeViewColumn * track_column;
	GtkTreeViewColumn * rva_column;
	GtkTreeViewColumn * length_column;

	GtkWidget * progbar;
	GtkWidget * progbar_container;
	GtkWidget * progbar_stop_button;

	GtkWidget * widget;

} playlist_t;

typedef struct {

	playlist_t * pl;

	volatile int data_written;

	GSList * list;
	GList * plfm_list;

	void * xml_doc;
	void * xml_node;

	int index;
	int mode;
	int on_the_fly;
	int clear;
	int start_playback;

} playlist_transfer_t;

playlist_t * playlist_get_current(void);
playlist_t * playlist_get_playing(void);

playlist_transfer_t * playlist_transfer_get(int mode, char * name);

GtkTreePath * playlist_get_playing_path(playlist_t * pl);

void delayed_playlist_rearrange(void);
void playlist_size_allocate_all(void);
void playlist_reorder_columns_all(int * order);

void create_playlist(void);
void show_playlist(void);
void hide_playlist(void);

void playlist_set_playing(playlist_t * pl, int playing);

void playlist_stats_set_busy(void);
void playlist_progress_bar_show(playlist_t * pl);
void playlist_progress_bar_hide_all(void);

void playlist_ensure_tab_exists(void);
void playlist_load_entity(const char * filename, int mode, playlist_transfer_t * pt);
void playlist_save_all(char * filename);

void playlist_disable_bold_font(void);
void set_playlist_color(void);

void playlist_content_changed(playlist_t * pl);
void playlist_selection_changed(playlist_t * pl);
void playlist_drag_end(GtkWidget * widget, GdkDragContext * drag_context, gpointer data);
gint playlist_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent);
void playlist_foreach_selected(playlist_t * pl,
			       void (* foreach)(playlist_t *, GtkTreeIter *, void *),
			       void * data);

void mark_track(GtkTreeStore * store, GtkTreeIter * piter);
void unmark_track(GtkTreeStore * store, GtkTreeIter * piter);


#ifdef HAVE_CDDA
void playlist_add_cdda(GtkTreeIter * iter_drive, unsigned long hash);
void playlist_remove_cdda(char * device_path);
#endif /* HAVE_CDDA */

enum {
	COLUMN_TRACK_NAME = 0,
	COLUMN_PHYSICAL_FILENAME,
	COLUMN_SELECTION_COLOR,
	COLUMN_VOLUME_ADJUSTMENT,
	COLUMN_VOLUME_ADJUSTMENT_DISP,
	COLUMN_DURATION,  
	COLUMN_DURATION_DISP,
	COLUMN_FONT_WEIGHT,

	NUMBER_OF_COLUMNS /* it must be the last entry here */
};

#endif /* _PLAYLIST_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
