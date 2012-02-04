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

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "common.h"
#include "utils_gui.h"
#include "playlist.h"
#include "options.h"
#include "i18n.h"
#include "search_playlist.h"


enum {
	SEARCH_F_CS = (1 << 0),  /* case sensitive */
	SEARCH_F_EM = (1 << 1),  /* exact matches only */
	SEARCH_F_SF = (1 << 2)   /* select first and close */
};

extern options_t options;

extern GtkWidget * playlist_window;
extern GtkWidget * main_window;
extern GList * playlists;

static GtkWidget * search_window = NULL;
static GtkWidget * searchkey_entry;

static GtkWidget * check_case;
static GtkWidget * check_exact;
static GtkWidget * check_sfac;
static GtkWidget * sres_list;

static GtkListStore * search_store;
static GtkTreeSelection * search_select;

static int casesens;
static int exactonly;
static int selectfc;

static void
get_toggle_buttons_state(void) {

        casesens = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_case)) ? 1 : 0;
	exactonly = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_exact)) ? 1 : 0;
	selectfc = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_sfac)) ? 1 : 0;
}

static void
clear_search_store(void) {

	int i;
	GtkTreeIter iter;

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(search_store), &iter, NULL, i++)) {

		gpointer gptr;
		GtkTreePath * path;

		gtk_tree_model_get(GTK_TREE_MODEL(search_store), &iter,
				   1, &gptr, -1);

		path = (GtkTreePath *)gptr;

		if (path != NULL) {
			gtk_tree_path_free(path);
		}
	}

	gtk_list_store_clear(search_store);
}


static gint
close_button_clicked(GtkWidget * widget, gpointer data) {

        get_toggle_buttons_state();
        options.search_pl_flags = (casesens * SEARCH_F_CS) | (exactonly * SEARCH_F_EM) | (selectfc * SEARCH_F_SF);

	clear_search_store();
        gtk_widget_destroy(search_window);
        search_window = NULL;
        return TRUE;
}


static int
search_window_close(GtkWidget * widget, gpointer * data) {

        get_toggle_buttons_state();
        options.search_pl_flags = (casesens * SEARCH_F_CS) | (exactonly * SEARCH_F_EM) | (selectfc * SEARCH_F_SF);

	clear_search_store();
        search_window = NULL;
        return 0;
}

static gint
sfac_clicked(GtkWidget * widget, gpointer data) {

        get_toggle_buttons_state();

        if (selectfc) {
                gtk_widget_hide(sres_list);
                gtk_window_resize(GTK_WINDOW(search_window), 420, 138);

        } else {
                gtk_window_resize(GTK_WINDOW(search_window), 420, 355);
                gtk_widget_show(sres_list);
        }

        return TRUE;
}


void
search_foreach(playlist_t * pl, GPatternSpec * pattern, GtkTreeIter * list_iter, int album_node) {

	char text[MAXLEN];
	char * tmp = NULL;
	playlist_data_t * pldata;

	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), list_iter, PL_COL_DATA, &pldata, -1);
	if (album_node) {
		snprintf(text, MAXLEN-1, "%s: %s", pldata->artist, pldata->album);
	} else {
		playlist_data_get_display_name(text, pldata);
	}

	if (casesens) {
		tmp = strdup(text);
	} else {
		tmp = g_utf8_strup(text, -1);
	}

	if (g_pattern_match_string(pattern, tmp)) {

		GtkTreeIter iter;
		GtkTreePath * path;

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(pl->store), list_iter);
		gtk_list_store_append(search_store, &iter);
		gtk_list_store_set(search_store, &iter, 0, text,
				   1, (gpointer)path, 2, pl->name, 3, (gpointer)pl, -1);
	}

	g_free(tmp);
}

static gint
search_button_clicked(GtkWidget * widget, gpointer data) {

	int valid;
	const char * key_string = gtk_entry_get_text(GTK_ENTRY(searchkey_entry));
	char key[MAXLEN];
	GPatternSpec * pattern;

	int i;
	GtkTreeIter list_iter;
	GtkTreeIter sfac_iter;

	GList * node = NULL;


        get_toggle_buttons_state();

	clear_search_store();

	valid = 0;
	for (i = 0; key_string[i] != '\0'; i++) {
		if ((key_string[i] != '?') && (key_string[i] != '*')) {
			valid = 1;
			break;
		}
	}
	if (!valid) {
		return TRUE;
	}

	if (!casesens) {
		key_string = g_utf8_strup(key_string, -1);
	}

	if (exactonly) {
		strcpy(key, key_string);
	} else {
		snprintf(key, MAXLEN-1, "*%s*", key_string);
	}

	pattern = g_pattern_spec_new(key);

	for (node = playlists; node; node = node->next) {

		playlist_t * pl = (playlist_t *)node->data;

		i = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store),
						     &list_iter, NULL, i++)) {

			if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), &list_iter)) {

				int j = 0;
				GtkTreeIter iter;

				search_foreach(pl, pattern, &list_iter, 1/*album node*/);

				while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store),
								     &iter, &list_iter, j++)) {
					search_foreach(pl, pattern, &iter, 0/*track node*/);
				}
			} else {
				search_foreach(pl, pattern, &list_iter, 0/*track node*/);
			}
		}
	}

	g_pattern_spec_free(pattern);

        if (selectfc) {
                
                if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(search_store), &sfac_iter) == TRUE)
                        gtk_tree_selection_select_iter(search_select, &sfac_iter);

                close_button_clicked(NULL, NULL);
        }

        return TRUE;
}


static void
search_selection_changed(GtkTreeSelection * treeselection, gpointer user_data) {

	GtkTreeIter iter;
	GtkTreePath * path;
	GtkTreePath * path_up;
	gpointer gptr1;
	gpointer gptr2;
	playlist_t * pl;

	if (!gtk_tree_selection_get_selected(search_select, NULL, &iter)) {
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(search_store), &iter,
			   1, &gptr1, 3, &gptr2, -1);

	path = (GtkTreePath *)gptr1;
	path_up = gtk_tree_path_copy(path);

	pl = (playlist_t *)gptr2;

	if (gtk_tree_path_up(path_up)) {
		gtk_tree_view_expand_row(GTK_TREE_VIEW(pl->view), path_up, FALSE);
	}

	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(pl->view), path, NULL, TRUE, 0.5f, 0.5f);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(pl->view), path, NULL, FALSE);

	playlist_set_current(pl);

	gtk_tree_path_free(path_up);
}


static gint
search_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	switch (kevent->keyval) {

	case GDK_Escape:
		close_button_clicked(NULL, NULL);
		return TRUE;
		break;

	case GDK_Return:
	case GDK_KP_Enter:
		search_button_clicked(NULL, NULL);
                return TRUE;
		break;
	}

	return FALSE;
}


