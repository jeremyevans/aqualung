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
#include "music_browser.h"
#include "store_file.h"
#include "i18n.h"
#include "options.h"
#include "search.h"


extern options_t options;

extern GtkTreeStore * music_store;
extern GtkWidget * music_tree;

extern GtkWidget * browser_window;

GtkWidget * search_window = NULL;
static int search_store_quit = 0;
GtkWidget * searchkey_entry;

GtkWidget * check_case;
GtkWidget * check_exact;
GtkWidget * check_sfac;

GtkWidget * check_artist;
GtkWidget * check_record;
GtkWidget * check_track;
GtkWidget * check_comment;
GtkWidget * sres_list;

GtkListStore * search_store;
GtkTreeSelection * search_select;

int casesens;
int exactonly;
int selectfc;
int artist_yes;
int record_yes;
int track_yes;
int comment_yes;

static void
get_toggle_buttons_state(void) {

        casesens = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_case)) ? 1 : 0;
	exactonly = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_exact)) ? 1 : 0;
	selectfc = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_sfac)) ? 1 : 0;
	artist_yes = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_artist)) ? 1 : 0;
	record_yes = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_record)) ? 1 : 0;
	track_yes = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_track)) ? 1 : 0;
	comment_yes = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_comment)) ? 1 : 0;
}

void
clear_search_store(void) {

	int i;
	GtkTreeIter iter;

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(search_store), &iter, NULL, i++)) {

		gpointer gptr;
		GtkTreePath * path;

		gtk_tree_model_get(GTK_TREE_MODEL(search_store), &iter,
				   3, &gptr, -1);

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
        options.search_ms_flags = (casesens * SEARCH_F_CS) | (exactonly * SEARCH_F_EM) | (selectfc * SEARCH_F_SF) |
                          (artist_yes * SEARCH_F_AN) | (record_yes * SEARCH_F_RT) | (track_yes * SEARCH_F_TT) |
                          (comment_yes * SEARCH_F_CO);

	search_store_quit = 1;
	clear_search_store();
        gtk_widget_destroy(search_window);
        search_window = NULL;
        return TRUE;
}


int
search_window_close(GtkWidget * widget, gpointer * data) {

        get_toggle_buttons_state();
        options.search_ms_flags = (casesens * SEARCH_F_CS) | (exactonly * SEARCH_F_EM) | (selectfc * SEARCH_F_SF) |
                          (artist_yes * SEARCH_F_AN) | (record_yes * SEARCH_F_RT) | (track_yes * SEARCH_F_TT) |
                          (comment_yes * SEARCH_F_CO);

	search_store_quit = 1;
	clear_search_store();
        search_window = NULL;
        return 0;
}

static gint
sfac_clicked(GtkWidget * widget, gpointer data) {

        get_toggle_buttons_state();

        if (selectfc) {

                gtk_widget_hide(sres_list);
                gtk_window_resize(GTK_WINDOW(search_window), 420, 215);

        } else {

                gtk_window_resize(GTK_WINDOW(search_window), 420, 430);
                gtk_widget_show(sres_list);

        }

        return TRUE;
}

