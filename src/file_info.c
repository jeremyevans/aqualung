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
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

#ifdef HAVE_MOD
#include <libmodplug/modplug.h>
#include "decoder/dec_mod.h"
#endif /* HAVE_MOD */

#ifdef HAVE_WAVPACK
#include <wavpack/wavpack.h>
#include "decoder/dec_wavpack.h"
#endif /* HAVE_WAVPACK */

#ifdef HAVE_CDDA
#include "cdda.h"
#endif /* HAVE_CDDA */

#include "common.h"
#include "utils.h"
#include "utils_gui.h"
#include "cover.h"
#include "decoder/file_decoder.h"
#include "music_browser.h"
#include "store_file.h"
#include "gui_main.h"
#include "options.h"
#include "trashlist.h"
#include "i18n.h"
#include "metadata.h"
#include "metadata_api.h"
#include "metadata_id3v1.h"
#include "metadata_id3v2.h"
#include "file_info.h"


/* requested width of value display/edit widgets */
#define VAL_WIDGET_WIDTH 250

/* dimensions of cover thumbnail */
#define THUMB_SIZE 48

/* import destination codes */
enum {
	IMPORT_DEST_ARTIST,
	IMPORT_DEST_RECORD,
	IMPORT_DEST_TITLE,
	IMPORT_DEST_YEAR,
	IMPORT_DEST_NUMBER,
	IMPORT_DEST_COMMENT,
	IMPORT_DEST_RVA
};


extern options_t options;

#define FI_MAXPAGES 256

typedef struct {
	int tag;
	GtkWidget * table;
	int n_rows;
	int n_cols;
	GtkWidget * combo;
	GSList * slist; /* list of insertable frames */
} pageidx_t;

typedef struct {
	/* arguments to show_file_info */
	GtkTreeModel * model;
	GtkTreeIter iter_track;
	fileinfo_model_func_t mfun;
	int mindepth;
	gboolean allow_ms_import;
	gboolean display_cover;

	/* entries containing displayed data */
	GtkWidget * entry_name;
	GtkWidget * entry_path;
	GtkWidget * cover_image_area;
	GtkWidget * entry_format;
	GtkWidget * entry_length;
	GtkWidget * entry_sr;
	GtkWidget * entry_ch;
	GtkWidget * entry_bw;
	GtkWidget * entry_nsamples;
	GtkWidget * entry_mode;

	/* work data */
	gboolean media_ok;
	char * name;
	char * filename;
	decoder_t * dec;
	int is_cdda;
	fileinfo_t fileinfo;

	int bail_out;
	file_decoder_t * fdec;
	metadata_t * meta;
	trashlist_t * trash;
	GtkWidget * info_window;
	GtkWidget * event_box;
	GtkWidget * cover_align;
	GtkWidget * hbox;
	GtkWidget * add_tag_table;
	GtkWidget * button_table;
	GtkWidget * prev_button;
	GtkWidget * next_button;
	GtkWidget * save_button;
	GtkWidget * nb;
	GtkWidget * combo;
	int addable_tags; /* META_TAG_*, bitwise or-ed */
	gint n_pages; /* number of notebook pages */
	pageidx_t pageidx[FI_MAXPAGES];
	int selected_tag;
	int dirty; /* whether the dialog has unsaved changes */
	gboolean cover_set_from_apic;

	/* for MOD info */
	GtkWidget * smp_instr_list;
	GtkListStore * smp_instr_list_store;
} fi_t;

typedef struct {
	fi_t * fi;
	meta_frame_t * frame;
        int dest_type; /* one of IMPORT_DEST_* */
} import_data_t;



typedef struct {
	fi_t * fi;
	char savefile[MAXLEN];
	unsigned int image_size;
	void * image_data;
	GtkWidget * save_button;
} save_pic_t;

typedef struct {
	fi_t * fi;
	meta_frame_t * frame;
	save_pic_t * save_pic;
} change_pic_t;


/* 'source widget' for APIC frames */
typedef struct {
	GtkWidget * descr_entry;
	GtkWidget * type_combo;
	GtkWidget * mime_label;
	GtkWidget * image;
} apic_source_t;


extern GtkWidget * main_window;
extern GtkTreeStore * music_store;
extern GtkWidget * music_tree;


#ifdef HAVE_MOD
void module_info_fill_page(fi_t * fi, meta_frame_t * frame, GtkWidget * vbox);
#endif /* HAVE_MOD */


gint fi_save(GtkWidget * widget, gpointer data);
void fi_procframe_ins(fi_t * fi, meta_frame_t * frame);
void fi_procframe_add_tag_page(fi_t * fi, meta_frame_t * frame);


fi_t *
fi_new(void) {

	fi_t * fi;
	int i;

	if ((fi = (fi_t *)calloc(1, sizeof(fi_t))) == NULL) {
		fprintf(stderr, "error: fileinfo_new(): calloc error\n");
		return NULL;
	}

	for (i = 0; i < FI_MAXPAGES; i++) {
		fi->pageidx[i].tag = -1;
	}
	fi->selected_tag = -1;

	return fi;
}

void
fi_unload(fi_t * fi) {
	fi->media_ok = FALSE;

	/* We don't need to destroy these widgets if the window is being
	   closed; see info_window_close. */
	if (fi->save_button) {
		if (GTK_IS_WIDGET(fi->save_button)) gtk_widget_destroy(fi->save_button);
		fi->save_button = NULL;
	}
	if (fi->add_tag_table) {
		if (GTK_IS_WIDGET(fi->add_tag_table)) gtk_widget_destroy(fi->add_tag_table);
		fi->add_tag_table = NULL;
	}

	if (fi->fdec != NULL) {
		file_decoder_delete(fi->fdec);
		fi->fdec = NULL;
		fi->meta = NULL;
	}
	free(fi->name);
	free(fi->filename);

	trashlist_free(fi->trash);
	fi->trash = NULL;
	fi->cover_set_from_apic = FALSE;
}

void
fi_delete(fi_t * fi) {

	int i;

	if (fi == NULL)	return;

	fi_unload(fi);

	for (i = 0; i < FI_MAXPAGES; i++) {
		g_slist_free(fi->pageidx[i].slist);
		fi->pageidx[i].slist = NULL;
	}
	free(fi);
}


void
fi_mark_changed(fi_t * fi) {

	if (fi->dirty != 0)
		return;

	gtk_window_set_title(GTK_WINDOW(fi->info_window), _("*File info"));
	fi->dirty = 1;
}


void
fi_mark_unchanged(fi_t * fi) {

	if (fi->dirty == 0)
		return;

	gtk_window_set_title(GTK_WINDOW(fi->info_window), _("File info"));
	fi->dirty = 0;
}


import_data_t *
import_data_new(void) {

	import_data_t * data;

	if ((data = (import_data_t *)calloc(1, sizeof(import_data_t))) == NULL) {
		fprintf(stderr, "error: import_data_new(): calloc error\n");
		return NULL;
	}
	return data;
}


int
fi_close_dialog(fi_t * fi) {

	int ret;
	GtkWidget * dialog = gtk_message_dialog_new(GTK_WINDOW(fi->info_window),
						    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_MESSAGE_WARNING,
						    GTK_BUTTONS_NONE,
						    _("There are unsaved changes to the file metadata."));

	gtk_window_set_title(GTK_WINDOW(dialog), _("Warning"));
	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
			       _("Save and close"), 1,
			       _("Discard changes"), 2,
			       _("Do not close"), 0,
			       NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), 0);

	ret = aqualung_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return ret;
}


int
fi_can_close(fi_t * fi) {

	if (fi->dirty != 0) {
		switch (fi_close_dialog(fi)) {
		case 1: /* Save and close */
			if (fi_save(NULL, fi)) {
				fi_mark_unchanged(fi);
				return TRUE;
			} else {
				return FALSE;
			}
		case 2: /* Discard changes */
			fi_mark_unchanged(fi);
			return TRUE;
		default: /* Do not close */
			return FALSE;
		}
	}
	return TRUE;
}


gint
info_window_close(GtkWidget * widget, GdkEventAny * event, gpointer data) {

	fi_t * fi = (fi_t *)data;

	if (fi_can_close(fi) != TRUE) {
		return TRUE;
	}

	/* These widgets will be destroyed along with info_window;
	   stop fi_unload from trying to delete them too. */
	fi->save_button = NULL;
	fi->add_tag_table = NULL;

	unregister_toplevel_window(fi->info_window);
	gtk_widget_destroy(fi->info_window);

	fi_delete(fi);
	return TRUE;
}


gint
dismiss(GtkWidget * widget, gpointer data) {

	return info_window_close(widget, NULL, data);
}


gint
info_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent, gpointer data) {

	fi_t * fi = (fi_t *)data;
	int page;

	switch (kevent->keyval) {
	case GDK_KEY_Escape:
		dismiss(NULL, data);
		return TRUE;
		break;
	case GDK_KEY_Return:
		page = (gtk_notebook_get_current_page(GTK_NOTEBOOK(fi->nb)) + 1) % fi->n_pages;
		gtk_notebook_set_current_page(GTK_NOTEBOOK(fi->nb), page);
		break;
	}
	return FALSE;
}


int
fi_set_frame_from_source(fi_t * fi, meta_frame_t * frame) {

	char * parsefmt = meta_get_field_parsefmt(frame->type);
	char parsefmt_esc[MAXLEN];

	escape_percents(parsefmt, parsefmt_esc);
	if (META_FIELD_TEXT(frame->type)) {
		if (frame->field_val != NULL) {
			free(frame->field_val);
		}
		if (frame->type == META_FIELD_GENRE) {
			frame->field_val = gtk_combo_box_text_get_active_text(
                           GTK_COMBO_BOX_TEXT(frame->source));
		} else {
			frame->field_val = strdup(gtk_entry_get_text(GTK_ENTRY(frame->source)));
		}
	} else if (META_FIELD_INT(frame->type)) {
		int val = 0;
		const char * str = gtk_entry_get_text(GTK_ENTRY(frame->source));
		if (sscanf(str, parsefmt, &val) < 1) {
			char msg[MAXLEN];
			arr_snprintf(msg,
				     _("Conversion error in field %s:\n"
				       "'%s' does not conform to format '%s'!"),
				     frame->field_name, str, "%s");
			message_dialog(_("Error"),
				       fi->info_window, GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK, NULL, msg, parsefmt_esc);
			return -1;
		} else {
			frame->int_val = val;
		}
	} else if (META_FIELD_FLOAT(frame->type)) {
		float val = 0;
		const char * str = gtk_entry_get_text(GTK_ENTRY(frame->source));
		if (sscanf(str, parsefmt, &val) < 1) {
			char msg[MAXLEN];
			arr_snprintf(msg,
				     _("Conversion error in field %s:\n"
				       "'%s' does not conform to format '%s'!"),
				     frame->field_name, str, "%s");
			message_dialog(_("Error"),
				       fi->info_window, GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK, NULL, msg, parsefmt_esc);
			return -1;
		} else {
			frame->float_val = val;
		}
	} else if (META_FIELD_BIN(frame->type)) {
		if (frame->type == META_FIELD_APIC) {
			apic_source_t * source = (apic_source_t *)frame->source;
			if (frame->field_val != NULL) {
				free(frame->field_val);
			}
			frame->field_val = strdup(gtk_entry_get_text(GTK_ENTRY(source->descr_entry)));
			frame->int_val = gtk_combo_box_get_active(GTK_COMBO_BOX(source->type_combo));
			if (frame->length == 0) {
				message_dialog(_("Error"),
					       fi->info_window, GTK_MESSAGE_ERROR,
					       GTK_BUTTONS_OK, NULL,
					       _("Attached Picture frame with no image set!\n"
						 "Please set an image or remove the frame."));
				return -1;
			}
		}
	}
	return 0;
}


gint
fi_save(GtkWidget * widget, gpointer data) {

	fi_t * fi = (fi_t *)data;
	metadata_t * meta = fi->meta;
	meta_frame_t * frame = meta->root;
	int status = 0;

	while (frame != NULL) {
		if (fi_set_frame_from_source(fi, frame) < 0) {
			status = -1;
			break;
		}
		frame = frame->next;
	}

	if (status != 0) {
		return FALSE;
	}

	if (fi->fdec->meta_write != NULL) {
		int ret;
		ret = fi->fdec->meta_write(fi->fdec, fi->meta);
		if (ret == META_ERROR_NONE) {
			fi_mark_unchanged(fi);
			return TRUE;
		} else {
			message_dialog(_("Error"),
				       fi->info_window, GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK, NULL,
				       _("Failed to write metadata to file.\n"
					 "Reason: %s"),
				       metadata_strerror(ret));
			return FALSE;
		}
	} else {
		fprintf(stderr, "programmer error: fdec->meta_write == NULL\n");
	}

	return FALSE;
}


void
import_button_pressed(GtkWidget * widget, gpointer gptr_data) {

	import_data_t * data = (import_data_t *)gptr_data;
	fi_t * fi = data->fi;
	meta_frame_t * frame = data->frame;
	GtkTreeModel * model = fi->model;
	GtkTreeIter iter_track = fi->iter_track;
	GtkTreeIter record_iter;
	GtkTreeIter artist_iter;
	GtkTreePath * path;
	char tmp[MAXLEN];

	record_data_t * record_data;
	track_data_t * track_data;

	if (fi_set_frame_from_source(fi, frame) < 0) {
		return;
	}

	switch (data->dest_type) {
	case IMPORT_DEST_TITLE:
		gtk_tree_store_set(music_store, &iter_track, MS_COL_NAME, frame->field_val, -1);
		music_store_mark_changed(&iter_track);
		break;
	case IMPORT_DEST_RECORD:
		gtk_tree_model_iter_parent(model, &record_iter, &iter_track);
		gtk_tree_store_set(music_store, &record_iter, MS_COL_NAME, frame->field_val, -1);
		music_store_mark_changed(&record_iter);
		break;
	case IMPORT_DEST_ARTIST:
		gtk_tree_model_iter_parent(model, &record_iter, &iter_track);
		gtk_tree_model_iter_parent(model, &artist_iter, &record_iter);
		gtk_tree_store_set(music_store, &artist_iter, MS_COL_NAME, frame->field_val, -1);
		gtk_tree_store_set(music_store, &artist_iter, MS_COL_SORT, frame->field_val, -1);
		path = gtk_tree_model_get_path(model, &iter_track);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(music_tree), path, NULL, TRUE, 0.5f, 0.0f);
		music_store_mark_changed(&artist_iter);
		break;
	case IMPORT_DEST_YEAR:
		gtk_tree_model_iter_parent(model, &record_iter, &iter_track);
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &record_iter, MS_COL_DATA, &record_data, -1);
		if (sscanf(frame->field_val, "%d", &record_data->year) < 1) {
			char msg[MAXLEN];
			arr_snprintf(msg,
				     _("Error converting field %s to Year:\n"
				       "'%s' is not an integer number!"),
				     frame->field_name, frame->field_val);
			message_dialog(_("Error"),
				       fi->info_window, GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK, NULL, msg);
			return;
		} else {
			music_store_mark_changed(&record_iter);
		}
		break;
	case IMPORT_DEST_NUMBER:
		arr_snprintf(tmp, "%02d", frame->int_val);
		gtk_tree_store_set(music_store, &iter_track, MS_COL_SORT, tmp, -1);
		music_store_mark_changed(&iter_track);
		break;
	case IMPORT_DEST_COMMENT:
		gtk_tree_model_get(model, &iter_track, MS_COL_DATA, &track_data, -1);
		tmp[0] = '\0';
		if (track_data->comment != NULL) {
			arr_strlcat(tmp, track_data->comment);
		}
		if ((tmp[strlen(tmp)-1] != '\n') && (tmp[0] != '\0')) {
			arr_strlcat(tmp, "\n");
		}
		arr_strlcat(tmp, frame->field_val);
		free_strdup(&track_data->comment, tmp);
		music_store_mark_changed(&iter_track);
		break;
	case IMPORT_DEST_RVA:
		gtk_tree_model_get(model, &iter_track, MS_COL_DATA, &track_data, -1);
		track_data->rva = frame->float_val;
		track_data->use_rva = 1;
		music_store_mark_changed(&iter_track);
		break;
	}
}

int
lookup_page(fi_t * fi, int tag) {

	int i;
	for (i = 0; i < FI_MAXPAGES; i++) {
		if (fi->pageidx[i].tag == tag) {
			return i;
		}
	}
	return -1;
}

void
fi_set_page(fi_t * fi) {
	if (fi->selected_tag > -1) { /* try to select first page with the same tag */
		int idx = lookup_page(fi, fi->selected_tag);
		if (idx > -1) {
			gtk_notebook_set_current_page(GTK_NOTEBOOK(fi->nb), idx);
			return;
		}
	}
	/* fallback to default behaviour */
	fi->n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(fi->nb));
	if (fi->n_pages > 1) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(fi->nb), options.tags_tab_first ? 1 : 0);
	}
}

int
fi_tabwidth(fi_t * fi, metadata_t * meta) {

	/* tabwidth is 2, +1 if called from Music Store, +1 if editable */
	int tabwidth = 2;
	tabwidth += fi->allow_ms_import? 1 : 0;
	tabwidth += meta->writable ? 1 : 0;
	return tabwidth;
}


void
save_pic_button_pressed(GtkWidget * widget, gpointer data) {

	save_pic_t * save_pic = (save_pic_t *)data;
	fi_t * fi = save_pic->fi;

	GSList * lfiles = NULL;
	char filename[MAXLEN];
	char * dirname;

	dirname = g_path_get_dirname(options.currdir);
	arr_snprintf(filename, "%s/%s", dirname, save_pic->savefile);
	g_free(dirname);

	lfiles = file_chooser(_("Please specify the file to save the image to."),
			      fi->info_window,
			      GTK_FILE_CHOOSER_ACTION_SAVE,
			      FILE_CHOOSER_FILTER_NONE,
			      FALSE,
			      filename, CHAR_ARRAY_SIZE(filename));

	if (lfiles != NULL) {

		FILE * f = fopen(filename, "wb");

		g_slist_free(lfiles);

		if (f == NULL) {
			fprintf(stderr, "error: fopen() failed\n");
			return;
		}
		if (fwrite(save_pic->image_data, 1, save_pic->image_size, f) != save_pic->image_size) {
			fprintf(stderr, "fwrite() error\n");
			return;
		}
		fclose(f);

		arr_strlcpy(options.currdir, filename);
	}
}


void
save_pic_update(save_pic_t * save_pic, fi_t * fi, meta_frame_t * frame) {

	int i, r;
	char mtype[20];
	char savefilename[256];

	mtype[0] = '\0';
	arr_strlcpy(savefilename, "picture.");
	r = sscanf(frame->field_name, "image/%s", mtype);
	if (r == 0) {
		arr_strlcpy(mtype, frame->field_name);
	}
	for (i = 0; mtype[i] != '\0'; i++) {
		mtype[i] = tolower(mtype[i]);
	}
	if (mtype[0] == '\0') {
		arr_strlcpy(mtype, "dat");
	}
	arr_strlcat(savefilename, mtype);

	save_pic->fi = fi;
	arr_strlcpy(save_pic->savefile, savefilename);
	save_pic->image_size = frame->length;
	save_pic->image_data = frame->data;

	if (save_pic->image_size > 0) {
		gtk_widget_set_sensitive(save_pic->save_button, TRUE);
	} else {
		gtk_widget_set_sensitive(save_pic->save_button, FALSE);
	}
}


