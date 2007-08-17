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


#define BUFSIZE 10240


extern GtkWidget * browser_window;

typedef struct {

	char * infile;

	char * artist;
	char * album;
	char * title;
	int year;
	int no;       

} export_item_t;


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
export_free(export_t * export) {

#ifdef _WIN32
	g_mutex_free(export->mutex);
#endif /* _WIN32 */

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

void
export_item_free(export_item_t * item) {

	free(item->infile);
	free(item->artist);
	free(item->album);
	free(item->title);
	free(item);
}

gboolean
export_finish(gpointer user_data) {

	export_t * export = (export_t *)user_data;
	GSList * node;

	if (export->progbar_tag > 0) {
		g_source_remove(export->progbar_tag);
	}

	if (export->prog_window) {
		unregister_toplevel_window(export->prog_window);
		gtk_widget_destroy(export->prog_window);
		export->prog_window = NULL;
	}

	for (node = export->slist; node; node = node->next) {
		export_item_free((export_item_t *)node->data);
	}

	g_slist_free(export->slist);
        export_free(export);

	return FALSE;
}

void
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

	str[j] = '\0';

	strcpy(buf, str);
	g_free(str);

	buf[limit] = '\0';
}

void
export_make_path_unique(char * dir, char * file) {

	char test[MAXLEN];
	char i = '2';

	snprintf(test, MAXLEN-1, "%s/%s", dir, file);

	while (g_file_test(test, G_FILE_TEST_EXISTS) && i <= 'z') {

		dir[strlen(dir)-1] = i;
		snprintf(test, MAXLEN-1, "%s/%s", dir, file);

		if ((i >= '2' && i < '9') || (i >= 'a' && i <= 'z')) {
			++i;
		} else {
			i = 'a';
		}
	}
}

int
export_item_set_path(export_t * export, export_item_t * item, char * path, char * ext, int index) {

	char d_artist[MAXLEN];
	char d_album[MAXLEN];
	char track[MAXLEN];
	char buf[3*MAXLEN];
	char str_no[16];
	char str_index[16];

	d_artist[0] = '\0';
	d_album[0] = '\0';
	track[0] = '\0';
	buf[0] = '\0';

	strcat(buf, export->outdir);

	if (export->dir_for_artist) {
		strncpy(d_artist, item->artist, MAXLEN-1);
		export_compress_str(d_artist, export->dir_len_limit);
		strcat(buf, "/");
		strcat(buf, d_artist);
		if (!is_dir(buf) && mkdir(buf, S_IRUSR | S_IWUSR | S_IXUSR) < 0) {
			fprintf(stderr, "mkdir: %s: %s\n", buf, strerror(errno));
			return -1;
		}
	}

	if (export->dir_for_album) {
		strncpy(d_album, item->album, MAXLEN-1);
		export_compress_str(d_album, export->dir_len_limit);
		strcat(buf, "/");
		strcat(buf, d_album);
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

	if (export->dir_for_album && g_file_test(path, G_FILE_TEST_EXISTS)) {
		export_make_path_unique(buf, track);
		if (!is_dir(buf) && mkdir(buf, S_IRUSR | S_IWUSR | S_IXUSR) < 0) {
			fprintf(stderr, "mkdir: %s: %s\n", buf, strerror(errno));
			return -1;
		}
		snprintf(path, MAXLEN-1, "%s/%s", buf, track);
	}

	return 0;
}

gboolean
update_progbar_ratio(gpointer user_data) {

	export_t * export = (export_t *)user_data;

	if (export->prog_window) {
		char tmp[16];
		snprintf(tmp, 15, "%d%%", (int)(export->ratio * 100));
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(export->progbar), export->ratio);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(export->progbar), tmp);
	} else {
		return FALSE;
	}

	return TRUE;
}

