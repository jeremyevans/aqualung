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
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <libxml/globals.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef HAVE_CDDB
#include "cddb_lookup.h"
#endif /* HAVE_CDDB */

#ifdef HAVE_TRANSCODING
#include "export.h"
#endif /* HAVE_TRANSCODING */

#include "athread.h"
#include "common.h"
#include "utils.h"
#include "utils_gui.h"
#include "build_store.h"
#include "cover.h"
#include "file_info.h"
#include "decoder/file_decoder.h"
#include "metadata_api.h"
#include "httpc.h"
#include "options.h"
#include "volume.h"
#include "playlist.h"
#include "i18n.h"
#include "music_browser.h"
#include "store_file.h"


extern options_t options;

extern GtkListStore * ms_pathlist_store;
extern char pl_color_inactive[14];

extern GtkWidget * browser_window;
extern GtkTreeStore * music_store;
extern GtkTreeSelection * music_select;
extern GtkWidget * music_tree;

int stop_adding_to_playlist;
int ms_progress_bar_semaphore;
int ms_progress_bar_num;
int ms_progress_bar_den;

GtkWidget * ms_progress_bar;
GtkWidget * ms_progress_bar_container;
GtkWidget * ms_progress_bar_stop_button;

/* popup menus for tree items */
GtkWidget * store_menu;
GtkWidget * store__addlist;
GtkWidget * store__addlist_albummode;
GtkWidget * store__separator1;
GtkWidget * store__add;
GtkWidget * store__build;
GtkWidget * store__edit;
#ifdef HAVE_TRANSCODING
GtkWidget * store__export;
#endif /* HAVE_TRANSCODING */
GtkWidget * store__save;
GtkWidget * store__remove;
GtkWidget * store__separator2;
GtkWidget * store__addart;
GtkWidget * store__separator3;
GtkWidget * store__volume;
GtkWidget * store__volume_menu;
GtkWidget * store__volume_unmeasured;
GtkWidget * store__volume_all;
GtkWidget * store__search;

GtkWidget * artist_menu;
GtkWidget * artist__addlist;
GtkWidget * artist__addlist_albummode;
GtkWidget * artist__separator1;
GtkWidget * artist__add;
GtkWidget * artist__edit;
#ifdef HAVE_TRANSCODING
GtkWidget * artist__export;
#endif /* HAVE_TRANSCODING */
GtkWidget * artist__remove;
GtkWidget * artist__separator2;
GtkWidget * artist__addrec;
GtkWidget * artist__separator3;
GtkWidget * artist__volume;
GtkWidget * artist__volume_menu;
GtkWidget * artist__volume_unmeasured;
GtkWidget * artist__volume_all;
GtkWidget * artist__search;

GtkWidget * record_menu;
GtkWidget * record__addlist;
GtkWidget * record__addlist_albummode;
GtkWidget * record__separator1;
GtkWidget * record__add;
GtkWidget * record__edit;
#ifdef HAVE_TRANSCODING
GtkWidget * record__export;
#endif /* HAVE_TRANSCODING */
GtkWidget * record__remove;
GtkWidget * record__separator2;
GtkWidget * record__addtrk;
#ifdef HAVE_CDDB
GtkWidget * record__cddb;
#endif /* HAVE_CDDB */
GtkWidget * record__separator3;
GtkWidget * record__volume;
GtkWidget * record__volume_menu;
GtkWidget * record__volume_unmeasured;
GtkWidget * record__volume_all;
GtkWidget * record__search;

GtkWidget * track_menu;
GtkWidget * track__addlist;
GtkWidget * track__separator1;
GtkWidget * track__add;
GtkWidget * track__edit;
#ifdef HAVE_TRANSCODING
GtkWidget * track__export;
#endif /* HAVE_TRANSCODING */
GtkWidget * track__remove;
GtkWidget * track__separator2;
GtkWidget * track__fileinfo;
GtkWidget * track__separator3;
GtkWidget * track__volume;
GtkWidget * track__volume_menu;
GtkWidget * track__volume_unmeasured;
GtkWidget * track__volume_all;
GtkWidget * track__search;

extern GdkPixbuf * icon_store;
GdkPixbuf * icon_artist;
GdkPixbuf * icon_record;
GdkPixbuf * icon_track;

GtkWidget * store__tag;
GtkWidget * artist__tag;
GtkWidget * record__tag;
GtkWidget * track__tag;

typedef struct _batch_tag_t {

	char filename[MAXLEN];
	char title[MAXLEN];
	char artist[MAXLEN];
	char album[MAXLEN];
	char comment[MAXLEN];
	char year[MAXLEN];
	int trackno;

	struct _batch_tag_t * next;

} batch_tag_t;

batch_tag_t * batch_tag_root = NULL;


void artist__add_cb(gpointer data);
void record__add_cb(gpointer data);
void track__add_cb(gpointer data);

void store__edit_cb(gpointer data);
void artist__edit_cb(gpointer data);
void record__edit_cb(gpointer data);
void track__edit_cb(gpointer data);

void store__remove_cb(gpointer data);
void artist__remove_cb(gpointer data);
void record__remove_cb(gpointer data);
void track__remove_cb(gpointer data);

#ifdef HAVE_TRANSCODING
void store__export_cb(gpointer data);
void artist__export_cb(gpointer data);
void record__export_cb(gpointer data);
void track__export_cb(gpointer data);
#endif /* HAVE_TRANSCODING */

void store__build_cb(gpointer data);
void store__save_cb(gpointer data);
void store__save_all_cb(gpointer data);

void store__volume_unmeasured_cb(gpointer data);
void store__volume_all_cb(gpointer data);
void artist__volume_unmeasured_cb(gpointer data);
void artist__volume_all_cb(gpointer data);
void record__volume_unmeasured_cb(gpointer data);
void record__volume_all_cb(gpointer data);
void track__volume_unmeasured_cb(gpointer data);
void track__volume_all_cb(gpointer data);

void track__fileinfo_cb(gpointer data);

static gboolean store_model_func(GtkTreeModel * model, GtkTreeIter iter, char**name, char**file);

struct keybinds store_keybinds[] = {
	{store__addlist_defmode, GDK_KEY_a, GDK_KEY_A, 0},
	{store__add_cb, GDK_KEY_n, GDK_KEY_N, 0},
	{store__build_cb, GDK_KEY_b, GDK_KEY_B, 0},
	{store__edit_cb, GDK_KEY_e, GDK_KEY_E, 0},
	{store__build_cb, GDK_KEY_u, GDK_KEY_U, 0},
	{store__save_cb, GDK_KEY_s, GDK_KEY_S, 0},
	{store__volume_unmeasured_cb, GDK_KEY_v, GDK_KEY_V, 0},
	{store__remove_cb, GDK_KEY_Delete, GDK_KEY_KP_Delete, 0},
#ifdef HAVE_TRANSCODING
	{store__export_cb, GDK_KEY_x, GDK_KEY_X, 0},
#endif /* HAVE_TRANSCODING */
	{artist__add_cb, GDK_KEY_n, GDK_KEY_N, GDK_CONTROL_MASK},
	{NULL, 0, 0}
};

struct keybinds artist_keybinds[] = {
	{artist__addlist_defmode, GDK_KEY_a, GDK_KEY_A, 0},
	{artist__add_cb, GDK_KEY_n, GDK_KEY_N, 0},
	{artist__edit_cb, GDK_KEY_e, GDK_KEY_E, 0},
	{artist__volume_unmeasured_cb, GDK_KEY_v, GDK_KEY_V, 0},
	{artist__remove_cb, GDK_KEY_Delete, GDK_KEY_KP_Delete, 0},
#ifdef HAVE_TRANSCODING
	{artist__export_cb, GDK_KEY_x, GDK_KEY_X, 0},
#endif /* HAVE_TRANSCODING */
	{record__add_cb, GDK_KEY_n, GDK_KEY_N, GDK_CONTROL_MASK},
	{NULL, 0, 0}
};

struct keybinds record_keybinds[] = {
	{record__addlist_defmode, GDK_KEY_a, GDK_KEY_A, 0},
	{record__add_cb, GDK_KEY_n, GDK_KEY_N, 0},
	{record__edit_cb, GDK_KEY_e, GDK_KEY_E, 0},
	{record__volume_unmeasured_cb, GDK_KEY_v, GDK_KEY_V, 0},
	{record__remove_cb, GDK_KEY_Delete, GDK_KEY_KP_Delete, 0},
#ifdef HAVE_TRANSCODING
	{record__export_cb, GDK_KEY_x, GDK_KEY_X, 0},
#endif /* HAVE_TRANSCODING */
	{track__add_cb, GDK_KEY_n, GDK_KEY_N, GDK_CONTROL_MASK},
	{NULL, 0, 0}
};

struct keybinds track_keybinds[] = {
	{track__addlist_cb, GDK_KEY_a, GDK_KEY_A, 0},
	{track__add_cb, GDK_KEY_n, GDK_KEY_N, 0},
	{track__edit_cb, GDK_KEY_e, GDK_KEY_E, 0},
	{track__volume_unmeasured_cb, GDK_KEY_v, GDK_KEY_V, 0},
	{track__remove_cb, GDK_KEY_Delete, GDK_KEY_KP_Delete, 0},
	{track__fileinfo_cb, GDK_KEY_i, GDK_KEY_I, 0},
#ifdef HAVE_TRANSCODING
	{track__export_cb, GDK_KEY_x, GDK_KEY_X, 0},
#endif /* HAVE_TRANSCODING */
	{NULL, 0, 0}
};


void
store_data_free(store_data_t * data) {

	free(data->file);
	if (data->comment != NULL) {
		free(data->comment);
	}
	free(data);
}

void
artist_data_free(artist_data_t * data) {

	if (data->comment != NULL) {
		free(data->comment);
	}
	free(data);
}

void
record_data_free(record_data_t * data) {

	if (data->comment != NULL) {
		free(data->comment);
	}
	free(data);
}

void
track_data_free(track_data_t * data) {

	free(data->file);
	if (data->comment != NULL) {
		free(data->comment);
	}
	free(data);
}

gboolean
store_file_remove_track(GtkTreeIter * iter) {

	track_data_t * data;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, MS_COL_DATA, &data, -1);
	track_data_free(data);

	return gtk_tree_store_remove(music_store, iter);
}

gboolean
store_file_remove_record(GtkTreeIter * iter) {

	record_data_t * data;
	GtkTreeIter track_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &track_iter, iter, i++)) {
		store_file_remove_track(&track_iter);
	}

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, MS_COL_DATA, &data, -1);
	record_data_free(data);

	return gtk_tree_store_remove(music_store, iter);
}

gboolean
store_file_remove_artist(GtkTreeIter * iter) {

	artist_data_t * data;
	GtkTreeIter record_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &record_iter, iter, i++)) {
		store_file_remove_record(&record_iter);
	}

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, MS_COL_DATA, &data, -1);
	artist_data_free(data);

	return gtk_tree_store_remove(music_store, iter);
}

gboolean
store_file_remove_store(GtkTreeIter * iter) {

	store_data_t * data;
	GtkTreeIter artist_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &artist_iter, iter, i++)) {
		store_file_remove_artist(&artist_iter);
	}

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, MS_COL_DATA, &data, -1);
	store_data_free(data);

	return gtk_tree_store_remove(music_store, iter);
}


void
create_dialog_layout(char * title, GtkWidget ** dialog, GtkWidget ** table, int rows) {

        *dialog = gtk_dialog_new_with_buttons(title,
					      GTK_WINDOW(browser_window),
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					      NULL);
        gtk_widget_set_size_request(*dialog, 400, -1);
	gtk_window_set_position(GTK_WINDOW(*dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_dialog_set_default_response(GTK_DIALOG(*dialog), GTK_RESPONSE_ACCEPT);

	*table = gtk_table_new(rows, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(*dialog))),
			   *table, FALSE, TRUE, 2);

}

void
insert_comment_text_view(GtkWidget * dialog, GtkTextBuffer ** buffer, char * text) {

	GtkWidget * content_area;
	GtkWidget * hbox;
	GtkWidget * viewport;
        GtkWidget * scrwin;
	GtkWidget * label;
	GtkWidget * view;

	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(content_area), hbox, FALSE, TRUE, 2);

	label = gtk_label_new(_("Comments:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(content_area), viewport, TRUE, TRUE, 2);

        scrwin = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrwin);

	*buffer = gtk_text_buffer_new(NULL);
	if (text != NULL) {
		gtk_text_buffer_set_text(GTK_TEXT_BUFFER(*buffer), text, -1);
	}

        view = gtk_text_view_new_with_buffer(*buffer);
        gtk_widget_set_size_request(view, -1, 100);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(view), TRUE);
        gtk_container_add(GTK_CONTAINER(scrwin), view);
}

void
browse_button_store_clicked(GtkButton * button, gpointer data) {

	file_chooser_with_entry(_("Please select the xml file for this store."),
				browser_window,
				GTK_FILE_CHOOSER_ACTION_SAVE,
				FILE_CHOOSER_FILTER_STORE,
				(GtkWidget *)data,
				options.storedir, CHAR_ARRAY_SIZE(options.storedir));
}

int
add_store_dialog(char * name, size_t name_size, store_data_t ** data) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * name_entry;
	GtkWidget * file_entry;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

	create_dialog_layout(_("Create empty store"), &dialog, &table, 2);
	insert_label_entry(table, _("Visible name:"), &name_entry, NULL, 0, 1, TRUE);
	insert_label_entry_browse(table, _("Filename:"), &file_entry, options.storedir, 1, 2,
				  browse_button_store_clicked);
	insert_comment_text_view(dialog, &buffer, NULL);

	gtk_widget_grab_focus(name_entry);
	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		const char * pfile = g_filename_from_utf8(gtk_entry_get_text(GTK_ENTRY(file_entry)), -1, NULL, NULL, NULL);
		char file[MAXLEN];

		g_strlcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)), name_size);
		if (name[0] == '\0') {
			gtk_widget_grab_focus(name_entry);
			goto display;
		}

		file[0] = '\0';
		if (pfile == NULL || pfile[0] == '\0') {
			gtk_widget_grab_focus(file_entry);
			goto display;
		}

		if ((*data = (store_data_t *)calloc(1, sizeof(store_data_t))) == NULL) {
			fprintf(stderr, "add_store_dialog: calloc error\n");
			return 0;
		}

		(*data)->type = STORE_TYPE_FILE;

		normalize_filename(pfile, file, CHAR_ARRAY_SIZE(file));
		free_strdup(&(*data)->file, file);

		arr_strlcpy(options.storedir, file);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		free_strdup(&(*data)->comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
								&iter_start, &iter_end, TRUE));
		gtk_widget_destroy(dialog);
		return 1;
        } else {
		gtk_widget_destroy(dialog);
                return 0;
        }
}


int
edit_store_dialog(char * name, size_t name_size, store_data_t * data) {

	GtkWidget * dialog;
	GtkWidget * content_area;
	GtkWidget * table;
	GtkWidget * name_entry;
	GtkWidget * file_entry;
	GtkWidget * rel_check;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;
	char * utf8;

	create_dialog_layout(_("Edit Store"), &dialog, &table, 3);
	insert_label_entry(table, _("Visible name:"), &name_entry, name, 0, 1, TRUE);

	utf8 = g_filename_to_utf8(data->file, -1, NULL, NULL, NULL);
	insert_label_entry(table, _("Filename:"), &file_entry, utf8, 1, 2, FALSE);
	g_free(utf8);

	insert_comment_text_view(dialog, &buffer, data->comment);

	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	rel_check = gtk_check_button_new_with_label(_("Use relative paths in store file"));
	gtk_widget_set_name(rel_check, "check_on_window");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rel_check), data->use_relative_paths);
	gtk_box_pack_start(GTK_BOX(content_area), rel_check, FALSE, FALSE, 5);

	gtk_box_pack_start(GTK_BOX(content_area), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE,
			   FALSE, 5);

	gtk_widget_grab_focus(name_entry);
	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		g_strlcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)), name_size);

		if (name[0] == '\0') {
			gtk_widget_grab_focus(name_entry);
			goto display;
		}

		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		free_strdup(&data->comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
								&iter_start, &iter_end, TRUE));
		set_option_from_toggle(rel_check, &data->use_relative_paths);

		gtk_widget_destroy(dialog);
		return 1;
        } else {
		gtk_widget_destroy(dialog);
                return 0;
        }
}

void
entry_copy_text(GtkEntry * entry, gpointer data) {

	gtk_entry_set_text(GTK_ENTRY(data), gtk_entry_get_text(entry));
	gtk_widget_grab_focus((GtkWidget *)data);
}

int
add_artist_dialog(char * name, size_t name_size, char * sort, size_t sort_size, artist_data_t ** data) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * name_entry;
	GtkWidget * sort_entry;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

	create_dialog_layout(_("Add Artist"), &dialog, &table, 2);
	insert_label_entry(table, _("Visible name:"), &name_entry, name, 0, 1, TRUE);
	insert_label_entry(table, _("Name to sort by:"), &sort_entry, sort, 1, 2, TRUE);
	insert_comment_text_view(dialog, &buffer, NULL);

        g_signal_connect(G_OBJECT(name_entry), "activate", G_CALLBACK(entry_copy_text), sort_entry);

	gtk_widget_grab_focus(name_entry);
	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	sort[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		g_strlcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)), name_size);

		if (name[0] == '\0') {
			gtk_widget_grab_focus(name_entry);
			goto display;
		}

		g_strlcpy(sort, gtk_entry_get_text(GTK_ENTRY(sort_entry)), sort_size);

		if ((*data = (artist_data_t *)calloc(1, sizeof(artist_data_t))) == NULL) {
			fprintf(stderr, "add_artist_dialog: calloc error\n");
			return 0;
		}

		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		free_strdup(&(*data)->comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
								&iter_start, &iter_end, TRUE));
		gtk_widget_destroy(dialog);
		return 1;
        } else {
		gtk_widget_destroy(dialog);
		return 0;
        }
}


int
edit_artist_dialog(char * name, size_t name_size, char * sort, size_t sort_size, artist_data_t * data) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * name_entry;
	GtkWidget * sort_entry;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

	create_dialog_layout(_("Edit Artist"), &dialog, &table, 2);
	insert_label_entry(table, _("Visible name:"), &name_entry, name, 0, 1, TRUE);
	insert_label_entry(table, _("Name to sort by:"), &sort_entry, sort, 1, 2, TRUE);
	insert_comment_text_view(dialog, &buffer, data->comment);

        g_signal_connect(G_OBJECT(name_entry), "activate", G_CALLBACK(entry_copy_text), sort_entry);

	gtk_widget_grab_focus(name_entry);
	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	sort[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		g_strlcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)), name_size);

		if (name[0] == '\0') {
			gtk_widget_grab_focus(name_entry);
			goto display;
		}

		g_strlcpy(sort, gtk_entry_get_text(GTK_ENTRY(sort_entry)), sort_size);

		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		free_strdup(&data->comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
								&iter_start, &iter_end, TRUE));
		gtk_widget_destroy(dialog);
		return 1;
        } else {
		gtk_widget_destroy(dialog);
                return 0;
        }
}

void
browse_button_record_clicked(GtkButton * button, gpointer data) {

	GtkListStore * store = (GtkListStore *)data;
	GtkTreeIter iter;
        GSList * lfiles, * node;


	lfiles = file_chooser(_("Please select the audio files for this record."),
			      browser_window,
			      GTK_FILE_CHOOSER_ACTION_OPEN,
			      FILE_CHOOSER_FILTER_AUDIO,
			      TRUE,
			      options.audiodir, CHAR_ARRAY_SIZE(options.audiodir));

	for (node = lfiles; node; node = node->next) {

		char * filename = (char *)node->data;

		if (filename[strlen(filename)-1] != '/') {

			char * utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, 0, utf8, -1);
			g_free(utf8);
		}

		g_free(node->data);
	}

	g_slist_free(lfiles);
}


void
clicked_tracklist_header(GtkWidget * widget, gpointer data) {

	GtkListStore * store = (GtkListStore *)data;

	gtk_list_store_clear(store);
}


int
add_record_dialog(char * name, size_t name_size, char * sort, size_t sort_size, char *** strings, record_data_t ** data) {

	GtkWidget * dialog;
	GtkWidget * content_area;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * name_entry;
	GtkWidget * sort_entry;
	GtkWidget * year_spin;
	GtkWidget * list_label;

	GtkWidget * viewport;
	GtkWidget * scrolled_win;
	GtkWidget * tracklist_tree;
	GtkListStore * store;
	GtkCellRenderer * cell;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	gchar * str;

	GtkWidget * browse_button;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;
	int n, i;


	create_dialog_layout(_("Add Record"), &dialog, &table, 3);
	insert_label_entry(table, _("Visible name:"), &name_entry, name, 0, 1, TRUE);
	insert_label_spin(table, _("Year:"), &year_spin, 0, 1, 2);
	insert_label_entry(table, _("Name to sort by:"), &sort_entry, sort, 2, 3, TRUE);

        g_signal_connect(G_OBJECT(name_entry), "activate", G_CALLBACK(entry_copy_text), sort_entry);
        g_signal_connect(G_OBJECT(year_spin), "activate", G_CALLBACK(entry_copy_text), sort_entry);

	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	list_label = gtk_label_new(_("Auto-create tracks from these files:"));
        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(hbox), list_label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(content_area), hbox, FALSE, TRUE, 2);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_widget_set_size_request(viewport, -1, 150);
        gtk_box_pack_start(GTK_BOX(content_area), viewport, TRUE, TRUE, 2);

	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_win);

	/* setup track list */
	store = gtk_list_store_new(1, G_TYPE_STRING);
	tracklist_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
        gtk_container_add(GTK_CONTAINER(scrolled_win), tracklist_tree);
	gtk_widget_set_size_request(tracklist_tree, 250, 50);

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Clear list"), cell, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tracklist_tree), GTK_TREE_VIEW_COLUMN(column));
	gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tracklist_tree), TRUE);
	g_signal_connect(G_OBJECT(gtk_tree_view_column_get_button(column)),
                         "clicked", G_CALLBACK(clicked_tracklist_header), store);
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), 0, GTK_SORT_ASCENDING);

	browse_button = gui_stock_label_button(_("_Add files..."), GTK_STOCK_ADD);
        gtk_box_pack_start(GTK_BOX(content_area), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked", G_CALLBACK(browse_button_record_clicked), store);

	insert_comment_text_view(dialog, &buffer, NULL);

	gtk_widget_grab_focus(name_entry);
	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	sort[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		g_strlcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)), name_size);

		if (name[0] == '\0') {
			gtk_widget_grab_focus(name_entry);
			goto display;
		}

		g_strlcpy(sort, gtk_entry_get_text(GTK_ENTRY(sort_entry)), sort_size);

		if ((n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL)) > 0) {

			gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
			if (!(*strings = calloc(n + 1, sizeof(char *)))) {
				fprintf(stderr, "add_record_dialog(): calloc error\n");
				return 0;
			}
			for (i = 0; i < n; i++) {

				char * filename;

				gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, &str, -1);
				gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);

				filename = g_filename_from_utf8(str, -1, NULL, NULL, NULL);
				(*strings)[i] = strdup(filename);

				g_free(str);
				g_free(filename);

				if (!(*strings)[i]) {
					fprintf(stderr, "add_record_dialog(): strdup error\n");
					return 0;
				}
			}
			(*strings)[i] = NULL;
		}

		if ((*data = (record_data_t *)calloc(1, sizeof(record_data_t))) == NULL) {
			fprintf(stderr, "add_record_dialog: calloc error\n");
			return 0;
		}

		(*data)->year = gtk_spin_button_get_value(GTK_SPIN_BUTTON(year_spin));

		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		free_strdup(&(*data)->comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
								   &iter_start, &iter_end, TRUE));

		gtk_widget_destroy(dialog);
		return 1;
        } else {
		gtk_widget_destroy(dialog);
		return 0;
        }
}


int
edit_record_dialog(char * name, size_t name_size, char * sort, size_t sort_size, record_data_t * data) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * name_entry;
	GtkWidget * sort_entry;
	GtkWidget * year_spin;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

	create_dialog_layout(_("Edit Record"), &dialog, &table, 3);
	insert_label_entry(table, _("Visible name:"), &name_entry, name, 0, 1, TRUE);
	insert_label_spin(table, _("Year:"), &year_spin, data->year, 1, 2);
	insert_label_entry(table, _("Name to sort by:"), &sort_entry, sort, 2, 3, TRUE);
	insert_comment_text_view(dialog, &buffer, data->comment);

        g_signal_connect(G_OBJECT(name_entry), "activate", G_CALLBACK(entry_copy_text), sort_entry);
        g_signal_connect(G_OBJECT(year_spin), "activate", G_CALLBACK(entry_copy_text), sort_entry);

	gtk_widget_grab_focus(name_entry);
	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	sort[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		g_strlcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)), name_size);

		if (name[0] == '\0') {
			gtk_widget_grab_focus(name_entry);
			goto display;
		}

		g_strlcpy(sort, gtk_entry_get_text(GTK_ENTRY(sort_entry)), sort_size);

		data->year = gtk_spin_button_get_value(GTK_SPIN_BUTTON(year_spin));

		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		free_strdup(&data->comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
								&iter_start, &iter_end, TRUE));
		gtk_widget_destroy(dialog);
		return 1;
        } else {
		gtk_widget_destroy(dialog);
                return 0;
        }
}


void
browse_button_track_clicked(GtkButton * button, gpointer data) {

	file_chooser_with_entry(_("Please select the audio file for this track."),
				browser_window,
				GTK_FILE_CHOOSER_ACTION_OPEN,
				FILE_CHOOSER_FILTER_AUDIO,
				(GtkWidget *)data,
				options.audiodir, CHAR_ARRAY_SIZE(options.audiodir));
}


int
add_track_dialog(char * name, size_t name_size, char * sort, size_t sort_size, track_data_t ** data) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * name_entry;
	GtkWidget * sort_entry;
	GtkWidget * file_entry;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

	create_dialog_layout(_("Add Track"), &dialog, &table, 3);
	insert_label_entry(table, _("Visible name:"), &name_entry, NULL, 0, 1, TRUE);
	insert_label_entry(table, _("Name to sort by:"), &sort_entry, NULL, 1, 2, TRUE);
	insert_label_entry_browse(table, _("Filename:"), &file_entry, options.audiodir, 2, 3,
				  browse_button_track_clicked);
	insert_comment_text_view(dialog, &buffer, NULL);

	gtk_widget_grab_focus(name_entry);
	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	sort[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		const char * pfile = g_filename_from_utf8(gtk_entry_get_text(GTK_ENTRY(file_entry)), -1, NULL, NULL, NULL);
		char file[MAXLEN];
		float duration;

		g_strlcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)), name_size);
		if (name[0] == '\0') {
			gtk_widget_grab_focus(name_entry);
			goto display;
		}

		file[0] = '\0';
		if (pfile == NULL || pfile[0] == '\0') {
			gtk_widget_grab_focus(file_entry);
			goto display;
		}

		if ((*data = (track_data_t *)calloc(1, sizeof(track_data_t))) == NULL) {
			fprintf(stderr, "add_track_dialog: calloc error\n");
			return 0;
		}

		g_strlcpy(sort, gtk_entry_get_text(GTK_ENTRY(sort_entry)), sort_size);

		normalize_filename(pfile, file, CHAR_ARRAY_SIZE(file));
		free_strdup(&(*data)->file, file);

		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		free_strdup(&(*data)->comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
								&iter_start, &iter_end, TRUE));
		duration = get_file_duration((*data)->file);
		(*data)->duration = (duration > 0.0f) ? duration : 0.0f;
		(*data)->volume = 1.0f;

		gtk_widget_destroy(dialog);
		return 1;
        } else {
		gtk_widget_destroy(dialog);
		return 0;
	}
}


void
use_rva_check_button_cb(GtkWidget * widget, gpointer data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		gtk_widget_set_sensitive((GtkWidget *)data, TRUE);
    	} else {
		gtk_widget_set_sensitive((GtkWidget *)data, FALSE);
	}
}

void
edit_track_done(GtkEntry * entry, gpointer data) {

	gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_ACCEPT);
}

int
edit_track_dialog(char * name, size_t name_size, char * sort, size_t sort_size, track_data_t * data) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * name_entry;
	GtkWidget * sort_entry;
	GtkWidget * file_entry;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;
	GtkWidget * table2;
	GtkWidget * duration_entry;
	GtkWidget * volume_entry;
	GtkWidget * check_button;
	GObject * adj_manual_rva;
	GtkWidget * spin_button;

	char str[MAXLEN];
	char * utf8;

	create_dialog_layout(_("Edit Track"), &dialog, &table, 6);
	insert_label_entry(table, _("Visible name:"), &name_entry, name, 0, 1, TRUE);
	insert_label_entry(table, _("Name to sort by:"), &sort_entry, sort, 1, 2, TRUE);

        g_signal_connect(G_OBJECT(name_entry), "activate", G_CALLBACK(edit_track_done), dialog);

	utf8 = g_filename_to_utf8(data->file, -1, NULL, NULL, NULL);
	insert_label_entry_browse(table, _("Filename:"), &file_entry, utf8, 2, 3,
				  browse_button_track_clicked);
	g_free(utf8);

	time2time(data->duration, str, CHAR_ARRAY_SIZE(str));
	insert_label_entry(table, _("Duration:"), &duration_entry, str, 3, 4, FALSE);

	if (data->volume <= 0.1f) {
		arr_snprintf(str, "%.1f dBFS", data->volume);
	} else {
		arr_snprintf(str, _("Unmeasured"));
	}
	insert_label_entry(table, _("Volume level:"), &volume_entry, str, 4, 5, FALSE);

        table2 = gtk_table_new(1, 2, FALSE);
        gtk_table_attach(GTK_TABLE(table), table2, 0, 2, 5, 6,
                         GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	check_button = gtk_check_button_new_with_label(_("Use manual RVA value [dB]"));
	gtk_widget_set_name(check_button, "check_on_window");
        gtk_table_attach(GTK_TABLE(table2), check_button, 0, 1, 0, 1,
                         GTK_FILL, GTK_FILL, 5, 3);

	adj_manual_rva = G_OBJECT(gtk_adjustment_new(data->rva, -70.0f, 20.0f, 0.1f, 1.0f, 0.0f));
	spin_button = gtk_spin_button_new(GTK_ADJUSTMENT(adj_manual_rva), 0.3, 1);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_button), TRUE);
	gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_button), FALSE);
        gtk_table_attach(GTK_TABLE(table2), spin_button, 1, 2, 0, 1,
                         GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 3);

	g_signal_connect(G_OBJECT(check_button), "toggled", G_CALLBACK(use_rva_check_button_cb),
			 (gpointer)spin_button);

	if (data->use_rva) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), TRUE);
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), FALSE);
		gtk_widget_set_sensitive(spin_button, FALSE);
	}

	insert_comment_text_view(dialog, &buffer, data->comment);

	gtk_widget_grab_focus(name_entry);
	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	sort[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		const char * pfile = g_filename_from_utf8(gtk_entry_get_text(GTK_ENTRY(file_entry)), -1, NULL, NULL, NULL);
		char file[MAXLEN];

		g_strlcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)), name_size);
		if (name[0] == '\0') {
			gtk_widget_grab_focus(name_entry);
			goto display;
		}

		file[0] = '\0';
		if (pfile == NULL || pfile[0] == '\0') {
			gtk_widget_grab_focus(file_entry);
			goto display;
		}

		g_strlcpy(sort, gtk_entry_get_text(GTK_ENTRY(sort_entry)), sort_size);

		normalize_filename(pfile, file, CHAR_ARRAY_SIZE(file));
		free_strdup(&data->file, file);

		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		free_strdup(&data->comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
								&iter_start, &iter_end, TRUE));

		data->rva = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_manual_rva));

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_button))) {
			data->use_rva = 1;
		} else {
			data->use_rva = 0;
		}

		gtk_widget_destroy(dialog);
		return 1;
        } else {
		gtk_widget_destroy(dialog);
		return 0;
        }
}


int
confirm_dialog(char * title, char * text) {

        int ret = message_dialog(title,
				 browser_window,
				 GTK_MESSAGE_QUESTION,
				 GTK_BUTTONS_YES_NO,
				 NULL,
				 text);

        return (ret == GTK_RESPONSE_YES);
}


int
is_store_path_readonly(GtkTreePath * p) {

	GtkTreeIter iter;
        GtkTreePath * path;
	store_data_t * data;

	path = gtk_tree_path_copy(p);

        while (gtk_tree_path_get_depth(path) > 1) {
                gtk_tree_path_up(path);
        }

        gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &data, -1);
	gtk_tree_path_free(path);

        return data->readonly;
}


int
is_store_iter_readonly(GtkTreeIter * i) {

	GtkTreePath * path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), i);
	int ret = is_store_path_readonly(path);
	gtk_tree_path_free(path);
        return ret;
}


