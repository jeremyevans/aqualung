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

#ifdef _WIN32
#include <glib.h>
#else
#include <pthread.h>
#endif /* _WIN32 */

enum {
	PLAYLIST_LOAD,
	PLAYLIST_LOAD_TAB,
	PLAYLIST_ENQUEUE
};

typedef struct {

	AQUALUNG_THREAD_DECLARE(thread_id)
	AQUALUNG_MUTEX_DECLARE(thread_mutex)
	AQUALUNG_MUTEX_DECLARE(wait_mutex)
	AQUALUNG_COND_DECLARE(thread_wait)

	volatile int thread_stop;

	int progbar_semaphore;
	int ms_semaphore;

	int index;

	char name[MAXLEN];
	int name_set;
	int playing;
	int closed;

	GtkTreeStore * store;
	GtkTreeSelection * select;
	GtkWidget * view;
	GtkWidget * scroll;
	GtkWidget * label;
	GtkWidget * tab_close_button;
	GtkWidget * tab_menu;
	GtkWidget * tab__close_undo;

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
	int threshold;

	GSList * list;
	GList * pldata_list;

	void * xml_doc;
	void * xml_node;
	int * xml_ref;

	char * filename;

	int clear;
	int start_playback;

} playlist_transfer_t;

playlist_t * playlist_tab_new(char * name);
playlist_t * playlist_tab_new_if_nonempty(char * name);

playlist_t * playlist_get_current(void);
void playlist_set_current(playlist_t * pl);
playlist_t * playlist_get_playing(void);

GtkTreePath * playlist_get_playing_path(playlist_t * pl);

void playlist_size_allocate_all(void);
void playlist_reorder_columns_all(int * order);
void playlist_show_hide_close_buttons(gboolean state);

void create_playlist(void);
void show_playlist(void);
void hide_playlist(void);

void playlist_set_playing(playlist_t * pl, int playing);

void playlist_stats_set_busy(void);
void playlist_progress_bar_show(playlist_t * pl);
void playlist_progress_bar_hide_all(void);
void show_hide_close_buttons(gboolean state);

void playlist_ensure_tab_exists(void);
void playlist_load(GSList * list, int mode, char * tab_name, int start_playback);
void playlist_save_all(char * filename);

void playlist_set_color(void);
void playlist_reset_display_names(void);
void playlist_disable_bold_font(void);
void playlist_set_font(int cond);

void playlist_content_changed(playlist_t * pl);
void playlist_selection_changed(playlist_t * pl);
void playlist_drag_end(GtkWidget * widget, GdkDragContext * drag_context, gpointer data);

void playlist_menu_set_popup_sensitivity(playlist_t * pl);
gint playlist_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent);
void playlist_foreach_selected(playlist_t * pl,
			       int (* foreach)(playlist_t *, GtkTreeIter *, void *),
			       void * data);

void recalc_album_node(playlist_t * pl, GtkTreeIter * iter);
void mark_track(GtkTreeStore * store, GtkTreeIter * piter);
void unmark_track(GtkTreeStore * store, GtkTreeIter * piter);

void show_active_position_in_playlist(playlist_t * pl);

#ifdef HAVE_CDDA
void playlist_add_cdda(GtkTreeIter * iter_drive, unsigned long hash);
void playlist_remove_cdda(char * device_path);
#endif /* HAVE_CDDA */

enum {
	PL_FLAG_ACTIVE      = (1 << 0),
	PL_FLAG_COVER       = (1 << 1),
	PL_FLAG_ALBUM_NODE  = (1 << 2),
	PL_FLAG_ALBUM_CHILD = (1 << 3)
};

typedef struct {

	char * artist;
	char * album;
	char * title;   /* NULL for album nodes */
	char * file;    /* NULL for album nodes */
	float voladj;   /* volume adjustment [dB] */
	float duration; /* length in seconds */
	unsigned size;  /* file size in bytes */

	unsigned short ntracks; /* number of children nodes (for album nodes only) */
	unsigned short actrack; /* active children node (for album nodes only) */
	int flags;

} playlist_data_t;

playlist_data_t * playlist_data_new(void);
void playlist_data_get_display_name(char * list_str, playlist_data_t * pldata);

#define PL_IS_SET_FLAG(plist, flag) (plist->flags & flag)
#define PL_SET_FLAG(plist, flag) (plist->flags |= flag)
#define PL_UNSET_FLAG(plist, flag) (plist->flags &= ~flag)

#define IS_PL_ACTIVE(plist) PL_IS_SET_FLAG(plist, PL_FLAG_ACTIVE)
#define IS_PL_COVER(plist) PL_IS_SET_FLAG(plist, PL_FLAG_COVER)
#define IS_PL_ALBUM_NODE(plist) PL_IS_SET_FLAG(plist, PL_FLAG_ALBUM_NODE)
#define IS_PL_ALBUM_CHILD(plist) PL_IS_SET_FLAG(plist, PL_FLAG_ALBUM_CHILD)
#define IS_PL_TOPLEVEL(plist) (IS_PL_ALBUM_NODE(plist) || !IS_PL_ALBUM_CHILD(plist))


enum {
	PL_COL_NAME = 0,
	PL_COL_VADJ,
	PL_COL_DURA,
	PL_COL_COLO,
	PL_COL_FONT,
	PL_COL_DATA,

	PL_COL_COUNT
};

#endif /* _PLAYLIST_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
