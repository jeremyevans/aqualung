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

#include <gtk/gtk.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "gui_main.h"
#include "i18n.h"
#include "skin.h"


extern char confdir[MAXLEN];
extern GtkWidget * main_window;

char skin[MAXLEN];

GtkWidget * skin_window;
GtkListStore * skin_store;
GtkTreeSelection * skin_select;


static int
filter(const struct dirent * de) {

	return de->d_type == DT_DIR && de->d_name[0] != '.';
}


static gint
cancel(GtkWidget * widget, gpointer data) {

	gtk_widget_destroy(skin_window);
	return TRUE;
}

static gint
apply(GtkWidget * widget, gpointer data) {

	GtkTreeIter iter;
	char * str;
	int i = 0;

	do {
		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(skin_store), &iter, NULL, i++);

	} while (!gtk_tree_selection_iter_is_selected(skin_select, &iter));
	
	gtk_tree_model_get(GTK_TREE_MODEL(skin_store), &iter, 1, &str, -1);

	gtk_widget_destroy(skin_window);
	strcpy(skin, str);
	g_free(str);

	change_skin(skin);

	return TRUE;
}


void
create_skin_window() {

	GtkWidget * vbox;
	GtkWidget * viewp;
	GtkWidget * scrolledwin;

	GtkWidget * skin_list;
	GtkTreeIter iter;
	GtkTreeViewColumn *column;
	GtkCellRenderer * renderer;

	GtkWidget * hbox;
	GtkWidget * apply_btn;
	GtkWidget * cancel_btn;

	struct dirent ** ent;
	int n;


	skin_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_transient_for(GTK_WINDOW(skin_window), GTK_WINDOW(main_window));
	gtk_widget_set_size_request(skin_window, 250, 150);
        gtk_window_set_title(GTK_WINDOW(skin_window), _("Skin chooser"));
	gtk_window_set_position(GTK_WINDOW(skin_window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(skin_window), TRUE);
        gtk_container_set_border_width(GTK_CONTAINER(skin_window), 2);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(skin_window), vbox);

	viewp = gtk_viewport_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), viewp, TRUE, TRUE, 0);
	
	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(viewp), scrolledwin);

	skin_store = gtk_list_store_new(2,
					G_TYPE_STRING,  /* skin name */
					G_TYPE_STRING); /* path */
        skin_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(skin_store));
        skin_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(skin_list));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Available skins"), renderer, "text", 0, NULL);
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(skin_store), 0, GTK_SORT_ASCENDING);
  
	gtk_tree_view_append_column(GTK_TREE_VIEW(skin_list), column);
	gtk_container_add(GTK_CONTAINER(scrolledwin), skin_list);

	/* per-user skins */
	n = scandir(confdir, &ent, filter, alphasort);
	if (n >= 0) {
		int c;
		char path[MAXLEN];

		for (c = 0; c < n; ++c) {
			gtk_list_store_append(skin_store, &iter);
			snprintf(path, MAXLEN - 1, "%s/%s", confdir, ent[c]->d_name);
			gtk_list_store_set(skin_store, &iter, 0, ent[c]->d_name, 1, path, -1);
		}
	}

	/* system wide skins */
	n = scandir(SKINDIR, &ent, filter, alphasort);
	if (n >= 0) {
		int c;
		char path[MAXLEN];

		for (c = 0; c < n; ++c) {
			int found = 0;

			if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(skin_store), &iter)) {
				int i = 0;
				char * str;

				do {
					gtk_tree_model_get(GTK_TREE_MODEL(skin_store), &iter, 0, &str, -1);
					if (strcmp(str, ent[c]->d_name) == 0) {
						found = 1;
					}
					g_free(str);
					
				} while (i++, gtk_tree_model_iter_next(GTK_TREE_MODEL(skin_store), &iter));
			}

			if (!found) {
				gtk_list_store_append(skin_store, &iter);
				snprintf(path, MAXLEN - 1, "%s/%s", SKINDIR, ent[c]->d_name);
				gtk_list_store_set(skin_store, &iter, 0, ent[c]->d_name, 1, path, -1);
			}
		}
	}

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);

	cancel_btn = gtk_button_new_with_label(_("Cancel"));
	gtk_widget_set_size_request(cancel_btn, 60, 30);
	g_signal_connect(cancel_btn, "clicked", G_CALLBACK(cancel), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), cancel_btn, FALSE, FALSE, 3);

	apply_btn = gtk_button_new_with_label(_("Apply"));
	gtk_widget_set_size_request(apply_btn, 60, 30);
	g_signal_connect(apply_btn, "clicked", G_CALLBACK(apply), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), apply_btn, FALSE, FALSE, 3);

	gtk_widget_show_all(skin_window);

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(skin_store), &iter)) {
		int i = 0;
		char * str;
		
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(skin_store), &iter, 1, &str, -1);
			if (strcmp(str, skin) == 0) {
				gtk_tree_selection_select_iter(skin_select, &iter);
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(skin_list),
					 gtk_tree_model_get_path(GTK_TREE_MODEL(skin_store), &iter),
					 NULL, FALSE);
			}
			g_free(str);

		} while (i++, gtk_tree_model_iter_next(GTK_TREE_MODEL(skin_store), &iter));
	}
}
