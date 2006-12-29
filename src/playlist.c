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
#include "core.h"
#include "rb.h"
#include "gui_main.h"
#include "music_browser.h"
#include "file_info.h"
#include "decoder/dec_mod.h"
#include "decoder/file_decoder.h"
#include "meta_decoder.h"
#include "volume.h"
#include "options.h"
#include "i18n.h"
#include "search_playlist.h"
#include "playlist.h"
#include "ifp_device.h"

extern options_t options;

extern void assign_audio_fc_filters(GtkFileChooser *fc);
extern void assign_playlist_fc_filters(GtkFileChooser *fc);

extern char pl_color_active[14];
extern char pl_color_inactive[14];

extern GtkTooltips * aqualung_tooltips;

extern GtkWidget* gui_stock_label_button(gchar *blabel, const gchar *bstock);

extern PangoFontDescription *fd_playlist;
extern PangoFontDescription *fd_statusbar;

GtkWidget * playlist_window;
GtkWidget * da_dialog;
GtkWidget * scrolled_win;

extern GtkWidget * main_window;
extern GtkWidget * info_window;
extern GtkWidget * vol_window;

extern GtkTreeSelection * music_select;

gint playlist_pos_x;
gint playlist_pos_y;
gint playlist_size_x;
gint playlist_size_y;
gint playlist_on;
gint playlist_color_is_set;

gint playlist_drag_y;
gint playlist_scroll_up_tag = -1;
gint playlist_scroll_dn_tag = -1;

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
gint vol_n_tracks;
gint vol_is_average;
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

GtkWidget * iw_widget;

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
GtkWidget * rem__dead;
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

void add_directory(GtkWidget * widget, gpointer data);
void init_plist_menu(GtkWidget *append_menu);
void show_active_position_in_playlist(void);
void show_active_position_in_playlist_toggle(void);
void show_last_position_in_playlist(void);
void expand_collapse_album_node(void);

gchar command[RB_CONTROL_SIZE];

gchar fileinfo_name[MAXLEN];
gchar fileinfo_file[MAXLEN];

extern int is_file_loaded;
extern int is_paused;
extern int allow_seeks;

extern char current_file[MAXLEN];

extern rb_t * rb_gui2disk;

extern GtkWidget * playlist_toggle;


typedef struct _playlist_filemeta {
        char * title;
        float duration;
        float voladj;
} playlist_filemeta;


playlist_filemeta * playlist_filemeta_get(char * physical_name, char * alt_name, int composit);
void playlist_filemeta_free(playlist_filemeta * plfm);

void iw_show_info (GtkWidget *dialog, GtkWindow *transient, gchar *message);
void iw_hide_info (void);
void remove_files (void);

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
	gint i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {
		gint j = 0;
		GtkTreeIter iter_child;

		gtk_tree_store_set(play_store, &iter, COLUMN_FONT_WEIGHT, PANGO_WEIGHT_NORMAL, -1);
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, &iter, j++)) {
			gtk_tree_store_set(play_store, &iter_child, COLUMN_FONT_WEIGHT, PANGO_WEIGHT_NORMAL, -1);
		}
	}
}


void
adjust_playlist_item_color(GtkTreeIter * piter, char * active, char * inactive) {

	gchar * str;

	gtk_tree_model_get(GTK_TREE_MODEL(play_store), piter, 2, &str, -1);
	
	if (strcmp(str, pl_color_active) == 0) {
		gtk_tree_store_set(play_store, piter, COLUMN_SELECTION_COLOR, active, -1);
		if (options.show_active_track_name_in_bold) {
			gtk_tree_store_set(play_store, piter, COLUMN_FONT_WEIGHT, PANGO_WEIGHT_BOLD, -1);
		}
	}
	
	if (strcmp(str, pl_color_inactive) == 0) {
		gtk_tree_store_set(play_store, piter, COLUMN_SELECTION_COLOR, inactive, -1);
		gtk_tree_store_set(play_store, piter, COLUMN_FONT_WEIGHT, PANGO_WEIGHT_NORMAL, -1);
	}

	g_free(str);
}


void
set_playlist_color() {
	
	GtkTreeIter iter;
	gchar active[14];
	gchar inactive[14];
	gint i = 0;

        sprintf(active, "#%04X%04X%04X",
		play_list->style->fg[SELECTED].red,
		play_list->style->fg[SELECTED].green,
		play_list->style->fg[SELECTED].blue);

	sprintf(inactive, "#%04X%04X%04X",
		play_list->style->fg[INSENSITIVE].red,
		play_list->style->fg[INSENSITIVE].green,
		play_list->style->fg[INSENSITIVE].blue);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {
		gint j = 0;
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
playlist_foreach_selected(void (* foreach)(GtkTreeIter *, void *), void * data) {

	GtkTreeIter iter_top;
	GtkTreeIter iter;
	gint i;
	gint j;

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_top, NULL, i++)) {

		gboolean topsel = gtk_tree_selection_iter_is_selected(play_select, &iter_top);

		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(play_store), &iter_top)) {

			j = 0;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, &iter_top, j++)) {
				if (topsel || gtk_tree_selection_iter_is_selected(play_select, &iter)) {
					(*foreach)(&iter, data);
				}
			}

		} else if (topsel) {
			(*foreach)(&iter_top, data);
		}
	}
}


GtkTreePath *
get_playing_path(GtkTreeStore * store) {

	GtkTreeIter iter;
	GtkTreeIter iter_child;
	gchar * str;
	gint i = 0;

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter))
		return NULL;

	do {
		gint j = 0;
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COLUMN_SELECTION_COLOR, &str, -1);
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter_child, &iter, j++)) {
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter_child, COLUMN_SELECTION_COLOR, &str, -1);
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
	gchar cmd;
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
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	gchar * pname;
	gchar * pfile;


        switch (kevent->keyval) {

        case GDK_Insert:
	case GDK_KP_Insert:
                if(kevent->state & GDK_SHIFT_MASK) {  /* SHIFT + Insert */
                        add_directory(NULL, NULL);
                } else {
                        add_files(NULL, NULL);
                }
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
	case GDK_F1:
	case GDK_i:
	case GDK_I:
		gtk_tree_view_get_cursor(GTK_TREE_VIEW(play_list), &path, &column);

		if (path &&
		    gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path) &&
		    !gtk_tree_model_iter_has_child(GTK_TREE_MODEL(play_store), &iter)) {

			GtkTreeIter dummy;
			
			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
					   COLUMN_TRACK_NAME, &pname, COLUMN_PHYSICAL_FILENAME, &pfile, -1);
			
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
	case GDK_u:
	case GDK_U:
		cut__sel_cb(NULL);
		return TRUE;
		break;
	case GDK_f:
	case GDK_F:
		plist__search_cb(NULL);
		return TRUE;
		break;
        case GDK_a:
        case GDK_A:
                show_active_position_in_playlist_toggle();
                return TRUE;
                break;
        case GDK_w:
        case GDK_W:
                gtk_tree_view_collapse_all(GTK_TREE_VIEW(play_list));  
                show_active_position_in_playlist();
                return TRUE;
                break;
        case GDK_Delete:
	case GDK_KP_Delete:
                if(kevent->state & GDK_SHIFT_MASK) {  /* SHIFT + Delete */
                        gtk_tree_store_clear(play_store);
			playlist_content_changed();
                } else if(kevent->state & GDK_CONTROL_MASK) {  /* CTRL + Delete */
                        remove_files();
                } else {
			rem__sel_cb(NULL);
                }
                return TRUE;
		break;
