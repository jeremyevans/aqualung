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
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
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
#include "i18n.h"
#include "search_playlist.h"
#include "playlist.h"

extern void set_sliders_width(void);

extern char pl_color_active[14];
extern char pl_color_inactive[14];

extern char currdir[MAXLEN];
extern char cwd[MAXLEN];

extern char title_format[MAXLEN];

extern GtkWidget* gui_stock_label_button(gchar *blabel, const gchar *bstock);

extern PangoFontDescription *fd_playlist;

extern int override_skin_settings;

int auto_save_playlist = 1;
int show_rva_in_playlist = 0;
int show_length_in_playlist = 1;
int show_track_name_using_bold = 1;
int plcol_idx[3] = { 0, 1, 2 };

int auto_use_ext_meta_artist = 0;
int auto_use_ext_meta_record = 0;
int auto_use_ext_meta_track = 0;

int enable_playlist_statusbar = 1;
int enable_playlist_statusbar_shadow = 1;

int alt_L;
int alt_R;
int shift_L;
int shift_R;
int ctrl_L;
int ctrl_R;

GtkWidget * playlist_window;
GtkWidget * da_dialog;

extern GtkWidget * main_window;
extern GtkWidget * info_window;
extern GtkWidget * vol_window;

int playlist_pos_x;
int playlist_pos_y;
int playlist_size_x;
int playlist_size_y;
int playlist_on;
int playlist_color_is_set;
extern int main_size_x;
extern int main_size_y;

extern int playlist_is_embedded;

extern int rva_is_enabled;
extern float rva_refvol;
extern float rva_steepness;
extern int rva_use_averaging;
extern int rva_use_linear_thresh;
extern float rva_avg_linear_thresh;
extern float rva_avg_stddev_thresh;

extern int drift_x;
extern int drift_y;

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

GtkWidget * play_list;
GtkListStore * play_store = 0;
GtkTreeSelection * play_select;
GtkTreeViewColumn * track_column;
GtkTreeViewColumn * rva_column;
GtkTreeViewColumn * length_column;
GtkCellRenderer * track_renderer;
GtkCellRenderer * rva_renderer;
GtkCellRenderer * length_renderer;

GtkWidget * statusbar_total;
GtkWidget * statusbar_selected;

/* popup menus */
GtkWidget * add_menu;
GtkWidget * add__single;
GtkWidget * add__direct;
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
GtkWidget * plist__separator2;
GtkWidget * plist__fileinfo;
GtkWidget * plist__search;

char command[RB_CONTROL_SIZE];

char fileinfo_name[MAXLEN];
char fileinfo_file[MAXLEN];

extern int is_file_loaded;
extern int is_paused;
extern int allow_seeks;

extern char current_file[MAXLEN];


extern pthread_mutex_t disk_thread_lock;
extern pthread_cond_t  disk_thread_wake;
extern jack_ringbuffer_t * rb_gui2disk;

extern GtkWidget * playlist_toggle;

extern int drag_info;


/* used to store array of sequence numbers of tracks selected for RVA calc */
int * seqnums = NULL;


void rem__sel_cb(gpointer data);
void cut__sel_cb(gpointer data);
void plist__search_cb(gpointer data);
void direct_add(GtkWidget * widget, gpointer * data);


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

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(play_store), &iter)) {
		do {
        		gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_NORMAL, -1);
		} while (i++, gtk_tree_model_iter_next(GTK_TREE_MODEL(play_store), &iter));
        }

}

void
set_playlist_color() {
	
	GtkTreeIter iter;
	char * str;
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

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(play_store), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 2, &str, -1);

			if (strcmp(str, pl_color_active) == 0) {
				gtk_list_store_set(play_store, &iter, 2, active, -1);
                                if(show_track_name_using_bold)
                                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
				g_free(str);
                        }

			if (strcmp(str, pl_color_inactive) == 0) {
				gtk_list_store_set(play_store, &iter, 2, inactive, -1);
				gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_NORMAL, -1);
				g_free(str);
			}

		} while (i++, gtk_tree_model_iter_next(GTK_TREE_MODEL(play_store), &iter));

        }

        strcpy(pl_color_active, active);
	strcpy(pl_color_inactive, inactive);
        
}


long
get_playing_pos(GtkListStore * store) {

	GtkTreeIter iter;
	char * str;
	int i = 0;

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 2, &str, -1);
			if (strcmp(str, pl_color_active) == 0) {
				g_free(str);
				return i;
			}

			g_free(str);
		
		} while (i++, gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
	}
	return -1;
}


