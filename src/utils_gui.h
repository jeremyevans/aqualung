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

#include <config.h>

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_SYSTRAY
int get_systray_semaphore(void);
#endif /* HAVE_SYSTRAY */

gint aqualung_dialog_run(GtkDialog * dialog);

GtkWidget* gui_stock_label_button(gchar *label, const gchar *stock);

void deflicker(void);

GSList * file_chooser(char * title, GtkWidget * parent,
		      GtkFileChooserAction action, int filter, gint multiple);
void file_chooser_with_entry(char * title, GtkWidget * parent,
			     GtkFileChooserAction action, int filter, GtkWidget * entry);

int message_dialog(char * title, GtkWidget * parent, GtkMessageType type,
		   GtkButtonsType buttons, GtkWidget * extra, char * text, ...);

enum {
	FILE_CHOOSER_FILTER_NONE = 0,
	FILE_CHOOSER_FILTER_AUDIO,
	FILE_CHOOSER_FILTER_PLAYLIST,
	FILE_CHOOSER_FILTER_STORE
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _UTILS_GUI_H */
