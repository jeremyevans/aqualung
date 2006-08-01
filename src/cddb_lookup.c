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

#ifdef HAVE_CDDB

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <cddb/cddb.h>

#include "common.h"
#include "i18n.h"
#include "options.h"
#include "decoder/file_decoder.h"
#include "music_browser.h"
#include "build_store.h"
#include "cddb_lookup.h"

#define CDDB_THREAD_ERROR   -1
#define CDDB_THREAD_BUSY     0
#define CDDB_THREAD_SUCCESS  1
#define CDDB_THREAD_FREE     2

extern options_t options;

extern GtkWidget* gui_stock_label_button(gchar *blabel, const gchar *bstock);

extern GtkWidget * browser_window;
extern GtkTreeStore * music_store;
extern GtkTreeSelection * music_select;


pthread_t cddb_thread_id;

int cddb_thread_state = CDDB_THREAD_FREE;

int cddb_query_aborted = 0;

GtkTreeIter iter_record;

GtkWidget * progress_win = NULL;
GtkWidget * progress_label;
GtkWidget * progbar;
int progress_counter = 0;
int progress_prev = 0;

GtkWidget * combo;
GtkListStore * track_store;

GtkWidget * artist_entry;
GtkWidget * title_entry;
GtkWidget * year_entry;
GtkWidget * category_entry;
GtkWidget * genre_entry;
GtkWidget * ext_entry;

int * frames = NULL;
int record_length;
int track_count;

cddb_disc_t ** records = NULL;
int record_count;


static void
abort_cb(GtkWidget * widget, gpointer data) {

	cddb_query_aborted = 1;
}


static void
create_progress_window(void) {

	GtkWidget * vbox;
	GtkWidget * hbuttonbox;
	GtkWidget * hseparator;
	GtkWidget * abort_button;


	progress_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(progress_win), _("CDDB query"));
        gtk_window_set_position(GTK_WINDOW(progress_win), GTK_WIN_POS_CENTER);
        gtk_window_resize(GTK_WINDOW(progress_win), 330, 120);
        g_signal_connect(G_OBJECT(progress_win), "delete_event",
			 G_CALLBACK(abort_cb), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(progress_win), 10);
	
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(progress_win), vbox);

	progress_label = gtk_label_new(_("Retrieving matches from server..."));
	gtk_box_pack_start(GTK_BOX(vbox), progress_label, FALSE, FALSE, 0);
	
	progbar = gtk_progress_bar_new();
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), _("Connecting to CDDB server..."));
	gtk_box_pack_start(GTK_BOX(vbox), progbar, FALSE, FALSE, 6);
	
        hseparator = gtk_hseparator_new ();
        gtk_widget_show (hseparator);
        gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, TRUE, 5);

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        abort_button = gui_stock_label_button(_("Abort"), GTK_STOCK_CANCEL);
        g_signal_connect(G_OBJECT(abort_button), "clicked",
			 G_CALLBACK(abort_cb), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), abort_button);   

        gtk_widget_grab_focus(abort_button);

	gtk_widget_show_all(progress_win);
}


static void
destroy_progress_window() {

	gtk_widget_destroy(progress_win);
	progress_win = NULL;
}


static int
init_query_data() {

	GtkTreeIter iter_track;
        GtkTreeModel * model;

	char * pfile;
	char file[MAXLEN];

	float duration;
	int i;
	float length = 0.0f;
	float fr_offset = 150.0f; /* leading 2 secs in frames */


        if (gtk_tree_selection_get_selected(music_select, &model, &iter_record)) {

		track_count = 0;
		while (gtk_tree_model_iter_nth_child(model, &iter_track, &iter_record, track_count++));
		track_count--;

		if ((frames = (int *)malloc(sizeof(int) * track_count)) == NULL) {
			fprintf(stderr, "cddb_lookup.c: init_query_data(): malloc error\n");
			return 1;
		}

		for (i = 0; gtk_tree_model_iter_nth_child(model, &iter_track, &iter_record, i); i++) {

			gtk_tree_model_get(model, &iter_track, 2, &pfile, 4, &duration, -1);

			strncpy(file, pfile, MAXLEN-1);
			g_free(pfile);

			if (duration == 0.0f) {
				if ((duration = get_file_duration(file)) == 0.0f) {
					return 1;
				}
				gtk_tree_store_set(music_store, &iter_track, 4, duration, -1);
				music_store_mark_changed();
			}

			frames[i] = (int)fr_offset;

			length += duration;
			fr_offset += duration * 75.0f;
		}

		record_length = length;
	}

	return 0;
}