static gint
search_button_clicked(GtkWidget * widget, gpointer data) {

	int valid;
	const char * key_string = gtk_entry_get_text(GTK_ENTRY(searchkey_entry));
	char key[MAXLEN];
	GPatternSpec * pattern;

	int h, i, j, k;
	GtkTreeIter store_iter;
	GtkTreeIter artist_iter;
	GtkTreeIter record_iter;
	GtkTreeIter track_iter;
	GtkTreeIter sfac_iter;

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
		arr_strlcpy(key, key_string);
	} else {
		arr_snprintf(key, "*%s*", key_string);
	}

	pattern = g_pattern_spec_new(key);


	h = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &store_iter, NULL, h++)) {

		if (iter_get_store_type(&store_iter) != STORE_TYPE_FILE) {
			continue;
		}

		i = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     &artist_iter, &store_iter, i++)) {

			char * artist_name;
			artist_data_t * artist_data;


			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &artist_iter,
					   MS_COL_NAME, &artist_name,
					   MS_COL_DATA, &artist_data, -1);

			if (artist_yes) {
				char * tmp = NULL;
				if (casesens) {
					tmp = strdup(artist_name);
				} else {
					tmp = g_utf8_strup(artist_name, -1);
				}
				if (g_pattern_spec_match_string(pattern, tmp)) {

					GtkTreeIter iter;
					GtkTreePath * path;

					path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store),
								       &artist_iter);
					gtk_list_store_append(search_store, &iter);
					gtk_list_store_set(search_store, &iter,
							   0, artist_name,
							   1, "",
							   2, "",
							   3, (gpointer)path,
							   -1);
				}
				g_free(tmp);
			}

			if (comment_yes && artist_data->comment != NULL) {
				char * tmp = NULL;
				if (casesens) {
					tmp = strdup(artist_data->comment);
				} else {
					tmp = g_utf8_strup(artist_data->comment, -1);
				}
				if (g_pattern_spec_match_string(pattern, tmp)) {

					GtkTreeIter iter;
					GtkTreePath * path;

					path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store),
								       &artist_iter);
					gtk_list_store_append(search_store, &iter);
					gtk_list_store_set(search_store, &iter,
							   0, artist_name,
							   1, "",
							   2, "",
							   3, (gpointer)path,
							   -1);
				}
				g_free(tmp);
			}

			j = 0;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &record_iter,
							     &artist_iter, j++)) {

				char * record_name;
				record_data_t * record_data;

				gtk_tree_model_get(GTK_TREE_MODEL(music_store), &record_iter,
						   MS_COL_NAME, &record_name,
						   MS_COL_DATA, &record_data, -1);
				
				if (record_yes) {
					char * tmp = NULL;
					if (casesens) {
						tmp = strdup(record_name);
					} else {
						tmp = g_utf8_strup(record_name, -1);
					}
					if (g_pattern_spec_match_string(pattern, tmp)) {
						
						GtkTreeIter iter;
						GtkTreePath * path;
						
						path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store),
									       &record_iter);
						gtk_list_store_append(search_store, &iter);
						gtk_list_store_set(search_store, &iter,
								   0, artist_name,
								   1, record_name,
								   2, "",
								   3, (gpointer)path,
								   -1);
					}
					g_free(tmp);
				}
				
				if (comment_yes && record_data->comment != NULL) {
					char * tmp = NULL;
					if (casesens) {
						tmp = strdup(record_data->comment);
					} else {
						tmp = g_utf8_strup(record_data->comment, -1);
					}
					if (g_pattern_spec_match_string(pattern, tmp)) {
						
						GtkTreeIter iter;
						GtkTreePath * path;
						
						path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store),
									       &record_iter);
						gtk_list_store_append(search_store, &iter);
						gtk_list_store_set(search_store, &iter,
								   0, artist_name,
								   1, record_name,
								   2, "",
								   3, (gpointer)path,
								   -1);
					}
					g_free(tmp);
				}
				
				k = 0;
				while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
								     &track_iter, &record_iter, k++)) {
					
					char * track_name;
					track_data_t * track_data;
					
					gtk_tree_model_get(GTK_TREE_MODEL(music_store), &track_iter,
							   MS_COL_NAME, &track_name,
							   MS_COL_DATA, &track_data, -1);
					
					if (track_yes) {
						char * tmp = NULL;
						if (casesens) {
							tmp = strdup(track_name);
						} else {
							tmp = g_utf8_strup(track_name, -1);
						}
						if (g_pattern_spec_match_string(pattern, tmp)) {
							
							GtkTreeIter iter;
							GtkTreePath * path;
							
							path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store),
										       &track_iter);
							gtk_list_store_append(search_store, &iter);
							gtk_list_store_set(search_store, &iter,
									   0, artist_name,
									   1, record_name,
									   2, track_name,
									   3, (gpointer)path,
									   -1);
						}
						g_free(tmp);
					}
					
					if (comment_yes && track_data->comment != NULL) {
						char * tmp = NULL;
						if (casesens) {
							tmp = strdup(track_data->comment);
						} else {
							tmp = g_utf8_strup(track_data->comment, -1);
						}
						if (g_pattern_spec_match_string(pattern, tmp)) {
							
							GtkTreeIter iter;
							GtkTreePath * path;
							
							path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store),
										       &track_iter);
							gtk_list_store_append(search_store, &iter);
							gtk_list_store_set(search_store, &iter,
									   0, artist_name,
									   1, record_name,
									   2, track_name,
									   3, (gpointer)path,
									   -1);
						}
						g_free(tmp);
					}

					g_free(track_name);
				}
				g_free(record_name);
			}
			g_free(artist_name);
		}
	}
		
	g_pattern_spec_free(pattern);
	
	if (selectfc) {
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(search_store), &sfac_iter) == TRUE) {
			gtk_tree_selection_select_iter(search_select, &sfac_iter);
		}
		
		close_button_clicked(NULL, NULL);
	}

	return TRUE;
}


void
search_selection_changed(GtkTreeSelection * treeselection, gpointer user_data) {

	GtkTreeIter iter;
	GtkTreePath * path;
	gpointer gptr;

	if (search_store_quit)
		return;

	if (!gtk_tree_selection_get_selected(search_select, NULL, &iter)) {
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(search_store), &iter,
			   3, &gptr, -1);

	path = (GtkTreePath *)gptr;
	gtk_tree_view_collapse_all(GTK_TREE_VIEW(music_tree));
	gtk_tree_view_expand_to_path(GTK_TREE_VIEW(music_tree), path);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(music_tree), path, NULL, TRUE, 0.5f, 0.5f);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(music_tree), path, NULL, FALSE);
}


gint
search_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	switch (kevent->keyval) {

	case GDK_KEY_Escape:
		close_button_clicked(NULL, NULL);
		return TRUE;
		break;

	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		search_button_clicked(NULL, NULL);
		return TRUE;
		break;
	}

	return FALSE;
}