void 
start_playback_from_playlist(GtkTreePath * path) {

	GtkTreeIter iter;
	char * str;
	long n;
	char cmd;
	cue_t cue;

	if (!allow_seeks)
		return;

	n = get_playing_pos(play_store);
	if (n != -1) {
		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
		gtk_list_store_set(play_store, &iter, 2, pl_color_inactive, -1);
                gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_NORMAL, -1);
	}
	
	gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path);
	gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
        if(show_track_name_using_bold)
                gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
	
	n = get_playing_pos(play_store);
	gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
	gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 1, &str, 3, &(cue.voladj), -1);
	cue.filename = strdup(g_locale_from_utf8(str, -1, NULL, NULL, NULL));
	strncpy(current_file, str, MAXLEN-1);
	g_free(str);

	g_signal_handler_block(G_OBJECT(play_button), play_id);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), TRUE);
	g_signal_handler_unblock(G_OBJECT(play_button), play_id);

	if (is_paused) {
		is_paused = 0;
		g_signal_handler_block(G_OBJECT(pause_button), pause_id);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button), FALSE);
		g_signal_handler_unblock(G_OBJECT(pause_button), pause_id);
	}
		
	cmd = CMD_CUE;
	jack_ringbuffer_write(rb_gui2disk, &cmd, sizeof(char));
	jack_ringbuffer_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
	if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
		pthread_cond_signal(&disk_thread_wake);
		pthread_mutex_unlock(&disk_thread_lock);
	}
	
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
        gboolean k;

	int i;

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
                direct_add(NULL, NULL);
                return TRUE;
                break;
        case GDK_q:
	case GDK_Q:
	case GDK_Escape:
                if(!playlist_is_embedded)
                        playlist_window_close(NULL, NULL, NULL);
		return TRUE;
		break;
	case GDK_i:
	case GDK_I:
		gtk_tree_view_get_cursor(GTK_TREE_VIEW(play_list), &path, &column);

		if (path && gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path)) {

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
                if (ctrl_L || ctrl_R) {
                        cut__sel_cb(NULL);
                }                                                
		return TRUE;
		break;
	case GDK_s:
	case GDK_S:
		plist__search_cb(NULL);
		return TRUE;
		break;
        
        case GDK_Delete:
	case GDK_KP_Delete:
                if (shift_L || shift_R) {

                        gtk_list_store_clear(play_store);

                } else {

                        i = 0;
                        do {
                                k = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++);
                        } while (k && !gtk_tree_selection_iter_is_selected(play_select, &iter));

                        if(k) {

                                path = gtk_tree_model_get_path(GTK_TREE_MODEL(play_store), &iter);

                                rem__sel_cb(NULL);

                                gtk_tree_selection_select_path(play_select, path);
                                gtk_tree_view_set_cursor(GTK_TREE_VIEW(play_list), path, NULL, FALSE);

                        }
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
						  &path, &column, NULL, NULL)) {

			if (gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path)) {

				gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
						   0, &pname, 1, &pfile, -1);

				strncpy(fileinfo_name, pname, MAXLEN-1);
				strncpy(fileinfo_file, pfile, MAXLEN-1);
				free(pname);
				free(pfile);
				gtk_widget_set_sensitive(plist__fileinfo, TRUE);
			}
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
                                             playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_SAVE, 
                                             GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        set_sliders_width();

        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), currdir);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "playlist.xml");

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);


        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

		strncpy(filename, selected_filename, MAXLEN-1);
		save_playlist(filename);

                strncpy(currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
                                                                         MAXLEN-1);

                g_free(selected_filename);
        }

        gtk_widget_destroy(dialog);
}


void
plist__load_cb(gpointer data) {

        GtkWidget * dialog;
        const gchar * selected_filename;
	char filename[MAXLEN];


        dialog = gtk_file_chooser_dialog_new(_("Please specify the file to load the playlist from."), 
                                             playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_OPEN, 
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        set_sliders_width();

        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), currdir);

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);


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

                strncpy(currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
                                                                         MAXLEN-1);
        }

        gtk_widget_destroy(dialog);

	playlist_content_changed();

}


void
plist__enqueue_cb(gpointer data) {

        GtkWidget * dialog;
        const gchar * selected_filename;
	char filename[MAXLEN];


        dialog = gtk_file_chooser_dialog_new(_("Please specify the file to load the playlist from."), 
                                             playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_OPEN, 
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        set_sliders_width();

        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), currdir);

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);


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

                strncpy(currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
                                                                         MAXLEN-1);
        }

        gtk_widget_destroy(dialog);
        
        playlist_content_changed();
}


gint
watch_vol_calc(gpointer data) {

        float * volumes = (float *)data;
	GtkTreeIter iter;

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
							 rva_use_linear_thresh,
							 rva_avg_linear_thresh,
							 rva_avg_stddev_thresh,
							 rva_refvol, rva_steepness);

		voladj2str(voladj, voladj_str);

		int i = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {
			int j, k = -1;

			for (j = 0; j < vol_n_tracks; j++) {
				if (seqnums[j] == i) {
					k = j;
				}
			}

			if (k != -1) {
				gtk_list_store_set(play_store, &iter,
						   3, voladj, 4, voladj_str, -1);
			}
		}
	} else {
		float voladj;
		char voladj_str[32];

		int i = 0;

		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter,
						     NULL, i++)) {
			
			int j, k = -1;

			for (j = 0; j < vol_n_tracks; j++) {
				if (seqnums[j] == i) {
					k = j;
				}
			}

			if (k != -1) {
				
				voladj = rva_from_volume(volumes[k], rva_refvol, rva_steepness);
				voladj2str(voladj, voladj_str);
				gtk_list_store_set(play_store, &iter, 3, voladj,
						   4, voladj_str, -1);
			}
		}
	}

	free(volumes);
	volumes = NULL;
	free(seqnums);
	seqnums = NULL;
        return FALSE;
}


void
plist_setup_vol_calc(void) {

	GtkTreeIter iter;
	int i;

        char * pfile;
        char file[MAXLEN];

	vol_queue_t * q = NULL;
	float * volumes = NULL;


        if (vol_window != NULL) {
                return;
        }

	i = 0;
	vol_n_tracks = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {
		if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {

                        gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 1, &pfile, -1);
                        strncpy(file, pfile, MAXLEN-1);
                        g_free(pfile);

                        if (q == NULL) {
                                q = vol_queue_push(NULL, file, iter/*dummy*/);
                        } else {
                                vol_queue_push(q, file, iter/*dummy*/);
                        }
			++vol_n_tracks;

			seqnums = (int *)realloc(seqnums, vol_n_tracks * sizeof(int));
			if (!seqnums) {
				fprintf(stderr, "realloc error in plist_setup_vol_calc()\n");
				return;
			}
			seqnums[vol_n_tracks-1] = i;
		}
	}

	if (vol_n_tracks == 0)
		return;

	if (!rva_is_enabled) {
		GtkWidget * dialog = gtk_message_dialog_new(playlist_is_embedded ?
				         GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_INFO,
					 GTK_BUTTONS_CLOSE,
					 _("Playback RVA is currently disabled. "
					   "Enable it at Settings->Playback RVA."));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		free(seqnums);
		seqnums = NULL;
		return;
	}

	if ((volumes = calloc(vol_n_tracks, sizeof(float))) == NULL) {
		fprintf(stderr, "calloc error in plist__rva_separate_cb()\n");
		free(seqnums);
		seqnums = NULL;
		return;
	}

	calculate_volume(q, volumes);
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
plist__fileinfo_cb(gpointer data) {

	GtkTreeIter dummy;

	show_file_info(fileinfo_name, fileinfo_file, 0, NULL, dummy);
}

void
plist__search_cb(gpointer data) {
	
	search_playlist_dialog();
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


void
add_file_to_playlist(gchar *filename) {

        float voladj = 0.0f;
	char voladj_str[32];
	float duration = 0.0f;
	char duration_str[32];
	char artist_name[MAXLEN];
	char record_name[MAXLEN];
	char track_name[MAXLEN];
	char display_name[MAXLEN];
	metadata * meta = NULL;
	int use_meta = 0;
	GtkTreeIter play_iter;
	gchar * substr;


        if ((substr = strrchr(filename, '/')) == NULL)
                substr = filename;
        else
                ++substr;

        artist_name[0] = '\0';
        record_name[0] = '\0';
        track_name[0] = '\0';

        if (auto_use_ext_meta_artist ||
            auto_use_ext_meta_record ||
            auto_use_ext_meta_track) {

                meta = meta_new();
                if (!meta_read(meta, filename)) {
                        meta_free(meta);
                        meta = NULL;
                }
        }

        if ((meta != NULL) && auto_use_ext_meta_artist) {
                meta_get_artist(meta, artist_name);
                if (artist_name[0] != '\0') {
                        use_meta = 1;
                }
        }

        if ((meta != NULL) && auto_use_ext_meta_record) {
                meta_get_record(meta, record_name);
                if (record_name[0] != '\0') {
                        use_meta = 1;
                }
        }

        if ((meta != NULL) && auto_use_ext_meta_track) {
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

        if (meta != NULL) {
                meta_free(meta);
                meta = NULL;
                if (use_meta) {
                        make_title_string(display_name, title_format,
                                          artist_name, record_name, track_name);
                } else {
                        strcpy(display_name, substr);
                }
        } else {
                strcpy(display_name, substr);
        }


        if (rva_is_enabled) {
                meta = meta_new();
                if (meta_read(meta, filename)) {
                        if (!meta_get_rva(meta, &voladj)) {
                                voladj = 0.0f;
                        }
                } else {
                        voladj = 0.0f;
                }
                meta_free(meta);
        } else {
                voladj = 0.0f;
        }
        voladj2str(voladj, voladj_str);

        duration = get_file_duration(filename);
        time2time(duration, duration_str);

        gtk_list_store_append(play_store, &play_iter);
        gtk_list_store_set(play_store, &play_iter, 0, display_name, 1, filename,
                           2, pl_color_inactive,
                           3, voladj, 4, voladj_str,
                           5, duration, 6, duration_str, 7, PANGO_WEIGHT_NORMAL, -1);
}

gint
dialog_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	switch (kevent->keyval) {

        case GDK_Escape:
                gtk_dialog_response(GTK_DIALOG(da_dialog), GTK_RESPONSE_CLOSE);
                return TRUE;
                break;
        }

        return FALSE;
}

void
direct_add(GtkWidget * widget, gpointer * data) {

        GtkWidget *dialog;
        GSList *lfiles, *node;


        dialog = gtk_file_chooser_dialog_new(_("Select files..."), 
                                             playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                             GTK_FILE_CHOOSER_ACTION_OPEN, 
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        set_sliders_width();

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        deflicker();

        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), currdir);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                strncpy(currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
                                                                         MAXLEN-1);

                lfiles = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));

                node = lfiles;

                while(node) {

                        add_file_to_playlist((char *)node->data);
                        g_free(node->data);

                        node = node->next;
                }

                g_slist_free(lfiles);
        }


        delayed_playlist_rearrange(100);

        gtk_widget_destroy(dialog);

	playlist_content_changed();

}


