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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <regex.h>
#include <gtk/gtk.h>
#include <sys/stat.h>

#include "common.h"
#include "i18n.h"
#include "options.h"
#include "music_browser.h"
#include "meta_decoder.h"
#include "build_store.h"
#include "cddb_lookup.h"

#define BUILD_THREAD_BUSY 0
#define BUILD_THREAD_FREE 1

#define BUILD_STORE       0
#define BUILD_ARTIST      1

#define ARTIST_SORT_NAME  0
#define ARTIST_SORT_DIR   1

#define RECORD_SORT_NAME  0
#define RECORD_SORT_DIR   1
#define RECORD_SORT_YEAR  2

extern options_t options;
extern GtkTreeStore * music_store;

extern GtkWidget * browser_window;
extern GtkWidget * gui_stock_label_button(gchar * blabel, const gchar * bstock);
extern void set_sliders_width(void);


pthread_t build_thread_id;

/* 0: busy, 1: free to start new build */
int build_thread_state = BUILD_THREAD_FREE;

int build_cancelled = 0;

int build_type;

GtkTreeIter store_iter;
GtkTreeIter artist_iter;

GtkWidget * build_prog_window = NULL;
GtkWidget * prog_cancel_button;
GtkWidget * prog_file_entry;
GtkWidget * prog_action_label;

GtkWidget * meta_check_enable;
GtkWidget * meta_check_wspace;
GtkWidget * meta_check_title;
GtkWidget * meta_check_artist;
GtkWidget * meta_check_record;
GtkWidget * meta_check_rva;

#ifdef HAVE_CDDB
GtkWidget * cddb_check_enable;
GtkWidget * cddb_check_title;
GtkWidget * cddb_check_artist;
GtkWidget * cddb_check_record;
#endif /* HAVE_CDDB */

GtkWidget * fs_radio_preset;
GtkWidget * fs_radio_regexp;
GtkWidget * fs_check_rm_number;
GtkWidget * fs_check_rm_ext;
GtkWidget * fs_check_underscore;
GtkWidget * fs_entry_regexp1;
GtkWidget * fs_entry_regexp2;
GtkWidget * fs_label_regexp;
GtkWidget * fs_label_repl;
GtkWidget * fs_label_error;

int artist_sort_by = ARTIST_SORT_NAME;
int record_sort_by = RECORD_SORT_YEAR;
int reset_existing_data = 0;
int add_year_to_comment = 0;

int pri_meta_first = 1;

int meta_enable = 1;
int meta_wspace = 1;
int meta_title = 1;
int meta_artist = 1;
int meta_record = 1;
int meta_rva = 1;

int cddb_enable = 1;
int cddb_title = 1;
int cddb_artist = 1;
int cddb_record = 1;

int fs_preset = 1;
int fs_rm_number = 1;
int fs_rm_ext = 1;
int fs_underscore = 1;

char fs_regexp[MAXLEN];
char fs_replacement[MAXLEN];
regex_t fs_compiled;

char root[MAXLEN];


map_t *
map_new(char * str) {

	map_t * map;

	if ((map = (map_t *)malloc(sizeof(map_t))) == NULL) {
		fprintf(stderr, "build_store.c: map_new(): malloc error\n");
		return NULL;
	}

	strncpy(map->str, str, MAXLEN-1);
	map->count = 1;
	map->next = NULL;

	return map;
}

void
map_put(map_t ** map, char * str) {

	map_t * pmap;
	map_t * _pmap;

	if (*map == NULL) {
		*map = map_new(str);
	} else {

		for (_pmap = pmap = *map; pmap; _pmap = pmap, pmap = pmap->next) {

			char * key1 = g_utf8_casefold(str, -1);
			char * key2 = g_utf8_casefold(pmap->str, -1);

			if (!g_utf8_collate(key1, key2)) {
				pmap->count++;
				g_free(key1);
				g_free(key2);
				return;
			}

			g_free(key1);
			g_free(key2);
		}

		_pmap->next = map_new(str);
	}
}

char *
map_get_max(map_t * map) {

	map_t * pmap;
	int max = 0;
	char * str = NULL;

	for (pmap = map; pmap; pmap = pmap->next) {
		if (max < pmap->count) {
			str = pmap->str;
			max = pmap->count;
		}
	}

	return str;
}

void
map_free(map_t * map) {

	map_t * pmap;

	for (pmap = map; pmap; map = pmap) {
		pmap = map->next;
		free(map);
	}
}


void
meta_check_enable_toggled(GtkWidget * widget, gpointer * data) {

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_enable));

	gtk_widget_set_sensitive(meta_check_wspace, state);
	gtk_widget_set_sensitive(meta_check_title, state);
	gtk_widget_set_sensitive(meta_check_artist, state);
	gtk_widget_set_sensitive(meta_check_record, state);
	gtk_widget_set_sensitive(meta_check_rva, state);
}


#ifdef HAVE_CDDB
void
cddb_check_enable_toggled(GtkWidget * widget, gpointer * data) {

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cddb_check_enable));

	gtk_widget_set_sensitive(cddb_check_title, state);
	gtk_widget_set_sensitive(cddb_check_artist, state);
	gtk_widget_set_sensitive(cddb_check_record, state);
}
#endif /* HAVE_CDDB */


