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
#include <unistd.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#ifdef _WIN32
#include <glib.h>
#else
#include <pthread.h>
#endif /* _WIN32 */

#ifdef HAVE_LADSPA
#include <lrdf.h>
#endif /* HAVE_LADSPA */

#ifdef HAVE_SRC
#include <samplerate.h>
#endif /* HAVE_SRC */

#include "common.h"
#include "utils.h"
#include "utils_gui.h"
#include "core.h"
#include "rb.h"
#include "cover.h"
#include "transceiver.h"
#include "decoder/file_decoder.h"
#include "about.h"
#include "options.h"
#include "skin.h"
#include "ports.h"
#include "music_browser.h"
#include "store_file.h"
#include "playlist.h"
#include "plugin.h"
#include "file_info.h"
#include "i18n.h"
#include "cdda.h"
#include "loop_bar.h"
#include "httpc.h"
#include "metadata.h"
#include "gui_main.h"
#include "version.h"

/* receive at most this much remote messages in one run of timeout_callback() */
#define MAX_RCV_COUNT 32

/* period of main timeout callback [ms] */
#define TIMEOUT_PERIOD 100

extern options_t options;

char pl_color_active[14];
char pl_color_inactive[14];

PangoFontDescription *fd_playlist;
PangoFontDescription *fd_browser;
PangoFontDescription *fd_bigtimer;
PangoFontDescription *fd_smalltimer;
PangoFontDescription *fd_songtitle;
PangoFontDescription *fd_songinfo;
PangoFontDescription *fd_statusbar;

/* Communication between gui thread and disk thread */
extern AQUALUNG_MUTEX_DECLARE(disk_thread_lock)
extern AQUALUNG_COND_DECLARE(disk_thread_wake)
extern rb_t * rb_gui2disk;
extern rb_t * rb_disk2gui;

#ifdef HAVE_JACK
extern jack_client_t * jack_client;
extern char * client_name;
extern int jack_is_shutdown;
#endif /* HAVE_JACK */

extern int aqualung_socket_fd;
extern int aqualung_session_id;

extern GtkTreeStore * music_store;
extern GtkListStore * running_store;

extern GtkWidget * plist_menu;
extern GtkWidget * playlist_notebook;

void init_plist_menu(GtkWidget *append_menu);

/* the physical name of the file that is playing, or a '\0'. */
char current_file[MAXLEN];

/* default window title */
char win_title[MAXLEN];

char send_cmd, recv_cmd;
char command[RB_CONTROL_SIZE];
fileinfo_t fileinfo;
status_t status;
unsigned long out_SR;
extern int output;
unsigned long rb_size;
unsigned long long total_samples;
unsigned long long sample_pos;

/* this flag set to 1 in core.c if --play
   for current instance is specified. */
int immediate_start = 0; 

/* the tab to load remote files */
char * tab_name;

/* volume & balance sliders */
double vol_prev = 0.0f;
double vol_lin = 1.0f;
double bal_prev = 0.0f;
extern double left_gain;
extern double right_gain;

/* label/display data */
fileinfo_t disp_info;
unsigned long disp_samples;
unsigned long disp_pos;

/* tooltips group */
GtkTooltips * aqualung_tooltips;

GtkWidget * main_window;
extern GtkWidget * browser_window;
extern GtkWidget * playlist_window;
extern GtkWidget * fxbuilder_window;
extern GtkWidget * ports_window;
extern GtkWidget * info_window;
extern GtkWidget * vol_window;
extern GtkWidget * build_prog_window;
extern GtkWidget * ripper_prog_window;
extern GtkWidget * browser_paned;

extern int music_store_changed;

extern int fxbuilder_on;

int skin_being_changed;

GtkObject * adj_pos;
GtkObject * adj_vol;
GtkObject * adj_bal;
GtkWidget * scale_pos;
GtkWidget * scale_vol;
GtkWidget * scale_bal;

GtkWidget * vbox_sep;

#ifdef HAVE_LOOP
GtkWidget * loop_bar;
#endif /* HAVE_LOOP */

GtkWidget * time_labels[3];
int refresh_time_label = 1;


gulong play_id;
gulong pause_id;

GtkWidget * play_button;
GtkWidget * pause_button;
GtkWidget * prev_button;
GtkWidget * stop_button;
GtkWidget * next_button;
GtkWidget * repeat_button;
GtkWidget * repeat_all_button;
GtkWidget * shuffle_button;

GtkWidget * label_title;
GtkWidget * label_format;
GtkWidget * label_samplerate;
GtkWidget * label_bps;
GtkWidget * label_mono;
GtkWidget * label_output;
GtkWidget * label_src_type;

int x_scroll_start;
int x_scroll_pos;
int scroll_btn;

GtkWidget * plugin_toggle;
GtkWidget * musicstore_toggle;
GtkWidget * playlist_toggle;

guint timeout_tag;
guint vol_bal_timeout_tag = 0;

gint timeout_callback(gpointer data);

/* whether we are refreshing the scale on STATUS commands recv'd from disk thread */
int refresh_scale = 1;
/* suppress scale refreshing after seeking (discard this much STATUS packets).
   Prevents position slider to momentarily jump back to original position. */
int refresh_scale_suppress = 0;

/* whether we allow seeks (depending on if we are at the end of the track) */
int allow_seeks = 1;

/* controls when to load the new file's data on display */
int fresh_new_file = 1;
int fresh_new_file_prev = 1;

/* whether we have a file loaded, that is currently playing (or paused) */
int is_file_loaded = 0;

/* whether playback is paused */
int is_paused = 0;


/* popup menu for configuration */
GtkWidget * conf_menu;
GtkWidget * conf__options;
GtkWidget * conf__skin;
GtkWidget * conf__jack;
GtkWidget * conf__fileinfo;
GtkWidget * conf__about;
GtkWidget * conf__quit;

GtkWidget * bigtimer_label;
GtkWidget * smalltimer_label_1;
GtkWidget * smalltimer_label_2;

/* systray stuff */
#ifdef HAVE_SYSTRAY

GtkStatusIcon * systray_icon;

GtkWidget * systray_menu;
GtkWidget * systray__show;
GtkWidget * systray__hide;
GtkWidget * systray__play;
GtkWidget * systray__pause;
GtkWidget * systray__stop;
GtkWidget * systray__prev;
GtkWidget * systray__next;
GtkWidget * systray__quit;

int warn_wm_not_systray_capable = 0;

void hide_all_windows(gpointer data);

#endif /* HAVE_SYSTRAY */

int systray_main_window_on = 1;

void create_main_window(char * skin_path);

void toggle_noeffect(int id, int state);

gint prev_event(GtkWidget * widget, GdkEvent * event, gpointer data);
gint play_event(GtkWidget * widget, GdkEvent * event, gpointer data);
gint pause_event(GtkWidget * widget, GdkEvent * event, gpointer data);
gint stop_event(GtkWidget * widget, GdkEvent * event, gpointer data);
gint next_event(GtkWidget * widget, GdkEvent * event, gpointer data);

void load_config(void);

void playlist_toggled(GtkWidget * widget, gpointer data);

GtkWidget * cover_align;
GtkWidget * c_event_box;
GtkWidget * cover_image_area;
gint cover_show_flag;

extern char fileinfo_name[MAXLEN];
extern char fileinfo_file[MAXLEN];

extern GtkWidget * plist__fileinfo;
extern GtkWidget * plist__rva;

extern gint playlist_state;
extern gint browser_state;


void
try_waking_disk_thread(void) {

	if (AQUALUNG_MUTEX_TRYLOCK(disk_thread_lock)) {
		AQUALUNG_COND_SIGNAL(disk_thread_wake)
		AQUALUNG_MUTEX_UNLOCK(disk_thread_lock)
	}
}


void
set_title_label(char * str) {

	gchar default_title[MAXLEN];
        char tmp[MAXLEN];

	if (is_file_loaded) {
		if (GTK_IS_LABEL(label_title)) {
			gtk_label_set_text(GTK_LABEL(label_title), str);
			if (options.show_sn_title) {
				strncpy(tmp, str, MAXLEN-1);
				strncat(tmp, " - ", MAXLEN-1);
				strncat(tmp, win_title, MAXLEN-1);
				gtk_window_set_title(GTK_WINDOW(main_window), tmp);
#ifdef HAVE_SYSTRAY
				gtk_status_icon_set_tooltip(systray_icon, tmp);
#endif /* HAVE_SYSTRAY */
			} else {
				gtk_window_set_title(GTK_WINDOW(main_window), win_title);
#ifdef HAVE_SYSTRAY
				gtk_status_icon_set_tooltip(systray_icon, win_title);
#endif /* HAVE_SYSTRAY */
			}
		}
	} else {
		if (GTK_IS_LABEL(label_title)) {
                        sprintf(default_title, "Aqualung %s", AQUALUNG_VERSION);
			gtk_label_set_text(GTK_LABEL(label_title), default_title);
			gtk_window_set_title(GTK_WINDOW(main_window), win_title);
#ifdef HAVE_SYSTRAY
			gtk_status_icon_set_tooltip(systray_icon, win_title);
#endif /* HAVE_SYSTRAY */
                }         
        }
}


void
set_format_label(char * format_str) {

	if (!is_file_loaded) {
		if (GTK_IS_LABEL(label_format))
			gtk_label_set_text(GTK_LABEL(label_format), "");
		return;
	}

	if (GTK_IS_LABEL(label_format))
		gtk_label_set_text(GTK_LABEL(label_format), format_str);
}


void
format_bps_label(int bps, int format_flags, char * str) {

	if (bps == 0) {
		strcpy(str, "N/A kbit/s");
		return;
	}

	if (format_flags & FORMAT_VBR) {
		sprintf(str, "%.1f kbit/s VBR", bps/1000.0);
	} else {
		if (format_flags & FORMAT_UBR) {
			sprintf(str, "%.1f kbit/s UBR", bps/1000.0);
		} else {
			sprintf(str, "%.1f kbit/s", bps/1000.0);
		}
	}
}

void
set_bps_label(int bps, int format_flags) {
	
	char str[MAXLEN];

	format_bps_label(bps, format_flags, str);

	if (is_file_loaded) {
		if (GTK_IS_LABEL(label_bps))
			gtk_label_set_text(GTK_LABEL(label_bps), str);
	} else {
		if (GTK_IS_LABEL(label_bps))
			gtk_label_set_text(GTK_LABEL(label_bps), "");
	}
}


void
set_samplerate_label(int sr) {
	
	char str[MAXLEN];

	sprintf(str, "%d Hz", sr);

	if (is_file_loaded) {
		if (GTK_IS_LABEL(label_samplerate))
			gtk_label_set_text(GTK_LABEL(label_samplerate), str);
	} else {
		if (GTK_IS_LABEL(label_samplerate))
			gtk_label_set_text(GTK_LABEL(label_samplerate), "");
	}
}


void
set_mono_label(int is_mono) {

	if (is_file_loaded) {
		if (is_mono) {
			if (GTK_IS_LABEL(label_mono))
				gtk_label_set_text(GTK_LABEL(label_mono), _("MONO"));
		} else {
			if (GTK_IS_LABEL(label_mono))
				gtk_label_set_text(GTK_LABEL(label_mono), _("STEREO"));
		}
	} else {
		if (GTK_IS_LABEL(label_mono))
			gtk_label_set_text(GTK_LABEL(label_mono), "");
	}
}



void
set_output_label(int output, int out_SR) {

	char str[MAXLEN];
	
	switch (output) {
#ifdef HAVE_OSS
	case OSS_DRIVER:
		sprintf(str, "%s OSS @ %d Hz", _("Output:"), out_SR);
		break;
#endif /* HAVE_OSS */
#ifdef HAVE_ALSA
	case ALSA_DRIVER:
		sprintf(str, "%s ALSA @ %d Hz", _("Output:"), out_SR);
		break;
#endif /* HAVE_ALSA */
#ifdef HAVE_JACK
	case JACK_DRIVER:
		sprintf(str, "%s JACK @ %d Hz", _("Output:"), out_SR);
		break;
#endif /* HAVE_JACK */
#ifdef _WIN32
	case WIN32_DRIVER:
		sprintf(str, "%s Win32 @ %d Hz", _("Output:"), out_SR);
		break;
#endif /* _WIN32 */
	default:
		strcpy(str, _("No output"));
		break;
	}
	
	if (GTK_IS_LABEL(label_output))
		gtk_label_set_text(GTK_LABEL(label_output), str);
}


void
set_src_type_label(int src_type) {
	
	char str[MAXLEN];
	
	strcpy(str, _("SRC Type: "));
#ifdef HAVE_SRC
	strcat(str, src_get_name(src_type));
#else
	strcat(str, _("None"));
#endif /* HAVE_SRC */

	if (GTK_IS_LABEL(label_src_type))
		gtk_label_set_text(GTK_LABEL(label_src_type), str);
}


void
refresh_time_displays(void) {

	char str[MAXLEN];

	if (is_file_loaded) {
		if (refresh_time_label || options.time_idx[0] != 0) {
			sample2time(disp_info.sample_rate, disp_pos, str, 0);
			if (GTK_IS_LABEL(time_labels[0])) 
				gtk_label_set_text(GTK_LABEL(time_labels[0]), str);
                        
		}

		if (refresh_time_label || options.time_idx[0] != 1) {
			if (disp_samples == 0) {
				strcpy(str, " N/A ");
			} else {
				sample2time(disp_info.sample_rate, disp_samples - disp_pos, str, 1);
			}
			if (GTK_IS_LABEL(time_labels[1])) 
				gtk_label_set_text(GTK_LABEL(time_labels[1]), str);
                        
		}
		
		if (refresh_time_label || options.time_idx[0] != 2) {
			if (disp_samples == 0) {
				strcpy(str, " N/A ");
			} else {
				sample2time(disp_info.sample_rate, disp_samples, str, 0);
			}
			if (GTK_IS_LABEL(time_labels[2])) 
				gtk_label_set_text(GTK_LABEL(time_labels[2]), str);
                        
		}
	} else {
		int i;
		for (i = 0; i < 3; i++) {
			if (GTK_IS_LABEL(time_labels[i]))
				gtk_label_set_text(GTK_LABEL(time_labels[i]), " 00:00 ");
		}
	}

}