static void
set_popup_sensitivity(GtkTreePath * path) {

	gboolean writable = !is_store_path_readonly(path);
	gboolean tag_free = (batch_tag_root == NULL);

	gtk_widget_set_sensitive(store__build, writable && !build_is_busy());
	gtk_widget_set_sensitive(store__edit, writable);
	gtk_widget_set_sensitive(store__save, writable);
	gtk_widget_set_sensitive(store__addart, writable);
	gtk_widget_set_sensitive(store__volume, writable);

	gtk_widget_set_sensitive(artist__add, writable);
	gtk_widget_set_sensitive(artist__edit, writable);
	gtk_widget_set_sensitive(artist__remove, writable);
	gtk_widget_set_sensitive(artist__addrec, writable);
	gtk_widget_set_sensitive(artist__volume, writable);

	gtk_widget_set_sensitive(record__add, writable);
	gtk_widget_set_sensitive(record__edit, writable);
	gtk_widget_set_sensitive(record__remove, writable);
	gtk_widget_set_sensitive(record__addtrk, writable);
#ifdef HAVE_CDDB
	gtk_widget_set_sensitive(record__cddb, writable);
#endif /* HAVE_CDDB */
	gtk_widget_set_sensitive(record__volume, writable);

	gtk_widget_set_sensitive(track__add, writable);
	gtk_widget_set_sensitive(track__edit, writable);
	gtk_widget_set_sensitive(track__remove, writable);
	gtk_widget_set_sensitive(track__volume, writable);

	gtk_widget_set_sensitive(store__tag, tag_free);
	gtk_widget_set_sensitive(artist__tag, tag_free);
	gtk_widget_set_sensitive(record__tag, tag_free);
	gtk_widget_set_sensitive(track__tag, tag_free);
}


static void
add_path_to_playlist(GtkTreePath * path, GtkTreeIter * piter, int new_tab) {

	int depth = gtk_tree_path_get_depth(path);

	gtk_tree_selection_select_path(music_select, path);

	if (new_tab) {
		char * name;
		void * data;
		GtkTreeIter iter;

		gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter, path);
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_NAME, &name, MS_COL_DATA, &data, -1);

		if (depth == 1 && ((store_data_t *)data)->dirty) {
			playlist_tab_new_if_nonempty(name + 1);
		} else {
			playlist_tab_new_if_nonempty(name);
		}

		g_free(name);
	}

	switch (depth) {
	case 1:
		store__addlist_defmode(piter);
		break;
	case 2:
		artist__addlist_defmode(piter);
		break;
	case 3:
		record__addlist_defmode(piter);
		break;
	case 4:
		track__addlist_cb(piter);
		break;
	}
}


/****************************************/


void
generic_remove_cb(char * title, int (* remove_cb)(GtkTreeIter *)) {

	GtkTreeIter iter;
	GtkTreeModel * model;
	int ok = 1;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) {
			return;
		}

		if (options.ms_confirm_removal) {
			char * name;
			char text[MAXLEN];

			gtk_tree_model_get(model, &iter, MS_COL_NAME, &name, -1);
			arr_snprintf(text, _("Really remove \"%s\" from the Music Store?"), name);
			g_free(name);

			ok = confirm_dialog(title, text);
		}

		if (ok) {

			GtkTreeIter parent;

			music_store_mark_changed(&iter);
			gtk_tree_model_iter_parent(model, &parent, &iter);

			if (remove_cb(&iter)) {
				gtk_tree_selection_select_iter(music_select, &iter);
			} else {
				int last;

				if ((last = gtk_tree_model_iter_n_children(model, &parent))) {
					gtk_tree_model_iter_nth_child(model, &iter, &parent, last-1);
					gtk_tree_selection_select_iter(music_select, &iter);
				} else {
					gtk_tree_selection_select_iter(music_select, &parent);
				}
			}
		}
	}
}


/* returns the duration of the track */
float
track_addlist_iter(GtkTreeIter iter_track, playlist_t * pl,
		   GtkTreeIter * parent, GtkTreeIter * dest,
		   float avg_voladj, int use_avg_voladj) {

	GtkTreeIter dest_parent;
        GtkTreeIter iter_artist;
        GtkTreeIter iter_record;
	GtkTreeIter list_iter;

	track_data_t * data;

	char list_str[MAXLEN];
	char * artist_name;
	char * record_name;
	char * track_name;

	float voladj = 0.0f;
	char voladj_str[32];
	char duration_str[MAXLEN];

	file_decoder_t * fdec = NULL;
	playlist_data_t * pldata = NULL;


	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track,
			   MS_COL_NAME, &track_name,
			   MS_COL_DATA, &data, -1);

	gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_record, &iter_track);
	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record, MS_COL_NAME, &record_name, -1);

	gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_artist, &iter_record);
	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_artist, MS_COL_NAME, &artist_name, -1);

	if (parent != NULL ||
	    (dest != NULL && gtk_tree_model_iter_parent(GTK_TREE_MODEL(pl->store), &dest_parent, dest))) {
		GtkTreeIter * piter = (parent != NULL) ? parent : &dest_parent;
		playlist_data_t * pdata;

		gtk_tree_model_get(GTK_TREE_MODEL(pl->store), piter, PL_COL_DATA, &pdata, -1);
		if (pdata->artist && pdata->album && artist_name && record_name &&
		    !strcmp(pdata->artist, artist_name) && !strcmp(pdata->album, record_name)) {
			arr_strlcpy(list_str, track_name);
		} else {
			make_title_string(list_str, CHAR_ARRAY_SIZE(list_str), options.title_format,
					  artist_name, record_name, track_name);
		}
	} else {
		make_title_string(list_str, CHAR_ARRAY_SIZE(list_str), options.title_format,
				  artist_name, record_name, track_name);
	}

	if (options.rva_is_enabled) {
		if (options.rva_use_averaging && use_avg_voladj) {
			voladj = avg_voladj;
		} else {
			if (data->use_rva > 0) {
				voladj = data->rva;
			} else {
				if (data->volume <= 0.1f) {
					voladj = rva_from_volume(data->volume);
				} else { /* unmeasured, see if there is RVA data in the file */
					if ((fdec == NULL) || !metadata_get_rva(fdec->meta, &voladj)) {
						voladj = options.rva_no_rva_voladj;
					}
				}
			}
		}
	}

	time2time(data->duration, duration_str, CHAR_ARRAY_SIZE(duration_str));
	voladj2str(voladj, voladj_str, CHAR_ARRAY_SIZE(voladj_str));

	if ((pldata = playlist_data_new()) == NULL) {
		return 0;
	}

	pldata->artist = strdup(artist_name);
	pldata->album = strdup(record_name);
	pldata->title = strdup(track_name);
	pldata->file = strdup(data->file);

	pldata->voladj = voladj;
	pldata->duration = data->duration;
	pldata->size = data->size;
	pldata->flags = PL_FLAG_COVER;

	/* either parent or dest should be set, but not both */
	gtk_tree_store_insert_before(pl->store, &list_iter, parent, dest);

	gtk_tree_store_set(pl->store, &list_iter,
			   PL_COL_NAME, list_str,
			   PL_COL_VADJ, voladj_str,
			   PL_COL_DURA, duration_str,
			   PL_COL_COLO, pl_color_inactive,
			   PL_COL_FONT, PANGO_WEIGHT_NORMAL,
			   PL_COL_DATA, pldata, -1);

	if (fdec != NULL) {
		file_decoder_close(fdec);
		file_decoder_delete(fdec);
	}

	g_free(track_name);
	g_free(record_name);
	g_free(artist_name);

	return data->duration;
}


void
record_addlist_iter(GtkTreeIter iter_record, playlist_t * pl,
		    GtkTreeIter * dest, int album_mode) {

        GtkTreeIter iter_track;
	GtkTreeIter list_iter;
	GtkTreeIter * plist_iter;

	int i;
	int nlevels;

	float volume;
	float voladj = 0.0f;
	float record_duration = 0.0f;

	playlist_data_t * pldata = NULL;


	if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter_record) == 0) {
		return;
	}

	if (options.rva_is_enabled && options.rva_use_averaging) { /* save track volumes */

		float * volumes = NULL;
		track_data_t * data;
		i = 0;
		nlevels = 0;

		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track,
						     &iter_record, i++)) {
			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track, MS_COL_DATA, &data, -1);

			volume = data->volume;

			if (volume > 0.1f) { /* unmeasured */
				volume = options.rva_refvol;
			}

			nlevels++;
			if ((volumes = realloc(volumes, nlevels * sizeof(float))) == NULL) {
				fprintf(stderr, "record__addlist_cb: realloc error\n");
				return;
			}
			volumes[nlevels-1] = volume;
		}

		voladj = rva_from_multiple_volumes(nlevels, volumes);

		free(volumes);
	}

	if (album_mode) {

		GtkTreeIter iter_artist;
		char * record_name;
		char * artist_name;
		char list_str[MAXLEN];

		if ((pldata = playlist_data_new()) == NULL) {
			return;
		}

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record, MS_COL_NAME, &record_name, -1);
		gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_artist, &iter_record);
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_artist, MS_COL_NAME, &artist_name, -1);

		arr_snprintf(list_str, "%s: %s", artist_name, record_name);

		pldata->artist = strdup(artist_name);
		pldata->album = strdup(record_name);

		gtk_tree_store_insert_before(pl->store, &list_iter, NULL, dest);
		gtk_tree_store_set(pl->store, &list_iter,
				   PL_COL_NAME, list_str,
				   PL_COL_VADJ, "",
				   PL_COL_DURA, "00:00",
				   PL_COL_COLO, pl_color_inactive,
				   PL_COL_FONT, PANGO_WEIGHT_NORMAL,
				   PL_COL_DATA, pldata, -1);

		g_free(record_name);
		g_free(artist_name);

		plist_iter = &list_iter;
		dest = NULL;
	} else {
		plist_iter = NULL;
	}

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track, &iter_record, i++)) {
		record_duration += track_addlist_iter(iter_track, pl, plist_iter, dest, voladj,
						      options.rva_use_averaging);
	}

	if (album_mode) {
		char duration_str[MAXLEN];
		pldata->duration = record_duration;
		time2time(record_duration, duration_str, CHAR_ARRAY_SIZE(duration_str));
		gtk_tree_store_set(pl->store, &list_iter, PL_COL_DURA, duration_str, -1);
	}
}


typedef struct {

	GtkTreeIter iter_store;
	GtkTreeIter iter_artist;
	GtkTreeIter iter_record;
	GtkTreeIter dest;
	playlist_t * pl;
	int dest_null;
	int album_mode;
	int i_artist;
	int i_record;
	int count;

} addlist_iter_t;


void
ms_progress_bar_update() {

	if (ms_progress_bar != NULL) {
		++ms_progress_bar_num;
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ms_progress_bar),
					      (double)(ms_progress_bar_num) / ms_progress_bar_den);
	}
}


void
ms_progress_bar_stop_clicked(GtkWidget * widget, gpointer data) {

	stop_adding_to_playlist = 1;
}

void
ms_progress_bar_show(void) {

	++ms_progress_bar_semaphore;

	if (ms_progress_bar != NULL) {
		return;
	}

	stop_adding_to_playlist = 0;

	playlist_stats_set_busy();

	ms_progress_bar = gtk_progress_bar_new();
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ms_progress_bar), 0.0);
        gtk_box_pack_start(GTK_BOX(ms_progress_bar_container), ms_progress_bar, TRUE, TRUE, 0);

	ms_progress_bar_stop_button = gtk_button_new_with_label(_("Stop adding songs"));
        gtk_box_pack_start(GTK_BOX(ms_progress_bar_container), ms_progress_bar_stop_button, FALSE, FALSE, 0);
        g_signal_connect(G_OBJECT(ms_progress_bar_stop_button), "clicked",
			 G_CALLBACK(ms_progress_bar_stop_clicked), NULL);

	gtk_widget_show_all(ms_progress_bar_container);
}


void
store_file_progress_bar_hide(void) {

	if (ms_progress_bar != NULL) {
		gtk_widget_destroy(ms_progress_bar);
		ms_progress_bar = NULL;
	}

	if (ms_progress_bar_stop_button != NULL) {
		gtk_widget_destroy(ms_progress_bar_stop_button);
		ms_progress_bar_stop_button = NULL;
	}
}

void
finalize_addlist_iter(addlist_iter_t * add_list) {

	ms_progress_bar_semaphore--;

	add_list->pl->ms_semaphore--;

	if (browser_window != NULL && ms_progress_bar_semaphore == 0) {
		store_file_progress_bar_hide();
		if (add_list->pl == playlist_get_current()) {
			playlist_content_changed(add_list->pl);
		}

		gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(add_list->pl->track_column), GTK_TREE_VIEW_COLUMN_AUTOSIZE);
		gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(add_list->pl->rva_column), GTK_TREE_VIEW_COLUMN_AUTOSIZE);
		gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(add_list->pl->length_column), GTK_TREE_VIEW_COLUMN_AUTOSIZE);

		ms_progress_bar_num = 0;
		ms_progress_bar_den = 0;
	}
}


gboolean
artist_addlist_iter_cb(gpointer data) {

	addlist_iter_t * add_iter = (addlist_iter_t *)data;

	if (stop_adding_to_playlist) {
		goto finish;
	}

	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
			  &add_iter->iter_record, &add_iter->iter_artist, add_iter->i_record++)) {

		if (!add_iter->dest_null && !gtk_tree_store_iter_is_valid(add_iter->pl->store, &add_iter->dest)) {
			add_iter->dest_null = 1;
		}

		record_addlist_iter(add_iter->iter_record, add_iter->pl,
				    add_iter->dest_null ? NULL : &add_iter->dest,
				    add_iter->album_mode);

		ms_progress_bar_update();

		return TRUE;
	}

 finish:

	finalize_addlist_iter(add_iter);
	free(add_iter);

	return FALSE;
}

gboolean
store_addlist_iter_cb(gpointer data) {

	addlist_iter_t * add_iter = (addlist_iter_t *)data;

	if (stop_adding_to_playlist) {
		goto finish;
	}

	if (add_iter->i_artist > 0 && gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					  &add_iter->iter_record, &add_iter->iter_artist, add_iter->i_record++)) {

		if (!add_iter->dest_null && !gtk_tree_store_iter_is_valid(add_iter->pl->store, &add_iter->dest)) {
			add_iter->dest_null = 1;
		}

		record_addlist_iter(add_iter->iter_record, add_iter->pl,
				    add_iter->dest_null ? NULL : &add_iter->dest,
				    add_iter->album_mode);

		return TRUE;
	} else {
		add_iter->i_record = 0;

		if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					  &add_iter->iter_artist, &add_iter->iter_store, add_iter->i_artist++)) {

			ms_progress_bar_update();

			return TRUE;
		}
	}

 finish:

	finalize_addlist_iter(add_iter);
	free(add_iter);

	return FALSE;
}


void
artist_addlist_iter(GtkTreeIter iter_artist, playlist_t * pl, GtkTreeIter * dest, int album_mode) {

	addlist_iter_t * add_iter;

	if ((add_iter = (addlist_iter_t *)malloc(sizeof(addlist_iter_t))) == NULL) {
		fprintf(stderr, "malloc error in artist_adlist_iter\n");
		return;
	}

	if ((add_iter->count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter_artist)) == 0) {
		free(add_iter);
		return;
	}

	add_iter->pl = pl;

	ms_progress_bar_den += add_iter->count;

	if (dest == NULL) {
		add_iter->dest_null = 1;
	} else {
		add_iter->dest_null = 0;
		add_iter->dest = *dest;
	}

	add_iter->iter_artist = iter_artist;
	add_iter->album_mode = album_mode;
	add_iter->i_record = 0;

	playlist_stats_set_busy();

	ms_progress_bar_show();
	pl->ms_semaphore++;
	aqualung_idle_add(artist_addlist_iter_cb, (gpointer)add_iter);
}


void
store_addlist_iter(GtkTreeIter iter_store, playlist_t * pl, GtkTreeIter * dest, int album_mode) {

	addlist_iter_t * add_iter;

	if ((add_iter = (addlist_iter_t *)malloc(sizeof(addlist_iter_t))) == NULL) {
		fprintf(stderr, "malloc error in store_adlist_iter\n");
		return;
	}

	if ((add_iter->count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter_store)) == 0) {
		free(add_iter);
		return;
	}

	add_iter->pl = pl;

	ms_progress_bar_den += add_iter->count;

	if (dest == NULL) {
		add_iter->dest_null = 1;
	} else {
		add_iter->dest_null = 0;
		add_iter->dest = *dest;
	}

	add_iter->iter_store = iter_store;
	add_iter->album_mode = album_mode;
	add_iter->i_artist = 0;

	playlist_stats_set_busy();

    /* set sizing to fixed for speeding up adding new stuff */
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(pl->track_column), GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(pl->rva_column), GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(pl->length_column), GTK_TREE_VIEW_COLUMN_FIXED);

	ms_progress_bar_show();
	pl->ms_semaphore++;
	aqualung_idle_add(store_addlist_iter_cb, (gpointer)add_iter);
}


/****************************************/

void
search_cb(gpointer data) {

	music_store_search();
}


/* mode: 0 normal, 1 album mode */