#ifdef HAVE_IFP
	case GDK_t:
	case GDK_T:
                aifp_transfer_files();
		return TRUE;
		break;
#endif /* HAVE_IFP */

        case GDK_grave:
                expand_collapse_album_node();
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
	gchar * pname;
	gchar * pfile;

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

			if (gtk_tree_selection_count_selected_rows(play_select) == 0)
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(play_list), path, NULL, FALSE);

			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
					   COLUMN_TRACK_NAME, &pname, COLUMN_PHYSICAL_FILENAME, &pfile, -1);
					
			strncpy(fileinfo_name, pname, MAXLEN-1);
			strncpy(fileinfo_file, pfile, MAXLEN-1);
			free(pname);
			free(pfile);
					
			gtk_widget_set_sensitive(plist__fileinfo, TRUE);
		} else {
			gtk_widget_set_sensitive(plist__fileinfo, FALSE);
		}

		gtk_widget_set_sensitive(plist__rva, (vol_window == NULL) ? TRUE : FALSE);

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
	gchar filename[MAXLEN];


        dialog = gtk_file_chooser_dialog_new(_("Please specify the file to save the playlist to."), 
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_SAVE, 
                                             GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        deflicker();
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "playlist.xml");

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                iw_show_info(dialog, options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), _("Saving playlist..."));

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

		strncpy(filename, selected_filename, MAXLEN-1);
		save_playlist(filename);

                strncpy(options.currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
                                                                         MAXLEN-1);
                iw_hide_info();
                g_free(selected_filename);
        }

        gtk_widget_destroy(dialog);
}


void
plist__load_cb(gpointer data) {

        GtkWidget * dialog;
        const gchar * selected_filename;
	gchar filename[MAXLEN];


        dialog = gtk_file_chooser_dialog_new(_("Please specify the file to load the playlist from."), 
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_OPEN, 
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        deflicker();
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        assign_playlist_fc_filters(GTK_FILE_CHOOSER(dialog));
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                iw_show_info(dialog, options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), _("Loading playlist..."));

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
                iw_hide_info();
        }

        gtk_widget_destroy(dialog);

	playlist_content_changed();
}


void
plist__enqueue_cb(gpointer data) {

        GtkWidget * dialog;
        const gchar * selected_filename;
	gchar filename[MAXLEN];


        dialog = gtk_file_chooser_dialog_new(_("Please specify the file to load the playlist from."), 
                                             options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_OPEN, 
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        deflicker();
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        assign_playlist_fc_filters(GTK_FILE_CHOOSER(dialog));

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                iw_show_info(dialog, options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), _("Enqueuing playlist..."));

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
                iw_hide_info();
        }

        gtk_widget_destroy(dialog);

        playlist_content_changed();
}