void
fs_radio_preset_toggled(GtkWidget * widget, gpointer * data) {

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fs_radio_preset));

	gtk_widget_set_sensitive(fs_check_rm_number, state);
	gtk_widget_set_sensitive(fs_check_rm_ext, state);
	gtk_widget_set_sensitive(fs_check_underscore, state);

	gtk_widget_set_sensitive(fs_entry_regexp1, !state);
	gtk_widget_set_sensitive(fs_entry_regexp2, !state);
	gtk_widget_set_sensitive(fs_label_regexp, !state);
	gtk_widget_set_sensitive(fs_label_repl, !state);

	if (state) {
		gtk_widget_hide(fs_label_error);
	} else {
		gtk_widget_show(fs_label_error);
	}
}


int
browse_button_clicked(GtkWidget * widget, gpointer * data) {

        GtkWidget * dialog;
	const gchar * selected_filename = gtk_entry_get_text(GTK_ENTRY(data));


        dialog = gtk_file_chooser_dialog_new(_("Please select the root directory."),
                                             GTK_WINDOW(browser_window),
					     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                             GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), options.show_hidden);
	} 

        set_sliders_width();    /* MAGIC */

        if (strlen(selected_filename)) {
      		char * locale = g_locale_from_utf8(selected_filename, -1, NULL, NULL, NULL);
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), locale);
		g_free(locale);
	} else {
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
	}

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);


        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * utf8;

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		utf8 = g_locale_to_utf8(selected_filename, -1, NULL, NULL, NULL);
		gtk_entry_set_text(GTK_ENTRY(data), utf8);

                strncpy(options.currdir, selected_filename, MAXLEN-1);
		g_free(utf8);
        }


        gtk_widget_destroy(dialog);

        set_sliders_width();    /* MAGIC */

	return 0;
}


int
build_dialog(void) {

	GtkWidget * dialog;
	GtkWidget * notebook;
	GtkWidget * label;
	GtkWidget * table;
	GtkWidget * hbox;

	GtkWidget * root_entry;
	GtkWidget * browse_button;

	GtkWidget * gen_vbox;
	GtkWidget * gen_path_frame = NULL;
	GtkWidget * gen_artist_frame;
	GtkWidget * gen_artist_vbox;
	GtkWidget * gen_radio_artist_name;
	GtkWidget * gen_radio_artist_dir;
	GtkWidget * gen_record_frame;
	GtkWidget * gen_record_vbox;
	GtkWidget * gen_radio_record_name;
	GtkWidget * gen_radio_record_dir;
	GtkWidget * gen_radio_record_year;
	GtkWidget * gen_check_reset_data;
	GtkWidget * gen_check_add_year;

	GtkWidget * meta_vbox;

	GtkWidget * fs_vbox;

#ifdef HAVE_CDDB
	GtkWidget * gen_pri_frame;
	GtkWidget * gen_pri_vbox;
	GtkWidget * pri_radio_meta;
	GtkWidget * pri_radio_cddb;
	GtkWidget * cddb_vbox;
#endif /* HAVE_CDDB */

	GdkColor red = { 0, 50000, 0, 0 };

	char * title = NULL;
        int ret;

	if (build_type == BUILD_STORE) {
		title = _("Build store");
	} else if (build_type == BUILD_ARTIST) {
		title = _("Build artist");
	}

        dialog = gtk_dialog_new_with_buttons(title,
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), notebook);


	/* General */

        gen_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(gen_vbox), 5);
	label = gtk_label_new(_("General"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gen_vbox, label);

	if (build_type == BUILD_STORE) {
		gen_path_frame = gtk_frame_new(_("Root path"));
	} else if (build_type == BUILD_ARTIST) {
		gen_path_frame = gtk_frame_new(_("Artist path"));
	}
        gtk_box_pack_start(GTK_BOX(gen_vbox), gen_path_frame, FALSE, FALSE, 5);

        hbox = gtk_hbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
	gtk_container_add(GTK_CONTAINER(gen_path_frame), hbox);

        root_entry = gtk_entry_new_with_max_length(MAXLEN-1);
        gtk_box_pack_start(GTK_BOX(hbox), root_entry, TRUE, TRUE, 5);

	browse_button = gui_stock_label_button(_("_Browse..."), GTK_STOCK_OPEN);
        gtk_box_pack_start(GTK_BOX(hbox), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked", G_CALLBACK(browse_button_clicked),
			 (gpointer *)root_entry);

#ifdef HAVE_CDDB
	gen_pri_frame = gtk_frame_new(_("Data source priority"));
        gtk_box_pack_start(GTK_BOX(gen_vbox), gen_pri_frame, FALSE, FALSE, 5);

        gen_pri_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(gen_pri_vbox), 5);
	gtk_container_add(GTK_CONTAINER(gen_pri_frame), gen_pri_vbox);

	pri_radio_meta = gtk_radio_button_new_with_label(NULL, _("Metadata first, then CDDB"));
	gtk_widget_set_name(pri_radio_meta, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_pri_vbox), pri_radio_meta, FALSE, FALSE, 0);


	pri_radio_cddb = gtk_radio_button_new_with_label_from_widget(
		       GTK_RADIO_BUTTON(pri_radio_meta), _("CDDB first, then metadata"));
	gtk_widget_set_name(pri_radio_cddb, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_pri_vbox), pri_radio_cddb, FALSE, FALSE, 0);
#endif /* HAVE_CDDB */


	gen_artist_frame = gtk_frame_new(_("Sort artists by"));
        gtk_box_pack_start(GTK_BOX(gen_vbox), gen_artist_frame, FALSE, FALSE, 5);

        gen_artist_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(gen_artist_vbox), 5);
	gtk_container_add(GTK_CONTAINER(gen_artist_frame), gen_artist_vbox);

	gen_radio_artist_name = gtk_radio_button_new_with_label(NULL, _("Artist name"));
	gtk_widget_set_name(gen_radio_artist_name, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_artist_vbox), gen_radio_artist_name, FALSE, FALSE, 0);

	gen_radio_artist_dir = gtk_radio_button_new_with_label_from_widget(
			   GTK_RADIO_BUTTON(gen_radio_artist_name), _("Directory name"));
	gtk_widget_set_name(gen_radio_artist_dir, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_artist_vbox), gen_radio_artist_dir, FALSE, FALSE, 0);

	switch (artist_sort_by) {
	case ARTIST_SORT_NAME:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gen_radio_artist_name), TRUE);
		break;
	case ARTIST_SORT_DIR:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gen_radio_artist_dir), TRUE);
		break;
	}


	gen_record_frame = gtk_frame_new(_("Sort records by"));
        gtk_box_pack_start(GTK_BOX(gen_vbox), gen_record_frame, FALSE, FALSE, 5);

        gen_record_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(gen_record_vbox), 5);
	gtk_container_add(GTK_CONTAINER(gen_record_frame), gen_record_vbox);

	gen_radio_record_name = gtk_radio_button_new_with_label(NULL, _("Record name"));
	gtk_widget_set_name(gen_radio_record_name, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_record_vbox), gen_radio_record_name, FALSE, FALSE, 0);

	gen_radio_record_dir = gtk_radio_button_new_with_label_from_widget(
			   GTK_RADIO_BUTTON(gen_radio_record_name), _("Directory name"));
	gtk_widget_set_name(gen_radio_record_dir, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_record_vbox), gen_radio_record_dir, FALSE, FALSE, 0);

	gen_radio_record_year = gtk_radio_button_new_with_label_from_widget(
			   GTK_RADIO_BUTTON(gen_radio_record_name), _("Year"));
	gtk_widget_set_name(gen_radio_record_year, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_record_vbox), gen_radio_record_year, FALSE, FALSE, 0);

	switch (record_sort_by) {
	case RECORD_SORT_NAME:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gen_radio_record_name), TRUE);
		break;
	case RECORD_SORT_DIR:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gen_radio_record_dir), TRUE);
		break;
	case RECORD_SORT_YEAR:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gen_radio_record_year), TRUE);
		break;
	}


	gen_check_add_year =
		gtk_check_button_new_with_label(_("Add year to the comments of new records"));
	gtk_widget_set_name(gen_check_add_year, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_vbox), gen_check_add_year, FALSE, FALSE, 0);

	if (add_year_to_comment) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gen_check_add_year), TRUE);
	}


	gen_check_reset_data =
		gtk_check_button_new_with_label(_("Re-read data for existing tracks"));
	gtk_widget_set_name(gen_check_reset_data, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_vbox), gen_check_reset_data, FALSE, FALSE, 0);

	if (reset_existing_data) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gen_check_reset_data), TRUE);
	}


	/* Metadata */

        meta_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(meta_vbox), 5);
	label = gtk_label_new(_("Metadata"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), meta_vbox, label);

	meta_check_enable =
		gtk_check_button_new_with_label(_("Import file metadata"));
	gtk_widget_set_name(meta_check_enable, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_enable, FALSE, FALSE, 5);

	if (meta_enable) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_enable), TRUE);
	}

	g_signal_connect(G_OBJECT(meta_check_enable), "toggled",
			 G_CALLBACK(meta_check_enable_toggled), NULL);

	meta_check_title =
		gtk_check_button_new_with_label(_("Import track titles"));
	gtk_widget_set_name(meta_check_title, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_title, FALSE, FALSE, 0);

	if (meta_title) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_title), TRUE);
	}

	meta_check_artist =
		gtk_check_button_new_with_label(_("Import artist"));
	gtk_widget_set_name(meta_check_artist, "check_on_notebook");

	if (build_type == BUILD_STORE) {
		gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_artist, FALSE, FALSE, 0);
	}

	if (meta_artist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_artist), TRUE);
	}

	meta_check_record =
		gtk_check_button_new_with_label(_("Import record"));
	gtk_widget_set_name(meta_check_record, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_record, FALSE, FALSE, 0);

	if (meta_record) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_record), TRUE);
	}

	meta_check_rva =
		gtk_check_button_new_with_label(_("Import RVA"));
	gtk_widget_set_name(meta_check_rva, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_rva, FALSE, FALSE, 0);

	if (meta_rva) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_rva), TRUE);
	}

	meta_check_wspace =
		gtk_check_button_new_with_label(_("Exclude whitespace-only metadata"));
	gtk_widget_set_name(meta_check_wspace, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_wspace, FALSE, FALSE, 0);

	if (meta_wspace) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_wspace), TRUE);
	}

	if (!meta_enable) {
		gtk_widget_set_sensitive(meta_check_title, FALSE);
		gtk_widget_set_sensitive(meta_check_artist, FALSE);
		gtk_widget_set_sensitive(meta_check_record, FALSE);
		gtk_widget_set_sensitive(meta_check_rva, FALSE);
		gtk_widget_set_sensitive(meta_check_wspace, FALSE);
	}


	/* CDDB */

#ifdef HAVE_CDDB
        cddb_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(cddb_vbox), 5);
	label = gtk_label_new(_("CDDB"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), cddb_vbox, label);

	cddb_check_enable =
		gtk_check_button_new_with_label(_("Perform CDDB lookup for records"));
	gtk_widget_set_name(cddb_check_enable, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(cddb_vbox), cddb_check_enable, FALSE, FALSE, 5);

	if (cddb_enable) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cddb_check_enable), TRUE);
	}

	g_signal_connect(G_OBJECT(cddb_check_enable), "toggled",
			 G_CALLBACK(cddb_check_enable_toggled), NULL);


	cddb_check_title =
		gtk_check_button_new_with_label(_("Import track titles"));
	gtk_widget_set_name(cddb_check_title, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(cddb_vbox), cddb_check_title, FALSE, FALSE, 0);

	if (cddb_title) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cddb_check_title), TRUE);
	}

	cddb_check_artist =
		gtk_check_button_new_with_label(_("Import artist"));
	gtk_widget_set_name(cddb_check_artist, "check_on_notebook");

	if (build_type == BUILD_STORE) {
		gtk_box_pack_start(GTK_BOX(cddb_vbox), cddb_check_artist, FALSE, FALSE, 0);
	}

	if (cddb_artist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cddb_check_artist), TRUE);
	}

	cddb_check_record =
		gtk_check_button_new_with_label(_("Import record"));
	gtk_widget_set_name(cddb_check_record, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(cddb_vbox), cddb_check_record, FALSE, FALSE, 0);

	if (cddb_record) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cddb_check_record), TRUE);
	}

	if (!cddb_enable) {
		gtk_widget_set_sensitive(cddb_check_title, FALSE);
		gtk_widget_set_sensitive(cddb_check_artist, FALSE);
		gtk_widget_set_sensitive(cddb_check_record, FALSE);
	}

#endif /* HAVE_CDDB */

	/* Filenames */

        fs_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(fs_vbox), 5);
	label = gtk_label_new(_("Filesystem"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), fs_vbox, label);

	fs_radio_preset = gtk_radio_button_new_with_label(NULL, _("Predefined transformations"));
	gtk_widget_set_name(fs_radio_preset, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(fs_vbox), fs_radio_preset, FALSE, FALSE, 5);

	if (fs_preset) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fs_radio_preset), TRUE);
	}

	g_signal_connect(G_OBJECT(fs_radio_preset), "toggled",
			 G_CALLBACK(fs_radio_preset_toggled), NULL);

	fs_check_rm_number =
		gtk_check_button_new_with_label(_("Remove leading number"));
	gtk_widget_set_name(fs_check_rm_number, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(fs_vbox), fs_check_rm_number, FALSE, FALSE, 0);

	if (fs_rm_number) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fs_check_rm_number), TRUE);
	}

	fs_check_rm_ext =
		gtk_check_button_new_with_label(_("Remove file extension"));
	gtk_widget_set_name(fs_check_rm_ext, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(fs_vbox), fs_check_rm_ext, FALSE, FALSE, 0);

	if (fs_rm_ext) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fs_check_rm_ext), TRUE);
	}

	fs_check_underscore =
		gtk_check_button_new_with_label(_("Convert underscore to space"));
	gtk_widget_set_name(fs_check_underscore, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(fs_vbox), fs_check_underscore, FALSE, FALSE, 0);

	if (fs_underscore) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fs_check_underscore), TRUE);
	}

	fs_radio_regexp = gtk_radio_button_new_with_label_from_widget(
			      GTK_RADIO_BUTTON(fs_radio_preset), _("Regular expression"));
	gtk_widget_set_name(fs_radio_regexp, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(fs_vbox), fs_radio_regexp, FALSE, FALSE, 5);


	table = gtk_table_new(2, 2, FALSE);

        hbox = gtk_hbox_new(FALSE, 0);
	fs_label_regexp = gtk_label_new(_("regexp:"));
	gtk_widget_set_sensitive(fs_label_regexp, FALSE);
        gtk_box_pack_start(GTK_BOX(hbox), fs_label_regexp, FALSE, FALSE, 5);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 0, 5);

	fs_entry_regexp1 = gtk_entry_new();
	gtk_widget_set_sensitive(fs_entry_regexp1, FALSE);
	gtk_table_attach(GTK_TABLE(table), fs_entry_regexp1, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	fs_label_repl = gtk_label_new(_("replacement:"));
	gtk_widget_set_sensitive(fs_label_repl, FALSE);
        gtk_box_pack_start(GTK_BOX(hbox), fs_label_repl, FALSE, FALSE, 5);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 0, 5);

	fs_entry_regexp2 = gtk_entry_new();
	gtk_widget_set_sensitive(fs_entry_regexp2, FALSE);
	gtk_table_attach(GTK_TABLE(table), fs_entry_regexp2, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

        gtk_box_pack_start(GTK_BOX(fs_vbox), table, FALSE, FALSE, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	fs_label_error = gtk_label_new("");
	gtk_widget_modify_fg(fs_label_error, GTK_STATE_NORMAL, &red);
        gtk_box_pack_start(GTK_BOX(hbox), fs_label_error, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(fs_vbox), hbox, FALSE, FALSE, 5);


	/* run dialog */

	gtk_widget_show_all(dialog);

 display:

	root[0] = '\0';

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * proot = g_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(root_entry)), -1, NULL, NULL, NULL);

		gtk_label_set_text(GTK_LABEL(fs_label_error), "");

		if (proot[0] == '~') {
			snprintf(root, MAXLEN-1, "%s%s", options.home, proot + 1);
		} else if (proot[0] == '/') {
			strncpy(root, proot, MAXLEN-1);
		} else if (proot[0] != '\0') {
			snprintf(root, MAXLEN-1, "%s/%s", options.cwd, proot);
		}

		g_free(proot);

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gen_radio_artist_name))) {
			artist_sort_by = ARTIST_SORT_NAME;
		}

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gen_radio_artist_dir))) {
			artist_sort_by = ARTIST_SORT_DIR;
		}


		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gen_radio_record_name))) {
			record_sort_by = RECORD_SORT_NAME;
		}

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gen_radio_record_dir))) {
			record_sort_by = RECORD_SORT_DIR;
		}

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gen_radio_record_year))) {
			record_sort_by = RECORD_SORT_YEAR;
		}

		add_year_to_comment = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gen_check_add_year));
		reset_existing_data = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gen_check_reset_data));

		meta_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_enable));
		meta_wspace = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_wspace));
		meta_title = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_title));
		meta_artist = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_artist));
		meta_record = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_record));
		meta_rva = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_rva));

