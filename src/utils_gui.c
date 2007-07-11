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

#include "utils_gui.h"


#ifdef HAVE_SYSTRAY
int systray_semaphore = 0;
#endif /* HAVE_SYSTRAY */


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

