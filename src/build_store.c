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

extern options_t options;
extern GtkTreeStore * music_store;

extern GtkWidget * browser_window;
extern GtkWidget * gui_stock_label_button(gchar * blabel, const gchar * bstock);
extern void set_sliders_width(void);


pthread_t build_store_thread_id;

GtkTreeIter store_iter;

GtkWidget * meta_check_enable;
GtkWidget * meta_check_wspace;
GtkWidget * meta_check_title;
GtkWidget * meta_check_artist;
GtkWidget * meta_check_record;

#ifdef HAVE_CDDB
GtkWidget * cddb_check_enable;
GtkWidget * cddb_check_title;
GtkWidget * cddb_check_artist;
GtkWidget * cddb_check_record;
/*
GtkWidget * cddb_radio_batch;
GtkWidget * cddb_radio_interactive;
*/
#endif /* HAVE_CDDB */

GtkWidget * fs_radio_preset;
GtkWidget * fs_radio_regexp;
GtkWidget * fs_check_rm_number;
GtkWidget * fs_check_rm_ext;
GtkWidget * fs_check_underscore;
GtkWidget * fs_entry_regexp1;
GtkWidget * fs_entry_regexp2;

int meta_enable = 1;
int meta_wspace = 1;
int meta_title = 1;
int meta_artist = 1;
int meta_record = 1;

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


void
meta_check_enable_toggled(GtkWidget * widget, gpointer * data) {

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_enable));

	gtk_widget_set_sensitive(meta_check_wspace, state);
	gtk_widget_set_sensitive(meta_check_title, state);
	gtk_widget_set_sensitive(meta_check_artist, state);
	gtk_widget_set_sensitive(meta_check_record, state);
}


#ifdef HAVE_CDDB
void
cddb_check_enable_toggled(GtkWidget * widget, gpointer * data) {

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cddb_check_enable));

	gtk_widget_set_sensitive(cddb_check_title, state);
	gtk_widget_set_sensitive(cddb_check_artist, state);
	gtk_widget_set_sensitive(cddb_check_record, state);
	/*
	gtk_widget_set_sensitive(cddb_radio_batch, state);
	gtk_widget_set_sensitive(cddb_radio_interactive, state);
	*/
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

	GtkWidget * meta_vbox;

#ifdef HAVE_CDDB
	GtkWidget * cddb_vbox;
#endif /* HAVE_CDDB */

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


	/* Metadata */

        meta_vbox = gtk_vbox_new(FALSE, 0);
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
		gtk_check_button_new_with_label(_("Import title"));
	gtk_widget_set_name(meta_check_title, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_title, FALSE, FALSE, 0);

	if (meta_title) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_title), TRUE);
	}

	meta_check_artist =
		gtk_check_button_new_with_label(_("Import artist"));
	gtk_widget_set_name(meta_check_artist, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_artist, FALSE, FALSE, 0);

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

	meta_check_wspace =
		gtk_check_button_new_with_label(_("Exclude whitespace-only metadata"));
	gtk_widget_set_name(meta_check_wspace, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(meta_vbox), meta_check_wspace, FALSE, FALSE, 0);

	if (meta_wspace) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(meta_check_wspace), TRUE);
	}


	/* CDDB */

#ifdef HAVE_CDDB
        cddb_vbox = gtk_vbox_new(FALSE, 0);
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
		gtk_check_button_new_with_label(_("Import title"));
	gtk_widget_set_name(cddb_check_title, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(cddb_vbox), cddb_check_title, FALSE, FALSE, 0);

	if (cddb_title) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cddb_check_title), TRUE);
	}

	cddb_check_artist =
		gtk_check_button_new_with_label(_("Import artist"));
	gtk_widget_set_name(cddb_check_artist, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(cddb_vbox), cddb_check_artist, FALSE, FALSE, 0);

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

	/*
	cddb_radio_batch = gtk_radio_button_new_with_label(NULL, _("Batch mode (I'm feeling lucky)"));
	gtk_widget_set_name(cddb_radio_batch, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(cddb_vbox), cddb_radio_batch, FALSE, FALSE, 0);

	cddb_radio_interactive = gtk_radio_button_new_with_label_from_widget(
			     GTK_RADIO_BUTTON(cddb_radio_batch), _("Interactive mode (let me choose if multiple matches found)"));
	gtk_widget_set_name(cddb_radio_interactive, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(cddb_vbox), cddb_radio_interactive, FALSE, FALSE, 0);
	*/
