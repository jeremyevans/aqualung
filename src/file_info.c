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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#ifdef HAVE_ID3
#include <id3tag.h>
#endif /* HAVE_ID3 */

#ifdef HAVE_FLAC
#include <FLAC/format.h>
#include <FLAC/metadata.h>
#endif /* HAVE_FLAC */

#include "common.h"
#include "file_decoder.h"
#include "music_browser.h"
#include "gui_main.h"
#include "trashlist.h"
#include "i18n.h"
#include "file_info.h"


/* import destination codes */
#define IMPORT_DEST_ARTIST   1
#define IMPORT_DEST_RECORD   2
#define IMPORT_DEST_TITLE    3
#define IMPORT_DEST_NUMBER   4
#define IMPORT_DEST_COMMENT  5
#define IMPORT_DEST_RVA      6

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

GtkWidget * info_window = NULL;
trashlist_t * fileinfo_trash = NULL;


float convf(char * s) {

        float val, pow;
        int i, sign;

        for (i = 0; s[i] == ' ' || s[i] == '\n' || s[i] == '\t'; i++);
        sign = 1;
        if (s[i] == '+' || s[i] == '-')
                sign = (s[i++] == '+') ? 1 : -1;
        for (val = 0; s[i] >= '0' && s[i] <= '9'; i++)
                val = 10 * val + s[i] - '0';
        if ((s[i] == '.') || (s[i] == ','))
                i++;
        for (pow = 1; s[i] >= '0' && s[i] <= '9'; i++) {
                val = 10 * val + s[i] - '0';
                pow *= 10;
        }
        return(sign * val / pow);
}


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
	return TRUE;
}


int
info_window_close(GtkWidget * widget, gpointer * data) {

	info_window = NULL;
	trashlist_free(fileinfo_trash);
	fileinfo_trash = NULL;
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


#ifdef HAVE_ID3
static void
show_rva2(struct id3_tag * tag, GtkWidget * table, int * cnt,
	  int is_called_from_browser, GtkTreeModel * model, GtkTreeIter track_iter) {

	struct id3_frame * frame;
	char str[MAXLEN];

	frame = id3_tag_findframe(tag, "RVA2", 0);
	if (frame) {
		id3_latin1_t const * id;
		id3_byte_t const * data;
		id3_length_t length;

		enum {
			CHANNEL_OTHER         = 0x00,
			CHANNEL_MASTER_VOLUME = 0x01,
			CHANNEL_FRONT_RIGHT   = 0x02,
			CHANNEL_FRONT_LEFT    = 0x03,
			CHANNEL_BACK_RIGHT    = 0x04,
			CHANNEL_BACK_LEFT     = 0x05,
			CHANNEL_FRONT_CENTRE  = 0x06,
			CHANNEL_BACK_CENTRE   = 0x07,
			CHANNEL_SUBWOOFER     = 0x08
		};

		id = id3_field_getlatin1(id3_frame_field(frame, 0));
		data = id3_field_getbinarydata(id3_frame_field(frame, 1), &length);

		assert(id && data);
		while (length >= 4) {
			unsigned int peak_bytes;

			peak_bytes = (data[3] + 7) / 8;
			if (4 + peak_bytes > length)
				break;

			if (data[0] == CHANNEL_MASTER_VOLUME) {
				signed int voladj_fixed;
				double voladj_float;

				voladj_fixed = (data[1] << 8) | (data[2] << 0);
				voladj_fixed |= -(voladj_fixed & 0x8000);

				voladj_float = (double) voladj_fixed / 512;

				snprintf(str, MAXLEN-1, "%+.1f dB (tagged by %s)",
					 voladj_float, id);

				if (!is_called_from_browser) {
					append_table(table, cnt, _("Relative Volume"), str, NULL, NULL);
				} else {
					import_data_t * data = import_data_new();
					trashlist_add(fileinfo_trash, data);

					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_RVA;
					data->fval = voladj_float;
					append_table(table, cnt, _("Relative Volume"), str,
						     _("Import as RVA"), data);
				}
				break;
			}

			data   += 4 + peak_bytes;
			length -= 4 + peak_bytes;
		}
	}
}


static void
show_id3(struct id3_tag const * tag, GtkWidget * table, int * cnt,
	 int is_called_from_browser, GtkTreeModel * model, GtkTreeIter track_iter) {

	unsigned int i;
	struct id3_frame const * frame;
	id3_ucs4_t const * ucs4;
	id3_utf8_t * utf8;
	char str[MAXLEN];

	static struct {
		char const * id;
		char const * label;
	} const info[] = {
		{ ID3_FRAME_TITLE,  X_("Title") },
		{ "TIT3",           0 },  /* Subtitle */
		{ "TCOP",           0 },  /* Copyright */
		{ "TPRO",           0 },  /* Produced */
		{ "TCOM",           X_("Composer") },
		{ ID3_FRAME_ARTIST, X_("Artist") },
		{ "TPE2",           X_("Orchestra") },
		{ "TPE3",           X_("Conductor") },
		{ "TEXT",           X_("Lyricist") },
		{ ID3_FRAME_ALBUM,  X_("Album") },
		{ ID3_FRAME_TRACK,  X_("Track") },
		{ ID3_FRAME_YEAR,   X_("Year") },
		{ "TPUB",           X_("Publisher") },
		{ ID3_FRAME_GENRE,  X_("Genre") },
		{ "TRSN",           X_("Station") },
		{ "TENC",           X_("Encoder") }
	};

	/* text information */

	for (i = 0; i < sizeof(info) / sizeof(info[0]); ++i) {

		union id3_field const * field;
		unsigned int nstrings, j;

		frame = id3_tag_findframe(tag, info[i].id, 0);
		if (frame == 0)
			continue;

		field = id3_frame_field(frame, 1);
		nstrings = id3_field_getnstrings(field);

		for (j = 0; j < nstrings; ++j) {
			ucs4 = id3_field_getstrings(field, j);
			assert(ucs4);

			if (strcmp(info[i].id, ID3_FRAME_GENRE) == 0)
				ucs4 = id3_genre_name(ucs4);

			utf8 = id3_ucs4_utf8duplicate(ucs4);
			if (utf8 == 0)
				goto fail;

			if (j == 0 && info[i].label) {
				snprintf(str, MAXLEN-1, "%s:", gettext(info[i].label));
				if (!is_called_from_browser) {
					append_table(table, cnt, str, utf8, NULL, NULL);
				} else {
					import_data_t * data = import_data_new();
					trashlist_add(fileinfo_trash, data);

					if (strcmp(info[i].id, ID3_FRAME_TITLE) == 0) {

						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_TITLE;
						strncpy(data->str, utf8, MAXLEN-1);
						append_table(table, cnt, str, utf8,
							     _("Import as Title"), data);
					} else
					if (strcmp(info[i].id, ID3_FRAME_ARTIST) == 0) {

						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_ARTIST;
						strncpy(data->str, utf8, MAXLEN-1);
						append_table(table, cnt, str, utf8,
							     _("Import as Artist"), data);
					} else
					if (strcmp(info[i].id, ID3_FRAME_ALBUM) == 0) {

						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_RECORD;
						strncpy(data->str, utf8, MAXLEN-1);
						append_table(table, cnt, str, utf8,
							     _("Import as Record"), data);
					} else
					if (strcmp(info[i].id, ID3_FRAME_TRACK) == 0) {

						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_NUMBER;
						strncpy(data->str, utf8, MAXLEN-1);
						append_table(table, cnt, str, utf8,
							     _("Import as Track number"), data);
					} else
					if ((strcmp(info[i].id, "TCOM") == 0) ||
					    (strcmp(info[i].id, "TPE2") == 0) ||
					    (strcmp(info[i].id, "TPE3") == 0) ||
					    (strcmp(info[i].id, "TEXT") == 0) ||
					    (strcmp(info[i].id, ID3_FRAME_YEAR) == 0) ||
					    (strcmp(info[i].id, "TPUB") == 0) ||
					    (strcmp(info[i].id, ID3_FRAME_GENRE) == 0) ||
					    (strcmp(info[i].id, "TRSN") == 0) ||
					    (strcmp(info[i].id, "TENC") == 0)) {

						char tmp[MAXLEN];
						
						snprintf(tmp, MAXLEN-1, "%s %s", str, utf8);
						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_COMMENT;
						strncpy(data->str, tmp, MAXLEN-1);
						append_table(table, cnt, str, utf8,
							     _("Add to Comments"), data);
					}
				}
			} else {
				if (strcmp(info[i].id, "TCOP") == 0 ||
				    strcmp(info[i].id, "TPRO") == 0) {

					snprintf(str, MAXLEN-1, "%s",
						 (info[i].id[1] == 'C') ?
                                               _("Copyright (C)") : _("Produced (P)"));

					if (!is_called_from_browser) {
						append_table(table, cnt, str, utf8, NULL, NULL);
					} else {
						import_data_t * data = import_data_new();
						char tmp[MAXLEN];
						
						trashlist_add(fileinfo_trash, data);

						snprintf(tmp, MAXLEN-1, "%s: %s", str, utf8);
						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_COMMENT;
						strncpy(data->str, tmp, MAXLEN-1);
						append_table(table, cnt, str, utf8,
							     _("Add to Comments"), data);
					}
				} else {
					if (!is_called_from_browser) {
						append_table(table, cnt, "", utf8, NULL, NULL);
					} else {
						import_data_t * data = import_data_new();
						trashlist_add(fileinfo_trash, data);
						
						data->model = model;
						data->track_iter = track_iter;
						data->dest_type = IMPORT_DEST_COMMENT;
						strncpy(data->str, utf8, MAXLEN-1);
						append_table(table, cnt, "", utf8,
							     _("Add to Comments"), data);
					}
				}
			}
			free(utf8);
		}
	}

	/* comments */

	i = 0;
	while ((frame = id3_tag_findframe(tag, ID3_FRAME_COMMENT, i++))) {
		ucs4 = id3_field_getstring(id3_frame_field(frame, 2));
		assert(ucs4);

		if (*ucs4)
			continue;

		ucs4 = id3_field_getfullstring(id3_frame_field(frame, 3));
		assert(ucs4);

		utf8 = id3_ucs4_utf8duplicate(ucs4);
		if (utf8 == 0)
			goto fail;

		sprintf(str, "%s:", _("Comment"));
		if (!is_called_from_browser) {
			append_table(table, cnt, str, utf8, NULL, NULL);
		} else {
			import_data_t * data = import_data_new();
			char tmp[MAXLEN];
			
			trashlist_add(fileinfo_trash, data);
			
			snprintf(tmp, MAXLEN-1, "%s %s", str, utf8);
			data->model = model;
			data->track_iter = track_iter;
			data->dest_type = IMPORT_DEST_COMMENT;
			strncpy(data->str, tmp, MAXLEN-1);
			append_table(table, cnt, str, utf8, _("Add to Comments"), data);
		}
		free(utf8);
		break;
	}

	if (0) {
	fail:
		fprintf(stderr, "show_id3(): error: memory allocation error in libid3\n");
	}
}
#endif /* HAVE_ID3 */


#ifdef HAVE_FLAC
void
build_flac_vc(FLAC__StreamMetadata * flacmeta, GtkWidget * table_flac, int * cnt,
	      int is_called_from_browser, GtkTreeModel * model, GtkTreeIter track_iter) {

	char field[MAXLEN];
	char str[MAXLEN];
	int i,j,k;

	for (i = 0; i < flacmeta->data.vorbis_comment.num_comments; i++) {
		
		for (j = 0; (flacmeta->data.vorbis_comment.comments[i].entry[j] != '=') &&
			     (j < MAXLEN-1); j++) {
			
			field[j] = (j == 0) ?
				toupper(flacmeta->data.vorbis_comment.comments[i].entry[j]) :
				tolower(flacmeta->data.vorbis_comment.comments[i].entry[j]);
		}
		field[j++] = ':';
		field[j] = '\0';

		for (k = 0; (j < flacmeta->data.vorbis_comment.comments[i].length) &&
			     (k < MAXLEN-1); j++) {

			str[k++] = flacmeta->data.vorbis_comment.comments[i].entry[j];
		}
		str[k] = '\0';

		if (!is_called_from_browser) {
			append_table(table_flac, cnt, field, str, NULL, NULL);
		} else {
			import_data_t * data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			
			if (strcmp(field, "Title:") == 0) {
				
				data->model = model;
				data->track_iter = track_iter;
				data->dest_type = IMPORT_DEST_TITLE;
				strncpy(data->str, str, MAXLEN-1);
				append_table(table_flac, cnt, field, str, _("Import as Title"), data);
			} else
			if (strcmp(field, "Album:") == 0) {
				
				data->model = model;
				data->track_iter = track_iter;
				data->dest_type = IMPORT_DEST_RECORD;
				strncpy(data->str, str, MAXLEN-1);
				append_table(table_flac, cnt, field, str, _("Import as Record"), data);
			} else
			if (strcmp(field, "Artist:") == 0) {
				
				data->model = model;
				data->track_iter = track_iter;
				data->dest_type = IMPORT_DEST_ARTIST;
				strncpy(data->str, str, MAXLEN-1);
				append_table(table_flac, cnt, field, str, _("Import as Artist"), data);
			} else
			if (strcmp(field, "Replaygain_track_gain:") == 0) {
				
				data->model = model;
				data->track_iter = track_iter;
				data->dest_type = IMPORT_DEST_RVA;
				data->fval = convf(str);
				append_table(table_flac, cnt, field, str, _("Import as RVA"), data);
			} else {
				char tmp[MAXLEN];
				
				snprintf(tmp, MAXLEN-1, "%s %s", field, str);
				data->model = model;
				data->track_iter = track_iter;
				data->dest_type = IMPORT_DEST_COMMENT;
				strncpy(data->str, tmp, MAXLEN-1);
				append_table(table_flac, cnt, field, str,
					     _("Add to Comments"), data);
			}
		}
	}


	for (j = 0; j < flacmeta->data.vorbis_comment.vendor_string.length; j++) {
		
		field[j] = flacmeta->data.vorbis_comment.vendor_string.entry[j];
	}
	field[j] = '\0';
	if (!is_called_from_browser) {
		append_table(table_flac, cnt, _("Vendor:"), field, NULL, NULL);
	} else {
		import_data_t * data = import_data_new();
		char tmp[MAXLEN];
		
		trashlist_add(fileinfo_trash, data);

		snprintf(tmp, MAXLEN-1, "%s %s", _("Vendor:"), field);
		data->model = model;
		data->track_iter = track_iter;
		data->dest_type = IMPORT_DEST_COMMENT;
		strncpy(data->str, tmp, MAXLEN-1);
		append_table(table_flac, cnt, _("Vendor:"), field, _("Add to Comments"), data);
	}
}
#endif /* HAVE_FLAC */


void
show_file_info(char * name, char * file, int is_called_from_browser,
	       GtkTreeModel * model, GtkTreeIter track_iter) {

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


	if (info_window != NULL) {
		gtk_widget_destroy(info_window);
		info_window = NULL;
	}

	if (fileinfo_trash != NULL) {
		trashlist_free(fileinfo_trash);
		fileinfo_trash = NULL;
	}
	fileinfo_trash = trashlist_new();

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
        gtk_window_set_title(GTK_WINDOW(info_window), _("File info"));
	gtk_window_set_position(GTK_WINDOW(info_window), GTK_WIN_POS_CENTER);
	gtk_widget_set_size_request(GTK_WIDGET(info_window), 500, -1);
	g_signal_connect(G_OBJECT(info_window), "delete_event",
			 G_CALLBACK(info_window_close), NULL);
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


#ifdef HAVE_ID3
	{

		struct id3_file * id3file;
		struct id3_tag * id3tag;
		int cnt = 0;

		if ((id3file = id3_file_open(g_locale_from_utf8(file, -1, NULL, NULL, NULL),
					     ID3_FILE_MODE_READONLY)) != NULL) {
			if ((id3tag = id3_file_tag(id3file)) != NULL) {

				/* ID3v2 notebook page */
				
				vbox_id3v2 = gtk_vbox_new(FALSE, 4);
				table_id3v2 = gtk_table_new(0, 3, FALSE);
				gtk_box_pack_start(GTK_BOX(vbox_id3v2), table_id3v2,
						   TRUE, TRUE, 10);
				label_id3v2 = gtk_label_new(_("ID3v2 tags"));
				gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_id3v2,
							 label_id3v2);

				show_id3(id3tag, table_id3v2, &cnt,
					 is_called_from_browser, model, track_iter);
				show_rva2(id3tag, table_id3v2, &cnt,
					  is_called_from_browser, model, track_iter);

				if (cnt == 0) {
					gtk_notebook_remove_page(GTK_NOTEBOOK(nb),
					  gtk_notebook_get_current_page(GTK_NOTEBOOK(nb)));
				}
			}
		}
	}
#endif /* HAVE_ID3 */

#ifdef HAVE_OGG_VORBIS
	if (fdec->file_lib == VORBIS_LIB) {

		/* Vorbis comments notebook page */

		vorbis_comment * vc = ov_comment(&(fdec->vf), -1);
		char field[MAXLEN];
		int cnt;
		int i, j;
		
		vbox_vorbis = gtk_vbox_new(FALSE, 4);
		table_vorbis = gtk_table_new(0, 3, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox_vorbis), table_vorbis, TRUE, TRUE, 10);
		label_vorbis = gtk_label_new(_("Vorbis comments"));
		gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_vorbis, label_vorbis);
	
		for (cnt = 0; cnt < vc->comments; ) {

			for (i = 0; (vc->user_comments[cnt][i] != '=') &&
				     (vc->user_comments[cnt][i] != '\0') &&
				     (i < MAXLEN-2); i++) {

				field[i] = (i == 0) ? toupper(vc->user_comments[cnt][i]) :
					tolower(vc->user_comments[cnt][i]);
			}
			field[i++] = ':';
			field[i] = '\0';
			for (j = 0; (vc->user_comments[cnt][i] != '\0') && (j < MAXLEN-1); i++) {
				str[j++] = vc->user_comments[cnt][i];
			}
			str[j] = '\0';

			if (!is_called_from_browser) {
				append_table(table_vorbis, &cnt, field, str, NULL, NULL);
			} else {
				import_data_t * data = import_data_new();
				trashlist_add(fileinfo_trash, data);
				
				if (strcmp(field, "Title:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_TITLE;
					strncpy(data->str, str, MAXLEN-1);
					append_table(table_vorbis, &cnt, field, str,
						     _("Import as Title"), data);
				} else
				if (strcmp(field, "Album:") == 0) {
						
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_RECORD;
					strncpy(data->str, str, MAXLEN-1);
					append_table(table_vorbis, &cnt, field, str,
						     _("Import as Record"), data);
				} else
				if (strcmp(field, "Artist:") == 0) {
					
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_ARTIST;
					strncpy(data->str, str, MAXLEN-1);
					append_table(table_vorbis, &cnt, field, str,
						     _("Import as Artist"), data);
				} else
				if (strcmp(field, "Replaygain_track_gain:") == 0) {
						
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_RVA;
					data->fval = convf(str);
					append_table(table_vorbis, &cnt, field, str,
						     _("Import as RVA"), data);
				} else {
					char tmp[MAXLEN];
					
					snprintf(tmp, MAXLEN-1, "%s %s", field, str);
					data->model = model;
					data->track_iter = track_iter;
					data->dest_type = IMPORT_DEST_COMMENT;
					strncpy(data->str, tmp, MAXLEN-1);
					append_table(table_vorbis, &cnt, field, str,
						     _("Add to Comments"), data);
				}
			}
		}

		strncpy(str, vc->vendor, MAXLEN-1);
		if (!is_called_from_browser) {
			append_table(table_vorbis, &cnt, _("Vendor:"), str, NULL, NULL);
		} else {
			import_data_t * data = import_data_new();
			char tmp[MAXLEN];
			
			trashlist_add(fileinfo_trash, data);

			snprintf(tmp, MAXLEN-1, "%s %s", _("Vendor:"), str);
			data->model = model;
			data->track_iter = track_iter;
			data->dest_type = IMPORT_DEST_COMMENT;
			strncpy(data->str, tmp, MAXLEN-1);
			append_table(table_vorbis, &cnt, _("Vendor:"), str, _("Add to Comments"), data);
		}
	}
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_FLAC
	if (fdec->file_lib == FLAC_LIB) {

		/* FLAC metadata notebook page */
		
		FLAC__Metadata_SimpleIterator * iter = FLAC__metadata_simple_iterator_new();
		FLAC__StreamMetadata * flacmeta = NULL;
		int cnt = 0;

		if (!FLAC__metadata_simple_iterator_init(iter,
		    g_locale_from_utf8(file, -1, NULL, NULL, NULL), false, false)) {
			fprintf(stderr,
				"show_file_info(): error: "
				"FLAC__metadata_simple_iterator_init() failed on %s\n",
				g_locale_from_utf8(file, -1, NULL, NULL, NULL));
			return;
		}

		vbox_flac = gtk_vbox_new(FALSE, 4);
		table_flac = gtk_table_new(0, 3, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox_flac), table_flac, TRUE, TRUE, 10);
		label_flac = gtk_label_new(_("FLAC metadata"));
		gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_flac, label_flac);

		while (1) {
			flacmeta = FLAC__metadata_simple_iterator_get_block(iter);
			if (flacmeta == NULL)
				break;

			/* process a VORBIS_COMMENT metadata block */
			if (flacmeta->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {

				build_flac_vc(flacmeta, table_flac, &cnt,
					      is_called_from_browser, model, track_iter);
			}

			FLAC__metadata_object_delete(flacmeta);
			if (!FLAC__metadata_simple_iterator_next(iter))
				break;
		}

		FLAC__metadata_simple_iterator_delete(iter);
	}
#endif /* HAVE_FLAC */

	/* end of notebook stuff */

	dismiss_btn = gtk_button_new_with_label(_("Dismiss"));
	g_signal_connect(dismiss_btn, "clicked", G_CALLBACK(dismiss), NULL);
	gtk_box_pack_end(GTK_BOX(vbox), dismiss_btn, FALSE, FALSE, 4);

	gtk_widget_show_all(info_window);

	file_decoder_close(fdec);
	file_decoder_delete(fdec);
}