gboolean
set_prog_src_file_entry_idle(gpointer user_data) {

	export_t * export = (export_t *)user_data;

	if (export->prog_window) {

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

	if (export->prog_window) {

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
export_item(export_t * export, export_item_t * item, int index) {

	file_decoder_t * fdec;
	file_encoder_t * fenc;
	encoder_mode_t mode;
	char * ext = "raw";

	float buf[2*BUFSIZE];
	int n_read;
	long long samples_read = 0;

	memset(&mode, 0, sizeof(encoder_mode_t));

	switch (export->format) {
	case ENC_SNDFILE_LIB: ext = "wav"; break;
	case ENC_FLAC_LIB:    ext = "flac"; break;
	case ENC_VORBIS_LIB:  ext = "ogg"; break;
	case ENC_LAME_LIB:    ext = "mp3"; break;
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
		strncpy(mode.meta.artist, item->artist, MAXLEN-1);
		strncpy(mode.meta.album, item->album, MAXLEN-1);
		strncpy(mode.meta.title, item->title, MAXLEN-1);
		snprintf(mode.meta.year, MAXLEN-1, "%d", item->year);
		snprintf(mode.meta.track, MAXLEN-1, "%d", item->no);
	}
	
	fenc = file_encoder_new();

	if (file_encoder_open(fenc, &mode)) {
		return;
	}

	while (!export->cancelled) {

		n_read = file_decoder_read(fdec, buf, BUFSIZE);
		file_encoder_write(fenc, buf, n_read);
		
		samples_read += n_read;

		export->ratio = (double)samples_read / fdec->fileinfo.total_samples;

		if (n_read < BUFSIZE) {
			break;
		}
	}

	file_decoder_close(fdec);
	file_encoder_close(fenc);
	file_decoder_delete(fdec);
	file_encoder_delete(fenc);
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
				(GtkWidget *)data);
}

GtkWidget *
export_create_format_combo(void) {

	GtkWidget * combo = gtk_combo_box_new_text();
#if defined(HAVE_SNDFILE) || defined(HAVE_FLAC)
	int n = 0;
#endif /* HAVE_SNDFILE || HAVE_FLAC */

#ifdef HAVE_SNDFILE
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "WAV");
	++n;
#endif /* HAVE_SNDFILE */
#ifdef HAVE_FLAC
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "FLAC");
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), n);
#endif /* HAVE_FLAC */
#ifdef HAVE_VORBISENC
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Ogg Vorbis");
#endif /* HAVE_VORBISENC */
#ifdef HAVE_LAME
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "MP3");
#endif /* HAVE_LAME */

	if (gtk_combo_box_get_active(GTK_COMBO_BOX(combo)) == -1) {
	    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), FALSE);
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
		gtk_range_set_value(GTK_RANGE(export->bitrate_scale), 5);
	}
	if (strcmp(text, "Ogg Vorbis") == 0) {
		gtk_widget_show(export->bitrate_scale);
		gtk_widget_show(export->bitrate_label);
		gtk_label_set_text(GTK_LABEL(export->bitrate_label), _("Bitrate [kbps]:"));
		gtk_widget_show(export->bitrate_value_label);
		gtk_widget_hide(export->vbr_check);
		gtk_widget_show(export->meta_check);

		gtk_range_set_range(GTK_RANGE(export->bitrate_scale), 32, 320);
		gtk_range_set_value(GTK_RANGE(export->bitrate_scale), 256);
	}
	if (strcmp(text, "MP3") == 0) {
		gtk_widget_show(export->bitrate_scale);
		gtk_widget_show(export->bitrate_label);
		gtk_label_set_text(GTK_LABEL(export->bitrate_label), _("Bitrate [kbps]:"));
		gtk_widget_show(export->bitrate_value_label);
		gtk_widget_show(export->vbr_check);
		gtk_widget_show(export->meta_check);

		gtk_range_set_range(GTK_RANGE(export->bitrate_scale), 32, 320);
		gtk_range_set_value(GTK_RANGE(export->bitrate_scale), 256);
	}

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
                                             GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
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

	insert_label_entry_browse(table, _("Target directory:"), &outdir_entry, NULL, 0, 1, export_browse_cb);

        export->check_dir_artist = gtk_check_button_new_with_label(_("Create subdirectories for artists"));
        gtk_widget_set_name(export->check_dir_artist, "check_on_notebook");
        gtk_table_attach(GTK_TABLE(table), export->check_dir_artist, 0, 3, 1, 2, GTK_FILL, GTK_FILL, 5, 5);
        g_signal_connect(G_OBJECT(export->check_dir_artist), "toggled",
			 G_CALLBACK(check_dir_limit_toggled), export);

        export->check_dir_album = gtk_check_button_new_with_label(_("Create subdirectories for albums"));
        gtk_widget_set_name(export->check_dir_album, "check_on_notebook");
        gtk_table_attach(GTK_TABLE(table), export->check_dir_album, 0, 3, 2, 3, GTK_FILL, GTK_FILL, 5, 5);
        g_signal_connect(G_OBJECT(export->check_dir_album), "toggled",
			 G_CALLBACK(check_dir_limit_toggled), export);

	insert_label_spin_with_limits(table, _("Subdirectory name\nlength limit:"), &export->dirlen_spin, 16, 4, 64, 3, 4);
	gtk_widget_set_sensitive(export->dirlen_spin, FALSE);

        help_button = gtk_button_new_from_stock(GTK_STOCK_HELP); 
	g_signal_connect(help_button, "clicked", G_CALLBACK(export_format_help_cb), export);
	insert_label_entry_button(table, _("Filename template:"), &templ_entry, "track%i.%x", help_button, 4, 5);

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
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(export->vbr_check), TRUE);

        export->meta_check = gtk_check_button_new_with_label(_("Tag files with metadata"));
        gtk_widget_set_name(export->meta_check, "check_on_notebook");
        gtk_table_attach(GTK_TABLE(table), export->meta_check, 0, 2, 3, 4,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 4);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(export->meta_check), TRUE);

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

		set_option_from_entry(templ_entry, export->template, MAXLEN);
		export->format = export_get_format_from_combo(export->format_combo);
		export->bitrate = gtk_range_get_value(GTK_RANGE(export->bitrate_scale));
		set_option_from_toggle(export->vbr_check, &export->vbr);
		set_option_from_toggle(export->meta_check, &export->write_meta);
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