void
search_dialog(void) {

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
	search_store_quit = 0;

        search_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(search_window), _("Search the Music Store"));
        gtk_window_set_position(GTK_WINDOW(search_window), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for(GTK_WINDOW(search_window), GTK_WINDOW(browser_window));
        g_signal_connect(G_OBJECT(search_window), "delete_event",
                         G_CALLBACK(search_window_close), NULL);
        g_signal_connect(G_OBJECT(search_window), "key_press_event",
			 G_CALLBACK(search_window_key_pressed), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(search_window), 5);

        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_show(vbox);
        gtk_container_add(GTK_CONTAINER(search_window), vbox);


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_show(hbox);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Key: "));
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

        searchkey_entry = gtk_entry_new();
        gtk_widget_show(searchkey_entry);
        gtk_box_pack_start(GTK_BOX(hbox), searchkey_entry, TRUE, TRUE, 5);


	table = gtk_table_new(5, 2, FALSE);
        gtk_widget_show(table);
        gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, TRUE, 3);

	check_case = gtk_check_button_new_with_label(_("Case sensitive"));
        gtk_widget_show(check_case);     
	gtk_widget_set_name(check_case, "check_on_window");
	gtk_table_attach(GTK_TABLE(table), check_case, 0, 1, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 1, 5);

	check_exact = gtk_check_button_new_with_label(_("Exact matches only"));
        gtk_widget_show(check_exact);     
	gtk_widget_set_name(check_exact, "check_on_window");
	gtk_table_attach(GTK_TABLE(table), check_exact, 1, 2, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 1, 5);

	check_sfac = gtk_check_button_new_with_label(_("Select first and close window"));
        gtk_widget_show(check_sfac);     
	gtk_widget_set_name(check_sfac, "check_on_window");
        g_signal_connect(G_OBJECT(check_sfac), "clicked", G_CALLBACK(sfac_clicked), NULL);
	gtk_table_attach(GTK_TABLE(table), check_sfac, 0, 1, 1, 2,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 1, 5);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_show(hbox);     
	label = gtk_label_new(_("Search in:"));
        gtk_widget_show(label);     
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 2, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 1, 5);


	check_artist = gtk_check_button_new_with_label(_("Artist names"));
        gtk_widget_show(check_artist);     
	gtk_widget_set_name(check_artist, "check_on_window");
	gtk_table_attach(GTK_TABLE(table), check_artist, 0, 1, 3, 4,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 1, 1);

	check_record = gtk_check_button_new_with_label(_("Record titles"));
        gtk_widget_show(check_record);     
	gtk_widget_set_name(check_record, "check_on_window");
	gtk_table_attach(GTK_TABLE(table), check_record, 0, 1, 4, 5,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 1, 1);

	check_track = gtk_check_button_new_with_label(_("Track titles"));
        gtk_widget_show(check_track);     
	gtk_widget_set_name(check_track, "check_on_window");
	gtk_table_attach(GTK_TABLE(table), check_track, 1, 2, 3, 4,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 1, 1);

	check_comment = gtk_check_button_new_with_label(_("Comments"));
        gtk_widget_show(check_comment);     
	gtk_widget_set_name(check_comment, "check_on_window");
	gtk_table_attach(GTK_TABLE(table), check_comment, 1, 2, 4, 5,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 1, 1);

	hbox = sres_list = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
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
					  G_TYPE_STRING,   /* artist */
					  G_TYPE_STRING,   /* record */
					  G_TYPE_STRING,   /* track */
					  G_TYPE_POINTER); /* * GtkTreePath */

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
        search_column = gtk_tree_view_column_new_with_attributes(_("Artist"),
                                                             search_renderer,
                                                             "text", 0,
                                                             NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(search_column),
                                        GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(search_column), TRUE);
        gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(search_column), 0);
        gtk_tree_view_append_column(GTK_TREE_VIEW(search_list), search_column);

        search_renderer = gtk_cell_renderer_text_new();
        search_column = gtk_tree_view_column_new_with_attributes(_("Record"),
                                                             search_renderer,
                                                             "text", 1,
                                                             NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(search_column),
                                        GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(search_column), TRUE);
        gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(search_column), 1);
        gtk_tree_view_append_column(GTK_TREE_VIEW(search_list), search_column);

        search_renderer = gtk_cell_renderer_text_new();
        search_column = gtk_tree_view_column_new_with_attributes(_("Track"),
                                                             search_renderer,
                                                             "text", 2,
                                                             NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(search_column),
                                        GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(search_column), TRUE);
        gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(search_column), 2);
        gtk_tree_view_append_column(GTK_TREE_VIEW(search_list), search_column);

	hbuttonbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
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

        if (options.search_ms_flags & SEARCH_F_CS)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_case), TRUE);
        if (options.search_ms_flags & SEARCH_F_EM)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_exact), TRUE);
        if (options.search_ms_flags & SEARCH_F_SF)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_sfac), TRUE);
        if (options.search_ms_flags & SEARCH_F_AN)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_artist), TRUE);
        if (options.search_ms_flags & SEARCH_F_RT)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_record), TRUE);
        if (options.search_ms_flags & SEARCH_F_TT)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_track), TRUE);
        if (options.search_ms_flags & SEARCH_F_CO)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_comment), TRUE);

	gtk_widget_show(search_window);
}


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

