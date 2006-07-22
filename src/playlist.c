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
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <pthread.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include "common.h"
#include "core.h"
#include "gui_main.h"
#include "music_browser.h"
#include "file_info.h"
#include "decoder/file_decoder.h"
#include "meta_decoder.h"
#include "volume.h"
#include "options.h"
#include "i18n.h"
#include "search_playlist.h"
#include "playlist.h"
#include "ifp_device.h"

extern options_t options;

extern void set_sliders_width(void);
extern void assign_audio_fc_filters(GtkFileChooser *fc);
extern void assign_playlist_fc_filters(GtkFileChooser *fc);

extern char pl_color_active[14];
extern char pl_color_inactive[14];

extern GtkTooltips * aqualung_tooltips;

extern GtkWidget* gui_stock_label_button(gchar *blabel, const gchar *bstock);

extern PangoFontDescription *fd_playlist;
extern PangoFontDescription *fd_statusbar;

int alt_L;
int alt_R;
int shift_L;
int shift_R;
int ctrl_L;
int ctrl_R;

GtkWidget * playlist_window;
GtkWidget * da_dialog;
GtkWidget * scrolled_win;

extern GtkWidget * main_window;
extern GtkWidget * info_window;
extern GtkWidget * vol_window;

int playlist_pos_x;
int playlist_pos_y;
int playlist_size_x;
int playlist_size_y;
int playlist_on;
int playlist_color_is_set;

int playlist_drag_y;
int playlist_scroll_up_tag = -1;
int playlist_scroll_dn_tag = -1;

extern int main_size_x;
extern int main_size_y;

extern int main_pos_x;
extern int main_pos_y;

extern int browser_pos_x;
extern int browser_pos_y;
extern int browser_on;

extern gulong play_id;
extern gulong pause_id;
extern GtkWidget * play_button;
extern GtkWidget * pause_button;

extern int vol_finished;
extern int vol_index;
int vol_n_tracks;
int vol_is_average;
vol_queue_t * pl_vol_queue;

/* used to store array of tree iters of tracks selected for RVA calc */
GtkTreeIter * vol_iters;


GtkWidget * play_list;
GtkTreeStore * play_store = 0;
GtkTreeSelection * play_select;
GtkTreeViewColumn * track_column;
GtkTreeViewColumn * rva_column;
GtkTreeViewColumn * length_column;
GtkCellRenderer * track_renderer;
GtkCellRenderer * rva_renderer;
GtkCellRenderer * length_renderer;

GtkWidget * statusbar_total;
GtkWidget * statusbar_total_label;
GtkWidget * statusbar_selected;
GtkWidget * statusbar_selected_label;

/* popup menus */
GtkWidget * add_menu;
GtkWidget * add__files;
GtkWidget * add__dir;
GtkWidget * sel_menu;
GtkWidget * sel__none;
GtkWidget * sel__all;
GtkWidget * sel__inv;
GtkWidget * rem_menu;
GtkWidget * cut__sel;
GtkWidget * rem__all;
GtkWidget * rem__sel;
GtkWidget * plist_menu;
GtkWidget * plist__save;
GtkWidget * plist__load;
GtkWidget * plist__enqueue;
GtkWidget * plist__separator1;
GtkWidget * plist__rva;
GtkWidget * plist__rva_menu;
GtkWidget * plist__rva_separate;
GtkWidget * plist__rva_average;
GtkWidget * plist__reread_file_meta;
GtkWidget * plist__separator2;
GtkWidget * plist__fileinfo;
GtkWidget * plist__search;

#ifdef HAVE_IFP
GtkWidget * plist__send_songs_to_iriver;
GtkWidget * plist__separator3;
#endif /* HAVE_IFP */

#define MBYTES  1048576         /* bytes per MB */

void init_plist_menu(GtkWidget *append_menu);

char command[RB_CONTROL_SIZE];

char fileinfo_name[MAXLEN];
char fileinfo_file[MAXLEN];

extern int is_file_loaded;
extern int is_paused;
extern int allow_seeks;

extern char current_file[MAXLEN];

extern jack_ringbuffer_t * rb_gui2disk;

extern GtkWidget * playlist_toggle;


typedef struct _playlist_filemeta {
        char * title;
        float duration;
        float voladj;
} playlist_filemeta;


playlist_filemeta * playlist_filemeta_get(char * physical_name, char * alt_name);
void playlist_filemeta_free(playlist_filemeta * plfm);


void rem__sel_cb(gpointer data);
void cut__sel_cb(gpointer data);
void plist__search_cb(gpointer data);
void add_files(GtkWidget * widget, gpointer data);
void recalc_album_node(GtkTreeIter * iter);


void
voladj2str(float voladj, char * str) {

	if (fabs(voladj) < 0.05f) {
		sprintf(str, " %.1f dB", 0.0f);
	} else {
		if (voladj >= 0.0f) {
			sprintf(str, " %.1f dB", voladj);
		} else {
			sprintf(str, "%.1f dB", voladj);
		}
	}
}

void
disable_bold_font_in_playlist() {

        GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {
		int j = 0;
		GtkTreeIter iter_child;

		gtk_tree_store_set(play_store, &iter, 7, PANGO_WEIGHT_NORMAL, -1);
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, &iter, j++)) {
			gtk_tree_store_set(play_store, &iter_child, 7, PANGO_WEIGHT_NORMAL, -1);
		}
	}
}


void
adjust_playlist_item_color(GtkTreeIter * piter, char * active, char * inactive) {

	char * str;

	gtk_tree_model_get(GTK_TREE_MODEL(play_store), piter, 2, &str, -1);
	
	if (strcmp(str, pl_color_active) == 0) {
		gtk_tree_store_set(play_store, piter, 2, active, -1);
		if (options.show_active_track_name_in_bold)
			gtk_tree_store_set(play_store, piter, 7, PANGO_WEIGHT_BOLD, -1);
		g_free(str);
	}
	
	if (strcmp(str, pl_color_inactive) == 0) {
		gtk_tree_store_set(play_store, piter, 2, inactive, -1);
		gtk_tree_store_set(play_store, piter, 7, PANGO_WEIGHT_NORMAL, -1);
		g_free(str);
	}
}


void
set_playlist_color() {
	
	GtkTreeIter iter;
	char active[14];
	char inactive[14];
	int i = 0;

        sprintf(active, "#%04X%04X%04X",
		play_list->style->fg[SELECTED].red,
		play_list->style->fg[SELECTED].green,
		play_list->style->fg[SELECTED].blue);

	sprintf(inactive, "#%04X%04X%04X",
		play_list->style->fg[INSENSITIVE].red,
		play_list->style->fg[INSENSITIVE].green,
		play_list->style->fg[INSENSITIVE].blue);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {
		int j = 0;
		GtkTreeIter iter_child;

		adjust_playlist_item_color(&iter, active, inactive);
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, &iter, j++)) {
			adjust_playlist_item_color(&iter_child, active, inactive);
		}
        }

        strcpy(pl_color_active, active);
	strcpy(pl_color_inactive, inactive);        
}


/* Calls foreach on each selected track iter. A track is selected iff
   it is selected or its parent album node is selected. */

void
playlist_foreach_selected(void (* foreach)(GtkTreeIter *)) {

	GtkTreeIter iter_top;
	GtkTreeIter iter;
	int i;
	int j;

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_top, NULL, i++)) {

		gboolean topsel = gtk_tree_selection_iter_is_selected(play_select, &iter_top);

		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(play_store), &iter_top)) {

			j = 0;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, &iter_top, j++)) {
				if (topsel || gtk_tree_selection_iter_is_selected(play_select, &iter)) {
					(*foreach)(&iter);
				}
			}

		} else if (topsel) {
			(*foreach)(&iter_top);
		}
	}
}


GtkTreePath *
get_playing_path(GtkTreeStore * store) {

	GtkTreeIter iter;
	GtkTreeIter iter_child;
	char * str;
	int i = 0;

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter))
		return NULL;

	do {
		int j = 0;
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 2, &str, -1);
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter_child, &iter, j++)) {
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter_child, 2, &str, -1);
			if (strcmp(str, pl_color_active) == 0) {
				g_free(str);
				return gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter_child);
			}
			g_free(str);
		}
		if (j == 1) { /* toplevel leafnode a.k.a. ordinary track */
			if (strcmp(str, pl_color_active) == 0) {
				g_free(str);
				return gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter);
			}		
			g_free(str);
		}
		
	} while (i++, gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));

	return NULL;
}


void 
start_playback_from_playlist(GtkTreePath * path) {

	GtkTreeIter iter;
	GtkTreePath * p;
	char cmd;
	cue_t cue;

	if (!allow_seeks)
		return;

	p = get_playing_path(play_store);
	if (p != NULL) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, p);
		gtk_tree_path_free(p);
		unmark_track(&iter);
	}
	
	gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path);
	if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter) > 0) {
		GtkTreeIter iter_child;
		gtk_tree_model_iter_children(GTK_TREE_MODEL(play_store), &iter_child, &iter);
		mark_track(&iter_child);
	} else {
		mark_track(&iter);
	}
	
	p = get_playing_path(play_store);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, p);
	gtk_tree_path_free(p);
	cue_track_for_playback(&iter, &cue);

        if (options.show_sn_title) {
		refresh_displays();
        }

	toggle_noeffect(PLAY, TRUE);

	if (is_paused) {
		is_paused = 0;
		toggle_noeffect(PAUSE, FALSE);
	}
		
	cmd = CMD_CUE;
	jack_ringbuffer_write(rb_gui2disk, &cmd, sizeof(char));
	jack_ringbuffer_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
	try_waking_disk_thread();
	
	is_file_loaded = 1;
}


static gboolean
playlist_window_close(GtkWidget * widget, GdkEvent * event, gpointer data) {

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle), FALSE);

	return TRUE;
}

gint
playlist_window_key_released(GtkWidget * widget, GdkEventKey * kevent) {

        switch (kevent->keyval) {
	case GDK_Alt_L:
		alt_L = 0;
		break;
	case GDK_Alt_R:
		alt_R = 0;
		break;
	case GDK_Shift_L:
		shift_L = 0;
		break;
	case GDK_Shift_R:
		shift_R = 0;
		break;
	case GDK_Control_L:
		ctrl_L = 0;
		break;
	case GDK_Control_R:
		ctrl_R = 0;
		break;
	}


        return FALSE;
}

gint
playlist_window_focus_out(GtkWidget * widget, GdkEventFocus * event, gpointer data) {

        alt_L = 0;
        alt_R = 0;
        shift_L = 0;
        shift_R = 0;
        ctrl_L = 0;
        ctrl_R = 0;

        return FALSE;
}


gint
playlist_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	GtkTreePath * path;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	char * pname;
	char * pfile;

        switch (kevent->keyval) {
	case GDK_Alt_L:
		alt_L = 1;
		break;
	case GDK_Alt_R:
		alt_R = 1;
		break;
	case GDK_Shift_L:
		shift_L = 1;
		break;
	case GDK_Shift_R:
		shift_R = 1;
		break;
	case GDK_Control_L:
		ctrl_L = 1;
		break;
	case GDK_Control_R:
		ctrl_R = 1;
		break;
	}

	switch (kevent->keyval) {

        case GDK_Insert:
	case GDK_KP_Insert:
                add_files(NULL, NULL);
                return TRUE;
                break;
        case GDK_q:
	case GDK_Q:
	case GDK_Escape:
                if (!options.playlist_is_embedded) {
                        playlist_window_close(NULL, NULL, NULL);
		}
		return TRUE;
		break;
	case GDK_i:
	case GDK_I:
		gtk_tree_view_get_cursor(GTK_TREE_VIEW(play_list), &path, &column);

		if (path &&
		    gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path) &&
		    !gtk_tree_model_iter_has_child(GTK_TREE_MODEL(play_store), &iter)) {

			GtkTreeIter dummy;
			
			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
					   0, &pname, 1, &pfile, -1);
			
			strncpy(fileinfo_name, pname, MAXLEN-1);
			strncpy(fileinfo_file, pfile, MAXLEN-1);
			free(pname);
			free(pfile);
			show_file_info(fileinfo_name, fileinfo_file, 0, NULL, dummy);
		}

		return TRUE;
		break;
	case GDK_Return:
	case GDK_KP_Enter:
		gtk_tree_view_get_cursor(GTK_TREE_VIEW(play_list), &path, &column);

		if (path) {
			start_playback_from_playlist(path);
		}		
		return TRUE;
		break;
	case GDK_x:
	case GDK_X:
		cut__sel_cb(NULL);
		return TRUE;
		break;
	case GDK_f:
	case GDK_F:
		plist__search_cb(NULL);
		return TRUE;
		break;
        
        case GDK_Delete:
	case GDK_KP_Delete:
                if (shift_L || shift_R) {
                        gtk_tree_store_clear(play_store);
			playlist_content_changed();
                } else {
			rem__sel_cb(NULL);
                }
                return TRUE;
		break;
	}

	return FALSE;
}


gint
doubleclick_handler(GtkWidget * widget, GdkEventButton * event, gpointer func_data) {

	GtkTreePath * path;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	char * pname;
	char * pfile;

	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(play_list), event->x, event->y,
						  &path, &column, NULL, NULL)) {
			start_playback_from_playlist(path);
		}
	}

	if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(play_list), event->x, event->y,
						  &path, &column, NULL, NULL) &&
		    gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path) &&
		    !gtk_tree_model_iter_has_child(GTK_TREE_MODEL(play_store), &iter)) {

			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
					   0, &pname, 1, &pfile, -1);
					
			strncpy(fileinfo_name, pname, MAXLEN-1);
			strncpy(fileinfo_file, pfile, MAXLEN-1);
			free(pname);
			free(pfile);
					
			gtk_widget_set_sensitive(plist__fileinfo, TRUE);
		} else {
			gtk_widget_set_sensitive(plist__fileinfo, FALSE);
		}

		gtk_menu_popup(GTK_MENU(plist_menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);
		return TRUE;
	}
	return FALSE;
}


void
plist__save_cb(gpointer data) {

        GtkWidget * dialog;
        gchar * selected_filename;
	char filename[MAXLEN];


        dialog = gtk_file_chooser_dialog_new(_("Please specify the file to save the playlist to."), 
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_SAVE, 
                                             GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        set_sliders_width();    /* MAGIC */

        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "playlist.xml");

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

		strncpy(filename, selected_filename, MAXLEN-1);
		save_playlist(filename);

                strncpy(options.currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
                                                                         MAXLEN-1);

                g_free(selected_filename);
        }

        gtk_widget_destroy(dialog);

        set_sliders_width();    /* MAGIC */
}


void
plist__load_cb(gpointer data) {

        GtkWidget * dialog;
        const gchar * selected_filename;
	char filename[MAXLEN];


        dialog = gtk_file_chooser_dialog_new(_("Please specify the file to load the playlist from."), 
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_OPEN, 
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        set_sliders_width();    /* MAGIC */

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        assign_playlist_fc_filters(GTK_FILE_CHOOSER(dialog));
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

                strncpy(filename, selected_filename, MAXLEN-1);
		switch (is_playlist(filename)) {
		case 0:
			fprintf(stderr,
				"error: %s does not appear to be a valid playlist\n", filename);
			break;
		case 1:
			load_playlist(filename, 0);
			break;
		case 2:
			load_m3u(filename, 0);
			break;
		case 3:
			load_pls(filename, 0);
			break;
		}

                strncpy(options.currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
                                                                         MAXLEN-1);
        }

        gtk_widget_destroy(dialog);

        set_sliders_width();    /* MAGIC */

	playlist_content_changed();

}


void
plist__enqueue_cb(gpointer data) {

        GtkWidget * dialog;
        const gchar * selected_filename;
	char filename[MAXLEN];


        dialog = gtk_file_chooser_dialog_new(_("Please specify the file to load the playlist from."), 
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_OPEN, 
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        set_sliders_width();    /* MAGIC */

        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        assign_playlist_fc_filters(GTK_FILE_CHOOSER(dialog));

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

                strncpy(filename, selected_filename, MAXLEN-1);
		switch (is_playlist(filename)) {
		case 0:
			fprintf(stderr,
				"error: %s does not appear to be a valid playlist\n", filename);
			break;
		case 1:
			load_playlist(filename, 1);
			break;
		case 2:
			load_m3u(filename, 1);
			break;
		case 3:
			load_pls(filename, 1);
			break;
		}

                strncpy(options.currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
                                                                         MAXLEN-1);
        }

        gtk_widget_destroy(dialog);

        set_sliders_width();    /* MAGIC */
        
        playlist_content_changed();
}


gint
watch_vol_calc(gpointer data) {

        float * volumes = (float *)data;

        if (!vol_finished) {
                return TRUE;
        }

	if (vol_index != vol_n_tracks) {
		free(volumes);
		volumes = NULL;
		return FALSE;
	}

	if (vol_is_average) {
		char voladj_str[32];
		float voladj = rva_from_multiple_volumes(vol_n_tracks, volumes,
							 options.rva_use_linear_thresh,
							 options.rva_avg_linear_thresh,
							 options.rva_avg_stddev_thresh,
							 options.rva_refvol,
							 options.rva_steepness);
		int i;

		voladj2str(voladj, voladj_str);

		for (i = 0; i < vol_n_tracks; i++) {

			if (gtk_tree_store_iter_is_valid(play_store, &vol_iters[i])) {
				gtk_tree_store_set(play_store, &vol_iters[i],
						   3, voladj, 4, voladj_str, -1);
			}
		}
	} else {
		float voladj;
		char voladj_str[32];

		int i;

		for (i = 0; i < vol_n_tracks; i++) {
			if (gtk_tree_store_iter_is_valid(play_store, &vol_iters[i])) {
				voladj = rva_from_volume(volumes[i],
							 options.rva_refvol,
							 options.rva_steepness);
				voladj2str(voladj, voladj_str);
				gtk_tree_store_set(play_store, &vol_iters[i], 3, voladj,
						   4, voladj_str, -1);
			}
		}
	}

	free(volumes);
	volumes = NULL;
	free(vol_iters);
	vol_iters = NULL;
        return FALSE;
}


void
plist_setup_vol_foreach(GtkTreeIter * iter) {

        char * pfile;

	gtk_tree_model_get(GTK_TREE_MODEL(play_store), iter, 1, &pfile, -1);

	if (pl_vol_queue == NULL) {
		pl_vol_queue = vol_queue_push(NULL, pfile, *iter/*dummy*/);
	} else {
		vol_queue_push(pl_vol_queue, pfile, *iter/*dummy*/);
	}
	++vol_n_tracks;

	vol_iters = (GtkTreeIter *)realloc(vol_iters, vol_n_tracks * sizeof(GtkTreeIter));
	if (!vol_iters) {
		fprintf(stderr, "realloc error in plist_setup_vol_calc()\n");
		return;
	}
	vol_iters[vol_n_tracks-1] = *iter;

	g_free(pfile);
}

void
plist_setup_vol_calc(void) {

	float * volumes = NULL;

	pl_vol_queue = NULL;

        if (vol_window != NULL) {
                return;
        }

	vol_n_tracks = 0;

	playlist_foreach_selected(plist_setup_vol_foreach);

	if (vol_n_tracks == 0)
		return;

	if (!options.rva_is_enabled) {

		GtkWidget * dialog = gtk_dialog_new_with_buttons(
				 _("Warning"),
				 options.playlist_is_embedded ?
				 GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window),
				 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				 GTK_STOCK_YES, GTK_RESPONSE_ACCEPT,
				 GTK_STOCK_NO, GTK_RESPONSE_REJECT,
				 NULL);

		GtkWidget * label =  gtk_label_new(_("Playback RVA is currently disabled.\n"
						     "Do you want to enable it now?"));

		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, TRUE, 10);
		gtk_widget_show(label);

		if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
			free(vol_iters);
			vol_iters = NULL;
			gtk_widget_destroy(dialog);

			return;
		} else {
			options.rva_is_enabled = 1;
			gtk_widget_destroy(dialog);
		}
	}

	if ((volumes = calloc(vol_n_tracks, sizeof(float))) == NULL) {
		fprintf(stderr, "calloc error in plist__rva_separate_cb()\n");
		free(vol_iters);
		vol_iters = NULL;
		return;
	}


	calculate_volume(pl_vol_queue, volumes);
	gtk_timeout_add(200, watch_vol_calc, (gpointer)volumes);
}


void
plist__rva_separate_cb(gpointer data) {

	vol_is_average = 0;
	plist_setup_vol_calc();
}


void
plist__rva_average_cb(gpointer data) {

	vol_is_average = 1;
	plist_setup_vol_calc();
}


void
plist__reread_file_meta_foreach(GtkTreeIter * iter) {

	gchar * title;
	gchar * fullname;
	char voladj_str[32];
	char duration_str[32];
	playlist_filemeta * plfm = NULL;


	gtk_tree_model_get(GTK_TREE_MODEL(play_store), iter,
			   0, &title,
			   1, &fullname,
			   -1);

	plfm = playlist_filemeta_get(fullname, title);
	if (plfm == NULL) {
		fprintf(stderr, "plist__reread_file_meta_foreach(): "
			"playlist_filemeta_get() returned NULL\n");
		return;
	}
			
	voladj2str(plfm->voladj, voladj_str);
	time2time(plfm->duration, duration_str);
			
	gtk_tree_store_set(play_store, iter,
			   0, plfm->title,
			   1, fullname,
			   3, plfm->voladj, 4, voladj_str,
			   5, plfm->duration, 6, duration_str,
			   -1);
			
	playlist_filemeta_free(plfm);
	plfm = NULL;
	g_free(title);
	g_free(fullname);
}


void
plist__reread_file_meta_cb(gpointer data) {

	playlist_foreach_selected(plist__reread_file_meta_foreach);
	delayed_playlist_rearrange(100);
}


#ifdef HAVE_IFP
void
plist__send_songs_to_iriver_cb(gpointer data) {

        aifp_transfer_files();

}
#endif /* HAVE_IFP */

void
plist__fileinfo_cb(gpointer data) {

	GtkTreeIter dummy;

	show_file_info(fileinfo_name, fileinfo_file, 0, NULL, dummy);
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


/* physical_name should be UTF8 encoded */
/* if alt_name != NULL, it will be used as title if no meta is found */
playlist_filemeta *
playlist_filemeta_get(char * physical_name, char * alt_name) {

	char display_name[MAXLEN];
	char artist_name[MAXLEN];
	char record_name[MAXLEN];
	char track_name[MAXLEN];
	metadata * meta = NULL;
	int use_meta = 0;
	gchar * substr;

	playlist_filemeta * plfm = calloc(1, sizeof(playlist_filemeta));
	if (!plfm) {
		fprintf(stderr, "calloc error in playlist_filemeta_get()\n");
		return NULL;
	}

	if ((plfm->duration = get_file_duration(physical_name)) <= 0.0f) {
		return NULL;
	}

	if (options.rva_is_enabled) {
		meta = meta_new();
		if (meta_read(meta, physical_name)) {
			if (!meta_get_rva(meta, &(plfm->voladj))) {
				plfm->voladj = 0.0f;
			}
		} else {
			plfm->voladj = 0.0f;
		}
		meta_free(meta);
		meta = NULL;
	} else {
		plfm->voladj = 0.0f;
	}
	
	artist_name[0] = '\0';
	record_name[0] = '\0';
	track_name[0] = '\0';

	if (options.auto_use_ext_meta_artist ||
	    options.auto_use_ext_meta_record ||
	    options.auto_use_ext_meta_track) {
		
		meta = meta_new();
		if (!meta_read(meta, physical_name)) {
			meta_free(meta);
			meta = NULL;
		}
	}
	
	use_meta = 0;
	if ((meta != NULL) && options.auto_use_ext_meta_artist) {
		meta_get_artist(meta, artist_name);
		if (artist_name[0] != '\0') {
			use_meta = 1;
		}
	}
	
	if ((meta != NULL) && options.auto_use_ext_meta_record) {
		meta_get_record(meta, record_name);
		if (record_name[0] != '\0') {
			use_meta = 1;
		}
	}
	
	if ((meta != NULL) && options.auto_use_ext_meta_track) {
		meta_get_title(meta, track_name);
		if (track_name[0] != '\0') {
			use_meta = 1;
		}
	}
	
	if ((artist_name[0] != '\0') ||
	    (record_name[0] != '\0') ||
	    (track_name[0] != '\0')) {
		
		if (artist_name[0] == '\0') {
			strcpy(artist_name, _("Unknown"));
		}
		if (record_name[0] == '\0') {
			strcpy(record_name, _("Unknown"));
		}
		if (track_name[0] == '\0') {
			strcpy(track_name, _("Unknown"));
		}
	} else {
		use_meta = 0;
	}
	
	if (use_meta && (meta != NULL)) {
		make_title_string(display_name, options.title_format,
				  artist_name, record_name, track_name);
	} else {
		if (alt_name != NULL) {
			strcpy(display_name, alt_name);
		} else {
    			if ((substr = strrchr(physical_name, '/')) == NULL)
            			substr = physical_name;
    			else
            			++substr;
			substr = g_filename_display_name(substr);
			strcpy(display_name, substr);
			g_free(substr);
		} 
	}
	if (meta != NULL) {
		meta_free(meta);
		meta = NULL;
	}

	plfm->title = strdup(display_name);

	return plfm;
}


void
playlist_filemeta_free(playlist_filemeta * plfm) {

	free(plfm->title);
	free(plfm);
}


void
add_file_to_playlist(gchar *filename) {

	char voladj_str[32];
	char duration_str[32];
	GtkTreeIter play_iter;
	playlist_filemeta * plfm = NULL;

	if ((plfm = playlist_filemeta_get(filename, NULL)) == NULL) {
		return;
	}

        voladj2str(plfm->voladj, voladj_str);
        time2time(plfm->duration, duration_str);

        gtk_tree_store_append(play_store, &play_iter, NULL);
        gtk_tree_store_set(play_store, &play_iter, 0, plfm->title, 1, filename,
                           2, pl_color_inactive,
                           3, plfm->voladj, 4, voladj_str,
                           5, plfm->duration, 6, duration_str, 7, PANGO_WEIGHT_NORMAL, -1);

	playlist_filemeta_free(plfm);

}


static int
filter(const struct dirent * de) {

	return de->d_name[0] != '.';
}


void
add_dir_to_playlist(char * dirname) {

	int i;
	struct dirent ** ent;
	struct stat st_file;
	char path[MAXLEN];

	for (i = 0; i < scandir(dirname, &ent, filter, alphasort); i++) {

		snprintf(path, MAXLEN-1, "%s/%s", dirname, ent[i]->d_name);

		if (stat(path, &st_file) == -1) {
			continue;
		}

		if (S_ISDIR(st_file.st_mode)) {
			add_dir_to_playlist(path);
		} else {
			add_file_to_playlist(path);
		}
	}
}


void
add_files(GtkWidget * widget, gpointer data) {

        GtkWidget *dialog;

        dialog = gtk_file_chooser_dialog_new(_("Select files"), 
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_OPEN,
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        set_sliders_width();    /* MAGIC */

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        assign_audio_fc_filters(GTK_FILE_CHOOSER(dialog));
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		GSList *lfiles, *node;

                strncpy(options.currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
			MAXLEN-1);

                lfiles = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));

                node = lfiles;

                while (node) {

                        add_file_to_playlist((char *)node->data);
                        g_free(node->data);

                        node = node->next;
                }

                g_slist_free(lfiles);
        }


        gtk_widget_destroy(dialog);

        set_sliders_width();    /* MAGIC */

	playlist_content_changed();

        delayed_playlist_rearrange(100);
}


void
add_directory(GtkWidget * widget, gpointer data) {

        GtkWidget *dialog;

        dialog = gtk_file_chooser_dialog_new(_("Select directory"), 
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
					     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        set_sliders_width();    /* MAGIC */

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        assign_audio_fc_filters(GTK_FILE_CHOOSER(dialog));
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char dirname[MAXLEN];

                strncpy(options.currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
			MAXLEN-1);

                strncpy(dirname, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
			MAXLEN-1);

		add_dir_to_playlist(dirname);
        }


        gtk_widget_destroy(dialog);

        set_sliders_width();    /* MAGIC */

	playlist_content_changed();

        delayed_playlist_rearrange(100);
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
select_all(GtkWidget * widget, gpointer data) {

	gtk_tree_selection_select_all(play_select);
}


void
sel__all_cb(gpointer data) {

	select_all(NULL, NULL);
}


void
sel__none_cb(gpointer data) {

	gtk_tree_selection_unselect_all(play_select);
}


void
invert_item_selection(GtkTreeIter * piter) {

	if (gtk_tree_selection_iter_is_selected(play_select, piter)) {
		gtk_tree_selection_unselect_iter(play_select, piter);
	} else {
		gtk_tree_selection_select_iter(play_select, piter);
	}
}


void
sel__inv_cb(gpointer data) {

	GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		GtkTreeIter iter_child;
		int j = 0;

		invert_item_selection(&iter);

		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, &iter, j++)) {
			invert_item_selection(&iter_child);
		}
	}
}


void
rem__all_cb(gpointer data) {

	gtk_tree_store_clear(play_store);
        playlist_content_changed();
}


void
rem__sel_cb(gpointer data) {

	GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		GtkTreeIter iter_child;
		int j = 0;

		int n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter);

		if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {
			gtk_tree_store_remove(play_store, &iter);
			--i;
			continue;
		}

		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, &iter, j++)) {
			if (gtk_tree_selection_iter_is_selected(play_select, &iter_child)) {
				gtk_tree_store_remove(play_store, &iter_child);
				--j;
			}
		}

		/* if all tracks have been removed, also remove album node; else recalc album node */
		if (n) {
			if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter) == 0) {
				gtk_tree_store_remove(play_store, &iter);
				--i;
			} else {
				recalc_album_node(&iter);
			}
		}
	}
	playlist_content_changed();
}


void
remove_sel(GtkWidget * widget, gpointer data) {

	rem__sel_cb(NULL);
}


/* playlist item is selected -> keep, else -> remove
 * ret: 0 if kept, 1 if removed track
 */
int
cut_track_item(GtkTreeIter * piter) {

	if (!gtk_tree_selection_iter_is_selected(play_select, piter)) {
		gtk_tree_store_remove(play_store, piter);
		return 1;
	}
	return 0;
}


/* ret: 1 if at least one of album node's children are selected; else 0 */
int
any_tracks_selected(GtkTreeIter * piter) {

	int j = 0;
	GtkTreeIter iter_child;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, piter, j++)) {
		if (gtk_tree_selection_iter_is_selected(play_select, &iter_child)) {
			return 1;
		}
	}
	return 0;
}


/* cut selected callback */
void
cut__sel_cb(gpointer data) {

	GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		int n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter);

		if (n) { /* album node */
			if (any_tracks_selected(&iter)) {
				int j = 0;
				GtkTreeIter iter_child;
				while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store),
								     &iter_child, &iter, j++)) {
					j -= cut_track_item(&iter_child);
				}

				recalc_album_node(&iter);
			} else {
				i -= cut_track_item(&iter);
			}
		} else { /* track node */
			i -= cut_track_item(&iter);
		}
	}
	gtk_tree_selection_unselect_all(play_select);
	playlist_content_changed();
}


gint
playlist_rearrange_timeout_cb(gpointer data) {   

	playlist_size_allocate(NULL, NULL);

	return FALSE;
}


void
delayed_playlist_rearrange(int delay) {

	g_timeout_add(delay, playlist_rearrange_timeout_cb, NULL);
}


gint
playlist_size_allocate(GtkWidget * widget, GdkEventConfigure * event) {

	int avail;
	int track_width;
	int rva_width;
	int length_width;


	avail = play_list->allocation.width;

	if (gtk_tree_view_column_get_visible(GTK_TREE_VIEW_COLUMN(rva_column))) {
		gtk_tree_view_column_cell_get_size(GTK_TREE_VIEW_COLUMN(rva_column),
						   NULL, NULL, NULL, &rva_width, NULL);
	} else {
		rva_width = 1;
	}

	if (gtk_tree_view_column_get_visible(GTK_TREE_VIEW_COLUMN(length_column))) {
		gtk_tree_view_column_cell_get_size(GTK_TREE_VIEW_COLUMN(length_column),
						   NULL, NULL, NULL, &length_width, NULL);
	} else {
		length_width = 1;
	}

	track_width = avail - rva_width - length_width;
	if (track_width < 1)
		track_width = 1;

	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(track_column), track_width);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(rva_column), rva_width);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(length_column), length_width);


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
playlist_child_stats(GtkTreeIter * iter, int * count, float * duration, guint * songs_size, int selected) {

	int j = 0;
	gchar * tstr;
	struct stat statbuf;
	GtkTreeIter iter_child;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, iter, j++)) {
		
		if (!selected || gtk_tree_selection_iter_is_selected(play_select, &iter_child)) {
			
			float len = 0;

			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter_child, 5, &len, 1, &tstr, -1);
			*duration += len;
			(*count)++;
			if (g_stat(tstr, &statbuf) != -1) {
				*songs_size += statbuf.st_size;
			}
			g_free(tstr);			
		}
	}
}


/* if selected == true -> stats for selected tracks; else: all tracks */
void
playlist_stats(int selected) {

	GtkTreeIter iter;
	int i = 0;

	int count = 0;
	float duration = 0;
	float len = 0;
	char str[MAXLEN];
	char time[16];
        gchar * tstr;
        guint songs_size;
        struct stat statbuf;

	if (!options.enable_playlist_statusbar) return;

        songs_size = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		int n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter);
		if (n > 0) { /* album node -- count children tracks */
			if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {
				playlist_child_stats(&iter, &count, &duration, &songs_size, 0/*false*/);
			} else {
				playlist_child_stats(&iter, &count, &duration, &songs_size, selected);
			}
		} else {
			if (!selected || gtk_tree_selection_iter_is_selected(play_select, &iter)) {
				gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 5, &len, 1, &tstr, -1);
				duration += len;
				count++;
				if (g_stat(tstr, &statbuf) != -1) {
					songs_size += statbuf.st_size;
				}
				g_free(tstr);
			}
                }
	}


	time2time(duration, time);
	if (count == 1) {
                if (options.show_songs_size_in_statusbar) {
                        sprintf(str, _("%d track [%s] (%d MB)"), count, time, songs_size/MBYTES);
                } else {
                        sprintf(str, _("%d track [%s] "), count, time);
                }
	} else {
                if (options.show_songs_size_in_statusbar) {
                        sprintf(str, _("%d tracks [%s] (%d MB)"), count, time, songs_size/MBYTES);
                } else {
                        sprintf(str, _("%d tracks [%s] "), count, time);
                }
	}

	if (selected) {
		gtk_label_set_text(GTK_LABEL(statusbar_selected), str);
	} else {
		gtk_label_set_text(GTK_LABEL(statusbar_total), str);
	}
}


void
recalc_album_node(GtkTreeIter * iter) {
	int count = 0;
	float duration = 0;
	guint songs_size;
	char time[16];

	playlist_child_stats(iter, &count, &duration, &songs_size, 0/*false*/);
	time2time(duration, time);
	gtk_tree_store_set(play_store, iter, 5, duration, 6, time, -1);
}


void
playlist_selection_changed(GtkTreeSelection * sel, gpointer data) {

	playlist_stats(1/* true */);
}


void
playlist_content_changed(void) {

	playlist_stats(0/*false*/);
}


void
playlist_drag_begin(GtkWidget * widget, GdkDragContext * drag_context, gpointer data) {

	GtkTargetEntry target_table[] = {
		{ "", GTK_TARGET_SAME_APP, 0 }
	};

	gtk_drag_dest_set(play_list,
			  GTK_DEST_DEFAULT_ALL,
			  target_table,
			  1,
			  GDK_ACTION_MOVE);
}


void
playlist_drag_data_get(GtkWidget * widget, GdkDragContext * drag_context,
		      GtkSelectionData * data, guint info, guint time, gpointer user_data) {

	gtk_selection_data_set(data, data->target, 8, (const guchar *) "list\0", 5);
}


void
playlist_perform_drag(GtkTreeModel * model,
		      GtkTreeIter * sel_iter, GtkTreePath * sel_path,
		      GtkTreeIter * pos_iter, GtkTreePath * pos_path) {

	int cmp = gtk_tree_path_compare(sel_path, pos_path);
	int sel_depth = gtk_tree_path_get_depth(sel_path);
	int pos_depth = gtk_tree_path_get_depth(pos_path);

	int * sel_idx = gtk_tree_path_get_indices(sel_path);
	int * pos_idx = gtk_tree_path_get_indices(pos_path);

	if (cmp == 0) {
		return;
	}

	if (sel_depth == pos_depth && (sel_depth == 1 || sel_idx[0] == pos_idx[0])) {
		if (cmp == 1) {
			gtk_tree_store_move_before(play_store, sel_iter, pos_iter);
		} else {
			gtk_tree_store_move_after(play_store, sel_iter, pos_iter);
		}
	} else {

		GtkTreeIter iter;
		GtkTreeIter sel_parent;
		GtkTreeIter pos_parent;
		int recalc_sel_parent = 0;
		int recalc_pos_parent = 0;
		char * tname;
		char * fname;
		char * color;
		float voladj;
		char * voldisp;
		float duration;
		char * durdisp;
		int fontw;

		if (gtk_tree_model_iter_has_child(model, sel_iter)) {
			return;
		}

		if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(play_store), &sel_parent, sel_iter)) {
			recalc_sel_parent = 1;
		}

		if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(play_store), &pos_parent, pos_iter)) {
			recalc_pos_parent = 1;
		}

		gtk_tree_model_get(model, sel_iter, 0, &tname, 1, &fname, 2, &color,
				   3, &voladj, 4, &voldisp, 5, &duration,
				   6, &durdisp, 7, &fontw, -1);

		if (cmp == 1) {
			gtk_tree_store_insert_before(GTK_TREE_STORE(model),
						     &iter, NULL, pos_iter);
		} else {
			gtk_tree_store_insert_after(GTK_TREE_STORE(model),
						    &iter, NULL, pos_iter);
		}

		gtk_tree_store_set(GTK_TREE_STORE(model), &iter, 0, tname, 1, fname, 2, color,
				   3, voladj, 4, voldisp, 5, duration,
				   6, durdisp, 7, fontw, -1);

		gtk_tree_store_remove(GTK_TREE_STORE(model), sel_iter);

		if (recalc_sel_parent) {
			if (gtk_tree_model_iter_has_child(model, &sel_parent)) {
				recalc_album_node(&sel_parent);
			} else {
				gtk_tree_store_remove(GTK_TREE_STORE(model), &sel_parent);
			}
		}

		if (recalc_pos_parent) {
			recalc_album_node(&pos_parent);
		}

		g_free(tname);
		g_free(fname);
		g_free(color);
		g_free(voldisp);
		g_free(durdisp);
	}
}


gint
playlist_drag_data_received(GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y, 
			    GtkSelectionData  * data, guint info, guint time) {

	GtkTreeViewColumn * column;

	if (!strcmp(data->data, "store")) {

		GtkTreePath * path = NULL;
		GtkTreeIter * piter = NULL;
		GtkTreeIter iter;
		GtkTreeIter parent;

		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(play_list),
						  x, y, &path, &column, NULL, NULL)) {

			if (info != 4) { /* dragging store, artist or record */
				while (gtk_tree_path_get_depth(path) > 1) {
					gtk_tree_path_up(path);
				}
			}

			gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path);
			piter = &iter;
		}

		switch (info) {
		case 1:
			store__addlist_defmode(piter);
			break;
		case 2:
			artist__addlist_defmode(piter);
			break;
		case 3:
			record__addlist_defmode(piter);
			break;
		case 4:
			track__addlist_cb(piter);

			if (piter && gtk_tree_model_iter_parent(GTK_TREE_MODEL(play_store),
								&parent, piter)) {
				recalc_album_node(&parent);
			}

			break;
		}

		if (path) {
			gtk_tree_path_free(path);
		}

	} else if (!strcmp(data->data, "list")) {

		GtkTreeModel * model;
		GtkTreeIter sel_iter;
		GtkTreeIter pos_iter;
		GtkTreePath * sel_path = NULL;
		GtkTreePath * pos_path = NULL;

		gtk_tree_selection_set_mode(play_select, GTK_SELECTION_SINGLE);

		if (gtk_tree_selection_get_selected(play_select, &model, &sel_iter)) {

			sel_path = gtk_tree_model_get_path(model, &sel_iter);

			if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(play_list),
						  x, y, &pos_path, &column, NULL, NULL)) {

				gtk_tree_model_get_iter(model, &pos_iter, pos_path);

				playlist_perform_drag(model,
						      &sel_iter, sel_path,
						      &pos_iter, pos_path);
			}
		}

		if (sel_path) {
			gtk_tree_path_free(sel_path);
		}

		if (pos_path) {
			gtk_tree_path_free(pos_path);
		}


		gtk_tree_selection_set_mode(play_select, GTK_SELECTION_MULTIPLE);
	}

	return FALSE;
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
playlist_scroll_up(gpointer data) {

	g_signal_emit_by_name(G_OBJECT(scrolled_win), "scroll-child",
			      GTK_SCROLL_STEP_BACKWARD, FALSE/*vertical*/, NULL);

	return TRUE;
}

gint
playlist_scroll_dn(gpointer data) {

	g_signal_emit_by_name(G_OBJECT(scrolled_win), "scroll-child",
			      GTK_SCROLL_STEP_FORWARD, FALSE/*vertical*/, NULL);

	return TRUE;
}


void
playlist_drag_leave(GtkWidget * widget, GdkDragContext * drag_context, guint time) {

	if (playlist_drag_y < widget->allocation.height / 2) {
		if (playlist_scroll_up_tag == -1) {
			playlist_scroll_up_tag = g_timeout_add(100, playlist_scroll_up, NULL);
		}
	} else {
		if (playlist_scroll_dn_tag == -1) {
			playlist_scroll_dn_tag = g_timeout_add(100, playlist_scroll_dn, NULL);
		}
	}
}


gboolean
playlist_drag_motion(GtkWidget * widget, GdkDragContext * context,
		     gint x, gint y, guint time) {

	playlist_drag_y = y;
	playlist_remove_scroll_tags();

	return TRUE;
}


void
playlist_drag_end(GtkWidget * widget, GdkDragContext * drag_context, gpointer data) {

	playlist_remove_scroll_tags();
	gtk_drag_dest_unset(play_list);
}


void
create_playlist(void) {

	GtkWidget * vbox;

	GtkWidget * hbox_bottom;
	GtkWidget * add_button;
	GtkWidget * selall_button;
	GtkWidget * remsel_button;

	GtkWidget * viewport;

	GtkWidget * statusbar;
	GtkWidget * statusbar_viewport;

	int i;

	GdkPixbuf * pixbuf;
	char path[MAXLEN];

	GtkTargetEntry target_table[] = {
		{ "", GTK_TARGET_SAME_APP, 0 }
	};

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
                g_signal_connect(G_OBJECT(playlist_window), "key_release_event",
                                 G_CALLBACK(playlist_window_key_released), NULL);
        }

        g_signal_connect(G_OBJECT(playlist_window), "focus_out_event",
                         G_CALLBACK(playlist_window_focus_out), NULL);
	gtk_widget_set_events(playlist_window, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

        /* embedded playlist? */
        if (!options.playlist_is_embedded) {
                plist_menu = gtk_menu_new();
                init_plist_menu(plist_menu);
        }

        vbox = gtk_vbox_new(FALSE, 2);
        gtk_container_add(GTK_CONTAINER(playlist_window), vbox);

        /* create playlist */
	if (!play_store) {
		play_store = gtk_tree_store_new(8,
						G_TYPE_STRING,          /* track name */
						G_TYPE_STRING,          /* physical filename */
						G_TYPE_STRING,          /* color for selections */
						G_TYPE_FLOAT,           /* volume adjustment [dB] */
						G_TYPE_STRING,          /* volume adj. displayed */
						G_TYPE_FLOAT,           /* duration (in secs) */
						G_TYPE_STRING,          /* duration displayed */
                                                G_TYPE_INT);            /* font weight */
	}


        play_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(play_store));
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(play_list), FALSE);
	gtk_widget_set_name(play_list, "play_list");
	gtk_widget_set_size_request(play_list, 100, 100);

        if (options.override_skin_settings) {
                gtk_widget_modify_font (play_list, fd_playlist);
        }

        if (options.enable_pl_rules_hint) {
                gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(play_list), TRUE);
        }

        playlist_color_is_set = 0;

	for (i = 0; i < 3; i++) {
		switch (options.plcol_idx[i]) {
		case 0:
			track_renderer = gtk_cell_renderer_text_new();
			track_column = gtk_tree_view_column_new_with_attributes("Tracks",
										track_renderer,
										"text", 0,
										"foreground", 2,
                                                                                "weight", 7,
										NULL);
			gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(track_column),
							GTK_TREE_VIEW_COLUMN_FIXED);
			gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(track_column), 3);
			gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(track_column), FALSE);
			gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(track_column), 10);
			gtk_tree_view_append_column(GTK_TREE_VIEW(play_list), track_column);
			break;

		case 1:
			rva_renderer = gtk_cell_renderer_text_new();
			g_object_set((gpointer)rva_renderer, "xalign", 1.0, NULL);
			rva_column = gtk_tree_view_column_new_with_attributes("RVA",
									      rva_renderer,
									      "text", 4,
									      "foreground", 2,
                                                                              "weight", 7,
									      NULL);
			gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(rva_column),
							GTK_TREE_VIEW_COLUMN_FIXED);
			gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(rva_column), 3);
			if (options.show_rva_in_playlist) {
				gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(rva_column), TRUE);
				gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(rva_column), 50);
			} else {
				gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(rva_column), FALSE);
				gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(rva_column), 1);
			}
			gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(rva_column), FALSE);
			gtk_tree_view_append_column(GTK_TREE_VIEW(play_list), rva_column);
			break;

		case 2:
			length_renderer = gtk_cell_renderer_text_new();
			g_object_set((gpointer)length_renderer, "xalign", 1.0, NULL);
			length_column = gtk_tree_view_column_new_with_attributes("Length",
										 length_renderer,
										 "text", 6,
										 "foreground", 2,
                                                                                 "weight", 7,
										 NULL);
			gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(length_column),
							GTK_TREE_VIEW_COLUMN_FIXED);
			gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(length_column), 3);
			if (options.show_length_in_playlist) {
				gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(length_column), TRUE);
				gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(length_column), 50);
			} else {
				gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(length_column), FALSE);
				gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(length_column), 1);
			}
			gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(length_column), FALSE);
			gtk_tree_view_append_column(GTK_TREE_VIEW(play_list), length_column);
		}
	}

        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(play_list), FALSE);


	/* setup drag and drop inside playlist for reordering */
	
	gtk_drag_source_set(play_list,
			    GDK_BUTTON1_MASK,
			    target_table,
			    1,
			    GDK_ACTION_MOVE);

	
	snprintf(path, MAXLEN-1, "%s/drag.png", DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		gtk_drag_source_set_icon_pixbuf(play_list, pixbuf);
	}	
	

	g_signal_connect(G_OBJECT(play_list), "drag_begin",
			 G_CALLBACK(playlist_drag_begin), NULL);

	g_signal_connect(G_OBJECT(play_list), "drag_end",
			 G_CALLBACK(playlist_drag_end), NULL);

	g_signal_connect(G_OBJECT(play_list), "drag_data_get",
			 G_CALLBACK(playlist_drag_data_get), NULL);

	g_signal_connect(G_OBJECT(play_list), "drag_leave",
			 G_CALLBACK(playlist_drag_leave), NULL);

	g_signal_connect(G_OBJECT(play_list), "drag_motion",
			 G_CALLBACK(playlist_drag_motion), NULL);

	g_signal_connect(G_OBJECT(play_list), "drag_data_received",
			 G_CALLBACK(playlist_drag_data_received), NULL);
	

        play_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(play_list));
        gtk_tree_selection_set_mode(play_select, GTK_SELECTION_MULTIPLE);

	g_signal_connect(G_OBJECT(play_select), "changed",
			 G_CALLBACK(playlist_selection_changed), NULL);


	g_signal_connect(G_OBJECT(play_list), "button_press_event",
			 G_CALLBACK(doubleclick_handler), NULL);


	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), viewport, TRUE, TRUE, 0);

	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_win);

	gtk_container_add(GTK_CONTAINER(scrolled_win), play_list);


	/* statusbar */
	if (options.enable_playlist_statusbar) {
		statusbar_viewport = gtk_viewport_new(NULL, NULL);
		gtk_widget_set_name(statusbar_viewport, "info_viewport");
		gtk_box_pack_start(GTK_BOX(vbox), statusbar_viewport, FALSE, TRUE, 2);
		
		statusbar = gtk_hbox_new(FALSE, 0);
		gtk_container_set_border_width(GTK_CONTAINER(statusbar), 1);
		gtk_container_add(GTK_CONTAINER(statusbar_viewport), statusbar);
		
		statusbar_selected = gtk_label_new("");
		gtk_widget_set_name(statusbar_selected, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_selected, FALSE, TRUE, 0);
		
		statusbar_selected_label = gtk_label_new(_("Selected: "));
		gtk_widget_set_name(statusbar_selected_label, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_selected_label, FALSE, TRUE, 0);
		
		gtk_box_pack_end(GTK_BOX(statusbar), gtk_vseparator_new(), FALSE, TRUE, 5);
		
		statusbar_total = gtk_label_new("");
		gtk_widget_set_name(statusbar_total, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_total, FALSE, TRUE, 0);
		
		statusbar_total_label = gtk_label_new(_("Total: "));
		gtk_widget_set_name(statusbar_total_label, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_total_label, FALSE, TRUE, 0);

                if (options.override_skin_settings) {
                        gtk_widget_modify_font (statusbar_selected, fd_statusbar);
                        gtk_widget_modify_font (statusbar_selected_label, fd_statusbar);
                        gtk_widget_modify_font (statusbar_total, fd_statusbar);
                        gtk_widget_modify_font (statusbar_total_label, fd_statusbar);
                }
		
		playlist_selection_changed(NULL, NULL);
		playlist_content_changed();
	}
	

	/* bottom area of playlist window */
        hbox_bottom = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox_bottom, FALSE, TRUE, 0);

	add_button = gtk_button_new_with_label(_("Add files"));
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), add_button, _("Add files to playlist\n(Press right mouse button for menu)"), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_bottom), add_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(add_files), NULL);

	selall_button = gtk_button_new_with_label(_("Select all"));
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), selall_button, _("Select all songs in playlist\n(Press right mouse button for menu)"), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_bottom), selall_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(selall_button), "clicked", G_CALLBACK(select_all), NULL);
	
	remsel_button = gtk_button_new_with_label(_("Remove selected"));
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), remsel_button, _("Remove selected songs from playlist\n(Press right mouse button for menu)"), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_bottom), remsel_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(remsel_button), "clicked", G_CALLBACK(remove_sel), NULL);


        add_menu = gtk_menu_new();

        add__dir = gtk_menu_item_new_with_label(_("Add directory"));
        gtk_menu_shell_append(GTK_MENU_SHELL(add_menu), add__dir);
        g_signal_connect_swapped(G_OBJECT(add__dir), "activate", G_CALLBACK(add__dir_cb), NULL);
	gtk_widget_show(add__dir);

        add__files = gtk_menu_item_new_with_label(_("Add files"));
        gtk_menu_shell_append(GTK_MENU_SHELL(add_menu), add__files);
        g_signal_connect_swapped(G_OBJECT(add__files), "activate", G_CALLBACK(add__files_cb), NULL);
	gtk_widget_show(add__files);

        g_signal_connect_swapped(G_OBJECT(add_button), "event", G_CALLBACK(add_cb), NULL);


        sel_menu = gtk_menu_new();

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

        g_signal_connect_swapped(G_OBJECT(selall_button), "event", G_CALLBACK(sel_cb), NULL);


        rem_menu = gtk_menu_new();

        rem__all = gtk_menu_item_new_with_label(_("Remove all"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), rem__all);
        g_signal_connect_swapped(G_OBJECT(rem__all), "activate", G_CALLBACK(rem__all_cb), NULL);
    	gtk_widget_show(rem__all);

        cut__sel = gtk_menu_item_new_with_label(_("Cut selected"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), cut__sel);
        g_signal_connect_swapped(G_OBJECT(cut__sel), "activate", G_CALLBACK(cut__sel_cb), NULL);
    	gtk_widget_show(cut__sel);

        rem__sel = gtk_menu_item_new_with_label(_("Remove selected"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), rem__sel);
        g_signal_connect_swapped(G_OBJECT(rem__sel), "activate", G_CALLBACK(rem__sel_cb), NULL);
    	gtk_widget_show(rem__sel);

        g_signal_connect_swapped(G_OBJECT(remsel_button), "event", G_CALLBACK(rem_cb), NULL);


        g_signal_connect(G_OBJECT(playlist_window), "size_allocate",
			 G_CALLBACK(playlist_size_allocate), NULL);

}        
        


void
show_playlist(void) {

	playlist_on = 1;

	if (!options.playlist_is_embedded) {
		gtk_window_move(GTK_WINDOW(playlist_window), playlist_pos_x, playlist_pos_y);
		gtk_window_resize(GTK_WINDOW(playlist_window), playlist_size_x, playlist_size_y);
	} else {
		gtk_window_get_size(GTK_WINDOW(main_window), &main_size_x, &main_size_y);
		gtk_window_resize(GTK_WINDOW(main_window), main_size_x, main_size_y + playlist_size_y + 6);
	}

        gtk_widget_show_all(playlist_window);

	if (!playlist_color_is_set) {
        	set_playlist_color();
		playlist_color_is_set = 1;
	}
}


void
hide_playlist(void) {

	playlist_on = 0;
	if (!options.playlist_is_embedded) {
		gtk_window_get_position(GTK_WINDOW(playlist_window), &playlist_pos_x, &playlist_pos_y);
		gtk_window_get_size(GTK_WINDOW(playlist_window), &playlist_size_x, &playlist_size_y);
	} else {
		playlist_size_x = playlist_window->allocation.width;
		playlist_size_y = playlist_window->allocation.height;
		gtk_window_get_size(GTK_WINDOW(main_window), &main_size_x, &main_size_y);
	}
        gtk_widget_hide(playlist_window);

	if (options.playlist_is_embedded) {
		gtk_window_resize(GTK_WINDOW(main_window), main_size_x, main_size_y - playlist_size_y - 6);
	}

}


xmlNodePtr
save_track_node(GtkTreeIter * piter, xmlNodePtr root, char * nodeID) {

	char * track_name;
	char * phys_name;
	gchar *converted_temp;
	char * color;
	float voladj;
	float duration;
        char str[32];
        xmlNodePtr node;

	gtk_tree_model_get(GTK_TREE_MODEL(play_store), piter,
			   0, &track_name, 1, &phys_name, 2, &color,
			   3, &voladj, 5, &duration, -1);
	
	node = xmlNewTextChild(root, NULL, (const xmlChar*) nodeID, NULL);
	
	xmlNewTextChild(node, NULL, (const xmlChar*) "track_name", (const xmlChar*) track_name);
	if (!strcmp(nodeID,"record")) {
	    xmlNewTextChild(node, NULL, (const xmlChar*) "phys_name", (const xmlChar*) phys_name);
	} else {
	    converted_temp = g_filename_to_uri(phys_name, NULL, NULL);
	    xmlNewTextChild(node, NULL, (const xmlChar*) "phys_name", (const xmlChar*) converted_temp);
	    g_free(converted_temp);
	};
	/* FIXME: dont use #000000 color as active if you dont want special fx in playlist 8-) */
	
	if (strcmp(color, pl_color_active) == 0) {
		strcpy(str, "yes");
	} else {
		strcpy(str, "no");
	}
	xmlNewTextChild(node, NULL, (const xmlChar*) "is_active", (const xmlChar*) str);
	
	snprintf(str, 31, "%f", voladj);
	xmlNewTextChild(node, NULL, (const xmlChar*) "voladj", (const xmlChar*) str);
	
	snprintf(str, 31, "%f", duration);
	xmlNewTextChild(node, NULL, (const xmlChar*) "duration", (const xmlChar*) str);
	
	g_free(track_name);
	g_free(phys_name);
	g_free(color);

	return node;
}


void
save_playlist(char * filename) {

        int i = 0;
        GtkTreeIter iter;
        xmlDocPtr doc;
        xmlNodePtr root;

        doc = xmlNewDoc((const xmlChar*) "1.0");
        root = xmlNewNode(NULL, (const xmlChar*) "aqualung_playlist");
        xmlDocSetRootElement(doc, root);

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		int n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter);

		if (n) { /* album node */
			int j = 0;
			GtkTreeIter iter_child;
			xmlNodePtr node;

			node = save_track_node(&iter, root, "record");
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store),
							     &iter_child, &iter, j++)) {
				save_track_node(&iter_child, node, "track");
			}
		} else { /* track node */
			save_track_node(&iter, root, "track");
		}
        }
        xmlSaveFormatFile(filename, doc, 1);
}


void
parse_playlist_track(xmlDocPtr doc, xmlNodePtr cur, GtkTreeIter * pparent_iter, int sel_ok) {

        xmlChar * key;
	gchar *converted_temp;
	GError *error;
	GtkTreeIter iter;
	int is_record_node;
	char track_name[MAXLEN];
	char phys_name[MAXLEN];
	char color[32];
	float voladj = 0.0f;
	char voladj_str[32];
	float duration = 0.0f;
	char duration_str[32];

	track_name[0] = '\0';
	phys_name[0] = '\0';
	color[0] = '\0';
	
	is_record_node = !xmlStrcmp(cur->name, (const xmlChar *)"record");
        cur = cur->xmlChildrenNode;
	if (cur) {
	    gtk_tree_store_append(play_store, &iter, pparent_iter);
	    gtk_tree_store_set(play_store, &iter, 0, "", 1, "", 2, "", 3, 0.0f, 
				4, "", 5, 0.0f, 6, "", -1);
	}

        while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"track_name"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                strncpy(track_name, (char *) key, sizeof(track_name)-1);
				track_name[sizeof(track_name)-1] = '\0';
			};
                        xmlFree(key);
			gtk_tree_store_set(play_store, &iter, 0, track_name, -1); 
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"phys_name"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				if (is_record_node) {
				//"record" node keeps special data here 
				    strncpy(phys_name, (char *) key, sizeof(phys_name)-1);
				    phys_name[sizeof(phys_name)-1] = '\0';
				} else if ((converted_temp = g_filename_from_uri((char *) key, NULL, NULL))) {
                            	    strncpy(phys_name, converted_temp, sizeof(phys_name)-1);
				    phys_name[sizeof(phys_name)-1] = '\0';
				    g_free(converted_temp);
				} else {
				    //try to read utf8 filename from outdated file
				    converted_temp = g_locale_from_utf8((char *) key, -1, NULL, NULL, &error);
				    if ((converted_temp!=NULL)) {
					strncpy(phys_name, converted_temp, sizeof(phys_name)-1);
					phys_name[sizeof(phys_name)-1] = '\0';
					g_free(converted_temp);
				    } else {
				    //last try - maybe it's plain locale filename
					strncpy(phys_name, (char *) key, sizeof(phys_name)-1);
					phys_name[sizeof(phys_name)-1] = '\0';
				    };
				};
			};
                        xmlFree(key);
			gtk_tree_store_set(play_store, &iter, 1, phys_name, -1); 
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"is_active"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				if ((xmlStrcmp(key, (const xmlChar *)"yes")) || (!sel_ok)) {
					strncpy(color, pl_color_inactive, sizeof(color)-1);
					color[sizeof(color)-1] = '\0';
				} else {
					strncpy(color, pl_color_active, sizeof(color)-1);
					color[sizeof(color)-1] = '\0';
				};
			} else {
			    strncpy(color, pl_color_inactive, sizeof(color)-1);
			    color[sizeof(color)-1] = '\0';
			};
                        xmlFree(key);
			gtk_tree_store_set(play_store, &iter, 2, color, -1);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"voladj"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				voladj = convf((char *) key);
                        }
                        xmlFree(key);
			voladj2str(voladj, voladj_str);
			gtk_tree_store_set(play_store, &iter, 3, voladj, 4, voladj_str, -1);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"duration"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				duration = convf((char *) key);
                        }
                        xmlFree(key);
			time2time(duration, duration_str);
			gtk_tree_store_set(play_store, &iter, 5, duration, 6, duration_str, -1);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"track"))) {
                        parse_playlist_track(doc, cur, &iter, sel_ok);
		}
		cur = cur->next;
	}
}

void
load_playlist(char * filename, int enqueue) {

	GtkTreePath * p;
        xmlDocPtr doc;
        xmlNodePtr cur;
	FILE * f;
	int sel_ok = 0;

        if ((f = fopen(filename, "rt")) == NULL) {
                return;
        }
        fclose(f);

        doc = xmlParseFile(filename);
        if (doc == NULL) {
                fprintf(stderr, "An XML error occured while parsing %s\n", filename);
                return;
        }

        cur = xmlDocGetRootElement(doc);
        if (cur == NULL) {
                fprintf(stderr, "load_playlist: empty XML document\n");
                xmlFreeDoc(doc);
                return;
        }

        if (xmlStrcmp(cur->name, (const xmlChar *)"aqualung_playlist")) {
                fprintf(stderr, "load_playlist: XML document of the wrong type, "
                        "root node != aqualung_playlist\n");
                xmlFreeDoc(doc);
                return;
        }

	if (pl_color_active[0] == '\0')
		strcpy(pl_color_active, "#ffffff");
	if (pl_color_inactive[0] == '\0')
		strcpy(pl_color_inactive, "#000000");

	if (!enqueue)
		gtk_tree_store_clear(play_store);

	if ((p = get_playing_path(play_store)) == NULL) {
		sel_ok = 1;
	} else {
		gtk_tree_path_free(p);
	}
		
        cur = cur->xmlChildrenNode;
        while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"track"))) {
                        parse_playlist_track(doc, cur, NULL, sel_ok);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"record"))) {
                        parse_playlist_track(doc, cur, NULL, sel_ok);
                }
                cur = cur->next;
        }

        xmlFreeDoc(doc);

	delayed_playlist_rearrange(100);
	playlist_content_changed();
}


void
load_m3u(char * filename, int enqueue) {

	FILE * f;
	int c;
	int i = 0;
	int n;
	char * str;
	char pl_dir[MAXLEN];
	char line[MAXLEN];
	char name[MAXLEN];
	char path[MAXLEN];
	char tmp[MAXLEN];
	int have_name = 0;
	GtkTreeIter iter;
	char voladj_str[32];
	char duration_str[32];
	playlist_filemeta * plfm = NULL;


	if ((str = strrchr(filename, '/')) == NULL) {
		printf("load_m3u(): programmer error: playlist path is not absolute\n");
	}
	for (i = 0; (i < (str - filename)) && (i < MAXLEN-1); i++) {
		pl_dir[i] = filename[i];
	}
	pl_dir[i] = '\0';

	if ((f = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "unable to open .m3u playlist: %s\n", filename);
		return;
	}

	if (!enqueue)
		gtk_tree_store_clear(play_store);

	i = 0;
	while ((c = fgetc(f)) != EOF) {
		if ((c != '\n') && (c != '\r') && (i < MAXLEN)) {
			if ((i > 0) || ((c != ' ') && (c != '\t'))) {
				line[i++] = c;
			}
		} else {
			line[i] = '\0';
			if (i == 0)
				continue;
			i = 0;

			if (strstr(line, "#EXTM3U") == line) {
				continue;
			}
			
			if (strstr(line, "#EXTINF:") == line) {

				char str_duration[64];
				int cnt = 0;
				
				/* We parse the timing, but throw it away. 
				   This may change in the future. */
				while ((line[cnt+8] >= '0') && (line[cnt+8] <= '9')) {
					str_duration[cnt] = line[cnt+8];
					++cnt;
				}
				str_duration[cnt] = '\0';
				snprintf(name, MAXLEN-1, "%s", line+cnt+9);
				have_name = 1;

			} else {
				/* safeguard against http:// and C:\ stuff */
				if (strstr(line, "http://") == line) {
					fprintf(stderr, "Ignoring playlist item: %s\n", line);
					i = 0;
					have_name = 0;
					continue;
				}
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
					char * ch;
					if ((ch = strrchr(path, '/')) != NULL) {
						++ch;
						snprintf(name, MAXLEN-1, "%s", ch);
					} else {
						fprintf(stderr,
							"warning: ain't this a directory? : %s\n", path);
						snprintf(name, MAXLEN-1, "%s", path);
					}
				}
				have_name = 0;

				
				plfm = playlist_filemeta_get(path,
							     have_name ? name : NULL);
				if (plfm == NULL) {
					fprintf(stderr, "load_m3u(): playlist_filemeta_get() returned NULL\n");
					continue;
				}

				time2time(plfm->duration, duration_str);
				voladj2str(plfm->voladj, voladj_str);

				gtk_tree_store_append(play_store, &iter, NULL);
				gtk_tree_store_set(play_store, &iter,
						   0, plfm->title,
						   1, g_locale_to_utf8(path, -1, NULL, NULL, NULL),
						   2, pl_color_inactive,
						   3, plfm->voladj, 4, voladj_str,
						   5, plfm->duration, 6, duration_str,
						   7, PANGO_WEIGHT_NORMAL, -1);

				playlist_filemeta_free(plfm);
				plfm = NULL;
			}
		}
	}
	delayed_playlist_rearrange(100);
}


