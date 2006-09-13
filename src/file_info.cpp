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
typedef struct {
	TagLib::FLAC::File * taglib_flac_file;
	TagLib::Ogg::Vorbis::File * taglib_oggv_file;
	TagLib::MPEG::File * taglib_mpeg_file;
	TagLib::MPC::File * taglib_mpc_file;

	TagLib::Tag * tag;

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
} save_basic_t;
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

	return save_basic;
}
#endif /* HAVE_TAGLIB */


static gint
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


static gint
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
	case GDK_q:
	case GDK_Q:
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
append_table(GtkWidget * table, int * cnt, bool editable, char * field, char * value,
	     char * importbtn_text, import_data_t * data) {

	GtkWidget * hbox;
	GtkWidget * entry;
	GtkWidget * label;

	GtkWidget * button;

	gtk_table_resize(GTK_TABLE(table), *cnt + 1, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(field);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, *cnt, *cnt+1, GTK_FILL, GTK_FILL, 5, 3);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), value);

	if (!editable) {
		GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
		gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	}

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


static void
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

	GtkWidget * button = gtk_button_new_with_label(_("Save to file..."));
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(save_pic_button_pressed), (gpointer)save_pic);
	gtk_box_pack_end(GTK_BOX(vbox), button, FALSE, FALSE, 3);

	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 3);
	gtk_box_pack_end(GTK_BOX(hbox), image, FALSE, FALSE, 3);
	gtk_table_attach(GTK_TABLE(table), frame, 0, 3, *cnt, *cnt+1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	(*cnt)++;
}


#ifdef HAVE_TAGLIB
static void
save_basic_fields(GtkWidget * widget, gpointer data) {

	save_basic_t * save_basic = (save_basic_t *)data;
	char buf[MAXLEN];
	int i;
	TagLib::String str;

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_title)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	str = TagLib::String(buf, TagLib::String::UTF8);
	save_basic->tag->setTitle(str);

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_artist)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	str = TagLib::String(buf, TagLib::String::UTF8);
	save_basic->tag->setArtist(str);

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_album)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	str = TagLib::String(buf, TagLib::String::UTF8);
	save_basic->tag->setAlbum(str);

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_comment)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	str = TagLib::String(buf, TagLib::String::UTF8);
	save_basic->tag->setComment(str);

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_genre)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	str = TagLib::String(buf, TagLib::String::UTF8);
	save_basic->tag->setGenre(str);

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_year)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	if (sscanf(buf, "%d", &i) < 1) {
		i = 0;
	}
	if ((i < 0) || (i > 9999)) {
		i = 0;
	}
	save_basic->tag->setYear(i);

	strncpy(buf, gtk_entry_get_text(GTK_ENTRY(save_basic->entry_track)), MAXLEN-1);
	cut_trailing_whitespace(buf);
	if (sscanf(buf, "%d", &i) < 1) {
		i = 0;
	}
	if ((i < 0) || (i > 9999)) {
		i = 0;
	}
	save_basic->tag->setTrack(i);

	/* save the file */
	if (save_basic->taglib_flac_file != NULL)
		save_basic->taglib_flac_file->save();
	if (save_basic->taglib_oggv_file != NULL)
		save_basic->taglib_oggv_file->save();
	if (save_basic->taglib_mpeg_file != NULL) {
		int tags;
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


/* simple mode for tags that don't require anything fancy (currently id3v1 and ape) */
int
build_simple_page(GtkNotebook * nb, GtkWidget ** ptable, fileinfo_mode_t mode,
		  bool editable, save_basic_t * save_basic, char * nb_label) {

	TagLib::Tag * tag = save_basic->tag;

	GtkWidget * vbox = gtk_vbox_new(FALSE, 4);
	GtkWidget * table = gtk_table_new(0, 3, FALSE);
	GtkWidget * label = gtk_label_new(nb_label);
	GtkWidget * scrwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 10);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrwin), vbox);
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), scrwin, label);

	int cnt = 0;
	char str[MAXLEN];

	if (mode.is_called_from_browser) {
		import_data_t * data;

		strncpy(str, (char *)tag->title().toCString(true), MAXLEN-1);
		if (is_all_wspace(str)) {
			data = NULL;
		} else {
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = mode.model;
			data->track_iter = mode.track_iter;
			data->dest_type = IMPORT_DEST_TITLE;
			strncpy(data->str, str, MAXLEN-1);
		}
		save_basic->entry_title = append_table(table, &cnt, editable, _("Title:"),
						       str, _("Import as Title"), data);

		strncpy(str, (char *)tag->artist().toCString(true), MAXLEN-1);
		if (is_all_wspace(str)) {
			data = NULL;
		} else {
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = mode.model;
			data->track_iter = mode.track_iter;
			data->dest_type = IMPORT_DEST_ARTIST;
			strncpy(data->str, str, MAXLEN-1);
		}
		save_basic->entry_artist = append_table(table, &cnt, editable, _("Artist:"),
							str, _("Import as Artist"), data);

		strncpy(str, (char *)tag->album().toCString(true), MAXLEN-1);
		if (is_all_wspace(str)) {
			data = NULL;
		} else {
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = mode.model;
			data->track_iter = mode.track_iter;
			data->dest_type = IMPORT_DEST_RECORD;
			strncpy(data->str, str, MAXLEN-1);
		}
		save_basic->entry_album = append_table(table, &cnt, editable, _("Album:"),
						       str, _("Import as Record"), data);

		if (tag->track() != 0) {
			sprintf(str, "%d", tag->track());
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = mode.model;
			data->track_iter = mode.track_iter;
			data->dest_type = IMPORT_DEST_NUMBER;
			strncpy(data->str, str, MAXLEN-1);
		} else {
			str[0] = '\0';
			data = NULL;
		}
		save_basic->entry_track = append_table(table, &cnt, editable, _("Track:"),
						       str, _("Import as Track number"), data);

		if (tag->year() != 0) {
			sprintf(str, "%d", tag->year());
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = mode.model;
			data->track_iter = mode.track_iter;
			data->dest_type = IMPORT_DEST_COMMENT;
			snprintf(data->str, MAXLEN-1, "%s %s", _("Year:"), str);
		} else {
			str[0] = '\0';
			data = NULL;
		}
		save_basic->entry_year = append_table(table, &cnt, editable, _("Year:"),
						      str, _("Add to Comments"), data);

		strncpy(str, (char *)tag->genre().toCString(true), MAXLEN-1);
		if (is_all_wspace(str)) {
			data = NULL;
		} else {
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = mode.model;
			data->track_iter = mode.track_iter;
			data->dest_type = IMPORT_DEST_COMMENT;
			snprintf(data->str, MAXLEN-1, "%s %s", _("Genre:"), str);
		}
		save_basic->entry_genre = append_table(table, &cnt, editable, _("Genre:"),
						       str, _("Add to Comments"), data);

		strncpy(str, (char *)tag->comment().toCString(true), MAXLEN-1);
		if (is_all_wspace(str)) {
			data = NULL;
		} else {
			data = import_data_new();
			trashlist_add(fileinfo_trash, data);
			data->model = mode.model;
			data->track_iter = mode.track_iter;
			data->dest_type = IMPORT_DEST_COMMENT;
			snprintf(data->str, MAXLEN-1, "%s %s", _("Comment:"), str);
		}
		save_basic->entry_comment = append_table(table, &cnt, editable, _("Comment:"),
							 str, _("Add to Comments"), data);
	} else {
		save_basic->entry_title = append_table(table, &cnt, editable, _("Title:"),
			(char *)tag->title().toCString(true),  NULL, NULL);
		save_basic->entry_artist = append_table(table, &cnt, editable, _("Artist:"),
			(char *)tag->artist().toCString(true), NULL, NULL);
		save_basic->entry_album = append_table(table, &cnt, editable, _("Album:"),
			(char *)tag->album().toCString(true),  NULL, NULL);
		if (tag->track() != 0) {
			sprintf(str, "%d", tag->track());
		} else {
			str[0] = '\0';
		}
		save_basic->entry_track = append_table(table, &cnt, editable, _("Track:"),
						       str, NULL, NULL);
		if (tag->year() != 0) {
			sprintf(str, "%d", tag->year());
		} else {
			str[0] = '\0';
		}
		save_basic->entry_year = append_table(table, &cnt, editable, _("Year:"),
						      str, NULL, NULL);
		save_basic->entry_genre = append_table(table, &cnt, editable, _("Genre:"),
			(char *)tag->genre().toCString(true),   NULL, NULL);
		save_basic->entry_comment = append_table(table, &cnt, editable, _("Comment:"),
			(char *)tag->comment().toCString(true), NULL, NULL);
	}

	if (ptable) {
		*ptable = table;
	}

	if (editable) {
		GtkWidget * button = gtk_button_new_with_label(_("Save basic fields"));
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(save_basic_fields), (gpointer)save_basic);
		gtk_table_resize(GTK_TABLE(table), cnt + 1, 3);
		gtk_table_attach(GTK_TABLE(table), button, 0, 3, cnt, cnt+1, GTK_FILL, GTK_FILL, 5, 3);
		++cnt;
	}
	return cnt;
}


