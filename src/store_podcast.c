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

#ifdef HAVE_PODCAST

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#ifdef _WIN32
#include <glib.h>
#else
#include <pthread.h>
#endif /* _WIN32 */

#include "common.h"
#include "i18n.h"
#include "utils.h"
#include "utils_gui.h"
#include "options.h"
#include "file_info.h"
#include "export.h"
#include "playlist.h"
#include "music_browser.h"
#include "podcast.h"
#include "store_podcast.h"


extern options_t options;

extern char pl_color_inactive[14];

extern GtkWidget * browser_window;
extern GtkTreeStore * music_store;
extern GtkTreeSelection * music_select;

extern GdkPixbuf * icon_track;
GdkPixbuf * icon_feed;
GdkPixbuf * icon_podcasts;

GtkWidget * podcast_track_menu;
GtkWidget * podcast_track__addlist;
#ifdef HAVE_EXPORT
GtkWidget * podcast_track__export;
#endif /* HAVE_EXPORT */
GtkWidget * podcast_track__fileinfo;

GtkWidget * podcast_feed_menu;
GtkWidget * podcast_feed__addlist;
GtkWidget * podcast_feed__addlist_albummode;
GtkWidget * podcast_feed__addlist_new;
GtkWidget * podcast_feed__addlist_albummode_new;
GtkWidget * podcast_feed__subscribe;
GtkWidget * podcast_feed__edit;
GtkWidget * podcast_feed__update;
GtkWidget * podcast_feed__abort;
GtkWidget * podcast_feed__remove;
#ifdef HAVE_EXPORT
GtkWidget * podcast_feed__export;
GtkWidget * podcast_feed__export_new;
#endif /* HAVE_EXPORT */

GtkWidget * podcast_store_menu;
GtkWidget * podcast_store__addlist;
GtkWidget * podcast_store__addlist_albummode;
GtkWidget * podcast_store__addlist_new;
GtkWidget * podcast_store__addlist_albummode_new;
GtkWidget * podcast_store__subscribe;
GtkWidget * podcast_store__update;
GtkWidget * podcast_store__reorder;
#ifdef HAVE_EXPORT
GtkWidget * podcast_store__export;
GtkWidget * podcast_store__export_new;
#endif /* HAVE_EXPORT */
GtkWidget * podcast_store__update_enabled;

void podcast_store__addlist_defmode(gpointer data);
void podcast_store__update_cb(gpointer data);

void podcast_feed__addlist_defmode(gpointer data);
void podcast_feed__subscribe_cb(gpointer data);
void podcast_feed__edit_cb(gpointer data);
void podcast_feed__update_cb(gpointer data);
void podcast_feed__remove_cb(gpointer data);

void podcast_track__addlist_cb(gpointer data);
void podcast_track__fileinfo_cb(gpointer data);

#ifdef HAVE_EXPORT
void podcast_store__export_cb(gpointer data);
void podcast_feed__export_cb(gpointer data);
void podcast_track__export_cb(gpointer data);
#endif /* HAVE_EXPORT */

struct keybinds podcast_store_keybinds[] = {
	{podcast_store__addlist_defmode, GDK_a, GDK_A, 0},
	{podcast_store__update_cb, GDK_u, GDK_U, 0},
#ifdef HAVE_EXPORT
	{podcast_store__export_cb, GDK_x, GDK_X, 0},
#endif /* HAVE_EXPORT */
	{podcast_feed__subscribe_cb, GDK_n, GDK_N, GDK_CONTROL_MASK},
	{NULL, 0, 0}
};

struct keybinds podcast_feed_keybinds[] = {
	{podcast_feed__addlist_defmode, GDK_a, GDK_A, 0},
	{podcast_feed__subscribe_cb, GDK_n, GDK_N, 0},
	{podcast_feed__edit_cb, GDK_e, GDK_E, 0},
	{podcast_feed__update_cb, GDK_u, GDK_U, 0},
	{podcast_feed__remove_cb, GDK_Delete, GDK_KP_Delete, 0},
#ifdef HAVE_EXPORT
	{podcast_feed__export_cb, GDK_x, GDK_X, 0},
#endif /* HAVE_EXPORT */
	{NULL, 0, 0}
};

struct keybinds podcast_track_keybinds[] = {
	{podcast_track__addlist_cb, GDK_a, GDK_A, 0},
	{podcast_track__fileinfo_cb, GDK_i, GDK_I, 0},
#ifdef HAVE_EXPORT
	{podcast_track__export_cb, GDK_x, GDK_X, 0},
#endif /* HAVE_EXPORT */
	{NULL, 0, 0}
};


gboolean
store_podcast_remove_podcast_item(GtkTreeIter * iter) {

	podcast_item_t * item;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, MS_COL_DATA, &item, -1);
	podcast_item_free(item);

	return gtk_tree_store_remove(music_store, iter);
}

gboolean
store_podcast_remove_podcast(GtkTreeIter * iter) {

	podcast_t * podcast;
	GtkTreeIter track_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &track_iter, iter, i++)) {
		store_podcast_remove_podcast_item(&track_iter);
	}

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, MS_COL_DATA, &podcast, -1);
	podcast_free(podcast);

	return gtk_tree_store_remove(music_store, iter);
}

int
store_podcast_get_store_iter(GtkTreeIter * store_iter) {

	store_t * data;
	GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter, NULL, i++)) {

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &data, -1);
		if (data->type == STORE_TYPE_PODCAST) {
			*store_iter = iter;
			return 0;
		}
	}

	printf("WARNING: store_podcast_get_store_iter could not find store iter\n");
	return -1;
}

int
store_podcast_get_pod_iter(GtkTreeIter * pod_iter, podcast_t * podcast) {

	podcast_t * data;
	GtkTreeIter store_iter;
	GtkTreeIter iter;
	int i = 0;

	if (store_podcast_get_store_iter(&store_iter) != 0) {
		return 0;
	}

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter, &store_iter, i++)) {

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &data, -1);
		if (podcast == data) {
			*pod_iter = iter;
			return 0;
		}
	}

	printf("WARNING: store_podcast_get_pod_iter could not find podcast iter\n");
	return -1;
}

/*************************************************/

void
podcast_browse_button_clicked(GtkButton * button, gpointer data) {

	file_chooser_with_entry(_("Please select the download directory for this podcast."),
				browser_window,
				GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
				FILE_CHOOSER_FILTER_NONE,
				(GtkWidget *)data,
				options.podcastdir);
}

void
check_button_toggle_sensitivity(GtkToggleButton * togglebutton, gpointer data) {

	if (gtk_toggle_button_get_active(togglebutton)) {
		gtk_widget_set_sensitive((GtkWidget *)data, TRUE);
	} else {
		gtk_widget_set_sensitive((GtkWidget *)data, FALSE);
	}
}

void
insert_check_label_spin(GtkWidget * table, GtkWidget ** check, char * ltext, GtkWidget ** spin,
			double spinval, double min, double max, int y1, int y2, int active) {

	*check = gtk_check_button_new_with_label(ltext);
	gtk_widget_set_name(*check, "check_on_window");
	gtk_table_attach(GTK_TABLE(table), *check, 0, 1, y1, y2, GTK_FILL, GTK_FILL, 5, 5);

	*spin = gtk_spin_button_new_with_range(min, max, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(*spin), spinval);
        gtk_table_attach(GTK_TABLE(table), *spin, 1, 2, y1, y2,
                         GTK_EXPAND | GTK_FILL, GTK_FILL, 2, 5);

	g_signal_connect(G_OBJECT(*check), "toggled",
			 G_CALLBACK(check_button_toggle_sensitivity), *spin);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(*check), active);
	gtk_widget_set_sensitive(*spin, active);
}