#endif /* HAVE_CDDB */

	/* Filenames */

        fs_vbox = gtk_vbox_new(FALSE, 0);
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

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		const char * proot = gtk_entry_get_text(GTK_ENTRY(root_entry));

		if (proot[0] == '~') {
			snprintf(root, MAXLEN-1, "%s%s", options.home, proot + 1);
		} else if (proot[0] == '/') {
			strncpy(root, proot, MAXLEN-1);
		} else if (proot[0] != '\0') {
			snprintf(root, MAXLEN-1, "%s/%s", options.cwd, proot);
		}

		meta_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_enable));
		meta_wspace = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_wspace));
		meta_title = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_title));
		meta_artist = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_artist));
		meta_record = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(meta_check_record));

#ifdef HAVE_CDDB
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

		if (root[0] == '\0' ||
		    (!fs_preset && regcomp(&fs_compiled, fs_regexp, REG_EXTENDED))) {
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
num_invalid_tracks(track_t * tracks) {

	int invalid = 0;
	track_t * ptrack;

	for (ptrack = tracks; ptrack; ptrack = ptrack->next) {
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

		regmatch_t regmatch[10];

		if (!regexec(&fs_compiled, src, 10, regmatch, 0)) {
		
			int i;

			dest[0] = '\0';

			strncat(dest, src, regmatch[0].rm_so);

			for (i = 0; i < strlen(fs_replacement) - 1; i++) {
				if (fs_replacement[i] == '\\' && isdigit(fs_replacement[i+1])) {
					int j = fs_replacement[i+1] - '0';
					
					if (j == 0 || j > fs_compiled.re_nsub) {
						i++;
						continue;
					}
					
					strncat(dest, src + regmatch[j].rm_so,
						regmatch[j].rm_eo - regmatch[j].rm_so);
					i++;
				} else {
					strncat(dest, fs_replacement + i, 1);
				}
			}
			
			strncat(dest, fs_replacement + i, 1);
			strcat(dest, src + regmatch[0].rm_eo);
		} else {
			strncpy(dest, src, MAXLEN-1);
		}
	}
}

void
process_record(char * dir_record, char * artist_d_name, char * record_d_name) {

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

			if ((track = (track_t *)malloc(sizeof(track_t))) == NULL) {
				fprintf(stderr, "build_store.c: process_record(): malloc error\n");
				return;
			} else {
				track->duration = duration;
				strncpy(track->d_name, ent_track[i]->d_name, MAXLEN-1);
				strncpy(track->filename, filename, MAXLEN-1);
				track->valid = 0;
				track->next = NULL;

				num_tracks++;
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

	/* metadata */

	if (meta_enable) {

		for (ptrack = tracks; ptrack; ptrack = ptrack->next) {

			meta = meta_new();

			if (meta_read(meta, ptrack->filename)) {
				if (meta_artist &&
				    !artist_is_set && meta_get_artist(meta, artist)) {
					if (!meta_wspace || !is_wspace(artist)) {
						artist_is_set = 1;
					}
				}
				if (meta_record &&
				    !record_is_set && meta_get_record(meta, record)) {
					if (!meta_wspace || !is_wspace(record)) {
						record_is_set = 1;
					}
				}
				if (meta_title &&
				    !ptrack->valid && meta_get_title(meta, ptrack->name)) {
					if (!meta_wspace || !is_wspace(ptrack->name)) {
						ptrack->valid = 1;
					}
				}
			}

			meta_free(meta);
		}

		if (artist_is_set && record_is_set && num_invalid_tracks(tracks) == 0) {
			goto finish;
		}
	}

	/* cddb */

#ifdef HAVE_CDDB
	if (cddb_enable) {

		cddb_get_batch(tracks, artist, record, &artist_is_set, &record_is_set,
			       cddb_title, cddb_artist, cddb_record);

		if (artist_is_set && record_is_set && num_invalid_tracks(tracks) == 0) {
			goto finish;
		}
	}
#endif /* HAVE_CDDB */

	/* filesystem */

	if (!artist_is_set) {
		strncpy(artist, artist_d_name, MAXLEN-1);
		artist_is_set = 1;
	}

	if (!record_is_set) {
		strncpy(record, record_d_name, MAXLEN-1);
		record_is_set = 1;
	}

	for (i = 0, ptrack = tracks; ptrack; i++, ptrack = ptrack->next) {

		if (!ptrack->valid) {
			printf("transform %d\n", i);
			transform_filename(ptrack->name, ptrack->d_name);
			ptrack->valid = 1;
		}
	}


	/* add tracks to music store */

 finish:

	if (!get_iter_for_artist_and_record(&store_iter, &record_iter, artist, record)) {
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

			process_record(dir_record, ent_artist[i]->d_name, ent_record[j]->d_name);
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
