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
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "common.h"
#include "utils.h"
#include "utils_gui.h"
#include "core.h"
#include "httpc.h"
#include "rb.h"
#include "cdda.h"
#include "gui_main.h"
#include "music_browser.h"
#include "file_info.h"
#include "decoder/dec_mod.h"
#include "decoder/file_decoder.h"
#include "metadata_api.h"
#include "volume.h"
#include "options.h"
#include "i18n.h"
#include "search_playlist.h"
#include "playlist.h"
#include "ifp_device.h"
#include "export.h"


extern options_t options;

extern char pl_color_active[14];
extern char pl_color_inactive[14];

extern GtkTooltips * aqualung_tooltips;

extern PangoFontDescription *fd_playlist;
extern PangoFontDescription *fd_statusbar;

extern GtkWidget * main_window;
extern GtkWidget * info_window;

extern GtkTreeSelection * music_select;

extern gulong play_id;
extern gulong pause_id;
extern GtkWidget * play_button;
extern GtkWidget * pause_button;

extern int is_file_loaded;
extern int is_paused;
extern int allow_seeks;

extern char current_file[MAXLEN];

extern rb_t * rb_gui2disk;

extern GtkWidget * playlist_toggle;


GtkWidget * playlist_window;
GtkWidget * playlist_color_indicator;

gint playlist_scroll_up_tag = -1;
gint playlist_scroll_dn_tag = -1;

GtkWidget * statusbar_total;
GtkWidget * statusbar_selected;
GtkWidget * statusbar_total_label;
GtkWidget * statusbar_selected_label;


/* popup menus */
GtkWidget * add_menu;
GtkWidget * add__files;
GtkWidget * add__dir;
GtkWidget * add__url;
GtkWidget * sel_menu;
GtkWidget * sel__none;
GtkWidget * sel__all;
GtkWidget * sel__inv;
GtkWidget * rem_menu;
GtkWidget * cut__sel;
GtkWidget * rem__all;
GtkWidget * rem__dead;
GtkWidget * rem__sel;
GtkWidget * plist_menu;
GtkWidget * plist__tab_new;
GtkWidget * plist__save;
GtkWidget * plist__save_all;
GtkWidget * plist__load;
GtkWidget * plist__enqueue;
GtkWidget * plist__load_tab;
GtkWidget * plist__rva;
GtkWidget * plist__rva_menu;
GtkWidget * plist__rva_separate;
GtkWidget * plist__rva_average;
GtkWidget * plist__reread_file_meta;
GtkWidget * plist__fileinfo;
GtkWidget * plist__search;
#ifdef HAVE_IFP
GtkWidget * plist__send_songs_to_iriver;
#endif /* HAVE_IFP */
#ifdef HAVE_EXPORT
GtkWidget * plist__export;
#endif /* HAVE_EXPORT */

gchar command[RB_CONTROL_SIZE];

gchar fileinfo_name[MAXLEN];
gchar fileinfo_file[MAXLEN];

typedef struct {
	GtkTreeStore * store;
	int has_album_node;
} clipboard_t;

clipboard_t * clipboard;

enum {
	PLAYLIST_INVALID,
	PLAYLIST_XML_SINGLE,
	PLAYLIST_XML_MULTI,
	PLAYLIST_M3U,
	PLAYLIST_PLS
};

void create_playlist_gui(playlist_t * pl);

void playlist_notebook_prev_page(void);
void playlist_notebook_next_page(void);
void playlist_tab_close(GtkButton * button, gpointer data);
void playlist_tab_close_undo(void);
void playlist_progress_bar_hide(playlist_t * pl);

playlist_transfer_t * playlist_transfer_get(int mode, char * tab_name, int start_playback);


playlist_data_t * playlist_filemeta_get(char * filename);

void playlist_unlink_files(playlist_t * pl);
void set_cursor_in_playlist(playlist_t * pl, GtkTreeIter *iter, gboolean scroll);
void select_active_position_in_playlist(playlist_t * pl);

void delayed_playlist_rearrange(playlist_t * pl);
void playlist_selection_changed(playlist_t * pl);
void playlist_selection_changed_cb(GtkTreeSelection * select, gpointer data);

void sel__all_cb(gpointer data);
void sel__none_cb(gpointer data);
void rem__all_cb(gpointer data);
void rem__sel_cb(gpointer data);
void cut__sel_cb(gpointer data);
void plist__search_cb(gpointer data);
void rem_all(playlist_t * pl);
void add_files(GtkWidget * widget, gpointer data);

void tab__rename_cb(gpointer data);

void add_directory(GtkWidget * widget, gpointer data);
void init_plist_menu(GtkWidget *append_menu);
void show_active_position_in_playlist(playlist_t * pl);
void show_active_position_in_playlist_toggle(playlist_t * pl);
void expand_collapse_album_node(playlist_t * pl);

void playlist_save(playlist_t * pl, char * filename);

int playlist_get_type(char * filename);
void playlist_load_xml_multi(char * filename, int start_playback);
void playlist_load_xml_single(char * filename, playlist_transfer_t * pt);
void * playlist_load_m3u_thread(void * arg);
void * playlist_load_pls_thread(void * arg);


GtkWidget * playlist_notebook;

GList * playlists;
GList * playlists_closed;


playlist_data_t *
playlist_data_new() {

	playlist_data_t * data;

	if ((data = (playlist_data_t *)calloc(1, sizeof(playlist_data_t))) == NULL) {
		fprintf(stderr, "playlist_data_new(): calloc error\n");
		return NULL;
	}

	data->voladj = 0.0f;
	data->duration = 0.0f;
	data->size = 0;
	data->flags = 0;

	return data;
}

void
playlist_data_copy_noalloc(playlist_data_t * dest, playlist_data_t * src) {

	if (src->artist) {
		free_strdup(&dest->artist, src->artist);
	}
	if (src->album) {
		free_strdup(&dest->album, src->album);
	}
	if (src->title) {
		free_strdup(&dest->title, src->title);
	}
	if (src->file) {
		free_strdup(&dest->file, src->file);
	}

	dest->voladj = src->voladj;
	dest->duration = src->duration;
	dest->size = src->size;
	dest->flags = src->flags;
}

playlist_data_t *
playlist_data_copy(playlist_data_t * src) {

	playlist_data_t * dest;

	if ((dest = playlist_data_new()) == NULL) {
		return NULL;
	}

	playlist_data_copy_noalloc(dest, src);

	return dest;
}

void
playlist_data_free(playlist_data_t * data) {

	if (data->artist) {
		free(data->artist);
	}
	if (data->album) {
		free(data->album);
	}
	if (data->title) {
		free(data->title);
	}
	if (data->file) {
		free(data->file);
	}
	free(data);
}

gboolean
playlist_remove_track(GtkTreeStore * store, GtkTreeIter * iter) {

	playlist_data_t * data;

	gtk_tree_model_get(GTK_TREE_MODEL(store), iter, PL_COL_DATA, &data, -1);
	playlist_data_free(data);

	return gtk_tree_store_remove(store, iter);
}


GtkTreeStore *
playlist_store_new() {

	return gtk_tree_store_new(PL_COL_COUNT,
				  G_TYPE_STRING,    /* title string */
				  G_TYPE_STRING,    /* volume adj. displayed */
				  G_TYPE_STRING,    /* duration displayed */
				  G_TYPE_STRING,    /* color */
				  G_TYPE_INT,       /* font weight */
				  G_TYPE_POINTER);  /* pointer to struct playlist_data_t */
}

playlist_t *
playlist_new(char * name) {

	playlist_t * pl = NULL;

	if ((pl = (playlist_t *)calloc(1, sizeof(playlist_t))) == NULL) {
		fprintf(stderr, "playlist_new(): calloc error\n");
		return NULL;
	}

	playlists = g_list_append(playlists, pl);

	AQUALUNG_COND_INIT(pl->thread_wait);

#ifdef _WIN32
	pl->thread_mutex = g_mutex_new();
	pl->wait_mutex = g_mutex_new();
	pl->thread_wait = g_cond_new();
#endif

	if (name != NULL) {
		strncpy(pl->name, name, MAXLEN-1);
		pl->name_set = 1;
	} else {
		strncpy(pl->name, _("(Untitled)"), MAXLEN-1);
	}

	pl->index = -1;

	pl->store = playlist_store_new();

	return pl;
}

void
playlist_free(playlist_t * pl) {

	playlists = g_list_remove(playlists, pl);

#ifdef _WIN32
	g_mutex_free(pl->thread_mutex);
	g_mutex_free(pl->wait_mutex);
	g_cond_free(pl->thread_wait);
#endif

	free(pl);
}


gint
playlist_compare_widget(gconstpointer list, gconstpointer widget) {

	return ((playlist_t *)list)->widget != widget;
}

gint
playlist_compare_playing(gconstpointer list, gconstpointer dummy) {

	return ((playlist_t *)list)->playing == 0;
}

gint
playlist_compare_name(gconstpointer list, gconstpointer name) {

	return strcmp(((playlist_t *)list)->name, (char *)name);
}

gint
playlist_compare_index(gconstpointer p1, gconstpointer p2) {

	int i1 = ((playlist_t *)p1)->index;
	int i2 = ((playlist_t *)p2)->index;

	if (i1 == -1) {
		i1 = 1000;
	}

	if (i2 == -1) {
		i2 = 1000;
	}

	if (i1 < i2) {
		return -1;
	} else if (i1 > i2) {
		return 1;
	}

	return 0;
}

playlist_t *
playlist_find(gconstpointer data, GCompareFunc func) {

	GList * find = g_list_find_custom(playlists, data, func);

	if (find != NULL) {
		return (playlist_t *)(find->data);
	}

	return NULL;
}


playlist_t *
playlist_get_current() {

	int idx = gtk_notebook_get_current_page(GTK_NOTEBOOK(playlist_notebook));
	GtkWidget * widget = gtk_notebook_get_nth_page(GTK_NOTEBOOK(playlist_notebook), idx);

	return playlist_find(widget, playlist_compare_widget);
}

playlist_t *
playlist_get_playing() {

	return playlist_find(NULL/*dummy*/, playlist_compare_playing);
}

playlist_t *
playlist_get_by_page_num(int num) {

	GtkWidget * widget = gtk_notebook_get_nth_page(GTK_NOTEBOOK(playlist_notebook), num);

	playlist_t * pl = playlist_find(widget, playlist_compare_widget);

	if (pl == NULL) {
		printf("WARNING: playlist_get_by_page_num() == NULL\n");
	}

	return pl;
}


playlist_t *
playlist_get_by_name(char * name) {

	return playlist_find(name, playlist_compare_name);
}


void
playlist_set_current(playlist_t * pl) {

	int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook));
	int i;

	for (i = 0; i < n; i++) {
		if (pl->widget == gtk_notebook_get_nth_page(GTK_NOTEBOOK(playlist_notebook), i)) {
			gtk_notebook_set_current_page(GTK_NOTEBOOK(playlist_notebook), i);
			return;
		}
	}
}

int
playlist_is_empty(playlist_t * pl) {

	GtkTreeIter dummy;

	if (!pl->name_set &&
	    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(pl->store), &dummy) == FALSE) {
		return 1;
	}

	return 0;
}

void
playlist_node_copy(GtkTreeStore * sstore, GtkTreeIter * siter,
		   GtkTreeStore * tstore, GtkTreeIter * titer,
		   GtkTreeIter * iter, int mode) {

	gchar * name;
	gchar * vadj;
	gchar * dura;

	playlist_data_t * sdata;
	playlist_data_t * tdata;
	int unmark = 0;

	gtk_tree_model_get(GTK_TREE_MODEL(sstore), siter,
			   PL_COL_NAME, &name,
			   PL_COL_VADJ, &vadj,
			   PL_COL_DURA, &dura,
			   PL_COL_DATA, &sdata, -1);

	if ((tdata = playlist_data_copy(sdata)) == NULL) {
		return;
	}

	if (sstore != tstore) {
		PL_UNSET_FLAG(tdata, PL_FLAG_ACTIVE);
	}

	if (mode == 0) {
		gtk_tree_store_insert_after(tstore, iter, NULL, titer);
	} else if (mode == 1) {
		gtk_tree_store_insert_before(tstore, iter, NULL, titer);
	} else {
		gtk_tree_store_append(tstore, iter, titer);
	}

	if (tdata->file) {
		GtkTreeIter parent;
		int name_set = 0;
		if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(tstore), &parent, iter)) {
			playlist_data_t * pdata;
			gtk_tree_model_get(GTK_TREE_MODEL(tstore), &parent, PL_COL_DATA, &pdata, -1);
			if (!strcmp(pdata->artist, tdata->artist) && !strcmp(pdata->album, tdata->album)) {
				gtk_tree_store_set(tstore, iter, PL_COL_NAME, tdata->title, -1);
				name_set = 1;
			}
		}
		if (!name_set) {
			char list_str[MAXLEN];
			playlist_data_get_display_name(list_str, tdata);
			gtk_tree_store_set(tstore, iter, PL_COL_NAME, list_str, -1);
		}
	} else {
		gtk_tree_store_set(tstore, iter,PL_COL_NAME, name, -1);
	}

	gtk_tree_store_set(tstore, iter,
			   PL_COL_VADJ, vadj,
			   PL_COL_DURA, dura,
			   PL_COL_COLO, IS_PL_ACTIVE(tdata) ? pl_color_active : pl_color_inactive,
			   PL_COL_FONT, (IS_PL_ACTIVE(tdata) && options.show_active_track_name_in_bold) ?
			                 PANGO_WEIGHT_BOLD :
			                 PANGO_WEIGHT_NORMAL,
			   PL_COL_DATA, tdata, -1);

	if (unmark) {
		unmark_track(tstore, iter);
	}

	g_free(name);
	g_free(vadj);
	g_free(dura);
}

void
playlist_node_deep_copy(GtkTreeStore * sstore, GtkTreeIter * siter,
			GtkTreeStore * tstore, GtkTreeIter * titer,
			int mode) {

	GtkTreeIter iter;
	GtkTreeIter dummy;
	GtkTreeIter child_siter;
	int i;


	playlist_node_copy(sstore, siter, tstore, titer, &iter, mode);

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(sstore), &child_siter, siter, i++)) {
		playlist_node_copy(sstore, &child_siter, tstore, &iter, &dummy, 2/*append*/);
	}
}

int
all_tracks_selected(playlist_t * pl, GtkTreeIter * piter) {

	gint j = 0;
	GtkTreeIter iter_child;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, piter, j++)) {
		if (!gtk_tree_selection_iter_is_selected(pl->select, &iter_child)) {
			return 0;
		}
	}
	return 1;
}

void
playlist_copy(playlist_t * pl) {

	GtkTreeIter iter;
	gint i = 0;

	if (clipboard == NULL) {
		if ((clipboard = (clipboard_t *)calloc(1, sizeof(clipboard_t))) == NULL) {
			fprintf(stderr, "playlist_copy(): calloc error\n");
			return;
		}
		clipboard->store = playlist_store_new();
	} else {
		gtk_tree_store_clear(clipboard->store);
	}

	clipboard->has_album_node = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(pl->select, &iter)) {
			playlist_node_deep_copy(pl->store, &iter, clipboard->store, NULL, 2);
			clipboard->has_album_node = 1;
			continue;
		}

		if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) == 0) {
			continue;
		}

		if (all_tracks_selected(pl, &iter)) {
			playlist_node_deep_copy(pl->store, &iter, clipboard->store, NULL, 2);
			clipboard->has_album_node = 1;
		} else {
			int j = 0;
			GtkTreeIter iter_child;
			GtkTreeIter dummy;

			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, &iter, j++)) {
				if (gtk_tree_selection_iter_is_selected(pl->select, &iter_child)) {
					playlist_node_copy(pl->store, &iter_child,
							   clipboard->store, NULL, &dummy, 2);
				}
			}
		}
	}
}

void
playlist_cut(playlist_t * pl) {

	playlist_copy(pl);
	rem__sel_cb(pl);
}

void
playlist_paste(playlist_t * pl, int before) {

	gint i = 0;
	GtkTreeIter iter;
	GtkTreeIter titer;
	GtkTreePath * path;

	gtk_tree_view_get_cursor(GTK_TREE_VIEW(pl->view), &path, NULL);

	if (path != NULL) {

		if (clipboard->has_album_node && gtk_tree_path_get_depth(path) > 1) {
			gtk_tree_path_free(path);
			return;
		}

		gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &titer, path);
	}

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(clipboard->store), &iter, NULL, i++)) {

		if (path == NULL) {
			playlist_node_deep_copy(clipboard->store, &iter, pl->store, NULL, 2);
		} else {
			if (before) {
				playlist_node_deep_copy(clipboard->store, &iter, pl->store, &titer, 1);
			} else {
				playlist_node_deep_copy(clipboard->store, &iter, pl->store, &titer, 0);
				gtk_tree_model_iter_next(GTK_TREE_MODEL(pl->store), &titer);
			}
		}
	}

	if (path != NULL) {
		gtk_tree_path_free(path);
	}

	playlist_content_changed(pl);
}


playlist_transfer_t *
playlist_transfer_new(playlist_t * pl) {

	playlist_transfer_t * pt = NULL;

	if ((pt = (playlist_transfer_t *)calloc(1, sizeof(playlist_transfer_t))) == NULL) {
		fprintf(stderr, "playlist_transfer_new(): calloc error\n");
		return NULL;
	}

	if (pl == NULL) {
		if ((pt->pl = playlist_get_current()) == NULL) {
			free(pt);
			return NULL;
		}
	} else {
		pt->pl = pl;
	}

	pt->threshold = 1;

	return pt;
}


void
playlist_transfer_free(playlist_transfer_t * pt) {

	if (pt->xml_ref != NULL) {
		*(pt->xml_ref) -= 1;
		if (*(pt->xml_ref) == 0) {
			xmlFreeDoc((xmlDocPtr)pt->xml_doc);
			free(pt->xml_ref);
		}
	} else if (pt->xml_doc != NULL) {
		xmlFreeDoc((xmlDocPtr)pt->xml_doc);
	}

	if (pt->filename) {
		free(pt->filename);
	}

	free(pt);
}

void
playlist_reset_display_names(void) {

	GList * node = NULL;
	playlist_data_t * data = NULL;
	char list_str[MAXLEN];

	for (node = playlists; node; node = node->next) {
		playlist_t * pl = (playlist_t *)node->data;
		GtkTreeIter iter;
		int i = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {
			if (!gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), &iter)) {
				gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_DATA, &data, -1);
				playlist_data_get_display_name(list_str, data);
				gtk_tree_store_set(pl->store, &iter, PL_COL_NAME, list_str, -1);
			}
		}
	}
}

void
playlist_disable_bold_font_foreach(gpointer data, gpointer user_data) {

        GtkTreeIter iter;
	gint i = 0;

	GtkTreeStore * store = ((playlist_t *)data)->store;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, i++)) {
		gint j = 0;
		GtkTreeIter iter_child;

		gtk_tree_store_set(store, &iter, PL_COL_FONT, PANGO_WEIGHT_NORMAL, -1);
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter_child, &iter, j++)) {
			gtk_tree_store_set(store, &iter_child, PL_COL_FONT, PANGO_WEIGHT_NORMAL, -1);
		}
	}
}

void
playlist_disable_bold_font(void) {

	g_list_foreach(playlists, playlist_disable_bold_font_foreach, NULL);
}

void
playlist_set_font_foreach(gpointer data, gpointer user_data) {

	gtk_widget_modify_font(((playlist_t *)data)->view, fd_playlist);
}

void
playlist_set_font(int cond) {

        if (cond) {
		gtk_widget_modify_font(statusbar_selected, fd_statusbar);
		gtk_widget_modify_font(statusbar_selected_label, fd_statusbar);
		gtk_widget_modify_font(statusbar_total, fd_statusbar);
		gtk_widget_modify_font(statusbar_total_label, fd_statusbar);

		g_list_foreach(playlists, playlist_set_font_foreach, NULL);
	}
}

void
playlist_set_markup(playlist_t * pl) {

	if (pl->playing) {
		char str[MAXLEN];

		snprintf(str, MAXLEN-1, "<b>%s</b>", pl->name);
		gtk_label_set_markup(GTK_LABEL(pl->label), str);
		gtk_widget_modify_fg(pl->label, GTK_STATE_NORMAL,
				     &playlist_color_indicator->style->fg[GTK_STATE_ACTIVE]);
		gtk_widget_modify_fg(pl->label, GTK_STATE_ACTIVE,
				     &playlist_color_indicator->style->fg[GTK_STATE_ACTIVE]);
	} else {
		gtk_label_set_text(GTK_LABEL(pl->label), pl->name);
		gtk_widget_modify_fg(pl->label, GTK_STATE_NORMAL,
				     &playlist_color_indicator->style->fg[GTK_STATE_NORMAL]);
		gtk_widget_modify_fg(pl->label, GTK_STATE_ACTIVE,
				     &playlist_color_indicator->style->fg[GTK_STATE_NORMAL]);
	}
}

void
playlist_set_playing(playlist_t * pl, int playing) {

	pl->playing = playing;
	playlist_set_markup(pl);
}

void
adjust_playlist_item_color(GtkTreeStore * store, GtkTreeIter * iter,
			   char * active, char * inactive) {

	playlist_data_t * data;
	gchar * str;

	gtk_tree_model_get(GTK_TREE_MODEL(store), iter, PL_COL_DATA, &data, -1);
	
	if (IS_PL_ACTIVE(data)) {
		gtk_tree_store_set(store, iter, PL_COL_COLO, active, -1);
		if (options.show_active_track_name_in_bold) {
			gtk_tree_store_set(store, iter, PL_COL_FONT, PANGO_WEIGHT_BOLD, -1);
		}
	} else {
		gtk_tree_store_set(store, iter,
				   PL_COL_COLO, inactive,
				   PL_COL_FONT, PANGO_WEIGHT_NORMAL, -1);
	}

	g_free(str);
}


void
adjust_playlist_color(playlist_t * pl, char * active, char * inactive) {

	int i = 0;
	GtkTreeIter iter;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {
		int j = 0;
		GtkTreeIter iter_child;
			
		adjust_playlist_item_color(pl->store, &iter, active, inactive);
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store),
						     &iter_child, &iter, j++)) {
			adjust_playlist_item_color(pl->store, &iter_child,
						   active, inactive);
		}
	}
}

void
playlist_set_color(void) {

	GList * node = NULL;

	char active[14];
	char inactive[14];

	GdkColor color;

	int rs = 0, gs = 0, bs = 0;
	int ri = 0, gi = 0, bi = 0;


        if (options.override_skin_settings &&
	    (gdk_color_parse(options.activesong_color, &color) == TRUE)) {

                rs = color.red;
                gs = color.green;
                bs = color.blue;

                if (rs == 0 && gs == 0 && bs == 0) {
                        rs = 1;
		}
        } else {
		rs = playlist_color_indicator->style->fg[SELECTED].red;
		gs = playlist_color_indicator->style->fg[SELECTED].green;
		bs = playlist_color_indicator->style->fg[SELECTED].blue;
	}

        if (options.override_skin_settings &&
	    (gdk_color_parse(options.song_color, &color) == TRUE)) {

                ri = color.red;
                gi = color.green;
                bi = color.blue;

        } else {
        	ri = playlist_color_indicator->style->fg[INSENSITIVE].red;
        	gi = playlist_color_indicator->style->fg[INSENSITIVE].green;
        	bi = playlist_color_indicator->style->fg[INSENSITIVE].blue;
        }

        sprintf(active, "#%04X%04X%04X", rs, gs, bs);
	sprintf(inactive, "#%04X%04X%04X", ri, gi, bi);

	for (node = playlists; node; node = node->next) {
		playlist_t * pl = (playlist_t *)node->data;
		adjust_playlist_color(pl, active, inactive);
		playlist_set_playing(pl, pl->playing);
	}

	for (node = playlists_closed; node; node = node->next) {
		playlist_t * pl = (playlist_t *)node->data;
		adjust_playlist_color(pl, active, inactive);
	}

        strcpy(pl_color_active, active);
	strcpy(pl_color_inactive, inactive);
}

void
playlist_foreach_selected(playlist_t * pl, int (* foreach)(playlist_t *, GtkTreeIter *, void *),
			  void * data) {

	GtkTreeIter iter_top;
	GtkTreeIter iter;
	gint i;
	gint j;

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_top, NULL, i++)) {

		gboolean topsel = gtk_tree_selection_iter_is_selected(pl->select, &iter_top);

		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), &iter_top)) {

			j = 0;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, &iter_top, j++)) {
				if (topsel || gtk_tree_selection_iter_is_selected(pl->select, &iter)) {
					if (foreach(pl, &iter, data)) {
						return;
					}
				}
			}

		} else if (topsel) {
			if (foreach(pl, &iter_top, data)) {
				return;
			}
		}
	}
}

int
playlist_selection_file_types_foreach(playlist_t * pl, GtkTreeIter * iter, void * user_data) {

	playlist_data_t * pldata;
	int * i = (int *)user_data;

	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), iter, PL_COL_DATA, &pldata, -1);

	if (cdda_is_cdtrack(pldata->file)) {
		*i = 1;
	} else if (!httpc_is_url(pldata->file)) {
		*i = 0;
	}

	return *i == 0;
}

/* return: 0: selection has audio file; 1: has cdda but no audio; 2: has only http or empty */
int
playlist_selection_file_types(playlist_t * pl) {

	int type = 2;

	playlist_foreach_selected(pl, playlist_selection_file_types_foreach, &type);

	return type;
}

GtkTreePath *
playlist_get_playing_path(playlist_t * pl) {

	playlist_data_t * data;
	GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_DATA, &data, -1);

		if (IS_PL_ACTIVE(data)) {
			if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) > 0) {

				playlist_data_t * data_child;
				GtkTreeIter iter_child;
				int j = 0;

				while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store),
								     &iter_child, &iter, j++)) {

					gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter_child,
							   PL_COL_DATA, &data_child, -1);

					if (IS_PL_ACTIVE(data_child)) {
						return gtk_tree_model_get_path(GTK_TREE_MODEL(pl->store), &iter_child);
					}
				}
			} else {
				return gtk_tree_model_get_path(GTK_TREE_MODEL(pl->store), &iter);
			}
		}
	}

	return NULL;
}

void
playlist_set_active(GtkTreeStore * store, GtkTreeIter * piter) {

	playlist_data_t * data;

	gtk_tree_model_get(GTK_TREE_MODEL(store), piter, PL_COL_DATA, &data, -1);
	PL_SET_FLAG(data, PL_FLAG_ACTIVE);

	gtk_tree_store_set(store, piter, PL_COL_COLO, pl_color_active, -1);
	if (options.show_active_track_name_in_bold) {
		gtk_tree_store_set(store, piter, PL_COL_FONT, PANGO_WEIGHT_BOLD, -1);
	}
}

void
playlist_set_inactive(GtkTreeStore * store, GtkTreeIter * piter) {

	playlist_data_t * data;

	gtk_tree_model_get(GTK_TREE_MODEL(store), piter, PL_COL_DATA, &data, -1);
	PL_UNSET_FLAG(data, PL_FLAG_ACTIVE);

	gtk_tree_store_set(store, piter,
			   PL_COL_COLO, pl_color_inactive,
			   PL_COL_FONT, PANGO_WEIGHT_NORMAL, -1);
}

void
mark_track(GtkTreeStore * store, GtkTreeIter * piter) {

        int j, n;
        char * name;
        char counter[MAXLEN];
	char tmpname[MAXLEN];

	playlist_data_t * data;


	gtk_tree_model_get(GTK_TREE_MODEL(store), piter, PL_COL_DATA, &data, -1);
	if (IS_PL_ACTIVE(data)) {
		return;
	}


        n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), piter);

        if (n) {
		GtkTreeIter iter_child;
		playlist_data_t * data_child;

                gtk_tree_model_get(GTK_TREE_MODEL(store), piter,  PL_COL_NAME, &name, -1);
                strncpy(tmpname, name, MAXLEN-1);

                j = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter_child, piter, j++)) {

			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter_child, PL_COL_DATA, &data_child, -1);
			if (IS_PL_ACTIVE(data_child)) {
				break;
			}
		}

		if (j > n) {
			return;
		}

                sprintf(counter, _(" (%d/%d)"), j, n);
                strncat(tmpname, counter, MAXLEN-1);
		gtk_tree_store_set(store, piter, PL_COL_NAME, tmpname, -1);
		data->ntracks = n;
		data->actrack = j;
                g_free(name);
        }

	playlist_set_active(store, piter);

	if (gtk_tree_store_iter_depth(store, piter)) { /* track node of album */
		GtkTreeIter iter_parent;

		gtk_tree_model_iter_parent(GTK_TREE_MODEL(store), &iter_parent, piter);
		mark_track(store, &iter_parent);
	}
}


void
unmark_track(GtkTreeStore * store, GtkTreeIter * piter) {

	int n;

	playlist_set_inactive(store, piter);

        n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), piter);

        if (n) {
		char name[MAXLEN];
		playlist_data_t * data;

		gtk_tree_model_get(GTK_TREE_MODEL(store), piter, PL_COL_DATA, &data, -1);
		snprintf(name, MAXLEN-1, "%s: %s", data->artist, data->album);
		gtk_tree_store_set(store, piter, PL_COL_NAME, name, -1);
	}

	if (gtk_tree_store_iter_depth(store, piter)) { /* track node of album */
		GtkTreeIter iter_parent;

		gtk_tree_model_iter_parent(GTK_TREE_MODEL(store), &iter_parent, piter);
		unmark_track(store, &iter_parent);
	}
}


void
playlist_start_playback_at_path(playlist_t * pl, GtkTreePath * path) {

	GtkTreeIter iter;
	GtkTreePath * p;
	gchar cmd;
	cue_t cue;

	playlist_t * plist = NULL;


	if (!allow_seeks) {
		return;
	}

	while ((plist = playlist_get_playing()) != NULL) {
		playlist_set_playing(plist, 0);
	}

	playlist_set_playing(pl, 1);

	p = playlist_get_playing_path(pl);
 	if (p != NULL) {
 		gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
 		gtk_tree_path_free(p);
		unmark_track(pl->store, &iter);
 	}

	gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, path);

	if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) > 0) {
		GtkTreeIter iter_child;
		gtk_tree_model_iter_children(GTK_TREE_MODEL(pl->store), &iter_child, &iter);
		mark_track(pl->store, &iter_child);
	} else {
		mark_track(pl->store, &iter);
	}

	p = playlist_get_playing_path(pl);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
	gtk_tree_path_free(p);


	playlist_data_t * pldata;
	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_DATA, &pldata, -1);
	cue_track_for_playback(pl->store, &iter, &cue);

        if (options.show_sn_title) {
		refresh_displays();
        }

	toggle_noeffect(PLAY, TRUE);

	if (is_paused) {
		is_paused = 0;
		toggle_noeffect(PAUSE, FALSE);
	}
		
	cmd = CMD_CUE;
	rb_write(rb_gui2disk, &cmd, sizeof(char));
	rb_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
	try_waking_disk_thread();
	
	is_file_loaded = 1;
}


static gboolean
playlist_window_close(GtkWidget * widget, GdkEvent * event, gpointer data) {

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle), FALSE);

	return TRUE;
}


gint
playlist_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	GtkTreePath * path;
	GtkTreeIter iter;
	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return TRUE;
	}

        switch (kevent->keyval) {

        case GDK_Insert:
	case GDK_KP_Insert:
                if (kevent->state & GDK_SHIFT_MASK) {  /* SHIFT + Insert */
                        add_directory(NULL, NULL);
                } else {
                        add_files(NULL, NULL);
                }
                return TRUE;
        case GDK_q:
	case GDK_Q:
	case GDK_Escape:
                if (!options.playlist_is_embedded) {
                        playlist_window_close(NULL, NULL, NULL);
		}
		return TRUE;
	case GDK_F1:
	case GDK_i:
	case GDK_I:
		gtk_tree_view_get_cursor(GTK_TREE_VIEW(pl->view), &path, NULL);

		if (path &&
		    gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, path) &&
		    !gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), &iter)) {

			GtkTreeIter dummy;
			gchar * name;
			playlist_data_t * data;
			
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
					   PL_COL_NAME, &name,
					   PL_COL_DATA, &data, -1);

			if (!name) {
				free(name);
				return TRUE;
			}
			
			strncpy(fileinfo_name, name, MAXLEN-1);
			strncpy(fileinfo_file, data->file, MAXLEN-1);
			free(name);

			show_file_info(fileinfo_name, fileinfo_file, 0, NULL, dummy, IS_PL_COVER(data));
		}

		if (path) {
			gtk_tree_path_free(path);
		}

		return TRUE;
	case GDK_Return:
	case GDK_KP_Enter:
		gtk_tree_view_get_cursor(GTK_TREE_VIEW(pl->view), &path, NULL);

		if (path) {
			playlist_start_playback_at_path(pl, path);
			gtk_tree_path_free(path);
		}
		return TRUE;
	case GDK_u:
	case GDK_U:
		cut__sel_cb(NULL);
		return TRUE;
	case GDK_f:
	case GDK_F:
		plist__search_cb(pl);
		return TRUE;
        case GDK_a:
        case GDK_A:
		if (kevent->state & GDK_CONTROL_MASK) {
			if (kevent->state & GDK_SHIFT_MASK) {
				sel__none_cb(NULL);
			} else {
				sel__all_cb(NULL);
			}
		} else {
                        if (kevent->state & GDK_MOD1_MASK) {  /* ALT + a */
                                playlist_set_current(playlist_get_playing());
                                show_active_position_in_playlist(playlist_get_playing());
                        } else {
        			show_active_position_in_playlist_toggle(pl);
                        }
		}
                return TRUE;
        case GDK_w:
        case GDK_W:
		if (kevent->state & GDK_CONTROL_MASK) {
			playlist_tab_close(NULL, pl);
		} else {
			gtk_tree_view_collapse_all(GTK_TREE_VIEW(pl->view));
			show_active_position_in_playlist(pl);
		}
		return TRUE;
        case GDK_Delete:
	case GDK_KP_Delete:
                if (kevent->state & GDK_SHIFT_MASK) {  /* SHIFT + Delete */
			rem__all_cb(NULL);
                } else if (kevent->state & GDK_CONTROL_MASK) {  /* CTRL + Delete */
                        playlist_unlink_files(pl);
                } else {
			rem__sel_cb(NULL);
                }
		return TRUE;
	case GDK_t:
	case GDK_T:
		if (kevent->state & GDK_CONTROL_MASK) {
			if (kevent->state & GDK_SHIFT_MASK) {   /* CTRL + SHIFT T */
				playlist_tab_close_undo();
			} else {   /* CTRL + T */
				playlist_tab_new(NULL);
			}
		}
#ifdef HAVE_IFP
		else {
			if (kevent->state & GDK_SHIFT_MASK) {   /* SHIFT T */
                                aifp_transfer_files(DOWNLOAD_MODE);
                        } else {
                                aifp_transfer_files(UPLOAD_MODE);
                        }
                }
#endif /* HAVE_IFP */
		return TRUE;
	case GDK_r:
	case GDK_R:
		if (kevent->state & GDK_CONTROL_MASK) {  /* CTRL + R */
                        tab__rename_cb(playlist_get_current());
                }
		return TRUE;
        case GDK_grave:
                expand_collapse_album_node(pl);
                return TRUE;
	case GDK_Page_Up:
		if (kevent->state & GDK_CONTROL_MASK) {
			playlist_notebook_prev_page();
			return TRUE;
		}
		break;
	case GDK_Page_Down:
		if (kevent->state & GDK_CONTROL_MASK) {
			playlist_notebook_next_page();
			return TRUE;
		}
		break;
	case GDK_ISO_Left_Tab: /* it is usually mapped to SHIFT + TAB */
		if (kevent->state & GDK_CONTROL_MASK) {
			playlist_notebook_prev_page();
		}
		return TRUE;
	case GDK_Tab:
		if (kevent->state & GDK_CONTROL_MASK) {
			if (kevent->state & GDK_SHIFT_MASK) {
				playlist_notebook_prev_page();
			} else {
				playlist_notebook_next_page();
			}
		}
		return TRUE;
	case GDK_c:
	case GDK_C:
		if (kevent->state & GDK_CONTROL_MASK) {
			playlist_copy(playlist_get_current());
			return TRUE;
		}
		break;
	case GDK_x:
	case GDK_X:
		if (kevent->state & GDK_CONTROL_MASK) {
			playlist_cut(playlist_get_current());
			return TRUE;
		}
		break;
	case GDK_v:
	case GDK_V:
		if (kevent->state & GDK_CONTROL_MASK) {
			if (kevent->state & GDK_SHIFT_MASK) {
				playlist_paste(playlist_get_current(), 0/*after*/);
			} else {
				playlist_paste(playlist_get_current(), 1/*before*/);
			}
			return TRUE;
		}
		break;
	}

	return FALSE;
}

gint
playlist_notebook_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	if ((kevent->state & GDK_CONTROL_MASK) &&
	    (kevent->keyval == GDK_Page_Up || kevent->keyval == GDK_Page_Down)) {
		/* ignore default tab switching key handler */
		return TRUE;
	}

	if (kevent->keyval == GDK_Tab ||
	    kevent->keyval == GDK_ISO_Left_Tab) {
		/* prevent focus traversal */
		return TRUE;
	}

	return FALSE;
}

gint
playlist_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	if ((kevent->state & GDK_CONTROL_MASK) &&
	    (kevent->keyval == GDK_Page_Up || kevent->keyval == GDK_Page_Down)) {
		/* ignore default key handler for PageUp/Down when
		   CTRL is pressed to prevent unwanted cursor motion */
		return TRUE;
	}

	if (kevent->keyval == GDK_Tab ||
	    kevent->keyval == GDK_ISO_Left_Tab) {
		/* prevent focus traversal */
		return TRUE;
	}

	return FALSE;
}

void
playlist_row_expand_collapse(GtkTreeView * view, GtkTreeIter * iter, GtkTreePath * path, gpointer data) {

	playlist_t * pl = (playlist_t *)data;

	delayed_playlist_rearrange(pl);
}

void
playlist_menu_set_popup_sensitivity(playlist_t * pl) {

	int file_types = 0;
	int has_selection = (gtk_tree_selection_count_selected_rows(pl->select) != 0);

	if (has_selection) {
		file_types = playlist_selection_file_types(pl);
	}

	gtk_widget_set_sensitive(plist__reread_file_meta, has_selection && (file_types == 0));
	gtk_widget_set_sensitive(plist__rva, has_selection && (file_types <= 1));
#ifdef HAVE_EXPORT
	gtk_widget_set_sensitive(plist__export, has_selection && (file_types == 0));
#endif /* HAVE_EXPORT */
#ifdef HAVE_IFP
	gtk_widget_set_sensitive(plist__send_songs_to_iriver, has_selection && (file_types == 0));
#endif  /* HAVE_IFP */
}

gint
doubleclick_handler(GtkWidget * widget, GdkEventButton * event, gpointer data) {

	GtkTreePath * path;
	GtkTreeIter iter;
	playlist_t * pl = (playlist_t *)data;

	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(pl->view), event->x, event->y,
						  &path, NULL, NULL, NULL)) {
			playlist_start_playback_at_path(pl, path);
			gtk_tree_path_free(path);
		}
	}

	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(pl->view), event->x, event->y,
						  &path, NULL, NULL, NULL) &&
		    gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, path)) {

			if (gtk_tree_selection_count_selected_rows(pl->select) == 0) {
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(pl->view), path, NULL, FALSE);
			}

			if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), &iter)) {
				gtk_widget_set_sensitive(plist__fileinfo, FALSE);
			} else {
				gchar * name;
				playlist_data_t * data;

				gtk_widget_set_sensitive(plist__fileinfo, TRUE);

				gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
						   PL_COL_NAME, &name,
						   PL_COL_DATA, &data, -1);

				strncpy(fileinfo_name, name, MAXLEN-1);
				strncpy(fileinfo_file, data->file, MAXLEN-1);
				free(name);
			}
		} else {
			gtk_widget_set_sensitive(plist__fileinfo, FALSE);
		}

		playlist_menu_set_popup_sensitivity(pl);

		gtk_menu_popup(GTK_MENU(plist_menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);
		return TRUE;
	}
	return FALSE;
}


static int
filter(const struct dirent * de) {

	return de->d_name[0] != '.';
}


gboolean
finalize_add_to_playlist(gpointer data) {

	playlist_transfer_t * pt = (playlist_transfer_t *)data;

	pt->pl->progbar_semaphore--;

	if (pt->pl->progbar_semaphore != 0) {
		return FALSE;
	}

	if (playlist_window == NULL) {
		return FALSE;
	}

	if (pt->pl == playlist_get_current()) {
		playlist_content_changed(pt->pl);
		if (pt->start_playback) {
			GtkTreeModel * model = GTK_TREE_MODEL(pt->pl->store);
			GtkTreePath * path = NULL;
			GtkTreeIter iter;

			if (gtk_tree_model_get_iter_first(model, &iter)) {
				if (gtk_tree_model_iter_has_child(model, &iter)) {
					GtkTreeIter iter_child;
					gtk_tree_model_iter_children(model, &iter_child, &iter);
					path = gtk_tree_model_get_path(model, &iter_child);
				} else {
					path = gtk_tree_model_get_path(model, &iter);
				}
			}
			if (path != NULL) {
				playlist_start_playback_at_path(pt->pl, path);
				gtk_tree_path_free(path);
			}
		}
	}

	playlist_progress_bar_hide(pt->pl);

	if (pt->xml_ref != NULL) {
		select_active_position_in_playlist(pt->pl);
	}

	return FALSE;
}

void
playlist_data_get_display_name(char * list_str, playlist_data_t * pldata) {

	if (pldata->artist && pldata->album && pldata->title) {
		make_title_string(list_str, options.title_format,
				  pldata->artist, pldata->album, pldata->title);
	} else if (pldata->artist && pldata->title) {
		make_title_string_no_album(list_str, options.title_format_no_album,
					   pldata->artist, pldata->title);
	} else if (pldata->title) {
		strncpy(list_str, pldata->title, MAXLEN-1);
	} else {
		strncpy(list_str, pldata->file, MAXLEN-1);
	}
}

gboolean
add_file_to_playlist(gpointer data) {

	playlist_transfer_t * pt = (playlist_transfer_t *)data;
	GList * node;
	int finish = 0;

	AQUALUNG_MUTEX_LOCK(pt->pl->wait_mutex);

	if (pt->clear) {
		rem_all(pt->pl);
		pt->clear = 0;
	}

	for (node = pt->pldata_list; node; node = node->next) {

		GtkTreeIter iter;
		GtkTreePath * path;
		char voladj_str[32];
		char duration_str[MAXLEN];
		char list_str[MAXLEN];

		playlist_data_t * pldata = (playlist_data_t *)node->data;

		pt->data_written--;

		if (pldata == NULL) {
			pt->data_written = 0;
			finish = 1;
			break;
		}

		voladj2str(pldata->voladj, voladj_str);
		time2time_na(pldata->duration, duration_str);

		if (IS_PL_TOPLEVEL(pldata)) {
			gtk_tree_store_append(pt->pl->store, &iter, NULL);
		} else {
			GtkTreeIter parent;
			int n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pt->pl->store), NULL);

			if (n == 0) {
				/* someone viciously cleared the list while adding tracks to album node;
				   ignore further tracks added to this node */
				playlist_data_free(pldata);
				continue;
			}

			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pt->pl->store), &parent, NULL, n-1);
			gtk_tree_store_append(pt->pl->store, &iter, &parent);
		}

		if (IS_PL_ALBUM_NODE(pldata)) {
			if (IS_PL_ACTIVE(pldata)) {
				snprintf(list_str, MAXLEN-1, "%s: %s (%d/%d)",
					 pldata->artist, pldata->album,
					 pldata->actrack, pldata->ntracks);
			} else {
				snprintf(list_str, MAXLEN-1, "%s: %s",
					 pldata->artist, pldata->album);
			}
		} else if (IS_PL_ALBUM_CHILD(pldata)) {
			GtkTreeIter parent;
			playlist_data_t * pdata;
			if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(pt->pl->store), &parent, &iter)) {
				gtk_tree_model_get(GTK_TREE_MODEL(pt->pl->store), &parent, PL_COL_DATA, &pdata, -1);
				if (!strcmp(pdata->artist, pldata->artist) && !strcmp(pdata->album, pldata->album)) {
					strncpy(list_str, pldata->title, MAXLEN-1);
				} else {
					playlist_data_get_display_name(list_str, pldata);
				}
			}
		} else {
			playlist_data_get_display_name(list_str, pldata);
		}

		gtk_tree_store_set(pt->pl->store, &iter,
				   PL_COL_NAME, list_str,
				   PL_COL_VADJ, IS_PL_ALBUM_NODE(pldata) ? "" : voladj_str,
				   PL_COL_DURA, duration_str,
				   PL_COL_COLO, IS_PL_ACTIVE(pldata) ? pl_color_active : pl_color_inactive,
				   PL_COL_FONT, (IS_PL_ACTIVE(pldata) && options.show_active_track_name_in_bold) ?
				                 PANGO_WEIGHT_BOLD :
				                 PANGO_WEIGHT_NORMAL,
				   PL_COL_DATA, pldata,
				   -1);

		if (IS_PL_ACTIVE(pldata) && !IS_PL_ALBUM_NODE(pldata) &&
		    (path = playlist_get_playing_path(pt->pl)) != NULL) {
			/* deactivate item if playing path already exists */
			GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(pt->pl->store), &iter);
			if (gtk_tree_path_compare(path, p) != 0) {
				unmark_track(pt->pl->store, &iter);
			}
			gtk_tree_path_free(path);
			gtk_tree_path_free(p);
		}

		if (pt->start_playback && IS_PL_ACTIVE(pldata) && !IS_PL_ALBUM_NODE(pldata)) {
			GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(pt->pl->store), &iter);
			playlist_start_playback_at_path(pt->pl, p);
			gtk_tree_path_free(p);
			pt->start_playback = 0;
		}
	}

	g_list_free(pt->pldata_list);
	pt->pldata_list = NULL;

	if (finish) {
		finalize_add_to_playlist(pt);
	}

	AQUALUNG_MUTEX_UNLOCK(pt->pl->wait_mutex);
	AQUALUNG_COND_SIGNAL(pt->pl->thread_wait);

	return FALSE;
}


void
playlist_thread_add_to_list(playlist_transfer_t * pt, playlist_data_t * pldata) {

	AQUALUNG_MUTEX_LOCK(pt->pl->wait_mutex);

	pt->pldata_list = g_list_append(pt->pldata_list, pldata);
	pt->data_written++;

	if (pt->data_written >= pt->threshold || pldata == NULL) {

		if (pt->threshold < 64) {
			pt->threshold *= 2;
		}

		g_idle_add(add_file_to_playlist, pt);

		while (pt->data_written > 0) {
			AQUALUNG_COND_WAIT(pt->pl->thread_wait, pt->pl->wait_mutex);
		}
	}

	AQUALUNG_MUTEX_UNLOCK(pt->pl->wait_mutex);
}

void *
add_files_to_playlist_thread(void * arg) {

	playlist_transfer_t * pt = (playlist_transfer_t *)arg;
	GSList * node = NULL;

	AQUALUNG_THREAD_DETACH();

	AQUALUNG_MUTEX_LOCK(pt->pl->thread_mutex);

	for (node = pt->list; node; node = node->next) {

		if (!pt->pl->thread_stop) {
			playlist_data_t * pldata = NULL;

			if ((pldata = playlist_filemeta_get((char *)node->data)) != NULL) {
				playlist_thread_add_to_list(pt, pldata);
			}
		}

		g_free(node->data);
	}

	playlist_thread_add_to_list(pt, NULL);

	g_slist_free(pt->list);

	AQUALUNG_MUTEX_UNLOCK(pt->pl->thread_mutex);

	playlist_transfer_free(pt);

	return NULL;
}


void
add_files(GtkWidget * widget, gpointer data) {

        GSList * files = file_chooser(_("Select files"),
				      options.playlist_is_embedded ? main_window : playlist_window,
				      GTK_FILE_CHOOSER_ACTION_OPEN,
				      FILE_CHOOSER_FILTER_AUDIO,
				      TRUE,
				      options.audiodir);
        if (files != NULL) {

		playlist_transfer_t * pt = playlist_transfer_get(PLAYLIST_ENQUEUE, NULL, 0);

		if (pt != NULL) {
			pt->list = files;
			playlist_progress_bar_show(pt->pl);
			AQUALUNG_THREAD_CREATE(pt->pl->thread_id,
					       NULL,
					       add_files_to_playlist_thread,
					       pt);
		}
        }
}


void
add_dir_to_playlist(playlist_transfer_t * pt, char * dirname) {

	gint i, n;
	struct dirent ** ent;
	gchar path[MAXLEN];


	if (pt->pl->thread_stop) {
		return;
	}

	n = scandir(dirname, &ent, filter, alphasort);
	for (i = 0; i < n; i++) {

		if (pt->pl->thread_stop) {
			break;
		}

		snprintf(path, MAXLEN-1, "%s/%s", dirname, ent[i]->d_name);

		if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
			free(ent[i]);
			continue;
		}

		if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
			add_dir_to_playlist(pt, path);
		} else {
			playlist_data_t * pldata = NULL;

			if ((pldata = playlist_filemeta_get(path)) != NULL) {
				playlist_thread_add_to_list(pt, pldata);
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
add_dir_to_playlist_thread(void * arg) {


	playlist_transfer_t * pt = (playlist_transfer_t *)arg;
	GSList * node = NULL;

	AQUALUNG_THREAD_DETACH();

	AQUALUNG_MUTEX_LOCK(pt->pl->thread_mutex);

	for (node = pt->list; node; node = node->next) {

		add_dir_to_playlist(pt, (char *)node->data);
		g_free(node->data);
	}

	playlist_thread_add_to_list(pt, NULL);

	g_slist_free(pt->list);

	AQUALUNG_MUTEX_UNLOCK(pt->pl->thread_mutex);

	playlist_transfer_free(pt);

	return NULL;
}


void
add_directory(GtkWidget * widget, gpointer data) {

        GSList * dirs = file_chooser(_("Select directory"),
				     options.playlist_is_embedded ? main_window : playlist_window,
				     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
				     FILE_CHOOSER_FILTER_NONE,
				     TRUE,
				     options.audiodir);
        if (dirs != NULL) {

		playlist_transfer_t * pt = playlist_transfer_get(PLAYLIST_ENQUEUE, NULL, 0);

		if (pt != NULL) {
			pt->list = dirs;
			playlist_progress_bar_show(pt->pl);
			AQUALUNG_THREAD_CREATE(pt->pl->thread_id,
					       NULL,
					       add_dir_to_playlist_thread, pt);
		}
        }
}

gint 
add_url_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data) {

        if (event->keyval == GDK_Return) {
                gtk_dialog_response(GTK_DIALOG(widget), GTK_RESPONSE_ACCEPT);
                return TRUE;
        }

        return FALSE;
}

void
add_url(GtkWidget * widget, gpointer data) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * hbox2;
	GtkWidget * url_entry;
	GtkWidget * url_label;
	char url[MAXLEN];

        dialog = gtk_dialog_new_with_buttons(_("Add URL"),
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

        gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, -1);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
        g_signal_connect (G_OBJECT (dialog), "key_press_event",
                        G_CALLBACK (add_url_key_press_cb), NULL);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, TRUE, 2);

	url_label = gtk_label_new(_("URL:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), url_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox2, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

        url_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(url_entry), MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(url_entry), "http://");
        gtk_box_pack_start(GTK_BOX(hbox2), url_entry, TRUE, TRUE, 0);

	gtk_widget_grab_focus(url_entry);

	gtk_widget_show_all(dialog);

 display:
	url[0] = '\0';
        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		
		GtkTreeIter iter;
		gchar voladj_str[32];
		gchar duration_str[MAXLEN];
		playlist_t * pl = playlist_get_current();
		playlist_data_t * data;

                strncpy(url, gtk_entry_get_text(GTK_ENTRY(url_entry)), MAXLEN-1);
		
		if (url[0] == '\0' || strstr(url, "http://") != url || strlen(url) <= strlen("http://")) {
			gtk_widget_grab_focus(url_entry);
			goto display;
		}

		if ((data = playlist_data_new()) == NULL) {
			return;
		}

		data->file = strdup(url);
		data->duration = 0.0f;
		data->voladj = options.rva_no_rva_voladj;

		voladj2str(data->voladj, voladj_str);
		time2time_na(data->duration, duration_str);

		gtk_tree_store_append(pl->store, &iter, NULL);
		gtk_tree_store_set(pl->store, &iter,
				   PL_COL_NAME, url,
				   PL_COL_VADJ, voladj_str,
				   PL_COL_DURA, duration_str,
				   PL_COL_COLO, pl_color_inactive,
				   PL_COL_FONT, PANGO_WEIGHT_NORMAL,
				   PL_COL_DATA, data, -1);

		playlist_content_changed(pl);
        }

        gtk_widget_destroy(dialog);
}


void
plist__save_cb(gpointer data) {

	GSList * file;
	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

        file = file_chooser(_("Please specify the file to save the playlist to."),
			    options.playlist_is_embedded ? main_window : playlist_window,
			    GTK_FILE_CHOOSER_ACTION_SAVE,
			    FILE_CHOOSER_FILTER_NONE,
			    FALSE,
			    options.plistdir);

        if (file != NULL) {
		playlist_save(pl, (char *)file->data);
                g_free(file->data);
		g_slist_free(file);
        }
}


void
plist__save_all_cb(gpointer data) {

        GSList * file;
	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

        file = file_chooser(_("Please specify the file to save the playlist to."),
			    options.playlist_is_embedded ? main_window : playlist_window,
			    GTK_FILE_CHOOSER_ACTION_SAVE,
			    FILE_CHOOSER_FILTER_NONE,
			    FALSE,
			    options.plistdir);
        if (file != NULL) {
		playlist_save_all((char *)file->data);
                g_free(file->data);
		g_slist_free(file);
        }
}


playlist_transfer_t *
playlist_transfer_get(int mode, char * tab_name, int start_playback) {

	playlist_t * pl = NULL;
	playlist_transfer_t * pt = NULL;

	if (mode == PLAYLIST_LOAD_TAB) {
		pl = playlist_tab_new(tab_name);
		pt = playlist_transfer_new(pl);
	} else {
		if (tab_name == NULL) {
			pl = playlist_get_current();
		} else {
			pl = playlist_get_by_name(tab_name);
		}
		if (pl == NULL) {
			pl = playlist_tab_new(tab_name);
		}
		pt = playlist_transfer_new(pl);
	}

	if (mode == PLAYLIST_LOAD) {
		pt->clear = 1;
	}

	pt->start_playback = start_playback;

	return pt;
}


void
playlist_load(GSList * list, int mode, char * tab_name, int start_playback) {

	GSList * node;
	playlist_t * pl = NULL;
	playlist_transfer_t * pt = NULL;
	char fullname[MAXLEN];
	int type;

	for (node = list; node; node = node->next) {

		normalize_filename((char *)node->data, fullname);
		type = playlist_get_type(fullname);

		if (type != PLAYLIST_XML_MULTI) {
			/* content goes to the same playlist */
			if (pl == NULL) {
				pt = playlist_transfer_get(mode, tab_name, start_playback);
				pl = pt->pl;
			} else {
				pt = playlist_transfer_new(pl);
			}
		}

		switch (type) {
		case PLAYLIST_INVALID:
			/* not a playlist, trying to load as audio file */
			pt->list = g_slist_append(NULL, strdup(fullname));
			playlist_progress_bar_show(pt->pl);
			
			if (g_file_test(fullname, G_FILE_TEST_IS_DIR)) {
				AQUALUNG_THREAD_CREATE(pt->pl->thread_id,
						       NULL,
						       add_dir_to_playlist_thread, pt);
			} else {
				AQUALUNG_THREAD_CREATE(pt->pl->thread_id,
						       NULL,
						       add_files_to_playlist_thread, pt);
			}

			break;

		case PLAYLIST_XML_MULTI:
			playlist_load_xml_multi(fullname, start_playback);
			break;

		case PLAYLIST_XML_SINGLE:
			playlist_load_xml_single(fullname, pt);
			break;

		case PLAYLIST_M3U:
			playlist_progress_bar_show(pt->pl);
			pt->filename = strdup(fullname);
			AQUALUNG_THREAD_CREATE(pt->pl->thread_id,
					       NULL,
					       playlist_load_m3u_thread,
					       pt);
			break;

		case PLAYLIST_PLS:
			playlist_progress_bar_show(pt->pl);
			pt->filename = strdup(fullname);
			AQUALUNG_THREAD_CREATE(pt->pl->thread_id,
					       NULL,
					       playlist_load_pls_thread,
					       pt);
			break;
		}

		free(node->data);
	}

	g_slist_free(list);
}

void
playlist_load_dialog(int mode) {

	GSList * files;

        files = file_chooser(_("Please specify the file to load the playlist from."), 
			     options.playlist_is_embedded ? main_window : playlist_window, 
			     GTK_FILE_CHOOSER_ACTION_OPEN, 
			     FILE_CHOOSER_FILTER_PLAYLIST,
			     FALSE,
			     options.plistdir);
        if (files != NULL) {
		playlist_load(files, mode, NULL, 0);
        }
}

void
plist__load_cb(gpointer data) {

	playlist_load_dialog(PLAYLIST_LOAD);
}

void
plist__enqueue_cb(gpointer data) {

	playlist_load_dialog(PLAYLIST_ENQUEUE);
}

void
plist__load_tab_cb(gpointer data) {

	playlist_load_dialog(PLAYLIST_LOAD_TAB);
}


int
plist_setup_vol_foreach(playlist_t * pl, GtkTreeIter * iter, void * data) {

	volume_t * vol = (volume_t *)data;
	playlist_data_t * pldata;

	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), iter, PL_COL_DATA, &pldata, -1);

	if (!httpc_is_url(pldata->file)) {
		volume_push(vol, pldata->file, *iter);
	}

	return 0;
}

void
plist_setup_vol_calc(playlist_t * pl, int type) {

	volume_t * vol;

	if (!options.rva_is_enabled) {

		int ret = message_dialog(_("Warning"),
					 options.playlist_is_embedded ? main_window : playlist_window,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_YES_NO,
					 NULL,
					 _("Playback RVA is currently disabled.\n"
					   "Do you want to enable it now?"));

		if (ret != GTK_RESPONSE_YES) {
			return;
		}

		options.rva_is_enabled = 1;
	}

	if ((vol = volume_new(pl->store, type)) == NULL) {
		return;
	}

	playlist_foreach_selected(pl, plist_setup_vol_foreach, vol);

	volume_start(vol);
}

void
plist__rva_separate_cb(gpointer data) {

	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	plist_setup_vol_calc(pl, VOLUME_SEPARATE);
}

void
plist__rva_average_cb(gpointer data) {

	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	plist_setup_vol_calc(pl, VOLUME_AVERAGE);
}


int
plist__reread_file_meta_foreach(playlist_t * pl, GtkTreeIter * iter, void * user_data) {

	GtkTreeIter parent;
	int name_set = 0;

	char voladj_str[32];
	char duration_str[MAXLEN];
	playlist_data_t * data;
	playlist_data_t * tmp;

	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), iter, PL_COL_DATA, &data, -1);

	if (!g_file_test(data->file, G_FILE_TEST_EXISTS)) {
		return 0;
	}

	if ((tmp = playlist_filemeta_get(data->file)) == NULL) {
		fprintf(stderr, "plist__reread_file_meta_foreach(): "
			"playlist_filemeta_get() returned NULL\n");
		return 0;
	}

	if (tmp->artist) {
		free_strdup(&data->artist, tmp->artist);
	}
	if (tmp->album) {
		free_strdup(&data->album, tmp->album);
	}
	if (tmp->title) {
		free_strdup(&data->title, tmp->title);
	}
	data->voladj = tmp->voladj;
	data->duration = tmp->duration;
	data->size = tmp->size;

	playlist_data_free(tmp);

	voladj2str(data->voladj, voladj_str);
	time2time_na(data->duration, duration_str);

	gtk_tree_store_set(pl->store, iter,
			   PL_COL_VADJ, voladj_str,
			   PL_COL_DURA, duration_str,
			   -1);

	if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(pl->store), &parent, iter)) {
		playlist_data_t * pdata;
		gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &parent, PL_COL_DATA, &pdata, -1);
		if (!strcmp(pdata->artist, data->artist) && !strcmp(pdata->album, data->album)) {
			gtk_tree_store_set(pl->store, iter, PL_COL_NAME, data->title, -1);
			name_set = 1;
		}
	}

	if (!name_set) {
		char list_str[MAXLEN];
		playlist_data_get_display_name(list_str, data);
		gtk_tree_store_set(pl->store, iter, PL_COL_NAME, list_str, -1);
	}

	return 0;
}


void
plist__reread_file_meta_cb(gpointer data) {
	
	GtkTreeIter iter;
	GtkTreeIter iter_child;
	gint i = 0;
	gint j = 0;
	gint reread = 0;
	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	playlist_foreach_selected(pl, plist__reread_file_meta_foreach, NULL);
	
	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {
		
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), &iter)) {
			
			reread = 0;
			
			if (gtk_tree_selection_iter_is_selected(pl->select, &iter)) {
				reread = 1;
			} else {
				j = 0;
				while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store),
								     &iter_child, &iter, j++)) {
					if (gtk_tree_selection_iter_is_selected(pl->select,
										&iter_child)) {
						reread = 1;
						break;
					}
				}
			}
			
			if (reread) {
				recalc_album_node(pl, &iter);
			}
		}
	}
	
	playlist_content_changed(pl);
}

#ifdef HAVE_EXPORT
int
plist__export_foreach(playlist_t * pl, GtkTreeIter * iter, void * data) {

	export_t * export = (export_t *)data;
	playlist_data_t * pldata;
	file_decoder_t * fdec;

	char artist[MAXLEN];
	char album[MAXLEN];
	char title[MAXLEN];
	int year = 0;
	int no = 0;

	artist[0] = '\0';
	album[0] = '\0';
	title[0] = '\0';

	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), iter, PL_COL_DATA, &pldata, -1);

	if (!g_file_test(pldata->file, G_FILE_TEST_EXISTS)) {
		return 0;
	}

	fdec = file_decoder_new();
	if (fdec == NULL) {
		fprintf(stderr, "plist__export_foreach: file_decoder_new() failed\n");
		return 0;
	}

	if (file_decoder_open(fdec, pldata->file) == 0) {

		char * tmp;

		if (pldata->artist) {
			strcpy(artist, pldata->artist);
		} else if (metadata_get_artist(fdec->meta, &tmp)) {
			strncpy(artist, tmp, MAXLEN-1);
		}

		if (pldata->album) {
			strcpy(album, pldata->album);
		} else if (metadata_get_album(fdec->meta, &tmp)) {
			strncpy(album, tmp, MAXLEN-1);
		}

		if (pldata->title) {
			strcpy(title, pldata->title);
		} else if (metadata_get_title(fdec->meta, &tmp)) {
			strncpy(title, tmp, MAXLEN-1);
		}

		if (metadata_get_date(fdec->meta, &tmp)) {
			year = atoi(tmp);
		}

		if (!metadata_get_tracknum(fdec->meta, &no)) {
			GtkTreePath * path = gtk_tree_model_get_path(GTK_TREE_MODEL(pl->store), iter);
			int depth = gtk_tree_path_get_depth(path);
			gint * indices = gtk_tree_path_get_indices(path);
			no = indices[depth-1] + 1;
			gtk_tree_path_free(path);
		}

		file_decoder_close(fdec);
	}
	file_decoder_delete(fdec);

	export_append_item(export, pldata->file, artist, album, title, year, no);

	return 0;
}

void
plist__export_cb(gpointer data) {

	export_t * export;
	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	if ((export = export_new()) == NULL) {
		return;
	}

	playlist_foreach_selected(pl, plist__export_foreach, export);

	export_start(export);
}
#endif /* HAVE_EXPORT */

#ifdef HAVE_IFP
void
plist__send_songs_to_iriver_cb(gpointer data) {

        aifp_transfer_files(UPLOAD_MODE);

}
#endif /* HAVE_IFP */

void
plist__fileinfo_cb(gpointer user_data) {

	GtkTreeIter dummy;
	GtkTreePath * path;
	GtkTreeIter iter;
        playlist_t * pl;
	playlist_data_t * data;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

        gtk_tree_view_get_cursor(GTK_TREE_VIEW(pl->view), &path, NULL);

        if (path &&
            gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, path) &&
            !gtk_tree_model_iter_has_child(GTK_TREE_MODEL(pl->store), &iter)) {

                gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_DATA, &data, -1);
	        show_file_info(fileinfo_name, fileinfo_file, 0, NULL, dummy, IS_PL_COVER(data));
        }
}

void
plist__search_cb(gpointer data) {
	
	search_playlist_dialog();
}


static gboolean
add_cb(GtkWidget * widget, GdkEvent * event) {

        if (event->type == GDK_BUTTON_PRESS) {
                GdkEventButton * bevent = (GdkEventButton *) event;

                if (bevent->button == 3) {

			gtk_menu_popup(GTK_MENU(add_menu), NULL, NULL, NULL, NULL,
				       bevent->button, bevent->time);

			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}


static gboolean
sel_cb(GtkWidget * widget, GdkEvent * event) {

        if (event->type == GDK_BUTTON_PRESS) {
                GdkEventButton * bevent = (GdkEventButton *) event;

                if (bevent->button == 3) {
			gtk_menu_popup(GTK_MENU(sel_menu), NULL, NULL, NULL, NULL,
				       bevent->button, bevent->time);
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}


static gboolean
rem_cb(GtkWidget * widget, GdkEvent * event) {

        if (event->type == GDK_BUTTON_PRESS) {
                GdkEventButton * bevent = (GdkEventButton *) event;

                if (bevent->button == 3) {
			gtk_menu_popup(GTK_MENU(rem_menu), NULL, NULL, NULL, NULL,
				       bevent->button, bevent->time);
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}


playlist_data_t *
playlist_filemeta_get(char * filename) {

	playlist_data_t * data;
	file_decoder_t * fdec;
	struct stat statbuf;
	char * tmp;

	if ((data = playlist_data_new()) == NULL) {
		return NULL;
	}
	
	fdec = file_decoder_new();
	if (fdec == NULL) {
		fprintf(stderr, "playlist_filemeta_get: file_decoder_new() failed\n");
		playlist_data_free(data);
		return NULL;
	}
	if (file_decoder_open(fdec, filename) != 0) {
		file_decoder_delete(fdec);
		playlist_data_free(data);
		return NULL;
	}

	if (g_stat(data->file, &statbuf) != -1) {
		data->size = statbuf.st_size;
	}

	data->duration = (float)fdec->fileinfo.total_samples / fdec->fileinfo.sample_rate;

	if (options.rva_is_enabled) {
		if (!metadata_get_rva(fdec->meta, &data->voladj)) {
			data->voladj = options.rva_no_rva_voladj;
		}
	} else {
		data->voladj = 0.0f;
	}
	
	if (metadata_get_artist(fdec->meta, &tmp)) {
		free_strdup(&data->artist, tmp);
	}
	if (metadata_get_album(fdec->meta, &tmp)) {
		free_strdup(&data->album, tmp);
	}
	if (metadata_get_title(fdec->meta, &tmp)) {
		free_strdup(&data->title, tmp);
	}

	file_decoder_close(fdec);
	file_decoder_delete(fdec);

	/*
	if (data->artist == NULL) {
		data->artist = strndup(_("Unknown"), MAXLEN-1);
	}
	if (data->album == NULL) {
		data->album = strndup(_("Unknown"), MAXLEN-1);
	}
	if (data->title == NULL) {
		data->title = strndup(_("Unknown"), MAXLEN-1);
	}
	*/

	data->file = g_strdup(filename);

	return data;
}


void
add__files_cb(gpointer data) {

	add_files(NULL, NULL);
}


void
add__dir_cb(gpointer data) {

	add_directory(NULL, NULL);
}


void
add__url_cb(gpointer data) {

	add_url(NULL, NULL);
}


void
select_all(GtkWidget * widget, gpointer data) {

	playlist_t * pl = playlist_get_current();

	if (pl != NULL) {
		gtk_tree_selection_select_all(pl->select);
	}
}


void
sel__all_cb(gpointer data) {

	select_all(NULL, data);
}


void
sel__none_cb(gpointer data) {

	playlist_t * pl;

	if ((pl = playlist_get_current()) != NULL) {
		gtk_tree_selection_unselect_all(pl->select);
	}
}


void
sel__inv_foreach(GtkTreePath * path, gpointer data) {

	gtk_tree_selection_unselect_path((GtkTreeSelection *)data, path);
	gtk_tree_path_free(path);
}


void
sel__inv_cb(gpointer data) {


	GList * list = NULL;
	playlist_t * pl = NULL;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	list = gtk_tree_selection_get_selected_rows(pl->select, NULL);

	g_signal_handlers_block_by_func(G_OBJECT(pl->select), playlist_selection_changed_cb, pl);

	gtk_tree_selection_select_all(pl->select);
	g_list_foreach(list, (GFunc)sel__inv_foreach, pl->select);
	g_list_free(list);

	g_signal_handlers_unblock_by_func(G_OBJECT(pl->select), playlist_selection_changed_cb, pl);

	playlist_selection_changed(pl);
}


void
rem_all(playlist_t * pl) {

	g_signal_handlers_block_by_func(G_OBJECT(pl->select), playlist_selection_changed_cb, pl);

	gtk_tree_store_clear(pl->store);

	g_signal_handlers_unblock_by_func(G_OBJECT(pl->select), playlist_selection_changed_cb, pl);

	if (pl == playlist_get_current()) {
		playlist_selection_changed(pl);
		playlist_content_changed(pl);
	}
}

void
rem__all_cb(gpointer data) {

	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	rem_all(pl);
}

void
rem__sel_cb(gpointer data) {

	GtkTreeModel * model;
	GtkTreeIter iter;
	gint i = 0;
	playlist_t * pl;
	GtkTreePath * path = NULL;


	if (data != NULL) {
		pl = (playlist_t *)data;
	} else if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	g_signal_handlers_block_by_func(G_OBJECT(pl->select), playlist_selection_changed_cb, pl);

	model = GTK_TREE_MODEL(pl->store);

	while (gtk_tree_model_iter_nth_child(model, &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(pl->select, &iter)) {
			if (path == NULL) {
				path = gtk_tree_model_get_path(model, &iter);
			}
			playlist_remove_track(pl->store, &iter);
			i--;
			continue;
		}

		if (gtk_tree_model_iter_n_children(model, &iter) == 0) {
			continue;
		}

		if (all_tracks_selected(pl, &iter)) {
			if (path == NULL) {
				path = gtk_tree_model_get_path(model, &iter);
			}
			playlist_remove_track(pl->store, &iter);
			i--;
		} else {
			int j = 0;
			int recalc = 0;
			GtkTreeIter iter_child;

			while (gtk_tree_model_iter_nth_child(model, &iter_child, &iter, j++)) {
				if (gtk_tree_selection_iter_is_selected(pl->select, &iter_child)) {
					if (path == NULL) {
						path = gtk_tree_model_get_path(model, &iter_child);
					}
					playlist_remove_track(pl->store, &iter_child);
					recalc = 1;
					j--;
				}
			}

			if (recalc) {
				recalc_album_node(pl, &iter);
			}
		}
	}

	if (path != NULL) {
		if (gtk_tree_model_get_iter(model, &iter, path)) {
			set_cursor_in_playlist(pl, &iter, TRUE);
		} else {
			if (gtk_tree_path_get_depth(path) == 1) {
				int n = gtk_tree_model_iter_n_children(model, NULL);
				if (n > 0) {
					gtk_tree_model_iter_nth_child(model, &iter, NULL, n-1);
					set_cursor_in_playlist(pl, &iter, TRUE);
				}
			} else {
				GtkTreeIter iter_child;

				gtk_tree_path_up(path);
				gtk_tree_model_get_iter(model, &iter, path);

				gtk_tree_model_iter_nth_child(model, &iter_child, &iter,
						gtk_tree_model_iter_n_children(model, &iter)-1);
				set_cursor_in_playlist(pl, &iter_child, TRUE);
			}
		}

		gtk_tree_path_free(path);
	}

	g_signal_handlers_unblock_by_func(G_OBJECT(pl->select), playlist_selection_changed_cb, pl);


	playlist_content_changed(pl);
	playlist_selection_changed(pl);
}


int
rem__dead_skip(char * filename) {

	return (filename != NULL && !cdda_is_cdtrack(filename) && !httpc_is_url(filename) &&
		(g_file_test(filename, G_FILE_TEST_IS_REGULAR) == FALSE));
}

void
rem__dead_cb(gpointer user_data) {

	GtkTreeIter iter;
	gint i = 0;
	playlist_t * pl;
	playlist_data_t * data;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		GtkTreeIter iter_child;
		gint j = 0;


                gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_DATA, &data, -1);

                gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter);

                if (n == 0 && rem__dead_skip(data->file)) {
                        playlist_remove_track(pl->store, &iter);
                        --i;
                        continue;
                }

                if (n) {
                        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, &iter, j++)) {
                                gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter_child, PL_COL_DATA, &data, -1);

                                if (rem__dead_skip(data->file)) {
                                        playlist_remove_track(pl->store, &iter_child);
                                        --j;
                                }
                        }

                        /* remove album node if empty */
                        if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter) == 0) {
				playlist_remove_track(pl->store, &iter);
                                --i;
                        } else {
                                recalc_album_node(pl, &iter);
                        }
                }

        }

	playlist_content_changed(pl);
}

void
remove_sel(GtkWidget * widget, gpointer data) {

	rem__sel_cb(data);
}


/* playlist item is selected -> keep, else -> remove
 * ret: 0 if kept, 1 if removed track
 */
int
cut_track_item(playlist_t * pl, GtkTreeIter * piter) {

	if (!gtk_tree_selection_iter_is_selected(pl->select, piter)) {
		playlist_remove_track(pl->store, piter);
		return 1;
	}
	return 0;
}


/* ret: 1 if at least one of album node's children are selected; else 0 */
int
any_tracks_selected(playlist_t * pl, GtkTreeIter * piter) {

	gint j = 0;
	GtkTreeIter iter_child;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, piter, j++)) {
		if (gtk_tree_selection_iter_is_selected(pl->select, &iter_child)) {
			return 1;
		}
	}
	return 0;
}


/* cut selected callback */
void
cut__sel_cb(gpointer data) {

	GtkTreeIter iter;
	gint i = 0;
	playlist_t * pl;

	if ((pl = playlist_get_current()) == NULL) {
		return;
	}

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter);

		if (n) { /* album node */
			if (any_tracks_selected(pl, &iter)) {
				gint j = 0;
				GtkTreeIter iter_child;
				while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store),
								     &iter_child, &iter, j++)) {
					j -= cut_track_item(pl, &iter_child);
				}

				recalc_album_node(pl, &iter);
			} else {
				i -= cut_track_item(pl, &iter);
			}
		} else { /* track node */
			i -= cut_track_item(pl, &iter);
		}
	}

	gtk_tree_selection_unselect_all(pl->select);
	playlist_content_changed(pl);
}


void
playlist_reorder_columns_foreach(gpointer data, gpointer user_data) {

	playlist_t * pl = (playlist_t *)data;
	int * order = (int *)user_data;
	GtkTreeViewColumn * cols[3];
	int i;

	cols[0] = pl->track_column;
	cols[1] = pl->rva_column;
	cols[2] = pl->length_column;

        for (i = 2; i >= 0; i--) {
		gtk_tree_view_move_column_after(GTK_TREE_VIEW(pl->view),
						cols[order[i]], NULL);
	}

	gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pl->rva_column),
					 options.show_rva_in_playlist);
	gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pl->length_column),
					 options.show_length_in_playlist);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(pl->view), options.enable_pl_rules_hint);

	delayed_playlist_rearrange(NULL);
}

void
playlist_reorder_columns_all(int * order) {

	g_list_foreach(playlists, playlist_reorder_columns_foreach, order);

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook)) == 1) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(playlist_notebook),
					   options.playlist_always_show_tabs);
	}
}


gint
playlist_size_allocate(GtkWidget * widget, GdkEventConfigure * event, gpointer data) {

	gint avail;
	gint track_width;
	gint rva_width;
	gint length_width;

	playlist_t * pl = (playlist_t *)data;

	if (pl == NULL || pl->closed) {
		return TRUE;
	}

	avail = pl->view->allocation.width;

	if (gtk_tree_view_column_get_visible(GTK_TREE_VIEW_COLUMN(pl->rva_column))) {
		gtk_tree_view_column_cell_get_size(GTK_TREE_VIEW_COLUMN(pl->rva_column),
						   NULL, NULL, NULL, &rva_width, NULL);
		rva_width += 5;
	} else {
		rva_width = 1;
	}

	if (gtk_tree_view_column_get_visible(GTK_TREE_VIEW_COLUMN(pl->length_column))) {
		gtk_tree_view_column_cell_get_size(GTK_TREE_VIEW_COLUMN(pl->length_column),
						   NULL, NULL, NULL, &length_width, NULL);
		length_width += 5;
	} else {
		length_width = 1;
	}

	track_width = avail - rva_width - length_width;
	if (track_width < 1)
		track_width = 1;

	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->track_column), track_width);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->rva_column), rva_width);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->length_column), length_width);

	if (options.playlist_is_embedded) {
		if (main_window->window != NULL) {
			gtk_widget_queue_draw(main_window);
		}
	} else {
		if (playlist_window->window != NULL) {
			gtk_widget_queue_draw(playlist_window);
		}
	}

        return TRUE;
}

void
playlist_size_allocate_all_foreach(gpointer data, gpointer user_data) {

	playlist_size_allocate(NULL, NULL, data);
}


void
playlist_size_allocate_all() {

	g_list_foreach(playlists, playlist_size_allocate_all_foreach, NULL);
}


gint
playlist_rearrange_timeout_cb(gpointer data) {   

	if (data == NULL) {
		playlist_size_allocate_all();
	} else {
		playlist_size_allocate(NULL, NULL, (playlist_t *)data);
	}

	return FALSE;
}

void
delayed_playlist_rearrange(playlist_t * pl) {

	g_timeout_add(100, playlist_rearrange_timeout_cb, pl);
}


void
playlist_stats_set_busy() {

	gtk_label_set_text(GTK_LABEL(statusbar_total), _("counting..."));
}


void
playlist_child_stats(playlist_t * pl, GtkTreeIter * iter,
		     int * ntrack, float * length, double * size, int selected) {

	int j = 0;
	GtkTreeIter iter_child;
	playlist_data_t * data;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, iter, j++)) {
		
		if (!selected || gtk_tree_selection_iter_is_selected(pl->select, &iter_child)) {
			
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter_child, PL_COL_DATA, &data, -1);

			if (data->size == 0) {
				struct stat statbuf;
				if (g_stat(data->file, &statbuf) != -1) {
					data->size = statbuf.st_size;
				}
			}

			*size += data->size / 1024.0;
			*length += data->duration;
			(*ntrack)++;
		}
	}
}


/* if selected == true -> stats for selected tracks; else: all tracks */
void
playlist_stats(playlist_t * pl, int selected) {

	GtkTreeIter iter;
	int i = 0;

	int ntrack = 0;
	float length = 0;
        double size = 0;

	char str[MAXLEN];
	char length_str[MAXLEN];
	char tmp[MAXLEN];

	playlist_data_t * data;


	if (!options.enable_playlist_statusbar) {
		return;
	}

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter);
		if (n > 0) {
			if (gtk_tree_selection_iter_is_selected(pl->select, &iter)) {
				playlist_child_stats(pl, &iter, &ntrack, &length, &size, 0/*false*/);
			} else {
				playlist_child_stats(pl, &iter, &ntrack, &length, &size, selected);
			}
		} else {
			if (!selected || gtk_tree_selection_iter_is_selected(pl->select, &iter)) {

				gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_DATA, &data, -1);

				if (data->size == 0) {
					struct stat statbuf;
					if (g_stat(data->file, &statbuf) != -1) {
						data->size = statbuf.st_size;
					}
				}

				size += data->size / 1024.0;
				length += data->duration;
				ntrack++;
			}
                }
	}

	sprintf(str, " %d %s ", ntrack, (ntrack == 1) ? _("track") : _("tracks"));

	if (length > 0.0f || ntrack == 0) {
		time2time(length, length_str);
		sprintf(tmp, " [%s] ", length_str);
		strcat(str, tmp);
	} 

	if (options.pl_statusbar_show_size) {
		if (size > 1024 * 1024) {
			sprintf(tmp, " (%.1f GB) ", size / (1024 * 1024));
	        	strcat(str, tmp);
		} else if (size > (1 << 10)){
			sprintf(tmp, " (%.1f MB) ", size / 1024);
        		strcat(str, tmp);
		} else if (size > 0 || ntrack == 0) {
			sprintf(tmp, " (%.1f KB) ", size);
		        strcat(str, tmp);
		}
	}

	if (selected) {
		gtk_label_set_text(GTK_LABEL(statusbar_selected), str);
	} else {
		gtk_label_set_text(GTK_LABEL(statusbar_total), str);
	}
}


void
recalc_album_node(playlist_t * pl, GtkTreeIter * iter) {

	gint ntrack = 0;
	gfloat length = 0;
	double size = 0.0;
	gchar time[MAXLEN];
	playlist_data_t * data;

	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), iter, PL_COL_DATA, &data, -1);

	playlist_child_stats(pl, iter, &ntrack, &length, &size, 0/*false*/);

	time2time(length, time);
	gtk_tree_store_set(pl->store, iter, PL_COL_DURA, time, -1);
	data->duration = length;

	if (IS_PL_ACTIVE(data)) {
		unmark_track(pl->store, iter);
		mark_track(pl->store, iter);
	}
}


void
playlist_selection_changed(playlist_t * pl) {

	playlist_stats(pl, 1/* true */);
}


void
playlist_content_changed(playlist_t * pl) {

	if (pl->progbar_semaphore == 0 && pl->ms_semaphore == 0) {
		playlist_stats(pl, 0/*false*/);
		delayed_playlist_rearrange(pl);
	} else {
		playlist_stats_set_busy();
	}
}

void
playlist_selection_changed_cb(GtkTreeSelection * select, gpointer data) {

	playlist_t * pl = (playlist_t *)data;

	if (pl == playlist_get_current()) {
		playlist_selection_changed(pl);
	}
}

void
playlist_drag_data_get(GtkWidget * widget, GdkDragContext * drag_context,
		      GtkSelectionData * data, guint info, guint time, gpointer user_data) {

	char tmp[32];
	sprintf(tmp, "%p", user_data);
	gtk_selection_data_set(data, data->target, 8, (guchar *)tmp, strlen(tmp));
}

void
playlist_perform_drag(playlist_t * spl, GtkTreeIter * siter, GtkTreePath * spath,
		      playlist_t * tpl, GtkTreeIter * titer, GtkTreePath * tpath) {

	GtkTreeIter sparent;
	GtkTreeIter tparent;
	int recalc_sparent = 0;
	int recalc_tparent = 0;
	gint cmp = 2/*append*/;


	if (tpath != NULL) {
		cmp = gtk_tree_path_compare(spath, tpath);
	}

	if (spl == tpl && cmp == 0) {
		return;
	}

	if (titer && tpath &&
	    gtk_tree_model_iter_has_child(GTK_TREE_MODEL(spl->store), siter) &&
	    !gtk_tree_model_iter_has_child(GTK_TREE_MODEL(tpl->store), titer) &&
	    gtk_tree_path_get_depth(tpath) == 2) {
		return;
	}

	if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(spl->store), &sparent, siter)) {
		recalc_sparent = 1;
	}

	if (titer && gtk_tree_model_iter_parent(GTK_TREE_MODEL(tpl->store), &tparent, titer)) {
		recalc_tparent = 1;
	}

	playlist_node_deep_copy(spl->store, siter, tpl->store, titer, titer ? (cmp == 1) : 2);

	gtk_tree_store_remove(spl->store, siter);
	/*
	if (recalc_sparent ^ recalc_tparent) {

		playlist_data_t * data;
		gtk_tree_model_get(GTK_TREE_MODEL(tpl->store), titer, PL_COL_DATA, &data, -1);

		if (recalc_tparent) {
			printf("title\n");
			gtk_tree_store_set(tpl->store, titer, PL_COL_NAME, data->title, -1);
		} else {
			printf("display_name\n");
			char list_str[MAXLEN];
			playlist_data_get_display_name(list_str, data);
			gtk_tree_store_set(tpl->store, titer, PL_COL_NAME, list_str, -1);
		}
	}
	*/

	if (recalc_sparent) {
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(spl->store), &sparent)) {
			recalc_album_node(spl, &sparent);
			unmark_track(spl->store, &sparent);
			mark_track(spl->store, &sparent);
		} else {
			gtk_tree_store_remove(spl->store, &sparent);
		}
	}

	if (recalc_tparent) {
		recalc_album_node(tpl, &tparent);
		unmark_track(tpl->store, &tparent);
		mark_track(tpl->store, &tparent);
	}

	playlist_content_changed(tpl);
}


void
playlist_remove_scroll_tags() {

	if (playlist_scroll_up_tag > 0) {	
		g_source_remove(playlist_scroll_up_tag);
		playlist_scroll_up_tag = -1;
	}
	if (playlist_scroll_dn_tag > 0) {	
		g_source_remove(playlist_scroll_dn_tag);
		playlist_scroll_dn_tag = -1;
	}
}

gint
playlist_drag_data_received(GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y, 
			    GtkSelectionData * data, guint info, guint time, gpointer user_data) {

	playlist_t * tpl = (playlist_t *)user_data;

	if (info == 0) { /* drag and drop inside or between playlists */

		GtkTreeIter siter;
		GtkTreeIter titer;
		GtkTreePath * spath = NULL;
		GtkTreePath * tpath = NULL;
		playlist_t * spl = NULL;


		sscanf((char *)data->data, "%p", &spl);
		gtk_tree_selection_set_mode(spl->select, GTK_SELECTION_SINGLE);

		if (gtk_tree_selection_get_selected(spl->select, NULL, &siter)) {

			spath = gtk_tree_model_get_path(GTK_TREE_MODEL(spl->store), &siter);

			if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tpl->view),
							  x, y, &tpath, NULL, NULL, NULL)) {

				gtk_tree_model_get_iter(GTK_TREE_MODEL(tpl->store), &titer, tpath);
				playlist_perform_drag(spl, &siter, spath, tpl, &titer, tpath);
			} else {
				playlist_perform_drag(spl, &siter, spath, tpl, NULL, NULL);
			}
		}

		if (spath) {
			gtk_tree_path_free(spath);
		}

		if (tpath) {
			gtk_tree_path_free(tpath);
		}

		gtk_tree_selection_set_mode(spl->select, GTK_SELECTION_MULTIPLE);

	} else if (info == 1) { /* drag and drop from music store to playlist */

		GtkTreePath * path = NULL;
		GtkTreeIter * piter = NULL;
		GtkTreeIter ms_iter;
		GtkTreeIter pl_iter;
		GtkTreeIter parent;
		GtkTreeModel * model;

		if (!gtk_tree_selection_get_selected(music_select, &model, &ms_iter)) {
			return FALSE;
		}

		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tpl->view),
						  x, y, &path, NULL, NULL, NULL)) {

			if (!music_store_iter_is_track(&ms_iter)) {
				while (gtk_tree_path_get_depth(path) > 1) {
					gtk_tree_path_up(path);
				}
			}
			
			gtk_tree_model_get_iter(GTK_TREE_MODEL(tpl->store), &pl_iter, path);
			piter = &pl_iter;
		}

		music_store_iter_addlist_defmode(&ms_iter, piter, 0/*new_tab*/);

		if (piter && gtk_tree_model_iter_parent(GTK_TREE_MODEL(tpl->store), &parent, piter) &&
		    music_store_iter_is_track(&ms_iter)) {
			recalc_album_node(tpl, &parent);
			unmark_track(tpl->store, &parent);
			mark_track(tpl->store, &parent);
		}

		if (path) {
			gtk_tree_path_free(path);
		}

	} else if (info == 2) { /* drag and drop from external app */

		GSList * list = NULL;
		gchar ** uri_list;
		gchar * str = NULL;
		int i;
		char file[MAXLEN];

		for (i = 0; *((gchar *)data->data + i); i++) {
			if (*((gchar *)data->data + i) == '\r') {
				*((gchar *)data->data + i) = '\n';
			}
		}

		uri_list = g_strsplit((gchar *)data->data, "\n", 0);

		for (i = 0; uri_list[i]; i++) {

			if (*(uri_list[i]) == '\0') {
				continue;
			}

			if ((str = g_filename_from_uri(uri_list[i], NULL, NULL)) != NULL) {
				strncpy(file, str, MAXLEN-1);
				g_free(str);
			} else {
				int off = 0;

				if (strstr(uri_list[i], "file:") == uri_list[i] ||
				    strstr(uri_list[i], "FILE:") == uri_list[i]) {
					off = 5;
				}

				while (uri_list[i][off] == '/' && uri_list[i][off+1] == '/') {
					off++;
				}

				strncpy(file, uri_list[i] + off, MAXLEN-1);
			}

			if ((str = g_locale_from_utf8(file, -1, NULL, NULL, NULL)) != NULL) {
				strncpy(file, str, MAXLEN-1);
				g_free(str);
			}

			if (g_file_test(file, G_FILE_TEST_EXISTS) || httpc_is_url(file)) {
				list = g_slist_append(list, strdup(file));
			}
		}

		if (list != NULL) {
			playlist_load(list, PLAYLIST_ENQUEUE, NULL, 0);
		}

		g_strfreev(uri_list);

		playlist_remove_scroll_tags();
	}

	return FALSE;
}

gint
playlist_scroll_up(gpointer data) {

	playlist_t * pl = (playlist_t *)data;

#if (GTK_CHECK_VERSION(2,8,0))
	gboolean dummy;
	GtkRange * range = GTK_RANGE(gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(pl->scroll)));

	g_signal_emit_by_name(G_OBJECT(range), "move-slider", GTK_SCROLL_STEP_UP, &dummy);
#else
        g_signal_emit_by_name(G_OBJECT(pl->scroll), "scroll-child",
                              GTK_SCROLL_STEP_BACKWARD, FALSE/*vertical*/, NULL);
#endif /* GTK_CHECK_VERSION */

	return TRUE;
}

gint
playlist_scroll_dn(gpointer data) {

	playlist_t * pl = (playlist_t *)data;

#if (GTK_CHECK_VERSION(2,8,0))
	gboolean dummy;
	GtkRange * range = GTK_RANGE(gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(pl->scroll)));

	g_signal_emit_by_name(G_OBJECT(range), "move-slider", GTK_SCROLL_STEP_DOWN, &dummy);
#else
        g_signal_emit_by_name(G_OBJECT(pl->scroll), "scroll-child",
                              GTK_SCROLL_STEP_FORWARD, FALSE/*vertical*/, NULL);
#endif /* GTK_CHECK_VERSION */

	return TRUE;
}


void
playlist_drag_leave(GtkWidget * widget, GdkDragContext * drag_context,
		    guint time, gpointer data) {

	playlist_remove_scroll_tags();
}


gboolean
playlist_drag_motion(GtkWidget * widget, GdkDragContext * context,
		     gint x, gint y, guint time, gpointer data) {

	if (y < 30) {
		if (playlist_scroll_up_tag == -1) {
			playlist_scroll_up_tag = g_timeout_add(100, playlist_scroll_up, data);
		}
	} else {
		if (playlist_scroll_up_tag > 0) {	
			g_source_remove(playlist_scroll_up_tag);
			playlist_scroll_up_tag = -1;
		}
	}

	if (y > widget->allocation.height - 30) {
		if (playlist_scroll_dn_tag == -1) {
			playlist_scroll_dn_tag = g_timeout_add(100, playlist_scroll_dn, data);
		}
	} else {
		if (playlist_scroll_dn_tag > 0) {	
			g_source_remove(playlist_scroll_dn_tag);
			playlist_scroll_dn_tag = -1;
		}
	}

	return TRUE;
}

void
playlist_drag_end(GtkWidget * widget, GdkDragContext * drag_context, gpointer data) {

	playlist_remove_scroll_tags();
}


void
playlist_rename(playlist_t * pl, char * name) {

	if (name != NULL) {
		strncpy(pl->name, name, MAXLEN-1);
		pl->name_set = 1;
		playlist_set_markup(pl);
	}
}

void
playlist_notebook_prev_page(void) {

	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(playlist_notebook)) == 0) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(playlist_notebook), -1);
	} else {
		gtk_notebook_prev_page(GTK_NOTEBOOK(playlist_notebook));
	}
}

void
playlist_notebook_next_page(void) {

	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(playlist_notebook)) == 
	    gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook)) - 1) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(playlist_notebook), 0);
	} else {
		gtk_notebook_next_page(GTK_NOTEBOOK(playlist_notebook));
	}
}


void
playlist_tab_close(GtkButton * button, gpointer data) {

	playlist_t * pl = (playlist_t *)data;
	int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook));
	int i;

	if (pl->progbar_semaphore > 0 || pl->ms_semaphore > 0) {
		return;
	}

	for (i = 0; i < n; i++) {
		if (pl->widget == gtk_notebook_get_nth_page(GTK_NOTEBOOK(playlist_notebook), i)) {
			gtk_notebook_remove_page(GTK_NOTEBOOK(playlist_notebook), i);

			if (playlist_is_empty(pl)) {
				playlist_free(pl);
			} else {
				pl->closed = 1;
				pl->playing = 0;
				pl->index = i;
				playlists_closed = g_list_prepend(playlists_closed, pl);
				playlists = g_list_remove(playlists, pl);
			}

			if (!options.playlist_always_show_tabs &&
			    gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook)) == 1) {
				gtk_notebook_set_show_tabs(GTK_NOTEBOOK(playlist_notebook), FALSE);
			}

			break;
		}
	}

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook)) == 0) {
		playlist_tab_new(NULL);
	}
}

void
playlist_tab_close_undo(void) {

	GList * first = g_list_first(playlists_closed);

	if (first != NULL) {
		playlist_t * pl = (playlist_t *)first->data;

		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook)) == 1) {
			playlist_t * pl = (playlist_t *)playlists->data;
			if (playlist_is_empty(pl)) {
				gtk_notebook_remove_page(GTK_NOTEBOOK(playlist_notebook), 0);
				playlist_free(pl);
			}
		}

		pl->closed = 0;
		playlists_closed = g_list_delete_link(playlists_closed, first);
		playlists = g_list_insert(playlists, pl, pl->index);
		create_playlist_gui(pl);
	}
}

gint
playlist_notebook_clicked(GtkWidget * widget, GdkEventButton * event, gpointer data) {

	if (event->type == GDK_2BUTTON_PRESS && event->button == 1 &&
	    event->y < 25 && event->x > 25 && event->x < widget->allocation.width - 25) {
		playlist_tab_new(NULL);
		return TRUE;
	}

	return FALSE;
}

gint
playlist_tab_label_clicked(GtkWidget * widget, GdkEventButton * event, gpointer data) {

	playlist_t * pl = (playlist_t *)data;

	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {

		GtkTreePath * path = playlist_get_playing_path(pl);

		if (path != NULL) {
			playlist_start_playback_at_path(pl, path);
			gtk_tree_path_free(path);
		} else {
			GtkTreeIter iter;
			if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(pl->store), &iter)) {
				path = gtk_tree_model_get_path(GTK_TREE_MODEL(pl->store), &iter);
				playlist_start_playback_at_path(pl, path);
				gtk_tree_path_free(path);
			}
		}

		return TRUE;
	}

	if (event->button == 2) {
		playlist_tab_close(NULL, pl);
		return TRUE;
	}

	if (event->button == 3) {
		if (g_list_length(playlists_closed) > 0) {
			gtk_widget_set_sensitive(pl->tab__close_undo, TRUE);
		} else {
			gtk_widget_set_sensitive(pl->tab__close_undo, FALSE);
		}
		gtk_menu_popup(GTK_MENU(pl->tab_menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

gint 
tab__rename_key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer data) {

        if (event->keyval == GDK_Return) {
                gtk_dialog_response (GTK_DIALOG(widget), GTK_RESPONSE_ACCEPT);
                return TRUE;
        }

        return FALSE;
}

void
tab__rename_cb(gpointer data) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * hbox2;
	GtkWidget * entry;
	GtkWidget * label;

	char name[MAXLEN];
	playlist_t * pl = (playlist_t *)data;


        dialog = gtk_dialog_new_with_buttons(_("Rename playlist"),
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

        gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, -1);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
        g_signal_connect (G_OBJECT (dialog), "key_press_event",
                        G_CALLBACK (tab__rename_key_press_cb), NULL);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, TRUE, 2);

	label = gtk_label_new(_("Name:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox2, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

        entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(entry), MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(entry), pl->name);
        gtk_box_pack_start(GTK_BOX(hbox2), entry, TRUE, TRUE, 0);

	gtk_widget_grab_focus(entry);

	gtk_widget_show_all(dialog);

 display:
	name[0] = '\0';
        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		
                strncpy(name, gtk_entry_get_text(GTK_ENTRY(entry)), MAXLEN-1);

		if (name[0] == '\0') {
			gtk_widget_grab_focus(entry);
			goto display;
		}

		playlist_rename(pl, name);
        }

        gtk_widget_destroy(dialog);
}


void
tab__new_cb(gpointer data) {

	playlist_tab_new(NULL);
}

void
tab__close_cb(gpointer data) {

	playlist_t * pl = (playlist_t *)data;

	playlist_tab_close(NULL, pl);
}

void
tab__close_undo_cb(gpointer data) {

	playlist_tab_close_undo();
}

void
tab__close_other_cb(gpointer data) {

	playlist_t * pl = (playlist_t *)data;
	GList * node;

	for (node = playlists; node && node->data != pl; node = playlists) {
		playlist_tab_close(NULL, node->data);
	}

	for (node = playlists->next; node && node->data != pl; node = playlists->next) {
		playlist_tab_close(NULL, node->data);
	}
}

void
playlist_show_hide_close_buttons(gboolean state) {

    	GList * list;

	for (list = playlists; list; list = list->next) {
		playlist_t * pl = (playlist_t *)list->data;
                if (state == TRUE) {
                        gtk_widget_show(pl->tab_close_button);
                } else {
                        gtk_widget_hide(pl->tab_close_button);
                }
	}
}

GtkWidget *
create_playlist_tab_label(playlist_t * pl) {

	GtkWidget * hbox;
	GtkWidget * event_box;
        GtkWidget * image;
        GdkPixbuf * pixbuf;

	GtkWidget * separator;
	GtkWidget * tab__new;
	GtkWidget * tab__rename;
	GtkWidget * tab__close;
	GtkWidget * tab__close_other;

	char path[MAXLEN];


	hbox = gtk_hbox_new(FALSE, 4);
        GTK_WIDGET_UNSET_FLAGS(hbox, GTK_CAN_FOCUS);

	event_box = gtk_event_box_new();
	pl->label = gtk_label_new(pl->name);
	gtk_widget_set_name(pl->label, "playlist_tab_label");
	gtk_container_add(GTK_CONTAINER(event_box), hbox);

	pl->tab_close_button = gtk_button_new();
	gtk_widget_set_name(pl->tab_close_button, "playlist_tab_close_button");
	sprintf(path, "%s/tab-close.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		image = gtk_image_new_from_pixbuf(pixbuf);
		gtk_container_add(GTK_CONTAINER(pl->tab_close_button), image);
	}

	gtk_button_set_relief(GTK_BUTTON(pl->tab_close_button), GTK_RELIEF_NONE);
        GTK_WIDGET_UNSET_FLAGS(pl->tab_close_button, GTK_CAN_FOCUS);
	gtk_widget_set_size_request(pl->tab_close_button, 16, 16);

        gtk_box_pack_start(GTK_BOX(hbox), pl->label, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), pl->tab_close_button, FALSE, FALSE, 0);

	gtk_widget_show_all(hbox);

        g_signal_connect(G_OBJECT(pl->tab_close_button), "clicked", G_CALLBACK(playlist_tab_close), pl);

	g_signal_connect(G_OBJECT(event_box), "button_press_event",
			 G_CALLBACK(playlist_tab_label_clicked), pl);

	/* popup menu for tabs */

	pl->tab_menu = gtk_menu_new();
        tab__new = gtk_menu_item_new_with_label(_("New tab"));
        tab__rename = gtk_menu_item_new_with_label(_("Rename"));
        tab__close = gtk_menu_item_new_with_label(_("Close tab"));
        pl->tab__close_undo = gtk_menu_item_new_with_label(_("Undo close tab"));
	tab__close_other = gtk_menu_item_new_with_label(_("Close other tabs"));

        gtk_menu_shell_append(GTK_MENU_SHELL(pl->tab_menu), tab__new);
        gtk_menu_shell_append(GTK_MENU_SHELL(pl->tab_menu), tab__rename);

	separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(pl->tab_menu), separator);
	gtk_widget_show(separator);

        gtk_menu_shell_append(GTK_MENU_SHELL(pl->tab_menu), tab__close);
        gtk_menu_shell_append(GTK_MENU_SHELL(pl->tab_menu), pl->tab__close_undo);

	separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(pl->tab_menu), separator);
	gtk_widget_show(separator);

        gtk_menu_shell_append(GTK_MENU_SHELL(pl->tab_menu), tab__close_other);

        g_signal_connect_swapped(G_OBJECT(tab__new), "activate", G_CALLBACK(tab__new_cb), pl);
        g_signal_connect_swapped(G_OBJECT(tab__rename), "activate", G_CALLBACK(tab__rename_cb), pl);
        g_signal_connect_swapped(G_OBJECT(tab__close), "activate", G_CALLBACK(tab__close_cb), pl);
        g_signal_connect_swapped(G_OBJECT(pl->tab__close_undo), "activate", G_CALLBACK(tab__close_undo_cb), pl);
        g_signal_connect_swapped(G_OBJECT(tab__close_other), "activate", G_CALLBACK(tab__close_other_cb), pl);

        gtk_widget_show(tab__rename);
        gtk_widget_show(tab__new);
	gtk_widget_show(tab__close);
	gtk_widget_show(pl->tab__close_undo);
        gtk_widget_show(tab__close_other);

        if (options.playlist_show_close_button_in_tab == FALSE) {
                gtk_widget_hide(pl->tab_close_button);
        }

	return event_box;
}

void
create_playlist_gui(playlist_t * pl) {

	GtkWidget * viewport;
	GtkCellRenderer * track_renderer;
	GtkCellRenderer * rva_renderer;
	GtkCellRenderer * length_renderer;

	int i;

	GdkPixbuf * pixbuf;
	gchar path[MAXLEN];

	GtkTargetEntry source_table[] = {
		{ "aqualung-list-list", GTK_TARGET_SAME_APP, 0 }
	};

	GtkTargetEntry target_table[] = {
		{ "aqualung-list-list", GTK_TARGET_SAME_APP, 0 },
		{ "aqualung-browser-list", GTK_TARGET_SAME_APP, 1 },
		{ "STRING", 0, 2 },
		{ "text/plain", 0, 2 },
		{ "text/uri-list", 0, 2 }
	};

        pl->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pl->store));
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(pl->view), FALSE);
	gtk_widget_set_name(pl->view, "play_list");
	gtk_widget_set_size_request(pl->view, 100, 100);

	g_signal_connect(G_OBJECT(pl->view), "key_press_event",
			 G_CALLBACK(playlist_key_pressed), NULL);
	g_signal_connect(G_OBJECT(pl->view), "row_expanded",
			 G_CALLBACK(playlist_row_expand_collapse), pl);
	g_signal_connect(G_OBJECT(pl->view), "row_collapsed",
			 G_CALLBACK(playlist_row_expand_collapse), pl);

	if (options.override_skin_settings) {
                gtk_widget_modify_font(pl->view, fd_playlist);
	}

	if (options.enable_pl_rules_hint) {
		gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(pl->view), TRUE);
	}

	for (i = 0; i < 3; i++) {
		switch (options.plcol_idx[i]) {
		case 0:
			track_renderer = gtk_cell_renderer_text_new();
			g_object_set((gpointer)track_renderer, "xalign", 0.0, NULL);
			pl->track_column = gtk_tree_view_column_new_with_attributes("Tracks",
										track_renderer,
										"text", PL_COL_NAME,
										"foreground", PL_COL_COLO,
                                                                                "weight", PL_COL_FONT,
										NULL);
			gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(pl->track_column),
							GTK_TREE_VIEW_COLUMN_FIXED);
			gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(pl->track_column), 3);
			gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(pl->track_column), FALSE);
			gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->track_column), 10);
                        g_object_set(G_OBJECT(track_renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
                        gtk_cell_renderer_set_fixed_size(track_renderer, 10, -1);
			gtk_tree_view_append_column(GTK_TREE_VIEW(pl->view), pl->track_column);
			break;

		case 1:
			rva_renderer = gtk_cell_renderer_text_new();
			g_object_set((gpointer)rva_renderer, "xalign", 1.0, NULL);
			pl->rva_column = gtk_tree_view_column_new_with_attributes("RVA",
									      rva_renderer,
									      "text", PL_COL_VADJ,
									      "foreground", PL_COL_COLO,
                                                                              "weight", PL_COL_FONT,
									      NULL);
			gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(pl->rva_column),
							GTK_TREE_VIEW_COLUMN_FIXED);
			gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(pl->rva_column), 3);
			if (options.show_rva_in_playlist) {
				gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pl->rva_column), TRUE);
				gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->rva_column), 50);
			} else {
				gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pl->rva_column), FALSE);
				gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->rva_column), 1);
			}
			gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(pl->rva_column), FALSE);
			gtk_tree_view_append_column(GTK_TREE_VIEW(pl->view), pl->rva_column);
			break;

		case 2:
			length_renderer = gtk_cell_renderer_text_new();
			g_object_set((gpointer)length_renderer, "xalign", 1.0, NULL);
			pl->length_column = gtk_tree_view_column_new_with_attributes("Length",
										 length_renderer,
										 "text", PL_COL_DURA,
										 "foreground", PL_COL_COLO,
                                                                                 "weight", PL_COL_FONT,
										 NULL);
			gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(pl->length_column),
							GTK_TREE_VIEW_COLUMN_FIXED);
			gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(pl->length_column), 3);
			if (options.show_length_in_playlist) {
				gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pl->length_column), TRUE);
				gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->length_column), 50);
			} else {
				gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(pl->length_column), FALSE);
				gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(pl->length_column), 1);
			}
			gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(pl->length_column), FALSE);
			gtk_tree_view_append_column(GTK_TREE_VIEW(pl->view), pl->length_column);
		}
	}

        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(pl->view), FALSE);

	g_signal_connect(G_OBJECT(pl->view), "button_press_event",
			 G_CALLBACK(doubleclick_handler), pl);


	/* setup drag and drop */
	
	gtk_drag_source_set(pl->view,
			    GDK_BUTTON1_MASK,
			    source_table,
			    sizeof(source_table) / sizeof(GtkTargetEntry),
			    GDK_ACTION_MOVE);

	gtk_drag_dest_set(pl->view,
			  GTK_DEST_DEFAULT_ALL,
			  target_table,
			  sizeof(target_table) / sizeof(GtkTargetEntry),
			  GDK_ACTION_MOVE | GDK_ACTION_COPY);

	snprintf(path, MAXLEN-1, "%s/drag.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		gtk_drag_source_set_icon_pixbuf(pl->view, pixbuf);
	}	

	g_signal_connect(G_OBJECT(pl->view), "drag_end",
			 G_CALLBACK(playlist_drag_end), pl);

	g_signal_connect(G_OBJECT(pl->view), "drag_data_get",
			 G_CALLBACK(playlist_drag_data_get), pl);

	g_signal_connect(G_OBJECT(pl->view), "drag_leave",
			 G_CALLBACK(playlist_drag_leave), pl);

	g_signal_connect(G_OBJECT(pl->view), "drag_motion",
			 G_CALLBACK(playlist_drag_motion), pl);

	g_signal_connect(G_OBJECT(pl->view), "drag_data_received",
			 G_CALLBACK(playlist_drag_data_received), pl);
	

        pl->select = gtk_tree_view_get_selection(GTK_TREE_VIEW(pl->view));
        gtk_tree_selection_set_mode(pl->select, GTK_SELECTION_MULTIPLE);

	g_signal_connect(G_OBJECT(pl->select), "changed",
			 G_CALLBACK(playlist_selection_changed_cb), pl);


        pl->widget = gtk_vbox_new(FALSE, 2);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(pl->widget), viewport, TRUE, TRUE, 0);

	pl->scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pl->scroll),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), pl->scroll);

	gtk_container_add(GTK_CONTAINER(pl->scroll), pl->view);


	pl->progbar_container = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(pl->widget), pl->progbar_container, FALSE, FALSE, 0);

	if (pl->progbar_semaphore > 0) {
		pl->progbar_semaphore--;
		playlist_progress_bar_show(pl);
	}

        g_signal_connect(G_OBJECT(pl->widget), "size_allocate",
			 G_CALLBACK(playlist_size_allocate), pl);

	gtk_widget_show_all(pl->widget);

	gtk_notebook_insert_page(GTK_NOTEBOOK(playlist_notebook),
				 pl->widget,
				 create_playlist_tab_label(pl),
				 pl->index);

	gtk_notebook_set_current_page(GTK_NOTEBOOK(playlist_notebook), pl->index);

	playlist_set_playing(pl, pl->playing);

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook)) > 1) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(playlist_notebook), TRUE);
	}

#if (GTK_CHECK_VERSION(2,10,0))
	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(playlist_notebook), pl->widget, TRUE);
#endif /* GTK_CHECK_VERSION */

	gtk_widget_grab_focus(pl->view);
}

playlist_t *
playlist_tab_new(char * name) {

	playlist_t * pl = NULL;

	pl = playlist_new(name);
	create_playlist_gui(pl);
	return pl;
}

playlist_t *
playlist_tab_new_if_nonempty(char * name) {

	playlist_t * pl = (playlist_t *)playlists->data;

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook)) == 1 &&
	    playlist_is_empty(pl)) {
		playlist_rename(pl, name);
		return pl;
	}

	return playlist_tab_new(name);
}

void
notebook_switch_page(GtkNotebook * notebook, GtkNotebookPage * page,
		     guint page_num, gpointer user_data) {

	playlist_t * pl = playlist_get_by_page_num(page_num);

	if (pl == NULL) {
		return;
	}

	playlist_content_changed(pl);
	playlist_selection_changed(pl);
}

#if (GTK_CHECK_VERSION(2,10,0))
void
notebook_page_reordered(GtkNotebook * notebook, GtkWidget * child,
			guint page_num, gpointer user_data) {

	GList * node;

	for (node = playlists; node; node = node->next) {
		playlist_t * pl = (playlist_t *)node->data;
		pl->index = gtk_notebook_page_num(notebook, pl->widget);
	}

	playlists = g_list_sort(playlists, playlist_compare_index);
}
#endif /* GTK_CHECK_VERSION */


void
create_playlist(void) {

	GtkWidget * vbox;

	GtkWidget * statusbar;
	GtkWidget * statusbar_scrolledwin;
	GtkWidget * statusbar_viewport;

	GtkWidget * hbox_bottom;
	GtkWidget * add_button;
	GtkWidget * sel_button;
	GtkWidget * rem_button;


	if (pl_color_active[0] == '\0') {
		strcpy(pl_color_active, "#000080");
	}

	if (pl_color_inactive[0] == '\0') {
		strcpy(pl_color_inactive, "#010101");
	}


        /* window creating stuff */
	if (!options.playlist_is_embedded) {
		playlist_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(playlist_window), _("Playlist"));
		gtk_container_set_border_width(GTK_CONTAINER(playlist_window), 2);
		g_signal_connect(G_OBJECT(playlist_window), "delete_event",
				 G_CALLBACK(playlist_window_close), NULL);
		gtk_widget_set_size_request(playlist_window, 300, 200);
	} else {
		gtk_widget_set_size_request(playlist_window, 200, 200);
	}

        if (!options.playlist_is_embedded) {
                g_signal_connect(G_OBJECT(playlist_window), "key_press_event",
                                 G_CALLBACK(playlist_window_key_pressed), NULL);
        }

	gtk_widget_set_events(playlist_window, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);


        if (!options.playlist_is_embedded) {
                plist_menu = gtk_menu_new();
		register_toplevel_window(plist_menu, TOP_WIN_SKIN);
                init_plist_menu(plist_menu);
        }

        vbox = gtk_vbox_new(FALSE, 2);
        gtk_container_add(GTK_CONTAINER(playlist_window), vbox);

	/* this widget should not be visually noticeable, it is only used for
	   getting active and inactive playlist colors from the skin/rc file */
	playlist_color_indicator = gtk_hbox_new(FALSE, 0);
	gtk_widget_set_name(playlist_color_indicator, "playlist_color_indicator");
        gtk_box_pack_start(GTK_BOX(vbox), playlist_color_indicator, FALSE, FALSE, 0);

	/* create notebook */
	playlist_notebook = gtk_notebook_new();
	gtk_notebook_set_show_border(GTK_NOTEBOOK(playlist_notebook), FALSE);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(playlist_notebook), TRUE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(playlist_notebook),
				   options.playlist_always_show_tabs);
        gtk_box_pack_start(GTK_BOX(vbox), playlist_notebook, TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(playlist_notebook), "key_press_event",
			 G_CALLBACK(playlist_notebook_key_pressed), NULL);

	g_signal_connect(G_OBJECT(playlist_notebook), "button_press_event",
			       G_CALLBACK(playlist_notebook_clicked), NULL);

	g_signal_connect(G_OBJECT(playlist_notebook), "switch_page",
			 G_CALLBACK(notebook_switch_page), NULL);

#if (GTK_CHECK_VERSION(2,10,0))
	g_signal_connect(G_OBJECT(playlist_notebook), "page_reordered",
			 G_CALLBACK(notebook_page_reordered), NULL);
#endif /* GTK_CHECK_VERSION */

	if (playlists != NULL) {
		GList * list;
		for (list = playlists; list; list = list->next) {
			playlist_t * pl = (playlist_t *)list->data;
			if (!pl->closed) {
				create_playlist_gui(pl);
			}
		}
	}


	/* statusbar */
	if (options.enable_playlist_statusbar) {

		statusbar_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
                GTK_WIDGET_UNSET_FLAGS(statusbar_scrolledwin, GTK_CAN_FOCUS);
		gtk_widget_set_size_request(statusbar_scrolledwin, 1, -1);    /* MAGIC */
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(statusbar_scrolledwin),
					       GTK_POLICY_NEVER, GTK_POLICY_NEVER);

		statusbar_viewport = gtk_viewport_new(NULL, NULL);
		gtk_widget_set_name(statusbar_viewport, "info_viewport");

		gtk_container_add(GTK_CONTAINER(statusbar_scrolledwin), statusbar_viewport);
		gtk_box_pack_start(GTK_BOX(vbox), statusbar_scrolledwin, FALSE, TRUE, 2);

		gtk_widget_set_events(statusbar_viewport,
				      GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

		g_signal_connect(G_OBJECT(statusbar_viewport), "button_press_event",
				 G_CALLBACK(scroll_btn_pressed), NULL);
		g_signal_connect(G_OBJECT(statusbar_viewport), "button_release_event",
				 G_CALLBACK(scroll_btn_released), (gpointer)statusbar_scrolledwin);
		g_signal_connect(G_OBJECT(statusbar_viewport), "motion_notify_event",
				 G_CALLBACK(scroll_motion_notify), (gpointer)statusbar_scrolledwin);
		
		statusbar = gtk_hbox_new(FALSE, 0);
		gtk_container_set_border_width(GTK_CONTAINER(statusbar), 1);
		gtk_container_add(GTK_CONTAINER(statusbar_viewport), statusbar);
		
		statusbar_selected = gtk_label_new("");
		gtk_widget_set_name(statusbar_selected, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_selected, FALSE, TRUE, 0);
		
		statusbar_selected_label = gtk_label_new(_(" Selected: "));
		gtk_widget_set_name(statusbar_selected_label, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_selected_label, FALSE, TRUE, 0);
		
		gtk_box_pack_end(GTK_BOX(statusbar), gtk_vseparator_new(), FALSE, TRUE, 5);
		
		statusbar_total = gtk_label_new("");
		gtk_widget_set_name(statusbar_total, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_total, FALSE, TRUE, 0);
		
		statusbar_total_label = gtk_label_new(_("Total: "));
		gtk_widget_set_name(statusbar_total_label, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_total_label, FALSE, TRUE, 0);
	}


	playlist_set_font(options.override_skin_settings);

	/* bottom area of playlist window */
        hbox_bottom = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox_bottom, FALSE, TRUE, 0);

	add_button = gtk_button_new_with_label(_("Add files"));
        GTK_WIDGET_UNSET_FLAGS(add_button, GTK_CAN_FOCUS);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), add_button, _("Add files to playlist\n(Press right mouse button for menu)"), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_bottom), add_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(add_files), NULL);

	sel_button = gtk_button_new_with_label(_("Select all"));
        GTK_WIDGET_UNSET_FLAGS(sel_button, GTK_CAN_FOCUS);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), sel_button, _("Select all songs in playlist\n(Press right mouse button for menu)"), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_bottom), sel_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(sel_button), "clicked", G_CALLBACK(select_all), NULL);
	
	rem_button = gtk_button_new_with_label(_("Remove selected"));
        GTK_WIDGET_UNSET_FLAGS(rem_button, GTK_CAN_FOCUS);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), rem_button, _("Remove selected songs from playlist\n(Press right mouse button for menu)"), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_bottom), rem_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(rem_button), "clicked", G_CALLBACK(remove_sel), NULL);


        add_menu = gtk_menu_new();
	register_toplevel_window(add_menu, TOP_WIN_SKIN);

        add__dir = gtk_menu_item_new_with_label(_("Add directory"));
        gtk_menu_shell_append(GTK_MENU_SHELL(add_menu), add__dir);
        g_signal_connect_swapped(G_OBJECT(add__dir), "activate", G_CALLBACK(add__dir_cb), NULL);
	gtk_widget_show(add__dir);

        add__url = gtk_menu_item_new_with_label(_("Add URL"));
        gtk_menu_shell_append(GTK_MENU_SHELL(add_menu), add__url);
        g_signal_connect_swapped(G_OBJECT(add__url), "activate", G_CALLBACK(add__url_cb), NULL);
	gtk_widget_show(add__url);

        add__files = gtk_menu_item_new_with_label(_("Add files"));
        gtk_menu_shell_append(GTK_MENU_SHELL(add_menu), add__files);
        g_signal_connect_swapped(G_OBJECT(add__files), "activate", G_CALLBACK(add__files_cb), NULL);
	gtk_widget_show(add__files);

        g_signal_connect_swapped(G_OBJECT(add_button), "event", G_CALLBACK(add_cb), NULL);


        sel_menu = gtk_menu_new();
	register_toplevel_window(sel_menu, TOP_WIN_SKIN);

        sel__none = gtk_menu_item_new_with_label(_("Select none"));
        gtk_menu_shell_append(GTK_MENU_SHELL(sel_menu), sel__none);
        g_signal_connect_swapped(G_OBJECT(sel__none), "activate", G_CALLBACK(sel__none_cb), NULL);
	gtk_widget_show(sel__none);

        sel__inv = gtk_menu_item_new_with_label(_("Invert selection"));
        gtk_menu_shell_append(GTK_MENU_SHELL(sel_menu), sel__inv);
        g_signal_connect_swapped(G_OBJECT(sel__inv), "activate", G_CALLBACK(sel__inv_cb), NULL);
	gtk_widget_show(sel__inv);

        sel__all = gtk_menu_item_new_with_label(_("Select all"));
        gtk_menu_shell_append(GTK_MENU_SHELL(sel_menu), sel__all);
        g_signal_connect_swapped(G_OBJECT(sel__all), "activate", G_CALLBACK(sel__all_cb), NULL);
	gtk_widget_show(sel__all);

        g_signal_connect_swapped(G_OBJECT(sel_button), "event", G_CALLBACK(sel_cb), NULL);


        rem_menu = gtk_menu_new();
	register_toplevel_window(rem_menu, TOP_WIN_SKIN);

        rem__all = gtk_menu_item_new_with_label(_("Remove all"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), rem__all);
        g_signal_connect_swapped(G_OBJECT(rem__all), "activate", G_CALLBACK(rem__all_cb), NULL);
    	gtk_widget_show(rem__all);

        rem__dead = gtk_menu_item_new_with_label(_("Remove dead"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), rem__dead);
        g_signal_connect_swapped(G_OBJECT(rem__dead), "activate", G_CALLBACK(rem__dead_cb), NULL);
    	gtk_widget_show(rem__dead);

        cut__sel = gtk_menu_item_new_with_label(_("Cut selected"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), cut__sel);
        g_signal_connect_swapped(G_OBJECT(cut__sel), "activate", G_CALLBACK(cut__sel_cb), NULL);
    	gtk_widget_show(cut__sel);

        rem__sel = gtk_menu_item_new_with_label(_("Remove selected"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), rem__sel);
        g_signal_connect_swapped(G_OBJECT(rem__sel), "activate", G_CALLBACK(rem__sel_cb), NULL);
    	gtk_widget_show(rem__sel);

        g_signal_connect_swapped(G_OBJECT(rem_button), "event", G_CALLBACK(rem_cb), NULL);
}
       

void
playlist_ensure_tab_exists() {

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook)) == 0) {
		playlist_tab_new(NULL);
	}
}


void
show_playlist(void) {

	options.playlist_on = 1;

	if (!options.playlist_is_embedded) {
		gtk_window_move(GTK_WINDOW(playlist_window), options.playlist_pos_x, options.playlist_pos_y);
		gtk_window_resize(GTK_WINDOW(playlist_window), options.playlist_size_x, options.playlist_size_y);
		register_toplevel_window(playlist_window, TOP_WIN_SKIN | TOP_WIN_TRAY);
	} else {
		gtk_window_get_size(GTK_WINDOW(main_window), &options.main_size_x, &options.main_size_y);
		gtk_window_resize(GTK_WINDOW(main_window), options.main_size_x, options.main_size_y + options.playlist_size_y + 6);
	}

        gtk_widget_show_all(playlist_window);
}

void
hide_playlist(void) {

	options.playlist_on = 0;
	if (!options.playlist_is_embedded) {
		gtk_window_get_position(GTK_WINDOW(playlist_window), &options.playlist_pos_x, &options.playlist_pos_y);
		gtk_window_get_size(GTK_WINDOW(playlist_window), &options.playlist_size_x, &options.playlist_size_y);
		register_toplevel_window(playlist_window, TOP_WIN_SKIN);
	} else {
		options.playlist_size_x = playlist_window->allocation.width;
		options.playlist_size_y = playlist_window->allocation.height;
		gtk_window_get_size(GTK_WINDOW(main_window), &options.main_size_x, &options.main_size_y);
	}

        gtk_widget_hide(playlist_window);

	if (options.playlist_is_embedded) {
		gtk_window_resize(GTK_WINDOW(main_window), options.main_size_x, options.main_size_y - options.playlist_size_y - 6);
	}

}


xmlNodePtr
save_track_node(playlist_t * pl, GtkTreeIter * piter, xmlNodePtr root, char * nodeID) {

        gchar str[32];
        xmlNodePtr node;
	playlist_data_t * data;


	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), piter, PL_COL_DATA, &data,  -1);

	node = xmlNewTextChild(root, NULL, (const xmlChar*) nodeID, NULL);
	xmlNewTextChild(node, NULL, (const xmlChar*) "artist", (const xmlChar*) data->artist);
	xmlNewTextChild(node, NULL, (const xmlChar*) "album", (const xmlChar*) data->album);

	if (strcmp(nodeID, "record")) {

		char * file = NULL;

		xmlNewTextChild(node, NULL, (const xmlChar*) "title", (const xmlChar*) data->title);

		if (cdda_is_cdtrack(data->file) || httpc_is_url(data->file)) {
			file = g_strdup(data->file);
		} else {
			file = g_filename_to_uri(data->file, NULL, NULL);
		}
		xmlNewTextChild(node, NULL, (const xmlChar*) "file", (const xmlChar*) file);
		g_free(file);

		snprintf(str, 31, "%f", data->voladj);
		xmlNewTextChild(node, NULL, (const xmlChar*) "voladj", (const xmlChar*) str);

		snprintf(str, 31, "%u", data->size);
		xmlNewTextChild(node, NULL, (const xmlChar*) "size", (const xmlChar*) str);
	} else if (IS_PL_ACTIVE(data)){
		snprintf(str, 31, "%u", data->ntracks);
		xmlNewTextChild(node, NULL, (const xmlChar*) "ntracks", (const xmlChar*) str);

		snprintf(str, 31, "%u", data->actrack);
		xmlNewTextChild(node, NULL, (const xmlChar*) "actrack", (const xmlChar*) str);
	}

	snprintf(str, 31, "%f", data->duration);
	xmlNewTextChild(node, NULL, (const xmlChar*) "duration", (const xmlChar*) str);

	if (IS_PL_ACTIVE(data)) {
		xmlNewTextChild(node, NULL, (const xmlChar*) "active", NULL);
	}

	if (IS_PL_COVER(data)) {
		xmlNewTextChild(node, NULL, (const xmlChar*) "cover", NULL);
	}
	
	return node;
}

void
playlist_save_data(playlist_t * pl, xmlNodePtr dest) {

        gint i = 0;
        GtkTreeIter iter;

	if (pl->name_set) {
		xmlNewTextChild(dest, NULL, (const xmlChar*) "name", (const xmlChar*) pl->name);
	}

	if (pl->playing) {
		xmlNewTextChild(dest, NULL, (const xmlChar*) "active", NULL);
	}

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter);

		if (n) { /* album node */
			gint j = 0;
			GtkTreeIter iter_child;
			xmlNodePtr node;

			node = save_track_node(pl, &iter, dest, "record");
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store),
							     &iter_child, &iter, j++)) {
				save_track_node(pl, &iter_child, node, "track");
			}
		} else { /* track node */
			save_track_node(pl, &iter, dest, "track");
		}
        }
}


void
playlist_save(playlist_t * pl, char * filename) {

        xmlDocPtr doc;
        xmlNodePtr root;

        doc = xmlNewDoc((const xmlChar*) "1.0");
        root = xmlNewNode(NULL, (const xmlChar*) "aqualung_playlist");
        xmlDocSetRootElement(doc, root);

	playlist_save_data(pl, root);

        xmlSaveFormatFile(filename, doc, 1);
	xmlFreeDoc(doc);
}

void
playlist_save_all(char * filename) {

        xmlDocPtr doc;
        xmlNodePtr root;
	xmlNodePtr tab;

    	GList * list;

        doc = xmlNewDoc((const xmlChar*) "1.0");
        root = xmlNewNode(NULL, (const xmlChar*) "aqualung_playlist_tabs");
        xmlDocSetRootElement(doc, root);

	for (list = playlists; list; list = list->next) {
		playlist_t * pl = (playlist_t *)list->data;

		if (!pl->closed) {
			tab = xmlNewTextChild(root, NULL, (const xmlChar*) "tab", NULL);
			playlist_save_data(pl, tab);
		}
	}

	{
		int i = gtk_notebook_get_current_page(GTK_NOTEBOOK(playlist_notebook));
		char str[32];
		snprintf(str, 31, "%d", i);
		xmlNewTextChild(root, NULL, (const xmlChar*) "current_page", (const xmlChar*)str);
	}

        xmlSaveFormatFile(filename, doc, 1);
	xmlFreeDoc(doc);
}


xmlChar *
parse_playlist_name(xmlDocPtr doc, xmlNodePtr _cur) {

	xmlNodePtr cur;

        for (cur = _cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
                if (!xmlStrcmp(cur->name, (const xmlChar *)"name")) {
                        return xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                }
	}

	return NULL;
}

int
parse_playlist_is_active(xmlDocPtr doc, xmlNodePtr _cur) {

	xmlNodePtr cur;

        for (cur = _cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
                if (!xmlStrcmp(cur->name, (const xmlChar *)"active")) {
                        return 1;
                }
	}

	return 0;
}

void
parse_playlist_track(playlist_transfer_t * pt, xmlDocPtr doc, xmlNodePtr _cur, int flags) {

        xmlChar * key;
	xmlNodePtr cur;
	playlist_data_t * data = NULL;
	int is_album_node;

	
	if ((data = playlist_data_new()) == NULL) {
		return;
	}

	data->flags = flags;
	is_album_node = !xmlStrcmp(_cur->name, (const xmlChar *)"record");

        for (cur = _cur->xmlChildrenNode; cur != NULL; cur = cur->next) {

                if (!xmlStrcmp(cur->name, (const xmlChar *)"artist")) {
                        if ((key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)) != NULL) {
				data->artist = strndup((char *)key, MAXLEN-1);
				xmlFree(key);
			}
		} else if (!xmlStrcmp(cur->name, (const xmlChar *)"album")) {
                        if ((key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)) != NULL) {
				data->album = strndup((char *)key, MAXLEN-1);
				xmlFree(key);
			}
		} else if (!xmlStrcmp(cur->name, (const xmlChar *)"title")) {
                        if ((key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)) != NULL) {
				data->title = strndup((char *)key, MAXLEN-1);
				xmlFree(key);
			}
                } else if (!xmlStrcmp(cur->name, (const xmlChar *)"file")) {
                        if ((key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)) != NULL) {
				gchar * tmp;
				if ((tmp = g_filename_from_uri((char *) key, NULL, NULL))) {
					data->file = strndup(tmp, MAXLEN-1);
					g_free(tmp);
				} else {
					data->file = strndup((char *)key, MAXLEN-1);
				}
				xmlFree(key);
			}
                } else if (!xmlStrcmp(cur->name, (const xmlChar *)"voladj")) {
                        if ((key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)) != NULL) {
				data->voladj = convf((char *)key);
				xmlFree(key);
			}
                } else if (!xmlStrcmp(cur->name, (const xmlChar *)"duration")) {
                        if ((key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)) != NULL) {
				data->duration = convf((char *)key);
				xmlFree(key);
			}
                } else if (!xmlStrcmp(cur->name, (const xmlChar *)"size")) {
                        if ((key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)) != NULL) {
				sscanf((char *)key, "%u", &data->size);
				xmlFree(key);
			}
                } else if (!xmlStrcmp(cur->name, (const xmlChar *)"ntracks")) {
                        if ((key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)) != NULL) {
				sscanf((char *)key, "%hu", &data->ntracks);
				xmlFree(key);
			}
                } else if (!xmlStrcmp(cur->name, (const xmlChar *)"actrack")) {
                        if ((key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)) != NULL) {
				sscanf((char *)key, "%hu", &data->actrack);
				xmlFree(key);
			}
                } else if (!xmlStrcmp(cur->name, (const xmlChar *)"active")) {
			PL_SET_FLAG(data, PL_FLAG_ACTIVE);
                } else if (!xmlStrcmp(cur->name, (const xmlChar *)"cover")) {
			PL_SET_FLAG(data, PL_FLAG_COVER);
		}
	}

	if (IS_PL_ALBUM_NODE(data) || data->file != NULL) {
		playlist_thread_add_to_list(pt, data);
	}

	if (is_album_node) {
		for (cur = _cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
			if (!xmlStrcmp(cur->name, (const xmlChar *)"track")) {
				parse_playlist_track(pt, doc, cur, PL_FLAG_ALBUM_CHILD);
			}
		}
	}
}

void *
playlist_load_xml_thread(void * arg) {

	playlist_transfer_t * pt = (playlist_transfer_t *)arg;
        xmlDocPtr doc = (xmlDocPtr)pt->xml_doc;
        xmlNodePtr cur = (xmlNodePtr)pt->xml_node;

	AQUALUNG_THREAD_DETACH();

	AQUALUNG_MUTEX_LOCK(pt->pl->thread_mutex);

	for (cur = cur->xmlChildrenNode; cur != NULL && !pt->pl->thread_stop; cur = cur->next) {

		if (!xmlStrcmp(cur->name, (const xmlChar *)"track")) {
			parse_playlist_track(pt, doc, cur, 0);
		}

		if (!xmlStrcmp(cur->name, (const xmlChar *)"record")) {
			parse_playlist_track(pt, doc, cur, PL_FLAG_ALBUM_NODE);
		}
	}

	playlist_thread_add_to_list(pt, NULL);

	AQUALUNG_MUTEX_UNLOCK(pt->pl->thread_mutex);

	playlist_transfer_free(pt);

	return NULL;
}

void
playlist_load_xml_single(char * filename, playlist_transfer_t * pt) {

        xmlDocPtr doc;
        xmlNodePtr cur;

        doc = xmlParseFile(filename);
        if (doc == NULL) {
                fprintf(stderr, "An XML error occured while parsing %s\n", filename);
                return;
        }

        cur = xmlDocGetRootElement(doc);
        if (cur == NULL) {
                fprintf(stderr, "empty XML document\n");
                xmlFreeDoc(doc);
                return;
        }

	if (xmlStrcmp(cur->name, (const xmlChar *)"aqualung_playlist")) {
		fprintf(stderr, "XML document of the wrong type\n");
		xmlFreeDoc(doc);
		return;
	}

	if (!pt->pl->name_set || pt->clear) {
		xmlChar * pl_name = parse_playlist_name(doc, cur);
		if (pl_name != NULL) {
			playlist_rename(pt->pl, (char *)pl_name);
			xmlFree(pl_name);
		}
	}

	if (playlist_get_playing() == NULL) {
		playlist_set_playing(pt->pl, parse_playlist_is_active(doc, cur));
	}

	pt->xml_doc = doc;
	pt->xml_node = cur;

	playlist_progress_bar_show(pt->pl);
	AQUALUNG_THREAD_CREATE(pt->pl->thread_id, NULL,
			       playlist_load_xml_thread, pt);
}

void
playlist_load_xml_multi(char * filename, int start_playback) {

        xmlDocPtr doc;
        xmlNodePtr cur;
	int * ref = NULL;
	int current = -1;

        doc = xmlParseFile(filename);
        if (doc == NULL) {
                fprintf(stderr, "An XML error occured while parsing %s\n", filename);
                return;
        }

        cur = xmlDocGetRootElement(doc);
        if (cur == NULL) {
                fprintf(stderr, "empty XML document\n");
                xmlFreeDoc(doc);
                return;
        }

	if (xmlStrcmp(cur->name, (const xmlChar *)"aqualung_playlist_tabs")) {
		fprintf(stderr, "XML document of the wrong type\n");
		xmlFreeDoc(doc);
		return;
	}

	if ((ref = (int *)malloc(sizeof(int *))) == NULL) {
		fprintf(stderr, "playlist_load_xml_multi(): malloc error\n");
		return;
	}

	*ref = 1;

	for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {

		if (!xmlStrcmp(cur->name, (const xmlChar *)"tab")) {

			playlist_t * pl = NULL;
			playlist_transfer_t * pt = NULL;
			xmlChar * pl_name = NULL;

			pl_name = parse_playlist_name(doc, cur);
			pl = playlist_tab_new((char *)pl_name);
			if (pl_name != NULL) {
				xmlFree(pl_name);
			}

			if (playlist_get_playing() == NULL) {
				playlist_set_playing(pl, parse_playlist_is_active(doc, cur));
			}

			pt = playlist_transfer_new(pl);

			pt->xml_doc = doc;
			pt->xml_node = cur;
			pt->xml_ref = ref;

			*ref += 1;

			pt->start_playback = start_playback;
			start_playback = 0;

			playlist_progress_bar_show(pt->pl);
			AQUALUNG_THREAD_CREATE(pt->pl->thread_id, NULL,
					       playlist_load_xml_thread, pt);

		} else if (!xmlStrcmp(cur->name, (const xmlChar *)"current_page")) {
                        xmlChar * key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				current = atoi((char *)key);
				xmlFree(key);
                        }
		}

	}

	*ref -= 1;

	gtk_notebook_set_current_page(GTK_NOTEBOOK(playlist_notebook), current);
}

void *
playlist_load_m3u_thread(void * arg) {

	FILE * f;
	gint c;
	gint i = 0;
	gint n;
	gchar * str;
	gchar line[MAXLEN];
	gchar name[MAXLEN];
	gchar path[MAXLEN];
	gchar tmp[MAXLEN];
	gchar pl_dir[MAXLEN];
	gint have_name = 0;

	playlist_transfer_t * pt = (playlist_transfer_t *)arg;
	playlist_data_t * data = NULL;


	AQUALUNG_THREAD_DETACH();

	AQUALUNG_MUTEX_LOCK(pt->pl->thread_mutex);

	if ((f = fopen(pt->filename, "rb")) == NULL) {
		fprintf(stderr, "unable to open .m3u playlist: %s\n", pt->filename);
		goto finish;
	}

	str = strrchr(pt->filename, '/');
	for (i = 0; (i < (str - pt->filename)) && (i < MAXLEN-1); i++) {
 		pl_dir[i] = pt->filename[i];
 	}
 	pl_dir[i] = '\0';

	i = 0;
	while ((c = fgetc(f)) != EOF && !pt->pl->thread_stop) {
		if ((c != '\n') && (c != '\r') && (i < MAXLEN)) {
			if ((i > 0) || ((c != ' ') && (c != '\t'))) {
				line[i++] = c;
			}
		} else {
			line[i] = '\0';
			if (i == 0) {
				continue;
			}
			i = 0;

			if (strstr(line, "#EXTM3U") == line) {
				continue;
			}
			
			if (strstr(line, "#EXTINF:") == line) {

				char str_duration[64];
				int cnt = 0;
				
				/* We parse the timing, but throw it away. 
				   This may change in the future. */
				while ((line[cnt+8] >= '0') && (line[cnt+8] <= '9') && cnt < 63) {
					str_duration[cnt] = line[cnt+8];
					++cnt;
				}
				str_duration[cnt] = '\0';
				snprintf(name, MAXLEN-1, "%s", line+cnt+9);
				have_name = 1;

			} else {
                                if (!httpc_is_url(line)) {
                                        /* safeguard against C:\ stuff */
                                        if ((line[1] == ':') && (line[2] == '\\')) {
                                                fprintf(stderr, "Ignoring playlist item: %s\n", line);
                                                i = 0;
                                                have_name = 0;
                                                continue;
                                        }

                                        snprintf(path, MAXLEN-1, "%s", line);

                                        /* path curing: turn \-s into /-s */
                                        for (n = 0; n < strlen(path); n++) {
                                                if (path[n] == '\\')
                                                        path[n] = '/';
                                        }

                                        if (path[0] != '/') {
                                                strncpy(tmp, path, MAXLEN-1);
                                                snprintf(path, MAXLEN-1, "%s/%s", pl_dir, tmp);
                                        }

                                        if (!have_name) {
                                                gchar * ch;
                                                if ((ch = strrchr(path, '/')) != NULL) {
                                                        ++ch;
                                                        snprintf(name, MAXLEN-1, "%s", ch);
                                                } else {
                                                        fprintf(stderr,
                                                                "warning: ain't this a directory? : %s\n", path);
                                                        snprintf(name, MAXLEN-1, "%s", path);
                                                }
                                        }

                                        data = playlist_filemeta_get(path);
                                } else {

                                        data = playlist_filemeta_get(line);
                                }

				if (have_name) {
					free_strdup(&data->title, name);
				}

				have_name = 0;

				if (data == NULL) {
					fprintf(stderr, "load_m3u(): playlist_filemeta_get() returned NULL\n");
				} else {
					playlist_thread_add_to_list(pt, data);
				}
			}
		}
	}

	playlist_thread_add_to_list(pt, NULL);

 finish:

	AQUALUNG_MUTEX_UNLOCK(pt->pl->thread_mutex);

	playlist_transfer_free(pt);

	return NULL;
}


void
load_pls_load(playlist_transfer_t * pt, char * file, char * title, gint * have_file, gint * have_title) {

	playlist_data_t * data = NULL;

	if (*have_file == 0) {
		return;
	}

	data = playlist_filemeta_get(file);

	if (*have_title) {
		free_strdup(&data->title, title);
	}

	if (data == NULL) {
		fprintf(stderr, "load_pls_load(): playlist_filemeta_get() returned NULL\n");
	} else {
		playlist_thread_add_to_list(pt, data);
	}

	*have_file = *have_title = 0;
}

void *
playlist_load_pls_thread(void * arg) {

	FILE * f;
	gint c;
	gint i = 0;
	gint n;
	gchar * str;
	gchar line[MAXLEN];
	gchar file[MAXLEN];
	gchar title[MAXLEN];
	gchar tmp[MAXLEN];
	gchar pl_dir[MAXLEN];
	gint have_file = 0;
	gint have_title = 0;
	gchar numstr_file[10];
	gchar numstr_title[10];

	playlist_transfer_t * pt = (playlist_transfer_t *)arg;


	AQUALUNG_THREAD_DETACH();

	AQUALUNG_MUTEX_LOCK(pt->pl->thread_mutex);

	if ((f = fopen(pt->filename, "rb")) == NULL) {
		fprintf(stderr, "unable to open .pls playlist: %s\n", pt->filename);
		goto finish;
	}

	str = strrchr(pt->filename, '/');
	for (i = 0; (i < (str - pt->filename)) && (i < MAXLEN-1); i++) {
 		pl_dir[i] = pt->filename[i];
 	}
 	pl_dir[i] = '\0';

	i = 0;
	while ((c = fgetc(f)) != EOF && !pt->pl->thread_stop) {
		if ((c != '\n') && (c != '\r') && (i < MAXLEN)) {
			if ((i > 0) || ((c != ' ') && (c != '\t'))) {
				line[i++] = c;
			}
		} else {
			line[i] = '\0';
			if (i == 0)
				continue;
			i = 0;

			if (strstr(line, "[playlist]") == line) {
				continue;
			}
			if (strcasestr(line, "NumberOfEntries") == line) {
				continue;
			}
			if (strstr(line, "Version") == line) {
				continue;
			}
			
			if (strstr(line, "File") == line) {

				char numstr[10];
				char * ch;
				int m;

				if (have_file) {
					load_pls_load(pt, file, title, &have_file, &have_title);
				}

				if ((ch = strstr(line, "=")) == NULL) {
					fprintf(stderr, "Syntax error in %s\n", pt->filename);
					goto finish;
				}
				
				++ch;
				while ((*ch == ' ') || (*ch == '\t'))
					++ch;

                                if (!httpc_is_url(ch)) {

                                        m = 0;
                                        for (n = 0; (line[n+4] != '=') && (m < sizeof(numstr)); n++) {
                                                if ((line[n+4] != ' ') && (line[n+4] != '\t'))
                                                        numstr[m++] = line[n+4];
                                        }
                                        numstr[m] = '\0';
                                        strncpy(numstr_file, numstr, sizeof(numstr_file));

                                        /* safeguard against C:\ stuff */
                                        if ((ch[1] == ':') && (ch[2] == '\\')) {
                                                fprintf(stderr, "Ignoring playlist item: %s\n", ch);
                                                i = 0;
                                                have_file = have_title = 0;
                                                continue;
                                        }

                                        snprintf(file, MAXLEN-1, "%s", ch);

                                        /* path curing: turn \-s into /-s */

                                        for (n = 0; n < strlen(file); n++) {
                                                if (file[n] == '\\')
                                                        file[n] = '/';
                                        }

                                        if (file[0] != '/') {
                                                strncpy(tmp, file, MAXLEN-1);
                                                snprintf(file, MAXLEN-1, "%s/%s", pl_dir, tmp);
                                        }

                                } else {
                                        snprintf(file, MAXLEN-1, "%s", ch);
                                }

				have_file = 1;

			} else if (strstr(line, "Title") == line) {

				char numstr[10];
				char * ch;
				int m;

				if ((ch = strstr(line, "=")) == NULL) {
					fprintf(stderr, "Syntax error in %s\n", pt->filename);
					goto finish;
				}
				
				++ch;
				while ((*ch == ' ') || (*ch == '\t'))
					++ch;

				m = 0;
				for (n = 0; (line[n+5] != '=') && (m < sizeof(numstr)); n++) {
					if ((line[n+5] != ' ') && (line[n+5] != '\t'))
						numstr[m++] = line[n+5];
				}
				numstr[m] = '\0';
				strncpy(numstr_title, numstr, sizeof(numstr_title));

				snprintf(title, MAXLEN-1, "%s", ch);
				have_title = 1;


			} else if (strstr(line, "Length") == line) {

				/* We parse the timing, but throw it away. 
				   This may change in the future. */

				char * ch;
				if ((ch = strstr(line, "=")) == NULL) {
					fprintf(stderr, "Syntax error in %s\n", pt->filename);
					goto finish;
				}
				
				++ch;
				while ((*ch == ' ') || (*ch == '\t'))
					++ch;

			} else {
				fprintf(stderr, 
					"Syntax error: invalid line in playlist: %s\n", line);
				goto finish;
			}

			if (!have_file || !have_title) {
				continue;
			}
			
			if (strcmp(numstr_file, numstr_title) != 0) {
				continue;
			}

			load_pls_load(pt, file, title, &have_file, &have_title);
		}
	}

	load_pls_load(pt, file, title, &have_file, &have_title);

	playlist_thread_add_to_list(pt, NULL);

 finish:

	AQUALUNG_MUTEX_UNLOCK(pt->pl->thread_mutex);

	playlist_transfer_free(pt);

	return NULL;
}


int
playlist_get_type(char * filename) {

	FILE * f;
	gchar buf1[] = "<?xml version=\"1.0\"?>\n<aqualung_playlist_tabs"; /* this must be the 1st */
	gchar buf2[] = "<?xml version=\"1.0\"?>\n<aqualung_playlist";      /* this must be the 2nd */
	gchar inbuf[48];
	gint len;

	if ((f = fopen(filename, "rb")) == NULL) {
		return 0;
	}

	len = fread(inbuf, 1, 47, f);
	fclose(f);

	inbuf[len] = '\0';

	if (strstr(inbuf, buf1) == inbuf) {
		return PLAYLIST_XML_MULTI;
	}

	if (strstr(inbuf, buf2) == inbuf) {
		return PLAYLIST_XML_SINGLE;
	}

	/* not Aqualung playlist, decide by file extension */

	len = strlen(filename);
	if (len < 5) {
		return PLAYLIST_INVALID;
	}

	if ((filename[len-4] == '.') &&
	    ((filename[len-3] == 'm') || (filename[len-3] == 'M')) &&
	    (filename[len-2] == '3') &&
	    ((filename[len-1] == 'u') || (filename[len-1] == 'U'))) {
		return PLAYLIST_M3U;
	}

	if ((filename[len-4] == '.') &&
	    ((filename[len-3] == 'p') || (filename[len-3] == 'P')) &&
	    ((filename[len-2] == 'l') || (filename[len-2] == 'L')) &&
	    ((filename[len-1] == 's') || (filename[len-1] == 'S'))) {
		return PLAYLIST_PLS;
	}

	return PLAYLIST_INVALID;
}


void
init_plist_menu(GtkWidget *append_menu) {

	GtkWidget * separator;

        plist__tab_new = gtk_menu_item_new_with_label(_("New tab"));
        plist__save = gtk_menu_item_new_with_label(_("Save playlist"));
        plist__save_all = gtk_menu_item_new_with_label(_("Save all playlists"));
        plist__load_tab = gtk_menu_item_new_with_label(_("Load playlist in new tab"));
        plist__load = gtk_menu_item_new_with_label(_("Load playlist"));
        plist__enqueue = gtk_menu_item_new_with_label(_("Enqueue playlist"));
#ifdef HAVE_IFP
        plist__send_songs_to_iriver = gtk_menu_item_new_with_label(_("Send to iFP device"));
#endif  /* HAVE_IFP */
#ifdef HAVE_EXPORT
        plist__export = gtk_menu_item_new_with_label(_("Export files"));
#endif /* HAVE_EXPORT */
        plist__rva = gtk_menu_item_new_with_label(_("Calculate RVA"));
        plist__rva_menu = gtk_menu_new();
        plist__rva_separate = gtk_menu_item_new_with_label(_("Separate"));
        plist__rva_average = gtk_menu_item_new_with_label(_("Average"));
        plist__reread_file_meta = gtk_menu_item_new_with_label(_("Reread file metadata"));
        plist__fileinfo = gtk_menu_item_new_with_label(_("File info..."));
        plist__search = gtk_menu_item_new_with_label(_("Search..."));

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__tab_new);

	separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), separator);
	gtk_widget_show(separator);

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__save);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__save_all);

	separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), separator);
	gtk_widget_show(separator);

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__load_tab);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__load);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__enqueue);

	separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), separator);
	gtk_widget_show(separator);

#ifdef HAVE_IFP
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__send_songs_to_iriver);
#endif  /* HAVE_IFP */
#ifdef HAVE_EXPORT
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__export);
#endif /* HAVE_EXPORT */

	separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), separator);
	gtk_widget_show(separator);

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__rva);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(plist__rva), plist__rva_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(plist__rva_menu), plist__rva_separate);
        gtk_menu_shell_append(GTK_MENU_SHELL(plist__rva_menu), plist__rva_average);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__reread_file_meta);

	separator = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), separator);
	gtk_widget_show(separator);

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__fileinfo);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__search);

        g_signal_connect_swapped(G_OBJECT(plist__tab_new), "activate", G_CALLBACK(tab__new_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__save), "activate", G_CALLBACK(plist__save_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__save_all), "activate", G_CALLBACK(plist__save_all_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__load_tab), "activate", G_CALLBACK(plist__load_tab_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__load), "activate", G_CALLBACK(plist__load_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__enqueue), "activate", G_CALLBACK(plist__enqueue_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__rva_separate), "activate", G_CALLBACK(plist__rva_separate_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__rva_average), "activate", G_CALLBACK(plist__rva_average_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__reread_file_meta), "activate", G_CALLBACK(plist__reread_file_meta_cb), NULL);

#ifdef HAVE_IFP
        g_signal_connect_swapped(G_OBJECT(plist__send_songs_to_iriver), "activate", G_CALLBACK(plist__send_songs_to_iriver_cb), NULL);
#endif  /* HAVE_IFP */
#ifdef HAVE_EXPORT
        g_signal_connect_swapped(G_OBJECT(plist__export), "activate", G_CALLBACK(plist__export_cb), NULL);
#endif /* HAVE_EXPORT */

        g_signal_connect_swapped(G_OBJECT(plist__fileinfo), "activate", G_CALLBACK(plist__fileinfo_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__search), "activate", G_CALLBACK(plist__search_cb), NULL);

        gtk_widget_show(plist__tab_new);
        gtk_widget_show(plist__save);
        gtk_widget_show(plist__save_all);
        gtk_widget_show(plist__load_tab);
        gtk_widget_show(plist__load);
        gtk_widget_show(plist__enqueue);
        gtk_widget_show(plist__rva);
        gtk_widget_show(plist__rva_separate);
        gtk_widget_show(plist__rva_average);
        gtk_widget_show(plist__reread_file_meta);
#ifdef HAVE_IFP
        gtk_widget_show(plist__send_songs_to_iriver);
#endif  /* HAVE_IFP */
#ifdef HAVE_EXPORT
        gtk_widget_show(plist__export);
#endif /* HAVE_EXPORT */
        gtk_widget_show(plist__fileinfo);
        gtk_widget_show(plist__search);
}


void
show_active_position_in_playlist(playlist_t * pl) {

        GtkTreeIter iter;
	playlist_data_t * data;
        int flag = 0;

        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(pl->store), &iter)) {

                do {
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_DATA, &data, -1);
		        if (IS_PL_ACTIVE(data)) {
                                flag = 1;
                                break;
                        }
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(pl->store), &iter));

                if (!flag) {
                        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, 0);
                }

                set_cursor_in_playlist(pl, &iter, TRUE);
        }
}

void
show_active_position_in_playlist_toggle(playlist_t * pl) {

        GtkTreeIter iter, iter_child;
	playlist_data_t * data;
        int flag, cflag, j;


        flag = cflag = 0;

        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(pl->store), &iter)) {

                do {
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_DATA, &data, -1);
		        if (IS_PL_ACTIVE(data)) {
                                flag = 1;
		                if (gtk_tree_selection_iter_is_selected(pl->select, &iter) &&
                                    gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter)) {

					GtkTreePath * visible_path;

                                        gtk_tree_view_get_cursor(GTK_TREE_VIEW(pl->view), &visible_path, NULL);

                                        if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(pl->view), visible_path)) {
                                                gtk_tree_view_expand_row(GTK_TREE_VIEW(pl->view), visible_path, FALSE);
                                        }

					gtk_tree_path_free(visible_path);

                                        j = 0;
                                        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, &iter, j++)) {
                                                gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter_child,
								   PL_COL_DATA, &data, -1);
                                                if (IS_PL_ACTIVE(data)) {
                                                        cflag = 1;
                                                        break;
                                                }
                                        }
                                }
                                break;
                        }
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(pl->store), &iter));

                if (flag) {
                        if (cflag) {
                                set_cursor_in_playlist(pl, &iter_child, TRUE);
                        } else {
                                set_cursor_in_playlist(pl, &iter, TRUE);
                        }
                }
        }
}


void
expand_collapse_album_node(playlist_t * pl) {

	GtkTreeIter iter, iter_child;
	gint i, j;
        GtkTreePath *path;

        i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(pl->select, &iter) &&
                    gtk_tree_model_iter_n_children(GTK_TREE_MODEL(pl->store), &iter)) {

                        gtk_tree_view_get_cursor(GTK_TREE_VIEW(pl->view), &path, NULL);

                        if (path && gtk_tree_view_row_expanded(GTK_TREE_VIEW(pl->view), path)) {
                                gtk_tree_view_collapse_row(GTK_TREE_VIEW(pl->view), path);
                        } else {
                                gtk_tree_view_expand_row(GTK_TREE_VIEW(pl->view), path, FALSE);
                        }
                        
                        continue;
                }

                j = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(pl->store), &iter_child, &iter, j++)) {

                        if (gtk_tree_selection_iter_is_selected(pl->select, &iter_child) &&
                            gtk_tree_model_iter_parent(GTK_TREE_MODEL(pl->store), &iter, &iter_child)) {

                                path = gtk_tree_model_get_path (GTK_TREE_MODEL(pl->store), &iter);

                                if (path) {
                                        gtk_tree_view_collapse_row(GTK_TREE_VIEW(pl->view), path);
                                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(pl->view), path, NULL, FALSE);
                                }
                        }
                }
        }
}


static gboolean
pl_progress_bar_update(gpointer data) {

	playlist_t * pl = (playlist_t *)data;

	if (pl->progbar != NULL) {
		gtk_progress_bar_pulse(GTK_PROGRESS_BAR(pl->progbar));
		return TRUE;
	}

	return FALSE;
}

static void
pl_progress_bar_stop_clicked(GtkWidget * widget, gpointer data) {

	playlist_t * pl = (playlist_t *)data;

	pl->thread_stop = 1;
}

void
playlist_progress_bar_show(playlist_t * pl) {

	pl->progbar_semaphore++;

	if (pl->progbar != NULL) {
		return;
	}

	pl->thread_stop = 0;

	playlist_stats_set_busy();

	pl->progbar = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(pl->progbar_container), pl->progbar, TRUE, TRUE, 0);

	pl->progbar_stop_button = gtk_button_new_with_label(_("Stop adding files"));
        gtk_box_pack_start(GTK_BOX(pl->progbar_container), pl->progbar_stop_button, FALSE, FALSE, 0);
        g_signal_connect(G_OBJECT(pl->progbar_stop_button), "clicked",
			 G_CALLBACK(pl_progress_bar_stop_clicked), pl);

	gtk_widget_show_all(pl->progbar_container);

	g_timeout_add(200, pl_progress_bar_update, pl);
}


void
playlist_progress_bar_hide(playlist_t * pl) {

	if (pl->progbar != NULL) {
		gtk_widget_destroy(pl->progbar);
		pl->progbar = NULL;
	}

	if (pl->progbar_stop_button != NULL) {
		gtk_widget_destroy(pl->progbar_stop_button);
		pl->progbar_stop_button = NULL;
	}
}

void
playlist_progress_bar_hide_all() {

	GList * list;

	for (list = playlists; list; list = list->next) {
		playlist_progress_bar_hide((playlist_t *)list->data);
	}
}


int
playlist_unlink_files_foreach(playlist_t * pl, GtkTreeIter * iter, void * data) {

	playlist_data_t * pldata;

	gtk_tree_model_get(GTK_TREE_MODEL(pl->store), iter, PL_COL_DATA, &pldata, -1);

	if (g_file_test(pldata->file, G_FILE_TEST_EXISTS)) {

		GtkListStore * store = (GtkListStore *)data;
		GtkTreeIter i;

		gtk_list_store_append(store, &i);
		gtk_list_store_set(store, &i, 0, pldata->file, 1, gtk_tree_iter_copy(iter), -1);
	}

	return 0;
}

void
playlist_unlink_files(playlist_t * pl) {

	GtkWidget * view;
	GtkWidget * scrwin;
	GtkWidget * viewport;
	GtkCellRenderer * cell;
	GtkTreeViewColumn * column;
	GtkListStore * store;
	GtkTreeIter iter;
	int res;
	int i;
	int error = 0;


	store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);

	playlist_foreach_selected(pl, playlist_unlink_files_foreach, store);

	if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL) == 0) {
		return;
	}

	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Files selected for removal"), cell, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), GTK_TREE_VIEW_COLUMN(column));

	viewport = gtk_viewport_new(NULL, NULL);
	scrwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(scrwin, -1, 200);

	gtk_container_add(GTK_CONTAINER(viewport), scrwin);
	gtk_container_add(GTK_CONTAINER(scrwin), view);

	res = message_dialog(_("Remove files"),
		       options.playlist_is_embedded ? main_window : playlist_window,
		       GTK_MESSAGE_QUESTION,
		       GTK_BUTTONS_YES_NO,
		       viewport,
		       _("The selected files will be deleted from the filesystem. "
			 "No recovery will be possible after this operation.\n\n"
			 "Do you want to proceed?"));

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, i++)) {

		GtkTreeIter * pl_iter;

		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 1, &pl_iter, -1);

		if (res == GTK_RESPONSE_YES) {

			gchar * file;
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, &file, -1);

			if (unlink(file) != 0) {
				fprintf(stderr, "unlink: unable to unlink %s\n", file);
				perror("unlink");
				gtk_tree_selection_unselect_iter(pl->select, pl_iter);
				++error;
			}

			g_free(file);
		}

		gtk_tree_iter_free(pl_iter);
	}

	if (res == GTK_RESPONSE_YES) {
		rem__sel_cb(pl);
	}

	if (error) {
		message_dialog(_("Remove files"),
			       options.playlist_is_embedded ? main_window : playlist_window,
			       GTK_MESSAGE_WARNING,
			       GTK_BUTTONS_OK,
			       NULL,
			       (error == 1) ? _("Unable to remove %d file.") : _("Unable to remove %d files."),
			       error);
	}
}


void
set_cursor_in_playlist(playlist_t * pl, GtkTreeIter *iter, gboolean scroll) {

        GtkTreePath * visible_path;

        visible_path = gtk_tree_model_get_path(GTK_TREE_MODEL(pl->store), iter);

        if (visible_path) {
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(pl->view), visible_path, NULL, FALSE);
                if (scroll == TRUE) {
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW (pl->view), visible_path, 
						     NULL, TRUE, 0.5, 0.0);
                }
                gtk_tree_path_free(visible_path);
        }
}

void
select_active_position_in_playlist(playlist_t * pl) {

	GtkTreeIter iter;
	playlist_data_t * data;

        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(pl->store), &iter)) {

                do {
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_DATA, &data, -1);
		        if (IS_PL_ACTIVE(data)) {
                                set_cursor_in_playlist(pl, &iter, FALSE);
				break;
                        }
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(pl->store), &iter));
        }
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