void
refresh_displays(void) {

	GtkTreePath * p;
	GtkTreeIter iter;
	char * title_str;
	char * phys_str;
	playlist_t * pl;

	refresh_time_displays();

	set_format_label(disp_info.format_str);
	set_samplerate_label(disp_info.sample_rate);
	set_bps_label(disp_info.bps, disp_info.format_flags);
	set_mono_label(disp_info.is_mono);
	set_output_label(output, out_SR);
	set_src_type_label(options.src_type);

	if ((pl = playlist_get_playing()) == NULL) {
		if ((pl = playlist_get_current()) == NULL) {
			set_title_label("");
			cover_show_flag = 0;
			gtk_widget_hide(cover_image_area);
			gtk_widget_hide(c_event_box);
			gtk_widget_hide(cover_align);
			return;
		}
	}

	p = playlist_get_playing_path(pl);
	if (p != NULL) {
		int n = gtk_tree_path_get_depth(p);
		gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
		gtk_tree_path_free(p);
		
		if (n > 1) { /* track under album node */
			GtkTreeIter iter_parent;
			char artist[MAXLEN];
			char record[MAXLEN];
			char list_str[MAXLEN];
			
			gtk_tree_model_iter_parent(GTK_TREE_MODEL(pl->store), &iter_parent, &iter);
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter_parent,
					   PL_COL_PHYSICAL_FILENAME, &title_str,
					   -1);
			unpack_strings(title_str, artist, record);
			g_free(title_str);
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
					   PL_COL_TRACK_NAME, &title_str,
					   PL_COL_PHYSICAL_FILENAME, &phys_str,
					   -1);
			make_title_string(list_str, options.title_format, artist, record, title_str);
			if (!is_file_loaded || !httpc_is_url(phys_str)) {
				set_title_label(list_str);
			}
			g_free(title_str);
			g_free(phys_str);
		} else {
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
					   PL_COL_TRACK_NAME, &title_str,
					   PL_COL_PHYSICAL_FILENAME, &phys_str,
					   -1);
			if (!is_file_loaded || !httpc_is_url(phys_str)) {
				set_title_label(title_str);
			}
			g_free(title_str);
			g_free(phys_str);
		}
		if (is_file_loaded) {
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter,
					   PL_COL_PHYSICAL_FILENAME, &phys_str,
					   -1);
			display_cover(cover_image_area, c_event_box, cover_align,
				      48, 48, phys_str, TRUE, TRUE);
			g_free(phys_str);
		}
	} else if (!is_file_loaded) {
		set_title_label("");
		cover_show_flag = 0;
		gtk_widget_hide(cover_image_area);
		gtk_widget_hide(c_event_box);
		gtk_widget_hide(cover_align);
	}
}


void
zero_displays(void) {

	disp_info.total_samples = 0;
	disp_info.sample_rate = 0;
	disp_info.channels = 2;
	disp_info.is_mono = 0;
	disp_info.bps = 0;

	disp_samples = 0;
	disp_pos = 0;

	refresh_displays();
}


void
save_window_position(void) {

	gtk_window_get_position(GTK_WINDOW(main_window), &options.main_pos_x, &options.main_pos_y);

	if (!options.playlist_is_embedded && options.playlist_on) {
		gtk_window_get_position(GTK_WINDOW(playlist_window), &options.playlist_pos_x, &options.playlist_pos_y);
	}

	if (options.browser_on) {
		gtk_window_get_position(GTK_WINDOW(browser_window), &options.browser_pos_x, &options.browser_pos_y);
	}

	gtk_window_get_size(GTK_WINDOW(main_window), &options.main_size_x, &options.main_size_y);
	gtk_window_get_size(GTK_WINDOW(browser_window), &options.browser_size_x, &options.browser_size_y);

	if (!options.playlist_is_embedded) {
		gtk_window_get_size(GTK_WINDOW(playlist_window), &options.playlist_size_x, &options.playlist_size_y);
	} else {
		options.playlist_size_x = playlist_window->allocation.width;
		options.playlist_size_y = playlist_window->allocation.height;
	}

	if (!options.hide_comment_pane) {
		options.browser_paned_pos = gtk_paned_get_position(GTK_PANED(browser_paned));
	}
}


void
restore_window_position(void) {

	gtk_window_move(GTK_WINDOW(main_window), options.main_pos_x, options.main_pos_y);
	deflicker();
	gtk_window_move(GTK_WINDOW(browser_window), options.browser_pos_x, options.browser_pos_y);
	deflicker();
	if (!options.playlist_is_embedded) {
		gtk_window_move(GTK_WINDOW(playlist_window), options.playlist_pos_x, options.playlist_pos_y);
		deflicker();
	}
	
	gtk_window_resize(GTK_WINDOW(main_window), options.main_size_x, options.main_size_y);
	deflicker();
	gtk_window_resize(GTK_WINDOW(browser_window), options.browser_size_x, options.browser_size_y);
	deflicker();
	if (!options.playlist_is_embedded) {
		gtk_window_resize(GTK_WINDOW(playlist_window), options.playlist_size_x, options.playlist_size_y);
		deflicker();
	}

	if (!options.hide_comment_pane) {
		gtk_paned_set_position(GTK_PANED(browser_paned), options.browser_paned_pos);
		deflicker();
	}
}


void
change_skin(char * path) {

	int current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(playlist_notebook));

	int st_play = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button));
	int st_pause = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pause_button));
	int st_r_track = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_button));
	int st_r_all = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_all_button));
	int st_shuffle = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button));
	int st_fx = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(plugin_toggle));
	int st_mstore = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(musicstore_toggle));
	int st_plist = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(playlist_toggle));
	char title[MAXLEN];

#ifdef HAVE_LADSPA
	GtkTreeIter iter;
	gpointer gp_instance;
	int i = 0;
#endif /* HAVE_LADSPA */

	char rcpath[MAXLEN];


	skin_being_changed = 1;

	strncpy(title, gtk_label_get_text(GTK_LABEL(label_title)), MAXLEN-1);

	g_source_remove(timeout_tag);
#ifdef HAVE_CDDA
	cdda_timeout_stop();
#endif /* HAVE_CDDA */

	save_window_position();

	music_store_progress_bar_hide();
	playlist_progress_bar_hide_all();

	gtk_widget_destroy(main_window);
	main_window = NULL;

	if (!options.playlist_is_embedded) {
		gtk_widget_destroy(playlist_window);
		playlist_window = NULL;
	}

	gtk_widget_destroy(browser_window);
	browser_window = NULL;

	deflicker();

#ifdef HAVE_LADSPA
	gtk_widget_destroy(fxbuilder_window);
	deflicker();
#endif /* HAVE_LADSPA */
	
	sprintf(rcpath, "%s/rc", path);
	gtk_rc_parse(rcpath);

	create_main_window(path);
	deflicker();
	create_playlist();
	deflicker();
	create_music_browser();
	deflicker();
#ifdef HAVE_LADSPA
	create_fxbuilder();
	deflicker();
#endif /* HAVE_LADSPA */
	
	toggle_noeffect(PLAY, st_play);
	toggle_noeffect(PAUSE, st_pause);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_button), st_r_track);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_all_button), st_r_all);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shuffle_button), st_shuffle);

	if (options.playlist_is_embedded) {
		g_signal_handlers_block_by_func(G_OBJECT(playlist_toggle), playlist_toggled, NULL);
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle), st_plist);
	if (options.playlist_is_embedded) {
		g_signal_handlers_unblock_by_func(G_OBJECT(playlist_toggle), playlist_toggled, NULL);
	}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(musicstore_toggle), st_mstore);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plugin_toggle), st_fx);

	gtk_widget_show_all(main_window);

	refresh_displays();

	if (is_file_loaded) {
		set_title_label(title);
		display_cover(cover_image_area, c_event_box, cover_align,
			      48, 48, current_file, TRUE, TRUE);
	}

#ifdef HAVE_LOOP
	if (!st_r_track) {
		gtk_widget_hide(loop_bar);
	}
#endif /* HAVE_LOOP */

	deflicker();

	if (options.playlist_is_embedded) {
		if (!options.playlist_on) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle), FALSE);
			gtk_widget_hide(playlist_window);
			deflicker();
		}
	}

#ifdef HAVE_LADSPA
        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(running_store), &iter, NULL, i) &&
		i < MAX_PLUGINS) {

		gtk_tree_model_get(GTK_TREE_MODEL(running_store), &iter, 1, &gp_instance, -1);
		gtk_widget_reset_rc_styles(((plugin_instance *)gp_instance)->window);
		gtk_widget_queue_draw(((plugin_instance *)gp_instance)->window);
		deflicker();
		++i;
	}
#endif /* HAVE_LADSPA */

	if (info_window) {
		gtk_widget_reset_rc_styles(info_window);
		gtk_widget_queue_draw(info_window);
		gtk_window_present(GTK_WINDOW(info_window));
		deflicker();
	}

	if (vol_window) {
		gtk_widget_reset_rc_styles(vol_window);
		gtk_widget_queue_draw(vol_window);
		deflicker();
	}

	if (build_prog_window) {
		gtk_widget_reset_rc_styles(build_prog_window);
		gtk_widget_queue_draw(build_prog_window);
		deflicker();
	}

	if (ripper_prog_window) {
		gtk_widget_reset_rc_styles(ripper_prog_window);
		gtk_widget_queue_draw(ripper_prog_window);
		deflicker();
	}

#ifdef HAVE_SYSTRAY
	gtk_widget_reset_rc_styles(systray_menu);
	gtk_widget_queue_draw(systray_menu);
	deflicker();
#endif /* HAVE_SYSTRAY */

	restore_window_position();
	deflicker();

	playlist_set_color();
	
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_vol), options.vol);
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_bal), options.bal);

	timeout_tag = g_timeout_add(TIMEOUT_PERIOD, timeout_callback, NULL);
#ifdef HAVE_CDDA
	cdda_timeout_start();
#endif /* HAVE_CDDA */

	skin_being_changed = 0;

        playlist_size_allocate_all();
	playlist_content_changed(playlist_get_current());
	playlist_selection_changed(playlist_get_current());

        gtk_notebook_set_current_page(GTK_NOTEBOOK(playlist_notebook), current_page);
        show_active_position_in_playlist(playlist_get_current());
}

gboolean
main_window_close(GtkWidget * widget, GdkEvent * event, gpointer data) {

	send_cmd = CMD_FINISH;
	rb_write(rb_gui2disk, &send_cmd, 1);
	try_waking_disk_thread();

#ifdef HAVE_CDDA
	cdda_shutdown();
#endif /* HAVE_CDDA */

	if (systray_main_window_on) {
		save_window_position();
	}

	save_config();

#ifdef HAVE_LADSPA
	save_plugin_data();
	lrdf_cleanup();
#endif /* HAVE_LADSPA */

#ifdef HAVE_SYSTRAY
        gtk_status_icon_set_visible(GTK_STATUS_ICON(systray_icon), FALSE);
#endif /* HAVE_SYSTRAY */

	if (options.auto_save_playlist) {
		char playlist_name[MAXLEN];

		snprintf(playlist_name, MAXLEN-1, "%s/%s", options.confdir, "playlist.xml");
		playlist_save_all(playlist_name);
	}

        pango_font_description_free(fd_playlist);
        pango_font_description_free(fd_browser);

	gtk_main_quit();

	return FALSE;
}

void
main_window_closing(void) {

	if (music_store_changed) {
		GtkWidget * dialog;
		int resp;

                dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, 
                                                _("One or more stores in Music Store have been modified.\n"
                                                "Do you want to save them before exiting?"));

		gtk_dialog_add_buttons(GTK_DIALOG(dialog),
				       _("Save"), GTK_RESPONSE_YES,
				       _("Discard changes"), GTK_RESPONSE_NO,
				       _("Do not exit"), GTK_RESPONSE_CANCEL,
				       NULL);

		gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
		gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
		gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
                gtk_window_set_title(GTK_WINDOW(dialog), _("Quit"));
		
		resp = aqualung_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		if (resp == GTK_RESPONSE_CANCEL) {
			return;
		}

		if (resp == GTK_RESPONSE_YES) {
			save_all_music_store();
		}
	}

	main_window_close(NULL, NULL, NULL);
}

gboolean
main_window_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

#ifndef HAVE_SYSTRAY
	if (event->type == GDK_DELETE) {
		main_window_closing();
		return TRUE;
	}
#endif /* !HAVE_SYSTRAY */

	return FALSE;
}

void
main_window_resize(void) {

        if (options.playlist_is_embedded && !options.playlist_on) {
		gtk_window_resize(GTK_WINDOW(main_window), options.main_size_x,
				  options.main_size_y - 14 - 6 - 6 -
				  playlist_window->allocation.height);
	}

	if (!options.playlist_is_embedded) {
		gtk_window_resize(GTK_WINDOW(main_window),
				  options.main_size_x, options.main_size_y - 14 - 6);
	}
}

/***********************************************************************************/


void
conf__options_cb(gpointer data) {

	create_options_window();
}


void
conf__skin_cb(gpointer data) {

	create_skin_window();
}


void
conf__jack_cb(gpointer data) {

#ifdef HAVE_JACK
	port_setup_dialog();
#endif /* HAVE_JACK */
}


void
conf__fileinfo_cb(gpointer data) {

	if (is_file_loaded) {

		GtkTreeIter dummy;
		const char * name = gtk_label_get_text(GTK_LABEL(label_title));
	
		show_file_info((char *)name, current_file, 0, NULL, dummy);
	}
}


void
conf__about_cb(gpointer data) {

	create_about_window();
}


void
conf__quit_cb(gpointer data) {

	main_window_closing();
}


gint
vol_bal_timeout_callback(gpointer data) {

	refresh_time_label = 1;
	vol_bal_timeout_tag = 0;
	refresh_time_displays();

	return FALSE;
}


void
musicstore_toggled(GtkWidget * widget, gpointer data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		show_music_browser();
	} else {
		hide_music_browser();
	}
}


void
playlist_toggled(GtkWidget * widget, gpointer data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		show_playlist();
	} else {
		hide_playlist();
	}
}


void
plugin_toggled(GtkWidget * widget, gpointer data) {

#ifdef HAVE_LADSPA
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		show_fxbuilder();
	} else {
		hide_fxbuilder();
	}
#else
	/* XXX */
	printf("Aqualung compiled without LADSPA plugin support.\n");
#endif /* HAVE_LADSPA */
}


