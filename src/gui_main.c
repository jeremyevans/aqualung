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
#include <math.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <lrdf.h>

#ifdef HAVE_SNDFILE
#include <sndfile.h>
#endif /* HAVE_SNDFILE */

#ifdef HAVE_SRC
#include <samplerate.h>
#endif /* HAVE_SRC */

#ifdef HAVE_MPC
#include <musepack/musepack.h>
#endif /* HAVE_MPC */


#include "common.h"
#include "core.h"
#include "transceiver.h"
#include "decoder/file_decoder.h"
#include "about.h"
#include "options.h"
#include "skin.h"
#include "ports.h"
#include "music_browser.h"
#include "playlist.h"
#include "plugin.h"
#include "file_info.h"
#include "i18n.h"
#include "gui_main.h"
/*#include "version.h"*/

/* receive at most this much remote messages in one run of timeout_callback() */
#define MAX_RCV_COUNT 32

/* period of main timeout callback [ms] */
#define TIMEOUT_PERIOD 100

char pl_color_active[14];
char pl_color_inactive[14];

char playlist_font[MAX_FONTNAME_LEN];
char browser_font[MAX_FONTNAME_LEN];
char bigtimer_font[MAX_FONTNAME_LEN];
char smalltimer_font[MAX_FONTNAME_LEN];

PangoFontDescription *fd_playlist;
PangoFontDescription *fd_browser;
PangoFontDescription *fd_bigtimer;
PangoFontDescription *fd_smalltimer;

char activesong_color[MAX_COLORNAME_LEN];

/* Communication between gui thread and disk thread */
extern pthread_mutex_t disk_thread_lock;
extern pthread_cond_t  disk_thread_wake;
extern jack_ringbuffer_t * rb_gui2disk;
extern jack_ringbuffer_t * rb_disk2gui;

extern pthread_mutex_t output_thread_lock;

extern jack_client_t * jack_client;
extern char * client_name;
extern int jack_is_shutdown;
extern const size_t sample_size;

extern int aqualung_socket_fd;
extern int aqualung_session_id;

extern GtkListStore * play_store;
extern GtkListStore * running_store;
extern GtkWidget * play_list;

/* normally $HOME/.aqualung */
char confdir[MAXLEN];
/* to keep track of file selector dialogs; starts with $HOME */
char currdir[MAXLEN];
/* current working directory when program is started */
char cwd[MAXLEN];

extern char title_format[MAXLEN];
extern char default_param[MAXLEN];
extern char skin[MAXLEN];

/* the physical name of the file that is playing, or a '\0'. */
char current_file[MAXLEN];

char send_cmd, recv_cmd;
char command[RB_CONTROL_SIZE];
fileinfo_t fileinfo;
status_t status;
unsigned long out_SR;
extern int output;
unsigned long rb_size;
unsigned long long total_samples;
unsigned long long sample_pos;

#ifdef HAVE_SRC
extern int src_type;
extern int src_type_parsed;
#else
int src_type = 0;
int src_type_parsed = 0;
#endif /* HAVE_SRC */

int immediate_start = 0; /* this flag set to 1 in core.c if --play
			  * for current instance is specified.
			  */

extern int ladspa_is_postfader;
extern int auto_save_playlist;
extern int show_rva_in_playlist;
extern int show_length_in_playlist;
extern int plcol_idx[3];
extern int auto_use_meta_artist;
extern int auto_use_meta_record;
extern int auto_use_meta_track;
extern int auto_use_ext_meta_artist;
extern int auto_use_ext_meta_record;
extern int auto_use_ext_meta_track;
extern int hide_comment_pane;
extern int hide_comment_pane_shadow;
extern int enable_playlist_statusbar;
extern int enable_playlist_statusbar_shadow;

int replaygain_tag_to_use = 0;
int rva_is_enabled = 0;
int rva_env = 0;
float rva_refvol = -12.0f;
float rva_steepness = 1.0f;
int rva_use_averaging = 1;
int rva_use_linear_thresh = 0;
float rva_avg_linear_thresh = 3.0f;
float rva_avg_stddev_thresh = 2.0f;
int enable_tooltips = 1;
int playlist_is_embedded = 0;
int playlist_is_embedded_shadow = 0;
int buttons_at_the_bottom = 1;
int override_skin_settings = 0;

/* volume & balance sliders */
double vol = 0.0f;
double vol_prev = 0.0f;
double vol_lin = 1.0f;
double bal = 0.0f;
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
extern GtkWidget * browser_paned;

int main_pos_x;
int main_pos_y;
int main_size_x;
int main_size_y;

extern int browser_pos_x;
extern int browser_pos_y;
extern int browser_size_x;
extern int browser_size_y;
extern int browser_on;
extern int browser_paned_pos;

extern int playlist_pos_x;
extern int playlist_pos_y;
extern int playlist_size_x;
extern int playlist_size_y;
extern int playlist_on;

extern int fxbuilder_on;

GtkObject * adj_pos;
GtkObject * adj_vol;
GtkObject * adj_bal;
GtkWidget * scale_pos;
GtkWidget * scale_vol;
GtkWidget * scale_bal;

GtkWidget * time_labels[3];
int time_idx[3] = { 0, 1, 2 };
int refresh_time_label = 1;


gulong play_id;
gulong pause_id;

GtkWidget * play_button;
GtkWidget * pause_button;
GtkWidget * repeat_button;
GtkWidget * repeat_all_button;
GtkWidget * shuffle_button;

int repeat_on = 0;
int repeat_all_on = 0;
int shuffle_on = 0;

GtkWidget * label_title;
GtkWidget * label_format;
GtkWidget * label_samplerate;
GtkWidget * label_bps;
GtkWidget * label_mono;
GtkWidget * label_output;
GtkWidget * label_src_type;

int alt_L;
int alt_R;
int shift_L;
int shift_R;
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
GtkWidget * conf__separator;
GtkWidget * conf__about;

GtkWidget * bigtimer_label;
GtkWidget * smalltimer_label_1;
GtkWidget * smalltimer_label_2;

void create_main_window(char * skin_path);

gint prev_event(GtkWidget * widget, GdkEvent * event, gpointer data);
gint play_event(GtkWidget * widget, GdkEvent * event, gpointer data);
gint pause_event(GtkWidget * widget, GdkEvent * event, gpointer data);
gint stop_event(GtkWidget * widget, GdkEvent * event, gpointer data);
gint next_event(GtkWidget * widget, GdkEvent * event, gpointer data);

void save_config(void);
void load_config(void);

void playlist_toggled(GtkWidget * widget, gpointer data);

void set_sliders_width(void);


void
deflicker(void) {

	while (g_main_context_iteration(NULL, FALSE));
}


/* returns (hh:mm:ss) or (mm:ss) format time string from sample position */
void
sample2time(unsigned long SR, unsigned long long sample, char * str, int sign) {

	int h;
	char m, s;

	if (!SR)
		SR = 1;

	h = (sample / SR) / 3600;
	m = (sample / SR) / 60 - h * 60;
	s = (sample / SR) - h * 3600 - m * 60;

	if (h > 0)
		sprintf(str, (sign)?("-%02d:%02d:%02d"):("%02d:%02d:%02d"), h, m, s);
	else
		sprintf(str, (sign)?("-%02d:%02d"):("%02d:%02d"), m, s);
}

/* converts a length measured in seconds to the appropriate string */
void
time2time(float seconds, char * str) {

	int h;
	char m, s;

	h = seconds / 3600;
	m = seconds / 60 - h * 60;
	s = seconds - h * 3600 - m * 60;

	if (h > 0) {
		if (h > 9) {
			sprintf(str, "%02d:%02d:%02d", h, m, s);
		} else {
			sprintf(str, "%1d:%02d:%02d", h, m, s);
		}
	} else {
		sprintf(str, "%02d:%02d", m, s);
	}
}


void
set_title_label(char * str) {
/*gchar default_title[MAXLEN];*/

	if (is_file_loaded) {
		if (GTK_IS_LABEL(label_title))
			gtk_label_set_text(GTK_LABEL(label_title), str);
	} else {
		if (GTK_IS_LABEL(label_title)) {
/*                        sprintf(default_title, "Aqualung %s", aqualung_version);*/
/*			gtk_label_set_text(GTK_LABEL(label_title), default_title);*/
			gtk_label_set_text(GTK_LABEL(label_title), "");
                }
        }
}

