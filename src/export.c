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
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gtk/gtk.h>

#ifdef _WIN32
#include <glib.h>
#else
#include <pthread.h>
#endif /* _WIN32 */

#include "common.h"
#include "i18n.h"
#include "utils.h"
#include "utils_gui.h"
#include "decoder/file_decoder.h"
#include "encoder/file_encoder.h"
#include "encoder/enc_lame.h"
#include "store_file.h"
#include "playlist.h"
#include "export.h"
#include "options.h"

#ifdef HAVE_EXPORT

#define BUFSIZE 10240

extern GtkWidget * browser_window;
extern options_t options;

GtkWidget * export_window;
int export_slot_count;

typedef struct {

	char * infile;

	char * artist;
	char * album;
	char * title;
	int year;
	int no;       

} export_item_t;


char *
export_compress_str(char * buf, int limit) {

	char * str;
	char * valid = "abcdefghijklmnopqrstuvwxyz0123456789";
	int i = 0;
	int j = 0;

	str = g_ascii_strdown(buf, -1);
	g_strcanon(str, valid, '_');

	for (i = 0; str[i]; i++) {
		if (str[i] == '_') {
			continue;
		}
		str[j] = str[i];
		j++;
	}

	if (limit < j) {
		str[limit] = '\0';
	} else {
		str[j] = '\0';
	}

	return str;
}

int
export_map_has(export_map_t * map, char * val) {

	export_map_t * pmap;

	for (pmap = map; pmap; pmap = pmap->next) {
		if (!strcmp(pmap->val, val)) {
			return 1;
		}
	}

	return 0;
}

export_map_t *
export_map_new(export_map_t * _map, char * key, int limit) {

	export_map_t * map;
	char * tmp;
	int i = '2';

	if ((map = (export_map_t *)malloc(sizeof(export_map_t))) == NULL) {
		fprintf(stderr, "export_map_new(): malloc error\n");
		return NULL;
	}

	map->next = NULL;
	map->key = strdup(key);

	tmp = export_compress_str(key, limit);

	while (export_map_has(_map, tmp) && i <= 'z') {

		tmp[strlen(tmp)-1] = i;

		if ((i >= '2' && i < '9') || (i >= 'a' && i <= 'z')) {
			++i;
		} else {
			i = 'a';
		}
	}

	map->val = strdup(tmp);

	g_free(tmp);
	return map;
}

char *
export_map_put(export_map_t ** map, char * key, int limit) {

	export_map_t * pmap;
	export_map_t * _pmap;

	if (key == NULL || key[0] == '\0') {
		return NULL;
	}

	if (*map == NULL) {
		*map = export_map_new(NULL, key, limit);
		return (*map)->val;
	} else {

		for (_pmap = pmap = *map; pmap; _pmap = pmap, pmap = pmap->next) {

			char * key1 = g_utf8_casefold(key, -1);
			char * key2 = g_utf8_casefold(pmap->key, -1);

			if (!g_utf8_collate(key1, key2)) {
				g_free(key1);
				g_free(key2);
				return pmap->val;
			}

			g_free(key1);
			g_free(key2);
		}

		_pmap->next = export_map_new(*map, key, limit);
		return _pmap->next->val;
	}
}

void
export_map_free(export_map_t * map) {

	export_map_t * pmap;

	for (pmap = map; pmap; map = pmap) {
		pmap = map->next;
		free(map->key);
		free(map->val);
		free(map);
	}
}


export_t *
export_new(void) {

	export_t * export;

	if ((export = (export_t *)calloc(1, sizeof(export_t))) == NULL) {
		fprintf(stderr, "export_new: calloc error\n");
		return NULL;
	}

#ifdef _WIN32
        export->mutex = g_mutex_new();
#endif /* _WIN32 */

	return export;
}

void
export_item_free(export_item_t * item) {

	free(item->infile);
	free(item->artist);
	free(item->album);
	free(item->title);
	free(item);
}

