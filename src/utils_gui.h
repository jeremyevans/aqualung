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

enum {
	TOP_WIN_SKIN = (1 << 0),
	TOP_WIN_TRAY = (1 << 1)
};

void register_toplevel_window(GtkWidget * window, int flag);
void unregister_toplevel_window(GtkWidget * window);
void toplevel_window_foreach(int flag, void (* callback)(GtkWidget * window));


enum {
	FILE_CHOOSER_FILTER_NONE = 0,
	FILE_CHOOSER_FILTER_AUDIO,
	FILE_CHOOSER_FILTER_PLAYLIST,
	FILE_CHOOSER_FILTER_STORE
};


#ifdef HAVE_SYSTRAY
int get_systray_semaphore(void);
#endif /* HAVE_SYSTRAY */

gint aqualung_dialog_run(GtkDialog * dialog);

GtkWidget* gui_stock_label_button(gchar *label, const gchar *stock);

void deflicker(void);

void directory_chooser(char * title, GtkWidget * parent, char * directory);
GSList * file_chooser(char * title, GtkWidget * parent,
		      GtkFileChooserAction action, int filter, gint multiple);
void file_chooser_with_entry(char * title, GtkWidget * parent,
			     GtkFileChooserAction action, int filter, GtkWidget * entry);

int message_dialog(char * title, GtkWidget * parent, GtkMessageType type,
		   GtkButtonsType buttons, GtkWidget * extra, char * text, ...);

void insert_label_entry(GtkWidget * table, char * ltext, GtkWidget ** entry,
			char * etext, int y1, int y2, gboolean editable);

void insert_label_entry_browse(GtkWidget * table, char * ltext, GtkWidget ** entry,
			       char * etext, int y1, int y2, 
			       void (* browse_cb)(GtkButton * button, gpointer data));

void insert_label_entry_button(GtkWidget * table, char * ltext, GtkWidget ** entry,
			       char * etext, GtkWidget * button, int y1, int y2);

void insert_label_spin(GtkWidget * table, char * ltext, GtkWidget ** spin,
		       int spinval, int y1, int y2);

void insert_label_spin_with_limits(GtkWidget * table, char * ltext, GtkWidget ** spin,
				   int spinval, int min, int max, int y1, int y2);

void set_option_from_toggle(GtkWidget * widget, int * opt);
void set_option_from_combo(GtkWidget * widget, int * opt);
void set_option_from_spin(GtkWidget * widget, int * opt);
void set_option_from_entry(GtkWidget * widget, char * opt, int n);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _UTILS_GUI_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8:

