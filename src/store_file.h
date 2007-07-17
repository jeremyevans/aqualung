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


#ifndef _STORE_FILE_H
#define _STORE_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gtk/gtk.h>

gboolean store_file_event_cb(GdkEvent * event, GtkTreeIter * iter, GtkTreePath * path);
void store_file_selection_changed(void);
void store_file_load_icons(void);
void store_file_create_popup_menu(void);

void music_store_mark_changed(GtkTreeIter * iter);
void music_store_mark_saved(GtkTreeIter* iter_store);

void load_music_store(char * file, char * sort);
void load_all_music_store(void);
void save_music_store(GtkTreeIter * iter_store);
void save_all_music_store(void);

void store__add_cb(gpointer data);
void artist__add_cb(gpointer data);
void record__add_cb(gpointer data);
void track__add_cb(gpointer data);

void store__edit_cb(gpointer data);
void artist__edit_cb(gpointer data);
void record__edit_cb(gpointer data);
void track__edit_cb(gpointer data);

void store__remove_cb(gpointer data);
void artist__remove_cb(gpointer data);
void record__remove_cb(gpointer data);
void track__remove_cb(gpointer data);

void store__save_all_cb(gpointer data);

int is_store_iter_readonly(GtkTreeIter * i);
void ms_progress_bar_show(void);



void store__addlist_defmode(gpointer data);
void artist__addlist_defmode(gpointer data);
void record__addlist_defmode(gpointer data);
void track__addlist_cb(gpointer data);

typedef struct {
	int type;
	char * file;    /* physical filename */
	int readonly;
	int dirty;      /* 1: modified since last save, 0: saved */
	char * comment;
} store_data_t;

typedef struct {
	char * comment;
} artist_data_t;

typedef struct {
	int year;
	char * comment;
} record_data_t;

typedef struct {
	char * file;    /* physical filename */
	float duration; /* length in seconds */
	float volume;   /* average volume in dBFS */
	float rva;      /* manual RVA in dB */
	int use_rva;    /* use manual RVA */
	char * comment;
} track_data_t;


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _STORE_FILE_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