/* create == 1 : add new podcast; else edit existing podcast */
int
podcast_dialog(podcast_t ** podcast, int create) {

	GtkWidget * dialog;
	GtkWidget * table;

	GtkWidget * url_entry;
	GtkWidget * dir_entry;
	GtkWidget * check_check;
	GtkWidget * check_spin;

	GtkWidget * frame;
	GtkWidget * count_check;
	GtkWidget * date_check;
	GtkWidget * size_check;
	GtkWidget * count_spin;
	GtkWidget * date_spin;
	GtkWidget * size_spin;

        dialog = gtk_dialog_new_with_buttons(create ? _("Subscribe to new feed") : _("Edit feed settings"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	table = gtk_table_new(3, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, TRUE, 2);

	insert_label_entry(table, _("Podcast URL:"), &url_entry,
			   create ? NULL : (*podcast)->url,
			   0, 1, create);

	if (create) {
		insert_label_entry_browse(table, _("Download directory:"), &dir_entry, options.podcastdir, 1, 2,
					  podcast_browse_button_clicked);
	} else {
		insert_label_entry(table, _("Download directory:"), &dir_entry, (*podcast)->dir, 1, 2, FALSE);
	}


	if (options.podcasts_autocheck || create) {
		insert_check_label_spin(table, &check_check, _("Auto-check interval [hour]:"), &check_spin,
					create ? 1.0 : (*podcast)->check_interval / 3600.0, 0.25, 168.0,
					3, 4, create || (*podcast)->flags & PODCAST_AUTO_CHECK);
		gtk_spin_button_set_digits(GTK_SPIN_BUTTON(check_spin), 2);
		gtk_spin_button_set_increments(GTK_SPIN_BUTTON(check_spin), 0.25, 1.0);
	} else {
		GtkWidget * label = gtk_label_new(_("Automatic update has been disabled for all feeds\n"
						    "in the Podcasts store popup menu."));
		GtkWidget * icon = gtk_image_new_from_stock(GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_BUTTON);
		GtkWidget * hbox = gtk_hbox_new(FALSE, 0);

		gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 5);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 2, 3, 4, GTK_FILL, GTK_FILL, 5, 5);
	}

	frame = gtk_frame_new(_("Limits"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame, FALSE, TRUE, 5);
	table = gtk_table_new(3, 2, FALSE);
        gtk_container_add(GTK_CONTAINER(frame), table);

	insert_check_label_spin(table, &count_check, _("Maximum number of items:"), &count_spin,
				create ? 10 : (*podcast)->count_limit, 0, 100,
				0, 1, create || (*podcast)->flags & PODCAST_COUNT_LIMIT);

	insert_check_label_spin(table, &date_check, _("Remove older items [day]:"), &date_spin,
				create ? 14 : (*podcast)->date_limit / 86400, 0, 100,
				1, 2, create ? FALSE : (*podcast)->flags & PODCAST_DATE_LIMIT);

	insert_check_label_spin(table, &size_check, _("Maximum space to use [MB]:"), &size_spin,
				create ? 100 : (*podcast)->size_limit / (1024 * 1024), 0, 1024,
				2, 3, create || (*podcast)->flags & PODCAST_SIZE_LIMIT);

	gtk_widget_grab_focus(url_entry);
	gtk_widget_show_all(dialog);

 display:

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		if (create) {
			const char * pdir = g_filename_from_utf8(gtk_entry_get_text(GTK_ENTRY(dir_entry)), -1, NULL, NULL, NULL);
			char dir[MAXLEN];
			char url[MAXLEN];

			strncpy(url, gtk_entry_get_text(GTK_ENTRY(url_entry)), MAXLEN-1);
			if (url[0] == '\0') {
				gtk_widget_grab_focus(url_entry);
				goto display;
			}

			dir[0] = '\0';
			if (pdir == NULL || pdir[0] == '\0') {
				gtk_widget_grab_focus(dir_entry);
				goto display;
			}

			if ((*podcast = podcast_new()) == NULL) {
				return 0;
			}

			normalize_filename(pdir, dir);
			(*podcast)->dir = strdup(dir);
			(*podcast)->url = strdup(url);
			strncpy(options.podcastdir, dir, MAXLEN-1);
		}

		if (options.podcasts_autocheck || create) {
			(*podcast)->check_interval = gtk_spin_button_get_value(GTK_SPIN_BUTTON(check_spin)) * 3600;
			set_option_bit_from_toggle(check_check, &(*podcast)->flags, PODCAST_AUTO_CHECK);
		}

		set_option_bit_from_toggle(count_check, &(*podcast)->flags, PODCAST_COUNT_LIMIT);
		set_option_bit_from_toggle(date_check, &(*podcast)->flags, PODCAST_DATE_LIMIT);
		set_option_bit_from_toggle(size_check, &(*podcast)->flags, PODCAST_SIZE_LIMIT);

		(*podcast)->count_limit = gtk_spin_button_get_value(GTK_SPIN_BUTTON(count_spin));
		(*podcast)->date_limit = gtk_spin_button_get_value(GTK_SPIN_BUTTON(date_spin)) * 86400;
		(*podcast)->size_limit = gtk_spin_button_get_value(GTK_SPIN_BUTTON(size_spin)) * 1024 * 1024;

		gtk_widget_destroy(dialog);
		return 1;
	} else {
		gtk_widget_destroy(dialog);
		return 0;
	}
}

void
podcast_track_mark_read(GtkTreeIter * iter, podcast_item_t * item) {

	item->new = 0;
	gtk_tree_store_set(music_store, iter, MS_COL_FONT, PANGO_WEIGHT_NORMAL, -1);
}

/* returns the duration of the track */
float
podcast_track_addlist_iter(GtkTreeIter iter_track, playlist_t * pl,
			   GtkTreeIter * parent, GtkTreeIter * dest, int new) {

	GtkTreeIter dest_parent;
        GtkTreeIter pod_iter;
	GtkTreeIter list_iter;

	podcast_t * podcast;
	podcast_item_t * item;

	char list_str[MAXLEN];
	char duration_str[MAXLEN];

	playlist_data_t * pldata = NULL;


	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track, MS_COL_DATA, &item, -1);

	if (new && !item->new) {
		return 0.0f;
	}

	gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &pod_iter, &iter_track);
	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &pod_iter, MS_COL_DATA, &podcast, -1);

	if (parent != NULL ||
	    (dest != NULL && gtk_tree_model_iter_parent(GTK_TREE_MODEL(pl->store), &dest_parent, dest))) {
		GtkTreeIter * piter = (parent != NULL) ? parent : &dest_parent;
		playlist_data_t * pdata;

		gtk_tree_model_get(GTK_TREE_MODEL(pl->store), piter, PL_COL_DATA, &pdata, -1);
		if (pdata->artist && pdata->album && podcast->author && podcast->title &&
		    !strcmp(pdata->artist, podcast->author) && !strcmp(pdata->album, podcast->title)) {
			strcpy(list_str, item->title);
		} else {
			make_title_string(list_str, options.title_format, podcast->author, podcast->title, item->title);
		}
	} else {
		make_title_string(list_str, options.title_format, podcast->author, podcast->title, item->title);
	}

	time2time(item->duration, duration_str);

	if ((pldata = playlist_data_new()) == NULL) {
		return 0;
	}

	if (podcast->author) {
		pldata->artist = strndup(podcast->author, MAXLEN-1);
	}
	pldata->album = strdup(podcast->title);
	pldata->title = strdup(item->title);
	pldata->file = strdup(item->file);

	pldata->size = item->size;
	pldata->duration = item->duration;
	pldata->flags = PL_FLAG_COVER;

	/* either parent or dest should be set, but not both */
	gtk_tree_store_insert_before(pl->store, &list_iter, parent, dest);

	gtk_tree_store_set(pl->store, &list_iter,
			   PL_COL_NAME, list_str,
			   PL_COL_VADJ, "",
			   PL_COL_DURA, duration_str,
			   PL_COL_COLO, pl_color_inactive,
			   PL_COL_FONT, PANGO_WEIGHT_NORMAL,
			   PL_COL_DATA, pldata, -1);

	podcast_track_mark_read(&iter_track, item);

	return item->duration;
}

