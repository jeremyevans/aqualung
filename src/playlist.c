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
#include "file_decoder.h"
#include "meta_decoder.h"
#include "i18n.h"
#include "playlist.h"


extern char pl_color_active[14];
extern char pl_color_inactive[14];

extern char currdir[MAXLEN];
extern char cwd[MAXLEN];

int auto_save_playlist = 1;
int show_rva_in_playlist = 0;
int show_length_in_playlist = 1;
int plcol_idx[3] = { 0, 1, 2 };

GtkWidget * playlist_window;

extern GtkWidget * info_window;

int playlist_pos_x;
int playlist_pos_y;
int playlist_size_x;
int playlist_size_y;
int playlist_on;
int playlist_color_is_set;

extern int rva_is_enabled;
extern float rva_refvol;
extern float rva_steepness;

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

GtkWidget * play_list;
GtkListStore * play_store = 0;
GtkTreeSelection * play_select;
GtkTreeViewColumn * track_column;
GtkTreeViewColumn * rva_column;
GtkTreeViewColumn * length_column;
GtkCellRenderer * track_renderer;
GtkCellRenderer * rva_renderer;
GtkCellRenderer * length_renderer;


/* popup menus */
GtkWidget * sel_menu;
GtkWidget * sel__none;
GtkWidget * sel__all;
GtkWidget * sel__inv;
GtkWidget * rem_menu;
GtkWidget * rem__all;
GtkWidget * rem__sel;
GtkWidget * plist_menu;
GtkWidget * plist__save;
GtkWidget * plist__load;
GtkWidget * plist__enqueue;
GtkWidget * plist__separator;
GtkWidget * plist__fileinfo;

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


void rem__sel_cb(gpointer data);


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
				g_free(str);
			}

			if (strcmp(str, pl_color_inactive) == 0) {
				gtk_list_store_set(play_store, &iter, 2, inactive, -1);
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
	}
	
	gtk_tree_model_get_iter(GTK_TREE_MODEL(play_store), &iter, path);
	gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
	
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
playlist_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	GtkTreePath * path;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	char * pname;
	char * pfile;

	int i;

	switch (kevent->keyval) {
	case GDK_q:
	case GDK_Q:
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
	case GDK_Delete:
	case GDK_KP_Delete:
		i = 0;
		do {
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++);
		} while (!gtk_tree_selection_iter_is_selected(play_select, &iter));
		
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(play_store), &iter);

		rem__sel_cb(NULL);

		gtk_tree_selection_select_path(play_select, path);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(play_list), path, NULL, FALSE);

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

        GtkWidget * file_selector;
        const gchar * selected_filename;
	char filename[MAXLEN];
        char * c;

        file_selector = gtk_file_selection_new(_("Please specify the file to save the playlist to."));
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), currdir);
        gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), "playlist.xml");
        gtk_widget_show(file_selector);

        if (gtk_dialog_run(GTK_DIALOG(file_selector)) == GTK_RESPONSE_OK) {
                selected_filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector));

		strncpy(filename, selected_filename, MAXLEN-1);
		save_playlist(filename);
                strncpy(currdir, gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector)),
                                                                 MAXLEN-1);
                if (currdir[strlen(currdir)-1] != '/') {
                        c = strrchr(currdir, '/');
                        if (*(++c))
                                *c = '\0';
                }
        }
        gtk_widget_destroy(file_selector);
}


void
plist__load_cb(gpointer data) {

        GtkWidget * file_selector;
        const gchar * selected_filename;
	char filename[MAXLEN];
        char * c;

        file_selector = gtk_file_selection_new(_("Please specify the file to load the playlist from."));
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), currdir);
        gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));
        gtk_widget_show(file_selector);

        if (gtk_dialog_run(GTK_DIALOG(file_selector)) == GTK_RESPONSE_OK) {
                selected_filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector));

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
                strncpy(currdir, gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector)),
                                                                 MAXLEN-1);
                if (currdir[strlen(currdir)-1] != '/') {
                        c = strrchr(currdir, '/');
                        if (*(++c))
                                *c = '\0';
                }
        }
        gtk_widget_destroy(file_selector);
}


void
plist__enqueue_cb(gpointer data) {

        GtkWidget * file_selector;
        const gchar * selected_filename;
	char filename[MAXLEN];
        char * c;

        file_selector = gtk_file_selection_new("Please specify the file to load the playlist from.");
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), currdir);
        gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));
        gtk_widget_show(file_selector);

        if (gtk_dialog_run(GTK_DIALOG(file_selector)) == GTK_RESPONSE_OK) {
                selected_filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector));

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
                strncpy(currdir, gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector)),
                                                                 MAXLEN-1);
                if (currdir[strlen(currdir)-1] != '/') {
                        c = strrchr(currdir, '/');
                        if (*(++c))
                                *c = '\0';
                }
        }
        gtk_widget_destroy(file_selector);
}


void
plist__fileinfo_cb(gpointer data) {

	GtkTreeIter dummy;

	show_file_info(fileinfo_name, fileinfo_file, 0, NULL, dummy);
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


int
browse_direct_clicked(GtkWidget * widget, gpointer * data) {

        GtkWidget * file_selector;
        gchar ** selected_filenames;
        GtkListStore * model = (GtkListStore *)data;
        GtkTreeIter iter;
        int i;
	char * c;

        file_selector = gtk_file_selection_new(_("Please select the audio files for direct adding."));
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector), currdir);
        gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));
        gtk_file_selection_set_select_multiple(GTK_FILE_SELECTION(file_selector), TRUE);
        gtk_widget_show(file_selector);

        if (gtk_dialog_run(GTK_DIALOG(file_selector)) == GTK_RESPONSE_OK) {
                selected_filenames = gtk_file_selection_get_selections(GTK_FILE_SELECTION(file_selector));
                for (i = 0; selected_filenames[i] != NULL; i++) {
                        if (selected_filenames[i][strlen(selected_filenames[i])-1] != '/') {

                                gtk_list_store_append(model, &iter);
                                gtk_list_store_set(model, &iter, 0,
				        g_locale_to_utf8(selected_filenames[i], -1, NULL, NULL, NULL), -1);
                        }
                }
                g_strfreev(selected_filenames);

                strncpy(currdir, gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector)),
                                                                 MAXLEN-1);
                if (currdir[strlen(currdir)-1] != '/') {
                        c = strrchr(currdir, '/');
                        if (*(++c))
                                *c = '\0';
                }
        }
        gtk_widget_destroy(file_selector);

        return 0;
}


void
clicked_direct_list_header(GtkWidget * widget, gpointer * data) {

        GtkListStore * model = (GtkListStore *)data;

        gtk_list_store_clear(model);
}


void
direct_add(GtkWidget * widget, gpointer * data) {

        GtkWidget * dialog;
        GtkWidget * list_label;
        GtkWidget * viewport;
	GtkWidget * scrolled_win;
        GtkWidget * tracklist_tree;
        GtkListStore * model;
        GtkCellRenderer * cell;
        GtkTreeViewColumn * column;
        GtkTreeIter iter;
	GtkTreeIter play_iter;
        GtkWidget * browse_button;
        gchar * str;
	gchar * substr;
        int n, i;

	metadata * meta;


        dialog = gtk_dialog_new_with_buttons(_("Direct add"),
                                             GTK_WINDOW(playlist_window),
                                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                             NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 320, 300);
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

        list_label = gtk_label_new(_("\nDirectly add these files to playlist:"));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), list_label, FALSE, TRUE, 2);

        viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_win);

        /* setup track list */
        model = gtk_list_store_new(1, G_TYPE_STRING);
        tracklist_tree = gtk_tree_view_new();
        gtk_tree_view_set_model(GTK_TREE_VIEW(tracklist_tree), GTK_TREE_MODEL(model));
        gtk_container_add(GTK_CONTAINER(scrolled_win), tracklist_tree);
        gtk_widget_set_size_request(tracklist_tree, 250, 50);

        cell = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_("Clear list"), cell, "text", 0, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tracklist_tree), GTK_TREE_VIEW_COLUMN(column));
        gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tracklist_tree), TRUE);
        g_signal_connect(G_OBJECT(column->button), "clicked", G_CALLBACK(clicked_direct_list_header),
                         (gpointer *)model);
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), 0, GTK_SORT_ASCENDING);

        browse_button = gtk_button_new_with_label(_("Add files..."));
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked", G_CALLBACK(browse_direct_clicked),
                         (gpointer *)model);

        gtk_widget_show_all(dialog);


        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                n = 0;
                if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter)) {
                        /* count the number of list items */
                        n = 1;
                        while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter))
                                n++;
                }
                if (n) {
                        gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter);

                        for (i = 0; i < n; i++) {

				float voladj = 0.0f;
				char voladj_str[32];
				float duration = 0.0f;
				char duration_str[32];

                                gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &str, -1);
                                gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);

				if ((substr = strrchr(str, '/')) == NULL)
					substr = str;
				else
					++substr;


				if (rva_is_enabled) {
					meta = meta_new();
					if (meta_read(meta, str)) {
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

				duration = get_file_duration(str);
				time2time(duration, duration_str);

				gtk_list_store_append(play_store, &play_iter);
				gtk_list_store_set(play_store, &play_iter, 0, substr, 1, str,
						   2, pl_color_inactive,
						   3, voladj, 4, voladj_str,
						   5, duration, 6, duration_str, -1);

                                g_free(str);
                        }			
			delayed_playlist_rearrange(100);
                }
        }

        gtk_widget_destroy(dialog);
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
}