gint
watch_vol_calc(gpointer data) {

        gfloat * volumes = (gfloat *)data;

        if (!vol_finished) {
                return TRUE;
        }

	if (vol_index != vol_n_tracks) {
		free(volumes);
		volumes = NULL;
		return FALSE;
	}

	if (vol_is_average) {
		gchar voladj_str[32];
		gfloat voladj = rva_from_multiple_volumes(vol_n_tracks, volumes,
							 options.rva_use_linear_thresh,
							 options.rva_avg_linear_thresh,
							 options.rva_avg_stddev_thresh,
							 options.rva_refvol,
							 options.rva_steepness);
		gint i;

		voladj2str(voladj, voladj_str);

		for (i = 0; i < vol_n_tracks; i++) {

			if (gtk_tree_store_iter_is_valid(play_store, &vol_iters[i])) {
				gtk_tree_store_set(play_store, &vol_iters[i],
						   COLUMN_VOLUME_ADJUSTMENT, voladj, COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str, -1);
			}
		}
	} else {
		gfloat voladj;
		gchar voladj_str[32];

		gint i;

		for (i = 0; i < vol_n_tracks; i++) {
			if (gtk_tree_store_iter_is_valid(play_store, &vol_iters[i])) {
				voladj = rva_from_volume(volumes[i],
							 options.rva_refvol,
							 options.rva_steepness);
				voladj2str(voladj, voladj_str);
				gtk_tree_store_set(play_store, &vol_iters[i], COLUMN_VOLUME_ADJUSTMENT, voladj,
						   COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str, -1);
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
plist_setup_vol_foreach(GtkTreeIter * iter, void * data) {

        gchar * pfile;

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

	gfloat * volumes = NULL;

	pl_vol_queue = NULL;

        if (vol_window != NULL) {
                return;
        }

	vol_n_tracks = 0;

	playlist_foreach_selected(plist_setup_vol_foreach, NULL);

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

		if (aqualung_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
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
	g_timeout_add(200, watch_vol_calc, (gpointer)volumes);
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
plist__reread_file_meta_foreach(GtkTreeIter * iter, void * data) {

	gchar * title;
	gchar * fullname;
	gchar voladj_str[32];
	gchar duration_str[MAXLEN];
	gint composit;
	playlist_filemeta * plfm = NULL;
	GtkTreeIter dummy;


	gtk_tree_model_get(GTK_TREE_MODEL(play_store), iter,
			   COLUMN_TRACK_NAME, &title,
			   COLUMN_PHYSICAL_FILENAME, &fullname,
			   -1);

	if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(play_store), &dummy, iter)) {
		composit = 0;
	} else {
		composit = 1;
	}

	plfm = playlist_filemeta_get(fullname, title, composit);
	if (plfm == NULL) {
		fprintf(stderr, "plist__reread_file_meta_foreach(): "
			"playlist_filemeta_get() returned NULL\n");
		return;
	}
			
	voladj2str(plfm->voladj, voladj_str);
	time2time_na(plfm->duration, duration_str);
	
	gtk_tree_store_set(play_store, iter,
			   COLUMN_TRACK_NAME, plfm->title,
			   COLUMN_PHYSICAL_FILENAME, fullname,
			   COLUMN_VOLUME_ADJUSTMENT, plfm->voladj, COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str,
			   COLUMN_DURATION, plfm->duration, COLUMN_DURATION_DISP, duration_str,
			   -1);
			
	playlist_filemeta_free(plfm);
	plfm = NULL;
	g_free(title);
	g_free(fullname);
}


void
plist__reread_file_meta_cb(gpointer data) {
	
	GtkTreeIter iter;
	GtkTreeIter iter_child;
	gint i = 0;
	gint j = 0;
	gint reread = 0;
	
	playlist_foreach_selected(plist__reread_file_meta_foreach, NULL);
	
	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {
		
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(play_store), &iter)) {
			
			reread = 0;
			
			if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {
				reread = 1;
			} else {
				j = 0;
				while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store),
								     &iter_child, &iter, j++)) {
					if (gtk_tree_selection_iter_is_selected(play_select,
										&iter_child)) {
						reread = 1;
						break;
					}
				}
			}
			
			if (reread) {
				recalc_album_node(&iter);
			}
		}
	}
	
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
playlist_filemeta_get(char * physical_name, char * alt_name, int composit) {

	gchar display_name[MAXLEN];
	gchar artist_name[MAXLEN];
	gchar record_name[MAXLEN];
	gchar track_name[MAXLEN];
	metadata * meta = NULL;
	gint use_meta = 0;
	gchar * substr;

#if defined(HAVE_MOD) && (defined(HAVE_LIBZ) || defined(HAV_LIBBZ2))
        gchar * pos;
#endif /* (HAVE_MOD && (HAVE_LIBZ || HAVE_LIBBZ2)) */

#ifdef HAVE_MOD
        if (is_valid_mod_extension(physical_name)) {
                composit = 0;
        }

#ifdef HAVE_LIBZ	
        if ((pos = strrchr(physical_name, '.')) != NULL) {
                pos++;

                if (!strcasecmp(pos, "gz")) {
                    composit = 0;
                }

#ifdef HAVE_LIBBZ2
                if (!strcasecmp(pos, "bz2")) {
                    composit = 0;
                }
#endif /* HAVE LIBBZ2 */

        }
#endif /* HAVE LIBZ */

#endif /* HAVE_MOD */

	playlist_filemeta * plfm = calloc(1, sizeof(playlist_filemeta));
	if (!plfm) {
		fprintf(stderr, "calloc error in playlist_filemeta_get()\n");
		return NULL;
	}

	if ((plfm->duration = get_file_duration(physical_name)) < 0.0f) {
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
		if (composit) {
			make_title_string(display_name, options.title_format,
					  artist_name, record_name, track_name);
		} else {
			strncpy(display_name, track_name, MAXLEN-1);

		}
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

	plfm->title = g_strdup(display_name);

	return plfm;
}


void
playlist_filemeta_free(playlist_filemeta * plfm) {

	free(plfm->title);
	free(plfm);
}


void
add_file_to_playlist(gchar *filename) {

	gchar voladj_str[32];
	gchar duration_str[MAXLEN];
	GtkTreeIter play_iter;
	playlist_filemeta * plfm = NULL;

	if ((plfm = playlist_filemeta_get(filename, NULL, 1)) == NULL) {
		return;
	}

        voladj2str(plfm->voladj, voladj_str);
	time2time_na(plfm->duration, duration_str);

        gtk_tree_store_append(play_store, &play_iter, NULL);
        gtk_tree_store_set(play_store, &play_iter, COLUMN_TRACK_NAME, plfm->title, COLUMN_PHYSICAL_FILENAME, filename,
                           COLUMN_SELECTION_COLOR, pl_color_inactive,
                           COLUMN_VOLUME_ADJUSTMENT, plfm->voladj, COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str,
                           COLUMN_DURATION, plfm->duration, COLUMN_DURATION_DISP, duration_str, COLUMN_FONT_WEIGHT, PANGO_WEIGHT_NORMAL, -1);

	playlist_filemeta_free(plfm);
}


static int
filter(const struct dirent * de) {

	return de->d_name[0] != '.';
}


void
add_dir_to_playlist(char * dirname) {

	gint i, n;
	struct dirent ** ent;
	struct stat st_file;
	gchar path[MAXLEN];

	n = scandir(dirname, &ent, filter, alphasort);
	for (i = 0; i < n; i++) {

		snprintf(path, MAXLEN-1, "%s/%s", dirname, ent[i]->d_name);

		if (stat(path, &st_file) == -1) {
			continue;
		}

		if (S_ISDIR(st_file.st_mode)) {
			add_dir_to_playlist(path);
		} else {
			add_file_to_playlist(path);
		}

		free(ent[i]);
	}

	if (n > 0) {
		free(ent);
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

        deflicker();
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        assign_audio_fc_filters(GTK_FILE_CHOOSER(dialog));
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		GSList *lfiles, *node;

                iw_show_info(dialog, options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), _("Adding files..."));

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
                iw_hide_info();
        }


        gtk_widget_destroy(dialog);

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

        deflicker();
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		GSList *lfiles, *node;

                iw_show_info(dialog, options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), _("Adding files recursively..."));

                strncpy(options.currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
			MAXLEN-1);

                lfiles = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));

                node = lfiles;

                while (node) {

                        add_dir_to_playlist((char *)node->data);
                        g_free(node->data);

                        node = node->next;
                }

                g_slist_free(lfiles);
                iw_hide_info();
        }


        gtk_widget_destroy(dialog);

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
	gint i = 0;

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
	gint i = 0, old_pos = 0, old_pos_child = -1;
        GtkTreePath * visible_path;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		GtkTreeIter iter_child;
		gint j = 0;
		gint modified = 0;
		gchar * str;

		gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter);

		if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {
			gtk_tree_store_remove(play_store, &iter);
			--i;
                        old_pos = i;
			continue;
		}

		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, &iter, j++)) {
			if (gtk_tree_selection_iter_is_selected(play_select, &iter_child)) {

				gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter_child, COLUMN_SELECTION_COLOR, &str, -1);
				if (strcmp(str, pl_color_active) == 0) {
					unmark_track(&iter_child);
				}

				g_free(str);

				gtk_tree_store_remove(play_store, &iter_child);
				--j;
                                old_pos_child = j;
				modified = 1;
			}
		}

		/* if all tracks have been removed, also remove album node; else recalc album node */
		if (n) {
			if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter) == 0) {
				gtk_tree_store_remove(play_store, &iter);
				--i;
                                old_pos = i;
			} else if (modified) {
				recalc_album_node(&iter);

				gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, COLUMN_SELECTION_COLOR, &str, -1);
				if (strcmp(str, pl_color_active) == 0) {
					unmark_track(&iter);
					mark_track(&iter);
				}
				g_free(str);
			}
		}
	}


        if (old_pos_child == -1) {

                for(i=0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i); i++);

                if (i) {
                        if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, old_pos)) {
                                gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i-1);
                        }

                        if(!old_pos) {
                                gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, 0);
                        }

                        visible_path = gtk_tree_model_get_path (GTK_TREE_MODEL(play_store), &iter);
                        if (visible_path) {
                                gtk_tree_view_set_cursor (GTK_TREE_VIEW (play_list), visible_path, NULL, TRUE);
                                gtk_tree_path_free(visible_path);
                        }
                }
        }

	playlist_content_changed();
}


