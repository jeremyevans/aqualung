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

#include <gtk/gtk.h>

void create_music_browser(void);
void show_music_browser(void);
void hide_music_browser(void);

void music_store_search(void);

int path_get_store_type(GtkTreePath * p);
int iter_get_store_type(GtkTreeIter * i);
void music_store_iter_addlist_defmode(GtkTreeIter * ms_iter, GtkTreeIter * pl_iter, int new_tab);
int music_store_iter_is_track(GtkTreeIter * iter);

void music_store_selection_changed(void);
void music_browser_set_font(int cond);

void music_store_mark_changed(GtkTreeIter * iter);
void music_store_mark_saved(GtkTreeIter* iter_store);

void music_store_load_all(void);
void music_store_save_all(void);


struct keybinds {
	void (*callback)(gpointer);
	int keyval1;
	int keyval2;
};

enum {
	MS_COL_NAME = 0,
	MS_COL_SORT,
	MS_COL_FONT,
	MS_COL_ICON,
	MS_COL_DATA,

	MS_COL_COUNT
};

enum {
	STORE_TYPE_INVALID,
	STORE_TYPE_FILE,
	STORE_TYPE_CDDA,
	STORE_TYPE_PODCAST
};

typedef union {
	int type;
} store_t;


#endif /* _MUSIC_BROWSER_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
