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
#include "utils.h"
#include "utils_gui.h"
#include "i18n.h"
#include "options.h"
#include "music_browser.h"
#include "store_cdda.h"
#include "store_file.h"
#include "playlist.h"
#include "cdda.h"
#include "cddb_lookup.h"

extern options_t options;

extern GtkWidget * browser_window;
extern GtkTreeStore * music_store;
extern GtkTreeSelection * music_select;


enum {
	CASE_UP,
	CASE_DOWN
};

enum {
	CDDB_INIT,
	CDDB_BUSY,
	CDDB_SUCCESS,
	CDDB_ERROR,
	CDDB_NO_MATCH,
	CDDB_ABORTED
};

enum {
	CDDB_TYPE_QUERY,
	CDDB_TYPE_SUBMIT,
	CDDB_TYPE_SUBMIT_NEW
};

typedef struct {

	AQUALUNG_THREAD_DECLARE(thread_id);
	AQUALUNG_MUTEX_DECLARE(mutex);

	int state;
	int type;

	int ntracks;
	int * frames;
	int record_length;

	cddb_disc_t ** records;
	int nrecords;

	int counter;
	int aborted;

	GtkTreeIter iter_record;

	GtkWidget * progress_win;
	GtkWidget * progress_label;
	GtkWidget * progbar;

	GtkWidget * combo;

	GtkWidget * artist_entry;
	GtkWidget * title_entry;
	GtkWidget * year_spinner;
	GtkWidget * category_entry;
	GtkWidget * category_combo;
	GtkWidget * genre_entry;
	GtkWidget * ext_entry;
	GtkWidget * track_list;
	GtkListStore * track_store;

	GtkWidget * year_import_button;
	GtkWidget * title_import_button;

	int year_imported;
	int title_imported;

} cddb_lookup_t;


void cddb_dialog(cddb_lookup_t * data);
void cddb_dialog_load_disc(cddb_lookup_t * data, cddb_disc_t * disc);


const char *
notnull(const char * str) {

	return str ? str : "";
}

cddb_lookup_t *
cddb_lookup_new () {

	cddb_lookup_t * data;

	if ((data = calloc(1, sizeof(cddb_lookup_t))) == NULL) {
		fprintf(stderr, "cddb_lookup_new: calloc error\n");
		return NULL;
	}

#ifdef _WIN32
	data->mutex = g_mutex_new();
#endif /* _WIN32 */

	data->state = CDDB_INIT;

	return data;
}

void
cddb_lookup_set_state(cddb_lookup_t * data, int state) {

	AQUALUNG_MUTEX_LOCK(data->mutex);
	data->state = state;
	AQUALUNG_MUTEX_UNLOCK(data->mutex);
}

int
cddb_lookup_get_state(cddb_lookup_t * data) {

	int state;

	AQUALUNG_MUTEX_LOCK(data->mutex);
	state = data->state;
	AQUALUNG_MUTEX_UNLOCK(data->mutex);

	return state;
}

void
cddb_lookup_free(cddb_lookup_t * data) {

	if (data->records != NULL) {
		int i;
		for (i = 0; i < data->nrecords; i++) {
			if (data->records[i] != NULL) {
				cddb_disc_destroy(data->records[i]);
			}
		}
		free(data->records);
	}

	if (data->frames != NULL) {
		free(data->frames);
	}

#ifdef _WIN32
	g_mutex_free(data->mutex);
#endif /* _WIN32 */

	free(data);
}