void
assembly_format_label(char * str, int v_major, int v_minor) {

	switch (v_major) {
	case 0:
		str[0] = '\0';
		break;

#ifdef HAVE_SNDFILE
	case SF_FORMAT_WAV:
		strcpy(str, "Microsoft WAV");
		break;
	case SF_FORMAT_AIFF:
		strcpy(str, "Apple/SGI AIFF");
		break;
	case SF_FORMAT_AU:
		strcpy(str, "Sun/NeXT AU");
		break;
	case SF_FORMAT_PAF:
		strcpy(str, "Ensoniq PARIS");
		break;
	case SF_FORMAT_SVX:
		strcpy(str, "Amiga IFF / SVX8 / SV16");
		break;
	case SF_FORMAT_NIST:
		strcpy(str, "Sphere NIST");
		break;
	case SF_FORMAT_VOC:
		strcpy(str, "VOC");
		break;
	case SF_FORMAT_IRCAM:
		strcpy(str, "Berkeley/IRCAM/CARL");
		break;
	case SF_FORMAT_W64:
		strcpy(str, "Sonic Foundry 64 bit RIFF/WAV");
		break;
	case SF_FORMAT_MAT4:
		strcpy(str, "Matlab (tm) V4.2 / GNU Octave 2.0");
		break;
	case SF_FORMAT_MAT5:
		strcpy(str, "Matlab (tm) V5.0 / GNU Octave 2.1");
		break;
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
	case FORMAT_FLAC:
		strcpy(str, "FLAC");
		break;
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
	case FORMAT_VORBIS:
		strcpy(str, "Ogg Vorbis");
		break;
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPC
	case FORMAT_MPC:
		strcpy(str, "Musepack");
		break;
#endif /* HAVE_MPC */

#ifdef HAVE_MPEG
	case FORMAT_MAD:
		strcpy(str, "MPEG Audio");
		break;
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
        case FORMAT_MOD:
                strcpy(str, "MOD Audio");
                break;
#endif /* HAVE_MOD */

#ifdef HAVE_MAC
        case FORMAT_MAC:
                strcpy(str, "Monkey's Audio");
                break;
#endif /* HAVE_MAC */

	default:
		strcpy(str, _("Unrecognized"));
		break;
	}

#ifdef HAVE_SNDFILE
	if (v_major < 0x1000000) {
		switch (v_minor) {
		case SF_FORMAT_PCM_S8:
			sprintf(str, "%s %s%s%s", str, "(8 ", _("bit signed"), ")");
			break;
		case SF_FORMAT_PCM_U8:
			sprintf(str, "%s %s%s%s", str, "(16 ", _("bit unsigned"), ")");
			break;
		case SF_FORMAT_PCM_16:
			sprintf(str, "%s %s%s%s", str, "(16 ", _("bit signed"), ")");
			break;
		case SF_FORMAT_PCM_24:
			sprintf(str, "%s %s%s%s", str, "(24 ", _("bit signed"), ")");
			break;
		case SF_FORMAT_PCM_32:
			sprintf(str, "%s %s%s%s", str, "(32 ", _("bit signed"), ")");
			break;
		case SF_FORMAT_FLOAT:
			sprintf(str, "%s %s%s%s", str, "(32 ", _("bit float"), ")");
			break;
		case SF_FORMAT_DOUBLE:
			sprintf(str, "%s %s%s%s", str, "(64 ", _("bit double"), ")");
			break;
		case SF_FORMAT_ULAW:
			sprintf(str, "%s %s%s%s", str, "(u-Law ", _("encoding"), ")");
			break;
		case SF_FORMAT_ALAW:
			sprintf(str, "%s %s%s%s", str, "(A-Law ", _("encoding"), ")");
			break;
		case SF_FORMAT_IMA_ADPCM:
			sprintf(str, "%s %s%s%s", str, "(IMA ADPCM ", _("encoding"), ")");
			break;
		case SF_FORMAT_MS_ADPCM:
			sprintf(str, "%s %s%s%s", str, "(Microsoft ADPCM ", _("encoding"), ")");
			break;
		case SF_FORMAT_GSM610:
			sprintf(str, "%s %s%s%s", str, "(GSM 6.10 ", _("encoding"), ")");
			break;
		case SF_FORMAT_VOX_ADPCM:
			sprintf(str, "%s %s%s%s", str, "(Oki Dialogic ADPCM ", _("encoding"), ")");
			break;
		case SF_FORMAT_G721_32:
			sprintf(str, "%s %s%s%s", str, "(32kbps G721 ADPCM ", _("encoding"), ")");
			break;
		case SF_FORMAT_G723_24:
			sprintf(str, "%s %s%s%s", str, "(24kbps G723 ADPCM ", _("encoding"), ")");
			break;
		case SF_FORMAT_G723_40:
			sprintf(str, "%s %s%s%s", str, "(40kbps G723 ADPCM ", _("encoding"), ")");
			break;
		case SF_FORMAT_DWVW_12:
			sprintf(str, "%s %s%s%s", str, "(12 bit DWVW ", _("encoding"), ")");
			break;
		case SF_FORMAT_DWVW_16:
			sprintf(str, "%s %s%s%s", str, "(16 bit DWVW ", _("encoding"), ")");
			break;
		case SF_FORMAT_DWVW_24:
			sprintf(str, "%s %s%s%s", str, "(24 bit DWVW ", _("encoding"), ")");
			break;
		case SF_FORMAT_DWVW_N:
			sprintf(str, "%s %s%s%s", str, "(N bit DWVW ", _("encoding"), ")");
			break;
		}
	}
#endif /* HAVE_SNDFILE */

#ifdef HAVE_MPC
	if (v_major == FORMAT_MPC) {
		
		switch (v_minor) {
		case 7:
			sprintf(str, "%s (%s)", str, _("Profile: Telephone"));
			break;
		case 8:
			sprintf(str, "%s (%s)", str, _("Profile: Thumb"));
			break;
		case 9:
			sprintf(str, "%s (%s)", str, _("Profile: Radio"));
			break;
		case 10:
			sprintf(str, "%s (%s)", str, _("Profile: Standard"));
			break;
		case 11:
			sprintf(str, "%s (%s)", str, _("Profile: Xtreme"));
			break;
		case 12:
			sprintf(str, "%s (%s)", str, _("Profile: Insane"));
			break;
		case 13:
			sprintf(str, "%s (%s)", str, _("Profile: Braindead"));
			break;
		}
	}
#endif /* HAVE_MPC */

#ifdef HAVE_MPEG
	if (v_major == FORMAT_MAD) {
		
		if (v_minor & 0xff7) {
			
			strcat(str, " (");
			
			switch (v_minor & MPEG_LAYER_MASK) {
			case MPEG_LAYER_I:
				strcat(str, _("Layer I"));
				break;
			case MPEG_LAYER_II:
				strcat(str, _("Layer II"));
				break;
			case MPEG_LAYER_III:
				strcat(str, _("Layer III"));
				break;
			}
		}

	        if ((v_minor & MPEG_LAYER_MASK) && (v_minor & (MPEG_MODE_MASK | MPEG_EMPH_MASK)))
			strcat(str, ", ");

		switch (v_minor & MPEG_MODE_MASK) {
		case MPEG_MODE_SINGLE:
			strcat(str, _("Single channel"));
			break;
		case MPEG_MODE_DUAL:
			strcat(str, _("Dual channel"));
			break;
		case MPEG_MODE_JOINT:
			strcat(str, _("Joint stereo"));
			break;
		case MPEG_MODE_STEREO:
			strcat(str, _("Stereo"));
			break;
		}

		if ((v_minor & MPEG_MODE_MASK) && (v_minor & MPEG_EMPH_MASK))
			strcat(str, ", ");


		switch (v_minor & MPEG_EMPH_MASK) {
		case MPEG_EMPH_NONE:
			strcat(str, _("Emphasis: none"));
			break;
		case MPEG_EMPH_5015:
			sprintf(str, "%s%s 50/15 us", str, _("Emphasis:"));
			break;
		case MPEG_EMPH_J_17:
			sprintf(str, "%s%s CCITT J.17", str, _("Emphasis:"));
			break;
		case MPEG_EMPH_RES:
			strcat(str, _("Emphasis: reserved"));
			break;
		}
		
		strcat(str, ")");
	}
#endif /* HAVE_MPEG */

#ifdef HAVE_MAC
	if (v_major == FORMAT_MAC) {

		switch (v_minor) {
		case MAC_COMP_FAST:
			sprintf(str, "%s (%s)", str, _("Compression: Fast"));
			break;
		case MAC_COMP_NORMAL:
			sprintf(str, "%s (%s)", str, _("Compression: Normal"));
			break;
		case MAC_COMP_HIGH:
			sprintf(str, "%s (%s)", str, _("Compression: High"));
			break;
		case MAC_COMP_EXTRA:
			sprintf(str, "%s (%s)", str, _("Compression: Extra High"));
			break;
		case MAC_COMP_INSANE:
			sprintf(str, "%s (%s)", str, _("Compression: Insane"));
			break;
		}
	}
#endif /* HAVE_MAC */
}

void
set_format_label(int v_major, int v_minor) {

	char str[MAXLEN];

	if (!is_file_loaded) {
		if (GTK_IS_LABEL(label_format))
			gtk_label_set_text(GTK_LABEL(label_format), "");
		return;
	}

	assembly_format_label(str, v_major, v_minor);

	if (GTK_IS_LABEL(label_format))
		gtk_label_set_text(GTK_LABEL(label_format), str);
}

void
set_bps_label(int bps) {
	
	char str[MAXLEN];

	sprintf(str, "%.1f kbit/s", bps/1000.0);

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
	case JACK_DRIVER:
		sprintf(str, "%s JACK @ %d Hz", _("Output:"), out_SR);
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
		if (refresh_time_label || time_idx[0] != 0) {
			sample2time(disp_info.sample_rate, disp_pos, str, 0);
			if (GTK_IS_LABEL(time_labels[0])) 
				gtk_label_set_text(GTK_LABEL(time_labels[0]), str);
                        
		}

		if (refresh_time_label || time_idx[0] != 1) {
			sample2time(disp_info.sample_rate, disp_samples - disp_pos, str, 1);
			if (GTK_IS_LABEL(time_labels[1])) 
				gtk_label_set_text(GTK_LABEL(time_labels[1]), str);
                        
		}
		
		if (refresh_time_label || time_idx[0] != 2) {
			sample2time(disp_info.sample_rate, disp_samples, str, 0);
			if (GTK_IS_LABEL(time_labels[2])) 
				gtk_label_set_text(GTK_LABEL(time_labels[2]), str);
                        
		}
	} else {
		int i;
		for (i = 0; i < 3; i++) {
			if (GTK_IS_LABEL(time_labels[i]))
/*				gtk_label_set_text(GTK_LABEL(time_labels[i]), "-  :  -");*/
				gtk_label_set_text(GTK_LABEL(time_labels[i]), "       ");
		}
	}

}


void
refresh_displays(void) {

	long n;
	GtkTreeIter iter;
	char * title_str;

	refresh_time_displays();

	if (play_store) {
		n = get_playing_pos(play_store);
		if (n != -1) {
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 0, &title_str, -1);
			set_title_label(title_str);
			g_free(title_str);
		} else {
			set_title_label("");
		}
	} else {
		set_title_label("");
	}

	set_format_label(disp_info.format_major, disp_info.format_minor);
	set_samplerate_label(disp_info.sample_rate);
	set_bps_label(disp_info.bps);
	set_mono_label(disp_info.is_mono);

	set_output_label(output, out_SR);
	set_src_type_label(src_type);
}


void
zero_displays(void) {

	disp_info.total_samples = 0;
	disp_info.sample_rate = 0;
	disp_info.is_mono = 0;
	disp_info.format_major = 0;
	disp_info.format_minor = 0;
	disp_info.bps = 0;

	disp_samples = 0;
	disp_pos = 0;

	refresh_displays();
}


void
save_window_position(void) {

	gtk_window_get_position(GTK_WINDOW(main_window), &main_pos_x, &main_pos_y);

	if (!playlist_is_embedded && playlist_on) {
		gtk_window_get_position(GTK_WINDOW(playlist_window), &playlist_pos_x, &playlist_pos_y);
	}

	if (browser_on) {
		gtk_window_get_position(GTK_WINDOW(browser_window), &browser_pos_x, &browser_pos_y);
	}

	gtk_window_get_size(GTK_WINDOW(main_window), &main_size_x, &main_size_y);
	gtk_window_get_size(GTK_WINDOW(browser_window), &browser_size_x, &browser_size_y);

	if (!playlist_is_embedded) {
		gtk_window_get_size(GTK_WINDOW(playlist_window), &playlist_size_x, &playlist_size_y);
	} else {
		playlist_size_x = playlist_window->allocation.width;
		playlist_size_y = playlist_window->allocation.height;
	}

	if (!hide_comment_pane) {
		browser_paned_pos = gtk_paned_get_position(GTK_PANED(browser_paned));
	}
}


void
restore_window_position(void) {

	gtk_window_move(GTK_WINDOW(main_window), main_pos_x, main_pos_y);
	deflicker();
	gtk_window_move(GTK_WINDOW(browser_window), browser_pos_x, browser_pos_y);
	deflicker();
	if (!playlist_is_embedded) {
		gtk_window_move(GTK_WINDOW(playlist_window), playlist_pos_x, playlist_pos_y);
		deflicker();
	}
	
	gtk_window_resize(GTK_WINDOW(main_window), main_size_x, main_size_y);
	deflicker();
	gtk_window_resize(GTK_WINDOW(browser_window), browser_size_x, browser_size_y);
	deflicker();
	if (!playlist_is_embedded) {
		gtk_window_resize(GTK_WINDOW(playlist_window), playlist_size_x, playlist_size_y);
		deflicker();
	}

	if (!hide_comment_pane) {
		gtk_paned_set_position(GTK_PANED(browser_paned), browser_paned_pos);
		deflicker();
	}
}


void
change_skin(char * path) {

	int st_play = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button));
	int st_pause = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pause_button));
	int st_r_track = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_button));
	int st_r_all = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_all_button));
	int st_shuffle = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button));
	int st_fx = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(plugin_toggle));
	int st_mstore = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(musicstore_toggle));
	int st_plist = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(playlist_toggle));

	GtkTreeIter iter;
	gpointer gp_instance;
	int i = 0;

	char rcpath[MAXLEN];

        GdkColor color;

	g_source_remove(timeout_tag);


	save_window_position();

	gtk_widget_destroy(main_window);
	deflicker();
	if (!playlist_is_embedded) {
		gtk_widget_destroy(playlist_window);
		deflicker();
	}
	gtk_widget_destroy(browser_window);
	deflicker();
	gtk_widget_destroy(fxbuilder_window);
	deflicker();
	
	sprintf(rcpath, "%s/rc", path);
	gtk_rc_parse(rcpath);

	create_main_window(path);
	deflicker();
	create_playlist();
	deflicker();
	create_music_browser();
	deflicker();
	create_fxbuilder();
	deflicker();
	
	g_signal_handler_block(G_OBJECT(play_button), play_id);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), st_play);
	g_signal_handler_unblock(G_OBJECT(play_button), play_id);

	g_signal_handler_block(G_OBJECT(pause_button), pause_id);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button), st_pause);
	g_signal_handler_unblock(G_OBJECT(pause_button), pause_id);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_button), st_r_track);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_all_button), st_r_all);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shuffle_button), st_shuffle);

	if (playlist_is_embedded)
		g_signal_handlers_block_by_func(G_OBJECT(playlist_toggle), playlist_toggled, NULL);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle), st_plist);
	if (playlist_is_embedded)
		g_signal_handlers_unblock_by_func(G_OBJECT(playlist_toggle), playlist_toggled, NULL);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(musicstore_toggle), st_mstore);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plugin_toggle), st_fx);
	gtk_widget_show_all(main_window);
	deflicker();
	deflicker();

	if (playlist_is_embedded) {
		if (!playlist_on) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle), FALSE);
			gtk_widget_hide(playlist_window);
			deflicker();
		}
	}

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(running_store), &iter, NULL, i) &&
		i < MAX_PLUGINS) {

		gtk_tree_model_get(GTK_TREE_MODEL(running_store), &iter, 1, &gp_instance, -1);
		gtk_widget_reset_rc_styles(((plugin_instance *)gp_instance)->window);
		gtk_widget_queue_draw(((plugin_instance *)gp_instance)->window);
		deflicker();
		++i;
	}

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

        set_sliders_width();
	restore_window_position();
	deflicker();
	refresh_displays();

        if(override_skin_settings && (gdk_color_parse(activesong_color, &color) == TRUE)) {
                play_list->style->fg[SELECTED].red = color.red;
                play_list->style->fg[SELECTED].green = color.green;
                play_list->style->fg[SELECTED].blue = color.blue;
        }

	set_playlist_color();
	
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_vol), vol);
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_bal), bal);

	timeout_tag = g_timeout_add(TIMEOUT_PERIOD, timeout_callback, NULL);
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

	port_setup_dialog();
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


gint
vol_bal_timeout_callback(gpointer data) {

	refresh_time_label = 1;
	vol_bal_timeout_tag = 0;
	refresh_time_displays();

	return FALSE;
}


void
main_window_close(GtkWidget * widget, gpointer data) {

	send_cmd = CMD_FINISH;
	jack_ringbuffer_write(rb_gui2disk, &send_cmd, 1);
        if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
                pthread_cond_signal(&disk_thread_wake);
                pthread_mutex_unlock(&disk_thread_lock);
        }

	save_music_store();
	save_window_position();
	save_config();
	save_plugin_data();

	if (auto_save_playlist) {
		char playlist_name[MAXLEN];

		snprintf(playlist_name, MAXLEN-1, "%s/%s", confdir, "playlist.xml");
		save_playlist(playlist_name);
	}

        pango_font_description_free(fd_playlist);
        pango_font_description_free(fd_browser);

	lrdf_cleanup();
	close_app_socket();
	gtk_main_quit();
}


gint
main_window_key_pressed(GtkWidget * widget, GdkEventKey * event) {

	switch (event->keyval) {
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
	}


	if (alt_L || alt_R) {
		switch (event->keyval) {	
		case GDK_KP_Divide:
			refresh_time_label = 0;
			if (vol_bal_timeout_tag) {
				g_source_remove(vol_bal_timeout_tag);
			}

			vol_bal_timeout_tag = g_timeout_add(1000, vol_bal_timeout_callback, NULL);
			g_signal_emit_by_name(G_OBJECT(scale_bal), "move-slider",
					      GTK_SCROLL_STEP_BACKWARD, NULL);
			return TRUE;
			break;
		case GDK_KP_Multiply:
			refresh_time_label = 0;
			if (vol_bal_timeout_tag) {
				g_source_remove(vol_bal_timeout_tag);
			}
			
			vol_bal_timeout_tag = g_timeout_add(1000, vol_bal_timeout_callback, NULL);
			g_signal_emit_by_name(G_OBJECT(scale_bal),
					      "move-slider", GTK_SCROLL_STEP_FORWARD, NULL);
			break;
			return TRUE;
		}
	} else {
		switch (event->keyval) {
		case GDK_KP_Divide:
			refresh_time_label = 0;
			if (vol_bal_timeout_tag) {
				g_source_remove(vol_bal_timeout_tag);
			}
			
			vol_bal_timeout_tag = g_timeout_add(1000, vol_bal_timeout_callback, NULL);
			g_signal_emit_by_name(G_OBJECT(scale_vol), "move-slider",
					      GTK_SCROLL_STEP_BACKWARD, NULL);
			return TRUE;
			break;
		case GDK_KP_Multiply:
			refresh_time_label = 0;
			if (vol_bal_timeout_tag) {
				g_source_remove(vol_bal_timeout_tag);
			}
			
			vol_bal_timeout_tag = g_timeout_add(1000, vol_bal_timeout_callback, NULL);
			g_signal_emit_by_name(G_OBJECT(scale_vol),
					      "move-slider", GTK_SCROLL_STEP_FORWARD, NULL);
			return TRUE;
			break;
                case GDK_Right:
                        if (is_file_loaded && allow_seeks) {
                                refresh_scale = 0;
                                g_signal_emit_by_name(G_OBJECT(scale_pos), "move-slider",
                                                      GTK_SCROLL_STEP_FORWARD, NULL);
                        }
                        return TRUE;
                        break;
                case GDK_Left:
                        if (is_file_loaded && allow_seeks) {
                                refresh_scale = 0;
                                g_signal_emit_by_name(G_OBJECT(scale_pos), "move-slider",
                                                      GTK_SCROLL_STEP_BACKWARD, NULL);
                        }
                        return TRUE;
                        break;
		case GDK_Down:
			if (playlist_is_embedded && gtk_widget_is_focus(play_list)) {
				return FALSE;
			}
			next_event(NULL, NULL, NULL);
			return TRUE;
			break;
		case GDK_Up:
			if (playlist_is_embedded && gtk_widget_is_focus(play_list)) {
				return FALSE;
			}
			prev_event(NULL, NULL, NULL);
			return TRUE;
			break;
		case GDK_s:
		case GDK_S:
			if (playlist_is_embedded && gtk_widget_is_focus(play_list)) {
				return FALSE;
			}
			stop_event(NULL, NULL, NULL);
			return TRUE;
			break;
		case GDK_space:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button),
			        !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pause_button)));
			return TRUE;
			break;
		case GDK_p:
		case GDK_P:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
			        !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button)));
			return TRUE;
			break;
			
		case GDK_i:
		case GDK_I:
			if (playlist_is_embedded && gtk_widget_is_focus(play_list)) {
				return FALSE;
			}
			conf__fileinfo_cb(NULL);
			return TRUE;
			break;

		case GDK_BackSpace:
			if (allow_seeks) {
				seek_t seek;
				
				send_cmd = CMD_SEEKTO;
				seek.seek_to_pos = 0.0f;
				jack_ringbuffer_write(rb_gui2disk, &send_cmd, 1);
				jack_ringbuffer_write(rb_gui2disk, (char *)&seek, sizeof(seek_t));
				
				if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
					pthread_cond_signal(&disk_thread_wake);
					pthread_mutex_unlock(&disk_thread_lock);
				}
			}
			
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pause_button)))
				gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_pos), 0.0f);
			
			return TRUE;
			break;
		}
	}
	return FALSE;
}


gint
main_window_key_released(GtkWidget * widget, GdkEventKey * event) {

	switch (event->keyval) {
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
        case GDK_Right:
        case GDK_Left:
                if (is_file_loaded && allow_seeks && refresh_scale == 0) {
                        seek_t seek;

                        refresh_scale = 1;
                        send_cmd = CMD_SEEKTO;
                        seek.seek_to_pos = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_pos))
                                / 100.0f * total_samples;
                        jack_ringbuffer_write(rb_gui2disk, &send_cmd, 1);
                        jack_ringbuffer_write(rb_gui2disk, (char *)&seek, sizeof(seek_t));
                        if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
                                pthread_cond_signal(&disk_thread_wake);
                                pthread_mutex_unlock(&disk_thread_lock);
                        }
                }
                break;
	}

	return FALSE;
}


gint
main_window_focus_out(GtkWidget * widget, GdkEventFocus * event, gpointer data) {

        alt_L = 0;
        alt_R = 0;
        shift_L = 0;
        shift_R = 0;

        refresh_scale = 1;

        return FALSE;
}


gint
main_window_state_changed(GtkWidget * widget, GdkEventWindowState * event, gpointer data) {
	
	if (event->new_window_state == GDK_WINDOW_STATE_ICONIFIED) {
		if (browser_on)
			gtk_window_iconify(GTK_WINDOW(browser_window));

		if (!playlist_is_embedded && playlist_on)
			gtk_window_iconify(GTK_WINDOW(playlist_window));

		if (vol_window)
			gtk_window_iconify(GTK_WINDOW(vol_window));

		if (info_window)
			gtk_window_iconify(GTK_WINDOW(info_window));

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
	}

	if (event->new_window_state == 0) {
		if (browser_on)
			gtk_window_deiconify(GTK_WINDOW(browser_window));

		if (!playlist_is_embedded && playlist_on)
			gtk_window_deiconify(GTK_WINDOW(playlist_window));

		if (vol_window)
			gtk_window_deiconify(GTK_WINDOW(vol_window));

		if (info_window)
			gtk_window_deiconify(GTK_WINDOW(info_window));

		if (fxbuilder_on)
			gtk_window_deiconify(GTK_WINDOW(fxbuilder_window));

	}
	
        return FALSE;
}


gint
main_window_button_pressed(GtkWidget * widget, GdkEventButton * event) {

	if (event->button == 3) {

		if (is_file_loaded && (current_file[0] != '\0')) {
			gtk_widget_set_sensitive(conf__fileinfo, TRUE);
		} else {
			gtk_widget_set_sensitive(conf__fileinfo, FALSE);
		}

		gtk_menu_popup(GTK_MENU(conf_menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);
	}

	return TRUE;
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

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		show_fxbuilder();
	} else {
		hide_fxbuilder();
	}
}


static gint
scale_button_press_event(GtkWidget * widget, GdkEventButton * event) {

	if (event->button == 3)
		return FALSE;

	if (!is_file_loaded)
		return FALSE;

	if (!allow_seeks)
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
		
		if (refresh_scale == 0) {
			refresh_scale = 1;

			send_cmd = CMD_SEEKTO;
			seek.seek_to_pos = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_pos))
				/ 100.0f * total_samples;
			jack_ringbuffer_write(rb_gui2disk, &send_cmd, 1);
			jack_ringbuffer_write(rb_gui2disk, (char *)&seek, sizeof(seek_t));
			if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
				pthread_cond_signal(&disk_thread_wake);
				pthread_mutex_unlock(&disk_thread_lock);
			}
		}
	}

	return FALSE;
}


void
changed_pos(GtkAdjustment * adj, gpointer data) {

        char str[16];

	if (!is_file_loaded)
		gtk_adjustment_set_value(adj, 0.0f);

        if(enable_tooltips) {
                sprintf(str, "Position: %d%%", (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(adj))); 
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), scale_pos, str, NULL);
        }
}


gint
scale_vol_button_press_event(GtkWidget * widget, GdkEventButton * event) {

	char str[10];
	vol = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_vol));

	if (shift_L || shift_R) {
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_vol), 0);
		return TRUE;
	}

	if (vol < -40.5f) {
		sprintf(str, _("Mute"));
	} else {
		sprintf(str, _("%d dB"), (int)vol);
	}

	gtk_label_set_text(GTK_LABEL(time_labels[time_idx[0]]), str);
	
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

	vol = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_vol));
	vol = (int)vol;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_vol), vol);

        if (vol < -40.5f) {
                sprintf(str, _("Mute"));
        } else {
                sprintf(str, _("%d dB"), (int)vol);
        }

        if (!shift_L && !shift_R && !refresh_time_label) {
		gtk_label_set_text(GTK_LABEL(time_labels[time_idx[0]]), str);
        }

        sprintf(str2, "Volume: %s", str);
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
	bal = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_bal));

	if (shift_L || shift_R) {
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_bal), 0);
		return TRUE;
	}
	
	if (bal != 0.0f) {
		if (bal > 0.0f) {
			sprintf(str, _("%d%% R"), (int)bal);
		} else {
			sprintf(str, _("%d%% L"), -1*(int)bal);
		}
	} else {
		sprintf(str, _("C"));
	}

	gtk_label_set_text(GTK_LABEL(time_labels[time_idx[0]]), str);
	
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

	bal = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_bal));
	bal = (int)bal;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_bal), bal);

        if (bal != 0.0f) {
                if (bal > 0.0f) {
                        sprintf(str, _("%d%% R"), (int)bal);
                } else {
                        sprintf(str, _("%d%% L"), -1*(int)bal);
                }
        } else {
                sprintf(str, _("C"));
        }
	
        if (!shift_L && !shift_R && !refresh_time_label) {
		gtk_label_set_text(GTK_LABEL(time_labels[time_idx[0]]), str);
	}

        sprintf(str2, "Balance: %s", str);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), scale_bal, str2, NULL);
}


gint
scale_bal_button_release_event(GtkWidget * widget, GdkEventButton * event) {

	refresh_time_label = 1;
	refresh_time_displays();

	return FALSE;
}


gint
prev_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

	GtkTreeIter iter;
	long n;
	long n_items;
	char cmd;
	cue_t cue;
	char * str;

	if (!allow_seeks)
		return FALSE;

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
		/* normal or repeat mode */
		n = get_playing_pos(play_store);
		if (n != -1) {
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
			gtk_list_store_set(play_store, &iter, 2, pl_color_inactive, -1);
                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_NORMAL, -1);
			if (n == 0)
				n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), NULL);
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n-1);
			gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
			
		} else {
			if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(play_store), &iter)) {
				gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
                                gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
			}
		}
	} else {
		/* shuffle mode */
		n = get_playing_pos(play_store);
		if (n != -1) {
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
			gtk_list_store_set(play_store, &iter, 2, pl_color_inactive, -1);
                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_NORMAL, -1);
		}
		n_items = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), NULL);
		if (n_items) {
			n = (double)rand() * n_items / RAND_MAX;
			if (n == n_items)
				--n;
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
			gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
		}
	}

	if (is_file_loaded) {

		if ((n = get_playing_pos(play_store)) == -1) {

			if (is_paused) {

				is_paused = 0;
				g_signal_handler_block(G_OBJECT(pause_button), pause_id);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button), FALSE);
				g_signal_handler_unblock(G_OBJECT(pause_button), pause_id);

				stop_event(NULL, NULL, NULL);
			}

			return FALSE;
		}

		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
		gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 1, &str, 3, &(cue.voladj), -1);
		cue.filename = strdup(g_locale_from_utf8(str, -1, NULL, NULL, NULL));
		strncpy(current_file, str, MAXLEN-1);
		g_free(str);

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
	}

	return FALSE;
}


gint
next_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

	GtkTreeIter iter;
	long n;
	long n_items;
	char cmd;
	cue_t cue;
	char * str;

	if (!allow_seeks)
		return FALSE;

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
		/* normal or repeat mode */
		n = get_playing_pos(play_store);
		if (n != -1) {
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
			gtk_list_store_set(play_store, &iter, 2, pl_color_inactive, -1);
                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_NORMAL, -1);
			if (n == (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), NULL) - 1))
				n = -1;
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n+1);
			gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
			
		} else {
			if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(play_store), &iter)) {
				gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
                                gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
			}
		}
	} else {
		/* shuffle mode */
		n = get_playing_pos(play_store);
		if (n != -1) {
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
			gtk_list_store_set(play_store, &iter, 2, pl_color_inactive, -1);
                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_NORMAL, -1);
		}
		n_items = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), NULL);
		if (n_items) {
			n = (double)rand() * n_items / RAND_MAX;
			if (n == n_items)
				--n;
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
			gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
		}
	}

	if (is_file_loaded) {

		if ((n = get_playing_pos(play_store)) == -1) {

			if (is_paused) {

				is_paused = 0;
				g_signal_handler_block(G_OBJECT(pause_button), pause_id);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button), FALSE);
				g_signal_handler_unblock(G_OBJECT(pause_button), pause_id);

				stop_event(NULL, NULL, NULL);
			}

			return FALSE;
		}

		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);
		gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 1, &str, 3, &(cue.voladj), -1);
		cue.filename = strdup(g_locale_from_utf8(str, -1, NULL, NULL, NULL));
		strncpy(current_file, str, MAXLEN-1);
		g_free(str);

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
	}

	return FALSE;
}


gint
play_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

	GtkTreeIter iter;
	long n;
	long n_items;
	char cmd;
	cue_t cue;
	char * str;

	if (!is_paused) {
		cmd = CMD_CUE;
		cue.filename = NULL;

		n = get_playing_pos(play_store);
		if (n != -1) {
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, n);

			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 1, &str,
					   3, &(cue.voladj), -1);
			cue.filename = strdup(g_locale_from_utf8(str, -1, NULL, NULL, NULL));
			strncpy(current_file, str, MAXLEN-1);
			g_free(str);
			is_file_loaded = 1;
			g_signal_handler_block(G_OBJECT(play_button), play_id);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), TRUE);
			g_signal_handler_unblock(G_OBJECT(play_button), play_id);

		} else {
			if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
                                /* normal or repeat mode */
				if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(play_store), &iter)) {
					gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
                                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
					gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 1, &str,
							   3, &(cue.voladj), -1);
					cue.filename = strdup(g_locale_from_utf8(str, -1, NULL, NULL, NULL));
					strncpy(current_file, str, MAXLEN-1);
					g_free(str);
					is_file_loaded = 1;
					g_signal_handler_block(G_OBJECT(play_button), play_id);
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), TRUE);
					g_signal_handler_unblock(G_OBJECT(play_button), play_id);
				} else {
					is_file_loaded = 0;
					current_file[0] = '\0';
					zero_displays();
					g_signal_handler_block(G_OBJECT(play_button), play_id);
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), FALSE);
					g_signal_handler_unblock(G_OBJECT(play_button), play_id);
				}

			} else { /* shuffle mode */
				n_items = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store), NULL);
				if (n_items) {
					n = (double)rand() * n_items / RAND_MAX;
					if (n == n_items)
						--n;
					gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter,
								      NULL, n);
					gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
                                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
					gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 1, &str,
							   3, &(cue.voladj), -1);
					cue.filename = strdup(g_locale_from_utf8(str, -1, NULL, NULL, NULL));
					strncpy(current_file, str, MAXLEN-1);
					g_free(str);
					is_file_loaded = 1;
					g_signal_handler_block(G_OBJECT(play_button), play_id);
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), TRUE);
					g_signal_handler_unblock(G_OBJECT(play_button), play_id);
				} else {
					is_file_loaded = 0;
					current_file[0] = '\0';
					zero_displays();
					g_signal_handler_block(G_OBJECT(play_button), play_id);
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), FALSE);
					g_signal_handler_unblock(G_OBJECT(play_button), play_id);
				}
			}
		}
		jack_ringbuffer_write(rb_gui2disk, &cmd, sizeof(char));
		jack_ringbuffer_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
			
	} else {
		is_paused = 0;
		g_signal_handler_block(G_OBJECT(pause_button), pause_id);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button), FALSE);
		g_signal_handler_unblock(G_OBJECT(pause_button), pause_id);
		send_cmd = CMD_RESUME;
		jack_ringbuffer_write(rb_gui2disk, &send_cmd, 1);
	}

	if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
		pthread_cond_signal(&disk_thread_wake);
		pthread_mutex_unlock(&disk_thread_lock);
	}

	return FALSE;
}


gint
pause_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

	if ((!allow_seeks) || (!is_file_loaded)) {
		g_signal_handler_block(G_OBJECT(pause_button), pause_id);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button), FALSE);
		g_signal_handler_unblock(G_OBJECT(pause_button), pause_id);
		return FALSE;
	}

	if (!is_paused) {
		is_paused = 1;
		g_signal_handler_block(G_OBJECT(play_button), play_id);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), FALSE);
		g_signal_handler_unblock(G_OBJECT(play_button), play_id);
		send_cmd = CMD_PAUSE;
		jack_ringbuffer_write(rb_gui2disk, &send_cmd, 1);

	} else {
		is_paused = 0;
		g_signal_handler_block(G_OBJECT(play_button), play_id);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), TRUE);
		g_signal_handler_unblock(G_OBJECT(play_button), play_id);
		send_cmd = CMD_RESUME;
		jack_ringbuffer_write(rb_gui2disk, &send_cmd, 1);
	}

	if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
		pthread_cond_signal(&disk_thread_wake);
		pthread_mutex_unlock(&disk_thread_lock);
	}

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
	g_signal_handler_block(G_OBJECT(play_button), play_id);
	g_signal_handler_block(G_OBJECT(pause_button), pause_id);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button), FALSE);
	g_signal_handler_unblock(G_OBJECT(play_button), play_id);
	g_signal_handler_unblock(G_OBJECT(pause_button), pause_id);

        if (is_paused) {
                is_paused = 0;
                g_signal_handler_block(G_OBJECT(pause_button), pause_id);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button), FALSE);
                g_signal_handler_unblock(G_OBJECT(pause_button), pause_id);
        }

	cmd = CMD_CUE;
	cue.filename = NULL;
        jack_ringbuffer_write(rb_gui2disk, &cmd, sizeof(char));
        jack_ringbuffer_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));

	if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
		pthread_cond_signal(&disk_thread_wake);
		pthread_mutex_unlock(&disk_thread_lock);
	}
	
	return FALSE;
}


void
swap_labels(int a, int b) {

	GtkWidget * tmp;
	int t;

	tmp = time_labels[time_idx[a]];
	time_labels[time_idx[a]] = time_labels[time_idx[b]] ;
	time_labels[time_idx[b]] = tmp;

	t = time_idx[b];
	time_idx[b] = time_idx[a];
	time_idx[a] = t;
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

	if (scroll_btn != 1)
		return FALSE;

	if (!scroll_btn)
		return TRUE;

	if (dx < -10) {
		g_signal_emit_by_name(G_OBJECT(win), "scroll-child", GTK_SCROLL_STEP_FORWARD, TRUE, NULL);
		x_scroll_start = event->x;
		gdk_window_set_cursor(gtk_widget_get_parent_window(GTK_WIDGET(win)),
				      gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW));
	}

	if (dx > 10) {
		g_signal_emit_by_name(G_OBJECT(win), "scroll-child", GTK_SCROLL_STEP_BACKWARD, TRUE, NULL);
		x_scroll_start = event->x;
		gdk_window_set_cursor(gtk_widget_get_parent_window(GTK_WIDGET(win)),
				      gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW));
	}

	x_scroll_pos = event->x;

	return TRUE;
}


void
repeat_toggled(GtkWidget * widget, gpointer data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_button))) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_all_button), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shuffle_button), FALSE);
		repeat_on = 1;
	} else {
		repeat_on = 0;
	}
}


void
repeat_all_toggled(GtkWidget * widget, gpointer data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_all_button))) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_button), FALSE);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shuffle_button), FALSE);
		repeat_all_on = 1;
	} else {
		repeat_all_on = 0;
	}
}


void
shuffle_toggled(GtkWidget * widget, gpointer data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_button), FALSE);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_all_button), FALSE);
		shuffle_on = 1;
	} else {
		shuffle_on = 0;
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

        if(!pixbuf) {
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
			button = gtk_toggle_button_new_with_mnemonic(alt);
		} else {
			button = gtk_button_new_with_mnemonic(alt);
		}
	}

        return button;
}


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


void
create_main_window(char * skin_path) {

	GtkWidget * vbox;
	GtkWidget * disp_hbox;
	GtkWidget * btns_hbox;
	GtkWidget * title_hbox;
	GtkWidget * info_hbox;
	GtkWidget * vb_table;

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

	GtkWidget * prev_button;
	GtkWidget * stop_button;
	GtkWidget * next_button;

	GtkWidget * sr_table;

	char path[MAXLEN];
	char str_session_id[32];
	char title[MAXLEN];

	strcpy(title, "Aqualung");
	if (aqualung_session_id > 0) {
		sprintf(str_session_id, ".%d", aqualung_session_id);
		strcat(title, str_session_id);
	}
	if ((output == JACK_DRIVER) && (strcmp(client_name, "aqualung") != 0)) {
		strcat(title, " [");
		strcat(title, client_name);
		strcat(title, "]");
	}

	main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_name(main_window, "main_window");
	gtk_window_set_title(GTK_WINDOW(main_window), title);
        g_signal_connect(G_OBJECT(main_window), "delete_event", G_CALLBACK(main_window_close), NULL);
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

        /* initialize fonts */

	fd_playlist = pango_font_description_from_string(playlist_font);
 	fd_browser = pango_font_description_from_string(browser_font);
 	fd_bigtimer = pango_font_description_from_string(bigtimer_font);
 	fd_smalltimer = pango_font_description_from_string(smalltimer_font);


        aqualung_tooltips = gtk_tooltips_new();

	conf_menu = gtk_menu_new();

	conf__options = gtk_menu_item_new_with_label(_("Settings"));
	conf__skin = gtk_menu_item_new_with_label(_("Skin chooser"));
	conf__jack = gtk_menu_item_new_with_label(_("JACK port setup"));
	conf__fileinfo = gtk_menu_item_new_with_label(_("File info"));
	conf__separator = gtk_separator_menu_item_new();
	conf__about = gtk_menu_item_new_with_label(_("About"));

	gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__options);
	gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__skin);
	gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__jack);
	gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__fileinfo);
	gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__separator);
	gtk_menu_shell_append(GTK_MENU_SHELL(conf_menu), conf__about);

	g_signal_connect_swapped(G_OBJECT(conf__options), "activate", G_CALLBACK(conf__options_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(conf__skin), "activate", G_CALLBACK(conf__skin_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(conf__jack), "activate", G_CALLBACK(conf__jack_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(conf__fileinfo), "activate", G_CALLBACK(conf__fileinfo_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(conf__about), "activate", G_CALLBACK(conf__about_cb), NULL);

	if (output != JACK_DRIVER)
		gtk_widget_set_sensitive(conf__jack, FALSE);

	gtk_widget_set_sensitive(conf__fileinfo, FALSE);

	gtk_widget_show(conf__options);
	gtk_widget_show(conf__skin);
	gtk_widget_show(conf__jack);
	gtk_widget_show(conf__fileinfo);
	gtk_widget_show(conf__separator);
	gtk_widget_show(conf__about);


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
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(info_scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_NEVER);

	info_viewp = gtk_viewport_new(NULL, NULL);
	gtk_widget_set_name(info_viewp, "info_viewport");
	gtk_container_add(GTK_CONTAINER(info_scrolledwin), info_viewp);
	gtk_widget_set_events(info_viewp, GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
        g_signal_connect(G_OBJECT(info_viewp), "button_press_event", G_CALLBACK(scroll_btn_pressed), NULL);
        g_signal_connect(G_OBJECT(info_viewp), "button_release_event", G_CALLBACK(scroll_btn_released),
                         (gpointer)info_scrolledwin);
        g_signal_connect(G_OBJECT(info_viewp), "motion_notify_event", G_CALLBACK(scroll_motion_notify),
                         (gpointer)info_scrolledwin);

	info_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(info_hbox), 1);
	gtk_container_add(GTK_CONTAINER(info_viewp), info_hbox);



	title_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
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
	bigtimer_label = time_labels[time_idx[0]] = gtk_label_new("");

        if(override_skin_settings) {
                gtk_widget_modify_font (bigtimer_label, fd_bigtimer);
        }

        gtk_widget_set_name(time_labels[time_idx[0]], "big_timer_label");
	gtk_container_add(GTK_CONTAINER(time0_viewp), time_labels[time_idx[0]]);

	time_hbox1 = gtk_hbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(time_hbox1), 2);
	gtk_container_add(GTK_CONTAINER(time1_viewp), time_hbox1);

	smalltimer_label_1 = time_labels[time_idx[1]] = gtk_label_new("");

        if(override_skin_settings) {
                gtk_widget_modify_font (smalltimer_label_1, fd_smalltimer);
        }

        gtk_widget_set_name(time_labels[time_idx[1]], "small_timer_label");
	gtk_box_pack_start(GTK_BOX(time_hbox1), time_labels[time_idx[1]], TRUE, TRUE, 0);

	time_hbox2 = gtk_hbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(time_hbox2), 2);
	gtk_container_add(GTK_CONTAINER(time2_viewp), time_hbox2);

	smalltimer_label_2 = time_labels[time_idx[2]] = gtk_label_new("");

        if(override_skin_settings) {
                gtk_widget_modify_font (smalltimer_label_2, fd_smalltimer);
        }

        gtk_widget_set_name(time_labels[time_idx[2]], "small_timer_label");
	gtk_box_pack_start(GTK_BOX(time_hbox2), time_labels[time_idx[2]], TRUE, TRUE, 0);

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

        GTK_WIDGET_UNSET_FLAGS(scale_vol, GTK_CAN_FOCUS);
        GTK_WIDGET_UNSET_FLAGS(scale_bal, GTK_CAN_FOCUS);
        GTK_WIDGET_UNSET_FLAGS(scale_pos, GTK_CAN_FOCUS);


        /* Embedded playlist */
	if (playlist_is_embedded && buttons_at_the_bottom) {
		playlist_window = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), playlist_window, TRUE, TRUE, 3);
	}


        /* Button box with prev, play, pause, stop, next buttons */

	btns_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), btns_hbox, FALSE, FALSE, 0);

	sprintf(path, "%s/%s", skin_path, "prev");
	prev_button = create_button_with_image(path, 0, "prev");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), prev_button, _("Previous song (up)"), NULL);

	sprintf(path, "%s/%s", skin_path, "stop");
	stop_button = create_button_with_image(path, 0, "stop");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), stop_button, _("Stop (s)"), NULL);

	sprintf(path, "%s/%s", skin_path, "next");
	next_button = create_button_with_image(path, 0, "next");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), next_button, _("Next song (down)"), NULL);

	sprintf(path, "%s/%s", skin_path, "play");
	play_button = create_button_with_image(path, 1, "play");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), play_button, _("Play (p)"), NULL);

	sprintf(path, "%s/%s", skin_path, "pause");
	pause_button = create_button_with_image(path, 1, "pause");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), time_labels[time_idx[0]], "dupa1", NULL);
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), pause_button, _("Pause (space)"), NULL);

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
	playlist_toggle = create_button_with_image(path, 1, "P_L");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), playlist_toggle, _("Toggle playlist"), NULL);
	gtk_box_pack_end(GTK_BOX(btns_hbox), playlist_toggle, FALSE, FALSE, 0);
	
	sprintf(path, "%s/%s", skin_path, "ms");
	musicstore_toggle = create_button_with_image(path, 1, "M_S");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), musicstore_toggle, _("Toggle music store"), NULL);
	gtk_box_pack_end(GTK_BOX(btns_hbox), musicstore_toggle, FALSE, FALSE, 3);

	sprintf(path, "%s/%s", skin_path, "fx");
	plugin_toggle = create_button_with_image(path, 1, "F_X");
        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), plugin_toggle, _("Toggle LADSPA patch builder"), NULL);
	gtk_box_pack_end(GTK_BOX(btns_hbox), plugin_toggle, FALSE, FALSE, 0);

	g_signal_connect(playlist_toggle, "toggled", G_CALLBACK(playlist_toggled), NULL);
	g_signal_connect(musicstore_toggle, "toggled", G_CALLBACK(musicstore_toggled), NULL);
	g_signal_connect(plugin_toggle, "toggled", G_CALLBACK(plugin_toggled), NULL);

	GTK_WIDGET_UNSET_FLAGS(playlist_toggle, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(musicstore_toggle, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(plugin_toggle, GTK_CAN_FOCUS);

	gtk_box_pack_end(GTK_BOX(btns_hbox), sr_table, FALSE, FALSE, 3);

	/* Embedded playlist */
	if (playlist_is_embedded && !buttons_at_the_bottom) {
		playlist_window = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), playlist_window, TRUE, TRUE, 3);
	}

        if(enable_tooltips) {
                gtk_tooltips_enable(aqualung_tooltips);
        } else {
                gtk_tooltips_disable(aqualung_tooltips);
        }

}


void
process_filenames(char ** argv, int optind, int enqueue) {
	
	int i;
	
	for (i = optind; argv[i] != NULL; i++) {
		if ((enqueue) || (i > optind)) {
			add_to_playlist(argv[i], 1);
		} else {
			add_to_playlist(argv[i], 0);
		}
	}
}	


void
create_gui(int argc, char ** argv, int optind, int enqueue,
	   unsigned long rate, unsigned long rb_audio_size) {

	char * home;
	char path[MAXLEN];
	GList * glist = NULL;
	GdkPixbuf * pixbuf = NULL;
        GdkColor color;

	srand(time(0));
	sample_pos = 0;
	out_SR = rate;
	rb_size = rb_audio_size;

	gtk_init(&argc, &argv);
	lrdf_init();

	if (!(home = getenv("HOME"))) {
		/* no warning -- done that in core.c::load_default_cl() */
		home = ".";
	}

	sprintf(currdir, "%s/*", home);
	sprintf(confdir, "%s/.aqualung", home);

	if (chdir(confdir) != 0) {
		if (errno == ENOENT) {
			fprintf(stderr, "Creating directory %s\n", confdir);
			mkdir(confdir, S_IRUSR | S_IWUSR | S_IXUSR);
			chdir(confdir);
		} else {
			fprintf(stderr, "An error occured while attempting chdir(\"%s\"). errno = %d\n",
				confdir, errno);
		}
	}
	
	load_config();

	if (title_format[0] == '\0')
		sprintf(title_format, "%%a: %%t [%%r]");
	if (skin[0] == '\0') {
		sprintf(skin, "%s/default", SKINDIR);
		main_pos_x = 30;
		main_pos_y = 30;
		main_size_x = 530;
		main_size_y = 113;
		browser_pos_x = 30;
		browser_pos_y = 180;
		browser_size_x = 204;
		browser_size_y = 408;
		browser_on = 1;
		playlist_pos_x = 260;
		playlist_pos_y = 180;
		playlist_size_x = 300;
		playlist_size_y = 408;
		playlist_on = 1;
	}

	if (src_type == -1)
		src_type = 4;

	sprintf(path, "%s/rc", skin);
	gtk_rc_parse(path);

	create_main_window(skin);

	vol_prev = -101.0f;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_vol), vol);
	bal_prev = -101.0f;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_bal), bal);

	create_playlist();
        playlist_size_allocate(NULL, NULL);

	create_music_browser();

	create_fxbuilder();

	load_plugin_data();
	load_music_store();

	if (auto_save_playlist) {
		char playlist_name[MAXLEN];

		snprintf(playlist_name, MAXLEN-1, "%s/%s", confdir, "playlist.xml");
		load_playlist(playlist_name, 0);
	}

	sprintf(path, "%s/icon_16.png", DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		glist = g_list_append(glist, gdk_pixbuf_new_from_file(path, NULL));
	}

	sprintf(path, "%s/icon_32.png", DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		glist = g_list_append(glist, gdk_pixbuf_new_from_file(path, NULL));
	}

	sprintf(path, "%s/icon_64.png", DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		glist = g_list_append(glist, gdk_pixbuf_new_from_file(path, NULL));
	}

	if (glist != NULL) {
		gtk_window_set_default_icon_list(glist);
	}

	if (repeat_on)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_button), TRUE);

	if (repeat_all_on)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_all_button), TRUE);

	if (shuffle_on)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shuffle_button), TRUE);


	if (browser_on) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(musicstore_toggle), TRUE);
		deflicker();
	}

	if (playlist_on) {
		if (playlist_is_embedded)
			g_signal_handlers_block_by_func(G_OBJECT(playlist_toggle), playlist_toggled, NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle), TRUE);
		deflicker();
		if (playlist_is_embedded)
			g_signal_handlers_unblock_by_func(G_OBJECT(playlist_toggle), playlist_toggled, NULL);
	}

	restore_window_position();
	gtk_widget_show_all(main_window);
	deflicker();

	if (playlist_is_embedded) {
		if (!playlist_on) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playlist_toggle), FALSE);
			gtk_widget_hide(playlist_window);
			deflicker();
		}
	}

	zero_displays();
	set_playlist_color();
        set_sliders_width();

	/* read command line filenames */
	process_filenames(argv, optind, enqueue);

	/* activate jack client and connect ports */
	if (output == JACK_DRIVER) {
		jack_client_start();
	}

	/* set timeout function */
	timeout_tag = g_timeout_add(TIMEOUT_PERIOD, timeout_callback, NULL);

        /* update sliders' tooltips */
        if(enable_tooltips) {
                changed_vol(GTK_ADJUSTMENT(adj_vol), NULL);
                changed_bal(GTK_ADJUSTMENT(adj_bal), NULL);
	        changed_pos(GTK_ADJUSTMENT(adj_pos), NULL);
        }

        /* change color of active song in playlist */
        if(override_skin_settings && (gdk_color_parse(activesong_color, &color) == TRUE)) {
                play_list->style->fg[SELECTED].red = color.red;
                play_list->style->fg[SELECTED].green = color.green;
                play_list->style->fg[SELECTED].blue = color.blue;
                set_playlist_color();
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


gint
timeout_callback(gpointer data) {

	long pos;
	long n;
	long n_items;
	char * str;
	char cmd;
	cue_t cue;
	GtkTreeIter iter;
	static double left_gain_shadow;
	static double right_gain_shadow;
	static int update_pending = 0;
	static int jack_popup_beenthere = 0;
	char rcmd;
	static char cmdbuf[MAXLEN];
	int rcv_count;
	static int last_rcmd_loadenq = 0;


	while (jack_ringbuffer_read_space(rb_disk2gui)) {
		jack_ringbuffer_read(rb_disk2gui, &recv_cmd, 1);
		switch (recv_cmd) {

		case CMD_FILEREQ:
			cmd = CMD_CUE;
			cue.filename = NULL;
			cue.voladj = 0.0f;

			if (!is_file_loaded)
				break; /* ignore leftover filereq message */

			n = get_playing_pos(play_store);
			if (n != -1) {
				if ((!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) &&
				    (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_button)))) {
					/* normal or list repeat mode */
					gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter,
								      NULL, n);
					gtk_list_store_set(play_store, &iter, 2, pl_color_inactive, -1);
                                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_NORMAL, -1);
					if (gtk_tree_model_iter_next(GTK_TREE_MODEL(play_store), &iter)) {
						gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
                                                gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
						gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
								   1, &str, 3, &(cue.voladj), -1);
						cue.filename = strdup(g_locale_from_utf8(str, -1, NULL, NULL, NULL));
						strncpy(current_file, str, MAXLEN-1);
						g_free(str);
						is_file_loaded = 1;
					} else {
						if (!gtk_toggle_button_get_active(
							    GTK_TOGGLE_BUTTON(repeat_all_button))) {
							/* normal mode */
							is_file_loaded = 0;
							current_file[0] = '\0';
							allow_seeks = 1;
							changed_pos(GTK_ADJUSTMENT(adj_pos), NULL);
							zero_displays();
							g_signal_handler_block(G_OBJECT(play_button),
									       play_id);
							gtk_toggle_button_set_active(
								GTK_TOGGLE_BUTTON(play_button), FALSE);
							g_signal_handler_unblock(G_OBJECT(play_button),
										 play_id);
						} else {
							/* list repeat mode */
							if (gtk_tree_model_get_iter_first(
								    GTK_TREE_MODEL(play_store),&iter)) {
								gtk_list_store_set(play_store, &iter, 2,
										   pl_color_active, -1);
                                                                gtk_list_store_set(play_store, &iter, 7, 
                                                                                   PANGO_WEIGHT_BOLD, -1);
								gtk_tree_model_get(GTK_TREE_MODEL(play_store),
								   &iter, 1, &str, 3, &(cue.voladj), -1);
								cue.filename = strdup(
									g_locale_from_utf8(str, -1, NULL, NULL, NULL));
								strncpy(current_file, str, MAXLEN-1);
								g_free(str);
							} else {
								is_file_loaded = 0;
								current_file[0] = '\0';
								allow_seeks = 1;
								changed_pos(GTK_ADJUSTMENT(adj_pos), NULL);
								zero_displays();
								g_signal_handler_block(
									G_OBJECT(play_button), play_id);
								gtk_toggle_button_set_active(
									GTK_TOGGLE_BUTTON(play_button),
									FALSE);
								g_signal_handler_unblock(
									G_OBJECT(play_button), play_id);
							}
						}
					}
				} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_button))) {
					/* repeat mode */
					gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter,
								      NULL, n);
						gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
								   1, &str, 3, &(cue.voladj), -1);
						cue.filename = strdup(g_locale_from_utf8(str, -1, NULL, NULL, NULL));
						strncpy(current_file, str, MAXLEN-1);
						g_free(str);
						is_file_loaded = 1;
				} else {
					/* shuffle mode */
					gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter,
								      NULL, n);
					gtk_list_store_set(play_store, &iter, 2, pl_color_inactive, -1);
                                        gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_NORMAL, -1);

					n_items = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store),
										 NULL);
					if (n_items) {
						n = (double)rand() * n_items / RAND_MAX;
						if (n == n_items)
							--n;
						gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store),
									      &iter, NULL, n);
						gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
                                                gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
						gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
								   1, &str, 3, &(cue.voladj), -1);
						cue.filename = strdup(g_locale_from_utf8(str, -1, NULL, NULL, NULL));
						strncpy(current_file, str, MAXLEN-1);
						g_free(str);
						is_file_loaded = 1;
					} else {
						is_file_loaded = 0;
						current_file[0] = '\0';
						zero_displays();
						g_signal_handler_block(G_OBJECT(play_button), play_id);
						gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
									     FALSE);
						g_signal_handler_unblock(G_OBJECT(play_button), play_id);
					}
				}
			} else {
				if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shuffle_button))) {
					/* normal or repeat mode */
					if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(play_store),&iter)) {
						gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
                                                gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
						gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
								   1, &str, 3, &(cue.voladj), -1);
						cue.filename = strdup(g_locale_from_utf8(str, -1, NULL, NULL, NULL));
						strncpy(current_file, str, MAXLEN-1);
						g_free(str);
					} else {
						is_file_loaded = 0;
						current_file[0] = '\0';
						allow_seeks = 1;
						changed_pos(GTK_ADJUSTMENT(adj_pos), NULL);
						zero_displays();
						g_signal_handler_block(G_OBJECT(play_button), play_id);
						gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
									     FALSE);
						g_signal_handler_unblock(G_OBJECT(play_button), play_id);
					}
				} else { /* shuffle mode */
					n_items = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(play_store),
										 NULL);
					if (n_items) {
						n = (double)rand() * n_items / RAND_MAX;
						if (n == n_items)
							--n;
						gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store),
									      &iter, NULL, n);
						gtk_list_store_set(play_store, &iter, 2, pl_color_active, -1);
                                                gtk_list_store_set(play_store, &iter, 7, PANGO_WEIGHT_BOLD, -1);
						gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter,
								   1, &str, 3, &(cue.voladj), -1);
						cue.filename = strdup(g_locale_from_utf8(str, -1, NULL, NULL, NULL));
						strncpy(current_file, str, MAXLEN-1);
						g_free(str);
						is_file_loaded = 1;
					} else {
						is_file_loaded = 0;
						current_file[0] = '\0';
						zero_displays();
						g_signal_handler_block(G_OBJECT(play_button), play_id);
						gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
									     FALSE);
						g_signal_handler_unblock(G_OBJECT(play_button), play_id);
					}
				}
			}

			if (cue.filename != NULL) {
				jack_ringbuffer_write(rb_gui2disk, &cmd, sizeof(char));
				jack_ringbuffer_write(rb_gui2disk, (void *)&cue, sizeof(cue_t));
			} else {
				send_cmd = CMD_STOPWOFL;
				jack_ringbuffer_write(rb_gui2disk, &send_cmd, sizeof(char));
			}

			if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
				pthread_cond_signal(&disk_thread_wake);
				pthread_mutex_unlock(&disk_thread_lock);
			}

			break;

		case CMD_FILEINFO:
			while (jack_ringbuffer_read_space(rb_disk2gui) < sizeof(fileinfo_t))
				;
			jack_ringbuffer_read(rb_disk2gui, (char *)&fileinfo, sizeof(fileinfo_t));

			total_samples = fileinfo.total_samples;
			status.samples_left = fileinfo.total_samples;
			status.sample_offset = 0;
			fresh_new_file = fresh_new_file_prev = 0;
			break;

		case CMD_STATUS:
			while (jack_ringbuffer_read_space(rb_disk2gui) < sizeof(status_t))
				;
			jack_ringbuffer_read(rb_disk2gui, (char *)&status, sizeof(status_t));

			pos = total_samples - status.samples_left;

			if ((is_file_loaded) && (status.samples_left < 2*status.sample_offset)) {
				allow_seeks = 0;
			} else {
				allow_seeks = 1;
			}

			if ((!fresh_new_file) && (pos > status.sample_offset))
				fresh_new_file = 1;

			if (fresh_new_file && !fresh_new_file_prev) {
				disp_info = fileinfo;
				disp_samples = total_samples;
				if (pos > status.sample_offset)
					disp_pos = pos - status.sample_offset;
				else
					disp_pos = 0;
				refresh_displays();
			} else {
				if (pos > status.sample_offset)
					disp_pos = pos - status.sample_offset;
				else
					disp_pos = 0;

				if (is_file_loaded)
					refresh_time_displays();
			}

			fresh_new_file_prev = fresh_new_file;

			if (refresh_scale && GTK_IS_ADJUSTMENT(adj_pos)) {
				gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_pos),
							 100.0f * (double)(pos) / total_samples);
			}
			break;

		default:
			fprintf(stderr, "gui: unexpected command %d recv'd from disk\n", recv_cmd);
			break;
		}
	}
        /* update volume & balance if necessary */
	if ((vol != vol_prev) || (bal != bal_prev) || (update_pending)) {
		vol_prev = vol;
		vol_lin = (vol < -40.5f) ? 0 : db2lin(vol);
		bal_prev = bal;
		if (bal >= 0.0f) {
			left_gain_shadow = vol_lin * db2lin(-0.4f * bal);
			right_gain_shadow = vol_lin;
		} else {
			left_gain_shadow = vol_lin;
			right_gain_shadow = vol_lin * db2lin(0.4f * bal);
		}
		if (pthread_mutex_trylock(&output_thread_lock) == 0) {
			left_gain = left_gain_shadow;
			right_gain = right_gain_shadow;
			pthread_mutex_unlock(&output_thread_lock);
			update_pending = 0;
		} else {
			update_pending = 1;
		}
	}

	/* receive and execute remote commands, if any */
	rcv_count = 0;
	while (((rcmd = receive_message(aqualung_socket_fd, cmdbuf)) != 0) && (rcv_count < MAX_RCV_COUNT)) {
		switch (rcmd) {
		case RCMD_BACK:
			prev_event(NULL, NULL, NULL);
			last_rcmd_loadenq = 0;
			break;
		case RCMD_PLAY:
			if (last_rcmd_loadenq != 2) {
				if (is_paused) {
					stop_event(NULL, NULL, NULL);
				}
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
					!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button)));
			}
			last_rcmd_loadenq = 0;
			break;
		case RCMD_PAUSE:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause_button),
			        !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pause_button)));
			last_rcmd_loadenq = 0;
			break;
		case RCMD_STOP:
			stop_event(NULL, NULL, NULL);
			last_rcmd_loadenq = 0;
			break;
		case RCMD_FWD:
			next_event(NULL, NULL, NULL);
			last_rcmd_loadenq = 0;
			break;
		case RCMD_LOAD:
			add_to_playlist(cmdbuf, 0);
			last_rcmd_loadenq = 1;
			break;
		case RCMD_ENQUEUE:
			add_to_playlist(cmdbuf, 1);
			if (last_rcmd_loadenq != 1)
				last_rcmd_loadenq = 2;
			break;
		case RCMD_VOLADJ:
			adjust_remote_volume(cmdbuf);
			last_rcmd_loadenq = 0;
			break;
		case RCMD_QUIT:
			main_window_close(NULL, NULL);
			last_rcmd_loadenq = 0;
			break;
		}
		++rcv_count;
	}

	if (immediate_start) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(play_button),
		        !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(play_button)));
		immediate_start = 0;
	}

	/* check for JACK shutdown condition */
	if (output == JACK_DRIVER) {
		if (pthread_mutex_trylock(&output_thread_lock) == 0) {
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
			pthread_mutex_unlock(&output_thread_lock);
		}
	}
	return TRUE;
}



void
run_gui(void) {

	gtk_main();

	return;
}



void
save_config(void) {

        xmlDocPtr doc;
        xmlNodePtr root;
        char c, d;
        FILE * fin;
        FILE * fout;
        char tmpname[MAXLEN];
        char config_file[MAXLEN];
	char str[32];


        sprintf(config_file, "%s/config.xml", confdir);

        doc = xmlNewDoc((const xmlChar *) "1.0");
        root = xmlNewNode(NULL, (const xmlChar *) "aqualung_config");
        xmlDocSetRootElement(doc, root);


        xmlNewTextChild(root, NULL, (const xmlChar *) "default_param", (xmlChar *) default_param);
        xmlNewTextChild(root, NULL, (const xmlChar *) "title_format", (xmlChar *) title_format);
        xmlNewTextChild(root, NULL, (const xmlChar *) "skin", (xmlChar *) skin);

	snprintf(str, 31, "%d", src_type);
        xmlNewTextChild(root, NULL, (const xmlChar *) "src_type", (xmlChar *) str);

	snprintf(str, 31, "%d", ladspa_is_postfader);
        xmlNewTextChild(root, NULL, (const xmlChar *) "ladspa_is_postfader", (xmlChar *) str);
	snprintf(str, 31, "%d", auto_save_playlist);
        xmlNewTextChild(root, NULL, (const xmlChar *) "auto_save_playlist", (xmlChar *) str);
	snprintf(str, 31, "%d", show_rva_in_playlist);
        xmlNewTextChild(root, NULL, (const xmlChar *) "show_rva_in_playlist", (xmlChar *) str);
	snprintf(str, 31, "%d", show_length_in_playlist);
        xmlNewTextChild(root, NULL, (const xmlChar *) "show_length_in_playlist", (xmlChar *) str);

	snprintf(str, 31, "%d", auto_use_meta_artist);
        xmlNewTextChild(root, NULL, (const xmlChar *) "auto_use_meta_artist", (xmlChar *) str);
	snprintf(str, 31, "%d", auto_use_meta_record);
        xmlNewTextChild(root, NULL, (const xmlChar *) "auto_use_meta_record", (xmlChar *) str);
	snprintf(str, 31, "%d", auto_use_meta_track);
        xmlNewTextChild(root, NULL, (const xmlChar *) "auto_use_meta_track", (xmlChar *) str);

	snprintf(str, 31, "%d", auto_use_ext_meta_artist);
        xmlNewTextChild(root, NULL, (const xmlChar *) "auto_use_ext_meta_artist", (xmlChar *) str);
	snprintf(str, 31, "%d", auto_use_ext_meta_record);
        xmlNewTextChild(root, NULL, (const xmlChar *) "auto_use_ext_meta_record", (xmlChar *) str);
	snprintf(str, 31, "%d", auto_use_ext_meta_track);
        xmlNewTextChild(root, NULL, (const xmlChar *) "auto_use_ext_meta_track", (xmlChar *) str);

	snprintf(str, 31, "%d", enable_tooltips);
        xmlNewTextChild(root, NULL, (const xmlChar *) "enable_tooltips", (xmlChar *) str);

	snprintf(str, 31, "%d", buttons_at_the_bottom);
        xmlNewTextChild(root, NULL, (const xmlChar *) "buttons_at_the_bottom", (xmlChar *) str);

	snprintf(str, 31, "%d", hide_comment_pane_shadow);
        xmlNewTextChild(root, NULL, (const xmlChar *) "hide_comment_pane", (xmlChar *) str);

	snprintf(str, 31, "%d", override_skin_settings);
        xmlNewTextChild(root, NULL, (const xmlChar *) "override_skin_settings", (xmlChar *) str);

	snprintf(str, 31, "%d", replaygain_tag_to_use);
        xmlNewTextChild(root, NULL, (const xmlChar *) "replaygain_tag_to_use", (xmlChar *) str);

	snprintf(str, 31, "%f", vol);
        xmlNewTextChild(root, NULL, (const xmlChar *) "volume", (xmlChar *) str);
	snprintf(str, 31, "%f", bal);
        xmlNewTextChild(root, NULL, (const xmlChar *) "balance", (xmlChar *) str);

	snprintf(str, 31, "%d", rva_is_enabled);
        xmlNewTextChild(root, NULL, (const xmlChar *) "rva_is_enabled", (xmlChar *) str);
	snprintf(str, 31, "%d", rva_env);
        xmlNewTextChild(root, NULL, (const xmlChar *) "rva_env", (xmlChar *) str);
	snprintf(str, 31, "%f", rva_refvol);
        xmlNewTextChild(root, NULL, (const xmlChar *) "rva_refvol", (xmlChar *) str);
	snprintf(str, 31, "%f", rva_steepness);
        xmlNewTextChild(root, NULL, (const xmlChar *) "rva_steepness", (xmlChar *) str);
	snprintf(str, 31, "%d", rva_use_averaging);
        xmlNewTextChild(root, NULL, (const xmlChar *) "rva_use_averaging", (xmlChar *) str);
	snprintf(str, 31, "%d", rva_use_linear_thresh);
        xmlNewTextChild(root, NULL, (const xmlChar *) "rva_use_linear_thresh", (xmlChar *) str);
	snprintf(str, 31, "%f", rva_avg_linear_thresh);
        xmlNewTextChild(root, NULL, (const xmlChar *) "rva_avg_linear_thresh", (xmlChar *) str);
	snprintf(str, 31, "%f", rva_avg_stddev_thresh);
        xmlNewTextChild(root, NULL, (const xmlChar *) "rva_avg_stddev_thresh", (xmlChar *) str);

	snprintf(str, 31, "%d", main_pos_x);
        xmlNewTextChild(root, NULL, (const xmlChar *) "main_pos_x", (xmlChar *) str);
	snprintf(str, 31, "%d", main_pos_y);
        xmlNewTextChild(root, NULL, (const xmlChar *) "main_pos_y", (xmlChar *) str);

	snprintf(str, 31, "%d", main_size_x);
        xmlNewTextChild(root, NULL, (const xmlChar *) "main_size_x", (xmlChar *) str);
	if (playlist_is_embedded && !playlist_is_embedded_shadow && playlist_on) {
		snprintf(str, 31, "%d", main_size_y - playlist_window->allocation.height - 6);
	} else {
		snprintf(str, 31, "%d", main_size_y);
	}
        xmlNewTextChild(root, NULL, (const xmlChar *) "main_size_y", (xmlChar *) str);

	snprintf(str, 31, "%d", browser_pos_x);
        xmlNewTextChild(root, NULL, (const xmlChar *) "browser_pos_x", (xmlChar *) str);
	snprintf(str, 31, "%d", browser_pos_y);
        xmlNewTextChild(root, NULL, (const xmlChar *) "browser_pos_y", (xmlChar *) str);
	snprintf(str, 31, "%d", browser_size_x);
        xmlNewTextChild(root, NULL, (const xmlChar *) "browser_size_x", (xmlChar *) str);
	snprintf(str, 31, "%d", browser_size_y);
        xmlNewTextChild(root, NULL, (const xmlChar *) "browser_size_y", (xmlChar *) str);
	snprintf(str, 31, "%d", browser_on);
        xmlNewTextChild(root, NULL, (const xmlChar *) "browser_is_visible", (xmlChar *) str);
	snprintf(str, 31, "%d", browser_paned_pos);
        xmlNewTextChild(root, NULL, (const xmlChar *) "browser_paned_pos", (xmlChar *) str);
	snprintf(str, MAX_FONTNAME_LEN, "%s", browser_font);
        xmlNewTextChild(root, NULL, (const xmlChar *) "browser_font", (xmlChar *) str);

	snprintf(str, 31, "%d", playlist_pos_x);
        xmlNewTextChild(root, NULL, (const xmlChar *) "playlist_pos_x", (xmlChar *) str);
	snprintf(str, 31, "%d", playlist_pos_y);
        xmlNewTextChild(root, NULL, (const xmlChar *) "playlist_pos_y", (xmlChar *) str);
	snprintf(str, 31, "%d", playlist_size_x);
        xmlNewTextChild(root, NULL, (const xmlChar *) "playlist_size_x", (xmlChar *) str);
	snprintf(str, 31, "%d", playlist_size_y);
        xmlNewTextChild(root, NULL, (const xmlChar *) "playlist_size_y", (xmlChar *) str);
	snprintf(str, 31, "%d", playlist_on);
        xmlNewTextChild(root, NULL, (const xmlChar *) "playlist_is_visible", (xmlChar *) str);
	snprintf(str, 31, "%d", playlist_is_embedded_shadow);
        xmlNewTextChild(root, NULL, (const xmlChar *) "playlist_is_embedded", (xmlChar *) str);
	snprintf(str, 31, "%d", enable_playlist_statusbar_shadow);
        xmlNewTextChild(root, NULL, (const xmlChar *) "enable_playlist_statusbar", (xmlChar *) str);
	snprintf(str, MAX_FONTNAME_LEN, "%s", playlist_font);
        xmlNewTextChild(root, NULL, (const xmlChar *) "playlist_font", (xmlChar *) str);

	snprintf(str, MAX_FONTNAME_LEN, "%s", bigtimer_font);
        xmlNewTextChild(root, NULL, (const xmlChar *) "bigtimer_font", (xmlChar *) str);
	snprintf(str, MAX_FONTNAME_LEN, "%s", smalltimer_font);
        xmlNewTextChild(root, NULL, (const xmlChar *) "smalltimer_font", (xmlChar *) str);

	snprintf(str, MAX_COLORNAME_LEN, "%s", activesong_color);
        xmlNewTextChild(root, NULL, (const xmlChar *) "activesong_color", (xmlChar *) str);

	snprintf(str, 31, "%d", repeat_on);
        xmlNewTextChild(root, NULL, (const xmlChar *) "repeat_on", (xmlChar *) str);
	snprintf(str, 31, "%d", repeat_all_on);
        xmlNewTextChild(root, NULL, (const xmlChar *) "repeat_all_on", (xmlChar *) str);
	snprintf(str, 31, "%d", shuffle_on);
        xmlNewTextChild(root, NULL, (const xmlChar *) "shuffle_on", (xmlChar *) str);

	snprintf(str, 31, "%d", time_idx[0]);
        xmlNewTextChild(root, NULL, (const xmlChar *) "time_idx_0", (xmlChar *) str);
	snprintf(str, 31, "%d", time_idx[1]);
        xmlNewTextChild(root, NULL, (const xmlChar *) "time_idx_1", (xmlChar *) str);
	snprintf(str, 31, "%d", time_idx[2]);
        xmlNewTextChild(root, NULL, (const xmlChar *) "time_idx_2", (xmlChar *) str);

	snprintf(str, 31, "%d", plcol_idx[0]);
        xmlNewTextChild(root, NULL, (const xmlChar *) "plcol_idx_0", (xmlChar *) str);
	snprintf(str, 31, "%d", plcol_idx[1]);
        xmlNewTextChild(root, NULL, (const xmlChar *) "plcol_idx_1", (xmlChar *) str);
	snprintf(str, 31, "%d", plcol_idx[2]);
        xmlNewTextChild(root, NULL, (const xmlChar *) "plcol_idx_2", (xmlChar *) str);


        sprintf(tmpname, "%s/config.xml.temp", confdir);
        xmlSaveFormatFile(tmpname, doc, 1);

        if ((fin = fopen(config_file, "rt")) == NULL) {
                fprintf(stderr, "Error opening file: %s\n", config_file);
                return;
        }
        if ((fout = fopen(tmpname, "rt")) == NULL) {
                fprintf(stderr, "Error opening file: %s\n", tmpname);
                return;
        }

        c = 0; d = 0;
        while (((c = fgetc(fin)) != EOF) && ((d = fgetc(fout)) != EOF)) {
                if (c != d) {
                        fclose(fin);
                        fclose(fout);
                        unlink(config_file);
                        rename(tmpname, config_file);
                        return;
                }
        }

        fclose(fin);
        fclose(fout);
        unlink(tmpname);
}


void
load_config(void) {

        xmlDocPtr doc;
        xmlNodePtr cur;
        xmlNodePtr root;
	xmlChar * key;
        char config_file[MAXLEN];
        FILE * f;

        sprintf(config_file, "%s/config.xml", confdir);

        if ((f = fopen(config_file, "rt")) == NULL) {
		/* no warning -- done that in core.c::load_default_cl() */
                doc = xmlNewDoc((const xmlChar *) "1.0");
                root = xmlNewNode(NULL, (const xmlChar *) "aqualung_config");
                xmlDocSetRootElement(doc, root);
                xmlSaveFormatFile(config_file, doc, 1);
                return;
        }
        fclose(f);

        doc = xmlParseFile(config_file);
        if (doc == NULL) {
                fprintf(stderr, "An XML error occured while parsing %s\n", config_file);
                return;
        }

        cur = xmlDocGetRootElement(doc);
        if (cur == NULL) {
                fprintf(stderr, "load_config: empty XML document\n");
                xmlFreeDoc(doc);
                return;
        }

        if (xmlStrcmp(cur->name, (const xmlChar *)"aqualung_config")) {
                fprintf(stderr, "load_config: XML document of the wrong type, "
			"root node != aqualung_config\n");
                xmlFreeDoc(doc);
                return;
        }


	default_param[0] = '\0';
	title_format[0] = '\0';
	skin[0] = '\0';
	vol = 0.0f;
	bal = 0.0f;
	browser_paned_pos = 250;
        enable_tooltips = 1;
	hide_comment_pane = hide_comment_pane_shadow = 0;
        buttons_at_the_bottom = 1;
        override_skin_settings = 0;


        cur = cur->xmlChildrenNode;
        while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"default_param"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(default_param, (char *) key, MAXLEN-1);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"title_format"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(title_format, (char *) key, MAXLEN-1);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"skin"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(skin, (char *) key, MAXLEN-1);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"src_type"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if ((key != NULL) && (!src_type_parsed))
				sscanf((char *) key, "%d", &src_type);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"ladspa_is_postfader"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &ladspa_is_postfader);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"auto_save_playlist"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &auto_save_playlist);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"auto_use_meta_artist"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &auto_use_meta_artist);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"auto_use_meta_record"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &auto_use_meta_record);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"auto_use_meta_track"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &auto_use_meta_track);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"auto_use_ext_meta_artist"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &auto_use_ext_meta_artist);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"auto_use_ext_meta_record"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &auto_use_ext_meta_record);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"auto_use_ext_meta_track"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &auto_use_ext_meta_track);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"show_rva_in_playlist"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &show_rva_in_playlist);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"show_length_in_playlist"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &show_length_in_playlist);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"enable_tooltips"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				sscanf((char *) key, "%d", &enable_tooltips);
			}
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"buttons_at_the_bottom"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				sscanf((char *) key, "%d", &buttons_at_the_bottom);
			}
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"hide_comment_pane"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				sscanf((char *) key, "%d", &hide_comment_pane);
				hide_comment_pane_shadow = hide_comment_pane;
			}
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"override_skin_settings"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				sscanf((char *) key, "%d", &override_skin_settings);
			}
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"replaygain_tag_to_use"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &replaygain_tag_to_use);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"volume"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				vol = convf((char *) key);
			}
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"balance"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				bal = convf((char *) key);
			}
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"rva_is_enabled"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &rva_is_enabled);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"rva_env"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &rva_env);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"rva_refvol"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				rva_refvol = convf((char *) key);
			}
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"rva_steepness"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				rva_steepness = convf((char *) key);
			}
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"rva_use_averaging"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &rva_use_averaging);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"rva_use_linear_thresh"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
				sscanf((char *) key, "%d", &rva_use_linear_thresh);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"rva_avg_linear_thresh"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				rva_avg_linear_thresh = convf((char *) key);
			}
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"rva_avg_stddev_thresh"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
				rva_avg_stddev_thresh = convf((char *) key);
			}
                        xmlFree(key);
                }

                if ((!xmlStrcmp(cur->name, (const xmlChar *)"main_pos_x"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &main_pos_x);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"main_pos_y"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &main_pos_y);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"main_size_x"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &main_size_x);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"main_size_y"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &main_size_y);
                        xmlFree(key);
                }

                if ((!xmlStrcmp(cur->name, (const xmlChar *)"browser_pos_x"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &browser_pos_x);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"browser_pos_y"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &browser_pos_y);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"browser_size_x"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &browser_size_x);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"browser_size_y"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &browser_size_y);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"browser_is_visible"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &browser_on);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"browser_paned_pos"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &browser_paned_pos);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"browser_font"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(browser_font, (char *) key, MAX_FONTNAME_LEN-1);
                        xmlFree(key);
                }

                if ((!xmlStrcmp(cur->name, (const xmlChar *)"playlist_pos_x"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &playlist_pos_x);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"playlist_pos_y"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &playlist_pos_y);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"playlist_size_x"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &playlist_size_x);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"playlist_size_y"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &playlist_size_y);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"playlist_is_visible"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &playlist_on);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"playlist_is_embedded"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                sscanf((char *) key, "%d", &playlist_is_embedded);
				playlist_is_embedded_shadow = playlist_is_embedded;
			}
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"enable_playlist_statusbar"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                sscanf((char *) key, "%d", &enable_playlist_statusbar);
				enable_playlist_statusbar_shadow = enable_playlist_statusbar;
			}
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"playlist_font"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(playlist_font, (char *) key, MAX_FONTNAME_LEN-1);
                        xmlFree(key);
                }

                if ((!xmlStrcmp(cur->name, (const xmlChar *)"bigtimer_font"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(bigtimer_font, (char *) key, MAX_FONTNAME_LEN-1);
                        xmlFree(key);
                }

                if ((!xmlStrcmp(cur->name, (const xmlChar *)"smalltimer_font"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(smalltimer_font, (char *) key, MAX_FONTNAME_LEN-1);
                        xmlFree(key);
                }

                if ((!xmlStrcmp(cur->name, (const xmlChar *)"activesong_color"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(activesong_color, (char *) key, MAX_COLORNAME_LEN-1);
                        xmlFree(key);
                }

                if ((!xmlStrcmp(cur->name, (const xmlChar *)"repeat_on"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &repeat_on);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"repeat_all_on"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &repeat_all_on);
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"shuffle_on"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &shuffle_on);
                        xmlFree(key);
                }

                if ((!xmlStrcmp(cur->name, (const xmlChar *)"time_idx_0"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &(time_idx[0]));
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"time_idx_1"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &(time_idx[1]));
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"time_idx_2"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &(time_idx[2]));
                        xmlFree(key);
                }

                if ((!xmlStrcmp(cur->name, (const xmlChar *)"plcol_idx_0"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &(plcol_idx[0]));
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"plcol_idx_1"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &(plcol_idx[1]));
                        xmlFree(key);
                }
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"plcol_idx_2"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf((char *) key, "%d", &(plcol_idx[2]));
                        xmlFree(key);
                }
                cur = cur->next;
        }

        xmlFreeDoc(doc);
        return;
}


GtkWidget* 
gui_stock_label_button(gchar *blabel, const gchar *bstock) {

	GtkWidget *button;
	GtkWidget *alignment;
	GtkWidget *hbox;
	GtkWidget *image;

	button = g_object_new (GTK_TYPE_BUTTON, "visible", TRUE, NULL);

	alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (alignment), hbox);

	image = gtk_image_new_from_stock (bstock, GTK_ICON_SIZE_BUTTON);
	if (image)
		gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);

	if (blabel)
		gtk_box_pack_start (GTK_BOX (hbox),
		g_object_new (GTK_TYPE_LABEL, "label", blabel, "use_underline", TRUE, NULL),
		FALSE, TRUE, 0);

	gtk_widget_show_all (alignment);
	gtk_container_add (GTK_CONTAINER (button), alignment);

	return button;
}


void
set_sliders_width(void) {

        GTK_SCALE(scale_vol)->range.slider_size_fixed = 1;
	GTK_SCALE(scale_vol)->range.min_slider_size = 11;
	gtk_widget_queue_draw(scale_vol);
	deflicker();
	GTK_SCALE(scale_bal)->range.slider_size_fixed = 1;
	GTK_SCALE(scale_bal)->range.min_slider_size = 11;
	gtk_widget_queue_draw(scale_bal);
	deflicker();

}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

