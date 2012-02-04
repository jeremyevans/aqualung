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

#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <fnmatch.h>
#include <regex.h>
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys/stat.h>

#include "common.h"
#include "utils.h"
#include "utils_gui.h"
#include "utils_xml.h"
#include "decoder/file_decoder.h"
#include "i18n.h"
#include "options.h"
#include "music_browser.h"
#include "store_file.h"
#include "metadata_api.h"
#include "build_store.h"
#ifdef HAVE_CDDB
#include "cddb_lookup.h"
#endif /* HAVE_CDDB */


extern options_t options;
extern GtkTreeStore * music_store;

extern GtkWidget * browser_window;

extern GdkPixbuf * icon_artist;
extern GdkPixbuf * icon_record;
extern GdkPixbuf * icon_track;

#ifdef HAVE_SNDFILE
extern char * valid_extensions_sndfile[];
#endif /* HAVE_SNDFILE */

#ifdef HAVE_MPEG
extern char * valid_extensions_mpeg[];
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
extern char * valid_extensions_mod[];
#endif /* HAVE_MOD */


int build_busy;


enum {
	BUILD_TYPE_STRICT,
	BUILD_TYPE_LOOSE,
};

enum {
	SORT_NAME = 0,
	SORT_NAME_LOW,
	SORT_DIR,
	SORT_DIR_LOW,
	SORT_YEAR
};

enum {
	CAP_ALL_WORDS = 0,
	CAP_FIRST_WORD
};

enum {
	RECORD_NEW = 0,
	RECORD_EXISTS
};

enum {
	DATA_SRC_CDDB = 0,
	DATA_SRC_META,
	DATA_SRC_FILE
};


typedef struct _build_track_t {

	char filename[MAXLEN];
	char d_name[MAXLEN];

	char name[3][MAXLEN];
	char final[MAXLEN];

	char comment[MAXLEN];
	float duration;
	float rva;
	int rva_found;

	struct _build_track_t * next;

} build_track_t;

typedef struct {

	char d_name[MAXLEN];
	char dirname[MAXLEN];
	char sort[MAXLEN];

	char name[3][MAXLEN];
	char final[MAXLEN];

	char year[MAXLEN];
	char comment[MAXLEN];

	int unknown;

} build_record_t;

typedef struct {

	char d_name[MAXLEN];
	char sort[MAXLEN];

	char name[3][MAXLEN];
	char final[MAXLEN];

	int unknown;

} build_artist_t;

typedef struct {

	build_artist_t artist;
	build_record_t record;
	build_track_t * tracks;

	int flag;
	GtkTreeIter iter;

} build_disc_t;


typedef struct {

	int enabled[3];
	int type[3];
	int cddb_mask;

} data_src_t;

typedef struct {

	data_src_t * model;

	GtkListStore * list;

} data_src_gui_t;


typedef struct {

	int enabled;
	int pre_enabled;
	int low_enabled;
	int mode;
	char pattern[MAXLEN];
	char ** pre_stringv;

} capitalize_t;

typedef struct {

	capitalize_t * model;

	GtkWidget * check;
	GtkWidget * combo;
	GtkWidget * check_pre;
	GtkWidget * entry_pre;
	GtkWidget * check_low;

} capitalize_gui_t;


typedef struct {

	char regexp[MAXLEN];
	char replacement[MAXLEN];
	regex_t compiled;

	int rm_number;
	int rm_extension;
	int us_to_space;
	int rm_multi_spaces;

} file_transform_t;

typedef struct {

	file_transform_t * model;

	GtkWidget * check_rm_number;
	GtkWidget * check_rm_extension;
	GtkWidget * check_us_to_space;
	GtkWidget * check_rm_multi_spaces;

	GtkWidget * entry_regexp;
	GtkWidget * entry_replacement;

	GtkWidget * label_error;

} file_transform_gui_t;

typedef struct {

	AQUALUNG_THREAD_DECLARE(thread_id);
	AQUALUNG_MUTEX_DECLARE(mutex);

	int type;

	int cancelled;
	int write_data_locked;

	data_src_t * data_src_artist;
	data_src_t * data_src_record;
	data_src_t * data_src_track;

	capitalize_t * capitalize_artist;
	capitalize_t * capitalize_record;
	capitalize_t * capitalize_track;

	file_transform_t * file_transform_artist;
	file_transform_t * file_transform_record;
	file_transform_t * file_transform_track;
	file_transform_t * file_transform_sandbox;

	file_transform_gui_t * snd_file_trans_gui;

	char * file;

	GtkTreeIter store_iter;
	GtkTreeIter artist_iter;

	int artist_iter_is_set;
	map_t * artist_name_map;

	char root[MAXLEN];
	int artist_dir_depth;
	int reset_existing_data;
	int remove_dead_files;

	int artist_sort_by;
	int record_sort_by;

	int rec_add_year_to_comment;
	int trk_rva_enabled;
	int trk_comment_enabled;

	int excl_enabled;
	char excl_pattern[MAXLEN];
	char ** excl_patternv;
	int incl_enabled;
	char incl_pattern[MAXLEN];
	char ** incl_patternv;

	int cddb_enabled;
	int meta_enabled;

	GtkWidget * prog_window;
	GtkWidget * prog_cancel_button;
	GtkWidget * prog_file_entry;
	GtkWidget * prog_action_label;
	GtkWidget * snd_entry_input;
	GtkWidget * snd_entry_output;


	build_disc_t * disc;
	char action[MAXLEN];
	char path[MAXLEN];

} build_store_t;



void file_transform(char * buf, file_transform_t * model);


data_src_t *
data_src_new() {

	data_src_t * model;

	if ((model = (data_src_t *)malloc(sizeof(data_src_t))) == NULL) {
		fprintf(stderr, "build_store.c: data_src_new(): malloc error\n");
		return NULL;
	}

	model->type[DATA_SRC_CDDB] = DATA_SRC_CDDB;
	model->type[DATA_SRC_META] = DATA_SRC_META;
	model->type[DATA_SRC_FILE] = DATA_SRC_FILE;

#ifdef HAVE_CDDB
	model->enabled[0] = TRUE;
	model->cddb_mask = 1;
#else
	model->enabled[0] = FALSE;
	model->cddb_mask = 0;
#endif /* HAVE_CDDB */

	model->enabled[1] = TRUE;
	model->enabled[2] = TRUE;

	return model;
}

void
data_src_save(xmlNodePtr root, char * nodeID, data_src_t * model) {

	int i;
	xmlNodePtr node = xmlNewTextChild(root, NULL, (const xmlChar *) nodeID, NULL);

	for (i = 0; i < 3; i++) {
		xml_save_int_array(node, "enabled", model->enabled, i);
		xml_save_int_array(node, "type", model->type, i);
	}
}

void
data_src_load(xmlDocPtr doc, xmlNodePtr node, char * nodeID, data_src_t ** model) {

	if (!xmlStrcmp(node->name, (const xmlChar *)nodeID)) {

		int i;
		xmlNodePtr cur;

		*model = data_src_new();

		for (cur = node->xmlChildrenNode; cur != NULL; cur = cur->next) {

			for (i = 0; i < 3; i++) {
				xml_load_int_array(doc, cur, "enabled", (*model)->enabled, i);
				xml_load_int_array(doc, cur, "type", (*model)->type, i);
			}
		}

#ifdef HAVE_CDDB
		(*model)->cddb_mask = 1;
#else
		(*model)->cddb_mask = 0;
#endif /* HAVE_CDDB */
	}
}

void
data_src_cell_toggled(GtkCellRendererToggle * cell, gchar * path, gpointer data) {

	GtkTreeIter iter;
	data_src_gui_t * gui = (data_src_gui_t *)data;

	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(gui->list), &iter, path)) {
		gboolean bool;
		int type;

		gtk_tree_model_get(GTK_TREE_MODEL(gui->list), &iter, 0, &bool, 1, &type, -1);
		gtk_list_store_set(GTK_LIST_STORE(gui->list), &iter,
				   0, !bool && (type != DATA_SRC_CDDB || gui->model->cddb_mask), -1);
	}
}

data_src_gui_t *
data_src_gui_new(data_src_t * model, GtkWidget * target_vbox) {

	data_src_gui_t * gui;

	int i;
	GtkTreeIter iter;
	GtkWidget * viewport;
	GtkWidget * tree_view;
	GtkTreeViewColumn * column;
	GtkCellRenderer * cell;

	if ((gui = (data_src_gui_t *)malloc(sizeof(data_src_gui_t))) == NULL) {
		fprintf(stderr, "build_store.c: data_src_gui_new(): malloc error\n");
		return NULL;
	}

	gui->model = model;

	gui->list = gtk_list_store_new(3,
				       G_TYPE_BOOLEAN,
				       G_TYPE_INT,
				       G_TYPE_STRING);

	tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(gui->list));
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(tree_view), TRUE);

	cell = gtk_cell_renderer_toggle_new();
	g_signal_connect(cell, "toggled", (GCallback)data_src_cell_toggled, gui);
	column = gtk_tree_view_column_new_with_attributes(_("Enabled"), cell, "active", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), GTK_TREE_VIEW_COLUMN(column));

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Source"), cell, "text", 2, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), GTK_TREE_VIEW_COLUMN(column));

	viewport = gtk_viewport_new(NULL, NULL);

	for (i = 0; i < 3; i++) {

		gtk_list_store_append(gui->list, &iter);
		gtk_list_store_set(gui->list, &iter, 0, gui->model->enabled[i], 1, gui->model->type[i], -1);

		switch (gui->model->type[i]) {
		case DATA_SRC_CDDB:
			if (model->cddb_mask) {
				gtk_list_store_set(gui->list, &iter, 2, _("CDDB"), -1);
			} else {
				gtk_list_store_set(gui->list, &iter, 2, _("CDDB (not available)"), -1);
			}
			break;
		case DATA_SRC_META:
			gtk_list_store_set(gui->list, &iter, 2, _("Metadata"), -1);
			break;
		case DATA_SRC_FILE:
			gtk_list_store_set(gui->list, &iter, 2, _("Filesystem"), -1);
			break;
		}
	}

        gtk_container_add(GTK_CONTAINER(viewport), tree_view);
        gtk_box_pack_start(GTK_BOX(target_vbox), viewport, FALSE, FALSE, 5);

	return gui;
}

void
data_src_gui_sync(data_src_gui_t * gui) {

	GtkTreeIter iter;
	int i;

	for (i = 0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(gui->list), &iter, NULL, i); i++) {

		gtk_tree_model_get(GTK_TREE_MODEL(gui->list), &iter,
				   0, gui->model->enabled + i,
				   1, gui->model->type + i,
				   -1);
	}
}


capitalize_t *
capitalize_new() {

	capitalize_t * model;

	if ((model = (capitalize_t *)calloc(1, sizeof(capitalize_t))) == NULL) {
		fprintf(stderr, "build_store.c: capitalize_new(): calloc error\n");
		return NULL;
	}

	model->enabled = TRUE;
	model->pre_enabled = TRUE;
	model->low_enabled = TRUE;
	model->mode = CAP_ALL_WORDS;

	strncpy(model->pattern, "CD,a),b),c),d),I,II,III,IV,V,VI,VII,VIII,IX,X", MAXLEN-1);

	return model;
}

void
capitalize_save(xmlNodePtr root, char * nodeID, capitalize_t * model) {

	xmlNodePtr node = xmlNewTextChild(root, NULL, (const xmlChar *) nodeID, NULL);

	xml_save_int(node, "enabled", model->enabled);
	xml_save_int(node, "pre_enabled", model->pre_enabled);
	xml_save_int(node, "low_enabled", model->low_enabled);
	xml_save_int(node, "mode", model->mode);
	xml_save_str(node, "pattern", model->pattern);
}

void
capitalize_load(xmlDocPtr doc, xmlNodePtr node, char * nodeID, capitalize_t ** model) {

	if (!xmlStrcmp(node->name, (const xmlChar *)nodeID)) {

		xmlNodePtr cur;

		*model = capitalize_new();

		for (cur = node->xmlChildrenNode; cur != NULL; cur = cur->next) {
			xml_load_int(doc, cur, "enabled", &(*model)->enabled);
			xml_load_int(doc, cur, "pre_enabled", &(*model)->pre_enabled);
			xml_load_int(doc, cur, "low_enabled", &(*model)->low_enabled);
			xml_load_int(doc, cur, "mode", &(*model)->mode);
			xml_load_str(doc, cur, "pattern", (*model)->pattern);
		}
	}
}