#ifdef HAVE_CDDB
		pri_meta_first = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pri_radio_meta));
		cddb_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cddb_check_enable));
		cddb_title = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cddb_check_title));
		cddb_artist = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cddb_check_artist));
		cddb_record = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cddb_check_record));
#endif /* HAVE_CDDB */

		fs_preset = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fs_radio_preset));
		fs_rm_number = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fs_check_rm_number));
		fs_rm_ext = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fs_check_rm_ext));
		fs_underscore = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fs_check_underscore));

		strncpy(fs_regexp, gtk_entry_get_text(GTK_ENTRY(fs_entry_regexp1)), MAXLEN - 1);
		strncpy(fs_replacement, gtk_entry_get_text(GTK_ENTRY(fs_entry_regexp2)), MAXLEN - 1);

		if (!fs_preset) {

			int err;

			if ((err = regcomp(&fs_compiled, fs_regexp, REG_EXTENDED))) {

				char msg[MAXLEN];
				char * utf8;

				regerror(err, &fs_compiled, msg, MAXLEN);
				utf8 = g_locale_to_utf8(msg, -1, NULL, NULL, NULL);
				gtk_label_set_text(GTK_LABEL(fs_label_error), utf8);
				g_free(utf8);

				goto display;
			}

			if (!regexec(&fs_compiled, "", 0, NULL, 0)) {
				gtk_label_set_text(GTK_LABEL(fs_label_error),
						   _("Regexp matches empty string"));
				goto display;
			}
		}

		if (root[0] == '\0') {
			goto display;
		}

		ret = 1;

        } else {
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


void
prog_window_close(GtkWidget * widget, gpointer data) {

	build_cancelled = 1;
	gtk_widget_destroy(build_prog_window);
	build_prog_window = NULL;
}

void
cancel_build(GtkWidget * widget, gpointer data) {

	build_cancelled = 1;
	gtk_widget_destroy(build_prog_window);
	build_prog_window = NULL;
}

gboolean
finish_build(gpointer data) {

	gtk_widget_destroy(build_prog_window);
	build_prog_window = NULL;

	build_thread_state = BUILD_THREAD_FREE;

	return FALSE;
}


void
progress_window(void) {

	GtkWidget * table;
	GtkWidget * label;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * hbuttonbox;
	GtkWidget * hseparator;

	char * title = NULL;

	if (build_type == BUILD_STORE) {
		title = _("Building store from filesystem");
	} else if (build_type == BUILD_ARTIST) {
		title = _("Building artist from filesystem");
	}

	build_prog_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(build_prog_window), title);
        gtk_window_set_position(GTK_WINDOW(build_prog_window), GTK_WIN_POS_CENTER);
        gtk_window_resize(GTK_WINDOW(build_prog_window), 430, 110);
        g_signal_connect(G_OBJECT(build_prog_window), "delete_event",
                         G_CALLBACK(prog_window_close), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(build_prog_window), 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(build_prog_window), vbox);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Directory:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 5, 5);

        prog_file_entry = gtk_entry_new();
        gtk_editable_set_editable(GTK_EDITABLE(prog_file_entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), prog_file_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Action:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	prog_action_label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), prog_action_label, FALSE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 5);

        hseparator = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, TRUE, 5);

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        prog_cancel_button = gui_stock_label_button (_("Abort"), GTK_STOCK_CANCEL); 
        g_signal_connect(prog_cancel_button, "clicked", G_CALLBACK(cancel_build), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), prog_cancel_button);   

        gtk_widget_grab_focus(prog_cancel_button);

        gtk_widget_show_all(build_prog_window);
}

gboolean
set_prog_file_entry(gpointer data) {

	if (build_prog_window) {

		char * utf8 = g_filename_display_name((char *)data);

		gtk_entry_set_text(GTK_ENTRY(prog_file_entry), utf8);
		gtk_widget_grab_focus(prog_cancel_button);
		g_free(utf8);
	}

	return FALSE;
}


gboolean
set_prog_action_label(gpointer data) {

	if (build_prog_window) {
		gtk_label_set_text(GTK_LABEL(prog_action_label), (char *)data);
	}

	return FALSE;
}


static int
filter(const struct dirent * de) {

	return de->d_name[0] != '.';
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


int
is_wspace(char * str) {

	int i;

	for (i = 0; str[i]; i++) {
		if (str[i] != ' ' || str[i] != '\t') {
			return 0;
		}
	}

	return 1;
}

int
num_invalid_tracks(build_record_t * record) {

	int invalid = 0;
	build_track_t * ptrack;

	for (ptrack = record->tracks; ptrack; ptrack = ptrack->next) {
		if (!ptrack->valid) {
			invalid++;
		}
	}

	return invalid;
}

void
transform_filename(char * dest, char * src) {

	if (fs_preset) {

		int i;
		char tmp[MAXLEN];
		char * ptmp = tmp;
		
		strncpy(tmp, src, MAXLEN-1);

		if (fs_rm_number) {
			while (*ptmp && (isdigit(*ptmp) || *ptmp == ' ' ||
					 *ptmp == '_' || *ptmp == '-')) {
				ptmp++;
			}
		}

		if (fs_rm_ext) {
			for (i = strlen(ptmp) - 1; ptmp[i] != '.'; i--);
			ptmp[i] = '\0';
		}

		if (fs_underscore) {
			for (i = 0; ptmp[i]; i++) {
				if (ptmp[i] == '_') {
					ptmp[i] = ' ';
				}
			}
		}

		strncpy(dest, ptmp, MAXLEN-1);

	} else {

		int offs = 0;
		regmatch_t regmatch[10];

		dest[0] = '\0';

		while (!regexec(&fs_compiled, src + offs, 10, regmatch, 0)) {
		
			int i = 0;
			int b = strlen(fs_replacement) - 1;

			strncat(dest, src + offs, regmatch[0].rm_so);

			for (i = 0; i < b; i++) {

				if (fs_replacement[i] == '\\' && isdigit(fs_replacement[i+1])) {

					int j = fs_replacement[i+1] - '0';
				
					if (j == 0 || j > fs_compiled.re_nsub) {
						i++;
						continue;
					}
				
					strncat(dest, src + offs + regmatch[j].rm_so,
						regmatch[j].rm_eo - regmatch[j].rm_so);
					i++;
				} else {
					strncat(dest, fs_replacement + i, 1);
				}
			}
			
			strncat(dest, fs_replacement + i, 1);
			offs += regmatch[0].rm_eo;
		}

		if (!*dest) {
			strncpy(dest, src + offs, MAXLEN-1);
		} else {
			strcat(dest, src + offs);
		}
	}
}


void
process_meta(build_record_t * record) {

	build_track_t * ptrack;
	metadata * meta = meta_new();

	map_t * map_artist = NULL;
	map_t * map_record = NULL;
	map_t * map_year = NULL;

	char tmp[MAXLEN];

	for (ptrack = record->tracks; ptrack; ptrack = ptrack->next) {

		meta = meta_new();

		if (meta_read(meta, ptrack->filename)) {
			if (meta_artist &&
			    !record->artist_valid && meta_get_artist(meta, tmp)) {
				if (!meta_wspace || !is_wspace(tmp)) {
					map_put(&map_artist, tmp);
				}
			}
			if (meta_record &&
			    !record->record_valid && meta_get_record(meta, tmp)) {
				if (!meta_wspace || !is_wspace(tmp)) {
					map_put(&map_record, tmp);
				}
			}
			if (meta_title &&
			    !ptrack->valid && meta_get_title(meta, ptrack->name)) {
				if (!meta_wspace || !is_wspace(ptrack->name)) {
					ptrack->valid = 1;
				}
			}
			if (!record->year_valid && meta_get_year(meta, tmp)) {
				if (!meta_wspace || !is_wspace(tmp)) {
					map_put(&map_year, tmp);
				}
			}
			if (meta_rva) {
				meta_get_rva(meta, &ptrack->rva);
			}
		}

		meta_free(meta);
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

	map_free(map_artist);
	map_free(map_record);
	map_free(map_year);
}


void
add_new_track(GtkTreeIter * record_iter, build_track_t * ptrack, int i) {

	GtkTreeIter iter;
	char sort_name[3];

	snprintf(sort_name, 3, "%02d", i + 1);

	gtk_tree_store_append(music_store, &iter, record_iter);
	gtk_tree_store_set(music_store, &iter,
			   0, ptrack->name,
			   1, sort_name,
			   2, ptrack->filename,
			   3, "",
			   4, ptrack->duration,
			   5, ptrack->rva,
			   7, -1.0f,
			   -1);
}


void
process_record(char * dir_record, char * artist_d_name, char * record_d_name) {

	GtkTreeIter record_iter;
	int result = 0;

	int i;
	struct dirent ** ent_track;
	char basename[MAXLEN];
	char filename[MAXLEN];

	build_record_t record;

	int num_tracks = 0;

	build_track_t * ptrack = NULL;
	build_track_t * last_track = NULL;
	float duration;

	char * artist_sort_name = "";
	char * record_sort_name = "";
	char * record_comment = "";

	record.artist[0] = '\0';
	record.record[0] = '\0';
	record.year[0] = '\0';
	record.artist_valid = 0;
	record.record_valid = 0;
	record.year_valid = 0;
	record.tracks = NULL;

	g_idle_add(set_prog_action_label, (gpointer) _("Scanning files"));

	for (i = 0; i < scandir(dir_record, &ent_track, filter, alphasort); i++) {

		strncpy(basename, ent_track[i]->d_name, MAXLEN-1);
		snprintf(filename, MAXLEN-1, "%s/%s", dir_record, ent_track[i]->d_name);

		if ((duration = get_file_duration(filename)) > 0.0f) {

			build_track_t * track;

			if ((track = (build_track_t *)malloc(sizeof(build_track_t))) == NULL) {
				fprintf(stderr, "build_store.c: process_record(): malloc error\n");
				return;
			} else {
				strncpy(track->d_name, ent_track[i]->d_name, MAXLEN-1);
				strncpy(track->filename, filename, MAXLEN-1);
				track->duration = duration;
				track->rva = 1.0f;
				track->valid = 0;
				track->next = NULL;

				num_tracks++;
			}

			if (record.tracks == NULL) {
				record.tracks = last_track = track;
			} else {
				last_track->next = track;
				last_track = track;
			}
		}
	}

	if (record.tracks == NULL) {
		return;
	}

	/* metadata and cddb */

	if (pri_meta_first) {
		if (meta_enable) {
			g_idle_add(set_prog_action_label, (gpointer) _("Processing metadata"));
			process_meta(&record);
		}
	} else {
#ifdef HAVE_CDDB
		if (cddb_enable) {
			g_idle_add(set_prog_action_label, (gpointer) _("CDDB lookup"));
			cddb_get_batch(&record, cddb_title, cddb_artist, cddb_record);
		}
#endif /* HAVE_CDDB */
	}

	if (record.artist_valid && record.record_valid && num_invalid_tracks(&record) == 0 &&
	    (pri_meta_first || !meta_rva) &&
	    (record.year_valid || (record_sort_by != RECORD_SORT_YEAR && !add_year_to_comment))) {
		goto finish;
	}

	if (!pri_meta_first) {
		if (meta_enable) {
			g_idle_add(set_prog_action_label, (gpointer) _("Processing metadata"));
			process_meta(&record);
		}
	} else {
#ifdef HAVE_CDDB
		if (cddb_enable) {
			g_idle_add(set_prog_action_label, (gpointer) _("CDDB lookup"));
			cddb_get_batch(&record, cddb_title, cddb_artist, cddb_record);
		}
#endif /* HAVE_CDDB */
	}

	if (record.artist_valid && record.record_valid && num_invalid_tracks(&record) == 0) {
		goto finish;
	}

	/* filesystem */

	g_idle_add(set_prog_action_label, (gpointer) _("File name transformation"));

	if (!record.artist_valid) {
		char * utf8 = g_filename_display_name(artist_d_name);
		strncpy(record.artist, utf8, MAXLEN-1);
		record.artist_valid = 1;
		g_free(utf8);
	}

	if (!record.record_valid) {
		char * utf8 = g_filename_display_name(record_d_name);
		strncpy(record.record, utf8, MAXLEN-1);
		record.record_valid = 1;
		g_free(utf8);
	}

	for (i = 0, ptrack = record.tracks; ptrack; i++, ptrack = ptrack->next) {

		if (!ptrack->valid) {
			char * utf8 = g_filename_display_name(ptrack->d_name);
			transform_filename(ptrack->name, utf8);
			ptrack->valid = 1;
			g_free(utf8);
		}
	}


	/* add tracks to music store */

 finish:

	music_store_mark_changed();

	switch (artist_sort_by) {
	case ARTIST_SORT_NAME:
		artist_sort_name = record.artist;
		break;
	case ARTIST_SORT_DIR:
		artist_sort_name = artist_d_name;
		break;
	}

	switch (record_sort_by) {
	case RECORD_SORT_NAME:
		record_sort_name = record.record;
		break;
	case RECORD_SORT_DIR:
		record_sort_name = record_d_name;
		break;
	case RECORD_SORT_YEAR:
		record_sort_name = record.year;
		break;
	}

	if (add_year_to_comment) {
		record_comment = record.year;
	}

	if (build_type == BUILD_STORE) {
		result = store_get_iter_for_artist_and_record(&store_iter, &record_iter,
							      record.artist, artist_sort_name,
							      record.record, record_sort_name,
							      record_comment);
	} else if (build_type == BUILD_ARTIST) {
		result = artist_get_iter_for_record(&artist_iter, &record_iter,
						    record.record, record_sort_name,
						    record_comment);
	}

	if (result == 0) {
		for (i = 0, ptrack = record.tracks; ptrack; i++, ptrack = ptrack->next) {
			add_new_track(&record_iter, ptrack, i);
		}
	}

	if (result == 1) {
		for (i = 0, ptrack = record.tracks; ptrack; i++, ptrack = ptrack->next) {

			GtkTreeIter iter;
			int has = 0;
			int j = 0;

			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
							     &iter, &record_iter, j++)) {
				char * file = NULL;

				gtk_tree_model_get(GTK_TREE_MODEL(music_store),
						   &iter, 2, &file, -1);

				if (!strcmp(file, ptrack->filename)) {
					has = 1;
					g_free(file);
					break;
				}

				g_free(file);
			}

			if (has) {
				if (reset_existing_data) {
					if (ptrack->rva <= 0.1f) {
						gtk_tree_store_set(music_store, &iter,
								   0, ptrack->name,
								   4, ptrack->duration,
								   5, ptrack->rva,
								   -1);
					} else {
						gtk_tree_store_set(music_store, &iter,
								   0, ptrack->name,
								   4, ptrack->duration,
								   -1);
					}
				}
			} else {
				add_new_track(&record_iter, ptrack, i);
			}
		}
	}

	/* free tracklist */

	for (ptrack = record.tracks; ptrack; record.tracks = ptrack) {
		ptrack = record.tracks->next;
		free(record.tracks);
	}
}


