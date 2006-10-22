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
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gstdio.h>

#ifdef HAVE_MOD_INFO
#include <libmodplug/modplug.h>
#endif /* HAVE_MOD_INFO */

#ifdef HAVE_MOD
#include "decoder/dec_mod.h"
#endif /* HAVE_MOD */

#ifdef HAVE_TAGLIB
#include <id3v1tag.h>
#include <id3v2tag.h>
#include <apetag.h>
#include <xiphcomment.h>

#include <apeitem.h>
#include <attachedpictureframe.h>
#include <relativevolumeframe.h>
#include <textidentificationframe.h>

#include <mpegfile.h>
#include <mpcfile.h>
#include <flacfile.h>
#include <vorbisfile.h>

#include <tmap.h>
#include <tlist.h>
#include <tbytevector.h>
#include <tfile.h>
#include <tstring.h>
#endif /* HAVE_TAGLIB */

#include "common.h"
#include "core.h"
#include "cover.h"
#include "decoder/file_decoder.h"
#include "music_browser.h"
#include "gui_main.h"
#include "options.h"
#include "trashlist.h"
#include "build_store.h"
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
extern GtkWidget* gui_stock_label_button(gchar *label, const gchar *stock);

GtkWidget * fi_event_box;

typedef struct {
        GtkTreeModel * model;
        GtkTreeIter track_iter;
        int dest_type; /* one of the above codes */
        char str[MAXLEN];
        float fval;
} import_data_t;

typedef struct {
	int is_called_from_browser;
	GtkTreeModel * model;
	GtkTreeIter track_iter;
} fileinfo_mode_t;

typedef struct {
	char savefile[MAXLEN];
	unsigned int image_size;
	void * image_data;
} save_pic_t;


#ifdef HAVE_TAGLIB

/* edit modes */
#define EDITABLE_NO    0x00
#define EDITABLE_YES   0x01
#define EDITABLE_GENRE 0x02


/* used in flags below */
#define CREATE_ID3v2 0x02
#define CREATE_APE   0x04
#define REMOVE_ID3v1 0x10
#define REMOVE_ID3v2 0x20
#define REMOVE_APE   0x40

typedef struct {
	TagLib::FLAC::File * taglib_flac_file;
	TagLib::Ogg::Vorbis::File * taglib_oggv_file;
	TagLib::MPEG::File * taglib_mpeg_file;
	TagLib::MPC::File * taglib_mpc_file;

	TagLib::ID3v1::Tag * id3v1_tag;
	TagLib::ID3v2::Tag * id3v2_tag;
	TagLib::APE::Tag * ape_tag;
	TagLib::Ogg::XiphComment * oxc;

	GtkWidget * entry_title;
	GtkWidget * entry_artist;
	GtkWidget * entry_album;
	GtkWidget * entry_track;
	GtkWidget * entry_year;
	GtkWidget * entry_genre;
	GtkWidget * entry_comment;

	GtkNotebook * nb;
	GtkWidget * hbox;
	GtkWidget * hbox_inner;
	fileinfo_mode_t mode;

	int flags;
	int id3v1_page_no;
	int id3v2_page_no;
	int ape_page_no;
	int oxc_page_no;
} save_basic_t;

typedef struct {
	int flags; /* only the exact action associated w/ button */
	save_basic_t * save_basic;
} tagbutton_data_t;

typedef struct {
	int save_page_no; /* only the exact page to save */
	save_basic_t * save_basic;
} savepage_data_t;
#endif /* HAVE_TAGLIB */


extern GtkWidget * main_window;
extern GtkTreeStore * music_store;
extern GtkTreeSelection * music_select;
extern GtkWidget * music_tree;

GtkWidget * info_window = NULL;
trashlist_t * fileinfo_trash = NULL;

gint n_pages = 0;       /* number of notebook pages */
GtkWidget * nb;         /* notebook widget */

#ifdef HAVE_MOD_INFO
file_decoder_t * md_fdec = NULL;
GtkWidget * smp_instr_list;
GtkListStore * smp_instr_list_store = NULL;
void fill_module_info_page(mod_info *mdi, GtkWidget *vbox, char *file);
#endif /* HAVE_MOD_INFO */

import_data_t *
import_data_new(void) {

	import_data_t * data;

	if ((data = (import_data_t *)calloc(1, sizeof(import_data_t))) == NULL) {
		fprintf(stderr, "error: import_data_new(): calloc error\n");
		return NULL;
	}
	return data;
}


#ifdef HAVE_TAGLIB
save_basic_t *
save_basic_new(void) {

	save_basic_t * save_basic = (save_basic_t *)calloc(1, sizeof(save_basic_t));
	if (!save_basic) {
		fprintf(stderr, "error: save_basic_new(): calloc error\n");
		return NULL;
	}

	save_basic->id3v1_page_no = -1;
	save_basic->id3v2_page_no = -1;
	save_basic->ape_page_no = -1;
	save_basic->oxc_page_no = -1;

	return save_basic;
}


tagbutton_data_t *
tagbutton_data_new(void) {

	tagbutton_data_t * tdata = (tagbutton_data_t *)calloc(1, sizeof(tagbutton_data_t));
	if (!tdata) {
		fprintf(stderr, "error: tagbutton_data_new(): calloc error\n");
		return NULL;
	}

	return tdata;
}


savepage_data_t *
savepage_data_new(void) {

	savepage_data_t * sdata = (savepage_data_t *)calloc(1, sizeof(savepage_data_t));
	if (!sdata) {
		fprintf(stderr, "error: savepage_data_new(): calloc error\n");
		return NULL;
	}

	sdata->save_page_no = -1;

	return sdata;
}
#endif /* HAVE_TAGLIB */


gint
dismiss(GtkWidget * widget, gpointer data) {

	metadata * meta = (metadata *)data;
	meta_free(meta);

#ifdef HAVE_MOD_INFO
        if (md_fdec) {
                file_decoder_close(md_fdec);
                file_decoder_delete(md_fdec);
                md_fdec = NULL;
        }
#endif /* HAVE_MOD_INFO */

        gtk_widget_destroy(info_window);
	info_window = NULL;
	trashlist_free(fileinfo_trash);
	fileinfo_trash = NULL;
	return TRUE;
}


gint
info_window_close(GtkWidget * widget, GdkEventAny * event, gpointer data) {

	metadata * meta = (metadata *)data;
	meta_free(meta);

	info_window = NULL;
	trashlist_free(fileinfo_trash);
	fileinfo_trash = NULL;
	return FALSE;
}


gint
info_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent, gpointer data) {

	switch (kevent->keyval) {
	case GDK_Escape:
		dismiss(NULL, data);
		return TRUE;
		break;
	case GDK_Return:
		int page = (gtk_notebook_get_current_page(GTK_NOTEBOOK(nb)) + 1) % n_pages;
		gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), page);
		break;
	}
	return FALSE;
}


char *
lookup_id3v1_genre(int code) {

	switch (code) {
	case 0: return "Blues";
	case 1: return "Classic Rock";
	case 2: return "Country";
	case 3: return "Dance";
	case 4: return "Disco";
	case 5: return "Funk";
	case 6: return "Grunge";
	case 7: return "Hip-Hop";
	case 8: return "Jazz";
	case 9: return "Metal";
	case 10: return "New Age";
	case 11: return "Oldies";
	case 12: return "Other";
	case 13: return "Pop";
	case 14: return "R&B";
	case 15: return "Rap";
	case 16: return "Reggae";
	case 17: return "Rock";
	case 18: return "Techno";
	case 19: return "Industrial";
	case 20: return "Alternative";
	case 21: return "Ska";
	case 22: return "Death Metal";
	case 23: return "Pranks";
	case 24: return "Soundtrack";
	case 25: return "Euro-Techno";
	case 26: return "Ambient";
	case 27: return "Trip-Hop";
	case 28: return "Vocal";
	case 29: return "Jazz+Funk";
	case 30: return "Fusion";
	case 31: return "Trance";
	case 32: return "Classical";
	case 33: return "Instrumental";
	case 34: return "Acid";
	case 35: return "House";
	case 36: return "Game";
	case 37: return "Sound Clip";
	case 38: return "Gospel";
	case 39: return "Noise";
	case 40: return "AlternRock";
	case 41: return "Bass";
	case 42: return "Soul";
	case 43: return "Punk";
	case 44: return "Space";
	case 45: return "Meditative";
	case 46: return "Instrumental Pop";
	case 47: return "Instrumental Rock";
	case 48: return "Ethnic";
	case 49: return "Gothic";
	case 50: return "Darkwave";
	case 51: return "Techno-Industrial";
	case 52: return "Electronic";
	case 53: return "Pop-Folk";
	case 54: return "Eurodance";
	case 55: return "Dream";
	case 56: return "Southern Rock";
	case 57: return "Comedy";
	case 58: return "Cult";
	case 59: return "Gangsta";
	case 60: return "Top 40";
	case 61: return "Christian Rap";
	case 62: return "Pop/Funk";
	case 63: return "Jungle";
	case 64: return "Native American";
	case 65: return "Cabaret";
	case 66: return "New Wave";
	case 67: return "Psychadelic";
	case 68: return "Rave";
	case 69: return "Showtunes";
	case 70: return "Trailer";
	case 71: return "Lo-Fi";
	case 72: return "Tribal";
	case 73: return "Acid Punk";
	case 74: return "Acid Jazz";
	case 75: return "Polka";
	case 76: return "Retro";
	case 77: return "Musical";
	case 78: return "Rock & Roll";
	case 79: return "Hard Rock";
	default: return NULL;
	}
}