GtkWidget *
make_image_from_binary(meta_frame_t * frame) {

	GdkPixbuf * pixbuf;
	GdkPixbuf * pixbuf_scaled;
	GtkWidget * image;
	int width, height;
	int new_width, new_height;

	void * data = frame->data;
	int length = frame->length;

	GdkPixbufLoader * loader;

	if (length == 0) {
		return gtk_label_new(_("(no image)"));
	}

	if (strcmp(frame->field_name, "-->") == 0) {
		/* display URL */
		char * str = meta_id3v2_to_utf8(0x0/*ascii*/, data, length);
		GtkWidget * label = gtk_label_new(str);
		g_free(str);
		return label;
	}

	loader = gdk_pixbuf_loader_new();
	if (gdk_pixbuf_loader_write(loader, frame->data, frame->length, NULL) != TRUE) {
		fprintf(stderr, "make_image_from_binary: failed to load image #1\n");
		g_object_unref(loader);
		return gtk_label_new(_("(error loading image)"));
	}

	if (gdk_pixbuf_loader_close(loader, NULL) != TRUE) {
		fprintf(stderr, "make_image_from_binary: failed to load image #2\n");
		g_object_unref(loader);
		return gtk_label_new(_("(error loading image)"));
	}

	pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	new_width = (width > VAL_WIDGET_WIDTH) ? VAL_WIDGET_WIDTH : width;
	new_height = ((double)new_width / (double)width) * height;
	pixbuf_scaled = gdk_pixbuf_scale_simple(pixbuf, new_width, new_height, GDK_INTERP_BILINEAR);
	image = gtk_image_new_from_pixbuf(pixbuf_scaled);
	g_object_unref(pixbuf_scaled);
	g_object_unref(loader);
	return image;
}


void
change_pic_button_pressed(GtkWidget * widget, gpointer data) {

	change_pic_t * change_pic = (change_pic_t *)data;
	save_pic_t * save_pic = change_pic->save_pic;
	fi_t * fi = change_pic->fi;
	meta_frame_t * frame = change_pic->frame;
	apic_source_t * source = ((apic_source_t *)(frame->source));
	GSList * lfiles = NULL;

	char str[MAXLEN];
	GdkPixbufFormat * pformat;
	gchar ** mime_types;

	GtkWidget * vbox; /* parent of image */

	lfiles = file_chooser(_("Please specify the file to load the image from."),
			      fi->info_window,
			      GTK_FILE_CHOOSER_ACTION_OPEN,
			      FILE_CHOOSER_FILTER_NONE,
			      FALSE,
			      options.currdir, CHAR_ARRAY_SIZE(options.currdir));

	if (lfiles != NULL) {
		g_slist_free(lfiles);
	} else {
		return;
	}

	pformat = gdk_pixbuf_get_file_info(options.currdir, NULL, NULL);
	if (pformat == NULL) {
		char msg[MAXLEN];
		arr_snprintf(msg, _("Could not load image from:\n%s"), options.currdir);
		message_dialog(_("Error"), fi->info_window, GTK_MESSAGE_ERROR,
			       GTK_BUTTONS_OK, NULL, msg);
		return;
	}

	if (frame->data != NULL) {
		free(frame->data);
	}
	if (!g_file_get_contents(options.currdir, ((gchar **)(&frame->data)),
				 ((gsize *)(&frame->length)), NULL)) {
		fprintf(stderr, "g_file_get_contents failed on %s\n", options.currdir);
		return;
	}

	mime_types = gdk_pixbuf_format_get_mime_types(pformat);
	if (mime_types[0] != NULL) {
		if (frame->field_name != NULL) {
			free(frame->field_name);
		}
		frame->field_name = strdup(mime_types[0]);
		g_strfreev(mime_types);
	} else {
		fprintf(stderr, "error: no mime type for image %s\n", options.currdir);
		g_strfreev(mime_types);
		return;
	}

	vbox = gtk_widget_get_parent(source->image);
	gtk_widget_destroy(source->image);
	source->image = make_image_from_binary(frame);
	gtk_widget_show(source->image);
	gtk_container_add(GTK_CONTAINER(vbox), source->image);
	arr_snprintf(str, _("MIME type: %s"), frame->field_name);
	gtk_label_set_text(GTK_LABEL(source->mime_label), str);
	/* update callback data for 'Save picture' */
	save_pic_update(save_pic, fi, frame);

	fi_mark_changed(fi);
}


GtkWidget *
fi_procframe_label_apic(fi_t * fi, meta_frame_t * frame) {

	metadata_t * meta = fi->meta;
	apic_source_t * source;

	char * pic_caption;
	char str[MAXLEN];
	GtkWidget * label_frame;
	GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * button;

	change_pic_t * change_pic;

	save_pic_t * save_pic = (save_pic_t *)malloc(sizeof(save_pic_t));
	if (save_pic == NULL)
		return NULL;

	trashlist_add(fi->trash, save_pic);

	change_pic = (change_pic_t *)malloc(sizeof(change_pic_t));
	if (change_pic == NULL)
		return NULL;

	trashlist_add(fi->trash, change_pic);
	change_pic->fi = fi;
	change_pic->frame = frame;
	change_pic->save_pic = save_pic;

	frame->source = calloc(1, sizeof(apic_source_t));
	if (frame->source == NULL)
		return NULL;
	trashlist_add(fi->trash, frame->source);
	source = ((apic_source_t *)(frame->source));

	meta_get_fieldname(META_FIELD_APIC, &pic_caption);
	arr_snprintf(str, "%s:", pic_caption);

	label_frame = gtk_frame_new(pic_caption);
	gtk_container_add(GTK_CONTAINER(label_frame), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
	arr_snprintf(str, _("MIME type: %s"), frame->field_name);
	source->mime_label = gtk_label_new(str);
	gtk_box_pack_start(GTK_BOX(hbox), source->mime_label, FALSE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
	label = gtk_label_new(_("Picture type:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	if (meta->writable) {
		int i = 0;
		char * type_str;
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
		source->type_combo = gtk_combo_box_text_new();
		while (1) {
			type_str = meta_id3v2_apic_type_to_string(i);
			if (type_str == NULL) {
				break;
			}
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(source->type_combo), type_str);
			++i;
		}
		gtk_combo_box_set_active(GTK_COMBO_BOX(source->type_combo), frame->int_val);
		gtk_box_pack_start(GTK_BOX(hbox), source->type_combo, TRUE, TRUE, 0);
	} else {
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
		arr_snprintf(str, "%s",
			     meta_id3v2_apic_type_to_string(frame->int_val));
		label = gtk_label_new(str);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	}

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
	label = gtk_label_new(_("Description:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	if (meta->writable) {
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
		source->descr_entry = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(source->descr_entry), frame->field_val);
		gtk_box_pack_start(GTK_BOX(hbox), source->descr_entry, TRUE, TRUE, 0);
	} else {
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
		label = gtk_label_new((frame->field_val[0] == '\0') ?
				      _("(no description)") : frame->field_val);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	}


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_hexpand(hbox, TRUE);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);

	button = gui_stock_label_button(_("Change"), GTK_STOCK_OPEN);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 3);
	if (meta->writable) {
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(change_pic_button_pressed),
				 (gpointer)change_pic);
	} else {
		gtk_widget_set_sensitive(button, FALSE);
	}

	save_pic->save_button = gui_stock_label_button(_("Save"), GTK_STOCK_SAVE);
	gtk_box_pack_start(GTK_BOX(hbox), save_pic->save_button, TRUE, TRUE, 3);
	g_signal_connect(G_OBJECT(save_pic->save_button), "clicked",
			 G_CALLBACK(save_pic_button_pressed),
			 (gpointer)save_pic);

	save_pic_update(save_pic, fi, frame);

	return label_frame;
}


GtkWidget *
fi_procframe_label(fi_t * fi, meta_frame_t * frame) {

	if (frame->type == META_FIELD_APIC) {
		return fi_procframe_label_apic(fi, frame);
	} else {
		char str[MAXLEN];
		GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		GtkWidget * label;
		arr_snprintf(str, "%s:", frame->field_name);
		label = gtk_label_new(str);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		return hbox;
	}
}


void
fi_entry_changed_cb(GtkEntry * entry, gpointer data) {

	fi_t * fi = (fi_t *)data;
	fi_mark_changed(fi);
}


gint
genre_cmp(gconstpointer a, gconstpointer b) {

	return strcmp(id3v1_genre_str_from_code(GPOINTER_TO_INT(a)),
		      id3v1_genre_str_from_code(GPOINTER_TO_INT(b)));
}