void
podcast_feed_addlist_iter(GtkTreeIter iter_record, playlist_t * pl, GtkTreeIter * dest, int album_mode, int new) {

        GtkTreeIter iter_track;
	GtkTreeIter list_iter;
	GtkTreeIter * plist_iter;
	
	int i;
	float record_duration = 0.0f;
	playlist_data_t * pldata = NULL;

	if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter_record) == 0) {
		return;
	}

	if (album_mode && new) {
		int has_new = 0;

		i = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track, &iter_record, i++)) {
			podcast_item_t * item;
			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track, MS_COL_DATA, &item, -1);
			if (item->new) {
				has_new = 1;
				break;
			}
		}

		if (!has_new) {
			return;
		}
	}

	if (album_mode) {

		char list_str[MAXLEN];
		podcast_t * podcast;

		if ((pldata = playlist_data_new()) == NULL) {
			return;
		}

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record, MS_COL_DATA, &podcast, -1);

		snprintf(list_str, MAXLEN-1, "%s: %s",
			 podcast->author ? podcast->author : _("Podcasts"),
			 podcast->title);

		pldata->artist = strndup(podcast->author ? podcast->author : _("Podcasts"), MAXLEN-1);
		pldata->album = strdup(podcast->title);

		gtk_tree_store_insert_before(pl->store, &list_iter, NULL, dest);
		gtk_tree_store_set(pl->store, &list_iter,
				   PL_COL_NAME, list_str,
				   PL_COL_VADJ, "",
				   PL_COL_DURA, "00:00",
				   PL_COL_COLO, pl_color_inactive,
				   PL_COL_FONT, PANGO_WEIGHT_NORMAL,
				   PL_COL_DATA, pldata, -1);

		plist_iter = &list_iter;
		dest = NULL;
	} else {
		plist_iter = NULL;
	}

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track, &iter_record, i++)) {
		record_duration += podcast_track_addlist_iter(iter_track, pl, plist_iter, dest, new);
	}

	if (album_mode) {
		char duration_str[MAXLEN];
		pldata->duration = record_duration;
		time2time(record_duration, duration_str);
		gtk_tree_store_set(pl->store, &list_iter, PL_COL_DURA, duration_str, -1);
	}
}

void
podcast_track__addlist_cb(gpointer data) {

        GtkTreeIter iter_track;
	playlist_t * pl = playlist_get_current();

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_track)) {
		podcast_track_addlist_iter(iter_track, pl, NULL, (GtkTreeIter *)data, 0);
		if (pl == playlist_get_current()) {
			playlist_content_changed(pl);
		}
		store_podcast_save();
	}
}


void
podcast_track__fileinfo_cb(gpointer data) {

	GtkTreeIter iter_track;
	GtkTreeIter pod_iter;
	GtkTreeModel * model;

	char list_str[MAXLEN];

	if (gtk_tree_selection_get_selected(music_select, &model, &iter_track)) {

		podcast_item_t * item;
		podcast_t * podcast;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track, MS_COL_DATA, &item, -1);

		gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &pod_iter, &iter_track);
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &pod_iter, MS_COL_DATA, &podcast, -1);

		make_title_string(list_str, options.title_format, podcast->author, podcast->title, item->title);

		show_file_info(list_str, item->file, 0, model, iter_track, TRUE);
	}
}

void
podcast_feed__addlist_with_mode(int mode, int new, gpointer data) {

        GtkTreeIter iter_record;
	playlist_t * pl = playlist_get_current();

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_record)) {
		podcast_feed_addlist_iter(iter_record, pl, (GtkTreeIter *)data, mode, new);
		if (pl == playlist_get_current()) {
			playlist_content_changed(pl);
		}
		store_podcast_save();
	}
}

void
podcast_feed__addlist_defmode(gpointer data) {

	podcast_feed__addlist_with_mode(options.playlist_is_tree, 0, data);
}

void
podcast_feed__addlist_albummode_cb(gpointer data) {

	podcast_feed__addlist_with_mode(1, 0, data);
}

void
podcast_feed__addlist_cb(gpointer data) {

	podcast_feed__addlist_with_mode(0, 0, data);
}

void
podcast_feed__addlist_albummode_new_cb(gpointer data) {

	podcast_feed__addlist_with_mode(1, 1, data);
}

void
podcast_feed__addlist_new_cb(gpointer data) {

	podcast_feed__addlist_with_mode(0, 1, data);
}


void
podcast_store_addlist_iter(GtkTreeIter iter_store, playlist_t * pl, GtkTreeIter * dest, int album_mode, int new) {

	GtkTreeIter pod_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &pod_iter, &iter_store, i++)) {
		podcast_feed_addlist_iter(pod_iter, pl, dest, album_mode, new);
	}
}

void
podcast_store__addlist_with_mode(int mode, int new, gpointer data) {

        GtkTreeIter iter_store;
	playlist_t * pl = playlist_get_current();

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_store)) {
		podcast_store_addlist_iter(iter_store, pl, (GtkTreeIter *)data, mode, new);
		if (pl == playlist_get_current()) {
			playlist_content_changed(pl);
		}
		store_podcast_save();
	}
}

void
podcast_store__addlist_defmode(gpointer data) {

	podcast_store__addlist_with_mode(options.playlist_is_tree, 0, data);
}

void
podcast_store__addlist_albummode_cb(gpointer data) {

	podcast_store__addlist_with_mode(1, 0, data);
}

void
podcast_store__addlist_cb(gpointer data) {

	podcast_store__addlist_with_mode(0, 0, data);
}

void
podcast_store__addlist_albummode_new_cb(gpointer data) {

	podcast_store__addlist_with_mode(1, 1, data);
}

void
podcast_store__addlist_new_cb(gpointer data) {

	podcast_store__addlist_with_mode(0, 1, data);
}


void
podcast_feed__subscribe_cb(gpointer data) {

	podcast_t * podcast;

	if (podcast_dialog(&podcast, 1/*create*/)) {

		GtkTreeIter store_iter;
		GtkTreeIter pod_iter;

		store_podcast_get_store_iter(&store_iter);

		gtk_tree_store_append(music_store, &pod_iter, &store_iter);
		gtk_tree_store_set(music_store, &pod_iter,
				   MS_COL_NAME, _("Updating..."),
				   MS_COL_DATA, podcast, -1);

		if (options.enable_ms_tree_icons) {
			gtk_tree_store_set(music_store, &pod_iter, MS_COL_ICON, icon_feed, -1);
		}

		store_podcast_save();
		podcast_update(podcast);
	}
}

void
podcast_feed__edit_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		podcast_t * podcast;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &podcast, -1);

		if (podcast->state != PODCAST_STATE_IDLE) {
			return;
		}

		podcast->state = PODCAST_STATE_UPDATE;

		if (podcast_dialog(&podcast, 0/*edit*/)) {
			store_podcast_save();
		}

		podcast->state = PODCAST_STATE_IDLE;
	}
}

void
podcast_feed__update_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {
		podcast_t * podcast;
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &podcast, -1);

		if (podcast->state != PODCAST_STATE_IDLE) {
			return;
		}

		gtk_tree_store_set(music_store, &iter, MS_COL_NAME, _("Updating..."), -1);
		podcast_update(podcast);
	}
}

void
podcast_feed__abort_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {
		podcast_t * podcast;
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &podcast, -1);
		if (podcast->state == PODCAST_STATE_UPDATE) {
			podcast->state = PODCAST_STATE_ABORTED;
		}
	}
}

void
podcast_feed__remove_check_cb(GtkWidget * widget, gpointer data) {

	set_option_from_toggle(widget, (int *)data);
}

void
podcast_feed__remove_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		GtkWidget * check;
		int unlink_files = 1;
		podcast_t * podcast;
		char * title;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &podcast, -1);

		if (podcast->state != PODCAST_STATE_IDLE) {
			return;
		}

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_NAME, &title, -1);

		check = gtk_check_button_new_with_label(_("Delete downloaded items from the filesystem"));
		gtk_widget_set_name(check, "check_on_window");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), unlink_files);
		g_signal_connect(G_OBJECT(check), "toggled",
				 G_CALLBACK(podcast_feed__remove_check_cb), &unlink_files);

		if (message_dialog(_("Remove feed"),
				   browser_window,
				   GTK_MESSAGE_QUESTION,
				   GTK_BUTTONS_YES_NO,
				   check,
				   _("Really remove '%s' from the Music Store?"),
				   title) == GTK_RESPONSE_YES) {

			if (unlink_files) {
				GSList * node;
				for (node = podcast->items; node; node = node->next) {
					unlink(((podcast_item_t *)node->data)->file);
				}
			}

			store_podcast_remove_podcast(&iter);
			store_podcast_save();
		}

		g_free(title);
	}
}