int
cddb_connection_setup(cddb_conn_t ** conn) {

	if ((*conn = cddb_new()) == NULL) {
		fprintf(stderr, "cddb_lookup.c: cddb_connection_setup(): cddb_new error\n");
		return 1;
	}

	cddb_set_server_name(*conn, options.cddb_server);
	cddb_set_timeout(*conn, options.cddb_timeout);
	cddb_set_charset(*conn, "UTF-8");

	if (options.cddb_local[0] != '\0') {
		cddb_cache_set_dir(*conn, options.cddb_local);
	}

	if (options.cddb_cache_only) {
		cddb_cache_only(*conn);
	}

	if (options.inet_use_proxy) {
		cddb_http_enable(*conn);
		cddb_set_server_port(*conn, 80);
		cddb_http_proxy_enable(*conn);
		cddb_set_http_proxy_server_name(*conn, options.inet_proxy);
		cddb_set_http_proxy_server_port(*conn, options.inet_proxy_port);
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

void
cddb_lookup(cddb_lookup_t * data) {

	cddb_conn_t * conn = NULL;
	cddb_disc_t * disc = NULL;
	cddb_track_t * track = NULL;

	int i;

	if (cddb_connection_setup(&conn) == 1) {
		cddb_lookup_set_state(data, CDDB_ERROR);
		return;
	}

	if ((disc = cddb_disc_new()) == NULL) {
		fprintf(stderr, "cddb_lookup(): cddb_disc_new error\n");
		cddb_lookup_set_state(data, CDDB_ERROR);
		return;
	}

	cddb_disc_set_length(disc, data->record_length);

	for (i = 0; i < data->ntracks; i++) {
		track = cddb_track_new();
		cddb_track_set_frame_offset(track, data->frames[i]);
		cddb_disc_add_track(disc, track);
	}

	data->nrecords = cddb_query(conn, disc);

	if (data->nrecords <= 0) {
		cddb_destroy(conn);
		cddb_disc_destroy(disc);
		cddb_lookup_set_state(data, (data->nrecords == 0) ? CDDB_NO_MATCH : CDDB_ERROR);
		return;
	}

	cddb_lookup_set_state(data, CDDB_BUSY);

	if ((data->records = (cddb_disc_t **)calloc(data->nrecords, sizeof(cddb_disc_t *))) == NULL) {
		fprintf(stderr, "cddb_lookup(): malloc error\n");
		cddb_lookup_set_state(data, CDDB_ERROR);
		return;
	}

	for (i = 0; i < data->nrecords; i++) {

		if (cddb_lookup_get_state(data) == CDDB_ABORTED) {
			break;
		}

		if (i > 0 && !cddb_query_next(conn, disc)) {
			break;
		}
		cddb_read(conn, disc);
		data->records[i] = cddb_disc_clone(disc);

		AQUALUNG_MUTEX_LOCK(data->mutex);
		data->counter++;
		AQUALUNG_MUTEX_UNLOCK(data->mutex);
	}

	cddb_destroy(conn);
	cddb_disc_destroy(disc);

	if (cddb_lookup_get_state(data) == CDDB_ABORTED) {
		cddb_lookup_free(data);
		return;
	}

	cddb_lookup_set_state(data, CDDB_SUCCESS);
}


static void
abort_cb(GtkWidget * widget, gpointer user_data) {

	cddb_lookup_set_state((cddb_lookup_t *)user_data, CDDB_ABORTED);
}


void
create_query_progress_window(cddb_lookup_t * data) {

	GtkWidget * vbox;
	GtkWidget * hbuttonbox;
	GtkWidget * hseparator;
	GtkWidget * abort_button;


	data->progress_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(data->progress_win), _("CDDB query"));
        gtk_window_set_position(GTK_WINDOW(data->progress_win), GTK_WIN_POS_CENTER);
        gtk_window_resize(GTK_WINDOW(data->progress_win), 330, 120);
        g_signal_connect(G_OBJECT(data->progress_win), "delete_event",
			 G_CALLBACK(abort_cb), data);
        gtk_container_set_border_width(GTK_CONTAINER(data->progress_win), 10);
	
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(data->progress_win), vbox);

	data->progress_label = gtk_label_new(_("Retrieving matches from server..."));
	gtk_box_pack_start(GTK_BOX(vbox), data->progress_label, FALSE, FALSE, 0);
	
	data->progbar = gtk_progress_bar_new();
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(data->progbar), _("Connecting to CDDB server..."));
	gtk_box_pack_start(GTK_BOX(vbox), data->progbar, FALSE, FALSE, 6);
	
        hseparator = gtk_hseparator_new ();
        gtk_widget_show (hseparator);
        gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, TRUE, 5);

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        abort_button = gui_stock_label_button(_("Abort"), GTK_STOCK_CANCEL);
        g_signal_connect(G_OBJECT(abort_button), "clicked",
			 G_CALLBACK(abort_cb), data);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), abort_button);   

        gtk_widget_grab_focus(abort_button);

	gtk_widget_show_all(data->progress_win);
}


#ifdef HAVE_CDDA
void
store_cdda_export_merged(cddb_lookup_t * data, char * artist, char * record, char * genre, int year, char ** tracks) {

	int i;
	GtkTreeIter iter;

	char name[MAXLEN];
	cdda_drive_t * drive;

	for (i = 0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						  &iter, &data->iter_record, i); i++) {

		gtk_tree_store_set(music_store, &iter, MS_COL_NAME, tracks[i], -1);
	}

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &data->iter_record, MS_COL_DATA, &drive, -1);

	strncpy(drive->disc.artist_name, artist, MAXLEN-1);
	strncpy(drive->disc.record_name, record, MAXLEN-1);
	strncpy(drive->disc.genre, genre, MAXLEN-1);
	drive->disc.year = year;

	snprintf(name, MAXLEN-1, "%s: %s",
		 drive->disc.artist_name,
		 drive->disc.record_name);

	gtk_tree_store_set(music_store, &data->iter_record, MS_COL_NAME, name, -1);

	music_store_selection_changed(STORE_TYPE_CDDA);
}
#endif /* HAVE_CDDA */


