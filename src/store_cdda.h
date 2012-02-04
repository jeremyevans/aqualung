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

#ifndef AQUALUNG_STORE_CDDA_H
#define AQUALUNG_STORE_CDDA_H

#include <gtk/gtk.h>

#include "cdda.h"

void create_cdda_node(void);
int store_cdda_iter_is_track(GtkTreeIter * iter);
void store_cdda_iter_addlist_defmode(GtkTreeIter * ms_iter, GtkTreeIter * pl_iter, int new_tab);
void store_cdda_selection_changed(GtkTreeIter * iter, GtkTextBuffer * buffer, GtkLabel * statusbar);
gboolean store_cdda_event_cb(GdkEvent * event, GtkTreeIter * iter, GtkTreePath * path);
void store_cdda_load_icons(void);
void store_cdda_create_popup_menu(void);

gboolean store_cdda_remove_track(GtkTreeIter * iter);
gboolean store_cdda_remove_record(GtkTreeIter * iter);

void cdda_record_auto_query_cddb(GtkTreeIter * drive_iter);

void cdda_add_to_playlist(GtkTreeIter * iter_drive, unsigned long hash);
void cdda_remove_from_playlist(cdda_drive_t * drive);

typedef struct {
	char * path;
	float duration;
} cdda_track_t;


#endif /* AQUALUNG_STORE_CDDA_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