void
rem__dead_cb(gpointer data) {

	GtkTreeIter iter;
	gint i = 0;
        gchar *filename;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

                gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, COLUMN_PHYSICAL_FILENAME, &filename, -1);

                gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter);

                if (!n && g_file_test (filename, G_FILE_TEST_IS_REGULAR) == FALSE) {
                        g_free (filename);
                        gtk_tree_store_remove(play_store, &iter);
                        --i;
                        continue;
                }

                g_free (filename);

		GtkTreeIter iter_child;
		gint j = 0;

                if (n) {

                        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, &iter, j++)) {
                                gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter_child, 1, &filename, -1);

                                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR) == FALSE) {
                                        gtk_tree_store_remove(play_store, &iter_child);
                                        --j;
                                }

                                g_free (filename);
                        }

                        /* remove album node if empty */
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

	gint j = 0;
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
	gint i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter);

		if (n) { /* album node */
			if (any_tracks_selected(&iter)) {
				gint j = 0;
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

	gint avail;
	gint track_width;
	gint rva_width;
	gint length_width;


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
playlist_child_stats(GtkTreeIter * iter, int * count, float * duration, float * songs_size, int selected) {

	gint j = 0;
	gchar * tstr;
	struct stat statbuf;
	GtkTreeIter iter_child;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, iter, j++)) {
		
		if (!selected || gtk_tree_selection_iter_is_selected(play_select, &iter_child)) {
			
			float len = 0;

			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter_child, COLUMN_DURATION, &len, 
                                           COLUMN_PHYSICAL_FILENAME, &tstr, -1);
			*duration += len;
			(*count)++;
			if (stat(tstr, &statbuf) != -1) {
				*songs_size += statbuf.st_size / 1024.0;
			}
			g_free(tstr);
		}
	}
}


