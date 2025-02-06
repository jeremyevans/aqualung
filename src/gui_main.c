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
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

#ifdef HAVE_JACK
#include <jack/jack.h>
#endif /* HAVE_JACK */

#ifdef HAVE_SRC
#include <samplerate.h>
#endif /* HAVE_SRC */

#ifdef HAVE_LADSPA
#include <lrdf.h>
#include "plugin.h"
#endif /* HAVE_LADSPA */

#ifdef HAVE_CDDA
#include "cdda.h"
#include "store_cdda.h"
#endif /* HAVE_CDDA */

#ifdef HAVE_JACK_MGMT
#include "ports.h"
#endif /* HAVE_JACK_MGMT */

#ifdef HAVE_LOOP
#include "loop_bar.h"
#endif /* HAVE_LOOP */

#ifdef HAVE_PODCAST
#include "store_podcast.h"
#endif /* HAVE_PODCAST */

#include "athread.h"
#include "ext_lua.h"
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
#include "playlist.h"
#include "file_info.h"
#include "i18n.h"
#include "httpc.h"
#include "metadata.h"
#include "metadata_api.h"
#include "music_browser.h"
#include "version.h"
#include "gui_main.h"


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
extern char * jack_shutdown_reason;
#endif /* HAVE_JACK */

extern int aqualung_socket_fd;
extern int aqualung_session_id;

extern GtkListStore * running_store;

extern GtkWidget * plist_menu;
extern GtkWidget * playlist_notebook;

void init_plist_menu(GtkWidget *append_menu);

/* file decoder instance used by the disk thread - use with caution! */
extern file_decoder_t * fdec;

/* the physical name of the file that is playing, or a '\0'. */
char current_file[MAXLEN];

/* embedded picture in the file that is playing */
void * embedded_picture = NULL;
int embedded_picture_size = 0;

/* default window title */
char win_title[MAXLEN];

/* current application title when a file is playing */
char playing_app_title[MAXLEN];

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

/* Whether to stop playing after currently played song ends. */
int stop_after_current_song = 0;

/* Whether the systray is used in this instance */
int systray_used = 0;

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

GtkWidget * main_window;

extern GtkWidget * browser_window;
extern GtkWidget * playlist_window;
extern GtkWidget * vol_window;
extern GtkWidget * browser_paned;

extern int music_store_changed;

#ifdef HAVE_LADSPA
extern int fxbuilder_on;
extern GtkWidget * fxbuilder_window;
GtkWidget * plugin_toggle;
#endif /* HAVE_LADSPA */

GObject * adj_pos;
GObject * adj_vol;
GObject * adj_bal;
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
GtkWidget * label_input;
GtkWidget * label_output;
GtkWidget * label_src_type;

int x_scroll_start;
int x_scroll_pos;
int scroll_btn;

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

/* Whether the next key binding should be for a custom key binding */
int custom_main_keybinding_expected = 0;

/* popup menu for configuration */
GtkWidget * conf_menu;
GtkWidget * conf__options;
GtkWidget * conf__skin;
GtkWidget * conf__fileinfo;
GtkWidget * conf__about;
GtkWidget * conf__quit;
#ifdef HAVE_JACK_MGMT
extern GtkWidget * ports_window;
GtkWidget * conf__jack;
#endif /* HAVE_JACK_MGMT */

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

int wm_not_systray_capable = 0;

void hide_all_windows(gpointer data);

/* Used for not reacting too quickly to consecutive mouse wheel events */
guint32 last_systray_scroll_event_time = 0;

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

	tmp[0] = '\0';

	if (is_file_loaded) {
		/* Remember current title. */
		arr_strlcpy(playing_app_title, str);

		gtk_label_set_text(GTK_LABEL(label_title), str);
		if (options.show_sn_title) {
			if (stop_after_current_song) {
				arr_strlcat(tmp, "[");
				arr_strlcat(tmp, _("STOPPING"));
				arr_strlcat(tmp, "] ");
			}
			arr_strlcat(tmp, str);
			arr_strlcat(tmp, " - ");
			arr_strlcat(tmp, win_title);
			gtk_window_set_title(GTK_WINDOW(main_window), tmp);
#ifdef HAVE_SYSTRAY
			if (systray_used) {
				aqualung_status_icon_set_tooltip_text(systray_icon, tmp);
			}
#endif /* HAVE_SYSTRAY */
		} else {
			gtk_window_set_title(GTK_WINDOW(main_window), win_title);
#ifdef HAVE_SYSTRAY
			if (systray_used) {
				aqualung_status_icon_set_tooltip_text(systray_icon, win_title);
			}
#endif /* HAVE_SYSTRAY */
		}
	} else {
		arr_snprintf(default_title, "Aqualung %s", AQUALUNG_VERSION);
		gtk_label_set_text(GTK_LABEL(label_title), default_title);
		gtk_window_set_title(GTK_WINDOW(main_window), win_title);
#ifdef HAVE_SYSTRAY
		if (systray_used) {
			aqualung_status_icon_set_tooltip_text(systray_icon, win_title);
		}
#endif /* HAVE_SYSTRAY */
        }
}

void
hide_cover_thumbnail(void) {
        cover_show_flag = 0;
        gtk_widget_hide(cover_image_area);
        gtk_widget_hide(c_event_box);
        gtk_widget_hide(cover_align);
}

void
format_bps_label(int bps, int format_flags, char * str, size_t str_size) {

	if (bps == 0) {
		g_strlcpy(str, "N/A kbit/s", str_size);
		return;
	}

	if (format_flags & FORMAT_VBR) {
		snprintf(str, str_size, "%.1f kbit/s VBR", bps/1000.0);
	} else {
		if (format_flags & FORMAT_UBR) {
			snprintf(str, str_size, "%.1f kbit/s UBR", bps/1000.0);
		} else {
			snprintf(str, str_size, "%.1f kbit/s", bps/1000.0);
		}
	}
}

void
set_input_label(fileinfo_t di) {
	char str[MAXLEN];

	if (!is_file_loaded) {
		arr_strlcpy(str, _("IDLE"));
	} else {
		char bps_str [128];
		format_bps_label(di.bps, di.format_flags, bps_str, CHAR_ARRAY_SIZE(bps_str));
		arr_snprintf(str, "%s %ld Hz %s %s",
			     (di.is_mono ? _("MONO") : _("STEREO")),
			     di.sample_rate, bps_str, di.format_str);
	}
	gtk_label_set_text(GTK_LABEL(label_input), str);
}

void
set_output_label(int output, int out_SR) {

	char str[MAXLEN];

	switch (output) {
#ifdef HAVE_PULSE
	case PULSE_DRIVER:
		arr_snprintf(str, "PulseAudio @ %d Hz", out_SR);
		break;
#endif /* HAVE_PULSE */
#ifdef HAVE_SNDIO
	case SNDIO_DRIVER:
		arr_snprintf(str, "sndio @ %d Hz", out_SR);
		break;
#endif /* HAVE_SNDIO */
#ifdef HAVE_OSS
	case OSS_DRIVER:
		arr_snprintf(str, "OSS @ %d Hz", out_SR);
		break;
#endif /* HAVE_OSS */
#ifdef HAVE_ALSA
	case ALSA_DRIVER:
		arr_snprintf(str, "ALSA @ %d Hz", out_SR);
		break;
#endif /* HAVE_ALSA */
#ifdef HAVE_JACK
	case JACK_DRIVER:
		arr_snprintf(str, "JACK @ %d Hz", out_SR);
		break;
#endif /* HAVE_JACK */
#ifdef HAVE_WINMM
	case WIN32_DRIVER:
		arr_snprintf(str, "Win32 @ %d Hz", out_SR);
		break;
#endif /* HAVE_WINMM */
	default:
		arr_strlcpy(str, _("No output"));
		break;
	}

	gtk_label_set_text(GTK_LABEL(label_output), str);
}



void
set_src_type_label(int src_type) {

#ifdef HAVE_SRC
	gtk_label_set_text(GTK_LABEL(label_src_type), src_get_name(src_type));
#else
	gtk_label_set_text(GTK_LABEL(label_src_type), _("None"));
#endif /* HAVE_SRC */

}


void
refresh_time_displays(void) {

	char str[MAXLEN];

	if (is_file_loaded) {
		if (refresh_time_label || options.time_idx[0] != 0) {
			sample2time(disp_info.sample_rate, disp_pos, str, CHAR_ARRAY_SIZE(str), 0);
			gtk_label_set_text(GTK_LABEL(time_labels[0]), str);

		}

		if (refresh_time_label || options.time_idx[0] != 1) {
			if (disp_samples == 0) {
				arr_strlcpy(str, " N/A ");
			} else {
				sample2time(disp_info.sample_rate, disp_samples - disp_pos, str, CHAR_ARRAY_SIZE(str), 1);
			}
			gtk_label_set_text(GTK_LABEL(time_labels[1]), str);

		}

		if (refresh_time_label || options.time_idx[0] != 2) {
			if (disp_samples == 0) {
				arr_strlcpy(str, " N/A ");
			} else {
				sample2time(disp_info.sample_rate, disp_samples, str, CHAR_ARRAY_SIZE(str), 0);
			}
			gtk_label_set_text(GTK_LABEL(time_labels[2]), str);

		}
	} else {
		int i;
		for (i = 0; i < 3; i++) {
			gtk_label_set_text(GTK_LABEL(time_labels[i]), " 00:00 ");
		}
	}
}

#ifdef HAVE_LOOP

void
loop_bar_update_tooltip(void) {

        if (options.enable_tooltips) {
		char str[MAXLEN];

		if (is_file_loaded) {
			char start[32];
			char end[32];
			sample2time(disp_info.sample_rate, total_samples * options.loop_range_start, start, CHAR_ARRAY_SIZE(start), 0);
			sample2time(disp_info.sample_rate, total_samples * options.loop_range_end, end, CHAR_ARRAY_SIZE(end), 0);
			arr_snprintf(str, _("Loop range: %d-%d%% [%s - %s]"),
				     (int)(100 * options.loop_range_start),
				     (int)(100 * options.loop_range_end),
				     start, end);
		} else {
			arr_snprintf(str, _("Loop range: %d-%d%%"),
				     (int)(100 * options.loop_range_start),
				     (int)(100 * options.loop_range_end));
		}

                aqualung_widget_set_tooltip_text(loop_bar, str);
        }
}

void
loop_range_changed_cb(AqualungLoopBar * bar, float start, float end, gpointer data) {

	options.loop_range_start = start;
	options.loop_range_end = end;
	loop_bar_update_tooltip();
}

#endif /* HAVE_LOOP */

void
refresh_displays(void) {

	GtkTreePath * p;
	GtkTreeIter iter;
	playlist_t * pl;

	refresh_time_displays();

#ifdef HAVE_LOOP
	loop_bar_update_tooltip();
#endif /* HAVE_LOOP */

	set_input_label(disp_info);
	set_output_label(output, out_SR);
	set_src_type_label(options.src_type);

	if ((pl = playlist_get_playing()) == NULL) {
		if ((pl = playlist_get_current()) == NULL) {
			set_title_label("");
                        hide_cover_thumbnail();
			return;
		}
	}

	p = playlist_get_playing_path(pl);
	if (p != NULL) {
		playlist_data_t * pldata;

		gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
		gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_DATA, &pldata, -1);
		gtk_tree_path_free(p);

		if (!httpc_is_url(pldata->file) && !options.use_ext_title_format) {
			char list_str[MAXLEN];
			playlist_data_get_display_name(list_str, CHAR_ARRAY_SIZE(list_str), pldata);
			set_title_label(list_str);
		} else if (!is_file_loaded) {
			char * name;
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_NAME, &name, -1);
			set_title_label(name);
			g_free(name);
		}

		if (is_file_loaded) {
                        if (!options.dont_show_cover) {
				if (embedded_picture != NULL && (find_cover_filename(pldata->file) == NULL || !options.use_external_cover_first)) {
					display_cover_from_binary(cover_image_area, c_event_box, cover_align, 48, 48,
						      embedded_picture, embedded_picture_size, TRUE, TRUE);
				} else {
					if (options.show_cover_for_ms_tracks_only) {
						if (IS_PL_COVER(pldata)) {
							display_cover(cover_image_area, c_event_box, cover_align,
								      48, 48, pldata->file, TRUE, TRUE);
						} else {
							hide_cover_thumbnail();
						}
					} else {
						display_cover(cover_image_area, c_event_box, cover_align,
							      48, 48, pldata->file, TRUE, TRUE);
					}
				}
                        }
		}
	} else if (!is_file_loaded) {
		set_title_label("");
                hide_cover_thumbnail();
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
		GtkAllocation allocation;
		gtk_widget_get_allocation(playlist_window, &allocation);
		options.playlist_size_x = allocation.width;
		options.playlist_size_y = allocation.height;
	}

	if (!options.hide_comment_pane) {
		options.browser_paned_pos = gtk_paned_get_position(GTK_PANED(browser_paned));
	}
}


void
restore_window_position(void) {

	gtk_window_move(GTK_WINDOW(main_window), options.main_pos_x, options.main_pos_y);
	gtk_window_move(GTK_WINDOW(browser_window), options.browser_pos_x, options.browser_pos_y);
	if (!options.playlist_is_embedded) {
		gtk_window_move(GTK_WINDOW(playlist_window), options.playlist_pos_x, options.playlist_pos_y);
	}

	gtk_window_resize(GTK_WINDOW(main_window), options.main_size_x, options.main_size_y);
	gtk_window_resize(GTK_WINDOW(browser_window), options.browser_size_x, options.browser_size_y);
	if (!options.playlist_is_embedded) {
		gtk_window_resize(GTK_WINDOW(playlist_window), options.playlist_size_x, options.playlist_size_y);
	}

	if (!options.hide_comment_pane) {
		gtk_paned_set_position(GTK_PANED(browser_paned), options.browser_paned_pos);
	}
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
	if (systray_used) {
		gtk_status_icon_set_visible(GTK_STATUS_ICON(systray_icon), FALSE);
	}
#endif /* HAVE_SYSTRAY */

	if (options.auto_save_playlist) {
		char playlist_name[MAXLEN];

		arr_snprintf(playlist_name, "%s/%s", options.confdir, "playlist.xml");
		playlist_save_all(playlist_name);
	}

        pango_font_description_free(fd_playlist);
        pango_font_description_free(fd_browser);

	finalize_options();

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
			music_store_save_all();
		}
	}

	main_window_close(NULL, NULL, NULL);
}

gboolean
main_window_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

	if (event->type == GDK_DELETE) {
#ifdef HAVE_SYSTRAY
		if (systray_used) {
			hide_all_windows(NULL);
		} else {
			main_window_closing();
		}
#else
		main_window_closing();
#endif /* HAVE_SYSTRAY */
		return TRUE;
	}

	return FALSE;
}

void
main_window_resize(void) {

        if (options.playlist_is_embedded && !options.playlist_on) {
		GtkAllocation allocation;
		gtk_widget_get_allocation(playlist_window, &allocation);
		gtk_window_resize(GTK_WINDOW(main_window), options.main_size_x,
				  options.main_size_y - 14 - 6 - 6 - allocation.height);
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


#ifdef HAVE_JACK_MGMT
void
conf__jack_cb(gpointer data) {

	port_setup_dialog();
}
#endif /* HAVE_JACK_MGMT */


void
conf__fileinfo_cb(gpointer data) {

	playlist_t * pl;
	GtkTreePath * p;
	GtkTreeIter iter;

	if (is_file_loaded) {
		pl = playlist_get_playing();
	} else {
		pl = playlist_get_current();
	}
	if (pl == NULL) return;
	p = playlist_get_playing_path(pl);
	if (p == NULL) return;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
	gtk_tree_path_free(p);
	show_file_info(GTK_TREE_MODEL(pl->store), iter, playlist_model_func, 0, FALSE, TRUE);
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


#ifdef HAVE_LADSPA
void
plugin_toggled(GtkWidget * widget, gpointer data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		show_fxbuilder();
	} else {
		hide_fxbuilder();
	}
}
#endif /* HAVE_LADSPA */

void
toggle_stop_after_current_song() {

	if (is_file_loaded) {
		stop_after_current_song = !stop_after_current_song;
		set_title_label(playing_app_title);
	}
}

void
move_song_position_slider(GtkScrollType scroll_type) {

	if (is_file_loaded && allow_seeks && total_samples != 0) {
		/* 5 seconds step */
		gdouble step = 5 * 100.0f * disp_info.sample_rate / total_samples;
		gdouble pos = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_pos));

		refresh_scale = 0;
		if (scroll_type == GTK_SCROLL_STEP_FORWARD) { 
			pos += step;
			if (pos > 100.0) {
				pos = 100.0;
			}
			gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_pos), pos);
		} else if (scroll_type == GTK_SCROLL_STEP_BACKWARD) {
			pos -= step;
			if (pos < 0) {
				pos = 0.0;
			}
			gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_pos), pos);
		}
	}
}

void
seek_song(void) {

	if (is_file_loaded && allow_seeks && refresh_scale == 0 && total_samples != 0) {
		seek_t seek;

		refresh_scale = 1;
		send_cmd = CMD_SEEKTO;
		seek.seek_to_pos = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_pos)) / 100.0f * total_samples;
		rb_write(rb_gui2disk, &send_cmd, 1);
		rb_write(rb_gui2disk, (char *)&seek, sizeof(seek_t));
		try_waking_disk_thread();
		refresh_scale_suppress = 2;
        }
}

void
change_volume_or_balance(GtkWidget * slider_widget, GtkScrollType scroll_type) {

	refresh_time_label = 0;
	if (vol_bal_timeout_tag) {
		g_source_remove(vol_bal_timeout_tag);
	}

	vol_bal_timeout_tag = aqualung_timeout_add(1000, vol_bal_timeout_callback, NULL);

	g_signal_emit_by_name(G_OBJECT(slider_widget), "move-slider",
			      scroll_type, NULL);
}

gint
main_window_key_pressed(GtkWidget * widget, GdkEventKey * event) {

        int playlist_tabs = gtk_notebook_get_n_pages(GTK_NOTEBOOK(playlist_notebook));

	if (custom_main_keybinding_expected) {
		switch (event->keyval) {
		case GDK_KEY_Shift_L:
		case GDK_KEY_Shift_R:
		case GDK_KEY_Control_L:
		case GDK_KEY_Control_R:
		case GDK_KEY_Alt_L:
		case GDK_KEY_Alt_R:
		case GDK_KEY_Super_L:
		case GDK_KEY_Super_R:
			return FALSE;
		default:
			run_custom_main_keybinding(gdk_keyval_name(event->keyval), event->state);
			custom_main_keybinding_expected = 0;
			return TRUE;
		}
	}

	switch (event->keyval) {
	case GDK_KEY_KP_Divide:
	case GDK_KEY_slash:
                if (event->state & GDK_MOD1_MASK) {  /* ALT + KP_Divide */
			change_volume_or_balance(scale_bal, GTK_SCROLL_STEP_BACKWARD);
		} else {
			change_volume_or_balance(scale_vol, GTK_SCROLL_STEP_BACKWARD);
		}
		return TRUE;
	case GDK_KEY_KP_Multiply:
	case GDK_KEY_asterisk:
                if (event->state & GDK_MOD1_MASK) {  /* ALT + KP_Multiply */
			change_volume_or_balance(scale_bal, GTK_SCROLL_STEP_FORWARD);
		} else {
			change_volume_or_balance(scale_vol, GTK_SCROLL_STEP_FORWARD);
		}
		return TRUE;
	case GDK_KEY_Right:
		move_song_position_slider(GTK_SCROLL_STEP_FORWARD);
		return TRUE;
	case GDK_KEY_Left:
		move_song_position_slider(GTK_SCROLL_STEP_BACKWARD);
		return TRUE;
	case GDK_KEY_b:
	case GDK_KEY_B:
	case GDK_KEY_period:
	case GDK_KEY_AudioNext:
		next_event(NULL, NULL, NULL);
		return TRUE;
	case GDK_KEY_z:
	case GDK_KEY_Z:
		if (event->state & GDK_CONTROL_MASK) {
			custom_main_keybinding_expected = 1;
			return TRUE;
		}
	case GDK_KEY_y:
	case GDK_KEY_Y:
	case GDK_KEY_comma:
	case GDK_KEY_AudioPrev:
		prev_event(NULL, NULL, NULL);
		return TRUE;
	case GDK_KEY_s:
	case GDK_KEY_S:
                if (event->state & GDK_MOD1_MASK) {  /* ALT + s */
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(musicstore_toggle),
						     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(musicstore_toggle)));
		} else {
			if (!(event->state & GDK_CONTROL_MASK)) {
				stop_event(NULL, NULL, NULL);
			} else {
				toggle_stop_after_current_song();
			}
		}
		return TRUE;
	case GDK_KEY_v:
	case GDK_KEY_V:
		if (!(event->state & GDK_CONTROL_MASK)) {
			stop_event(NULL, NULL, NULL);
			return TRUE;
		}
		break;
	case GDK_KEY_AudioStop:
		stop_event(NULL, NULL, NULL);
		return TRUE;
	case GDK_KEY_c:
	case GDK_KEY_C:
	case GDK_KEY_space:
		if (!(event->state & GDK_CONTROL_MASK)) {
			if (!options.combine_play_pause) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button),
		                !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pause_button)));
			} else if (is_file_loaded) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
		                !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button)));
			}
			return TRUE;
		}
		break;
	case GDK_KEY_p:
	case GDK_KEY_P:
	case GDK_KEY_AudioPlay:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
					     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button)));
		return TRUE;
	case GDK_KEY_i:
	case GDK_KEY_I:
		if (options.playlist_is_embedded) {
			return playlist_window_key_pressed(widget, event);
		} else {
			conf__fileinfo_cb(NULL);
		}
		break;
	case GDK_KEY_BackSpace:
		if (allow_seeks && total_samples != 0) {
			seek_t seek;

			send_cmd = CMD_SEEKTO;
			seek.seek_to_pos = 0.0f;
			rb_write(rb_gui2disk, &send_cmd, 1);
			rb_write(rb_gui2disk, (char *)&seek, sizeof(seek_t));
			try_waking_disk_thread();
			refresh_scale_suppress = 2;
		}

		if ((!options.combine_play_pause) && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pause_button))) {
			gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_pos), 0.0f);
		}
		return TRUE;
	case GDK_KEY_l:
	case GDK_KEY_L:
                if (event->state & GDK_MOD1_MASK) {  /* ALT + l */
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle),
						     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(playlist_toggle)));
		}
		return TRUE;
	case GDK_KEY_x:
	case GDK_KEY_X:
		if (event->state & GDK_CONTROL_MASK) {
			break;
		}
                if (event->state & GDK_MOD1_MASK) {  /* ALT + x */
#ifdef HAVE_LADSPA
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plugin_toggle),
						     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(plugin_toggle)));
#endif /* HAVE_LADSPA */
		} else {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
						     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button)));
		}
		return TRUE;
	case GDK_KEY_q:
	case GDK_KEY_Q:
                if (event->state & GDK_CONTROL_MASK) {  /* CTRL + q */
			main_window_closing();
		}
		return TRUE;
	case GDK_KEY_k:
	case GDK_KEY_K:
                if (!options.disable_skin_support_settings) {
                	create_skin_window();
                }
                return TRUE;
	case GDK_KEY_o:
	case GDK_KEY_O:
                create_options_window();
                return TRUE;
	case GDK_KEY_1:
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
	case GDK_KEY_2:
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
	case GDK_KEY_3:
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
	case GDK_KEY_4:
	case GDK_KEY_5:
	case GDK_KEY_6:
	case GDK_KEY_7:
	case GDK_KEY_8:
	case GDK_KEY_9:
	case GDK_KEY_0:
                if (event->state & GDK_MOD1_MASK) {  /* ALT */

                        int val = event->keyval - GDK_KEY_0;
                        val = (val == 0) ? 10 : val;

                        if(playlist_tabs >= val) {
                                gtk_notebook_set_current_page(GTK_NOTEBOOK(playlist_notebook), val-1);
                        }
                }
                return TRUE;
#ifdef HAVE_SYSTRAY
	case GDK_KEY_Escape:
		if (systray_used) {
			hide_all_windows(NULL);
		}
		return TRUE;
#endif /* HAVE_SYSTRAY */

#ifdef HAVE_LOOP
	case GDK_KEY_less:
		if (options.repeat_on && is_file_loaded) {
			float pos = (total_samples > 0) ? ((double)sample_pos / total_samples) : 0;
			aqualung_loop_bar_adjust_start(AQUALUNG_LOOP_BAR(loop_bar), pos);
		}
		return TRUE;
	case GDK_KEY_greater:
		if (options.repeat_on && is_file_loaded) {
			float pos = (total_samples > 0) ? ((double)sample_pos / total_samples) : 1;
			aqualung_loop_bar_adjust_end(AQUALUNG_LOOP_BAR(loop_bar), pos);
		}
		return TRUE;
#endif /* HAVE_LOOP */
	}

        if (options.playlist_is_embedded) {
		playlist_window_key_pressed(widget, event);
        }

	return FALSE;
}


gint
main_window_key_released(GtkWidget * widget, GdkEventKey * event) {

	switch (event->keyval) {
        case GDK_KEY_Right:
        case GDK_KEY_Left:
		seek_song();
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
#ifdef HAVE_LADSPA
		if (fxbuilder_on) {
			gtk_window_deiconify(GTK_WINDOW(fxbuilder_window));
		}
#endif /* HAVE_LADSPA */
	}
        return FALSE;
}


gint
main_window_button_pressed(GtkWidget * widget, GdkEventButton * event, gpointer data) {

	if (event->button == 3) {
		if (options.playlist_is_embedded) {
			/* translate and crop event coordinates over playlist widget */
			GtkAllocation allocation;
			gtk_widget_get_allocation(playlist_window, &allocation);
			event->x -= allocation.x;
			if (event->x > allocation.width) {
				event->x = -1;
			}
			event->y -= allocation.y;
			if (event->y > allocation.height) {
				event->y = -1;
			}
			return playlist_window_button_pressed(widget, event, playlist_get_current());
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


void
changed_pos(GtkAdjustment * adj, gpointer data) {
	static int pos = -1;

	if (!is_file_loaded) {
		gtk_adjustment_set_value(adj, 0.0f);
	}

        if (options.enable_tooltips) {
		int newpos = (int)gtk_adjustment_get_value(adj);
		if (pos != newpos) {
			char str[32];
			arr_snprintf(str, _("Position: %d%%"), newpos);
			aqualung_widget_set_tooltip_text(scale_pos, str);
			pos = newpos;
		}
        }
}


gint
scale_vol_button_press_event(GtkWidget * widget, GdkEventButton * event) {

	char str[32];
	options.vol = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_vol));

        if (event->state & GDK_SHIFT_MASK) {  /* SHIFT */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_vol), 0);
		return TRUE;
	}

	if (options.vol < -40.5f) {
		arr_snprintf(str, _("Mute"));
	} else {
		arr_snprintf(str, _("%d dB"), (int)options.vol);
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

	char str[32];
	char str2[32];

	options.vol = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_vol));
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_vol), options.vol);

        if (options.vol < -40.5f) {
                arr_snprintf(str, _("Mute"));
        } else {
                arr_snprintf(str, _("%d dB"), (int)options.vol);
        }

        if (!refresh_time_label) {
		gtk_label_set_text(GTK_LABEL(time_labels[options.time_idx[0]]), str);
        }

        arr_snprintf(str2, _("Volume: %s"), str);
        aqualung_widget_set_tooltip_text(scale_vol, str2);
}


gint
scale_vol_button_release_event(GtkWidget * widget, GdkEventButton * event) {

	refresh_time_label = 1;
	refresh_time_displays();

	return FALSE;
}


gint
scale_bal_button_press_event(GtkWidget * widget, GdkEventButton * event) {

	char str[32];
	options.bal = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_bal));

        if (event->state & GDK_SHIFT_MASK) {  /* SHIFT */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_bal), 0);
		return TRUE;
	}

	if (options.bal != 0.0f) {
		if (options.bal > 0.0f) {
			arr_snprintf(str, _("%d%% R"), (int)options.bal);
		} else {
			arr_snprintf(str, _("%d%% L"), -1*(int)options.bal);
		}
	} else {
		arr_snprintf(str, _("C"));
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

	char str[32];
	char str2[32];

	options.bal = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_bal));
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_bal), options.bal);

        if (options.bal != 0.0f) {
                if (options.bal > 0.0f) {
                        arr_snprintf(str, _("%d%% R"), (int)options.bal);
                } else {
                        arr_snprintf(str, _("%d%% L"), -1*(int)options.bal);
                }
        } else {
                arr_snprintf(str, _("C"));
        }

        if (!refresh_time_label) {
		gtk_label_set_text(GTK_LABEL(time_labels[options.time_idx[0]]), str);
	}

        arr_snprintf(str2, _("Balance: %s"), str);
        aqualung_widget_set_tooltip_text(scale_bal, str2);
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

	playlist_data_t * data;

	gtk_tree_model_get(GTK_TREE_MODEL(store), piter, PL_COL_DATA, &data, -1);
	cue->filename = strdup(data->file);
	cue->voladj = options.rva_is_enabled ? data->voladj : 0.0f;
	arr_strlcpy(current_file, cue->filename);
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
	GtkTreeIter iter = *piter;

	if (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter)) {
		*piter = iter;
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), piter)) {
			get_child_iter(store, piter, 1/* first */);
		}
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
			return choose_next_track(store, piter);
		}
	}
}


/* simpler case than choose_next_track(); no support for wrap-around at end of list.
 * used by the timeout callback for track flow-through */
int
choose_adjacent_track(GtkTreeStore * store, GtkTreeIter * piter) {
	GtkTreeIter iter = *piter;

	if (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter)) {
		*piter = iter;
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), piter)) {
			get_child_iter(store, piter, 1/* first */);
		}
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
			return choose_adjacent_track(store, piter);
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
prepare_playback(playlist_t * pl, GtkTreeIter * piter, cue_t * pcue) {

	mark_track(pl, piter);
	cue_track_for_playback(pl->store, piter, pcue);
	is_file_loaded = 1;
	toggle_noeffect(PLAY, TRUE);
}


void
unprepare_playback(void) {

	is_file_loaded = 0;
	stop_after_current_song = 0;
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
			unmark_track(pl, &iter);
			if (choose_prev_track(pl->store, &iter)) {
				mark_track(pl, &iter);
			}
		} else {
			if (choose_first_track(pl->store, &iter)) {
				mark_track(pl, &iter);
			}
		}
	} else {
		/* shuffle mode */
		p = playlist_get_playing_path(pl);
		if (p != NULL) {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_path_free(p);
			unmark_track(pl, &iter);
		}
		if (choose_random_track(pl->store, &iter)) {
			mark_track(pl, &iter);
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

		stop_after_current_song = 0;
		if (is_paused) {
			is_paused = 0;
			toggle_noeffect(PAUSE, FALSE);
			toggle_noeffect(PLAY, TRUE);
		}

		cmd = CMD_CUE;
		flush_rb_disk2gui();
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
			unmark_track(pl, &iter);
			if (choose_next_track(pl->store, &iter)) {
				mark_track(pl, &iter);
			}
		} else {
			if (choose_first_track(pl->store, &iter)) {
				mark_track(pl, &iter);
			}
		}
	} else {
		/* shuffle mode */
		p = playlist_get_playing_path(pl);
		if (p != NULL) {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_path_free(p);
			unmark_track(pl, &iter);
		}
		if (choose_random_track(pl->store, &iter)) {
			mark_track(pl, &iter);
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

		stop_after_current_song = 0;
		if (is_paused) {
			is_paused = 0;
			toggle_noeffect(PAUSE, FALSE);
			toggle_noeffect(PLAY, TRUE);
		}

		cmd = CMD_CUE;
		flush_rb_disk2gui();
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
		if (!options.combine_play_pause) {
			toggle_noeffect(PAUSE, FALSE);
		}
		send_cmd = CMD_RESUME;
		rb_write(rb_gui2disk, &send_cmd, 1);
		try_waking_disk_thread();
		return FALSE;
	}
	if (options.combine_play_pause && is_file_loaded) {
		return pause_event(widget, event, data);
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
		prepare_playback(pl, &iter, &cue);
	} else {
		if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
			/* normal or repeat mode */
			if (choose_first_track(pl->store, &iter)) {
				prepare_playback(pl, &iter, &cue);
			} else {
				unprepare_playback();
			}
		} else { /* shuffle mode */
			if (choose_random_track(pl->store, &iter)) {
				prepare_playback(pl, &iter, &cue);
			} else {
				unprepare_playback();
			}
		}
	}
	if (cue.filename == NULL) {
		stop_event(NULL, NULL, NULL);
	} else {
		flush_rb_disk2gui();
		rb_write(rb_gui2disk, &cmd, sizeof(char));
		rb_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
		try_waking_disk_thread();
	}
	return FALSE;
}


gint
pause_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

	if ((!allow_seeks) || (!is_file_loaded)) {
		if (!options.combine_play_pause) {
			toggle_noeffect(PAUSE, FALSE);
		}
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
	stop_after_current_song = 0;
	current_file[0] = '\0';
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_pos), 0.0f);
	zero_displays();
	toggle_noeffect(PLAY, FALSE);
	if (!options.combine_play_pause) {
		toggle_noeffect(PAUSE, FALSE);
	}

	is_paused = 0;
	allow_seeks = 1;

	cmd = CMD_CUE;
	cue.filename = NULL;
	flush_rb_disk2gui();
        rb_write(rb_gui2disk, &cmd, sizeof(char));
        rb_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
	try_waking_disk_thread();

 	show_scale_pos(TRUE);

        /* hide cover */
        hide_cover_thumbnail();

	if (embedded_picture != NULL) {
		free(embedded_picture);
		embedded_picture = NULL;
		embedded_picture_size = 0;
	}

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

	if (stop_after_current_song) {
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
			unmark_track(pl, &iter);
			if (choose_adjacent_track(pl->store, &iter)) {
				prepare_playback(pl, &iter, pcue);
			} else {
				if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_all_button))) {
					/* normal mode */
					allow_seeks = 1;
					changed_pos(GTK_ADJUSTMENT(adj_pos), NULL);
					unprepare_playback();
				} else {
					/* list repeat mode */
					if (choose_first_track(pl->store, &iter)) {
						prepare_playback(pl, &iter, pcue);
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
			prepare_playback(pl, &iter, pcue);
		} else {
			/* shuffle mode */
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_path_free(p);
			unmark_track(pl, &iter);
			if (choose_random_track(pl->store, &iter)) {
				prepare_playback(pl, &iter, pcue);
			} else {
				unprepare_playback();
			}
		}
	} else { /* no marked track in playlist */
		if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
			/* normal or repeat mode */
			if (choose_first_track(pl->store, &iter)) {
				prepare_playback(pl, &iter, pcue);
			} else {
				allow_seeks = 1;
				changed_pos(GTK_ADJUSTMENT(adj_pos), NULL);
				unprepare_playback();
			}
		} else { /* shuffle mode */
			if (choose_random_track(pl->store, &iter)) {
				prepare_playback(pl, &iter, pcue);
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
		GtkRange * range = GTK_RANGE(gtk_scrolled_window_get_hscrollbar(GTK_SCROLLED_WINDOW(win)));
		g_signal_emit_by_name(G_OBJECT(range), "move-slider", GTK_SCROLL_STEP_RIGHT, &dummy);
		x_scroll_start = event->x;
		gdk_window_set_cursor(gtk_widget_get_parent_window(GTK_WIDGET(win)),
				      gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW));
	}

	if (dx > 10) {
		GtkRange * range = GTK_RANGE(gtk_scrolled_window_get_hscrollbar(GTK_SCROLLED_WINDOW(win)));
		g_signal_emit_by_name(G_OBJECT(range), "move-slider", GTK_SCROLL_STEP_LEFT, &dummy);
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


void
button_set_content(GtkWidget * button, char * imgpath, char * alt) {

        GdkPixbuf * pixbuf;
	GtkWidget * widget;

	if ((widget = gtk_bin_get_child(GTK_BIN(button))) != NULL) {
		gtk_widget_destroy(widget);
	}

	if ((pixbuf = gdk_pixbuf_new_from_file(imgpath, NULL)) != NULL) {
		widget = gtk_image_new_from_pixbuf(pixbuf);
	} else {
		widget = gtk_label_new(alt);
	}

	gtk_container_add(GTK_CONTAINER(button), widget);
	gtk_widget_show(widget);
}

void
main_buttons_set_content(char * skin_path) {

	char path[MAXLEN];

	arr_snprintf(path, "%s/%s", skin_path, "prev.png");
	button_set_content(prev_button, path, "prev");
	arr_snprintf(path, "%s/%s", skin_path, "stop.png");
	button_set_content(stop_button, path, "stop");
	arr_snprintf(path, "%s/%s", skin_path, "next.png");
	button_set_content(next_button, path, "next");
	if (options.combine_play_pause) {
		arr_snprintf(path, "%s/%s", skin_path, "play_pause.png");
		button_set_content(play_button, path, "play/pause");
	} else {
		arr_snprintf(path, "%s/%s", skin_path, "play.png");
		button_set_content(play_button, path, "play");
		arr_snprintf(path, "%s/%s", skin_path, "pause.png");
		button_set_content(pause_button, path, "pause");
	}
	arr_snprintf(path, "%s/%s", skin_path, "repeat.png");
	button_set_content(repeat_button, path, "repeat");
	arr_snprintf(path, "%s/%s", skin_path, "repeat_all.png");
	button_set_content(repeat_all_button, path, "rep_all");
	arr_snprintf(path, "%s/%s", skin_path, "shuffle.png");
	button_set_content(shuffle_button, path, "shuffle");

	arr_snprintf(path, "%s/%s", skin_path, "pl.png");
	button_set_content(playlist_toggle, path, "PL");
	arr_snprintf(path, "%s/%s", skin_path, "ms.png");
	button_set_content(musicstore_toggle, path, "MS");
#ifdef HAVE_LADSPA
	arr_snprintf(path, "%s/%s", skin_path, "fx.png");
	button_set_content(plugin_toggle, path, "FX");
#endif /* HAVE_LADSPA */
}


#ifdef HAVE_JACK
void
jack_shutdown_window(void) {

	message_dialog(_("JACK connection lost"),
		       main_window,
		       GTK_MESSAGE_ERROR,
		       GTK_BUTTONS_OK,
		       NULL,
		       _("JACK has either been shutdown or it "
			 "disconnected Aqualung. Reason: %s"),
		       jack_shutdown_reason);
}
#endif /* HAVE_JACK */


void
set_win_title(void) {

	char str_session_id[32];

	arr_strlcpy(win_title, "Aqualung");
	if (aqualung_session_id > 0) {
		arr_snprintf(str_session_id, ".%d", aqualung_session_id);
		arr_strlcat(win_title, str_session_id);
	}
#ifdef HAVE_JACK
	if ((output == JACK_DRIVER) && (strcmp(client_name, "aqualung") != 0)) {
		arr_strlcat(win_title, " [");
		arr_strlcat(win_title, client_name);
		arr_strlcat(win_title, "]");
	}
#endif /* HAVE_JACK */
}


#ifdef HAVE_SYSTRAY
void
wm_systray_warn_cb(GtkWidget * widget, gpointer data) {

	set_option_from_toggle(widget, &options.wm_systray_warn);
}

void
hide_all_windows(gpointer data) {

	if (wm_not_systray_capable) {
		main_window_closing();
		return;
	}

	if (gtk_status_icon_is_embedded(systray_icon) == FALSE) {

		if (!wm_not_systray_capable && options.wm_systray_warn) {

			GtkWidget * check = gtk_check_button_new_with_label(_("Warn me if the Window Manager does not support system tray"));

			gtk_widget_set_name(check, "check_on_window");
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), TRUE);
			g_signal_connect(G_OBJECT(check), "toggled",
					 G_CALLBACK(wm_systray_warn_cb), NULL);

			message_dialog(_("Warning"),
				       options.playlist_is_embedded ? main_window : playlist_window,
				       GTK_MESSAGE_WARNING,
				       GTK_BUTTONS_OK,
				       check,
				       _("Aqualung is compiled with system tray support, but "
					 "the status icon could not be embedded in the notification "
					 "area. Your desktop may not have support for a system tray, "
					 "or it has not been configured correctly."));

			wm_not_systray_capable = 1;
		} else {
			main_window_closing();
		}

		return;

	} else {
		options.wm_systray_warn = 1;
	}

	if (!systray_main_window_on) {
		return;
	}

	systray_main_window_on = 0;

	save_window_position();
	toplevel_window_foreach(TOP_WIN_TRAY, gtk_widget_hide);

	gtk_widget_hide(systray__hide);
	gtk_widget_show(systray__show);
}

void
show_all_windows(gpointer data) {

	systray_main_window_on = 1;

	toplevel_window_foreach(TOP_WIN_TRAY, gtk_widget_show);
	restore_window_position();

	gtk_widget_hide(systray__show);
	gtk_widget_show(systray__hide);
}
#endif /* HAVE_SYSTRAY */


gboolean
cover_press_button_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data) {

        GtkTreePath * p;
	GtkTreeIter iter;
	playlist_t * pl;

	if ((pl = playlist_get_playing()) == NULL) {
		return FALSE;
	}

	if (event->type == GDK_BUTTON_PRESS && event->button == 1) { /* LMB ? */
		p = playlist_get_playing_path(pl);
		if (p != NULL) {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_path_free(p);
			playlist_data_t * pldata;
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_DATA, &pldata, -1);
			if (is_file_loaded) {
				if (embedded_picture != NULL && (find_cover_filename(pldata->file) == NULL || !options.use_external_cover_first)) {
					display_zoomed_cover_from_binary(main_window, c_event_box, embedded_picture, embedded_picture_size);
				} else {
					display_zoomed_cover(main_window, c_event_box, pldata->file);
				}
			}
		}
        }
        return TRUE;
}

void
main_window_set_font(int cond) {

        if (cond) {
		gtk_widget_modify_font(bigtimer_label, fd_bigtimer);
		gtk_widget_modify_font(smalltimer_label_1, fd_smalltimer);
		gtk_widget_modify_font(smalltimer_label_2, fd_smalltimer);
		gtk_widget_modify_font(label_title, fd_songtitle);
		gtk_widget_modify_font(label_input, fd_songinfo);
		gtk_widget_modify_font(label_output, fd_songinfo);
		gtk_widget_modify_font(label_src_type, fd_songinfo);
	}
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

	GtkWidget * time_grid;
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

	GtkWidget * sr_grid;

        char path[MAXLEN];

	set_win_title();
	playing_app_title[0] = '\0';
	
	main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_name(main_window, "main_window");
	register_toplevel_window(main_window, TOP_WIN_SKIN | TOP_WIN_TRAY);
	gtk_window_set_title(GTK_WINDOW(main_window), win_title);
#ifdef HAVE_SYSTRAY
	if (systray_used) {
		aqualung_status_icon_set_tooltip_text(systray_icon, win_title);
	}
#endif /* HAVE_SYSTRAY */

        g_signal_connect(G_OBJECT(main_window), "event", G_CALLBACK(main_window_event), NULL);

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

        conf_menu = gtk_menu_new();
	register_toplevel_window(conf_menu, TOP_WIN_SKIN);

        if (options.playlist_is_embedded) {
                init_plist_menu(conf_menu);
                plist_menu = conf_menu;
        }

        conf__options = gtk_menu_item_new_with_label(_("Settings"));
        conf__skin = gtk_menu_item_new_with_label(_("Skin chooser"));
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

#ifdef HAVE_JACK_MGMT
	if (output == JACK_DRIVER) {
		conf__jack = gtk_menu_item_new_with_label(_("JACK port setup"));
		gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__jack);
		g_signal_connect_swapped(G_OBJECT(conf__jack), "activate",
					 G_CALLBACK(conf__jack_cb), NULL);
		gtk_widget_show(conf__jack);
	}
#endif /* HAVE_JACK_MGMT */

        if (!options.playlist_is_embedded) {
                gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__fileinfo);
                gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__separator1);
	}
        gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__about);
	gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__separator2);
        gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__quit);

        g_signal_connect_swapped(G_OBJECT(conf__options), "activate", G_CALLBACK(conf__options_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(conf__skin), "activate", G_CALLBACK(conf__skin_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(conf__about), "activate", G_CALLBACK(conf__about_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(conf__quit), "activate", G_CALLBACK(conf__quit_cb), NULL);

        if (!options.playlist_is_embedded) {
                g_signal_connect_swapped(G_OBJECT(conf__fileinfo), "activate", G_CALLBACK(conf__fileinfo_cb), NULL);
                gtk_widget_show(conf__fileinfo);
	}

        gtk_widget_show(conf__options);
        if (!options.disable_skin_support_settings) {
                gtk_widget_show(conf__skin);
        }
        gtk_widget_show(conf__separator1);
        gtk_widget_show(conf__about);
        gtk_widget_show(conf__separator2);
        gtk_widget_show(conf__quit);


	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(main_window), vbox);


	disp_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_box_pack_start(GTK_BOX(vbox), disp_hbox, FALSE, FALSE, 0);


	time_grid = gtk_grid_new();
	disp_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	gtk_box_pack_start(GTK_BOX(disp_hbox), time_grid, FALSE, FALSE, 0);
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


	gtk_grid_attach(GTK_GRID(time_grid), time0_viewp, 0, 0, 2, 1);
	gtk_widget_set_vexpand(time0_viewp, TRUE);
	gtk_widget_set_vexpand(time_grid, FALSE);
	gtk_grid_attach(GTK_GRID(time_grid), time1_viewp, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(time_grid), time2_viewp, 1, 1, 1, 1);


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

	info_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
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

	title_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_set_border_width(GTK_CONTAINER(title_hbox), 1);
	gtk_container_add(GTK_CONTAINER(title_viewp), title_hbox);

	gtk_box_pack_start(GTK_BOX(disp_vbox), title_scrolledwin, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(disp_vbox), info_scrolledwin, TRUE, FALSE, 0);

	/* labels */
	bigtimer_label = time_labels[options.time_idx[0]] = gtk_label_new("");

        gtk_widget_set_name(time_labels[options.time_idx[0]], "big_timer_label");
	gtk_container_add(GTK_CONTAINER(time0_viewp), time_labels[options.time_idx[0]]);

	time_hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(time_hbox1), 2);
	gtk_container_add(GTK_CONTAINER(time1_viewp), time_hbox1);

	smalltimer_label_1 = time_labels[options.time_idx[1]] = gtk_label_new("");

        gtk_widget_set_name(time_labels[options.time_idx[1]], "small_timer_label");
	gtk_box_pack_start(GTK_BOX(time_hbox1), time_labels[options.time_idx[1]], TRUE, TRUE, 0);

	time_hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(time_hbox2), 2);
	gtk_container_add(GTK_CONTAINER(time2_viewp), time_hbox2);

	smalltimer_label_2 = time_labels[options.time_idx[2]] = gtk_label_new("");

        gtk_widget_set_name(time_labels[options.time_idx[2]], "small_timer_label");
	gtk_box_pack_start(GTK_BOX(time_hbox2), time_labels[options.time_idx[2]], TRUE, TRUE, 0);

	label_title = gtk_label_new("");
        gtk_widget_set_name(label_title, "label_title");
	gtk_box_pack_start(GTK_BOX(title_hbox), label_title, FALSE, FALSE, 3);

	label_input = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(info_hbox), label_input, FALSE, FALSE, 3);
	gtk_widget_set_name(label_input, "label_info");

	gtk_box_pack_start(GTK_BOX(info_hbox), gtk_vseparator_new(), FALSE, FALSE, 3);

	label_output = gtk_label_new("");
	gtk_widget_set_name(label_output, "label_info");
	gtk_box_pack_start(GTK_BOX(info_hbox), label_output, FALSE, FALSE, 3);

	gtk_box_pack_start(GTK_BOX(info_hbox), gtk_vseparator_new(), FALSE, FALSE, 3);

	label_src_type = gtk_label_new("");
	gtk_widget_set_name(label_src_type, "label_info");
	gtk_box_pack_start(GTK_BOX(info_hbox), label_src_type, FALSE, FALSE, 3);

	main_window_set_font(options.override_skin_settings);

	/* Volume and balance slider */
	vb_table = gtk_table_new(1, 3, FALSE);
        gtk_table_set_col_spacings(GTK_TABLE(vb_table), 3);
	gtk_box_pack_start(GTK_BOX(disp_vbox), vb_table, TRUE, FALSE, 0);

	adj_vol = gtk_adjustment_new(0.0f, -41.0f, 6.0f, 1.0f, 3.0f, 0.0f);
	g_signal_connect(G_OBJECT(adj_vol), "value_changed", G_CALLBACK(changed_vol), NULL);
	scale_vol = gtk_hscale_new(GTK_ADJUSTMENT(adj_vol));
	gtk_widget_set_name(scale_vol, "scale_vol");
	g_signal_connect(G_OBJECT(scale_vol), "button_press_event",
			 G_CALLBACK(scale_vol_button_press_event), NULL);
	g_signal_connect(G_OBJECT(scale_vol), "button_release_event",
			 G_CALLBACK(scale_vol_button_release_event), NULL);
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
	g_signal_connect(G_OBJECT(scale_bal), "button_press_event",
			 G_CALLBACK(scale_bal_button_press_event), NULL);
	g_signal_connect(G_OBJECT(scale_bal), "button_release_event",
			 G_CALLBACK(scale_bal_button_release_event), NULL);
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
	g_signal_connect(G_OBJECT(scale_pos), "button_press_event",
			 G_CALLBACK(scale_button_press_event), NULL);
	g_signal_connect(G_OBJECT(scale_pos), "button_release_event",
			 G_CALLBACK(scale_button_release_event), NULL);
	gtk_scale_set_digits(GTK_SCALE(scale_pos), 0);
        gtk_scale_set_draw_value(GTK_SCALE(scale_pos), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), scale_pos, FALSE, FALSE, 3);

        vbox_sep = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), vbox_sep, FALSE, FALSE, 2);

	gtk_widget_set_can_focus(scale_vol, FALSE);
	gtk_widget_set_can_focus(scale_bal, FALSE);
	gtk_widget_set_can_focus(scale_pos, FALSE);

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
		playlist_window = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_set_name(playlist_window, "playlist_window");
		gtk_box_pack_start(GTK_BOX(vbox), playlist_window, TRUE, TRUE, 3);
	}

        /* Button box with prev, play, pause, stop, next buttons */

	btns_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), btns_hbox, FALSE, FALSE, 0);

	prev_button = gtk_button_new();
        aqualung_widget_set_tooltip_text(prev_button, _("Previous song"));

	stop_button = gtk_button_new();
        aqualung_widget_set_tooltip_text(stop_button, _("Stop"));

	next_button = gtk_button_new();
        aqualung_widget_set_tooltip_text(next_button, _("Next song"));

	play_button = gtk_toggle_button_new();

	if (options.combine_play_pause) {
		aqualung_widget_set_tooltip_text(play_button, _("Play/Pause"));
	} else {
		aqualung_widget_set_tooltip_text(play_button, _("Play"));
		pause_button = gtk_toggle_button_new();
		aqualung_widget_set_tooltip_text(pause_button, _("Pause"));
	}

	gtk_widget_set_can_focus(prev_button, FALSE);
	gtk_widget_set_can_focus(stop_button, FALSE);
	gtk_widget_set_can_focus(next_button, FALSE);
	gtk_widget_set_can_focus(play_button, FALSE);
	if (!options.combine_play_pause) {
		gtk_widget_set_can_focus(pause_button, FALSE);
	}

	gtk_box_pack_start(GTK_BOX(btns_hbox), prev_button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(btns_hbox), play_button, FALSE, FALSE, 0);
	if (!options.combine_play_pause) {
		gtk_box_pack_start(GTK_BOX(btns_hbox), pause_button, FALSE, FALSE, 0);
	}
	gtk_box_pack_start(GTK_BOX(btns_hbox), stop_button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(btns_hbox), next_button, FALSE, FALSE, 0);

	g_signal_connect(prev_button, "clicked", G_CALLBACK(prev_event), NULL);
	play_id = g_signal_connect(play_button, "toggled", G_CALLBACK(play_event), NULL);
	if (!options.combine_play_pause) {
		pause_id = g_signal_connect(pause_button, "toggled", G_CALLBACK(pause_event), NULL);
	}
	g_signal_connect(stop_button, "clicked", G_CALLBACK(stop_event), NULL);
	g_signal_connect(next_button, "clicked", G_CALLBACK(next_event), NULL);


	/* toggle buttons for shuffle and repeat */
	sr_grid = gtk_grid_new();

	repeat_button = gtk_toggle_button_new();
        aqualung_widget_set_tooltip_text(repeat_button, _("Repeat current song"));
	gtk_grid_attach(GTK_GRID(sr_grid), repeat_button, 0, 0, 1, 1);
	g_signal_connect(repeat_button, "toggled", G_CALLBACK(repeat_toggled), NULL);

	repeat_all_button = gtk_toggle_button_new();
        aqualung_widget_set_tooltip_text(repeat_all_button, _("Repeat all songs"));
	gtk_grid_attach(GTK_GRID(sr_grid), repeat_all_button, 1, 0, 1, 1);
	g_signal_connect(repeat_all_button, "toggled", G_CALLBACK(repeat_all_toggled), NULL);

	shuffle_button = gtk_toggle_button_new();
        aqualung_widget_set_tooltip_text(shuffle_button, _("Shuffle songs"));
	gtk_grid_attach(GTK_GRID(sr_grid), shuffle_button, 2, 0, 1, 1);
	g_signal_connect(shuffle_button, "toggled", G_CALLBACK(shuffle_toggled), NULL);

	gtk_widget_set_can_focus(repeat_button, FALSE);
	gtk_widget_set_can_focus(repeat_all_button, FALSE);
	gtk_widget_set_can_focus(shuffle_button, FALSE);

        /* toggle buttons for sub-windows visibility */
	playlist_toggle = gtk_toggle_button_new();
        aqualung_widget_set_tooltip_text(playlist_toggle, _("Toggle playlist"));
	gtk_box_pack_end(GTK_BOX(btns_hbox), playlist_toggle, FALSE, FALSE, 0);

	musicstore_toggle = gtk_toggle_button_new();
        aqualung_widget_set_tooltip_text(musicstore_toggle, _("Toggle music store"));
	gtk_box_pack_end(GTK_BOX(btns_hbox), musicstore_toggle, FALSE, FALSE, 3);

	g_signal_connect(playlist_toggle, "toggled", G_CALLBACK(playlist_toggled), NULL);
	g_signal_connect(musicstore_toggle, "toggled", G_CALLBACK(musicstore_toggled), NULL);

	gtk_widget_set_can_focus(playlist_toggle, FALSE);
	gtk_widget_set_can_focus(musicstore_toggle, FALSE);

#ifdef HAVE_LADSPA
	plugin_toggle = gtk_toggle_button_new();
	aqualung_widget_set_tooltip_text(plugin_toggle, _("Toggle LADSPA patch builder"));
	gtk_box_pack_end(GTK_BOX(btns_hbox), plugin_toggle, FALSE, FALSE, 0);
	g_signal_connect(plugin_toggle, "toggled", G_CALLBACK(plugin_toggled), NULL);
	gtk_widget_set_can_focus(plugin_toggle, FALSE);
#endif /* HAVE_LADSPA */

	gtk_box_pack_end(GTK_BOX(btns_hbox), sr_grid, FALSE, FALSE, 3);

        if (options.disable_skin_support_settings) {
	        arr_snprintf(path, "%s/no_skin", AQUALUNG_SKINDIR);
	        main_buttons_set_content(path);
        } else {
	        main_buttons_set_content(skin_path);
        }
        set_buttons_relief();

	/* Embedded playlist */
	if (options.playlist_is_embedded && !options.buttons_at_the_bottom) {
		playlist_window = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_set_name(playlist_window, "playlist_window");
		gtk_box_pack_start(GTK_BOX(vbox), playlist_window, TRUE, TRUE, 3);
	}

	aqualung_tooltips_set_enabled(options.enable_tooltips);
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

GtkScrollType
reverse_direction(GtkScrollType scroll_type) {
	switch (scroll_type) {
	case GTK_SCROLL_STEP_BACKWARD:
		return GTK_SCROLL_STEP_FORWARD;
	case GTK_SCROLL_STEP_FORWARD:
		return GTK_SCROLL_STEP_BACKWARD;
	default:
		return scroll_type;
	}
}

gboolean
systray_mouse_wheel(int mouse_wheel_option, gboolean vertical_wheel,
       	guint32 time_since_last_event, GtkScrollType scroll_type) {

	switch (mouse_wheel_option) {
	case SYSTRAY_MW_CMD_VOLUME:
		if (time_since_last_event > 100) {
			if (vertical_wheel)
				scroll_type = reverse_direction(scroll_type);
			change_volume_or_balance(scale_vol, scroll_type);
			return TRUE;
		}
		break;
	case SYSTRAY_MW_CMD_BALANCE:
		if (time_since_last_event > 100) {
			if (vertical_wheel)
				scroll_type = reverse_direction(scroll_type);
			change_volume_or_balance(scale_bal, scroll_type);
			return TRUE;
		}
		break;
	case SYSTRAY_MW_CMD_SONG_POSITION:
		move_song_position_slider(scroll_type);
		seek_song();
		return TRUE;
	case SYSTRAY_MW_CMD_NEXT_PREV_SONG:
		// At most one per second.
		if (time_since_last_event > 1000) {
			if (scroll_type == GTK_SCROLL_STEP_BACKWARD) {
				prev_event(NULL, NULL, NULL);
				return TRUE;
			} else if (scroll_type == GTK_SCROLL_STEP_FORWARD) {
				next_event(NULL, NULL, NULL);
				return TRUE;
			}
		}
		break;
	}

	return FALSE;
}

gboolean
systray_scroll_event_cb(GtkStatusIcon * systray_icon,
			GdkEventScroll * event,
			gpointer user_data) {

	gboolean result = FALSE;
	guint32 time_since_last_event;

	if (!options.use_systray)
		return FALSE;

	time_since_last_event = event->time - last_systray_scroll_event_time;

	switch (event->direction) {
	case GDK_SCROLL_LEFT:
		result = systray_mouse_wheel(options.systray_mouse_wheel_horizontal,
		        FALSE, time_since_last_event, GTK_SCROLL_STEP_BACKWARD);
		break;
	case GDK_SCROLL_RIGHT:
		result = systray_mouse_wheel(options.systray_mouse_wheel_horizontal,
		        FALSE, time_since_last_event, GTK_SCROLL_STEP_FORWARD);
		break;
	case GDK_SCROLL_UP:
		result = systray_mouse_wheel(options.systray_mouse_wheel_vertical,
		        TRUE, time_since_last_event, GTK_SCROLL_STEP_BACKWARD);
		break;
	case GDK_SCROLL_DOWN:
		result = systray_mouse_wheel(options.systray_mouse_wheel_vertical,
		        TRUE, time_since_last_event, GTK_SCROLL_STEP_FORWARD);
		break;
	}

	if (result) {
		last_systray_scroll_event_time = event->time;
	}

	return result;
}

gboolean
systray_mouse_button(int mouse_button_option) {

	switch (mouse_button_option) {
	case SYSTRAY_MB_CMD_PLAY_STOP_SONG:
		if (is_file_loaded) {
			systray__stop_cb(NULL);
		} else {
			systray__play_cb(NULL);
		}
		return TRUE;
	case SYSTRAY_MB_CMD_PLAY_PAUSE_SONG:
		if (is_file_loaded && !options.combine_play_pause) {
			systray__pause_cb(NULL);
		} else {
			systray__play_cb(NULL);
		}
		return TRUE;
	case SYSTRAY_MB_CMD_PREV_SONG:
		systray__prev_cb(NULL);
		return TRUE;
	case SYSTRAY_MB_CMD_NEXT_SONG:
		systray__next_cb(NULL);
		return TRUE;
	}
	return FALSE;
}

gboolean
systray_button_press_event_cb(GtkStatusIcon * systray_icon,
                              GdkEventButton * event,
                              gpointer user_data) {

	int i;

	if (!options.use_systray)
		return FALSE;

	if (event->type == GDK_BUTTON_PRESS) {
		for (i = 0; i < options.systray_mouse_buttons_count; i++) {
			if (options.systray_mouse_buttons[i].button_nr == event->button) {
				return systray_mouse_button(options.systray_mouse_buttons[i].command);
			}
		}
	}

	return FALSE;
}

/* returns a hbox with a stock image and label in it */
GtkWidget *
create_systray_menu_item(const gchar * stock_id, char * text) {

	GtkWidget * hbox;
	GtkWidget * widget;

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
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

	arr_snprintf(path, "%s/icon_64.png", AQUALUNG_DATADIR);
	systray_icon = gtk_status_icon_new_from_file(path);

        g_signal_connect_swapped(G_OBJECT(systray_icon), "activate",
				 G_CALLBACK(systray_activate_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray_icon), "popup-menu",
				 G_CALLBACK(systray_popup_menu_cb), (gpointer)systray_icon);
	g_signal_connect(G_OBJECT(systray_icon), "scroll-event",
			 G_CALLBACK(systray_scroll_event_cb), NULL);
	g_signal_connect(G_OBJECT(systray_icon), "button-press-event",
			 G_CALLBACK(systray_button_press_event_cb), NULL);

        systray_menu = gtk_menu_new();
	register_toplevel_window(systray_menu, TOP_WIN_SKIN);

        systray__show = gtk_menu_item_new_with_label(_("Show Aqualung"));
        systray__hide = gtk_menu_item_new_with_label(_("Hide Aqualung"));

        systray__separator1 = gtk_separator_menu_item_new();

        systray__play = gtk_menu_item_new();
	gtk_container_add(GTK_CONTAINER(systray__play),
			create_systray_menu_item(GTK_STOCK_MEDIA_PLAY,
			options.combine_play_pause ? _("Play/Pause") : _("Play")));

	if (!options.combine_play_pause) {
		systray__pause = gtk_menu_item_new();
		gtk_container_add(GTK_CONTAINER(systray__pause),
				  create_systray_menu_item(GTK_STOCK_MEDIA_PAUSE, _("Pause")));
	}

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
	if (!options.combine_play_pause) {
		gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__pause);
	}
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__stop);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__prev);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__next);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__separator2);
        gtk_menu_shell_append(GTK_MENU_SHELL(systray_menu), systray__quit);

        g_signal_connect_swapped(G_OBJECT(systray__show), "activate", G_CALLBACK(systray__show_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray__hide), "activate", G_CALLBACK(systray__hide_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray__play), "activate", G_CALLBACK(systray__play_cb), NULL);
        if (!options.combine_play_pause) {
		g_signal_connect_swapped(G_OBJECT(systray__pause), "activate", G_CALLBACK(systray__pause_cb), NULL);
	}
        g_signal_connect_swapped(G_OBJECT(systray__stop), "activate", G_CALLBACK(systray__stop_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray__prev), "activate", G_CALLBACK(systray__prev_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray__next), "activate", G_CALLBACK(systray__next_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(systray__quit), "activate", G_CALLBACK(systray__quit_cb), NULL);

        gtk_widget_show(systray_menu);
        gtk_widget_show(systray__hide);
        gtk_widget_show(systray__separator1);
	gtk_widget_show(systray__play);
	if (!options.combine_play_pause) {
		gtk_widget_show(systray__pause);
	}
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

	gdk_threads_enter();
	gtk_init(&argc, &argv);
#ifdef HAVE_LADSPA
	lrdf_init();
#endif /* HAVE_LADSPA */

	load_config();

	if (options.title_format[0] == '\0')
		arr_snprintf(options.title_format, "%%a: %%t [%%r]");
	if (options.skin[0] == '\0') {
		arr_snprintf(options.skin, "%s/default", AQUALUNG_SKINDIR);
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
		arr_snprintf(options.cddb_server, "gnudb.org");
	}

	if (options.src_type == -1) {
		options.src_type = 4;
	}

        if (options.disable_skin_support_settings) {
	        arr_snprintf(path, "%s/no_skin/rc", AQUALUNG_SKINDIR);
        } else {
	        arr_snprintf(path, "%s/rc", options.skin);
        }
	gtk_rc_parse(path);

#ifdef HAVE_SYSTRAY
	if (options.use_systray) {
		systray_used = 1;
		setup_systray();
	}
#endif /* HAVE_SYSTRAY */

	create_main_window(options.skin);

	vol_prev = -101.0f;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_vol), options.vol);
	bal_prev = -101.0f;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_bal), options.bal);

	create_playlist();
	playlist_auto_save_reset();

	create_music_browser();

#ifdef HAVE_LADSPA
	create_fxbuilder();
	load_plugin_data();
#endif /* HAVE_LADSPA */

#ifdef HAVE_CDDA
	create_cdda_node();
	cdda_scanner_start();
#endif /* HAVE_CDDA */

#ifdef HAVE_PODCAST
	create_podcast_node();
	store_podcast_load();
	store_podcast_updater_start();
#endif /* HAVE_PODCAST */

	music_store_load_all();

	options_store_watcher_start();

	arr_snprintf(path, "%s/icon_16.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		glist = g_list_append(glist, gdk_pixbuf_new_from_file(path, NULL));
	}

	arr_snprintf(path, "%s/icon_24.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		glist = g_list_append(glist, gdk_pixbuf_new_from_file(path, NULL));
	}

	arr_snprintf(path, "%s/icon_32.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		glist = g_list_append(glist, gdk_pixbuf_new_from_file(path, NULL));
	}

	arr_snprintf(path, "%s/icon_48.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		glist = g_list_append(glist, gdk_pixbuf_new_from_file(path, NULL));
	}

	arr_snprintf(path, "%s/icon_64.png", AQUALUNG_DATADIR);
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
	}

	if (options.playlist_on) {
		if (options.playlist_is_embedded) {
			g_signal_handlers_block_by_func(G_OBJECT(playlist_toggle), playlist_toggled, NULL);
		}
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle), TRUE);
		if (options.playlist_is_embedded) {
			g_signal_handlers_unblock_by_func(G_OBJECT(playlist_toggle), playlist_toggled, NULL);
		}
	}

	restore_window_position();

	gtk_widget_show_all(main_window);

#ifdef HAVE_SYSTRAY
	if (options.use_systray && options.systray_start_minimized) {
		toplevel_window_foreach(TOP_WIN_TRAY, gtk_widget_hide);
		systray_main_window_on = 0;
	}
#endif /* HAVE_SYSTRAY */

        hide_cover_thumbnail();

        gtk_widget_hide(vbox_sep);

	if (options.playlist_is_embedded) {
		if (!options.playlist_on) {
                        hide_playlist();
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

		arr_snprintf(file, "%s/%s", options.confdir, "playlist.xml");
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
	timeout_tag = aqualung_timeout_add(TIMEOUT_PERIOD, timeout_callback, NULL);

	/* re-apply skin to override possible WM theme */
	if (!options.disable_skin_support_settings) {
		apply_skin(options.skin);
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

	char buf[MAXLEN];
	char tmp[MAXLEN];
	playlist_t * pl;
	file_decoder_t * fdec = (file_decoder_t *)meta->fdec;
	char * artist = NULL;
	char * album = NULL;
	char * title = NULL;
	char * icy_name = NULL;
	char * icy_descr = NULL;
	meta_frame_t * frame;

	frame = metadata_get_frame_by_type(meta, META_FIELD_APIC, NULL);
	if (frame != NULL) {
		if (embedded_picture != NULL) {
			free(embedded_picture);
		}
		embedded_picture_size = frame->length;
		embedded_picture = malloc(embedded_picture_size);
		if (embedded_picture == NULL) {
			embedded_picture_size = 0;
		} else {
			memcpy(embedded_picture, frame->data, embedded_picture_size);
		}
	} else {
		if (embedded_picture != NULL) {
			free(embedded_picture);
			embedded_picture = NULL;
		}
		embedded_picture_size = 0;
	}

	if ((!httpc_is_url(fdec->filename)) && !options.use_ext_title_format) {
		return;
	}

	buf[0] = '\0';
	tmp[0] = '\0';

	// Abuse artist variable
	if ((artist = application_title_format(fdec)) != NULL) {
		arr_strlcpy(buf, artist);
		free(artist);
		artist = NULL;
	}

	if (buf[0] == '\0') {
		metadata_get_artist(meta, &artist);
		metadata_get_album(meta, &album);
		metadata_get_title(meta, &title);
		metadata_get_icy_name(meta, &icy_name);

		if ((artist && !is_all_wspace(artist)) ||
		    (album && !is_all_wspace(album)) ||
		    (title && !is_all_wspace(title))) {
			make_title_string(tmp, CHAR_ARRAY_SIZE(tmp), options.title_format,
					  artist, album, title);
			if (icy_name != NULL) {
				arr_snprintf(buf, "%s (%s)", tmp, icy_name);
			}
		} else if (icy_name != NULL) {
			arr_strlcpy(buf, icy_name);
		}
	}

	if (buf[0] != '\0') {
		set_title_label(buf);
	} else {
		set_title_label(fdec->filename);
	}

	if (icy_name == NULL) {
		return;
	}

	buf[0] = '\0';

	metadata_get_icy_descr(meta, &icy_descr);

	if (icy_descr != NULL) {
		arr_snprintf(buf, "%s (%s)", icy_name, icy_descr);
	} else {
		arr_strlcpy(buf, icy_name);
	}

	/* set playlist_str for playlist entry */
	if ((pl = playlist_get_playing()) != NULL) {
		GtkTreePath * p = playlist_get_playing_path(pl);
		if (p != NULL) {
			GtkTreeIter iter;
			playlist_data_t * data;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(pl->store), &iter, p);
			gtk_tree_model_get(GTK_TREE_MODEL(pl->store), &iter, PL_COL_DATA, &data, -1);

			free_strdup(&data->title, buf);
			gtk_tree_store_set(pl->store, &iter, PL_COL_NAME, buf, -1);

			gtk_tree_path_free(p);
		}
	}
}

void
flush_rb_disk2gui(void) {

	char recv_cmd;

	while (rb_read_space(rb_disk2gui)) {
		rb_read(rb_disk2gui, &recv_cmd, 1);
	}
}

gint
timeout_callback(gpointer data) {

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

			sample_pos = 0;
			total_samples = fileinfo.total_samples;
			status.samples_left = fileinfo.total_samples;
			status.sample_pos = 0;
			status.sample_offset = 0;
			fresh_new_file = fresh_new_file_prev = 0;
			run_hooks_fdec("track_change", fdec);
			break;

		case CMD_STATUS:
			while (rb_read_space(rb_disk2gui) < sizeof(status_t))
				;
			rb_read(rb_disk2gui, (char *)&status, sizeof(status_t));

			sample_pos = total_samples - status.samples_left;
#ifdef HAVE_LOOP
			if (options.repeat_on &&
			    (sample_pos < total_samples * options.loop_range_start ||
			     sample_pos > total_samples * options.loop_range_end)) {

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
				sample_pos = status.sample_pos - status.sample_offset;
			}

			if ((!fresh_new_file) && (sample_pos > status.sample_offset)) {
				fresh_new_file = 1;
			}

			if (fresh_new_file && !fresh_new_file_prev) {
				disp_info = fileinfo;
				disp_samples = total_samples;
				if (sample_pos > status.sample_offset) {
					disp_pos = sample_pos - status.sample_offset;
				} else {
					disp_pos = 0;
				}
				refresh_displays();
			} else {
				if (sample_pos > status.sample_offset) {
					disp_pos = sample_pos - status.sample_offset;
				} else {
					disp_pos = 0;
				}

				if (disp_info.sample_rate != status.sample_rate) {
					disp_info.sample_rate = status.sample_rate;
					refresh_displays();
				}

				if (is_file_loaded) {
					refresh_time_displays();
				}
			}

			fresh_new_file_prev = fresh_new_file;

			if (refresh_scale && !refresh_scale_suppress) {
				if (total_samples == 0) {
					gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_pos), 0.0f);
				} else {
					gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_pos),
								 100.0 * sample_pos / total_samples);
					run_hooks("track_position_change");
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
	while (((rcmd = receive_message(aqualung_socket_fd, cmdbuf, CHAR_ARRAY_SIZE(cmdbuf))) != 0) && (rcv_count < MAX_RCV_COUNT)) {
		switch (rcmd) {
		case RCMD_BACK:
			prev_event(NULL, NULL, NULL);
			break;
		case RCMD_PAUSE:
			if (!options.combine_play_pause) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button),
				        !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pause_button)));
				break;
			}
		case RCMD_PLAY:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
			     !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button)));
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
		case RCMD_CUSTOM:
			run_custom_remote_command(cmdbuf);
			break;
		case RCMD_VOLADJ:
			adjust_remote_volume(cmdbuf);
			break;
		case RCMD_QUIT:
			main_window_closing();
			break;
		default:
			fprintf(stderr, "Unrecognized remote command received: %i\n", (int)rcmd);
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
#ifdef HAVE_JACK_MGMT
				if (ports_window) {
					ports_clicked_close(NULL, NULL);
				}
				gtk_widget_set_sensitive(conf__jack, FALSE);
#endif /* HAVE_JACK_MGMT */
				jack_client_close(jack_client);
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
	gdk_threads_leave();

	return;
}


void
set_buttons_relief(void) {

	GtkWidget *rbuttons_table[] = {
		prev_button, stop_button, next_button, play_button,
		pause_button, repeat_button, repeat_all_button, shuffle_button,
#ifdef HAVE_LADSPA
		plugin_toggle,
#endif /* HAVE_LADSPA */
		playlist_toggle, musicstore_toggle
	};

	gint i, n;
        i = sizeof(rbuttons_table)/sizeof(GtkWidget*);

        for (n = 0; n < i; n++) {
		if ((rbuttons_table[n] != pause_button) || !options.combine_play_pause) {
			gtk_button_set_relief(GTK_BUTTON(rbuttons_table[n]),
				(options.disable_buttons_relief) ? GTK_RELIEF_NONE : GTK_RELIEF_NORMAL);
		}
	}
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :

