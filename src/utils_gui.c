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

#include <config.h>

#include <string.h>

#include "common.h"
#include "options.h"
#include "utils_gui.h"


#ifdef HAVE_SYSTRAY
int systray_semaphore = 0;
#endif /* HAVE_SYSTRAY */

extern options_t options;


#ifdef HAVE_SYSTRAY
int
get_systray_semaphore() {

	return systray_semaphore;
}
#endif /* HAVE_SYSTRAY */


gint
aqualung_dialog_run(GtkDialog * dialog) {
#ifdef HAVE_SYSTRAY
	int ret;
	systray_semaphore++;
	gtk_window_present(GTK_WINDOW(dialog));
	ret = gtk_dialog_run(dialog);
	systray_semaphore--;
	return ret;
#else
	gtk_window_present(GTK_WINDOW(dialog));
	return gtk_dialog_run(dialog);
#endif /* HAVE_SYSTRAY */
}


/* create button with stock item 
 *
 * in: label - label for buttor        (label=NULL  to disable label, label=-1 to disable button relief)
 *     stock - stock icon identifier                                
 */
GtkWidget* 
gui_stock_label_button(gchar *label, const gchar *stock) {

	GtkWidget *button;
	GtkWidget *alignment;
	GtkWidget *hbox;
	GtkWidget *image;

	button = g_object_new (GTK_TYPE_BUTTON, "visible", TRUE, NULL);

        if (label== (gchar *)-1) {
                gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);  
        }

	alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (alignment), hbox);

	image = gtk_image_new_from_stock (stock, GTK_ICON_SIZE_BUTTON);

        if (image) {
		gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);
        }

        if (label != NULL && label != (gchar *)-1) {
		gtk_box_pack_start (GTK_BOX (hbox),
		g_object_new (GTK_TYPE_LABEL, "label", label, "use_underline", TRUE, NULL),
		FALSE, TRUE, 0);
        }

	gtk_widget_show_all (alignment);
	gtk_container_add (GTK_CONTAINER (button), alignment);

	return button;
}


void
deflicker(void) {
	while (g_main_context_iteration(NULL, FALSE));
}


void
file_chooser_with_entry(char * title, GtkWidget * parent, GtkFileChooserAction action, GtkWidget * entry) {

        GtkWidget * dialog;
	const gchar * selected_filename = gtk_entry_get_text(GTK_ENTRY(entry));

        dialog = gtk_file_chooser_dialog_new(title,
                                             GTK_WINDOW(parent),
					     action,
                                             GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), options.show_hidden);
	} 

        deflicker();

        if (strlen(selected_filename)) {
      		char * locale = g_locale_from_utf8(selected_filename, -1, NULL, NULL, NULL);
                char tmp[MAXLEN];
                tmp[0] = '\0';

		if (locale == NULL) {
			gtk_widget_destroy(dialog);
			return;
		}

		normalize_filename(locale, tmp);
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), tmp);
		g_free(locale);
	} else {
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
	}

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * utf8;

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		utf8 = g_locale_to_utf8(selected_filename, -1, NULL, NULL, NULL);

		if (utf8 == NULL) {
			gtk_widget_destroy(dialog);
		}

		gtk_entry_set_text(GTK_ENTRY(entry), utf8);

                strncpy(options.currdir, selected_filename, MAXLEN-1);
		g_free(utf8);
        }

        gtk_widget_destroy(dialog);
}
