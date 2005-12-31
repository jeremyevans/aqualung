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

void create_options_window(void);

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


	/* General */
	char title_format[MAXLEN];
	char default_param[MAXLEN];
	int enable_tooltips;
	int buttons_at_the_bottom;
	int simple_view_in_fx;
	int show_sn_title;
	int united_minimization;

	/* Playlist */
	int playlist_is_embedded;
	int playlist_is_embedded_shadow;
	int auto_save_playlist;
	int enable_playlist_statusbar;
	int enable_playlist_statusbar_shadow;
	int show_rva_in_playlist;
	int show_length_in_playlist;
	int show_active_track_name_in_bold;
	int plcol_idx[3];

	/* Music Store */
	int hide_comment_pane;
	int hide_comment_pane_shadow;
	int enable_mstore_statusbar;
	int enable_mstore_statusbar_shadow;
	int autoexpand_stores;
	int show_hidden;
	int cover_width;
	int magnify_smaller_images;

	/* DSP */
	int ladspa_is_postfader;
	/* TODO: src_type should be moved here too */
	
	/* RVA */
	int rva_is_enabled;
	int rva_env;
	float rva_refvol;
	float rva_steepness;
	int rva_use_averaging;
	int rva_use_linear_thresh;
	float rva_avg_linear_thresh;
	float rva_avg_stddev_thresh;

	/* Metadata */
	int auto_use_meta_artist;
	int auto_use_meta_record;
	int auto_use_meta_track;
	int auto_use_ext_meta_artist;
	int auto_use_ext_meta_record;
	int auto_use_ext_meta_track;
	int replaygain_tag_to_use;

	/* CDDB */
	char cddb_server[MAXLEN];
	int cddb_use_http;
	int cddb_timeout;

	/* Appearance */
	int override_skin_settings;
	char playlist_font[MAX_FONTNAME_LEN];
	char browser_font[MAX_FONTNAME_LEN];
	char bigtimer_font[MAX_FONTNAME_LEN];
	char smalltimer_font[MAX_FONTNAME_LEN];
	char activesong_color[MAX_COLORNAME_LEN];

} options_t;

#endif /* _OPTIONS_H */