gint
main_window_key_pressed(GtkWidget * widget, GdkEventKey * event) {

        int playlist_tabs = gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook));

	switch (event->keyval) {	
	case GDK_KP_Divide:
		refresh_time_label = 0;
		if (vol_bal_timeout_tag) {
			g_source_remove(vol_bal_timeout_tag);
		}
			
		vol_bal_timeout_tag = g_timeout_add(1000, vol_bal_timeout_callback, NULL);

                if (event->state & GDK_MOD1_MASK) {  /* ALT + KP_Divide */
			g_signal_emit_by_name(G_OBJECT(scale_bal), "move-slider",
					      GTK_SCROLL_STEP_BACKWARD, NULL);
		} else {
			g_signal_emit_by_name(G_OBJECT(scale_vol), "move-slider",
					      GTK_SCROLL_STEP_BACKWARD, NULL);
		}
		return TRUE;
	case GDK_KP_Multiply:
		refresh_time_label = 0;
		if (vol_bal_timeout_tag) {
			g_source_remove(vol_bal_timeout_tag);
		}
		
		vol_bal_timeout_tag = g_timeout_add(1000, vol_bal_timeout_callback, NULL);

                if (event->state & GDK_MOD1_MASK) {  /* ALT + KP_Multiply */
			g_signal_emit_by_name(G_OBJECT(scale_bal),
					      "move-slider", GTK_SCROLL_STEP_FORWARD, NULL);
		} else {
			g_signal_emit_by_name(G_OBJECT(scale_vol),
					      "move-slider", GTK_SCROLL_STEP_FORWARD, NULL);
		}
		return TRUE;
	case GDK_Right:
		if (is_file_loaded && allow_seeks && total_samples != 0) {
			refresh_scale = 0;
			g_signal_emit_by_name(G_OBJECT(scale_pos), "move-slider",
					      GTK_SCROLL_STEP_FORWARD, NULL);
		}
		return TRUE;
	case GDK_Left:
		if (is_file_loaded && allow_seeks && total_samples != 0) {
			refresh_scale = 0;
			g_signal_emit_by_name(G_OBJECT(scale_pos), "move-slider",
					      GTK_SCROLL_STEP_BACKWARD, NULL);
		}
		return TRUE;
	case GDK_b:
	case GDK_B:
	case GDK_period:
		next_event(NULL, NULL, NULL);
		return TRUE;
	case GDK_z:
	case GDK_Z:
	case GDK_y:
	case GDK_Y:
	case GDK_comma:
		prev_event(NULL, NULL, NULL);
		return TRUE;
	case GDK_s:
	case GDK_S:
                if (event->state & GDK_MOD1_MASK) {  /* ALT + s */
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(musicstore_toggle),
						     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(musicstore_toggle)));
		} else {
			stop_event(NULL, NULL, NULL);
		}
		return TRUE;
	case GDK_v:
	case GDK_V:
		if (!(event->state & GDK_CONTROL_MASK)) {
			stop_event(NULL, NULL, NULL);
			return TRUE;
		}
		break;
	case GDK_c:
	case GDK_C:
	case GDK_space:
		if (!(event->state & GDK_CONTROL_MASK)) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button),
						     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pause_button)));
			return TRUE;
		}
		break;
	case GDK_p:
	case GDK_P:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
					     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button)));
		return TRUE;
	case GDK_i:
	case GDK_I:
		if (!options.playlist_is_embedded) {
                        conf__fileinfo_cb(NULL);
        		return TRUE;
                }
                break;
	case GDK_BackSpace:
		if (allow_seeks && total_samples != 0) {
			seek_t seek;
			
			send_cmd = CMD_SEEKTO;
			seek.seek_to_pos = 0.0f;
			rb_write(rb_gui2disk, &send_cmd, 1);
			rb_write(rb_gui2disk, (char *)&seek, sizeof(seek_t));
			try_waking_disk_thread();
			refresh_scale_suppress = 2;
		}
		
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pause_button)))
			gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_pos), 0.0f);
		
		return TRUE;
	case GDK_l:
	case GDK_L:
                if (event->state & GDK_MOD1_MASK) {  /* ALT + l */
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle),
						     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(playlist_toggle)));
		}
		return TRUE;
	case GDK_x:
	case GDK_X:
		if (event->state & GDK_CONTROL_MASK) {
			break;
		}
                if (event->state & GDK_MOD1_MASK) {  /* ALT + x */
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plugin_toggle),
						     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(plugin_toggle)));
		} else {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
						     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button)));
		}
		return TRUE;
	case GDK_q:
	case GDK_Q:
                if (event->state & GDK_CONTROL_MASK) {  /* CTRL + q */
			main_window_closing();
		}
		return TRUE;
	case GDK_k:
	case GDK_K:
        	create_skin_window();
                return TRUE;
	case GDK_o:
	case GDK_O:
                create_options_window();
                return TRUE;
	case GDK_1:
                if (event->state & GDK_MOD1_MASK) {  /* ALT + 1 */
                        if(playlist_tabs >= 1) {
                                gtk_notebook_set_current_page(GTK_NOTEBOOK(playlist_notebook), 1-1);
                        }
                } else {
                        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_button))) {
                                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_button), FALSE);
                        } else {
                                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_button), TRUE);
                        }
                }
                return TRUE;
	case GDK_2:
                if (event->state & GDK_MOD1_MASK) {  /* ALT + 2 */
                        if(playlist_tabs >= 2) {
                                gtk_notebook_set_current_page(GTK_NOTEBOOK(playlist_notebook), 2-1);
                        }
                } else {
                        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_all_button))) {
                                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_all_button), FALSE);
                        } else {
                                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_all_button), TRUE);
                        }
                }
                return TRUE;
	case GDK_3:
                if (event->state & GDK_MOD1_MASK) {  /* ALT + 3 */
                        if(playlist_tabs >= 3) {
                                gtk_notebook_set_current_page(GTK_NOTEBOOK(playlist_notebook), 3-1);
                        }
                } else {
                        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
                                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shuffle_button), FALSE);
                        } else {
                                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shuffle_button), TRUE);
                        }
                }
                return TRUE;
	case GDK_4:
	case GDK_5:
	case GDK_6:
	case GDK_7:
	case GDK_8:
	case GDK_9:
	case GDK_0:
                if (event->state & GDK_MOD1_MASK) {  /* ALT */

                        int val = event->keyval - GDK_0;
                        val = (val == 0) ? 10 : val;

                        if(playlist_tabs >= val) {
                                gtk_notebook_set_current_page(GTK_NOTEBOOK(playlist_notebook), val-1);
                        }
                }
                return TRUE;
#ifdef HAVE_SYSTRAY
	case GDK_Escape:
		hide_all_windows(NULL);
		return TRUE;
#endif /* HAVE_SYSTRAY */
	}
	

        if (options.playlist_is_embedded) {
		playlist_window_key_pressed(widget, event);
        }

	return FALSE;
}


gint
main_window_key_released(GtkWidget * widget, GdkEventKey * event) {

	switch (event->keyval) {
        case GDK_Right:
        case GDK_Left:
                if (is_file_loaded && allow_seeks && refresh_scale == 0 && total_samples != 0) {
                        seek_t seek;

                        refresh_scale = 1;
                        send_cmd = CMD_SEEKTO;
                        seek.seek_to_pos = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_pos))
                                / 100.0f * total_samples;
                        rb_write(rb_gui2disk, &send_cmd, 1);
                        rb_write(rb_gui2disk, (char *)&seek, sizeof(seek_t));
			try_waking_disk_thread();
			refresh_scale_suppress = 2;
                }
                break;
	}

	return FALSE;
}


gint
main_window_focus_out(GtkWidget * widget, GdkEventFocus * event, gpointer data) {

        refresh_scale = 1;

        return FALSE;
}


gint
main_window_state_changed(GtkWidget * widget, GdkEventWindowState * event, gpointer data) {

        if (!options.united_minimization)
		return FALSE;
	
	if (event->new_window_state == GDK_WINDOW_STATE_ICONIFIED) {
		if (options.browser_on) {
			gtk_window_iconify(GTK_WINDOW(browser_window));
		}
		
		if (!options.playlist_is_embedded && options.playlist_on) {
			gtk_window_iconify(GTK_WINDOW(playlist_window));
		}
		
		if (vol_window) {
			gtk_window_iconify(GTK_WINDOW(vol_window));
		}

		if (info_window) {
			gtk_window_iconify(GTK_WINDOW(info_window));
		}
		
#ifdef HAVE_LADSPA
		if (fxbuilder_on) {
			GtkTreeIter iter;
			gpointer gp_instance;
			int i = 0;
			
			gtk_window_iconify(GTK_WINDOW(fxbuilder_window));
			
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(running_store), &iter, NULL, i) &&
			       i < MAX_PLUGINS) {
				gtk_tree_model_get(GTK_TREE_MODEL(running_store), &iter, 1, &gp_instance, -1);
				gtk_widget_hide(((plugin_instance *)gp_instance)->window);
				++i;
			}
		}
#endif /* HAVE_LADSPA */
	}
	
	if (event->new_window_state == 0) {
		if (options.browser_on) {
			gtk_window_deiconify(GTK_WINDOW(browser_window));
		}
		
		if (!options.playlist_is_embedded && options.playlist_on) {
			gtk_window_deiconify(GTK_WINDOW(playlist_window));
		}

		if (vol_window) {
			gtk_window_deiconify(GTK_WINDOW(vol_window));
		}

		if (info_window) {
			gtk_window_deiconify(GTK_WINDOW(info_window));
		}
		
		if (fxbuilder_on) {
			gtk_window_deiconify(GTK_WINDOW(fxbuilder_window));
		}
	}
        return FALSE;
}


gint
main_window_button_pressed(GtkWidget * widget, GdkEventButton * event) {

	if (event->button == 3) {

		GtkWidget * fileinfo;

		if (options.playlist_is_embedded) {
			fileinfo = plist__fileinfo;
		} else {
			fileinfo = conf__fileinfo;
		}

		if (is_file_loaded && (current_file[0] != '\0')) {

			const char * name = gtk_label_get_text(GTK_LABEL(label_title));

			strncpy(fileinfo_name, name, MAXLEN-1);
			strncpy(fileinfo_file, current_file, MAXLEN-1);

			gtk_widget_set_sensitive(fileinfo, TRUE);
		} else {
			gtk_widget_set_sensitive(fileinfo, FALSE);
		}

                gtk_menu_popup(GTK_MENU(conf_menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);
	}

	return TRUE;
}


static gint
scale_button_press_event(GtkWidget * widget, GdkEventButton * event) {

	if (event->button == 3)
		return FALSE;

	if (!is_file_loaded)
		return FALSE;

	if (!allow_seeks)
		return FALSE;

	if (total_samples == 0)
		return FALSE;

	refresh_scale = 0;
	return FALSE;
}


static gint
scale_button_release_event(GtkWidget * widget, GdkEventButton * event) {

	seek_t seek;

	if (is_file_loaded) {
		
		if (!allow_seeks)
			return FALSE;

		if (total_samples == 0)
			return FALSE;
		
		if (refresh_scale == 0) {
			refresh_scale = 1;

			send_cmd = CMD_SEEKTO;
			seek.seek_to_pos = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_pos))
				/ 100.0f * total_samples;
			rb_write(rb_gui2disk, &send_cmd, 1);
			rb_write(rb_gui2disk, (char *)&seek, sizeof(seek_t));
			try_waking_disk_thread();
			refresh_scale_suppress = 2;
		}
	}

	return FALSE;
}


#ifdef HAVE_LOOP

void
loop_range_changed_cb(AqualungLoopBar * bar, float start, float end, gpointer data) {

	options.loop_range_start = start;
	options.loop_range_end = end;
}

#endif /* HAVE_LOOP */

void
changed_pos(GtkAdjustment * adj, gpointer data) {

        char str[16];

	if (!is_file_loaded)
		gtk_adjustment_set_value(adj, 0.0f);

        if (options.enable_tooltips) {
                sprintf(str, _("Position: %d%%"), (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(adj))); 
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), scale_pos, str, NULL);
        }
}


gint
scale_vol_button_press_event(GtkWidget * widget, GdkEventButton * event) {

	char str[10];
	options.vol = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_vol));

        if (event->state & GDK_SHIFT_MASK) {  /* SHIFT */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_vol), 0);
		return TRUE;
	}

	if (options.vol < -40.5f) {
		sprintf(str, _("Mute"));
	} else {
		sprintf(str, _("%d dB"), (int)options.vol);
	}

	gtk_label_set_text(GTK_LABEL(time_labels[options.time_idx[0]]), str);
	
	refresh_time_label = 0;

	if (event->button == 3) {
		return TRUE;
	} else {
		return FALSE;
	}
}


void
changed_vol(GtkAdjustment * adj, gpointer date) {

	char str[10], str2[32];

	options.vol = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_vol));
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_vol), options.vol);

        if (options.vol < -40.5f) {
                sprintf(str, _("Mute"));
        } else {
                sprintf(str, _("%d dB"), (int)options.vol);
        }

        if (!refresh_time_label) {
		gtk_label_set_text(GTK_LABEL(time_labels[options.time_idx[0]]), str);
        }

        sprintf(str2, _("Volume: %s"), str);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), scale_vol, str2, NULL);
}


gint
scale_vol_button_release_event(GtkWidget * widget, GdkEventButton * event) {

	refresh_time_label = 1;
	refresh_time_displays();

	return FALSE;
}


gint
scale_bal_button_press_event(GtkWidget * widget, GdkEventButton * event) {

	char str[10];
	options.bal = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_bal));

        if (event->state & GDK_SHIFT_MASK) {  /* SHIFT */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_bal), 0);
		return TRUE;
	}
	
	if (options.bal != 0.0f) {
		if (options.bal > 0.0f) {
			sprintf(str, _("%d%% R"), (int)options.bal);
		} else {
			sprintf(str, _("%d%% L"), -1*(int)options.bal);
		}
	} else {
		sprintf(str, _("C"));
	}

	gtk_label_set_text(GTK_LABEL(time_labels[options.time_idx[0]]), str);
	
	refresh_time_label = 0;

	if (event->button == 3) {
		return TRUE;
	} else {
		return FALSE;
	}
}