void
store__addlist_with_mode(int mode, gpointer data) {

        GtkTreeIter iter_store;
	playlist_t * pl = playlist_get_current();

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_store)) {
		store_addlist_iter(iter_store, pl, (GtkTreeIter *)data, mode);
	}
}

void
store__addlist_defmode(gpointer data) {

	store__addlist_with_mode(options.playlist_is_tree, data);
}


void
store__addlist_albummode_cb(gpointer data) {

	store__addlist_with_mode(1, data);
}


void
store__addlist_cb(gpointer data) {

	store__addlist_with_mode(0, data);
}


void
store__add_cb(gpointer user_data) {

	GtkTreeIter iter;
	char name[MAXLEN];
	store_data_t * data;
	xmlDocPtr doc;
	xmlNodePtr root;

	name[0] = '\0';

	if (add_store_dialog(name, CHAR_ARRAY_SIZE(name), &data)) {

		if (access(data->file, F_OK) == 0) {
			message_dialog(_("Create empty store"),
				       browser_window,
				       GTK_MESSAGE_INFO,
				       GTK_BUTTONS_OK,
				       NULL,
				       _("The store '%s' already exists.\nAdd it on the Settings/Music Store tab."),
				       data->file);
		} else {

			char * utf8 = g_filename_to_utf8(data->file, -1, NULL, NULL, NULL);

			gtk_tree_store_append(music_store, &iter, NULL);

			gtk_tree_store_set(music_store, &iter,
					   MS_COL_NAME, name,
					   MS_COL_FONT, PANGO_WEIGHT_BOLD,
					   MS_COL_DATA, data, -1);

                        if (options.enable_ms_tree_icons) {
                                gtk_tree_store_set(music_store, &iter, MS_COL_ICON, icon_store, -1);
                        }

			doc = xmlNewDoc((const xmlChar *) "1.0");
			root = xmlNewNode(NULL, (const xmlChar *) "music_store");
			xmlDocSetRootElement(doc, root);

			xmlNewTextChild(root, NULL, (const xmlChar *) "name", (xmlChar *) name);
			if (data->comment != NULL && data->comment[0] != '\0') {
				xmlNewTextChild(root, NULL, (const xmlChar *) "comment", (xmlChar *) data->comment);
			}

			xmlSaveFormatFile(data->file, doc, 1);
			xmlFreeDoc(doc);

			gtk_list_store_append(ms_pathlist_store, &iter);
			gtk_list_store_set(ms_pathlist_store, &iter, 0, data->file, 1, utf8, 2, _("rw"), -1);
			g_free(utf8);
		}
	}
}


void
store__build_cb(gpointer data) {

	GtkTreeIter store_iter;
	GtkTreeModel * model;

	if (gtk_tree_selection_get_selected(music_select, &model, &store_iter)) {

		store_data_t * data;

                gtk_tree_model_get(model, &store_iter, MS_COL_DATA, &data, -1);

		if (data->readonly) {
			return;
		}

		build_store(&store_iter, data->file);
	}
}


void
store__edit_cb(gpointer user_data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	store_data_t * data;

	char * pname;
	char name[MAXLEN+1];
	int offset;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

                gtk_tree_model_get(model, &iter,
				   MS_COL_NAME, &pname,
				   MS_COL_DATA, &data, -1);

		if (data->readonly) {
			g_free(pname);
			return;
		}

		arr_strlcpy(name, pname);
		g_free(pname);

		offset = ((data->dirty) ? 1 : 0);
		if (edit_store_dialog(name + offset, CHAR_ARRAY_SIZE(name) - offset, data)) {
			gtk_tree_store_set(music_store, &iter,
					   MS_COL_NAME, name, -1);

			music_store_mark_changed(&iter);
		}
        }
}


void
store_volume_calc(int unmeasured) {

        GtkTreeIter iter_store;
        GtkTreeIter iter_artist;
        GtkTreeIter iter_record;
        GtkTreeIter iter_track;
        GtkTreeModel * model;

	int h, i, j;
	volume_t * vol = NULL;

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_store)) {

		if (is_store_iter_readonly(&iter_store)) {
			return;
		}

		if ((vol = volume_new(music_store, VOLUME_SEPARATE)) == NULL) {
			return;
		}

		h = 0;
		while (gtk_tree_model_iter_nth_child(model, &iter_artist, &iter_store, h++)) {

			i = 0;
			while (gtk_tree_model_iter_nth_child(model, &iter_record, &iter_artist, i++)) {

				j = 0;
				while (gtk_tree_model_iter_nth_child(model, &iter_track, &iter_record, j++)) {

					track_data_t * data;

					gtk_tree_model_get(model, &iter_track, MS_COL_DATA, &data, -1);

					if (!unmeasured || data->volume > 0.1f) {
						volume_push(vol, data->file, iter_track);
					}
				}
			}
		}

		volume_start(vol);
	}
}


void
store__volume_unmeasured_cb(gpointer data) {

	store_volume_calc(1);
}


void
store__volume_all_cb(gpointer data) {

	store_volume_calc(0);
}


void
store__remove_cb(gpointer user_data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	store_data_t * data;

	char * pname;
	char name[MAXLEN];
	char text[MAXLEN];
	int i = 0;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

                gtk_tree_model_get(model, &iter,
				   MS_COL_NAME, &pname,
				   MS_COL_DATA, &data, -1);

		arr_strlcpy(name, pname);
                g_free(pname);

		arr_snprintf(text, _("Really remove store \"%s\" from the Music Store?"),
			     (data->dirty) ? (name + 1) : (name));
		if (confirm_dialog(_("Remove Store"), text)) {

			char * file = strdup(data->file);

			if (data->dirty) {
				if (confirm_dialog(_("Remove Store"),
						   _("Do you want to save the store before removing?"))) {
					store_file_save(&iter);
				} else {
					music_store_mark_saved(&iter);
				}
			}

			if (store_file_remove_store(&iter)) {
				gtk_tree_selection_select_iter(music_select, &iter);
			} else {
				int last;

				if ((last = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), NULL))) {
					gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
								      &iter, NULL, last-1);
					gtk_tree_selection_select_iter(music_select, &iter);
				}
			}

			i = 0;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ms_pathlist_store),
							     &iter, NULL, i++)) {
				gtk_tree_model_get(GTK_TREE_MODEL(ms_pathlist_store), &iter, 0, &pname, -1);
				if (!strcmp(file, pname)) {
					gtk_list_store_remove(ms_pathlist_store, &iter);
				}
				g_free(pname);
			}
		}
	}
}


void
store__save_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {
		store_file_save(&iter);
	}
}


/****************************************/


/* mode: 0 normal, 1 album mode */

void
artist__addlist_with_mode(int mode, gpointer data) {

        GtkTreeIter iter_artist;
	playlist_t * pl = playlist_get_current();

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_artist)) {
		artist_addlist_iter(iter_artist, pl, (GtkTreeIter *)data, mode);
	}
}

void
artist__addlist_defmode(gpointer data) {

	artist__addlist_with_mode(options.playlist_is_tree, data);
}

void
artist__addlist_albummode_cb(gpointer data) {

	artist__addlist_with_mode(1, data);
}


void
artist__addlist_cb(gpointer data) {

	artist__addlist_with_mode(0, data);
}


void
artist__add_cb(gpointer user_data) {

	GtkTreeIter iter;
	GtkTreeIter parent_iter;
	GtkTreePath * parent_path;
	GtkTreeModel * model;

	artist_data_t * data;

	char name[MAXLEN];
	char sort[MAXLEN];

	name[0] = '\0';
	sort[0] = '\0';

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		if (is_store_iter_readonly(&parent_iter)) return;

		/* get iter to music store (parent) */
		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		if (gtk_tree_path_get_depth(parent_path) == 2) {
			gtk_tree_path_up(parent_path);
		}
		gtk_tree_model_get_iter(model, &parent_iter, parent_path);
		gtk_tree_path_free(parent_path);

		if (add_artist_dialog(name, CHAR_ARRAY_SIZE(name), sort, CHAR_ARRAY_SIZE(sort), &data)) {
			gtk_tree_store_append(music_store, &iter, &parent_iter);
			gtk_tree_store_set(music_store, &iter,
					   MS_COL_NAME, name,
					   MS_COL_SORT, sort,
					   MS_COL_DATA, data, -1);

                        if (options.enable_ms_tree_icons) {
			        gtk_tree_store_set(music_store, &iter, MS_COL_ICON, icon_artist, -1);
                        }
			music_store_mark_changed(&iter);
		}
	}
}


void
artist__edit_cb(gpointer user_data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	char * pname;
	char * psort;
	char name[MAXLEN];
	char sort[MAXLEN];

	artist_data_t * data;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) return;

                gtk_tree_model_get(model, &iter,
				   MS_COL_NAME, &pname,
				   MS_COL_SORT, &psort,
				   MS_COL_DATA, &data, -1);

		arr_strlcpy(name, pname);
		arr_strlcpy(sort, psort);
                g_free(pname);
                g_free(psort);

		if (edit_artist_dialog(name, CHAR_ARRAY_SIZE(name), sort, CHAR_ARRAY_SIZE(sort), data)) {

			gtk_tree_store_set(music_store, &iter,
					   MS_COL_NAME, name,
					   MS_COL_SORT, sort, -1);

			music_store_mark_changed(&iter);
		}
        }
}


void
artist_volume_calc(int unmeasured) {

        GtkTreeIter iter_artist;
        GtkTreeIter iter_record;
        GtkTreeIter iter_track;
        GtkTreeModel * model;

	int i, j;
	volume_t * vol = NULL;

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_artist)) {

		if (is_store_iter_readonly(&iter_artist)) {
			return;
		}

		if ((vol = volume_new(music_store, VOLUME_SEPARATE)) == NULL) {
			return;
		}

		i = 0;
		while (gtk_tree_model_iter_nth_child(model, &iter_record, &iter_artist, i++)) {

			j = 0;
			while (gtk_tree_model_iter_nth_child(model, &iter_track, &iter_record, j++)) {

				track_data_t * data;

				gtk_tree_model_get(model, &iter_track, MS_COL_DATA, &data, -1);

				if (!unmeasured || data->volume > 0.1f) {
					volume_push(vol, data->file, iter_track);
				}
			}
		}

		volume_start(vol);
	}
}


void
artist__volume_unmeasured_cb(gpointer data) {

	artist_volume_calc(1);
}


void
artist__volume_all_cb(gpointer data) {

	artist_volume_calc(0);
}


void
artist__remove_cb(gpointer data) {

	generic_remove_cb(_("Remove Artist"), store_file_remove_artist);
}

/************************************/


/* mode: 0 normal, 1 album mode */

void
record__addlist_with_mode(int mode, gpointer data) {

        GtkTreeIter iter_record;
	playlist_t * pl = playlist_get_current();

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_record)) {
		record_addlist_iter(iter_record, pl, (GtkTreeIter *)data, mode);
		if (pl == playlist_get_current()) {
			playlist_content_changed(pl);
		}
	}
}

void
record__addlist_defmode(gpointer data) {

	record__addlist_with_mode(options.playlist_is_tree, data);
}

void
record__addlist_albummode_cb(gpointer data) {

	record__addlist_with_mode(1, data);
}


void
record__addlist_cb(gpointer data) {

	record__addlist_with_mode(0, data);
}


void
record__add_cb(gpointer user_data) {

	GtkTreeIter iter;
	GtkTreeIter parent_iter;
	GtkTreeIter child_iter;
	GtkTreePath * parent_path;
	GtkTreeModel * model;

	record_data_t * data;

	char name[MAXLEN];
	char sort[MAXLEN];
	char ** strings = NULL;
	char * str;
	char str_n[16];

	int i;

	name[0] = '\0';
	sort[0] = '\0';

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		if (is_store_iter_readonly(&parent_iter)) return;

		/* get iter to artist (parent) */
		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		if (gtk_tree_path_get_depth(parent_path) == 3)
			gtk_tree_path_up(parent_path);
		gtk_tree_model_get_iter(model, &parent_iter, parent_path);

		if (add_record_dialog(name, CHAR_ARRAY_SIZE(name), sort, CHAR_ARRAY_SIZE(sort), &strings, &data)) {

			gtk_tree_store_append(music_store, &iter, &parent_iter);
			gtk_tree_store_set(music_store, &iter,
					   MS_COL_NAME, name,
					   MS_COL_SORT, sort,
					   MS_COL_DATA, data, -1);

                        if (options.enable_ms_tree_icons) {
                                gtk_tree_store_set(music_store, &iter, MS_COL_ICON, icon_record, -1);
                        }

			music_store_mark_changed(&iter);

			if (strings) {
				for (i = 0; strings[i] != NULL; i++) {
					arr_snprintf(str_n, "%02d", i+1);
					str = strings[i];
					while (strstr(str, "/")) {
						str = strstr(str, "/") + 1;
					}
					if (str) {
						track_data_t * track;
						char * utf8 = g_filename_to_utf8(str, -1, NULL, NULL, NULL);
						float duration = get_file_duration(strings[i]);

						if ((track = (track_data_t *)calloc(1, sizeof(track_data_t))) == NULL) {
							fprintf(stderr, "record__add_cb: calloc error\n");
							return;
						}

						track->file = strdup(strings[i]);
						track->duration = duration > 0.0f ? duration : 0.0f;
						track->volume = 1.0f;
						track->use_rva = 0;

						gtk_tree_store_append(music_store, &child_iter, &iter);
						gtk_tree_store_set(music_store, &child_iter,
								   MS_COL_NAME, utf8,
								   MS_COL_SORT, str_n,
								   MS_COL_DATA, track, -1);

                                                if (options.enable_ms_tree_icons) {
                                                        gtk_tree_store_set(music_store, &child_iter,
                                                                           MS_COL_ICON, icon_track, -1);
						}
						g_free(utf8);
					}
					free(strings[i]);
				}
				free(strings);
			}
		}
	}
}


void
record__edit_cb(gpointer user_data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	record_data_t * data;

	char * pname;
	char * psort;

	char name[MAXLEN];
	char sort[MAXLEN];

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) {
			return;
		}

                gtk_tree_model_get(model, &iter,
				   MS_COL_NAME, &pname,
				   MS_COL_SORT, &psort,
				   MS_COL_DATA, &data, -1);

		arr_strlcpy(name, pname);
		arr_strlcpy(sort, psort);
                g_free(pname);
                g_free(psort);

		if (edit_record_dialog(name, CHAR_ARRAY_SIZE(name), sort, CHAR_ARRAY_SIZE(sort), data)) {

			gtk_tree_store_set(music_store, &iter,
					   MS_COL_NAME, name,
					   MS_COL_SORT, sort, -1);

			music_store_mark_changed(&iter);
		}
        }
}


void
record_volume_calc(int unmeasured) {

        GtkTreeIter iter_record;
        GtkTreeIter iter_track;
        GtkTreeModel * model;

	int i;
	volume_t * vol = NULL;

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_record)) {

		if (is_store_iter_readonly(&iter_record)) {
			return;
		}

		if ((vol = volume_new(music_store, VOLUME_SEPARATE)) == NULL) {
			return;
		}

		i = 0;
		while (gtk_tree_model_iter_nth_child(model, &iter_track, &iter_record, i++)) {

			track_data_t * data;

			gtk_tree_model_get(model, &iter_track, MS_COL_DATA, &data, -1);

			if (!unmeasured || data->volume > 0.1f) {
				volume_push(vol, data->file, iter_track);
			}
		}
	}

	volume_start(vol);
}


void
record__volume_unmeasured_cb(gpointer data) {

	record_volume_calc(1);
}


void
record__volume_all_cb(gpointer data) {

	record_volume_calc(0);
}


void
record__remove_cb(gpointer data) {

	generic_remove_cb(_("Remove Record"), store_file_remove_record);
}


#ifdef HAVE_CDDB

static int
cddb_init_query_data(GtkTreeIter * iter_record, int * ntracks, int ** frames, int * length) {

	int i;
	float len = 0.0f;
	float offset = 150.0f; /* leading 2 secs in frames */

	GtkTreeIter iter_track;
	track_data_t * data;

	*ntracks = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), iter_record);

	if ((*frames = (int *)calloc(*ntracks, sizeof(int))) == NULL) {
		fprintf(stderr, "store_file.c: cddb_init_query_data: calloc error\n");
		return 1;
	}

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &iter_track, iter_record, i)) {

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track,
				   MS_COL_DATA, &data, -1);

		*((*frames) + i) = (int)offset;

		len += data->duration;
		offset += 75.0f * data->duration;
		++i;
	}

	*length = (int)len;

	return 0;
}

void
record__cddb_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter) &&
	    !is_store_iter_readonly(&iter)) {

		int ntracks;
		int * frames;
		int length;

		if (cddb_init_query_data(&iter, &ntracks, &frames, &length) == 0) {
			cddb_start_query(&iter, ntracks, frames, length);
		}
	}
}

#endif /* HAVE_CDDB */


/************************************/


void
track__addlist_cb(gpointer data) {

        GtkTreeIter iter_track;
	playlist_t * pl = playlist_get_current();

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_track)) {
		track_addlist_iter(iter_track, pl, NULL, (GtkTreeIter *)data, 0.0f, 0);
		if (pl == playlist_get_current()) {
			playlist_content_changed(pl);
		}
	}
}


void
track__add_cb(gpointer user_data) {

	GtkTreeIter iter;
	GtkTreeIter parent_iter;
	GtkTreePath * parent_path;
	GtkTreeModel * model;

	track_data_t * data;

	char name[MAXLEN];
	char sort[MAXLEN];

	name[0] = '\0';
	sort[0] = '\0';

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		if (is_store_iter_readonly(&parent_iter)) {
			return;
		}

		/* get iter to artist (parent) */
		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		if (gtk_tree_path_get_depth(parent_path) == 4) {
			gtk_tree_path_up(parent_path);
		}
		gtk_tree_model_get_iter(model, &parent_iter, parent_path);

		if (add_track_dialog(name, CHAR_ARRAY_SIZE(name), sort, CHAR_ARRAY_SIZE(sort), &data)) {

			gtk_tree_store_append(music_store, &iter, &parent_iter);
			gtk_tree_store_set(music_store, &iter,
					   MS_COL_NAME, name,
					   MS_COL_SORT, sort,
					   MS_COL_DATA, data, -1);

	                if (options.enable_ms_tree_icons) {
                                gtk_tree_store_set(music_store, &iter, MS_COL_ICON, icon_track, -1);
                        }

			music_store_mark_changed(&iter);
		}
	}
}


void
track__edit_cb(gpointer user_data) {

        GtkTreeIter iter;
        GtkTreeModel * model;

	track_data_t * data;

        char * pname;
        char * psort;

        char name[MAXLEN];
        char sort[MAXLEN];

        if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) {
			return;
		}

                gtk_tree_model_get(model, &iter,
				   MS_COL_NAME, &pname,
				   MS_COL_SORT, &psort,
				   MS_COL_DATA, &data, -1);

                arr_strlcpy(name, pname);
                arr_strlcpy(sort, psort);
                g_free(pname);
                g_free(psort);

		if (edit_track_dialog(name, CHAR_ARRAY_SIZE(name),
				      sort, CHAR_ARRAY_SIZE(sort), data)) {

                        gtk_tree_store_set(music_store, &iter,
					   MS_COL_NAME, name,
					   MS_COL_SORT, sort, -1);

			music_store_mark_changed(&iter);
                }
        }
}


void
track__fileinfo_cb(gpointer user_data) {

        GtkTreeModel * model;
        GtkTreeIter iter_track;

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_track)) {
		show_file_info(model, iter_track, store_model_func, 1,
			       is_store_iter_readonly(&iter_track) ? FALSE : TRUE, TRUE);
        }
}

void
track_volume_calc(int unmeasured) {

        GtkTreeIter iter_track;
        GtkTreeModel * model;

	track_data_t * data;
	volume_t * vol = NULL;

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_track)) {

		if (is_store_iter_readonly(&iter_track)) {
			return;
		}

		if ((vol = volume_new(music_store, VOLUME_SEPARATE)) == NULL) {
			return;
		}

                gtk_tree_model_get(model, &iter_track, MS_COL_DATA, &data, -1);

		if (!unmeasured || data->volume > 0.1f) {
			volume_push(vol, data->file, iter_track);
			volume_start(vol);
		}
        }
}


void
track__volume_unmeasured_cb(gpointer data) {

	track_volume_calc(1);
}


void
track__volume_all_cb(gpointer data) {

	track_volume_calc(0);
}


void
track__remove_cb(gpointer data) {

	generic_remove_cb(_("Remove Track"), store_file_remove_track);
}


/************************************/


#ifdef HAVE_TRANSCODING
void
track_export(GtkTreeIter * iter_track, export_t * export, char * _artist, char * _album, int year) {

	GtkTreeIter iter_artist;
	GtkTreeIter iter_record;

	track_data_t * track_data;

	char artist[MAXLEN];
	char album[MAXLEN];
	char * title;
	char * str_no;
	int no = 0;


	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_track,
			   MS_COL_NAME, &title,
			   MS_COL_SORT, &str_no,
			   MS_COL_DATA, &track_data, -1);

	sscanf(str_no, "%d", &no);

	if (_album == NULL || _artist == NULL) {
		gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_record, iter_track);
	}

	if (_album == NULL) {
		record_data_t * record_data;
		char * tmp;

		gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_record, iter_track);
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record,
				   MS_COL_NAME, &tmp,
				   MS_COL_DATA, &record_data, -1);

		arr_strlcpy(album, tmp);
		g_free(tmp);

		year = record_data->year;
	} else {
		arr_strlcpy(album, _album);
	}

	if (_artist == NULL) {
		char * tmp;

		gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_artist, &iter_record);
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_artist,
				   MS_COL_NAME, &tmp, -1);

		arr_strlcpy(artist, tmp);
		g_free(tmp);
	} else {
		arr_strlcpy(artist, _artist);
	}

	export_append_item(export, track_data->file, artist, album, title, year, no);

	g_free(title);
	g_free(str_no);
}

void
record_export(GtkTreeIter * iter_record, export_t * export, char * _artist) {

	GtkTreeIter iter_artist;
	GtkTreeIter iter_track;

	char artist[MAXLEN];
	char * record;
	record_data_t * record_data;
	int i;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_record,
			   MS_COL_NAME, &record,
			   MS_COL_DATA, &record_data, -1);

	if (_artist == NULL) {
		char * tmp;

		gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_artist, iter_record);
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_artist,
				   MS_COL_NAME, &tmp, -1);

		arr_strlcpy(artist, tmp);
		g_free(tmp);
	} else {
		arr_strlcpy(artist, _artist);
	}

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track, iter_record, i++)) {
		track_export(&iter_track, export, artist, record, record_data->year);
	}

	g_free(record);
}

void
artist_export(GtkTreeIter * iter_artist, export_t * export) {

	GtkTreeIter iter_record;

	char * artist;
	int i;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_artist,
			   MS_COL_NAME, &artist, -1);

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_record, iter_artist, i++)) {
		record_export(&iter_record, export, artist);
	}

	g_free(artist);
}

void
track__export_cb(gpointer user_data) {

	GtkTreeIter iter_track;
	export_t * export;

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_track)) {

		if ((export = export_new()) == NULL) {
			return;
		}

		track_export(&iter_track, export, NULL, NULL, 0);

		export_start(export);
	}
}

void
record__export_cb(gpointer user_data) {

	GtkTreeIter iter_record;
	export_t * export;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter_record)) {

		if ((export = export_new()) == NULL) {
			return;
		}

		record_export(&iter_record, export, NULL);

		export_start(export);
	}
}

void
artist__export_cb(gpointer user_data) {

	GtkTreeIter iter_artist;
	export_t * export;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter_artist)) {

		if ((export = export_new()) == NULL) {
			return;
		}

		artist_export(&iter_artist, export);

		export_start(export);
	}
}

void
store__export_cb(gpointer user_data) {

	GtkTreeIter iter_store;
	GtkTreeIter iter_artist;
	export_t * export;
	int i;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter_store)) {

		if ((export = export_new()) == NULL) {
			return;
		}

		i = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     &iter_artist, &iter_store, i++)) {
			artist_export(&iter_artist, export);
		}

		export_start(export);
	}
}
#endif /* HAVE_TRANSCODING */


/************************************/


AQUALUNG_THREAD_DECLARE(tag_thread_id)
volatile int batch_tag_cancelled;


GtkTreeIter store_tag_iter;
GtkTreeIter artist_tag_iter;
GtkTreeIter record_tag_iter;
GtkTreeIter track_tag_iter;

char artist_tag[MAXLEN];
char album_tag[MAXLEN];
char year_tag[MAXLEN];

batch_tag_t * batch_tag_curr = NULL;

GtkWidget * tag_prog_window;
GtkWidget * tag_prog_file_entry;
GtkWidget * tag_prog_cancel_button;
GtkListStore * tag_error_list;

int
create_tag_dialog() {

	GtkWidget * dialog;
	GtkWidget * vbox;
	GtkWidget * check_artist;
	GtkWidget * check_record;
	GtkWidget * check_track;
	GtkWidget * check_comment;
	GtkWidget * check_trackno;
	GtkWidget * check_year;


        dialog = gtk_dialog_new_with_buttons(_("Update file metadata"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
			   vbox, FALSE, FALSE, 0);

	check_artist = gtk_check_button_new_with_label(_("Artist name"));
	check_record = gtk_check_button_new_with_label(_("Record name"));
	check_track = gtk_check_button_new_with_label(_("Track name"));
	check_comment = gtk_check_button_new_with_label(_("Track comment"));
	check_trackno = gtk_check_button_new_with_label(_("Track number"));
	check_year = gtk_check_button_new_with_label(_("Year"));

	gtk_widget_set_name(check_artist, "check_on_window");
	gtk_widget_set_name(check_record, "check_on_window");
	gtk_widget_set_name(check_track, "check_on_window");
	gtk_widget_set_name(check_comment, "check_on_window");
	gtk_widget_set_name(check_trackno, "check_on_window");
	gtk_widget_set_name(check_year, "check_on_window");

	gtk_box_pack_start(GTK_BOX(vbox), check_artist, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), check_record, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), check_track, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), check_comment, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), check_trackno, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), check_year, FALSE, FALSE, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_artist),
				     options.batch_tag_flags & BATCH_TAG_ARTIST);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_record),
				     options.batch_tag_flags & BATCH_TAG_ALBUM);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_track),
				     options.batch_tag_flags & BATCH_TAG_TITLE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_comment),
				     options.batch_tag_flags & BATCH_TAG_COMMENT);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_trackno),
				     options.batch_tag_flags & BATCH_TAG_TRACKNO);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_year),
				     options.batch_tag_flags & BATCH_TAG_YEAR);

	gtk_widget_show_all(dialog);

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		set_option_bit_from_toggle(check_artist, &options.batch_tag_flags, BATCH_TAG_ARTIST);
		set_option_bit_from_toggle(check_record, &options.batch_tag_flags, BATCH_TAG_ALBUM);
		set_option_bit_from_toggle(check_track, &options.batch_tag_flags, BATCH_TAG_TITLE);
		set_option_bit_from_toggle(check_comment, &options.batch_tag_flags, BATCH_TAG_COMMENT);
		set_option_bit_from_toggle(check_year, &options.batch_tag_flags, BATCH_TAG_YEAR);
		set_option_bit_from_toggle(check_trackno, &options.batch_tag_flags, BATCH_TAG_TRACKNO);

		if (options.batch_tag_flags) {
			gtk_widget_destroy(dialog);
			return 1;
		}
	}

        gtk_widget_destroy(dialog);
	return 0;
}

void
tag_prog_window_close(GtkWidget * widget, gpointer data) {

	batch_tag_cancelled = 1;
	batch_tag_root = batch_tag_curr = NULL;

	if (tag_prog_window) {
		gtk_widget_destroy(tag_prog_window);
		tag_prog_window = NULL;
	}
}

void
cancel_batch_tag(GtkWidget * widget, gpointer data) {

	tag_prog_window_close(NULL, NULL);
}

void
create_tag_prog_window(void) {


	GtkWidget * table;
	GtkWidget * label;
	GtkWidget * vbox;
	GtkWidget * hbox_result;
	GtkWidget * label_result;
	GtkWidget * hbox;
	GtkWidget * hbuttonbox;
	GtkWidget * hseparator;
	GtkWidget * viewport;
	GtkWidget * scrollwin;
	GtkWidget * tag_error_view;
	GtkTreeViewColumn * column;
	GtkCellRenderer * renderer;

	tag_prog_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(tag_prog_window), _("Update file metadata"));
        gtk_window_set_position(GTK_WINDOW(tag_prog_window), GTK_WIN_POS_CENTER);
        gtk_window_resize(GTK_WINDOW(tag_prog_window), 600, 300);
        g_signal_connect(G_OBJECT(tag_prog_window), "delete_event",
                         G_CALLBACK(tag_prog_window_close), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(tag_prog_window), 20);

        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(tag_prog_window), vbox);

	table = gtk_table_new(3, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);

        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("File:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 5, 5);

        tag_prog_file_entry = gtk_entry_new();
        gtk_editable_set_editable(GTK_EDITABLE(tag_prog_file_entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), tag_prog_file_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);


	hbox_result = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label_result = gtk_label_new(_("Failed to set metadata for the following files:"));
	gtk_box_pack_start(GTK_BOX(hbox_result), label_result, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox_result, 0, 2, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 5);

	tag_error_list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
        tag_error_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tag_error_list));
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tag_error_view), FALSE);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Filename"),
							  renderer,
							  "text", 0,
							  NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tag_error_view), column);
	column = gtk_tree_view_column_new_with_attributes(_("Reason"),
							  renderer,
							  "text", 1,
							  NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tag_error_view), column);

	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        viewport = gtk_viewport_new(NULL, NULL);
	gtk_table_attach(GTK_TABLE(table), viewport, 0, 2, 2, 3,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);
	gtk_container_add(GTK_CONTAINER(viewport), scrollwin);
	gtk_container_add(GTK_CONTAINER(scrollwin), tag_error_view);

        hseparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, TRUE, 5);

	hbuttonbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        tag_prog_cancel_button = gui_stock_label_button (_("Abort"), GTK_STOCK_CANCEL);
        g_signal_connect(tag_prog_cancel_button, "clicked", G_CALLBACK(cancel_batch_tag), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), tag_prog_cancel_button);

        gtk_widget_grab_focus(tag_prog_cancel_button);

        gtk_widget_show_all(tag_prog_window);
}

gboolean
set_tag_prog_file_entry(gpointer data) {

	if (tag_prog_window) {

		char * utf8 = g_filename_display_name((char *)data);
		gtk_entry_set_text(GTK_ENTRY(tag_prog_file_entry), utf8);
		gtk_widget_grab_focus(tag_prog_cancel_button);
		g_free(utf8);
	}

	return FALSE;
}

gboolean
batch_tag_finish(gpointer data) {

	if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(tag_error_list), NULL) > 0) {
		gtk_entry_set_text(GTK_ENTRY(tag_prog_file_entry), "");
		gtk_button_set_label(GTK_BUTTON(tag_prog_cancel_button), GTK_STOCK_CLOSE);
		gtk_button_set_use_stock(GTK_BUTTON(tag_prog_cancel_button), TRUE);
	} else {
		tag_prog_window_close(NULL, NULL);
	}

	return FALSE;
}


typedef struct {
	char * filename;
	int ret;
} batch_tag_error_t;


gboolean
batch_tag_append_error(gpointer data) {

	batch_tag_error_t * err = (batch_tag_error_t *)data;
	GtkTreeIter iter;

	gtk_list_store_append(tag_error_list, &iter);
	gtk_list_store_set(tag_error_list, &iter,
			   0, err->filename,
			   1, metadata_strerror(err->ret),
			   -1);

	free(err->filename);
	free(err);

	return FALSE;
}


void *
update_tag_thread(void * args) {

	batch_tag_t * ptag = (batch_tag_t *)args;
	batch_tag_t * _ptag = ptag;

	AQUALUNG_THREAD_DETACH()

	while (ptag) {
		int ret;

		if (batch_tag_cancelled) {

			while (ptag) {
				ptag = ptag->next;
				free(_ptag);
				_ptag = ptag;
			}

			aqualung_idle_add(batch_tag_finish, NULL);

			return NULL;
		}

		aqualung_idle_add(set_tag_prog_file_entry, (gpointer)ptag->filename);

		ret = meta_update_basic(ptag->filename,
					(options.batch_tag_flags & BATCH_TAG_TITLE) ? ptag->title : NULL,
					(options.batch_tag_flags & BATCH_TAG_ARTIST) ? ptag->artist : NULL,
					(options.batch_tag_flags & BATCH_TAG_ALBUM) ? ptag->album : NULL,
					(options.batch_tag_flags & BATCH_TAG_COMMENT) ? ptag->comment : NULL,
					NULL /* genre */,
					(options.batch_tag_flags & BATCH_TAG_YEAR) ? ptag->year : NULL,
					(options.batch_tag_flags & BATCH_TAG_TRACKNO) ? ptag->trackno : -1);

		if (ret < 0) {
			batch_tag_error_t * err =
				(batch_tag_error_t *)calloc(sizeof(batch_tag_error_t), 1);
			if (err == NULL) {
				fprintf(stderr, "update_tag_thread: calloc error\n");
			} else {
				err->filename = strdup(ptag->filename);
				err->ret = ret;
				aqualung_idle_add(batch_tag_append_error, (gpointer)err);
			}
		}

		ptag = ptag->next;
		free(_ptag);
		_ptag = ptag;
	}

	aqualung_idle_add(batch_tag_finish, NULL);

	return NULL;
}

gboolean
track_batch_tag(gpointer data) {

	char * title;
	char * track;

	batch_tag_t * ptag;
	track_data_t * track_data;


	if ((ptag = (batch_tag_t *)calloc(1, sizeof(batch_tag_t))) == NULL) {
		fprintf(stderr, "music_store.c: track_batch_tag(): calloc error");
		return FALSE;
	}

	if (batch_tag_root == NULL) {
		batch_tag_root = batch_tag_curr = ptag;
	} else {
		batch_tag_curr->next = ptag;
		batch_tag_curr = ptag;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &track_tag_iter,
			   MS_COL_NAME, &title,
			   MS_COL_SORT, &track,
			   MS_COL_DATA, &track_data, -1);

	arr_strlcpy(ptag->artist, artist_tag);
	arr_strlcpy(ptag->album, album_tag);
	arr_strlcpy(ptag->year, year_tag);
	arr_strlcpy(ptag->title, title);
	if (sscanf(track, "%d", &ptag->trackno) < 1) {
		ptag->trackno = -1;
	}

	if (track_data->file != NULL) {
		arr_strlcpy(ptag->filename, track_data->file);
	} else {
		ptag->filename[0] = '\0';
	}

	if (track_data->comment != NULL) {
		arr_strlcpy(ptag->comment, track_data->comment);
	} else {
		ptag->comment[0] = '\0';
	}

	g_free(title);
	g_free(track);

	if (data) {
		batch_tag_cancelled = 0;
		create_tag_prog_window();
		AQUALUNG_THREAD_CREATE(tag_thread_id, NULL, update_tag_thread, batch_tag_root)
	}

	return FALSE;
}

void
record_batch_tag_set_from_iter(GtkTreeIter * iter) {

	char * str;
	record_data_t * data;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, MS_COL_NAME, &str, -1);
	arr_strlcpy(album_tag, str);
	g_free(str);

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, MS_COL_DATA, &data, -1);
	arr_snprintf(year_tag, "%d", data->year);
}

gboolean
record_batch_tag(gpointer data) {

	GtkTreeIter iter_track;
	int i = 0;

	record_batch_tag_set_from_iter(&record_tag_iter);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &iter_track, &record_tag_iter, i++)) {
		track_tag_iter = iter_track;
		track_batch_tag(NULL);
	}

	if (data) {
		batch_tag_cancelled = 0;
		create_tag_prog_window();
		AQUALUNG_THREAD_CREATE(tag_thread_id, NULL, update_tag_thread, batch_tag_root)
	}

	return FALSE;
}

void
artist_batch_tag_set_from_iter(GtkTreeIter * iter) {

	char * str;
	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, MS_COL_NAME, &str, -1);
	arr_strlcpy(artist_tag, str);
	g_free(str);
}

gboolean
artist_batch_tag(gpointer data) {

	GtkTreeIter iter_record;
	int i = 0;

	artist_batch_tag_set_from_iter(&artist_tag_iter);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &iter_record, &artist_tag_iter, i++)) {
		record_tag_iter = iter_record;
		record_batch_tag(NULL);
	}

	if (data) {
		batch_tag_cancelled = 0;
		create_tag_prog_window();
		AQUALUNG_THREAD_CREATE(tag_thread_id, NULL, update_tag_thread, batch_tag_root)
	}

	return FALSE;
}

gboolean
store_batch_tag(gpointer data) {

	GtkTreeIter iter_artist;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &iter_artist, &store_tag_iter, i++)) {
		artist_tag_iter = iter_artist;
		artist_batch_tag(NULL);
	}

	if (data) {
		batch_tag_cancelled = 0;
		create_tag_prog_window();
		AQUALUNG_THREAD_CREATE(tag_thread_id, NULL, update_tag_thread, batch_tag_root)
	}

	return FALSE;
}

void
track__tag_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		if (create_tag_dialog()) {

			GtkTreeIter iter_record;
			GtkTreeIter iter_artist;

			gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store),
						   &iter_record, &iter);
			record_batch_tag_set_from_iter(&iter_record);

			gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store),
						   &iter_artist, &iter_record);
			artist_batch_tag_set_from_iter(&iter_artist);

			track_tag_iter = iter;
			aqualung_idle_add(track_batch_tag, (gpointer)1);
		}
	}
}

void
record__tag_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		if (create_tag_dialog()) {

			GtkTreeIter iter_artist;
			gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store),
						   &iter_artist, &iter);

			artist_batch_tag_set_from_iter(&iter_artist);

			record_tag_iter = iter;
			aqualung_idle_add(record_batch_tag, (gpointer)1);
		}
	}
}

void
artist__tag_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		if (create_tag_dialog()) {
			artist_tag_iter = iter;
			aqualung_idle_add(artist_batch_tag, (gpointer)1);
		}
	}
}

void
store__tag_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		if (create_tag_dialog()) {
			store_tag_iter = iter;
			aqualung_idle_add(store_batch_tag, (gpointer)1);
		}
	}
}



/************************************/


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

	case 1:
		comment = ((store_data_t *)data)->comment;
		break;
	case 2:
		comment = ((artist_data_t *)data)->comment;
		break;
	case 3:
		comment = ((record_data_t *)data)->comment;
		break;
	case 4:
		comment = ((track_data_t *)data)->comment;
		break;
	}

	if (comment != NULL && comment[0] != '\0') {
		gtk_text_buffer_insert(buffer, text_iter, comment, -1);
	} else {
		gtk_text_buffer_insert(buffer, text_iter, _("(no comment)"), -1);
	}
}


static void
track_status_bar_info(GtkTreeModel * model, GtkTreeIter * iter, double * size, float * length) {

	track_data_t * data;

	gtk_tree_model_get(model, iter, MS_COL_DATA, &data, -1);

	if (data->size == 0) {
		struct stat statbuf;
		if (stat(data->file, &statbuf) != -1) {
			data->size = statbuf.st_size;
		}
	}

	*size += data->size / 1024.0;
	*length += data->duration;
}

static void
record_status_bar_info(GtkTreeModel * model, GtkTreeIter * iter, double * size, float * length,
		       int * ntrack) {

	GtkTreeIter track_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(model, &track_iter, iter, i++)) {
		track_status_bar_info(model, &track_iter, size, length);
	}

	*ntrack += i - 1;
}

static void
artist_status_bar_info(GtkTreeModel * model, GtkTreeIter * iter, double * size, float * length,
		       int * ntrack, int * nrecord) {

	GtkTreeIter record_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(model, &record_iter, iter, i++)) {
		record_status_bar_info(model, &record_iter, size, length, ntrack);
	}

	*nrecord += i - 1;
}

void
store_status_bar_info(GtkTreeModel * model, GtkTreeIter * iter, double * size, float * length,
		       int * ntrack, int * nrecord, int * nartist) {

	GtkTreeIter artist_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(model, &artist_iter, iter, i++)) {
		artist_status_bar_info(model, &artist_iter, size, length, ntrack, nrecord);
	}

	*nartist = i - 1;
}


static void
set_status_bar_info(GtkTreeIter * tree_iter, GtkLabel * statusbar) {

	int ntrack = 0, nrecord = 0, nartist = 0;
	float length = 0.0f;
	double size = 0.0;

	store_data_t * store_data;
	record_data_t * record_data;
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
	case 4:
		track_status_bar_info(model, tree_iter, &size, &length);
		ntrack = 1;
		arr_snprintf(str, "%s ", name);
		break;
	case 3:
		gtk_tree_model_get(model, tree_iter, MS_COL_DATA, &record_data, -1);
		record_status_bar_info(model, tree_iter, &size, &length, &ntrack);
		if (is_valid_year(record_data->year)) {
			arr_snprintf(str, "%s (%d):  %d %s ", name, record_data->year,
				     ntrack, (ntrack == 1) ? _("track") : _("tracks"));
		} else {
			arr_snprintf(str, "%s:  %d %s ", name,
				     ntrack, (ntrack == 1) ? _("track") : _("tracks"));
		}

		break;
	case 2:
		artist_status_bar_info(model, tree_iter, &size, &length, &ntrack, &nrecord);
		arr_snprintf(str, "%s:  %d %s, %d %s ", name,
			     nrecord, (nrecord == 1) ? _("record") : _("records"),
			     ntrack, (ntrack == 1) ? _("track") : _("tracks"));
		break;
	case 1:
		gtk_tree_model_get(model, tree_iter, MS_COL_DATA, &store_data, -1);
		store_status_bar_info(model, tree_iter, &size, &length, &ntrack, &nrecord, &nartist);
		arr_snprintf(str, "%s:  %d %s, %d %s, %d %s ", store_data->dirty ? name+1 : name,
			     nartist, (nartist == 1) ? _("artist") : _("artists"),
			     nrecord, (nrecord == 1) ? _("record") : _("records"),
			     ntrack, (ntrack == 1) ? _("track") : _("tracks"));
		break;
	}

	g_free(name);

	if (length > 0.0f || ntrack == 0) {
		time2time(length, length_str, CHAR_ARRAY_SIZE(length_str));
		arr_snprintf(tmp, " [%s] ", length_str);
	} else {
		arr_strlcpy(tmp, " [N/A] ");
	}

	arr_strlcat(str, tmp);

	if (options.ms_statusbar_show_size) {
		if (size > 1024 * 1024) {
			arr_snprintf(tmp, " (%.1f GB) ", size / (1024 * 1024));
		} else if (size > 1024) {
			arr_snprintf(tmp, " (%.1f MB) ", size / 1024);
		} else if (size > 0 || ntrack == 0) {
			arr_snprintf(tmp, " (%.1f KB) ", size);
		} else {
			arr_strlcpy(tmp, " (N/A) ");
		}
		arr_strlcat(str, tmp);
	}

	gtk_label_set_text(statusbar, str);
}



/*********************************************************************************/

/* If a non-absolute filename is in the store, interpret it as
 * relative to the directory containing the store file. Otherwise,
 * just return the absolute filename as found in the store file.
 */
char *
track_get_absolute_path(char * store_dirname, char * filename, const GtkTreeIter * iter_track) {

	char * path = NULL;
	gchar * tmp = NULL;

	if (((tmp = g_filename_from_uri(filename, NULL, NULL)) == NULL) &&
	    (tmp = g_filename_from_utf8(filename, -1, NULL, NULL, NULL)) == NULL) {
		tmp = g_strdup(filename);
	}

	if (g_path_is_absolute(tmp)) {
		path = strndup(tmp, MAXLEN-1);
	} else {
		gchar * tmppath = g_build_filename(store_dirname, tmp, NULL);
		path = strndup(tmppath, MAXLEN-1);
		g_free(tmppath);
	}

	g_free(tmp);

	return path;
}

void
parse_track(xmlDocPtr doc, xmlNodePtr cur, GtkTreeIter * iter_record, char * store_dirname, int * save) {

	GtkTreeIter iter_track;
	xmlChar * key;

	char name[MAXLEN];
	char sort[MAXLEN];
	track_data_t * data;

	name[0] = '\0';
	sort[0] = '\0';

	if ((data = (track_data_t *)calloc(1, sizeof(track_data_t))) == NULL) {
		fprintf(stderr, "parse_track: calloc error\n");
		return;
	}

	gtk_tree_store_append(music_store, &iter_track, iter_record);
	gtk_tree_store_set(music_store, &iter_track,
			   MS_COL_NAME, "",
			   MS_COL_SORT, "",
			   MS_COL_DATA, data, -1);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &iter_track, MS_COL_ICON, icon_track, -1);
	}

	data->duration = 0.0f;
	data->volume = 1.0f;
	data->rva = 0.0f;
	data->use_rva = 0;

 	for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {

		if ((!xmlStrcmp(cur->name, (const xmlChar *)"name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				arr_strlcpy(name, (char *) key);
				xmlFree(key);
			}
			if (name[0] == '\0') {
				fprintf(stderr, "Error in XML music_store: track <name> is required, but NULL\n");
			}
			gtk_tree_store_set(music_store, &iter_track, MS_COL_NAME, name, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"sort_name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				arr_strlcpy(sort, (char *) key);
				xmlFree(key);
			}
			gtk_tree_store_set(music_store, &iter_track, MS_COL_SORT, sort, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"file"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				if (httpc_is_url((char *)key)) {
					data->file = strndup((char *)key, MAXLEN-1);
				} else {
					data->file = track_get_absolute_path(store_dirname, (char *)key, &iter_track);
				}
				xmlFree(key);
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"size"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				sscanf((char *)key, "%u", &data->size);
				xmlFree(key);
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"comment"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				data->comment = strndup((char *)key, MAXLEN-1);
				xmlFree(key);
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"duration"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                data->duration = convf((char *) key);
				xmlFree(key);
                        }
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"volume"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                data->volume = convf((char *) key);
				xmlFree(key);
                        }
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"rva"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                data->rva = convf((char *) key);
				xmlFree(key);
                        }
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"use_rva"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                data->use_rva = convf((char *) key);
				xmlFree(key);
                        }
                }
	}

	if (data->size == 0) {
		struct stat statbuf;
		if (stat(data->file, &statbuf) != -1) {
			data->size = statbuf.st_size;
			*save = 1;
		}
	}

	if (data->file == NULL) {
		fprintf(stderr, "Error in XML music_store: track <file> is required, but NULL\n");
		store_file_remove_track(&iter_track);
	}
}


void
parse_record(xmlDocPtr doc, xmlNodePtr cur, GtkTreeIter * iter_artist, char * store_dirname, int * save) {

	GtkTreeIter iter_record;
	xmlChar * key;

	char name[MAXLEN];
	char sort[MAXLEN];

	record_data_t * data;

	name[0] = '\0';
	sort[0] = '\0';

	if ((data = (record_data_t *)calloc(1, sizeof(record_data_t))) == NULL) {
		fprintf(stderr, "parse_record: calloc error\n");
		return;
	}

	gtk_tree_store_append(music_store, &iter_record, iter_artist);
	gtk_tree_store_set(music_store, &iter_record,
			   MS_COL_NAME, "",
			   MS_COL_SORT, "",
			   MS_COL_DATA, data, -1);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &iter_record, MS_COL_ICON, icon_record, -1);
	}

	for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {

		if ((!xmlStrcmp(cur->name, (const xmlChar *)"name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				arr_strlcpy(name, (char *) key);
				xmlFree(key);
			}
			if (name[0] == '\0') {
				fprintf(stderr, "Error in XML music_store: "
				       "Record <name> is required, but NULL\n");
			}
			gtk_tree_store_set(music_store, &iter_record, MS_COL_NAME, name, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"sort_name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				arr_strlcpy(sort, (char *) key);
				/* parse year from sort key if otherwise not set */
				if (is_valid_year(atoi(sort)) && !is_valid_year(data->year)) {
					data->year = atoi(sort);
				}
				xmlFree(key);
			}
			gtk_tree_store_set(music_store, &iter_record, MS_COL_SORT, sort, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"comment"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				data->comment = strndup((char *)key, MAXLEN-1);
				xmlFree(key);
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"year"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				data->year = atoi((char *)key);
				xmlFree(key);
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"track"))) {
			parse_track(doc, cur, &iter_record, store_dirname, save);
		}
	}
}


void
parse_artist(xmlDocPtr doc, xmlNodePtr cur, GtkTreeIter * iter_store, char * store_dirname, int * save) {

	GtkTreeIter iter_artist;
	xmlChar * key;

	char name[MAXLEN];
	char sort[MAXLEN];

	artist_data_t * data;

	name[0] = '\0';
	sort[0] = '\0';

	if ((data = (artist_data_t *)calloc(1, sizeof(artist_data_t))) == NULL) {
		fprintf(stderr, "parse_artist: calloc error\n");
		return;
	}

	gtk_tree_store_append(music_store, &iter_artist, iter_store);
	gtk_tree_store_set(music_store, &iter_artist,
			   MS_COL_NAME, "",
			   MS_COL_SORT, "",
			   MS_COL_DATA, data, -1);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &iter_artist, MS_COL_ICON, icon_artist, -1);
	}

	for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {

		if ((!xmlStrcmp(cur->name, (const xmlChar *)"name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				arr_strlcpy(name, (char *) key);
				xmlFree(key);
			}
			if (name[0] == '\0') {
				fprintf(stderr, "Error in XML music_store: "
				       "Artist <name> is required, but NULL\n");
			}
			gtk_tree_store_set(music_store, &iter_artist, MS_COL_NAME, name, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"sort_name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				arr_strlcpy(sort, (char *) key);
				xmlFree(key);
			}
			gtk_tree_store_set(music_store, &iter_artist, MS_COL_SORT, sort, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"comment"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				data->comment = strndup((char *)key, MAXLEN-1);
				xmlFree(key);
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"record"))) {
			parse_record(doc, cur, &iter_artist, store_dirname, save);
		}
	}
}


void
store_file_load(char * store_file, char * sort) {

	GtkTreeIter iter_store;

	char name[MAXLEN];

	name[0] = '\0';

	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar * key;
	char * store_dirname;
	int save = 0;

	store_data_t * data;

	if (access(store_file, R_OK) != 0) {
		return;
	}

	doc = xmlParseFile(store_file);
	if (doc == NULL) {
		fprintf(stderr, "An XML error occured while parsing %s\n", store_file);
		return;
	}

	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		fprintf(stderr, "store_file_load: empty XML document\n");
		xmlFreeDoc(doc);
		return;
	}

	if (xmlStrcmp(cur->name, (const xmlChar *)"music_store")) {
		fprintf(stderr, "store_file_load: XML document of the wrong type, "
			"root node != music_store\n");
		xmlFreeDoc(doc);
		return;
	}

	if ((data = (store_data_t *)calloc(1, sizeof(store_data_t))) == NULL) {
		fprintf(stderr, "store_file_load: calloc error\n");
		return;
	}


	data->type = STORE_TYPE_FILE;
	data->file = strdup(store_file);
	data->use_relative_paths = 0;
	store_dirname = g_path_get_dirname(data->file);

	if (access(store_file, W_OK) == 0) {
		data->readonly = 0;
	} else {
		data->readonly = 1;
	}

	gtk_tree_store_append(music_store, &iter_store, NULL);

	gtk_tree_store_set(music_store, &iter_store,
			   MS_COL_NAME, _("Music Store"),
			   MS_COL_SORT, sort,
			   MS_COL_FONT, PANGO_WEIGHT_BOLD,
			   MS_COL_DATA, data, -1);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &iter_store, MS_COL_ICON, icon_store, -1);
	}

	for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {

		if ((!xmlStrcmp(cur->name, (const xmlChar *)"name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				arr_strlcpy(name, (char *) key);
				xmlFree(key);
			}
			if (name[0] == '\0') {
				fprintf(stderr, "Error in XML music_store: "
					"Music Store <name> is required, but NULL\n");
			}
			gtk_tree_store_set(music_store, &iter_store, MS_COL_NAME, name, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"comment"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				data->comment = strndup((char *)key, MAXLEN-1);
				xmlFree(key);
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"use_relative_paths"))) {
			data->use_relative_paths = 1;
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"artist"))) {
			parse_artist(doc, cur, &iter_store, store_dirname, &save);
		}
	}

	xmlFreeDoc(doc);
	g_free(store_dirname);

	if (save && !data->readonly) {
		music_store_mark_changed(&iter_store);
		store_file_save(&iter_store);
	}

	if (options.autoexpand_stores) {
		GtkTreePath * path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), &iter_store);
		gtk_tree_view_expand_row(GTK_TREE_VIEW(music_tree), path, FALSE);
		gtk_tree_path_free(path);
	}
}


/**********************************************************************************/


void
save_track(xmlDocPtr doc, xmlNodePtr node_track, GtkTreeIter * iter_track,
	   char * store_dirname, int dirname_strlen, int use_relative_paths) {

	xmlNodePtr node;
	char * name;
	char * sort;
	track_data_t * data;
	char str[32];

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_track,
			   MS_COL_NAME, &name,
			   MS_COL_SORT, &sort,
			   MS_COL_DATA, &data,
			   -1);

	node = xmlNewTextChild(node_track, NULL, (const xmlChar *) "track", NULL);
	if (name[0] == '\0') {
		fprintf(stderr, "saving music_store XML: warning: track node with empty <name>\n");
	}
	xmlNewTextChild(node, NULL, (const xmlChar *) "name", (const xmlChar *) name);
	if (sort[0] != '\0') {
		xmlNewTextChild(node, NULL, (const xmlChar *) "sort_name", (const xmlChar *) sort);
	}

	if (data->file == NULL && data->file[0] != '\0') {
		fprintf(stderr, "saving music_store XML: warning: track node with empty <file>\n");
		xmlNewTextChild(node, NULL, (const xmlChar *) "file", (const xmlChar *) "");
	} else {
		if (httpc_is_url(data->file)) {
			gchar * tmp = g_filename_to_utf8(data->file, -1, NULL, NULL, NULL);
			xmlNewTextChild(node, NULL, (const xmlChar *) "file", (const xmlChar *) tmp);
			g_free(tmp);
		} else if (use_relative_paths && g_str_has_prefix(data->file, store_dirname)) {
			gchar * tmp = data->file + dirname_strlen + 1;
			xmlNewTextChild(node, NULL, (const xmlChar *) "file", (const xmlChar *) tmp);
		} else {
			gchar * tmp = g_filename_to_uri(data->file, NULL, NULL);
			xmlNewTextChild(node, NULL, (const xmlChar *) "file", (const xmlChar *) tmp);
			g_free(tmp);
		}
	}

	if (data->size != 0) {
		arr_snprintf(str, "%u", data->size);
		xmlNewTextChild(node, NULL, (const xmlChar *) "size", (const xmlChar *) str);
	}

	if (data->comment != NULL && data->comment[0] != '\0') {
		xmlNewTextChild(node, NULL, (const xmlChar *) "comment", (const xmlChar *) data->comment);
	}

	if (data->duration != 0.0f) {
		arr_snprintf(str, "%.1f", data->duration);
		xmlNewTextChild(node, NULL, (const xmlChar *) "duration", (const xmlChar *) str);
	}

	if (data->volume <= 0.1f) {
		arr_snprintf(str, "%.1f", data->volume);
		xmlNewTextChild(node, NULL, (const xmlChar *) "volume", (const xmlChar *) str);
	}

	if (data->rva != 0.0f) {
		arr_snprintf(str, "%.1f", data->rva);
		xmlNewTextChild(node, NULL, (const xmlChar *) "rva", (const xmlChar *) str);
	}

	if (data->use_rva) {
		arr_snprintf(str, "%d", data->use_rva);
		xmlNewTextChild(node, NULL, (const xmlChar *) "use_rva", (const xmlChar *) str);
	}

	g_free(name);
	g_free(sort);
}


void
save_record(xmlDocPtr doc, xmlNodePtr node_record, GtkTreeIter * iter_record,
	    char * store_dirname, int dirname_strlen, int use_relative_paths) {

	xmlNodePtr node;
	char * name;
	char * sort;
	record_data_t * data;
	GtkTreeIter iter_track;
	int i = 0;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_record,
			   MS_COL_NAME, &name,
			   MS_COL_SORT, &sort,
			   MS_COL_DATA, &data, -1);

	node = xmlNewTextChild(node_record, NULL, (const xmlChar *) "record", NULL);
	if (name[0] == '\0') {
		fprintf(stderr, "saving music_store XML: warning: record node with empty <name>\n");
	}
	xmlNewTextChild(node, NULL, (const xmlChar *) "name", (const xmlChar *) name);
	if (sort[0] != '\0') {
		xmlNewTextChild(node, NULL, (const xmlChar *) "sort_name", (const xmlChar *) sort);
	}
	if (data->comment != NULL && data->comment[0] != '\0') {
		xmlNewTextChild(node, NULL, (const xmlChar *) "comment", (const xmlChar *) data->comment);
	}
	if (data->year != 0) {
		char str[32];
		arr_snprintf(str, "%d", data->year);
		xmlNewTextChild(node, NULL, (const xmlChar *) "year", (const xmlChar *) str);
	}

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track, iter_record, i++)) {
		save_track(doc, node, &iter_track, store_dirname, dirname_strlen, use_relative_paths);
	}

	g_free(name);
	g_free(sort);
}


void
save_artist(xmlDocPtr doc, xmlNodePtr root, GtkTreeIter * iter_artist,
	    char * store_dirname, int dirname_strlen, int use_relative_paths) {

	xmlNodePtr node;
	char * name;
	char * sort;
	artist_data_t * data;
	GtkTreeIter iter_record;
	int i = 0;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_artist,
			   MS_COL_NAME, &name,
			   MS_COL_SORT, &sort,
			   MS_COL_DATA, &data, -1);

	node = xmlNewTextChild(root, NULL, (const xmlChar *) "artist", NULL);
	if (name[0] == '\0') {
		fprintf(stderr, "saving music_store XML: warning: artist node with empty <name>\n");
	}
	xmlNewTextChild(node, NULL, (const xmlChar *) "name", (const xmlChar *) name);
	if (sort[0] != '\0') {
		xmlNewTextChild(node, NULL, (const xmlChar *) "sort_name", (const xmlChar *) sort);
	}
	if (data->comment != NULL && data->comment[0] != '\0') {
		xmlNewTextChild(node, NULL, (const xmlChar *) "comment", (const xmlChar *) data->comment);
	}

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_record, iter_artist, i++)) {
		save_record(doc, node, &iter_record, store_dirname, dirname_strlen, use_relative_paths);
	}

	g_free(name);
	g_free(sort);
}


void
store_file_save(GtkTreeIter * iter_store) {

	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr build_node;
	char * store_dirname;
	int dirname_strlen;
	char * name;
	int i;
	store_data_t * data;
	GtkTreeIter iter_artist;


	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_store, MS_COL_DATA, &data, -1);

	if (!data->dirty) {
		return;
	}

	if (access(data->file, W_OK) != 0) {
		message_dialog(_("Warning"),
			       browser_window,
			       GTK_MESSAGE_WARNING,
			       GTK_BUTTONS_CLOSE,
			       NULL,
			       _("File \"%s\" does not exist or your write permission has been withdrawn. "
				 "Check if the partition containing the store file has been unmounted."),
			       data->file);
		return;
	}

	music_store_mark_saved(iter_store);

	store_dirname = g_path_get_dirname(data->file);
	dirname_strlen = strlen(store_dirname);

	doc = xmlNewDoc((const xmlChar *) "1.0");
	root = xmlNewNode(NULL, (const xmlChar *) "music_store");
	xmlDocSetRootElement(doc, root);

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_store, MS_COL_NAME, &name, -1);
	if (name[0] == '\0') {
		fprintf(stderr, "saving music_store XML: warning: empty <name>\n");
	}
	xmlNewTextChild(root, NULL, (const xmlChar *) "name", (const xmlChar *) name);
	g_free(name);

	if (data->comment != NULL && data->comment[0] != '\0') {
		xmlNewTextChild(root, NULL, (const xmlChar *) "comment", (const xmlChar *) data->comment);
	}

	if (data->use_relative_paths) {
		xmlNewChild(root, NULL, (const xmlChar *) "use_relative_paths", NULL);
	}

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_artist, iter_store, i++)) {
		save_artist(doc, root, &iter_artist, store_dirname, dirname_strlen, data->use_relative_paths);
	}

	if ((build_node = build_store_get_xml_node(data->file)) != NULL) {
		xmlAddChild(root, build_node);
	}

	xmlSaveFormatFile(data->file, doc, 1);
	xmlFreeDoc(doc);
	g_free(store_dirname);
}


/*************************************************/

/* music store interface */

int
store_file_iter_is_track(GtkTreeIter * iter) {

	GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), iter);
	int ret = (gtk_tree_path_get_depth(p) == 4);
	gtk_tree_path_free(p);
	return ret;
}


void
store_file_iter_addlist_defmode(GtkTreeIter * ms_iter, GtkTreeIter * pl_iter, int new_tab) {

	GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), ms_iter);
	add_path_to_playlist(p, pl_iter, new_tab);
	gtk_tree_path_free(p);
}


void
store_file_selection_changed(GtkTreeIter * tree_iter, GtkTextBuffer * buffer, GtkLabel * statusbar) {

	GtkTextIter text_iter;

	gtk_text_buffer_get_iter_at_offset(buffer, &text_iter, 0);

	insert_cover(tree_iter, &text_iter, buffer);
	set_comment_text(tree_iter, &text_iter, buffer);

	if (options.enable_mstore_statusbar) {
		set_status_bar_info(tree_iter, statusbar);
	}
}


gboolean
store_file_event_cb(GdkEvent * event, GtkTreeIter * iter, GtkTreePath * path) {

	if (event->type == GDK_BUTTON_PRESS) {

		GdkEventButton * bevent = (GdkEventButton *)event;

                if (bevent->button == 3) {

			set_popup_sensitivity(path);

			switch (gtk_tree_path_get_depth(path)) {
			case 1:
				gtk_menu_popup(GTK_MENU(store_menu), NULL, NULL, NULL, NULL,
					       bevent->button, bevent->time);
				break;
			case 2:
				gtk_menu_popup(GTK_MENU(artist_menu), NULL, NULL, NULL, NULL,
					       bevent->button, bevent->time);
				break;
			case 3:
				gtk_menu_popup(GTK_MENU(record_menu), NULL, NULL, NULL, NULL,
					       bevent->button, bevent->time);
				break;
			case 4:
				gtk_menu_popup(GTK_MENU(track_menu), NULL, NULL, NULL, NULL,
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
			for (i = 0; store_keybinds[i].callback; ++i) {
				if ((store_keybinds[i].state == 0 && kevent->state == 0) ||
				    store_keybinds[i].state & kevent->state) {
					if (kevent->keyval == store_keybinds[i].keyval1 ||
					    kevent->keyval == store_keybinds[i].keyval2) {
						(store_keybinds[i].callback)(NULL);
					}
				}
			}
			break;
		case 2:
			for (i = 0; artist_keybinds[i].callback; ++i) {
				if ((artist_keybinds[i].state == 0 && kevent->state == 0) ||
				    artist_keybinds[i].state & kevent->state) {
					if (kevent->keyval == artist_keybinds[i].keyval1 ||
					    kevent->keyval == artist_keybinds[i].keyval2) {
						(artist_keybinds[i].callback)(NULL);
					}
				}
			}
			break;
		case 3:
			for (i = 0; record_keybinds[i].callback; ++i) {
				if ((record_keybinds[i].state == 0 && kevent->state == 0) ||
				    record_keybinds[i].state & kevent->state) {
					if (kevent->keyval == record_keybinds[i].keyval1 ||
					    kevent->keyval == record_keybinds[i].keyval2) {
						(record_keybinds[i].callback)(NULL);
					}
				}
			}
			break;
		case 4:
			for (i = 0; track_keybinds[i].callback; ++i) {
				if ((track_keybinds[i].state == 0 && kevent->state == 0) ||
				    track_keybinds[i].state & kevent->state) {
					if (kevent->keyval == track_keybinds[i].keyval1 ||
					    kevent->keyval == track_keybinds[i].keyval2) {
						(track_keybinds[i].callback)(NULL);
					}
				}
			}
			break;
		}
	}

	return FALSE;
}


void
store_file_load_icons(void) {

	char path[MAXLEN];

	arr_snprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-artist.png");
	icon_artist = gdk_pixbuf_new_from_file (path, NULL);
	arr_snprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-record.png");
	icon_record = gdk_pixbuf_new_from_file (path, NULL);
	arr_snprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-track.png");
	icon_track = gdk_pixbuf_new_from_file (path, NULL);
}


void
store_file_create_popup_menu(void) {

	/* create popup menu for music store tree items */
	store_menu = gtk_menu_new();
	register_toplevel_window(store_menu, TOP_WIN_SKIN);

	store__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	store__addlist_albummode = gtk_menu_item_new_with_label(_("Add to playlist (Album mode)"));
	store__separator1 = gtk_separator_menu_item_new();
	store__add = gtk_menu_item_new_with_label(_("Create empty store..."));
	store__build = gtk_menu_item_new_with_label(_("Build / Update store from filesystem..."));
	store__edit = gtk_menu_item_new_with_label(_("Edit store..."));
#ifdef HAVE_TRANSCODING
	store__export = gtk_menu_item_new_with_label(_("Export store..."));
#endif /* HAVE_TRANSCODING */
	store__save = gtk_menu_item_new_with_label(_("Save store"));
	store__remove = gtk_menu_item_new_with_label(_("Remove store"));
	store__separator2 = gtk_separator_menu_item_new();
	store__addart = gtk_menu_item_new_with_label(_("Add new artist to this store..."));
	store__separator3 = gtk_separator_menu_item_new();
	store__volume = gtk_menu_item_new_with_label(_("Calculate volume (recursive)"));
	store__volume_menu = gtk_menu_new();
	store__volume_unmeasured = gtk_menu_item_new_with_label(_("Unmeasured tracks only"));
	store__volume_all = gtk_menu_item_new_with_label(_("All tracks"));
	store__tag = gtk_menu_item_new_with_label(_("Batch-update file metadata..."));
	store__search = gtk_menu_item_new_with_label(_("Search..."));

	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__addlist_albummode);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__add);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__build);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__edit);
#ifdef HAVE_TRANSCODING
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__export);
#endif /* HAVE_TRANSCODING */
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__save);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__remove);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__addart);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__separator3);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__volume);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(store__volume), store__volume_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(store__volume_menu), store__volume_unmeasured);
        gtk_menu_shell_append(GTK_MENU_SHELL(store__volume_menu), store__volume_all);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__tag);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__search);

	g_signal_connect_swapped(G_OBJECT(store__addlist), "activate", G_CALLBACK(store__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__addlist_albummode), "activate", G_CALLBACK(store__addlist_albummode_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__add), "activate", G_CALLBACK(store__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__build), "activate", G_CALLBACK(store__build_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__edit), "activate", G_CALLBACK(store__edit_cb), NULL);
#ifdef HAVE_TRANSCODING
	g_signal_connect_swapped(G_OBJECT(store__export), "activate", G_CALLBACK(store__export_cb), NULL);
#endif /* HAVE_TRANSCODING */
	g_signal_connect_swapped(G_OBJECT(store__save), "activate", G_CALLBACK(store__save_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__remove), "activate", G_CALLBACK(store__remove_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__addart), "activate", G_CALLBACK(artist__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__volume_unmeasured), "activate", G_CALLBACK(store__volume_unmeasured_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__volume_all), "activate", G_CALLBACK(store__volume_all_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__tag), "activate", G_CALLBACK(store__tag_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__search), "activate", G_CALLBACK(search_cb), NULL);

	gtk_widget_show(store__addlist);
	gtk_widget_show(store__addlist_albummode);
	gtk_widget_show(store__separator1);
	gtk_widget_show(store__add);
	gtk_widget_show(store__build);
	gtk_widget_show(store__edit);
#ifdef HAVE_TRANSCODING
	gtk_widget_show(store__export);
#endif /* HAVE_TRANSCODING */
	gtk_widget_show(store__save);
	gtk_widget_show(store__remove);
	gtk_widget_show(store__separator2);
	gtk_widget_show(store__addart);
	gtk_widget_show(store__separator3);
	gtk_widget_show(store__volume);
	gtk_widget_show(store__volume_unmeasured);
	gtk_widget_show(store__volume_all);
	gtk_widget_show(store__tag);
	gtk_widget_show(store__search);

	/* create popup menu for artist tree items */
	artist_menu = gtk_menu_new();
	register_toplevel_window(artist_menu, TOP_WIN_SKIN);

	artist__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	artist__addlist_albummode = gtk_menu_item_new_with_label(_("Add to playlist (Album mode)"));
	artist__separator1 = gtk_separator_menu_item_new();
	artist__add = gtk_menu_item_new_with_label(_("Add new artist..."));
	artist__edit = gtk_menu_item_new_with_label(_("Edit artist..."));
#ifdef HAVE_TRANSCODING
	artist__export = gtk_menu_item_new_with_label(_("Export artist..."));
#endif /* HAVE_TRANSCODING */
	artist__remove = gtk_menu_item_new_with_label(_("Remove artist"));
	artist__separator2 = gtk_separator_menu_item_new();
	artist__addrec = gtk_menu_item_new_with_label(_("Add new record to this artist..."));
	artist__separator3 = gtk_separator_menu_item_new();
	artist__volume = gtk_menu_item_new_with_label(_("Calculate volume (recursive)"));
	artist__volume_menu = gtk_menu_new();
	artist__volume_unmeasured = gtk_menu_item_new_with_label(_("Unmeasured tracks only"));
	artist__volume_all = gtk_menu_item_new_with_label(_("All tracks"));
	artist__tag = gtk_menu_item_new_with_label(_("Batch-update file metadata..."));
	artist__search = gtk_menu_item_new_with_label(_("Search..."));

	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__addlist_albummode);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__add);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__edit);
#ifdef HAVE_TRANSCODING
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__export);
#endif /* HAVE_TRANSCODING */
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__remove);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__addrec);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__separator3);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__volume);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(artist__volume), artist__volume_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(artist__volume_menu), artist__volume_unmeasured);
        gtk_menu_shell_append(GTK_MENU_SHELL(artist__volume_menu), artist__volume_all);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__tag);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__search);

	g_signal_connect_swapped(G_OBJECT(artist__addlist), "activate", G_CALLBACK(artist__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__addlist_albummode), "activate", G_CALLBACK(artist__addlist_albummode_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__add), "activate", G_CALLBACK(artist__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__edit), "activate", G_CALLBACK(artist__edit_cb), NULL);
#ifdef HAVE_TRANSCODING
	g_signal_connect_swapped(G_OBJECT(artist__export), "activate", G_CALLBACK(artist__export_cb), NULL);
#endif /* HAVE_TRANSCODING */
	g_signal_connect_swapped(G_OBJECT(artist__remove), "activate", G_CALLBACK(artist__remove_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__addrec), "activate", G_CALLBACK(record__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__volume_unmeasured), "activate", G_CALLBACK(artist__volume_unmeasured_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__volume_all), "activate", G_CALLBACK(artist__volume_all_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__tag), "activate", G_CALLBACK(artist__tag_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__search), "activate", G_CALLBACK(search_cb), NULL);

	gtk_widget_show(artist__addlist);
	gtk_widget_show(artist__addlist_albummode);
	gtk_widget_show(artist__separator1);
	gtk_widget_show(artist__add);
	gtk_widget_show(artist__edit);
#ifdef HAVE_TRANSCODING
	gtk_widget_show(artist__export);
#endif /* HAVE_TRANSCODING */
	gtk_widget_show(artist__remove);
	gtk_widget_show(artist__separator2);
	gtk_widget_show(artist__addrec);
	gtk_widget_show(artist__separator3);
	gtk_widget_show(artist__volume);
	gtk_widget_show(artist__volume_unmeasured);
	gtk_widget_show(artist__volume_all);
	gtk_widget_show(artist__tag);
	gtk_widget_show(artist__search);

	/* create popup menu for record tree items */
	record_menu = gtk_menu_new();
	register_toplevel_window(record_menu, TOP_WIN_SKIN);

	record__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	record__addlist_albummode = gtk_menu_item_new_with_label(_("Add to playlist (Album mode)"));
	record__separator1 = gtk_separator_menu_item_new();
	record__add = gtk_menu_item_new_with_label(_("Add new record..."));
	record__edit = gtk_menu_item_new_with_label(_("Edit record..."));
#ifdef HAVE_TRANSCODING
	record__export = gtk_menu_item_new_with_label(_("Export record..."));
#endif /* HAVE_TRANSCODING */
	record__remove = gtk_menu_item_new_with_label(_("Remove record"));
	record__separator2 = gtk_separator_menu_item_new();
	record__addtrk = gtk_menu_item_new_with_label(_("Add new track to this record..."));
#ifdef HAVE_CDDB
	record__cddb = gtk_menu_item_new_with_label(_("CDDB query for this record..."));
#endif /* HAVE_CDDB */
	record__separator3 = gtk_separator_menu_item_new();
	record__volume = gtk_menu_item_new_with_label(_("Calculate volume (recursive)"));
	record__volume_menu = gtk_menu_new();
	record__volume_unmeasured = gtk_menu_item_new_with_label(_("Unmeasured tracks only"));
	record__volume_all = gtk_menu_item_new_with_label(_("All tracks"));
	record__tag = gtk_menu_item_new_with_label(_("Batch-update file metadata..."));
	record__search = gtk_menu_item_new_with_label(_("Search..."));

	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__addlist_albummode);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__add);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__edit);
#ifdef HAVE_TRANSCODING
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__export);
#endif /* HAVE_TRANSCODING */
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__remove);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__addtrk);
#ifdef HAVE_CDDB
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__cddb);
#endif /* HAVE_CDDB */
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__separator3);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__volume);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(record__volume), record__volume_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(record__volume_menu), record__volume_unmeasured);
        gtk_menu_shell_append(GTK_MENU_SHELL(record__volume_menu), record__volume_all);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__tag);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__search);

	g_signal_connect_swapped(G_OBJECT(record__addlist), "activate", G_CALLBACK(record__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__addlist_albummode), "activate", G_CALLBACK(record__addlist_albummode_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__add), "activate", G_CALLBACK(record__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__edit), "activate", G_CALLBACK(record__edit_cb), NULL);
#ifdef HAVE_TRANSCODING
	g_signal_connect_swapped(G_OBJECT(record__export), "activate", G_CALLBACK(record__export_cb), NULL);
#endif /* HAVE_TRANSCODING */
	g_signal_connect_swapped(G_OBJECT(record__remove), "activate", G_CALLBACK(record__remove_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__addtrk), "activate", G_CALLBACK(track__add_cb), NULL);
#ifdef HAVE_CDDB
	g_signal_connect_swapped(G_OBJECT(record__cddb), "activate", G_CALLBACK(record__cddb_cb), NULL);
#endif /* HAVE_CDDB */
	g_signal_connect_swapped(G_OBJECT(record__volume_unmeasured), "activate", G_CALLBACK(record__volume_unmeasured_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__volume_all), "activate", G_CALLBACK(record__volume_all_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__tag), "activate", G_CALLBACK(record__tag_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__search), "activate", G_CALLBACK(search_cb), NULL);

	gtk_widget_show(record__addlist);
	gtk_widget_show(record__addlist_albummode);
	gtk_widget_show(record__separator1);
	gtk_widget_show(record__add);
	gtk_widget_show(record__edit);
#ifdef HAVE_TRANSCODING
	gtk_widget_show(record__export);
#endif /* HAVE_TRANSCODING */
	gtk_widget_show(record__remove);
	gtk_widget_show(record__separator2);
	gtk_widget_show(record__addtrk);
#ifdef HAVE_CDDB
	gtk_widget_show(record__cddb);
#endif /* HAVE_CDDB */
	gtk_widget_show(record__separator3);
	gtk_widget_show(record__volume);
	gtk_widget_show(record__volume_unmeasured);
	gtk_widget_show(record__volume_all);
	gtk_widget_show(record__tag);
	gtk_widget_show(record__search);

	/* create popup menu for track tree items */
	track_menu = gtk_menu_new();
	register_toplevel_window(track_menu, TOP_WIN_SKIN);

	track__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	track__separator1 = gtk_separator_menu_item_new();
	track__add = gtk_menu_item_new_with_label(_("Add new track..."));
	track__edit = gtk_menu_item_new_with_label(_("Edit track..."));
#ifdef HAVE_TRANSCODING
	track__export = gtk_menu_item_new_with_label(_("Export track..."));
#endif /* HAVE_TRANSCODING */
	track__remove = gtk_menu_item_new_with_label(_("Remove track"));
	track__separator2 = gtk_separator_menu_item_new();
	track__fileinfo = gtk_menu_item_new_with_label(_("File info..."));
	track__separator3 = gtk_separator_menu_item_new();
	track__volume = gtk_menu_item_new_with_label(_("Calculate volume"));
	track__volume_menu = gtk_menu_new();
	track__volume_unmeasured = gtk_menu_item_new_with_label(_("Only if unmeasured"));
	track__volume_all = gtk_menu_item_new_with_label(_("In any case"));
	track__tag = gtk_menu_item_new_with_label(_("Update file metadata..."));
	track__search = gtk_menu_item_new_with_label(_("Search..."));

	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__add);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__edit);
#ifdef HAVE_TRANSCODING
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__export);
#endif /* HAVE_TRANSCODING */
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__remove);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__fileinfo);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__separator3);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__volume);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(track__volume), track__volume_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(track__volume_menu), track__volume_unmeasured);
        gtk_menu_shell_append(GTK_MENU_SHELL(track__volume_menu), track__volume_all);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__tag);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__search);

	g_signal_connect_swapped(G_OBJECT(track__addlist), "activate", G_CALLBACK(track__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__add), "activate", G_CALLBACK(track__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__edit), "activate", G_CALLBACK(track__edit_cb), NULL);
#ifdef HAVE_TRANSCODING
	g_signal_connect_swapped(G_OBJECT(track__export), "activate", G_CALLBACK(track__export_cb), NULL);
#endif /* HAVE_TRANSCODING */
	g_signal_connect_swapped(G_OBJECT(track__remove), "activate", G_CALLBACK(track__remove_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__fileinfo), "activate", G_CALLBACK(track__fileinfo_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__volume_unmeasured), "activate", G_CALLBACK(track__volume_unmeasured_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__volume_all), "activate", G_CALLBACK(track__volume_all_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__tag), "activate", G_CALLBACK(track__tag_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__search), "activate", G_CALLBACK(search_cb), NULL);

	gtk_widget_show(track__addlist);
	gtk_widget_show(track__separator1);
	gtk_widget_show(track__add);
	gtk_widget_show(track__edit);
#ifdef HAVE_TRANSCODING
	gtk_widget_show(track__export);
#endif /* HAVE_TRANSCODING */
	gtk_widget_show(track__remove);
	gtk_widget_show(track__separator2);
	gtk_widget_show(track__fileinfo);
	gtk_widget_show(track__separator3);
	gtk_widget_show(track__volume);
	gtk_widget_show(track__volume_unmeasured);
	gtk_widget_show(track__volume_all);
	gtk_widget_show(track__tag);
	gtk_widget_show(track__search);
}

void
store_file_insert_progress_bar(GtkWidget * vbox) {

	ms_progress_bar_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_box_pack_start(GTK_BOX(vbox), ms_progress_bar_container, FALSE, FALSE, 1);

	if (ms_progress_bar_semaphore > 0) {
		ms_progress_bar_show();
	}
}

void
store_file_set_toolbar_sensitivity(GtkTreeIter * iter, GtkWidget * edit,
				   GtkWidget * add, GtkWidget * remove) {

	GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), iter);
	int depth = gtk_tree_path_get_depth(p);
	int val = !is_store_iter_readonly(iter);

	gtk_widget_set_sensitive(edit, val);
	gtk_widget_set_sensitive(add, val);
	gtk_widget_set_sensitive(remove, val || depth == 1);

	gtk_tree_path_free(p);
}

void
store_file_toolbar__edit_cb(gpointer data) {

	GtkTreeModel * model;
	GtkTreeIter parent_iter;
	GtkTreePath * parent_path;
        gint level;

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		level = gtk_tree_path_get_depth(parent_path);

                switch (level) {

                        case 1: /* store */
                                store__edit_cb(NULL);
                                break;
                        case 2: /* artist */
                                artist__edit_cb(NULL);
                                break;
                        case 3: /* album */
                                record__edit_cb(NULL);
                                break;
                        case 4: /* track */
                                track__edit_cb(NULL);
                                break;

                        default:
                                break;
                }
        }
}

void
store_file_toolbar__add_cb(gpointer data) {

	GtkTreeModel * model;
	GtkTreeIter parent_iter;
	GtkTreePath * parent_path;
        gint level;

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		level = gtk_tree_path_get_depth(parent_path);

                switch (level) {

                        case 1: /* store */
                                store__add_cb(NULL);
                                break;
                        case 2: /* artist */
                                artist__add_cb(NULL);
                                break;
                        case 3: /* album */
                                record__add_cb(NULL);
                                break;
                        case 4: /* track */
                                track__add_cb(NULL);
                                break;
                }
        }
}

void
store_file_toolbar__remove_cb(gpointer data) {

	GtkTreeModel * model;
	GtkTreeIter parent_iter;
	GtkTreePath * parent_path;
        gint level;

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		level = gtk_tree_path_get_depth(parent_path);

                switch (level) {

                        case 1: /* store */
                                store__remove_cb(NULL);
                                break;
                        case 2: /* artist */
                                artist__remove_cb(NULL);
                                break;
                        case 3: /* album */
                                record__remove_cb(NULL);
                                break;
                        case 4: /* track */
                                track__remove_cb(NULL);
                                break;
                }
        }
}

/* passed as fileinfo_model_func_t argument to show_file_info */
static gboolean
store_model_func(GtkTreeModel * model, GtkTreeIter iter, char**name, char**file) {
	char * artist_name;
	char * record_name;
	char * track_name;
	track_data_t * data;
	GtkTreeIter iter_record;
	GtkTreeIter iter_artist;
	char buf[MAXLEN];

	gtk_tree_model_get(model, &iter, MS_COL_NAME, &track_name, MS_COL_DATA, &data, -1);
	gtk_tree_model_iter_parent(model, &iter_record, &iter);
	gtk_tree_model_get(model, &iter_record, MS_COL_NAME, &record_name, -1);
	gtk_tree_model_iter_parent(model, &iter_artist, &iter_record);
	gtk_tree_model_get(model, &iter_artist, MS_COL_NAME, &artist_name, -1);

	make_title_string(buf, CHAR_ARRAY_SIZE(buf), options.title_format,
			  artist_name, record_name, track_name);
	g_free(artist_name);
	g_free(record_name);
	g_free(track_name);

	*name = strndup(buf, MAXLEN-1);
	*file = strdup(data->file);
	return TRUE;
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :
