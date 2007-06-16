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
#include <gtk/gtk.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <glib.h>
#else
#include <pthread.h>
#endif /* _WIN32*/

#include "common.h"
#include "utils.h"
#include "i18n.h"
#include "options.h"
#include "music_browser.h"
#include "gui_main.h"
#include "meta_decoder.h"
#include "build_store.h"
#include "cddb_lookup.h"


extern options_t options;
extern GtkTreeStore * music_store;

extern GtkWidget * browser_window;
extern GtkWidget * gui_stock_label_button(gchar * blabel, const gchar * bstock);

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


AQUALUNG_THREAD_DECLARE(build_thread_id)


enum {
	BUILD_TYPE_STRICT,
	BUILD_TYPE_LOOSE,
	BUILD_TYPE_NONE
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


int build_thread_state = BUILD_THREAD_FREE;
int build_type = BUILD_TYPE_STRICT;

volatile int build_cancelled = 0;
volatile int write_data_locked = 0;


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


GtkTreeIter store_iter;
GtkTreeIter artist_iter;

int artist_iter_is_set = 0;
map_t * artist_name_map = NULL;

char root[MAXLEN];
int artist_dir_depth = 1;
int reset_existing_data = 0;

int artist_sort_by = SORT_NAME_LOW;
int record_sort_by = SORT_YEAR;

int rec_add_year_to_comment = 0;
int trk_rva_enabled = 0;
int trk_comment_enabled = 0;

int excl_enabled = 1;
char excl_pattern[MAXLEN];
char ** excl_patternv = NULL;
int incl_enabled = 0;
char incl_pattern[MAXLEN];
char ** incl_patternv = NULL;

int cddb_enabled;
int meta_enabled;

GtkWidget * build_prog_window = NULL;
GtkWidget * prog_cancel_button;
GtkWidget * prog_file_entry;
GtkWidget * prog_action_label;
GtkWidget * snd_entry_output;



void file_transform(char * buf, file_transform_t * model);


data_src_t *
data_src_new() {

	data_src_t * model;

	if ((model = (data_src_t *)malloc(sizeof(data_src_t))) == NULL) {
		fprintf(stderr, "build_store.c: data_src_new(): malloc error\n");
		return NULL;
	}

	model->type[0] = DATA_SRC_CDDB;
	model->type[1] = DATA_SRC_META;
	model->type[2] = DATA_SRC_FILE;

#ifdef HAVE_CDDB
	model->enabled[0] = TRUE;
	model->cddb_mask = 1;
#else
	model->enabled[0] = FALSE;
	model->cddb_mask = 0;
#endif /* HAVE_CDDB*/

	model->enabled[1] = TRUE;
	model->enabled[2] = TRUE;

	return model;
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
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(gui->list), &iter, NULL, i)) {

		gtk_tree_model_get(GTK_TREE_MODEL(gui->list), &iter,
				   0, gui->model->enabled + i,
				   1, gui->model->type + i,
				   -1);
		++i;
	}
}


capitalize_t *
capitalize_new() {

	capitalize_t * model;

	if ((model = (capitalize_t *)malloc(sizeof(capitalize_t))) == NULL) {
		fprintf(stderr, "build_store.c: capitalize_new(): malloc error\n");
		return NULL;
	}

	model->enabled = TRUE;
	model->pre_enabled = TRUE;
	model->low_enabled = TRUE;
	model->mode = CAP_ALL_WORDS;
	model->pre_stringv = NULL;

	return model;
}

void
capitalize_check_pre_toggled(GtkWidget * widget, gpointer * data) {

	capitalize_gui_t * gui = (capitalize_gui_t *)data;

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui->check_pre));
	gtk_widget_set_sensitive(gui->entry_pre, state);
}

void
capitalize_check_toggled(GtkWidget * widget, gpointer * data) {

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
	gtk_entry_set_text(GTK_ENTRY(gui->entry_pre), "CD,a),b),c),d),I,II,III,IV,V,VI,VII,VIII,IX,X");
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

	if ((err = regcomp(&gui->model->compiled, gui->model->regexp, REG_EXTENDED))) {

		char msg[MAXLEN];
		char * utf8;

		regerror(err, &gui->model->compiled, msg, MAXLEN);
		utf8 = g_locale_to_utf8(msg, -1, NULL, NULL, NULL);
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


map_t *
map_new(char * str) {

	map_t * map;

	if ((map = (map_t *)malloc(sizeof(map_t))) == NULL) {
		fprintf(stderr, "build_store.c: map_new(): malloc error\n");
		return NULL;
	}

	strncpy(map->str, str, MAXLEN-1);
	map->count = 1;
	map->next = NULL;

	return map;
}

void
map_put(map_t ** map, char * str) {

	map_t * pmap;
	map_t * _pmap;

	if (str == NULL || str[0] == '\0') {
		return;
	}

	if (*map == NULL) {
		*map = map_new(str);
	} else {

		for (_pmap = pmap = *map; pmap; _pmap = pmap, pmap = pmap->next) {

			char * key1 = g_utf8_casefold(str, -1);
			char * key2 = g_utf8_casefold(pmap->str, -1);

			if (!g_utf8_collate(key1, key2)) {
				pmap->count++;
				g_free(key1);
				g_free(key2);
				return;
			}

			g_free(key1);
			g_free(key2);
		}

		_pmap->next = map_new(str);
	}
}

char *
map_get_max(map_t * map) {

	map_t * pmap;
	int max = 0;
	char * str = NULL;

	for (pmap = map; pmap; pmap = pmap->next) {
		if (max <= pmap->count) {
			str = pmap->str;
			max = pmap->count;
		}
	}

	return str;
}

void
map_free(map_t * map) {

	map_t * pmap;

	for (pmap = map; pmap; map = pmap) {
		pmap = map->next;
		free(map);
	}
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
	char * file;

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

				gtk_tree_model_get(GTK_TREE_MODEL(music_store), &track_iter, 2, &file, -1);

				if (strcmp(file, ptrack->filename)) {
					match = 0;
					g_free(file);
					break;
				}
				
				g_free(file);

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
	char * file;


	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &artist_iter, store_iter, i++)) {
		j = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     &record_iter, &artist_iter, j++)) {
			k = 0;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
							     &track_iter, &record_iter, k++)) {

				gtk_tree_model_get(GTK_TREE_MODEL(music_store), &track_iter, 2, &file, -1);

				if (!strcmp(file, filename)) {

					if (__track_iter) {
						*__track_iter = track_iter;
					}

					g_free(file);

					return 1;
				}

				g_free(file);
			}
		}
	}

	return 0;
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
				   0, &artist_name, -1);

		if (collate(disc->artist.final, artist_name)) {
			g_free(artist_name);
			continue;
		}

		j = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     record_iter, artist_iter, j++)) {
			char * record_name;

			gtk_tree_model_get(GTK_TREE_MODEL(music_store), record_iter,
					   0, &record_name, -1);

			if (!collate(disc->record.final, record_name)) {
				g_free(record_name);
				g_free(artist_name);
				return RECORD_EXISTS;
			}

			g_free(record_name);
		}

		/* create record */
		gtk_tree_store_append(music_store, record_iter, artist_iter);
		gtk_tree_store_set(music_store, record_iter,
				   0, disc->record.final, 1, disc->record.sort,
				   2, "", 3, disc->record.comment, -1);
                if (options.enable_ms_tree_icons) {
                        gtk_tree_store_set(music_store, record_iter, 9, icon_record, -1);
                }

		g_free(artist_name);
		return RECORD_NEW;
	}

	/* create both artist and record */
	gtk_tree_store_append(music_store, artist_iter, store_iter);
	gtk_tree_store_set(music_store, artist_iter,
			   0, disc->artist.final, 1, disc->artist.sort, 2, "", 3, "", -1);
        if (options.enable_ms_tree_icons) {
                gtk_tree_store_set(music_store, artist_iter, 9, icon_artist, -1);
        }

	gtk_tree_store_append(music_store, record_iter, artist_iter);
	gtk_tree_store_set(music_store, record_iter,
			   0, disc->record.final, 1, disc->record.sort,
			   2, "", 3, disc->record.comment, -1);
        if (options.enable_ms_tree_icons) {
                gtk_tree_store_set(music_store, record_iter, 9, icon_record, -1);
        }

	return RECORD_NEW;
}


int
store_get_iter_for_tracklist(GtkTreeIter * store_iter,
			     GtkTreeIter * artist_iter,
			     GtkTreeIter * record_iter, build_disc_t * disc) {
	int i;

	/* check if record already exists */

	if (store_contains_disc(store_iter, artist_iter, record_iter, disc)) {

		char * name;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), artist_iter, 0, &name, -1);

		if (strstr(name, _("Unknown Artist")) == name) {
			gtk_tree_store_set(music_store, artist_iter,
					   0, disc->artist.final,
					   1, disc->artist.sort,
					   -1);
		}

		g_free(name);

		return RECORD_EXISTS;
	}


	/* no such record -- check if the artist of the record exists */

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     artist_iter, store_iter, i++)) {

		char * artist_name;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), artist_iter,
				   0, &artist_name, -1);

		if (collate(disc->artist.final, artist_name)) {
			g_free(artist_name);
			continue;
		}

		/* artist found, create record */

		gtk_tree_store_append(music_store, record_iter, artist_iter);
		gtk_tree_store_set(music_store, record_iter,
				   0, disc->record.final, 1, disc->record.sort,
				   2, "", 3, disc->record.comment, -1);
                if (options.enable_ms_tree_icons) {
                        gtk_tree_store_set(music_store, record_iter, 9, icon_record, -1);
                }

		g_free(artist_name);
		return RECORD_NEW;
	}


	/* no such artist -- create both artist and record */

	gtk_tree_store_append(music_store, artist_iter, store_iter);
	gtk_tree_store_set(music_store, artist_iter,
			   0, disc->artist.final, 1, disc->artist.sort, 2, "", 3, "", -1);
        if (options.enable_ms_tree_icons) {
                gtk_tree_store_set(music_store, artist_iter, 9, icon_artist, -1);
        }

	gtk_tree_store_append(music_store, record_iter, artist_iter);
	gtk_tree_store_set(music_store, record_iter,
			   0, disc->record.final, 1, disc->record.sort,
			   2, "", 3, disc->record.comment, -1);
        if (options.enable_ms_tree_icons) {
                gtk_tree_store_set(music_store, record_iter, 9, icon_record, -1);
        }

	/* start contest for artist name */

	map_put(&artist_name_map, disc->artist.final);

	return RECORD_NEW;
}


int
artist_get_iter_for_tracklist(GtkTreeIter * artist_iter,
			      GtkTreeIter * record_iter, build_disc_t * disc) {

	int i, j;
	int n_tracks;
	GtkTreeIter track_iter;
	char * file;

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

			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &track_iter, 2, &file, -1);

			if (strcmp(file, ptrack->filename)) {
				match = 0;
				g_free(file);
				break;
			}
				
			g_free(file);

			ptrack = ptrack->next;
		}

		if (match) {
			return RECORD_EXISTS;
		}
	}


	/* no such record -- create it */

	gtk_tree_store_append(music_store, record_iter, artist_iter);
	gtk_tree_store_set(music_store, record_iter,
			   0, disc->record.final, 1, disc->record.sort,
			   2, "", 3, disc->record.comment, -1);
        if (options.enable_ms_tree_icons) {
                gtk_tree_store_set(music_store, record_iter, 9, icon_record, -1);
        }

	return RECORD_NEW;
}


void
gen_check_filter_toggled(GtkWidget * widget, gpointer * data) {

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gtk_widget_set_sensitive(GTK_WIDGET(data), state);
}


int
browse_button_clicked(GtkWidget * widget, gpointer data) {

        GtkWidget * dialog;
	const gchar * selected_filename = gtk_entry_get_text(GTK_ENTRY(data));


        dialog = gtk_file_chooser_dialog_new(_("Please select the root directory."),
                                             GTK_WINDOW(browser_window),
					     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                             GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), options.show_hidden);
	} 

        deflicker();

        if (strlen(selected_filename)) {
      		char * locale = g_locale_from_utf8(selected_filename, -1, NULL, NULL, NULL);
		char tmp[MAXLEN];
		tmp[0] = '\0';

		if (locale == NULL) {
			gtk_widget_destroy(dialog);
			return 0;
		}

		normalize_filename(locale, tmp);
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), tmp);
		g_free(locale);
	} else {
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
	}

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);


        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * utf8;

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		utf8 = g_locale_to_utf8(selected_filename, -1, NULL, NULL, NULL);

		if (utf8 == NULL) {
			gtk_widget_destroy(dialog);
			return 0;
		}

		gtk_entry_set_text(GTK_ENTRY(data), utf8);

                strncpy(options.currdir, selected_filename, MAXLEN-1);
		g_free(utf8);
        }

        gtk_widget_destroy(dialog);

	return 0;
}


void
snd_button_test_clicked(GtkWidget * widget, gpointer data) {

	char buf[MAXLEN];
	int err;

	err = file_transform_gui_sync(snd_file_trans_gui);

	if (err) {
		gtk_entry_set_text(GTK_ENTRY(snd_entry_output), "");
		return;
	}

	buf[0] = '\0';
	strncpy(buf, (char *)gtk_entry_get_text(GTK_ENTRY(data)), MAXLEN-1);
	file_transform(buf, snd_file_trans_gui->model);

	regfree(&snd_file_trans_gui->model->compiled);

	gtk_entry_set_text(GTK_ENTRY(snd_entry_output), buf);
}



int
build_type_dialog(void) {

	GtkWidget * dialog;
	GtkWidget * notebook;
	GtkWidget * radio_strict;
	GtkWidget * radio_loose;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * label;

	int ret;

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

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, NULL);

	radio_strict = gtk_radio_button_new_with_label(NULL, _("Directory driven"));
	gtk_widget_set_name(radio_strict, "check_on_notebook");
	gtk_box_pack_start(GTK_BOX(vbox), radio_strict, FALSE, FALSE, 0);

	label = gtk_label_new(_("Follows the directory structure to identify the artists and\n"
				"records. The files are added on a record basis."));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 30);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	radio_loose = gtk_radio_button_new_with_label_from_widget(
			  GTK_RADIO_BUTTON(radio_strict), _("Independent"));
	gtk_widget_set_name(radio_loose, "check_on_notebook");
	gtk_box_pack_start(GTK_BOX(vbox), radio_loose, FALSE, FALSE, 5);

	label = gtk_label_new(_("Recursive search from the root directory for audio files.\n"
				"The files are processed independently, so only metadata\n"
				"and filename transformation are available."));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 30);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	if (build_type == BUILD_TYPE_STRICT) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_strict), TRUE);
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_loose), TRUE);
	}

	gtk_widget_show_all(dialog);

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_strict))) {
			ret = build_type = BUILD_TYPE_STRICT;
		} else {
			ret = build_type = BUILD_TYPE_LOOSE;
		}

	} else {
		ret = BUILD_TYPE_NONE;
	}


        gtk_widget_destroy(dialog);

	return ret;
}