void
make_genre_combo(meta_frame_t * frame, GtkWidget ** widget, GtkWidget ** entry) {

	int i;
	GtkWidget * combo = gtk_combo_box_text_new_with_entry();
	GSList * list = NULL;
	GSList * _list;

	for (i = 0; i < 256; i++) {
		char * genre = id3v1_genre_str_from_code(i);
		if (genre == NULL) {
			break;
		}
		list = g_slist_append(list, GINT_TO_POINTER(i));
	}

	list = g_slist_sort(list, genre_cmp);
	_list = list;
	while (_list != NULL) {
		char * genre = id3v1_genre_str_from_code(GPOINTER_TO_INT(_list->data));
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), genre);
		_list = g_slist_next(_list);
	}
	g_slist_free(list);

	gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 4);

	*widget = combo;
	*entry = GTK_WIDGET(gtk_bin_get_child(GTK_BIN(combo)));

	if (frame->field_val[0] == '\0') { /* set default genre if none present */
		gtk_entry_set_text(GTK_ENTRY(*entry), id3v1_genre_str_from_code(0));
	} else {
		gtk_entry_set_text(GTK_ENTRY(*entry), frame->field_val);
	}

	/* for ID3v1, only predefined genres can be selected, no editing allowed */
	if (frame->tag == META_TAG_ID3v1) {
		gtk_widget_set_can_focus(*entry, FALSE);
		gtk_editable_set_editable(GTK_EDITABLE(*entry), FALSE);
	}
}


void
make_apic_widget(meta_frame_t * frame, GtkWidget ** widget, GtkWidget ** entry) {

	GtkWidget * apic_frame = gtk_frame_new(NULL);
	GtkWidget * image = make_image_from_binary(frame);
	GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	gtk_frame_set_shadow_type(GTK_FRAME(apic_frame), GTK_SHADOW_NONE);
	gtk_container_add(GTK_CONTAINER(apic_frame), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), image, TRUE, TRUE, 0);

	*widget = apic_frame;
	*entry = apic_frame;
	((apic_source_t *)(frame->source))->image = image;
}


GtkWidget *
fi_procframe_entry(fi_t * fi, meta_frame_t * frame) {

	metadata_t * meta = fi->meta;
	GtkWidget * widget = NULL;
	GtkWidget * entry = NULL;
	if (META_FIELD_BIN(frame->type)) {
		/* TODO create image/whatever widget, etc. */
		if (frame->type == META_FIELD_APIC) {
			make_apic_widget(frame, &widget, &entry);
		}
	} else if (META_FIELD_INT(frame->type)) {
		char str[MAXLEN];
		char * format = meta_get_field_renderfmt(frame->type);
		widget = entry = gtk_entry_new();
		arr_snprintf(str, format, frame->int_val);
		gtk_entry_set_text(GTK_ENTRY(entry), str);
	} else if (META_FIELD_FLOAT(frame->type)) {
		char str[MAXLEN];
		char * format = meta_get_field_renderfmt(frame->type);
		widget = entry = gtk_entry_new();
		arr_snprintf(str, format, frame->float_val);
		gtk_entry_set_text(GTK_ENTRY(entry), str);
	} else {
		if (meta->writable && (frame->type == META_FIELD_GENRE)) {
			make_genre_combo(frame, &widget, &entry);
		} else {
			widget = entry = gtk_entry_new();
			gtk_entry_set_text(GTK_ENTRY(entry), frame->field_val);
		}
	}

	if ((META_FIELD_TEXT(frame->type)) ||
	    (META_FIELD_INT(frame->type)) ||
	    (META_FIELD_FLOAT(frame->type))) {
		if (!meta->writable) {
			gtk_widget_set_can_focus(entry, FALSE);
			gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
		} else {
			g_signal_connect(G_OBJECT(widget), "changed",
					 G_CALLBACK(fi_entry_changed_cb), (gpointer)fi);
		}
		gtk_widget_set_size_request(widget, VAL_WIDGET_WIDTH, -1);
	}
	return widget;
}


import_data_t *
make_import_data_from_frame(fi_t * fi, meta_frame_t * frame, char * label, size_t label_size) {

	import_data_t * data = import_data_new();
	trashlist_add(fi->trash, data);
	data->fi = fi;
	data->frame = frame;

	switch (frame->type) {
	case META_FIELD_TITLE:
		data->dest_type = IMPORT_DEST_TITLE;
		g_strlcpy(label, _("Import as Title"), label_size);
		break;
	case META_FIELD_ARTIST:
		data->dest_type = IMPORT_DEST_ARTIST;
		g_strlcpy(label, _("Import as Artist"), label_size);
		break;
	case META_FIELD_ALBUM:
		data->dest_type = IMPORT_DEST_RECORD;
		g_strlcpy(label, _("Import as Record"), label_size);
		break;
	case META_FIELD_DATE:
		data->dest_type = IMPORT_DEST_YEAR;
		g_strlcpy(label, _("Import as Year"), label_size);
		break;
	case META_FIELD_TRACKNO:
		data->dest_type = IMPORT_DEST_NUMBER;
		g_strlcpy(label, _("Import as Track No."), label_size);
		break;
	case META_FIELD_RG_TRACK_GAIN:
	case META_FIELD_RG_ALBUM_GAIN:
	case META_FIELD_RVA2:
		data->dest_type = IMPORT_DEST_RVA;
		g_strlcpy(label, _("Import as RVA"), label_size);
		break;
	default:
		data->dest_type = IMPORT_DEST_COMMENT;
		g_strlcpy(label, _("Add to Comments"), label_size);
		break;
	}

	return data;
}


GtkWidget *
fi_procframe_importbtn(fi_t * fi, meta_frame_t * frame) {

	char label[MAXLEN];
	GtkWidget * button = gtk_button_new();
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(import_button_pressed),
			 (gpointer)make_import_data_from_frame(fi, frame, label, CHAR_ARRAY_SIZE(label)));
	gtk_button_set_label(GTK_BUTTON(button), label);
	return button;
}


void
fi_fill_tagcombo(GtkComboBox * combo, int addable_tags) {

	int i;
	int tag = 1;

	if (!GTK_IS_COMBO_BOX(combo))
		return;

	for (i = 0; i < 100; i++) {
		gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(combo), 0);
	}

	if (addable_tags == 0) {
		gtk_widget_set_sensitive(GTK_WIDGET(combo), FALSE);
		return;
	}

	gtk_widget_set_sensitive(GTK_WIDGET(combo), TRUE);

	while (tag <= META_TAG_MAX) {
		if (addable_tags & tag) {
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), meta_get_tagname(tag));
		}
		tag <<= 1;
	}

	gtk_combo_box_set_active(combo, 0);
}

gint
fi_fill_combo_cmp(gconstpointer a, gconstpointer b) {

	int field1 = GPOINTER_TO_INT(a);
	int field2 = GPOINTER_TO_INT(b);
	char * str1;
	char * str2;

	if (!meta_get_fieldname(field1, &str1)) {
		fprintf(stderr, "fi_fill_combo_cmp: programmer error #1\n");
	}
	if (!meta_get_fieldname(field2, &str2)) {
		fprintf(stderr, "fi_fill_combo_cmp: programmer error #2\n");
	}

	return strcmp(str1, str2);
}


void
fi_fill_combo_foreach(gpointer data, gpointer user_data) {

	int field = GPOINTER_TO_INT(data);
	char * str;
	GtkComboBox * combo = (GtkComboBox *)user_data;
	if (!meta_get_fieldname(field, &str)) {
		fprintf(stderr, "fi_fill_combo_foreach: programmer error\n");
	}
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), str);
}

void
fi_fill_combo(GtkComboBox * combo, GSList * slist) {

	int i;
	GSList * sorted_list = g_slist_copy(slist);
	for (i = 0; i < 100; i++) {
		gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(combo), 0);
	}
	sorted_list = g_slist_sort(sorted_list, fi_fill_combo_cmp);
	g_slist_foreach(sorted_list, fi_fill_combo_foreach, combo);
	g_slist_free(sorted_list);
	gtk_combo_box_set_active(combo, 0);
}

typedef struct {
	fi_t * fi;
	meta_frame_t * frame;
	GtkWidget * label;
	GtkWidget * entry;
	GtkWidget * importbtn;
	GtkWidget * delbtn;
} fi_del_t;

void
fi_del_button_pressed(GtkWidget * widget, gpointer data) {

	fi_del_t * fi_del = (fi_del_t *)data;
	fi_t * fi = fi_del->fi;
	metadata_t * meta = fi_del->fi->meta;
	meta_frame_t * frame = fi_del->frame;

	if (frame->flags & META_FIELD_UNIQUE) {
		int page = lookup_page(fi, frame->tag);
		GtkWidget * combo = fi->pageidx[page].combo;
		GSList * slist = fi->pageidx[page].slist;
		slist = g_slist_append(slist, GINT_TO_POINTER(frame->type));
		fi->pageidx[page].slist = slist;
		fi_fill_combo(GTK_COMBO_BOX(combo), slist);
	}

	metadata_remove_frame(meta, frame);
	meta_frame_free(frame);

	gtk_widget_destroy(fi_del->label);
	gtk_widget_destroy(fi_del->entry);
	if (fi_del->importbtn != NULL) {
		gtk_widget_destroy(fi_del->importbtn);
	}
	gtk_widget_destroy(fi_del->delbtn);
	fi_mark_changed(fi);
}

GtkWidget *
fi_procframe_delbtn(fi_t * fi, meta_frame_t * frame,
		    GtkWidget * label, GtkWidget * entry, GtkWidget * importbtn) {

	fi_del_t * fi_del;
	GtkWidget * button = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(button),
			     gtk_image_new_from_stock(GTK_STOCK_DELETE,
						      GTK_ICON_SIZE_MENU));

	if (frame->flags & META_FIELD_MANDATORY) {
		gtk_widget_set_sensitive(button, FALSE);
		return button;
	}

	fi_del = calloc(1, sizeof(fi_del_t));
	fi_del->fi = fi;
	fi_del->frame = frame;
	fi_del->label = label;
	fi_del->entry = entry;
	fi_del->importbtn = importbtn;
	fi_del->delbtn = button;
	trashlist_add(fi->trash, fi_del);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(fi_del_button_pressed),
			 (gpointer)fi_del);
	return button;
}


typedef struct {
	fi_t * fi;
	int tag;
} fi_add_t;


void
fi_add_button_pressed(GtkWidget * widget, gpointer data) {

	fi_add_t * fi_add = (fi_add_t *)data;
	fi_t * fi = fi_add->fi;
	metadata_t * meta = fi_add->fi->meta;
	int page = lookup_page(fi, fi_add->tag);
	GtkWidget * combo = fi->pageidx[page].combo;
	GSList * slist = fi->pageidx[page].slist;

	meta_frame_t * frame = meta_frame_new();
	int type;
	char * str;

	/* Create new frame */
	char * combo_entry = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
	if (combo_entry == NULL) {
		return;
	}

	type = meta_frame_type_from_name(combo_entry);
	g_free(combo_entry);

	frame->tag = fi_add->tag;
	frame->type = type;
	frame->flags = meta_get_default_flags(fi_add->tag, type);
	if (meta_get_fieldname(type, &str)) {
		frame->field_name = strdup(str);
	} else {
		fprintf(stderr, "fi_add_button_pressed: programmer error\n");
	}
	if (options.metaedit_auto_clone) {
		metadata_clone_frame(meta, frame);
	}
	if (frame->field_val == NULL) {
		frame->field_val = strdup("");
	}
	frame->next = NULL;

	metadata_add_frame(meta, frame);

	if (frame->flags & META_FIELD_UNIQUE) {
		slist = g_slist_remove(slist, GINT_TO_POINTER(frame->type));
		fi->pageidx[page].slist = slist;
		fi_fill_combo(GTK_COMBO_BOX(combo), slist);
	}

	fi_procframe_ins(fi, frame);
	fi_mark_changed(fi);
}

GtkWidget *
fi_procframe_addbtn(fi_t * fi, int tag) {

	fi_add_t * fi_add;
	GtkWidget * button = gui_stock_label_button(_("Add"), GTK_STOCK_ADD);

	fi_add = calloc(1, sizeof(fi_add_t));
	fi_add->fi = fi;
	fi_add->tag = tag;
	trashlist_add(fi->trash, fi_add);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(fi_add_button_pressed),
			 (gpointer)fi_add);
	return button;
}

void
fi_addtag_button_pressed(GtkWidget * widget, gpointer data) {

	fi_t * fi = (fi_t *)data;
	metadata_t * meta = fi->meta;
	meta_frame_t * frame;
	int tag;
	char * combo_entry = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(fi->combo));
	if (combo_entry == NULL) {
		return;
	}
	frame = meta_frame_new();
	tag = frame->tag = meta_tag_from_name(combo_entry);
	g_free(combo_entry);

	metadata_add_mandatory_frames(meta, frame->tag);

	fi_procframe_add_tag_page(fi, frame);
	fi->addable_tags &= ~frame->tag;
	fi_fill_tagcombo(GTK_COMBO_BOX(fi->combo), fi->addable_tags);
	meta_frame_free(frame);

	/* display fields, if any */
	frame = metadata_get_frame_by_tag(meta, tag, NULL);
	while (frame != NULL) {
		fi_procframe_ins(fi, frame);
		frame = metadata_get_frame_by_tag(meta, tag, frame);
	}
	fi_mark_changed(fi);
}

GtkWidget *
fi_procframe_addtagbtn(fi_t * fi) {

	GtkWidget * button = gui_stock_label_button(_("Add"), GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(fi_addtag_button_pressed),
			 (gpointer)fi);
	return button;
}


#ifdef HAVE_MOD
void
fi_procframe_ins_modinfo(fi_t * fi, meta_frame_t * frame) {

	int page = lookup_page(fi, frame->tag);
	GtkWidget * table = fi->pageidx[page].table;
	GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	module_info_fill_page(fi, frame, vbox);
	gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show_all(fi->nb);
}
#endif /* HAVE_MOD */


void
fi_procframe_ins(fi_t * fi, meta_frame_t * frame) {

	metadata_t * meta = fi->meta;
	int page = lookup_page(fi, frame->tag);
	GtkWidget * table = fi->pageidx[page].table;
	int n_rows = fi->pageidx[page].n_rows;
	int n_cols = fi->pageidx[page].n_cols;
	int col = 0;

	GtkWidget * label = NULL;
	GtkWidget * entry = NULL;
	GtkWidget * importbtn = NULL;
	GtkWidget * delbtn = NULL;

	if (frame->type == META_FIELD_HIDDEN) {
		return;
	}

#ifdef HAVE_MOD
	if (frame->type == META_FIELD_MODINFO) {
		fi_procframe_ins_modinfo(fi, frame);
		return;
	}
#endif /* HAVE_MOD */

	if (frame->type == META_FIELD_APIC) {
		/* only display first APIC found */
		meta_frame_t * first_apic = metadata_get_frame_by_type(fi->meta, META_FIELD_APIC, NULL);
		if (frame == first_apic &&
		    (find_cover_filename(fi->filename) == NULL || !options.use_external_cover_first)) {
			display_cover_from_binary(fi->cover_image_area, fi->event_box, fi->cover_align,
						  THUMB_SIZE, THUMB_SIZE, frame->data, frame->length, FALSE, TRUE);
			fi->cover_set_from_apic = TRUE;
		} else {
			display_cover(fi->cover_image_area, fi->event_box, fi->cover_align,
				      THUMB_SIZE, THUMB_SIZE, fi->filename, FALSE, TRUE);
		}
	}

	++n_rows;
	fi->pageidx[page].n_rows = n_rows;
	gtk_table_resize(GTK_TABLE(table), n_rows, n_cols);

	/* Field label */
	label = fi_procframe_label(fi, frame);
	gtk_table_attach(GTK_TABLE(table), label, col, col+1, n_rows-1, n_rows,
			 GTK_FILL, GTK_FILL, 5, 3);
	++col;

	/* Field entry */
	entry = fi_procframe_entry(fi, frame);
	gtk_table_attach(GTK_TABLE(table), entry, col, col+1, n_rows-1, n_rows,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);
	if (frame->type != META_FIELD_APIC) {
		frame->source = entry;
	}
	++col;

	/* Import button */
	if (fi->allow_ms_import) {
		importbtn = fi_procframe_importbtn(fi, frame);
		gtk_table_attach(GTK_TABLE(table), importbtn, col, col+1, n_rows-1, n_rows,
				 GTK_FILL, GTK_FILL, 5, 3);
		++col;
	}

	/* Delete button */
	if (meta->writable) {
		delbtn = fi_procframe_delbtn(fi, frame,
					     label, entry, importbtn);
		gtk_table_attach(GTK_TABLE(table), delbtn, col, col+1, n_rows-1, n_rows,
				 GTK_FILL, GTK_FILL, 5, 3);
		++col;
	}

	if (meta->writable && (frame->flags & META_FIELD_UNIQUE)) {
		GtkWidget * combo = fi->pageidx[page].combo;
		GSList * slist = fi->pageidx[page].slist;
		slist = g_slist_remove(slist, GINT_TO_POINTER(frame->type));
		fi->pageidx[page].slist = slist;
		fi_fill_combo(GTK_COMBO_BOX(combo), slist);
	}

	gtk_widget_show_all(fi->nb);
}

typedef struct {
	fi_t * fi;
	int tag;
} fi_deltag_t;

void
fi_deltag_button_pressed(GtkWidget * widget, gpointer data) {

	fi_deltag_t * fi_deltag = (fi_deltag_t *)data;
	fi_t * fi = fi_deltag->fi;
	int tag = fi_deltag->tag;
	metadata_t * meta = fi->meta;
	meta_frame_t * frame;
	int i;

	/* Remove all frames with frame->tag == tag */
	frame = meta->root;
	while (frame != NULL) {
		if (frame->tag == tag) {
			meta_frame_t * f = frame->next;
			metadata_remove_frame(meta, frame);
			meta_frame_free(frame);
			frame = f;
		} else {
			frame = frame->next;
		}
	}

	/* Remove notebook page and update fi->pageidx */
	for (i = FI_MAXPAGES-1; i >= 0; i--) {
		if (fi->pageidx[i].tag == tag) {
			int k;
			for (k = i; k < FI_MAXPAGES-1; k++) {
				fi->pageidx[k] = fi->pageidx[k+1];
			}
			fi->pageidx[FI_MAXPAGES-1].tag = -1;
			gtk_notebook_remove_page(GTK_NOTEBOOK(fi->nb), i);
			fi->n_pages--;
			break;
		}
	}

	fi->addable_tags |= tag;
	fi_fill_tagcombo(GTK_COMBO_BOX(fi->combo), fi->addable_tags);
	fi_mark_changed(fi);
}

GtkWidget *
fi_procframe_deltagbtn(fi_t * fi, int tag) {

	fi_deltag_t * fi_deltag;
	GtkWidget * button = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(button),
			     gtk_image_new_from_stock(GTK_STOCK_DELETE,
						      GTK_ICON_SIZE_MENU));

	fi_deltag = calloc(1, sizeof(fi_deltag_t));
	fi_deltag->fi = fi;
	fi_deltag->tag = tag;
	trashlist_add(fi->trash, fi_deltag);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(fi_deltag_button_pressed),
			 (gpointer)fi_deltag);
	return button;
}

