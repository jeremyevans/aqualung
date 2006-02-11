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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_ID3
#include <id3tag.h>
#endif /* HAVE_ID3 */

#ifdef HAVE_FLAC
#include <FLAC/format.h>
#include <FLAC/metadata.h>
#endif /* HAVE_FLAC */

#include "common.h"
#include "core.h"
#include "decoder/file_decoder.h"
#include "music_browser.h"
#include "gui_main.h"
#include "options.h"
#include "trashlist.h"
#include "i18n.h"
#include "meta_decoder.h"
#include "file_info.h"


/* import destination codes */
#define IMPORT_DEST_ARTIST   1
#define IMPORT_DEST_RECORD   2
#define IMPORT_DEST_TITLE    3
#define IMPORT_DEST_NUMBER   4
#define IMPORT_DEST_COMMENT  5
#define IMPORT_DEST_RVA      6


extern options_t options;

typedef struct _import_data_t {
        GtkTreeModel * model;
        GtkTreeIter track_iter;
        int dest_type; /* one of the above codes */
        char str[MAXLEN];
        float fval;
} import_data_t;


extern GtkWidget * main_window;
extern GtkTreeStore * music_store;
extern GtkTreeSelection * music_select;
extern GtkWidget * music_tree;

extern void set_sliders_width(void);

GtkWidget * info_window = NULL;
trashlist_t * fileinfo_trash = NULL;

gint page = 0;     /* current notebook page */
GtkWidget * nb;     /* notebook widget */

import_data_t *
import_data_new(void) {

	import_data_t * data;

	if ((data = calloc(1, sizeof(import_data_t))) == NULL) {
		fprintf(stderr, "error: import_data_new(): calloc error\n");
		return NULL;
	}
	return data;
}


static gint
dismiss(GtkWidget * widget, gpointer data) {

	gtk_widget_destroy(info_window);
	info_window = NULL;
	trashlist_free(fileinfo_trash);
	fileinfo_trash = NULL;
        set_sliders_width();
	return TRUE;
}


int
info_window_close(GtkWidget * widget, gpointer * data) {

	info_window = NULL;
	trashlist_free(fileinfo_trash);
	fileinfo_trash = NULL;
        set_sliders_width();
	return 0;
}


static void
import_button_pressed(GtkWidget * widget, gpointer gptr_data) {

	import_data_t * data = (import_data_t *)gptr_data;
	GtkTreeIter record_iter;
	GtkTreeIter artist_iter;
	GtkTreePath * path;
	char tmp[MAXLEN];
	char * ptmp;
	float ftmp;

	switch (data->dest_type) {
	case IMPORT_DEST_TITLE:
		gtk_tree_store_set(music_store, &(data->track_iter), 0, data->str, -1);
		break;
	case IMPORT_DEST_RECORD:
		gtk_tree_model_iter_parent(data->model, &record_iter, &(data->track_iter));
		gtk_tree_store_set(music_store, &record_iter, 0, data->str, -1);
		break;
	case IMPORT_DEST_ARTIST:
		gtk_tree_model_iter_parent(data->model, &record_iter, &(data->track_iter));
		gtk_tree_model_iter_parent(data->model, &artist_iter, &record_iter);
		gtk_tree_store_set(music_store, &artist_iter, 0, data->str, -1);
		gtk_tree_store_set(music_store, &artist_iter, 1, data->str, -1);
		path = gtk_tree_model_get_path(data->model, &(data->track_iter));
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(music_tree), path, NULL, TRUE, 0.5f, 0.0f);
		break;
	case IMPORT_DEST_NUMBER:
		if (data->str[0] != '0') {
			tmp[0] = '0';
			tmp[1] = '\0';
		} else {
			tmp[0] = '\0';
		}
		strncat(tmp, data->str, MAXLEN-1);
		gtk_tree_store_set(music_store, &(data->track_iter), 1, tmp, -1);
		break;
	case IMPORT_DEST_COMMENT:
		gtk_tree_model_get(data->model, &(data->track_iter), 3, &ptmp, -1);
		tmp[0] = '\0';
		if (ptmp != NULL) {
			strncat(tmp, ptmp, MAXLEN-1);
		}
		if ((tmp[strlen(tmp)-1] != '\n') && (tmp[0] != '\0')) {
			strncat(tmp, "\n", MAXLEN-1);
		}
		strncat(tmp, data->str, MAXLEN-1);
		gtk_tree_store_set(music_store, &(data->track_iter), 3, tmp, -1);
		tree_selection_changed_cb(music_select, NULL);
		break;
	case IMPORT_DEST_RVA:
		ftmp = 1.0f;
		gtk_tree_store_set(music_store, &(data->track_iter), 6, data->fval, -1);
		gtk_tree_store_set(music_store, &(data->track_iter), 7, ftmp, -1);
		break;
	}

	music_store_mark_changed();
}


void
append_table(GtkWidget * table, int * cnt, char * field, char * value,
	     char * importbtn_text, import_data_t * data) {

	GtkWidget * hbox;
	GtkWidget * entry;
	GtkWidget * label;

	GtkWidget * button;

	gtk_table_resize(GTK_TABLE(table), *cnt + 1, 2);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(field);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, *cnt, *cnt+1, GTK_FILL, GTK_FILL, 5, 3);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), value);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);

	if (importbtn_text == NULL) {
		gtk_table_attach(GTK_TABLE(table), entry, 1, 3, *cnt, *cnt+1,
				 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);
	} else {
		button = gtk_button_new_with_label(importbtn_text);
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(import_button_pressed), (gpointer)data);
		gtk_table_attach(GTK_TABLE(table), entry, 1, 2, *cnt, *cnt+1,
				 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);
		gtk_table_attach(GTK_TABLE(table), button, 2, 3, *cnt, *cnt+1,
				 GTK_FILL, GTK_FILL, 5, 3);
	}
	
	(*cnt)++;
}


gint
info_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	switch (kevent->keyval) {

	case GDK_q:
	case GDK_Q:
	case GDK_Escape:
		dismiss(NULL, NULL);
		return TRUE;
		break;
		
	case GDK_Return:
		gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), page);
		page = (page+1) % gtk_notebook_get_n_pages(GTK_NOTEBOOK(nb));
		break;
		
	}

	return FALSE;
}


void
show_file_info(char * name, char * file, int is_called_from_browser,
	       GtkTreeModel * model, GtkTreeIter track_iter) {

        char str[MAXLEN];

	GtkWidget * vbox;
	GtkWidget * table;
	GtkWidget * hbox_name;
	GtkWidget * label_name;
	GtkWidget * entry_name;
	GtkWidget * hbox_path;
	GtkWidget * label_path;
	GtkWidget * entry_path;
	GtkWidget * hbuttonbox;
	GtkWidget * dismiss_btn;

	GtkWidget * vbox_file;
	GtkWidget * label_file;
	GtkWidget * table_file;
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * entry;

#ifdef HAVE_ID3
	GtkWidget * vbox_id3v2;
	GtkWidget * label_id3v2;
	GtkWidget * table_id3v2;
#endif /* HAVE_ID3 */

#ifdef HAVE_OGG_VORBIS
	GtkWidget * vbox_vorbis;
	GtkWidget * label_vorbis;
	GtkWidget * table_vorbis;
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_FLAC
	GtkWidget * vbox_flac;
	GtkWidget * label_flac;
	GtkWidget * table_flac;
#endif /* HAVE_FLAC */

	metadata * meta = meta_new();
#ifdef HAVE_ID3
	id3_tag_data * id3;
#endif /* HAVE_ID3 */
	oggv_comment * oggv;
	int cnt;


	if (info_window != NULL) {
		gtk_widget_destroy(info_window);
		info_window = NULL;
	}

	if (fileinfo_trash != NULL) {
		trashlist_free(fileinfo_trash);
		fileinfo_trash = NULL;
	}
	fileinfo_trash = trashlist_new();

	if (!meta_read(meta, file)) {
		fprintf(stderr, "show_file_info(): meta_read() returned an error\n");
		return;
	}

        set_sliders_width();

	info_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(info_window), _("File info"));
	gtk_window_set_transient_for(GTK_WINDOW(info_window), GTK_WINDOW(main_window));
	gtk_window_set_position(GTK_WINDOW(info_window), GTK_WIN_POS_CENTER);
	gtk_widget_set_size_request(GTK_WIDGET(info_window), 500, -1);
	g_signal_connect(G_OBJECT(info_window), "delete_event",
			 G_CALLBACK(info_window_close), NULL);
        g_signal_connect(G_OBJECT(info_window), "key_press_event",
			 G_CALLBACK(info_window_key_pressed), NULL);
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
	page = 0;


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
	assembly_format_label(str, meta->format_major, meta->format_minor);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Length:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
	sample2time(meta->sample_rate, meta->total_samples, str, 0);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 1, 2,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Samplerate:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
	sprintf(str, _("%ld Hz"), meta->sample_rate);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Channel count:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
	if (meta->is_mono)
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
	sprintf(str, _("%.1f kbit/s"), meta->bps / 1000.0f);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 4, 5,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Total samples:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 5, 6, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
	sprintf(str, "%lld", meta->total_samples);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 5, 6,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);


#ifdef HAVE_ID3
	cnt = 0;
	id3 = meta->id3_root;
	if (id3->next != NULL) {
		id3 = id3->next;
		
		vbox_id3v2 = gtk_vbox_new(FALSE, 4);
		table_id3v2 = gtk_table_new(0, 3, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox_id3v2), table_id3v2,
				   TRUE, TRUE, 10);
		label_id3v2 = gtk_label_new(_("ID3v2 tags"));
		gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_id3v2,
					 label_id3v2);
		page++;

		while (id3 != NULL) {
			
			if (!is_called_from_browser) {
				append_table(table_id3v2, &cnt, (char *) id3->label, (char *) id3->utf8,
					     NULL, NULL);
			} else {
				import_data_t * data = import_data_new();
				trashlist_add(fileinfo_trash, data);
				
				if (strcmp(id3->id, ID3_FRAME_TITLE) == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_TITLE;
					strncpy(data->str, (char *) id3->utf8, MAXLEN-1);
					append_table(table_id3v2, &cnt,
						     id3->label, (char *) id3->utf8,
						     _("Import as Title"), data);
				} else
				if (strcmp(id3->id, ID3_FRAME_ARTIST) == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_ARTIST;
					strncpy(data->str, (char *) id3->utf8, MAXLEN-1);
					append_table(table_id3v2, &cnt,
						     id3->label, (char *) id3->utf8,
						     _("Import as Artist"), data);
				} else
				if (strcmp(id3->id, ID3_FRAME_ALBUM) == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_RECORD;
					strncpy(data->str, (char *) id3->utf8, MAXLEN-1);
					append_table(table_id3v2, &cnt,
						     id3->label, (char *) id3->utf8,
						     _("Import as Record"), data);
				} else
				if (strcmp(id3->id, ID3_FRAME_TRACK) == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_NUMBER;
					strncpy(data->str, (char *) id3->utf8, MAXLEN-1);
					append_table(table_id3v2, &cnt,
						     id3->label, (char *) id3->utf8,
						     _("Import as Track number"), data);
				} else
				if (strcmp(id3->id, "RVA2") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_RVA;
					data->fval = id3->fval;
					append_table(table_id3v2, &cnt,
						     _("Relative Volume"), (char *) id3->utf8,
						     _("Import as RVA"), data);
					
				} else {
					char tmp[MAXLEN];
					
					snprintf(tmp, MAXLEN-1, "%s %s",
						 id3->label, id3->utf8);
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_COMMENT;
					strncpy(data->str, tmp, MAXLEN-1);
					append_table(table_id3v2, &cnt,
						     id3->label, (char *) id3->utf8,
						     _("Add to Comments"), data);
				}
			}
			id3 = id3->next;
		}
	}
#endif /* HAVE_ID3 */

