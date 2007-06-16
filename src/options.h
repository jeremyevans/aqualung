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

#ifndef _OPTIONS_H
#define _OPTIONS_H

#include "common.h"
#include "utils.h"

typedef struct {

	/* home directory */
	char home[MAXLEN];
	/* normally $HOME/.aqualung */
	char confdir[MAXLEN];
	/* to keep track of file selector dialogs; starts with $HOME */
	char currdir[MAXLEN];
	/* current working directory when program is started */
	char cwd[MAXLEN];
	
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
	float loop_range_start;
	float loop_range_end;

	/* General */
	char title_format[MAXLEN];
	char default_param[MAXLEN];
	int enable_tooltips;
	int buttons_at_the_bottom;
	int buttons_at_the_bottom_shadow;
   	int disable_buttons_relief;
	int simple_view_in_fx;
	int simple_view_in_fx_shadow;
	int show_sn_title;
	int united_minimization;
	int show_hidden;
   	int main_window_always_on_top;
	int tags_tab_first;

	/* Playlist */
	int playlist_is_embedded;
	int playlist_is_embedded_shadow;
	int playlist_always_show_tabs;
	int playlist_show_close_button_in_tab;
	int playlist_is_tree;
	int album_shuffle_mode;
	int auto_save_playlist;
	int enable_playlist_statusbar;
	int enable_playlist_statusbar_shadow;
	int pl_statusbar_show_size;
	int show_rva_in_playlist;
	int show_length_in_playlist;
	int show_active_track_name_in_bold;
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
	int auto_use_meta_artist;
	int auto_use_meta_record;
	int auto_use_meta_track;
	int auto_use_ext_meta_artist;
	int auto_use_ext_meta_record;
	int auto_use_ext_meta_track;
	int replaygain_tag_to_use;

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

	/* Appearance */
	int override_skin_settings;
	char playlist_font[MAX_FONTNAME_LEN];
	char browser_font[MAX_FONTNAME_LEN];
	char bigtimer_font[MAX_FONTNAME_LEN];
	char smalltimer_font[MAX_FONTNAME_LEN];
	char songtitle_font[MAX_FONTNAME_LEN];
	char songinfo_font[MAX_FONTNAME_LEN];
	char statusbar_font[MAX_FONTNAME_LEN];
	char activesong_color[MAX_COLORNAME_LEN];

} options_t;

void create_options_window(void);
void append_ms_pathlist(char * path, char * name);

void save_config(void);
void load_config(void);

#endif /* _OPTIONS_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