void
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
		music_store_mark_changed(&(data->track_iter));
		break;
	case IMPORT_DEST_RECORD:
		gtk_tree_model_iter_parent(data->model, &record_iter, &(data->track_iter));
		gtk_tree_store_set(music_store, &record_iter, 0, data->str, -1);
		music_store_mark_changed(&(data->track_iter));
		break;
	case IMPORT_DEST_ARTIST:
		gtk_tree_model_iter_parent(data->model, &record_iter, &(data->track_iter));
		gtk_tree_model_iter_parent(data->model, &artist_iter, &record_iter);
		gtk_tree_store_set(music_store, &artist_iter, 0, data->str, -1);
		gtk_tree_store_set(music_store, &artist_iter, 1, data->str, -1);
		path = gtk_tree_model_get_path(data->model, &(data->track_iter));
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(music_tree), path, NULL, TRUE, 0.5f, 0.0f);
		music_store_mark_changed(&(data->track_iter));
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
		music_store_mark_changed(&(data->track_iter));
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
		music_store_mark_changed(&(data->track_iter));
		break;
	case IMPORT_DEST_RVA:
		ftmp = 1.0f;
		gtk_tree_store_set(music_store, &(data->track_iter), 6, data->fval, -1);
		gtk_tree_store_set(music_store, &(data->track_iter), 7, ftmp, -1);
		music_store_mark_changed(&(data->track_iter));
		break;
	}
}


/* returns the text entry widget */
GtkWidget *
append_table(GtkWidget * table, int * cnt, int edit_mode, char * field, char * value,
	     char * importbtn_text, import_data_t * data) {

	GtkWidget * hbox;
	GtkWidget * entry = NULL;
	GtkWidget * label;

	GtkWidget * button;

	gtk_table_resize(GTK_TABLE(table), *cnt + 1, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(field);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, *cnt, *cnt+1, GTK_FILL, GTK_FILL, 5, 3);

#ifdef HAVE_TAGLIB
	if (edit_mode == EDITABLE_GENRE) {
		entry = gtk_combo_box_new_text();
		int selected = 0;
		int i = 0;
		gtk_combo_box_append_text(GTK_COMBO_BOX(entry), _("(not set)"));
		while (lookup_id3v1_genre(i) != NULL) {
			gtk_combo_box_append_text(GTK_COMBO_BOX(entry), lookup_id3v1_genre(i));
			if (strcmp(lookup_id3v1_genre(i), value) == 0) {
				selected = i+1;
			}
			++i;
		}
		gtk_combo_box_set_active(GTK_COMBO_BOX(entry), selected);
	} else {
		entry = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(entry), value);
	}

	if (edit_mode == EDITABLE_NO) {
		GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
		gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	}
#endif /* HAVE_TAG_LIB */

	if (importbtn_text == NULL) {
		gtk_table_attach(GTK_TABLE(table), entry, 1, 3, *cnt, *cnt+1,
				 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);
	} else if (data != NULL) {
		button = gtk_button_new_with_label(importbtn_text);
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(import_button_pressed), (gpointer)data);
		gtk_table_attach(GTK_TABLE(table), entry, 1, 2, *cnt, *cnt+1,
				 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);
		gtk_table_attach(GTK_TABLE(table), button, 2, 3, *cnt, *cnt+1,
				 GTK_FILL, GTK_FILL, 5, 3);
	} else { /* we have no data -> dummy button, no signal attached */
		button = gtk_button_new_with_label(importbtn_text);
		gtk_table_attach(GTK_TABLE(table), entry, 1, 2, *cnt, *cnt+1,
				 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);
		gtk_table_attach(GTK_TABLE(table), button, 2, 3, *cnt, *cnt+1,
				 GTK_FILL, GTK_FILL, 5, 3);
	}
	
	(*cnt)++;
	return entry;
}


void
save_pic_button_pressed(GtkWidget * widget, gpointer data) {

	save_pic_t * save_pic = (save_pic_t *)data;
        GtkWidget * dialog;
        gchar * selected_filename;
	char filename[MAXLEN];

        dialog = gtk_file_chooser_dialog_new(_("Please specify the file to save the image to."), 
                                             GTK_WINDOW(info_window), 
                                             GTK_FILE_CHOOSER_ACTION_SAVE, 
                                             GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);

        deflicker();
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), save_pic->savefile);

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		strncpy(filename, selected_filename, MAXLEN-1);
                g_free(selected_filename);

		FILE * f = fopen(filename, "wb");
		if (f == NULL) {
			printf("error: fopen() failed\n");
			gtk_widget_destroy(dialog);
			return;
		}
		if (fwrite(save_pic->image_data, 1, save_pic->image_size, f) != save_pic->image_size) {
			printf("fwrite() error\n");
			gtk_widget_destroy(dialog);
			return;
		}
		fclose(f);

                strncpy(options.currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)), MAXLEN-1);
        }
        gtk_widget_destroy(dialog);
}


void
append_table_pic(GtkWidget * table, int * cnt, GtkWidget * image,
		 char * type, char * descr, char * savefilename,
		 void * image_data, int image_size) {

	char type_str[MAXLEN];
	char descr_str[MAXLEN];

	save_pic_t * save_pic = (save_pic_t *)malloc(sizeof(save_pic_t));
	if (save_pic == NULL)
		return;

	trashlist_add(fileinfo_trash, save_pic);
	strncpy(save_pic->savefile, savefilename, MAXLEN-1);
	save_pic->image_size = image_size;
	save_pic->image_data = image_data;

	snprintf(type_str, MAXLEN-1, "%s: %s", _("Type"), type);
	if (descr != NULL) {
		snprintf(descr_str, MAXLEN-1, "%s: %s", _("Description"), descr);
	} else {
		strcpy(descr_str, _("No description"));
	}

	GtkWidget * frame = gtk_frame_new(_("Embedded picture"));
	GtkWidget * hbox = gtk_hbox_new(FALSE, 0);
	GtkWidget * hbox_i;
	GtkWidget * vbox = gtk_vbox_new(FALSE, 0);
	GtkWidget * label;
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);

	hbox_i = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(type_str);
	gtk_box_pack_start(GTK_BOX(hbox_i), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_i, FALSE, FALSE, 3);
	hbox_i = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(descr_str);
	gtk_box_pack_start(GTK_BOX(hbox_i), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_i, FALSE, FALSE, 3);

        GtkWidget * hbuttonbox = gtk_hbutton_box_new();
        gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);
        GtkWidget * button = gui_stock_label_button(_("Save to file..."), GTK_STOCK_SAVE);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), button);   
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(save_pic_button_pressed), (gpointer)save_pic);
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, FALSE, 3);  

	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 3);
	gtk_box_pack_end(GTK_BOX(hbox), image, FALSE, FALSE, 3);
	gtk_table_attach(GTK_TABLE(table), frame, 0, 3, *cnt, *cnt+1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	(*cnt)++;
}


#ifdef HAVE_TAGLIB
void
save_basic_fields(GtkWidget * widget, gpointer data) {

	savepage_data_t * sdata = (savepage_data_t *)data;
	save_basic_t * save_basic = sdata->save_basic;
	char buf[MAXLEN];
	int i;
	TagLib::String str;
	TagLib::Tag * tag = NULL;

	if (sdata->save_page_no == save_basic->id3v1_page_no) {
		tag = dynamic_cast<TagLib::Tag *>(save_basic->id3v1_tag);
	} else if (sdata->save_page_no == save_basic->id3v2_page_no) {
		tag = dynamic_cast<TagLib::Tag *>(save_basic->id3v2_tag);
	} else if (sdata->save_page_no == save_basic->ape_page_no) {
		tag = dynamic_cast<TagLib::Tag *>(save_basic->ape_tag);
	} else if (sdata->save_page_no == save_basic->oxc_page_no) {
		tag = dynamic_cast<TagLib::Tag *>(save_basic->oxc);
	} else {
		fprintf(stderr, "save_basic_fields(): error looking up metadata type\n");
		return;
	}

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_title)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	/* hack to work around TagLib not saving the tag if it's empty */
	if (buf[0] == '\0') {
		buf[0] = ' ';
		buf[1] = '\0';
	}
	str = TagLib::String(buf, TagLib::String::UTF8);	
	tag->setTitle(str);

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_artist)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	str = TagLib::String(buf, TagLib::String::UTF8);
	tag->setArtist(str);

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_album)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	str = TagLib::String(buf, TagLib::String::UTF8);
	tag->setAlbum(str);

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_comment)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	/* workaround silly TagLib when writing an empty comment to an id3v2 tag */
	if (save_basic->taglib_mpeg_file != NULL) {
		if (buf[0] == '\0') {
			buf[0] = ' ';
			buf[1] = '\0';
		}
	}
	str = TagLib::String(buf, TagLib::String::UTF8);
	tag->setComment(str);

	if (GTK_IS_ENTRY(save_basic->entry_genre)) {
		strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_genre)), MAXLEN-1);
		cut_trailing_whitespace(buf);
	} else {
		int selected = gtk_combo_box_get_active(GTK_COMBO_BOX(save_basic->entry_genre));
		if (selected > 0) {
			strncpy(buf, lookup_id3v1_genre(selected-1), MAXLEN-1);
		} else {
			buf[0] = '\0';
		}
	}
	str = TagLib::String(buf, TagLib::String::UTF8);
	tag->setGenre(str);

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_year)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	if (sscanf(buf, "%d", &i) < 1) {
		i = 0;
	}
	if ((i < 0) || (i > 9999)) {
		i = 0;
	}
	tag->setYear(i);

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_track)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	if (sscanf(buf, "%d", &i) < 1) {
		i = 0;
	}
	if ((i < 0) || (i > 9999)) {
		i = 0;
	}
	tag->setTrack(i);

	/* save the file */
	if (save_basic->taglib_flac_file != NULL)
		save_basic->taglib_flac_file->save();
	if (save_basic->taglib_oggv_file != NULL)
		save_basic->taglib_oggv_file->save();
	if (save_basic->taglib_mpeg_file != NULL) {
		int tags = 0;
		if (save_basic->id3v1_tag != NULL)
			tags |= TagLib::MPEG::File::ID3v1;
		if (save_basic->id3v2_tag != NULL)
			tags |= TagLib::MPEG::File::ID3v2;
		if (save_basic->ape_tag != NULL)
			tags |= TagLib::MPEG::File::APE;
		save_basic->taglib_mpeg_file->save(tags, false);
	}
	if (save_basic->taglib_mpc_file != NULL)
		save_basic->taglib_mpc_file->save();
}