void
load_pls(char * filename, int enqueue) {

	FILE * f;
	int c;
	int i = 0;
	int n;
	char * str;
	char pl_dir[MAXLEN];
	char line[MAXLEN];
	char file[MAXLEN];
	char title[MAXLEN];
	char tmp[MAXLEN];
	int have_file = 0;
	int have_title = 0;
	char numstr_file[10];
	char numstr_title[10];
	GtkTreeIter iter;
	char voladj_str[32];
	char duration_str[32];
	playlist_filemeta * plfm = NULL;


	if ((str = strrchr(filename, '/')) == NULL) {
		printf("load_pls(): programmer error: playlist path is not absolute\n");
	}
	for (i = 0; (i < (str - filename)) && (i < MAXLEN-1); i++) {
		pl_dir[i] = filename[i];
	}
	pl_dir[i] = '\0';

	if ((f = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "unable to open .pls playlist: %s\n", filename);
		return;
	}

	if (!enqueue)
		gtk_tree_store_clear(play_store);

	i = 0;
	while ((c = fgetc(f)) != EOF) {
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
			if (strstr(line, "NumberOfEntries") == line) {
				continue;
			}
			if (strstr(line, "Version") == line) {
				continue;
			}
			
			if (strstr(line, "File") == line) {

				char numstr[10];
				char * ch;
				int m;

				if ((ch = strstr(line, "=")) == NULL) {
					fprintf(stderr, "Syntax error in %s\n", filename);
					return;
				}
				
				++ch;
				while ((*ch == ' ') || (*ch == '\t'))
					++ch;

				m = 0;
				for (n = 0; (line[n+4] != '=') && (m < sizeof(numstr)); n++) {
					if ((line[n+4] != ' ') && (line[n+4] != '\t'))
						numstr[m++] = line[n+4];
				}
				numstr[m] = '\0';
				strncpy(numstr_file, numstr, sizeof(numstr_file));

				/* safeguard against http:// and C:\ stuff */
				if (strstr(ch, "http://") == ch) {
					fprintf(stderr, "Ignoring playlist item: %s\n", ch);
					i = 0;
					have_file = have_title = 0;
					continue;
				}
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

				have_file = 1;
				

			} else if (strstr(line, "Title") == line) {

				char numstr[10];
				char * ch;
				int m;

				if ((ch = strstr(line, "=")) == NULL) {
					fprintf(stderr, "Syntax error in %s\n", filename);
					return;
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
					fprintf(stderr, "Syntax error in %s\n", filename);
					return;
				}
				
				++ch;
				while ((*ch == ' ') || (*ch == '\t'))
					++ch;

			} else {
				fprintf(stderr, 
					"Syntax error: invalid line in playlist: %s\n", line);
				return;
			}

			if (!have_file || !have_title) {
				continue;
			}
			
			if (strcmp(numstr_file, numstr_title) != 0) {
				continue;
			}

			have_file = have_title = 0;

			plfm = playlist_filemeta_get(file,
						     have_title ? title : NULL);
			if (plfm == NULL) {
				fprintf(stderr, "load_pls(): playlist_filemeta_get() returned NULL\n");
				continue;
			}

			time2time(plfm->duration, duration_str);
			voladj2str(plfm->voladj, voladj_str);

			gtk_tree_store_append(play_store, &iter, NULL);
			gtk_tree_store_set(play_store, &iter,
					   0, plfm->title,
					   1, g_locale_to_utf8(file, -1, NULL, NULL, NULL),
					   2, pl_color_inactive,
					   3, plfm->voladj, 4, voladj_str,
					   5, plfm->duration, 6, duration_str, -1);

			playlist_filemeta_free(plfm);
			plfm = NULL;
		}
	}
	delayed_playlist_rearrange(100);
}


/* return values: 
 *   0: not a playlist 
 *   1: native aqualung (XML)
 *   2: .m3u
 *   3: .pls
 */
int
is_playlist(char * filename) {

	FILE * f;
	char buf[] = "<?xml version=\"1.0\"?>\n<aqualung_playlist>\0";
	char inbuf[64];
	int len;

	if ((f = fopen(filename, "rb")) == NULL) {
		return 0;
	}
	if (fread(inbuf, 1, strlen(buf)+1, f) != strlen(buf)+1) {
		fclose(f);
		goto _readext;
	}
	fclose(f);
	inbuf[strlen(buf)] = '\0';

	if (strcmp(buf, inbuf) == 0) {
		return 1;
	}

 _readext:
	len = strlen(filename);
	if (len < 5) {
		return 0;
	}

	if ((filename[len-4] == '.') &&
	    ((filename[len-3] == 'm') || (filename[len-3] == 'M')) &&
	    (filename[len-2] == '3') &&
	    ((filename[len-1] == 'u') || (filename[len-1] == 'U'))) {
		return 2;
	}

	if ((filename[len-4] == '.') &&
	    ((filename[len-3] == 'p') || (filename[len-3] == 'P')) &&
	    ((filename[len-2] == 'l') || (filename[len-2] == 'L')) &&
	    ((filename[len-1] == 's') || (filename[len-1] == 'S'))) {
		return 3;
	}

	return 0;
}


void
add_to_playlist(char * filename, int enqueue) {

	char fullname[MAXLEN];
	char * home;
	char * path = filename;
	GtkTreeIter iter;
	char voladj_str[32];
	char duration_str[32];
	playlist_filemeta * plfm = NULL;


	if (!filename)
		return;

	switch (filename[0]) {
	case '/':
		strcpy(fullname, filename);
		break;
	case '~':
		++path;
		if (!(home = getenv("HOME"))) {
			fprintf(stderr, "add_to_playlist(): cannot resolve home directory\n");
			return;
		}
		snprintf(fullname, MAXLEN-1, "%s/%s", home, path);
		break;
	default:
		snprintf(fullname, MAXLEN-1, "%s/%s", options.cwd, filename);
		break;
	}


        if (pl_color_active[0] == '\0')
                strcpy(pl_color_active, "#ffffff");
        if (pl_color_inactive[0] == '\0')
                strcpy(pl_color_inactive, "#000000");

	switch (is_playlist(fullname)) {

	case 0: /* not a playlist -- load the file itself */
		if (!enqueue)
			gtk_tree_store_clear(play_store);

		plfm = playlist_filemeta_get(fullname, NULL);
		if (plfm == NULL) {
			fprintf(stderr, "add_to_playlist(): playlist_filemeta_get() returned NULL\n");
			return;
		}

		voladj2str(plfm->voladj, voladj_str);
		time2time(plfm->duration, duration_str);

                gtk_tree_store_append(play_store, &iter, NULL);
                gtk_tree_store_set(play_store, &iter,
				   0, plfm->title,
				   1, g_locale_to_utf8(fullname, -1, NULL, NULL, NULL),
				   2, pl_color_inactive,
				   3, plfm->voladj, 4, voladj_str,
				   5, plfm->duration, 6, duration_str,
				   -1);

		playlist_filemeta_free(plfm);
		plfm = NULL;

		delayed_playlist_rearrange(100);
		break;

	case 1: /* native aqualung playlist (XML) */
		load_playlist(fullname, enqueue);
		break;

	case 2: /* .m3u */
		load_m3u(fullname, enqueue);
		break;

	case 3: /* .pls */
		load_pls(fullname, enqueue);
		break;
	}
	playlist_content_changed();
}

void
init_plist_menu(GtkWidget *append_menu) {

        plist__save = gtk_menu_item_new_with_label(_("Save playlist"));
        plist__load = gtk_menu_item_new_with_label(_("Load playlist"));
        plist__enqueue = gtk_menu_item_new_with_label(_("Enqueue playlist"));
        plist__separator1 = gtk_separator_menu_item_new();
        plist__rva = gtk_menu_item_new_with_label(_("Calculate RVA"));
        plist__rva_menu = gtk_menu_new();
        plist__rva_separate = gtk_menu_item_new_with_label(_("Separate"));
        plist__rva_average = gtk_menu_item_new_with_label(_("Average"));
        plist__reread_file_meta = gtk_menu_item_new_with_label(_("Reread file metadata"));
        plist__separator2 = gtk_separator_menu_item_new();

#ifdef HAVE_IFP
        plist__send_songs_to_iriver = gtk_menu_item_new_with_label(_("Send songs to iFP device"));
        plist__separator3 = gtk_separator_menu_item_new();
#endif  /* HAVE_IFP */

        plist__fileinfo = gtk_menu_item_new_with_label(_("File info..."));
        plist__search = gtk_menu_item_new_with_label(_("Search..."));

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__save);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__load);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__enqueue);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__separator1);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__rva);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(plist__rva), plist__rva_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(plist__rva_menu), plist__rva_separate);
        gtk_menu_shell_append(GTK_MENU_SHELL(plist__rva_menu), plist__rva_average);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__reread_file_meta);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__separator2);

#ifdef HAVE_IFP
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__send_songs_to_iriver);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__separator3);
#endif  /* HAVE_IFP */

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__fileinfo);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__search);

        g_signal_connect_swapped(G_OBJECT(plist__save), "activate", G_CALLBACK(plist__save_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__load), "activate", G_CALLBACK(plist__load_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__enqueue), "activate", G_CALLBACK(plist__enqueue_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__rva_separate), "activate", G_CALLBACK(plist__rva_separate_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__rva_average), "activate", G_CALLBACK(plist__rva_average_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__reread_file_meta), "activate", G_CALLBACK(plist__reread_file_meta_cb), NULL);

#ifdef HAVE_IFP
        g_signal_connect_swapped(G_OBJECT(plist__send_songs_to_iriver), "activate", G_CALLBACK(plist__send_songs_to_iriver_cb), NULL);
#endif  /* HAVE_IFP */

        g_signal_connect_swapped(G_OBJECT(plist__fileinfo), "activate", G_CALLBACK(plist__fileinfo_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(plist__search), "activate", G_CALLBACK(plist__search_cb), NULL);

        gtk_widget_show(plist__save);
        gtk_widget_show(plist__load);
        gtk_widget_show(plist__enqueue);
        gtk_widget_show(plist__separator1);
        gtk_widget_show(plist__rva);
        gtk_widget_show(plist__rva_separate);
        gtk_widget_show(plist__rva_average);
        gtk_widget_show(plist__reread_file_meta);
        gtk_widget_show(plist__separator2);

#ifdef HAVE_IFP
        gtk_widget_show(plist__send_songs_to_iriver);
        gtk_widget_show(plist__separator3);
#endif  /* HAVE_IFP */
        
        gtk_widget_show(plist__fileinfo);
        gtk_widget_show(plist__search);
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

