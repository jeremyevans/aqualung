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

void tree_selection_changed_cb(GtkTreeSelection * selection, gpointer data);

void make_title_string(char * dest, char * template, char * artist, char * record, char * track);

void create_music_browser(void);
void show_music_browser(void);
void hide_music_browser(void);

void music_store_mark_changed(void);
void music_store_mark_saved(void);

void load_music_store(void);
void save_music_store(void);

void store__addlist_defmode(gpointer data);
void artist__addlist_defmode(gpointer data);
void record__addlist_defmode(gpointer data);
void track__addlist_cb(gpointer data);

int store_get_iter_for_artist_and_record(GtkTreeIter * store_iter, GtkTreeIter * iter,
					 const char * artist, const char * artist_sort_name,
					 const char * record, const char * record_sort_name,
					 const char * record_comment);
int artist_get_iter_for_record(GtkTreeIter * artist_iter, GtkTreeIter * iter,
			       const char * record, const char * record_sort_name,
			       const char * record_name);

#endif /* _MUSIC_BROWSER_H */