/* simple mode for fields that don't require anything fancy */
int
build_simple_page(save_basic_t * save_basic, TagLib::Tag * tag, int * cnt, GtkWidget ** ptable,
		  int edit_mode, char * nb_label) {

	GtkWidget * vbox = gtk_vbox_new(FALSE, 4);
	GtkWidget * table = gtk_table_new(0, 3, FALSE);
	GtkWidget * label = gtk_label_new(nb_label);
	GtkWidget * scrwin = gtk_scrolled_window_new(NULL, NULL);
	GtkWidget * viewp = gtk_viewport_new(NULL, NULL);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewp), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 10);
	gtk_container_add(GTK_CONTAINER(scrwin), viewp);
	gtk_container_add(GTK_CONTAINER(viewp), vbox);
	int page_id = gtk_notebook_append_page(GTK_NOTEBOOK(save_basic->nb), scrwin, label);

	char str[MAXLEN];

	if (save_basic->mode.is_called_from_browser) {
		import_data_t * data;

		strncpy(str, (char *)tag->title().toCString(true), MAXLEN-1);
		cut_trailing_whitespace(str);
		if (is_all_wspace(str)) {
			data = NULL;
		} else {
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = save_basic->mode.model;
			data->track_iter = save_basic->mode.track_iter;
			data->dest_type = IMPORT_DEST_TITLE;
			strncpy(data->str, str, MAXLEN-1);
		}
		save_basic->entry_title = append_table(table, cnt, edit_mode ? EDITABLE_YES : EDITABLE_NO,
						       _("Title:"), str, _("Import as Title"), data);

		strncpy(str, (char *)tag->artist().toCString(true), MAXLEN-1);
		cut_trailing_whitespace(str);
		if (is_all_wspace(str)) {
			data = NULL;
		} else {
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = save_basic->mode.model;
			data->track_iter = save_basic->mode.track_iter;
			data->dest_type = IMPORT_DEST_ARTIST;
			strncpy(data->str, str, MAXLEN-1);
		}
		save_basic->entry_artist = append_table(table, cnt, edit_mode ? EDITABLE_YES : EDITABLE_NO,
							_("Artist:"), str, _("Import as Artist"), data);

		strncpy(str, (char *)tag->album().toCString(true), MAXLEN-1);
		cut_trailing_whitespace(str);
		if (is_all_wspace(str)) {
			data = NULL;
		} else {
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = save_basic->mode.model;
			data->track_iter = save_basic->mode.track_iter;
			data->dest_type = IMPORT_DEST_RECORD;
			strncpy(data->str, str, MAXLEN-1);
		}
		save_basic->entry_album = append_table(table, cnt, edit_mode ? EDITABLE_YES : EDITABLE_NO,
						       _("Album:"), str, _("Import as Record"), data);

		if (tag->track() != 0) {
			sprintf(str, "%d", tag->track());
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = save_basic->mode.model;
			data->track_iter = save_basic->mode.track_iter;
			data->dest_type = IMPORT_DEST_NUMBER;
			strncpy(data->str, str, MAXLEN-1);
		} else {
			str[0] = '\0';
			data = NULL;
		}
		save_basic->entry_track = append_table(table, cnt, edit_mode ? EDITABLE_YES : EDITABLE_NO,
						       _("Track:"), str, _("Import as Track number"), data);

		if (tag->year() != 0) {
			sprintf(str, "%d", tag->year());
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = save_basic->mode.model;
			data->track_iter = save_basic->mode.track_iter;
			data->dest_type = IMPORT_DEST_COMMENT;
			snprintf(data->str, MAXLEN-1, "%s %s", _("Year:"), str);
		} else {
			str[0] = '\0';
			data = NULL;
		}
		save_basic->entry_year = append_table(table, cnt, edit_mode ? EDITABLE_YES : EDITABLE_NO,
						      _("Year:"), str, _("Add to Comments"), data);

		strncpy(str, (char *)tag->genre().toCString(true), MAXLEN-1);
		cut_trailing_whitespace(str);
		if (is_all_wspace(str)) {
			data = NULL;
		} else {
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = save_basic->mode.model;
			data->track_iter = save_basic->mode.track_iter;
			data->dest_type = IMPORT_DEST_COMMENT;
			snprintf(data->str, MAXLEN-1, "%s %s", _("Genre:"), str);
		}
		save_basic->entry_genre = append_table(table, cnt, edit_mode, _("Genre:"),
						       str, _("Add to Comments"), data);

		strncpy(str, (char *)tag->comment().toCString(true), MAXLEN-1);
		cut_trailing_whitespace(str);
		if (is_all_wspace(str)) {
			data = NULL;
		} else {
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = save_basic->mode.model;
			data->track_iter = save_basic->mode.track_iter;
			data->dest_type = IMPORT_DEST_COMMENT;
			snprintf(data->str, MAXLEN-1, "%s %s", _("Comment:"), str);
		}
		save_basic->entry_comment = append_table(table, cnt, edit_mode ? EDITABLE_YES : EDITABLE_NO,
							 _("Comment:"), str, _("Add to Comments"), data);
	} else {
		strncpy(str, (char *)tag->title().toCString(true), MAXLEN-1);
		cut_trailing_whitespace(str);
		save_basic->entry_title = append_table(table, cnt, edit_mode ? EDITABLE_YES : EDITABLE_NO,
						       _("Title:"), str,  NULL, NULL);

		strncpy(str, (char *)tag->artist().toCString(true), MAXLEN-1);
		cut_trailing_whitespace(str);
		save_basic->entry_artist = append_table(table, cnt, edit_mode ? EDITABLE_YES : EDITABLE_NO,
							_("Artist:"), str, NULL, NULL);

		strncpy(str, (char *)tag->album().toCString(true), MAXLEN-1);
		cut_trailing_whitespace(str);
		save_basic->entry_album = append_table(table, cnt, edit_mode ? EDITABLE_YES : EDITABLE_NO,
						       _("Album:"), str,  NULL, NULL);

		if (tag->track() != 0) {
			sprintf(str, "%d", tag->track());
		} else {
			str[0] = '\0';
		}
		save_basic->entry_track = append_table(table, cnt, edit_mode ? EDITABLE_YES : EDITABLE_NO,
						       _("Track:"), str, NULL, NULL);

		if (tag->year() != 0) {
			sprintf(str, "%d", tag->year());
		} else {
			str[0] = '\0';
		}
		save_basic->entry_year = append_table(table, cnt, edit_mode ? EDITABLE_YES : EDITABLE_NO,
						      _("Year:"), str, NULL, NULL);

		strncpy(str, (char *)tag->genre().toCString(true), MAXLEN-1);
		cut_trailing_whitespace(str);
		save_basic->entry_genre = append_table(table, cnt, edit_mode, _("Genre:"), str,   NULL, NULL);

		strncpy(str, (char *)tag->comment().toCString(true), MAXLEN-1);
		cut_trailing_whitespace(str);
		save_basic->entry_comment = append_table(table, cnt, edit_mode ? EDITABLE_YES : EDITABLE_NO,
							 _("Comment:"), str, NULL, NULL);
	}

	if (ptable) {
		*ptable = table;
	}

	if (edit_mode) {
                GtkWidget * hbuttonbox = gtk_hbutton_box_new();
                gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);
                GtkWidget * button = gui_stock_label_button(_("Save basic fields"), GTK_STOCK_SAVE);
  	        gtk_container_add(GTK_CONTAINER(hbuttonbox), button);   
		savepage_data_t * sdata = savepage_data_new();
		trashlist_add(fileinfo_trash, sdata);
		sdata->save_page_no = page_id;
		sdata->save_basic = save_basic;
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(save_basic_fields), (gpointer)sdata);
		gtk_table_resize(GTK_TABLE(table), *cnt + 1, 3);
		gtk_table_attach(GTK_TABLE(table), hbuttonbox, 1, 3, *cnt, *cnt + 1, GTK_FILL, GTK_FILL, 5, 3);
		(*cnt)++;
	}
	return page_id;
}


int
build_id3v1_page(save_basic_t * save_basic, int edit_mode, TagLib::ID3v1::Tag * id3v1_tag) {

	int cnt = 0;
	TagLib::Tag * tag = dynamic_cast<TagLib::Tag *>(id3v1_tag);
	save_basic->id3v1_tag = id3v1_tag;
	int page_no = build_simple_page(save_basic, tag, &cnt, NULL, edit_mode, _("ID3v1"));
	gtk_widget_show_all(GTK_WIDGET(save_basic->nb));
	return page_no;
}