gboolean
podcast_delayed_update_cb(gpointer data) {

	podcast_update((podcast_t *)data);
	return FALSE;
}

/* data != NULL indicates automatic update */
void
podcast_store__update_cb(gpointer data) {

	podcast_t * podcast;
	GtkTreeIter store_iter;
	GtkTreeIter iter;
	GTimeVal tval;
	int i = 0;

	if (store_podcast_get_store_iter(&store_iter) != 0) {
		return;
	}

	if (data != NULL) {
		g_get_current_time(&tval);
	}

	for (i = 0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter, &store_iter, i); ++i) {

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &podcast, -1);

		if (podcast->state != PODCAST_STATE_IDLE) {
			continue;
		}

		if (data != NULL) {

			if (!(podcast->flags & PODCAST_AUTO_CHECK)) {
				/* auto check not enabled */
				continue;
			}

			if (tval.tv_sec - podcast->last_checked < podcast->check_interval) {
				/* too young to be updated */
				continue;
			}
		}

		podcast->state = PODCAST_STATE_PENDING;
		gtk_tree_store_set(music_store, &iter, MS_COL_NAME, _("Updating..."), -1);
		aqualung_timeout_add(1000 * i, podcast_delayed_update_cb, podcast);
	}
}

void
podcast_store__reorder_cb(gpointer data) {

	GtkWidget * dialog;
	GtkWidget * label;
	GtkWidget * list;
	GtkWidget * viewport;
	GtkListStore * store;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

	GtkTreeIter store_iter;
	GtkTreeIter pod_iter;
	GtkTreeIter list_iter;
	int i, res;


        dialog = gtk_dialog_new_with_buttons(_("Reorder feeds"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

        store = gtk_list_store_new(2,
				   G_TYPE_STRING,    /* title */
				   G_TYPE_POINTER);  /* GtkTreeIter */

	label = gtk_label_new(_("Drag and drop entries in the list\n"
				"to set the feed order in the Music Store."));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, FALSE, 5);

        list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("",
							  renderer,
							  "text", 0,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);
        gtk_tree_view_set_reorderable(GTK_TREE_VIEW(list), TRUE);

	viewport = gtk_viewport_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(viewport), list);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 5);

	i = 0;
	store_podcast_get_store_iter(&store_iter);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &pod_iter, &store_iter, i++)) {
		char title[MAXLEN];
		podcast_t * podcast;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &pod_iter, MS_COL_DATA, &podcast, -1);
		podcast_get_display_name(podcast, title);
		gtk_list_store_append(store, &list_iter);
		gtk_list_store_set(store, &list_iter, 0, title, 1, gtk_tree_iter_copy(&pod_iter), -1);
	}

	gtk_widget_show_all(dialog);

        res = aqualung_dialog_run(GTK_DIALOG(dialog));

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &list_iter, NULL, i++)) {

		GtkTreeIter * iter;
		gtk_tree_model_get(GTK_TREE_MODEL(store), &list_iter, 1, &iter, -1);

		if (res == GTK_RESPONSE_ACCEPT) {
			char sort[16];

			snprintf(sort, 15, "%03d", i);
			gtk_tree_store_set(music_store, iter, MS_COL_SORT, sort, -1);
		}

		gtk_tree_iter_free(iter);
	}

	if (res == GTK_RESPONSE_ACCEPT) {
		store_podcast_save();
	}

	gtk_widget_destroy(dialog);
}

void
podcast_store__update_enabled_cb(gpointer data) {

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(podcast_store__update_enabled))) {
		options.podcasts_autocheck = 1;
	} else {
		options.podcasts_autocheck = 0;
	}
}

void
podcast_tree_mark_read(GtkTreeIter * iter, int depth) {

	if (depth == 3) {
		podcast_item_t * item;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, MS_COL_DATA, &item, -1);
		podcast_track_mark_read(iter, item);
	} else {
		GtkTreeIter iter_child;
		int i = 0;

		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_child, iter, i++)) {
			podcast_tree_mark_read(&iter_child, depth + 1);
		}
	}
}

#ifdef HAVE_EXPORT
void
podcast_track_export(GtkTreeIter * iter_track, export_t * export, char * author, char * feed, int new_only) {

	podcast_item_t * item;
	podcast_t * podcast;

	char artist[MAXLEN];
	char album[MAXLEN];
	char * title;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_track, MS_COL_DATA, &item, -1);

	if (new_only && !item->new) {
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_track, MS_COL_NAME, &title, -1);


	if (author == NULL || feed == NULL) {
		GtkTreeIter iter_podcast;

		gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_podcast, iter_track);
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_podcast, MS_COL_DATA, &podcast, -1);
	}

	strncpy(artist, (author == NULL) ? podcast->author : author, MAXLEN-1);
	strncpy(album, (feed == NULL) ? podcast->title : feed, MAXLEN-1);

	export_append_item(export, item->file, artist, album, title, 0, 0);

	g_free(title);
}

void
podcast_feed_export(GtkTreeIter * iter_podcast, export_t * export, int new_only) {

	GtkTreeIter iter_track;
	podcast_t * podcast;
	int i;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_podcast, MS_COL_DATA, &podcast, -1);

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track, iter_podcast, i++)) {
		podcast_track_export(&iter_track, export, podcast->author, podcast->title, new_only);
	}
}

void
podcast_track__export_cb(gpointer user_data) {

	GtkTreeIter iter_track;
	export_t * export;

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_track)) {

		if ((export = export_new()) == NULL) {
			return;
		}

		podcast_track_export(&iter_track, export, NULL, NULL, 0);

		if (export_start(export)) {
			podcast_tree_mark_read(&iter_track, 3);
			store_podcast_save();
		}
	}
}

void
podcast_feed__export_cb(gpointer user_data) {

	GtkTreeIter iter_podcast;
	export_t * export;

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_podcast)) {

		if ((export = export_new()) == NULL) {
			return;
		}

		podcast_feed_export(&iter_podcast, export, (user_data == (gpointer)1) ? 1 : 0);

		if (export_start(export)) {
			podcast_tree_mark_read(&iter_podcast, 2);
			store_podcast_save();
		}
	}
}

void
podcast_store__export_cb(gpointer user_data) {

	GtkTreeIter iter_store;
	GtkTreeIter iter_podcast;
	export_t * export;
	int i;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter_store)) {

		if ((export = export_new()) == NULL) {
			return;
		}

		i = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     &iter_podcast, &iter_store, i++)) {
			podcast_feed_export(&iter_podcast, export, (user_data == (gpointer)1) ? 1 : 0);
		}

		if (export_start(export)) {
			podcast_tree_mark_read(&iter_store, 1);
			store_podcast_save();
		}
	}
}
#endif /* HAVE_EXPORT */

/*************************************************/

typedef struct {

	podcast_t * podcast;
	podcast_item_t * item;

} podcast_transfer_t;


podcast_transfer_t *
podcast_transfer_new(podcast_t * podcast, podcast_item_t * item) {

	podcast_transfer_t * pt;

	if ((pt = (podcast_transfer_t *)malloc(sizeof(podcast_transfer_t))) == NULL) {
		fprintf(stderr, "podcast_transfer_new: malloc error\n");
		return NULL;
	}

	pt->podcast = podcast;
	pt->item = item;

	return pt;
}

podcast_download_t *
podcast_download_new(podcast_t * podcast) {

	podcast_download_t * pd;

	if ((pd = (podcast_download_t *)calloc(1, sizeof(podcast_download_t))) == NULL) {
		fprintf(stderr, "podcast_download_new: calloc error\n");
		return NULL;
	}

	pd->podcast = podcast;

	return pd;
}

void
podcast_iter_set_display_name(podcast_t * podcast, GtkTreeIter * pod_iter) {

	char name_str[MAXLEN];

	podcast_get_display_name(podcast, name_str);
	gtk_tree_store_set(music_store, pod_iter, MS_COL_NAME, name_str, -1);
}

gboolean
store_podcast_update_podcast_cb(gpointer data) {

	podcast_download_t * pd = (podcast_download_t *)data;
	GtkTreeIter pod_iter;

	if (store_podcast_get_pod_iter(&pod_iter, pd->podcast) != 0) {
		return FALSE;
	}

	podcast_iter_set_display_name(pd->podcast, &pod_iter);
	store_podcast_save();

	pd->podcast->state = PODCAST_STATE_IDLE;

	free(pd);
	return FALSE;
}