/* if selected == true -> stats for selected tracks; else: all tracks */
void
playlist_stats(int selected) {

	GtkTreeIter iter;
	gint i = 0;

	gint count = 0;
	gfloat duration = 0;
	gfloat len = 0;
	gchar str[MAXLEN];
	gchar time[MAXLEN];
        gchar * tstr;
        gfloat songs_size, m_size;
        struct stat statbuf;

	if (!options.enable_playlist_statusbar) return;

        songs_size = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter);
		if (n > 0) { /* album node -- count children tracks */
			if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {
				playlist_child_stats(&iter, &count, &duration, &songs_size, 0/*false*/);
			} else {
				playlist_child_stats(&iter, &count, &duration, &songs_size, selected);
			}
		} else {
			if (!selected || gtk_tree_selection_iter_is_selected(play_select, &iter)) {
				gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, COLUMN_DURATION, &len, 
                                                   COLUMN_PHYSICAL_FILENAME, &tstr, -1);
				duration += len;
				count++;
				if (stat(tstr, &statbuf) != -1) {
					songs_size += statbuf.st_size / 1024.0;
				}
				g_free(tstr);
			}
                }
	}


	time2time(duration, time);
        m_size = songs_size / 1024.0;

	if (count == 1) {
                if (options.pl_statusbar_show_size && (m_size > 0)) {
                        if(m_size < 1024) {
                                sprintf(str, _("%d track [%s] (%.1f MB)"), count, time, m_size);
                        } else {
                                sprintf(str, _("%d track [%s] (%.1f GB)"), count, time, m_size / 1024.0);
                        }
                } else {
                        sprintf(str, _("%d track [%s] "), count, time);
                }
	} else {
                if (options.pl_statusbar_show_size && (m_size > 0)) {
                        if(m_size < 1024) {
                                sprintf(str, _("%d tracks [%s] (%.1f MB)"), count, time, m_size);
                        } else {
                                sprintf(str, _("%d tracks [%s] (%.1f GB)"), count, time, m_size / 1024.0);
                        }
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
	gint count = 0;
	gfloat duration = 0;
	gfloat songs_size;
	gchar time[MAXLEN];

	playlist_child_stats(iter, &count, &duration, &songs_size, 0/*false*/);
	time2time(duration, time);
	gtk_tree_store_set(play_store, iter, COLUMN_DURATION, duration, COLUMN_DURATION_DISP, time, -1);
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
playlist_drag_data_get(GtkWidget * widget, GdkDragContext * drag_context,
		      GtkSelectionData * data, guint info, guint time, gpointer user_data) {

	gtk_selection_data_set(data, data->target, 8, (const guchar *) "list\0", 5);
}


void
playlist_perform_drag(GtkTreeModel * model,
		      GtkTreeIter * sel_iter, GtkTreePath * sel_path,
		      GtkTreeIter * pos_iter, GtkTreePath * pos_path) {

	gint cmp = gtk_tree_path_compare(sel_path, pos_path);
	gint sel_depth = gtk_tree_path_get_depth(sel_path);
	gint pos_depth = gtk_tree_path_get_depth(pos_path);

	gint * sel_idx = gtk_tree_path_get_indices(sel_path);
	gint * pos_idx = gtk_tree_path_get_indices(pos_path);

	if (cmp == 0) {
		return;
	}

	if (sel_depth == pos_depth && (sel_depth == 1 /* top */ || sel_idx[0] == pos_idx[0])) {

		GtkTreeIter parent;

		if (cmp == 1) {
			gtk_tree_store_move_before(play_store, sel_iter, pos_iter);
		} else {
			gtk_tree_store_move_after(play_store, sel_iter, pos_iter);
		}

		if (gtk_tree_model_iter_parent(model, &parent, sel_iter)) {
			unmark_track(&parent);
			mark_track(&parent);
		}
	} else {

		GtkTreeIter iter;
		GtkTreeIter sel_parent;
		GtkTreeIter pos_parent;
		gint recalc_sel_parent = 0;
		gint recalc_pos_parent = 0;
		gchar * tname;
		gchar * fname;
		gchar * color;
		gfloat voladj;
		gchar * voldisp;
		gfloat duration;
		gchar * durdisp;
		gint fontw;

		if (gtk_tree_model_iter_has_child(model, sel_iter)) {
			return;
		}

		if (gtk_tree_model_iter_parent(model, &sel_parent, sel_iter)) {
			recalc_sel_parent = 1;
		}

		if (gtk_tree_model_iter_parent(model, &pos_parent, pos_iter)) {
			recalc_pos_parent = 1;
		}

		gtk_tree_model_get(model, sel_iter, COLUMN_TRACK_NAME, &tname, 
                                   COLUMN_PHYSICAL_FILENAME, &fname, COLUMN_SELECTION_COLOR, &color,
				   COLUMN_VOLUME_ADJUSTMENT, &voladj, COLUMN_VOLUME_ADJUSTMENT_DISP, &voldisp, 
                                   COLUMN_DURATION, &duration, COLUMN_DURATION_DISP, &durdisp, 
                                   COLUMN_FONT_WEIGHT, &fontw, -1);

		if (cmp == 1) {
			gtk_tree_store_insert_before(play_store, &iter, NULL, pos_iter);
		} else {
			gtk_tree_store_insert_after(play_store, &iter, NULL, pos_iter);
		}

		gtk_tree_store_set(play_store, &iter, COLUMN_TRACK_NAME, tname, COLUMN_PHYSICAL_FILENAME, fname, 
                                   COLUMN_SELECTION_COLOR, color, COLUMN_VOLUME_ADJUSTMENT, voladj, 
                                   COLUMN_VOLUME_ADJUSTMENT_DISP, voldisp, COLUMN_DURATION, duration,
				   COLUMN_DURATION_DISP, durdisp, COLUMN_FONT_WEIGHT, fontw, -1);

		gtk_tree_store_remove(play_store, sel_iter);

		if (recalc_sel_parent) {
			if (gtk_tree_model_iter_has_child(model, &sel_parent)) {
				recalc_album_node(&sel_parent);
				unmark_track(&sel_parent);
				mark_track(&sel_parent);
			} else {
				gtk_tree_store_remove(play_store, &sel_parent);
			}
		}

		if (recalc_pos_parent) {
			recalc_album_node(&pos_parent);
			unmark_track(&pos_parent);
			mark_track(&pos_parent);
		}

		g_free(tname);
		g_free(fname);
		g_free(color);
		g_free(voldisp);
		g_free(durdisp);
	}
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
			    GtkSelectionData  * data, guint info, guint time) {

	GtkTreeViewColumn * column;

	if (info == 0) { /* drag and drop inside Aqualung */

		if (!strcmp((gchar *)data->data, "store")) {

			GtkTreePath * path = NULL;
			GtkTreeIter * piter = NULL;
			GtkTreeIter iter;
			GtkTreeIter parent;
			GtkTreeModel * model;
			int depth;

			if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {
				depth = gtk_tree_path_get_depth(gtk_tree_model_get_path(model, &iter));
#ifdef HAVE_CDDA
				if (is_store_iter_cdda(&iter))
					depth += 1;
#endif /* HAVE_CDDA */
			} else {
				return FALSE;
			}

			if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(play_list),
							  x, y, &path, &column, NULL, NULL)) {

				if (depth != 4) { /* dragging store, artist or record */
					while (gtk_tree_path_get_depth(path) > 1) {
						gtk_tree_path_up(path);
					}
				}

				gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path);
				piter = &iter;
			}

			switch (depth) {
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
					unmark_track(&parent);
					mark_track(&parent);
				}

				break;
			}

			if (path) {
				gtk_tree_path_free(path);
			}
			
		} else if (!strcmp((gchar *)data->data, "list")) {

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

	} else { /* drag and drop from external app */

		gchar ** uri_list;
		gchar * str = NULL;
		int i;
		char file[MAXLEN];
		struct stat st_file;

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

			if (stat(file, &st_file) == 0) {
				if (S_ISDIR(st_file.st_mode)) {
					add_dir_to_playlist(file);
				} else {
					add_file_to_playlist(file);
				}
			}
		}

		g_strfreev(uri_list);

		playlist_remove_scroll_tags();
	}

	return FALSE;
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
	GtkWidget * statusbar_scrolledwin;
	GtkWidget * statusbar_viewport;

	gint i;

	GdkPixbuf * pixbuf;
	gchar path[MAXLEN];

	GtkTargetEntry source_table[] = {
		{ "STRING", GTK_TARGET_SAME_APP, 0 }
	};

	GtkTargetEntry target_table[] = {
		{ "STRING", GTK_TARGET_SAME_APP, 0 },
		{ "STRING", 0, 1 },
		{ "text/plain", 0, 1 },
		{ "text/uri-list", 0, 1 }
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
        }

	gtk_widget_set_events(playlist_window, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);


        if (!options.playlist_is_embedded) {
                plist_menu = gtk_menu_new();
                init_plist_menu(plist_menu);
        }

        vbox = gtk_vbox_new(FALSE, 2);
        gtk_container_add(GTK_CONTAINER(playlist_window), vbox);

        /* create playlist */
	if (!play_store) {
		play_store = gtk_tree_store_new(NUMBER_OF_COLUMNS,
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
                        g_object_set(G_OBJECT(track_renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
                        gtk_cell_renderer_set_fixed_size(track_renderer, 10, -1);
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


	/* setup drag and drop */
	
	gtk_drag_source_set(play_list,
			    GDK_BUTTON1_MASK,
			    source_table,
			    1,
			    GDK_ACTION_MOVE);
	
	gtk_drag_dest_set(play_list,
			  GTK_DEST_DEFAULT_ALL,
			  target_table,
			  4,
			  GDK_ACTION_MOVE | GDK_ACTION_COPY);

	snprintf(path, MAXLEN-1, "%s/drag.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		gtk_drag_source_set_icon_pixbuf(play_list, pixbuf);
	}	

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
        GTK_WIDGET_UNSET_FLAGS(add_button, GTK_CAN_FOCUS);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), add_button, _("Add files to playlist\n(Press right mouse button for menu)"), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_bottom), add_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(add_files), NULL);

	selall_button = gtk_button_new_with_label(_("Select all"));
        GTK_WIDGET_UNSET_FLAGS(selall_button, GTK_CAN_FOCUS);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), selall_button, _("Select all songs in playlist\n(Press right mouse button for menu)"), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_bottom), selall_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(selall_button), "clicked", G_CALLBACK(select_all), NULL);
	
	remsel_button = gtk_button_new_with_label(_("Remove selected"));
        GTK_WIDGET_UNSET_FLAGS(remsel_button, GTK_CAN_FOCUS);
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

	gchar * track_name;
	gchar * phys_name;
	gchar *converted_temp;
	gchar * color;
	gfloat voladj;
	gfloat duration;
        gchar str[32];
        xmlNodePtr node;

	gtk_tree_model_get(GTK_TREE_MODEL(play_store), piter,
			   COLUMN_TRACK_NAME, &track_name, COLUMN_PHYSICAL_FILENAME, &phys_name, 
                           COLUMN_SELECTION_COLOR, &color,
			   COLUMN_VOLUME_ADJUSTMENT, &voladj, COLUMN_DURATION, &duration, -1);
	
	node = xmlNewTextChild(root, NULL, (const xmlChar*) nodeID, NULL);
	
	xmlNewTextChild(node, NULL, (const xmlChar*) "track_name", (const xmlChar*) track_name);
	if (!strcmp(nodeID,"record")) {
		xmlNewTextChild(node, NULL, (const xmlChar*) "phys_name", (const xmlChar*) phys_name);
	} else {
		if ((strlen(phys_name) > 4) &&
		    (phys_name[0] == 'C') &&
		    (phys_name[1] == 'D') &&
		    (phys_name[2] == 'D') &&
		    (phys_name[3] == 'A')) {
			converted_temp = strdup(phys_name);
		} else {
			converted_temp = g_filename_to_uri(phys_name, NULL, NULL);
		}
		xmlNewTextChild(node, NULL, (const xmlChar*) "phys_name", (const xmlChar*) converted_temp);
		g_free(converted_temp);
	};

	/* FIXME: don't use #000000 (black) color for active song */
	
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

        gint i = 0;
        GtkTreeIter iter;
        xmlDocPtr doc;
        xmlNodePtr root;

        doc = xmlNewDoc((const xmlChar*) "1.0");
        root = xmlNewNode(NULL, (const xmlChar*) "aqualung_playlist");
        xmlDocSetRootElement(doc, root);

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter);

		if (n) { /* album node */
			gint j = 0;
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
	xmlFreeDoc(doc);
}


