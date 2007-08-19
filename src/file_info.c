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

#ifdef HAVE_WAVPACK
#include "decoder/dec_wavpack.h"
#endif /* HAVE_WAVPACK */

#include "common.h"
#include "utils.h"
#include "utils_gui.h"
#include "core.h"
#include "cdda.h"
#include "cover.h"
#include "decoder/file_decoder.h"
#include "music_browser.h"
#include "store_file.h"
#include "gui_main.h"
#include "options.h"
#include "trashlist.h"
#include "i18n.h"
#include "file_info.h"


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
extern GtkWidget* gui_stock_label_button(gchar *label, const gchar *stock);

GtkWidget * fi_event_box;

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
	int is_called_from_browser;
	char * filename;
	file_decoder_t * fdec;
	metadata_t * meta;
	trashlist_t * trash;
	GtkTreeModel * model;
	GtkTreeIter track_iter;
	GtkWidget * info_window;
	GtkWidget * hbox;
	GtkWidget * button_table;
	GtkWidget * save_button;
	GtkWidget * nb;
	GtkWidget * combo;
	int addable_tags; /* META_TAG_*, bitwise or-ed */
	gint n_pages; /* number of notebook pages */
	pageidx_t pageidx[FI_MAXPAGES];

	/* for MOD info */
	GtkWidget * smp_instr_list;
	GtkListStore * smp_instr_list_store;
} fi_t;

typedef struct {
	fi_t * fi;
	meta_frame_t * frame;
        int dest_type; /* one of IMPORT_DEST_* */
} import_data_t;


/*
typedef struct {
	char savefile[MAXLEN];
	unsigned int image_size;
	void * image_data;
} save_pic_t;
*/

extern GtkWidget * main_window;
extern GtkTreeStore * music_store;
extern GtkTreeSelection * music_select;
extern GtkWidget * music_tree;


#ifdef HAVE_MOD_INFO
void module_info_fill_page(fi_t * fi, meta_frame_t * frame, GtkWidget * vbox);
#endif /* HAVE_MOD_INFO */


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

	return fi;
}

void
fi_delete(fi_t * fi) {

	int i;

	if (fi == NULL)
		return;

	if (fi->fdec != NULL) {
		file_decoder_delete(fi->fdec);
	}
	free(fi->filename);

	for (i = 0; i < FI_MAXPAGES; i++) {
		g_slist_free(fi->pageidx[i].slist);
		fi->pageidx[i].slist = NULL;
	}
	free(fi);
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


gint
dismiss(GtkWidget * widget, gpointer data) {

	fi_t * fi = (fi_t *)data;

	unregister_toplevel_window(fi->info_window);
        gtk_widget_destroy(fi->info_window);
	trashlist_free(fi->trash);

	fi_delete(fi);
	return TRUE;
}


gint
info_window_close(GtkWidget * widget, GdkEventAny * event, gpointer data) {

	fi_t * fi = (fi_t *)data;

	unregister_toplevel_window(fi->info_window);
	trashlist_free(fi->trash);

	fi_delete(fi);
	return FALSE;
}


gint
info_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent, gpointer data) {

	fi_t * fi = (fi_t *)data;
	int page;

	switch (kevent->keyval) {
	case GDK_Escape:
		dismiss(NULL, data);
		return TRUE;
		break;
	case GDK_Return:
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
		frame->field_val = strdup(gtk_entry_get_text(GTK_ENTRY(frame->source)));
	} else if (META_FIELD_INT(frame->type)) {
		int val = 0;
		const char * str = gtk_entry_get_text(GTK_ENTRY(frame->source));
		if (sscanf(str, parsefmt, &val) < 1) {
			char msg[MAXLEN];
			snprintf(msg, MAXLEN-1,
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
			snprintf(msg, MAXLEN-1,
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
		/* TODO */
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

	if (status == 0) {
		if (fi->fdec->meta_write != NULL) {
			fi->fdec->meta_write(fi->fdec, fi->meta);
		} else {
			fprintf(stderr, "programmer error: fdec->meta_write == NULL\n");
		}
	}

	return TRUE;
}


void
import_button_pressed(GtkWidget * widget, gpointer gptr_data) {

	import_data_t * data = (import_data_t *)gptr_data;
	fi_t * fi = data->fi;
	meta_frame_t * frame = data->frame;
	GtkTreeModel * model = fi->model;
	GtkTreeIter track_iter = fi->track_iter;
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
		gtk_tree_store_set(music_store, &track_iter, MS_COL_NAME, frame->field_val, -1);
		music_store_mark_changed(&track_iter);
		break;
	case IMPORT_DEST_RECORD:
		gtk_tree_model_iter_parent(model, &record_iter, &track_iter);
		gtk_tree_store_set(music_store, &record_iter, MS_COL_NAME, frame->field_val, -1);
		music_store_mark_changed(&record_iter);
		break;
	case IMPORT_DEST_ARTIST:
		gtk_tree_model_iter_parent(model, &record_iter, &track_iter);
		gtk_tree_model_iter_parent(model, &artist_iter, &record_iter);
		gtk_tree_store_set(music_store, &artist_iter, MS_COL_NAME, frame->field_val, -1);
		gtk_tree_store_set(music_store, &artist_iter, MS_COL_SORT, frame->field_val, -1);
		path = gtk_tree_model_get_path(model, &track_iter);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(music_tree), path, NULL, TRUE, 0.5f, 0.0f);
		music_store_mark_changed(&artist_iter);
		break;
	case IMPORT_DEST_YEAR:
		gtk_tree_model_iter_parent(model, &record_iter, &track_iter);
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &record_iter, MS_COL_DATA, &record_data, -1);
		if (sscanf(frame->field_val, "%d", &record_data->year) < 1) {
			char msg[MAXLEN];
			snprintf(msg, MAXLEN-1,
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
		snprintf(tmp, MAXLEN-1, "%02d", frame->int_val);
		gtk_tree_store_set(music_store, &track_iter, MS_COL_SORT, tmp, -1);
		music_store_mark_changed(&track_iter);
		break;
	case IMPORT_DEST_COMMENT:
		gtk_tree_model_get(model, &track_iter, MS_COL_DATA, &track_data, -1);
		tmp[0] = '\0';
		if (track_data->comment != NULL) {
			strncat(tmp, track_data->comment, MAXLEN-1);
		}
		if ((tmp[strlen(tmp)-1] != '\n') && (tmp[0] != '\0')) {
			strncat(tmp, "\n", MAXLEN-1);
		}
		strncat(tmp, frame->field_val, MAXLEN-1);
		free_strdup(&track_data->comment, tmp);
		music_store_mark_changed(&track_iter);
		break;
	case IMPORT_DEST_RVA:
		gtk_tree_model_get(model, &track_iter, MS_COL_DATA, &track_data, -1);
		track_data->rva = frame->float_val;
		track_data->use_rva = 1;
		music_store_mark_changed(&track_iter);
		break;
	}
}


void
fi_set_page(fi_t * fi) {

	fi->n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(fi->nb));
        if (fi->n_pages > 1) {
                gtk_notebook_set_current_page(GTK_NOTEBOOK(fi->nb), options.tags_tab_first ? 1 : 0);
        }
}


int
lookup_page(fi_t * fi, int tag) {

	int i;
	for (i = 0; i < FI_MAXPAGES; i++) {
		if (fi->pageidx[i].tag == tag)
			return i;
	}
	return -1;
}

int
fi_tabwidth(fi_t * fi, metadata_t * meta) {

	/* tabwidth is 2, +1 if called from Music Store, +1 if editable */
	int tabwidth = 2;
	tabwidth += fi->is_called_from_browser? 1 : 0;
	tabwidth += meta->writable ? 1 : 0;
	return tabwidth;
}

GtkWidget *
fi_procframe_label(char * field_name) {

	char str[MAXLEN];
	GtkWidget * hbox = gtk_hbox_new(FALSE, 0);
	GtkWidget * label;
	snprintf(str, MAXLEN-1, "%s:", field_name);
	label = gtk_label_new(str);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	return hbox;
}

GtkWidget *
fi_procframe_entry(fi_t * fi, meta_frame_t * frame) {

	metadata_t * meta = fi->meta;
	GtkWidget * entry = NULL;
	if (META_FIELD_BIN(frame->type)) {
		/* TODO create image/whatever widget, etc. */
	} else {
		entry = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(entry), frame->field_val);
	}

	if ((META_FIELD_TEXT(frame->type)) ||
	    (META_FIELD_INT(frame->type)) ||
	    (META_FIELD_FLOAT(frame->type))) {
		if (!meta->writable) {
			GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
			gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
		}
		gtk_widget_set_size_request(entry, 250, -1);
	}
	return entry;
}


import_data_t *
make_import_data_from_frame(fi_t * fi, meta_frame_t * frame, char * label) {

	import_data_t * data = import_data_new();
	trashlist_add(fi->trash, data);
	data->fi = fi;
	data->frame = frame;
	
	switch (frame->type) {
	case META_FIELD_TITLE:
		data->dest_type = IMPORT_DEST_TITLE;
		strcpy(label, _("Import as Title"));
		break;
	case META_FIELD_ARTIST:
		data->dest_type = IMPORT_DEST_ARTIST;
		strcpy(label, _("Import as Artist"));
		break;
	case META_FIELD_ALBUM:
		data->dest_type = IMPORT_DEST_RECORD;
		strcpy(label, _("Import as Record"));
		break;
	case META_FIELD_DATE:
		data->dest_type = IMPORT_DEST_YEAR;
		strcpy(label, _("Import as Year"));
		break;
	case META_FIELD_TRACKNO:
		data->dest_type = IMPORT_DEST_NUMBER;
		strcpy(label, _("Import as Track No."));
		break;
	case META_FIELD_RG_TRACK_GAIN:
	case META_FIELD_RG_ALBUM_GAIN:
	case META_FIELD_RVA2:
		data->dest_type = IMPORT_DEST_RVA;
		strcpy(label, _("Import as RVA"));
		break;
	default:
		data->dest_type = IMPORT_DEST_COMMENT;
		strcpy(label, _("Add to Comments"));
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
			 (gpointer)make_import_data_from_frame(fi, frame, label));
	gtk_button_set_label(GTK_BUTTON(button), label);
	return button;
}


void
fi_fill_tagcombo(GtkComboBox * combo, int addable_tags) {

	int i;

	if (!GTK_IS_COMBO_BOX(combo))
		return;

	for (i = 0; i < 100; i++) {
		gtk_combo_box_remove_text(combo, 0);
	}

	if (addable_tags == 0) {
		gtk_widget_set_sensitive(GTK_WIDGET(combo), FALSE);
		return;
	}

	gtk_widget_set_sensitive(GTK_WIDGET(combo), TRUE);

	if (addable_tags & META_TAG_ID3v1)
		gtk_combo_box_append_text(combo, meta_get_tagname(META_TAG_ID3v1));
	if (addable_tags & META_TAG_ID3v2)
		gtk_combo_box_append_text(combo, meta_get_tagname(META_TAG_ID3v2));
	if (addable_tags & META_TAG_APE)
		gtk_combo_box_append_text(combo, meta_get_tagname(META_TAG_APE));
	if (addable_tags & META_TAG_OXC)
		gtk_combo_box_append_text(combo, meta_get_tagname(META_TAG_OXC));

	gtk_combo_box_set_active(combo, 0);	
}

void
fi_fill_combo_foreach(gpointer data, gpointer user_data) {

	int field = GPOINTER_TO_INT(data);
	char * str;
	GtkComboBox * combo = (GtkComboBox *)user_data;
	if (!meta_get_fieldname(field, &str)) {
		fprintf(stderr, "fi_fill_combo_foreach: programmer error\n");
	}
	gtk_combo_box_append_text(combo, str);
}

void
fi_fill_combo(GtkComboBox * combo, GSList * slist) {

	int i;
	for (i = 0; i < 100; i++) {
		gtk_combo_box_remove_text(combo, 0);
	}
	g_slist_foreach(slist, fi_fill_combo_foreach, combo);
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

	/* XXX testing only */
	printf("\n\nDump after removal\n");
	metadata_dump(meta);
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
	char * str;

	/* Create new frame */
	char * combo_entry = gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo));
	int type = meta_frame_type_from_name(combo_entry);
	/* XXX test */printf("\ncombo_entry = %s, frametype = %d\n", combo_entry, type);
	g_free(combo_entry);

	frame->tag = fi_add->tag;
	frame->type = type;
	frame->flags = meta_get_default_flags(fi_add->tag, type);
	if (meta_get_fieldname(type, &str)) {
		frame->field_name = strdup(str);
	} else {
		fprintf(stderr, "fi_add_button_pressed: programmer error\n");
	}
	frame->field_val = strdup("");
	frame->next = NULL;

	metadata_add_frame(meta, frame);

	if (frame->flags & META_FIELD_UNIQUE) {
		slist = g_slist_remove(slist, GINT_TO_POINTER(frame->type));
		fi->pageidx[page].slist = slist;
		fi_fill_combo(GTK_COMBO_BOX(combo), slist);
	}

	fi_procframe_ins(fi, frame);

	/* XXX testing only */
	printf("\n\nDump after addition\n");
	metadata_dump(meta);
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
	char * combo_entry = gtk_combo_box_get_active_text(GTK_COMBO_BOX(fi->combo));
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

	/* XXX testing only */
	printf("\n\nDump after tag addition\n");
	metadata_dump(meta);
}

GtkWidget *
fi_procframe_addtagbtn(fi_t * fi) {

	GtkWidget * button = gui_stock_label_button(_("Add"), GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(fi_addtag_button_pressed),
			 (gpointer)fi);
	return button;
}


#ifdef HAVE_MOD_INFO
void
fi_procframe_ins_modinfo(fi_t * fi, meta_frame_t * frame) {

	int page = lookup_page(fi, frame->tag);
	GtkWidget * table = fi->pageidx[page].table;
	GtkWidget * vbox = gtk_vbox_new(FALSE, 0);

	module_info_fill_page(fi, frame, vbox);
	gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show_all(fi->nb);		
}
#endif /* HAVE_MOD_INFO */


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

#ifdef HAVE_MOD_INFO
	if (frame->type == META_FIELD_MODINFO) {
		fi_procframe_ins_modinfo(fi, frame);
		return;
	}
#endif /* HAVE_MOD_INFO */

	++n_rows;
	fi->pageidx[page].n_rows = n_rows;
	gtk_table_resize(GTK_TABLE(table), n_rows, n_cols);

	/* Field label */
	label = fi_procframe_label(frame->field_name);
	gtk_table_attach(GTK_TABLE(table), label, col, col+1, n_rows-1, n_rows,
			 GTK_FILL, GTK_FILL, 5, 3);
	++col;

	/* Field entry */
	entry = fi_procframe_entry(fi, frame);
	gtk_table_attach(GTK_TABLE(table), entry, col, col+1, n_rows-1, n_rows,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 3);
	frame->source = entry;
	++col;

	/* Import button */
	if (fi->is_called_from_browser) {
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
	for (i = 0; i < FI_MAXPAGES; i++) {
		if (fi->pageidx[i].tag == tag) {
			int k;
			for (k = i; k < FI_MAXPAGES-1; k++) {
				fi->pageidx[k] = fi->pageidx[k+1];
			}
			fi->pageidx[FI_MAXPAGES-1].tag = -1;
			gtk_notebook_remove_page(GTK_NOTEBOOK(fi->nb), i);
			--fi->n_pages;
			break;
		}
	}

	fi->addable_tags |= tag;
	fi_fill_tagcombo(GTK_COMBO_BOX(fi->combo), fi->addable_tags);

	/* XXX testing only */
	printf("\n\nDump after tag removal\n");
	metadata_dump(meta);
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
	GtkWidget * vbox = gtk_vbox_new(FALSE, 4);
	GtkWidget * vbox_padding = gtk_vbox_new(FALSE, 0);
	GtkWidget * scrwin = gtk_scrolled_window_new(NULL, NULL);
	GtkWidget * table = gtk_table_new(0, fi_tabwidth(fi, meta), FALSE);
	GtkWidget * label = gtk_label_new(meta_get_tagname(frame->tag));
	GtkWidget * hbox = gtk_hbox_new(FALSE, 0);
	GtkWidget * combo = gtk_combo_box_new_text();
	GtkWidget * addbtn, * delbtn;
	GSList * slist = meta_get_possible_fields(frame->tag);
	
	gtk_box_pack_start(GTK_BOX(vbox_padding), table, TRUE, TRUE, 7);
	gtk_box_pack_start(GTK_BOX(vbox), scrwin, TRUE, TRUE, 0);
	gtk_widget_set_size_request(scrwin, -1, 275);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrwin),
					    GTK_SHADOW_NONE);
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

	if ((fi->nb == NULL) || !GTK_IS_NOTEBOOK(fi->nb)) {
		fi_procmeta_wait_data_t * fi_procmeta_wait_data =
			calloc(1, sizeof(fi_procmeta_wait_data_t));
		fi_procmeta_wait_data->meta = meta;
		fi_procmeta_wait_data->data = data;
		g_timeout_add(100, fi_procmeta_wait, (gpointer)fi_procmeta_wait_data);
		return;
	}

	/* remove possible previous metadata pages from notebook */
	for (i = 0; i < FI_MAXPAGES; i++) {
		if (fi->pageidx[i].tag != -1) {
			gtk_notebook_remove_page(GTK_NOTEBOOK(fi->nb), i);
			fi->pageidx[i].tag = -1;
		}
	}

	if (fi->meta != NULL) {
		metadata_free(fi->meta);
	}
	fi->meta = meta;
	fi->addable_tags = meta->valid_tags;
	frame = meta->root;

	/* XXX debug only */
	metadata_dump(meta);

	while (frame != NULL) {
		fi_procframe(fi, frame);
		frame = frame->next;
	}

	if (fi->meta->writable && (fi->save_button == NULL)) {
		fi->save_button = gtk_button_new_from_stock(GTK_STOCK_SAVE);
		g_signal_connect(fi->save_button, "clicked",
				 G_CALLBACK(fi_save), (gpointer)fi);
		gtk_table_attach(GTK_TABLE(fi->button_table), fi->save_button,
				 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 3);
		gtk_widget_show_all(fi->info_window);
	}

	if (fi->meta->writable && (fi->combo == NULL)) {

		GtkWidget * addbtn, * label;

		addbtn = fi_procframe_addtagbtn(fi);
		gtk_box_pack_start(GTK_BOX(fi->hbox), addbtn, FALSE, FALSE, 5);
		
		label = gtk_label_new(_("tag:"));
		gtk_box_pack_start(GTK_BOX(fi->hbox), label, FALSE, FALSE, 5);
		
		fi->combo = gtk_combo_box_new_text();
		fi_fill_tagcombo(GTK_COMBO_BOX(fi->combo), fi->addable_tags);
		gtk_box_pack_start(GTK_BOX(fi->hbox), fi->combo, FALSE, FALSE, 5);

		gtk_widget_show_all(fi->info_window);
	}

	fi_set_page(fi);
}


gboolean    
fi_cover_press_button_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {

	fi_t * fi = (fi_t *)data;

	if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
                display_zoomed_cover(fi->info_window, fi_event_box, (gchar *)fi->filename);
        }
        return TRUE;
}


void
show_file_info(char * name, char * file, int is_called_from_browser,
	       GtkTreeModel * model, GtkTreeIter track_iter) {

        char str[MAXLEN];
	gchar * file_display;

	GtkWidget * vbox;
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
	GtkWidget * fi_cover_align;

	GtkWidget * vbox_file;
	GtkWidget * label_file;
	GtkWidget * table_file;
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * entry;


	decoder_t * dec;

	int is_cdda = 0;
	fileinfo_t fileinfo;

	fi_t * fi = fi_new();
	if (fi == NULL) {
		return;
	}

#ifdef HAVE_CDDA
        if ((strlen(file) > 4) &&
            (file[0] == 'C') &&
            (file[1] == 'D') &&
            (file[2] == 'D') &&
            (file[3] == 'A')) {

                char device_path[CDDA_MAXLEN];
                long hash;
                int track;
                cdda_drive_t * drive;

                if (sscanf(file, "CDDA %s %lX %u", device_path, &hash, &track) < 3) {
                        return;
                }

                drive = cdda_get_drive_by_device_path(device_path);
                if ((drive == NULL) || (drive->disc.hash == 0L)) {
			return;
		}
		
		is_cdda = 1;
		
		fileinfo.format_str = _("Audio CD");
		fileinfo.sample_rate = 44100;
		fileinfo.is_mono = 0;
		fileinfo.format_flags = 0;
		fileinfo.bps = 2*16*44100;
		fileinfo.total_samples = (drive->disc.toc[track] - drive->disc.toc[track-1]) * 588;
        }
#endif /* HAVE_CDDA */

	if (!is_cdda) {
		fi->fdec = file_decoder_new();
		if (fi->fdec == NULL) {
			fi_delete(fi);
			return;
		}
		
		file_decoder_set_meta_cb(fi->fdec, fi_procmeta, fi);
		if (file_decoder_open(fi->fdec, file) != 0) {
			fi_delete(fi);
			return;		
		}
		
		dec = (decoder_t *)fi->fdec->pdec;

		fileinfo.format_str = dec->format_str;
		fileinfo.sample_rate = fi->fdec->fileinfo.sample_rate;
		fileinfo.is_mono = fi->fdec->fileinfo.is_mono;
		fileinfo.format_flags = fi->fdec->fileinfo.format_flags;
		fileinfo.bps = fi->fdec->fileinfo.bps;
		fileinfo.total_samples = fi->fdec->fileinfo.total_samples;
	}

	fi->is_called_from_browser = is_called_from_browser;
	fi->model = model;
	fi->track_iter = track_iter;
	fi->filename = strdup(file);
	fi->trash = trashlist_new();

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

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(fi->info_window), vbox);

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

	file_display = g_filename_display_name(file);
	entry_path = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry_path, GTK_CAN_FOCUS);
	gtk_entry_set_text(GTK_ENTRY(entry_path), file_display);
	g_free(file_display);
	gtk_editable_set_editable(GTK_EDITABLE(entry_path), FALSE);
	gtk_table_attach(GTK_TABLE(table), entry_path, 1, 2, 1, 2,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 2);

	fi_cover_align = gtk_alignment_new(0.5f, 0.5f, 0.0f, 0.0f);
	gtk_box_pack_start(GTK_BOX(hbox_t), fi_cover_align, FALSE, FALSE, 0);
        fi_cover_image_area = gtk_image_new();
        fi_event_box = gtk_event_box_new ();
	gtk_container_add(GTK_CONTAINER(fi_cover_align), fi_event_box);
        gtk_container_add (GTK_CONTAINER (fi_event_box), fi_cover_image_area);
        g_signal_connect(G_OBJECT(fi_event_box), "button_press_event",
                         G_CALLBACK(fi_cover_press_button_cb), (gpointer)fi);

        display_cover(fi_cover_image_area, fi_event_box, fi_cover_align,
		      48, 48, file, FALSE, TRUE);

	fi->hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), fi->hbox, FALSE, FALSE, 5);

	fi->nb = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), fi->nb, TRUE, TRUE, 5);

	/* Audio data notebook page */

	vbox_file = gtk_vbox_new(FALSE, 4);
	table_file = gtk_table_new(6, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_file), table_file, TRUE, TRUE, 10);
	label_file = gtk_label_new(_("Audio data"));
	fi->pageidx[0].tag = -1;
	gtk_notebook_append_page(GTK_NOTEBOOK(fi->nb), vbox_file, label_file);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Format:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
	gtk_widget_set_size_request(entry, 350, -1);
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	gtk_entry_set_text(GTK_ENTRY(entry), fileinfo.format_str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 0, 1,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Length:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 3);
	entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);
	if (fileinfo.total_samples == 0) {
		strcpy(str, "N/A");
	} else {
		sample2time(fileinfo.sample_rate, fileinfo.total_samples, str, 0);
	}
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
	sprintf(str, _("%ld Hz"), fileinfo.sample_rate);
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
	if (fileinfo.is_mono)
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
	if (fileinfo.bps == 0) {
		strcpy(str, "N/A kbit/s");
	} else {
		format_bps_label(fileinfo.bps, fileinfo.format_flags, str);
	}
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
	if (fileinfo.total_samples == 0) {
		strcpy(str, "N/A");
	} else {
		sprintf(str, "%lld", fileinfo.total_samples);
	}
	gtk_entry_set_text(GTK_ENTRY(entry), str);
	gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
	gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 5, 6,
			 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);

#ifdef HAVE_WAVPACK
	if (!is_cdda && fi->fdec->file_lib == WAVPACK_LIB) {
		wavpack_pdata_t * pd = (wavpack_pdata_t *)dec->pdata;
		int mode = WavpackGetMode(pd->wpc);

		hbox = gtk_hbox_new(FALSE, 0);
		label = gtk_label_new(_("Mode:"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_table_attach(GTK_TABLE(table_file), hbox, 0, 1, 7, 8, GTK_FILL, GTK_FILL, 5, 3);
		entry = gtk_entry_new();
		GTK_WIDGET_UNSET_FLAGS(entry, GTK_CAN_FOCUS);

		if ((mode & MODE_LOSSLESS) && (mode & MODE_WVC)) {
			strncpy(str, "Hybrid Lossless", MAXLEN-1);
		} else if (mode & MODE_LOSSLESS) {
			strncpy(str, "Lossless", MAXLEN-1);
		} else {
			strncpy(str, "Hybrid Lossy", MAXLEN-1);
		}
		cut_trailing_whitespace(str);
		gtk_entry_set_text(GTK_ENTRY(entry), str);
		gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
		gtk_table_attach(GTK_TABLE(table_file), entry, 1, 2, 7, 8,
				(GtkAttachOptions)(GTK_EXPAND | GTK_FILL), GTK_FILL, 5, 3);
	}
#endif /* HAVE_WAVPACK */


	/* end of notebook stuff */

	gtk_widget_grab_focus(fi->nb);

	fi->button_table = gtk_table_new(1, 2, TRUE);
	gtk_box_pack_end(GTK_BOX(fi->hbox), fi->button_table, FALSE, TRUE, 3);

        dismiss_btn = gtk_button_new_from_stock (GTK_STOCK_CLOSE); 
	g_signal_connect(dismiss_btn, "clicked", G_CALLBACK(dismiss), (gpointer)fi);
	gtk_table_attach(GTK_TABLE(fi->button_table), dismiss_btn, 1, 2, 0, 1,
			 GTK_FILL, GTK_FILL, 5, 3);

	gtk_widget_show_all(fi->info_window);

	fi_set_page(fi);

	/* fi_t object is freed in info_window destroy handlers */
}


#ifdef HAVE_MOD_INFO
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

                        sprintf(number, "%2d", i);
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
radio_buttons_cb(GtkToggleButton *toggle_button, gboolean state, gpointer data) {

	fi_t * fi = (fi_t *)data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button))) {
                show_list(fi, 0);
        } else {
                show_list(fi, 1);
        }
        set_first_row(fi);
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
				 G_CALLBACK(radio_buttons_cb), (gpointer)fi);

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
#endif /* HAVE_MOD_INFO */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