void
search_playlist_dialog(void) {

	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * button;
	GtkWidget * table;
	GtkWidget * hbuttonbox;

        GtkWidget * search_viewport;
        GtkWidget * search_scrwin;
        GtkWidget * search_list;
        GtkCellRenderer * search_renderer;
        GtkTreeViewColumn * search_column;


        if (search_window != NULL) {
		return;
        }

        search_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(search_window), _("Search the Playlist"));
        gtk_window_set_position(GTK_WINDOW(search_window), GTK_WIN_POS_CENTER_ON_PARENT);

	if (options.playlist_is_embedded) {
		gtk_window_set_transient_for(GTK_WINDOW(search_window), GTK_WINDOW(main_window));
	} else {
		gtk_window_set_transient_for(GTK_WINDOW(search_window), GTK_WINDOW(playlist_window));
	}

	gtk_window_set_modal(GTK_WINDOW(search_window), TRUE);
        g_signal_connect(G_OBJECT(search_window), "delete_event",
                         G_CALLBACK(search_window_close), NULL);
        g_signal_connect(G_OBJECT(search_window), "key_press_event",
			 G_CALLBACK(search_window_key_pressed), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(search_window), 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_widget_show(vbox);
        gtk_container_add(GTK_CONTAINER(search_window), vbox);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_widget_show(hbox);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Key: "));
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

        searchkey_entry = gtk_entry_new();
        gtk_widget_show(searchkey_entry);
        gtk_box_pack_start(GTK_BOX(hbox), searchkey_entry, TRUE, TRUE, 5);


	table = gtk_table_new(4, 2, FALSE);
        gtk_widget_show(table);
        gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, TRUE, 3);

	check_case = gtk_check_button_new_with_label(_("Case sensitive"));
        gtk_widget_show(check_case);
	gtk_widget_set_name(check_case, "check_on_window");
	gtk_table_attach(GTK_TABLE(table), check_case, 0, 1, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 1, 4);

	check_exact = gtk_check_button_new_with_label(_("Exact matches only"));
        gtk_widget_show(check_exact);     
        gtk_widget_set_name(check_exact, "check_on_window");
	gtk_table_attach(GTK_TABLE(table), check_exact, 1, 2, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 1, 4);

	check_sfac = gtk_check_button_new_with_label(_("Select first and close window"));
        gtk_widget_show(check_sfac);     
	gtk_widget_set_name(check_sfac, "check_on_window");
        g_signal_connect(G_OBJECT(check_sfac), "clicked", G_CALLBACK(sfac_clicked), NULL);
	gtk_table_attach(GTK_TABLE(table), check_sfac, 0, 1, 1, 2,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 1, 4);


	hbox = sres_list = gtk_hbox_new(FALSE, 0);
        gtk_widget_show(hbox);     
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 3);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 3);

	search_viewport = gtk_viewport_new(NULL, NULL);
        gtk_widget_show(search_viewport);     
        gtk_box_pack_start(GTK_BOX(hbox), search_viewport, TRUE, TRUE, 0);

        search_scrwin = gtk_scrolled_window_new(NULL, NULL);
        gtk_widget_show(search_scrwin);     
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(search_scrwin),
                                       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(search_viewport), search_scrwin);


        search_store = gtk_list_store_new(4,
					  G_TYPE_STRING,   /* title */
					  G_TYPE_POINTER,  /* GtkTreePath */
					  G_TYPE_STRING,   /* playlist name */
					  G_TYPE_POINTER); /* playlist_t pointer */

        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(search_store), 0, GTK_SORT_ASCENDING);

        search_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(search_store));
        gtk_widget_show(search_list);     
        gtk_widget_set_size_request(search_list, 400, 200);
        gtk_container_add(GTK_CONTAINER(search_scrwin), search_list);
        search_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(search_list));
        gtk_tree_selection_set_mode(search_select, GTK_SELECTION_SINGLE);
        g_signal_connect(G_OBJECT(search_select), "changed",
                         G_CALLBACK(search_selection_changed), NULL);

        search_renderer = gtk_cell_renderer_text_new();
        search_column = gtk_tree_view_column_new_with_attributes(_("Playlist"),
                                                             search_renderer,
                                                             "text", 2,
                                                             NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(search_column),
                                        GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(search_column), TRUE);
        gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(search_column), 2);
        gtk_tree_view_append_column(GTK_TREE_VIEW(search_list), search_column);

        search_renderer = gtk_cell_renderer_text_new();
        search_column = gtk_tree_view_column_new_with_attributes(_("Title"),
                                                             search_renderer,
                                                             "text", 0,
                                                             NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(search_column),
                                        GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(search_column), TRUE);
        gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(search_column), 0);
        gtk_tree_view_append_column(GTK_TREE_VIEW(search_list), search_column);

	hbuttonbox = gtk_hbutton_box_new();
        gtk_widget_show (hbuttonbox);
        gtk_box_set_spacing(GTK_BOX(hbuttonbox), 8);
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        button = gui_stock_label_button(_("Search"), GTK_STOCK_FIND);
        gtk_widget_show(button);     
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), button);   
        g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(search_button_clicked), NULL);

        button = gtk_button_new_from_stock (GTK_STOCK_CLOSE); 
        gtk_widget_show(button);     
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), button);   
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(close_button_clicked), NULL);


        if (options.search_pl_flags & SEARCH_F_CS)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_case), TRUE);
        if (options.search_pl_flags & SEARCH_F_EM)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_exact), TRUE);
        if (options.search_pl_flags & SEARCH_F_SF)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_sfac), TRUE);

        gtk_widget_show(search_window);
}


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