void
select_all(GtkWidget * widget, gpointer * data) {

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
sel__inv_cb(gpointer data) {

	GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(play_select, &iter))
			gtk_tree_selection_unselect_iter(play_select, &iter);
		else
			gtk_tree_selection_select_iter(play_select, &iter);
			
	}
}


void
rem__all_cb(gpointer data) {

	gtk_list_store_clear(play_store);
	playlist_content_changed();
}


void
rem__sel_cb(gpointer data) {

	GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {
			gtk_list_store_remove(play_store, &iter);
			--i;
		}
	}

	playlist_content_changed();
}


void
remove_sel(GtkWidget * widget, gpointer * data) {

	rem__sel_cb(NULL);
}


/* cut selected callback */
void
cut__sel_cb(gpointer data) {

	GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(play_select, &iter))
			gtk_tree_selection_unselect_iter(play_select, &iter);
		else
			gtk_tree_selection_select_iter(play_select, &iter);
			
		if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {
			gtk_list_store_remove(play_store, &iter);
			--i;
		}
	}

	playlist_content_changed();
}


gint
playlist_drag_data_received(GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y, 
			    GtkSelectionData  * data, guint info, guint time) {

	GtkTreePath * path;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	GtkTreeIter * piter = 0;

	if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(play_list), x, y, &path, &column, NULL, NULL)) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path);
		piter = &iter;
	}

	switch (drag_info) {
	case 1:
		artist__addlist_cb(piter);
		break;
	case 2:
		record__addlist_cb(piter);
		break;
	case 3:
		track__addlist_cb(piter);
		break;
	}

	return FALSE;
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
		gtk_cell_renderer_get_size(GTK_CELL_RENDERER(rva_renderer),
					   play_list, NULL, NULL, NULL, &rva_width, NULL);
	} else {
		rva_width = 1;
	}

	if (gtk_tree_view_column_get_visible(GTK_TREE_VIEW_COLUMN(length_column))) {
		gtk_cell_renderer_get_size(GTK_CELL_RENDERER(length_renderer),
					   play_list, NULL, NULL, NULL, &length_width, NULL);
	} else {
		length_width = 1;
	}

	track_width = avail - rva_width - length_width - 6;
	if (track_width < 1)
		track_width = 1;

	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(track_column), track_width);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(rva_column), rva_width);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(length_column), length_width);


	if (playlist_is_embedded) {
		if (main_window->window != NULL) {
			gtk_widget_queue_draw(main_window);
			deflicker();
		}
	} else {
		if (playlist_window->window != NULL) {
			gtk_widget_queue_draw(playlist_window);
			deflicker();
		}
	}

        return TRUE;
}


void
playlist_selection_changed(GtkTreeSelection * sel, gpointer data) {

	GtkTreeIter iter;
	int i = 0;

	int count = 0;
	float duration = 0;
	float len = 0;
	char str[MAXLEN];
	char time[16];

	if (!enable_playlist_statusbar) return;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {
			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 5, &len, -1);
			duration += len;
			count++;
		}
	}

	time2time(duration, time);
	if (count == 1) {
		sprintf(str, _("%d track [%s]"), count, time);
	} else {
		sprintf(str, _("%d tracks [%s]"), count, time);
	}

	gtk_label_set_text(GTK_LABEL(statusbar_selected), str);
}


void
playlist_content_changed(void) {

	GtkTreeIter iter;
	int i = 0;

	float duration = 0;
	float len = 0;
	char str[MAXLEN];
	char time[16];

	if (!enable_playlist_statusbar) return;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {
			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 5, &len, -1);
			duration += len;
	}

	time2time(duration, time);
	if (i == 2) {
		sprintf(str, _("%d track [%s]"), i - 1, time);
	} else {
		sprintf(str, _("%d tracks [%s]"), i - 1, time);
	}

	gtk_label_set_text(GTK_LABEL(statusbar_total), str);
}