void
fi_procframe_add_tag_page(fi_t * fi, meta_frame_t * frame) {

	metadata_t * meta = fi->meta;
	GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	GtkWidget * vbox_padding = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget * scrwin = gtk_scrolled_window_new(NULL, NULL);
	GtkWidget * table = gtk_table_new(0, fi_tabwidth(fi, meta), FALSE);
	GtkWidget * label = gtk_label_new(meta_get_tagname(frame->tag));
	GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget * combo = gtk_combo_box_text_new();
	GtkWidget * addbtn, * delbtn;
	GSList * slist = meta_get_possible_fields(frame->tag);

	gtk_box_pack_start(GTK_BOX(vbox_padding), table, TRUE, TRUE, 7);
	gtk_box_pack_start(GTK_BOX(vbox), scrwin, TRUE, TRUE, 0);
	gtk_widget_set_size_request(scrwin, -1, 275);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrwin), GTK_SHADOW_NONE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrwin), vbox_padding);

	gtk_notebook_append_page(GTK_NOTEBOOK(fi->nb), vbox, label);
	fi->n_pages++;
	fi->pageidx[fi->n_pages-1].tag = frame->tag;
	fi->pageidx[fi->n_pages-1].table = table;
	fi->pageidx[fi->n_pages-1].combo = combo;
	fi->pageidx[fi->n_pages-1].slist = slist;
	fi->pageidx[fi->n_pages-1].n_rows = 0;
	fi->pageidx[fi->n_pages-1].n_cols = fi_tabwidth(fi, meta);

	if (meta->writable) {
		addbtn = fi_procframe_addbtn(fi, frame->tag);
		gtk_box_pack_start(GTK_BOX(hbox), addbtn, FALSE, FALSE, 5);

		label = gtk_label_new(_("field:"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

		fi_fill_combo(GTK_COMBO_BOX(combo), slist);
		gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, FALSE, 5);

		delbtn = fi_procframe_deltagbtn(fi, frame->tag);
		gtk_box_pack_end(GTK_BOX(hbox), delbtn, FALSE, FALSE, 5);

		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

		fi->addable_tags &= ~frame->tag;
		fi_fill_tagcombo(GTK_COMBO_BOX(fi->combo), fi->addable_tags);
	}
	gtk_widget_show_all(fi->nb);
}

void
fi_procframe(fi_t * fi, meta_frame_t * frame) {

	int page = lookup_page(fi, frame->tag);
	if (page == -1) {
		fi_procframe_add_tag_page(fi, frame);
	}
	fi_procframe_ins(fi, frame);
}

typedef struct {
	metadata_t * meta;
	void * data;
} fi_procmeta_wait_data_t;

void fi_procmeta(metadata_t * meta, void * data);

gboolean
fi_procmeta_wait(gpointer cb_data) {

	fi_procmeta_wait_data_t * wd = (fi_procmeta_wait_data_t *)cb_data;
	fi_procmeta(wd->meta, wd->data);
	free(wd);
	return FALSE;
}

void
fi_procmeta(metadata_t * meta, void * data) {

	fi_t * fi = (fi_t *)data;
	meta_frame_t * frame;
	int i;

	if (fi->bail_out) {
		/* this condition signals that file_decoder_open in show_file_info has failed */
		fi_delete(fi);
		return;
	}

	if ((fi->nb == NULL) || !GTK_IS_NOTEBOOK(fi->nb)) {
		fi_procmeta_wait_data_t * fi_procmeta_wait_data = calloc(1, sizeof(fi_procmeta_wait_data_t));
		fi_procmeta_wait_data->meta = meta;
		fi_procmeta_wait_data->data = data;
		aqualung_timeout_add(100, fi_procmeta_wait, (gpointer)fi_procmeta_wait_data);
		return;
	}

	if (fi->meta == meta) {
		return;
	}

	/* remove possible previous metadata pages from notebook */
	for (i = FI_MAXPAGES-1; i > 0; i--) {
		if (fi->pageidx[i].tag != -1) {
			gtk_notebook_remove_page(GTK_NOTEBOOK(fi->nb), i);
			fi->pageidx[i].tag = -1;
		}
	}
	fi->n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(fi->nb));

	if (fi->meta != NULL) {
		metadata_free(fi->meta);
	}
	fi->meta = meta;
	fi->addable_tags = meta->valid_tags;
	frame = meta->root;

	if (fi->meta->writable && (fi->save_button == NULL)) {
		fi->save_button = gtk_button_new_from_stock(GTK_STOCK_SAVE);
		g_signal_connect(fi->save_button, "clicked",
				 G_CALLBACK(fi_save), (gpointer)fi);
		gtk_table_attach(GTK_TABLE(fi->button_table), fi->save_button,
				 0, 1, 0, 1, GTK_FILL, GTK_FILL, 3, 0);
		gtk_widget_show_all(fi->info_window);
	}

	if (fi->meta->writable && (fi->add_tag_table == NULL)) {
		GtkWidget * addbtn;

		fi->add_tag_table = gtk_table_new(1, 3, FALSE);
		gtk_box_pack_start(GTK_BOX(fi->hbox), fi->add_tag_table, FALSE, TRUE, 2);

		addbtn = fi_procframe_addtagbtn(fi);
		gtk_table_attach(GTK_TABLE(fi->add_tag_table), addbtn,
				 0, 1, 0, 1, GTK_FILL, GTK_FILL, 3, 0);

		gtk_table_attach(GTK_TABLE(fi->add_tag_table), gtk_label_new(_("tag:")),
				 1, 2, 0, 1, GTK_FILL, GTK_FILL, 3, 0);

		fi->combo = gtk_combo_box_text_new();
		fi_fill_tagcombo(GTK_COMBO_BOX(fi->combo), fi->addable_tags);
		gtk_table_attach(GTK_TABLE(fi->add_tag_table), fi->combo,
				 2, 3, 0, 1, GTK_FILL, GTK_FILL, 3, 0);

		gtk_widget_show_all(fi->info_window);
	}

	while (frame != NULL) {
		fi_procframe(fi, frame);
		frame = frame->next;
	}

	fi_set_page(fi);
}


gboolean
fi_cover_press_button_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {

	fi_t * fi = (fi_t *)data;

	if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
		meta_frame_t * frame;
		frame = metadata_get_frame_by_type(fi->meta, META_FIELD_APIC, NULL);
		if (frame != NULL && (find_cover_filename(fi->filename) == NULL || !options.use_external_cover_first)) {
			display_zoomed_cover_from_binary(fi->info_window, fi->event_box, frame->data, frame->length);
		} else {
			display_zoomed_cover(fi->info_window, fi->event_box, (gchar *)fi->filename);
		}
        }
        return TRUE;
}

void
fi_add_file_table_row(char * caption, GtkWidget ** entry, GtkWidget * table, int row) {
	GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget * label = gtk_label_new(caption);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, row, row+1, GTK_FILL, GTK_FILL, 5, 3);
	*entry = gtk_entry_new();
	gtk_widget_set_size_request(*entry, 350, -1);
	gtk_widget_set_can_focus(*entry, FALSE);
	gtk_editable_set_editable(GTK_EDITABLE(*entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), *entry, 1, 2, row, row+1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);
}

void
fi_set_common_entries(fi_t * fi) {
        char str[MAXLEN];
	gchar * file_display;

	if (!fi->media_ok) {
		if (GTK_IS_ENTRY(fi->entry_name)) gtk_entry_set_text(GTK_ENTRY(fi->entry_name), fi->name);
		if (GTK_IS_ENTRY(fi->entry_path)) gtk_entry_set_text(GTK_ENTRY(fi->entry_path), "(unable to open)");
		if (GTK_IS_ENTRY(fi->entry_format)) gtk_entry_set_text(GTK_ENTRY(fi->entry_format), "");
		if (GTK_IS_ENTRY(fi->entry_length)) gtk_entry_set_text(GTK_ENTRY(fi->entry_length), "");
		if (GTK_IS_ENTRY(fi->entry_sr)) gtk_entry_set_text(GTK_ENTRY(fi->entry_sr), "");
		if (GTK_IS_ENTRY(fi->entry_ch)) gtk_entry_set_text(GTK_ENTRY(fi->entry_ch), "");
		if (GTK_IS_ENTRY(fi->entry_bw)) gtk_entry_set_text(GTK_ENTRY(fi->entry_bw), "");
		if (GTK_IS_ENTRY(fi->entry_nsamples)) gtk_entry_set_text(GTK_ENTRY(fi->entry_nsamples), "");
		if (GTK_IS_ENTRY(fi->entry_mode)) gtk_entry_set_text(GTK_ENTRY(fi->entry_mode), "");
		return;
	}

	/* Heading */

	gtk_entry_set_text(GTK_ENTRY(fi->entry_name), fi->name);

	file_display = g_filename_display_name(fi->filename);
	gtk_entry_set_text(GTK_ENTRY(fi->entry_path), file_display);
	g_free(file_display);

	if (!fi->cover_set_from_apic) {
		if (options.show_cover_for_ms_tracks_only == TRUE) {
			if (fi->display_cover == TRUE) {
				display_cover(fi->cover_image_area, fi->event_box, fi->cover_align,
					      THUMB_SIZE, THUMB_SIZE, fi->filename, TRUE, TRUE);
			} else {
				hide_cover_thumbnail();
			}
		} else {
			display_cover(fi->cover_image_area, fi->event_box, fi->cover_align,
				      THUMB_SIZE, THUMB_SIZE, fi->filename, TRUE, TRUE);
		}
	}

	/* Audio data */

	gtk_entry_set_text(GTK_ENTRY(fi->entry_format), fi->fileinfo.format_str);

	if (fi->fileinfo.total_samples == 0) {
		arr_strlcpy(str, "N/A");
	} else {
		sample2time(fi->fileinfo.sample_rate, fi->fileinfo.total_samples, str, CHAR_ARRAY_SIZE(str), 0);
	}
	gtk_entry_set_text(GTK_ENTRY(fi->entry_length), str);

	arr_snprintf(str, _("%ld Hz"), fi->fileinfo.sample_rate);
	gtk_entry_set_text(GTK_ENTRY(fi->entry_sr), str);

	if (fi->fileinfo.is_mono) {
		arr_strlcpy(str, _("MONO"));
	} else {
		arr_strlcpy(str, _("STEREO"));
	}
	gtk_entry_set_text(GTK_ENTRY(fi->entry_ch), str);

	if (fi->fileinfo.bps == 0) {
		arr_strlcpy(str, "N/A kbit/s");
	} else {
		format_bps_label(fi->fileinfo.bps, fi->fileinfo.format_flags, str, CHAR_ARRAY_SIZE(str));
	}
	gtk_entry_set_text(GTK_ENTRY(fi->entry_bw), str);

	if (fi->fileinfo.total_samples == 0) {
		arr_strlcpy(str, "N/A");
	} else {
		arr_snprintf(str, "%lld", fi->fileinfo.total_samples);
	}
	gtk_entry_set_text(GTK_ENTRY(fi->entry_nsamples), str);

#ifdef HAVE_WAVPACK
	if (!fi->is_cdda && fi->fdec && fi->fdec->file_lib == WAVPACK_LIB) {
		wavpack_pdata_t * pd = (wavpack_pdata_t *)fi->dec->pdata;
		int mode = WavpackGetMode(pd->wpc);

		if ((mode & MODE_LOSSLESS) && (mode & MODE_WVC)) {
			arr_strlcpy(str, "Hybrid Lossless");
		} else if (mode & MODE_LOSSLESS) {
			arr_strlcpy(str, "Lossless");
		} else {
			arr_strlcpy(str, "Hybrid Lossy");
		}
		cut_trailing_whitespace(str);
		gtk_entry_set_text(GTK_ENTRY(fi->entry_mode), str);
	}
#endif /* HAVE_WAVPACK */
}