gboolean
store_podcast_update_podcast_download_cb(gpointer data) {

	podcast_download_t * pd = (podcast_download_t *)data;
	GtkTreeIter pod_iter;
	char name_str[MAXLEN];

	if (store_podcast_get_pod_iter(&pod_iter, pd->podcast) != 0) {
		return FALSE;
	}

	snprintf(name_str, MAXLEN-1, _("Downloading %d/%d (%d%%) ..."), pd->ncurrent, pd->ndownloads, pd->percent);
	gtk_tree_store_set(music_store, &pod_iter, MS_COL_NAME, name_str, -1);

	return FALSE;
}

gboolean
store_podcast_add_item_cb(gpointer data) {

	podcast_transfer_t * pt = (podcast_transfer_t *)data;
	GtkTreeIter pod_iter;
	GtkTreeIter iter;
	char sort[16];


	if (store_podcast_get_pod_iter(&pod_iter, pt->podcast) != 0) {
		return FALSE;
	}

	snprintf(sort, 15, "%014lld", 10000000000LL - pt->item->date);

	gtk_tree_store_append(music_store, &iter, &pod_iter);
	gtk_tree_store_set(music_store, &iter,
			   MS_COL_NAME, pt->item->title,
			   MS_COL_SORT, sort,
			   MS_COL_FONT, PANGO_WEIGHT_BOLD,
			   MS_COL_DATA, pt->item, -1);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &iter, MS_COL_ICON, icon_track, -1);
	}

	free(pt);

	store_podcast_save();

	return FALSE;
}

gboolean
store_podcast_remove_item_cb(gpointer data) {

	podcast_transfer_t * pt = (podcast_transfer_t *)data;
	podcast_item_t * item;
	GtkTreeIter pod_iter;
	GtkTreeIter iter;
	int i;


	if (store_podcast_get_pod_iter(&pod_iter, pt->podcast) != 0) {
		return FALSE;
	}

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter, &pod_iter, i++)) {

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &item, -1);
		if (pt->item == item) {
			store_podcast_remove_podcast_item(&iter);
			break;
		}
	}

	free(pt);

	store_podcast_save();

	return FALSE;
}

void
store_podcast_update_podcast(podcast_download_t * pd) {

	aqualung_idle_add(store_podcast_update_podcast_cb, pd);
}

void
store_podcast_update_podcast_download(podcast_download_t * pd) {

	aqualung_idle_add(store_podcast_update_podcast_download_cb, pd);
}

void
store_podcast_add_item(podcast_t * podcast, podcast_item_t * item) {

	podcast_transfer_t * pt = podcast_transfer_new(podcast, item);

	if (pt != NULL) {
		aqualung_idle_add(store_podcast_add_item_cb, pt);
	}
}

void
store_podcast_remove_item(podcast_t * podcast, podcast_item_t * item) {

	podcast_transfer_t * pt = podcast_transfer_new(podcast, item);

	if (pt != NULL) {
		aqualung_idle_add(store_podcast_remove_item_cb, pt);
	}
}

static void
set_comment_text(GtkTreeIter * tree_iter, GtkTextIter * text_iter, GtkTextBuffer * buffer) {

	char * comment = NULL;
	void * data;
	GtkTreePath * path;
	int level;

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), tree_iter);
	level = gtk_tree_path_get_depth(path);
	gtk_tree_path_free(path);

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), tree_iter, MS_COL_DATA, &data, -1);

	switch (level) {

	case 2:
		comment = ((podcast_t *)data)->desc;
		break;
	case 3:
		comment = ((podcast_item_t *)data)->desc;
		break;
	}

	if (comment != NULL && comment[0] != '\0') {
		gtk_text_buffer_insert(buffer, text_iter, comment, -1);
	}
}

static void
item_status_bar_info(GtkTreeModel * model, GtkTreeIter * iter, double * size, float * length,
		     int * nnewitem) {

	podcast_item_t * item;

	gtk_tree_model_get(model, iter, MS_COL_DATA, &item, -1);

	*size += item->size / 1024.0;
	*length += item->duration;

	if (item->new) {
		*nnewitem += 1;
	}
}

static void
feed_status_bar_info(GtkTreeModel * model, GtkTreeIter * iter, double * size, float * length,
		     int * nnewitem, int * nitem) {

	GtkTreeIter track_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(model, &track_iter, iter, i++)) {
		item_status_bar_info(model, &track_iter, size, length, nnewitem);
	}

	*nitem += i - 1;
}

static void
store_status_bar_info(GtkTreeModel * model, GtkTreeIter * iter, double * size, float * length,
		      int * nnewitem, int * nitem, int * nfeed) {

	GtkTreeIter record_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(model, &record_iter, iter, i++)) {
		feed_status_bar_info(model, &record_iter, size, length, nnewitem, nitem);
	}

	*nfeed = i - 1;
}

static void
set_status_bar_info(GtkTreeIter * tree_iter, GtkLabel * statusbar) {

	int nnewitem = 0, nitem = 0, nfeed = 0;
	float length = 0.0f;
	double size = 0.0;

	podcast_t * podcast;
	char str[MAXLEN];
	char length_str[MAXLEN];
	char tmp[MAXLEN];
	char * name;

	GtkTreeModel * model = GTK_TREE_MODEL(music_store);
	GtkTreePath * path;
	int depth;


	path = gtk_tree_model_get_path(model, tree_iter);
	depth = gtk_tree_path_get_depth(path);
	gtk_tree_path_free(path);

	gtk_tree_model_get(model, tree_iter, MS_COL_NAME, &name, -1);

	switch (depth) {
	case 3:
		item_status_bar_info(model, tree_iter, &size, &length, &nnewitem);
		sprintf(str, "%s ", name);
		break;
	case 2:
		gtk_tree_model_get(model, tree_iter, MS_COL_DATA, &podcast, -1);
		podcast_get_display_name(podcast, tmp);
		feed_status_bar_info(model, tree_iter, &size, &length, &nnewitem, &nitem);
		if (nnewitem > 0) {
			sprintf(str, "%s:  %d %s, %d %s ", tmp,
				nitem, (nitem == 1) ? _("item") : _("items"),
				nnewitem, (nnewitem == 1) ? _("new item") : _("new items"));
		} else {
			sprintf(str, "%s:  %d %s ", tmp,
				nitem, (nitem == 1) ? _("item") : _("items"));
		}
		break;
	case 1:
		store_status_bar_info(model, tree_iter, &size, &length, &nnewitem, &nitem, &nfeed);
		if (nnewitem > 0) {
			sprintf(str, "%s:  %d %s, %d %s, %d %s ", name,
				nfeed, (nfeed == 1) ? _("feed") : _("feeds"),
				nitem, (nitem == 1) ? _("item") : _("items"),
				nnewitem, (nnewitem == 1) ? _("new item") : _("new items"));
		} else {
			sprintf(str, "%s:  %d %s, %d %s ", name,
				nfeed, (nfeed == 1) ? _("feed") : _("feeds"),
				nitem, (nitem == 1) ? _("item") : _("items"));
		}
		break;
	}

	g_free(name);

	if (length > 0.0f || nitem == 0) {
		time2time(length, length_str);
		sprintf(tmp, " [%s] ", length_str);
	} else {
		strcpy(tmp, " [N/A] ");
	}

	strcat(str, tmp);

	if (options.ms_statusbar_show_size) {
		if (size > 1024 * 1024) {
			sprintf(tmp, " (%.1f GB) ", size / (1024 * 1024));
		} else if (size > 1024) {
			sprintf(tmp, " (%.1f MB) ", size / 1024);
		} else if (size > 0 || nitem == 0) {
			sprintf(tmp, " (%.1f KB) ", size);
		} else {
			strcpy(tmp, " (N/A) ");
		}
		strcat(str, tmp);
	}

	gtk_label_set_text(statusbar, str);
}