void
create_playlist(void) {

	GtkWidget * vbox;

	GtkWidget * hbox_bottom;
	GtkWidget * direct_button;
	GtkWidget * selall_button;
	GtkWidget * remsel_button;

	GtkWidget * viewport;
	GtkWidget * scrolled_win;

	GtkWidget * statusbar;
	GtkWidget * statusbar_viewport;
	GtkWidget * statusbar_label;

	int i;

        /* window creating stuff */
	if (!playlist_is_embedded) {
		playlist_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(playlist_window), _("Playlist"));
		gtk_container_set_border_width(GTK_CONTAINER(playlist_window), 2);
		g_signal_connect(G_OBJECT(playlist_window), "delete_event",
				 G_CALLBACK(playlist_window_close), NULL);
		gtk_widget_set_size_request(playlist_window, 300, 200);
	} else {
		gtk_widget_set_size_request(playlist_window, 200, 200);
	}

        g_signal_connect(G_OBJECT(playlist_window), "key_press_event",
                         G_CALLBACK(playlist_window_key_pressed), NULL);
        g_signal_connect(G_OBJECT(playlist_window), "key_release_event",
                         G_CALLBACK(playlist_window_key_released), NULL);
        g_signal_connect(G_OBJECT(playlist_window), "focus_out_event",
                         G_CALLBACK(playlist_window_focus_out), NULL);
	gtk_widget_set_events(playlist_window, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

        plist_menu = gtk_menu_new();

	plist__save = gtk_menu_item_new_with_label(_("Save playlist"));
	plist__load = gtk_menu_item_new_with_label(_("Load playlist"));
	plist__enqueue = gtk_menu_item_new_with_label(_("Enqueue playlist"));
	plist__separator1 = gtk_separator_menu_item_new();
	plist__rva = gtk_menu_item_new_with_label(_("Calculate RVA"));
	plist__rva_menu = gtk_menu_new();
	plist__rva_separate = gtk_menu_item_new_with_label(_("Separate"));
	plist__rva_average = gtk_menu_item_new_with_label(_("Average"));
	plist__separator2 = gtk_separator_menu_item_new();
	plist__fileinfo = gtk_menu_item_new_with_label(_("File info..."));
	plist__search = gtk_menu_item_new_with_label(_("Search..."));

	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__save);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__load);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__enqueue);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__rva);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(plist__rva), plist__rva_menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist__rva_menu), plist__rva_separate);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist__rva_menu), plist__rva_average);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__fileinfo);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__search);

	g_signal_connect_swapped(G_OBJECT(plist__save), "activate", G_CALLBACK(plist__save_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(plist__load), "activate", G_CALLBACK(plist__load_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(plist__enqueue), "activate", G_CALLBACK(plist__enqueue_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(plist__rva_separate), "activate", G_CALLBACK(plist__rva_separate_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(plist__rva_average), "activate", G_CALLBACK(plist__rva_average_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(plist__fileinfo), "activate", G_CALLBACK(plist__fileinfo_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(plist__search), "activate", G_CALLBACK(plist__search_cb), NULL);

	gtk_widget_show(plist__save);
	gtk_widget_show(plist__load);
	gtk_widget_show(plist__enqueue);
	gtk_widget_show(plist__separator1);
	gtk_widget_show(plist__rva);
	gtk_widget_show(plist__rva_separate);
	gtk_widget_show(plist__rva_average);
	gtk_widget_show(plist__separator2);
	gtk_widget_show(plist__fileinfo);
	gtk_widget_show(plist__search);

        vbox = gtk_vbox_new(FALSE, 2);
        gtk_container_add(GTK_CONTAINER(playlist_window), vbox);

        /* create playlist */
	if (!play_store) {
		play_store = gtk_list_store_new(8,
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

        if(override_skin_settings) {
                gtk_widget_modify_font (play_list, fd_playlist);
        }

        playlist_color_is_set = 0;

	if (playlist_is_embedded) {
		g_signal_connect(G_OBJECT(play_list), "key_press_event",
				 G_CALLBACK(playlist_window_key_pressed), NULL);
	}

	for (i = 0; i < 3; i++) {
		switch (plcol_idx[i]) {
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
			rva_column = gtk_tree_view_column_new_with_attributes("RVA",
									      rva_renderer,
									      "text", 4,
									      "foreground", 2,
                                                                              "weight", 7,
									      NULL);
			gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(rva_column),
							GTK_TREE_VIEW_COLUMN_FIXED);
			gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(rva_column), 3);
			if (show_rva_in_playlist) {
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
			length_column = gtk_tree_view_column_new_with_attributes("Length",
										 length_renderer,
										 "text", 6,
										 "foreground", 2,
                                                                                 "weight", 7,
										 NULL);
			gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(length_column),
							GTK_TREE_VIEW_COLUMN_FIXED);
			gtk_tree_view_column_set_spacing(GTK_TREE_VIEW_COLUMN(length_column), 3);
			if (show_length_in_playlist) {
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
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(play_list), TRUE);

        play_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(play_list));
        gtk_tree_selection_set_mode(play_select, GTK_SELECTION_MULTIPLE);

	g_signal_connect(G_OBJECT(play_select), "changed",
			 G_CALLBACK(playlist_selection_changed), NULL);


	g_signal_connect(G_OBJECT(play_list), "button_press_event",
			 G_CALLBACK(doubleclick_handler), NULL);
	g_signal_connect(G_OBJECT(play_list), "drag_data_received",
			 G_CALLBACK(playlist_drag_data_received), NULL);



	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), viewport, TRUE, TRUE, 0);

	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_win);

	gtk_container_add(GTK_CONTAINER(scrolled_win), play_list);


	/* statusbar */
	if (enable_playlist_statusbar) {
		statusbar_viewport = gtk_viewport_new(NULL, NULL);
		gtk_widget_set_name(statusbar_viewport, "info_viewport");
		gtk_box_pack_start(GTK_BOX(vbox), statusbar_viewport, FALSE, TRUE, 2);
		
		statusbar = gtk_hbox_new(FALSE, 0);
		gtk_container_set_border_width(GTK_CONTAINER(statusbar), 1);
		gtk_container_add(GTK_CONTAINER(statusbar_viewport), statusbar);
		
		statusbar_selected = gtk_label_new("");
		gtk_widget_set_name(statusbar_selected, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_selected, FALSE, TRUE, 0);
		
		statusbar_label = gtk_label_new(_("Selected: "));
		gtk_widget_set_name(statusbar_label, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_label, FALSE, TRUE, 0);
		
		gtk_box_pack_end(GTK_BOX(statusbar), gtk_vseparator_new(), FALSE, TRUE, 5);
		
		statusbar_total = gtk_label_new("");
		gtk_widget_set_name(statusbar_total, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_total, FALSE, TRUE, 0);
		
		statusbar_label = gtk_label_new(_("Total: "));
		gtk_widget_set_name(statusbar_label, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar), statusbar_label, FALSE, TRUE, 0);
		
		playlist_selection_changed(NULL, NULL);
		playlist_content_changed();
	}
	

	/* bottom area of playlist window */
        hbox_bottom = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox_bottom, FALSE, TRUE, 0);

	direct_button = gtk_button_new_with_label(_("Add files..."));
        gtk_box_pack_start(GTK_BOX(hbox_bottom), direct_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(direct_button), "clicked", G_CALLBACK(direct_add), NULL);

	selall_button = gtk_button_new_with_label(_("Select all"));
        gtk_box_pack_start(GTK_BOX(hbox_bottom), selall_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(selall_button), "clicked", G_CALLBACK(select_all), NULL);
	
	remsel_button = gtk_button_new_with_label(_("Remove selected"));
        gtk_box_pack_start(GTK_BOX(hbox_bottom), remsel_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(remsel_button), "clicked", G_CALLBACK(remove_sel), NULL);


        sel_menu = gtk_menu_new();

        sel__none = gtk_menu_item_new_with_label(_("Select none"));
        gtk_menu_shell_append(GTK_MENU_SHELL(sel_menu), sel__none);
        g_signal_connect_swapped(G_OBJECT(sel__none), "activate", G_CALLBACK(sel__none_cb), NULL);
	gtk_widget_show(sel__none);

        sel__all = gtk_menu_item_new_with_label(_("Select all"));
        gtk_menu_shell_append(GTK_MENU_SHELL(sel_menu), sel__all);
        g_signal_connect_swapped(G_OBJECT(sel__all), "activate", G_CALLBACK(sel__all_cb), NULL);
	gtk_widget_show(sel__all);

        sel__inv = gtk_menu_item_new_with_label(_("Invert selection"));
        gtk_menu_shell_append(GTK_MENU_SHELL(sel_menu), sel__inv);
        g_signal_connect_swapped(G_OBJECT(sel__inv), "activate", G_CALLBACK(sel__inv_cb), NULL);
	gtk_widget_show(sel__inv);

        g_signal_connect_swapped(G_OBJECT(selall_button), "event", G_CALLBACK(sel_cb), NULL);


        rem_menu = gtk_menu_new();

        cut__sel = gtk_menu_item_new_with_label(_("Cut selected"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), cut__sel);
        g_signal_connect_swapped(G_OBJECT(cut__sel), "activate", G_CALLBACK(cut__sel_cb), NULL);
    	gtk_widget_show(cut__sel);

        rem__all = gtk_menu_item_new_with_label(_("Remove all"));
        gtk_menu_shell_append(GTK_MENU_SHELL(rem_menu), rem__all);
        g_signal_connect_swapped(G_OBJECT(rem__all), "activate", G_CALLBACK(rem__all_cb), NULL);
    	gtk_widget_show(rem__all);

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

	if (!playlist_is_embedded) {
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
	if (!playlist_is_embedded) {
		gtk_window_get_position(GTK_WINDOW(playlist_window), &playlist_pos_x, &playlist_pos_y);
		gtk_window_get_size(GTK_WINDOW(playlist_window), &playlist_size_x, &playlist_size_y);
	} else {
		playlist_size_x = playlist_window->allocation.width;
		playlist_size_y = playlist_window->allocation.height;
		gtk_window_get_size(GTK_WINDOW(main_window), &main_size_x, &main_size_y);
	}
        gtk_widget_hide(playlist_window);

	if (playlist_is_embedded) {
		gtk_window_resize(GTK_WINDOW(main_window), main_size_x, main_size_y - playlist_size_y - 6);
	}

}


void
save_playlist(char * filename) {

        int i = 0;
        GtkTreeIter iter;
	char * track_name;
	char * phys_name;
	char * color;
        xmlDocPtr doc;
        xmlNodePtr root;
        xmlNodePtr node;
	float voladj;
	float duration;
        char str[32];


        doc = xmlNewDoc((const xmlChar*) "1.0");
        root = xmlNewNode(NULL, (const xmlChar*) "aqualung_playlist");
        xmlDocSetRootElement(doc, root);

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i)) {

                gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
				   0, &track_name, 1, &phys_name, 2, &color,
				   3, &voladj, 5, &duration, -1);

                node = xmlNewTextChild(root, NULL, (const xmlChar*) "track", NULL);

                xmlNewTextChild(node, NULL, (const xmlChar*) "track_name", (const xmlChar*) track_name);
                xmlNewTextChild(node, NULL, (const xmlChar*) "phys_name", (const xmlChar*) phys_name);

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
                ++i;
        }
        xmlSaveFormatFile(filename, doc, 1);
}


void
parse_playlist_track(xmlDocPtr doc, xmlNodePtr cur, int sel_ok) {

        xmlChar * key;
	GtkTreeIter iter;
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
	
        cur = cur->xmlChildrenNode;
        while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"track_name"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(track_name, (char *) key, MAXLEN-1);
                        xmlFree(key);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"phys_name"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(phys_name, (char *) key, MAXLEN-1);
                        xmlFree(key);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"is_active"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				if ((xmlStrcmp(key, (const xmlChar *)"yes")) || (!sel_ok)) {
					strncpy(color, pl_color_inactive, 31);
				} else {
					strncpy(color, pl_color_active, 31);
				}
			}
                        xmlFree(key);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"voladj"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				voladj = convf((char *) key);
                        }
                        xmlFree(key);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"duration"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				duration = convf((char *) key);
                        }
                        xmlFree(key);
		}
		cur = cur->next;
	}

	if ((track_name[0] != '\0') && (phys_name[0] != '\0') && (color[0] != '\0')) {
		voladj2str(voladj, voladj_str);
		time2time(duration, duration_str);
		gtk_list_store_append(play_store, &iter);
		gtk_list_store_set(play_store, &iter, 0, track_name, 1, phys_name,
				   2, color, 3, voladj, 4, voladj_str,
				   5, duration, 6, duration_str, -1);
	} else {
		fprintf(stderr, "warning: parse_playlist_track: incomplete data in playlist XML\n");
	}
}


void
load_playlist(char * filename, int enqueue) {

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
		gtk_list_store_clear(play_store);

	if (get_playing_pos(play_store) == -1)
		sel_ok = 1;
		
        cur = cur->xmlChildrenNode;
        while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"track"))) {
                        parse_playlist_track(doc, cur, sel_ok);
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
	float voladj;
	char voladj_str[32];
	float duration;
	char duration_str[32];

	char artist_name[MAXLEN];
	char record_name[MAXLEN];
	char track_name[MAXLEN];
	char display_name[MAXLEN];
	metadata * meta = NULL;
	int use_meta;


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
		gtk_list_store_clear(play_store);

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

				duration = get_file_duration(g_locale_to_utf8(path, -1, NULL, NULL, NULL));
				time2time(duration, duration_str);

				if (rva_is_enabled) {
					meta = meta_new();
					if (meta_read(meta, g_locale_to_utf8(path, -1, NULL, NULL, NULL))) {
						if (!meta_get_rva(meta, &voladj)) {
							voladj = 0.0f;
						}
					} else {
						voladj = 0.0f;
					}
					meta_free(meta);
					meta = NULL;
				} else {
					voladj = 0.0f;
				}
				
				voladj2str(voladj, voladj_str);

				artist_name[0] = '\0';
                                record_name[0] = '\0';
                                track_name[0] = '\0';

                                if (auto_use_ext_meta_artist ||
                                    auto_use_ext_meta_record ||
                                    auto_use_ext_meta_track) {

                                        meta = meta_new();
                                        if (!meta_read(meta, g_locale_to_utf8(path, -1, NULL, NULL, NULL))) {
                                                meta_free(meta);
                                                meta = NULL;
                                        }
                                }

				use_meta = 0;
                                if ((meta != NULL) && auto_use_ext_meta_artist) {
                                        meta_get_artist(meta, artist_name);
					if (artist_name[0] != '\0') {
						use_meta = 1;
					}
                                }

                                if ((meta != NULL) && auto_use_ext_meta_record) {
                                        meta_get_record(meta, record_name);
					if (record_name[0] != '\0') {
						use_meta = 1;
					}
                                }

                                if ((meta != NULL) && auto_use_ext_meta_track) {
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

                                if (meta != NULL) {
                                        meta_free(meta);
                                        meta = NULL;
					if (use_meta) {
						make_title_string(display_name, title_format,
								  artist_name, record_name, track_name);
					} else {
						strcpy(display_name, g_locale_to_utf8(name, -1,
										      NULL, NULL, NULL));
					}
                                } else {
                                        strcpy(display_name, g_locale_to_utf8(name, -1, NULL, NULL, NULL));
                                }



				gtk_list_store_append(play_store, &iter);
				gtk_list_store_set(play_store, &iter,
						   0, display_name,
						   1, g_locale_to_utf8(path, -1, NULL, NULL, NULL),
						   2, pl_color_inactive,
						   3, voladj, 4, voladj_str,
						   5, duration, 6, duration_str, 7, PANGO_WEIGHT_NORMAL, -1);
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
	float voladj;
	char voladj_str[32];
	float duration;
	char duration_str[32];

        char artist_name[MAXLEN];
        char record_name[MAXLEN];
        char track_name[MAXLEN];
        char display_name[MAXLEN];
	metadata * meta = NULL;
	int use_meta;


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
		gtk_list_store_clear(play_store);

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

			duration = get_file_duration(g_locale_to_utf8(file, -1, NULL, NULL, NULL));
			time2time(duration, duration_str);

			if (rva_is_enabled) {
				meta = meta_new();
				if (meta_read(meta, g_locale_to_utf8(file, -1, NULL, NULL, NULL))) {
					if (!meta_get_rva(meta, &voladj)) {
						voladj = 0.0f;
					}
				} else {
					voladj = 0.0f;
				}
				meta_free(meta);
				meta = NULL;
			} else {
				voladj = 0.0f;
			}

			voladj2str(voladj, voladj_str);

			artist_name[0] = '\0';
			record_name[0] = '\0';
			track_name[0] = '\0';

			if (auto_use_ext_meta_artist ||
			    auto_use_ext_meta_record ||
			    auto_use_ext_meta_track) {

				meta = meta_new();
				if (!meta_read(meta, g_locale_to_utf8(file, -1, NULL, NULL, NULL))) {
					meta_free(meta);
					meta = NULL;
				}
			}

			use_meta = 0;
			if ((meta != NULL) && auto_use_ext_meta_artist) {
				meta_get_artist(meta, artist_name);
				if (artist_name[0] != '\0') {
					use_meta = 1;
				}
			}

			if ((meta != NULL) && auto_use_ext_meta_record) {
				meta_get_record(meta, record_name);
				if (record_name[0] != '\0') {
					use_meta = 1;
				}
			}

			if ((meta != NULL) && auto_use_ext_meta_track) {
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

			if (meta != NULL) {
				meta_free(meta);
				meta = NULL;
				if (use_meta) {
					make_title_string(display_name, title_format,
							  artist_name, record_name, track_name);
				} else {
					strcpy(display_name, g_locale_to_utf8(file, -1, NULL, NULL, NULL));
				}
			} else {
				strcpy(display_name, g_locale_to_utf8(file, -1, NULL, NULL, NULL));
			}


			gtk_list_store_append(play_store, &iter);
			gtk_list_store_set(play_store, &iter,
					   0, display_name,
					   1, g_locale_to_utf8(file, -1, NULL, NULL, NULL),
					   2, pl_color_inactive,
					   3, voladj, 4, voladj_str,
					   5, duration, 6, duration_str, -1);
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
	char * fullname_utf8;
	char * endname;
	char * home;
	char * path = filename;
	GtkTreeIter iter;
	float voladj;
	char voladj_str[32];
	float duration;
	char duration_str[32];

        char artist_name[MAXLEN];
        char record_name[MAXLEN];
        char track_name[MAXLEN];
        char display_name[MAXLEN];
	metadata * meta = NULL;
	int use_meta;


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
		snprintf(fullname, MAXLEN-1, "%s/%s", cwd, filename);
		break;
	}


        if (pl_color_active[0] == '\0')
                strcpy(pl_color_active, "#ffffff");
        if (pl_color_inactive[0] == '\0')
                strcpy(pl_color_inactive, "#000000");

	switch (is_playlist(fullname)) {

	case 0: /* not a playlist -- load the file itself */
		if (!enqueue)
			gtk_list_store_clear(play_store);

		if ((endname = strrchr(fullname, '/')) == NULL) {
			endname = fullname;
		} else {
			++endname;
		}


		fullname_utf8 = g_locale_to_utf8(fullname, -1, NULL, NULL, NULL);
		duration = get_file_duration(fullname_utf8);

		if (rva_is_enabled) {
			meta = meta_new();
			if (meta_read(meta, fullname_utf8)) {
				if (!meta_get_rva(meta, &voladj)) {
					voladj = 0.0f;
				}
			} else {
				voladj = 0.0f;
			}
			meta_free(meta);
			meta = NULL;
		} else {
			voladj = 0.0f;
		}

		voladj2str(voladj, voladj_str);


		artist_name[0] = '\0';
		record_name[0] = '\0';
		track_name[0] = '\0';

		if (auto_use_ext_meta_artist ||
		    auto_use_ext_meta_record ||
		    auto_use_ext_meta_track) {

			meta = meta_new();
			if (!meta_read(meta, fullname_utf8)) {
				meta_free(meta);
				meta = NULL;
			}
		}

		use_meta = 0;
		if ((meta != NULL) && auto_use_ext_meta_artist) {
			meta_get_artist(meta, artist_name);
			if (artist_name[0] != '\0') {
				use_meta = 1;
			}
		}

		if ((meta != NULL) && auto_use_ext_meta_record) {
			meta_get_record(meta, record_name);
			if (record_name[0] != '\0') {
				use_meta = 1;
			}
		}

		if ((meta != NULL) && auto_use_ext_meta_track) {
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

		if (meta != NULL) {
			meta_free(meta);
			meta = NULL;
			if (use_meta) {
				make_title_string(display_name, title_format,
						  artist_name, record_name, track_name);
			} else {
				strcpy(display_name, g_locale_to_utf8(endname, -1, NULL, NULL, NULL));
			}
		} else {
			strcpy(display_name, g_locale_to_utf8(endname, -1, NULL, NULL, NULL));
		}


		time2time(duration, duration_str);

                gtk_list_store_append(play_store, &iter);
                gtk_list_store_set(play_store, &iter,
				   0, display_name,
				   1, fullname_utf8,
				   2, pl_color_inactive,
				   3, voladj, 4, voladj_str, 5, duration, 6, duration_str, -1);

		g_free(fullname_utf8);

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

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

