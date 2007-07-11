/*                                                     -*- linux-c -*-
    Copyright (C) 2007 Tom Szilagyi

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

#ifndef _UTILS_GUI_H
#define _UTILS_GUI_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

int get_systray_semaphore(void);
gint aqualung_dialog_run(GtkDialog * dialog);

GtkWidget* gui_stock_label_button(gchar *label, const gchar *stock);

void deflicker(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _UTILS_GUI_H */