void
remove_sel(GtkWidget * widget, gpointer * data) {

	rem__sel_cb(NULL);
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

	gtk_timeout_add(delay, playlist_rearrange_timeout_cb, NULL);
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
	
        return TRUE;
}


void
create_playlist(void) {

	GtkWidget * vbox;

	GtkWidget * table;
	GtkObject * vadj;
	GtkWidget * vscroll;

	GtkWidget * hbox_bottom;
	GtkWidget * direct_button;
	GtkWidget * selall_button;
	GtkWidget * remsel_button;

	GtkWidget * viewport;
	int i;


        /* window creating stuff */
        playlist_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(playlist_window), _("Playlist"));
	gtk_window_set_gravity(GTK_WINDOW(playlist_window), GDK_GRAVITY_STATIC);
        g_signal_connect(G_OBJECT(playlist_window), "delete_event", G_CALLBACK(playlist_window_close), NULL);
        g_signal_connect(G_OBJECT(playlist_window), "key_press_event",
			 G_CALLBACK(playlist_window_key_pressed), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(playlist_window), 2);
        gtk_widget_set_size_request(playlist_window, 300, 200);

	plist_menu = gtk_menu_new();

	plist__save = gtk_menu_item_new_with_label(_("Save playlist"));
	plist__load = gtk_menu_item_new_with_label(_("Load playlist"));
	plist__enqueue = gtk_menu_item_new_with_label(_("Enqueue playlist"));
	plist__separator = gtk_separator_menu_item_new();
	plist__fileinfo = gtk_menu_item_new_with_label(_("File info..."));

	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__save);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__load);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__enqueue);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__separator);
	gtk_menu_shell_append(GTK_MENU_SHELL(plist_menu), plist__fileinfo);

	g_signal_connect_swapped(G_OBJECT(plist__save), "activate", G_CALLBACK(plist__save_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(plist__load), "activate", G_CALLBACK(plist__load_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(plist__enqueue), "activate", G_CALLBACK(plist__enqueue_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(plist__fileinfo), "activate", G_CALLBACK(plist__fileinfo_cb), NULL);

	gtk_widget_show(plist__save);
	gtk_widget_show(plist__load);
	gtk_widget_show(plist__enqueue);
	gtk_widget_show(plist__separator);
	gtk_widget_show(plist__fileinfo);

        vbox = gtk_vbox_new(FALSE, 2);
        gtk_container_add(GTK_CONTAINER(playlist_window), vbox);

        /* create playlist */
	if (!play_store) {
		play_store = gtk_list_store_new(7,
						G_TYPE_STRING,  /* track name */
						G_TYPE_STRING,  /* physical filename */
						G_TYPE_STRING,  /* color for selections */
						G_TYPE_FLOAT,   /* volume adjustment [dB] */
						G_TYPE_STRING,  /* volume adj. displayed */
						G_TYPE_FLOAT,   /* duration (in secs) */
						G_TYPE_STRING); /* duration displayed */
	}

        play_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(play_store));
	gtk_widget_set_name(play_list, "play_list");
        gtk_widget_set_size_request(play_list, 100, 100);
	playlist_color_is_set = 0;


	for (i = 0; i < 3; i++) {
		switch (plcol_idx[i]) {
		case 0:
			track_renderer = gtk_cell_renderer_text_new();
			track_column = gtk_tree_view_column_new_with_attributes("Tracks",
										track_renderer,
										"text", 0,
										"foreground", 2,
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

	g_signal_connect(G_OBJECT(play_list), "button_press_event",
			 G_CALLBACK(doubleclick_handler), NULL);
	g_signal_connect(G_OBJECT(play_list), "drag_data_received",
			 G_CALLBACK(playlist_drag_data_received), NULL);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), viewport, TRUE, TRUE, 0);

	table = gtk_table_new(1, 2, FALSE);
        gtk_container_add(GTK_CONTAINER(viewport), table);

        vadj = gtk_adjustment_new(0.0f, 0.0f, 100.0f, 1.0f, 10.0f, 0.0f);
        gtk_tree_view_set_vadjustment(GTK_TREE_VIEW(play_list), GTK_ADJUSTMENT(vadj));
        vscroll = gtk_vscrollbar_new(GTK_ADJUSTMENT(vadj));

	gtk_table_attach(GTK_TABLE(table), play_list, 0, 1, 0, 1,
                         GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
        gtk_table_attach(GTK_TABLE(table), vscroll, 1, 2, 0, 1,
                         GTK_FILL, GTK_FILL, 0, 0);



	/* bottom area of playlist window */
        hbox_bottom = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox_bottom, FALSE, TRUE, 0);

	direct_button = gtk_button_new_with_label(_("Direct add"));
        gtk_box_pack_start(GTK_BOX(hbox_bottom), direct_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(direct_button), "clicked", G_CALLBACK(direct_add), NULL);

	selall_button = gtk_button_new_with_label(_("Select all"));
        gtk_box_pack_start(GTK_BOX(hbox_bottom), selall_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(selall_button), "clicked", G_CALLBACK(select_all), NULL);
	
	remsel_button = gtk_button_new_with_label(_("Remove selected"));
        gtk_box_pack_start(GTK_BOX(hbox_bottom), remsel_button, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(remsel_button), "clicked", G_CALLBACK(remove_sel), NULL);
	
	

	/* create popup menus */
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

        gtk_widget_show_all(playlist_window);
	gtk_window_move(GTK_WINDOW(playlist_window), playlist_pos_x, playlist_pos_y);
	gtk_window_resize(GTK_WINDOW(playlist_window), playlist_size_x, playlist_size_y);

	if (!playlist_color_is_set) {
		set_playlist_color();
		playlist_color_is_set = 1;
	}
}


void
hide_playlist(void) {

	playlist_on = 0;
	gtk_window_get_position(GTK_WINDOW(playlist_window), &playlist_pos_x, &playlist_pos_y);
	gtk_window_get_size(GTK_WINDOW(playlist_window), &playlist_size_x, &playlist_size_y);
        gtk_widget_hide(playlist_window);
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


        doc = xmlNewDoc("1.0");
        root = xmlNewNode(NULL, "aqualung_playlist");
        xmlDocSetRootElement(doc, root);

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i)) {

                gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
				   0, &track_name, 1, &phys_name, 2, &color,
				   3, &voladj, 5, &duration, -1);

                node = xmlNewTextChild(root, NULL, "track", NULL);

                xmlNewTextChild(node, NULL, "track_name", track_name);
                xmlNewTextChild(node, NULL, "phys_name", phys_name);

		if (strcmp(color, pl_color_active) == 0) {
			strcpy(str, "yes");
		} else {
			strcpy(str, "no");
		}
                xmlNewTextChild(node, NULL, "is_active", str);

		snprintf(str, 31, "%f", voladj);
		xmlNewTextChild(node, NULL, "voladj", str);

		snprintf(str, 31, "%f", duration);
		xmlNewTextChild(node, NULL, "duration", str);

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
                                strncpy(track_name, key, MAXLEN-1);
                        xmlFree(key);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"phys_name"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(phys_name, key, MAXLEN-1);
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
				voladj = convf(key);
                        }
                        xmlFree(key);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"duration"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				duration = convf(key);
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

	metadata * meta;


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

				duration = get_file_duration(path);
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
				} else {
					voladj = 0.0f;
				}
				
				voladj2str(voladj, voladj_str);


				gtk_list_store_append(play_store, &iter);
				gtk_list_store_set(play_store, &iter,
						   0, g_locale_to_utf8(name, -1, NULL, NULL, NULL),
						   1, g_locale_to_utf8(path, -1, NULL, NULL, NULL),
						   2, pl_color_inactive,
						   3, voladj, 4, voladj_str,
						   5, duration, 6, duration_str, -1);
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

	metadata * meta;


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

			duration = get_file_duration(file);
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
			} else {
				voladj = 0.0f;
			}

			voladj2str(voladj, voladj_str);


			gtk_list_store_append(play_store, &iter);
			gtk_list_store_set(play_store, &iter,
					   0, g_locale_to_utf8(title, -1, NULL, NULL, NULL),
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

	metadata * meta = NULL;


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
		} else {
			voladj = 0.0f;
		}

		voladj2str(voladj, voladj_str);
		g_free(fullname_utf8);
		time2time(duration, duration_str);

                gtk_list_store_append(play_store, &iter);
                gtk_list_store_set(play_store, &iter,
				   0, g_locale_to_utf8(endname, -1, NULL, NULL, NULL),
				   1, g_locale_to_utf8(fullname, -1, NULL, NULL, NULL),
				   2, pl_color_inactive,
				   3, voladj, 4, voladj_str, 5, duration, 6, duration_str, -1);

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
}