void
capitalize_check_pre_toggled(GtkWidget * widget, gpointer data) {

	capitalize_gui_t * gui = (capitalize_gui_t *)data;

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui->check_pre));
	gtk_widget_set_sensitive(gui->entry_pre, state);
}

void
capitalize_check_toggled(GtkWidget * widget, gpointer data) {

	capitalize_gui_t * gui = (capitalize_gui_t *)data;

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui->check));
	gtk_widget_set_sensitive(gui->combo, state);
	gtk_widget_set_sensitive(gui->check_pre, state);
	gtk_widget_set_sensitive(gui->check_low, state);

	if (state == FALSE) {
		gtk_widget_set_sensitive(gui->entry_pre, FALSE);
	} else {
		capitalize_check_pre_toggled(widget, data);
	}
}

capitalize_gui_t *
capitalize_gui_new(capitalize_t * model, GtkWidget * target_vbox) {

	capitalize_gui_t * gui;

	GtkWidget * frame;
	GtkWidget * vbox;
	GtkWidget * table;

	if ((gui = (capitalize_gui_t *)malloc(sizeof(capitalize_gui_t))) == NULL) {
		fprintf(stderr, "build_store.c: capitalize_gui_new(): malloc error\n");
		return NULL;
	}

	gui->model = model;

	frame = gtk_frame_new(_("Capitalization"));
        gtk_box_pack_start(GTK_BOX(target_vbox), frame, FALSE, FALSE, 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

	gui->check = gtk_check_button_new_with_label(_("Capitalize: "));
	gtk_widget_set_name(gui->check, "check_on_notebook");
	gtk_table_attach(GTK_TABLE(table), gui->check, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 0, 0);

	g_signal_connect(G_OBJECT(gui->check), "toggled",
			 G_CALLBACK(capitalize_check_toggled), gui);

	gui->combo = gtk_combo_box_new_text();
	gtk_table_attach(GTK_TABLE(table), gui->combo, 1, 2, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 0);

	gtk_combo_box_append_text(GTK_COMBO_BOX(gui->combo), _("All words"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(gui->combo), _("First word only"));

	gui->check_pre = gtk_check_button_new_with_label(_("Force case: "));
	gtk_widget_set_name(gui->check_pre, "check_on_notebook");
	gtk_table_attach(GTK_TABLE(table), gui->check_pre, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 0, 5);

	g_signal_connect(G_OBJECT(gui->check_pre), "toggled",
			 G_CALLBACK(capitalize_check_pre_toggled), gui);

        gui->entry_pre = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(gui->entry_pre), MAXLEN-1);
	gtk_entry_set_text(GTK_ENTRY(gui->entry_pre), model->pattern);
	gtk_table_attach(GTK_TABLE(table), gui->entry_pre, 1, 2, 1, 2,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 5);

	gui->check_low = gtk_check_button_new_with_label(_("Force other letters to lowercase"));
	gtk_widget_set_name(gui->check_low, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(vbox), gui->check_low, TRUE, TRUE, 0);


	switch (model->mode) {
	case CAP_ALL_WORDS:
		gtk_combo_box_set_active(GTK_COMBO_BOX(gui->combo), 0);
		break;
	case CAP_FIRST_WORD:
		gtk_combo_box_set_active(GTK_COMBO_BOX(gui->combo), 1);
		break;
	}

	if (model->pre_enabled) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->check_pre), TRUE);
	} else {
		gtk_widget_set_sensitive(gui->combo, FALSE);
	}

	if (model->low_enabled) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->check_low), TRUE);
	}

	if (model->enabled) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->check), TRUE);
	} else {
		gtk_widget_set_sensitive(gui->combo, FALSE);
		gtk_widget_set_sensitive(gui->check_pre, FALSE);
		gtk_widget_set_sensitive(gui->entry_pre, FALSE);
		gtk_widget_set_sensitive(gui->check_low, FALSE);
	}

	return gui;
}

void
capitalize_gui_sync(capitalize_gui_t * gui) {


	gui->model->enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui->check));
	gui->model->pre_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui->check_pre));
	gui->model->low_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui->check_low));

	gui->model->mode = gtk_combo_box_get_active(GTK_COMBO_BOX(gui->combo));
	strncpy(gui->model->pattern, gtk_entry_get_text(GTK_ENTRY(gui->entry_pre)), MAXLEN-1);

	gui->model->pre_stringv =
		g_strsplit(gtk_entry_get_text(GTK_ENTRY(gui->entry_pre)), ",", 0);
}


file_transform_t *
file_transform_new() {

	file_transform_t * model;

	if ((model = (file_transform_t *)malloc(sizeof(file_transform_t))) == NULL) {
		fprintf(stderr, "build_store.c: file_transform_new(): malloc error\n");
		return NULL;
	}

	model->regexp[0] = '\0';
	model->replacement[0] = '\0';

	model->rm_number = 0;
	model->rm_extension = 0;
	model->us_to_space = 1;
	model->rm_multi_spaces = 1;

	return model;
}

void
file_transform_save(xmlNodePtr root, char * nodeID, file_transform_t * model) {

	xmlNodePtr node = xmlNewTextChild(root, NULL, (const xmlChar *) nodeID, NULL);

	xml_save_str(node, "regexp", model->regexp);
	xml_save_str(node, "replacement", model->replacement);

	xml_save_int(node, "rm_number", model->rm_number);
	xml_save_int(node, "rm_extension", model->rm_extension);
	xml_save_int(node, "us_to_space", model->us_to_space);
	xml_save_int(node, "rm_multi_spaces", model->rm_multi_spaces);
}

void
file_transform_load(xmlDocPtr doc, xmlNodePtr node, char * nodeID, file_transform_t ** model) {

	if (!xmlStrcmp(node->name, (const xmlChar *)nodeID)) {

		xmlNodePtr cur;

		*model = file_transform_new();

		for (cur = node->xmlChildrenNode; cur != NULL; cur = cur->next) {

			xml_load_str(doc, cur, "regexp", (*model)->regexp);
			xml_load_str(doc, cur, "replacement", (*model)->replacement);

			xml_load_int(doc, cur, "rm_number", &(*model)->rm_number);
			xml_load_int(doc, cur, "rm_extension", &(*model)->rm_extension);
			xml_load_int(doc, cur, "us_to_space", &(*model)->us_to_space);
			xml_load_int(doc, cur, "rm_multi_spaces", &(*model)->rm_multi_spaces);
		}
	}
}

file_transform_gui_t *
file_transform_gui_new(file_transform_t * model, GtkWidget * target_vbox) {

	file_transform_gui_t * gui;

	GtkWidget * frame;
	GtkWidget * label;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * table;
	GdkColor red = { 0, 30000, 0, 0 };


	if ((gui = (file_transform_gui_t *)malloc(sizeof(file_transform_gui_t))) == NULL) {
		fprintf(stderr, "build_store.c: file_transform_gui_new(): malloc error\n");
		return NULL;
	}

	gui->model = model;


	frame = gtk_frame_new(_("Regular expression"));
        gtk_box_pack_start(GTK_BOX(target_vbox), frame, FALSE, FALSE, 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Regexp:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 0, 5);

	gui->entry_regexp = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), gui->entry_regexp, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Replace:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 0, 5);

	gui->entry_replacement = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), gui->entry_replacement, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

	gtk_entry_set_text(GTK_ENTRY(gui->entry_regexp), model->regexp);
	gtk_entry_set_text(GTK_ENTRY(gui->entry_replacement), model->replacement);

        hbox = gtk_hbox_new(FALSE, 0);
	gui->label_error = gtk_label_new("");
	gtk_widget_modify_fg(gui->label_error, GTK_STATE_NORMAL, &red);
        gtk_box_pack_start(GTK_BOX(hbox), gui->label_error, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);


	frame = gtk_frame_new(_("Predefined transformations"));
        gtk_box_pack_start(GTK_BOX(target_vbox), frame, FALSE, FALSE, 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	gui->check_rm_extension = gtk_check_button_new_with_label(_("Remove file extension"));
	gtk_widget_set_name(gui->check_rm_extension, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(vbox), gui->check_rm_extension, FALSE, FALSE, 0);

	gui->check_rm_number = gtk_check_button_new_with_label(_("Remove leading number"));
	gtk_widget_set_name(gui->check_rm_number, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(vbox), gui->check_rm_number, FALSE, FALSE, 0);

	gui->check_us_to_space = gtk_check_button_new_with_label(_("Convert underscore to space"));
	gtk_widget_set_name(gui->check_us_to_space, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(vbox), gui->check_us_to_space, FALSE, FALSE, 0);

	gui->check_rm_multi_spaces = gtk_check_button_new_with_label(_("Trim leading, tailing and duplicate spaces"));
	gtk_widget_set_name(gui->check_rm_multi_spaces, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(vbox), gui->check_rm_multi_spaces, FALSE, FALSE, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->check_rm_number), model->rm_number);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->check_rm_extension), model->rm_extension);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->check_us_to_space), model->us_to_space);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->check_rm_multi_spaces), model->rm_multi_spaces);

	return gui;
}

int
file_transform_gui_sync(file_transform_gui_t * gui) {

	int err;

	gui->model->rm_number = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui->check_rm_number));
	gui->model->rm_extension = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui->check_rm_extension));
	gui->model->us_to_space = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui->check_us_to_space));
	gui->model->rm_multi_spaces = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui->check_rm_multi_spaces));

	strncpy(gui->model->regexp, gtk_entry_get_text(GTK_ENTRY(gui->entry_regexp)), MAXLEN-1);
	strncpy(gui->model->replacement, gtk_entry_get_text(GTK_ENTRY(gui->entry_replacement)), MAXLEN-1);

	gtk_widget_hide(gui->label_error);

	if ((err = regcomp(&gui->model->compiled, gui->model->regexp, REG_EXTENDED | REG_ICASE))) {

		char msg[MAXLEN];
		char * utf8;

		regerror(err, &gui->model->compiled, msg, MAXLEN);
		utf8 = g_filename_to_utf8(msg, -1, NULL, NULL, NULL);
		gtk_label_set_text(GTK_LABEL(gui->label_error), utf8);
		gtk_widget_show(gui->label_error);
		g_free(utf8);

		regfree(&gui->model->compiled);
		gtk_widget_grab_focus(gui->entry_regexp);

		return err;
	}

	if (gui->model->regexp[0] != '\0' && !regexec(&gui->model->compiled, "", 0, NULL, 0)) {

		gtk_label_set_text(GTK_LABEL(gui->label_error), _("Regexp matches empty string"));
		gtk_widget_show(gui->label_error);
		regfree(&gui->model->compiled);
		gtk_widget_grab_focus(gui->entry_regexp);

		return -1;
	}

	return 0;
}


build_store_t *
build_store_new(GtkTreeIter * store_iter, char * file) {

	build_store_t * data;
	char * pfilter = NULL;
	int i;


	if ((data = (build_store_t *)calloc(1, sizeof(build_store_t))) == NULL) {
		fprintf(stderr, "build_store_new: calloc error\n");
		return NULL;
	}

#ifdef _WIN32
        data->mutex = g_mutex_new();
#endif /* _WIN32 */

        data->store_iter = *store_iter;
	data->file = strdup(file);

	data->type = BUILD_TYPE_STRICT;
	data->artist_dir_depth = 1;
	data->artist_sort_by = SORT_NAME_LOW;
	data->record_sort_by = SORT_YEAR;
	data->excl_enabled = 1;


#ifdef HAVE_SNDFILE
	for (i = 0; valid_extensions_sndfile[i] != NULL; i++) {
		strcat(data->incl_pattern, "*.");
		strcat(data->incl_pattern, valid_extensions_sndfile[i]);
		strcat(data->incl_pattern, ",");
	}
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
	strcat(data->incl_pattern, "*.flac,");
#endif /* HAVE_FLAC */

#ifdef HAVE_VORBIS
	strcat(data->incl_pattern, "*.ogg,");
#endif /* HAVE_VORBIS */

#ifdef HAVE_MPEG
	for (i = 0; valid_extensions_mpeg[i] != NULL; i++) {
		strcat(data->incl_pattern, "*.");
		strcat(data->incl_pattern, valid_extensions_mpeg[i]);
		strcat(data->incl_pattern, ",");
	}
#endif /* HAVE_MPEG */

#ifdef HAVE_SPEEX
	strcat(data->incl_pattern, "*.spx,");
#endif /* HAVE_SPEEX */

#ifdef HAVE_MPC
	strcat(data->incl_pattern, "*.mpc,");
#endif /* HAVE_MPC */

#ifdef HAVE_MAC
	strcat(data->incl_pattern, "*.ape,");
#endif /* HAVE_MAC */

#ifdef HAVE_MOD
	for (i = 0; valid_extensions_mod[i] != NULL; i++) {
		strcat(data->incl_pattern, "*.");
		strcat(data->incl_pattern, valid_extensions_mod[i]);
		strcat(data->incl_pattern, ",");
	}
#endif /* HAVE_MOD */

#ifdef HAVE_WAVPACK
	strcat(data->incl_pattern, "*.wv,");
#endif /* HAVE_WAVPACK */

	if ((pfilter = strrchr(data->incl_pattern, ',')) != NULL) {
		*pfilter = '\0';
	}

	strcpy(data->excl_pattern, "*.jpg,*.jpeg,*.png,*.gif,*.pls,*.m3u,*.cue,*.xml,*.html,*.htm,*.txt,*.ini,*.nfo");

	return data;
}


void
build_store_save(build_store_t * data) {

	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr node;

	doc = xmlParseFile(data->file);
        root = xmlDocGetRootElement(doc);

	for (node = root->xmlChildrenNode; node != NULL; node = node->next) {

		if (!xmlStrcmp(node->name, (const xmlChar *)"builder")) {
			xmlUnlinkNode(node);
			xmlFreeNode(node);
			break;
		}
	}

	node = xmlNewTextChild(root, NULL, (const xmlChar *)"builder", NULL);

	xml_save_int(node, "type", data->type);
	xml_save_str(node, "root", data->root);
	xml_save_int(node, "artist_dir_depth", data->artist_dir_depth);
	xml_save_int(node, "excl_enabled", data->excl_enabled);
	xml_save_str(node, "excl_pattern", data->excl_pattern);
	xml_save_int(node, "incl_enabled", data->incl_enabled);
	xml_save_str(node, "incl_pattern", data->incl_pattern);
	xml_save_int(node, "reset_existing_data", data->reset_existing_data);
	xml_save_int(node, "remove_dead_files", data->remove_dead_files);

	xml_save_int(node, "artist_sort_by", data->artist_sort_by);
	xml_save_int(node, "record_sort_by", data->record_sort_by);
	xml_save_int(node, "rec_add_year_to_comment", data->rec_add_year_to_comment);
	xml_save_int(node, "trk_rva_enabled", data->trk_rva_enabled);
	xml_save_int(node, "trk_comment_enabled", data->trk_comment_enabled);

	data_src_save(node, "data_src_artist", data->data_src_artist);
	data_src_save(node, "data_src_record", data->data_src_record);
	data_src_save(node, "data_src_track", data->data_src_track);

	capitalize_save(node, "capitalize_artist", data->capitalize_artist);
	capitalize_save(node, "capitalize_record", data->capitalize_record);
	capitalize_save(node, "capitalize_track", data->capitalize_track);

	file_transform_save(node, "file_transform_artist", data->file_transform_artist);
	file_transform_save(node, "file_transform_record", data->file_transform_record);
	file_transform_save(node, "file_transform_track", data->file_transform_track);

        xmlSaveFormatFile(data->file, doc, 1);
	xmlFreeDoc(doc);
}

int
build_store_load(build_store_t * data, int test_only) {

	int found = 0;
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr node;

	doc = xmlParseFile(data->file);
        root = xmlDocGetRootElement(doc);

	for (node = root->xmlChildrenNode; node != NULL; node = node->next) {

		if (!xmlStrcmp(node->name, (const xmlChar *)"builder")) {
			found = 1;
			break;
		}
	}

	if (test_only) {
		xmlFreeDoc(doc);
		return found;
	}

	if (found) {
		xmlNodePtr cur;

		for (cur = node->xmlChildrenNode; cur != NULL; cur = cur->next) {

			xml_load_int(doc, cur, "type", &data->type);
			xml_load_str(doc, cur, "root", data->root);
			xml_load_int(doc, cur, "artist_dir_depth", &data->artist_dir_depth);
			xml_load_int(doc, cur, "excl_enabled", &data->excl_enabled);
			xml_load_str(doc, cur, "excl_pattern", data->excl_pattern);
			xml_load_int(doc, cur, "incl_enabled", &data->incl_enabled);
			xml_load_str(doc, cur, "incl_pattern", data->incl_pattern);
			xml_load_int(doc, cur, "reset_existing_data", &data->reset_existing_data);
			xml_load_int(doc, cur, "remove_dead_files", &data->remove_dead_files);

			xml_load_int(doc, cur, "artist_sort_by", &data->artist_sort_by);
			xml_load_int(doc, cur, "record_sort_by", &data->record_sort_by);
			xml_load_int(doc, cur, "rec_add_year_to_comment", &data->rec_add_year_to_comment);
			xml_load_int(doc, cur, "trk_rva_enabled", &data->trk_rva_enabled);
			xml_load_int(doc, cur, "trk_comment_enabled", &data->trk_comment_enabled);

			data_src_load(doc, cur, "data_src_artist", &data->data_src_artist);
			data_src_load(doc, cur, "data_src_record", &data->data_src_record);
			data_src_load(doc, cur, "data_src_track", &data->data_src_track);

			capitalize_load(doc, cur, "capitalize_artist", &data->capitalize_artist);
			capitalize_load(doc, cur, "capitalize_record", &data->capitalize_record);
			capitalize_load(doc, cur, "capitalize_track", &data->capitalize_track);

			file_transform_load(doc, cur, "file_transform_artist", &data->file_transform_artist);
			file_transform_load(doc, cur, "file_transform_record", &data->file_transform_record);
			file_transform_load(doc, cur, "file_transform_track", &data->file_transform_track);
		}
	}

	xmlFreeDoc(doc);
	return found;
}

xmlNodePtr
build_store_get_xml_node(char * file) {

	xmlDocPtr doc;
	xmlNodePtr node;

	doc = xmlParseFile(file);

	if (doc == NULL) {
		return NULL;
	}

        node = xmlDocGetRootElement(doc);

	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {

		if (!xmlStrcmp(node->name, (const xmlChar *)"builder")) {
			xmlNodePtr res = xmlCopyNode(node, 1);
			xmlFreeDoc(doc);
			return res;
		}
	}

	xmlFreeDoc(doc);
	return NULL;
}

void
build_store_free(build_store_t * data) {

#ifdef _WIN32
	g_mutex_free(data->mutex);
#endif /* _WIN32 */

	free(data->file);

	g_strfreev(data->capitalize_artist->pre_stringv);
	data->capitalize_artist->pre_stringv = NULL;

	g_strfreev(data->capitalize_record->pre_stringv);
	data->capitalize_record->pre_stringv = NULL;

	g_strfreev(data->capitalize_track->pre_stringv);
	data->capitalize_track->pre_stringv = NULL;

	g_strfreev(data->excl_patternv);
	data->excl_patternv = NULL;

	g_strfreev(data->incl_patternv);
	data->incl_patternv = NULL;

	regfree(&data->file_transform_artist->compiled);
	regfree(&data->file_transform_record->compiled);
	regfree(&data->file_transform_track->compiled);

        free(data);
}


char *
filter_string(const char * str) {

	int len;
	char * tmp;
	int i;
	int j;


	len = strlen(str);

	if (len == 0) {
		return NULL;
	}

	if ((tmp = (char *)malloc((len + 1) * sizeof(char))) == NULL) {
		fprintf(stderr, "build_store.c: filter_string(): malloc error\n");
		return NULL;
	}

	for (i = 0, j = 0; i < len; i++) {
		if (str[i] != ' ' &&
		    str[i] != '.' &&
		    str[i] != ',' &&
		    str[i] != '?' &&
		    str[i] != '!' &&
		    str[i] != '&' &&
		    str[i] != '\'' &&
		    str[i] != '"' &&
		    str[i] != '-' &&
		    str[i] != '_' &&
		    str[i] != '/' &&
		    str[i] != '(' &&
		    str[i] != ')' &&
		    str[i] != '[' &&
		    str[i] != ']' &&
		    str[i] != '{' &&
		    str[i] != '}') {
			tmp[j++] = str[i];
		}
	}

	tmp[j] = '\0';

	return tmp;
}

int
collate(const char * str1, const char * str2) {

	char * tmp1 = filter_string(str1);
	char * tmp2 = filter_string(str2);

	char * key1 = g_utf8_casefold(tmp1, -1);
	char * key2 = g_utf8_casefold(tmp2, -1);

	int i = g_utf8_collate(key1, key2);

	g_free(key1);
	g_free(key2);
	free(tmp1);
	free(tmp2);

	return i;
}


int
store_contains_disc(GtkTreeIter * store_iter,
		    GtkTreeIter * __artist_iter,
		    GtkTreeIter * __record_iter, build_disc_t * disc) {


	int i, j, k;
	int n_tracks;
	GtkTreeIter artist_iter;
	GtkTreeIter record_iter;
	GtkTreeIter track_iter;

	build_track_t * ptrack = disc->tracks;

	for (n_tracks = 0; ptrack; ++n_tracks, ptrack = ptrack->next) ;

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &artist_iter, store_iter, i++)) {
		j = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     &record_iter, &artist_iter, j++)) {
			int match = 1;

			if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store),
							   &record_iter) != n_tracks) {
				continue;
			}

			k = 0;
			ptrack = disc->tracks;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
							     &track_iter, &record_iter, k++)) {

				track_data_t * data;

				gtk_tree_model_get(GTK_TREE_MODEL(music_store), &track_iter,
						   MS_COL_DATA, &data, -1);

				if (strcmp(data->file, ptrack->filename)) {
					match = 0;
					break;
				}

				ptrack = ptrack->next;
			}

			if (match) {
				if (__artist_iter) {
					*__artist_iter = artist_iter;
				}
				if (__record_iter) {
					*__record_iter = record_iter;
				}
				return 1;
			}
		}
	}

	return 0;
}


int
store_contains_track(GtkTreeIter * store_iter,
		     GtkTreeIter * __track_iter, char * filename) {


	int i, j, k;
	GtkTreeIter artist_iter;
	GtkTreeIter record_iter;
	GtkTreeIter track_iter;


	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &artist_iter, store_iter, i++)) {
		j = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     &record_iter, &artist_iter, j++)) {
			k = 0;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
							     &track_iter, &record_iter, k++)) {

				track_data_t * data;

				gtk_tree_model_get(GTK_TREE_MODEL(music_store), &track_iter,
						   MS_COL_DATA, &data, -1);

				if (!strcmp(data->file, filename)) {

					if (__track_iter) {
						*__track_iter = track_iter;
					}

					return 1;
				}
			}
		}
	}

	return 0;
}


void
create_record(GtkTreeIter * artist_iter, GtkTreeIter * record_iter, build_disc_t * disc) {

	record_data_t * record_data;

	if ((record_data = (record_data_t *)calloc(1, sizeof(record_data_t))) == NULL) {
		fprintf(stderr, "create_record: calloc error\n");
		return;
	}

	gtk_tree_store_append(music_store, record_iter, artist_iter);
	gtk_tree_store_set(music_store, record_iter,
			   MS_COL_NAME, disc->record.final,
			   MS_COL_SORT, disc->record.sort,
			   MS_COL_DATA, record_data, -1);

	record_data->comment = strdup(disc->record.comment);
	record_data->year = atoi(disc->record.year);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, record_iter, MS_COL_ICON, icon_record, -1);
	}
}

void
create_artist(GtkTreeIter * store_iter, GtkTreeIter * artist_iter, build_disc_t * disc) {

	artist_data_t * artist_data;

	if ((artist_data = (artist_data_t *)calloc(1, sizeof(artist_data_t))) == NULL) {
		fprintf(stderr, "create_artist: calloc error\n");
		return;
	}

	gtk_tree_store_append(music_store, artist_iter, store_iter);
	gtk_tree_store_set(music_store, artist_iter,
			   MS_COL_NAME, disc->artist.final,
			   MS_COL_SORT, disc->artist.sort,
			   MS_COL_DATA, artist_data, -1);

        if (options.enable_ms_tree_icons) {
                gtk_tree_store_set(music_store, artist_iter, MS_COL_ICON, icon_artist, -1);
        }
}

int
store_get_iter_for_artist_and_record(GtkTreeIter * store_iter,
				     GtkTreeIter * artist_iter,
				     GtkTreeIter * record_iter,
				     build_disc_t * disc) {
	int i;
	int j;

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     artist_iter, store_iter, i++)) {

		char * artist_name;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), artist_iter,
				   MS_COL_NAME, &artist_name, -1);

		if (collate(disc->artist.final, artist_name)) {
			g_free(artist_name);
			continue;
		}

		j = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     record_iter, artist_iter, j++)) {
			char * record_name;

			gtk_tree_model_get(GTK_TREE_MODEL(music_store), record_iter,
					   MS_COL_NAME, &record_name, -1);

			if (!collate(disc->record.final, record_name)) {
				g_free(record_name);
				g_free(artist_name);
				return RECORD_EXISTS;
			}

			g_free(record_name);
		}

		/* create record */
		create_record(artist_iter, record_iter, disc);

		g_free(artist_name);
		return RECORD_NEW;
	}

	/* create both artist and record */
	create_artist(store_iter, artist_iter, disc);
	create_record(artist_iter, record_iter, disc);

	return RECORD_NEW;
}


int
store_get_iter_for_tracklist(GtkTreeIter * store_iter,
			     GtkTreeIter * artist_iter,
			     GtkTreeIter * record_iter,
			     build_disc_t * disc, map_t ** artist_name_map) {
	int i;

	/* check if record already exists */

	if (store_contains_disc(store_iter, artist_iter, record_iter, disc)) {

		if (disc->artist.unknown) {
			gtk_tree_store_set(music_store, artist_iter,
					   MS_COL_NAME, disc->artist.final,
					   MS_COL_SORT, disc->artist.sort,
					   -1);
		}

		return RECORD_EXISTS;
	}


	/* no such record -- check if the artist of the record exists */

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     artist_iter, store_iter, i++)) {

		char * artist_name;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), artist_iter,
				   MS_COL_NAME, &artist_name, -1);

		if (collate(disc->artist.final, artist_name)) {
			g_free(artist_name);
			continue;
		}

		/* artist found, create record */
		create_record(artist_iter, record_iter, disc);

		g_free(artist_name);
		return RECORD_NEW;
	}


	/* no such artist -- create both artist and record */
	create_artist(store_iter, artist_iter, disc);
	create_record(artist_iter, record_iter, disc);

	/* start contest for artist name */
	map_put(artist_name_map, disc->artist.final);

	return RECORD_NEW;
}


int
artist_get_iter_for_tracklist(GtkTreeIter * artist_iter,
			      GtkTreeIter * record_iter, build_disc_t * disc) {

	int i, j;
	int n_tracks;
	GtkTreeIter track_iter;

	build_track_t * ptrack = disc->tracks;


	/* check if record already exists */

	for (n_tracks = 0; ptrack; ++n_tracks, ptrack = ptrack->next) ;

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     record_iter, artist_iter, i++)) {
		int match = 1;

		if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store),
						   record_iter) != n_tracks) {
			continue;
		}

		j = 0;
		ptrack = disc->tracks;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     &track_iter, record_iter, j++)) {

			track_data_t * data;

			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &track_iter,
					   MS_COL_DATA, &data, -1);

			if (strcmp(data->file, ptrack->filename)) {
				match = 0;
				break;
			}

			ptrack = ptrack->next;
		}

		if (match) {
			return RECORD_EXISTS;
		}
	}


	/* no such record -- create it */
	create_record(artist_iter, record_iter, disc);

	return RECORD_NEW;
}


void
gen_check_filter_toggled(GtkWidget * widget, gpointer * data) {

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gtk_widget_set_sensitive(GTK_WIDGET(data), state);
}


void
browse_button_clicked(GtkButton * button, gpointer data) {

	file_chooser_with_entry(_("Please select the root directory."),
				browser_window,
				GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
				FILE_CHOOSER_FILTER_NONE,
				(GtkWidget *)data,
				options.currdir);
}


void
snd_button_test_clicked(GtkWidget * widget, gpointer user_data) {

	build_store_t * data = (build_store_t *)user_data;
	char buf[MAXLEN];
	int err;

	err = file_transform_gui_sync(data->snd_file_trans_gui);

	if (err) {
		gtk_entry_set_text(GTK_ENTRY(data->snd_entry_output), "");
		return;
	}

	buf[0] = '\0';
	strncpy(buf, (char *)gtk_entry_get_text(GTK_ENTRY(data->snd_entry_input)), MAXLEN-1);
	file_transform(buf, data->snd_file_trans_gui->model);

	regfree(&data->snd_file_trans_gui->model->compiled);

	gtk_entry_set_text(GTK_ENTRY(data->snd_entry_output), buf);
}



int
build_type_dialog(build_store_t * build_data) {

	GtkWidget * dialog;
	GtkWidget * notebook;
	GtkWidget * radio_load;
	GtkWidget * radio_strict;
	GtkWidget * radio_loose;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * label;


        dialog = gtk_dialog_new_with_buttons(_("Select build type"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 400, -1);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), notebook);

        vbox = gtk_vbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, NULL);


	radio_strict = gtk_radio_button_new_with_label(NULL, _("Directory driven"));
	gtk_widget_set_name(radio_strict, "check_on_notebook");
 	gtk_box_pack_start(GTK_BOX(vbox), radio_strict, FALSE, FALSE, 0);

 	label = gtk_label_new(_("Follows the directory structure to identify the artists and\n"
 				"records. The files are added on a record basis."));
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 30);
 	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);


 	radio_loose = gtk_radio_button_new_with_label_from_widget(
				  GTK_RADIO_BUTTON(radio_strict), _("Independent"));
 	gtk_widget_set_name(radio_loose, "check_on_notebook");
 	gtk_box_pack_start(GTK_BOX(vbox), radio_loose, FALSE, FALSE, 0);

 	label = gtk_label_new(_("Recursive search from the root directory for audio files.\n"
 				"The files are processed independently, so only metadata\n"
 				"and filename transformation are available."));
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 30);
 	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);


	radio_load = gtk_radio_button_new_with_label_from_widget(
				 GTK_RADIO_BUTTON(radio_strict), _("Load settings from Music Store file"));

	if (build_store_load(build_data, 1)) {
		gtk_widget_set_name(radio_load, "check_on_notebook");
		gtk_box_pack_start(GTK_BOX(vbox), radio_load, FALSE, FALSE, 0);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_load), TRUE);
	}


	gtk_widget_show_all(dialog);

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_load))) {
			build_store_load(build_data, 0);
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_strict))) {
			build_data->type = BUILD_TYPE_STRICT;
		} else {
			build_data->type = BUILD_TYPE_LOOSE;
		}

                gtk_widget_destroy(dialog);
                return build_data->type;
	}

        gtk_widget_destroy(dialog);
	return -1;
}


int
build_dialog(build_store_t * data) {


	GtkWidget * dialog;
	GtkWidget * notebook;
	GtkWidget * label;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * vbox;
	GtkWidget * frame;

	/* General */

	GtkWidget * gen_vbox;

	GtkWidget * gen_root_entry;
	GtkWidget * gen_browse_button;
	GtkWidget * gen_depth_combo = NULL;

	GtkWidget * gen_check_excl;
	GtkWidget * gen_check_incl;
	GtkWidget * gen_entry_excl;
	GtkWidget * gen_entry_incl;

	GtkWidget * gen_excl_frame;
	GtkWidget * gen_excl_frame_hbox;
	GtkWidget * gen_incl_frame;
	GtkWidget * gen_incl_frame_hbox;

	GtkWidget * gen_check_reset_data;
	GtkWidget * gen_check_remove_dead;

	/* Artist */

	GtkWidget * art_vbox;
	GtkWidget * art_sort_combo;
	data_src_gui_t * art_src_gui;
	capitalize_gui_t * art_cap_gui;
	file_transform_gui_t * art_file_trans_gui;

	/* Record */

	GtkWidget * rec_vbox;
	GtkWidget * rec_sort_combo;
	data_src_gui_t * rec_src_gui;
	capitalize_gui_t * rec_cap_gui;
	file_transform_gui_t * rec_file_trans_gui;
	GtkWidget * rec_check_add_year;

	/* Track */

	GtkWidget * trk_vbox;
	data_src_gui_t * trk_src_gui;
	capitalize_gui_t * trk_cap_gui;
	file_transform_gui_t * trk_file_trans_gui;
	GtkWidget * trk_check_rva;
	GtkWidget * trk_check_comment;

	/* Sandbox */

	GtkWidget * snd_vbox;
	GtkWidget * snd_button_test;

        int i, ret;


        dialog = gtk_dialog_new_with_buttons(_("Build/Update store"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 400, -1);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), notebook);


	/* General */

        gen_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(gen_vbox), 5);
	label = gtk_label_new(_("General"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gen_vbox, label);

	frame = gtk_frame_new(_("Directory structure"));

        gtk_box_pack_start(GTK_BOX(gen_vbox), frame, FALSE, FALSE, 5);

	table = gtk_table_new(2, 3, FALSE);
        gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_container_add(GTK_CONTAINER(frame), table);

	label = gtk_label_new(_("Root path:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 0, 5);

        gen_root_entry = gtk_entry_new ();
        gtk_entry_set_max_length(GTK_ENTRY(gen_root_entry), MAXLEN-1);
	gtk_entry_set_text(GTK_ENTRY(gen_root_entry), data->root);
	gtk_table_attach(GTK_TABLE(table), gen_root_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	gen_browse_button = gui_stock_label_button(_("_Browse..."), GTK_STOCK_OPEN);
	gtk_table_attach(GTK_TABLE(table), gen_browse_button, 2, 3, 0, 1,
			 GTK_FILL, GTK_FILL, 0, 5);
        g_signal_connect(G_OBJECT(gen_browse_button), "clicked", G_CALLBACK(browse_button_clicked),
			 gen_root_entry);


	if (data->type == BUILD_TYPE_STRICT) {
		label = gtk_label_new(_("Structure:"));
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
				 GTK_FILL, GTK_FILL, 0, 0);

		gen_depth_combo = gtk_combo_box_new_text();
		gtk_combo_box_append_text(GTK_COMBO_BOX(gen_depth_combo),
					  _("root / record / track"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(gen_depth_combo),
					  _("root / artist / record / track"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(gen_depth_combo),
					  _("root / artist / artist / record / track"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(gen_depth_combo),
					  _("root / artist / artist / artist / record / track"));
		gtk_combo_box_set_active(GTK_COMBO_BOX(gen_depth_combo), data->artist_dir_depth);
		gtk_table_attach(GTK_TABLE(table), gen_depth_combo, 1, 3, 1, 2,
				 GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	}


	gen_excl_frame = gtk_frame_new(_("Exclude files matching wildcard"));
        gtk_box_pack_start(GTK_BOX(gen_vbox), gen_excl_frame, FALSE, FALSE, 5);
        gen_excl_frame_hbox = gtk_hbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(gen_excl_frame_hbox), 5);
	gtk_container_add(GTK_CONTAINER(gen_excl_frame), gen_excl_frame_hbox);

	gen_check_excl = gtk_check_button_new();
	gtk_widget_set_name(gen_check_excl, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_excl_frame_hbox), gen_check_excl, FALSE, FALSE, 0);

        gen_entry_excl = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(gen_entry_excl), MAXLEN-1);
	gtk_entry_set_text(GTK_ENTRY(gen_entry_excl), data->excl_pattern);
        gtk_box_pack_end(GTK_BOX(gen_excl_frame_hbox), gen_entry_excl, TRUE, TRUE, 0);

	if (data->excl_enabled) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gen_check_excl), TRUE);
	} else {
		gtk_widget_set_sensitive(gen_entry_excl, FALSE);
	}

	g_signal_connect(G_OBJECT(gen_check_excl), "toggled",
			 G_CALLBACK(gen_check_filter_toggled), gen_entry_excl);


	gen_incl_frame = gtk_frame_new(_("Include only files matching wildcard"));
        gtk_box_pack_start(GTK_BOX(gen_vbox), gen_incl_frame, FALSE, FALSE, 5);
        gen_incl_frame_hbox = gtk_hbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(gen_incl_frame_hbox), 5);
	gtk_container_add(GTK_CONTAINER(gen_incl_frame), gen_incl_frame_hbox);

	gen_check_incl = gtk_check_button_new();
	gtk_widget_set_name(gen_check_incl, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_incl_frame_hbox), gen_check_incl, FALSE, FALSE, 0);

        gen_entry_incl = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(gen_entry_incl), MAXLEN-1);
	gtk_entry_set_text(GTK_ENTRY(gen_entry_incl), data->incl_pattern);
        gtk_box_pack_end(GTK_BOX(gen_incl_frame_hbox), gen_entry_incl, TRUE, TRUE, 0);

	if (data->incl_enabled) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gen_check_incl), TRUE);
	} else {
		gtk_widget_set_sensitive(gen_entry_incl, FALSE);
	}

	g_signal_connect(G_OBJECT(gen_check_incl), "toggled",
			 G_CALLBACK(gen_check_filter_toggled), gen_entry_incl);


	gen_check_reset_data =
		gtk_check_button_new_with_label(_("Reread data for existing tracks"));
	gtk_widget_set_name(gen_check_reset_data, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_vbox), gen_check_reset_data, FALSE, FALSE, 0);

	if (data->reset_existing_data) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gen_check_reset_data), TRUE);
	}

	gen_check_remove_dead =
		gtk_check_button_new_with_label(_("Remove non-existing files from store"));
	gtk_widget_set_name(gen_check_remove_dead, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(gen_vbox), gen_check_remove_dead, FALSE, FALSE, 0);

	if (data->remove_dead_files) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gen_check_remove_dead), TRUE);
	}


	/* Artist */

        art_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(art_vbox), 5);
	label = gtk_label_new(_("Artist"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), art_vbox, label);


	if (data->data_src_artist == NULL) {
		data->data_src_artist = data_src_new();
		if (data->type == BUILD_TYPE_LOOSE) {
			data->data_src_artist->enabled[DATA_SRC_FILE] = 0;
		}
	}

	if (data->type == BUILD_TYPE_LOOSE) {
		data->data_src_artist->cddb_mask = 0;
		for (i = 0; i < 3; i++) {
			if (data->data_src_artist->type[i] == DATA_SRC_CDDB) {
				data->data_src_artist->enabled[DATA_SRC_CDDB] = 0;
				break;
			}
		}
	}

	art_src_gui = data_src_gui_new(data->data_src_artist, art_vbox);

	if (data->file_transform_artist == NULL) {
		data->file_transform_artist = file_transform_new();
	}
	art_file_trans_gui = file_transform_gui_new(data->file_transform_artist, art_vbox);

	if (data->capitalize_artist == NULL) {
		data->capitalize_artist = capitalize_new();
	}
	art_cap_gui = capitalize_gui_new(data->capitalize_artist, art_vbox);


	frame = gtk_frame_new(_("Sort artists by"));
        gtk_box_pack_start(GTK_BOX(art_vbox), frame, FALSE, FALSE, 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	art_sort_combo = gtk_combo_box_new_text();
        gtk_box_pack_start(GTK_BOX(vbox), art_sort_combo, FALSE, FALSE, 0);

	gtk_combo_box_append_text(GTK_COMBO_BOX(art_sort_combo), _("Artist name"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(art_sort_combo), _("Artist name (lowercase)"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(art_sort_combo), _("Directory name"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(art_sort_combo), _("Directory name (lowercase)"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(art_sort_combo), data->artist_sort_by);


	/* Record */

        rec_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(rec_vbox), 5);
	label = gtk_label_new(_("Record"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), rec_vbox, label);


	if (data->data_src_record == NULL) {
		data->data_src_record = data_src_new();
		if (data->type == BUILD_TYPE_LOOSE) {
			data->data_src_record->enabled[DATA_SRC_FILE] = 0;
		}
	}

	if (data->type == BUILD_TYPE_LOOSE) {
		data->data_src_record->cddb_mask = 0;
		for (i = 0; i < 3; i++) {
			if (data->data_src_record->type[i] == DATA_SRC_CDDB) {
				data->data_src_record->enabled[DATA_SRC_CDDB] = 0;
				break;
			}
		}
	}

	rec_src_gui = data_src_gui_new(data->data_src_record, rec_vbox);

	if (data->file_transform_record == NULL) {
		data->file_transform_record = file_transform_new();
	}
	rec_file_trans_gui = file_transform_gui_new(data->file_transform_record, rec_vbox);

	if (data->capitalize_record == NULL) {
		data->capitalize_record = capitalize_new();
	}
	rec_cap_gui = capitalize_gui_new(data->capitalize_record, rec_vbox);


	frame = gtk_frame_new(_("Sort records by"));
        gtk_box_pack_start(GTK_BOX(rec_vbox), frame, FALSE, FALSE, 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	rec_sort_combo = gtk_combo_box_new_text();
        gtk_box_pack_start(GTK_BOX(vbox), rec_sort_combo, FALSE, FALSE, 0);

	gtk_combo_box_append_text(GTK_COMBO_BOX(rec_sort_combo), _("Record name"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(rec_sort_combo), _("Record name (lowercase)"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(rec_sort_combo), _("Directory name"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(rec_sort_combo), _("Directory name (lowercase)"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(rec_sort_combo), _("Year"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(rec_sort_combo), data->record_sort_by);


	rec_check_add_year =
		gtk_check_button_new_with_label(_("Add year to the comments of new records"));
	gtk_widget_set_name(rec_check_add_year, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(rec_vbox), rec_check_add_year, FALSE, FALSE, 0);

	if (data->rec_add_year_to_comment) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rec_check_add_year), TRUE);
	}


	/* Track */

        trk_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(trk_vbox), 5);
	label = gtk_label_new(_("Track"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), trk_vbox, label);


	if (data->data_src_track == NULL) {
		data->data_src_track = data_src_new();
	}

	if (data->type == BUILD_TYPE_LOOSE) {
		data->data_src_track->cddb_mask = 0;
		for (i = 0; i < 3; i++) {
			if (data->data_src_track->type[i] == DATA_SRC_CDDB) {
				data->data_src_track->enabled[DATA_SRC_CDDB] = 0;
				break;
			}
		}
	}

	trk_src_gui = data_src_gui_new(data->data_src_track, trk_vbox);

	if (data->file_transform_track == NULL) {
		data->file_transform_track = file_transform_new();
	}
	trk_file_trans_gui = file_transform_gui_new(data->file_transform_track, trk_vbox);

	if (data->capitalize_track == NULL) {
		data->capitalize_track = capitalize_new();
	}
	trk_cap_gui = capitalize_gui_new(data->capitalize_track, trk_vbox);


	trk_check_rva =
		gtk_check_button_new_with_label(_("Import Replaygain tag as manual RVA"));
	gtk_widget_set_name(trk_check_rva, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(trk_vbox), trk_check_rva, FALSE, FALSE, 0);

	if (data->trk_rva_enabled) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(trk_check_rva), TRUE);
	}

	trk_check_comment =
		gtk_check_button_new_with_label(_("Import Comment tag"));
	gtk_widget_set_name(trk_check_comment, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(trk_vbox), trk_check_comment, FALSE, FALSE, 0);

	if (data->trk_comment_enabled) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(trk_check_comment), TRUE);
	}


	/* Sandbox */

        snd_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(snd_vbox), 5);
	label = gtk_label_new(_("Sandbox"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), snd_vbox, label);

	if (data->file_transform_sandbox == NULL) {
		data->file_transform_sandbox = file_transform_new();
	}

	data->snd_file_trans_gui = file_transform_gui_new(data->file_transform_sandbox, snd_vbox);

	frame = gtk_frame_new(_("Sandbox"));
        gtk_box_pack_start(GTK_BOX(snd_vbox), frame, FALSE, FALSE, 5);

	table = gtk_table_new(2, 2, FALSE);
        gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_container_add(GTK_CONTAINER(frame), table);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Filename:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 0, 5);

	data->snd_entry_input = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), data->snd_entry_input, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

	snd_button_test = gtk_button_new_with_label(_("Test"));
	gtk_table_attach(GTK_TABLE(table), snd_button_test, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 0, 5);
        g_signal_connect(G_OBJECT(snd_button_test), "clicked", G_CALLBACK(snd_button_test_clicked),
			 data);

	data->snd_entry_output = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), data->snd_entry_output, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);


	/* run dialog */

	gtk_widget_show_all(dialog);
	gtk_widget_hide(art_file_trans_gui->label_error);
	gtk_widget_hide(rec_file_trans_gui->label_error);
	gtk_widget_hide(trk_file_trans_gui->label_error);
	gtk_widget_hide(data->snd_file_trans_gui->label_error);
	gtk_widget_grab_focus(gen_root_entry);

 display:

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * proot = g_filename_from_utf8(gtk_entry_get_text(GTK_ENTRY(gen_root_entry)), -1, NULL, NULL, NULL);

		data->root[0] = '\0';

		if (proot == NULL || proot[0] == '\0') {
			gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
			gtk_widget_grab_focus(gen_root_entry);
			goto display;
		}

		normalize_filename(proot, data->root);
		g_free(proot);


		if (data->type == BUILD_TYPE_STRICT) {
			set_option_from_combo(gen_depth_combo, &data->artist_dir_depth);
		}

		set_option_from_toggle(gen_check_reset_data, &data->reset_existing_data);
		set_option_from_toggle(gen_check_remove_dead, &data->remove_dead_files);
		set_option_from_toggle(rec_check_add_year, &data->rec_add_year_to_comment);

		set_option_from_toggle(trk_check_rva, &data->trk_rva_enabled);
		set_option_from_toggle(trk_check_comment, &data->trk_comment_enabled);

		set_option_from_combo(art_sort_combo, &data->artist_sort_by);
		set_option_from_combo(rec_sort_combo, &data->record_sort_by);

		set_option_from_entry(gen_entry_excl, data->excl_pattern, MAXLEN);
		set_option_from_entry(gen_entry_incl, data->incl_pattern, MAXLEN);

		set_option_from_toggle(gen_check_excl, &data->excl_enabled);
		set_option_from_toggle(gen_check_incl, &data->incl_enabled);

		if (data->excl_enabled) {
			data->excl_patternv =
			    g_strsplit(gtk_entry_get_text(GTK_ENTRY(gen_entry_excl)), ",", 0);
		}

		if (data->incl_enabled) {
			data->incl_patternv =
			    g_strsplit(gtk_entry_get_text(GTK_ENTRY(gen_entry_incl)), ",", 0);
		}


		if (file_transform_gui_sync(art_file_trans_gui) != 0) {
			gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 1);
			goto display;
		}

		if (file_transform_gui_sync(rec_file_trans_gui) != 0) {
			gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 2);
			goto display;
		}

		if (file_transform_gui_sync(trk_file_trans_gui) != 0) {
			gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 3);
			goto display;
		}

		capitalize_gui_sync(art_cap_gui);
		capitalize_gui_sync(rec_cap_gui);
		capitalize_gui_sync(trk_cap_gui);

		data_src_gui_sync(art_src_gui);
		data_src_gui_sync(rec_src_gui);
		data_src_gui_sync(trk_src_gui);


		data->cddb_enabled = 0;
		for (i = 0; i < 3; i++) {
			if ((data->data_src_artist->type[i] == DATA_SRC_CDDB && data->data_src_artist->enabled[i]) ||
			    (data->data_src_record->type[i] == DATA_SRC_CDDB && data->data_src_record->enabled[i]) ||
			    (data->data_src_track->type[i] == DATA_SRC_CDDB && data->data_src_track->enabled[i])) {

				data->cddb_enabled = 1;
				break;
			}
		}

		data->meta_enabled = 0;
		for (i = 0; i < 3; i++) {
			if ((data->data_src_artist->type[i] == DATA_SRC_META && data->data_src_artist->enabled[i]) ||
			    (data->data_src_record->type[i] == DATA_SRC_META && data->data_src_record->enabled[i]) ||
			    (data->data_src_track->type[i] == DATA_SRC_META && data->data_src_track->enabled[i])) {

				data->meta_enabled = 1;
				break;
			}
		}

		build_store_save(data);

		ret = 1;

        } else {
                ret = 0;
        }

        gtk_widget_destroy(dialog);

	free(art_file_trans_gui);
	free(rec_file_trans_gui);
	free(trk_file_trans_gui);
	free(data->snd_file_trans_gui);

	free(art_cap_gui);
	free(rec_cap_gui);
	free(trk_cap_gui);

	free(art_src_gui);
	free(rec_src_gui);
	free(trk_src_gui);

        return ret;
}


void
cancel_build(GtkButton * button, gpointer user_data) {

	build_store_t * data = (build_store_t *)user_data;

	data->cancelled = 1;

	if (data->prog_window) {
		unregister_toplevel_window(data->prog_window);
		gtk_widget_destroy(data->prog_window);
		data->prog_window = NULL;
	}
}

gboolean
prog_window_close(GtkWidget * widget, GdkEvent * event, gpointer user_data) {

	cancel_build(NULL, user_data);
	return FALSE;
}


gboolean
finish_build(gpointer user_data) {

	build_store_t * data = (build_store_t *)user_data;

	if (data->prog_window) {
		unregister_toplevel_window(data->prog_window);
		gtk_widget_destroy(data->prog_window);
		data->prog_window = NULL;
	}

        build_store_free(data);

	build_busy = 0;

	return FALSE;
}


void
progress_window(build_store_t * data) {

	GtkWidget * table;
	GtkWidget * label;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * hbuttonbox;


	data->prog_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	register_toplevel_window(data->prog_window, TOP_WIN_SKIN | TOP_WIN_TRAY);
	gtk_window_set_transient_for(GTK_WINDOW(data->prog_window), GTK_WINDOW(browser_window));
        gtk_window_set_title(GTK_WINDOW(data->prog_window), _("Building store from filesystem"));
        gtk_window_set_position(GTK_WINDOW(data->prog_window), GTK_WIN_POS_CENTER);
        gtk_window_resize(GTK_WINDOW(data->prog_window), 430, 110);
        g_signal_connect(G_OBJECT(data->prog_window), "delete_event",
                         G_CALLBACK(prog_window_close), data);
        gtk_container_set_border_width(GTK_CONTAINER(data->prog_window), 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(data->prog_window), vbox);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

	insert_label_entry(table, _("Processing:"), &data->prog_file_entry, NULL, 0, 1, FALSE);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Action:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	data->prog_action_label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), data->prog_action_label, FALSE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 5);

        gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), FALSE, TRUE, 5);

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        data->prog_cancel_button = gui_stock_label_button (_("Abort"), GTK_STOCK_CANCEL);
        g_signal_connect(data->prog_cancel_button, "clicked", G_CALLBACK(cancel_build), data);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), data->prog_cancel_button);

        gtk_widget_grab_focus(data->prog_cancel_button);

        gtk_widget_show_all(data->prog_window);
}

gboolean
set_prog_file_entry_idle(gpointer user_data) {

	build_store_t * data = (build_store_t *)user_data;

	if (data->prog_window) {

		char * utf8 = NULL;

                AQUALUNG_MUTEX_LOCK(data->mutex);
                utf8 = g_filename_display_name(data->path);
                AQUALUNG_MUTEX_UNLOCK(data->mutex);

		gtk_entry_set_text(GTK_ENTRY(data->prog_file_entry), utf8);
		gtk_widget_grab_focus(data->prog_cancel_button);
		g_free(utf8);
	}

	return FALSE;
}

void
set_prog_file_entry(build_store_t * data, char * path) {

	AQUALUNG_MUTEX_LOCK(data->mutex);
	strncpy(data->path, path, MAXLEN-1);
        AQUALUNG_MUTEX_UNLOCK(data->mutex);

	aqualung_idle_add(set_prog_file_entry_idle, data);
}

gboolean
set_prog_action_label_idle(gpointer user_data) {

	build_store_t * data = (build_store_t *)user_data;

	if (data->prog_window) {
                AQUALUNG_MUTEX_LOCK(data->mutex);
		gtk_label_set_text(GTK_LABEL(data->prog_action_label), data->action);
                AQUALUNG_MUTEX_UNLOCK(data->mutex);
	}

	return FALSE;
}

void
set_prog_action_label(build_store_t * data, char * action) {

	AQUALUNG_MUTEX_LOCK(data->mutex);
	strncpy(data->action, action, MAXLEN-1);
	AQUALUNG_MUTEX_UNLOCK(data->mutex);

	aqualung_idle_add(set_prog_action_label_idle, data);
}

void
file_transform(char * buf, file_transform_t * model) {


	int i;
	char tmp[MAXLEN];
	char * p = tmp;

	int offs = 0;
	regmatch_t regmatch[10];


	tmp[0] = '\0';

	if (model->regexp[0] != '\0') {

		p = buf;

		while (!regexec(&model->compiled, p + offs, 10, regmatch, 0)) {

			int i = 0;
			int b = strlen(model->replacement) - 1;

			strncat(tmp, p + offs, regmatch[0].rm_so);

			for (i = 0; i < b; i++) {

				if (model->replacement[i] == '\\' && isdigit(model->replacement[i+1])) {

					int j = model->replacement[i+1] - '0';

					if (j == 0 || j > model->compiled.re_nsub) {
						i++;
						continue;
					}

					strncat(tmp, p + offs + regmatch[j].rm_so,
						regmatch[j].rm_eo - regmatch[j].rm_so);
					i++;
				} else {
					strncat(tmp, model->replacement + i, 1);
				}
			}

			strncat(tmp, model->replacement + i, 1);
			offs += regmatch[0].rm_eo;
		}

		if (!*tmp) {
			strncpy(tmp, p + offs, MAXLEN-1);
		} else {
			strcat(tmp, p + offs);
		}

	} else {
		strncpy(tmp, buf, MAXLEN-1);
	}


	p = tmp;

	if (model->rm_extension) {
		char * c = NULL;
		if ((c = strrchr(p, '.')) != NULL) {
			*c = '\0';
		}
	}

	if (model->rm_number) {

		if (strlen(p) >= 3 && isdigit(*p) && isdigit(*(p + 1)) &&
		    (*(p + 2) == ' ' || *(p + 2) == '_' || *(p + 2) == '-' || *(p + 2) == '.')) {

			p += 3;

			while (*p && (*p == ' ' || *p == '_' || *p == '-' || *p == '.')) {
				++p;
			}
		}

		if (*p) {
			buf[0] = '\0';
			strcpy(buf, p);
		}
	}

	if (model->us_to_space) {
		for (i = 0; p[i]; i++) {
			if (p[i] == '_') {
				p[i] = ' ';
			}
		}
	}

	if (model->rm_multi_spaces) {

		char t[MAXLEN];
		int j = 0;
		int len = strlen(p);

		for (i = 0; i < len; i++) {
			if (j == 0 && p[i] == ' ') {
				continue;
			}

			if (p[i] != ' ') {
				t[j++] = p[i];
				continue;
			} else {
				if (i + 1 < len && p[i + 1] != ' ') {
					t[j++] = ' ';
					continue;
				}
			}
		}

		t[j] = '\0';

		strcpy(p, t);
	}

	strncpy(buf, p, MAXLEN-1);
}


void
cap_preserve(char * buf, capitalize_t * model) {

	int i = 0;
	int len_buf = 0;
	gchar * sub = NULL;
	gchar * haystack = NULL;

	if (model->pre_stringv[0] == NULL) {
		return;
	}

	len_buf = strlen(buf);
	haystack = g_utf8_strdown(buf, -1);

	for (i = 0; model->pre_stringv[i]; i++) {
		int len_cap = 0;
		int off = 0;
		gchar * needle = NULL;
		gchar * p = NULL;

		if (*(model->pre_stringv[i]) == '\0') {
			continue;
		}

		len_cap = strlen(model->pre_stringv[i]);
		needle = g_utf8_strdown(model->pre_stringv[i], -1);

		while ((sub = strstr(haystack + off, needle)) != NULL) {
			int len_sub = strlen(sub);

			if (((p = g_utf8_find_prev_char(haystack, sub)) == NULL ||
			     !g_unichar_isalpha(g_utf8_get_char(p))) &&
			    ((p = g_utf8_find_next_char(sub + len_cap - 1, NULL)) == NULL ||
			     !g_unichar_isalpha(g_utf8_get_char(p)))) {
				strncpy(buf + len_buf - len_sub, model->pre_stringv[i], len_cap);
			}

			off = len_buf - len_sub + len_cap;
		}

		g_free(needle);
	}

	g_free(haystack);
}


int
cap_after(gunichar ch) {

	int i;
	gunichar * chars = NULL;

	chars = (gunichar *)malloc(8 * sizeof(gunichar));
	chars[0] = g_utf8_get_char(" ");
	chars[1] = g_utf8_get_char("\t");
	chars[2] = g_utf8_get_char("(");
	chars[3] = g_utf8_get_char("[");
	chars[4] = g_utf8_get_char("/");
	chars[5] = g_utf8_get_char("\"");
	chars[6] = g_utf8_get_char(".");
	chars[7] = g_utf8_get_char("-");

	for (i = 0; i < 8; i++) {
		if (chars[i] == ch) {
			free(chars);
			return 1;
		}
	}

	free(chars);
	return 0;
}


void
capitalize(char * buf, capitalize_t * model) {

	int n;
	gchar * p = buf;
	gchar * result = NULL;

	gunichar prev = 0;

	gchar ** stringv = NULL;

	for (n = 0; *p; p = g_utf8_next_char(p)) {
		gunichar ch = g_utf8_get_char(p);
		gunichar new_ch;
		int len;

		if (prev == 0 || (model->mode == CAP_ALL_WORDS && cap_after(prev))) {
			new_ch = g_unichar_totitle(ch);
		} else {
			if (model->low_enabled) {
				new_ch = g_unichar_tolower(ch);
			} else {
				new_ch = ch;
			}
		}

		if ((stringv = (gchar **)realloc(stringv, (n + 2) * sizeof(gchar *))) == NULL) {
			fprintf(stderr, "build_store.c: capitalize(): realloc error\n");
			return;
		}

		len = g_unichar_to_utf8(new_ch, NULL);

		if ((*(stringv + n) = (gchar *)malloc((len + 1) * sizeof(gchar))) == NULL) {
			fprintf(stderr, "build_store.c: capitalize(): malloc error\n");
			return;
		}

		g_unichar_to_utf8(new_ch, *(stringv + n));
		*(*(stringv + n) + len) = '\0';

		prev = ch;
		++n;
	}

	if (stringv == NULL) {
		return;
	}

	*(stringv + n) = NULL;

	result = g_strjoinv(NULL, stringv);
	strncpy(buf, result, MAXLEN-1);
	g_free(result);

	while (n >= 0) {
		free(*(stringv + n--));
	}

	free(stringv);

	if (model->pre_enabled) {
		cap_preserve(buf, model);
	}
}


void
process_meta(build_store_t * data, build_disc_t * disc) {

	build_track_t * ptrack;

	map_t * map_artist = NULL;
	map_t * map_record = NULL;
	map_t * map_year = NULL;

	for (ptrack = disc->tracks; ptrack; ptrack = ptrack->next) {

		char * tmp;
		file_decoder_t * fdec = file_decoder_new();
		if (fdec == NULL) {
			fprintf(stderr, "process_meta: file_decoder_new() failed\n");
			return;
		}
		if (file_decoder_open(fdec, ptrack->filename) == 0) {

			if (metadata_get_artist(fdec->meta, &tmp) && !is_all_wspace(tmp)) {
				map_put(&map_artist, tmp);
			}

			if (metadata_get_album(fdec->meta, &tmp) && !is_all_wspace(tmp)) {
				map_put(&map_record, tmp);
			}

			if (metadata_get_title(fdec->meta, &tmp) && !is_all_wspace(tmp)) {
				strncpy(ptrack->name[DATA_SRC_META], tmp, MAXLEN-1);
			}

			if (disc->record.year[0] == '\0' &&
			    metadata_get_date(fdec->meta, &tmp)) {

				int y = atoi(tmp);
				if (is_valid_year(y)) {
					map_put(&map_year, tmp);
				}
			}

			if (data->trk_rva_enabled) {
			        ptrack->rva_found = metadata_get_rva(fdec->meta, &ptrack->rva);
			} else {
				ptrack->rva_found = 0;
			}

			if (data->trk_comment_enabled &&
			    metadata_get_comment(fdec->meta, &tmp) &&
			    !is_all_wspace(tmp)) {

				strncpy(ptrack->comment, tmp, MAXLEN-1);
			}
			file_decoder_close(fdec);
		}
		file_decoder_delete(fdec);
	}

	if (map_artist) {
		strncpy(disc->artist.name[DATA_SRC_META], map_get_max(map_artist), MAXLEN-1);
	}

	if (map_record) {
		strncpy(disc->record.name[DATA_SRC_META], map_get_max(map_record), MAXLEN-1);
	}

	if (map_year) {
		strncpy(disc->record.year, map_get_max(map_year), MAXLEN-1);
	}

	map_free(map_artist);
	map_free(map_record);
	map_free(map_year);
}


void
add_new_track(GtkTreeIter * record_iter, build_track_t * ptrack, int i) {

	GtkTreeIter track_iter;
	char sort_name[16];
	track_data_t * track_data;


	if ((track_data = (track_data_t *)calloc(1, sizeof(track_data_t))) == NULL) {
		fprintf(stderr, "add_new_track: calloc error\n");
		return;
	}

	if (i == 0) {
		i = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), record_iter) + 1;
	}

	snprintf(sort_name, 15, "%02d", i);

	gtk_tree_store_append(music_store, &track_iter, record_iter);
	gtk_tree_store_set(music_store, &track_iter,
			   MS_COL_NAME, ptrack->final,
			   MS_COL_SORT, sort_name,
			   MS_COL_DATA, track_data, -1);

	track_data->file = strdup(ptrack->filename);
	track_data->comment = strdup(ptrack->comment);
	track_data->duration = ptrack->duration;
	track_data->volume = 1.0f;

	if (ptrack->rva_found) {
		track_data->rva = ptrack->rva;
		track_data->use_rva = 1;
	}

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &track_iter, MS_COL_ICON, icon_track, -1);
	}
}


gboolean
write_record_to_store(gpointer user_data) {

	build_store_t * data = (build_store_t *)user_data;
	build_track_t * ptrack;
	int result = 0;
	int i;

	GtkTreeIter record_iter;


	if (data->artist_iter_is_set) {
		result = artist_get_iter_for_tracklist(&data->artist_iter, &record_iter, data->disc);
	} else {
		result = store_get_iter_for_tracklist(&data->store_iter,
						      &data->artist_iter,
						      &record_iter,
						      data->disc,
						      &data->artist_name_map);
		data->artist_iter_is_set = 1;
	}

	if (data->artist_name_map != NULL && !data->disc->artist.unknown) {

		char * max = NULL;
		map_put(&data->artist_name_map, data->disc->artist.final);
		max = map_get_max(data->artist_name_map);

		gtk_tree_store_set(music_store, &data->artist_iter,
				   MS_COL_NAME, max,
				   MS_COL_SORT, data->disc->artist.sort, -1);
	}

	if (result == RECORD_NEW) {
		for (i = 0, ptrack = data->disc->tracks; ptrack; i++, ptrack = ptrack->next) {
			add_new_track(&record_iter, ptrack, i + 1);
		}
	}

	if (result == RECORD_EXISTS) {

		GtkTreeIter iter;

		if (data->disc->record.unknown) {
			gtk_tree_store_set(music_store, &record_iter,
					   MS_COL_NAME, data->disc->record.final, -1);
		}

		i = 0;
		ptrack = data->disc->tracks;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     &iter, &record_iter, i++)) {

			track_data_t * track_data;

			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter,
					   MS_COL_DATA, &track_data, -1);

			if (track_data->comment == NULL || track_data->comment[0] == '\0') {
				free_strdup(&track_data->comment, ptrack->comment);
			}

			gtk_tree_store_set(music_store, &iter, MS_COL_NAME, ptrack->final, -1);
			track_data->duration = ptrack->duration;

			if (ptrack->rva_found) {
				track_data->rva = ptrack->rva;
				track_data->use_rva = 1;
			}

			ptrack = ptrack->next;
		}
	}

	music_store_mark_changed(&data->store_iter);

	data->write_data_locked = 0;

	return FALSE;
}


gboolean
write_track_to_store(gpointer user_data) {

	build_store_t * data = (build_store_t *)user_data;

	GtkTreeIter record_iter;


	if (data->disc->flag) { /* track is present, reset data */

		track_data_t * track_data;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &data->disc->iter,
                                   MS_COL_DATA, &track_data, -1);

		if (track_data->comment == NULL || track_data->comment[0] == '\0') {
			free_strdup(&track_data->comment, data->disc->tracks->comment);
		}

		gtk_tree_store_set(music_store, &data->disc->iter,
                                   MS_COL_NAME, data->disc->tracks->final, -1);
		track_data->duration = data->disc->tracks->duration;

		if (data->disc->tracks->rva_found) {
			track_data->rva = data->disc->tracks->rva;
			track_data->use_rva = 1;
		}

	} else {

		GtkTreeIter artist_iter;

		store_get_iter_for_artist_and_record(&data->store_iter,
						     &artist_iter,
						     &record_iter,
						     data->disc);

		add_new_track(&record_iter, data->disc->tracks, 0 /* append */);
	}

	music_store_mark_changed(&data->store_iter);

	data->write_data_locked = 0;

	return FALSE;
}


static int
filter(const struct dirent * de) {

	return de->d_name[0] != '.';
}


int
filter_excl_incl(build_store_t * data, char * basename) {

	char * utf8 = g_filename_display_name(basename);

	if (data->excl_enabled) {
		int k;
		int match = 0;
		for (k = 0; data->excl_patternv[k]; k++) {

			if (*(data->excl_patternv[k]) == '\0') {
				continue;
			}

			if (fnmatch(data->excl_patternv[k], utf8, FNM_CASEFOLD) == 0) {
				match = 1;
				break;
			}
		}

		if (match) {
			g_free(utf8);
			return 0;
		}
	}

	if (data->incl_enabled) {
		int k;
		int match = 0;
		for (k = 0; data->incl_patternv[k]; k++) {

			if (*(data->incl_patternv[k]) == '\0') {
				continue;
			}

			if (fnmatch(data->incl_patternv[k], utf8, FNM_CASEFOLD) == 0) {
				match = 1;
				break;
			}
		}

		if (!match) {
			g_free(utf8);
			return 0;
		}
	}

	g_free(utf8);
	return 1;
}


void
get_file_list(build_store_t * data, char * dir_record, build_disc_t * disc, int open) {

	int i, n;
	struct dirent ** ent_track;
	char basename[MAXLEN];
	char filename[MAXLEN];

	build_track_t * last_track = NULL;
	float duration = 0.0f;

	char * utf8;


	n = scandir(dir_record, &ent_track, filter, alphasort);
	for (i = 0; i < n; i++) {

		strncpy(basename, ent_track[i]->d_name, MAXLEN-1);
		snprintf(filename, MAXLEN-1, "%s/%s", dir_record, ent_track[i]->d_name);

		if (is_dir(filename) || !filter_excl_incl(data, basename)) {
			free(ent_track[i]);
			continue;
		}

		if (!open || (duration = get_file_duration(filename)) > 0.0f) {

			build_track_t * track;

			if ((track = (build_track_t *)calloc(1, sizeof(build_track_t))) == NULL) {
				fprintf(stderr, "build_store.c: get_file_list(): calloc error\n");
				return;
			} else {
				strncpy(track->filename, filename, MAXLEN-1);

				utf8 = g_filename_display_name(ent_track[i]->d_name);
				strncpy(track->d_name, utf8, MAXLEN-1);
				g_free(utf8);

				track->duration = duration;
				track->rva = 1.0f;
				track->rva_found = 0;
			}

			if (last_track == NULL) {
				disc->tracks = last_track = track;
			} else {
				last_track->next = track;
				last_track = track;
			}
		}

		free(ent_track[i]);
	}

	if (n > 0) {
		free(ent_track);
	}
}


void
process_filename(build_store_t * data, build_disc_t * disc) {

	int i, j;
	char * utf8;
	build_track_t * ptrack;


	/* Artist */

	for (i = 0; i < 3; i++) {

		if (data->data_src_artist->enabled[i]) {

			if (data->data_src_artist->type[i] == DATA_SRC_FILE) {
				strncpy(disc->artist.name[DATA_SRC_FILE],
					disc->artist.d_name, MAXLEN-1);
			}

			if (disc->artist.name[data->data_src_artist->type[i]][0] != '\0') {
				strncpy(disc->artist.final,
					disc->artist.name[data->data_src_artist->type[i]], MAXLEN-1);
				break;
			}
		}
	}

	if (i == 3) {
		if (data->type == BUILD_TYPE_STRICT) {
			char tmp[MAXLEN];
			snprintf(tmp, MAXLEN-1, "%s (%s)", _("Unknown Artist"), disc->artist.d_name);
			strncpy(disc->artist.final, tmp, MAXLEN-1);
		} else {
			strncpy(disc->artist.final, _("Unknown Artist"), MAXLEN-1);
		}
		disc->artist.unknown = 1;
	} else {
		file_transform(disc->artist.final, data->file_transform_artist);

		if (data->capitalize_artist->enabled) {
			capitalize(disc->artist.final, data->capitalize_artist);
		}
	}


	/* Record */

	for (i = 0; i < 3; i++) {

		if (data->data_src_record->enabled[i]) {

			if (data->data_src_record->type[i] == DATA_SRC_FILE) {
				strncpy(disc->record.name[DATA_SRC_FILE],
					disc->record.d_name, MAXLEN-1);
			}

			if(disc->record.name[data->data_src_record->type[i]][0] != '\0') {
				strncpy(disc->record.final,
					disc->record.name[data->data_src_record->type[i]], MAXLEN-1);
				break;
			}
		}
	}

	if (i == 3) {
		if (data->type == BUILD_TYPE_STRICT) {
			char tmp[MAXLEN];
			snprintf(tmp, MAXLEN-1, "%s (%s)", _("Unknown Record"), disc->record.d_name);
			strncpy(disc->record.final, tmp, MAXLEN-1);
		} else {
			strncpy(disc->record.final, disc->record.dirname, MAXLEN-1);
		}
		disc->record.unknown = 1;
	} else {
		file_transform(disc->record.final, data->file_transform_record);

		if (data->capitalize_record->enabled) {
			capitalize(disc->record.final, data->capitalize_record);
		}
	}


	/* Tracks */

	for (i = 0, ptrack = disc->tracks; ptrack; i++, ptrack = ptrack->next) {

		strncpy(ptrack->name[DATA_SRC_FILE], ptrack->d_name, MAXLEN-1);

		for (j = 0; j < 3; j++) {

			if (data->data_src_track->enabled[j]) {

				if (data->data_src_track->type[i] == DATA_SRC_FILE) {
					strncpy(ptrack->name[DATA_SRC_FILE],
						ptrack->d_name, MAXLEN-1);
				}

				if (ptrack->name[data->data_src_track->type[j]][0] != '\0') {
					strncpy(ptrack->final,
						ptrack->name[data->data_src_track->type[j]], MAXLEN-1);
					break;
				}
			}
		}

		if (j == 3) {
			strncpy(ptrack->final, ptrack->d_name, MAXLEN-1);
		} else {
			file_transform(ptrack->final, data->file_transform_track);

			if (data->capitalize_track->enabled) {
				capitalize(ptrack->final, data->capitalize_track);
			}
		}
	}


	/* Artist and Record sort name */

	switch (data->artist_sort_by) {
	case SORT_NAME:
		strncpy(disc->artist.sort, disc->artist.final, MAXLEN-1);
		break;
	case SORT_NAME_LOW:
		utf8 = g_utf8_strdown(disc->artist.final, -1);
		strncpy(disc->artist.sort, utf8, MAXLEN-1);
		g_free(utf8);
		break;
	case SORT_DIR:
		strncpy(disc->artist.sort, disc->artist.d_name, MAXLEN-1);
		break;
	case SORT_DIR_LOW:
		utf8 = g_utf8_strdown(disc->artist.d_name, -1);
		strncpy(disc->artist.sort, utf8, MAXLEN-1);
		g_free(utf8);
		break;
	}

	if (disc->artist.unknown) {
		char tmp[MAXLEN];
		snprintf(tmp, MAXLEN-1, "0 %s", disc->artist.sort);
		strncpy(disc->artist.sort, tmp, MAXLEN-1);
	}

	switch (data->record_sort_by) {
	case SORT_YEAR:
		strncpy(disc->record.sort, disc->record.year, MAXLEN-1);
		break;
	case SORT_NAME:
		strncpy(disc->record.sort, disc->record.final, MAXLEN-1);
		break;
	case SORT_NAME_LOW:
		utf8 = g_utf8_strdown(disc->record.final, -1);
		strncpy(disc->record.sort, utf8, MAXLEN-1);
		g_free(utf8);
		break;
	case SORT_DIR:
		strncpy(disc->record.sort, disc->record.d_name, MAXLEN-1);
		break;
	case SORT_DIR_LOW:
		utf8 = g_utf8_strdown(disc->record.d_name, -1);
		strncpy(disc->record.sort, utf8, MAXLEN-1);
		g_free(utf8);
		break;
	}

	if (disc->record.unknown) {
		char tmp[MAXLEN];
		snprintf(tmp, MAXLEN-1, "0 %s", disc->record.sort);
		strncpy(disc->record.sort, tmp, MAXLEN-1);
	}
}


#ifdef HAVE_CDDB
static int
cddb_init_query_data(build_disc_t * disc, int * ntracks, int ** frames, int * length) {

	int i;
	float len = 0.0f;
	float offset = 150.0f; /* leading 2 secs in frames */

	build_track_t * ptrack = NULL;

	for (*ntracks = 0, ptrack = disc->tracks; ptrack; (*ntracks)++, ptrack = ptrack->next);

	if ((*frames = (int *)calloc(*ntracks, sizeof(int))) == NULL) {
		fprintf(stderr, "build_store.c: cddb_init_query_data(): calloc error\n");
		return 1;
	}

	for (i = 0, ptrack = disc->tracks; ptrack; i++, ptrack = ptrack->next) {

		*((*frames) + i) = (int)offset;
		len += ptrack->duration;
		offset += 75.0f * ptrack->duration;
	}

	*length = (int)len;

	return 0;
}
#endif /* HAVE_CDDB */


void
process_record(build_store_t * data, char * dir_record, char * artist_d_name, char * record_d_name) {

	struct timespec req_time;
	struct timespec rem_time;

	build_disc_t * disc = NULL;
	build_track_t * ptrack = NULL;

	char * utf8;


	req_time.tv_sec = 0;
        req_time.tv_nsec = 10000000;


	if ((disc = (build_disc_t *)calloc(1, sizeof(build_disc_t))) == NULL) {
		fprintf(stderr, "build_store.c: process_record(): calloc error\n");
		return;
	}

	utf8 = g_filename_display_name(artist_d_name);
	strncpy(disc->artist.d_name, utf8, MAXLEN-1);
	g_free(utf8);

	utf8 = g_filename_display_name(record_d_name);
	strncpy(disc->record.d_name, utf8, MAXLEN-1);
	g_free(utf8);


	set_prog_action_label(data, _("Scanning files"));

	if (!data->reset_existing_data) {
		get_file_list(data, dir_record, disc, 0/* do not open*/);

		if (disc->tracks == NULL) {
			goto finish;
		}

		if (store_contains_disc(&data->store_iter, NULL, NULL, disc)) {
			goto finish;
		}
	}

	get_file_list(data, dir_record, disc, 1/* try to open*/);

	if (disc->tracks == NULL) {
		goto finish;
	}

	if (!data->reset_existing_data) {
		if (store_contains_disc(&data->store_iter, NULL, NULL, disc)) {
			goto finish;
		}
	}

	/* at this point the record either exists or should be re-read */

	if (data->meta_enabled) {
		set_prog_action_label(data, _("Processing metadata"));
		process_meta(data, disc);
	}


#ifdef HAVE_CDDB
	if (data->cddb_enabled) {

		int ntracks;
		int * frames;
		int length;

		char artist[MAXLEN];
		char record[MAXLEN];
		int year = 0;
		char ** tracks;
		int i;

		artist[0] = '\0';
		record[0] = '\0';

		set_prog_action_label(data, _("CDDB lookup"));

		if (cddb_init_query_data(disc, &ntracks, &frames, &length) != 0) {
			return;
		}

		if ((tracks = calloc(ntracks, sizeof(char *))) == NULL) {
			fprintf(stderr, "process_record: calloc error\n");
			return;
		}

		for (i = 0; i < ntracks; i++) {
			if ((tracks[i] = calloc(1, MAXLEN * sizeof(char))) == NULL) {
				fprintf(stderr, "process_record: calloc error\n");
				return;
			}
		}

		cddb_query_batch(ntracks, frames, length, artist, record, &year, tracks);

		if (artist[0] != '\0') {
			strncpy(disc->artist.name[DATA_SRC_CDDB], artist, MAXLEN-1);
		}

		if (record[0] != '\0') {
			strncpy(disc->record.name[DATA_SRC_CDDB], record, MAXLEN-1);
		}

		if (year > 0) {
			snprintf(disc->record.year, MAXLEN-1, "%d", year);
		}

		for (i = 0, ptrack = disc->tracks; ptrack && i < ntracks; i++, ptrack = ptrack->next) {
			strncpy(ptrack->name[DATA_SRC_CDDB], tracks[i], MAXLEN-1);
			free(tracks[i]);
		}

		free(tracks);
	}
#endif /* HAVE_CDDB */


	set_prog_action_label(data, _("Name transformation"));
	process_filename(data, disc);


	if (data->rec_add_year_to_comment) {
		strncpy(disc->record.comment, disc->record.year, MAXLEN-1);
	}


	/* wait for the gtk thread to update music_store */

	data->write_data_locked = 1;
	data->disc = disc;

	aqualung_idle_add(write_record_to_store, data);

	while (data->write_data_locked) {
		nanosleep(&req_time, &rem_time);
	}

 finish:

	/* free tracklist */

	for (ptrack = disc->tracks; ptrack; disc->tracks = ptrack) {
		ptrack = disc->tracks->next;
		free(disc->tracks);
	}

	free(disc);
}


void
process_track(build_store_t * data, char * filename, char * d_name, float duration) {

	struct timespec req_time;
	struct timespec rem_time;

	GtkTreeIter iter_track;
        build_disc_t * disc;

	req_time.tv_sec = 0;
        req_time.tv_nsec = 10000000;


	if ((disc = (build_disc_t *)calloc(1, sizeof(build_disc_t))) == NULL) {
		fprintf(stderr, "build_store.c: process_track(): calloc error\n");
		return;
	}

	if ((disc->tracks = (build_track_t *)calloc(1, sizeof(build_track_t))) == NULL) {
		fprintf(stderr, "build_store.c: process_track(): calloc error\n");
		return;
	} else {
		char * utf8;

		utf8 = g_filename_display_name(d_name);
		strncpy(disc->tracks->d_name, utf8, MAXLEN-1);
		strncpy(disc->record.d_name, utf8, MAXLEN-1);
		strncpy(disc->artist.d_name, utf8, MAXLEN-1);
		g_free(utf8);

		utf8 = g_filename_display_name(filename);
		strncpy(disc->record.dirname, g_path_get_dirname(utf8), MAXLEN-1);
		g_free(utf8);

		strncpy(disc->tracks->filename, filename, MAXLEN-1);
		disc->tracks->duration = duration;
		disc->tracks->rva = 1.0f;
		disc->tracks->rva_found = 0;
	}


	disc->flag = store_contains_track(&data->store_iter, &iter_track, filename);
	disc->iter = iter_track;

	if (!data->reset_existing_data && disc->flag) {
		goto finish;
	}


	if (data->meta_enabled) {
		set_prog_action_label(data, _("Processing metadata"));
		process_meta(data, disc);
	}


	set_prog_action_label(data, _("Name transformation"));
	process_filename(data, disc);


	if (data->rec_add_year_to_comment) {
		strncpy(disc->record.comment, disc->record.year, MAXLEN-1);
	}


	/* wait for the gtk thread to update music_store */

	data->write_data_locked = 1;
	data->disc = disc;

	aqualung_idle_add(write_track_to_store, data);

	while (data->write_data_locked) {
		nanosleep(&req_time, &rem_time);
	}

 finish:

	free(disc->tracks);
	free(disc);
}


void
scan_artist_record(build_store_t * data, char * dir_artist, char * name_artist, int depth) {


	int i, n;
	struct dirent ** ent_record;
	char dir_record[MAXLEN];


	data->artist_iter_is_set = 0;

	n = scandir(dir_artist, &ent_record, filter, alphasort);
	for (i = 0; i < n; i++) {

		if (data->cancelled) {
			break;
		}

		snprintf(dir_record, MAXLEN-1, "%s/%s", dir_artist, ent_record[i]->d_name);

		if (!is_dir(dir_record)) {
			free(ent_record[i]);
			continue;
		}

		set_prog_file_entry(data, dir_record);

		if (depth == 0) {

			data->artist_iter_is_set = 0;

			process_record(data,
                                       dir_record,
				       ent_record[i]->d_name,
				       ent_record[i]->d_name);

			if (data->artist_name_map != NULL) {
				map_free(data->artist_name_map);
				data->artist_name_map = NULL;
			}

		} else if (depth == 1) {

			process_record(data,
                                       dir_record,
			               name_artist,
				       ent_record[i]->d_name);
		} else {
			scan_artist_record(data, dir_record, ent_record[i]->d_name, depth - 1);
		}

		free(ent_record[i]);
	}


	while (i < n) {
		free(ent_record[i]);
		++i;
	}

	if (n > 0) {
		free(ent_record);
	}

	if (data->artist_name_map != NULL) {
		map_free(data->artist_name_map);
		data->artist_name_map = NULL;
	}
}


void
scan_recursively(build_store_t * data, char * dir) {

	int i, n;
	struct dirent ** ent;
	char path[MAXLEN];

	n = scandir(dir, &ent, filter, alphasort);
	for (i = 0; i < n; i++) {

		if (data->cancelled) {
			break;
		}

		snprintf(path, MAXLEN-1, "%s/%s", dir, ent[i]->d_name);

		if (is_dir(path)) {
			scan_recursively(data, path);
		} else {
			float d;

			set_prog_file_entry(data, path);
			set_prog_action_label(data, _("Reading file"));

			if (filter_excl_incl(data, path) && (d = get_file_duration(path)) > 0.0f) {
				process_track(data, path, ent[i]->d_name, d);
			}
		}

		free(ent[i]);
	}

	while (i < n) {
		free(ent[i]);
		++i;
	}

	if (n > 0) {
		free(ent);
	}
}


void
remove_dead_files(build_store_t * data) {

	GtkTreeIter artist_iter;
	GtkTreeIter record_iter;
	GtkTreeIter track_iter;
	GtkTreeModel * model = GTK_TREE_MODEL(music_store);
	int i, j, k;
	track_data_t * track_data = NULL;
	char * artist = NULL;
	gboolean exists;

	if (!data->remove_dead_files) {
		return;
	}

	set_prog_action_label(data, _("Removing non-existing files"));

	gdk_threads_enter();

	i = 0;
	while (gtk_tree_model_iter_nth_child(model, &artist_iter, &data->store_iter, i++)) {

		if (data->cancelled) {
			goto finish;
		}

		gtk_tree_model_get(model, &artist_iter, MS_COL_NAME, &artist, -1);
		gtk_entry_set_text(GTK_ENTRY(data->prog_file_entry), artist);
		gtk_widget_grab_focus(data->prog_cancel_button);

		j = 0;
		while (gtk_tree_model_iter_nth_child(model, &record_iter, &artist_iter, j++)) {

			if (data->cancelled) {
				goto finish;
			}

			k = 0;
			while (gtk_tree_model_iter_nth_child(model, &track_iter, &record_iter, k++)) {

				gtk_tree_model_get(model, &track_iter, MS_COL_DATA, &track_data, -1);

				gdk_threads_leave();
				exists = g_file_test(track_data->file, G_FILE_TEST_EXISTS);
				gdk_threads_enter();

				if (!exists) {
					store_file_remove_track(&track_iter);
					music_store_mark_changed(&data->store_iter);
					--k;
				}
			}

			if (!gtk_tree_model_iter_has_child(model, &record_iter)) {
				store_file_remove_record(&record_iter);
				music_store_mark_changed(&data->store_iter);
				--j;
			}
		}

		g_free(artist);

		if (!gtk_tree_model_iter_has_child(model, &artist_iter)) {
			store_file_remove_artist(&artist_iter);
			music_store_mark_changed(&data->store_iter);
			--i;
		}
	}

 finish:
	gdk_threads_leave();
}

void *
build_thread_strict(void * arg) {

	build_store_t * data = (build_store_t *)arg;

	AQUALUNG_THREAD_DETACH();

	remove_dead_files(data);
	scan_artist_record(data, data->root, NULL, (data->artist_dir_depth == 0) ? 0 : (data->artist_dir_depth + 1));

	aqualung_idle_add(finish_build, data);

	return NULL;
}


void *
build_thread_loose(void * arg) {

	build_store_t * data = (build_store_t *)arg;

	AQUALUNG_THREAD_DETACH();

	remove_dead_files(data);
	scan_recursively(data, data->root);

	aqualung_idle_add(finish_build, data);

	return NULL;
}


void
build_store(GtkTreeIter * iter, char * file) {

	build_store_t * data;

        if ((data = build_store_new(iter, file)) == NULL) {
		return;
        }

	switch (build_type_dialog(data)) {

	case BUILD_TYPE_STRICT:

		if (build_dialog(data)) {
			progress_window(data);
			AQUALUNG_THREAD_CREATE(data->thread_id, NULL, build_thread_strict, data);
			build_busy = 1;
		}

		break;

	case BUILD_TYPE_LOOSE:

		if (build_dialog(data)) {
			progress_window(data);
			AQUALUNG_THREAD_CREATE(data->thread_id, NULL, build_thread_loose, data);
			build_busy = 1;
		}

		break;
	}
}

int build_is_busy(void) {

	return build_busy;
}


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :

