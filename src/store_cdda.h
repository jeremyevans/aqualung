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

#ifndef _STORE_CDDA_H
#define _STORE_CDDA_H

#include <config.h>

#ifdef HAVE_CDDA

#include <gtk/gtk.h>

#include "cdda.h"

int store_cdda_iter_is_track(GtkTreeIter * iter);
void store_cdda_iter_addlist_defmode(GtkTreeIter * ms_iter, GtkTreeIter * pl_iter, int new_tab);
void store_cdda_selection_changed(void);
gboolean store_cdda_event_cb(GdkEvent * event, GtkTreeIter * iter, GtkTreePath * path);
void store_cdda_load_icons(void);
void store_cdda_create_popup_menu(void);

void cdda_add_to_playlist(GtkTreeIter * iter_drive, unsigned long hash);
void cdda_remove_from_playlist(cdda_drive_t * drive);

void cdda_update_statusbar(void);

#endif /* HAVE_CDDA */

#endif /* _STORE_CDDA_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
