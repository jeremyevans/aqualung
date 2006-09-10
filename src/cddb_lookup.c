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
#include <cddb/cddb.h>

#ifdef _WIN32
#include <glib.h>
#else
#include <pthread.h>
#endif /* _WIN32 */

#include "common.h"
#include "i18n.h"
#include "options.h"
#include "decoder/file_decoder.h"
#include "gui_main.h"
#include "music_browser.h"
#include "build_store.h"
#include "cddb_lookup.h"
#include "version.h"

#define CASE_UP    0
#define CASE_DOWN  1

extern options_t options;

extern GtkWidget* gui_stock_label_button(gchar *blabel, const gchar *bstock);

extern GtkWidget * browser_window;
extern GtkTreeStore * music_store;
extern GtkTreeSelection * music_select;


AQUALUNG_THREAD_DECLARE(cddb_thread_id)

int cddb_thread_state = CDDB_THREAD_FREE;

volatile int cddb_query_aborted = 0;
volatile int cddb_data_locked = 0;

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
GtkWidget * year_spinner;
GtkWidget * category_entry;
GtkWidget * category_combo;
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

	char * pfile;
	char file[MAXLEN];

	int i;
	float duration = 0.0f;
	float length = 0.0f;
	float fr_offset = SECONDS_TO_FRAMES(2.0f); /* leading 2 secs in frames */


	track_count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter_record);

	if ((frames = (int *)malloc(sizeof(int) * track_count)) == NULL) {
		fprintf(stderr, "cddb_lookup.c: init_query_data(): malloc error\n");
		return 1;
	}

	for (i = 0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						  &iter_track, &iter_record, i); i++) {

		gtk_tree_model_get(GTK_TREE_MODEL(music_store),
				   &iter_track, 2, &pfile, 4, &duration, -1);

		strncpy(file, pfile, MAXLEN-1);
		g_free(pfile);

		if (duration == 0.0f) {
			if ((duration = get_file_duration(file)) == 0.0f) {
				return 1;
			}
			gtk_tree_store_set(music_store, &iter_track, 4, duration, -1);
			music_store_mark_changed(&iter_track);
		}

		frames[i] = (int)fr_offset;

		length += duration;
		fr_offset += SECONDS_TO_FRAMES(duration);
	}

	record_length = length;

	return 0;
}

static int
init_query_data_from_tracklist(build_track_t * tracks) {

	int i;
	float length = 0.0f;
	float fr_offset = SECONDS_TO_FRAMES(2.0f); /* leading 2 secs in frames */

	build_track_t * ptrack = NULL;

	for (track_count = 0, ptrack = tracks; ptrack; track_count++, ptrack = ptrack->next);

	if ((frames = (int *)malloc(sizeof(int) * track_count)) == NULL) {
		fprintf(stderr, "cddb_lookup.c: init_query_data_from_tracklist(): malloc error\n");
		return 1;
	}

	for (i = 0, ptrack = tracks; ptrack; i++, ptrack = ptrack->next) {

		frames[i] = (int)fr_offset;
		length += ptrack->duration;
		fr_offset += SECONDS_TO_FRAMES(ptrack->duration);
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

	music_store_mark_changed(&iter_record);
}


static void
import_as_artist(GtkWidget * widget, gpointer data) {

	GtkTreeIter iter;
	GtkTreePath * path;

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), &iter_record);
	gtk_tree_path_up(path);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter, path);
	gtk_tree_store_set(music_store, &iter, 0, gtk_entry_get_text(GTK_ENTRY(artist_entry)), -1);

	music_store_mark_changed(&iter);
}


static void
import_as_title(GtkWidget * widget, gpointer data) {

	gtk_tree_store_set(music_store, &iter_record, 0, gtk_entry_get_text(GTK_ENTRY(title_entry)), -1);
	music_store_mark_changed(&iter_record);
}

static void
import_as_sort_key(GtkWidget * widget, gpointer data) {

	gtk_tree_store_set(music_store, &iter_record, 1, gtk_entry_get_text(GTK_ENTRY(year_entry)), -1);
	music_store_mark_changed(&iter_record);
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
		snprintf(text, MAXLEN, "%d. %s: %s (%s)",
			 i + 1,
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
	button = gtk_button_new_with_label(_("Import as Sort Key"));
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(import_as_sort_key), NULL);
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

	if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		for (i = 0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
							  &iter_track, &iter_record, i); i++) {

			if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(track_store),
							  &iter_trlist, NULL, i)) {
				char * name;
				gtk_tree_model_get(GTK_TREE_MODEL(track_store),
						   &iter_trlist, 0, &name, -1);
				gtk_tree_store_set(music_store, &iter_track,
						   0, name, -1);
				g_free(name);
				music_store_mark_changed(&iter_track);
			}
		}

		tree_selection_changed_cb(music_select, NULL);
	}

	gtk_widget_destroy(dialog);
}


gboolean
create_cddb_write_error_dialog(gpointer data) {

	GtkWidget * dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW(browser_window),
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, 
					_("An error occurred while attempting "
					  "to submit the record to the CDDB server.\n"
					  "Make sure you provided an email address for "
					  "submission, or try to change the access protocol."));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
	
	aqualung_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	return FALSE;
}

int
create_cddb_write_warn_dialog(char * text) {

	int ret;
	GtkWidget * dialog;
	GtkWidget * label;

        dialog = gtk_dialog_new_with_buttons(_("Warning"),
					     NULL,
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
	
	label = gtk_label_new(text);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		ret = 0;
	} else {
		ret = 1;
	}

	gtk_widget_destroy(dialog);

	return ret;
}

int
check_case(char * text, int _case) {

	char * str;
	char * p;
	int has = 0;
	int ret = 0;

	for (p = text; *p; p = g_utf8_next_char(p)) {

		gunichar ch = g_utf8_get_char(p);

		if (g_unichar_islower(ch) || g_unichar_isupper(ch)) {
			has = 1;
			break;
		}
	}

	if (!has) {
		return 1;
	}

	switch (_case) {
	case CASE_UP:
		str = g_utf8_strup(text, -1);
		ret = strcmp(str, text);
		g_free(str);
		break;
	case CASE_DOWN:
		str = g_utf8_strdown(text, -1);
		ret = strcmp(str, text);
		g_free(str);
		break;
	}

	return ret;
}

static gboolean
create_cddb_submit_dialog(gpointer data) {

	GtkWidget * dialog;
	GtkWidget * table;

	GtkWidget * hbox;
	GtkWidget * label;

	GtkWidget * track_list;
	GtkWidget * viewport;
	GtkWidget * scrollwin;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTreeIter iter_artist;
	GtkTreeIter iter_track;
	GtkTreeIter iter_trlist;

	int i;
	char * str;
	int year;

	cddb_disc_t * disc = (cddb_disc_t *)data;

        dialog = gtk_dialog_new_with_buttons(_("CDDB submission"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_widget_set_size_request(dialog, 400, -1);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
	gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);

	table = gtk_table_new(7, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, FALSE, 2);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Artist:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	artist_entry = gtk_entry_new();

	gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_artist, &iter_record);
	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_artist, 0, &str, -1);
	gtk_entry_set_text(GTK_ENTRY(artist_entry), str);
	g_free(str);

	gtk_table_attach(GTK_TABLE(table), artist_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Title:"));

        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	title_entry = gtk_entry_new();

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record, 0, &str, -1);
	gtk_entry_set_text(GTK_ENTRY(title_entry), str);
	g_free(str);

	gtk_table_attach(GTK_TABLE(table), title_entry, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Year:"));

        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	year_spinner = gtk_spin_button_new_with_range(YEAR_MIN, YEAR_MAX, 1);

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record, 1, &str, -1);
	year = strtol(str, NULL, 10);
	g_free(str);

	if (!is_valid_year(year)) {
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record, 3, &str, -1);
		year = strtol(str, NULL, 10);
		g_free(str);
	}

	if (is_valid_year(year)) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(year_spinner), year);
	}

	gtk_table_attach(GTK_TABLE(table), year_spinner, 1, 2, 2, 3,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Category:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	category_combo = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(category_combo), _("(choose a category)"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(category_combo), _("data (ISO9660 and other data CDs)"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(category_combo), _("folk"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(category_combo), _("jazz"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(category_combo), _("miscellaneous"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(category_combo), _("rock"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(category_combo), _("country"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(category_combo), _("blues"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(category_combo), _("newage"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(category_combo), _("reagge"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(category_combo), _("classical"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(category_combo), _("soundtrack (movies, shows)"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(category_combo), 0);
	gtk_table_attach(GTK_TABLE(table), category_combo, 1, 2, 3, 4,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 4, 5, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Genre:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	genre_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), genre_entry, 1, 2, 4, 5,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 5, 6, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Extended data:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	ext_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), ext_entry, 1, 2, 5, 6,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);

	track_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_BOOLEAN);
        track_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(track_store));

	for (i = 0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						  &iter_track, &iter_record, i); i++) {
		char * name;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track, 0, &name, -1);
		gtk_list_store_append(track_store, &iter_trlist);
		gtk_list_store_set(track_store, &iter_trlist, 0, name, -1);
		g_free(name);
	}

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
        gtk_table_attach(GTK_TABLE(table), viewport, 0, 2, 6, 7,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);
        gtk_container_add(GTK_CONTAINER(viewport), scrollwin);
        gtk_container_add(GTK_CONTAINER(scrollwin), track_list);

	gtk_widget_show_all(dialog);

 display:

	if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * artist;
		char * title;
		char * genre;
		int category;

		artist = (char *)gtk_entry_get_text(GTK_ENTRY(artist_entry));
		if (is_all_wspace(artist)) {
			gtk_widget_grab_focus(artist_entry);
			goto display;
		}

		if (!check_case(artist, CASE_DOWN)) {
			if (create_cddb_write_warn_dialog(_("Artist appears to be in all lowercase.\n"
							    "Do you want to proceed?"))) {
				goto display;
			}
		}

		if (!check_case(artist, CASE_UP)) {
			if (create_cddb_write_warn_dialog(_("Artist appears to be in all uppercase.\n"
							    "Do you want to proceed?"))) {
				goto display;
			}
		}

		title = (char *)gtk_entry_get_text(GTK_ENTRY(title_entry));
		if (is_all_wspace(title)) {
			gtk_widget_grab_focus(title_entry);
			goto display;
		}

		if (!check_case(title, CASE_DOWN)) {
			if (create_cddb_write_warn_dialog(_("Title appears to be in all lowercase.\n"
							    "Do you want to proceed?"))) {
				goto display;
			}
		}

		if (!check_case(title, CASE_UP)) {
			if (create_cddb_write_warn_dialog(_("Title appears to be in all uppercase.\n"
							    "Do you want to proceed?"))) {
				goto display;
			}
		}

		category = gtk_combo_box_get_active(GTK_COMBO_BOX(category_combo));
		if (category == 0) {
			gtk_widget_grab_focus(category_combo);
			goto display;
		}

		genre = (char *)gtk_entry_get_text(GTK_ENTRY(genre_entry));
		if (genre[0] != '\0' && is_all_wspace(genre)) {
			gtk_widget_grab_focus(genre_entry);
			goto display;
		}

		for (i = 0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(track_store),
							  &iter_trlist, NULL, i); i++) {
			char * name;
			gtk_tree_model_get(GTK_TREE_MODEL(track_store),
					   &iter_trlist, 0, &name, -1);

			if (is_all_wspace(name)) {
				gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(track_list)), &iter_trlist);
				goto display;
			}

			cddb_track_set_title(cddb_disc_get_track(disc, i), name);
			g_free(name);
		}

		cddb_disc_set_artist(disc, artist);
		cddb_disc_set_title(disc, title);
		cddb_disc_set_year(disc, gtk_spin_button_get_value(GTK_SPIN_BUTTON(year_spinner)));
		cddb_disc_set_category(disc, category - 1);
		cddb_disc_set_genre(disc, genre);
		cddb_disc_set_ext_data(disc, gtk_entry_get_text(GTK_ENTRY(ext_entry)));

		cddb_thread_state = CDDB_THREAD_SUCCESS;
	} else {
		cddb_thread_state = CDDB_THREAD_ERROR;
	}

	gtk_widget_destroy(dialog);

	cddb_data_locked = 0;

	return FALSE;
}


int
cddb_connection_setup(cddb_conn_t ** conn) {

	if ((*conn = cddb_new()) == NULL) {
		fprintf(stderr, "cddb_lookup.c: cddb_connection_setup(): cddb_new error\n");
		cddb_thread_state = CDDB_THREAD_ERROR;
		return 1;
	}

	cddb_set_server_name(*conn, options.cddb_server);
	cddb_set_timeout(*conn, options.cddb_timeout);
	cddb_set_client(*conn, "Aqualung", aqualung_version);

	if (options.cddb_email[0] != '\0') {
		cddb_set_email_address(*conn, options.cddb_email);
	}

	if (options.cddb_use_proxy) {
		cddb_http_proxy_enable(*conn);
		cddb_set_http_proxy_server_name(*conn, options.cddb_proxy);
		cddb_set_http_proxy_server_port(*conn, options.cddb_proxy_port);
	} else {
		cddb_http_proxy_disable(*conn);
		if (options.cddb_use_http) {
			cddb_http_enable(*conn);
			cddb_set_server_port(*conn, 80);
		} else {
			cddb_http_disable(*conn);
			cddb_set_server_port(*conn, 888);
		}
	}

	return 0;
}

void *
cddb_thread(void * arg) {

	cddb_conn_t * conn = NULL;
	cddb_disc_t * disc = NULL;
	cddb_track_t * track = NULL;

	int i;


	if (cddb_connection_setup(&conn) == 1) {
		return NULL;
	}

	if ((disc = cddb_disc_new()) == NULL) {
		fprintf(stderr, "cddb_lookup.c: cddb_thread(): cddb_disc_new error\n");
		cddb_thread_state = CDDB_THREAD_ERROR;
		return NULL;
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
		return NULL;
	}

	if (record_count == 0) {
		cddb_destroy(conn);
		cddb_disc_destroy(disc);

		if (cddb_query_aborted) {
			free(frames);
		}

		libcddb_shutdown();

		cddb_thread_state = CDDB_THREAD_SUCCESS;
		return NULL;
	}

	if ((records = (cddb_disc_t **)malloc(sizeof(cddb_disc_t *) * record_count)) == NULL) {
		fprintf(stderr, "cddb_lookup.c: cddb_thread(): malloc error\n");
		cddb_thread_state = CDDB_THREAD_ERROR;
		return NULL;
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
		
		return NULL;
	}

	cddb_thread_state = CDDB_THREAD_SUCCESS;
	return NULL;
}


void *
cddb_submit_thread(void * arg) {

	cddb_conn_t * conn = NULL;
	cddb_disc_t * disc = NULL;
	cddb_track_t * track = NULL;

	int i;

	struct timespec req_time;
	struct timespec rem_time;

	req_time.tv_sec = 0;
        req_time.tv_nsec = 500000000;

	if (cddb_connection_setup(&conn) == 1) {
		cddb_destroy(conn);
		libcddb_shutdown();
		free(frames);
		cddb_thread_state = CDDB_THREAD_FREE;
		return NULL;
	}

	if ((disc = cddb_disc_new()) == NULL) {
		fprintf(stderr, "cddb_lookup.c: cddb_submit_thread(): cddb_disc_new error\n");
		cddb_destroy(conn);
		libcddb_shutdown();
		free(frames);
		cddb_thread_state = CDDB_THREAD_FREE;
		return NULL;
	}

	cddb_disc_set_length(disc, record_length);

	for (i = 0; i < track_count; i++) {
		track = cddb_track_new();
		cddb_track_set_frame_offset(track, frames[i]);
		cddb_disc_add_track(disc, track);
	}

	if (cddb_disc_calc_discid(disc) == 0) {
		fprintf(stderr, "cddb_lookup.c: cddb_submit_thread(): cddb_disc_calc_discid error\n");
		cddb_disc_destroy(disc);
		cddb_destroy(conn);
		libcddb_shutdown();
		free(frames);
		cddb_thread_state = CDDB_THREAD_FREE;
		return NULL;
	}

	cddb_data_locked = 1;
	g_idle_add(create_cddb_submit_dialog, (gpointer) disc);

	while (cddb_data_locked) {
		nanosleep(&req_time, &rem_time);
	}

	if (cddb_thread_state == CDDB_THREAD_SUCCESS) {
		if (!cddb_write(conn, disc)) {
			g_idle_add(create_cddb_write_error_dialog, NULL);
		}
	}

	cddb_destroy(conn);
	cddb_disc_destroy(disc);

	libcddb_shutdown();

	free(frames);

	cddb_thread_state = CDDB_THREAD_FREE;

	return NULL;
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
		
			aqualung_dialog_run(GTK_DIALOG(dialog));
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
		
				aqualung_dialog_run(GTK_DIALOG(dialog));
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
cddb_get(GtkTreeIter * iter) {

	cddb_thread_state = CDDB_THREAD_BUSY;

	iter_record = *iter;

	if (init_query_data()) {
		cddb_thread_state = CDDB_THREAD_FREE;
		return;
	}

	create_progress_window();

	cddb_query_aborted = 0;
	progress_counter = 0;
	progress_prev = 0;
	AQUALUNG_THREAD_CREATE(cddb_thread_id, NULL, cddb_thread, NULL)

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
			int y = cddb_disc_get_year(records[i]);
			if (is_valid_year(y)) {
				snprintf(tmp, MAXLEN-1, "%d", y);
				map_put(&map_year, tmp);
			}
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

void
cddb_submit(GtkTreeIter * iter) {

	cddb_thread_state = CDDB_THREAD_BUSY;

	iter_record = *iter;

	if (init_query_data()) {
		cddb_thread_state = CDDB_THREAD_FREE;
		return;
	}

	AQUALUNG_THREAD_CREATE(cddb_thread_id, NULL, cddb_submit_thread, NULL)
}

#endif /* HAVE_CDDB */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