/* Fill up internal data with values from referenced file.
 * Return TRUE if successful, FALSE on error.
 */
gboolean
fi_media_init(fi_t * fi) {

	fi->is_cdda = 0;
	fi->bail_out = 0;
	fi->trash = trashlist_new();

#ifdef HAVE_CDDA
        if (g_str_has_prefix(fi->filename, "CDDA ")) {

                char device_path[CDDA_MAXLEN];
                long hash;
                int track;
                cdda_drive_t * drive;

                if (sscanf(fi->filename, "CDDA %s %lX %u", device_path, &hash, &track) < 3) {
                        return FALSE;
                }

                drive = cdda_get_drive_by_device_path(device_path);
                if ((drive == NULL) || (drive->disc.hash == 0L)) {
			return FALSE;
		}

		fi->is_cdda = 1;

		fi->fileinfo.format_str = _("Audio CD");
		fi->fileinfo.sample_rate = 44100;
		fi->fileinfo.is_mono = 0;
		fi->fileinfo.format_flags = 0;
		fi->fileinfo.bps = 2*16*44100;
		fi->fileinfo.total_samples = (drive->disc.toc[track] - drive->disc.toc[track-1]) * 588;
        }
#endif /* HAVE_CDDA */

	if (!fi->is_cdda) {
		fi->fdec = file_decoder_new();
		if (fi->fdec == NULL) {
			return FALSE;
		}

		file_decoder_set_meta_cb(fi->fdec, fi_procmeta, fi);
		if (file_decoder_open(fi->fdec, fi->filename) != 0) {
			fi->bail_out = 1;
			return FALSE;
		}

		file_decoder_send_metadata(fi->fdec);
		fi->dec = (decoder_t *)fi->fdec->pdec;

		fi->fileinfo.format_str = fi->dec->format_str;
		fi->fileinfo.sample_rate = fi->fdec->fileinfo.sample_rate;
		fi->fileinfo.is_mono = fi->fdec->fileinfo.is_mono;
		fi->fileinfo.format_flags = fi->fdec->fileinfo.format_flags;
		fi->fileinfo.bps = fi->fdec->fileinfo.bps;
		fi->fileinfo.total_samples = fi->fdec->fileinfo.total_samples;
	}
	return TRUE;
}

void
fi_reload(fi_t * fi, GtkTreeIter iter) {
	int i;

	fi->selected_tag = fi->pageidx[gtk_notebook_get_current_page(GTK_NOTEBOOK(fi->nb))].tag;

	fi_unload(fi);
	fi->iter_track = iter;
	fi->mfun(fi->model, fi->iter_track, &fi->name, &fi->filename);

	/* remove any metadata pages from notebook */
	while (gtk_notebook_get_n_pages(GTK_NOTEBOOK(fi->nb)) > 1) {
		gtk_notebook_remove_page(GTK_NOTEBOOK(fi->nb), 1);
	}
	fi->n_pages = 1;
	for (i = 1; i < FI_MAXPAGES; i++) {
		fi->pageidx[i].tag = FALSE;
	}

	fi->media_ok = fi_media_init(fi);
	fi_set_common_entries(fi);
}

gint
fi_prev(GtkWidget * widget, gpointer data) {
	fi_t * fi = data;
	GtkTreeModel * model = fi->model;
	GtkTreeIter iter = fi->iter_track;
	GtkTreeIter prev;

	if (fi_can_close(fi) != TRUE) {
		return TRUE;
	}
	if (!tree_model_prev_iter(model, &iter, &prev, fi->mindepth)) {
		return TRUE;
	}
	fi_reload(fi, prev);
	return TRUE;
}

gint
fi_next(GtkWidget * widget, gpointer data) {
	fi_t * fi = data;
	GtkTreeModel * model = fi->model;
	GtkTreeIter iter = fi->iter_track;
	GtkTreeIter next;

	if (fi_can_close(fi) != TRUE) {
		return TRUE;
	}
	if (!tree_model_next_iter(model, &iter, &next, fi->mindepth)) {
		return TRUE;
	}
	fi_reload(fi, next);
	return TRUE;
}

void
show_file_info(GtkTreeModel * model, GtkTreeIter iter_track,
	       fileinfo_model_func_t mfun, int mindepth,
	       gboolean allow_ms_import, gboolean display_cover) {

	GtkWidget * vbox;
	GtkWidget * hbox_t;
	GtkWidget * table;
	GtkWidget * dismiss_btn;

	GtkWidget * vbox_file;
	GtkWidget * label_file;
	GtkWidget * table_file;

	fi_t * fi = fi_new();
	if (fi == NULL) {
		return;
	}

	/* save arguments */
	fi->model = model;
	fi->iter_track = iter_track;
	fi->mfun = mfun;
	fi->mindepth = mindepth;
	fi->allow_ms_import = allow_ms_import;
	fi->display_cover = display_cover;
	mfun(fi->model, fi->iter_track, &fi->name, &fi->filename);

	fi->info_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	register_toplevel_window(fi->info_window, TOP_WIN_SKIN | TOP_WIN_TRAY);
        gtk_window_set_title(GTK_WINDOW(fi->info_window), _("File info"));
	gtk_window_set_transient_for(GTK_WINDOW(fi->info_window), GTK_WINDOW(main_window));
	gtk_window_set_position(GTK_WINDOW(fi->info_window), GTK_WIN_POS_CENTER);
        gtk_window_set_resizable(GTK_WINDOW(fi->info_window), TRUE);
	g_signal_connect(G_OBJECT(fi->info_window), "delete_event",
			 G_CALLBACK(info_window_close), (gpointer)fi);
        g_signal_connect(G_OBJECT(fi->info_window), "key_press_event",
			 G_CALLBACK(info_window_key_pressed), (gpointer)fi);
	gtk_container_set_border_width(GTK_CONTAINER(fi->info_window), 5);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(fi->info_window), vbox);

	hbox_t = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_t, FALSE, FALSE, 5);

	table = gtk_table_new(2, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(hbox_t), table, TRUE, TRUE, 4);

	fi_add_file_table_row(_("Track:"), &fi->entry_name, table, 0);
	fi_add_file_table_row(_("File:"), &fi->entry_path, table, 1);

	fi->cover_align = gtk_alignment_new(0.5f, 0.5f, 0.0f, 0.0f);
	gtk_box_pack_start(GTK_BOX(hbox_t), fi->cover_align, FALSE, FALSE, 0);
        fi->cover_image_area = gtk_image_new();
        fi->event_box = gtk_event_box_new ();
	gtk_container_add(GTK_CONTAINER(fi->cover_align), fi->event_box);
        gtk_container_add (GTK_CONTAINER (fi->event_box), fi->cover_image_area);
        g_signal_connect(G_OBJECT(fi->event_box), "button_press_event",
                         G_CALLBACK(fi_cover_press_button_cb), (gpointer)fi);
	fi->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_end(GTK_BOX(vbox), fi->hbox, FALSE, FALSE, 5);

	fi->nb = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), fi->nb, TRUE, TRUE, 5);

	/* Audio data notebook page */

	vbox_file = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	table_file = gtk_table_new(6, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_file), table_file, TRUE, TRUE, 10);
	label_file = gtk_label_new(_("Audio data"));
	fi->pageidx[0].tag = -1;
	gtk_notebook_append_page(GTK_NOTEBOOK(fi->nb), vbox_file, label_file);
	fi->n_pages = 1;

	fi_add_file_table_row(_("Format:"), &fi->entry_format, table_file, 0);
	fi_add_file_table_row(_("Length:"), &fi->entry_length, table_file, 1);
	fi_add_file_table_row(_("Samplerate:"), &fi->entry_sr, table_file, 2);
	fi_add_file_table_row(_("Channel count:"), &fi->entry_ch, table_file, 3);
	fi_add_file_table_row(_("Bandwidth:"), &fi->entry_bw, table_file, 4);
	fi_add_file_table_row(_("Total samples:"), &fi->entry_nsamples, table_file, 5);
#ifdef HAVE_WAVPACK
	if (!fi->is_cdda && fi->fdec && fi->fdec->file_lib == WAVPACK_LIB) {
		fi_add_file_table_row(_("Mode:"), &fi->entry_mode, table_file, 6);
	}
#endif /* HAVE_WAVPACK */

	/* end of notebook stuff */
	gtk_widget_grab_focus(fi->nb);

	fi->button_table = gtk_table_new(1, 2, FALSE);
	gtk_box_pack_end(GTK_BOX(fi->hbox), fi->button_table, FALSE, TRUE, 2);

	fi->prev_button = gui_stock_label_button(NULL, GTK_STOCK_GO_BACK);
	g_signal_connect(fi->prev_button, "clicked", G_CALLBACK(fi_prev), (gpointer)fi);
	gtk_table_attach(GTK_TABLE(fi->button_table), fi->prev_button,
			 1, 2, 0, 1, GTK_FILL, GTK_FILL, 3, 0);

	fi->next_button = gui_stock_label_button(NULL, GTK_STOCK_GO_FORWARD);
	g_signal_connect(fi->next_button, "clicked", G_CALLBACK(fi_next), (gpointer)fi);
	gtk_table_attach(GTK_TABLE(fi->button_table), fi->next_button,
			 2, 3, 0, 1, GTK_FILL, GTK_FILL, 3, 0);

        dismiss_btn = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	g_signal_connect(dismiss_btn, "clicked", G_CALLBACK(dismiss), (gpointer)fi);
	gtk_table_attach(GTK_TABLE(fi->button_table), dismiss_btn, 3, 4, 0, 1,
			 GTK_FILL, GTK_FILL, 3, 0);

	gtk_widget_show_all(fi->info_window);

	fi->media_ok = fi_media_init(fi);
	fi_set_common_entries(fi);
	fi_set_page(fi);

	/* fi_t object is freed in info_window destroy handlers */
}


#ifdef HAVE_MOD
/*
 * type = 0 for sample list
 * type != 0 for instrument list
 */
void
show_list(fi_t * fi, gint type) {

	GtkTreeIter iter;
	gint i, len;
	gchar temp[MAXLEN], number[MAXLEN];
	decoder_t * md_dec;
	mod_pdata_t * md_pd;

        md_dec = (decoder_t *)(fi->fdec->pdec);
        md_pd = (mod_pdata_t *)md_dec->pdata;

        if (type) {
                len = ModPlug_NumInstruments(md_pd->mpf);
        } else {
                len = ModPlug_NumSamples(md_pd->mpf);
        }

        if (len) {
                gtk_list_store_clear(fi->smp_instr_list_store);
                for(i = 0; i < len; i++) {
                        memset(temp, 0, MAXLEN-1);

                        if (type) {
                                ModPlug_InstrumentName(md_pd->mpf, i, temp);
                        } else {
                                ModPlug_SampleName(md_pd->mpf, i, temp);
                        }

                        arr_snprintf(number, "%2d", i);
                        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(fi->smp_instr_list_store), &iter, NULL, i);
                        gtk_list_store_append(fi->smp_instr_list_store, &iter);
                        gtk_list_store_set(fi->smp_instr_list_store, &iter, 0, number, 1, temp, -1);
                }
        }
}

void
set_first_row(fi_t * fi) {

	GtkTreeIter iter;
	GtkTreePath * visible_path;

        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(fi->smp_instr_list_store), &iter, NULL, 0);
        visible_path = gtk_tree_model_get_path(GTK_TREE_MODEL(fi->smp_instr_list_store), &iter);
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(fi->smp_instr_list), visible_path, NULL, TRUE);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(fi->smp_instr_list), visible_path,
				     NULL, TRUE, 1.0, 0.0);
        gtk_widget_grab_focus(GTK_WIDGET(fi->smp_instr_list));
}

void
radio_buttons_cb(GtkToggleButton * toggle_button, gpointer data) {

	fi_t * fi = (fi_t *)data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button))) {
                show_list(fi, 0);
        } else {
                show_list(fi, 1);
        }
        set_first_row(fi);
}

static void
module_info_add_row(char * caption, GtkWidget ** mod_label, GtkWidget * table, int row) {
	GtkWidget * label = gtk_label_new(caption);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
			 (GtkAttachOptions)(GTK_FILL), (GtkAttachOptions)(0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	
	*mod_label = gtk_label_new("");
	gtk_widget_show(*mod_label);
	gtk_table_attach(GTK_TABLE(table), *mod_label, 1, 2, row, row+1,
			 (GtkAttachOptions)(GTK_FILL), (GtkAttachOptions)(0), 0, 0);
}

void
module_info_fill_page(fi_t * fi, meta_frame_t * frame, GtkWidget * vbox) {

	mod_info * mdi = (mod_info *)frame->data;

	gchar *a_type[] = {
		"None", "MOD", "S3M", "XM", "MED", "MTM", "IT", "669",
		"ULT", "STM", "FAR", "WAV", "AMF", "AMS", "DSM", "MDL",
		"OKT", "MID", "DMF", "PTM", "DBM", "MT2", "AMF0", "PSM",
		"J2B", "UMX"
	};

	gint i, n;
	gchar temp[MAXLEN];
	GtkWidget *table;
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
	GtkWidget *scrolledwindow;

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

        hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_show (hbox2);
        gtk_box_pack_start (GTK_BOX (vbox), hbox2, TRUE, TRUE, 0);

        vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_show (vbox2);
        gtk_box_pack_start (GTK_BOX (hbox2), vbox2, FALSE, FALSE, 0);

        if (mdi->instruments) {
                table = gtk_table_new (5, 2, FALSE);
                gtk_widget_show (table);
                gtk_box_pack_start (GTK_BOX (vbox2), table, FALSE, FALSE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (table), 8);
                gtk_table_set_row_spacings (GTK_TABLE (table), 6);
                gtk_table_set_col_spacings (GTK_TABLE (table), 4);

		module_info_add_row(_("Type:"), &mod_type_label, table, 0);
		module_info_add_row(_("Channels:"), &mod_channels_label, table, 1);
		module_info_add_row(_("Patterns:"), &mod_patterns_label, table, 2);
		module_info_add_row(_("Samples:"), &mod_samples_label, table, 3);
		module_info_add_row(_("Instruments:"), &mod_instruments_label, table, 4);

                arr_snprintf(temp, "%d", mdi->instruments);
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
		g_signal_connect(G_OBJECT(samples_radiobutton), "toggled",
				 G_CALLBACK(radio_buttons_cb), (gpointer)fi);

                instruments_radiobutton = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON(samples_radiobutton), _("Instruments"));
                gtk_widget_show (instruments_radiobutton);
                gtk_table_attach (GTK_TABLE (table), instruments_radiobutton, 0, 1, 1, 2,
                                  (GtkAttachOptions) (GTK_FILL),
                                  (GtkAttachOptions) (0), 0, 0);
        } else {
                table = gtk_table_new (4, 2, FALSE);
                gtk_widget_show (table);
                gtk_box_pack_start (GTK_BOX (vbox2), table, FALSE, FALSE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (table), 8);
                gtk_table_set_row_spacings (GTK_TABLE (table), 4);
                gtk_table_set_col_spacings (GTK_TABLE (table), 4);

		module_info_add_row(_("Type:"), &mod_type_label, table, 0);
		module_info_add_row(_("Channels:"), &mod_channels_label, table, 1);
		module_info_add_row(_("Patterns:"), &mod_patterns_label, table, 2);
		module_info_add_row(_("Samples:"), &mod_samples_label, table, 3);
		module_info_add_row(_("Instruments:"), &mod_instruments_label, table, 4);
        }

        n = mdi->type;
        i = 0;

        while (n > 0) {         /* calculate module type index */
                n >>= 1;
                i++;
        }

        gtk_label_set_text (GTK_LABEL(mod_type_label), a_type[i]);
        arr_snprintf(temp, "%d", mdi->channels);
        gtk_label_set_text (GTK_LABEL(mod_channels_label), temp);
        arr_snprintf(temp, "%d", mdi->patterns);
        gtk_label_set_text (GTK_LABEL(mod_patterns_label), temp);
        arr_snprintf(temp, "%d", mdi->samples);
        gtk_label_set_text (GTK_LABEL(mod_samples_label), temp);

        vseparator = gtk_vseparator_new ();
        gtk_widget_show (vseparator);
        gtk_box_pack_start (GTK_BOX (hbox2), vseparator, FALSE, FALSE, 4);

        vbox3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
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

        fi->smp_instr_list_store = gtk_list_store_new(2,
						      G_TYPE_STRING,
						      G_TYPE_STRING);
        fi->smp_instr_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(fi->smp_instr_list_store));
	gtk_widget_set_name(fi->smp_instr_list, "samples_instruments_list");
        gtk_widget_show (fi->smp_instr_list);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("No."), renderer, "text", 0, NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
                                        GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(column), 3);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), FALSE);
        gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(column), 40);
	gtk_tree_view_append_column(GTK_TREE_VIEW(fi->smp_instr_list), column);
	column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer, "text", 1, NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
                                        GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_append_column(GTK_TREE_VIEW(fi->smp_instr_list), column);
        gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(column), 3);
        gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), FALSE);
        gtk_container_add (GTK_CONTAINER (scrolledwindow), fi->smp_instr_list);

        if (mdi->instruments && mdi->type == 0x4) { /* if XM module go to instrument page */
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(instruments_radiobutton), TRUE);
        } else {
                show_list(fi, 0);
                set_first_row(fi);
        }
}
#endif /* HAVE_MOD */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :

