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


#ifndef _MUSIC_BROWSER_H
#define _MUSIC_BROWSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gtk/gtk.h>

void tree_selection_changed_cb(GtkTreeSelection * selection, gpointer data);

void create_music_browser(void);
void show_music_browser(void);
void hide_music_browser(void);

void music_store_mark_changed(GtkTreeIter * iter);
void music_store_mark_saved(GtkTreeIter* iter_store);

void load_music_store(char * file, char * sort);
void load_all_music_store(void);
void save_music_store(GtkTreeIter * iter_store);
void save_all_music_store(void);

void store__addlist_defmode(gpointer data);
void artist__addlist_defmode(gpointer data);
void record__addlist_defmode(gpointer data);
void track__addlist_cb(gpointer data);


#ifdef HAVE_CDDA
int is_store_path_cdda(GtkTreePath * p);
int is_store_iter_cdda(GtkTreeIter * i);
void update_cdda_status_bar(void);
#endif /* HAVE_CDDA */

void music_store_progress_bar_hide(void);
void music_store_set_status_bar_info(void);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* _MUSIC_BROWSER_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