void
parse_playlist_track(xmlDocPtr doc, xmlNodePtr cur, GtkTreeIter * pparent_iter, int sel_ok) {

        xmlChar * key;
	gchar *converted_temp;
	GError *error;
	GtkTreeIter iter;
	gint is_record_node;
	gchar track_name[MAXLEN];
	gchar phys_name[MAXLEN];
	gchar color[32];
	gfloat voladj = 0.0f;
	gchar voladj_str[32];
	gfloat duration = 0.0f;
	gchar duration_str[MAXLEN];

	track_name[0] = '\0';
	phys_name[0] = '\0';
	color[0] = '\0';
	
	is_record_node = !xmlStrcmp(cur->name, (const xmlChar *)"record");
        cur = cur->xmlChildrenNode;
	if (cur) {
	    gtk_tree_store_append(play_store, &iter, pparent_iter);
	    gtk_tree_store_set(play_store, &iter, COLUMN_TRACK_NAME, "", COLUMN_PHYSICAL_FILENAME, "", 
                               COLUMN_SELECTION_COLOR, "", COLUMN_VOLUME_ADJUSTMENT, 0.0f, 
			       COLUMN_VOLUME_ADJUSTMENT_DISP, "", COLUMN_DURATION, 0.0f, COLUMN_DURATION_DISP, "", -1);
	}

        while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"track_name"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                strncpy(track_name, (char *) key, sizeof(track_name)-1);
				track_name[sizeof(track_name)-1] = '\0';
			};
                        xmlFree(key);
			gtk_tree_store_set(play_store, &iter, COLUMN_TRACK_NAME, track_name, -1); 
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"phys_name"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				if (is_record_node) {
				    /* "record" node keeps special data here */
				    strncpy(phys_name, (char *) key, sizeof(phys_name)-1);
				    phys_name[sizeof(phys_name)-1] = '\0';
				} else if ((converted_temp = g_filename_from_uri((char *) key, NULL, NULL))) {
                            	    strncpy(phys_name, converted_temp, sizeof(phys_name)-1);
				    phys_name[sizeof(phys_name)-1] = '\0';
				    g_free(converted_temp);
				} else {
				    /* try to read utf8 filename from outdated file  */
				    converted_temp = g_locale_from_utf8((char *) key, -1, NULL, NULL, &error);
				    if ((converted_temp!=NULL)) {
					strncpy(phys_name, converted_temp, sizeof(phys_name)-1);
					phys_name[sizeof(phys_name)-1] = '\0';
					g_free(converted_temp);
				    } else {
				        /* last try - maybe it's plain locale filename */
					strncpy(phys_name, (char *) key, sizeof(phys_name)-1);
					phys_name[sizeof(phys_name)-1] = '\0';
				    };
				};
			};
                        xmlFree(key);
			gtk_tree_store_set(play_store, &iter, COLUMN_PHYSICAL_FILENAME, phys_name, -1); 
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
			gtk_tree_store_set(play_store, &iter, COLUMN_SELECTION_COLOR, color, -1);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"voladj"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				voladj = convf((char *) key);
                        }
                        xmlFree(key);
			voladj2str(voladj, voladj_str);
			gtk_tree_store_set(play_store, &iter, COLUMN_VOLUME_ADJUSTMENT, voladj, 
                                           COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str, -1);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"duration"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				duration = convf((char *) key);
                        }
                        xmlFree(key);
			time2time_na(duration, duration_str);
			gtk_tree_store_set(play_store, &iter, COLUMN_DURATION, duration, COLUMN_DURATION_DISP, duration_str, -1);
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
	gint sel_ok = 0;

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
	gint c;
	gint i = 0;
	gint n;
	gchar * str;
	gchar pl_dir[MAXLEN];
	gchar line[MAXLEN];
	gchar name[MAXLEN];
	gchar path[MAXLEN];
	gchar tmp[MAXLEN];
	gint have_name = 0;
	GtkTreeIter iter;
	gchar voladj_str[32];
	gchar duration_str[MAXLEN];
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
				have_name = 0;

				
				plfm = playlist_filemeta_get(path,
							     have_name ? name : NULL,
							     1);
				if (plfm == NULL) {
					fprintf(stderr, "load_m3u(): playlist_filemeta_get() returned NULL\n");
					continue;
				}

				time2time_na(plfm->duration, duration_str);
				voladj2str(plfm->voladj, voladj_str);

				gtk_tree_store_append(play_store, &iter, NULL);
				gtk_tree_store_set(play_store, &iter,
						   COLUMN_TRACK_NAME, plfm->title,
						   COLUMN_PHYSICAL_FILENAME, g_locale_to_utf8(path, -1, NULL, NULL, NULL),
						   COLUMN_SELECTION_COLOR, pl_color_inactive,
						   COLUMN_VOLUME_ADJUSTMENT, plfm->voladj, COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str,
						   COLUMN_DURATION, plfm->duration, COLUMN_DURATION_DISP, duration_str,
						   COLUMN_FONT_WEIGHT, PANGO_WEIGHT_NORMAL, -1);

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
	gint c;
	gint i = 0;
	gint n;
	gchar * str;
	gchar pl_dir[MAXLEN];
	gchar line[MAXLEN];
	gchar file[MAXLEN];
	gchar title[MAXLEN];
	gchar tmp[MAXLEN];
	gint have_file = 0;
	gint have_title = 0;
	gchar numstr_file[10];
	gchar numstr_title[10];
	GtkTreeIter iter;
	gchar voladj_str[32];
	gchar duration_str[32];
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
						     have_title ? title : NULL,
						     1);
			if (plfm == NULL) {
				fprintf(stderr, "load_pls(): playlist_filemeta_get() returned NULL\n");
				continue;
			}

			time2time_na(plfm->duration, duration_str);
			voladj2str(plfm->voladj, voladj_str);

			gtk_tree_store_append(play_store, &iter, NULL);
			gtk_tree_store_set(play_store, &iter,
					   COLUMN_TRACK_NAME, plfm->title,
					   COLUMN_PHYSICAL_FILENAME, g_locale_to_utf8(file, -1, NULL, NULL, NULL),
					   COLUMN_SELECTION_COLOR, pl_color_inactive,
					   COLUMN_VOLUME_ADJUSTMENT, plfm->voladj, COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str,
					   COLUMN_DURATION, plfm->duration, COLUMN_DURATION_DISP, duration_str, -1);

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
	gchar buf[] = "<?xml version=\"1.0\"?>\n<aqualung_playlist>\0";
	gchar inbuf[64];
	gint len;

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

	gchar fullname[MAXLEN];
	gchar * path = filename;
	GtkTreeIter iter;
	gchar voladj_str[32];
	gchar duration_str[MAXLEN];
	playlist_filemeta * plfm = NULL;


	if (!filename)
		return;

	switch (filename[0]) {
	case '/':
		strcpy(fullname, filename);
		break;
	case '~':
		++path;
		snprintf(fullname, MAXLEN-1, "%s/%s", options.home, path);
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

		plfm = playlist_filemeta_get(fullname, NULL, 1);
		if (plfm == NULL) {
			fprintf(stderr, "add_to_playlist(): playlist_filemeta_get() returned NULL\n");
			return;
		}

		voladj2str(plfm->voladj, voladj_str);
		time2time_na(plfm->duration, duration_str);

                gtk_tree_store_append(play_store, &iter, NULL);
                gtk_tree_store_set(play_store, &iter,
				   COLUMN_TRACK_NAME, plfm->title,
				   COLUMN_PHYSICAL_FILENAME, g_locale_to_utf8(fullname, -1, NULL, NULL, NULL),
				   COLUMN_SELECTION_COLOR, pl_color_inactive,
				   COLUMN_VOLUME_ADJUSTMENT, plfm->voladj, COLUMN_VOLUME_ADJUSTMENT_DISP, voladj_str,
				   COLUMN_DURATION, plfm->duration, COLUMN_DURATION_DISP, duration_str,
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

#ifdef HAVE_IFP
        plist__send_songs_to_iriver = gtk_menu_item_new_with_label(_("Send to iFP device"));
        plist__separator3 = gtk_separator_menu_item_new();
#endif  /* HAVE_IFP */

        plist__rva = gtk_menu_item_new_with_label(_("Calculate RVA"));
        plist__rva_menu = gtk_menu_new();
        plist__rva_separate = gtk_menu_item_new_with_label(_("Separate"));
        plist__rva_average = gtk_menu_item_new_with_label(_("Average"));
        plist__reread_file_meta = gtk_menu_item_new_with_label(_("Reread file metadata"));
        plist__separator2 = gtk_separator_menu_item_new();
        plist__fileinfo = gtk_menu_item_new_with_label(_("File info..."));
        plist__search = gtk_menu_item_new_with_label(_("Search..."));

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__save);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__load);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__enqueue);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__separator1);