void
insert_id3v2_text(GtkNotebook * nb, GtkWidget * table, int * cnt, fileinfo_mode_t mode,
		  TagLib::ID3v2::Frame * frame, char * frameID) {

	char descr[MAXLEN];
	char str[MAXLEN];

	TagLib::ID3v2::TextIdentificationFrame * tid_frame =
		dynamic_cast<TagLib::ID3v2::TextIdentificationFrame *>(frame);

	if (!lookup_id3v2_textframe(frameID, descr))
		return;

	int len = strlen(descr);
	if (len < MAXLEN-1) {
		descr[len++] = ':';
		descr[len] = '\0';
	}

	if (mode.is_called_from_browser) {
		import_data_t * data;

		data = import_data_new();
		trashlist_add(fileinfo_trash, data);
		data->model = mode.model;
		data->track_iter = mode.track_iter;
		data->dest_type = IMPORT_DEST_COMMENT;
		strncpy(str, (char *)tid_frame->toString().toCString(true), MAXLEN-1);
		snprintf(data->str, MAXLEN-1, "%s: %s", descr, str);
		append_table(table, cnt, false, descr, str, _("Add to Comments"), data);
	} else {
		strncpy(str, (char *)tid_frame->toString().toCString(true), MAXLEN-1);
		append_table(table, cnt, false, descr, str, NULL, NULL);
	}
}


void
insert_id3v2_rva2(GtkNotebook * nb, GtkWidget * table, int * cnt, fileinfo_mode_t mode,
		  char * id_str, float voladj) {
	
	char descr[MAXLEN];

	snprintf(descr, MAXLEN-1, "%+.1f dB (Id: %s)", voladj, id_str);

	if (mode.is_called_from_browser) {
		import_data_t * data;

		data = import_data_new();
		trashlist_add(fileinfo_trash, data);
		data->model = mode.model;
		data->track_iter = mode.track_iter;
		data->dest_type = IMPORT_DEST_RVA;
		data->fval = voladj;
		append_table(table, cnt, false, _("Relative Volume Adj.:"), descr, _("Import as RVA"), data);
	} else {
		append_table(table, cnt, false, _("Relative Volume Adj.:"), descr, NULL, NULL);
	}
}


char *
pic_type_to_string(int type) {

	using namespace TagLib::ID3v2;

	switch (type) {
	case AttachedPictureFrame::Other:
		return _("Other");
	case AttachedPictureFrame::FileIcon:
		return _("File icon (32x32 PNG)");
	case AttachedPictureFrame::OtherFileIcon:
		return _("File icon (other)");
	case AttachedPictureFrame::FrontCover:
		return _("Front cover");
	case AttachedPictureFrame::BackCover:
		return _("Back cover");
	case AttachedPictureFrame::LeafletPage:
		return _("Leaflet page");
	case AttachedPictureFrame::Media:
		return _("Album image");
	case AttachedPictureFrame::LeadArtist:
		return _("Lead artist/performer");
	case AttachedPictureFrame::Artist:
		return _("Artist/performer");
	case AttachedPictureFrame::Conductor:
		return _("Conductor");
	case AttachedPictureFrame::Band:
		return _("Band/orchestra");
	case AttachedPictureFrame::Composer:
		return _("Composer");
	case AttachedPictureFrame::Lyricist:
		return _("Lyricist/text writer");
	case AttachedPictureFrame::RecordingLocation:
		return _("Recording location/studio");
	case AttachedPictureFrame::DuringRecording:
		return _("During recording");
	case AttachedPictureFrame::DuringPerformance:
		return _("During performance");
	case AttachedPictureFrame::MovieScreenCapture:
		return _("Movie/video screen capture");
	case AttachedPictureFrame::ColouredFish:
		return _("A large, coloured fish");
	case AttachedPictureFrame::Illustration:
		return _("Illustration");
	case AttachedPictureFrame::BandLogo:
		return _("Band/artist logotype");
	case AttachedPictureFrame::PublisherLogo:
		return _("Publisher/studio logotype");
	default:
		return NULL;
	}
}


void
insert_id3v2_apic(GtkNotebook * nb, GtkWidget * table, int * cnt, fileinfo_mode_t mode,
		  TagLib::ID3v2::Frame * frame) {

	TagLib::ID3v2::AttachedPictureFrame * apic_frame =
		dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frame);
	
	char mime_type[20];
	char savefilename[256];
	char tmpname[256];
	void * image_data = NULL;

	mime_type[0] = '\0';
	strcpy(savefilename, "picture.");
	int r = sscanf(apic_frame->mimeType().toCString(true), "image/%s", mime_type);

	if (r == 0) {
		strncpy(mime_type, apic_frame->mimeType().toCString(true), 19);
	}
	for (int i = 0; mime_type[i] != '\0'; i++) {
		mime_type[i] = tolower(mime_type[i]);
	}
	if (mime_type[0] == '\0') {
		strcpy(mime_type, "dat");
	}
	strncat(savefilename, mime_type, 255);

	TagLib::ByteVector bv = apic_frame->picture();
	
	if (bv.size() <= 0)
		return;

	image_data = malloc(bv.size());
	if (!image_data)
		return;

	trashlist_add(fileinfo_trash, image_data);
	memcpy(image_data, bv.data(), bv.size());

	strcpy(tmpname, "/tmp/aqualung-picture-XXXXXX.dat");
	int fd = g_mkstemp(tmpname);
	if (fd < 0) {
		printf("error: g_mkstemp() failed\n");
		return;
	}
	if (write(fd, bv.data(), bv.size()) != (int)bv.size()) {
		printf("write() error\n");
		return;
	}
	close(fd);

	GtkWidget * image = gtk_image_new_from_file(tmpname);
	if (g_unlink(tmpname) < 0) {
		printf("error: g_unlink() failed on %s\n", tmpname);
		return;
	}

	append_table_pic(table, cnt, image,
			 pic_type_to_string(apic_frame->type()),
			 apic_frame->description().isEmpty() ?
			 NULL : (char *)apic_frame->description().toCString(true),
			 savefilename, image_data, bv.size());
}


int
build_id3v2_page(save_basic_t * save_basic, int edit_mode, TagLib::ID3v2::Tag * id3v2_tag) {

	int cnt = 0;
	GtkWidget * table;

	TagLib::ID3v2::FrameList l = id3v2_tag->frameList();
	std::list<TagLib::ID3v2::Frame*>::iterator i;
	
	TagLib::Tag * tag = dynamic_cast<TagLib::Tag *>(id3v2_tag);
	save_basic->id3v2_tag = id3v2_tag;
	int page_no = build_simple_page(save_basic, tag, &cnt, &table, edit_mode, _("ID3v2"));

	for (i = l.begin(); i != l.end(); ++i) {
		TagLib::ID3v2::Frame * frame = *i;
		char frameID[5];
		
		for(int j = 0; j < 4; j++) {
			frameID[j] = frame->frameID().data()[j];
		}
		frameID[4] = '\0';

		if (strcmp(frameID, "APIC") == 0) {
			insert_id3v2_apic(save_basic->nb, table, &cnt, save_basic->mode, frame);
		} else if (strcmp(frameID, "RVA2") == 0) {
			char id_str[MAXLEN];
			float voladj;
			read_rva2(frame->render().data() + 10, frame->size(), id_str, &voladj);
			insert_id3v2_rva2(save_basic->nb, table, &cnt, save_basic->mode, id_str, voladj);
		} else if (frameID[0] == 'T') {
			/* skip frames that are handled by the simple mode */
			if ((strcmp(frameID, "TIT2") != 0) && /* title */
			    (strcmp(frameID, "TPE1") != 0) && /* artist */
			    (strcmp(frameID, "TALB") != 0) && /* album */
			    (strcmp(frameID, "TRCK") != 0) && /* track no. */
			    (strcmp(frameID, "TCON") != 0) && /* genre */
			    (strcmp(frameID, "TDRC") != 0)) { /* year */
				insert_id3v2_text(save_basic->nb, table, &cnt, save_basic->mode, frame, frameID);
			}
		} else {
			/* XXX incomplete */
		}
	}
	gtk_widget_show_all(GTK_WIDGET(save_basic->nb));
	return page_no;
}


/* ape tags also use this */
void
insert_oxc(GtkNotebook * nb, GtkWidget * table, int * cnt, fileinfo_mode_t mode,
	   char * key, char * val) {

	if (mode.is_called_from_browser) {
		import_data_t * data;
		
		data = import_data_new();
		trashlist_add(fileinfo_trash, data);
		
		data->model = mode.model;
		data->track_iter = mode.track_iter;
		snprintf(data->str, MAXLEN-1, "%s %s", key, val);
		
		if ((strcmp(key, "Replaygain_track_gain:") == 0) ||
		    (strcmp(key, "Replaygain_album_gain:") == 0)) {
			
			data->fval = convf(val);
			data->dest_type = IMPORT_DEST_RVA;
			append_table(table, cnt, false, key, val, _("Import as RVA"), data);
		} else {
			data->fval = 0.0f;
			data->dest_type = IMPORT_DEST_COMMENT;
			append_table(table, cnt, false, key, val, _("Add to Comments"), data);
		}
	} else {
		append_table(table, cnt, false, key, val, NULL, NULL);
	}
}