void
build_id3v1_page(GtkNotebook * nb, fileinfo_mode_t mode, bool editable,
		 save_basic_t * save_basic, TagLib::ID3v1::Tag * id3v1_tag) {

	TagLib::Tag * tag = dynamic_cast<TagLib::Tag *>(id3v1_tag);
	save_basic->tag = tag;
	save_basic->id3v1_tag = id3v1_tag;
	build_simple_page(nb, NULL, mode, editable, save_basic, _("ID3v1"));
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

	snprintf(descr, MAXLEN-1, _("%+.1f dB (Id: %s)"), voladj, id_str);

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


void
build_id3v2_page(GtkNotebook * nb, fileinfo_mode_t mode, bool editable,
		 save_basic_t * save_basic, TagLib::ID3v2::Tag * id3v2_tag) {

	int cnt = 0;
	GtkWidget * table;

	TagLib::ID3v2::FrameList l = id3v2_tag->frameList();
	std::list<TagLib::ID3v2::Frame*>::iterator i;
	
	TagLib::Tag * tag = dynamic_cast<TagLib::Tag *>(id3v2_tag);
	save_basic->tag = tag;
	save_basic->id3v2_tag = id3v2_tag;
	cnt = build_simple_page(nb, &table, mode, editable, save_basic, _("ID3v2"));

	for (i = l.begin(); i != l.end(); ++i) {
		TagLib::ID3v2::Frame * frame = *i;
		char frameID[5];
		
		for(int j = 0; j < 4; j++) {
			frameID[j] = frame->frameID().data()[j];
		}
		frameID[4] = '\0';

		if (strcmp(frameID, "APIC") == 0) {
			insert_id3v2_apic(nb, table, &cnt, mode, frame);
		} else if (strcmp(frameID, "RVA2") == 0) {
			char id_str[MAXLEN];
			float voladj;
			read_rva2(frame->render().data() + 10, frame->size(), id_str, &voladj);
			insert_id3v2_rva2(nb, table, &cnt, mode, id_str, voladj);
		} else if (frameID[0] == 'T') {
			/* skip frames that are handled by the simple mode */
			if ((strcmp(frameID, "TIT2") != 0) && /* title */
			    (strcmp(frameID, "TPE1") != 0) && /* artist */
			    (strcmp(frameID, "TALB") != 0) && /* album */
			    (strcmp(frameID, "TRCK") != 0) && /* track no. */
			    (strcmp(frameID, "TCON") != 0) && /* genre */
			    (strcmp(frameID, "TDRC") != 0)) { /* year */
				insert_id3v2_text(nb, table, &cnt, mode, frame, frameID);
			}
		} else {
			/* XXX incomplete */
		}
	}
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


void
build_ape_page(GtkNotebook * nb, fileinfo_mode_t mode, bool editable,
	       save_basic_t * save_basic, TagLib::APE::Tag * ape_tag) {

	int cnt = 0;
	GtkWidget * table;
	
	TagLib::Tag * tag = dynamic_cast<TagLib::Tag *>(ape_tag);
	save_basic->tag = tag;
	save_basic->ape_tag = ape_tag;
	cnt = build_simple_page(nb, &table, mode, editable, save_basic, _("APE"));

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
			
			printf("'%s' =  '%s'\n", key, val);
			insert_oxc(nb, table, &cnt, mode, key, val);
		}
	}
}


void
build_oxc_page(GtkNotebook * nb, fileinfo_mode_t mode, bool editable,
	       save_basic_t * save_basic, TagLib::Ogg::XiphComment * oxc) {

	int cnt = 0;
	GtkWidget * table;

	TagLib::Tag * tag = dynamic_cast<TagLib::Tag *>(oxc);
	save_basic->tag = tag;
	save_basic->oxc = oxc;
	cnt = build_simple_page(nb, &table, mode, editable, save_basic, _("Ogg comments"));

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

			insert_oxc(nb, table, &cnt, mode, key, val);
		}
	}
	insert_oxc(nb, table, &cnt, mode, _("Vendor:"), (char *)oxc->vendorID().toCString(true));
}


#ifdef HAVE_FLAC
void
build_nb_pages_flac(metadata * meta, GtkNotebook * nb, fileinfo_mode_t mode) {

	TagLib::FLAC::File * taglib_flac_file =
		reinterpret_cast<TagLib::FLAC::File *>(meta->taglib_file);

	TagLib::File * taglib_file =
		dynamic_cast<TagLib::File *>(taglib_flac_file);

	bool editable = !taglib_file->readOnly();

	save_basic_t * save_basic = save_basic_new();
	trashlist_add(fileinfo_trash, save_basic);
	save_basic->taglib_flac_file = taglib_flac_file;

	if (taglib_flac_file->ID3v1Tag() && !taglib_flac_file->ID3v1Tag()->isEmpty()) {
		build_id3v1_page(nb, mode, false, save_basic, taglib_flac_file->ID3v1Tag());
	}
	if (taglib_flac_file->ID3v2Tag() && !taglib_flac_file->ID3v2Tag()->isEmpty()) {
		build_id3v2_page(nb, mode, false, save_basic, taglib_flac_file->ID3v2Tag());
	}
	if (taglib_flac_file->xiphComment()) {
		build_oxc_page(nb, mode, editable, save_basic, taglib_flac_file->xiphComment());
	}
}
#endif /* HAVE_FLAC */


#ifdef HAVE_OGG_VORBIS
void
build_nb_pages_oggv(metadata * meta, GtkNotebook * nb, fileinfo_mode_t mode) {

	TagLib::Ogg::Vorbis::File * taglib_oggv_file =
		reinterpret_cast<TagLib::Ogg::Vorbis::File *>(meta->taglib_file);

	TagLib::File * taglib_file =
		dynamic_cast<TagLib::File *>(taglib_oggv_file);

	bool editable = !taglib_file->readOnly();

	save_basic_t * save_basic = save_basic_new();
	trashlist_add(fileinfo_trash, save_basic);
	save_basic->taglib_oggv_file = taglib_oggv_file;

	if (taglib_oggv_file->tag()) {
		build_oxc_page(nb, mode, editable, save_basic, taglib_oggv_file->tag());
	}
}
#endif /* HAVE_OGG_VORBIS */


#ifdef HAVE_MPEG
void
build_nb_pages_mpeg(metadata * meta, GtkNotebook * nb, fileinfo_mode_t mode) {

	TagLib::MPEG::File * taglib_mpeg_file =
		reinterpret_cast<TagLib::MPEG::File *>(meta->taglib_file);

	TagLib::File * taglib_file =
		dynamic_cast<TagLib::File *>(taglib_mpeg_file);

	bool editable = !taglib_file->readOnly();

	save_basic_t * save_basic = save_basic_new();
	trashlist_add(fileinfo_trash, save_basic);
	save_basic->taglib_mpeg_file = taglib_mpeg_file;

	if (taglib_mpeg_file->ID3v1Tag() && !taglib_mpeg_file->ID3v1Tag()->isEmpty()) {
		build_id3v1_page(nb, mode, false, save_basic, taglib_mpeg_file->ID3v1Tag());
	}
	if (taglib_mpeg_file->ID3v2Tag() && !taglib_mpeg_file->ID3v2Tag()->isEmpty()) {
		build_id3v2_page(nb, mode, editable, save_basic, taglib_mpeg_file->ID3v2Tag());
	}
	if (taglib_mpeg_file->APETag() && !taglib_mpeg_file->APETag()->isEmpty()) {
		build_ape_page(nb, mode, false, save_basic, taglib_mpeg_file->APETag());
	}
}
#endif /* HAVE_MPEG */


#ifdef HAVE_MPC
void
build_nb_pages_mpc(metadata * meta, GtkNotebook * nb, fileinfo_mode_t mode) {

	TagLib::MPC::File * taglib_mpc_file =
		reinterpret_cast<TagLib::MPC::File *>(meta->taglib_file);

	TagLib::File * taglib_file =
		dynamic_cast<TagLib::File *>(taglib_mpc_file);

	bool editable = !taglib_file->readOnly();

	save_basic_t * save_basic = save_basic_new();
	trashlist_add(fileinfo_trash, save_basic);
	save_basic->taglib_mpc_file = taglib_mpc_file;

	if (taglib_mpc_file->ID3v1Tag() && !taglib_mpc_file->ID3v1Tag()->isEmpty()) {
		build_id3v1_page(nb, mode, false, save_basic, taglib_mpc_file->ID3v1Tag());
	}
	if (taglib_mpc_file->APETag() && !taglib_mpc_file->APETag()->isEmpty()) {
		build_ape_page(nb, mode, editable, save_basic, taglib_mpc_file->APETag());
	}
}
#endif /* HAVE_MPC */
#endif /* HAVE_TAGLIB */


void
show_file_info(char * name, char * file, int is_called_from_browser,
	       GtkTreeModel * model, GtkTreeIter track_iter) {

        char str[MAXLEN];
	gchar *file_display;

	GtkWidget * vbox;
	GtkWidget * hbox_t;
	GtkWidget * table;
	GtkWidget * hbox_name;
	GtkWidget * label_name;
	GtkWidget * entry_name;
	GtkWidget * hbox_path;
	GtkWidget * label_path;
	GtkWidget * entry_path;
	GtkWidget * hbuttonbox;
	GtkWidget * dismiss_btn;
	GtkWidget * cover_image_area;

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
	gtk_box_pack_start(GTK_BOX(vbox), hbox_t, FALSE, FALSE, 0);

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

        cover_image_area = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(hbox_t), cover_image_area, FALSE, FALSE, 0);

        display_cover(cover_image_area, 48, 48, file, FALSE);

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
		build_nb_pages_flac(meta, GTK_NOTEBOOK(nb), mode);
		break;
#endif /* HAVE_FLAC */
#ifdef HAVE_OGG_VORBIS
	case FORMAT_VORBIS:
		build_nb_pages_oggv(meta, GTK_NOTEBOOK(nb), mode);
		break;
#endif /* HAVE_OGG_VORBIS */
#ifdef HAVE_MPEG
	case FORMAT_MAD:
		build_nb_pages_mpeg(meta, GTK_NOTEBOOK(nb), mode);
		break;
#endif /* HAVE_MPEG */
#ifdef HAVE_MPC
	case FORMAT_MPC:
		build_nb_pages_mpc(meta, GTK_NOTEBOOK(nb), mode);
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

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, FALSE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        dismiss_btn = gtk_button_new_from_stock (GTK_STOCK_CLOSE); 
	g_signal_connect(dismiss_btn, "clicked", G_CALLBACK(dismiss), (gpointer)meta);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), dismiss_btn);   

	gtk_widget_show_all(info_window);

	n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(nb));

        if (n_pages > 1) {
                gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), options.tags_tab_first ? 1 : 0);
        }

	/* metadata object is freed in handlers when closing window */
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
GtkWidget *hseparator;
GtkWidget *hbox;
GtkWidget *samples_radiobutton = NULL;
GtkWidget *instruments_radiobutton = NULL;
GSList *samples_radiobutton_group = NULL;
GtkWidget *scrolledwindow;

GtkCellRenderer *renderer;
GtkTreeViewColumn *column;

        if (mdi->instruments) {

                table = gtk_table_new (2, 6, FALSE);
                gtk_widget_show (table);
                gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
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
                gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_widget_set_size_request (label, 150, -1);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_channels_label = gtk_label_new ("");
                gtk_widget_show (mod_channels_label);
                gtk_table_attach (GTK_TABLE (table), mod_channels_label, 3, 4, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Patterns:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 4, 5, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_widget_set_size_request (label, 150, -1);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_patterns_label = gtk_label_new ("");
                gtk_widget_show (mod_patterns_label);
                gtk_table_attach (GTK_TABLE (table), mod_patterns_label, 5, 6, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Samples:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_samples_label = gtk_label_new ("");
                gtk_widget_show (mod_samples_label);
                gtk_table_attach (GTK_TABLE (table), mod_samples_label, 1, 2, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Instruments:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 2, 3, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_instruments_label = gtk_label_new ("");
                gtk_widget_show (mod_instruments_label);
                gtk_table_attach (GTK_TABLE (table), mod_instruments_label, 3, 4, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                sprintf(temp, "%d", mdi->instruments);
                gtk_label_set_text (GTK_LABEL(mod_instruments_label), temp);

        } else {

                table = gtk_table_new (1, 8, FALSE);
                gtk_widget_show (table);
                gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
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
                gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_widget_set_size_request (label, 110, -1);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_channels_label = gtk_label_new ("");
                gtk_widget_show (mod_channels_label);
                gtk_table_attach (GTK_TABLE (table), mod_channels_label, 3, 4, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Patterns:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 4, 5, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_widget_set_size_request (label, 110, -1);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_patterns_label = gtk_label_new ("");
                gtk_widget_show (mod_patterns_label);
                gtk_table_attach (GTK_TABLE (table), mod_patterns_label, 5, 6, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);

                label = gtk_label_new (_("Samples:"));
                gtk_widget_show (label);
                gtk_table_attach (GTK_TABLE (table), label, 6, 7, 0, 1,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
                gtk_widget_set_size_request (label, 110, -1);
                gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

                mod_samples_label = gtk_label_new ("");
                gtk_widget_show (mod_samples_label);
                gtk_table_attach (GTK_TABLE (table), mod_samples_label, 7, 8, 0, 1,
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

        hseparator = gtk_hseparator_new ();
        gtk_widget_show (hseparator);
        gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, FALSE, 2);

        if (mdi->instruments) {

                hbox = gtk_hbox_new (FALSE, 0);
                gtk_widget_show (hbox);
                gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

                samples_radiobutton = gtk_radio_button_new_with_mnemonic (NULL, _("Samples"));
                gtk_widget_show (samples_radiobutton);
                gtk_box_pack_start (GTK_BOX (hbox), samples_radiobutton, FALSE, TRUE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (samples_radiobutton), 4);
                gtk_radio_button_set_group (GTK_RADIO_BUTTON (samples_radiobutton), samples_radiobutton_group);
                samples_radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (samples_radiobutton));
		g_signal_connect(G_OBJECT(samples_radiobutton), "toggled",
				 G_CALLBACK(radio_buttons_cb), NULL);

                instruments_radiobutton = gtk_radio_button_new_with_mnemonic (NULL, _("Instruments"));
                gtk_widget_show (instruments_radiobutton);
                gtk_box_pack_start (GTK_BOX (hbox), instruments_radiobutton, FALSE, TRUE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (instruments_radiobutton), 4);
                gtk_radio_button_set_group (GTK_RADIO_BUTTON (instruments_radiobutton), samples_radiobutton_group);
                samples_radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (instruments_radiobutton));
        }

        scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
        gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow), 4);
        gtk_widget_show (scrolledwindow);
        gtk_widget_set_size_request (scrolledwindow, -1, 220);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), 
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start (GTK_BOX (vbox), scrolledwindow, TRUE, TRUE, 0);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_SHADOW_IN);

        smp_instr_list_store = gtk_list_store_new(2, 
                                        G_TYPE_STRING,
                                        G_TYPE_STRING);
        smp_instr_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(smp_instr_list_store));
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

