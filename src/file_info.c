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

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "file_decoder.h"
#include "gui_main.h"
#include "i18n.h"
#include "file_info.h"


extern GtkWidget * main_window;

GtkWidget * info_window;

static gint
dismiss(GtkWidget * widget, gpointer data) {

	gtk_widget_destroy(info_window);
	return TRUE;
}


void
show_file_info(char * name, char * file) {

        file_decoder_t * fdec = NULL;
        char str[MAXLEN];

	GtkWidget * vbox;
	GtkWidget * table;
	GtkWidget * hbox_name;
	GtkWidget * label_name;
	GtkWidget * entry_name;
	GtkWidget * hbox_path;
	GtkWidget * label_path;
	GtkWidget * entry_path;
	GtkWidget * nb;
	GtkWidget * dismiss_btn;

	GtkWidget * vbox_file;
	GtkWidget * label_file;
	GtkWidget * table_file;
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * entry;

	GtkWidget * vbox_id3v2;
	GtkWidget * label_id3v2;
	GtkWidget * table_id3v2;

	GtkWidget * vbox_vorbis;
	GtkWidget * label_vorbis;
	GtkWidget * table_vorbis;

	GtkWidget * vbox_flac;
	GtkWidget * label_flac;
	GtkWidget * table_flac;


	if ((fdec = file_decoder_new()) == NULL) {
		fprintf(stderr, "show_file_info: error: file_decoder_new() returned NULL\n");
		return;
	}

	if (file_decoder_open(fdec, g_locale_from_utf8(file, -1, NULL, NULL, NULL), 44100)) {
		fprintf(stderr,	"file_decoder_open() failed on %s\n",
			g_locale_from_utf8(file, -1, NULL, NULL, NULL));
		return;
	}

	info_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_transient_for(GTK_WINDOW(info_window), GTK_WINDOW(main_window));
        gtk_window_set_title(GTK_WINDOW(info_window), _("File info"));
	gtk_window_set_position(GTK_WINDOW(info_window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(info_window), TRUE);
        gtk_container_set_border_width(GTK_CONTAINER(info_window), 5);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(info_window), vbox);

	table = gtk_table_new(2, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 4);

	hbox_name = gtk_hbox_new(FALSE, 0);
	label_name = gtk_label_new(_("Track:"));
	gtk_box_pack_start(GTK_BOX(hbox_name), label_name, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox_name, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 5, 2);

	entry_name = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry_name), name);
	gtk_editable_set_editable(GTK_EDITABLE(entry_name), FALSE);
	gtk_table_attach(GTK_TABLE(table), entry_name, 1, 2, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 2);

	hbox_path = gtk_hbox_new(FALSE, 0);
	label_path = gtk_label_new(_("File:"));
	gtk_box_pack_start(GTK_BOX(hbox_path), label_path, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox_path, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 2);

	entry_path = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry_path), file);
	gtk_editable_set_editable(GTK_EDITABLE(entry_path), FALSE);
	gtk_table_attach(GTK_TABLE(table), entry_path, 1, 2, 1, 2,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 2);

	nb = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), nb, TRUE, TRUE, 10);

	/* Audio data notebook page */

	vbox_file = gtk_vbox_new(FALSE, 4);
	table_file = gtk_table_new(6, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_file), table_file, TRUE, TRUE, 10);
	label_file = gtk_label_new(_("Audio data"));
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_file, label_file);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Format:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
	assembly_format_label(str, fdec->fileinfo.format_major, fdec->fileinfo.format_minor);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Length:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
	sample2time(fdec->fileinfo.sample_rate, fdec->fileinfo.total_samples, str, 0);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 1, 2,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Samplerate:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
	sprintf(str, _("%ld Hz"), fdec->fileinfo.sample_rate);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Channel count:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
	if (fdec->fileinfo.is_mono)
		strcpy(str, _("MONO"));
	else
		strcpy(str, _("STEREO"));
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 3, 4,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Bandwidth:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 4, 5, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
	sprintf(str, _("%.1f kbit/s"), fdec->fileinfo.bps / 1000.0f);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 4, 5,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Total samples:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 5, 6, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
	sprintf(str, "%lld", fdec->fileinfo.total_samples);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 5, 6,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);


	/* ID3v2 notebook page */

	vbox_id3v2 = gtk_vbox_new(FALSE, 4);
	table_id3v2 = gtk_table_new(6, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_id3v2), table_id3v2, TRUE, TRUE, 10);
	label_id3v2 = gtk_label_new(_("ID3v2 tags"));
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_id3v2, label_id3v2);


	/* TODO */


	/* Vorbis comments notebook page */

	vbox_vorbis = gtk_vbox_new(FALSE, 4);
	table_vorbis = gtk_table_new(6, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_vorbis), table_vorbis, TRUE, TRUE, 10);
	label_vorbis = gtk_label_new(_("Vorbis comments"));
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_vorbis, label_vorbis);

	/* TODO */


	/* FLAC metadata notebook page */

	vbox_flac = gtk_vbox_new(FALSE, 4);
	table_flac = gtk_table_new(6, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_flac), table_flac, TRUE, TRUE, 10);
	label_flac = gtk_label_new(_("FLAC metadata"));
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_flac, label_flac);

	/* TODO */



	/* end of notebook stuff */

	dismiss_btn = gtk_button_new_with_label(_("Dismiss"));
	g_signal_connect(dismiss_btn, "clicked", G_CALLBACK(dismiss), NULL);
	gtk_box_pack_end(GTK_BOX(vbox), dismiss_btn, FALSE, FALSE, 4);

	gtk_widget_show_all(info_window);

	file_decoder_close(fdec);
	file_decoder_delete(fdec);
}