void *
build_artist_thread(void * arg) {

	int i;
	struct dirent ** ent_record;
	char dir_record[MAXLEN];
	char artist_d_name[MAXLEN];

	i = strlen(root);
	while (root[--i] != '/');
	strncpy(artist_d_name, root + i + 1, MAXLEN-1);

	for (i = 0; i < scandir(root, &ent_record, filter, alphasort); i++) {

		snprintf(dir_record, MAXLEN-1, "%s/%s", root, ent_record[i]->d_name);
		
		if (!is_dir(dir_record)) {
			continue;
		}

		if (build_cancelled) {
			build_thread_state = BUILD_THREAD_FREE;
			return NULL;
		}

		g_idle_add(set_prog_file_entry, (gpointer)dir_record);

		process_record(dir_record, artist_d_name, ent_record[i]->d_name);
	}

	g_idle_add(finish_build, NULL);

	return NULL;
}

void *
build_store_thread(void * arg) {

	int i, j;
	struct dirent ** ent_artist;
	struct dirent ** ent_record;
	char dir_artist[MAXLEN];
	char dir_record[MAXLEN];

	for (i = 0; i < scandir(root, &ent_artist, filter, alphasort); i++) {

		snprintf(dir_artist, MAXLEN-1, "%s/%s", root, ent_artist[i]->d_name);

		if (!is_dir(dir_artist)) {
			continue;
		}

		for (j = 0; j < scandir(dir_artist, &ent_record, filter, alphasort); j++) {

			snprintf(dir_record, MAXLEN-1, "%s/%s", dir_artist, ent_record[j]->d_name);

			if (!is_dir(dir_record)) {
				continue;
			}

			if (build_cancelled) {
				build_thread_state = BUILD_THREAD_FREE;
				return NULL;
			}

			g_idle_add(set_prog_file_entry, (gpointer)dir_record);

			process_record(dir_record, ent_artist[i]->d_name, ent_record[j]->d_name);
		}
	}

	g_idle_add(finish_build, NULL);

	return NULL;
}

void
build_artist(GtkTreeIter iter) {

	build_thread_state = BUILD_THREAD_BUSY;

	artist_iter = iter;
	build_cancelled = 0;
	build_type = BUILD_ARTIST;

	if (build_dialog()) {
		progress_window();
		pthread_create(&build_thread_id, NULL, build_artist_thread, NULL);
	} else {
		build_thread_state = BUILD_THREAD_FREE;
	}
}

void
build_store(GtkTreeIter iter) {

	build_thread_state = BUILD_THREAD_BUSY;

	store_iter = iter;
	build_cancelled = 0;
	build_type = BUILD_STORE;

	if (build_dialog()) {
		progress_window();
		pthread_create(&build_thread_id, NULL, build_store_thread, NULL);
	} else {
		build_thread_state = BUILD_THREAD_FREE;
	}
}


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