int
build_dialog(int type) {


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
	GtkWidget * snd_entry_input;

        int i, ret;
	char * pfilter = NULL;


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
	gtk_entry_set_text(GTK_ENTRY(gen_root_entry), root);
	gtk_table_attach(GTK_TABLE(table), gen_root_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	gen_browse_button = gui_stock_label_button(_("_Browse..."), GTK_STOCK_OPEN);
	gtk_table_attach(GTK_TABLE(table), gen_browse_button, 2, 3, 0, 1,
			 GTK_FILL, GTK_FILL, 0, 5);
        g_signal_connect(G_OBJECT(gen_browse_button), "clicked", G_CALLBACK(browse_button_clicked),
			 (gpointer)gen_root_entry);


	if (build_type == BUILD_TYPE_STRICT) {
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
		gtk_combo_box_set_active(GTK_COMBO_BOX(gen_depth_combo), artist_dir_depth);
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

	if (excl_pattern[0] == '\0') {
		strcpy(excl_pattern, "*.jpg,*.jpeg,*.png,*.gif,*.pls,*.m3u,*.cue,*.xml,*.html,*.htm,*.txt,*.ini,*.nfo");
	}

        gen_entry_excl = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(gen_entry_excl), MAXLEN-1);
	gtk_entry_set_text(GTK_ENTRY(gen_entry_excl), excl_pattern);
        gtk_box_pack_end(GTK_BOX(gen_excl_frame_hbox), gen_entry_excl, TRUE, TRUE, 0);

	if (excl_enabled) {
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


	if (incl_pattern[0] == '\0') {

#ifdef HAVE_SNDFILE
		for (i = 0; valid_extensions_sndfile[i] != NULL; i++) {
			strcat(incl_pattern, "*.");
			strcat(incl_pattern, valid_extensions_sndfile[i]);
			strcat(incl_pattern, ",");
		}
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
		strcat(incl_pattern, "*.flac,");
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
		strcat(incl_pattern, "*.ogg,");
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPEG
		for (i = 0; valid_extensions_mpeg[i] != NULL; i++) {
			strcat(incl_pattern, "*.");
			strcat(incl_pattern, valid_extensions_mpeg[i]);
			strcat(incl_pattern, ",");
		}
#endif /* HAVE_MPEG */

#ifdef HAVE_SPEEX
		strcat(incl_pattern, "*.spx,");
#endif /* HAVE_SPEEX */

#ifdef HAVE_MPC
		strcat(incl_pattern, "*.mpc,");
#endif /* HAVE_MPC */

#ifdef HAVE_MAC
		strcat(incl_pattern, "*.ape,");
#endif /* HAVE_MAC */

#ifdef HAVE_MOD
		for (i = 0; valid_extensions_mod[i] != NULL; i++) {
			strcat(incl_pattern, "*.");
			strcat(incl_pattern, valid_extensions_mod[i]);
			strcat(incl_pattern, ",");
		}
#endif /* HAVE_MOD */

#ifdef HAVE_WAVPACK
		strcat(incl_pattern, "*.wv,");
#endif /* HAVE_WAVPACK */

		if ((pfilter = strrchr(incl_pattern, ',')) != NULL) {
			*pfilter = '\0';
		}
	}

	gtk_entry_set_text(GTK_ENTRY(gen_entry_incl), incl_pattern);
        gtk_box_pack_end(GTK_BOX(gen_incl_frame_hbox), gen_entry_incl, TRUE, TRUE, 0);

	if (incl_enabled) {
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

	if (reset_existing_data) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gen_check_reset_data), TRUE);
	}


	/* Artist */
	
        art_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(art_vbox), 5);
	label = gtk_label_new(_("Artist"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), art_vbox, label);


	if (data_src_artist == NULL) {
		data_src_artist = data_src_new();
	}

	if (type != BUILD_TYPE_STRICT) {
		data_src_artist->cddb_mask = 0;
		for (i = 0; i < 3; i++) {
			if (data_src_artist->type[i] == DATA_SRC_CDDB) {
				data_src_artist->enabled[DATA_SRC_CDDB] = 0;
				break;
			}
		}
	} else {
		data_src_artist->cddb_mask = 1;
	}

	art_src_gui = data_src_gui_new(data_src_artist, art_vbox);

	if (file_transform_artist == NULL) {
		file_transform_artist = file_transform_new();
	}
	art_file_trans_gui = file_transform_gui_new(file_transform_artist, art_vbox);

	if (capitalize_artist == NULL) {
		capitalize_artist = capitalize_new();
	}
	art_cap_gui = capitalize_gui_new(capitalize_artist, art_vbox);


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
	gtk_combo_box_set_active(GTK_COMBO_BOX(art_sort_combo), artist_sort_by);


	/* Record */
	
        rec_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(rec_vbox), 5);
	label = gtk_label_new(_("Record"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), rec_vbox, label);


	if (data_src_record == NULL) {
		data_src_record = data_src_new();
	}

	if (type != BUILD_TYPE_STRICT) {
		data_src_record->cddb_mask = 0;
		for (i = 0; i < 3; i++) {
			if (data_src_record->type[i] == DATA_SRC_CDDB) {
				data_src_record->enabled[DATA_SRC_CDDB] = 0;
				break;
			}
		}
	} else {
		data_src_record->cddb_mask = 1;
	}

	rec_src_gui = data_src_gui_new(data_src_record, rec_vbox);

	if (file_transform_record == NULL) {
		file_transform_record = file_transform_new();
	}
	rec_file_trans_gui = file_transform_gui_new(file_transform_record, rec_vbox);

	if (capitalize_record == NULL) {
		capitalize_record = capitalize_new();
	}
	rec_cap_gui = capitalize_gui_new(capitalize_record, rec_vbox);


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
	gtk_combo_box_set_active(GTK_COMBO_BOX(rec_sort_combo), record_sort_by);


	rec_check_add_year =
		gtk_check_button_new_with_label(_("Add year to the comments of new records"));
	gtk_widget_set_name(rec_check_add_year, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(rec_vbox), rec_check_add_year, FALSE, FALSE, 0);

	if (rec_add_year_to_comment) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rec_check_add_year), TRUE);
	}


	/* Track */
	
        trk_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(trk_vbox), 5);
	label = gtk_label_new(_("Track"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), trk_vbox, label);


	if (data_src_track == NULL) {
		data_src_track = data_src_new();
	}

	if (type != BUILD_TYPE_STRICT) {
		data_src_track->cddb_mask = 0;
		for (i = 0; i < 3; i++) {
			if (data_src_track->type[i] == DATA_SRC_CDDB) {
				data_src_track->enabled[DATA_SRC_CDDB] = 0;
				break;
			}
		}
	} else {
		data_src_track->cddb_mask = 1;
	}

	trk_src_gui = data_src_gui_new(data_src_track, trk_vbox);

	if (file_transform_track == NULL) {
		file_transform_track = file_transform_new();
	}
	trk_file_trans_gui = file_transform_gui_new(file_transform_track, trk_vbox);

	if (capitalize_track == NULL) {
		capitalize_track = capitalize_new();
	}
	trk_cap_gui = capitalize_gui_new(capitalize_track, trk_vbox);


	trk_check_rva =
		gtk_check_button_new_with_label(_("Import Replaygain tag as manual RVA"));
	gtk_widget_set_name(trk_check_rva, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(trk_vbox), trk_check_rva, FALSE, FALSE, 0);

	if (trk_rva_enabled) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(trk_check_rva), TRUE);
	}

	trk_check_comment =
		gtk_check_button_new_with_label(_("Import Comment tag"));
	gtk_widget_set_name(trk_check_comment, "check_on_notebook");
        gtk_box_pack_start(GTK_BOX(trk_vbox), trk_check_comment, FALSE, FALSE, 0);

	if (trk_comment_enabled) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(trk_check_comment), TRUE);
	}


	/* Sandbox */

        snd_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(snd_vbox), 5);
	label = gtk_label_new(_("Sandbox"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), snd_vbox, label);

	if (file_transform_sandbox == NULL) {
		file_transform_sandbox = file_transform_new();
	}

	snd_file_trans_gui = file_transform_gui_new(file_transform_sandbox, snd_vbox);

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

	snd_entry_input = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), snd_entry_input, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

	snd_button_test = gtk_button_new_with_label(_("Test"));
	gtk_table_attach(GTK_TABLE(table), snd_button_test, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 0, 5);
        g_signal_connect(G_OBJECT(snd_button_test), "clicked", G_CALLBACK(snd_button_test_clicked),
			 (gpointer)snd_entry_input);

	snd_entry_output = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), snd_entry_output, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);


	/* run dialog */

	gtk_widget_show_all(dialog);
	gtk_widget_hide(art_file_trans_gui->label_error);
	gtk_widget_hide(rec_file_trans_gui->label_error);
	gtk_widget_hide(trk_file_trans_gui->label_error);
	gtk_widget_hide(snd_file_trans_gui->label_error);
	gtk_widget_grab_focus(gen_root_entry);

 display:

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		
		char * proot = g_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(gen_root_entry)), -1, NULL, NULL, NULL);

		root[0] = '\0';

		if (proot == NULL || proot[0] == '\0') {
			gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
			gtk_widget_grab_focus(gen_root_entry);
			goto display;
		}

		normalize_filename(proot, root);
		g_free(proot);


		if (build_type == BUILD_TYPE_STRICT) {
			artist_dir_depth = gtk_combo_box_get_active(GTK_COMBO_BOX(gen_depth_combo));
		}

		reset_existing_data = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gen_check_reset_data));
		rec_add_year_to_comment = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rec_check_add_year));

		trk_rva_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(trk_check_rva));
		trk_comment_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(trk_check_comment));


		strncpy(excl_pattern, gtk_entry_get_text(GTK_ENTRY(gen_entry_excl)), MAXLEN-1);
		strncpy(incl_pattern, gtk_entry_get_text(GTK_ENTRY(gen_entry_incl)), MAXLEN-1);

		excl_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gen_check_excl));
		incl_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gen_check_incl));

		if (excl_enabled) {
			excl_patternv =
			    g_strsplit(gtk_entry_get_text(GTK_ENTRY(gen_entry_excl)), ",", 0);
		}

		if (incl_enabled) {
			incl_patternv =
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


		cddb_enabled = 0;
		for (i = 0; i < 3; i++) {
			if ((data_src_artist->type[i] == DATA_SRC_CDDB && data_src_artist->enabled[i]) ||
			    (data_src_record->type[i] == DATA_SRC_CDDB && data_src_record->enabled[i]) ||
			    (data_src_track->type[i] == DATA_SRC_CDDB && data_src_track->enabled[i])) {

				cddb_enabled = 1;
				break;
			}
		}

		meta_enabled = 0;
		for (i = 0; i < 3; i++) {
			if ((data_src_artist->type[i] == DATA_SRC_META && data_src_artist->enabled[i]) ||
			    (data_src_record->type[i] == DATA_SRC_META && data_src_record->enabled[i]) ||
			    (data_src_track->type[i] == DATA_SRC_META && data_src_track->enabled[i])) {

				meta_enabled = 1;
				break;
			}
		}

		ret = 1;

        } else {
                ret = 0;
        }

        gtk_widget_destroy(dialog);

	free(art_file_trans_gui);
	free(rec_file_trans_gui);
	free(trk_file_trans_gui);
	free(snd_file_trans_gui);

	free(art_cap_gui);
	free(rec_cap_gui);
	free(trk_cap_gui);

	free(art_src_gui);
	free(rec_src_gui);
	free(trk_src_gui);

        return ret;
}


void
prog_window_close(GtkWidget * widget, gpointer data) {

	build_cancelled = 1;

	if (build_prog_window) {
		gtk_widget_destroy(build_prog_window);
		build_prog_window = NULL;
	}
}

void
cancel_build(GtkWidget * widget, gpointer data) {

	prog_window_close(NULL, NULL);
}

gboolean
finish_build(gpointer data) {

	if (build_prog_window) {
		gtk_widget_destroy(build_prog_window);
		build_prog_window = NULL;
	}

	g_strfreev(capitalize_artist->pre_stringv);
	capitalize_artist->pre_stringv = NULL;

	g_strfreev(capitalize_record->pre_stringv);
	capitalize_record->pre_stringv = NULL;

	g_strfreev(capitalize_track->pre_stringv);
	capitalize_track->pre_stringv = NULL;

	g_strfreev(excl_patternv);
	excl_patternv = NULL;

	g_strfreev(incl_patternv);
	incl_patternv = NULL;

	regfree(&file_transform_artist->compiled);
	regfree(&file_transform_record->compiled);
	regfree(&file_transform_track->compiled);

	build_thread_state = BUILD_THREAD_FREE;

	return FALSE;
}


void
progress_window(void) {

	GtkWidget * table;
	GtkWidget * label;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * hbuttonbox;
	GtkWidget * hseparator;


	build_prog_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(build_prog_window), _("Building store from filesystem"));
        gtk_window_set_position(GTK_WINDOW(build_prog_window), GTK_WIN_POS_CENTER);
        gtk_window_resize(GTK_WINDOW(build_prog_window), 430, 110);
        g_signal_connect(G_OBJECT(build_prog_window), "delete_event",
                         G_CALLBACK(prog_window_close), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(build_prog_window), 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(build_prog_window), vbox);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

        hbox = gtk_hbox_new(FALSE, 0);
	if (build_type == BUILD_TYPE_STRICT) {
		label = gtk_label_new(_("Directory:"));
	} else {
		label = gtk_label_new(_("File:"));
	}
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 5, 5);

        prog_file_entry = gtk_entry_new();
        gtk_editable_set_editable(GTK_EDITABLE(prog_file_entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), prog_file_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Action:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	prog_action_label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), prog_action_label, FALSE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 5);

        hseparator = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, TRUE, 5);

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        prog_cancel_button = gui_stock_label_button (_("Abort"), GTK_STOCK_CANCEL); 
        g_signal_connect(prog_cancel_button, "clicked", G_CALLBACK(cancel_build), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), prog_cancel_button);   

        gtk_widget_grab_focus(prog_cancel_button);

        gtk_widget_show_all(build_prog_window);
}

gboolean
set_prog_file_entry(gpointer data) {

	if (build_prog_window) {

		char * utf8 = g_filename_display_name((char *)data);
		gtk_entry_set_text(GTK_ENTRY(prog_file_entry), utf8);
		gtk_widget_grab_focus(prog_cancel_button);
		g_free(utf8);
	}

	return FALSE;
}

gboolean
set_prog_action_label(gpointer data) {

	if (build_prog_window) {
		gtk_label_set_text(GTK_LABEL(prog_action_label), (char *)data);
	}

	return FALSE;
}

gboolean
set_prog_all(gpointer file, gpointer label) {

	set_prog_file_entry(file);
	set_prog_action_label(label);
	return FALSE;
}


int
is_all_wspace(char * str) {

	int i;

	for (i = 0; str[i]; i++) {
		if (str[i] != ' ' && str[i] != '\t') {
			return 0;
		}
	}

	return 1;
}

int
is_valid_year(long y) {

	return y > YEAR_MIN && y < YEAR_MAX;
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
		    (*(p + 2) == ' ' || *(p + 2) == '_' || *(p + 2) == '-')) {
		
			p += 3;

			while (*p && (*p == ' ' || *p == '_' || *p == '-')) {
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

	chars = (gunichar *)malloc(6 * sizeof(gunichar));
	chars[0] = g_utf8_get_char(" ");
	chars[1] = g_utf8_get_char("\t");
	chars[2] = g_utf8_get_char("(");
	chars[3] = g_utf8_get_char("[");
	chars[4] = g_utf8_get_char("/");
	chars[5] = g_utf8_get_char("\"");

	for (i = 0; i < 6; i++) {
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
process_meta(build_disc_t * disc) {

	build_track_t * ptrack;
	metadata * meta = meta_new();

	map_t * map_artist = NULL;
	map_t * map_record = NULL;
	map_t * map_year = NULL;

	char tmp[MAXLEN];

	for (ptrack = disc->tracks; ptrack; ptrack = ptrack->next) {

		meta = meta_new();

		if (meta_read(meta, ptrack->filename)) {

			if (meta_get_artist(meta, tmp) && !is_all_wspace(tmp)) {
				map_put(&map_artist, tmp);
			}

			if (meta_get_record(meta, tmp) && !is_all_wspace(tmp)) {
				map_put(&map_record, tmp);
			}

			if (meta_get_title(meta, tmp) && !is_all_wspace(tmp)) {
				strncpy(ptrack->name[DATA_SRC_META], tmp, MAXLEN-1);
			}

			if (disc->record.year[0] == '\0' && meta_get_year(meta, tmp)) {
				long y = strtol(tmp, NULL, 10);
				if (is_valid_year(y)) {
					map_put(&map_year, tmp);
				}
			}

			if (trk_rva_enabled) {
				meta_get_rva(meta, &ptrack->rva);
			}

			if (trk_comment_enabled && meta_get_comment(meta, tmp) && !is_all_wspace(tmp)) {
				strncpy(ptrack->comment, tmp, MAXLEN-1);
			}
		}

		meta_free(meta);
	}

	if (map_artist) {
		char * max = map_get_max(map_artist);

		if (max) {
			strncpy(disc->artist.name[DATA_SRC_META], max, MAXLEN-1);
		}
	}

	if (map_record) {
		char * max = map_get_max(map_record);

		if (max) {
			strncpy(disc->record.name[DATA_SRC_META], max, MAXLEN-1);
		}
	}

	if (map_year) {
		char * max = map_get_max(map_year);

		if (max) {
			strncpy(disc->record.year, max, MAXLEN-1);
		}
	}

	map_free(map_artist);
	map_free(map_record);
	map_free(map_year);
}


void
add_new_track(GtkTreeIter * record_iter, build_track_t * ptrack, int i) {

	GtkTreeIter iter;
	char sort_name[3];

	if (i == 0) {
		i = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), record_iter) + 1;
	}

	snprintf(sort_name, 3, "%02d", i);

	gtk_tree_store_append(music_store, &iter, record_iter);

	if (ptrack->rva > 0.1f) { /* rva unmeasured */
		gtk_tree_store_set(music_store, &iter,
				   0, ptrack->final,
				   1, sort_name,
				   2, ptrack->filename,
				   3, ptrack->comment,
				   4, ptrack->duration,
				   5, 1.0f,
				   6, 0.0f,
				   7, -1.0f,
				   -1);
	} else {
		gtk_tree_store_set(music_store, &iter,
				   0, ptrack->final,
				   1, sort_name,
				   2, ptrack->filename,
				   3, ptrack->comment,
				   4, ptrack->duration,
				   5, 1.0f,
				   6, ptrack->rva,
				   7, 1.0f,
				   -1);
	}

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &iter, 9, icon_track, -1);
	}
}


gboolean
write_record_to_store(gpointer data) {

	build_disc_t * disc = (build_disc_t *)data;
	build_track_t * ptrack;
	int result = 0;
	int i;

	GtkTreeIter record_iter;


	if (artist_iter_is_set) {
		result = artist_get_iter_for_tracklist(&artist_iter, &record_iter, disc);
	} else {
		result = store_get_iter_for_tracklist(&store_iter,
						      &artist_iter,
						      &record_iter,
						      disc);
		artist_iter_is_set = 1;
	}

	if (artist_name_map != NULL) {

		char * max = NULL;
		map_put(&artist_name_map, disc->artist.final);
		max = map_get_max(artist_name_map);

		gtk_tree_store_set(music_store, &artist_iter, 0, max, -1);

		if (artist_sort_by == SORT_NAME) {
			gtk_tree_store_set(music_store, &artist_iter, 1, max, -1);
		}

		if (artist_sort_by == SORT_NAME_LOW) {
			char * utf8 = g_utf8_strdown(disc->artist.final, -1);
			gtk_tree_store_set(music_store, &artist_iter, 1, utf8, -1);
			g_free(utf8);
		}
	}

	if (result == RECORD_NEW) {
		for (i = 0, ptrack = disc->tracks; ptrack; i++, ptrack = ptrack->next) {
			add_new_track(&record_iter, ptrack, i + 1);
		}
	}

	if (result == RECORD_EXISTS) {

		GtkTreeIter iter;
		char * name;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &record_iter, 0, &name, -1);

		if (strstr(name, _("Unknown Record")) == name) {
			gtk_tree_store_set(music_store, &record_iter,
					   0, disc->record.final,
					   1, disc->record.sort,
					   -1);
		}

		g_free(name);

		i = 0;
		ptrack = disc->tracks;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     &iter, &record_iter, i++)) {

			char * comment;

			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, 3, &comment, -1);

			if (comment[0] == '\0') {
				gtk_tree_store_set(music_store, &iter, 3, ptrack->comment, -1);
			}

			g_free(comment);

			if (ptrack->rva > 0.1f) { /* rva unmeasured */
				gtk_tree_store_set(music_store, &iter,
						   0, ptrack->final,
						   4, ptrack->duration,
						   -1);
			} else {
				gtk_tree_store_set(music_store, &iter,
						   0, ptrack->final,
						   4, ptrack->duration,
						   6, ptrack->rva,
						   7, 1.0f,
						   -1);
			}

			ptrack = ptrack->next;
		}
	}

	music_store_mark_changed(&store_iter);

	write_data_locked = 0;

	return FALSE;
}


gboolean
write_track_to_store(gpointer data) {

	build_disc_t * disc = (build_disc_t *)data;
	int result;

	GtkTreeIter artist_iter;
	GtkTreeIter record_iter;


	if (disc->flag) { /* track is present, reset data */

		char * comment;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &disc->iter, 3, &comment, -1);

		if (comment[0] == '\0') {
			gtk_tree_store_set(music_store, &disc->iter, 3, disc->tracks->comment, -1);
		}

		g_free(comment);

		if (disc->tracks->rva > 0.1f) { /* rva unmeasured */
			gtk_tree_store_set(music_store, &disc->iter,
					   0, disc->tracks->final,
					   4, disc->tracks->duration,
					   -1);
		} else {
			gtk_tree_store_set(music_store, &disc->iter,
					   0, disc->tracks->final,
					   4, disc->tracks->duration,
					   6, disc->tracks->rva,
					   7, 1.0f,
					   -1);
		}

	} else {

		result = store_get_iter_for_artist_and_record(&store_iter,
							      &artist_iter,
							      &record_iter,
							      disc);

		add_new_track(&record_iter, disc->tracks, 0 /* append */);
	}

	music_store_mark_changed(&store_iter);

	write_data_locked = 0;

	return FALSE;
}


static int
filter(const struct dirent * de) {

	return de->d_name[0] != '.';
}


int
is_dir(char * name) {

	struct stat st_file;

	if (stat(name, &st_file) == -1) {
		return 0;
	}

	return S_ISDIR(st_file.st_mode);
}


int
filter_excl_incl(char * basename) {

	char * utf8 = g_filename_display_name(basename);

	if (excl_enabled) {
		int k;
		int match = 0;
		for (k = 0; excl_patternv[k]; k++) {

			if (*(excl_patternv[k]) == '\0') {
				continue;
			}

			if (fnmatch(excl_patternv[k], utf8, FNM_CASEFOLD) == 0) {
				match = 1;
				break;
			}
		}

		if (match) {
			g_free(utf8);
			return 0;
		}
	}

	if (incl_enabled) {
		int k;
		int match = 0;
		for (k = 0; incl_patternv[k]; k++) {
			
			if (*(incl_patternv[k]) == '\0') {
				continue;
			}
			
			if (fnmatch(incl_patternv[k], utf8, FNM_CASEFOLD) == 0) {
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
get_file_list(char * dir_record, build_disc_t * disc, int open) {

	int i, n;
	struct dirent ** ent_track;
	char basename[MAXLEN];
	char filename[MAXLEN];

	build_track_t * last_track = NULL;
	float duration = 0.0f;;

	char * utf8;


	n = scandir(dir_record, &ent_track, filter, alphasort);
	for (i = 0; i < n; i++) {

		strncpy(basename, ent_track[i]->d_name, MAXLEN-1);
		snprintf(filename, MAXLEN-1, "%s/%s", dir_record, ent_track[i]->d_name);

		if (is_dir(filename) || !filter_excl_incl(basename)) {
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
process_filename(build_disc_t * disc) {

	int i, j;
	char * utf8;
	build_track_t * ptrack;


	/* Artist */

	for (i = 0; i < 3; i++) {

		if (data_src_artist->enabled[i]) {

			if (data_src_artist->type[i] == DATA_SRC_FILE) {
				strncpy(disc->artist.name[DATA_SRC_FILE],
					disc->artist.d_name, MAXLEN-1);
			}

			if (disc->artist.name[data_src_artist->type[i]][0] != '\0') {
				strncpy(disc->artist.final,
					disc->artist.name[data_src_artist->type[i]], MAXLEN-1);
				break;
			}
		}
	}

	if (i == 3) {
		if (build_type == BUILD_TYPE_STRICT) {
			char tmp[MAXLEN];
			snprintf(tmp, MAXLEN-1, "%s (%s)",
				 _("Unknown Artist"),
				 disc->artist.d_name);
			strncpy(disc->artist.final, tmp, MAXLEN-1);
		} else {
			strncpy(disc->artist.final, _("Unknown Artist"), MAXLEN-1);
		}
	} else {
		file_transform(disc->artist.final, file_transform_artist);

		if (capitalize_artist->enabled) {
			capitalize(disc->artist.final, capitalize_artist);
		}
	}


	/* Record */

	for (i = 0; i < 3; i++) {

		if (data_src_record->enabled[i]) {

			if (data_src_record->type[i] == DATA_SRC_FILE) {
				strncpy(disc->record.name[DATA_SRC_FILE],
					disc->record.d_name, MAXLEN-1);
			}

			if(disc->record.name[data_src_record->type[i]][0] != '\0') {
				strncpy(disc->record.final,
					disc->record.name[data_src_record->type[i]], MAXLEN-1);
				break;
			}
		}
	}

	if (i == 3) {
		if (build_type == BUILD_TYPE_STRICT) {
			char tmp[MAXLEN];
			snprintf(tmp, MAXLEN-1, "%s (%s)",
				 _("Unknown Record"),
				 disc->record.d_name);
			strncpy(disc->record.final, tmp, MAXLEN-1);
		} else {
			strncpy(disc->record.final, _("Unknown Record"), MAXLEN-1);
		}
	} else {
		file_transform(disc->record.final, file_transform_record);

		if (capitalize_record->enabled) {
			capitalize(disc->record.final, capitalize_record);
		}
	}


	/* Tracks */

	for (i = 0, ptrack = disc->tracks; ptrack; i++, ptrack = ptrack->next) {

		strncpy(ptrack->name[DATA_SRC_FILE], ptrack->d_name, MAXLEN-1);

		for (j = 0; j < 3; j++) {

			if (data_src_track->enabled[j]) {

				if (data_src_track->type[i] == DATA_SRC_FILE) {
					strncpy(ptrack->name[DATA_SRC_FILE],
						ptrack->d_name, MAXLEN-1);
				}

				if (ptrack->name[data_src_track->type[j]][0] != '\0') {
					strncpy(ptrack->final,
						ptrack->name[data_src_track->type[j]], MAXLEN-1);
					break;
				}
			}
		}

		if (j == 3) {
			strncpy(ptrack->final, ptrack->d_name, MAXLEN-1);
		} else {
			file_transform(ptrack->final, file_transform_track);

			if (capitalize_track->enabled) {
				capitalize(ptrack->final, capitalize_track);
			}
		}
	}


	/* Artist and Record sort name */

	switch (artist_sort_by) {
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


	switch (record_sort_by) {
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
}


void
process_record(char * dir_record, char * artist_d_name, char * record_d_name) {

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


	g_idle_add(set_prog_action_label, (gpointer) _("Scanning files"));

	if (!reset_existing_data) {
		get_file_list(dir_record, disc, 0/* do not open*/);

		if (disc->tracks == NULL) {
			goto finish;
		}

		if (store_contains_disc(&store_iter, NULL, NULL, disc)) {
			goto finish;
		}
	}

	get_file_list(dir_record, disc, 1/* try to open*/);

	if (disc->tracks == NULL) {
		goto finish;
	}

	if (!reset_existing_data) {
		if (store_contains_disc(&store_iter, NULL, NULL, disc)) {
			goto finish;
		}
	}


	if (meta_enabled) {
		g_idle_add(set_prog_action_label, (gpointer) _("Processing metadata"));
		process_meta(disc);
	}


#ifdef HAVE_CDDB
	if (cddb_enabled) {
		g_idle_add(set_prog_action_label, (gpointer) _("CDDB lookup"));
		cddb_get_batch(disc);
	}
#endif /* HAVE_CDDB */


	g_idle_add(set_prog_action_label, (gpointer) _("Name transformation"));
	process_filename(disc);


	if (rec_add_year_to_comment) {
		strncpy(disc->record.comment, disc->record.year, MAXLEN-1);
	}


	/* wait for the gtk thread to update music_store */

	write_data_locked = 1;

	g_idle_add_full(G_PRIORITY_HIGH_IDLE,
			write_record_to_store,
			(gpointer)disc,
			NULL);

	while (write_data_locked) {
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
process_track(char * filename, char * d_name, float duration) {

	struct timespec req_time;
	struct timespec rem_time;

	GtkTreeIter iter_track;

	build_disc_t * disc = NULL;

	char * utf8;


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
		strncpy(disc->tracks->filename, filename, MAXLEN-1);

		utf8 = g_filename_display_name(d_name);
		strncpy(disc->tracks->d_name, utf8, MAXLEN-1);
		strncpy(disc->artist.d_name, utf8, MAXLEN-1);
		strncpy(disc->record.d_name, utf8, MAXLEN-1);
		g_free(utf8);

		disc->tracks->duration = duration;
		disc->tracks->rva = 1.0f;
	}


	disc->flag = store_contains_track(&store_iter, &iter_track, filename);
	disc->iter = iter_track;

	if (!reset_existing_data && disc->flag) {
		goto finish;
	}


	if (meta_enabled) {
		g_idle_add(set_prog_action_label, (gpointer) _("Processing metadata"));
		process_meta(disc);
	}


	g_idle_add(set_prog_action_label, (gpointer) _("Name transformation"));
	process_filename(disc);


	if (rec_add_year_to_comment) {
		strncpy(disc->record.comment, disc->record.year, MAXLEN-1);
	}


	/* wait for the gtk thread to update music_store */

	write_data_locked = 1;

	g_idle_add_full(G_PRIORITY_HIGH_IDLE,
			write_track_to_store,
			(gpointer)disc,
			NULL);

	while (write_data_locked) {
		nanosleep(&req_time, &rem_time);
	}

 finish:

	free(disc->tracks);
	free(disc);
}


void
scan_artist_record(char * dir_artist, char * name_artist, int depth) {


	int i, n;
	struct dirent ** ent_record;
	char dir_record[MAXLEN];


	artist_iter_is_set = 0;

	n = scandir(dir_artist, &ent_record, filter, alphasort);
	for (i = 0; i < n; i++) {

		if (build_cancelled) {
			break;
		}

		snprintf(dir_record, MAXLEN-1, "%s/%s", dir_artist, ent_record[i]->d_name);

		if (!is_dir(dir_record)) {
			free(ent_record[i]);
			continue;
		}

		g_idle_add(set_prog_file_entry, (gpointer)dir_record);

		if (depth == 0) {

			artist_iter_is_set = 0;

			process_record(dir_record,
				       ent_record[i]->d_name,
				       ent_record[i]->d_name);

			if (artist_name_map != NULL) {
				map_free(artist_name_map);
				artist_name_map = NULL;
			}

		} else if (depth == 1) {

			process_record(dir_record,
			               name_artist,
				       ent_record[i]->d_name);
		} else {
			scan_artist_record(dir_record, ent_record[i]->d_name, depth - 1);
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

	if (artist_name_map != NULL) {
		map_free(artist_name_map);
		artist_name_map = NULL;
	}
}


void
scan_recursively(char * dir) {


	int i, n;
	struct dirent ** ent;
	char path[MAXLEN];

	n = scandir(dir, &ent, filter, alphasort);
	for (i = 0; i < n; i++) {

		if (build_cancelled) {
			break;
		}

		snprintf(path, MAXLEN-1, "%s/%s", dir, ent[i]->d_name);

		if (is_dir(path)) {
			scan_recursively(path);
		} else {
			float d;

			g_idle_add(set_prog_file_entry, (gpointer) path);
			g_idle_add(set_prog_action_label, (gpointer) _("Reading file"));

			if (filter_excl_incl(path) && (d = get_file_duration(path)) > 0.0f) {
				process_track(path, ent[i]->d_name, d);
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


void *
build_thread_strict(void * arg) {


	AQUALUNG_THREAD_DETACH()

	scan_artist_record(root, NULL, (artist_dir_depth == 0) ? 0 : (artist_dir_depth + 1));

	g_idle_add(finish_build, NULL);

	return NULL;
}


void *
build_thread_loose(void * arg) {


	AQUALUNG_THREAD_DETACH()

	scan_recursively(root);

	g_idle_add(finish_build, NULL);

	return NULL;
}


void
build_store(GtkTreeIter iter) {

	build_thread_state = BUILD_THREAD_BUSY;

	store_iter = iter;
	build_cancelled = 0;

	switch (build_type_dialog()) {

	case BUILD_TYPE_STRICT:

		if (build_dialog(BUILD_TYPE_STRICT)) {
			progress_window();
			AQUALUNG_THREAD_CREATE(build_thread_id, NULL, build_thread_strict, NULL)
		} else {
			build_thread_state = BUILD_THREAD_FREE;
		}

		break;

	case BUILD_TYPE_LOOSE:

		if (build_dialog(BUILD_TYPE_LOOSE)) {
			progress_window();
			AQUALUNG_THREAD_CREATE(build_thread_id, NULL, build_thread_loose, NULL)
		} else {
			build_thread_state = BUILD_THREAD_FREE;
		}

		break;

	default:

		build_thread_state = BUILD_THREAD_FREE;
		break;
	}
}


int
build_thread_test(int state) {

	return build_thread_state == state;
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

