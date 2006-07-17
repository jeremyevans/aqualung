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
#include <unistd.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <sys/stat.h>

#include "common.h"
#include "i18n.h"
#include "options.h"
#include "music_browser.h"
#include "meta_decoder.h"
#include "build_store.h"
#include "cddb_lookup.h"

extern options_t options;
extern GtkTreeStore * music_store;

extern GtkWidget * browser_window;
extern GtkWidget * gui_stock_label_button(gchar * blabel, const gchar * bstock);
extern void set_sliders_width(void);

enum {
	MODE_NONE,
	MODE_META,
	MODE_CDDB,
	MODE_FILE
};

int search_order[] = { MODE_META, MODE_CDDB, MODE_FILE };

pthread_t build_store_thread_id;

GtkTreeIter store_iter;

GtkWidget * meta_check_import;
GtkWidget * meta_check_exclude_wspace;
GtkWidget * meta_check_title;
GtkWidget * meta_check_artist;
GtkWidget * meta_check_record;

GtkWidget * cddb_check_enable;
GtkWidget * cddb_radio_batch;
GtkWidget * cddb_radio_interactive;

GtkWidget * fs_radio_preset;
GtkWidget * fs_radio_regexp;
GtkWidget * fs_check_rm_number;
GtkWidget * fs_check_rm_ext;
GtkWidget * fs_check_underscore;
GtkWidget * fs_entry_regexp1;
GtkWidget * fs_entry_regexp2;

char root[MAXLEN];
/*
char name[MAXLEN];
char file[MAXLEN];
char comment[MAXLEN];
*/


void
meta_check_import_toggled(GtkWidget * widget, gpointer * data) {

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_import));

	gtk_widget_set_sensitive(meta_check_exclude_wspace, state);
	gtk_widget_set_sensitive(meta_check_title, state);
	gtk_widget_set_sensitive(meta_check_artist, state);
	gtk_widget_set_sensitive(meta_check_record, state);
}

void
cddb_check_enable_toggled(GtkWidget * widget, gpointer * data) {

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cddb_check_enable));

	gtk_widget_set_sensitive(cddb_radio_batch, state);
	gtk_widget_set_sensitive(cddb_radio_interactive, state);
}


void
fs_radio_preset_toggled(GtkWidget * widget, gpointer * data) {

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fs_radio_preset));

	gtk_widget_set_sensitive(fs_check_rm_number, state);
	gtk_widget_set_sensitive(fs_check_rm_ext, state);
	gtk_widget_set_sensitive(fs_check_underscore, state);

	gtk_widget_set_sensitive(fs_entry_regexp1, !state);
	gtk_widget_set_sensitive(fs_entry_regexp2, !state);
}


int
browse_button_clicked(GtkWidget * widget, gpointer * data) {

        GtkWidget * dialog;
	const gchar * selected_filename = gtk_entry_get_text(GTK_ENTRY(data));


        dialog = gtk_file_chooser_dialog_new(_("Please select the root directory."),
                                             GTK_WINDOW(browser_window),
                                             GTK_FILE_CHOOSER_ACTION_SAVE,
                                             GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        set_sliders_width();    /* MAGIC */

        if (strlen(selected_filename)) {
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), selected_filename);
	} else {
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
	}

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);


        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		gtk_entry_set_text(GTK_ENTRY(data), selected_filename);

                strncpy(options.currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
                                                                         MAXLEN-1);
        }


        gtk_widget_destroy(dialog);

        set_sliders_width();    /* MAGIC */

	return 0;
}


int
build_store_dialog(void) {

	GtkWidget * dialog;
	GtkWidget * notebook;
	GtkWidget * label;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * hbox2;

	GtkWidget * root_entry;
	GtkWidget * root_label;
	GtkWidget * browse_button;
	/*
        GtkWidget * name_entry;
	GtkWidget * name_label;
	GtkWidget * file_entry;
	GtkWidget * file_label;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	*/
	GtkWidget * meta_vbox;

	GtkWidget * cddb_vbox;

	GtkWidget * fs_vbox;

        int ret;


        dialog = gtk_dialog_new_with_buttons(_("Build store"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

        gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, 300);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), notebook);


	/* General */

	table = gtk_table_new(3, 2, FALSE);
	label = gtk_label_new(_("General"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table, label);

	root_label = gtk_label_new(_("Root path:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), root_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 5, 5);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox2, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

        root_entry = gtk_entry_new_with_max_length(MAXLEN-1);
	gtk_entry_set_text(GTK_ENTRY(root_entry), "~/music"); // TMP: just for testing
        gtk_box_pack_start(GTK_BOX(hbox2), root_entry, TRUE, TRUE, 0);

	browse_button = gui_stock_label_button(_("_Browse..."), GTK_STOCK_OPEN);
        gtk_box_pack_start(GTK_BOX(hbox2), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked",
			 G_CALLBACK(browse_button_clicked),
			 (gpointer *)root_entry);

	/*
	file_label = gtk_label_new(_("Filename:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), file_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 5);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox2, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

        file_entry = gtk_entry_new_with_max_length(MAXLEN-1);
        gtk_box_pack_start(GTK_BOX(hbox2), file_entry, TRUE, TRUE, 0);


	browse_button = gui_stock_label_button(_("_Browse..."), GTK_STOCK_OPEN);
        gtk_box_pack_start(GTK_BOX(hbox2), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked", G_CALLBACK(browse_button_store_clicked),
			 (gpointer *)file_entry);
	

	name_label = gtk_label_new(_("Name:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 5);

        name_entry = gtk_entry_new_with_max_length(MAXLEN-1);
	gtk_table_attach(GTK_TABLE(table), name_entry, 1, 2, 2, 3,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);


	comment_label = gtk_label_new(_("Comments:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), comment_label, FALSE, FALSE, 5);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 2, 3, 4,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	viewport = gtk_viewport_new(NULL, NULL);
	gtk_table_attach(GTK_TABLE(table), viewport, 0, 2, 4, 5,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);

        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);
	*/

	/* Metadata */

        meta_vbox = gtk_vbox_new(FALSE, 0);
	label = gtk_label_new(_("Metadata"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), meta_vbox, label);

	meta_check_import =
		gtk_check_button_new_with_label(_("Auto-import file metadata"));
	gtk_widget_set_name(meta_check_import, "check_on_notebook");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_import), TRUE);
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_import, FALSE, FALSE, 5);

	g_signal_connect(G_OBJECT(meta_check_import), "toggled",
			 G_CALLBACK(meta_check_import_toggled), NULL);

	meta_check_exclude_wspace =
		gtk_check_button_new_with_label(_("Exclude whitespace-only metadata"));
	gtk_widget_set_name(meta_check_exclude_wspace, "check_on_notebook");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_exclude_wspace), TRUE);
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_exclude_wspace, FALSE, FALSE, 0);

	meta_check_title =
		gtk_check_button_new_with_label(_("Import title"));
	gtk_widget_set_name(meta_check_title, "check_on_notebook");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_title), TRUE);
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_title, FALSE, FALSE, 0);

	meta_check_artist =
		gtk_check_button_new_with_label(_("Import artist"));
	gtk_widget_set_name(meta_check_artist, "check_on_notebook");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_artist), TRUE);
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_artist, FALSE, FALSE, 0);

	meta_check_record =
		gtk_check_button_new_with_label(_("Import record"));
	gtk_widget_set_name(meta_check_record, "check_on_notebook");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_record), TRUE);
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_record, FALSE, FALSE, 0);


	/* CDDB */

        cddb_vbox = gtk_vbox_new(FALSE, 0);
	label = gtk_label_new(_("CDDB"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), cddb_vbox, label);

	cddb_check_enable =
		gtk_check_button_new_with_label(_("Perform CDDB lookup for records"));
	gtk_widget_set_name(cddb_check_enable, "check_on_notebook");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cddb_check_enable), TRUE);
        gtk_box_pack_start(GTK_BOX(cddb_vbox), cddb_check_enable, FALSE, FALSE, 5);

	g_signal_connect(G_OBJECT(cddb_check_enable), "toggled",
			 G_CALLBACK(cddb_check_enable_toggled), NULL);

	cddb_radio_batch = gtk_radio_button_new_with_label(NULL, _("Batch mode (I'm feeling lucky)"));
        gtk_box_pack_start(GTK_BOX(cddb_vbox), cddb_radio_batch, FALSE, FALSE, 0);

	cddb_radio_interactive = gtk_radio_button_new_with_label_from_widget(
			     GTK_RADIO_BUTTON(cddb_radio_batch), _("Interactive mode (let me choose if multiple matches found)"));
        gtk_box_pack_start(GTK_BOX(cddb_vbox), cddb_radio_interactive, FALSE, FALSE, 0);


	/* Filenames */

        fs_vbox = gtk_vbox_new(FALSE, 0);
	label = gtk_label_new(_("Filesystem"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), fs_vbox, label);

	fs_radio_preset = gtk_radio_button_new_with_label(NULL, _("Predefined transformations"));
        gtk_box_pack_start(GTK_BOX(fs_vbox), fs_radio_preset, FALSE, FALSE, 5);

	g_signal_connect(G_OBJECT(fs_radio_preset), "toggled",
			 G_CALLBACK(fs_radio_preset_toggled), NULL);

	fs_check_rm_number =
		gtk_check_button_new_with_label(_("Remove leading number"));
	gtk_widget_set_name(fs_check_rm_number, "check_on_notebook");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fs_check_rm_number), TRUE);
        gtk_box_pack_start(GTK_BOX(fs_vbox), fs_check_rm_number, FALSE, FALSE, 0);

	fs_check_rm_ext =
		gtk_check_button_new_with_label(_("Remove file extension"));
	gtk_widget_set_name(fs_check_rm_ext, "check_on_notebook");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fs_check_rm_ext), TRUE);
        gtk_box_pack_start(GTK_BOX(fs_vbox), fs_check_rm_ext, FALSE, FALSE, 0);

	fs_check_underscore =
		gtk_check_button_new_with_label(_("Convert underscore to space"));
	gtk_widget_set_name(fs_check_underscore, "check_on_notebook");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fs_check_underscore), TRUE);
        gtk_box_pack_start(GTK_BOX(fs_vbox), fs_check_underscore, FALSE, FALSE, 0);

	fs_radio_regexp = gtk_radio_button_new_with_label_from_widget(
			      GTK_RADIO_BUTTON(fs_radio_preset), _("Regular expression"));
        gtk_box_pack_start(GTK_BOX(fs_vbox), fs_radio_regexp, FALSE, FALSE, 5);


	table = gtk_table_new(2, 2, FALSE);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new("regexp:");
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 0, 5);

	fs_entry_regexp1 = gtk_entry_new();
	gtk_widget_set_sensitive(fs_entry_regexp1, FALSE);
	gtk_table_attach(GTK_TABLE(table), fs_entry_regexp1, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new("replacement:");
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 0, 5);

	fs_entry_regexp2 = gtk_entry_new();
	gtk_widget_set_sensitive(fs_entry_regexp2, FALSE);
	gtk_table_attach(GTK_TABLE(table), fs_entry_regexp2, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

        gtk_box_pack_start(GTK_BOX(fs_vbox), table, FALSE, FALSE, 5);


	/* run dialog */

	gtk_widget_show_all(dialog);

 display:

	root[0] = '\0';
	/*
	file[0] = '\0';
	comment[0] = '\0';
	*/

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		/*		
		GtkTextIter iter_start;
		GtkTextIter iter_end;
		*/
		//const char * pfile = gtk_entry_get_text(GTK_ENTRY(file_entry));
		const char * proot = gtk_entry_get_text(GTK_ENTRY(root_entry));
		/*
		if (pfile[0] == '~') {
			snprintf(file, MAXLEN-1, "%s%s", options.home, pfile + 1);
		} else if (pfile[0] == '/') {
			strncpy(file, pfile, MAXLEN-1);
		} else if (pfile[0] != '\0'){
			snprintf(file, MAXLEN-1, "%s/%s", options.cwd, pfile);
		}
		*/
		if (proot[0] == '~') {
			snprintf(root, MAXLEN-1, "%s%s", options.home, proot + 1);
		} else if (proot[0] == '/') {
			strncpy(root, proot, MAXLEN-1);
		} else if (proot[0] != '\0') {
			snprintf(root, MAXLEN-1, "%s/%s", options.cwd, proot);
		}
		/*
                strncpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)), MAXLEN-1);

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));
		*/
		if (/*(name[0] == '\0') || (file[0] == '\0') ||*/ (root[0] == '\0')) {
			goto display;
		}

		ret = 1;

        } else {
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


static int
filter(const struct dirent * de) {

	if (de->d_name[0] == '.') {
		return 0;
	}

	return 1;
}


int
is_dir(char * name) {

	struct stat st_file;

	if (stat(name, &st_file) == -1) {
		return 0;
	}

	return S_ISDIR(st_file.st_mode);
}


int
is_reg(char * name) {

	struct stat st_file;

	if (stat(name, &st_file) == -1) {
		return 0;
	}

	return S_ISREG(st_file.st_mode);
}


void
process_record(char * dir_record) {

	GtkTreeIter record_iter;

	int i;
	struct dirent ** ent_track;
	char basename[MAXLEN];
	char filename[MAXLEN];

	metadata * meta = meta_new();

	char artist[MAXLEN];
	char record[MAXLEN];

	int artist_is_set = 0;
	int record_is_set = 0;

	int num_tracks = 0;

	track_t * tracks = NULL;
	track_t * ptrack = NULL;
	track_t * last_track = NULL;
	float duration;

	for (i = 0; i < scandir(dir_record, &ent_track, filter, alphasort); i++) {

		strncpy(basename, ent_track[i]->d_name, MAXLEN-1);
		snprintf(filename, MAXLEN-1, "%s/%s", dir_record, ent_track[i]->d_name);

		if ((duration = get_file_duration(filename)) > 0.0f) {

			track_t * track;

			num_tracks++;

			if ((track = (track_t *)malloc(sizeof(track_t))) == NULL) {
				fprintf(stderr, "build_store.c: process_record(): malloc error\n");
				return;
			} else {
				track->duration = duration;
				strncpy(track->filename, filename, MAXLEN-1);
				track->valid = 0;
				track->next = NULL;
			}

			if (tracks == NULL) {
				tracks = last_track = track;
			} else {
				last_track->next = track;
				last_track = track;
			}

			printf(" |------ added file: %s\n", filename);
		}
	}

	if (tracks == NULL) {
		return;
	}

	for (ptrack = tracks; ptrack; ptrack = ptrack->next) {

		meta = meta_new();

		if (meta_read(meta, ptrack->filename)) {
			if (!artist_is_set && meta_get_artist(meta, artist)) {
				artist_is_set = 1;
			}
			if (!record_is_set && meta_get_record(meta, record)) {
				record_is_set = 1;
			}
			if (meta_get_title(meta, ptrack->name)) {
				ptrack->valid = 1;
			}
		}

		meta_free(meta);
	}

	if (artist_is_set && record_is_set &&
	    !get_iter_for_artist_and_record(&store_iter, &record_iter, artist, record)) {
		for (i = 0, ptrack = tracks; ptrack; i++, ptrack = ptrack->next) {

			GtkTreeIter iter;
			char sort_name[4];
			sprintf(sort_name, "%02d", i + 1);

			gtk_tree_store_append(music_store, &iter, &record_iter);
			gtk_tree_store_set(music_store, &iter, 0, ptrack->name, 1, sort_name,
					   2, ptrack->filename, 3, "",
					   4, ptrack->duration, 5, 1.0f, 7, -1.0f, -1);
		}
	}


	/* free tracklist */

	for (ptrack = tracks; ptrack; tracks = ptrack) {
		ptrack = tracks->next;
		free(tracks);
	}
}


void *
build_store_thread(void * arg) {

	int i, j;
	struct dirent ** ent_artist;
	struct dirent ** ent_record;
	char dir_artist[MAXLEN];
	char dir_record[MAXLEN];

	printf("--> build_store_thread\n");

	for (i = 0; i < scandir(root, &ent_artist, filter, alphasort); i++) {

		snprintf(dir_artist, MAXLEN-1, "%s/%s", root, ent_artist[i]->d_name);

		if (!is_dir(dir_artist)) {
			continue;
		}

		printf("[+] %s\n", ent_artist[i]->d_name);

		for (j = 0; j < scandir(dir_artist, &ent_record, filter, alphasort); j++) {

			snprintf(dir_record, MAXLEN-1, "%s/%s", dir_artist, ent_record[j]->d_name);

			if (!is_dir(dir_record)) {
				continue;
			}

			printf(" |--- %s\n", ent_record[j]->d_name);

			process_record(dir_record);
		}
	}

	printf("<-- build_store_thread\n");

	return NULL;
}

void
build_store(GtkTreeIter iter) {

	store_iter = iter;

	if (build_store_dialog()) {
		pthread_create(&build_store_thread_id, NULL, build_store_thread, NULL);
	}
}