void
export_free(export_t * export) {

	GSList * node;

#ifdef _WIN32
	g_mutex_free(export->mutex);
#endif /* _WIN32 */

	for (node = export->slist; node; node = node->next) {
		export_item_free((export_item_t *)node->data);
	}

	g_slist_free(export->slist);

	export_map_free(export->artist_map);
	export_map_free(export->record_map);
	free(export);
}

void
export_append_item(export_t * export, char * infile,
		   char * artist, char * album, char * title, int year, int no) {

	export_item_t * item;

	if ((item = (export_item_t *)calloc(1, sizeof(export_item_t))) == NULL) {
		fprintf(stderr, "export_append_item: calloc error\n");
		return;
	}

	item->infile = strdup(infile);

	item->artist = (artist && artist[0] != '\0') ? strdup(artist) : strdup(_("Unknown Artist"));
	item->album = (album && album[0] != '\0') ? strdup(album) : strdup(_("Unknown Album"));
	item->title = (title && title[0] != '\0') ? strdup(title) : strdup(_("Unknown Track"));
	item->year = year;
	item->no = no;

	export->slist = g_slist_append(export->slist, item);
}

gboolean
export_finish(gpointer user_data) {

	export_t * export = (export_t *)user_data;

	gtk_window_resize(GTK_WINDOW(export_window),
			  export_window->allocation.width,
			  export_window->allocation.height - export->slot->allocation.height);

	gtk_widget_destroy(export->slot);
	export->slot = NULL;

	g_source_remove(export->progbar_tag);

        export_free(export);

	--export_slot_count;

	if (export_slot_count == 0) {
		unregister_toplevel_window(export_window);
		gtk_widget_destroy(export_window);
		export_window = NULL;
	}

	return FALSE;
}

int
export_item_set_path(export_t * export, export_item_t * item, char * path, char * ext, int index) {

	char track[MAXLEN];
	char buf[3*MAXLEN];
	char str_no[16];
	char str_index[16];

	track[0] = '\0';
	buf[0] = '\0';

	strcat(buf, export->outdir);

	if (export->dir_for_artist) {
		strcat(buf, "/");
		strcat(buf, export_map_put(&export->artist_map, item->artist, export->dir_len_limit));
		if (!is_dir(buf) && mkdir(buf, S_IRUSR | S_IWUSR | S_IXUSR) < 0) {
			fprintf(stderr, "mkdir: %s: %s\n", buf, strerror(errno));
			return -1;
		}
	}

	if (export->dir_for_album) {
		strcat(buf, "/");
		strcat(buf, export_map_put(&export->record_map, item->album, export->dir_len_limit));
		if (!is_dir(buf) && mkdir(buf, S_IRUSR | S_IWUSR | S_IXUSR) < 0) {
			fprintf(stderr, "mkdir: %s: %s\n", buf, strerror(errno));
			return -1;
		}
	}

	snprintf(str_no, 15, "%02d", item->no);
	snprintf(str_index, 15, "%04d", index);

	make_string_va(track, export->template, '%', "%", 'a', item->artist, 'r', item->album,
		       't', item->title, 'n', str_no, 'x', ext, 'i', str_index, 0);

	snprintf(path, MAXLEN-1, "%s/%s", buf, track);
	return 0;
}

gboolean
update_progbar_ratio(gpointer user_data) {

	export_t * export = (export_t *)user_data;

	if (export->slot) {

		char tmp[16];

		AQUALUNG_MUTEX_LOCK(export->mutex);
		snprintf(tmp, 15, "%d%%", (int)(export->ratio * 100));
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(export->progbar), export->ratio);
		AQUALUNG_MUTEX_UNLOCK(export->mutex);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(export->progbar), tmp);
	}

	return TRUE;
}

gboolean
set_prog_src_file_entry_idle(gpointer user_data) {

	export_t * export = (export_t *)user_data;

	if (export->slot) {

		char * utf8 = NULL;

                AQUALUNG_MUTEX_LOCK(export->mutex);
                utf8 = g_filename_display_name(export->file1);
                AQUALUNG_MUTEX_UNLOCK(export->mutex);

		gtk_entry_set_text(GTK_ENTRY(export->prog_file_entry1), utf8);
		gtk_widget_grab_focus(export->prog_cancel_button);
		g_free(utf8);
	}

	return FALSE;
}

