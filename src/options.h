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

#ifndef AQUALUNG_OPTIONS_H
#define AQUALUNG_OPTIONS_H

#include "common.h"

typedef struct {
	int button_nr;
	int command;
} mouse_button_command_t;

typedef struct {

	/* home directory */
	char home[MAXLEN];

	/* normally $HOME/.aqualung */
	char confdir[MAXLEN];

	/* current working directory when program is started */
	char cwd[MAXLEN];

	/* to keep track of file selector dialogs; start with $HOME */
	char audiodir[MAXLEN];
	char currdir[MAXLEN];
	char exportdir[MAXLEN];
	char plistdir[MAXLEN];
	char podcastdir[MAXLEN];
	char ripdir[MAXLEN];
	char storedir[MAXLEN];

	/* directory of skin in use */
	char skin[MAXLEN];

	/* Misc - not accessible from the Settings dialog */
	float vol;
	float bal;
	int time_idx[3];
	int main_pos_x;
	int main_pos_y;
	int main_size_x;
	int main_size_y;
	int browser_pos_x;
	int browser_pos_y;
	int browser_size_x;
	int browser_size_y;
	int browser_on;
	int browser_paned_pos;
	int playlist_pos_x;
	int playlist_pos_y;
	int playlist_size_x;
	int playlist_size_y;
	int playlist_on;
	int repeat_on;
	int repeat_all_on;
	int shuffle_on;

	int search_pl_flags;
	int search_ms_flags;
        int ifpmanager_size_x;
        int ifpmanager_size_y;
	float loop_range_start;
	float loop_range_end;
	int wm_systray_warn;
	int podcasts_autocheck;

	int cdrip_deststore;
        int cdrip_file_format;
        int cdrip_bitrate;
        int cdrip_vbr;
        int cdrip_metadata;

	char export_template[MAXLEN];
	int export_subdir_artist;
	int export_subdir_album;
	int export_subdir_limit;
        int export_file_format;
        int export_bitrate;
        int export_vbr;
        int export_metadata;
	int export_filter_same;
	int export_excl_enabled;
	char export_excl_pattern[MAXLEN];

	int batch_tag_flags;

	/* General */
	char title_format[MAXLEN];
	char default_param[MAXLEN];
	int enable_tooltips;
   	int disable_buttons_relief;
	int combine_play_pause;
	int combine_play_pause_shadow;
	int simple_view_in_fx;
	int simple_view_in_fx_shadow;
	int show_sn_title;
	int united_minimization;
	int show_hidden;
   	int main_window_always_on_top;
	int tags_tab_first;
	int dont_show_cover;
	int use_external_cover_first;
	int show_cover_for_ms_tracks_only;
	int use_systray;
	int systray_start_minimized;
	int systray_mouse_wheel_horizontal;
	int systray_mouse_wheel_vertical;
	int systray_mouse_buttons_count;
	mouse_button_command_t * systray_mouse_buttons;

	/* Playlist */
	int playlist_is_embedded;
	int playlist_is_embedded_shadow;
	int buttons_at_the_bottom;
	int buttons_at_the_bottom_shadow;
	int playlist_always_show_tabs;
	int playlist_show_close_button_in_tab;
	int playlist_is_tree;
	int album_shuffle_mode;
	int auto_save_playlist;
	int playlist_auto_save;
	int playlist_auto_save_int;
	int enable_playlist_statusbar;
	int enable_playlist_statusbar_shadow;
	int pl_statusbar_show_size;
	int show_rva_in_playlist;
	int show_length_in_playlist;
	int show_active_track_name_in_bold;
	int auto_roll_to_active_track;
	int enable_pl_rules_hint;
	int plcol_idx[3];

	/* Music Store */
	int hide_comment_pane;
	int hide_comment_pane_shadow;
	int enable_mstore_statusbar;
	int enable_mstore_statusbar_shadow;
	int ms_statusbar_show_size;
   	int enable_mstore_toolbar;
	int enable_mstore_toolbar_shadow;
	int autoexpand_stores;
	int enable_ms_rules_hint;
	int enable_ms_tree_icons;
	int enable_ms_tree_icons_shadow;
	int ms_confirm_removal;
	int cover_width;
	int magnify_smaller_images;

	/* DSP */
	int ladspa_is_postfader;
	int src_type;

	/* RVA */
	int rva_is_enabled;
	int rva_env;
	float rva_refvol;
	float rva_steepness;
	int rva_use_averaging;
	int rva_use_linear_thresh;
	float rva_avg_linear_thresh;
	float rva_avg_stddev_thresh;
	float rva_no_rva_voladj;

	/* Metadata */
	int replaygain_tag_to_use;
	int batch_mpeg_add_id3v1;
	int batch_mpeg_add_id3v2;
	int batch_mpeg_add_ape;
	char encode_set[MAXLEN];
	int metaedit_auto_clone;
	int meta_use_basename_only;
	int meta_rm_extension;
	int meta_us_to_space;

	/* CD audio */
	int cdda_drive_speed;
	int cdda_paranoia_mode;
	int cdda_paranoia_maxretries;
	int cdda_force_drive_rescan;
	int cdda_add_to_playlist;
	int cdda_remove_from_playlist;

	/* CDDB */
	char cddb_server[MAXLEN];
	int cddb_timeout;
	char cddb_email[MAXLEN];
	char cddb_local[MAXLEN];
	int cddb_cache_only;
	int cddb_use_http;

	/* Internet */
	int inet_use_proxy;
	char inet_proxy[MAXLEN];
	int inet_proxy_port;
	char inet_noproxy_domains[MAXLEN];
	int inet_timeout;

	/* Appearance */
	int disable_skin_support_settings;
	int override_skin_settings;
	char playlist_font[MAX_FONTNAME_LEN];
	char browser_font[MAX_FONTNAME_LEN];
	char bigtimer_font[MAX_FONTNAME_LEN];
	char smalltimer_font[MAX_FONTNAME_LEN];
	char songtitle_font[MAX_FONTNAME_LEN];
	char songinfo_font[MAX_FONTNAME_LEN];
	char statusbar_font[MAX_FONTNAME_LEN];
	char song_color[MAX_COLORNAME_LEN];
	char activesong_color[MAX_COLORNAME_LEN];

	/* Lua */
	char ext_title_format_file[MAXLEN];
	int use_ext_title_format;
} options_t;

enum {
        SONG_COLOR = 1,
        ACTIVE_SONG_COLOR
};

// Mouse wheel commands on systray.
enum {
	SYSTRAY_MW_CMD_DO_NOTHING,
	SYSTRAY_MW_CMD_VOLUME,
	SYSTRAY_MW_CMD_BALANCE,
	SYSTRAY_MW_CMD_SONG_POSITION,
	SYSTRAY_MW_CMD_NEXT_PREV_SONG
};

// Mouse buttons commands on systray.
enum {
	SYSTRAY_MB_CMD_PLAY_STOP_SONG,
	SYSTRAY_MB_CMD_PLAY_PAUSE_SONG,
	SYSTRAY_MB_CMD_PREV_SONG,
	SYSTRAY_MB_CMD_NEXT_SONG,
	SYSTRAY_MB_CMD_LAST
};

enum {
	SYSTRAY_MB_COL_BUTTON,
	SYSTRAY_MB_COL_COMMAND,
	SYSTRAY_MB_LAST
};

void create_options_window(void);
void append_ms_pathlist(char * path, char * name);

void options_store_watcher_start(void);

void save_config(void);
void load_config(void);
void finalize_options(void);


#endif /* AQUALUNG_OPTIONS_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :
