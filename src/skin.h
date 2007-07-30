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

#ifndef _SKIN_H
#define _SKIN_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

void create_skin_window(void);
void register_toplevel_window(GtkWidget * window);
void unregister_toplevel_window(GtkWidget * window);
void apply_skin(char * path);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _SKIN_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