static void
set_popup_sensitivity(GtkTreePath * path) {

	int depth = gtk_tree_path_get_depth(path);
	GtkTreeIter iter;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter, path);

	if (depth == 2) {

		podcast_t * podcast;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &podcast, -1);

		gtk_widget_set_sensitive(podcast_feed__edit, podcast->state == PODCAST_STATE_IDLE);
		gtk_widget_set_sensitive(podcast_feed__remove, podcast->state == PODCAST_STATE_IDLE);
		gtk_widget_set_sensitive(podcast_feed__abort, podcast->state != PODCAST_STATE_ABORTED);

		if (podcast->state != PODCAST_STATE_IDLE) {
			gtk_widget_hide(podcast_feed__update);
			gtk_widget_show(podcast_feed__abort);
		} else {
			gtk_widget_show(podcast_feed__update);
			gtk_widget_hide(podcast_feed__abort);
		}
	}

	if (depth == 1) {
		int n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter);

		gtk_widget_set_sensitive(podcast_store__update, n > 0);
		gtk_widget_set_sensitive(podcast_store__reorder, n > 1);
		gtk_widget_set_sensitive(podcast_store__update_enabled, n > 0);
	}
}

static void
add_path_to_playlist(GtkTreePath * path, GtkTreeIter * piter, int new_tab) {

	int depth = gtk_tree_path_get_depth(path);

	gtk_tree_selection_select_path(music_select, path);

	if (new_tab) {
		GtkTreeIter iter;

		gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter, path);

		if (depth == 2) {
			char name[MAXLEN];
			podcast_t * podcast;

			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &podcast, -1);
			podcast_get_display_name(podcast, name);
			playlist_tab_new_if_nonempty(name);
		} else {
			char * name;
			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_NAME, &name, -1);
			playlist_tab_new_if_nonempty(name);
			g_free(name);
		}
	}

	switch (depth) {
	case 1:
		podcast_store__addlist_defmode(piter);
		break;
	case 2:
		podcast_feed__addlist_defmode(piter);
		break;
	case 3:
		podcast_track__addlist_cb(piter);
		break;
	}
}

/*************************************************/
/* music store interface */

int
store_podcast_iter_is_track(GtkTreeIter * iter) {

	GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), iter);
	int ret = (gtk_tree_path_get_depth(p) == 3);
	gtk_tree_path_free(p);
	return ret;
}


void
store_podcast_iter_addlist_defmode(GtkTreeIter * ms_iter, GtkTreeIter * pl_iter, int new_tab) {

	GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), ms_iter);
	add_path_to_playlist(p, pl_iter, new_tab);
	gtk_tree_path_free(p);
}


void
store_podcast_selection_changed(GtkTreeIter * tree_iter, GtkTextBuffer * buffer, GtkLabel * statusbar) {

	GtkTextIter text_iter;

	gtk_text_buffer_get_iter_at_offset(buffer, &text_iter, 0);
	set_comment_text(tree_iter, &text_iter, buffer);

	if (options.enable_mstore_statusbar) {
		set_status_bar_info(tree_iter, statusbar);
	}
}


gboolean
store_podcast_event_cb(GdkEvent * event, GtkTreeIter * iter, GtkTreePath * path) {

	if (event->type == GDK_BUTTON_PRESS) {

		GdkEventButton * bevent = (GdkEventButton *)event;

                if (bevent->button == 3) {

			set_popup_sensitivity(path);

			switch (gtk_tree_path_get_depth(path)) {
			case 1:
				gtk_menu_popup(GTK_MENU(podcast_store_menu), NULL, NULL, NULL, NULL,
					       bevent->button, bevent->time);
				break;
			case 2:
				gtk_menu_popup(GTK_MENU(podcast_feed_menu), NULL, NULL, NULL, NULL,
					       bevent->button, bevent->time);
				break;
			case 3:
				gtk_menu_popup(GTK_MENU(podcast_track_menu), NULL, NULL, NULL, NULL,
					       bevent->button, bevent->time);
				break;
			}
		}
	} 

	if (event->type == GDK_KEY_PRESS) {

		GdkEventKey * kevent = (GdkEventKey *) event;
		int i;
			
		switch (gtk_tree_path_get_depth(path)) {
		case 1: 
			for (i = 0; podcast_store_keybinds[i].callback; ++i) {
				if ((podcast_store_keybinds[i].state == 0 && kevent->state == 0) ||
				    podcast_store_keybinds[i].state & kevent->state) {
					if (kevent->keyval == podcast_store_keybinds[i].keyval1 ||
					    kevent->keyval == podcast_store_keybinds[i].keyval2) {
						(podcast_store_keybinds[i].callback)(NULL);
					}
				}
			}
			break;
		case 2:
			for (i = 0; podcast_feed_keybinds[i].callback; ++i) {
				if ((podcast_feed_keybinds[i].state == 0 && kevent->state == 0) ||
				    podcast_feed_keybinds[i].state & kevent->state) {
					if (kevent->keyval == podcast_feed_keybinds[i].keyval1 ||
					    kevent->keyval == podcast_feed_keybinds[i].keyval2) {
						(podcast_feed_keybinds[i].callback)(NULL);
					}
				}
			}
			break;
		case 3:
			for (i = 0; podcast_track_keybinds[i].callback; ++i) {
				if ((podcast_track_keybinds[i].state == 0 && kevent->state == 0) ||
				    podcast_track_keybinds[i].state & kevent->state) {
					if (kevent->keyval == podcast_track_keybinds[i].keyval1 ||
					    kevent->keyval == podcast_track_keybinds[i].keyval2) {
						(podcast_track_keybinds[i].callback)(NULL);
					}
				}
			}
			break;
		}
	}

	return FALSE;
}


void
store_podcast_load_icons(void) {

	char path[MAXLEN];

	sprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-podcast.png");
	icon_podcasts = gdk_pixbuf_new_from_file (path, NULL);
	sprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-feed.png");
	icon_feed = gdk_pixbuf_new_from_file (path, NULL);
}


void
store_podcast_create_popup_menu(void) {

	GtkWidget * separator;

	/* create popup menu for podcast_track tree items */
	podcast_track_menu = gtk_menu_new();
	register_toplevel_window(podcast_track_menu, TOP_WIN_SKIN);
	podcast_track__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
#ifdef HAVE_EXPORT
	podcast_track__export = gtk_menu_item_new_with_label(_("Export item..."));
#endif /* HAVE_EXPORT */
	podcast_track__fileinfo = gtk_menu_item_new_with_label(_("File info..."));
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_track_menu), podcast_track__addlist);
#ifdef HAVE_EXPORT
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_track_menu), podcast_track__export);
#endif /* HAVE_EXPORT */
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_track_menu), podcast_track__fileinfo);
 	g_signal_connect_swapped(G_OBJECT(podcast_track__addlist), "activate", G_CALLBACK(podcast_track__addlist_cb), NULL);
#ifdef HAVE_EXPORT
 	g_signal_connect_swapped(G_OBJECT(podcast_track__export), "activate", G_CALLBACK(podcast_track__export_cb), NULL);
#endif /* HAVE_EXPORT */
 	g_signal_connect_swapped(G_OBJECT(podcast_track__fileinfo), "activate", G_CALLBACK(podcast_track__fileinfo_cb), NULL);
	gtk_widget_show(podcast_track__addlist);
#ifdef HAVE_EXPORT
	gtk_widget_show(podcast_track__export);
#endif /* HAVE_EXPORT */
	gtk_widget_show(podcast_track__fileinfo);

	/* create popup menu for podcast_feed tree items */
	podcast_feed_menu = gtk_menu_new();
	register_toplevel_window(podcast_feed_menu, TOP_WIN_SKIN);

	podcast_feed__addlist = gtk_menu_item_new_with_label(_("Add all items to playlist"));
	podcast_feed__addlist_albummode = gtk_menu_item_new_with_label(_("Add all items to playlist (Album mode)"));
	podcast_feed__addlist_new = gtk_menu_item_new_with_label(_("Add new items to playlist"));
	podcast_feed__addlist_albummode_new = gtk_menu_item_new_with_label(_("Add new items to playlist (Album mode)"));
	podcast_feed__subscribe = gtk_menu_item_new_with_label(_("Subscribe to new feed"));
	podcast_feed__edit = gtk_menu_item_new_with_label(_("Edit feed"));
