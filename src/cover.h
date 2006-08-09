/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi
              (C) 2006 Tomasz Maka

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

#ifndef _COVER_H
#define _COVER_H

/* FIXME  34 is space for scrollbar width, window border and it is theme dependent */
#define SCROLLBAR_WIDTH         34

void    display_cover           (GtkWidget *image_area, gint dest_width, gint dest_height, gchar *song_filename, gboolean hide);
void    insert_cover            (GtkTextIter * iter);


#endif /* _COVER_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