void
cddb_lookup_merge(cddb_lookup_t * data, char * artist, char * record, char * genre, int * year, char ** tracks) {

	int i, j, y;

	map_t * map_artist = NULL;
	map_t * map_record = NULL;
	map_t * map_genre = NULL;
	map_t * map_year = NULL;
	map_t ** map_tracks = NULL;

	char tmp[MAXLEN];


	if ((map_tracks = (map_t **)calloc(data->ntracks, sizeof(map_t *))) == NULL) {
		fprintf(stderr, "cddb_lookup_merge: calloc error\n");
		return;
	}

	for (i = 0; i < data->nrecords; i++) {

		strncpy(tmp, notnull(cddb_disc_get_artist(data->records[i])), MAXLEN-1);
		if (!is_all_wspace(tmp)) {
			map_put(&map_artist, tmp);
		}

		strncpy(tmp, notnull(cddb_disc_get_title(data->records[i])), MAXLEN-1);
		if (!is_all_wspace(tmp)) {
			map_put(&map_record, tmp);
		}

		if (genre != NULL) {
			strncpy(tmp, notnull(cddb_disc_get_genre(data->records[i])), MAXLEN-1);
			if (!is_all_wspace(tmp)) {
				map_put(&map_genre, tmp);
			}
		}

		y = cddb_disc_get_year(data->records[i]);
		if (is_valid_year(y)) {
			snprintf(tmp, MAXLEN-1, "%d", y);
			map_put(&map_year, tmp);
		}

		for (j = 0; j < data->ntracks; j++) {
			strncpy(tmp,
				notnull(cddb_track_get_title(cddb_disc_get_track(data->records[i], j))),
				MAXLEN-1);
			if (!is_all_wspace(tmp)) {
				map_put(map_tracks + j, tmp);
			}
		}
	}

	if (map_artist) {
		strncpy(artist, map_get_max(map_artist), MAXLEN-1);
	}

	if (map_record) {
		strncpy(record, map_get_max(map_record), MAXLEN-1);
	}

	if (map_genre) {
		strncpy(genre, map_get_max(map_genre), MAXLEN-1);
	}

	if (map_year) {
		*year = atoi(map_get_max(map_year));
	}

	for (j = 0; j < data->ntracks; j++) {

		if (map_tracks[j]) {
			strncpy(tracks[j], map_get_max(map_tracks[j]), MAXLEN-1);
			map_free(map_tracks[j]);
		}
	}

	map_free(map_artist);
	map_free(map_record);
	map_free(map_genre);
	map_free(map_year);
	free(map_tracks);
}

gboolean
query_timeout_cb(gpointer user_data) {

	cddb_lookup_t * data = (cddb_lookup_t *)user_data;
	int state = cddb_lookup_get_state(data);
	char text[MAXLEN];

	switch (state) {
	case CDDB_INIT:
		return TRUE;
	case CDDB_BUSY:
		snprintf(text, MAXLEN, "%d / %d", data->counter, data->nrecords);
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(data->progbar),
					      (double)data->counter / data->nrecords);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(data->progbar), text);
		return TRUE;
	case CDDB_SUCCESS:
		gtk_widget_destroy(data->progress_win);
		cddb_dialog(data);
		break;
	case CDDB_ERROR:
		gtk_widget_destroy(data->progress_win);
		message_dialog(_("Error"),
			       browser_window,
			       GTK_MESSAGE_ERROR,
			       GTK_BUTTONS_OK,
			       NULL,
			       _("An error occurred while attempting "
				 "to connect to the CDDB server."));
		break;
	case CDDB_NO_MATCH:
		gtk_widget_destroy(data->progress_win);
		if (data->type == CDDB_TYPE_QUERY) {
			message_dialog(_("Warning"),
				       browser_window,
				       GTK_MESSAGE_WARNING,
				       GTK_BUTTONS_OK,
				       NULL,
				       _("No matching record found."));
		} else {
			data->type = CDDB_TYPE_SUBMIT_NEW;
			cddb_dialog(data);
		}
		break;
	case CDDB_ABORTED:
		gtk_widget_destroy(data->progress_win);
		return FALSE;
	}

	cddb_lookup_free(data);

	return FALSE;
}


#ifdef HAVE_CDDA
gboolean
cdda_auto_query_timeout_cb(gpointer user_data) {

	cddb_lookup_t * data = (cddb_lookup_t *)user_data;
	int state = cddb_lookup_get_state(data);

	if (state == CDDB_INIT || state == CDDB_BUSY) {
		return TRUE;
	}

	if (state == CDDB_SUCCESS) {
		char artist[MAXLEN];
		char record[MAXLEN];
		char genre[MAXLEN];
		int year = 0;
		char ** tracks;
		int i;

		artist[0] = '\0';
		record[0] = '\0';
		genre[0] = '\0';

		if ((tracks = calloc(data->ntracks, sizeof(char *))) == NULL) {
			fprintf(stderr, "cdda_auto_query_timeout_cb(): calloc error\n");
			return FALSE;
		}

		for (i = 0; i < data->ntracks; i++) {
			if ((tracks[i] = calloc(1, MAXLEN * sizeof(char))) == NULL) {
				fprintf(stderr, "cdda_auto_query_timeout_cb(): calloc error\n");
				return FALSE;
			}
		}

		cddb_lookup_merge(data, artist, record, genre, &year, tracks);
		store_cdda_export_merged(data, artist, record, genre, year, tracks);

		for (i = 0; i < data->ntracks; i++) {
			free(tracks[i]);
		}
		free(tracks);
	}

	if (options.cdda_add_to_playlist) {
		cdda_drive_t * drive;
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &data->iter_record,
				   MS_COL_DATA, &drive, -1);
		cdda_add_to_playlist(&data->iter_record, drive->disc.hash);
	}

	cddb_lookup_free(data);

	return FALSE;
}
#endif /* HAVE_CDDA */


static void
add_to_comments(cddb_lookup_t * data, GtkWidget * entry) {

	record_data_t * record_data;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &data->iter_record, MS_COL_DATA, &record_data, -1);

	if (record_data->comment != NULL && record_data->comment[0] != '\0') {
		char comment[MAXLEN];
		snprintf(comment, MAXLEN-1, "%s\n%s",
			 record_data->comment,
			 gtk_entry_get_text(GTK_ENTRY(entry)));
		free_strdup(&record_data->comment, comment);
	} else {
		free_strdup(&record_data->comment, gtk_entry_get_text(GTK_ENTRY(entry)));
	}

	music_store_mark_changed(&data->iter_record);
}

static void
add_category_to_comments(GtkWidget * widget, gpointer user_data) {

	cddb_lookup_t * data = (cddb_lookup_t *)user_data;
	add_to_comments(data, data->category_entry);
}

static void
add_genre_to_comments(GtkWidget * widget, gpointer user_data) {

	cddb_lookup_t * data = (cddb_lookup_t *)user_data;
	add_to_comments(data, data->genre_entry);
}

static void
add_ext_to_comments(GtkWidget * widget, gpointer user_data) {

	cddb_lookup_t * data = (cddb_lookup_t *)user_data;
	add_to_comments(data, data->ext_entry);
}

static void
import_as_artist(GtkWidget * widget, gpointer user_data) {

	GtkTreeIter parent;
	GtkTreePath * path;
	cddb_lookup_t * data = (cddb_lookup_t *)user_data;

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), &data->iter_record);
	gtk_tree_path_up(path);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &parent, path);
	gtk_tree_path_free(path);
	gtk_tree_store_set(music_store, &parent,
			   MS_COL_NAME, gtk_entry_get_text(GTK_ENTRY(data->artist_entry)), -1);

	music_store_mark_changed(&parent);
}


static void
import_as_title(GtkWidget * widget, gpointer user_data) {

	cddb_lookup_t * data = (cddb_lookup_t *)user_data;

	if (data->title_imported) {
		gtk_tree_store_set(music_store, &data->iter_record,
				   MS_COL_SORT, gtk_entry_get_text(GTK_ENTRY(data->title_entry)), -1);
	} else {
		gtk_tree_store_set(music_store, &data->iter_record,
				   MS_COL_NAME, gtk_entry_get_text(GTK_ENTRY(data->title_entry)), -1);

		data->title_imported = 1;
		gtk_button_set_label(GTK_BUTTON(data->title_import_button), _("Import as Sort Key"));
	}

	music_store_mark_changed(&data->iter_record);
}

static void
import_as_year(GtkWidget * widget, gpointer user_data) {

	cddb_lookup_t * data = (cddb_lookup_t *)user_data;
	int year = gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->year_spinner));

	if (data->year_imported) {
		char buf[16];
		snprintf(buf, 15, "%d", year);
		gtk_tree_store_set(music_store, &data->iter_record, MS_COL_SORT, buf, -1);
	} else {
		record_data_t * record_data;
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &data->iter_record,
				   MS_COL_DATA, &record_data, -1);
		record_data->year = year;

		data->year_imported = 1;
		gtk_button_set_label(GTK_BUTTON(data->year_import_button), _("Import as Sort Key"));
	}


	music_store_mark_changed(&data->iter_record);
}

gboolean
title_entry_focused(GtkWidget * widget, GdkEventFocus * event, gpointer user_data) {

	cddb_lookup_t * data = (cddb_lookup_t *)user_data;

	data->title_imported = 0;
	gtk_button_set_label(GTK_BUTTON(data->title_import_button), _("Import as Title"));

	return FALSE;
}

gboolean
year_spinner_focused(GtkWidget * widget, GdkEventFocus * event, gpointer user_data) {

	cddb_lookup_t * data = (cddb_lookup_t *)user_data;

	data->year_imported = 0;
	gtk_button_set_label(GTK_BUTTON(data->year_import_button), _("Import as Year"));

	return FALSE;
}


static void
changed_combo(GtkWidget * widget, gpointer * user_data) {

	cddb_lookup_t * data = (cddb_lookup_t *)user_data;
	int i = gtk_combo_box_get_active(GTK_COMBO_BOX(data->combo));

	if (i >= 0 && i < data->nrecords) {
		cddb_dialog_load_disc(data, data->records[i]);
	}

	if (data->type == CDDB_TYPE_QUERY && iter_get_store_type(&data->iter_record) == STORE_TYPE_FILE) {
		title_entry_focused(data->title_entry, NULL, user_data);
		year_spinner_focused(data->year_spinner, NULL, user_data);
	}
}


static void
cell_edited_callback(GtkCellRendererText * cell, gchar * path, gchar * text, gpointer user_data) {

	cddb_lookup_t * data = (cddb_lookup_t *)user_data;
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(data->track_store), &iter, path)) {
		gtk_list_store_set(data->track_store, &iter, 0, text, -1);
	}
}


gboolean
create_cddb_write_error_dialog(gpointer data) {

	message_dialog(_("Error"),
		       browser_window,
		       GTK_MESSAGE_ERROR,
		       GTK_BUTTONS_OK,
		       NULL,
		       (char *)data);

	return FALSE;
}

int
create_cddb_write_warn_dialog(char * text) {

	int ret = message_dialog(_("Warning"),
				 browser_window,
				 GTK_MESSAGE_WARNING,
				 GTK_BUTTONS_YES_NO,
				 NULL,
				 text);

	return (ret != GTK_RESPONSE_YES);
}

static int
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


int
cddb_submit_check(cddb_lookup_t * data) {

	GtkTreeIter iter_trlist;
	char * artist;
	char * title;
	char * genre;
	char * ext;
	int category;
	int year;
	int i;


	artist = (char *)gtk_entry_get_text(GTK_ENTRY(data->artist_entry));
	if (is_all_wspace(artist)) {
		gtk_widget_grab_focus(data->artist_entry);
		return 1;
	}

	if (!check_case(artist, CASE_DOWN)) {
		if (create_cddb_write_warn_dialog(_("Artist appears to be in all lowercase.\n"
						    "Do you want to proceed?"))) {
			gtk_widget_grab_focus(data->artist_entry);
			return 1;
		}
	}

	if (!check_case(artist, CASE_UP)) {
		if (create_cddb_write_warn_dialog(_("Artist appears to be in all uppercase.\n"
						    "Do you want to proceed?"))) {
			gtk_widget_grab_focus(data->artist_entry);
			return 1;
		}
	}

	title = (char *)gtk_entry_get_text(GTK_ENTRY(data->title_entry));
	if (is_all_wspace(title)) {
		gtk_widget_grab_focus(data->title_entry);
		return 1;
	}

	if (!check_case(title, CASE_DOWN)) {
		if (create_cddb_write_warn_dialog(_("Title appears to be in all lowercase.\n"
						    "Do you want to proceed?"))) {
			gtk_widget_grab_focus(data->title_entry);
			return 1;
		}
	}

	if (!check_case(title, CASE_UP)) {
		if (create_cddb_write_warn_dialog(_("Title appears to be in all uppercase.\n"
						    "Do you want to proceed?"))) {
			gtk_widget_grab_focus(data->title_entry);
			return 1;
		}
	}

	year = gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->year_spinner));

	if (year == YEAR_MIN) {
		if (create_cddb_write_warn_dialog(_("It is very likely that the year is wrong.\n"
						    "Do you want to proceed?"))) {
			gtk_widget_grab_focus(data->year_spinner);
			return 1;
		}
	}

	category = gtk_combo_box_get_active(GTK_COMBO_BOX(data->category_combo));
	if (category == 0) {
		gtk_widget_grab_focus(data->category_combo);
		return 1;
	}

	genre = (char *)gtk_entry_get_text(GTK_ENTRY(data->genre_entry));
	if (genre[0] != '\0' && is_all_wspace(genre)) {
		gtk_widget_grab_focus(data->genre_entry);
		return 1;
	}

	ext = (char *)gtk_entry_get_text(GTK_ENTRY(data->ext_entry));
	if (genre[0] != '\0' && is_all_wspace(ext)) {
		gtk_widget_grab_focus(data->ext_entry);
		return 1;
	}

	for (i = 0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(data->track_store),
						  &iter_trlist, NULL, i); i++) {
		char * name;
		gtk_tree_model_get(GTK_TREE_MODEL(data->track_store),
				   &iter_trlist, 0, &name, -1);
		
		if (is_all_wspace(name)) {
			gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(data->track_list)),
						       &iter_trlist);
			return 1;
		}
		
		g_free(name);
	}

	return 0;
}

void
cddb_submit(cddb_lookup_t * data) {

	GtkTreeIter iter_trlist;
	cddb_conn_t * conn = NULL;
	cddb_disc_t * disc = NULL;
	cddb_track_t * track = NULL;

	int i;

	if (cddb_connection_setup(&conn) == 1) {
		return;
	}

	if (!cddb_set_email_address(conn, options.cddb_email)) {
		create_cddb_write_error_dialog(_("The email address provided for submission is invalid."));
		cddb_destroy(conn);
		return;
	}

	cddb_http_enable(conn);
	cddb_set_server_port(conn, 80);

	if ((disc = cddb_disc_new()) == NULL) {
		fprintf(stderr, "cddb_lookup(): cddb_disc_new error\n");
		return;
	}

	for (i = 0; i < data->ntracks; i++) {
		track = cddb_track_new();
		cddb_track_set_frame_offset(track, data->frames[i]);
		cddb_disc_add_track(disc, track);
	}

	for (i = 0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(data->track_store),
						  &iter_trlist, NULL, i); i++) {
		char * name;
		gtk_tree_model_get(GTK_TREE_MODEL(data->track_store),
				   &iter_trlist, 0, &name, -1);

		cddb_track_set_title(cddb_disc_get_track(disc, i), name);
		g_free(name);
	}

	cddb_disc_set_length(disc, data->record_length);
	cddb_disc_set_artist(disc, gtk_entry_get_text(GTK_ENTRY(data->artist_entry)));
	cddb_disc_set_title(disc, gtk_entry_get_text(GTK_ENTRY(data->title_entry)));
	cddb_disc_set_year(disc, gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->year_spinner)));
	cddb_disc_set_category(disc, gtk_combo_box_get_active(GTK_COMBO_BOX(data->category_combo)) - 1);
	cddb_disc_set_genre(disc, gtk_entry_get_text(GTK_ENTRY(data->genre_entry)));
	cddb_disc_set_ext_data(disc, gtk_entry_get_text(GTK_ENTRY(data->ext_entry)));

	if (cddb_disc_calc_discid(disc) == 0) {
		fprintf(stderr, "cddb_lookup.c: cddb_submit_thread(): cddb_disc_calc_discid error\n");
		cddb_destroy(conn);
		cddb_disc_destroy(disc);
		return;
	}

	if (!cddb_write(conn, disc)) {
		create_cddb_write_error_dialog(_("An error occurred while submitting the record to the CDDB server."));
	}

	cddb_destroy(conn);
	cddb_disc_destroy(disc);
}


void
cddb_dialog_load_disc(cddb_lookup_t * data, cddb_disc_t * disc) {

	GtkTreeIter iter;
	int i;

	gtk_entry_set_text(GTK_ENTRY(data->artist_entry), notnull(cddb_disc_get_artist(disc)));
	gtk_entry_set_text(GTK_ENTRY(data->title_entry), notnull(cddb_disc_get_title(disc)));
	gtk_entry_set_text(GTK_ENTRY(data->category_entry), notnull(cddb_disc_get_category_str(disc)));
	gtk_entry_set_text(GTK_ENTRY(data->genre_entry), notnull(cddb_disc_get_genre(disc)));
	gtk_entry_set_text(GTK_ENTRY(data->ext_entry), notnull(cddb_disc_get_ext_data(disc)));

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->year_spinner), cddb_disc_get_year(disc));

	gtk_list_store_clear(GTK_LIST_STORE(data->track_store));

	for (i = 0; i < data->ntracks; i++) {
		gtk_list_store_append(data->track_store, &iter);
		gtk_list_store_set(data->track_store, &iter, 0,
				   notnull(cddb_track_get_title(cddb_disc_get_track(disc, i))), -1);
	}
}


void
store_load_tracklist(cddb_lookup_t * data) {

	GtkTreeIter iter;
	GtkTreeIter iter_trlist;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &iter, &data->iter_record, i++)) {
		char * name;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_NAME, &name, -1);
		gtk_list_store_append(data->track_store, &iter_trlist);
		gtk_list_store_set(data->track_store, &iter_trlist, 0, name, -1);
		g_free(name);
	}
}

void
cddb_dialog_load_store_file(cddb_lookup_t * data) {

	GtkTreeIter iter_artist;
	record_data_t * record_data;
	char * str;

	store_load_tracklist(data);

	gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_artist, &data->iter_record);
	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_artist, MS_COL_NAME, &str, -1);
	gtk_entry_set_text(GTK_ENTRY(data->artist_entry), str);
	g_free(str);

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &data->iter_record, MS_COL_NAME, &str, -1);
	gtk_entry_set_text(GTK_ENTRY(data->title_entry), str);
	g_free(str);

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &data->iter_record, MS_COL_DATA, &record_data, -1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->year_spinner), record_data->year);
}


#ifdef HAVE_CDDA
void
cddb_dialog_load_store_cdda(cddb_lookup_t * data) {

	store_load_tracklist(data);

	cdda_drive_t * drive;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &data->iter_record,
			   MS_COL_DATA, &drive, -1);

	gtk_entry_set_text(GTK_ENTRY(data->artist_entry), drive->disc.artist_name);
	gtk_entry_set_text(GTK_ENTRY(data->title_entry), drive->disc.record_name);
	gtk_entry_set_text(GTK_ENTRY(data->genre_entry), drive->disc.genre);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->year_spinner), drive->disc.year);
}
#endif /* HAVE_CDDA */


void
export_tracklist(cddb_lookup_t * data) {

	int i;
	GtkTreeIter iter;
	GtkTreeIter iter_trlist;

	for (i = 0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						  &iter, &data->iter_record, i); i++) {

		if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(data->track_store),
						  &iter_trlist, NULL, i)) {
			char * name;
			gtk_tree_model_get(GTK_TREE_MODEL(data->track_store), &iter_trlist, 0, &name, -1);
			gtk_tree_store_set(music_store, &iter, MS_COL_NAME, name, -1);
			g_free(name);
		}
	}
}

void
store_file_export(cddb_lookup_t * data) {

	export_tracklist(data);
	music_store_mark_changed(&data->iter_record);
}


#ifdef HAVE_CDDA
void
store_cdda_export(cddb_lookup_t * data) {

	char name[MAXLEN];
	cdda_drive_t * drive;

	export_tracklist(data);

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &data->iter_record, MS_COL_DATA, &drive, -1);

	strncpy(drive->disc.artist_name, gtk_entry_get_text(GTK_ENTRY(data->artist_entry)), MAXLEN-1);
	strncpy(drive->disc.record_name, gtk_entry_get_text(GTK_ENTRY(data->title_entry)), MAXLEN-1);
	strncpy(drive->disc.genre, gtk_entry_get_text(GTK_ENTRY(data->genre_entry)), MAXLEN-1);
	drive->disc.year = gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->year_spinner));

	snprintf(name, MAXLEN-1, "%s: %s",
		 drive->disc.artist_name,
		 drive->disc.record_name);

	gtk_tree_store_set(music_store, &data->iter_record, MS_COL_NAME, name, -1);
}
#endif /* HAVE_CDDA */


void
cddb_dialog(cddb_lookup_t * data) {

	GtkWidget * dialog;
	GtkWidget * table;

	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * button;

	GtkWidget * viewport;
	GtkWidget * scrollwin;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

	int i;
	char text[MAXLEN];


        dialog = gtk_dialog_new_with_buttons(NULL,
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	switch (data->type) {
	case CDDB_TYPE_QUERY:
		gtk_window_set_title(GTK_WINDOW(dialog), _("CDDB query"));
		break;
	case CDDB_TYPE_SUBMIT:
		gtk_window_set_title(GTK_WINDOW(dialog), _("Correct existing record"));
		break;
	case CDDB_TYPE_SUBMIT_NEW:
		gtk_window_set_title(GTK_WINDOW(dialog), _("Submit new record"));
		break;
	}

	table = gtk_table_new(8, 3, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, FALSE, 2);

	if (data->type != CDDB_TYPE_SUBMIT_NEW) {
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 3);
		label = gtk_label_new(_("Matches:"));
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

		data->combo = gtk_combo_box_new_text();
		for (i = 0; i < data->nrecords; i++) {
			snprintf(text, MAXLEN, "%d. %s: %s [%x] ",
				 i + 1,
				 notnull(cddb_disc_get_artist(data->records[i])),
				 notnull(cddb_disc_get_title(data->records[i])),
				 cddb_disc_get_discid(data->records[i]));
			gtk_combo_box_append_text(GTK_COMBO_BOX(data->combo), text);
		}

		gtk_combo_box_set_active(GTK_COMBO_BOX(data->combo), 0);
		g_signal_connect(data->combo, "changed", G_CALLBACK(changed_combo), data);
			 
		gtk_table_attach(GTK_TABLE(table), data->combo, 1, 3, 0, 1, GTK_FILL, GTK_FILL, 5, 3);
	}

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Artist:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	data->artist_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), data->artist_entry, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Title:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	data->title_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), data->title_entry, 1, 2, 2, 3,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Year:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	data->year_spinner = gtk_spin_button_new_with_range(YEAR_MIN, YEAR_MAX, 1);
	gtk_table_attach(GTK_TABLE(table), data->year_spinner, 1, 2, 3, 4,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 4, 5, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Category:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	if (data->type != CDDB_TYPE_SUBMIT_NEW) {
		data->category_entry = gtk_entry_new();
		gtk_table_attach(GTK_TABLE(table), data->category_entry, 1, 2, 4, 5,
				 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);
	} else {
		data->category_combo = gtk_combo_box_new_text();
		gtk_combo_box_append_text(GTK_COMBO_BOX(data->category_combo), _("(choose a category)"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(data->category_combo), "data");
		gtk_combo_box_append_text(GTK_COMBO_BOX(data->category_combo), "folk");
		gtk_combo_box_append_text(GTK_COMBO_BOX(data->category_combo), "jazz");
		gtk_combo_box_append_text(GTK_COMBO_BOX(data->category_combo), "misc");
		gtk_combo_box_append_text(GTK_COMBO_BOX(data->category_combo), "rock");
		gtk_combo_box_append_text(GTK_COMBO_BOX(data->category_combo), "country");
		gtk_combo_box_append_text(GTK_COMBO_BOX(data->category_combo), "blues");
		gtk_combo_box_append_text(GTK_COMBO_BOX(data->category_combo), "newage");
		gtk_combo_box_append_text(GTK_COMBO_BOX(data->category_combo), "reagge");
		gtk_combo_box_append_text(GTK_COMBO_BOX(data->category_combo), "classical");
		gtk_combo_box_append_text(GTK_COMBO_BOX(data->category_combo), "soundtrack");
		gtk_combo_box_set_active(GTK_COMBO_BOX(data->category_combo), 0);
		gtk_table_attach(GTK_TABLE(table), data->category_combo, 1, 2, 4, 5,
				 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);
	}

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 5, 6, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Genre:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	data->genre_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), data->genre_entry, 1, 2, 5, 6,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 6, 7, GTK_FILL, GTK_FILL, 5, 3);
	label = gtk_label_new(_("Extended data:"));
        gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);

	data->ext_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), data->ext_entry, 1, 2, 6, 7,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);

	if (data->type == CDDB_TYPE_QUERY && iter_get_store_type(&data->iter_record) == STORE_TYPE_FILE) {

		g_signal_connect(G_OBJECT(data->title_entry), "focus-in-event",
				 G_CALLBACK(title_entry_focused), data);
		g_signal_connect(G_OBJECT(data->year_spinner), "focus-in-event",
				 G_CALLBACK(year_spinner_focused), data);

		button = gtk_button_new_with_label(_("Import as Artist"));
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(import_as_artist), data);
		gtk_table_attach(GTK_TABLE(table), button, 2, 3, 1, 2, GTK_FILL, GTK_FILL, 5, 3);

		data->title_import_button = gtk_button_new_with_label(_("Import as Title"));
		g_signal_connect(G_OBJECT(data->title_import_button), "clicked",
				 G_CALLBACK(import_as_title), data);
		gtk_table_attach(GTK_TABLE(table), data->title_import_button,
				 2, 3, 2, 3, GTK_FILL, GTK_FILL, 5, 3);

		data->year_import_button = gtk_button_new_with_label(_("Import as Year"));
		g_signal_connect(G_OBJECT(data->year_import_button), "clicked",
				 G_CALLBACK(import_as_year), data);
		gtk_table_attach(GTK_TABLE(table), data->year_import_button,
				 2, 3, 3, 4, GTK_FILL, GTK_FILL, 5, 3);

		button = gtk_button_new_with_label(_("Add to Comments"));
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(add_category_to_comments), data);
		gtk_table_attach(GTK_TABLE(table), button, 2, 3, 4, 5, GTK_FILL, GTK_FILL, 5, 3);

		button = gtk_button_new_with_label(_("Add to Comments"));
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(add_genre_to_comments), data);
		gtk_table_attach(GTK_TABLE(table), button, 2, 3, 5, 6, GTK_FILL, GTK_FILL, 5, 3);

		button = gtk_button_new_with_label(_("Add to Comments"));
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(add_ext_to_comments), data);
		gtk_table_attach(GTK_TABLE(table), button, 2, 3, 6, 7, GTK_FILL, GTK_FILL, 5, 3);
	}

	data->track_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_BOOLEAN);
        data->track_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(data->track_store));

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "editable", TRUE, NULL);
	g_signal_connect(renderer, "edited", (GCallback)cell_edited_callback, data);
	column = gtk_tree_view_column_new_with_attributes(_("Tracks"),
							  renderer,
							  "text", 0,
							  NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(data->track_list), column);
        gtk_tree_view_set_reorderable(GTK_TREE_VIEW(data->track_list), FALSE);

        viewport = gtk_viewport_new(NULL, NULL);
	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(scrollwin, -1, 200);
        gtk_table_attach(GTK_TABLE(table), viewport, 0, 3, 7, 8,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 3);
        gtk_container_add(GTK_CONTAINER(viewport), scrollwin);
        gtk_container_add(GTK_CONTAINER(scrollwin), data->track_list);

	gtk_widget_show_all(dialog);

	if (data->type != CDDB_TYPE_SUBMIT_NEW) {
		cddb_dialog_load_disc(data, data->records[0]);
	} else {
		if (iter_get_store_type(&data->iter_record) == STORE_TYPE_FILE) {
			cddb_dialog_load_store_file(data);
		} else {
#ifdef HAVE_CDDA
			cddb_dialog_load_store_cdda(data);
#endif /* HAVE_CDDA */
		}
	}

 display:

	if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		switch (data->type) {
		case CDDB_TYPE_SUBMIT:
		case CDDB_TYPE_SUBMIT_NEW:
			if (cddb_submit_check(data)) {
				goto display;
			}
			cddb_submit(data);
			/* no break */
		case CDDB_TYPE_QUERY:
			if (iter_get_store_type(&data->iter_record) == STORE_TYPE_FILE) {
				store_file_export(data);
			} else {
#ifdef HAVE_CDDA
				store_cdda_export(data);
#endif /* HAVE_CDDA */
			}
			break;
		}
	}

	gtk_widget_destroy(dialog);
}


void *
cddb_lookup_thread(void * arg) {

	AQUALUNG_THREAD_DETACH();

	cddb_lookup((cddb_lookup_t *)arg);

	return NULL;
}

void
cddb_start_lookup_thread(GtkTreeIter * iter_record,
			 int type,
			 int progress,
			 int ntracks, int * frames, int length,
			 gboolean (*timeout_cb)(gpointer)) {

	cddb_lookup_t * data;

	if ((data = cddb_lookup_new()) == NULL) {
		return;
	}

	data->iter_record = *iter_record;
	data->type = type;
	data->ntracks = ntracks;
	data->frames = frames;
	data->record_length = length;

	AQUALUNG_THREAD_CREATE(data->thread_id, NULL, cddb_lookup_thread, data);
	if (progress) {
		create_query_progress_window(data);
	}
	aqualung_timeout_add(100, timeout_cb, data);
}

void
cddb_start_query(GtkTreeIter * iter_record, int ntracks, int * frames, int length) {

	cddb_start_lookup_thread(iter_record,
				 CDDB_TYPE_QUERY,
				 1,
				 ntracks, frames, length,
				 query_timeout_cb);
}

void
cddb_start_submit(GtkTreeIter * iter_record, int ntracks, int * frames, int length) {

	if (options.cddb_email[0] == '\0') {
		create_cddb_write_error_dialog(_("You have to provide an email address for CDDB submission."));
		return;
	}

	cddb_start_lookup_thread(iter_record,
				 CDDB_TYPE_SUBMIT,
				 1,
				 ntracks, frames, length,
				 query_timeout_cb);
}

#ifdef HAVE_CDDA
void
cddb_auto_query_cdda(GtkTreeIter * drive_iter, int ntracks, int * frames, int length) {

	cddb_start_lookup_thread(drive_iter,
				 CDDB_TYPE_QUERY,
				 0,
				 ntracks, frames, length,
				 cdda_auto_query_timeout_cb);
}
#endif /* HAVE_CDDA */

void
cddb_query_batch(int ntracks, int * frames, int length,
		 char * artist, char * record, int * year, char ** tracks) {

	cddb_lookup_t * data;

	if ((data = cddb_lookup_new()) == NULL) {
		return;
	}

	data->type = CDDB_TYPE_QUERY;
	data->ntracks = ntracks;
	data->frames = frames;
	data->record_length = length;

	cddb_lookup(data);
	cddb_lookup_merge(data, artist, record, NULL/*genre*/, year, tracks);
	cddb_lookup_free(data);
}

#endif /* HAVE_CDDB */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