#ifdef HAVE_IFP
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__send_songs_to_iriver);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__separator3);
#endif  /* HAVE_IFP */

        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__rva);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(plist__rva), plist__rva_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(plist__rva_menu), plist__rva_separate);
        gtk_menu_shell_append(GTK_MENU_SHELL(plist__rva_menu), plist__rva_average);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__reread_file_meta);
        gtk_menu_shell_append(GTK_MENU_SHELL(append_menu), plist__separator2);
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


void
show_active_position_in_playlist(void) {

        GtkTreeIter iter;
	gchar * str;
        gint flag;
        GtkTreePath * visible_path;
        GtkTreeViewColumn * visible_column;

        flag = 0;

        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(play_store), &iter)) {

                do {
			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, COLUMN_SELECTION_COLOR, &str, -1);
		        if (strcmp(str, pl_color_active) == 0) {
                                flag = 1;
		                if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {
                                    gtk_tree_view_get_cursor(GTK_TREE_VIEW(play_list), &visible_path, &visible_column);
                                }
                                break;
                        }
			g_free(str);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(play_store), &iter));

                if (!flag) {
                        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, 0);
                }

                visible_path = gtk_tree_model_get_path (GTK_TREE_MODEL(play_store), &iter);
                if (visible_path) {
                        gtk_tree_view_set_cursor (GTK_TREE_VIEW (play_list), visible_path, NULL, TRUE);
                        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (play_list), visible_path,
                                                      NULL, TRUE, 0.3, 0.0);
                        gtk_tree_path_free(visible_path);
                }
        }
}

void
show_active_position_in_playlist_toggle(void) {

        GtkTreeIter iter, iter_child;
	gchar * str;
        gint flag, cflag, j;
        GtkTreePath * visible_path;
        GtkTreeViewColumn * visible_column;

        flag = cflag = 0;

        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(play_store), &iter)) {

                do {
			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, COLUMN_SELECTION_COLOR, &str, -1);
		        if (strcmp(str, pl_color_active) == 0) {
                                flag = 1;
		                if (gtk_tree_selection_iter_is_selected(play_select, &iter) &&
                                    gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter)) {

                                        gtk_tree_view_get_cursor(GTK_TREE_VIEW(play_list), &visible_path, &visible_column);

                                        if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(play_list), visible_path)) {
                                                gtk_tree_view_expand_row(GTK_TREE_VIEW(play_list), visible_path, FALSE);
                                        }

                                        j = 0;

                                        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, &iter, j++)) {
                                                gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter_child, COLUMN_SELECTION_COLOR, &str, -1);
                                                if (strcmp(str, pl_color_active) == 0) {
                                                        cflag = 1;
                                                        break;
                                                }
                                        }
                                }
                                break;
                        }

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(play_store), &iter));

                if (flag) {
                        if (cflag) {
                                visible_path = gtk_tree_model_get_path (GTK_TREE_MODEL(play_store), &iter_child);
                                if (visible_path) {
                                        gtk_tree_view_set_cursor (GTK_TREE_VIEW (play_list), visible_path, NULL, TRUE);
                                        gtk_tree_path_free(visible_path);
                                }
                        } else {
                                visible_path = gtk_tree_model_get_path (GTK_TREE_MODEL(play_store), &iter);
                                if (visible_path) {
                                        gtk_tree_view_set_cursor (GTK_TREE_VIEW (play_list), visible_path, NULL, TRUE);
                                        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (play_list), visible_path,
                                                                      NULL, TRUE, 0.3, 0.0);
                                        gtk_tree_path_free(visible_path);
                                }
                        }
                }
        }
}