gboolean
set_prog_trg_file_entry_idle(gpointer user_data) {

	export_t * export = (export_t *)user_data;

	if (export->slot) {

		char * utf8 = NULL;

                AQUALUNG_MUTEX_LOCK(export->mutex);
                utf8 = g_filename_display_name(export->file2);
                AQUALUNG_MUTEX_UNLOCK(export->mutex);

		gtk_entry_set_text(GTK_ENTRY(export->prog_file_entry2), utf8);
		gtk_widget_grab_focus(export->prog_cancel_button);
		g_free(utf8);
	}

	return FALSE;
}

void
set_prog_src_file_entry(export_t * export, char * file) {

	AQUALUNG_MUTEX_LOCK(export->mutex);
	strncpy(export->file1, file, MAXLEN-1);
        AQUALUNG_MUTEX_UNLOCK(export->mutex);

	g_idle_add(set_prog_src_file_entry_idle, export);
}

void
set_prog_trg_file_entry(export_t * export, char * file) {

	AQUALUNG_MUTEX_LOCK(export->mutex);
	strncpy(export->file2, file, MAXLEN-1);
        AQUALUNG_MUTEX_UNLOCK(export->mutex);

	g_idle_add(set_prog_trg_file_entry_idle, export);
}


void
export_meta_amend_frame(metadata_t * meta, int tag, int type, export_item_t * item) {

	char * str;
	meta_frame_t * frame;

	/* see whether particular frame type is available in this tag */
	if (!meta_get_fieldname_embedded(tag, type, &str)) {
		return;
	}

	/* if yes, check for existance */
	frame = metadata_get_frame_by_tag_and_type(meta, tag, type, NULL);
	if (frame != NULL) {
		return;
	}

	/* not found, add it with content from export item */
	frame = meta_frame_new();
	frame->tag = tag;
	frame->type = type;
	switch (type) {
	case META_FIELD_TITLE:
		frame->field_val = strdup(item->title);
		break;
	case META_FIELD_ARTIST:
		frame->field_val = strdup(item->artist);
		break;
	case META_FIELD_ALBUM:
		frame->field_val = strdup(item->album);
		break;
	case META_FIELD_DATE:
		{
			char str_year[6];
			snprintf(str_year, 5, "%d", item->year);
			frame->field_val = strdup(str_year);
		}
		break;
	case META_FIELD_TRACKNO:
		frame->int_val = item->no;
		break;
	}

	metadata_add_frame(meta, frame);
}


/* Add metadata fields stored in Music Store / Playlist
 * if they were not transferred from source file metadata.
 */
void
export_meta_amend_stored_fields(metadata_t * meta, int tags, export_item_t * item) {

	int tag = META_TAG_MAX;

	/* iterate on possible output tags */
	while (tag) {

		if ((tags & tag) == 0) {
			tag >>= 1;
			continue;
		}

		if (strcmp(item->title, _("Unknown Track")) != 0) {
			export_meta_amend_frame(meta, tag, META_FIELD_TITLE, item);
		}
		if (strcmp(item->artist, _("Unknown Artist")) != 0) {
			export_meta_amend_frame(meta, tag, META_FIELD_ARTIST, item);
		}
		if (strcmp(item->album, _("Unknown Album")) != 0) {
			export_meta_amend_frame(meta, tag, META_FIELD_ALBUM, item);
		}
		if (item->year != 0) {
			export_meta_amend_frame(meta, tag, META_FIELD_DATE, item);
		}
		if (item->no != 0) {
			export_meta_amend_frame(meta, tag, META_FIELD_TRACKNO, item);
		}

		tag >>= 1;
	}
}


