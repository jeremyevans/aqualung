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

#ifdef HAVE_CDDA

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gdk/gdkkeysyms.h>

#ifndef _WIN32
#include <pthread.h>
#endif /* !_WIN32 */

#ifdef HAVE_CDDB
#define _TMP_HAVE_CDDB 1
#undef HAVE_CDDB
#endif /* HAVE_CDDB */
#include <cdio/cdio.h>
#include <cdio/cdda.h>
#include <cdio/logging.h>
#ifdef HAVE_CDDB
#undef HAVE_CDDB
#endif /* HAVE_CDDB */
#ifdef _TMP_HAVE_CDDB
#define HAVE_CDDB 1
#undef _TMP_HAVE_CDDB
#endif /* _TMP_HAVE_CDDB */


#include "common.h"
#include "utils.h"
#include "utils_gui.h"
#include "options.h"
#include "music_browser.h"
#include "build_store.h"
#include "cddb_lookup.h"
#include "playlist.h"
#include "rb.h"
#include "i18n.h"
#include "cdda.h"
#include "cd_ripper.h"
#include "store_cdda.h"


extern GtkTreeSelection * music_select;
extern char pl_color_inactive[14];

extern options_t options;

extern GdkPixbuf * icon_store;
extern GdkPixbuf * icon_record;
extern GdkPixbuf * icon_track;

extern GtkTreeStore * music_store;
extern GtkWidget * browser_window;
extern GList * playlists;


GtkWidget * cdda_track_menu;
GtkWidget * cdda_track__addlist;

GtkWidget * cdda_record_menu;
GtkWidget * cdda_record__addlist;
GtkWidget * cdda_record__addlist_albummode;
GtkWidget * cdda_record__separator1;
#ifdef HAVE_CDDB
GtkWidget * cdda_record__cddb;
GtkWidget * cdda_record__cddb_submit;
#endif /* HAVE_CDDB */
GtkWidget * cdda_record__rip;
GtkWidget * cdda_record__disc_info;
GtkWidget * cdda_record__eject;
GtkWidget * cdda_record__separator2;
GtkWidget * cdda_record__drive_info;

GtkWidget * cdda_store_menu;
GtkWidget * cdda_store__addlist;
GtkWidget * cdda_store__addlist_albummode;

GdkPixbuf * icon_cdda;
GdkPixbuf * icon_cdda_disc;
GdkPixbuf * icon_cdda_nodisc;


void cdda_store__addlist_defmode(gpointer data);
void cdda_record__addlist_defmode(gpointer data);
void cdda_track__addlist_cb(gpointer data);

struct keybinds cdda_store_keybinds[] = {
	{cdda_store__addlist_defmode, GDK_a, GDK_A},
	{NULL, 0}
};

struct keybinds cdda_record_keybinds[] = {
	{cdda_record__addlist_defmode, GDK_a, GDK_A},
	{NULL, 0}
};

struct keybinds cdda_track_keybinds[] = {
	{cdda_track__addlist_cb, GDK_a, GDK_A},
	{NULL, 0}
};


void
cdda_track_free(cdda_track_t * data) {

	free(data->path);
	free(data);
}

gboolean
store_cdda_remove_track(GtkTreeIter * iter) {

	cdda_track_t * data;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, MS_COL_DATA, &data, -1);
	cdda_track_free(data);

	return gtk_tree_store_remove(music_store, iter);
}

gboolean
store_cdda_remove_record(GtkTreeIter * iter) {

	GtkTreeIter track_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &track_iter, iter, i++)) {
		store_cdda_remove_track(&track_iter);
	}

	return gtk_tree_store_remove(music_store, iter);
}


void
cdda_info_row(char * text, int yes, GtkWidget * table, int * cnt) {

	GtkWidget * image;
	GtkWidget * label;
	GtkWidget * hbox;

	image = gtk_image_new_from_stock(yes ? GTK_STOCK_APPLY : GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON);
	label = gtk_label_new(text);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), image, 0, 1, *cnt, *cnt+1, GTK_FILL, GTK_FILL, 2, 1);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, *cnt, *cnt+1, GTK_FILL, GTK_FILL, 5, 1);
	(*cnt)++;
}


void
cdda_drive_info(cdda_drive_t * drive) {

	CdIo_t * cdio;
	cdio_hwinfo_t hwinfo;
	cdio_drive_read_cap_t read_cap;
	cdio_drive_write_cap_t write_cap;
	cdio_drive_misc_cap_t misc_cap;

        GtkWidget * dialog;
	GtkWidget * label;
	GtkWidget * hbox;
	GtkWidget * vbox;
	GtkWidget * notebook;
	GtkWidget * table;
	char str[MAXLEN];


	cdio = cdio_open(drive->device_path, DRIVER_UNKNOWN);
	if (!cdio_get_hwinfo(cdio, &hwinfo)) {
		cdio_destroy(cdio);
		return;
	}
	cdio_get_drive_cap(cdio, &read_cap, &write_cap, &misc_cap);
	cdio_destroy(cdio);

	snprintf(str, MAXLEN-1, "%s [%s]", _("Drive info"), cdda_displayed_device_path(drive->device_path));

        dialog = gtk_dialog_new_with_buttons(str,
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT |
					     GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
					     NULL);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox, FALSE, FALSE, 4);

	table = gtk_table_new(4, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 2);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Device path:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 4, 1);
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(cdda_displayed_device_path(drive->device_path));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 4, 1);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Vendor:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 4, 1);
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(hwinfo.psz_vendor);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 4, 1);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Model:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 4, 1);
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(hwinfo.psz_model);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 2, 3, GTK_FILL, GTK_FILL, 4, 1);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Revision:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 4, 1);
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(hwinfo.psz_revision);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 3, 4, GTK_FILL, GTK_FILL, 4, 1);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("The information below is reported by the drive, and\n"
				"may not reflect the actual capabilities of the device."));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 10);

	if ((misc_cap == CDIO_DRIVE_CAP_ERROR) &&
	    (read_cap == CDIO_DRIVE_CAP_ERROR) &&
	    (write_cap == CDIO_DRIVE_CAP_ERROR)) {

		goto cdda_info_finish;
	}

	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), notebook, TRUE, TRUE, 0);

	if (misc_cap != CDIO_DRIVE_CAP_ERROR) {
		int cnt = 0;
		label = gtk_label_new(_("General"));
		table = gtk_table_new(8, 2, FALSE);
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table, label);

		cdda_info_row(_("Eject"), misc_cap & CDIO_DRIVE_CAP_MISC_EJECT, table, &cnt);
		cdda_info_row(_("Close tray"), misc_cap & CDIO_DRIVE_CAP_MISC_CLOSE_TRAY, table, &cnt);
		cdda_info_row(_("Disable manual eject"), misc_cap & CDIO_DRIVE_CAP_MISC_LOCK, table, &cnt);
		cdda_info_row(_("Select juke-box disc"), misc_cap & CDIO_DRIVE_CAP_MISC_SELECT_DISC, table, &cnt);
		cdda_info_row(_("Set drive speed"), misc_cap & CDIO_DRIVE_CAP_MISC_SELECT_SPEED, table, &cnt);
		cdda_info_row(_("Detect media change"), misc_cap & CDIO_DRIVE_CAP_MISC_MEDIA_CHANGED, table, &cnt);
		cdda_info_row(_("Read multiple sessions"), misc_cap & CDIO_DRIVE_CAP_MISC_MULTI_SESSION, table, &cnt);
		cdda_info_row(_("Hard reset device"), misc_cap & CDIO_DRIVE_CAP_MISC_RESET, table, &cnt);
	}

	if (read_cap != CDIO_DRIVE_CAP_ERROR) {
		int cnt = 0;
		label = gtk_label_new(_("Reading"));
		table = gtk_table_new(16, 2, FALSE);
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table, label);

		cdda_info_row(_("Play CD Audio"), read_cap & CDIO_DRIVE_CAP_READ_AUDIO, table, &cnt);
		cdda_info_row(_("Read CD-DA"), read_cap & CDIO_DRIVE_CAP_READ_CD_DA, table, &cnt);
		cdda_info_row(_("Read CD+G"), read_cap & CDIO_DRIVE_CAP_READ_CD_G, table, &cnt);
		cdda_info_row(_("Read CD-R"), read_cap & CDIO_DRIVE_CAP_READ_CD_R, table, &cnt);
		cdda_info_row(_("Read CD-RW"), read_cap & CDIO_DRIVE_CAP_READ_CD_RW, table, &cnt);
		cdda_info_row(_("Read DVD-R"), read_cap & CDIO_DRIVE_CAP_READ_DVD_R, table, &cnt);
		cdda_info_row(_("Read DVD+R"), read_cap & CDIO_DRIVE_CAP_READ_DVD_PR, table, &cnt);
		cdda_info_row(_("Read DVD-RW"), read_cap & CDIO_DRIVE_CAP_READ_DVD_RW, table, &cnt);
		cdda_info_row(_("Read DVD+RW"), read_cap & CDIO_DRIVE_CAP_READ_DVD_RPW, table, &cnt);
		cdda_info_row(_("Read DVD-RAM"), read_cap & CDIO_DRIVE_CAP_READ_DVD_RAM, table, &cnt);
		cdda_info_row(_("Read DVD-ROM"), read_cap & CDIO_DRIVE_CAP_READ_DVD_ROM, table, &cnt);
		cdda_info_row(_("C2 Error Correction"), read_cap & CDIO_DRIVE_CAP_READ_C2_ERRS, table, &cnt);
		cdda_info_row(_("Read Mode 2 Form 1"), read_cap & CDIO_DRIVE_CAP_READ_MODE2_FORM1, table, &cnt);
		cdda_info_row(_("Read Mode 2 Form 2"), read_cap & CDIO_DRIVE_CAP_READ_MODE2_FORM2, table, &cnt);
		cdda_info_row(_("Read MCN"), read_cap & CDIO_DRIVE_CAP_READ_MCN, table, &cnt);
		cdda_info_row(_("Read ISRC"), read_cap & CDIO_DRIVE_CAP_READ_ISRC, table, &cnt);
	}

	if (write_cap != CDIO_DRIVE_CAP_ERROR) {
		int cnt = 0;
		label = gtk_label_new(_("Writing"));
		table = gtk_table_new(9, 2, FALSE);
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table, label);

		cdda_info_row(_("Write CD-R"), write_cap & CDIO_DRIVE_CAP_WRITE_CD_R, table, &cnt);
		cdda_info_row(_("Write CD-RW"), write_cap & CDIO_DRIVE_CAP_WRITE_CD_RW, table, &cnt);
		cdda_info_row(_("Write DVD-R"), write_cap & CDIO_DRIVE_CAP_WRITE_DVD_R, table, &cnt);
		cdda_info_row(_("Write DVD+R"), write_cap & CDIO_DRIVE_CAP_WRITE_DVD_PR, table, &cnt);
		cdda_info_row(_("Write DVD-RW"), write_cap & CDIO_DRIVE_CAP_WRITE_DVD_RW, table, &cnt);
		cdda_info_row(_("Write DVD+RW"), write_cap & CDIO_DRIVE_CAP_WRITE_DVD_RPW, table, &cnt);
		cdda_info_row(_("Write DVD-RAM"), write_cap & CDIO_DRIVE_CAP_WRITE_DVD_RAM, table, &cnt);
		cdda_info_row(_("Mount Rainier"), write_cap & CDIO_DRIVE_CAP_WRITE_MT_RAINIER, table, &cnt);
		cdda_info_row(_("Burn Proof"), write_cap & CDIO_DRIVE_CAP_WRITE_BURN_PROOF, table, &cnt);
	}

 cdda_info_finish:
	gtk_widget_show_all(dialog);
        aqualung_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


void
cdda_disc_info(cdda_drive_t * drive) {

	CdIo_t * cdio;
	cdtext_t * cdtext;
	track_t itrack;
	track_t ntracks;
	track_t track_last;
	int i;

        GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * label;
	GtkWidget * vbox;
	GtkWidget * hbox;

	GtkListStore * list;
	GtkWidget * view;
	GtkWidget * viewport;
	GtkWidget * scrolled_win;
	GtkCellRenderer * cell;

	GType types[MAX_CDTEXT_FIELDS + 1];
	GtkTreeViewColumn * columns[MAX_CDTEXT_FIELDS + 1];
	int visible[MAX_CDTEXT_FIELDS + 1];
	int has_some_cdtext = 0;


        dialog = gtk_dialog_new_with_buttons(_("Disc info"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT |
					     GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
					     NULL);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_widget_set_size_request(GTK_WIDGET(dialog), 500, 400);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox, TRUE, TRUE, 4);


	cdio = cdio_open(drive->device_path, DRIVER_UNKNOWN);
	cdtext = cdio_get_cdtext(cdio, 0);

	if (cdtext != NULL) {
		table = gtk_table_new(MAX_CDTEXT_FIELDS, 2, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 2);


		for (i = 0; i < MAX_CDTEXT_FIELDS; i++) {

			if (cdtext->field[i] == NULL || *(cdtext->field[i]) == '\0') {
				continue;
			}

			has_some_cdtext = 1;

			hbox = gtk_hbox_new(FALSE, 0);
			label = gtk_label_new(cdtext_field2str(i));
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
			gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, i, i + 1, GTK_FILL, GTK_FILL, 4, 1);

			hbox = gtk_hbox_new(FALSE, 0);
			label = gtk_label_new(cdtext->field[i]);
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
			gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, i, i + 1, GTK_FILL, GTK_FILL, 4, 1);
		}
	}


	types[0] = G_TYPE_INT;
	for (i = 1; i <= MAX_CDTEXT_FIELDS; i++) {
		types[i] = G_TYPE_STRING;
		visible[i] = 0;
	}

	list = gtk_list_store_newv(MAX_CDTEXT_FIELDS + 1,
				   types);

        view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list));
        viewport = gtk_viewport_new(NULL, NULL);
        scrolled_win = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
                                       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), viewport, TRUE, TRUE, 2);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_win);
        gtk_container_add(GTK_CONTAINER(scrolled_win), view);

        cell = gtk_cell_renderer_text_new();
	g_object_set((gpointer)cell, "xalign", 1.0, NULL);
        columns[0] = gtk_tree_view_column_new_with_attributes(_("No."), cell, "text", 0, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(view), GTK_TREE_VIEW_COLUMN(columns[0]));

	for (i = 0; i < MAX_CDTEXT_FIELDS; i++) {

		cell = gtk_cell_renderer_text_new();
		columns[i + 1] = gtk_tree_view_column_new_with_attributes(cdtext_field2str(i),
									  cell, "text", i + 1, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), GTK_TREE_VIEW_COLUMN(columns[i + 1]));
	}

	itrack = cdio_get_first_track_num(cdio);
	ntracks = cdio_get_num_tracks(cdio);
	track_last = itrack + ntracks;

	for (; itrack < track_last; itrack++) {

		GtkTreeIter iter;

		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter, 0, itrack, -1);

		cdtext = cdio_get_cdtext(cdio, itrack);

		if (cdtext == NULL) {
			continue;
		}

		for (i = 0; i < MAX_CDTEXT_FIELDS; i++) {

			if (cdtext->field[i] != NULL && *(cdtext->field[i]) != '\0') {
				gtk_list_store_set(list, &iter, i + 1, cdtext->field[i], -1);
				visible[i + 1] = 1;
				has_some_cdtext = 1;
			}
		}
	}

	for (i = 1; i <= MAX_CDTEXT_FIELDS; i++) {
		gtk_tree_view_column_set_visible(columns[i], visible[i]);
	}

	cdio_destroy(cdio);

	gtk_widget_show_all(dialog);

	if (has_some_cdtext) {
		aqualung_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	} else {
		gtk_widget_destroy(dialog);

		message_dialog(_("Disc info"),
			       browser_window,
			       GTK_MESSAGE_INFO,
			       GTK_BUTTONS_OK,
			       NULL,
			       _("This CD does not contain CD-Text information."));
	}
}


void
cdda_record_cb(void (* callback)(cdda_drive_t *)) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {
		cdda_drive_t * drive;
                gtk_tree_model_get(model, &iter, MS_COL_DATA, &drive, -1);
		(*callback)(drive);
	}
}

void
cdda_record__do_eject(cdda_drive_t * drive) {

	CdIo_t * cdio = cdio_open(drive->device_path, DRIVER_DEVICE);
	if (cdio) {
		cdio_eject_media(&cdio);
		if (cdio != NULL) {
			cdio_destroy(cdio);
		}
	}
}

void
cdda_record__disc_cb(gpointer data) {

	cdda_record_cb(cdda_disc_info);
}


void
cdda_record__drive_cb(gpointer data) {

	cdda_record_cb(cdda_drive_info);
}


void
cdda_record__eject_cb(gpointer data) {

	cdda_record_cb(cdda_record__do_eject);
}


#if defined(HAVE_SNDFILE) || defined(HAVE_FLAC) || defined(HAVE_VORBISENC) || defined(HAVE_LAME)
void
cdda_record__rip_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {
		cdda_drive_t * drive;
                gtk_tree_model_get(model, &iter, MS_COL_DATA, &drive, -1);
		cd_ripper(drive, &iter);
	}
}
#endif /* HAVE_* */


/********************************************/

/* returns the duration of the track */
float
cdda_track_addlist_iter(GtkTreeIter iter_track, playlist_t * pl, GtkTreeIter * parent, GtkTreeIter * dest) {

        GtkTreeIter iter_record;
	GtkTreeIter list_iter;

	cdda_track_t * data;

	char * ptrack_name;

        char artist_name[MAXLEN];
        char record_name[MAXLEN];
        char track_name[MAXLEN];
	char list_str[MAXLEN];

	char duration_str[MAXLEN];


	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track,
			   MS_COL_NAME, &ptrack_name,
			   MS_COL_DATA, &data, -1);

	strncpy(track_name, ptrack_name, MAXLEN-1);
	g_free(ptrack_name);

	if (parent == NULL) {
		cdda_drive_t * drive;
		
		gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store),
					   &iter_record, &iter_track);
		gtk_tree_model_get(GTK_TREE_MODEL(music_store),
				   &iter_record, MS_COL_DATA, &drive, -1);
		
		strncpy(artist_name, drive->disc.artist_name, MAXLEN-1);
		strncpy(record_name, drive->disc.record_name, MAXLEN-1);
	}

	if (parent != NULL) {
		strcpy(list_str, track_name);
	} else {
		make_title_string(list_str, options.title_format,
				  artist_name, record_name, track_name);
	}

	time2time(data->duration, duration_str);
	
	/* either parent or dest should be set, but not both */
	gtk_tree_store_insert_before(pl->store, &list_iter, parent, dest);
	
	gtk_tree_store_set(pl->store, &list_iter,
			   PL_COL_TRACK_NAME, list_str,
			   PL_COL_PHYSICAL_FILENAME, data->path,
			   PL_COL_SELECTION_COLOR, pl_color_inactive,
			   PL_COL_VOLUME_ADJUSTMENT, 0.0f,
			   PL_COL_VOLUME_ADJUSTMENT_DISP, "",
			   PL_COL_DURATION, data->duration,
			   PL_COL_DURATION_DISP, duration_str, -1);

	return data->duration;
}


void
cdda_record_addlist_iter(GtkTreeIter iter_record, playlist_t * pl, GtkTreeIter * dest, int album_mode) {

        GtkTreeIter iter_track;
	GtkTreeIter list_iter;
	GtkTreeIter * plist_iter;
	
	int i;
	float record_duration = 0.0f;


	if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter_record) == 0) {
		return;
	}

	if (album_mode) {

		char name_str[MAXLEN];
		char packed_str[MAXLEN];
		cdda_drive_t * drive;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record, MS_COL_DATA, &drive, -1);

		snprintf(name_str, MAXLEN-1, "%s: %s", drive->disc.artist_name, drive->disc.record_name);
		pack_strings(drive->disc.artist_name, drive->disc.record_name, packed_str);

		gtk_tree_store_insert_before(pl->store, &list_iter, NULL, dest);
		gtk_tree_store_set(pl->store, &list_iter,
				   PL_COL_TRACK_NAME, name_str,
				   PL_COL_PHYSICAL_FILENAME, packed_str,
				   PL_COL_SELECTION_COLOR, pl_color_inactive,
				   PL_COL_VOLUME_ADJUSTMENT, 0.0f,
				   PL_COL_VOLUME_ADJUSTMENT_DISP, "",
				   PL_COL_DURATION, 0.0f,
				   PL_COL_DURATION_DISP, "00:00", -1);
		plist_iter = &list_iter;
		dest = NULL;
	} else {
		plist_iter = NULL;
	}

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track, &iter_record, i++)) {
		record_duration += cdda_track_addlist_iter(iter_track, pl, plist_iter, dest);
	}

	if (album_mode) {
		char record_duration_str[MAXLEN];
		time2time(record_duration, record_duration_str);
		gtk_tree_store_set(pl->store, &list_iter,
				   PL_COL_DURATION, record_duration,
				   PL_COL_DURATION_DISP, record_duration_str, -1);
	}
}


void
cdda_track__addlist_cb(gpointer data) {

        GtkTreeIter iter_track;
	playlist_t * pl = playlist_get_current();

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_track)) {
		cdda_track_addlist_iter(iter_track, pl, NULL, (GtkTreeIter *)data);
		if (pl == playlist_get_current()) {
			playlist_content_changed(pl);
		}
		delayed_playlist_rearrange(pl);
	}
}

void
cdda_record__addlist_with_mode(int mode, gpointer data) {

        GtkTreeIter iter_record;
	playlist_t * pl = playlist_get_current();

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_record)) {
		cdda_record_addlist_iter(iter_record, pl, (GtkTreeIter *)data, mode);
		if (pl == playlist_get_current()) {
			playlist_content_changed(pl);
		}
		delayed_playlist_rearrange(pl);
	}
}

void
cdda_record__addlist_defmode(gpointer data) {

	cdda_record__addlist_with_mode(options.playlist_is_tree, data);
}

void
cdda_record__addlist_albummode_cb(gpointer data) {

	cdda_record__addlist_with_mode(1, data);
}


void
cdda_record__addlist_cb(gpointer data) {

	cdda_record__addlist_with_mode(0, data);
}


void
cdda_store_addlist_iter(GtkTreeIter iter_store, playlist_t * pl, GtkTreeIter * dest, int album_mode) {

	GtkTreeIter iter_record;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_record, &iter_store, i++)) {
		cdda_record_addlist_iter(iter_record, pl, dest, album_mode);
	}
}

void
cdda_store__addlist_with_mode(int mode, gpointer data) {

        GtkTreeIter iter_store;
	playlist_t * pl = playlist_get_current();

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_store)) {
		cdda_store_addlist_iter(iter_store, pl, (GtkTreeIter *)data, mode);
		if (pl == playlist_get_current()) {
			playlist_content_changed(pl);
		}
		delayed_playlist_rearrange(pl);
	}
}

void
cdda_store__addlist_defmode(gpointer data) {

	cdda_store__addlist_with_mode(options.playlist_is_tree, data);
}

void
cdda_store__addlist_albummode_cb(gpointer data) {

	cdda_store__addlist_with_mode(1, data);
}


void
cdda_store__addlist_cb(gpointer data) {

	cdda_store__addlist_with_mode(0, data);
}


void
cdda_add_to_playlist(GtkTreeIter * iter_drive, unsigned long hash) {

	int i = 0;
	GtkTreeIter iter;

	int target_found = 0;
	GtkTreeIter target_iter;

	playlist_t * pl = playlist_get_current();

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) > 0) {

			int j = 0;
			int has_cdda = 0;
			GtkTreeIter child;

			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &child, &iter, j++)) {

				char * pfile;
				gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &child,
						   PL_COL_PHYSICAL_FILENAME, &pfile, -1);

				if (!target_found && cdda_is_cdtrack(pfile)) {
					has_cdda = 1;
				}

				if (cdda_hash_matches(pfile, hash)) {
					g_free(pfile);
					return;
				}

				g_free(pfile);
			}

			if (!target_found && !has_cdda) {
				target_iter = iter;
				target_found = 1;
			}
		} else {

			char * pfile;
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
					   PL_COL_PHYSICAL_FILENAME, &pfile, -1);

			if (!target_found && cdda_is_cdtrack(pfile)) {
				target_iter = iter;
				target_found = 1;
			}

			if (cdda_hash_matches(pfile, hash)) {
				g_free(pfile);
				return;
			}
				
			g_free(pfile);
		}
	}

	if (target_found) {
		cdda_record_addlist_iter(*iter_drive, pl, &target_iter, options.playlist_is_tree);
	} else {
		cdda_record_addlist_iter(*iter_drive, pl, NULL, options.playlist_is_tree);
	}

	playlist_content_changed(pl);
	delayed_playlist_rearrange(pl);
}

void
cdda_remove_from_playlist_foreach(gpointer data, gpointer user_data) {

	int i = 0;
	GtkTreeIter iter;
	unsigned long hash;

	playlist_t * pl = (playlist_t *)data;
	cdda_drive_t * drive = (cdda_drive_t *)user_data;


	if (drive->disc.hash == 0) {
		hash = drive->disc.hash_prev;
	} else {
		hash = drive->disc.hash;
	}

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) > 0) {

			int j = 0;
			GtkTreeIter child;

			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &child, &iter, j++)) {

				char * pfile;
				gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &child,
						   PL_COL_PHYSICAL_FILENAME, &pfile, -1);

				if (cdda_hash_matches(pfile, hash)) {
					gtk_tree_store_remove(pl->store, &child);
					--j;
				}
				
				g_free(pfile);
			}

			if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) == 0) {
				gtk_tree_store_remove(pl->store, &iter);
				--i;
			} else {
				recalc_album_node(pl, &iter);
			}

		} else {

			char * pfile;
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
					   PL_COL_PHYSICAL_FILENAME, &pfile, -1);
			
			if (cdda_hash_matches(pfile, hash)) {
				gtk_tree_store_remove(pl->store, &iter);
				--i;
			}

			g_free(pfile);
		}
	}

	playlist_content_changed(pl);
}

void
cdda_remove_from_playlist(cdda_drive_t * drive) {

	g_list_foreach(playlists, cdda_remove_from_playlist_foreach, drive);
}


static void
track_status_bar_info(GtkTreeModel * model, GtkTreeIter * iter, float * length) {

	cdda_track_t * data;

	gtk_tree_model_get(model, iter, MS_COL_DATA, &data, -1);

	*length += data->duration;
}

static void
record_status_bar_info(GtkTreeModel * model, GtkTreeIter * iter, float * length,
		       int * ntrack, int * nrecord) {

	GtkTreeIter track_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(model, &track_iter, iter, i++)) {
		track_status_bar_info(model, &track_iter, length);
	}

	*ntrack += i - 1;

	if (i > 1) {
		(*nrecord)++;
	}
}

static void
store_status_bar_info(GtkTreeModel * model, GtkTreeIter * iter, float * length,
		       int * ntrack, int * nrecord, int * ndrive) {

	GtkTreeIter record_iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(model, &record_iter, iter, i++)) {
		record_status_bar_info(model, &record_iter, length, ntrack, nrecord);
	}

	*ndrive = i - 1;
}

static void
set_status_bar_info(GtkTreeIter * tree_iter, GtkLabel * statusbar) {

	int ntrack = 0, nrecord = 0, ndrive = 0;
	float length = 0.0f;

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
		track_status_bar_info(model, tree_iter, &length);
		sprintf(str, "%s: ", name);
		break;
	case 2:
		record_status_bar_info(model, tree_iter, &length, &ntrack, &nrecord);
		if (nrecord == 0) {
			sprintf(str, "%s: %s ", _("CD Audio"), name);
		} else {
			sprintf(str, "%s: %d %s ", name,
				ntrack, (ntrack == 1) ? _("track") : _("tracks"));
		}
		break;
	case 1:
		store_status_bar_info(model, tree_iter, &length, &ntrack, &nrecord, &ndrive);
		sprintf(str, "%s: %d %s, %d %s, %d %s ", name,
			ndrive, (ndrive == 1) ? _("drive") : _("drives"),
			nrecord, (nrecord == 1) ? _("record") : _("records"),
			ntrack, (ntrack == 1) ? _("track") : _("tracks"));
		break;
	}

	g_free(name);

	if (length > 0.0f) {
		time2time(length, length_str);
		sprintf(tmp, " [%s] ", length_str);
		strcat(str, tmp);
	}


	gtk_label_set_text(statusbar, str);
}


static void
set_popup_sensitivity(GtkTreePath * path) {

	gboolean build_free = build_thread_test(BUILD_THREAD_FREE);

#ifdef HAVE_CDDB
	gboolean cddb_free = cddb_thread_test(CDDB_THREAD_FREE);
#endif /* HAVE_CDDB */


	if (gtk_tree_path_get_depth(path) == 2) {

		GtkTreeIter iter;
		gboolean val_cdda;
		gboolean val_cdda_free;
		cdda_drive_t * drive;

		gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter, path);
		val_cdda = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter) > 0;
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &drive, -1);
		val_cdda_free = (drive->is_used == 0) ? TRUE : FALSE;

		gtk_widget_set_sensitive(cdda_record__addlist, val_cdda);
		gtk_widget_set_sensitive(cdda_record__addlist_albummode, val_cdda);

#ifdef HAVE_CDDB
		gtk_widget_set_sensitive(cdda_record__cddb, val_cdda && cddb_free && build_free);
		gtk_widget_set_sensitive(cdda_record__cddb_submit, val_cdda && cddb_free && build_free);
#endif /* HAVE_CDDB */

		gtk_widget_set_sensitive(cdda_record__rip, val_cdda && val_cdda_free);
		gtk_widget_set_sensitive(cdda_record__eject, val_cdda_free);
		gtk_widget_set_sensitive(cdda_record__disc_info, val_cdda);
	}
}


static void
add_path_to_playlist(GtkTreePath * path, GtkTreeIter * piter, int new_tab) {

	int depth = gtk_tree_path_get_depth(path);

	gtk_tree_selection_select_path(music_select, path);

	if (new_tab) {
		char * name;
		GtkTreeIter iter;

		gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter, path);
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_NAME, &name, -1);
		playlist_tab_new_if_nonempty(name);

		g_free(name);
	}

	switch (depth) {
	case 1:
		cdda_store__addlist_defmode(piter);
		break;
	case 2:
		cdda_record__addlist_defmode(piter);
		break;
	case 3:
		cdda_track__addlist_cb(piter);
		break;
	}
}



/************************************************/
/* music store interface */


int
store_cdda_iter_is_track(GtkTreeIter * iter) {

	GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), iter);
	int ret = (gtk_tree_path_get_depth(p) == 3);
	gtk_tree_path_free(p);
	return ret;
}


void
store_cdda_iter_addlist_defmode(GtkTreeIter * ms_iter, GtkTreeIter * pl_iter, int new_tab) {

	GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), ms_iter);
	add_path_to_playlist(p, pl_iter, new_tab);
	gtk_tree_path_free(p);
}


void
store_cdda_selection_changed(GtkTreeIter * tree_iter, GtkTextBuffer * buffer, GtkLabel * statusbar) {

	if (options.enable_mstore_statusbar) {
		set_status_bar_info(tree_iter, statusbar);
	}
}


gboolean
store_cdda_event_cb(GdkEvent * event, GtkTreeIter * iter, GtkTreePath * path) {

	if (event->type == GDK_BUTTON_PRESS) {

		GdkEventButton * bevent = (GdkEventButton *) event;

                if (bevent->button == 3) {

			set_popup_sensitivity(path);

			switch (gtk_tree_path_get_depth(path)) {
			case 1:
				gtk_menu_popup(GTK_MENU(cdda_store_menu), NULL, NULL, NULL, NULL,
					       bevent->button, bevent->time);
				break;
			case 2:
				gtk_menu_popup(GTK_MENU(cdda_record_menu), NULL, NULL, NULL, NULL,
					       bevent->button, bevent->time);
				break;
			case 3:
				gtk_menu_popup(GTK_MENU(cdda_track_menu), NULL, NULL, NULL, NULL,
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
			for (i = 0; cdda_store_keybinds[i].callback; ++i)
				if (kevent->keyval == cdda_store_keybinds[i].keyval1 ||
				    kevent->keyval == cdda_store_keybinds[i].keyval2)
					(cdda_store_keybinds[i].callback)(NULL);
			break;
		case 2:
			for (i = 0; cdda_record_keybinds[i].callback; ++i)
				if (kevent->keyval == cdda_record_keybinds[i].keyval1 ||
				    kevent->keyval == cdda_record_keybinds[i].keyval2)
					(cdda_record_keybinds[i].callback)(NULL);
			break;
		case 3:
			for (i = 0; cdda_track_keybinds[i].callback; ++i)
				if (kevent->keyval == cdda_track_keybinds[i].keyval1 ||
				    kevent->keyval == cdda_track_keybinds[i].keyval2)
					(cdda_track_keybinds[i].callback)(NULL);
			break;
		}
	}

	return FALSE;
}


void
store_cdda_load_icons(void) {

	char path[MAXLEN];

	sprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-cdda.png");
	icon_cdda = gdk_pixbuf_new_from_file (path, NULL);
	sprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-cdda-disk.png");
	icon_cdda_disc = gdk_pixbuf_new_from_file (path, NULL);
	sprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-cdda-nodisk.png");
	icon_cdda_nodisc = gdk_pixbuf_new_from_file (path, NULL);
}


void
store_cdda_create_popup_menu(void) {

	/* create popup menu for cdda_record tree items */
	cdda_record_menu = gtk_menu_new();
	cdda_record__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	cdda_record__addlist_albummode = gtk_menu_item_new_with_label(_("Add to playlist (Album mode)"));
	cdda_record__separator1 = gtk_separator_menu_item_new();
#ifdef HAVE_CDDB
	cdda_record__cddb = gtk_menu_item_new_with_label(_("CDDB query for this CD..."));
	cdda_record__cddb_submit = gtk_menu_item_new_with_label(_("Submit CD to CDDB database..."));
#endif /* HAVE_CDDB */
	cdda_record__rip = gtk_menu_item_new_with_label(_("Rip CD..."));
	cdda_record__disc_info = gtk_menu_item_new_with_label(_("Disc info..."));
	cdda_record__separator2 = gtk_separator_menu_item_new();
	cdda_record__drive_info = gtk_menu_item_new_with_label(_("Drive info..."));
	cdda_record__eject = gtk_menu_item_new_with_label(_("Eject"));

	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__addlist_albummode);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__separator1);
#ifdef HAVE_CDDB
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__cddb);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__cddb_submit);
#endif /* HAVE_CDDB */
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__rip);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__disc_info);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__drive_info);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__eject);

 	g_signal_connect_swapped(G_OBJECT(cdda_record__addlist), "activate", G_CALLBACK(cdda_record__addlist_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(cdda_record__addlist_albummode), "activate", G_CALLBACK(cdda_record__addlist_albummode_cb), NULL);
#ifdef HAVE_CDDB
	//g_signal_connect_swapped(G_OBJECT(cdda_record__cddb), "activate", G_CALLBACK(record__cddb_cb), NULL);
 	//g_signal_connect_swapped(G_OBJECT(cdda_record__cddb_submit), "activate", G_CALLBACK(record__cddb_submit_cb), NULL);
#endif /* HAVE_CDDB */
 	g_signal_connect_swapped(G_OBJECT(cdda_record__rip), "activate", G_CALLBACK(cdda_record__rip_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(cdda_record__disc_info), "activate", G_CALLBACK(cdda_record__disc_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(cdda_record__drive_info), "activate", G_CALLBACK(cdda_record__drive_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(cdda_record__eject), "activate", G_CALLBACK(cdda_record__eject_cb), NULL);

	gtk_widget_show(cdda_record__addlist);
	gtk_widget_show(cdda_record__addlist_albummode);
	gtk_widget_show(cdda_record__separator1);
#ifdef HAVE_CDDB
	gtk_widget_show(cdda_record__cddb);
	gtk_widget_show(cdda_record__cddb_submit);
#endif /* HAVE_CDDB */
#if defined(HAVE_SNDFILE) || defined(HAVE_FLAC) || defined(HAVE_VORBISENC) || defined(HAVE_LAME)
	gtk_widget_show(cdda_record__rip);
#endif /* HAVE_* */
	gtk_widget_show(cdda_record__disc_info);
	gtk_widget_show(cdda_record__separator2);
	gtk_widget_show(cdda_record__drive_info);
	gtk_widget_show(cdda_record__eject);

	/* create popup menu for cdda_track tree items */
	cdda_track_menu = gtk_menu_new();
	cdda_track__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_track_menu), cdda_track__addlist);
 	g_signal_connect_swapped(G_OBJECT(cdda_track__addlist), "activate", G_CALLBACK(cdda_track__addlist_cb), NULL);
	gtk_widget_show(cdda_track__addlist);

	/* create popup menu for cdda_store tree items */
	cdda_store_menu = gtk_menu_new();
	cdda_store__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	cdda_store__addlist_albummode = gtk_menu_item_new_with_label(_("Add to playlist (Album mode)"));

	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_store_menu), cdda_store__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_store_menu), cdda_store__addlist_albummode);

 	g_signal_connect_swapped(G_OBJECT(cdda_store__addlist), "activate", G_CALLBACK(cdda_store__addlist_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(cdda_store__addlist_albummode), "activate", G_CALLBACK(cdda_store__addlist_albummode_cb), NULL);

	gtk_widget_show(cdda_store__addlist);
	gtk_widget_show(cdda_store__addlist_albummode);
}


#endif /* HAVE_CDDA */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
