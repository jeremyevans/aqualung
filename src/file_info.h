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

#ifndef _FILE_INFO_H
#define _FILE_INFO_H

#include <gtk/gtk.h>


void show_file_info(char * name, char * file, int is_called_from_browser,
		    GtkTreeModel * model, GtkTreeIter iter_track, gboolean cover_flag);


#endif /* _FILE_INFO_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