int
build_ape_page(save_basic_t * save_basic, int edit_mode, TagLib::APE::Tag * ape_tag) {

	int cnt = 0;
	GtkWidget * table;
	
	TagLib::Tag * tag = dynamic_cast<TagLib::Tag *>(ape_tag);
	save_basic->ape_tag = ape_tag;
	int page_no = build_simple_page(save_basic, tag, &cnt, &table, edit_mode, _("APE"));

	TagLib::APE::ItemListMap m = ape_tag->itemListMap();
	for (TagLib::APE::ItemListMap::Iterator i = m.begin(); i != m.end(); ++i) {

		TagLib::StringList::Iterator j;
		TagLib::StringList l = (*i).second.toStringList();
		for (j = l.begin(); j != l.end(); j++) {
			
			char key[MAXLEN];
			char val[MAXLEN];
			char c;
			int k;
			
			/* skip comments that are handled by the simple mode */
			if ((strcmp("TITLE", (*i).first.toCString(true)) == 0) ||
			    (strcmp("ARTIST", (*i).first.toCString(true)) == 0) ||
			    (strcmp("ALBUM", (*i).first.toCString(true)) == 0) ||
			    (strcmp("YEAR", (*i).first.toCString(true)) == 0) ||
			    (strcmp("COMMENT", (*i).first.toCString(true)) == 0) ||
			    (strcmp("TRACK", (*i).first.toCString(true)) == 0) ||
			    (strcmp("GENRE", (*i).first.toCString(true)) == 0)) {
				
				continue;
			}
			
			for (k = 0; ((c = (*i).first.toCString(true)[k]) != '\0') && (k < MAXLEN-1); k++) {
				key[k] = (k == 0) ? toupper(c) : tolower(c);
			}
			key[k++] = ':';
			key[k] = '\0';
			
			for (k = 0; ((c = (*j).toCString(true)[k]) != '\0') && (k < MAXLEN-1); k++) {
				val[k] = c;
			}
			val[k] = '\0';
			
			insert_oxc(save_basic->nb, table, &cnt, save_basic->mode, key, val);
		}
	}
	gtk_widget_show_all(GTK_WIDGET(save_basic->nb));
	return page_no;
}


int
build_oxc_page(save_basic_t * save_basic, int edit_mode, TagLib::Ogg::XiphComment * oxc) {

	int cnt = 0;
	GtkWidget * table;

	TagLib::Tag * tag = dynamic_cast<TagLib::Tag *>(oxc);
	save_basic->oxc = oxc;
	int page_no = build_simple_page(save_basic, tag, &cnt, &table, edit_mode, _("Ogg comments"));

	TagLib::Ogg::FieldListMap m = oxc->fieldListMap();
	for (TagLib::Ogg::FieldListMap::Iterator i = m.begin(); i != m.end(); ++i) {
		for (TagLib::StringList::Iterator j = (*i).second.begin(); j != (*i).second.end(); ++j) {
			char key[MAXLEN];
			char val[MAXLEN];
			char c;
			int k;

			/* skip comments that are handled by the simple mode */
			if ((strcmp("TITLE", (*i).first.toCString(true)) == 0) ||
			    (strcmp("ARTIST", (*i).first.toCString(true)) == 0) ||
			    (strcmp("ALBUM", (*i).first.toCString(true)) == 0) ||
			    (strcmp("DATE", (*i).first.toCString(true)) == 0) ||
			    (strcmp("COMMENT", (*i).first.toCString(true)) == 0) ||
			    (strcmp("TRACKNUMBER", (*i).first.toCString(true)) == 0) ||
			    (strcmp("GENRE", (*i).first.toCString(true)) == 0)) {

				continue;
			}

			for (k = 0; ((c = (*i).first.toCString(true)[k]) != '\0') && (k < MAXLEN-1); k++) {
				key[k] = (k == 0) ? toupper(c) : tolower(c);
			}
			key[k++] = ':';
			key[k] = '\0';

			for (k = 0; ((c = (*j).toCString(true)[k]) != '\0') && (k < MAXLEN-1); k++) {
				val[k] = c;
			}
			val[k] = '\0';

			insert_oxc(save_basic->nb, table, &cnt, save_basic->mode, key, val);
		}
	}
	insert_oxc(save_basic->nb, table, &cnt, save_basic->mode, _("Vendor:"),
		   (char *)oxc->vendorID().toCString(true));

	gtk_widget_show_all(GTK_WIDGET(save_basic->nb));
	return page_no;
}


void build_tag_buttons(save_basic_t * save_basic);

void
create_remove_handler(GtkWidget * widget, gpointer data) {

	tagbutton_data_t * tdata = (tagbutton_data_t *)data;
	save_basic_t * save_basic = tdata->save_basic;
	int tags;

	switch (tdata->flags) {
	case CREATE_ID3v2:
		if (save_basic->taglib_mpeg_file != NULL) {

			save_basic->id3v2_tag = save_basic->taglib_mpeg_file->ID3v2Tag(true);
			save_basic->id3v2_tag->setTitle(" ");

			tags = TagLib::MPEG::File::ID3v2;
			if (save_basic->id3v1_tag != NULL)
				tags |= TagLib::MPEG::File::ID3v1;
			if (save_basic->ape_tag != NULL)
				tags |= TagLib::MPEG::File::APE;
			save_basic->taglib_mpeg_file->save(tags, false);

			/* close & reopen file to workaround buggy TagLib */
			{
				char buf[MAXLEN];
				strncpy(buf, save_basic->taglib_mpeg_file->name(), MAXLEN-1);
				save_basic->taglib_mpeg_file->~File();
				save_basic->taglib_mpeg_file = new TagLib::MPEG::File(buf, false);
			}

			/* if we have an ID3v1 page, re-create it in read-only mode */
			if (save_basic->id3v1_page_no != -1) {
				gtk_notebook_remove_page(save_basic->nb, save_basic->id3v1_page_no);
				save_basic->id3v1_page_no = -1;
				save_basic->id3v1_page_no =
					build_id3v1_page(save_basic, false, save_basic->taglib_mpeg_file->ID3v1Tag());
			}

			save_basic->id3v2_page_no =
				build_id3v2_page(save_basic, true, save_basic->taglib_mpeg_file->ID3v2Tag());

			save_basic->flags &= ~CREATE_ID3v2;
			save_basic->flags |= REMOVE_ID3v2;
			build_tag_buttons(save_basic);
		}
		break;
	case CREATE_APE:
		if (save_basic->taglib_mpc_file != NULL) {

			save_basic->ape_tag = save_basic->taglib_mpc_file->APETag(true);
			save_basic->ape_tag->setTitle(" ");
			save_basic->taglib_mpc_file->save();

			/* close & reopen file to workaround buggy TagLib */
			{
				char buf[MAXLEN];
				strncpy(buf, save_basic->taglib_mpc_file->name(), MAXLEN-1);
				save_basic->taglib_mpc_file->~File();
				save_basic->taglib_mpc_file = new TagLib::MPC::File(buf, false);
			}

			save_basic->ape_page_no =
				build_ape_page(save_basic, true, save_basic->taglib_mpc_file->APETag());

			save_basic->flags &= ~CREATE_APE;
			save_basic->flags |= REMOVE_APE;
			build_tag_buttons(save_basic);
		}
		break;
	case REMOVE_ID3v1:
		if (save_basic->taglib_mpeg_file != NULL) {
			save_basic->taglib_mpeg_file->strip(TagLib::MPEG::File::ID3v1);
			save_basic->id3v1_tag = NULL;

			tags = 0;
			if (save_basic->id3v2_tag != NULL)
				tags |= TagLib::MPEG::File::ID3v2;
			if (save_basic->ape_tag != NULL)
				tags |= TagLib::MPEG::File::APE;
			save_basic->taglib_mpeg_file->save(tags, true);
		}
		if (save_basic->taglib_mpc_file != NULL) {
			save_basic->taglib_mpc_file->remove(TagLib::MPC::File::ID3v1);
			save_basic->taglib_mpc_file->save();
		}

		if (save_basic->id3v2_page_no > save_basic->id3v1_page_no)
			save_basic->id3v2_page_no -= 1;
		if (save_basic->ape_page_no > save_basic->id3v1_page_no)
			save_basic->ape_page_no -= 1;
		if (save_basic->oxc_page_no > save_basic->id3v1_page_no)
			save_basic->oxc_page_no -= 1;

		gtk_notebook_remove_page(save_basic->nb, save_basic->id3v1_page_no);
		save_basic->id3v1_page_no = -1;
		save_basic->flags &= ~REMOVE_ID3v1;
		build_tag_buttons(save_basic);
		break;
	case REMOVE_ID3v2:
		if (save_basic->taglib_mpeg_file == NULL)
			break;

		save_basic->taglib_mpeg_file->strip(TagLib::MPEG::File::ID3v2);
		save_basic->id3v2_tag = NULL;

		tags = 0;
		if (save_basic->id3v1_tag != NULL)
			tags |= TagLib::MPEG::File::ID3v1;
		if (save_basic->ape_tag != NULL)
			tags |= TagLib::MPEG::File::APE;
		save_basic->taglib_mpeg_file->save(tags, true);

		if (save_basic->id3v1_page_no > save_basic->id3v2_page_no)
			save_basic->id3v1_page_no -= 1;
		if (save_basic->ape_page_no > save_basic->id3v2_page_no)
			save_basic->ape_page_no -= 1;
		if (save_basic->oxc_page_no > save_basic->id3v2_page_no)
			save_basic->oxc_page_no -= 1;
		
		gtk_notebook_remove_page(save_basic->nb, save_basic->id3v2_page_no);
		save_basic->id3v2_page_no = -1;
		save_basic->flags &= ~REMOVE_ID3v2;
		save_basic->flags |= CREATE_ID3v2;
		build_tag_buttons(save_basic);
		break;
	case REMOVE_APE:
		if (save_basic->taglib_mpeg_file != NULL) {
			save_basic->taglib_mpeg_file->strip(TagLib::MPEG::File::APE);

			tags = 0;
			if (save_basic->id3v1_tag != NULL)
				tags |= TagLib::MPEG::File::ID3v1;
			if (save_basic->id3v2_tag != NULL)
				tags |= TagLib::MPEG::File::ID3v2;
			save_basic->taglib_mpeg_file->save(tags, true);
		}
		if (save_basic->taglib_mpc_file != NULL) {
			save_basic->taglib_mpc_file->remove(TagLib::MPC::File::APE);
			save_basic->taglib_mpc_file->save();
		}
		save_basic->ape_tag = NULL;

		if (save_basic->id3v1_page_no > save_basic->ape_page_no)
			save_basic->id3v1_page_no -= 1;
		if (save_basic->id3v2_page_no > save_basic->ape_page_no)
			save_basic->id3v2_page_no -= 1;
		if (save_basic->oxc_page_no > save_basic->ape_page_no)
			save_basic->oxc_page_no -= 1;

		gtk_notebook_remove_page(save_basic->nb, save_basic->ape_page_no);
		save_basic->ape_page_no = -1;
		save_basic->flags &= ~REMOVE_APE;
		if (save_basic->taglib_mpc_file != NULL) {
			save_basic->flags |= CREATE_APE;
		}
		build_tag_buttons(save_basic);
		break;
	}
}


void
build_tag_buttons(save_basic_t * save_basic) {

	tagbutton_data_t * tdata = NULL;
	GtkWidget * button;
	GtkWidget * hbox;

	if (save_basic->hbox_inner != NULL) {
		gtk_widget_destroy(save_basic->hbox_inner);
	}
	save_basic->hbox_inner = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(save_basic->hbox_inner);
	gtk_container_add(GTK_CONTAINER(save_basic->hbox), save_basic->hbox_inner);

	hbox = save_basic->hbox_inner;

	if (save_basic->flags & REMOVE_ID3v1) {
                button = gui_stock_label_button(_("Remove ID3v1"), GTK_STOCK_REMOVE);

		gtk_widget_show(button);
		tdata = tagbutton_data_new();
		trashlist_add(fileinfo_trash, tdata);
		tdata->flags |= REMOVE_ID3v1;
		tdata->save_basic = save_basic;
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(create_remove_handler), (gpointer)tdata);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 3);
	}
	if (save_basic->flags & CREATE_ID3v2) {
                button = gui_stock_label_button(_("Create ID3v2"), GTK_STOCK_ADD);
		gtk_widget_show(button);
		tdata = tagbutton_data_new();
		trashlist_add(fileinfo_trash, tdata);
		tdata->flags |= CREATE_ID3v2;
		tdata->save_basic = save_basic;
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(create_remove_handler), (gpointer)tdata);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 3);
	}
	if (save_basic->flags & REMOVE_ID3v2) {
                button = gui_stock_label_button(_("Remove ID3v2"), GTK_STOCK_REMOVE);
		gtk_widget_show(button);
		tdata = tagbutton_data_new();
		trashlist_add(fileinfo_trash, tdata);
		tdata->flags |= REMOVE_ID3v2;
		tdata->save_basic = save_basic;
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(create_remove_handler), (gpointer)tdata);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 3);
	}
	if (save_basic->flags & CREATE_APE) {
                button = gui_stock_label_button(_("Create APE"), GTK_STOCK_ADD);
		gtk_widget_show(button);
		tdata = tagbutton_data_new();
		trashlist_add(fileinfo_trash, tdata);
		tdata->flags |= CREATE_APE;
		tdata->save_basic = save_basic;
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(create_remove_handler), (gpointer)tdata);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 3);
	}
	if (save_basic->flags & REMOVE_APE) {
                button = gui_stock_label_button(_("Remove APE"), GTK_STOCK_REMOVE);
		gtk_widget_show(button);
		tdata = tagbutton_data_new();
		trashlist_add(fileinfo_trash, tdata);
		tdata->flags |= REMOVE_APE;
		tdata->save_basic = save_basic;
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(create_remove_handler), (gpointer)tdata);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 3);
	}
}


#ifdef HAVE_FLAC
void
build_nb_pages_flac(metadata * meta, GtkNotebook * nb, GtkWidget * hbox, fileinfo_mode_t mode) {

	TagLib::FLAC::File * taglib_flac_file =
		reinterpret_cast<TagLib::FLAC::File *>(meta->taglib_file);

#ifdef HAVE_METAEDIT
	TagLib::File * taglib_file =
		dynamic_cast<TagLib::File *>(taglib_flac_file);

	int edit_mode = taglib_file->readOnly() ? EDITABLE_NO : EDITABLE_YES;
#else
	int edit_mode = EDITABLE_NO;
#endif /* HAVE_METAEDIT */

	save_basic_t * save_basic = save_basic_new();
	trashlist_add(fileinfo_trash, save_basic);
	save_basic->taglib_flac_file = taglib_flac_file;
	save_basic->nb = nb;
	save_basic->hbox = hbox;
	save_basic->mode = mode;

	if (taglib_flac_file->ID3v1Tag() && !taglib_flac_file->ID3v1Tag()->isEmpty()) {
		save_basic->id3v1_page_no =
		    build_id3v1_page(save_basic, EDITABLE_NO, taglib_flac_file->ID3v1Tag());
	}
	if (taglib_flac_file->ID3v2Tag() && !taglib_flac_file->ID3v2Tag()->isEmpty()) {
		save_basic->id3v2_page_no =
		    build_id3v2_page(save_basic, EDITABLE_NO, taglib_flac_file->ID3v2Tag());
	}
	if (taglib_flac_file->xiphComment()) {
		save_basic->oxc_page_no =
		    build_oxc_page(save_basic, edit_mode, taglib_flac_file->xiphComment());
	}

	if (edit_mode) {
		build_tag_buttons(save_basic);
	}
}
#endif /* HAVE_FLAC */


#ifdef HAVE_OGG_VORBIS
void
build_nb_pages_oggv(metadata * meta, GtkNotebook * nb, GtkWidget * hbox, fileinfo_mode_t mode) {

	TagLib::Ogg::Vorbis::File * taglib_oggv_file =
		reinterpret_cast<TagLib::Ogg::Vorbis::File *>(meta->taglib_file);

#ifdef HAVE_METAEDIT
	TagLib::File * taglib_file =
		dynamic_cast<TagLib::File *>(taglib_oggv_file);

	int edit_mode = taglib_file->readOnly() ? EDITABLE_NO : EDITABLE_YES;
#else
	int edit_mode = EDITABLE_NO;
#endif /* HAVE_METAEDIT */

	save_basic_t * save_basic = save_basic_new();
	trashlist_add(fileinfo_trash, save_basic);
	save_basic->taglib_oggv_file = taglib_oggv_file;
	save_basic->nb = nb;
	save_basic->hbox = hbox;
	save_basic->mode = mode;

	if (taglib_oggv_file->tag()) {
		save_basic->oxc_page_no =
		    build_oxc_page(save_basic, edit_mode, taglib_oggv_file->tag());
	}

	if (edit_mode) {
		build_tag_buttons(save_basic);
	}
}
#endif /* HAVE_OGG_VORBIS */


#ifdef HAVE_MPEG
void
build_nb_pages_mpeg(metadata * meta, GtkNotebook * nb, GtkWidget * hbox, fileinfo_mode_t mode) {

	TagLib::MPEG::File * taglib_mpeg_file =
		reinterpret_cast<TagLib::MPEG::File *>(meta->taglib_file);

#ifdef HAVE_METAEDIT
	TagLib::File * taglib_file =
		dynamic_cast<TagLib::File *>(taglib_mpeg_file);

	int edit_mode = taglib_file->readOnly() ? EDITABLE_NO : EDITABLE_YES;
#else
	int edit_mode = EDITABLE_NO;
#endif /* HAVE_METAEDIT */

	save_basic_t * save_basic = save_basic_new();
	trashlist_add(fileinfo_trash, save_basic);
	save_basic->taglib_mpeg_file = taglib_mpeg_file;
	save_basic->nb = nb;
	save_basic->hbox = hbox;
	save_basic->mode = mode;

	if (taglib_mpeg_file->ID3v1Tag() && !taglib_mpeg_file->ID3v1Tag()->isEmpty()) {
		/* allow editing only if there is no id3v2 tag */
		int id3v1_edit_mode = edit_mode;
		if ((id3v1_edit_mode == EDITABLE_YES) && taglib_mpeg_file->ID3v2Tag()) {
			id3v1_edit_mode = taglib_mpeg_file->ID3v2Tag()->isEmpty() ? EDITABLE_YES : EDITABLE_NO;
		}
		if (id3v1_edit_mode == EDITABLE_YES) {
			id3v1_edit_mode = EDITABLE_GENRE;
		}
		save_basic->id3v1_page_no =
		    build_id3v1_page(save_basic, id3v1_edit_mode, taglib_mpeg_file->ID3v1Tag());
		save_basic->flags |= REMOVE_ID3v1;
	}

	if (taglib_mpeg_file->ID3v2Tag() && !taglib_mpeg_file->ID3v2Tag()->isEmpty()) {
		save_basic->id3v2_page_no =
		    build_id3v2_page(save_basic, edit_mode, taglib_mpeg_file->ID3v2Tag());
		save_basic->flags |= REMOVE_ID3v2;
	} else {
		save_basic->flags |= CREATE_ID3v2;
	}
	if (taglib_mpeg_file->APETag()) {
		save_basic->ape_page_no =
		    build_ape_page(save_basic, EDITABLE_NO, taglib_mpeg_file->APETag());
		save_basic->flags |= REMOVE_APE;
	}

	if (edit_mode) {
		build_tag_buttons(save_basic);
	}
}
#endif /* HAVE_MPEG */


#ifdef HAVE_MPC
void
build_nb_pages_mpc(metadata * meta, GtkNotebook * nb, GtkWidget * hbox, fileinfo_mode_t mode) {

	TagLib::MPC::File * taglib_mpc_file =
		reinterpret_cast<TagLib::MPC::File *>(meta->taglib_file);

#ifdef HAVE_METAEDIT
	TagLib::File * taglib_file =
		dynamic_cast<TagLib::File *>(taglib_mpc_file);

	int edit_mode = taglib_file->readOnly() ? EDITABLE_NO : EDITABLE_YES;
#else
	int edit_mode = EDITABLE_NO;
#endif /* HAVE_METAEDIT */

	save_basic_t * save_basic = save_basic_new();
	trashlist_add(fileinfo_trash, save_basic);
	save_basic->taglib_mpc_file = taglib_mpc_file;
	save_basic->nb = nb;
	save_basic->hbox = hbox;
	save_basic->mode = mode;

	if (taglib_mpc_file->ID3v1Tag() && !taglib_mpc_file->ID3v1Tag()->isEmpty()) {
		save_basic->id3v1_page_no =
		    build_id3v1_page(save_basic, EDITABLE_NO, taglib_mpc_file->ID3v1Tag());
		save_basic->flags |= REMOVE_ID3v1;
	}
	if (taglib_mpc_file->APETag() && !taglib_mpc_file->APETag()->isEmpty()) {
		save_basic->ape_page_no =
		    build_ape_page(save_basic, edit_mode, taglib_mpc_file->APETag());
		save_basic->flags |= REMOVE_APE;
	} else {
		save_basic->flags |= CREATE_APE;
	}

	if (edit_mode) {
		build_tag_buttons(save_basic);
	}
}
#endif /* HAVE_MPC */
#endif /* HAVE_TAGLIB */

gboolean    
fi_cover_press_button_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data) {

	if (event->type == GDK_BUTTON_PRESS && event->button == 1) { /* LMB ? */
                display_zoomed_cover(info_window, fi_event_box, (gchar *) user_data);
        }
        return TRUE;
}    

void
show_file_info(char * name, char * file, int is_called_from_browser,
	       GtkTreeModel * model, GtkTreeIter track_iter) {

        char str[MAXLEN];
	gchar *file_display;

	GtkWidget * vbox;
	GtkWidget * hbox_tagbuttons;
	GtkWidget * hbox_t;
	GtkWidget * table;
	GtkWidget * hbox_name;
	GtkWidget * label_name;
	GtkWidget * entry_name;
	GtkWidget * hbox_path;
	GtkWidget * label_path;
	GtkWidget * entry_path;
	GtkWidget * dismiss_btn;
	GtkWidget * fi_cover_image_area;

	GtkWidget * vbox_file;
	GtkWidget * label_file;
	GtkWidget * table_file;
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * entry;

#ifdef HAVE_MOD_INFO
        mod_info * mdi;
	GtkWidget * vbox_mod;
	GtkWidget * label_mod;
#endif /* HAVE_MOD_INFO */

	metadata * meta = meta_new();

	fileinfo_mode_t mode;
	mode.is_called_from_browser = is_called_from_browser;
	mode.model = model;
	mode.track_iter = track_iter;

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
		meta_free(meta);
		return;
	}

	info_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(info_window), _("File info"));
	gtk_window_set_transient_for(GTK_WINDOW(info_window), GTK_WINDOW(main_window));
	gtk_window_set_position(GTK_WINDOW(info_window), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_default_size(GTK_WINDOW(info_window), 600, 500);
        gtk_window_set_resizable(GTK_WINDOW(info_window), TRUE);
	g_signal_connect(G_OBJECT(info_window), "delete_event",
			 G_CALLBACK(info_window_close), (gpointer)meta);
        g_signal_connect(G_OBJECT(info_window), "key_press_event",
			 G_CALLBACK(info_window_key_pressed), (gpointer)meta);
	gtk_container_set_border_width(GTK_CONTAINER(info_window), 5);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(info_window), vbox);

	hbox_t = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_t, FALSE, FALSE, 5);

	table = gtk_table_new(2, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(hbox_t), table, TRUE, TRUE, 4);

        hbox_name = gtk_hbox_new(FALSE, 0);
	label_name = gtk_label_new(_("Track:"));
	gtk_box_pack_start(GTK_BOX(hbox_name), label_name, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox_name, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 5, 2);

	entry_name = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry_name, GTK_CAN_FOCUS);
	gtk_entry_set_text(GTK_ENTRY(entry_name), name);
	gtk_editable_set_editable(GTK_EDITABLE(entry_name), FALSE);
	gtk_table_attach(GTK_TABLE(table), entry_name, 1, 2, 0, 1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 2);

	hbox_path = gtk_hbox_new(FALSE, 0);
	label_path = gtk_label_new(_("File:"));
	gtk_box_pack_start(GTK_BOX(hbox_path), label_path, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox_path, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 2);

	file_display=g_filename_display_name(file);
	entry_path = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry_path, GTK_CAN_FOCUS);
	gtk_entry_set_text(GTK_ENTRY(entry_path), file_display);
	g_free(file_display);
	gtk_editable_set_editable(GTK_EDITABLE(entry_path), FALSE);
	gtk_table_attach(GTK_TABLE(table), entry_path, 1, 2, 1, 2,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 2);

        fi_cover_image_area = gtk_image_new();
        fi_event_box = gtk_event_box_new ();
	gtk_box_pack_start(GTK_BOX(hbox_t), fi_event_box, FALSE, FALSE, 0);
        gtk_container_add (GTK_CONTAINER (fi_event_box), fi_cover_image_area);
        g_signal_connect(G_OBJECT(fi_event_box), "button_press_event",
                         G_CALLBACK(fi_cover_press_button_cb), file);

        display_cover(fi_cover_image_area, fi_event_box, 48, 48, file, FALSE, TRUE);

	hbox_tagbuttons = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox_tagbuttons, FALSE, FALSE, 5);

	nb = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), nb, TRUE, TRUE, 5);

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
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	assembly_format_label(str, meta->format_major, meta->format_minor);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 0, 1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Length:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	sample2time(meta->sample_rate, meta->total_samples, str, 0);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 1, 2,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Samplerate:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	sprintf(str, _("%ld Hz"), meta->sample_rate);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 2, 3,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Channel count:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	if (meta->is_mono)
		strcpy(str, _("MONO"));
	else
		strcpy(str, _("STEREO"));
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 3, 4,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Bandwidth:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 4, 5, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	sprintf(str, _("%.1f kbit/s"), meta->bps / 1000.0f);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 4, 5,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Total samples:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 5, 6, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	sprintf(str, "%lld", meta->total_samples);
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 5, 6,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);


#ifdef HAVE_TAGLIB
	switch (meta->format_major) {
#ifdef HAVE_FLAC
	case FORMAT_FLAC:
		build_nb_pages_flac(meta, GTK_NOTEBOOK(nb), hbox_tagbuttons, mode);
		break;
#endif /* HAVE_FLAC */
#ifdef HAVE_OGG_VORBIS
	case FORMAT_VORBIS:
		build_nb_pages_oggv(meta, GTK_NOTEBOOK(nb), hbox_tagbuttons, mode);
		break;
#endif /* HAVE_OGG_VORBIS */
#ifdef HAVE_MPEG
	case FORMAT_MAD:
		build_nb_pages_mpeg(meta, GTK_NOTEBOOK(nb), hbox_tagbuttons, mode);
		break;
#endif /* HAVE_MPEG */
#ifdef HAVE_MPC
	case FORMAT_MPC:
		build_nb_pages_mpc(meta, GTK_NOTEBOOK(nb), hbox_tagbuttons, mode);
		break;
#endif /* HAVE_MPC */
	}
#endif /* HAVE_TAGLIB */

#ifdef HAVE_MOD_INFO
	mdi = meta->mod_root;
	if (mdi->active) {
                if ((md_fdec = file_decoder_new()) == NULL) {
                        fprintf(stderr, "show_file_info(): error: file_decoder_new() returned NULL\n");
                }

                if (file_decoder_open(md_fdec, file)) {
                        fprintf(stderr, "file_decoder_open() failed on %s\n", file);
                }

                if (md_fdec->file_lib == MOD_LIB) {

                        vbox_mod = gtk_vbox_new(FALSE, 4);
                        label_mod = gtk_label_new(_("Module info"));
                        gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox_mod, label_mod);
                        fill_module_info_page(mdi, vbox_mod, file);
                } else {

                        file_decoder_close(md_fdec);
                        file_decoder_delete(md_fdec);
                }
        }
#endif /* HAVE_MOD_INFO */

	/* end of notebook stuff */

	gtk_widget_grab_focus(nb);

        dismiss_btn = gtk_button_new_from_stock (GTK_STOCK_CLOSE); 
	g_signal_connect(dismiss_btn, "clicked", G_CALLBACK(dismiss), (gpointer)meta);
        gtk_widget_set_size_request(dismiss_btn, 80, -1);
	gtk_box_pack_end(GTK_BOX(hbox_tagbuttons), dismiss_btn, FALSE, FALSE, 3);

	gtk_widget_show_all(info_window);

	n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(nb));

        if (n_pages > 1) {
                gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), options.tags_tab_first ? 1 : 0);
        }

	/* metadata object is freed in info_window destroy handlers */
}


#ifdef HAVE_MOD_INFO
/*
 * type = 0 for sample list
 * type != 0 for instrument list
 */
void
show_list (gint type) {
GtkTreeIter iter;
gint i, len;
gchar temp[MAXLEN], number[MAXLEN];
decoder_t * md_dec;
mod_pdata_t * md_pd;
        
        md_dec = (decoder_t *)(md_fdec->pdec);
        md_pd = (mod_pdata_t *)md_dec->pdata;

        if (type) {
                len = ModPlug_NumInstruments(md_pd->mpf);
        } else {
                len = ModPlug_NumSamples(md_pd->mpf);
        }

        if (len) {
                gtk_list_store_clear(smp_instr_list_store);
                for(i = 0; i < len; i++) {
                        memset(temp, 0, MAXLEN-1);

                        if (type) {
                                ModPlug_InstrumentName(md_pd->mpf, i, temp);
                        } else {
                                ModPlug_SampleName(md_pd->mpf, i, temp);
                        }

                        sprintf(number, "%2d", i);
                        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(smp_instr_list_store), &iter, NULL, i);
                        gtk_list_store_append(smp_instr_list_store, &iter);
                        gtk_list_store_set(smp_instr_list_store, &iter, 0, number, 1, temp, -1);
                }
        }
}

void
set_first_row (void) {
GtkTreeIter iter;
GtkTreePath * visible_path;

        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(smp_instr_list_store), &iter, NULL, 0);
        visible_path = gtk_tree_model_get_path (GTK_TREE_MODEL(smp_instr_list_store), &iter);
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (smp_instr_list), visible_path, NULL, TRUE);
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (smp_instr_list), visible_path,
                                      NULL, TRUE, 1.0, 0.0);
        gtk_widget_grab_focus(GTK_WIDGET(smp_instr_list));
}

void 
radio_buttons_cb (GtkToggleButton *toggle_button, gboolean state) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button))) {
                show_list (0);
        } else {
                show_list (1);
        }
        set_first_row();
}

void
fill_module_info_page(mod_info *mdi, GtkWidget *vbox, char *file) {

gchar *a_type[] = {
        "None", "MOD", "S3M", "XM", "MED", "MTM", "IT", "669",
        "ULT", "STM", "FAR", "WAV", "AMF", "AMS", "DSM", "MDL",
        "OKT", "MID", "DMF", "PTM", "DBM", "MT2", "AMF0", "PSM",
        "J2B", "UMX"
};

gint i, n;
gchar temp[MAXLEN];
GtkWidget *table;
GtkWidget *label;
GtkWidget *mod_type_label;
GtkWidget *mod_channels_label;
GtkWidget *mod_patterns_label;
GtkWidget *mod_samples_label;
GtkWidget *mod_instruments_label;
GtkWidget *vseparator;
GtkWidget *hbox2;
GtkWidget *vbox2;
GtkWidget *vbox3;
GtkWidget *samples_radiobutton = NULL;
GtkWidget *instruments_radiobutton = NULL;
GSList *samples_radiobutton_group = NULL;
GtkWidget *scrolledwindow;

GtkCellRenderer *renderer;
GtkTreeViewColumn *column;


        hbox2 = gtk_hbox_new (FALSE, 0);
        gtk_widget_show (hbox2);
        gtk_box_pack_start (GTK_BOX (vbox), hbox2, TRUE, TRUE, 0);

        vbox2 = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox2);
        gtk_box_pack_start (GTK_BOX (hbox2), vbox2, FALSE, FALSE, 0);

        if (mdi->instruments) {

                table = gtk_table_new (5, 2, FALSE);
                gtk_widget_show (table);
                gtk_box_pack_start (GTK_BOX (vbox2), table, FALSE, FALSE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (table), 8);
                gtk_table_set_row_spacings (GTK_TABLE (table), 6);
                gtk_table_set_col_spacings (GTK_TABLE (table), 4);

                label = gtk_label_new (_("Type:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_type_label = gtk_label_new ("");
                gtk_widget_show (mod_type_label);
                gtk_table_attach (GTK_TABLE (table), mod_type_label, 1, 2, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Channels:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_channels_label = gtk_label_new ("");
                gtk_widget_show (mod_channels_label);
                gtk_table_attach (GTK_TABLE (table), mod_channels_label, 1, 2, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Patterns:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_patterns_label = gtk_label_new ("");
                gtk_widget_show (mod_patterns_label);
                gtk_table_attach (GTK_TABLE (table), mod_patterns_label, 1, 2, 2, 3,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Samples:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_samples_label = gtk_label_new ("");
                gtk_widget_show (mod_samples_label);
                gtk_table_attach (GTK_TABLE (table), mod_samples_label, 1, 2, 3, 4,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Instruments:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 4, 5,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_instruments_label = gtk_label_new ("");
                gtk_widget_show (mod_instruments_label);
                gtk_table_attach (GTK_TABLE (table), mod_instruments_label, 1, 2, 4, 5,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                sprintf(temp, "%d", mdi->instruments);
                gtk_label_set_text (GTK_LABEL(mod_instruments_label), temp);


                table = gtk_table_new (2, 1, FALSE);
                gtk_widget_show (table);
                gtk_box_pack_end (GTK_BOX (vbox2), table, FALSE, FALSE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (table), 8);
                gtk_table_set_row_spacings (GTK_TABLE (table), 4);

                samples_radiobutton = gtk_radio_button_new_with_mnemonic (NULL, _("Samples"));
                gtk_widget_show (samples_radiobutton);
                gtk_table_attach (GTK_TABLE (table), samples_radiobutton, 0, 1, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_radio_button_set_group (GTK_RADIO_BUTTON (samples_radiobutton), samples_radiobutton_group);
                samples_radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (samples_radiobutton));
		g_signal_connect(G_OBJECT(samples_radiobutton), "toggled",
				 G_CALLBACK(radio_buttons_cb), NULL);

                instruments_radiobutton = gtk_radio_button_new_with_mnemonic (NULL, _("Instruments"));
                gtk_widget_show (instruments_radiobutton);
                gtk_table_attach (GTK_TABLE (table), instruments_radiobutton, 0, 1, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_radio_button_set_group (GTK_RADIO_BUTTON (instruments_radiobutton), samples_radiobutton_group);
                samples_radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (instruments_radiobutton));


        } else {

                table = gtk_table_new (4, 2, FALSE);
                gtk_widget_show (table);
                gtk_box_pack_start (GTK_BOX (vbox2), table, FALSE, FALSE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (table), 8);
                gtk_table_set_row_spacings (GTK_TABLE (table), 4);
                gtk_table_set_col_spacings (GTK_TABLE (table), 4);

                label = gtk_label_new (_("Type:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_type_label = gtk_label_new ("");
                gtk_widget_show (mod_type_label);
                gtk_table_attach (GTK_TABLE (table), mod_type_label, 1, 2, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Channels:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_channels_label = gtk_label_new ("");
                gtk_widget_show (mod_channels_label);
                gtk_table_attach (GTK_TABLE (table), mod_channels_label, 1, 2, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Patterns:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_patterns_label = gtk_label_new ("");
                gtk_widget_show (mod_patterns_label);
                gtk_table_attach (GTK_TABLE (table), mod_patterns_label, 1, 2, 2, 3,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Samples:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_samples_label = gtk_label_new ("");
                gtk_widget_show (mod_samples_label);
                gtk_table_attach (GTK_TABLE (table), mod_samples_label, 1, 2, 3, 4,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
        }

        n = mdi->type;
        i = 0;
        
        while (n > 0) {         /* calculate module type index */
                n >>= 1;
                i++;
        }

        gtk_label_set_text (GTK_LABEL(mod_type_label), a_type[i]);
        sprintf(temp, "%d", mdi->channels);
        gtk_label_set_text (GTK_LABEL(mod_channels_label), temp);
        sprintf(temp, "%d", mdi->patterns);
        gtk_label_set_text (GTK_LABEL(mod_patterns_label), temp);
        sprintf(temp, "%d", mdi->samples);
        gtk_label_set_text (GTK_LABEL(mod_samples_label), temp);

        vseparator = gtk_vseparator_new ();
        gtk_widget_show (vseparator);
        gtk_box_pack_start (GTK_BOX (hbox2), vseparator, FALSE, FALSE, 4);

        vbox3 = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox3);
        gtk_box_pack_start (GTK_BOX (hbox2), vbox3, TRUE, TRUE, 0);

        scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
        gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow), 4);
        gtk_widget_show (scrolledwindow);
        gtk_widget_set_size_request (scrolledwindow, -1, 220);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), 
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_box_pack_end (GTK_BOX (vbox3), scrolledwindow, TRUE, TRUE, 0);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_SHADOW_IN);

        smp_instr_list_store = gtk_list_store_new(2, 
                                        G_TYPE_STRING,
                                        G_TYPE_STRING);
        smp_instr_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(smp_instr_list_store));
	gtk_widget_set_name(smp_instr_list, "samples_instruments_list");
        gtk_widget_show (smp_instr_list);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("No."), renderer, "text", 0, NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
                                        GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(column), 3);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), FALSE);
        gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(column), 40);
	gtk_tree_view_append_column(GTK_TREE_VIEW(smp_instr_list), column);
	column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer, "text", 1, NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
                                        GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_append_column(GTK_TREE_VIEW(smp_instr_list), column);
        gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(column), 3);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), FALSE);
        gtk_container_add (GTK_CONTAINER (scrolledwindow), smp_instr_list);

        if (mdi->instruments && mdi->type == 0x4) { /* if XM module go to instrument page */
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (instruments_radiobutton), TRUE);
        } else {
                show_list (0);
                set_first_row();
        }
}
#endif /* HAVE_MOD_INFO */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