#ifdef HAVE_EXPORT
	podcast_feed__export = gtk_menu_item_new_with_label(_("Export all items..."));
	podcast_feed__export_new = gtk_menu_item_new_with_label(_("Export new items..."));
#endif /* HAVE_EXPORT */
	podcast_feed__update = gtk_menu_item_new_with_label(_("Update feed"));
	podcast_feed__abort = gtk_menu_item_new_with_label(_("Abort ongoing update"));
	podcast_feed__remove = gtk_menu_item_new_with_label(_("Remove feed"));

	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), podcast_feed__addlist_new);
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), podcast_feed__addlist_albummode_new);

	separator = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), separator);
	gtk_widget_show(separator);

	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), podcast_feed__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), podcast_feed__addlist_albummode);

	separator = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), separator);
	gtk_widget_show(separator);

	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), podcast_feed__subscribe);
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), podcast_feed__edit);
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), podcast_feed__update);
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), podcast_feed__abort);
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), podcast_feed__remove);

	separator = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), separator);
	gtk_widget_show(separator);

#ifdef HAVE_EXPORT
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), podcast_feed__export_new);
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_feed_menu), podcast_feed__export);
#endif /* HAVE_EXPORT */

 	g_signal_connect_swapped(G_OBJECT(podcast_feed__addlist), "activate", G_CALLBACK(podcast_feed__addlist_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_feed__addlist_albummode), "activate", G_CALLBACK(podcast_feed__addlist_albummode_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_feed__addlist_new), "activate", G_CALLBACK(podcast_feed__addlist_new_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_feed__addlist_albummode_new), "activate", G_CALLBACK(podcast_feed__addlist_albummode_new_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_feed__subscribe), "activate", G_CALLBACK(podcast_feed__subscribe_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_feed__edit), "activate", G_CALLBACK(podcast_feed__edit_cb), NULL);
#ifdef HAVE_EXPORT
 	g_signal_connect_swapped(G_OBJECT(podcast_feed__export), "activate", G_CALLBACK(podcast_feed__export_cb), (gpointer)0);
 	g_signal_connect_swapped(G_OBJECT(podcast_feed__export_new), "activate", G_CALLBACK(podcast_feed__export_cb), (gpointer)1);
#endif /* HAVE_EXPORT */
 	g_signal_connect_swapped(G_OBJECT(podcast_feed__update), "activate", G_CALLBACK(podcast_feed__update_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_feed__abort), "activate", G_CALLBACK(podcast_feed__abort_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_feed__remove), "activate", G_CALLBACK(podcast_feed__remove_cb), NULL);

	gtk_widget_show(podcast_feed__addlist);
	gtk_widget_show(podcast_feed__addlist_albummode);
	gtk_widget_show(podcast_feed__addlist_new);
	gtk_widget_show(podcast_feed__addlist_albummode_new);
	gtk_widget_show(podcast_feed__subscribe);
	gtk_widget_show(podcast_feed__edit);
#ifdef HAVE_EXPORT
	gtk_widget_show(podcast_feed__export);
	gtk_widget_show(podcast_feed__export_new);
#endif /* HAVE_EXPORT */
	gtk_widget_show(podcast_feed__update);
	gtk_widget_show(podcast_feed__remove);

	/* create popup menu for podcast_store tree items */
	podcast_store_menu = gtk_menu_new();
	register_toplevel_window(podcast_store_menu, TOP_WIN_SKIN);
	podcast_store__addlist = gtk_menu_item_new_with_label(_("Add all items to playlist"));
	podcast_store__addlist_albummode = gtk_menu_item_new_with_label(_("Add all items to playlist (Album mode)"));

	podcast_store__addlist_new = gtk_menu_item_new_with_label(_("Add new items to playlist"));
	podcast_store__addlist_albummode_new = gtk_menu_item_new_with_label(_("Add new items to playlist (Album mode)"));
	podcast_store__subscribe = gtk_menu_item_new_with_label(_("Subscribe to new feed"));
	podcast_store__update = gtk_menu_item_new_with_label(_("Update all feeds"));
	podcast_store__reorder = gtk_menu_item_new_with_label(_("Reorder feeds"));
#ifdef HAVE_EXPORT
	podcast_store__export = gtk_menu_item_new_with_label(_("Export all items..."));
	podcast_store__export_new = gtk_menu_item_new_with_label(_("Export new items..."));
#endif /* HAVE_EXPORT */
	podcast_store__update_enabled = gtk_check_menu_item_new_with_label(_("Automatically update feeds"));

	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), podcast_store__addlist_new);
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), podcast_store__addlist_albummode_new);

	separator = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), separator);
	gtk_widget_show(separator);

	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), podcast_store__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), podcast_store__addlist_albummode);

	separator = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), separator);
	gtk_widget_show(separator);

	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), podcast_store__subscribe);
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), podcast_store__update);
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), podcast_store__reorder);

	separator = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), separator);
	gtk_widget_show(separator);

#ifdef HAVE_EXPORT
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), podcast_store__export_new);
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), podcast_store__export);
#endif /* HAVE_EXPORT */

	separator = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), separator);
	gtk_widget_show(separator);

	gtk_menu_shell_append(GTK_MENU_SHELL(podcast_store_menu), podcast_store__update_enabled);

 	g_signal_connect_swapped(G_OBJECT(podcast_store__addlist), "activate", G_CALLBACK(podcast_store__addlist_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_store__addlist_albummode), "activate", G_CALLBACK(podcast_store__addlist_albummode_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_store__addlist_new), "activate", G_CALLBACK(podcast_store__addlist_new_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_store__addlist_albummode_new), "activate", G_CALLBACK(podcast_store__addlist_albummode_new_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_store__subscribe), "activate", G_CALLBACK(podcast_feed__subscribe_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_store__update), "activate", G_CALLBACK(podcast_store__update_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(podcast_store__reorder), "activate", G_CALLBACK(podcast_store__reorder_cb), NULL);
#ifdef HAVE_EXPORT
 	g_signal_connect_swapped(G_OBJECT(podcast_store__export), "activate", G_CALLBACK(podcast_store__export_cb), (gpointer)0);
 	g_signal_connect_swapped(G_OBJECT(podcast_store__export_new), "activate", G_CALLBACK(podcast_store__export_cb), (gpointer)1);
#endif /* HAVE_EXPORT */
 	g_signal_connect_swapped(G_OBJECT(podcast_store__update_enabled), "activate", G_CALLBACK(podcast_store__update_enabled_cb), NULL);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(podcast_store__update_enabled), options.podcasts_autocheck);

	gtk_widget_show(podcast_store__addlist);
	gtk_widget_show(podcast_store__addlist_albummode);
	gtk_widget_show(podcast_store__addlist_new);
	gtk_widget_show(podcast_store__addlist_albummode_new);
	gtk_widget_show(podcast_store__subscribe);
	gtk_widget_show(podcast_store__update);
	gtk_widget_show(podcast_store__reorder);
#ifdef HAVE_EXPORT
	gtk_widget_show(podcast_store__export);
	gtk_widget_show(podcast_store__export_new);
#endif /* HAVE_EXPORT */
	gtk_widget_show(podcast_store__update_enabled);
}

void
store_podcast_set_toolbar_sensitivity(GtkTreeIter * iter, GtkWidget * edit,
					   GtkWidget * add, GtkWidget * remove) {

	GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), iter);
	int depth = gtk_tree_path_get_depth(p);

	gtk_widget_set_sensitive(edit, depth == 2);
	gtk_widget_set_sensitive(add, depth < 3);
	gtk_widget_set_sensitive(remove, depth == 2);

	gtk_tree_path_free(p);
}

void
store_podcast_toolbar__edit_cb(gpointer data) {

	podcast_feed__edit_cb(NULL);
}

void
store_podcast_toolbar__add_cb(gpointer data) {

	podcast_feed__subscribe_cb(NULL);
}

void
store_podcast_toolbar__remove_cb(gpointer data) {

	podcast_feed__remove_cb(NULL);
}

void
create_podcast_node(void) {

	GtkTreeIter iter;
	store_t * data;

	if ((data = (store_t *)malloc(sizeof(store_t))) == NULL) {
		fprintf(stderr, "create_podcast_node: malloc error\n");
		return;
	}

	data->type = STORE_TYPE_PODCAST;

	gtk_tree_store_insert(music_store, &iter, NULL, 0);
	gtk_tree_store_set(music_store, &iter,
			   MS_COL_NAME, _("Podcasts"),
			   MS_COL_SORT, "001",
			   MS_COL_FONT, PANGO_WEIGHT_BOLD,
			   MS_COL_DATA, data, -1);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &iter, MS_COL_ICON, icon_podcasts, -1);
	}
}

gboolean
store_podcast_updater_cb(gpointer data) {

	if (options.podcasts_autocheck) {
		podcast_store__update_cb((gpointer)1);
	}

	return TRUE;
}

void
store_podcast_updater_start(void) {

	store_podcast_updater_cb(NULL);
	aqualung_timeout_add(5*60*1000, store_podcast_updater_cb, NULL);
}

/*************************************************/

void
save_podcast_item(xmlDocPtr doc, xmlNodePtr root, GtkTreeIter * iter) {

	xmlNodePtr node;
	podcast_item_t * item;
	char * sort;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter,
			   MS_COL_SORT, &sort,
			   MS_COL_DATA, &item, -1);
	
	node = xmlNewTextChild(root, NULL, (const xmlChar *) "item", NULL);
	xml_save_str(node, "file", item->file);
	xml_save_str(node, "title", item->title);
	xml_save_str(node, "desc", item->desc);
	xml_save_str(node, "url", item->url);
	xml_save_str(node, "sort", sort);

	xml_save_int(node, "new", item->new);
	xml_save_float(node, "duration", item->duration);
	xml_save_uint(node, "size", item->size);
	xml_save_uint(node, "date", item->date);

	g_free(sort);
}

void
save_podcast(xmlDocPtr doc, xmlNodePtr root, GtkTreeIter * pod_iter) {

	xmlNodePtr node;
	podcast_t * podcast;
	GtkTreeIter iter;
	int i;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), pod_iter, MS_COL_DATA, &podcast, -1);
	
	node = xmlNewTextChild(root, NULL, (const xmlChar *) "channel", NULL);
	xml_save_str(node, "dir", podcast->dir);
	xml_save_str(node, "title", podcast->title);
	xml_save_str(node, "author", podcast->author);
	xml_save_str(node, "desc", podcast->desc);
	xml_save_str(node, "url", podcast->url);

	xml_save_int(node, "flags", podcast->flags);
	xml_save_uint(node, "check_interval", podcast->check_interval);
	xml_save_uint(node, "last_checked", podcast->last_checked);
	xml_save_uint(node, "count_limit", podcast->count_limit);
	xml_save_uint(node, "date_limit", podcast->date_limit);
	xml_save_uint(node, "size_limit", podcast->size_limit);

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter, pod_iter, i++)) {
		save_podcast_item(doc, node, &iter);
	}
}

void
store_podcast_save(void) {

	xmlDocPtr doc;
	xmlNodePtr root;
	int i;
	GtkTreeIter iter_store;
	GtkTreeIter pod_iter;
	char file[MAXLEN];


	if (store_podcast_get_store_iter(&iter_store) != 0) {
		return;
	}

	doc = xmlNewDoc((const xmlChar *) "1.0");
	root = xmlNewNode(NULL, (const xmlChar *) "aqualung_podcast");
	xmlDocSetRootElement(doc, root);

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &pod_iter, &iter_store, i++)) {
		save_podcast(doc, root, &pod_iter);
	}

	snprintf(file, MAXLEN-1, "%s/podcast.xml", options.confdir);
	xmlSaveFormatFile(file, doc, 1);
	xmlFreeDoc(doc);

	music_store_selection_changed(STORE_TYPE_PODCAST);
}

void
parse_podcast_item(xmlDocPtr doc, xmlNodePtr cur, GtkTreeIter * pod_iter, podcast_t * podcast) {

	GtkTreeIter iter;
	podcast_item_t * item;
	char sort[MAXLEN];

	if ((item = podcast_item_new()) == NULL) {
		return;
	}

	podcast->items = g_slist_prepend(podcast->items, item);

	gtk_tree_store_append(music_store, &iter, pod_iter);
	gtk_tree_store_set(music_store, &iter,
			   MS_COL_NAME, "",
			   MS_COL_SORT, "",
			   MS_COL_DATA, item, -1);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &iter, MS_COL_ICON, icon_track, -1);
	}

	for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {

		xml_load_str_dup(doc, cur, "file", &item->file);
		xml_load_str_dup(doc, cur, "title", &item->title);
		xml_load_str_dup(doc, cur, "desc", &item->desc);
		xml_load_str_dup(doc, cur, "url", &item->url);
		xml_load_str(doc, cur, "sort", sort);

		xml_load_int(doc, cur, "new", &item->new);
		xml_load_float(doc, cur, "duration", &item->duration);
		xml_load_uint(doc, cur, "size", &item->size);
		xml_load_uint(doc, cur, "date", &item->date);
	}

	if (item->new) {
		gtk_tree_store_set(music_store, &iter, MS_COL_FONT, PANGO_WEIGHT_BOLD, -1);
	}
	gtk_tree_store_set(music_store, &iter, MS_COL_NAME, item->title, -1);
	gtk_tree_store_set(music_store, &iter, MS_COL_SORT, sort, -1);
}

void
parse_podcast(xmlDocPtr doc, xmlNodePtr cur, GtkTreeIter * iter_store) {

	GtkTreeIter pod_iter;
	podcast_t * podcast;

	if ((podcast = podcast_new()) == NULL) {
		return;
	}

	gtk_tree_store_append(music_store, &pod_iter, iter_store);
	gtk_tree_store_set(music_store, &pod_iter,
			   MS_COL_NAME, "",
			   MS_COL_SORT, "",
			   MS_COL_DATA, podcast, -1);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &pod_iter, MS_COL_ICON, icon_feed, -1);
	}

	for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {

		xml_load_str_dup(doc, cur, "dir", &podcast->dir);
		xml_load_str_dup(doc, cur, "title", &podcast->title);
		xml_load_str_dup(doc, cur, "author", &podcast->author);
		xml_load_str_dup(doc, cur, "desc", &podcast->desc);
		xml_load_str_dup(doc, cur, "url", &podcast->url);

		xml_load_int(doc, cur, "flags", &podcast->flags);
		xml_load_uint(doc, cur, "check_interval", &podcast->check_interval);
		xml_load_uint(doc, cur, "last_checked", &podcast->last_checked);
		xml_load_uint(doc, cur, "count_limit", &podcast->count_limit);
		xml_load_uint(doc, cur, "date_limit", &podcast->date_limit);
		xml_load_uint(doc, cur, "size_limit", &podcast->size_limit);

		if (!xmlStrcmp(cur->name, (const xmlChar *)"item")) {
			parse_podcast_item(doc, cur, &pod_iter, podcast);
		}
	}

	podcast_iter_set_display_name(podcast, &pod_iter);
}

void
store_podcast_load(void) {

	GtkTreeIter iter_store;

	xmlDocPtr doc;
	xmlNodePtr cur;
	char file[MAXLEN];
	
	snprintf(file, MAXLEN-1, "%s/podcast.xml", options.confdir);
	if (!g_file_test(file, G_FILE_TEST_EXISTS)) {
		return;
	}

	if (store_podcast_get_store_iter(&iter_store) != 0) {
		return;
	}

	doc = xmlParseFile(file);
	if (doc == NULL) {
		fprintf(stderr, "An XML error occured while parsing %s\n", file);
		return;
	}
	
	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		fprintf(stderr, "store_file_load: empty XML document\n");
		xmlFreeDoc(doc);
		return;
	}

	if (xmlStrcmp(cur->name, (const xmlChar *)"aqualung_podcast")) {
		fprintf(stderr, "store_file_load: XML document of the wrong type, "
			"root node != aqualung_podcast\n");
		xmlFreeDoc(doc);
		return;
	}

	for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
		if (!xmlStrcmp(cur->name, (const xmlChar *)"channel")) {
			parse_podcast(doc, cur, &iter_store);
		}
	}

	xmlFreeDoc(doc);
}

#endif /* HAVE_PODCAST */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