void
export_prog_window_close(GtkWidget * widget, gpointer user_data) {

	export_t * export = (export_t *)user_data;

	export->cancelled = 1;

	if (export->prog_window) {
		unregister_toplevel_window(export->prog_window);
		gtk_widget_destroy(export->prog_window);
		export->prog_window = NULL;
	}
}

void
export_progress_window(export_t * export) {

	GtkWidget * table;
	GtkWidget * vbox;
	GtkWidget * hbuttonbox;


	export->prog_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	register_toplevel_window(export->prog_window, TOP_WIN_SKIN | TOP_WIN_TRAY);
        gtk_window_set_title(GTK_WINDOW(export->prog_window), _("Exporting files"));
        gtk_window_set_position(GTK_WINDOW(export->prog_window), GTK_WIN_POS_CENTER);
        gtk_window_resize(GTK_WINDOW(export->prog_window), 430, 110);
        g_signal_connect(G_OBJECT(export->prog_window), "delete_event",
                         G_CALLBACK(export_prog_window_close), export);
        gtk_container_set_border_width(GTK_CONTAINER(export->prog_window), 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(export->prog_window), vbox);

	table = gtk_table_new(3, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

	insert_label_entry(table, _("Source file:"), &export->prog_file_entry1, NULL, 0, 1, FALSE);
	insert_label_entry(table, _("Target file:"), &export->prog_file_entry2, NULL, 1, 2, FALSE);

	export->progbar = gtk_progress_bar_new();
        gtk_table_attach(GTK_TABLE(table), export->progbar, 0, 2, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 5);

        gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), FALSE, TRUE, 5);

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        export->prog_cancel_button = gui_stock_label_button(_("Abort"), GTK_STOCK_CANCEL);
        g_signal_connect(export->prog_cancel_button, "clicked", G_CALLBACK(export_prog_window_close), export);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), export->prog_cancel_button);

        gtk_widget_grab_focus(export->prog_cancel_button);

        gtk_widget_show_all(export->prog_window);
}


void
export_start(export_t * export) {

	if (export_dialog(export)) {
		export_progress_window(export);
		export->progbar_tag = g_timeout_add(250, update_progbar_ratio, export);
		AQUALUNG_THREAD_CREATE(export->thread_id, NULL, export_thread, export);
	} else {
		export_finish(export);
	}
}