void
show_last_position_in_playlist(void) {

        GtkTreeIter iter;
	gint i;
        GtkTreePath * visible_path;

        for(i=0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i); i++);
        
        if(i) {
                gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i-1);

                visible_path = gtk_tree_model_get_path (GTK_TREE_MODEL(play_store), &iter);
                if (visible_path) {
                        gtk_tree_view_set_cursor (GTK_TREE_VIEW (play_list), visible_path, NULL, TRUE);
                        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (play_list), visible_path,
                                                      NULL, TRUE, 1.0, 0.0);
                        gtk_tree_path_free(visible_path);
                }
        }
}

void
expand_collapse_album_node(void) {

	GtkTreeIter iter, iter_child;
	gint i, j;
        GtkTreePath *path;
        GtkTreeViewColumn *column;

        i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(play_select, &iter) &&
                    gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter)) {

                        gtk_tree_view_get_cursor(GTK_TREE_VIEW(play_list), &path, &column);

                        if (path && gtk_tree_view_row_expanded(GTK_TREE_VIEW(play_list), path)) {
                                gtk_tree_view_collapse_row(GTK_TREE_VIEW(play_list), path);
                        } else {
                                gtk_tree_view_expand_row(GTK_TREE_VIEW(play_list), path, FALSE);
                        }
                        
                        continue;
                }

                j = 0;

                while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, &iter, j++)) {

                        if (gtk_tree_selection_iter_is_selected(play_select, &iter_child) &&
                            gtk_tree_model_iter_parent(GTK_TREE_MODEL(play_store), &iter, &iter_child)) {

                                path = gtk_tree_model_get_path (GTK_TREE_MODEL(play_store), &iter);

                                if (path) {
                                        gtk_tree_view_collapse_row(GTK_TREE_VIEW(play_list), path);
                                        gtk_tree_view_set_cursor (GTK_TREE_VIEW (play_list), path, NULL, TRUE);
                                }
                        }
                }
        }
}

void
iw_show_info (GtkWidget *dialog, GtkWindow *transient, gchar *message) {

        GtkWidget *frame;
        GtkWidget *label;

        if (dialog != NULL) {
                gtk_widget_hide (dialog);
                deflicker();
        }

        iw_widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_position(GTK_WINDOW(iw_widget), GTK_WIN_POS_CENTER_ON_PARENT);
        GTK_WIDGET_UNSET_FLAGS(iw_widget, GTK_CAN_FOCUS);
        gtk_widget_set_events(iw_widget, GDK_BUTTON_PRESS_MASK);
        gtk_window_set_modal(GTK_WINDOW(iw_widget), TRUE);
        gtk_window_set_transient_for(GTK_WINDOW(iw_widget), GTK_WINDOW(transient));
        gtk_window_set_decorated(GTK_WINDOW(iw_widget), FALSE);

        frame = gtk_frame_new(NULL);
        gtk_container_add (GTK_CONTAINER (iw_widget), frame);
        gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
        gtk_widget_show_now(frame);
        deflicker();

        label = gtk_label_new (message);
        gtk_container_add (GTK_CONTAINER (frame), label);
        gtk_misc_set_padding (GTK_MISC (label), 8, 8);
        gtk_widget_show_now(label);
        deflicker();

        gtk_widget_show_now(iw_widget);
        deflicker();
}


void
iw_hide_info (void) {

        if (iw_widget != NULL) {
                gtk_widget_destroy(iw_widget);
                iw_widget = NULL;
        }

        deflicker();
}


void
remove_files (void) {

        GtkWidget *info_dialog;
        gint response, n = 0, i = 0, j, files, last_pos;
	GtkTreeIter iter, iter_child;
        GtkTreePath *visible_path;
        gchar temp[MAXLEN], *filename;

        files = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

                if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {
		        n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), &iter);
                        files++;
                }

                j = 0;
                while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter_child, &iter, j++)) {
                        if (gtk_tree_selection_iter_is_selected(play_select, &iter_child)) {
                                n = 1;
                        }
                }
                if(n) break;
        }
        
        if (!n) {

                if (files == 1) {
                        sprintf(temp, _("The selected file will be deleted from the filesystem. "
					"No recovery will be possible after this operation.\n\n"
					"Are you sure?"));
                } else {
                        sprintf(temp, _("All selected files will be deleted from the filesystem. "
					"No recovery will be possible after this operation.\n\n"
					"Are you sure?"));
                }

                info_dialog = gtk_message_dialog_new (options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                                      GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, temp);

                gtk_window_set_title(GTK_WINDOW(info_dialog), _("Remove file"));
                gtk_widget_show (info_dialog);

                response = aqualung_dialog_run (GTK_DIALOG (info_dialog));     

                gtk_widget_destroy(info_dialog);

                if (response == GTK_RESPONSE_YES) {   

                        i = last_pos = 0;
                	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {
                                if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {

					char * filename_locale;

                                        gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, COLUMN_PHYSICAL_FILENAME, &filename, -1);

					if ((filename_locale = g_locale_from_utf8(filename, -1, NULL, NULL, NULL)) == NULL) {
						g_free(filename);
						continue;
					}

                                        n = unlink(filename_locale);
                                        if (n != -1) {
                                                gtk_tree_store_remove(play_store, &iter);
                                                --i;
                                                last_pos = (i-1) < 0 ? 0 : i-1;
                                        }

                                        g_free(filename_locale);
                                        g_free(filename);
                                }
                        }

                        for(i=0; gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i); i++);

                        if(i) {
                                gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, last_pos);
                                visible_path = gtk_tree_model_get_path (GTK_TREE_MODEL(play_store), &iter);
                                if (visible_path) {
                                        gtk_tree_view_set_cursor (GTK_TREE_VIEW (play_list), visible_path, NULL, TRUE);
                                        gtk_tree_path_free(visible_path);
                                }
                        }
                        playlist_content_changed();
                }
        }
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