void
changed_bal(GtkAdjustment * adj, gpointer date) {

	char str[10], str2[32];

	options.bal = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_bal));
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_bal), options.bal);

        if (options.bal != 0.0f) {
                if (options.bal > 0.0f) {
                        sprintf(str, _("%d%% R"), (int)options.bal);
                } else {
                        sprintf(str, _("%d%% L"), -1*(int)options.bal);
                }
        } else {
                sprintf(str, _("C"));
        }
	
        if (!refresh_time_label) {
		gtk_label_set_text(GTK_LABEL(time_labels[options.time_idx[0]]), str);
	}

        sprintf(str2, _("Balance: %s"), str);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), scale_bal, str2, NULL);
}


gint
scale_bal_button_release_event(GtkWidget * widget, GdkEventButton * event) {

	refresh_time_label = 1;
	refresh_time_displays();

	return FALSE;
}


void
show_scale_pos(gboolean state) {

        if (state == FALSE) {
                gtk_widget_hide(GTK_WIDGET(scale_pos));
                gtk_widget_show(GTK_WIDGET(vbox_sep));
                main_window_resize();
        } else {
                gtk_widget_show(GTK_WIDGET(scale_pos));
                gtk_widget_hide(GTK_WIDGET(vbox_sep));
        }
}


/******** Cue functions *********/

void
toggle_noeffect(int id, int state) {
	switch (id) {
	case PLAY:
		g_signal_handler_block(G_OBJECT(play_button), play_id);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), state);
		g_signal_handler_unblock(G_OBJECT(play_button), play_id);
		break;
	case PAUSE:
		g_signal_handler_block(G_OBJECT(pause_button), pause_id);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button), state);
		g_signal_handler_unblock(G_OBJECT(pause_button), pause_id);
		break;
	default:
		printf("error in gui_main.c/toggle_noeffect(): unknown id value %d\n", id);
		break;
	}
}


void
cue_track_for_playback(GtkTreeStore * store, GtkTreeIter * piter, cue_t * cue) {

	char * str;
	float voladj;

	gtk_tree_model_get(GTK_TREE_MODEL(store), piter, 1, &str, 3, &voladj, -1);
	cue->filename = strdup(str);
	cue->voladj = options.rva_is_enabled ? voladj : 0.0f;
	strncpy(current_file, str, MAXLEN-1);
	g_free(str);
}


/* retcode for choose_*_track(): 1->success, 0->empty list */
int
choose_first_track(GtkTreeStore * store, GtkTreeIter * piter) {

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), piter)) {
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), piter)) {
			GtkTreeIter iter_parent = *piter;
			gtk_tree_model_iter_children(GTK_TREE_MODEL(store), piter, &iter_parent);
		}
		return 1;
	}
	return 0;
}


/* get first or last child iter */
void
get_child_iter(GtkTreeStore * store, GtkTreeIter * piter, int first) {
	
	GtkTreeIter iter;
	if (first) {
		gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &iter, piter);
	} else {
		int n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), piter);
		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, piter, n-1);
	}
	*piter = iter;
}


int
choose_prev_track(GtkTreeStore * store, GtkTreeIter * piter) {

	GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(store), piter);

 try_again_prev:
	if (gtk_tree_path_prev(p)) {
		if (gtk_tree_path_get_depth(p) == 1) { /* toplevel */
			GtkTreeIter iter;
			gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, p);
			if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter)) {
				get_child_iter(store, &iter, 0/* last */);
			}
			*piter = iter;
		} else {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(store), piter, p);
		}
		gtk_tree_path_free(p);
		return 1;
	} else {
		if (gtk_tree_path_get_depth(p) == 1) { /* toplevel */
			GtkTreeIter iter;
			int n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);
			if (n) {
				gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, n-1);
				if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter)) {
					get_child_iter(store, &iter, 0/* last */);
				}
				*piter = iter;
				gtk_tree_path_free(p);
				return 1;
			} else {
				gtk_tree_path_free(p);
				return 0;
			}
		} else {
			gtk_tree_path_up(p);
			goto try_again_prev;
		}
	}
}


int
choose_next_track(GtkTreeStore * store, GtkTreeIter * piter) {

 try_again_next:
	if (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), piter)) {
		GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(store), piter);
		if (gtk_tree_path_get_depth(p) == 1) { /* toplevel */
			if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), piter)) {
				get_child_iter(store, piter, 1/* first */);
			}
		}
		gtk_tree_path_free(p);
		return 1;
	} else {
		GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(store), piter);
		if (gtk_tree_path_get_depth(p) == 1) { /* toplevel */
			int n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);
			if (n) {
				gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), piter);
				if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), piter)) {
					get_child_iter(store, piter, 1/* first */);
				}
				gtk_tree_path_free(p);
				return 1;
			} else {
				gtk_tree_path_free(p);
				return 0;
			}
		} else {
			gtk_tree_path_up(p);
			gtk_tree_model_get_iter(GTK_TREE_MODEL(store), piter, p);
			gtk_tree_path_free(p);
			goto try_again_next;
		}		
	}
}


/* simpler case than choose_next_track(); no support for wrap-around at end of list.
 * used by the timeout callback for track flow-through */
int
choose_adjacent_track(GtkTreeStore * store, GtkTreeIter * piter) {

 try_again_adjacent:
	if (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), piter)) {
		GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(store), piter);
		if (gtk_tree_path_get_depth(p) == 1) { /* toplevel */
			if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), piter)) {
				get_child_iter(store, piter, 1/* first */);
			}
		}
		gtk_tree_path_free(p);
		return 1;
	} else {
		GtkTreePath * p = gtk_tree_model_get_path(GTK_TREE_MODEL(store), piter);
		if (gtk_tree_path_get_depth(p) == 1) { /* toplevel */
			gtk_tree_path_free(p);
			return 0;
		} else {
			gtk_tree_path_up(p);
			gtk_tree_model_get_iter(GTK_TREE_MODEL(store), piter, p);
			gtk_tree_path_free(p);
			goto try_again_adjacent;
		}
	}
}


/* also used to pick the track numbered n_stop in the flattened tree */
long
count_playlist_tracks(GtkTreeStore * store, GtkTreeIter * piter, long n_stop) {

	GtkTreeIter iter;
	long i = 0;
	long n = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, i++)) {
		long c = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), &iter);
		long d = c;
		if (!c) ++c;
		if (n_stop > -1) {
			if (n_stop > c) {
				n_stop -= c;
			} else {
				if (d) {
					gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store),
								      piter, &iter, n_stop-1);
				} else {
					*piter = iter;
				}
				return 0;
			}
		}
		n += c;
	}
	return n;
}


int
random_toplevel_item(GtkTreeStore * store, GtkTreeIter * piter) {

	long n_items;
	long n;

	n_items = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);
	if (!n_items) {
		return 0;
	}

	n = (double)rand() * n_items / RAND_MAX;
	if (n == n_items)
		--n;
	
	gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), piter, NULL, n);
	return 1;
}


int
random_first_track(GtkTreeStore * store, GtkTreeIter * piter) {

	if (random_toplevel_item(store, piter)) {
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), piter)) {
			get_child_iter(store, piter, 1/* first */);
		}
		return 1;
	} else {
		return 0;
	}
}


int
choose_random_track(GtkTreeStore * store, GtkTreeIter * piter) {

	long n_items;
	long n;

	if (options.album_shuffle_mode) {
		if (gtk_tree_store_iter_is_valid(store, piter)) {
			int d = gtk_tree_store_iter_depth(store, piter);
			if (d) {
				if (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), piter)) {
					return 1;
				}
			}
		}
		return random_first_track(store, piter);
	} else {
		n_items = count_playlist_tracks(store, NULL, -1);
		if (n_items) {
			n = (double)rand() * n_items / RAND_MAX;
			if (n == n_items) {
				--n;
			}
			count_playlist_tracks(store, piter, n+1);
			return 1;
		}
		return 0;
	}
}


void
prepare_playback(GtkTreeStore * store, GtkTreeIter * piter, cue_t * pcue) {

	mark_track(store, piter);
	cue_track_for_playback(store, piter, pcue);
	is_file_loaded = 1;
	toggle_noeffect(PLAY, TRUE);
}


void
unprepare_playback(void) {
	
	is_file_loaded = 0;
	current_file[0] = '\0';
	zero_displays();
	toggle_noeffect(PLAY, FALSE);
}


gint
prev_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

	GtkTreeIter iter;
	GtkTreePath * p;
	char cmd;
	cue_t cue;
	playlist_t * pl;

	if (!allow_seeks)
		return FALSE;

	if (is_file_loaded) {
		pl = playlist_get_playing();
	} else {
		pl = playlist_get_current();
	}

	if (pl == NULL) {
		return FALSE;
	}

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
		/* normal or repeat mode */
		p = playlist_get_playing_path(pl);
		if (p != NULL) {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_path_free(p);
			unmark_track(pl->store, &iter);
			if (choose_prev_track(pl->store, &iter)) {
				mark_track(pl->store, &iter);
			}
		} else {
			if (choose_first_track(pl->store, &iter)) {
				mark_track(pl->store, &iter);
			}
		}
	} else {
		/* shuffle mode */
		p = playlist_get_playing_path(pl);
		if (p != NULL) {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_path_free(p);
			unmark_track(pl->store, &iter);
		}
		if (choose_random_track(pl->store, &iter)) {
			mark_track(pl->store, &iter);
		}
	}

	if (is_file_loaded) {
		if ((p = playlist_get_playing_path(pl)) == NULL) {
			if (is_paused) {
				is_paused = 0;
				toggle_noeffect(PAUSE, FALSE);
				stop_event(NULL, NULL, NULL);
			}
			return FALSE;
		}

		gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
		gtk_tree_path_free(p);
		cue_track_for_playback(pl->store, &iter, &cue);

		if (is_paused) {
			is_paused = 0;
			toggle_noeffect(PAUSE, FALSE);
			toggle_noeffect(PLAY, TRUE);
		}

		cmd = CMD_CUE;
		rb_write(rb_gui2disk, &cmd, sizeof(char));
		rb_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
		try_waking_disk_thread();
	}
	return FALSE;
}


gint
next_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

	GtkTreeIter iter;
	GtkTreePath * p;
	char cmd;
	cue_t cue;
	playlist_t * pl;

	if (!allow_seeks)
		return FALSE;

	if (is_file_loaded) {
		pl = playlist_get_playing();
	} else {
		pl = playlist_get_current();
	}

	if (pl == NULL) {
		return FALSE;
	}

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
		/* normal or repeat mode */
		p = playlist_get_playing_path(pl);
		if (p != NULL) {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_path_free(p);
			unmark_track(pl->store, &iter);
			if (choose_next_track(pl->store, &iter)) {
				mark_track(pl->store, &iter);
			}
		} else {
			if (choose_first_track(pl->store, &iter)) {
				mark_track(pl->store, &iter);
			}
		}
	} else {
		/* shuffle mode */
		p = playlist_get_playing_path(pl);
		if (p != NULL) {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_path_free(p);
			unmark_track(pl->store, &iter);
		}
		if (choose_random_track(pl->store, &iter)) {
			mark_track(pl->store, &iter);
		}
	}

	if (is_file_loaded) {
		if ((p = playlist_get_playing_path(pl)) == NULL) {
			if (is_paused) {
				is_paused = 0;
				toggle_noeffect(PAUSE, FALSE);
				stop_event(NULL, NULL, NULL);
			}
			return FALSE;
		}

		gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
		gtk_tree_path_free(p);
		cue_track_for_playback(pl->store, &iter, &cue);

		if (is_paused) {
			is_paused = 0;
			toggle_noeffect(PAUSE, FALSE);
			toggle_noeffect(PLAY, TRUE);
		}

		cmd = CMD_CUE;
		rb_write(rb_gui2disk, &cmd, sizeof(char));
		rb_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
		try_waking_disk_thread();
	}
	return FALSE;
}


gint
play_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

	GtkTreeIter iter;
	GtkTreePath * p;
	char cmd;
	cue_t cue;
	playlist_t * pl;

	if (is_paused) {
		is_paused = 0;
		toggle_noeffect(PAUSE, FALSE);
		send_cmd = CMD_RESUME;
		rb_write(rb_gui2disk, &send_cmd, 1);
		try_waking_disk_thread();
		return FALSE;
	}

	cmd = CMD_CUE;
	cue.filename = NULL;
	
	while ((pl = playlist_get_playing()) != NULL) {
		playlist_set_playing(pl, 0);
	}

	if ((pl = playlist_get_current()) == NULL) {
		return FALSE;
	}

	playlist_set_playing(pl, 1);

	p = playlist_get_playing_path(pl);
	if (p != NULL) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
		gtk_tree_path_free(p);
		prepare_playback(pl->store, &iter, &cue);
	} else {
		if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
			/* normal or repeat mode */
			if (choose_first_track(pl->store, &iter)) {
				prepare_playback(pl->store, &iter, &cue);
			} else {
				unprepare_playback();
			}
		} else { /* shuffle mode */
			if (choose_random_track(pl->store, &iter)) {
				prepare_playback(pl->store, &iter, &cue);
			} else {
				unprepare_playback();
			}
		}
	}
	if (cue.filename == NULL) {
		stop_event(NULL, NULL, NULL);
	} else {
		rb_write(rb_gui2disk, &cmd, sizeof(char));
		rb_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
		try_waking_disk_thread();
	}
	return FALSE;
}


gint
pause_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

	if ((!allow_seeks) || (!is_file_loaded)) {
		toggle_noeffect(PAUSE, FALSE);
		return FALSE;
	}

	if (!is_paused) {
		is_paused = 1;
		toggle_noeffect(PLAY, FALSE);
		send_cmd = CMD_PAUSE;
		rb_write(rb_gui2disk, &send_cmd, 1);

	} else {
		is_paused = 0;
		toggle_noeffect(PLAY, TRUE);
		send_cmd = CMD_RESUME;
		rb_write(rb_gui2disk, &send_cmd, 1);
	}

	try_waking_disk_thread();
	return FALSE;
}


gint
stop_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

	char cmd;
	cue_t cue;

	is_file_loaded = 0;
	current_file[0] = '\0';
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_pos), 0.0f);
	zero_displays();
	toggle_noeffect(PLAY, FALSE);
	toggle_noeffect(PAUSE, FALSE);

	is_paused = 0;
	allow_seeks = 1;

	cmd = CMD_CUE;
	cue.filename = NULL;
        rb_write(rb_gui2disk, &cmd, sizeof(char));
        rb_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
	try_waking_disk_thread();

 	show_scale_pos(TRUE);

        /* hide cover */
        cover_show_flag = 0;
        gtk_widget_hide(cover_image_area);
        gtk_widget_hide(c_event_box);
	gtk_widget_hide(cover_align);
			
	return FALSE;
}


/* called when a track ends without user intervention */
void
decide_next_track(cue_t * pcue) {

	GtkTreePath * p;
	GtkTreeIter iter;
	playlist_t * pl;

	if ((pl = playlist_get_playing()) == NULL) {
		unprepare_playback();
		return;
	}

	p = playlist_get_playing_path(pl);
	if (p != NULL) { /* there is a marked track in playlist */
		if ((!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) &&
		    (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_button)))) {
			/* normal or list repeat mode */
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_path_free(p);
			unmark_track(pl->store, &iter);
			if (choose_adjacent_track(pl->store, &iter)) {
				prepare_playback(pl->store, &iter, pcue);
			} else {
				if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_all_button))) {
					/* normal mode */
					allow_seeks = 1;
					changed_pos(GTK_ADJUSTMENT(adj_pos), NULL);
					unprepare_playback();
				} else {
					/* list repeat mode */
					if (choose_first_track(pl->store, &iter)) {
						prepare_playback(pl->store, &iter, pcue);
					} else {
						allow_seeks = 1;
						changed_pos(GTK_ADJUSTMENT(adj_pos), NULL);
						unprepare_playback();
					}
				}
			}
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_button))) {
			/* track repeat mode */
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_path_free(p);
			prepare_playback(pl->store, &iter, pcue);
		} else {
			/* shuffle mode */
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_path_free(p);
			unmark_track(pl->store, &iter);
			if (choose_random_track(pl->store, &iter)) {
				prepare_playback(pl->store, &iter, pcue);
			} else {
				unprepare_playback();
			}
		}
	} else { /* no marked track in playlist */
		if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
			/* normal or repeat mode */
			if (choose_first_track(pl->store, &iter)) {
				prepare_playback(pl->store, &iter, pcue);
			} else {
				allow_seeks = 1;
				changed_pos(GTK_ADJUSTMENT(adj_pos), NULL);
				unprepare_playback();
			}
		} else { /* shuffle mode */
			if (choose_random_track(pl->store, &iter)) {
				prepare_playback(pl->store, &iter, pcue);
			} else {
				unprepare_playback();
			}
		}
	}
}


/********************************************/

void
swap_labels(int a, int b) {

	GtkWidget * tmp;
	int t;

	tmp = time_labels[options.time_idx[a]];
	time_labels[options.time_idx[a]] = time_labels[options.time_idx[b]] ;
	time_labels[options.time_idx[b]] = tmp;

	t = options.time_idx[b];
	options.time_idx[b] = options.time_idx[a];
	options.time_idx[a] = t;
}


gint
time_label0_clicked(GtkWidget * widget, GdkEventButton * event) {

	switch (event->button) {
	case 1:
		swap_labels(0, 1);
		refresh_time_displays();
		break;
	case 3:
		swap_labels(0, 2);
		refresh_time_displays();
		break;
	}

	return TRUE;
}


gint
time_label1_clicked(GtkWidget * widget, GdkEventButton * event) {

	switch (event->button) {
	case 1:
		swap_labels(0, 1);
		refresh_time_displays();
		break;
	case 3:
		swap_labels(1, 2);
		refresh_time_displays();
		break;
	}

	return TRUE;
}


gint
time_label2_clicked(GtkWidget * widget, GdkEventButton * event) {

	switch (event->button) {
	case 1:
		swap_labels(1, 2);
		refresh_time_displays();
		break;
	case 3:
		swap_labels(0, 2);
		refresh_time_displays();
		break;
	}

	return TRUE;
}


gint
scroll_btn_pressed(GtkWidget * widget, GdkEventButton * event) {

	if (event->button != 1)
		return FALSE;

	x_scroll_start = event->x;
	x_scroll_pos = event->x;
	scroll_btn = event->button;

	return TRUE;
}

gint
scroll_btn_released(GtkWidget * widget, GdkEventButton * event, gpointer * win) {

	scroll_btn = 0;
	gdk_window_set_cursor(gtk_widget_get_parent_window(GTK_WIDGET(win)), NULL);

	return TRUE;
}

gint
scroll_motion_notify(GtkWidget * widget, GdkEventMotion * event, gpointer * win) {

	int dx = event->x - x_scroll_start;
	gboolean dummy;

	if (scroll_btn != 1)
		return FALSE;

	if (!scroll_btn)
		return TRUE;

	if (dx < -10) {
#if (GTK_CHECK_VERSION(2,8,0))
		GtkRange * range = GTK_RANGE(gtk_scrolled_window_get_hscrollbar(GTK_SCROLLED_WINDOW(win)));
		g_signal_emit_by_name(G_OBJECT(range), "move-slider", GTK_SCROLL_STEP_RIGHT, &dummy);
#else
		g_signal_emit_by_name(G_OBJECT(win), "scroll-child", GTK_SCROLL_STEP_FORWARD, &dummy);
#endif /* GTK_CHECK_VERSION */
		x_scroll_start = event->x;
		gdk_window_set_cursor(gtk_widget_get_parent_window(GTK_WIDGET(win)),
				      gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW));
	}

	if (dx > 10) {
#if (GTK_CHECK_VERSION(2,8,0))
		GtkRange * range = GTK_RANGE(gtk_scrolled_window_get_hscrollbar(GTK_SCROLLED_WINDOW(win)));
		g_signal_emit_by_name(G_OBJECT(range), "move-slider", GTK_SCROLL_STEP_LEFT, &dummy);
#else
		g_signal_emit_by_name(G_OBJECT(win), "scroll-child", GTK_SCROLL_STEP_BACKWARD, &dummy);			
#endif /* GTK_CHECK_VERSION */
		x_scroll_start = event->x;
		gdk_window_set_cursor(gtk_widget_get_parent_window(GTK_WIDGET(win)),
				      gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW));
	}

	x_scroll_pos = event->x;

	return TRUE;
}

#ifdef HAVE_LOOP
void
hide_loop_bar() {
	gtk_widget_hide(loop_bar);
        main_window_resize();
}
#endif /* HAVE_LOOP */

void
repeat_toggled(GtkWidget * widget, gpointer data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_button))) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_all_button), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shuffle_button), FALSE);
		options.repeat_on = 1;
	} else {
		options.repeat_on = 0;
	}

#ifdef HAVE_LOOP
	if (options.repeat_on) {
		gtk_widget_show(loop_bar);
	} else {
		hide_loop_bar();
	}
#endif /* HAVE_LOOP */
}


void
repeat_all_toggled(GtkWidget * widget, gpointer data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_all_button))) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_button), FALSE);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shuffle_button), FALSE);
		options.repeat_all_on = 1;
	} else {
		options.repeat_all_on = 0;
	}
}


void
shuffle_toggled(GtkWidget * widget, gpointer data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_button), FALSE);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_all_button), FALSE);
		options.shuffle_on = 1;
	} else {
		options.shuffle_on = 0;
	}
}


GtkWidget *
create_button_with_image(char * path, int toggle, char * alt) {

        GtkWidget * button;
        GdkPixbuf * pixbuf;
        GtkWidget * image;

        gchar tmp_path[MAXLEN];

        sprintf(tmp_path, "%s.xpm", path);

        pixbuf = gdk_pixbuf_new_from_file(tmp_path, NULL);

        if (!pixbuf) {
                sprintf(tmp_path, "%s.png", path);
                pixbuf = gdk_pixbuf_new_from_file(tmp_path, NULL);
        }

	if (pixbuf) {
		image = gtk_image_new_from_pixbuf(pixbuf);
		
		if (toggle) {
			button = gtk_toggle_button_new();
		} else {
			button = gtk_button_new();
		}
		gtk_container_add(GTK_CONTAINER(button), image);
	} else {
		if (toggle) {
			button = gtk_toggle_button_new_with_label(alt);
		} else {
			button = gtk_button_new_with_label(alt);
		}
	}

        return button;
}


#ifdef HAVE_JACK
void
jack_shutdown_window(void) {

	GtkWidget * window;
	GtkWidget * label;

        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window), _("JACK connection lost"));
        gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	label = gtk_label_new(_("JACK has either been shutdown or it\n\
disconnected Aqualung because it was\n\
not fast enough. All you can do now\n\
is restart both JACK and Aqualung.\n"));
        gtk_container_add(GTK_CONTAINER(window), label);
	gtk_widget_show_all(window);
}
#endif /* HAVE_JACK */


void
set_win_title(void) {

	char str_session_id[32];

	strcpy(win_title, "Aqualung");
	if (aqualung_session_id > 0) {
		sprintf(str_session_id, ".%d", aqualung_session_id);
		strcat(win_title, str_session_id);
	}
#ifdef HAVE_JACK
	if ((output == JACK_DRIVER) && (strcmp(client_name, "aqualung") != 0)) {
		strcat(win_title, " [");
		strcat(win_title, client_name);
		strcat(win_title, "]");
	}
#endif /* HAVE_JACK */
}


#ifdef HAVE_SYSTRAY
void
hide_all_windows(gpointer data) {
	/* Inverse operation of show_all_windows().
	 * note that hiding/showing multiple windows has to be done in a
	 * stack-like order, eg. hide main window last, show it first. */

	if (gtk_status_icon_is_embedded(systray_icon) == FALSE) {

		if (!warn_wm_not_systray_capable) {
			message_dialog(_("Warning"),
				       options.playlist_is_embedded ? main_window : playlist_window,
				       GTK_MESSAGE_WARNING,
				       GTK_BUTTONS_OK,
				       NULL,
				       _("Aqualung is compiled with system tray support, but "
					 "the status icon could not be embedded in the notification "
					 "area. Your desktop may not have support for a system tray, "
					 "or it has not been configured correctly."));
			warn_wm_not_systray_capable = 1;
		}

		return;
	}

	if (!systray_main_window_on) {
		return;
	}

	systray_main_window_on = 0;

	save_window_position();

	if (ripper_prog_window) {
		gtk_widget_hide(ripper_prog_window);
	}

	if (build_prog_window) {
		gtk_widget_hide(build_prog_window);
	}

	if (vol_window) {
		gtk_widget_hide(vol_window);
	}

	if (info_window) {
		gtk_widget_hide(info_window);
	}

	if (!options.playlist_is_embedded && options.playlist_on) {
		gtk_widget_hide(playlist_window);
	}

	if (options.browser_on) {
		gtk_widget_hide(browser_window);
	}

	if (fxbuilder_on) {
		gtk_widget_hide(fxbuilder_window);
	}

	gtk_widget_hide(main_window);

	gtk_widget_hide(systray__hide);
	gtk_widget_show(systray__show);
}

void
show_all_windows(gpointer data) {

	gtk_widget_show(main_window);
	systray_main_window_on = 1;
	deflicker();

	if (!options.playlist_is_embedded && options.playlist_on) {
		gtk_widget_show(playlist_window);
		deflicker();
	}

	if (options.browser_on) {
		gtk_widget_show(browser_window);
		deflicker();
	}

	if (fxbuilder_on) {
		gtk_widget_show(fxbuilder_window);
		deflicker();
	}

	if (info_window) {
		gtk_widget_show(info_window);
		deflicker();
	}

	if (vol_window) {
		gtk_widget_show(vol_window);
		deflicker();
	}

	if (build_prog_window) {
		gtk_widget_show(build_prog_window);
		deflicker();
	}

	if (ripper_prog_window) {
		gtk_widget_show(ripper_prog_window);
		deflicker();
	}

	restore_window_position();

	gtk_widget_hide(systray__show);
	gtk_widget_show(systray__hide);
}
#endif /* HAVE_SYSTRAY */

gboolean    
cover_press_button_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data) {

        GtkTreePath * p;
	GtkTreeIter iter;
	char * title_str;
	playlist_t * pl;

	if ((pl = playlist_get_playing()) == NULL) {
		return FALSE;
	}

	if (event->type == GDK_BUTTON_PRESS && event->button == 1) { /* LMB ? */
		p = playlist_get_playing_path(pl);
		if (p != NULL) {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_path_free(p);
			if (is_file_loaded) {
				gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_PHYSICAL_FILENAME, &title_str, -1);
				display_zoomed_cover(main_window, c_event_box, title_str);
				g_free(title_str);
			}
		}
        }
        return TRUE;
}    

void
create_main_window(char * skin_path) {

	GtkWidget * vbox;
	GtkWidget * disp_hbox;
	GtkWidget * btns_hbox;
	GtkWidget * title_hbox;
	GtkWidget * info_hbox;
	GtkWidget * vb_table;

	GtkWidget * conf__separator1;
	GtkWidget * conf__separator2;

	GtkWidget * time_table;
	GtkWidget * time0_viewp;
	GtkWidget * time1_viewp;
	GtkWidget * time2_viewp;
	GtkWidget * time_hbox1;
	GtkWidget * time_hbox2;

	GtkWidget * disp_vbox;
	GtkWidget * title_viewp;
	GtkWidget * title_scrolledwin;
	GtkWidget * info_viewp;
	GtkWidget * info_scrolledwin;
	GtkWidget * info_vsep;

	GtkWidget * sr_table;

	char path[MAXLEN];

	set_win_title();
	main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_name(main_window, "main_window");
	gtk_window_set_title(GTK_WINDOW(main_window), win_title);
#ifdef HAVE_SYSTRAY
	gtk_status_icon_set_tooltip(systray_icon, win_title);
#endif /* HAVE_SYSTRAY */

        g_signal_connect(G_OBJECT(main_window), "event", G_CALLBACK(main_window_event), NULL);

#ifdef HAVE_SYSTRAY
        g_signal_connect(G_OBJECT(main_window), "delete_event", G_CALLBACK(hide_all_windows), NULL);
#else
        g_signal_connect(G_OBJECT(main_window), "delete_event", G_CALLBACK(main_window_close), NULL);
#endif /* HAVE_SYSTRAY */

        g_signal_connect(G_OBJECT(main_window), "key_press_event", G_CALLBACK(main_window_key_pressed), NULL);
        g_signal_connect(G_OBJECT(main_window), "key_release_event",
			 G_CALLBACK(main_window_key_released), NULL);
        g_signal_connect(G_OBJECT(main_window), "button_press_event",
			 G_CALLBACK(main_window_button_pressed), NULL);
        g_signal_connect(G_OBJECT(main_window), "focus_out_event",
                         G_CALLBACK(main_window_focus_out), NULL);
        g_signal_connect(G_OBJECT(main_window), "window-state-event",
                         G_CALLBACK(main_window_state_changed), NULL);
	
	gtk_widget_set_events(main_window, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
	gtk_container_set_border_width(GTK_CONTAINER(main_window), 5);

        /* always on top ? */

	if (options.main_window_always_on_top) {
                gtk_window_set_keep_above (GTK_WINDOW(main_window), TRUE);
        }

        /* initialize fonts */

	fd_playlist = pango_font_description_from_string(options.playlist_font);
 	fd_browser = pango_font_description_from_string(options.browser_font);
 	fd_bigtimer = pango_font_description_from_string(options.bigtimer_font);
 	fd_smalltimer = pango_font_description_from_string(options.smalltimer_font);
 	fd_songtitle = pango_font_description_from_string(options.songtitle_font);
 	fd_songinfo = pango_font_description_from_string(options.songinfo_font);
 	fd_statusbar = pango_font_description_from_string(options.statusbar_font);

        aqualung_tooltips = gtk_tooltips_new();

        conf_menu = gtk_menu_new();

        if (options.playlist_is_embedded) {
                init_plist_menu(conf_menu);
                plist_menu = conf_menu;
        }

        conf__options = gtk_menu_item_new_with_label(_("Settings"));
        conf__skin = gtk_menu_item_new_with_label(_("Skin chooser"));
        conf__jack = gtk_menu_item_new_with_label(_("JACK port setup"));
        if (!options.playlist_is_embedded) {
                conf__fileinfo = gtk_menu_item_new_with_label(_("File info"));
	}
        conf__separator1 = gtk_separator_menu_item_new();
        conf__about = gtk_menu_item_new_with_label(_("About"));
        conf__separator2 = gtk_separator_menu_item_new();
        conf__quit = gtk_menu_item_new_with_label(_("Quit"));

        if (options.playlist_is_embedded) {
                gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__separator1);
	}
        gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__options);
        gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__skin);

#ifdef HAVE_JACK
        if (output == JACK_DRIVER) {
                gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__jack);
        }
#endif /* HAVE_JACK */

        if (!options.playlist_is_embedded) {
                gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__fileinfo);
                gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__separator1);
        }
        gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__about);
	gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__separator2);
        gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__quit);

        g_signal_connect_swapped(G_OBJECT(conf__options), "activate", G_CALLBACK(conf__options_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(conf__skin), "activate", G_CALLBACK(conf__skin_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(conf__jack), "activate", G_CALLBACK(conf__jack_cb), NULL);

        g_signal_connect_swapped(G_OBJECT(conf__about), "activate", G_CALLBACK(conf__about_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(conf__quit), "activate", G_CALLBACK(conf__quit_cb), NULL);

        if (!options.playlist_is_embedded) {
                g_signal_connect_swapped(G_OBJECT(conf__fileinfo), "activate", G_CALLBACK(conf__fileinfo_cb), NULL);
                gtk_widget_set_sensitive(conf__fileinfo, FALSE);
                gtk_widget_show(conf__fileinfo);
        }

        gtk_widget_show(conf__options);
        gtk_widget_show(conf__skin);
        gtk_widget_show(conf__jack);
        gtk_widget_show(conf__separator1);
        gtk_widget_show(conf__about);
        gtk_widget_show(conf__separator2);
        gtk_widget_show(conf__quit);


	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(main_window), vbox);


	disp_hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), disp_hbox, FALSE, FALSE, 0);


	time_table = gtk_table_new(2, 2, FALSE);
	disp_vbox = gtk_vbox_new(FALSE, 0);

	gtk_box_pack_start(GTK_BOX(disp_hbox), time_table, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(disp_hbox), disp_vbox, TRUE, TRUE, 0);

	time0_viewp = gtk_viewport_new(NULL, NULL);
        time1_viewp = gtk_viewport_new(NULL, NULL);
	time2_viewp = gtk_viewport_new(NULL, NULL);

	gtk_widget_set_name(time0_viewp, "time_viewport");
	gtk_widget_set_name(time1_viewp, "time_viewport");
	gtk_widget_set_name(time2_viewp, "time_viewport");

	g_signal_connect(G_OBJECT(time0_viewp), "button_press_event", G_CALLBACK(time_label0_clicked), NULL);
	g_signal_connect(G_OBJECT(time1_viewp), "button_press_event", G_CALLBACK(time_label1_clicked), NULL);
	g_signal_connect(G_OBJECT(time2_viewp), "button_press_event", G_CALLBACK(time_label2_clicked), NULL);


	gtk_table_attach(GTK_TABLE(time_table), time0_viewp, 0, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(time_table), time1_viewp, 0, 1, 1, 2,
			 GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(time_table), time2_viewp, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, 0, 0, 0);


	info_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
        gtk_widget_set_size_request(info_scrolledwin, 1, -1);           /* MAGIC */
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(info_scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_NEVER);

	info_viewp = gtk_viewport_new(NULL, NULL);
	gtk_widget_set_name(info_viewp, "info_viewport");
	gtk_container_add(GTK_CONTAINER(info_scrolledwin), info_viewp);
	gtk_widget_set_events(info_viewp, GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

        g_signal_connect(G_OBJECT(info_viewp), "button_press_event",
			 G_CALLBACK(scroll_btn_pressed), NULL);
        g_signal_connect(G_OBJECT(info_viewp), "button_release_event",
			 G_CALLBACK(scroll_btn_released), (gpointer)info_scrolledwin);
        g_signal_connect(G_OBJECT(info_viewp), "motion_notify_event",
			 G_CALLBACK(scroll_motion_notify), (gpointer)info_scrolledwin);

	info_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(info_hbox), 1);
	gtk_container_add(GTK_CONTAINER(info_viewp), info_hbox);


	title_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
        gtk_widget_set_size_request(title_scrolledwin, 1, -1);          /* MAGIC */
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(title_scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_NEVER);

	title_viewp = gtk_viewport_new(NULL, NULL);
	gtk_widget_set_name(title_viewp, "title_viewport");
	gtk_container_add(GTK_CONTAINER(title_scrolledwin), title_viewp);
	gtk_widget_set_events(title_viewp, GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
	g_signal_connect(G_OBJECT(title_viewp), "button_press_event", G_CALLBACK(scroll_btn_pressed), NULL);
	g_signal_connect(G_OBJECT(title_viewp), "button_release_event", G_CALLBACK(scroll_btn_released),
			 (gpointer)title_scrolledwin);
	g_signal_connect(G_OBJECT(title_viewp), "motion_notify_event", G_CALLBACK(scroll_motion_notify),
			 (gpointer)title_scrolledwin);

	title_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(title_hbox), 1);
	gtk_container_add(GTK_CONTAINER(title_viewp), title_hbox);

	gtk_box_pack_start(GTK_BOX(disp_vbox), title_scrolledwin, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(disp_vbox), info_scrolledwin, TRUE, FALSE, 0);

	/* labels */
	bigtimer_label = time_labels[options.time_idx[0]] = gtk_label_new("");

        if (options.override_skin_settings) {
                gtk_widget_modify_font (bigtimer_label, fd_bigtimer);
        }

        gtk_widget_set_name(time_labels[options.time_idx[0]], "big_timer_label");
	gtk_container_add(GTK_CONTAINER(time0_viewp), time_labels[options.time_idx[0]]);

	time_hbox1 = gtk_hbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(time_hbox1), 2);
	gtk_container_add(GTK_CONTAINER(time1_viewp), time_hbox1);

	smalltimer_label_1 = time_labels[options.time_idx[1]] = gtk_label_new("");

        if (options.override_skin_settings) {
                gtk_widget_modify_font (smalltimer_label_1, fd_smalltimer);
        }

        gtk_widget_set_name(time_labels[options.time_idx[1]], "small_timer_label");
	gtk_box_pack_start(GTK_BOX(time_hbox1), time_labels[options.time_idx[1]], TRUE, TRUE, 0);

	time_hbox2 = gtk_hbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(time_hbox2), 2);
	gtk_container_add(GTK_CONTAINER(time2_viewp), time_hbox2);

	smalltimer_label_2 = time_labels[options.time_idx[2]] = gtk_label_new("");

        if (options.override_skin_settings) {
                gtk_widget_modify_font (smalltimer_label_2, fd_smalltimer);
        }

        gtk_widget_set_name(time_labels[options.time_idx[2]], "small_timer_label");
	gtk_box_pack_start(GTK_BOX(time_hbox2), time_labels[options.time_idx[2]], TRUE, TRUE, 0);

	label_title = gtk_label_new("");
        gtk_widget_set_name(label_title, "label_title");
	gtk_box_pack_start(GTK_BOX(title_hbox), label_title, FALSE, FALSE, 3);
	
	label_mono = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(info_hbox), label_mono, FALSE, FALSE, 3);
	gtk_widget_set_name(label_mono, "label_info");

	label_samplerate = gtk_label_new("0");
	gtk_widget_set_name(label_samplerate, "label_info");
	gtk_box_pack_start(GTK_BOX(info_hbox), label_samplerate, FALSE, FALSE, 3);

	label_bps = gtk_label_new("");
	gtk_widget_set_name(label_bps, "label_info");
	gtk_box_pack_start(GTK_BOX(info_hbox), label_bps, FALSE, FALSE, 3);

	label_format = gtk_label_new("");
	gtk_widget_set_name(label_format, "label_info");
	gtk_box_pack_start(GTK_BOX(info_hbox), label_format, FALSE, FALSE, 3);

	info_vsep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(info_hbox), info_vsep, FALSE, FALSE, 3);

	label_output = gtk_label_new("");
	gtk_widget_set_name(label_output, "label_info");
	gtk_box_pack_start(GTK_BOX(info_hbox), label_output, FALSE, FALSE, 3);

	label_src_type = gtk_label_new("");
	gtk_widget_set_name(label_src_type, "label_info");
	gtk_box_pack_start(GTK_BOX(info_hbox), label_src_type, FALSE, FALSE, 3);

        if (options.override_skin_settings) {
                gtk_widget_modify_font (label_title, fd_songtitle);

                gtk_widget_modify_font (label_mono, fd_songinfo);
                gtk_widget_modify_font (label_samplerate, fd_songinfo);
                gtk_widget_modify_font (label_bps, fd_songinfo);
                gtk_widget_modify_font (label_format, fd_songinfo);
                gtk_widget_modify_font (label_output, fd_songinfo);
                gtk_widget_modify_font (label_src_type, fd_songinfo);
        }

	/* Volume and balance slider */
	vb_table = gtk_table_new(1, 3, FALSE);
        gtk_table_set_col_spacings(GTK_TABLE(vb_table), 3);
	gtk_box_pack_start(GTK_BOX(disp_vbox), vb_table, TRUE, FALSE, 0);

	adj_vol = gtk_adjustment_new(0.0f, -41.0f, 6.0f, 1.0f, 3.0f, 0.0f);
	g_signal_connect(G_OBJECT(adj_vol), "value_changed", G_CALLBACK(changed_vol), NULL);
	scale_vol = gtk_hscale_new(GTK_ADJUSTMENT(adj_vol));
	gtk_widget_set_name(scale_vol, "scale_vol");
	g_signal_connect(GTK_OBJECT(scale_vol), "button_press_event",
			   (GtkSignalFunc)scale_vol_button_press_event, NULL);
	g_signal_connect(GTK_OBJECT(scale_vol), "button_release_event",
			   (GtkSignalFunc)scale_vol_button_release_event, NULL);
	gtk_widget_set_size_request(scale_vol, -1, 8);
	gtk_scale_set_digits(GTK_SCALE(scale_vol), 0);
	gtk_scale_set_draw_value(GTK_SCALE(scale_vol), FALSE);
	gtk_table_attach(GTK_TABLE(vb_table), scale_vol, 0, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, 0, 0, 0);

        adj_bal = gtk_adjustment_new(0.0f, -100.0f, 100.0f, 1.0f, 10.0f, 0.0f);
        g_signal_connect(G_OBJECT(adj_bal), "value_changed", G_CALLBACK(changed_bal), NULL);
        scale_bal = gtk_hscale_new(GTK_ADJUSTMENT(adj_bal));
	gtk_scale_set_digits(GTK_SCALE(scale_bal), 0);
	gtk_widget_set_size_request(scale_bal, -1, 8);
	gtk_widget_set_name(scale_bal, "scale_bal");
	g_signal_connect(GTK_OBJECT(scale_bal), "button_press_event",
			   (GtkSignalFunc)scale_bal_button_press_event, NULL);
	g_signal_connect(GTK_OBJECT(scale_bal), "button_release_event",
			   (GtkSignalFunc)scale_bal_button_release_event, NULL);
        gtk_scale_set_draw_value(GTK_SCALE(scale_bal), FALSE);
	gtk_table_attach(GTK_TABLE(vb_table), scale_bal, 2, 3, 0, 1,
			 GTK_FILL | GTK_EXPAND, 0, 0, 0);

	/* Loop bar */

#ifdef HAVE_LOOP
	loop_bar = aqualung_loop_bar_new(options.loop_range_start, options.loop_range_end);
	g_signal_connect(loop_bar, "range-changed", G_CALLBACK(loop_range_changed_cb), NULL);
	gtk_widget_set_size_request(loop_bar, -1, 14);
	gtk_box_pack_start(GTK_BOX(vbox), loop_bar, FALSE, FALSE, 3);
#endif /* HAVE_LOOP */


	/* Position slider */
        adj_pos = gtk_adjustment_new(0, 0, 100, 1, 5, 0);
        g_signal_connect(G_OBJECT(adj_pos), "value_changed", G_CALLBACK(changed_pos), NULL);
        scale_pos = gtk_hscale_new(GTK_ADJUSTMENT(adj_pos));
	gtk_widget_set_name(scale_pos, "scale_pos");
	g_signal_connect(GTK_OBJECT(scale_pos), "button_press_event",
			   (GtkSignalFunc)scale_button_press_event, NULL);
	g_signal_connect(GTK_OBJECT(scale_pos), "button_release_event",
			   (GtkSignalFunc)scale_button_release_event, NULL);
	gtk_scale_set_digits(GTK_SCALE(scale_pos), 0);
        gtk_scale_set_draw_value(GTK_SCALE(scale_pos), FALSE);
	gtk_range_set_update_policy(GTK_RANGE(scale_pos), GTK_UPDATE_DISCONTINUOUS);
	gtk_box_pack_start(GTK_BOX(vbox), scale_pos, FALSE, FALSE, 3);

        vbox_sep = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), vbox_sep, FALSE, FALSE, 2);

        GTK_WIDGET_UNSET_FLAGS(scale_vol, GTK_CAN_FOCUS);
        GTK_WIDGET_UNSET_FLAGS(scale_bal, GTK_CAN_FOCUS);
        GTK_WIDGET_UNSET_FLAGS(scale_pos, GTK_CAN_FOCUS);

        /* cover display widget */

	cover_align = gtk_alignment_new(0.5f, 0.5f, 0.0f, 0.0f);
	gtk_box_pack_start(GTK_BOX(disp_hbox), cover_align, FALSE, FALSE, 0);
        cover_image_area = gtk_image_new();
        c_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(cover_align), c_event_box);
        gtk_container_add(GTK_CONTAINER(c_event_box), cover_image_area);
        g_signal_connect(G_OBJECT(c_event_box), "button_press_event",
                         G_CALLBACK(cover_press_button_cb), cover_image_area);

        /* Embedded playlist */

        if (options.playlist_is_embedded && options.buttons_at_the_bottom) {
		playlist_window = gtk_vbox_new(FALSE, 0);
		gtk_widget_set_name(playlist_window, "playlist_window");
		gtk_box_pack_start(GTK_BOX(vbox), playlist_window, TRUE, TRUE, 3);
	}

        /* Button box with prev, play, pause, stop, next buttons */

	btns_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), btns_hbox, FALSE, FALSE, 0);

	sprintf(path, "%s/%s", skin_path, "prev");
	prev_button = create_button_with_image(path, 0, "prev");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), prev_button, _("Previous song"), NULL);

	sprintf(path, "%s/%s", skin_path, "stop");
	stop_button = create_button_with_image(path, 0, "stop");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), stop_button, _("Stop"), NULL);

	sprintf(path, "%s/%s", skin_path, "next");
	next_button = create_button_with_image(path, 0, "next");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), next_button, _("Next song"), NULL);

	sprintf(path, "%s/%s", skin_path, "play");
	play_button = create_button_with_image(path, 1, "play");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), play_button, _("Play"), NULL);

	sprintf(path, "%s/%s", skin_path, "pause");
	pause_button = create_button_with_image(path, 1, "pause");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), pause_button, _("Pause"), NULL);

	GTK_WIDGET_UNSET_FLAGS(prev_button, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(stop_button, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(next_button, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(play_button, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(pause_button, GTK_CAN_FOCUS);

	gtk_box_pack_start(GTK_BOX(btns_hbox), prev_button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(btns_hbox), play_button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(btns_hbox), pause_button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(btns_hbox), stop_button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(btns_hbox), next_button, FALSE, FALSE, 0);

	g_signal_connect(prev_button, "clicked", G_CALLBACK(prev_event), NULL);
	play_id = g_signal_connect(play_button, "toggled", G_CALLBACK(play_event), NULL);
	pause_id = g_signal_connect(pause_button, "toggled", G_CALLBACK(pause_event), NULL);
	g_signal_connect(stop_button, "clicked", G_CALLBACK(stop_event), NULL);
	g_signal_connect(next_button, "clicked", G_CALLBACK(next_event), NULL);


	/* toggle buttons for shuffle and repeat */
	sr_table = gtk_table_new(2, 2, FALSE);

	sprintf(path, "%s/%s", skin_path, "repeat");
	repeat_button = create_button_with_image(path, 1, "repeat");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), repeat_button, _("Repeat current song"), NULL);
	gtk_widget_set_size_request(repeat_button, -1, 1);
	gtk_table_attach(GTK_TABLE(sr_table), repeat_button, 0, 1, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	g_signal_connect(repeat_button, "toggled", G_CALLBACK(repeat_toggled), NULL);

	sprintf(path, "%s/%s", skin_path, "repeat_all");
	repeat_all_button = create_button_with_image(path, 1, "rep_all");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), repeat_all_button, _("Repeat all songs"), NULL);
	gtk_widget_set_size_request(repeat_all_button, -1, 1);
	gtk_table_attach(GTK_TABLE(sr_table), repeat_all_button, 0, 1, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	g_signal_connect(repeat_all_button, "toggled", G_CALLBACK(repeat_all_toggled), NULL);

	sprintf(path, "%s/%s", skin_path, "shuffle");
	shuffle_button = create_button_with_image(path, 1, "shuffle");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), shuffle_button, _("Shuffle songs"), NULL);
	gtk_widget_set_size_request(shuffle_button, -1, 1);
	gtk_table_attach(GTK_TABLE(sr_table), shuffle_button, 1, 2, 0, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	g_signal_connect(shuffle_button, "toggled", G_CALLBACK(shuffle_toggled), NULL);

	GTK_WIDGET_UNSET_FLAGS(repeat_button, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(repeat_all_button, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(shuffle_button, GTK_CAN_FOCUS);

        /* toggle buttons for sub-windows visibility */
	sprintf(path, "%s/%s", skin_path, "pl");
	playlist_toggle = create_button_with_image(path, 1, "PL");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), playlist_toggle, _("Toggle playlist"), NULL);
	gtk_box_pack_end(GTK_BOX(btns_hbox), playlist_toggle, FALSE, FALSE, 0);
	
	sprintf(path, "%s/%s", skin_path, "ms");
	musicstore_toggle = create_button_with_image(path, 1, "MS");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), musicstore_toggle, _("Toggle music store"), NULL);
	gtk_box_pack_end(GTK_BOX(btns_hbox), musicstore_toggle, FALSE, FALSE, 3);

	sprintf(path, "%s/%s", skin_path, "fx");
	plugin_toggle = create_button_with_image(path, 1, "FX");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), plugin_toggle, _("Toggle LADSPA patch builder"), NULL);
#ifdef HAVE_LADSPA
	gtk_box_pack_end(GTK_BOX(btns_hbox), plugin_toggle, FALSE, FALSE, 0);
#endif /* HAVE_LADSPA */

	g_signal_connect(playlist_toggle, "toggled", G_CALLBACK(playlist_toggled), NULL);
	g_signal_connect(musicstore_toggle, "toggled", G_CALLBACK(musicstore_toggled), NULL);
	g_signal_connect(plugin_toggle, "toggled", G_CALLBACK(plugin_toggled), NULL);

	GTK_WIDGET_UNSET_FLAGS(playlist_toggle, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(musicstore_toggle, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(plugin_toggle, GTK_CAN_FOCUS);

	gtk_box_pack_end(GTK_BOX(btns_hbox), sr_table, FALSE, FALSE, 3);

        set_buttons_relief();

	/* Embedded playlist */
	if (options.playlist_is_embedded && !options.buttons_at_the_bottom) {
		playlist_window = gtk_vbox_new(FALSE, 0);
		gtk_widget_set_name(playlist_window, "playlist_window");
		gtk_box_pack_start(GTK_BOX(vbox), playlist_window, TRUE, TRUE, 3);
	}

        if (options.enable_tooltips) {
                gtk_tooltips_enable(aqualung_tooltips);
        } else {
                gtk_tooltips_disable(aqualung_tooltips);
        }
}


void
process_filenames(GSList * list, int enqueue, int start_playback, int has_tab, char * tab_name) {

	int mode;
	char * name = NULL;

	if (!has_tab) {
		if (enqueue) {
			mode = PLAYLIST_ENQUEUE;
		} else {
			mode = PLAYLIST_LOAD;
		}
	} else {
		if (strlen(tab_name) == 0) {
			mode = PLAYLIST_LOAD_TAB;
		} else {
			if (g_utf8_validate(tab_name, -1, NULL)) {
				name = g_strdup(tab_name);
			} else {
				name = g_locale_to_utf8(tab_name, -1, NULL, NULL, NULL);
			}
			if (name == NULL) {
				fprintf(stderr, "process_filenames(): unable to convert tab name to UTF-8\n");
			}

			if (enqueue) {
				mode = PLAYLIST_ENQUEUE;
			} else {
				mode = PLAYLIST_LOAD;
			}
		}
	}

	playlist_load(list, mode, name, start_playback);

	if (name != NULL) {
		g_free(name);
	}
}


/*** Systray support ***/
#ifdef HAVE_SYSTRAY
void
systray_popup_menu_cb(GtkStatusIcon * systray_icon, guint button, guint time, gpointer data) {

	if (get_systray_semaphore() == 0) {
		gtk_menu_popup(GTK_MENU(systray_menu), NULL, NULL,
			       gtk_status_icon_position_menu, data,
			       button, time);
	}
}

void
systray__show_cb(gpointer data) {

	show_all_windows(NULL);
}

void
systray__hide_cb(gpointer data) {

	hide_all_windows(NULL);
}

void
systray__play_cb(gpointer data) {

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
				     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button)));
}

void
systray__pause_cb(gpointer data) {

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button),
				     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pause_button)));
}

void
systray__stop_cb(gpointer data) {

	stop_event(NULL, NULL, NULL);
}

void
systray__prev_cb(gpointer data) {

	prev_event(NULL, NULL, NULL);
}

void
systray__next_cb(gpointer data) {

	next_event(NULL, NULL, NULL);
}

void
systray__quit_cb(gpointer data) {

	main_window_closing();
}

void
systray_activate_cb(GtkStatusIcon * systray_icon, gpointer data) {

	if (get_systray_semaphore() == 0) {
		if (!systray_main_window_on) {
			systray__show_cb(NULL);
		} else {
			systray__hide_cb(NULL);
		}
	}
}


/* returns a hbox with a stock image and label in it */
GtkWidget *
create_systray_menu_item(const gchar * stock_id, char * text) {

	GtkWidget * hbox;
	GtkWidget * widget;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_widget_show(hbox);
	widget = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_MENU);
	gtk_widget_show(widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 2);
	widget = gtk_label_new(text);
	gtk_widget_show(widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, TRUE, 2);

	return hbox;
}

void
setup_systray(void) {

	char path[MAXLEN];

	GtkWidget * systray__separator1;
	GtkWidget * systray__separator2;

	sprintf(path, "%s/icon_64.png", AQUALUNG_DATADIR);
	systray_icon = gtk_status_icon_new_from_file(path);

        g_signal_connect_swapped(G_OBJECT(systray_icon), "activate",
				 G_CALLBACK(systray_activate_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray_icon), "popup-menu",
				 G_CALLBACK(systray_popup_menu_cb), (gpointer)systray_icon);

        systray_menu = gtk_menu_new();

        systray__show = gtk_menu_item_new_with_label(_("Show Aqualung"));
        systray__hide = gtk_menu_item_new_with_label(_("Hide Aqualung"));

        systray__separator1 = gtk_separator_menu_item_new();

        systray__play = gtk_menu_item_new();
	gtk_container_add(GTK_CONTAINER(systray__play),
			  create_systray_menu_item(GTK_STOCK_MEDIA_PLAY, _("Play")));

        systray__pause = gtk_menu_item_new();
	gtk_container_add(GTK_CONTAINER(systray__pause),
			  create_systray_menu_item(GTK_STOCK_MEDIA_PAUSE, _("Pause")));

        systray__stop = gtk_menu_item_new();
	gtk_container_add(GTK_CONTAINER(systray__stop),
			  create_systray_menu_item(GTK_STOCK_MEDIA_STOP, _("Stop")));

        systray__prev = gtk_menu_item_new();
	gtk_container_add(GTK_CONTAINER(systray__prev),
			  create_systray_menu_item(GTK_STOCK_MEDIA_PREVIOUS, _("Previous")));

        systray__next = gtk_menu_item_new();
	gtk_container_add(GTK_CONTAINER(systray__next),
			  create_systray_menu_item(GTK_STOCK_MEDIA_NEXT, _("Next")));

        systray__separator2 = gtk_separator_menu_item_new();

        systray__quit = gtk_menu_item_new();
	gtk_container_add(GTK_CONTAINER(systray__quit),
			  create_systray_menu_item(GTK_STOCK_STOP, _("Quit")));
	
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__show);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__hide);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__separator1);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__play);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__pause);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__stop);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__prev);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__next);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__separator2);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__quit);

        g_signal_connect_swapped(G_OBJECT(systray__show), "activate", G_CALLBACK(systray__show_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray__hide), "activate", G_CALLBACK(systray__hide_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray__play), "activate", G_CALLBACK(systray__play_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray__pause), "activate", G_CALLBACK(systray__pause_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray__stop), "activate", G_CALLBACK(systray__stop_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray__prev), "activate", G_CALLBACK(systray__prev_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray__next), "activate", G_CALLBACK(systray__next_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray__quit), "activate", G_CALLBACK(systray__quit_cb), NULL);

        gtk_widget_show(systray_menu);
        gtk_widget_show(systray__hide);
        gtk_widget_show(systray__separator1);
	gtk_widget_show(systray__play);
	gtk_widget_show(systray__pause);
	gtk_widget_show(systray__stop);
	gtk_widget_show(systray__prev);
	gtk_widget_show(systray__next);
	
        gtk_widget_show(systray__separator2);
        gtk_widget_show(systray__quit);
}
#endif /* HAVE_SYSTRAY */

void
create_gui(int argc, char ** argv, int optind, int enqueue,
	   unsigned long rate, unsigned long rb_audio_size) {

	char path[MAXLEN];
	GList * glist = NULL;
	GdkPixbuf * pixbuf = NULL;

	srand(time(0));
	sample_pos = 0;
	out_SR = rate;
	rb_size = rb_audio_size;

	gtk_init(&argc, &argv);
#ifdef HAVE_LADSPA
	lrdf_init();
#endif /* HAVE_LADSPA */

	if (chdir(options.confdir) != 0) {
		if (errno == ENOENT) {
			fprintf(stderr, "Creating directory %s\n", options.confdir);
			mkdir(options.confdir, S_IRUSR | S_IWUSR | S_IXUSR);
			chdir(options.confdir);
		} else {
			fprintf(stderr, "An error occured while attempting chdir(\"%s\"). errno = %d\n",
				options.confdir, errno);
		}
	}

	load_config();

	if (options.title_format[0] == '\0')
		sprintf(options.title_format, "%%a: %%t [%%r]");
	if (options.skin[0] == '\0') {
		sprintf(options.skin, "%s/plain", AQUALUNG_SKINDIR);
		options.main_pos_x = 280;
		options.main_pos_y = 30;
		options.main_size_x = 380;
		options.main_size_y = 380;
		options.browser_pos_x = 30;
		options.browser_pos_y = 30;
		options.browser_size_x = 240;
		options.browser_size_y = 380;
		options.browser_on = 1;
		options.playlist_pos_x = 300;
		options.playlist_pos_y = 180;
		options.playlist_size_x = 400;
		options.playlist_size_y = 500;
		options.playlist_on = 1;
	}

	if (options.cddb_server[0] == '\0') {
		sprintf(options.cddb_server, "freedb.org");
	}

	if (options.src_type == -1) {
		options.src_type = 4;
	}

	sprintf(path, "%s/rc", options.skin);
	gtk_rc_parse(path);

#ifdef HAVE_SYSTRAY
	setup_systray();
#endif /* HAVE_SYSTRAY */

	create_main_window(options.skin);

	vol_prev = -101.0f;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_vol), options.vol);
	bal_prev = -101.0f;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_bal), options.bal);

	create_playlist();

        playlist_size_allocate_all();

	create_music_browser();

#ifdef HAVE_LADSPA
	create_fxbuilder();
	load_plugin_data();
#endif /* HAVE_LADSPA */

	load_all_music_store();
#ifdef HAVE_CDDA
	create_cdda_node();
	cdda_scanner_start();
#endif /* HAVE_CDDA */

	sprintf(path, "%s/icon_16.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		glist = g_list_append(glist, gdk_pixbuf_new_from_file(path, NULL));
	}

	sprintf(path, "%s/icon_24.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		glist = g_list_append(glist, gdk_pixbuf_new_from_file(path, NULL));
	}

	sprintf(path, "%s/icon_32.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		glist = g_list_append(glist, gdk_pixbuf_new_from_file(path, NULL));
	}

	sprintf(path, "%s/icon_48.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		glist = g_list_append(glist, gdk_pixbuf_new_from_file(path, NULL));
	}

	sprintf(path, "%s/icon_64.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		glist = g_list_append(glist, gdk_pixbuf_new_from_file(path, NULL));
	}

	if (glist != NULL) {
		gtk_window_set_default_icon_list(glist);
		g_list_free(glist);
	}

	if (options.repeat_on) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_button), TRUE);
	}

	if (options.repeat_all_on) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_all_button), TRUE);
	}

	if (options.shuffle_on) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shuffle_button), TRUE);
	}

        if (playlist_state != -1) {
                options.playlist_on = playlist_state;
	}

        if (browser_state != -1) {
                options.browser_on = browser_state;
	}

	if (options.browser_on) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(musicstore_toggle), TRUE);
		deflicker();
	}

	if (options.playlist_on) {
		if (options.playlist_is_embedded) {
			g_signal_handlers_block_by_func(G_OBJECT(playlist_toggle), playlist_toggled, NULL);
		}
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle), TRUE);
		deflicker();
		if (options.playlist_is_embedded) {
			g_signal_handlers_unblock_by_func(G_OBJECT(playlist_toggle), playlist_toggled, NULL);
		}
	}

	restore_window_position();

	gtk_widget_show_all(main_window);

	deflicker();

        cover_show_flag = 0;
        gtk_widget_hide(cover_align);
        gtk_widget_hide(c_event_box);
        gtk_widget_hide(cover_image_area);

        gtk_widget_hide(vbox_sep);

	if (options.playlist_is_embedded) {
		if (!options.playlist_on) {
                        hide_playlist();
			deflicker();
		}
	}

#ifdef HAVE_LOOP
	if (!options.repeat_on) {
		hide_loop_bar();
	}
#endif /* HAVE_LOOP */


        /* update sliders' tooltips */
        if (options.enable_tooltips) {
                changed_vol(GTK_ADJUSTMENT(adj_vol), NULL);
                changed_bal(GTK_ADJUSTMENT(adj_bal), NULL);
	        changed_pos(GTK_ADJUSTMENT(adj_pos), NULL);
        }

	zero_displays();
	if (options.auto_save_playlist) {
		/* start playback only if no files to be loaded on command line */
		int start_playback = (argv[optind] == NULL) && immediate_start;
		char file[MAXLEN];

		snprintf(file, MAXLEN-1, "%s/%s", options.confdir, "playlist.xml");
		if (g_file_test(file, G_FILE_TEST_EXISTS) == TRUE) {
			GSList * list = g_slist_append(NULL, strdup(file));
			playlist_load(list, PLAYLIST_LOAD_TAB, NULL, start_playback);
		}
	}

	/* read command line filenames */
	{
		int i;
		GSList * list = NULL;

		for (i = optind; argv[i] != NULL; i++) {
			list = g_slist_append(list, strdup(argv[i]));
		}

		if (list != NULL) {
			process_filenames(list, enqueue, immediate_start, (tab_name != NULL), tab_name);
		}
	}

	/* ensure that at least one playlist has been created */
	playlist_ensure_tab_exists();

	playlist_set_color();

	/* activate jack client and connect ports */
#ifdef HAVE_JACK
	if (output == JACK_DRIVER) {
		jack_client_start();
	}
#endif /* HAVE_JACK */

	/* set timeout function */
	timeout_tag = g_timeout_add(TIMEOUT_PERIOD, timeout_callback, NULL);

        if (options.playlist_is_embedded) {
                gtk_widget_set_sensitive(plist__fileinfo, FALSE);
        }
}


void
adjust_remote_volume(char * str) {

	char * endptr = NULL;
	int val = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_vol));

	switch (str[0]) {
	case 'm':
	case 'M':
		val = -41;
		break;
	case '=':
		val = strtol(str + 1, &endptr, 10);
		if (endptr[0] != '\0') {
			fprintf(stderr, "Cannot convert to integer value: %s\n", str + 1);
			return;
		}
		break;
	default:
		val += strtol(str, &endptr, 10);
		if (endptr[0] != '\0') {
			fprintf(stderr, "Cannot convert to integer value: %s\n", str);
			return;
		}
		break;
	}

	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_vol), val);
}


void
process_metablock(metadata_t * meta) {

	char title_str[MAXLEN];
	char playlist_str[MAXLEN];
	playlist_t * pl;

	/* metadata_dump(meta); */

	metadata_make_title_string(meta, title_str);
	metadata_make_playlist_string(meta, playlist_str);

	set_title_label(title_str);

	/* set playlist_str for playlist entry */
	if ((pl = playlist_get_playing()) != NULL) {
		GtkTreePath * p = playlist_get_playing_path(pl);
		if (p != NULL) {
			GtkTreeIter iter;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);

			gtk_tree_store_set(pl->store, &iter,
					   PL_COL_TRACK_NAME, playlist_str,
					   -1);

			gtk_tree_path_free(p);
		}
	}

	metadata_free(meta);
}


gint
timeout_callback(gpointer data) {

	long pos;
	char cmd;
	cue_t cue;
	static double left_gain_shadow;
	static double right_gain_shadow;
	char rcmd;
	static char cmdbuf[MAXLEN];
	int rcv_count;
	static GSList * file_list = NULL;
#ifdef HAVE_JACK
	static int jack_popup_beenthere = 0;
#endif /* HAVE_JACK */
	metadata_t * meta;


	while (rb_read_space(rb_disk2gui)) {
		rb_read(rb_disk2gui, &recv_cmd, 1);
		switch (recv_cmd) {

		case CMD_FILEREQ:
			cmd = CMD_CUE;
			cue.filename = NULL;
			cue.voladj = 0.0f;

			if (!is_file_loaded)
				break; /* ignore leftover filereq message */

			decide_next_track(&cue);

			if (cue.filename != NULL) {
				rb_write(rb_gui2disk, &cmd, sizeof(char));
				rb_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
			} else {
				send_cmd = CMD_STOPWOFL;
				rb_write(rb_gui2disk, &send_cmd, sizeof(char));
			}

			try_waking_disk_thread();
			break;

		case CMD_FILEINFO:
			while (rb_read_space(rb_disk2gui) < sizeof(fileinfo_t))
				;
			rb_read(rb_disk2gui, (char *)&fileinfo, sizeof(fileinfo_t));

			total_samples = fileinfo.total_samples;
			status.samples_left = fileinfo.total_samples;
			status.sample_pos = 0;
			status.sample_offset = 0;
			fresh_new_file = fresh_new_file_prev = 0;
			break;

		case CMD_STATUS:
			while (rb_read_space(rb_disk2gui) < sizeof(status_t))
				;
			rb_read(rb_disk2gui, (char *)&status, sizeof(status_t));

			pos = total_samples - status.samples_left;

#ifdef HAVE_LOOP
			if (options.repeat_on &&
			    (pos < total_samples * options.loop_range_start ||
			     pos > total_samples * options.loop_range_end)) {

				seek_t seek;

				send_cmd = CMD_SEEKTO;
				seek.seek_to_pos = options.loop_range_start * total_samples;
				rb_write(rb_gui2disk, &send_cmd, 1);
				rb_write(rb_gui2disk, (char *)&seek, sizeof(seek_t));
				try_waking_disk_thread();
				refresh_scale_suppress = 2;
			}
#endif /* HAVE_LOOP */

			if ((is_file_loaded) && (status.samples_left < 2*status.sample_offset)) {
				allow_seeks = 0;
			} else {
				allow_seeks = 1;
			}

			/* treat files with unknown length */
			if (total_samples == 0) {
				allow_seeks = 1;
				pos = status.sample_pos - status.sample_offset;
			}

			if ((!fresh_new_file) && (pos > status.sample_offset)) {
				fresh_new_file = 1;
			}

			if (fresh_new_file && !fresh_new_file_prev) {
				disp_info = fileinfo;
				disp_samples = total_samples;
				if (pos > status.sample_offset) {
					disp_pos = pos - status.sample_offset;
				} else {
					disp_pos = 0;
				}
				refresh_displays();
			} else {
				if (pos > status.sample_offset) {
					disp_pos = pos - status.sample_offset;
				} else {
					disp_pos = 0;
				}

				if (is_file_loaded) {
					refresh_time_displays();
				}
			}

			fresh_new_file_prev = fresh_new_file;

			if (refresh_scale && !refresh_scale_suppress && GTK_IS_ADJUSTMENT(adj_pos)) {
				if (total_samples == 0) {
					gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_pos), 0.0f);
				} else {
					gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_pos),
								 100.0f * (double)(pos) / total_samples);
				}
			}

			if (refresh_scale_suppress > 0) {
				--refresh_scale_suppress;
			}

                        show_scale_pos(!total_samples ? FALSE : TRUE);
			break;

		case CMD_METABLOCK:
			while (rb_read_space(rb_disk2gui) < sizeof(metadata_t *))
				;
			rb_read(rb_disk2gui, (char *)&meta, sizeof(metadata_t *));

			process_metablock(meta);
			break;

		default:
			fprintf(stderr, "gui: unexpected command %d recv'd from disk\n", recv_cmd);
			break;
		}
	}
        /* update volume & balance if necessary */
	if ((options.vol != vol_prev) || (options.bal != bal_prev)) {
		vol_prev = options.vol;
		vol_lin = (options.vol < -40.5f) ? 0 : db2lin(options.vol);
		bal_prev = options.bal;
		if (options.bal >= 0.0f) {
			left_gain_shadow = vol_lin * db2lin(-0.4f * options.bal);
			right_gain_shadow = vol_lin;
		} else {
			left_gain_shadow = vol_lin;
			right_gain_shadow = vol_lin * db2lin(0.4f * options.bal);
		}
			left_gain = left_gain_shadow;
			right_gain = right_gain_shadow;
	}

	/* receive and execute remote commands, if any */
	rcv_count = 0;
	while (((rcmd = receive_message(aqualung_socket_fd, cmdbuf)) != 0) && (rcv_count < MAX_RCV_COUNT)) {
		switch (rcmd) {
		case RCMD_BACK:
			prev_event(NULL, NULL, NULL);
			break;
		case RCMD_PLAY:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
			     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button)));
			break;
		case RCMD_PAUSE:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button),
			        !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pause_button)));
			break;
		case RCMD_STOP:
			stop_event(NULL, NULL, NULL);
			break;
		case RCMD_FWD:
			next_event(NULL, NULL, NULL);
			break;
		case RCMD_ADD_FILE:
			file_list = g_slist_append(file_list, strdup(cmdbuf));
			break;
		case RCMD_ADD_COMMIT:
			process_filenames(file_list, cmdbuf[0], cmdbuf[1], cmdbuf[2], cmdbuf+3);
			file_list = NULL;
			break;
		case RCMD_VOLADJ:
			adjust_remote_volume(cmdbuf);
			break;
		case RCMD_QUIT:
			main_window_closing();
			break;
		}
		++rcv_count;
	}

	/* check for JACK shutdown condition */
#ifdef HAVE_JACK
	if (output == JACK_DRIVER) {
		if (jack_is_shutdown) {
			if (is_file_loaded) {
				stop_event(NULL, NULL, NULL);
			}
			if (!jack_popup_beenthere) {
				jack_shutdown_window();
				if (ports_window) {
					ports_clicked_close(NULL, NULL);
				}
				gtk_widget_set_sensitive(conf__jack, FALSE);
				jack_popup_beenthere = 1;
			}
		}
	}
#endif /* HAVE_JACK */

	return TRUE;
}


void
run_gui(void) {

	gtk_main();

	return;
}


void
set_buttons_relief(void) {

	GtkWidget *rbuttons_table[] = {
		prev_button, stop_button, next_button, play_button,
		pause_button, repeat_button, repeat_all_button, shuffle_button,
		playlist_toggle, musicstore_toggle, plugin_toggle
	};

	gint i, n;

        i = sizeof(rbuttons_table)/sizeof(GtkWidget*);

        for (n = 0; n < i; n++) {
	        if (options.disable_buttons_relief) {
                        gtk_button_set_relief (GTK_BUTTON (rbuttons_table[n]), GTK_RELIEF_NONE); 
		} else {
                        gtk_button_set_relief (GTK_BUTTON (rbuttons_table[n]), GTK_RELIEF_NORMAL);
		}
	}
                
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