static int
init_query_data_from_tracklist(build_track_t * tracks) {

	int i;
	float length = 0.0f;
	float fr_offset = 150.0f; /* leading 2 secs in frames */

	build_track_t * ptrack = NULL;

	for (track_count = 0, ptrack = tracks; ptrack; track_count++, ptrack = ptrack->next);

	if ((frames = (int *)malloc(sizeof(int) * track_count)) == NULL) {
		fprintf(stderr, "cddb_lookup.c: init_query_data_from_tracklist(): malloc error\n");
		return 1;
	}

	for (i = 0, ptrack = tracks; ptrack; i++, ptrack = ptrack->next) {

		frames[i] = (int)fr_offset;
		length += ptrack->duration;
		fr_offset += ptrack->duration * 75.0f;
	}

	record_length = length;

	return 0;
}


static void
load_disc(cddb_disc_t * disc) {

	GtkTreeIter iter;
	char str[MAXLEN];
	int i;

	gtk_entry_set_text(GTK_ENTRY(artist_entry), cddb_disc_get_artist(disc));
	gtk_entry_set_text(GTK_ENTRY(title_entry), cddb_disc_get_title(disc));

	snprintf(str, MAXLEN-1, "%d", cddb_disc_get_year(disc));
	gtk_entry_set_text(GTK_ENTRY(year_entry), str);

	gtk_entry_set_text(GTK_ENTRY(category_entry), cddb_disc_get_category_str(disc));
	gtk_entry_set_text(GTK_ENTRY(genre_entry), cddb_disc_get_genre(disc));
	gtk_entry_set_text(GTK_ENTRY(ext_entry), cddb_disc_get_ext_data(disc));

	gtk_list_store_clear(GTK_LIST_STORE(track_store));

	for (i = 0; i < track_count; i++) {
		gtk_list_store_append(track_store, &iter);
		gtk_list_store_set(track_store, &iter,
				   0, cddb_track_get_title(cddb_disc_get_track(disc, i)), -1);
	}
}


static void
add_to_comments(GtkWidget * widget, gpointer data) {

	char * pcomment;
	char comment[MAXLEN];

	if (data == NULL) return;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record, 3, &pcomment, -1);

	comment[0] = '\0';

	if (pcomment != NULL) {
		strncpy(comment, pcomment, MAXLEN-1);
	}
	strncat(comment, (char *)data, MAXLEN-1);

	gtk_tree_store_set(music_store, &iter_record, 3, comment, -1);
	tree_selection_changed_cb(music_select, NULL);

	music_store_mark_changed();
}


static void
import_as_artist(GtkWidget * widget, gpointer data) {

	GtkTreeIter iter;
	GtkTreePath * path;

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), &iter_record);
	gtk_tree_path_up(path);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter, path);
	gtk_tree_store_set(music_store, &iter, 0, gtk_entry_get_text(GTK_ENTRY(artist_entry)), -1);

	music_store_mark_changed();
}


static void
import_as_title(GtkWidget * widget, gpointer data) {

	gtk_tree_store_set(music_store, &iter_record, 0, gtk_entry_get_text(GTK_ENTRY(title_entry)), -1);
	music_store_mark_changed();
}


static void
changed_combo(GtkWidget * widget, gpointer * data) {

	int i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

	if (i >= 0 && i < record_count) {
		load_disc(records[i]);
	}
}


static void
cell_edited_callback(GtkCellRendererText * cell, gchar * path, gchar * text, gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(track_store), &iter, path)) {
		gtk_list_store_set(track_store, &iter, 0, text, -1);
	}
}