void
export_item(export_t * export, export_item_t * item, int index) {

	file_decoder_t * fdec;
	file_encoder_t * fenc;
	encoder_mode_t mode;
	char * ext = "raw";
	int tags = 0;

	float buf[2*BUFSIZE];
	int n_read;
	long long samples_read = 0;

	memset(&mode, 0, sizeof(encoder_mode_t));

	switch (export->format) {
	case ENC_SNDFILE_LIB:
		ext = "wav";
		tags = 0;
		break;
	case ENC_FLAC_LIB:
		ext = "flac";
#ifdef HAVE_FLAC_8
		tags = META_TAG_OXC | META_TAG_FLAC_APIC;
#else
		tags = META_TAG_OXC;
#endif /* HAVE_FLAC_8 */
		break;
	case ENC_VORBIS_LIB:
		ext = "ogg";
		tags = META_TAG_OXC;
		break;
	case ENC_LAME_LIB:
		ext = "mp3";
		tags = META_TAG_ID3v1 | META_TAG_ID3v2 | META_TAG_APE;
		break;
	}

	fdec = file_decoder_new();

	if (file_decoder_open(fdec, item->infile)) {
		return;
	}

	if (export_item_set_path(export, item, mode.filename, ext, index) < 0) {
		return;
	}

	set_prog_src_file_entry(export, item->infile);
	set_prog_trg_file_entry(export, mode.filename);

	mode.file_lib = export->format;
	mode.sample_rate = fdec->fileinfo.sample_rate;
	mode.channels = fdec->fileinfo.channels;

	if (mode.file_lib == ENC_FLAC_LIB) {
		mode.clevel = export->bitrate;
	} else if (mode.file_lib == ENC_VORBIS_LIB) {
		mode.bps = export->bitrate * 1000;
	} else if (mode.file_lib == ENC_LAME_LIB) {
		mode.bps = export->bitrate * 1000;
		mode.vbr = export->vbr;
	}

	mode.write_meta = export->write_meta;
	if (mode.write_meta) {
		mode.meta = metadata_clone(fdec->meta, tags);
		export_meta_amend_stored_fields(mode.meta, tags, item);
	}
	
	fenc = file_encoder_new();

	if (file_encoder_open(fenc, &mode)) {
		return;
	}

	while (!export->cancelled) {

		n_read = file_decoder_read(fdec, buf, BUFSIZE);
		file_encoder_write(fenc, buf, n_read);
		
		samples_read += n_read;

		AQUALUNG_MUTEX_LOCK(export->mutex);
		export->ratio = (double)samples_read / fdec->fileinfo.total_samples;
		AQUALUNG_MUTEX_UNLOCK(export->mutex);

		if (n_read < BUFSIZE) {
			break;
		}
	}

	file_decoder_close(fdec);
	file_encoder_close(fenc);
	file_decoder_delete(fdec);
	file_encoder_delete(fenc);
	if (mode.meta != NULL) {
		metadata_free(mode.meta);
	}
}

void *
export_thread(void * arg) {

	export_t * export = (export_t *)arg;
	GSList * node;
	int index = 0;

	AQUALUNG_THREAD_DETACH();

	for (node = export->slist; node; node = node->next) {

		if (export->cancelled) {
			break;
		}

		++index;
		export_item(export, (export_item_t *)node->data, index);
	}

	g_idle_add(export_finish, export);

	return NULL;
}

void
export_browse_cb(GtkButton * button, gpointer data) {

	file_chooser_with_entry(_("Please select the directory for exported files."),
				browser_window,
				GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
				FILE_CHOOSER_FILTER_NONE,
				(GtkWidget *)data,
				options.exportdir);
}

GtkWidget *
export_create_format_combo(void) {

	GtkWidget * combo = gtk_combo_box_new_text();
	int n = -1;

#ifdef HAVE_SNDFILE
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "WAV");
	++n;
#endif /* HAVE_SNDFILE */
#ifdef HAVE_FLAC
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "FLAC");
	++n;
#endif /* HAVE_FLAC */
#ifdef HAVE_VORBISENC
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Ogg Vorbis");
	++n;
#endif /* HAVE_VORBISENC */
#ifdef HAVE_LAME
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "MP3");
	++n;
#endif /* HAVE_LAME */

	if (n >= options.export_file_format) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo), options.export_file_format);
	} else {
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	}

	return combo;
}


/* returns file_lib value */
int
export_get_format_from_combo(GtkWidget * combo) {

	int file_lib = -1;
	gchar * text = gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo));
	if (strcmp(text, "WAV") == 0) {
		file_lib = ENC_SNDFILE_LIB;
	}
	if (strcmp(text, "FLAC") == 0) {
		file_lib = ENC_FLAC_LIB;
	}
	if (strcmp(text, "Ogg Vorbis") == 0) {
		file_lib = ENC_VORBIS_LIB;
	}
	if (strcmp(text, "MP3") == 0) {
		file_lib = ENC_LAME_LIB;
	}
	g_free(text);
	return file_lib;
}

void
export_format_combo_changed(GtkWidget * widget, gpointer data) {

	export_t * export = (export_t *)data;
	gchar * text = gtk_combo_box_get_active_text(GTK_COMBO_BOX(widget));

	if (strcmp(text, "WAV") == 0) {
		gtk_widget_hide(export->bitrate_scale);
		gtk_widget_hide(export->bitrate_label);
		gtk_widget_hide(export->bitrate_value_label);
		gtk_widget_hide(export->vbr_check);
		gtk_widget_hide(export->meta_check);
	}
	if (strcmp(text, "FLAC") == 0) {
		gtk_widget_show(export->bitrate_scale);
		gtk_widget_show(export->bitrate_label);
		gtk_label_set_text(GTK_LABEL(export->bitrate_label), _("Compression level:"));
		gtk_widget_show(export->bitrate_value_label);
		gtk_widget_hide(export->vbr_check);
		gtk_widget_show(export->meta_check);

		gtk_range_set_range(GTK_RANGE(export->bitrate_scale), 0, 8);
		gtk_range_set_value(GTK_RANGE(export->bitrate_scale), options.export_bitrate);
	}
	if (strcmp(text, "Ogg Vorbis") == 0) {
		gtk_widget_show(export->bitrate_scale);
		gtk_widget_show(export->bitrate_label);
		gtk_label_set_text(GTK_LABEL(export->bitrate_label), _("Bitrate [kbps]:"));
		gtk_widget_show(export->bitrate_value_label);
		gtk_widget_hide(export->vbr_check);
		gtk_widget_show(export->meta_check);

		gtk_range_set_range(GTK_RANGE(export->bitrate_scale), 32, 320);
		gtk_range_set_value(GTK_RANGE(export->bitrate_scale), options.export_bitrate);
	}
	if (strcmp(text, "MP3") == 0) {
		gtk_widget_show(export->bitrate_scale);
		gtk_widget_show(export->bitrate_label);
		gtk_label_set_text(GTK_LABEL(export->bitrate_label), _("Bitrate [kbps]:"));
		gtk_widget_show(export->bitrate_value_label);
		gtk_widget_show(export->vbr_check);
		gtk_widget_show(export->meta_check);

		gtk_range_set_range(GTK_RANGE(export->bitrate_scale), 32, 320);
		gtk_range_set_value(GTK_RANGE(export->bitrate_scale), options.export_bitrate);
	}

        options.export_file_format = export_get_format_from_combo(widget);

	g_free(text);
}

void
export_bitrate_changed(GtkRange * range, gpointer data) {

	export_t * export = (export_t *)data;
	float val = gtk_range_get_value(range);
	gchar * text = gtk_combo_box_get_active_text(GTK_COMBO_BOX(export->format_combo));

	if (strcmp(text, "FLAC") == 0) {
		int i = (int)val;
		char str[256];
		switch (i) {
		case 0:
		case 8:
			snprintf(str, 255, "%d (%s)", i, (i == 0) ? _("fast") : _("best"));
			gtk_label_set_text(GTK_LABEL(export->bitrate_value_label), str);
			break;
		default:
			snprintf(str, 255, "%d", i);
			gtk_label_set_text(GTK_LABEL(export->bitrate_value_label), str);
			break;
		}
	}
	if (strcmp(text, "Ogg Vorbis") == 0) {
		int i = (int)val;
		char str[256];
		snprintf(str, 255, "%d", i);
		gtk_label_set_text(GTK_LABEL(export->bitrate_value_label), str);
	}
	if (strcmp(text, "MP3") == 0) {
		int i = (int)val;
		char str[256];
#ifdef HAVE_LAME
		i = lame_encoder_validate_bitrate(i, 0);
#endif /* HAVE_LAME */
		snprintf(str, 255, "%d", i);
		gtk_label_set_text(GTK_LABEL(export->bitrate_value_label), str);
	}
	g_free(text);
}

void
check_dir_limit_toggled(GtkToggleButton * toggle, gpointer data) {

	export_t * export = (export_t *)data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(export->check_dir_artist)) ||
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(export->check_dir_album))) {
		gtk_widget_set_sensitive(export->dirlen_spin, TRUE);
	} else {
		gtk_widget_set_sensitive(export->dirlen_spin, FALSE);
	}
}

void
export_format_help_cb(GtkButton * button, gpointer user_data) {

	message_dialog(_("Help"),
		       ((export_t *)user_data)->dialog,
		       GTK_MESSAGE_INFO,
		       GTK_BUTTONS_OK,
		       NULL,
		       _("\nThe template string you enter here will be used to\n"
			 "construct the filename of the exported files. The Artist,\n"
			 "Record and Track names are denoted by %%%%a, %%%%r and %%%%t.\n"
			 "The track number and format-dependent file extension are\n"
			 "denoted by %%%%n and %%%%x, respectively. The flag %%%%i gives\n"
			 "an identifier which is unique within an export session."));
}

int
export_dialog(export_t * export) {

	GtkWidget * help_button;
	GtkWidget * outdir_entry;
	GtkWidget * templ_entry;

        GtkWidget * table;
        GtkWidget * hbox;
        GtkWidget * frame;


        export->dialog = gtk_dialog_new_with_buttons(_("Export files"),
                                             GTK_WINDOW(browser_window),
                                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                             GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                             NULL);

        gtk_window_set_position(GTK_WINDOW(export->dialog), GTK_WIN_POS_CENTER);
        gtk_window_set_default_size(GTK_WINDOW(export->dialog), 400, -1);
        gtk_dialog_set_default_response(GTK_DIALOG(export->dialog), GTK_RESPONSE_REJECT);

	frame = gtk_frame_new(_("Location and filename"));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(export->dialog)->vbox), frame, FALSE, FALSE, 2);
        gtk_container_set_border_width(GTK_CONTAINER(frame), 5);

	table = gtk_table_new(5, 2, FALSE);
        gtk_container_add(GTK_CONTAINER(frame), table);

	insert_label_entry_browse(table, _("Target directory:"), &outdir_entry, options.exportdir, 0, 1, export_browse_cb);

        export->check_dir_artist = gtk_check_button_new_with_label(_("Create subdirectories for artists"));
        gtk_widget_set_name(export->check_dir_artist, "check_on_notebook");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(export->check_dir_artist), options.export_subdir_artist);
        gtk_table_attach(GTK_TABLE(table), export->check_dir_artist, 0, 3, 1, 2, GTK_FILL, GTK_FILL, 5, 5);
        g_signal_connect(G_OBJECT(export->check_dir_artist), "toggled",
			 G_CALLBACK(check_dir_limit_toggled), export);

        export->check_dir_album = gtk_check_button_new_with_label(_("Create subdirectories for albums"));
        gtk_widget_set_name(export->check_dir_album, "check_on_notebook");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(export->check_dir_album), options.export_subdir_album);
        gtk_table_attach(GTK_TABLE(table), export->check_dir_album, 0, 3, 2, 3, GTK_FILL, GTK_FILL, 5, 5);
        g_signal_connect(G_OBJECT(export->check_dir_album), "toggled",
			 G_CALLBACK(check_dir_limit_toggled), export);

	insert_label_spin_with_limits(table, _("Subdirectory name\nlength limit:"), &export->dirlen_spin, options.export_subdir_limit, 4, 64, 3, 4);
	gtk_widget_set_sensitive(export->dirlen_spin, options.export_subdir_artist || options.export_subdir_album);

        help_button = gtk_button_new_from_stock(GTK_STOCK_HELP); 
	g_signal_connect(help_button, "clicked", G_CALLBACK(export_format_help_cb), export);
	insert_label_entry_button(table, _("Filename template:"), &templ_entry, options.export_template, help_button, 4, 5);

	frame = gtk_frame_new(_("Format"));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(export->dialog)->vbox), frame, FALSE, FALSE, 2);
        gtk_container_set_border_width(GTK_CONTAINER(frame), 5);

        table = gtk_table_new(4, 2, TRUE);
        gtk_container_add(GTK_CONTAINER(frame), table);

        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("File format:")), FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 4);

	export->format_combo = export_create_format_combo();
        g_signal_connect(G_OBJECT(export->format_combo), "changed",
			 G_CALLBACK(export_format_combo_changed), export);
        gtk_table_attach(GTK_TABLE(table), export->format_combo, 1, 2, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 2);

        hbox = gtk_hbox_new(FALSE, 0);
	export->bitrate_label = gtk_label_new(_("Compression level:"));
        gtk_box_pack_start(GTK_BOX(hbox), export->bitrate_label, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 0);

	export->bitrate_scale = gtk_hscale_new_with_range(0, 8, 1);
        g_signal_connect(G_OBJECT(export->bitrate_scale), "value-changed",
			 G_CALLBACK(export_bitrate_changed), export);
	gtk_scale_set_draw_value(GTK_SCALE(export->bitrate_scale), FALSE);
	gtk_scale_set_digits(GTK_SCALE(export->bitrate_scale), 0);
        gtk_widget_set_size_request(export->bitrate_scale, 180, -1);
        gtk_table_attach(GTK_TABLE(table), export->bitrate_scale, 1, 2, 1, 2,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 0);

	export->bitrate_value_label = gtk_label_new("0 (fast)");
        gtk_table_attach(GTK_TABLE(table), export->bitrate_value_label, 1, 2, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 0);

        export->vbr_check = gtk_check_button_new_with_label(_("VBR encoding"));
        gtk_widget_set_name(export->vbr_check, "check_on_notebook");
        gtk_table_attach(GTK_TABLE(table), export->vbr_check, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(export->vbr_check), options.export_vbr);

        export->meta_check = gtk_check_button_new_with_label(_("Tag files with metadata"));
        gtk_widget_set_name(export->meta_check, "check_on_notebook");
        gtk_table_attach(GTK_TABLE(table), export->meta_check, 0, 2, 3, 4,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 4);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(export->meta_check), options.export_metadata);

	gtk_widget_show_all(export->dialog);
	export_format_combo_changed(export->format_combo, export);

 display:

	export->outdir[0] = '\0';
        if (aqualung_dialog_run(GTK_DIALOG(export->dialog)) == GTK_RESPONSE_ACCEPT) {

		char * poutdir = g_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(outdir_entry)), -1, NULL, NULL, NULL);

                if ((poutdir == NULL) || (poutdir[0] == '\0')) {
                        gtk_widget_grab_focus(outdir_entry);
			g_free(poutdir);
                        goto display;
                }

		normalize_filename(poutdir, export->outdir);
		g_free(poutdir);

		if (strlen(gtk_entry_get_text(GTK_ENTRY(templ_entry))) == 0) {
                        gtk_widget_grab_focus(templ_entry);
                        goto display;
		}

		if (access(export->outdir, R_OK | W_OK) != 0) {
			message_dialog(_("Error"),
				       export->dialog,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK,
				       NULL,
				       _("\nDestination directory is not read-write accessible!"));
			
                        gtk_widget_grab_focus(outdir_entry);
			goto display;
		}

		strncpy(options.exportdir, export->outdir, MAXLEN-1);
		set_option_from_entry(templ_entry, export->template, MAXLEN);
		set_option_from_entry(templ_entry, options.export_template, MAXLEN);
		options.export_file_format = export->format = export_get_format_from_combo(export->format_combo);
		options.export_bitrate = export->bitrate = gtk_range_get_value(GTK_RANGE(export->bitrate_scale));
		set_option_from_toggle(export->check_dir_artist, &options.export_subdir_artist);
		set_option_from_toggle(export->check_dir_album, &options.export_subdir_album);
		set_option_from_spin(export->dirlen_spin, &options.export_subdir_limit);
		set_option_from_toggle(export->vbr_check, &export->vbr);
                options.export_vbr = export->vbr;
		set_option_from_toggle(export->meta_check, &export->write_meta);
                options.export_metadata = export->write_meta;
		set_option_from_toggle(export->check_dir_artist, &export->dir_for_artist);
		set_option_from_toggle(export->check_dir_album, &export->dir_for_album);

		if (export->dir_for_artist || export->dir_for_album) {
			set_option_from_spin(export->dirlen_spin, &export->dir_len_limit);
		} else {
			export->dir_len_limit = MAXLEN-1;
		}

		gtk_widget_destroy(export->dialog);
		return 1;
	} else {
		gtk_widget_destroy(export->dialog);
		return 0;
	}
}

gboolean
export_window_event(GtkWidget * widget, GdkEvent * event, gpointer * data) {

	if (event->type == GDK_DELETE) {
		gtk_window_iconify(GTK_WINDOW(export_window));
		return TRUE;
	}

	return FALSE;
}

void
export_cancel_event(GtkButton * button, gpointer data) {

	export_t * export = (export_t *)data;

        export->cancelled = 1;
}

void
create_export_window() {

	GtkWidget * vbox;

	export_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	register_toplevel_window(export_window, TOP_WIN_SKIN | TOP_WIN_TRAY);
        gtk_window_set_title(GTK_WINDOW(export_window), _("Exporting files"));
        gtk_window_set_position(GTK_WINDOW(export_window), GTK_WIN_POS_CENTER);
        gtk_window_resize(GTK_WINDOW(export_window), 480, 110);
        g_signal_connect(G_OBJECT(export_window), "event",
                         G_CALLBACK(export_window_event), NULL);

        gtk_container_set_border_width(GTK_CONTAINER(export_window), 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(export_window), vbox);

        gtk_widget_show_all(export_window);
}

void
export_progress_window(export_t * export) {

	GtkWidget * vbox;

	++export_slot_count;

	if (export_window == NULL) {
		create_export_window();
	}

	vbox = gtk_bin_get_child(GTK_BIN(export_window));

	export->slot = gtk_table_new(5, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(vbox), export->slot, FALSE, FALSE, 0);

	gtk_table_attach(GTK_TABLE(export->slot), gtk_hseparator_new(), 0, 2, 0, 1,
			 GTK_FILL, GTK_FILL, 5, 5);

	insert_label_entry(export->slot, _("Source file:"), &export->prog_file_entry1, NULL, 1, 2, FALSE);
	insert_label_entry(export->slot, _("Target file:"), &export->prog_file_entry2, NULL, 2, 3, FALSE);

        export->prog_cancel_button = gui_stock_label_button(_("Abort"), GTK_STOCK_CANCEL);
        g_signal_connect(export->prog_cancel_button, "clicked", G_CALLBACK(export_cancel_event), export);
	insert_label_progbar_button(export->slot, _("Progress:"), &export->progbar, export->prog_cancel_button, 3, 4);

	gtk_table_attach(GTK_TABLE(export->slot), gtk_hseparator_new(), 0, 2, 4, 5,
			 GTK_FILL, GTK_FILL, 5, 5);

        gtk_widget_grab_focus(export->prog_cancel_button);

        gtk_widget_show_all(export->slot);
}


int
export_start(export_t * export) {

	if (export_dialog(export)) {
		export_progress_window(export);
		export->progbar_tag = g_timeout_add(250, update_progbar_ratio, export);
		AQUALUNG_THREAD_CREATE(export->thread_id, NULL, export_thread, export);
		return 1;
	}

	export_free(export);
	return 0;
}

#endif /* HAVE_EXPORT */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