#ifdef HAVE_OGG_VORBIS
	cnt = 0;
	oggv = meta->oggv_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		
		vbox_vorbis = gtk_vbox_new(FALSE, 4);
		table_vorbis = gtk_table_new(0, 3, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox_vorbis), table_vorbis, TRUE, TRUE, 10);
		label_vorbis = gtk_label_new(_("Vorbis comments"));
		gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_vorbis, label_vorbis);
		page++;
		
		while (oggv != NULL) {
			
			if (!is_called_from_browser) {
				append_table(table_vorbis, &cnt,
					     oggv->label, oggv->str, NULL, NULL);
			} else {
				import_data_t * data = import_data_new();
				trashlist_add(fileinfo_trash, data);
				
				if (strcmp(oggv->label, "Title:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_TITLE;
					strncpy(data->str, oggv->str, MAXLEN-1);
					append_table(table_vorbis, &cnt,
						     oggv->label, oggv->str,
						     _("Import as Title"), data);
				} else
				if (strcmp(oggv->label, "Album:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_RECORD;
					strncpy(data->str, oggv->str, MAXLEN-1);
					append_table(table_vorbis, &cnt,
						     oggv->label, oggv->str,
						     _("Import as Record"), data);
				} else
				if (strcmp(oggv->label, "Artist:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_ARTIST;
					strncpy(data->str, oggv->str, MAXLEN-1);
					append_table(table_vorbis, &cnt,
						     oggv->label, oggv->str,
						     _("Import as Artist"), data);

				} else if ((strcmp(oggv->label, "Replaygain_track_gain:") == 0) ||
					   (strcmp(oggv->label, "Replaygain_album_gain:") == 0)) {

					char replaygain_label[MAXLEN];

					switch (options.replaygain_tag_to_use) {
					case 0:
						strcpy(replaygain_label, "Replaygain_track_gain:");
						break;
					case 1:
						strcpy(replaygain_label, "Replaygain_album_gain:");
						break;
					default:
						fprintf(stderr, "file_info.c: illegal "
							"replaygain_tag_to_use value -- "
							"please see the programmers\n");
					}
					
					if (strcmp(oggv->label, replaygain_label) == 0) {
						
						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_RVA;
						data->fval = oggv->fval;
						append_table(table_vorbis, &cnt,
							     oggv->label, oggv->str,
							     _("Import as RVA"), data);
					} else {
						char tmp[MAXLEN];
						
						snprintf(tmp, MAXLEN-1, "%s %s",
							 oggv->label, oggv->str);
						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_COMMENT;
						strncpy(data->str, tmp, MAXLEN-1);
						append_table(table_vorbis, &cnt,
							     oggv->label, oggv->str,
							     _("Add to Comments"), data);
					}
				} else {
					char tmp[MAXLEN];
					
					snprintf(tmp, MAXLEN-1, "%s %s",
						 oggv->label, oggv->str);
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_COMMENT;
					strncpy(data->str, tmp, MAXLEN-1);
					append_table(table_vorbis, &cnt,
						     oggv->label, oggv->str,
						     _("Add to Comments"), data);
				}
			}
			oggv = oggv->next;
		}
	}
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_FLAC
	cnt = 0;
	oggv = meta->flac_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		
		vbox_flac = gtk_vbox_new(FALSE, 4);
		table_flac = gtk_table_new(0, 3, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox_flac), table_flac, TRUE, TRUE, 10);
		label_flac = gtk_label_new(_("FLAC metadata"));
		gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_flac, label_flac);
		page++;
		
		while (oggv != NULL) {
			
			if (!is_called_from_browser) {
				append_table(table_flac, &cnt,
					     oggv->label, oggv->str, NULL, NULL);
			} else {
				import_data_t * data = import_data_new();
				trashlist_add(fileinfo_trash, data);
				
				if (strcmp(oggv->label, "Title:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_TITLE;
					strncpy(data->str, oggv->str, MAXLEN-1);
					append_table(table_flac, &cnt,
						     oggv->label, oggv->str,
						     _("Import as Title"), data);
				} else
				if (strcmp(oggv->label, "Album:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_RECORD;
					strncpy(data->str, oggv->str, MAXLEN-1);
					append_table(table_flac, &cnt,
						     oggv->label, oggv->str,
						     _("Import as Record"), data);
				} else
				if (strcmp(oggv->label, "Artist:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_ARTIST;
					strncpy(data->str, oggv->str, MAXLEN-1);
					append_table(table_flac, &cnt,
						     oggv->label, oggv->str,
						     _("Import as Artist"), data);

				} else if ((strcmp(oggv->label, "Replaygain_track_gain:") == 0) ||
					   (strcmp(oggv->label, "Replaygain_album_gain:") == 0)) {

					char replaygain_label[MAXLEN];

					switch (options.replaygain_tag_to_use) {
					case 0:
						strcpy(replaygain_label, "Replaygain_track_gain:");
						break;
					case 1:
						strcpy(replaygain_label, "Replaygain_album_gain:");
						break;
					default:
						fprintf(stderr, "illegal replaygain_tag_to_use value -- "
							"please see the programmers\n");
					}
					
					if (strcmp(oggv->label, replaygain_label) == 0) {
						
						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_RVA;
						data->fval = oggv->fval;
						append_table(table_flac, &cnt,
							     oggv->label, oggv->str,
							     _("Import as RVA"), data);
					} else {
						char tmp[MAXLEN];
						
						snprintf(tmp, MAXLEN-1, "%s %s",
							 oggv->label, oggv->str);
						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_COMMENT;
						strncpy(data->str, tmp, MAXLEN-1);
						append_table(table_flac, &cnt,
							     oggv->label, oggv->str,
							     _("Add to Comments"), data);
					}
				} else {
					char tmp[MAXLEN];
					
					snprintf(tmp, MAXLEN-1, "%s %s",
						 oggv->label, oggv->str);
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_COMMENT;
					strncpy(data->str, tmp, MAXLEN-1);
					append_table(table_flac, &cnt,
						     oggv->label, oggv->str,
						     _("Add to Comments"), data);
				}
			}
			oggv = oggv->next;
		}
	}
#endif /* HAVE_FLAC */

	/* end of notebook stuff */

	gtk_widget_grab_focus(nb);

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, TRUE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        dismiss_btn = gtk_button_new_from_stock (GTK_STOCK_CLOSE); 
	g_signal_connect(dismiss_btn, "clicked", G_CALLBACK(dismiss), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), dismiss_btn);   

	gtk_widget_show_all(info_window);

	meta_free(meta);
}


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