static void
create_cddb_dialog(void) {

	GtkWidget * dialog;
	GtkWidget * table;

	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * button;

	GtkWidget * track_list;
	GtkWidget * viewport;
	GtkWidget * scrollwin;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTreeIter iter_track;
	GtkTreeIter iter_trlist;

	int i;
	char text[MAXLEN];


        dialog = gtk_dialog_new_with_buttons(_("CDDB query"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
	gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);

	table = gtk_table_new(8, 3, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, FALSE, 2);


	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Matches:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	combo = gtk_combo_box_new_text();
	for (i = 0; i < record_count; i++) {
		snprintf(text, MAXLEN, "%s: %s (%s)",
			 cddb_disc_get_artist(records[i]),
			 cddb_disc_get_title(records[i]),
			 cddb_disc_get_category_str(records[i]));
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo), text);
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	g_signal_connect(combo, "changed", G_CALLBACK(changed_combo), NULL);
			 
	gtk_table_attach(GTK_TABLE(table), combo, 1, 3, 0, 1, GTK_FILL, GTK_FILL, 5, 3);


	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Artist:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	artist_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), artist_entry, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);
	button = gtk_button_new_with_label(_("Import as Artist"));
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(import_as_artist), NULL);
	gtk_table_attach(GTK_TABLE(table), button, 2, 3, 1, 2, GTK_FILL, GTK_FILL, 5, 3);


	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Title:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	title_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), title_entry, 1, 2, 2, 3,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);
	button = gtk_button_new_with_label(_("Import as Title"));
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(import_as_title), NULL);
	gtk_table_attach(GTK_TABLE(table), button, 2, 3, 2, 3, GTK_FILL, GTK_FILL, 5, 3);


	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Year:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	year_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), year_entry, 1, 2, 3, 4,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);
	button = gtk_button_new_with_label(_("Add to Comments"));
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(add_to_comments), (gpointer)gtk_entry_get_text(GTK_ENTRY(year_entry)));
	gtk_table_attach(GTK_TABLE(table), button, 2, 3, 3, 4, GTK_FILL, GTK_FILL, 5, 3);


	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 4, 5, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Category:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	category_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), category_entry, 1, 2, 4, 5,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);
	button = gtk_button_new_with_label(_("Add to Comments"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(add_to_comments),
			 (gpointer)gtk_entry_get_text(GTK_ENTRY(category_entry)));
	gtk_table_attach(GTK_TABLE(table), button, 2, 3, 4, 5, GTK_FILL, GTK_FILL, 5, 3);


	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 5, 6, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Genre:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	genre_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), genre_entry, 1, 2, 5, 6,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);
	button = gtk_button_new_with_label(_("Add to Comments"));
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(add_to_comments), (gpointer)gtk_entry_get_text(GTK_ENTRY(genre_entry)));
	gtk_table_attach(GTK_TABLE(table), button, 2, 3, 5, 6, GTK_FILL, GTK_FILL, 5, 3);


	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 6, 7, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Extended data:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	ext_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), ext_entry, 1, 2, 6, 7,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);
	button = gtk_button_new_with_label(_("Add to Comments"));
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(add_to_comments), (gpointer)gtk_entry_get_text(GTK_ENTRY(ext_entry)));
	gtk_table_attach(GTK_TABLE(table), button, 2, 3, 6, 7, GTK_FILL, GTK_FILL, 5, 3);


	track_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_BOOLEAN);
        track_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(track_store));

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "editable", TRUE, NULL);
	g_signal_connect(renderer, "edited", (GCallback)cell_edited_callback, NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Tracks"),
							  renderer,
							  "text", 0,
							  NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(track_list), column);
        gtk_tree_view_set_reorderable(GTK_TREE_VIEW(track_list), FALSE);

        viewport = gtk_viewport_new(NULL, NULL);
	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(scrollwin, -1, 200);
        gtk_table_attach(GTK_TABLE(table), viewport, 0, 3, 7, 8,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);
        gtk_container_add(GTK_CONTAINER(viewport), scrollwin);
        gtk_container_add(GTK_CONTAINER(scrollwin), track_list);

	gtk_widget_show_all(dialog);

	load_disc(records[0]);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		for (i = 0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
							  &iter_track, &iter_record, i); i++) {

			if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(track_store),
							  &iter_trlist, NULL, i)) {
				char * name;
				gtk_tree_model_get(GTK_TREE_MODEL(track_store),
						   &iter_trlist, 0, &name, -1);
				gtk_tree_store_set(music_store, &iter_track,
						   0, name, -1);
			}
		}

		tree_selection_changed_cb(music_select, NULL);
		music_store_mark_changed();
	}

	gtk_widget_destroy(dialog);
}


