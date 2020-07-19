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

#include <config.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "common.h"
#include "utils_gui.h"
#include "gui_main.h"
#include "playlist.h"
#include "options.h"
#include "i18n.h"
#include "skin.h"


extern options_t options;

extern GtkWidget * main_window;

char * pdir;

GtkWidget * skin_window;
GtkListStore * skin_store;
GtkTreeSelection * skin_select;


void
apply_skin_foreach(GtkWidget * window) {

	gtk_widget_reset_rc_styles(window);
	gtk_widget_queue_draw(window);
}

void
apply_skin(char * path) {

	char rcpath[MAXLEN];
	
	arr_snprintf(rcpath, "%s/rc", path);
	gtk_rc_parse(rcpath);

	toplevel_window_foreach(TOP_WIN_SKIN, apply_skin_foreach);

	main_buttons_set_content(path);

	playlist_set_color();
}

static int
filter(const struct dirent * de) {

	struct stat st_file;
	char dirname[MAXLEN];


	if (de->d_name[0] == '.') {
		return 0;
	}

	arr_snprintf(dirname, "%s/%s", pdir, de->d_name);
	if (stat(dirname, &st_file) == -1) {
		fprintf(stderr,
			"error %s: skin.c/filter(): stat() failed on %s [likely cause: nonexistent file]\n",
			strerror(errno), dirname);
		return 0;
	}

	return S_ISDIR(st_file.st_mode);
}


static gint
cancel(GtkWidget * widget, gpointer data) {

	gtk_widget_destroy(skin_window);
	return TRUE;
}

static gint
apply(GtkWidget * widget, gpointer data) {

	GtkTreeModel * model;
	GtkTreeIter iter;
	char * str;

	if (gtk_tree_selection_get_selected(skin_select, &model, &iter)) {
	
		gtk_tree_model_get(model, &iter, 1, &str, -1);
		strcpy(options.skin, str);
		g_free(str);

		gtk_widget_destroy(skin_window);
		apply_skin(options.skin);
	}

	return TRUE;
}

static gint
skin_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	switch (kevent->keyval) {

	case GDK_q:
	case GDK_Q:
	case GDK_Escape:
		cancel(NULL, NULL);
		return TRUE;
		break;

        case GDK_Return:
	case GDK_KP_Enter:
		apply(NULL, NULL);
                return TRUE;
		break;
	}

	return FALSE;
}

static gint
skin_list_double_click(GtkWidget * widget, GdkEventButton * event, gpointer func_data) {

	if ((event->type == GDK_2BUTTON_PRESS) && (event->button == 1)) {
		apply(NULL, NULL);
		return TRUE;
	}

	return FALSE;
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

	GtkWidget * hbuttonbox;
	GtkWidget * apply_btn;
	GtkWidget * cancel_btn;

	struct dirent ** ent;
	int n;


	skin_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_transient_for(GTK_WINDOW(skin_window), GTK_WINDOW(main_window));
	gtk_widget_set_size_request(skin_window, 250, 240);
        gtk_window_set_title(GTK_WINDOW(skin_window), _("Skin chooser"));
	gtk_window_set_position(GTK_WINDOW(skin_window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(skin_window), TRUE);
        gtk_container_set_border_width(GTK_CONTAINER(skin_window), 2);

        g_signal_connect(G_OBJECT(skin_window), "key_press_event",
			 G_CALLBACK(skin_window_key_pressed), NULL);

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
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(skin_list), FALSE);

        g_signal_connect(G_OBJECT(skin_list), "button_press_event", 
                         G_CALLBACK(skin_list_double_click), NULL);      

        skin_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(skin_list));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Available skins"), renderer, "text", 0, NULL);
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(skin_store), 0, GTK_SORT_ASCENDING);
  
	gtk_tree_view_append_column(GTK_TREE_VIEW(skin_list), column);
	gtk_container_add(GTK_CONTAINER(scrolledwin), skin_list);

	/* per-user skins */
	pdir = options.confdir;
	n = scandir(options.confdir, &ent, filter, alphasort);
	if (n >= 0) {
		int c;
		char path[MAXLEN];

		for (c = 0; c < n; ++c) {
                        gtk_list_store_append(skin_store, &iter);
                        arr_snprintf(path, "%s/%s", options.confdir, ent[c]->d_name);
                        gtk_list_store_set(skin_store, &iter, 0, ent[c]->d_name, 1, path, -1);
			free(ent[c]);
		}
		free(ent);
	}

	/* system wide skins */
	pdir = AQUALUNG_SKINDIR;
	n = scandir(AQUALUNG_SKINDIR, &ent, filter, alphasort);
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
                                if (strncmp(ent[c]->d_name, "no_skin", MAXLEN-1)) {
                                        gtk_list_store_append(skin_store, &iter);
                                        arr_snprintf(path, "%s/%s", AQUALUNG_SKINDIR, ent[c]->d_name);
                                        gtk_list_store_set(skin_store, &iter, 0, ent[c]->d_name, 1, path, -1);
                                }
			}
			free(ent[c]);
		}
		free(ent);
	}

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);
        gtk_box_set_spacing(GTK_BOX(hbuttonbox), 8);
        gtk_container_set_border_width(GTK_CONTAINER(hbuttonbox), 3);

        apply_btn = gtk_button_new_from_stock (GTK_STOCK_APPLY); 
	g_signal_connect(apply_btn, "clicked", G_CALLBACK(apply), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), apply_btn);   

        cancel_btn = gtk_button_new_from_stock (GTK_STOCK_CANCEL); 
	g_signal_connect(cancel_btn, "clicked", G_CALLBACK(cancel), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), cancel_btn);   

	gtk_widget_show_all(skin_window);

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(skin_store), &iter)) {
		int i = 0;
		char * str;
		
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(skin_store), &iter, 1, &str, -1);
			if (strcmp(str, options.skin) == 0) {
				gtk_tree_selection_select_iter(skin_select, &iter);
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(skin_list),
					 gtk_tree_model_get_path(GTK_TREE_MODEL(skin_store), &iter),
					 NULL, FALSE);
			}
			g_free(str);

		} while (i++, gtk_tree_model_iter_next(GTK_TREE_MODEL(skin_store), &iter));
	}
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

