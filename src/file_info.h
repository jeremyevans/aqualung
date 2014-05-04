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

#ifndef AQUALUNG_FILE_INFO_H
#define AQUALUNG_FILE_INFO_H

#include <glib.h>
#include <gtk/gtk.h>

/* Since we want to use the fileinfo dialog over different models, the
 * caller must provide us a function to access relevant fields of its
 * tree model.
 * Returns TRUE if successful (name and file points to valid data).
 */
typedef gboolean (*fileinfo_model_func_t)(GtkTreeModel * model, GtkTreeIter iter, char**name, char**file);

void show_file_info(GtkTreeModel * model, GtkTreeIter iter_track,
		    fileinfo_model_func_t fi_mfun, int mindepth,
		    gboolean allow_ms_import, gboolean cover_flag);


#endif /* AQUALUNG_FILE_INFO_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