void *
cddb_thread(void * arg) {

	cddb_conn_t * conn = NULL;
	cddb_disc_t * disc = NULL;
	cddb_track_t * track = NULL;

	int i;


	if ((conn = cddb_new()) == NULL) {
		fprintf(stderr, "cddb_lookup.c: cddb_thread(): cddb_new error\n");
		cddb_thread_state = CDDB_THREAD_ERROR;
		return 0;
	}

	cddb_set_server_name(conn, options.cddb_server);
	cddb_set_timeout(conn, options.cddb_timeout);

	if (options.cddb_use_http) {
		cddb_http_enable(conn);
		cddb_set_server_port(conn, 80);
	}

	if ((disc = cddb_disc_new()) == NULL) {
		fprintf(stderr, "cddb_lookup.c: cddb_thread(): cddb_disc_new error\n");
		cddb_thread_state = CDDB_THREAD_ERROR;
		return 0;
	}

	cddb_disc_set_length(disc, record_length);

	for (i = 0; i < track_count; i++) {
		track = cddb_track_new();
		cddb_track_set_frame_offset(track, frames[i]);
		cddb_disc_add_track(disc, track);
	}

	record_count = cddb_query(conn, disc);

	if (record_count < 0) {
		cddb_thread_state = CDDB_THREAD_ERROR;
		return 0;
	}

	if (record_count == 0) {
		cddb_destroy(conn);
		cddb_disc_destroy(disc);

		if (cddb_query_aborted) {
			free(frames);
		}

		libcddb_shutdown();

		cddb_thread_state = CDDB_THREAD_SUCCESS;
		return 0;
	}

	if ((records = (cddb_disc_t **)malloc(sizeof(cddb_disc_t *) * record_count)) == NULL) {
		fprintf(stderr, "cddb_lookup.c: cddb_thread(): malloc error\n");
		cddb_thread_state = CDDB_THREAD_ERROR;
		return 0;
	}

	cddb_read(conn, disc);
	records[0] = cddb_disc_clone(disc);

	progress_counter = 1;

	for (i = 1; i < record_count && !cddb_query_aborted; i++) {
		cddb_query_next(conn, disc);
		cddb_read(conn, disc);
		records[i] = cddb_disc_clone(disc);

		progress_counter = i + 1;
	}

	cddb_destroy(conn);
	cddb_disc_destroy(disc);

	if (cddb_query_aborted) {
		int j;
		for (j = 0; j < i; j++) {
			cddb_disc_destroy(records[j]);
		}

		libcddb_shutdown();

		free(records);
		free(frames);
		
		return 0;
	}

	cddb_thread_state = CDDB_THREAD_SUCCESS;
	return 0;
}


static gint
cddb_timeout_callback(gpointer data) {

	int i;
	char text[MAXLEN];

	if (cddb_query_aborted) {
		destroy_progress_window();
		cddb_thread_state = CDDB_THREAD_FREE;
		return FALSE;
	}

	if (cddb_thread_state == 0) {
		if (progress_prev < progress_counter) {
			snprintf(text, MAXLEN, _("%d of %d"), progress_counter, record_count);
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progbar),
						      (double)progress_counter / record_count);
			gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progbar), text);
			progress_prev = progress_counter;
		}
		return TRUE;
	} else {
		destroy_progress_window();

		if (cddb_thread_state == -1) {
			GtkWidget * dialog;

                        dialog = gtk_message_dialog_new(GTK_WINDOW(browser_window),
                                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, 
                                             _("An error occurred while attempting "
						"to connect to the CDDB server."));
			gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
			gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
			gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
		
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);

		} else if (cddb_thread_state == 1) {

			if (record_count == 0) {
				GtkWidget * dialog;

				dialog = gtk_message_dialog_new(GTK_WINDOW(browser_window),
		        			     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, 
                                                     _("No matching record found."));
				gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
				gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
				gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
		
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);

			} else {

				create_cddb_dialog();

				for (i = 0; i < record_count; i++) {
					cddb_disc_destroy(records[i]);
				}

				free(records);
				libcddb_shutdown();
			}
		}
	}

	free(frames);

	cddb_thread_state = CDDB_THREAD_FREE;

	return FALSE;
}


void
cddb_get() {

	cddb_thread_state = CDDB_THREAD_BUSY;

	if (init_query_data()) {
		cddb_thread_state = CDDB_THREAD_FREE;
		return;
	}

	create_progress_window();

	cddb_query_aborted = 0;
	progress_counter = 0;
	progress_prev = 0;
	pthread_create(&cddb_thread_id, NULL, cddb_thread, NULL);

	g_timeout_add(100, cddb_timeout_callback, NULL);
}

void
cddb_get_batch(build_record_t * record, int cddb_title, int cddb_artist, int cddb_record) {

	int i, j;
	build_track_t * ptrack = NULL;

	map_t * map_artist = NULL;
	map_t * map_record = NULL;
	map_t * map_year = NULL;
	map_t ** map_tracks = NULL;

	char tmp[MAXLEN];


	cddb_thread_state = CDDB_THREAD_BUSY;

	if (init_query_data_from_tracklist(record->tracks)) {
		cddb_thread_state = CDDB_THREAD_FREE;
		return;
	}

	cddb_query_aborted = 0;
	progress_counter = 0;
	progress_prev = 0;

	cddb_thread(NULL);

	if (cddb_thread_state != CDDB_THREAD_SUCCESS || record_count == 0) {
		cddb_thread_state = CDDB_THREAD_FREE;
		return;
	}

	if ((map_tracks = (map_t **)malloc(sizeof(map_t *) * track_count)) == NULL) {
		fprintf(stderr, "cddb_lookup.c: cddb_get_batch(): malloc error\n");
		cddb_thread_state = CDDB_THREAD_FREE;
		return;
	}

	for (i = 0; i < track_count; i++) {
		map_tracks[i] = NULL;
	}

	for (i = 0; i < record_count; i++) {

		if (cddb_artist && !record->artist_valid) {
			strncpy(tmp, cddb_disc_get_artist(records[i]), MAXLEN-1);
			map_put(&map_artist, tmp);
		}

		if (cddb_record && !record->record_valid) {
			strncpy(tmp, cddb_disc_get_title(records[i]), MAXLEN-1);
			map_put(&map_record, tmp);
		}

		if (!record->year_valid) {
			snprintf(tmp, MAXLEN-1, "%d", cddb_disc_get_year(records[i]));
			map_put(&map_year, tmp);
		}
		
		if (cddb_title) {
			for (j = 0, ptrack = record->tracks;
			     ptrack && j < track_count; j++, ptrack = ptrack->next) {
				if (!ptrack->valid) {
					strncpy(tmp,
						cddb_track_get_title(cddb_disc_get_track(records[i], j)),
						MAXLEN-1);
					map_put(map_tracks + j, tmp);
				}
			}
		}
	}

	if (map_artist) {
		char * max = map_get_max(map_artist);

		if (max) {
			strncpy(record->artist, max, MAXLEN-1);
			record->artist_valid = 1;
		}
	}

	if (map_record) {
		char * max = map_get_max(map_record);

		if (max) {
			strncpy(record->record, max, MAXLEN-1);
			record->record_valid = 1;
		}
	}

	if (map_year) {
		char * max = map_get_max(map_year);

		if (max) {
			strncpy(record->year, max, MAXLEN-1);
			record->year_valid = 1;
		}
	}

	for (j = 0, ptrack = record->tracks; ptrack && j < track_count; j++, ptrack = ptrack->next) {

		if (!ptrack->valid) {
			char * max = map_get_max(map_tracks[j]);

			if (max) {
				strncpy(ptrack->name, max, MAXLEN-1);
				ptrack->valid = 1;
			}
		}

		map_free(map_tracks[j]);
	}

	map_free(map_artist);
	map_free(map_record);
	map_free(map_year);
	free(map_tracks);

	for (i = 0; i < record_count; i++) {
		cddb_disc_destroy(records[i]);
	}

	free(records);
	libcddb_shutdown();
	free(frames);

	cddb_thread_state = CDDB_THREAD_FREE;
}

#endif /* HAVE_CDDB */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

