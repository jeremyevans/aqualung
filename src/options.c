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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#ifdef HAVE_SRC
#include <samplerate.h>
#endif /* HAVE_SRC */

#include "common.h"
#include "gui_main.h"
#include "music_browser.h"
#include "playlist.h"
#include "i18n.h"
#include "options.h"


options_t options;
static int current_notebook_page = 0;
int appearance_changed;


#ifdef HAVE_SRC
extern int src_type;
GtkWidget * optmenu_src;
#endif /* HAVE_SRC */

extern GtkWidget * main_window;
extern GtkWidget * play_button;
extern GtkTooltips * aqualung_tooltips;

extern PangoFontDescription *fd_playlist;
extern PangoFontDescription *fd_browser;
extern PangoFontDescription *fd_bigtimer;
extern PangoFontDescription *fd_smalltimer;
extern PangoFontDescription *fd_songtitle;
extern PangoFontDescription *fd_songinfo;
extern PangoFontDescription *fd_statusbar;

extern GtkWidget * bigtimer_label;
extern GtkWidget * smalltimer_label_1;
extern GtkWidget * smalltimer_label_2;

extern GtkWidget * label_title;
extern GtkWidget * label_format;
extern GtkWidget * label_samplerate;
extern GtkWidget * label_bps;
extern GtkWidget * label_mono;
extern GtkWidget * label_output;
extern GtkWidget * label_src_type;

extern GtkWidget * statusbar_total;
extern GtkWidget * statusbar_total_label;
extern GtkWidget * statusbar_selected;
extern GtkWidget * statusbar_selected_label;

extern GtkWidget * statusbar_ms;

int auto_save_playlist_shadow;
int show_rva_in_playlist_shadow;
int pl_statusbar_show_size_shadow;
int ms_statusbar_show_size_shadow;
int show_length_in_playlist_shadow;
int show_active_track_name_in_bold_shadow;
int enable_pl_rules_hint_shadow;
int enable_ms_rules_hint_shadow;
int enable_ms_tree_icons_shadow;
int rva_is_enabled_shadow;
int rva_env_shadow;
float rva_refvol_shadow;
float rva_steepness_shadow;
int rva_use_averaging_shadow;
int rva_use_linear_thresh_shadow;
float rva_avg_linear_thresh_shadow;
float rva_avg_stddev_thresh_shadow;
int cover_width_shadow;

int restart_flag;
int override_past_state;
int track_name_in_bold_past_state;

extern int music_store_changed;

extern GtkWidget * music_tree;
extern GtkTreeStore * music_store;

extern GtkWidget * play_list;
extern GtkTreeViewColumn * track_column;
extern GtkTreeViewColumn * rva_column;
extern GtkTreeViewColumn * length_column;

extern GtkListStore * play_store;

extern GtkWidget* gui_stock_label_button(gchar *blabel, const gchar *bstock);
extern void disable_bold_font_in_playlist(void);
extern void set_sliders_width(void);
extern void playlist_selection_changed(GtkTreeSelection * sel, gpointer data);
extern void set_buttons_relief(void);
extern void show_active_position_in_playlist(void);

GtkWidget * options_window;
GtkWidget * notebook;
GtkWidget * optmenu_ladspa;
GtkWidget * optmenu_listening_env;
GtkWidget * optmenu_threshold;
GtkWidget * optmenu_replaygain;
GtkWidget * optmenu_cwidth;
GtkWidget * entry_title;
GtkWidget * entry_param;
GtkWidget * label_src;
GtkWidget * check_autoplsave;
GtkWidget * check_show_rva_in_playlist;
GtkWidget * check_pl_statusbar_show_size;
GtkWidget * check_ms_statusbar_show_size;
GtkWidget * check_show_length_in_playlist;
GtkWidget * check_show_active_track_name_in_bold;
GtkWidget * check_enable_pl_rules_hint;
GtkWidget * check_enable_ms_rules_hint;
GtkWidget * check_enable_ms_tree_icons;
GtkWidget * check_rva_is_enabled;
GtkWidget * check_rva_use_averaging;
GtkWidget * check_auto_use_meta_artist;
GtkWidget * check_auto_use_meta_record;
GtkWidget * check_auto_use_meta_track;
GtkWidget * check_auto_use_ext_meta_artist;
GtkWidget * check_auto_use_ext_meta_record;
GtkWidget * check_auto_use_ext_meta_track;
GtkWidget * check_enable_tooltips;
GtkWidget * check_show_sn_title;
GtkWidget * check_united_minimization;
GtkWidget * check_hide_comment_pane;
GtkWidget * check_enable_mstore_statusbar;
GtkWidget * check_enable_mstore_toolbar;
GtkWidget * check_expand_stores;
GtkWidget * check_show_hidden;
GtkWidget * check_tags_tab_first;
GtkWidget * check_playlist_is_embedded;
GtkWidget * check_playlist_is_tree;
GtkWidget * check_album_shuffle_mode;
GtkWidget * check_enable_playlist_statusbar;
GtkWidget * check_buttons_at_the_bottom;
GtkWidget * check_simple_view_in_fx;
GtkWidget * check_override_skin;
GtkWidget * check_magnify_smaller_images;
GtkWidget * check_main_window_always_on_top;
GtkWidget * check_disable_buttons_relief;

GtkListStore * ms_pathlist_store = NULL;
GtkTreeSelection * ms_pathlist_select;
GtkWidget * entry_ms_pathlist;

GtkWidget * cddb_server_entry;
GtkWidget * cddb_tout_spinner;
GtkWidget * cddb_proto_combo;

GtkObject * adj_refvol;
GtkObject * adj_steepness;
GtkObject * adj_linthresh;
GtkObject * adj_stdthresh;
GtkWidget * rva_drawing_area;
GdkPixmap * rva_pixmap = NULL;
GtkWidget * rva_viewport;
GtkWidget * spin_refvol;
GtkWidget * spin_steepness;
GtkWidget * spin_linthresh;
GtkWidget * spin_stdthresh;
GtkWidget * label_listening_env;
GtkWidget * label_refvol;
GtkWidget * label_steepness;
GtkWidget * label_threshold;
GtkWidget * label_linthresh;
GtkWidget * label_stdthresh;
GtkListStore * plistcol_store;

#define DEFAULT_FONT_NAME "Sans 11"

GtkWidget * entry_pl_font;
GtkWidget * entry_ms_font;
GtkWidget * entry_bt_font;
GtkWidget * entry_st_font;
GtkWidget * entry_songt_font;
GtkWidget * entry_si_font;
GtkWidget * entry_sb_font;
GtkWidget * button_pl_font;
GtkWidget * button_ms_font;
GtkWidget * button_bt_font;
GtkWidget * button_st_font;
GtkWidget * button_songt_font;
GtkWidget * button_si_font;
GtkWidget * button_sb_font;

GdkColor color;
GtkWidget * color_picker;

void draw_rva_diagram(void);
void show_restart_info(void);

GtkListStore * restart_list_store = NULL;


void
open_font_desc(void) {

        if (fd_playlist) pango_font_description_free(fd_playlist);
	fd_playlist = pango_font_description_from_string(options.playlist_font);
        if (fd_browser) pango_font_description_free(fd_browser);
	fd_browser = pango_font_description_from_string(options.browser_font);
        if (fd_bigtimer) pango_font_description_free(fd_bigtimer);
	fd_bigtimer = pango_font_description_from_string(options.bigtimer_font);
        if (fd_smalltimer) pango_font_description_free(fd_smalltimer);
	fd_smalltimer = pango_font_description_from_string(options.smalltimer_font);
        if (fd_songtitle) pango_font_description_free(fd_songtitle);
	fd_songtitle = pango_font_description_from_string(options.songtitle_font);
        if (fd_songinfo) pango_font_description_free(fd_songinfo);
	fd_songinfo = pango_font_description_from_string(options.songinfo_font);
        if (fd_statusbar) pango_font_description_free(fd_statusbar);
	fd_statusbar = pango_font_description_from_string(options.statusbar_font);
}

static gint
ok(GtkWidget * widget, gpointer data) {

	int i;
	int n;
	int n_prev = 3;
        GdkColor color;


        if (restart_flag) {
                show_restart_info();            
        }


	strncpy(options.title_format, gtk_entry_get_text(GTK_ENTRY(entry_title)), MAXLEN - 1);
	strncpy(options.default_param, gtk_entry_get_text(GTK_ENTRY(entry_param)), MAXLEN - 1);
	options.auto_save_playlist = auto_save_playlist_shadow;
	options.rva_is_enabled = rva_is_enabled_shadow;
	options.pl_statusbar_show_size = pl_statusbar_show_size_shadow;
	options.ms_statusbar_show_size = ms_statusbar_show_size_shadow;
	options.rva_env = rva_env_shadow;
	options.rva_refvol = rva_refvol_shadow;
	options.rva_steepness = rva_steepness_shadow;
	options.rva_use_averaging = rva_use_averaging_shadow;
	options.rva_use_linear_thresh = rva_use_linear_thresh_shadow;
	options.rva_avg_linear_thresh = rva_avg_linear_thresh_shadow;
	options.rva_avg_stddev_thresh = rva_avg_stddev_thresh_shadow;
	options.cover_width = cover_width_shadow;


	options.show_rva_in_playlist = show_rva_in_playlist_shadow;
	if (options.show_rva_in_playlist) {
		gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(rva_column), TRUE);
	} else {
		gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(rva_column), FALSE);
	}

	options.show_length_in_playlist = show_length_in_playlist_shadow;
	if (options.show_length_in_playlist) {
		gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(length_column), TRUE);
	} else {
		gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(length_column), FALSE);
	}

	options.show_active_track_name_in_bold = show_active_track_name_in_bold_shadow;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_show_active_track_name_in_bold))) {
		options.show_active_track_name_in_bold = 1;
	} else {
		options.show_active_track_name_in_bold = 0;
                disable_bold_font_in_playlist();
	}

	options.enable_pl_rules_hint = enable_pl_rules_hint_shadow;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_enable_pl_rules_hint))) {
		options.enable_pl_rules_hint = 1;
                gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(play_list), TRUE);
	} else {
		options.enable_pl_rules_hint = 0;
                gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(play_list), FALSE);
	}

        options.enable_ms_rules_hint = enable_ms_rules_hint_shadow;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_enable_ms_rules_hint))) {
		options.enable_ms_rules_hint = 1;
                gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(music_tree), TRUE);
	} else {
		options.enable_ms_rules_hint = 0;
                gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(music_tree), FALSE);
	}

        options.enable_ms_tree_icons = enable_ms_tree_icons_shadow;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_enable_ms_tree_icons))) {
		options.enable_ms_tree_icons = 1;
	} else {
		options.enable_ms_tree_icons = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_use_meta_artist))) {
		options.auto_use_meta_artist = 1;
	} else {
		options.auto_use_meta_artist = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_use_meta_record))) {
		options.auto_use_meta_record = 1;
	} else {
		options.auto_use_meta_record = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_use_meta_track))) {
		options.auto_use_meta_track = 1;
	} else {
		options.auto_use_meta_track = 0;
	}


	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_use_ext_meta_artist))) {
		options.auto_use_ext_meta_artist = 1;
	} else {
		options.auto_use_ext_meta_artist = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_use_ext_meta_record))) {
		options.auto_use_ext_meta_record = 1;
	} else {
		options.auto_use_ext_meta_record = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_use_ext_meta_track))) {
		options.auto_use_ext_meta_track = 1;
	} else {
		options.auto_use_ext_meta_track = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_enable_tooltips))) {
		options.enable_tooltips = 1;
                gtk_tooltips_enable(aqualung_tooltips);
	} else {
	        options.enable_tooltips = 0;
                gtk_tooltips_disable(aqualung_tooltips);
	}

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_buttons_at_the_bottom))) {
		options.buttons_at_the_bottom = 1;
	} else {
	        options.buttons_at_the_bottom = 0;
	}

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_disable_buttons_relief))) {
		options.disable_buttons_relief = 1;
                set_buttons_relief();
	} else {
	        options.disable_buttons_relief = 0;
                set_buttons_relief();
	}

#ifdef HAVE_LADSPA
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_simple_view_in_fx))) {
		options.simple_view_in_fx = 1;
	} else {
	        options.simple_view_in_fx = 0;
	}
#endif /* HAVE_LADSPA */

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_show_sn_title))) {
		options.show_sn_title = 1;
		refresh_displays();
	} else {
	        options.show_sn_title = 0;
		refresh_displays();
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_united_minimization))) {
		options.united_minimization = 1;
	} else {
	        options.united_minimization = 0;
	}

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_override_skin))) {
		options.override_skin_settings = 1;
	} else {
	        options.override_skin_settings = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_hide_comment_pane))) {
		options.hide_comment_pane_shadow = 1;
	} else {
	        options.hide_comment_pane_shadow = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_enable_mstore_statusbar))) {
		options.enable_mstore_statusbar_shadow = 1;
	} else {
	        options.enable_mstore_statusbar_shadow = 0;
	}

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_enable_mstore_toolbar))) {
		options.enable_mstore_toolbar_shadow = 1;
	} else {
	        options.enable_mstore_toolbar_shadow = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_expand_stores))) {
		options.autoexpand_stores = 1;
	} else {
		options.autoexpand_stores = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_show_hidden))) {
		options.show_hidden = 1;
	} else {
		options.show_hidden = 0;
	}

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_main_window_always_on_top))) {
		options.main_window_always_on_top = 1;
	} else {
		options.main_window_always_on_top = 0;
	}

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_tags_tab_first))) {
		options.tags_tab_first = 1;
	} else {
		options.tags_tab_first = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_playlist_is_embedded))) {
		options.playlist_is_embedded_shadow = 1;
	} else {
	        options.playlist_is_embedded_shadow = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_playlist_is_tree))) {
		options.playlist_is_tree = 1;
	} else {
	        options.playlist_is_tree = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_album_shuffle_mode))) {
		options.album_shuffle_mode = 1;
	} else {
	        options.album_shuffle_mode = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_enable_playlist_statusbar))) {
		options.enable_playlist_statusbar_shadow = 1;
	} else {
	        options.enable_playlist_statusbar_shadow = 0;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_magnify_smaller_images))) {
		options.magnify_smaller_images = 0;
	} else {
	        options.magnify_smaller_images = 1;
	}

	options.replaygain_tag_to_use = gtk_combo_box_get_active(GTK_COMBO_BOX(optmenu_replaygain));


#ifdef HAVE_CDDB
	options.cddb_use_http = gtk_combo_box_get_active(GTK_COMBO_BOX(cddb_proto_combo));
	strncpy(options.cddb_server, gtk_entry_get_text(GTK_ENTRY(cddb_server_entry)), MAXLEN-1);
	options.cddb_timeout = gtk_spin_button_get_value(GTK_SPIN_BUTTON(cddb_tout_spinner));
#endif /* HAVE_CDDB */


        for (i = 0; i < 3; i++) {

		GtkTreeIter iter;
		GtkTreeViewColumn * cols[4];
		char * pnumstr;

		cols[0] = track_column;
		cols[1] = rva_column;
		cols[2] = length_column;
		cols[3] = NULL;

		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(plistcol_store), &iter, NULL, i);
		
		gtk_tree_model_get(GTK_TREE_MODEL(plistcol_store), &iter, 1, &pnumstr, -1);
		n = atoi(pnumstr);
		g_free(pnumstr);

		gtk_tree_view_move_column_after(GTK_TREE_VIEW(play_list),
						cols[n], cols[n_prev]);

		options.plcol_idx[i] = n;
		n_prev = n;
	}	

        /* apply changes */

        if (!track_name_in_bold_past_state && options.show_active_track_name_in_bold == 1 &&
	    !appearance_changed) {

                /* reload skin */
                change_skin(options.skin);
                track_name_in_bold_past_state = 0;
        }

        if (options.override_skin_settings) {

                /* apply fonts */

                open_font_desc();

                gtk_widget_modify_font (music_tree, fd_browser);
                gtk_widget_modify_font (play_list, fd_playlist);

                gtk_widget_modify_font (bigtimer_label, fd_bigtimer);
                gtk_widget_modify_font (smalltimer_label_1, fd_smalltimer);
                gtk_widget_modify_font (smalltimer_label_2, fd_smalltimer);
                gtk_widget_modify_font (label_title, fd_songtitle);
                /* set font for song info labels */
                gtk_widget_modify_font (label_format, fd_songinfo);
                gtk_widget_modify_font (label_samplerate, fd_songinfo);
                gtk_widget_modify_font (label_bps, fd_songinfo);
                gtk_widget_modify_font (label_mono, fd_songinfo);
                gtk_widget_modify_font (label_output, fd_songinfo);
                gtk_widget_modify_font (label_src_type, fd_songinfo);
                /* set font for statusbars */
                gtk_widget_modify_font (statusbar_total, fd_statusbar);
                gtk_widget_modify_font (statusbar_total_label, fd_statusbar);
                gtk_widget_modify_font (statusbar_selected, fd_statusbar);
                gtk_widget_modify_font (statusbar_selected_label, fd_statusbar);
                gtk_widget_modify_font (statusbar_ms, fd_statusbar);

                /* apply colors */               

                if (gdk_color_parse(options.activesong_color, &color) == TRUE) {
 
                        /* sorry for this, but it's temporary workaround */
                        /* see playlist.c:1848 FIXME tag for details */

                        if(!color.red && !color.green && !color.blue)
                                color.red++;

                        play_list->style->fg[SELECTED].red = color.red;
                        play_list->style->fg[SELECTED].green = color.green;
                        play_list->style->fg[SELECTED].blue = color.blue;
                        set_playlist_color();
                }

                if (appearance_changed) {
                        change_skin(options.skin);
		}
        } else if (override_past_state) {

                /* reload skin */
                change_skin(options.skin);
                override_past_state = 0;

        } 
     
        /* always on top ? */
	if (options.main_window_always_on_top) {
                gtk_window_set_keep_above (GTK_WINDOW(main_window), TRUE);
        } else {
                gtk_window_set_keep_above (GTK_WINDOW(main_window), FALSE);
        }

        /* refresh statusbar */
        playlist_content_changed();
        playlist_selection_changed(NULL, NULL);
	music_store_set_status_bar_info();

        current_notebook_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));

	gtk_widget_destroy(options_window);

        set_sliders_width();    /* MAGIC */

	playlist_size_allocate(NULL, NULL);
        show_active_position_in_playlist();

	return TRUE;
}


static gint
cancel(GtkWidget * widget, gpointer data) {

	gtk_widget_destroy(options_window);

        set_sliders_width();    /* MAGIC */
	return TRUE;
}


static gint
options_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	switch (kevent->keyval) {

	case GDK_Escape:
		cancel(NULL, NULL);
		return TRUE;
		break;

        case GDK_Return:
	case GDK_KP_Enter:
		ok(NULL, NULL);
                return TRUE;
		break;
	}

	return FALSE;
}


void
autoplsave_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_autoplsave))) {
		auto_save_playlist_shadow = 1;
	} else {
		auto_save_playlist_shadow = 0;
	}
}


void
check_show_rva_in_playlist_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_show_rva_in_playlist))) {
		show_rva_in_playlist_shadow = 1;
	} else {
		show_rva_in_playlist_shadow = 0;
	}
}


void
check_show_length_in_playlist_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_show_length_in_playlist))) {
		show_length_in_playlist_shadow = 1;
	} else {
		show_length_in_playlist_shadow = 0;
	}
}

void
check_show_active_track_name_in_bold_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_show_active_track_name_in_bold))) {
		show_active_track_name_in_bold_shadow = 1;
	} else {
		show_active_track_name_in_bold_shadow = 0;
	}
}

void
check_pl_statusbar_show_size_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_pl_statusbar_show_size))) {
		pl_statusbar_show_size_shadow = 1;
	} else {
		pl_statusbar_show_size_shadow = 0;
	}
}

void
check_ms_statusbar_show_size_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_ms_statusbar_show_size))) {
		ms_statusbar_show_size_shadow = 1;
	} else {
		ms_statusbar_show_size_shadow = 0;
	}
}

void
check_enable_pl_rules_hint_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_enable_pl_rules_hint))) {
		enable_pl_rules_hint_shadow = 1;
	} else {
		enable_pl_rules_hint_shadow = 0;
	}
}

void
check_enable_ms_rules_hint_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_enable_ms_rules_hint))) {
		enable_ms_rules_hint_shadow = 1;
	} else {
		enable_ms_rules_hint_shadow = 0;
	}
}

void
check_enable_ms_tree_icons_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_enable_ms_tree_icons))) {
		enable_ms_tree_icons_shadow = 1;
	} else {
		enable_ms_tree_icons_shadow = 0;
	}
}

#ifdef HAVE_LADSPA
void
changed_ladspa_prepost(GtkWidget * widget, gpointer * data) {

	int status = gtk_combo_box_get_active(GTK_COMBO_BOX(optmenu_ladspa));
	options.ladspa_is_postfader = status;
}
#endif /* HAVE_LADSPA */


#ifdef HAVE_SRC
void
changed_src_type(GtkWidget * widget, gpointer * data) {

	src_type = gtk_combo_box_get_active(GTK_COMBO_BOX(optmenu_src));
	gtk_label_set_text(GTK_LABEL(label_src), src_get_description(src_type));
	set_src_type_label(src_type);
}
#endif /* HAVE_SRC */

void
changed_cover_width(GtkWidget * widget, gpointer * data) {

	cover_width_shadow = gtk_combo_box_get_active(GTK_COMBO_BOX(optmenu_cwidth));
}

void
check_rva_is_enabled_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_rva_is_enabled))) {
		rva_is_enabled_shadow = 1;
		gtk_widget_set_sensitive(optmenu_listening_env, TRUE);
		gtk_widget_set_sensitive(label_listening_env, TRUE);
		gtk_widget_set_sensitive(spin_refvol, TRUE);
		gtk_widget_set_sensitive(label_refvol, TRUE);
		gtk_widget_set_sensitive(spin_steepness, TRUE);
		gtk_widget_set_sensitive(label_steepness, TRUE);

		gtk_widget_set_sensitive(check_rva_use_averaging, TRUE);

		if (rva_use_averaging_shadow) {
			gtk_widget_set_sensitive(optmenu_threshold, TRUE);
			gtk_widget_set_sensitive(label_threshold, TRUE);
			gtk_widget_set_sensitive(spin_linthresh, TRUE);
			gtk_widget_set_sensitive(label_linthresh, TRUE);
			gtk_widget_set_sensitive(spin_stdthresh, TRUE);
			gtk_widget_set_sensitive(label_stdthresh, TRUE);
		}
	} else {
		rva_is_enabled_shadow = 0;
		gtk_widget_set_sensitive(optmenu_listening_env, FALSE);
		gtk_widget_set_sensitive(label_listening_env, FALSE);
		gtk_widget_set_sensitive(spin_refvol, FALSE);
		gtk_widget_set_sensitive(label_refvol, FALSE);
		gtk_widget_set_sensitive(spin_steepness, FALSE);
		gtk_widget_set_sensitive(label_steepness, FALSE);

		gtk_widget_set_sensitive(check_rva_use_averaging, FALSE);
		gtk_widget_set_sensitive(optmenu_threshold, FALSE);
		gtk_widget_set_sensitive(label_threshold, FALSE);
		gtk_widget_set_sensitive(spin_linthresh, FALSE);
		gtk_widget_set_sensitive(label_linthresh, FALSE);
		gtk_widget_set_sensitive(spin_stdthresh, FALSE);
		gtk_widget_set_sensitive(label_stdthresh, FALSE);
	}

	draw_rva_diagram();
}


void
check_rva_use_averaging_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_rva_use_averaging))) {
		rva_use_averaging_shadow = 1;
		gtk_widget_set_sensitive(optmenu_threshold, TRUE);
		gtk_widget_set_sensitive(label_threshold, TRUE);
		gtk_widget_set_sensitive(spin_linthresh, TRUE);
		gtk_widget_set_sensitive(label_linthresh, TRUE);
		gtk_widget_set_sensitive(spin_stdthresh, TRUE);
		gtk_widget_set_sensitive(label_stdthresh, TRUE);
	} else {
		rva_use_averaging_shadow = 0;
		gtk_widget_set_sensitive(optmenu_threshold, FALSE);
		gtk_widget_set_sensitive(label_threshold, FALSE);
		gtk_widget_set_sensitive(spin_linthresh, FALSE);
		gtk_widget_set_sensitive(label_linthresh, FALSE);
		gtk_widget_set_sensitive(spin_stdthresh, FALSE);
		gtk_widget_set_sensitive(label_stdthresh, FALSE);
	}
}


void
changed_listening_env(GtkWidget * widget, gpointer * data) {

	rva_env_shadow = gtk_combo_box_get_active(GTK_COMBO_BOX(optmenu_listening_env));

	switch (rva_env_shadow) {
	case 0: /* Audiophile */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_refvol), -12.0f);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_steepness), 1.0f);
		break;
	case 1: /* Living room */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_refvol), -12.0f);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_steepness), 0.7f);
		break;
	case 2: /* Office */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_refvol), -12.0f);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_steepness), 0.4f);
		break;
	case 3: /* Noisy workshop */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_refvol), -12.0f);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_steepness), 0.1f);
		break;
	default:
		fprintf(stderr, "programmer error: options.c/changed_listening_env(): "
			"invalid rva_env_shadow value.\nPlease report this to the programmers!\n");
		break;
	}
}


void
draw_rva_diagram(void) {

	GdkGC * gc;
	GdkColor fg_color;
	int i;
	int width = rva_viewport->allocation.width - 4;
	int height = rva_viewport->allocation.height - 4;
	int dw = width / 24;
	int dh = height / 24;
	int xoffs = (width - 24*dw) / 2 - 1;
	int yoffs = (height - 24*dh) / 2 - 1;
	float volx, voly;
	int px1, py1, px2, py2;


	gdk_draw_rectangle(rva_pixmap,
			   rva_drawing_area->style->black_gc,
			   TRUE,
			   0, 0,
			   rva_drawing_area->allocation.width,
			   rva_drawing_area->allocation.height);
	
	gc = gdk_gc_new(rva_pixmap);
	if (rva_is_enabled_shadow) {
		fg_color.red = 10000;
		fg_color.green = 10000;
		fg_color.blue = 10000;
	} else {
		fg_color.red = 5000;
		fg_color.green = 5000;
		fg_color.blue = 5000;
	}
	gdk_gc_set_rgb_fg_color(gc, &fg_color);

	for (i = 0; i <= 24; i++) {
		gdk_draw_line(rva_pixmap, gc,
			      xoffs + i * dw, yoffs,
			      xoffs + i * dw, yoffs + 24 * dh);
	}

	for (i = 0; i <= 24; i++) {
		gdk_draw_line(rva_pixmap, gc,
			      xoffs, yoffs + i * dh,
			      xoffs + 24 * dw, yoffs + i * dh);
	}

	if (rva_is_enabled_shadow) {
		fg_color.red = 0;
		fg_color.green = 0;
		fg_color.blue = 65535;
	} else {
		fg_color.red = 0;
		fg_color.green = 0;
		fg_color.blue = 30000;
	}
	gdk_gc_set_rgb_fg_color(gc, &fg_color);
	gdk_draw_line(rva_pixmap, gc, xoffs, yoffs + 24 * dh, xoffs + 24 * dw, yoffs);

	if (rva_is_enabled_shadow) {
		fg_color.red = 65535;
		fg_color.green = 0;
		fg_color.blue = 0;
	} else {
		fg_color.red = 30000;
		fg_color.green = 0;
		fg_color.blue = 0;
	}
	gdk_gc_set_rgb_fg_color(gc, &fg_color);


	volx = -24.0f;
	voly = volx + (volx - rva_refvol_shadow) * (rva_steepness_shadow - 1.0f);
	px1 = xoffs;
	py1 = yoffs - (voly * dh);

	volx = 0.0f;
	voly = volx + (volx - rva_refvol_shadow) * (rva_steepness_shadow - 1.0f);
	px2 = xoffs + 24*dw;
	py2 = yoffs - (voly * dh);

	gdk_draw_line(rva_pixmap, gc, px1, py1, px2, py2);

	gdk_draw_drawable(rva_drawing_area->window,
			rva_drawing_area->style->fg_gc[GTK_WIDGET_STATE(rva_drawing_area)],
			rva_pixmap,
			0, 0, 0, 0,
			width, height);

	g_object_unref(gc);
}


void
refvol_changed(GtkWidget * widget, gpointer * data) {

	rva_refvol_shadow = gtk_adjustment_get_value(GTK_ADJUSTMENT(widget));
	draw_rva_diagram();
}


void
steepness_changed(GtkWidget * widget, gpointer * data) {

	rva_steepness_shadow = gtk_adjustment_get_value(GTK_ADJUSTMENT(widget));
	draw_rva_diagram();
}


static gint
rva_configure_event(GtkWidget * widget, GdkEventConfigure * event) {

	if (rva_pixmap)
		g_object_unref(rva_pixmap);

	rva_pixmap = gdk_pixmap_new(widget->window,
				    widget->allocation.width,
				    widget->allocation.height,
				    -1);
	gdk_draw_rectangle(rva_pixmap,
			   widget->style->black_gc,
			   TRUE,
			   0, 0,
			   widget->allocation.width,
			   widget->allocation.height);
	draw_rva_diagram();
	return TRUE;
}


static gint
rva_expose_event(GtkWidget * widget, GdkEventExpose * event) {

	gdk_draw_drawable(widget->window,
			widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
			rva_pixmap,
			event->area.x, event->area.y,
			event->area.x, event->area.y,
			event->area.width, event->area.height);
	return FALSE;
}


void
changed_threshold(GtkWidget * widget, gpointer * data) {

	rva_use_linear_thresh_shadow =
	        gtk_combo_box_get_active(GTK_COMBO_BOX(optmenu_threshold));
}


void
linthresh_changed(GtkWidget * widget, gpointer * data) {

	rva_avg_linear_thresh_shadow = gtk_adjustment_get_value(GTK_ADJUSTMENT(widget));
}


void
stdthresh_changed(GtkWidget * widget, gpointer * data) {

	rva_avg_stddev_thresh_shadow = gtk_adjustment_get_value(GTK_ADJUSTMENT(widget)) / 100.0f;
}

void
show_restart_info(void) {

	GtkWidget * info_dialog;
	GtkWidget * list;
	GtkWidget * viewport;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

        info_dialog = gtk_message_dialog_new(GTK_WINDOW (options_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
					     GTK_MESSAGE_INFO,
					     GTK_BUTTONS_OK,
					     _("You will need to restart Aqualung for the following changes to take effect:"));


        list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(restart_list_store));
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("",
							  renderer,
							  "text", 0,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);

	viewport = gtk_viewport_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(viewport), list);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(info_dialog)->vbox), viewport, FALSE, FALSE, 6);


	gtk_widget_show_all(info_dialog);
        gtk_dialog_run(GTK_DIALOG(info_dialog));
        gtk_widget_destroy(info_dialog);
}

void
playlist_font_select(GtkWidget *widget) {

	gchar *s;
	GtkWidget *font_selector;
	gint response;

	font_selector = gtk_font_selection_dialog_new ("Select a font...");
	gtk_window_set_modal(GTK_WINDOW(font_selector), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(font_selector), GTK_WINDOW(options_window));
	gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG(font_selector), options.playlist_font);
	gtk_widget_show (font_selector);
	response = gtk_dialog_run (GTK_DIALOG (font_selector));

	if (response == GTK_RESPONSE_OK) {

		s = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG(font_selector));
		strncpy(options.playlist_font, s, MAX_FONTNAME_LEN);
		gtk_entry_set_text(GTK_ENTRY(entry_pl_font), s);
		g_free (s);

	}

        appearance_changed = 1;
	gtk_widget_destroy (font_selector);
}

void
browser_font_select(GtkWidget *widget) {

	gchar *s;
	GtkWidget *font_selector;
	gint response;

	font_selector = gtk_font_selection_dialog_new ("Select a font...");
	gtk_window_set_modal(GTK_WINDOW(font_selector), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(font_selector), GTK_WINDOW(options_window));
	gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG(font_selector), options.browser_font);
	gtk_widget_show (font_selector);
	response = gtk_dialog_run (GTK_DIALOG (font_selector));

	if (response == GTK_RESPONSE_OK) {

		s = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG(font_selector));
		strncpy(options.browser_font, s, MAX_FONTNAME_LEN);
		gtk_entry_set_text(GTK_ENTRY(entry_ms_font), s);
		g_free (s);

	}

        appearance_changed = 1;
	gtk_widget_destroy (font_selector);
}

void
bigtimer_font_select(GtkWidget *widget) {

	gchar *s;
	GtkWidget *font_selector;
	gint response;

	font_selector = gtk_font_selection_dialog_new ("Select a font...");
	gtk_window_set_modal(GTK_WINDOW(font_selector), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(font_selector), GTK_WINDOW(options_window));
	gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG(font_selector), options.bigtimer_font);
	gtk_widget_show (font_selector);
	response = gtk_dialog_run (GTK_DIALOG (font_selector));

	if (response == GTK_RESPONSE_OK) {

		s = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG(font_selector));
		strncpy(options.bigtimer_font, s, MAX_FONTNAME_LEN);
		gtk_entry_set_text(GTK_ENTRY(entry_bt_font), s);
		g_free (s);

	}

        appearance_changed = 1;
	gtk_widget_destroy (font_selector);
}

void
smalltimer_font_select(GtkWidget *widget) {

	gchar *s;
	GtkWidget *font_selector;
	gint response;

	font_selector = gtk_font_selection_dialog_new ("Select a font...");
	gtk_window_set_modal(GTK_WINDOW(font_selector), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(font_selector), GTK_WINDOW(options_window));
	gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG(font_selector), options.smalltimer_font);
	gtk_widget_show (font_selector);
	response = gtk_dialog_run (GTK_DIALOG (font_selector));

	if (response == GTK_RESPONSE_OK) {

		s = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG(font_selector));
		strncpy(options.smalltimer_font, s, MAX_FONTNAME_LEN);
		gtk_entry_set_text(GTK_ENTRY(entry_st_font), s);
		g_free (s);

	}

        appearance_changed = 1;
	gtk_widget_destroy (font_selector);
}

void
songtitle_font_select(GtkWidget *widget) {

	gchar *s;
	GtkWidget *font_selector;
	gint response;

	font_selector = gtk_font_selection_dialog_new ("Select a font...");
	gtk_window_set_modal(GTK_WINDOW(font_selector), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(font_selector), GTK_WINDOW(options_window));
	gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG(font_selector), options.songtitle_font);
	gtk_widget_show (font_selector);
	response = gtk_dialog_run (GTK_DIALOG (font_selector));

	if (response == GTK_RESPONSE_OK) {

		s = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG(font_selector));
		strncpy(options.songtitle_font, s, MAX_FONTNAME_LEN);
		gtk_entry_set_text(GTK_ENTRY(entry_songt_font), s);
		g_free (s);

	}

        appearance_changed = 1;
	gtk_widget_destroy (font_selector);
}

void
songinfo_font_select(GtkWidget *widget) {

	gchar *s;
	GtkWidget *font_selector;
	gint response;

	font_selector = gtk_font_selection_dialog_new ("Select a font...");
	gtk_window_set_modal(GTK_WINDOW(font_selector), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(font_selector), GTK_WINDOW(options_window));
	gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG(font_selector), options.songinfo_font);
	gtk_widget_show (font_selector);
	response = gtk_dialog_run (GTK_DIALOG (font_selector));

	if (response == GTK_RESPONSE_OK) {

		s = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG(font_selector));
		strncpy(options.songinfo_font, s, MAX_FONTNAME_LEN);
		gtk_entry_set_text(GTK_ENTRY(entry_si_font), s);
		g_free (s);

	}

        appearance_changed = 1;
	gtk_widget_destroy (font_selector);
}

void
statusbar_font_select(GtkWidget *widget) {

	gchar *s;
	GtkWidget *font_selector;
	gint response;

	font_selector = gtk_font_selection_dialog_new ("Select a font...");
	gtk_window_set_modal(GTK_WINDOW(font_selector), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(font_selector), GTK_WINDOW(options_window));
	gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG(font_selector), options.statusbar_font);
	gtk_widget_show (font_selector);
	response = gtk_dialog_run (GTK_DIALOG (font_selector));

	if (response == GTK_RESPONSE_OK) {

		s = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG(font_selector));
		strncpy(options.statusbar_font, s, MAX_FONTNAME_LEN);
		gtk_entry_set_text(GTK_ENTRY(entry_sb_font), s);
		g_free (s);

	}

        appearance_changed = 1;
	gtk_widget_destroy (font_selector);
}

void
restart_active(GtkToggleButton * togglebutton, gpointer data) {

	GtkTreeIter iter;
	char * text;
	int i = 0;


        restart_flag = 1;

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(restart_list_store), &iter, NULL, i++)) {

                gtk_tree_model_get(GTK_TREE_MODEL(restart_list_store), &iter, 0, &text, -1);

                if (!strcmp(text, (char *)data)) {
                        gtk_list_store_remove(restart_list_store, &iter);
                        if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(restart_list_store), &iter)) {
                                restart_flag = 0;
                        }
                        return;
                }
        }

	gtk_list_store_append(restart_list_store, &iter);
	gtk_list_store_set(restart_list_store, &iter, 0, (char *)data, -1);
}

void
set_sensitive_part(void) {

	GtkWidget *sensitive_table[] = {
		entry_ms_font, entry_pl_font, entry_bt_font, entry_st_font,
		entry_songt_font, entry_si_font, entry_sb_font, button_ms_font,
		button_pl_font, button_bt_font, button_st_font, button_songt_font,
		button_si_font, button_sb_font, color_picker
	};

	gboolean state;
        gint items, n;
        
        if (options.override_skin_settings) {
		state = TRUE;
	} else {
		state = FALSE;
	}

        items = sizeof(sensitive_table) / sizeof(GtkWidget*);

        for (n = 0; n < items; n++) {
                gtk_widget_set_sensitive(sensitive_table[n], state);
        }
}

void
cb_toggle_override_skin(GtkToggleButton *togglebutton, gpointer user_data) {

        options.override_skin_settings = options.override_skin_settings ? 0 : 1;
        appearance_changed = 1;
        set_sensitive_part();
}

void
color_selected(GtkColorButton *widget, gpointer user_data) {

	GdkColor c;
	gchar str[MAX_COLORNAME_LEN];

        appearance_changed = 1;
        gtk_color_button_get_color(widget, &c);
        sprintf(str, "#%02X%02X%02X", c.red * 256 / 65536, c.green * 256 / 65536, c.blue * 256 / 65536);

        strncpy(options.activesong_color, str, MAX_COLORNAME_LEN-1);
}

GtkWidget *
create_notebook_tab(char * text, char * imgfile) {

	GtkWidget * vbox;
        GdkPixbuf * pixbuf;
        GtkWidget * image;
	GtkWidget * label;

        char path[MAXLEN];

	vbox = gtk_vbox_new(FALSE, 0);

	label = gtk_label_new(text);
	gtk_box_pack_end(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	sprintf(path, "%s/%s", AQUALUNG_DATADIR, imgfile);

        pixbuf = gdk_pixbuf_new_from_file(path, NULL);

	if (pixbuf) {
		image = gtk_image_new_from_pixbuf(pixbuf);
		gtk_box_pack_end(GTK_BOX(vbox), image, FALSE, FALSE, 0);
	}

	gtk_widget_show_all(vbox);

	return vbox;
}

void
add_ms_pathlist_clicked(GtkWidget * widget, gpointer * data) {

	const char * pname;
	char name[MAXLEN];
	char * path;
	GtkTreeIter iter;
	int i;
	struct stat st_file;

	pname = gtk_entry_get_text(GTK_ENTRY(entry_ms_pathlist));

	if (pname[0] == '\0') return;

	if (pname[0] == '~') {
		snprintf(name, MAXLEN - 1, "%s%s", options.home, pname + 1);
	} else if (pname[0] == '/') {
		strncpy(name, pname, MAXLEN - 1);
	} else {
		GtkWidget * dialog;
		GtkWidget * label;
		
		dialog = gtk_dialog_new_with_buttons(_("Warning"),
						     GTK_WINDOW(options_window),
						     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
						     NULL);
		gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
		gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
		gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
		
		label = gtk_label_new(_("Paths must either be absolute or starting with a tilde."));
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, TRUE, 10);
		gtk_widget_show(label);
		
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		return;
	}

	if (stat(name, &st_file) != -1 && S_ISDIR(st_file.st_mode)) {
		return;
	}

	path = g_locale_from_utf8(name, -1, NULL, NULL, NULL);

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ms_pathlist_store), &iter, NULL, i++)) {
		char * p;

		gtk_tree_model_get(GTK_TREE_MODEL(ms_pathlist_store), &iter, 0, &p, -1);

		if (!strcmp(p, path)) {

			GtkWidget * dialog;

			dialog = gtk_message_dialog_new(GTK_WINDOW(options_window),
							GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
							GTK_MESSAGE_WARNING,
							GTK_BUTTONS_CLOSE,
							_("The specified store has already been added to the list."));
			gtk_widget_show(dialog);
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);

			g_free(p);
			g_free(path);
			return;
		}

		g_free(p);
	}
	

	gtk_entry_set_text(GTK_ENTRY(entry_ms_pathlist), "");
	gtk_list_store_append(ms_pathlist_store, &iter);
	gtk_list_store_set(ms_pathlist_store, &iter, 0, path, 1, name, -1);

	if (access(path, R_OK | W_OK) == 0) {
		gtk_list_store_set(ms_pathlist_store, &iter, 2, _("rw"), -1);
	} else if (access(path, R_OK) == 0) {
		gtk_list_store_set(ms_pathlist_store, &iter, 2, _("r"), -1);
	} else {
		gtk_list_store_set(ms_pathlist_store, &iter, 2, _("unreachable"), -1);
	}

	g_free(path);
}


void
remove_ms_pathlist_clicked(GtkWidget * widget, gpointer data) {

	GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ms_pathlist_store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(ms_pathlist_select, &iter)) {
			gtk_list_store_remove(ms_pathlist_store, &iter);
			--i;
		}
	}
}


void
browse_ms_pathlist_clicked(GtkWidget * widget, gpointer data) {

        GtkWidget * dialog;
	const gchar * selected_filename = gtk_entry_get_text(GTK_ENTRY(data));


        dialog = gtk_file_chooser_dialog_new(_("Please select a Music Store database."),
                                             GTK_WINDOW(options_window),
                                             GTK_FILE_CHOOSER_ACTION_OPEN,
                                             GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

        if (strlen(selected_filename)) {
		char * locale = g_locale_from_utf8(selected_filename, -1, NULL, NULL, NULL);
		char tmp[MAXLEN];
		tmp[0] = '\0';

		if (locale[0] == '~') {
			snprintf(tmp, MAXLEN-1, "%s%s", options.home, locale + 1);
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), tmp);
		} else if (locale[0] == '/') {
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), locale);
		} else if (locale[0] != '\0') {
			snprintf(tmp, MAXLEN-1, "%s/%s", options.cwd, locale + 1);
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), tmp);
		}

		g_free(locale);
	} else {
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_show_hidden))) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * utf8;

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		utf8 = g_locale_to_utf8(selected_filename, -1, NULL, NULL, NULL);
                gtk_entry_set_text(GTK_ENTRY(entry_ms_pathlist), utf8);

                strncpy(options.currdir, selected_filename, MAXLEN-1);
		g_free(utf8);
        }

        gtk_widget_destroy(dialog);
}


void
refresh_ms_pathlist_clicked(GtkWidget * widget, gpointer data) {

	GtkTreeIter iter;
	int i = 0;
	char * path;

	if (music_store_changed) {
		GtkWidget * dialog;
		GtkWidget * label;
		
		dialog = gtk_dialog_new_with_buttons(_("Warning"),
						     GTK_WINDOW(options_window),
						     GTK_DIALOG_MODAL |
						     GTK_DIALOG_DESTROY_WITH_PARENT |
						     GTK_FILE_CHOOSER_ACTION_OPEN,
						     GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
						     _("Discard changes"), GTK_RESPONSE_REJECT,
						     NULL);
		gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
		gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
		gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
		
		label = gtk_label_new(_("Music Store has been changed. You have to save it before\n"
					"refreshing if you want to keep the changes."));
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, TRUE, 10);
		gtk_widget_show(label);
		
		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
			save_music_store();
		}
		
		gtk_widget_destroy(dialog);
	}

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ms_pathlist_store),
					     &iter, NULL, i++)) {
		gtk_tree_model_get(GTK_TREE_MODEL(ms_pathlist_store), &iter, 0, &path, -1);
		
		if (access(path, R_OK | W_OK) == 0) {
			gtk_list_store_set(ms_pathlist_store, &iter, 2, _("rw"), -1);
		} else if (access(path, R_OK) == 0) {
			gtk_list_store_set(ms_pathlist_store, &iter, 2, _("r"), -1);
		} else {
			gtk_list_store_set(ms_pathlist_store, &iter, 2, _("unreachable"), -1);
		}
		
		g_free(path);
	}
	
	gtk_tree_store_clear(music_store);
	load_music_store();
	music_store_mark_saved();
}


void
display_title_format_help(void) {

	GtkWidget *help_dialog;

        help_dialog = gtk_message_dialog_new (GTK_WINDOW(options_window), 
                                              GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, 
                                              _("\nThe template string you enter here will be used to\n"
                                              "construct a single title line from an Artist, a Record\n"
                                              "and a Track name. These are denoted by %%a, %%r and %%t,\n"
                                              "respectively. Everything else you enter here will be\n"
                                              "literally copied into the resulting string.\n"));

        gtk_widget_show (help_dialog);
        gtk_dialog_run(GTK_DIALOG(help_dialog));
        gtk_widget_destroy(help_dialog);
}


void
display_implict_command_line_help(void) {

	GtkWidget *help_dialog;

        help_dialog = gtk_message_dialog_new (GTK_WINDOW(options_window), 
                                              GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, 
                                              _("\nThe string you enter here will be parsed as a command\n"
                                              "line before parsing the actual command line parameters.\n"
                                              "What you enter here will act as a default setting and may\n"
                                              "or may not be overrided from the 'real' command line.\n"
                                              "Example: enter '-o alsa -R' below to use ALSA output\n"
                                              "running realtime as a default.\n"));

        gtk_widget_show (help_dialog);
        gtk_dialog_run(GTK_DIALOG(help_dialog));
        gtk_widget_destroy(help_dialog);
}


void
display_pathlist_help(void) {

	GtkWidget *help_dialog;

        help_dialog = gtk_message_dialog_new (GTK_WINDOW(options_window), 
                                              GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, 
	                                      _("Paths must either be absolute or starting with a tilde,\n"
                				"which will be expanded to the user's home directory.\n\n"
                                                "You will need to click the Refresh button after adding or\n"
	                			"removing a store or reordering the list."));

        gtk_widget_show (help_dialog);
        gtk_dialog_run(GTK_DIALOG(help_dialog));
        gtk_widget_destroy(help_dialog);
}


void
create_options_window(void) {

	GtkWidget * vbox;

	GtkWidget * vbox_general;
	GtkWidget * frame_title;
	GtkWidget * frame_param;
	GtkWidget * frame_misc;
	GtkWidget * frame_cart;
	GtkWidget * vbox_title;
	GtkWidget * hbox_title;
	GtkWidget * vbox_param;
	GtkWidget * hbox_param;
	GtkWidget * vbox_misc;
	GtkWidget * vbox_cart;
	GtkWidget * vbox_appearance;

	GtkWidget * vbox_pl;
	GtkWidget * vbox_ms;
	GtkWidget * frame_ms_pathlist;
	GtkWidget * ms_pathlist_view;
	GtkWidget * vbox_ms_pathlist;
	GtkWidget * hbox_ms_pathlist;
	GtkWidget * hbox_ms_pathlist_2;
	GtkWidget * add_ms_pathlist;
	GtkWidget * browse_ms_pathlist;
	GtkWidget * remove_ms_pathlist;
	GtkWidget * refresh_ms_pathlist;
	GtkWidget * frame_plistcol;
	GtkWidget * vbox_plistcol;
	GtkWidget * label_plistcol;
	GtkWidget * viewport;
	GtkWidget * scrolled_win;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	GtkWidget * plistcol_list;
	GtkWidget * label;

	GtkWidget * vbox_dsp;
	GtkWidget * frame_ladspa;
	GtkWidget * frame_src;
	GtkWidget * frame_fonts;
	GtkWidget * frame_colors;
	GtkWidget * vbox_ladspa;
	GtkWidget * vbox_src;
	GtkWidget * vbox_fonts;
	GtkWidget * vbox_colors;

	GtkWidget * vbox_rva;
	GtkWidget * table_rva;
	GtkWidget * hbox_listening_env;
	GtkWidget * hbox_refvol;
	GtkWidget * hbox_steepness;
	GtkWidget * table_avg;
	GtkWidget * hbox_threshold;
	GtkWidget * hbox_linthresh;
	GtkWidget * hbox_stdthresh;
        GtkWidget * label_cwidth;
	GtkWidget * hbox_cwidth;

	GtkWidget * vbox_meta;

#ifdef HAVE_CDDB
	GtkWidget * table_cddb;
#endif /* HAVE_CDDB */

        GtkSizeGroup * label_size;

	GtkWidget * hbox;
	GtkWidget * hbox_s;
	GtkWidget * hbuttonbox;
	GtkWidget * ok_btn;
	GtkWidget * cancel_btn;
       	GtkWidget * help_btn_title;
	GtkWidget * help_btn_param;
	GtkWidget * help_pathlist;

#ifdef HAVE_LADSPA
	int status;
#endif /* HAVE_LADSPA */
	int i;


        restart_flag = 0;
	if (!restart_list_store) {
		restart_list_store = gtk_list_store_new(1, G_TYPE_STRING);
	} else {
		gtk_list_store_clear(restart_list_store);
	}

        appearance_changed = 0;

	options_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_transient_for(GTK_WINDOW(options_window), GTK_WINDOW(main_window));
        gtk_window_set_title(GTK_WINDOW(options_window), _("Settings"));
	gtk_window_set_position(GTK_WINDOW(options_window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(options_window), TRUE);
        gtk_container_set_border_width(GTK_CONTAINER(options_window), 5);

        g_signal_connect(G_OBJECT(options_window), "key_press_event",
			 G_CALLBACK(options_window_key_pressed), NULL);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(options_window), vbox);

	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);
	gtk_container_add(GTK_CONTAINER(vbox), notebook);

        label_size = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);

	/* "General" notebook page */

	vbox_general = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_general), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_general, create_notebook_tab(_("General"), "general.png"));

	frame_title = gtk_frame_new(_("Title format"));
	gtk_box_pack_start(GTK_BOX(vbox_general), frame_title, FALSE, TRUE, 5);

	vbox_title = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_title), 5);
	gtk_container_add(GTK_CONTAINER(frame_title), vbox_title);

        hbox_title = gtk_hbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(hbox_title), 2);
	gtk_box_pack_start(GTK_BOX(vbox_title), hbox_title, TRUE, TRUE, 0);

	entry_title = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(entry_title), MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(entry_title), options.title_format);
	gtk_box_pack_start(GTK_BOX(hbox_title), entry_title, TRUE, TRUE, 0);

        help_btn_title = gtk_button_new_from_stock (GTK_STOCK_HELP); 
	g_signal_connect(help_btn_title, "clicked", G_CALLBACK(display_title_format_help), NULL);
	gtk_box_pack_start(GTK_BOX(hbox_title), help_btn_title, FALSE, FALSE, 0);

	frame_param = gtk_frame_new(_("Implicit command line"));
	gtk_box_pack_start(GTK_BOX(vbox_general), frame_param, FALSE, TRUE, 5);

	vbox_param = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_param), 5);
	gtk_container_add(GTK_CONTAINER(frame_param), vbox_param);

        hbox_param = gtk_hbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(hbox_param), 2);
	gtk_box_pack_start(GTK_BOX(vbox_param), hbox_param, TRUE, TRUE, 0);

	entry_param = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(entry_param), MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(entry_param), options.default_param);
	gtk_box_pack_start(GTK_BOX(hbox_param), entry_param, TRUE, TRUE, 0);

        help_btn_param = gtk_button_new_from_stock (GTK_STOCK_HELP); 
	g_signal_connect(help_btn_param, "clicked", G_CALLBACK(display_implict_command_line_help), NULL);
	gtk_box_pack_start(GTK_BOX(hbox_param), help_btn_param, FALSE, FALSE, 0);

	frame_misc = gtk_frame_new(_("Miscellaneous"));
	gtk_box_pack_start(GTK_BOX(vbox_general), frame_misc, FALSE, TRUE, 5);

	vbox_misc = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_misc), 8);
	gtk_container_add(GTK_CONTAINER(frame_misc), vbox_misc);

	check_enable_tooltips =
		gtk_check_button_new_with_label(_("Enable tooltips"));

        gtk_widget_set_name(check_enable_tooltips, "check_on_notebook");
	if (options.enable_tooltips) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_tooltips), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_enable_tooltips, FALSE, FALSE, 0);

	check_buttons_at_the_bottom =
		gtk_check_button_new_with_label(_("Put control buttons at the bottom of playlist"));
	gtk_widget_set_name(check_buttons_at_the_bottom, "check_on_notebook");
	if (options.buttons_at_the_bottom) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_buttons_at_the_bottom), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_buttons_at_the_bottom, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (check_buttons_at_the_bottom), "toggled",
						G_CALLBACK (restart_active), _("Put control buttons at the bottom of playlist"));

        check_disable_buttons_relief =
		gtk_check_button_new_with_label(_("Disable control buttons relief"));
	gtk_widget_set_name(check_disable_buttons_relief, "check_on_notebook");
	if (options.disable_buttons_relief) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_disable_buttons_relief), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_disable_buttons_relief, FALSE, FALSE, 0);


        check_main_window_always_on_top = gtk_check_button_new_with_label(_("Keep main window always on top"));
	gtk_widget_set_name(check_main_window_always_on_top, "main_window_always_on_top");
	if (options.main_window_always_on_top) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_main_window_always_on_top), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_main_window_always_on_top, FALSE, FALSE, 0);


#ifdef HAVE_LADSPA
	check_simple_view_in_fx =
		gtk_check_button_new_with_label(_("Simple view in LADSPA patch builder"));
	gtk_widget_set_name(check_simple_view_in_fx, "check_on_notebook");
	if (options.simple_view_in_fx) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_simple_view_in_fx), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_simple_view_in_fx, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (check_simple_view_in_fx), "toggled",
						G_CALLBACK (restart_active), _("Simple view in LADSPA patch builder"));
#endif /* HAVE_LADSPA */

	check_united_minimization =
		gtk_check_button_new_with_label(_("United windows minimization"));

        gtk_widget_set_name(check_united_minimization, "check_on_notebook");
	if (options.united_minimization) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_united_minimization), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_united_minimization, FALSE, FALSE, 0);

	check_show_sn_title =
		gtk_check_button_new_with_label(_("Show song name in the main window's title"));

        gtk_widget_set_name(check_show_sn_title, "check_on_notebook");
	if (options.show_sn_title) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_sn_title), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_show_sn_title, FALSE, FALSE, 0);


	check_show_hidden = gtk_check_button_new_with_label(_("Show hidden files and directories in file choosers"));
	gtk_widget_set_name(check_show_hidden, "check_on_notebook");
	if (options.show_hidden) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_hidden), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_show_hidden, FALSE, FALSE, 0);

        check_tags_tab_first = gtk_check_button_new_with_label(_("Show tags tab first in the file info dialog"));
	gtk_widget_set_name(check_tags_tab_first, "check_on_notebook");
	if (options.tags_tab_first) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_tags_tab_first), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_tags_tab_first, FALSE, FALSE, 0);


        /* "Playlist" notebook page */

	vbox_pl = gtk_vbox_new(FALSE, 3);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_pl), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_pl, create_notebook_tab(_("Playlist"), "playlist.png"));
	
        check_playlist_is_embedded =
		gtk_check_button_new_with_label(_("Embed playlist into main window"));
	gtk_widget_set_name(check_playlist_is_embedded, "check_on_notebook");
	if (options.playlist_is_embedded_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_playlist_is_embedded), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_pl), check_playlist_is_embedded, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (check_playlist_is_embedded), "toggled",
		G_CALLBACK (restart_active), _("Embed playlist into main window"));

	check_autoplsave =
	    gtk_check_button_new_with_label(_("Save and restore the playlist on exit/startup"));
	gtk_widget_set_name(check_autoplsave, "check_on_notebook");
	if (options.auto_save_playlist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_autoplsave), TRUE);
	}
	auto_save_playlist_shadow = options.auto_save_playlist;
	g_signal_connect(G_OBJECT(check_autoplsave), "toggled",
			 G_CALLBACK(autoplsave_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_autoplsave, FALSE, TRUE, 0);

        check_playlist_is_tree =
		gtk_check_button_new_with_label(_("Album mode is the default when adding entire records"));
	gtk_widget_set_name(check_playlist_is_tree, "check_on_notebook");
	if (options.playlist_is_tree) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_playlist_is_tree), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_pl), check_playlist_is_tree, FALSE, TRUE, 0);

        check_album_shuffle_mode =
		gtk_check_button_new_with_label(_("When shuffling, records added in Album mode "
						  "are played in order"));
	gtk_widget_set_name(check_album_shuffle_mode, "check_on_notebook");
	if (options.album_shuffle_mode) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_album_shuffle_mode), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_pl), check_album_shuffle_mode, FALSE, TRUE, 0);

	check_enable_playlist_statusbar =
		gtk_check_button_new_with_label(_("Enable statusbar"));
	gtk_widget_set_name(check_enable_playlist_statusbar, "check_on_notebook");
	if (options.enable_playlist_statusbar_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_playlist_statusbar), TRUE);
	}
	options.enable_playlist_statusbar_shadow = options.enable_playlist_statusbar;
	g_signal_connect(G_OBJECT(check_enable_playlist_statusbar), "toggled",
			 G_CALLBACK(restart_active), _("Enable statusbar in playlist"));
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_enable_playlist_statusbar, FALSE, TRUE, 0);

	check_pl_statusbar_show_size =
		gtk_check_button_new_with_label(_("Show soundfile size in statusbar"));
	gtk_widget_set_name(check_pl_statusbar_show_size, "check_on_notebook");
	if (options.pl_statusbar_show_size) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_pl_statusbar_show_size), TRUE);
	}
	pl_statusbar_show_size_shadow = options.pl_statusbar_show_size;
	g_signal_connect(G_OBJECT(check_pl_statusbar_show_size), "toggled",
			 G_CALLBACK(check_pl_statusbar_show_size_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_pl_statusbar_show_size, FALSE, TRUE, 0);

        check_show_rva_in_playlist =
		gtk_check_button_new_with_label(_("Show RVA values"));
	gtk_widget_set_name(check_show_rva_in_playlist, "check_on_notebook");
	if (options.show_rva_in_playlist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_rva_in_playlist), TRUE);
	}
	show_rva_in_playlist_shadow = options.show_rva_in_playlist;
	g_signal_connect(G_OBJECT(check_show_rva_in_playlist), "toggled",
			 G_CALLBACK(check_show_rva_in_playlist_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_show_rva_in_playlist, FALSE, TRUE, 0);

	check_show_length_in_playlist =
		gtk_check_button_new_with_label(_("Show track lengths"));
	gtk_widget_set_name(check_show_length_in_playlist, "check_on_notebook");
	if (options.show_length_in_playlist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_length_in_playlist), TRUE);
	}
	show_length_in_playlist_shadow = options.show_length_in_playlist;
	g_signal_connect(G_OBJECT(check_show_length_in_playlist), "toggled",
			 G_CALLBACK(check_show_length_in_playlist_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_show_length_in_playlist, FALSE, TRUE, 0);
	
        track_name_in_bold_past_state = options.show_active_track_name_in_bold;

	check_show_active_track_name_in_bold =
		gtk_check_button_new_with_label(_("Show active track name in bold"));
	gtk_widget_set_name(check_show_active_track_name_in_bold, "check_on_notebook");
	if (options.show_active_track_name_in_bold) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_active_track_name_in_bold), TRUE);
	}
	show_active_track_name_in_bold_shadow = options.show_active_track_name_in_bold;
	g_signal_connect(G_OBJECT(check_show_active_track_name_in_bold), "toggled",
			 G_CALLBACK(check_show_active_track_name_in_bold_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_show_active_track_name_in_bold, FALSE, TRUE, 0);

	check_enable_pl_rules_hint =
		gtk_check_button_new_with_label(_("Enable rules hint"));
        gtk_widget_set_name(check_enable_pl_rules_hint, "check_on_notebook");
	if (options.enable_pl_rules_hint) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_pl_rules_hint), TRUE);
	}
	enable_pl_rules_hint_shadow = options.enable_pl_rules_hint;
	g_signal_connect(G_OBJECT(check_enable_pl_rules_hint), "toggled",
			 G_CALLBACK(check_enable_pl_rules_hint_toggled), NULL);
	gtk_box_pack_start(GTK_BOX(vbox_pl), check_enable_pl_rules_hint, FALSE, TRUE, 0);

        frame_plistcol = gtk_frame_new(_("Playlist column order"));
        gtk_box_pack_start(GTK_BOX(vbox_pl), frame_plistcol, FALSE, TRUE, 5);

        vbox_plistcol = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_plistcol), 8);
        gtk_container_add(GTK_CONTAINER(frame_plistcol), vbox_plistcol);
	
	label_plistcol = gtk_label_new(_("Drag and drop entries in the list below \n\
to set the column order in the Playlist."));
        gtk_box_pack_start(GTK_BOX(vbox_plistcol), label_plistcol, FALSE, TRUE, 5);


	plistcol_store = gtk_list_store_new(2,
					    G_TYPE_STRING,   /* Column name */
					    G_TYPE_STRING);  /* Column index */

        plistcol_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(plistcol_store));
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(plistcol_list), FALSE);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Column"),
							  renderer,
							  "text", 0,
							  NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(plistcol_list), column);
        gtk_tree_view_set_reorderable(GTK_TREE_VIEW(plistcol_list), TRUE);

        viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(vbox_plistcol), viewport, TRUE, TRUE, 5);

        gtk_container_add(GTK_CONTAINER(viewport), plistcol_list);

	for (i = 0; i < 3; i++) {
		switch (options.plcol_idx[i]) {
		case 0:
			gtk_list_store_append(plistcol_store, &iter);
			gtk_list_store_set(plistcol_store, &iter,
					   0, _("Track titles"), 1, "0", -1);
			break;
		case 1:
			gtk_list_store_append(plistcol_store, &iter);
			gtk_list_store_set(plistcol_store, &iter,
					   0, _("RVA values"), 1, "1", -1);
			break;
		case 2:
			gtk_list_store_append(plistcol_store, &iter);
			gtk_list_store_set(plistcol_store, &iter,
					   0, _("Track lengths"), 1, "2", -1);
			break;
		}
	}


        /* "Music store" notebook page */

	vbox_ms = gtk_vbox_new(FALSE, 3);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_ms), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_ms, create_notebook_tab(_("Music Store"), "music_store.png"));

	check_hide_comment_pane =
		gtk_check_button_new_with_label(_("Hide comment pane"));
	gtk_widget_set_name(check_hide_comment_pane, "check_on_notebook");
	if (options.hide_comment_pane_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_hide_comment_pane), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_ms), check_hide_comment_pane, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (check_hide_comment_pane), "toggled",
						G_CALLBACK (restart_active), _("Hide the Music Store comment pane"));
	check_enable_mstore_toolbar =
		gtk_check_button_new_with_label(_("Enable toolbar"));
	gtk_widget_set_name(check_enable_mstore_toolbar, "check_on_notebook");
	if (options.enable_mstore_toolbar_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_mstore_toolbar), TRUE);
	}
	options.enable_mstore_toolbar_shadow = options.enable_mstore_toolbar;
	g_signal_connect(G_OBJECT(check_enable_mstore_toolbar), "toggled",
			 G_CALLBACK(restart_active), _("Enable toolbar in Music Store"));
        gtk_box_pack_start(GTK_BOX(vbox_ms), check_enable_mstore_toolbar, FALSE, TRUE, 0);

	check_enable_mstore_statusbar =
		gtk_check_button_new_with_label(_("Enable statusbar"));
	gtk_widget_set_name(check_enable_mstore_statusbar, "check_on_notebook");
	if (options.enable_mstore_statusbar_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_mstore_statusbar), TRUE);
	}
	options.enable_mstore_statusbar_shadow = options.enable_mstore_statusbar;
	g_signal_connect(G_OBJECT(check_enable_mstore_statusbar), "toggled",
			 G_CALLBACK(restart_active), _("Enable statusbar in Music Store"));
        gtk_box_pack_start(GTK_BOX(vbox_ms), check_enable_mstore_statusbar, FALSE, TRUE, 0);

	check_ms_statusbar_show_size =
		gtk_check_button_new_with_label(_("Show soundfile size in statusbar"));
	gtk_widget_set_name(check_ms_statusbar_show_size, "check_on_notebook");
	if (options.ms_statusbar_show_size) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_ms_statusbar_show_size), TRUE);
	}
	ms_statusbar_show_size_shadow = options.ms_statusbar_show_size;
	g_signal_connect(G_OBJECT(check_ms_statusbar_show_size), "toggled",
			 G_CALLBACK(check_ms_statusbar_show_size_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_ms), check_ms_statusbar_show_size, FALSE, TRUE, 0);


	check_expand_stores = gtk_check_button_new_with_label(_("Expand Stores on startup"));
	gtk_widget_set_name(check_expand_stores, "check_on_notebook");
	if (options.autoexpand_stores) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_expand_stores), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_ms), check_expand_stores, FALSE, FALSE, 0);

	check_enable_ms_rules_hint =
		gtk_check_button_new_with_label(_("Enable rules hint"));
        gtk_widget_set_name(check_enable_ms_rules_hint, "check_on_notebook");
	if (options.enable_ms_rules_hint) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_ms_rules_hint), TRUE);
	}
	enable_ms_rules_hint_shadow = options.enable_ms_rules_hint;
	g_signal_connect(G_OBJECT(check_enable_ms_rules_hint), "toggled",
			 G_CALLBACK(check_enable_ms_rules_hint_toggled), NULL);
	gtk_box_pack_start(GTK_BOX(vbox_ms), check_enable_ms_rules_hint, FALSE, TRUE, 0);

	check_enable_ms_tree_icons =
		gtk_check_button_new_with_label(_("Enable tree node icons"));
        gtk_widget_set_name(check_enable_ms_tree_icons, "check_on_notebook");
	if (options.enable_ms_tree_icons) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_ms_tree_icons), TRUE);
	}
	enable_ms_tree_icons_shadow = options.enable_ms_tree_icons;
	g_signal_connect(G_OBJECT(check_enable_ms_tree_icons), "toggled",
			 G_CALLBACK(check_enable_ms_tree_icons_toggled), NULL);
	g_signal_connect (G_OBJECT (check_enable_ms_tree_icons), "toggled",
			  G_CALLBACK (restart_active), _("Enable Music Store tree node icons"));
	gtk_box_pack_start(GTK_BOX(vbox_ms), check_enable_ms_tree_icons, FALSE, TRUE, 0);

	frame_cart = gtk_frame_new(_("Cover art"));
	gtk_box_pack_start(GTK_BOX(vbox_ms), frame_cart, FALSE, TRUE, 5);

	vbox_cart = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_cart), 10);
	gtk_container_add(GTK_CONTAINER(frame_cart), vbox_cart);

        hbox_cwidth = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_cart), hbox_cwidth, FALSE, FALSE, 0);
        label_cwidth = gtk_label_new(_("Default cover width:"));
        gtk_box_pack_start(GTK_BOX(hbox_cwidth), label_cwidth, FALSE, FALSE, 0);

        hbox_s = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox_cwidth), hbox_s, TRUE, TRUE, 3);

	optmenu_cwidth = gtk_combo_box_new_text ();
        gtk_box_pack_start(GTK_BOX(hbox_cwidth), optmenu_cwidth, FALSE, FALSE, 0);
        gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_cwidth), _("50 pixels"));
        gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_cwidth), _("100 pixels"));
        gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_cwidth), _("200 pixels"));
        gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_cwidth), _("300 pixels"));
        gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_cwidth), _("use browser window width"));

        cover_width_shadow = options.cover_width;
        gtk_combo_box_set_active (GTK_COMBO_BOX (optmenu_cwidth), options.cover_width);
	g_signal_connect(optmenu_cwidth, "changed", G_CALLBACK(changed_cover_width), NULL);

        check_magnify_smaller_images =
		gtk_check_button_new_with_label(_("Do not magnify images with smaller width"));
	gtk_widget_set_name(check_magnify_smaller_images, "check_on_notebook");
	if (!options.magnify_smaller_images) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_magnify_smaller_images), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_cart), check_magnify_smaller_images, FALSE, FALSE, 0);


	frame_ms_pathlist = gtk_frame_new(_("Paths to Music Store databases"));
	gtk_box_pack_start(GTK_BOX(vbox_ms), frame_ms_pathlist, FALSE, TRUE, 5);
	
	vbox_ms_pathlist = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_ms_pathlist), 10);
	gtk_container_add(GTK_CONTAINER(frame_ms_pathlist), vbox_ms_pathlist);

	if (!ms_pathlist_store) {
		ms_pathlist_store = gtk_list_store_new(3,
						    G_TYPE_STRING,     /* path */
						    G_TYPE_STRING,     /* displayed name */
						    G_TYPE_STRING);    /* state (rw, r, unreachable) */
	}

	ms_pathlist_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ms_pathlist_store));
	ms_pathlist_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(ms_pathlist_view));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Path"), renderer,
							  "text", 1,
							  NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(ms_pathlist_view), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Access"), renderer,
							  "text", 2,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(ms_pathlist_view), column);

	gtk_widget_set_size_request(ms_pathlist_view, -1, 100);
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(ms_pathlist_view), TRUE);
        gtk_tree_view_set_reorderable(GTK_TREE_VIEW(ms_pathlist_view), TRUE);

	viewport = gtk_viewport_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox_ms_pathlist), viewport, FALSE, TRUE, 5);

        scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(viewport), scrolled_win);
	gtk_container_add(GTK_CONTAINER(scrolled_win), ms_pathlist_view);
	
	hbox_ms_pathlist = gtk_hbox_new(FALSE, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_ms_pathlist), hbox_ms_pathlist, FALSE, FALSE, 0);

	entry_ms_pathlist = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox_ms_pathlist), entry_ms_pathlist, TRUE, TRUE, 2);

	browse_ms_pathlist = gui_stock_label_button(_("Browse"), GTK_STOCK_OPEN);
        gtk_container_set_border_width(GTK_CONTAINER(browse_ms_pathlist), 2);
	g_signal_connect (G_OBJECT(browse_ms_pathlist), "clicked",
			  G_CALLBACK(browse_ms_pathlist_clicked), (gpointer)entry_ms_pathlist);
	gtk_box_pack_end(GTK_BOX(hbox_ms_pathlist), browse_ms_pathlist, FALSE, FALSE, 0);

	hbox_ms_pathlist_2 = gtk_hbox_new(FALSE, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_ms_pathlist), hbox_ms_pathlist_2, FALSE, FALSE, 0);

        help_pathlist = gtk_button_new_from_stock (GTK_STOCK_HELP); 
        gtk_container_set_border_width(GTK_CONTAINER(help_pathlist), 2);
	g_signal_connect(help_pathlist, "clicked", G_CALLBACK(display_pathlist_help), NULL);
	gtk_box_pack_start(GTK_BOX(hbox_ms_pathlist_2), help_pathlist, FALSE, FALSE, 0);

	refresh_ms_pathlist = gui_stock_label_button(_("Refresh"), GTK_STOCK_REFRESH);
        gtk_container_set_border_width(GTK_CONTAINER(refresh_ms_pathlist), 2);
	g_signal_connect (G_OBJECT(refresh_ms_pathlist), "clicked",
			  G_CALLBACK(refresh_ms_pathlist_clicked), NULL);
	gtk_box_pack_end(GTK_BOX(hbox_ms_pathlist_2), refresh_ms_pathlist, FALSE, FALSE, 0);
	
	remove_ms_pathlist = gui_stock_label_button(_("Remove"), GTK_STOCK_REMOVE);
        gtk_container_set_border_width(GTK_CONTAINER(remove_ms_pathlist), 2);
	g_signal_connect (G_OBJECT(remove_ms_pathlist), "clicked",
			  G_CALLBACK(remove_ms_pathlist_clicked), NULL);
	gtk_box_pack_end(GTK_BOX(hbox_ms_pathlist_2), remove_ms_pathlist, FALSE, FALSE, 0);

	add_ms_pathlist = gui_stock_label_button(_("Add"), GTK_STOCK_ADD);
        gtk_container_set_border_width(GTK_CONTAINER(add_ms_pathlist), 2);
	g_signal_connect (G_OBJECT(add_ms_pathlist), "clicked",
			  G_CALLBACK(add_ms_pathlist_clicked), NULL);
	gtk_box_pack_end(GTK_BOX(hbox_ms_pathlist_2), add_ms_pathlist, FALSE, FALSE, 0);


	/* "DSP" notebook page */

	vbox_dsp = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_dsp), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_dsp, create_notebook_tab(_("DSP"), "dsp.png"));

	frame_ladspa = gtk_frame_new(_("LADSPA plugin processing"));
	gtk_box_pack_start(GTK_BOX(vbox_dsp), frame_ladspa, FALSE, TRUE, 5);

	vbox_ladspa = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_ladspa), 10);
	gtk_container_add(GTK_CONTAINER(frame_ladspa), vbox_ladspa);

#ifdef HAVE_LADSPA
        optmenu_ladspa = gtk_combo_box_new_text (); 
        gtk_box_pack_start(GTK_BOX(vbox_ladspa), optmenu_ladspa, TRUE, TRUE, 0);

	{
	        gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_ladspa), _("Pre Fader (before Volume & Balance)"));
        	gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_ladspa), _("Post Fader (after Volume & Balance)"));

	}
	status = options.ladspa_is_postfader;
	gtk_combo_box_set_active (GTK_COMBO_BOX (optmenu_ladspa), status);
        g_signal_connect(optmenu_ladspa, "changed", G_CALLBACK(changed_ladspa_prepost), NULL);
#else
	{
		GtkWidget * label = gtk_label_new(_("Aqualung is compiled without LADSPA plugin support.\n\
See the About box and the documentation for details."));
		gtk_box_pack_start(GTK_BOX(vbox_ladspa), label, FALSE, TRUE, 5);
	}
#endif /* HAVE_LADSPA */


	frame_src = gtk_frame_new(_("Sample Rate Converter type"));
	gtk_box_pack_start(GTK_BOX(vbox_dsp), frame_src, FALSE, TRUE, 5);

	vbox_src = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_src), 10);
	gtk_container_add(GTK_CONTAINER(frame_src), vbox_src);

	label_src = gtk_label_new("");

#ifdef HAVE_SRC
	optmenu_src = gtk_combo_box_new_text ();
	gtk_box_pack_start(GTK_BOX(vbox_src), optmenu_src, TRUE, TRUE, 0);

	{
		int i = 0;

		while (src_get_name(i)) {
                        gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_src), src_get_name(i));
			++i;
		}

	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (optmenu_src), src_type);
	g_signal_connect(optmenu_src, "changed", G_CALLBACK(changed_src_type), NULL);

	gtk_label_set_text(GTK_LABEL(label_src), src_get_description(src_type));
#else
	gtk_label_set_text(GTK_LABEL(label_src),
			   _("Aqualung is compiled without Sample Rate Converter support.\n\
See the About box and the documentation for details."));

#endif /* HAVE_SRC */

	gtk_box_pack_start(GTK_BOX(vbox_src), label_src, TRUE, TRUE, 0);


	/* "Playback RVA" notebook page */

	vbox_rva = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_rva), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_rva, create_notebook_tab(_("Playback RVA"), "rva.png"));

	check_rva_is_enabled = gtk_check_button_new_with_label(_("Enable playback RVA"));
	gtk_widget_set_name(check_rva_is_enabled, "check_on_notebook");
	if (options.rva_is_enabled) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_rva_is_enabled), TRUE);
	}
	rva_is_enabled_shadow = options.rva_is_enabled;
	g_signal_connect(G_OBJECT(check_rva_is_enabled), "toggled",
			 G_CALLBACK(check_rva_is_enabled_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_rva), check_rva_is_enabled, FALSE, TRUE, 5);


	table_rva = gtk_table_new(4, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_rva), table_rva, TRUE, TRUE, 3);

	rva_viewport = gtk_viewport_new(NULL, NULL);
	gtk_widget_set_size_request(rva_viewport, 244, 244);
        gtk_table_attach(GTK_TABLE(table_rva), rva_viewport, 0, 2, 0, 1,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 2);
	
	rva_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(GTK_WIDGET(rva_drawing_area), 240, 240);
	gtk_container_add(GTK_CONTAINER(rva_viewport), rva_drawing_area);
	
	g_signal_connect(G_OBJECT(rva_drawing_area), "configure_event",
			 G_CALLBACK(rva_configure_event), NULL);
	g_signal_connect(G_OBJECT(rva_drawing_area), "expose_event",
			 G_CALLBACK(rva_expose_event), NULL);
	
        hbox_listening_env = gtk_hbox_new(FALSE, 0);
        label_listening_env = gtk_label_new(_("Listening environment:"));
        gtk_box_pack_start(GTK_BOX(hbox_listening_env), label_listening_env, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox_listening_env, 0, 1, 1, 2,
                         GTK_FILL, GTK_FILL, 5, 2);

	optmenu_listening_env = gtk_combo_box_new_text ();
        gtk_table_attach(GTK_TABLE(table_rva), optmenu_listening_env, 1, 2, 1, 2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 2);

	{

                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_listening_env), _("Audiophile"));
                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_listening_env), _("Living room"));
                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_listening_env), _("Office"));
                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_listening_env), _("Noisy workshop"));

	}

	rva_env_shadow = options.rva_env;
	gtk_combo_box_set_active (GTK_COMBO_BOX (optmenu_listening_env), options.rva_env);
	g_signal_connect(optmenu_listening_env, "changed", G_CALLBACK(changed_listening_env), NULL);

        hbox_refvol = gtk_hbox_new(FALSE, 0);
        label_refvol = gtk_label_new(_("Reference volume [dBFS] :"));
        gtk_box_pack_start(GTK_BOX(hbox_refvol), label_refvol, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox_refvol, 0, 1, 2, 3,
                         GTK_FILL, GTK_FILL, 5, 2);

	rva_refvol_shadow = options.rva_refvol;
        adj_refvol = gtk_adjustment_new(options.rva_refvol, -24.0f, 0.0f, 0.1f, 1.0f, 0.0f);
        spin_refvol = gtk_spin_button_new(GTK_ADJUSTMENT(adj_refvol), 0.1, 1);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_refvol), TRUE);
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_refvol), FALSE);
	g_signal_connect(G_OBJECT(adj_refvol), "value_changed",
			 G_CALLBACK(refvol_changed), NULL);
        gtk_table_attach(GTK_TABLE(table_rva), spin_refvol, 1, 2, 2, 3,
                         GTK_FILL, GTK_FILL, 5, 2);

        hbox_steepness = gtk_hbox_new(FALSE, 0);
        label_steepness = gtk_label_new(_("Steepness [dB/dB] :"));
        gtk_box_pack_start(GTK_BOX(hbox_steepness), label_steepness, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox_steepness, 0, 1, 3, 4,
                         GTK_FILL, GTK_FILL, 5, 2);

	rva_steepness_shadow = options.rva_steepness;
        adj_steepness = gtk_adjustment_new(options.rva_steepness, 0.0f, 1.0f, 0.01f, 0.1f, 0.0f);
        spin_steepness = gtk_spin_button_new(GTK_ADJUSTMENT(adj_steepness), 0.02, 2);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_steepness), TRUE);
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_steepness), FALSE);
	g_signal_connect(G_OBJECT(adj_steepness), "value_changed",
			 G_CALLBACK(steepness_changed), NULL);
        gtk_table_attach(GTK_TABLE(table_rva), spin_steepness, 1, 2, 3, 4,
                         GTK_FILL, GTK_FILL, 5, 2);



	check_rva_use_averaging =
	    gtk_check_button_new_with_label(_("Apply averaged RVA to tracks of the same record"));
	gtk_widget_set_name(check_rva_use_averaging, "check_on_notebook");
	if (options.rva_use_averaging) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_rva_use_averaging), TRUE);
	}
	rva_use_averaging_shadow = options.rva_use_averaging;
	g_signal_connect(G_OBJECT(check_rva_use_averaging), "toggled",
			 G_CALLBACK(check_rva_use_averaging_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_rva), check_rva_use_averaging, FALSE, TRUE, 5);


	table_avg = gtk_table_new(3, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_rva), table_avg, FALSE, TRUE, 3);

        hbox_threshold = gtk_hbox_new(FALSE, 0);
        label_threshold = gtk_label_new(_("Drop statistical aberrations based on"));
        gtk_box_pack_start(GTK_BOX(hbox_threshold), label_threshold, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_avg), hbox_threshold, 0, 1, 0, 1,
                         GTK_FILL, GTK_FILL, 5, 2);

	optmenu_threshold = gtk_combo_box_new_text ();
        gtk_table_attach(GTK_TABLE(table_avg), optmenu_threshold, 1, 2, 0, 1,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 2);

	{

                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_threshold), _("% of standard deviation"));
                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_threshold), _("Linear threshold [dB]"));

	}

	rva_use_linear_thresh_shadow = options.rva_use_linear_thresh;
	gtk_combo_box_set_active (GTK_COMBO_BOX (optmenu_threshold), options.rva_use_linear_thresh);
	g_signal_connect(optmenu_threshold, "changed", G_CALLBACK(changed_threshold), NULL);

        hbox_linthresh = gtk_hbox_new(FALSE, 0);
        label_linthresh = gtk_label_new(_("Linear threshold [dB] :"));
        gtk_box_pack_start(GTK_BOX(hbox_linthresh), label_linthresh, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_avg), hbox_linthresh, 0, 1, 1, 2,
                         GTK_FILL, GTK_FILL, 5, 2);

	rva_avg_linear_thresh_shadow = options.rva_avg_linear_thresh;
        adj_linthresh = gtk_adjustment_new(options.rva_avg_linear_thresh, 0.0f, 60.0f, 0.1f, 1.0f, 0.0f);
        spin_linthresh = gtk_spin_button_new(GTK_ADJUSTMENT(adj_linthresh), 0.1, 1);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_linthresh), TRUE);
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_linthresh), FALSE);
	g_signal_connect(G_OBJECT(adj_linthresh), "value_changed",
			 G_CALLBACK(linthresh_changed), NULL);
        gtk_table_attach(GTK_TABLE(table_avg), spin_linthresh, 1, 2, 1, 2,
                         GTK_FILL, GTK_FILL, 5, 2);

        hbox_stdthresh = gtk_hbox_new(FALSE, 0);
        label_stdthresh = gtk_label_new(_("% of standard deviation :"));
        gtk_box_pack_start(GTK_BOX(hbox_stdthresh), label_stdthresh, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_avg), hbox_stdthresh, 0, 1, 2, 3,
                         GTK_FILL, GTK_FILL, 5, 2);

	rva_avg_stddev_thresh_shadow = options.rva_avg_stddev_thresh;
        adj_stdthresh = gtk_adjustment_new(options.rva_avg_stddev_thresh * 100.0f,
					   0.0f, 500.0f, 1.0f, 10.0f, 0.0f);
        spin_stdthresh = gtk_spin_button_new(GTK_ADJUSTMENT(adj_stdthresh), 0.5, 0);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_stdthresh), TRUE);
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_stdthresh), FALSE);
	g_signal_connect(G_OBJECT(adj_stdthresh), "value_changed",
			 G_CALLBACK(stdthresh_changed), NULL);
        gtk_table_attach(GTK_TABLE(table_avg), spin_stdthresh, 1, 2, 2, 3,
                         GTK_FILL, GTK_FILL, 5, 2);


	if (!rva_use_averaging_shadow) {
		gtk_widget_set_sensitive(optmenu_threshold, FALSE);
		gtk_widget_set_sensitive(label_threshold, FALSE);
		gtk_widget_set_sensitive(spin_linthresh, FALSE);
		gtk_widget_set_sensitive(label_linthresh, FALSE);
		gtk_widget_set_sensitive(spin_stdthresh, FALSE);
		gtk_widget_set_sensitive(label_stdthresh, FALSE);
	}

	if (!rva_is_enabled_shadow) {
		gtk_widget_set_sensitive(optmenu_listening_env, FALSE);
		gtk_widget_set_sensitive(label_listening_env, FALSE);
		gtk_widget_set_sensitive(spin_refvol, FALSE);
		gtk_widget_set_sensitive(label_refvol, FALSE);
		gtk_widget_set_sensitive(spin_steepness, FALSE);
		gtk_widget_set_sensitive(label_steepness, FALSE);
		gtk_widget_set_sensitive(check_rva_use_averaging, FALSE);
		gtk_widget_set_sensitive(optmenu_threshold, FALSE);
		gtk_widget_set_sensitive(label_threshold, FALSE);
		gtk_widget_set_sensitive(spin_linthresh, FALSE);
		gtk_widget_set_sensitive(label_linthresh, FALSE);
		gtk_widget_set_sensitive(spin_stdthresh, FALSE);
		gtk_widget_set_sensitive(label_stdthresh, FALSE);
	}

	/* "Metadata" notebook page */

	vbox_meta = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_meta), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_meta, create_notebook_tab(_("Metadata"), "metadata.png"));

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("When adding to playlist, use file metadata (if available) "
				"instead of\ninformation from the Music Store for:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);
	check_auto_use_meta_artist = gtk_check_button_new_with_label(_("Artist name"));
	gtk_widget_set_name(check_auto_use_meta_artist, "check_on_notebook");
	if (options.auto_use_meta_artist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_use_meta_artist), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_auto_use_meta_artist, FALSE, TRUE, 20);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);
	check_auto_use_meta_record = gtk_check_button_new_with_label(_("Record name"));
	gtk_widget_set_name(check_auto_use_meta_record, "check_on_notebook");
	if (options.auto_use_meta_record) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_use_meta_record), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_auto_use_meta_record, FALSE, TRUE, 20);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);
	check_auto_use_meta_track = gtk_check_button_new_with_label(_("Track name"));
	gtk_widget_set_name(check_auto_use_meta_track, "check_on_notebook");
	if (options.auto_use_meta_track) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_use_meta_track), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_auto_use_meta_track, FALSE, TRUE, 20);


	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("When adding external files to playlist, use file metadata "
				"(if available) for:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);
	check_auto_use_ext_meta_artist = gtk_check_button_new_with_label(_("Artist name"));
	gtk_widget_set_name(check_auto_use_ext_meta_artist, "check_on_notebook");
	if (options.auto_use_ext_meta_artist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_use_ext_meta_artist), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_auto_use_ext_meta_artist, FALSE, TRUE, 20);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);
	check_auto_use_ext_meta_record = gtk_check_button_new_with_label(_("Record name"));
	gtk_widget_set_name(check_auto_use_ext_meta_record, "check_on_notebook");
	if (options.auto_use_ext_meta_record) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_use_ext_meta_record), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_auto_use_ext_meta_record, FALSE, TRUE, 20);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);
	check_auto_use_ext_meta_track = gtk_check_button_new_with_label(_("Track name"));
	gtk_widget_set_name(check_auto_use_ext_meta_track, "check_on_notebook");
	if (options.auto_use_ext_meta_track) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_use_ext_meta_track), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_auto_use_ext_meta_track, FALSE, TRUE, 20);


	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Replaygain tag to use in Ogg Vorbis and FLAC files: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);


	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 3);

	optmenu_replaygain = gtk_combo_box_new_text ();
        gtk_box_pack_start(GTK_BOX(hbox), optmenu_replaygain, FALSE, FALSE, 20);

	{

                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_replaygain), _("Replaygain_track_gain"));
                gtk_combo_box_append_text (GTK_COMBO_BOX (optmenu_replaygain), _("Replaygain_album_gain"));

	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (optmenu_replaygain), options.replaygain_tag_to_use);


	/* CDDB notebook page */

#ifdef HAVE_CDDB
	table_cddb = gtk_table_new(3, 2, FALSE);
        gtk_container_set_border_width(GTK_CONTAINER(table_cddb), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table_cddb, create_notebook_tab(_("CDDB"), "cddb.png"));

	label = gtk_label_new(_("CDDB server:"));
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_cddb), hbox, 0, 1, 0, 1,
                         GTK_FILL, GTK_FILL, 5, 5);

	cddb_server_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(cddb_server_entry), options.cddb_server);
        gtk_table_attach(GTK_TABLE(table_cddb), cddb_server_entry, 1, 2, 0, 1,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);


	label = gtk_label_new(_("Connection timeout [sec]:"));
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_cddb), hbox, 0, 1, 1, 2,
                         GTK_FILL, GTK_FILL, 5, 5);

	cddb_tout_spinner = gtk_spin_button_new_with_range(1, 60, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(cddb_tout_spinner), options.cddb_timeout);
        gtk_table_attach(GTK_TABLE(table_cddb), cddb_tout_spinner, 1, 2, 1, 2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

	label = gtk_label_new(_("Protocol:"));
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_cddb), hbox, 0, 1, 2, 3,
                         GTK_FILL, GTK_FILL, 5, 5);

	cddb_proto_combo = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(cddb_proto_combo), _("CDDBP (port 888)"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(cddb_proto_combo), _("HTTP (port 80)"));
	if (options.cddb_use_http) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(cddb_proto_combo), 1);
	} else {
		gtk_combo_box_set_active(GTK_COMBO_BOX(cddb_proto_combo), 0);
	}
        gtk_table_attach(GTK_TABLE(table_cddb), cddb_proto_combo, 1, 2, 2, 3,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

#endif /* HAVE_CDDB */

        /* Appearance notebook page */

        override_past_state = options.override_skin_settings;

	vbox_appearance = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_appearance), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_appearance, create_notebook_tab(_("Appearance"), "appearance.png"));

        check_override_skin =
		gtk_check_button_new_with_label(_("Override skin settings"));

        gtk_widget_set_name(check_override_skin, "check_on_notebook");
	if (options.override_skin_settings) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_override_skin), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_appearance), check_override_skin, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (check_override_skin), "toggled",
						G_CALLBACK (cb_toggle_override_skin), NULL);

	frame_fonts = gtk_frame_new(_("Fonts"));
	gtk_box_pack_start(GTK_BOX(vbox_appearance), frame_fonts, FALSE, TRUE, 5);

	vbox_fonts = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_fonts), 10);
	gtk_container_add(GTK_CONTAINER(frame_fonts), vbox_fonts);

        /* playlist font */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_fonts), hbox, FALSE, FALSE, 3);

	label = gtk_label_new(_("Playlist: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

        entry_pl_font = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry_pl_font, GTK_CAN_FOCUS);
        gtk_box_pack_start(GTK_BOX(hbox), entry_pl_font, TRUE, TRUE, 3);
        gtk_editable_set_editable(GTK_EDITABLE(entry_pl_font), FALSE);

        if (strlen(options.playlist_font)) {

                gtk_entry_set_text(GTK_ENTRY(entry_pl_font), options.playlist_font);

        } else {

                gtk_entry_set_text(GTK_ENTRY(entry_pl_font), DEFAULT_FONT_NAME);
                strcpy(options.playlist_font, DEFAULT_FONT_NAME);
        }

        button_pl_font =  gui_stock_label_button(_("Select"), GTK_STOCK_SELECT_FONT);
        gtk_box_pack_start(GTK_BOX(hbox), button_pl_font, FALSE, FALSE, 3);
	g_signal_connect (G_OBJECT (button_pl_font), "clicked",
						G_CALLBACK (playlist_font_select), NULL);

        /* music store font */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_fonts), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Music Store: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

        entry_ms_font = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry_ms_font, GTK_CAN_FOCUS);
        gtk_box_pack_start(GTK_BOX(hbox), entry_ms_font, TRUE, TRUE, 3);
        gtk_editable_set_editable(GTK_EDITABLE(entry_ms_font), FALSE);

        if (strlen(options.browser_font)) {

	        gtk_entry_set_text(GTK_ENTRY(entry_ms_font), options.browser_font);

        } else {

                gtk_entry_set_text(GTK_ENTRY(entry_ms_font), DEFAULT_FONT_NAME);
                strcpy(options.browser_font, DEFAULT_FONT_NAME);

        }

        button_ms_font = gui_stock_label_button(_("Select"), GTK_STOCK_SELECT_FONT);
        gtk_box_pack_start(GTK_BOX(hbox), button_ms_font, FALSE, FALSE, 3);
	g_signal_connect (G_OBJECT (button_ms_font), "clicked",
						G_CALLBACK (browser_font_select), NULL);


        /* big timer font */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_fonts), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Big timer: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

        entry_bt_font = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry_bt_font, GTK_CAN_FOCUS);
        gtk_box_pack_start(GTK_BOX(hbox), entry_bt_font, TRUE, TRUE, 3);
        gtk_editable_set_editable(GTK_EDITABLE(entry_bt_font), FALSE);

        if (strlen(options.bigtimer_font)) {

	        gtk_entry_set_text(GTK_ENTRY(entry_bt_font), options.bigtimer_font);

        } else {

                gtk_entry_set_text(GTK_ENTRY(entry_bt_font), DEFAULT_FONT_NAME);
                strcpy(options.bigtimer_font, DEFAULT_FONT_NAME);

        }

        button_bt_font = gui_stock_label_button(_("Select"), GTK_STOCK_SELECT_FONT);
        gtk_box_pack_start(GTK_BOX(hbox), button_bt_font, FALSE, FALSE, 3);
	g_signal_connect (G_OBJECT (button_bt_font), "clicked",
						G_CALLBACK (bigtimer_font_select), NULL);

        /* small timer font */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_fonts), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Small timers: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

        entry_st_font = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry_st_font, GTK_CAN_FOCUS);
        gtk_box_pack_start(GTK_BOX(hbox), entry_st_font, TRUE, TRUE, 3);
        gtk_editable_set_editable(GTK_EDITABLE(entry_st_font), FALSE);

        if (strlen(options.smalltimer_font)) {

	        gtk_entry_set_text(GTK_ENTRY(entry_st_font), options.smalltimer_font);

        } else {

                gtk_entry_set_text(GTK_ENTRY(entry_st_font), DEFAULT_FONT_NAME);
                strcpy(options.smalltimer_font, DEFAULT_FONT_NAME);

        }

        button_st_font = gui_stock_label_button(_("Select"), GTK_STOCK_SELECT_FONT);
        gtk_box_pack_start(GTK_BOX(hbox), button_st_font, FALSE, FALSE, 3);
	g_signal_connect (G_OBJECT (button_st_font), "clicked",
						G_CALLBACK (smalltimer_font_select), NULL);

        /* song title font */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_fonts), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Song title: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

        entry_songt_font = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry_songt_font, GTK_CAN_FOCUS);
        gtk_box_pack_start(GTK_BOX(hbox), entry_songt_font, TRUE, TRUE, 3);
        gtk_editable_set_editable(GTK_EDITABLE(entry_songt_font), FALSE);

        if (strlen(options.songtitle_font)) {

	        gtk_entry_set_text(GTK_ENTRY(entry_songt_font), options.songtitle_font);

        } else {

                gtk_entry_set_text(GTK_ENTRY(entry_songt_font), DEFAULT_FONT_NAME);
                strcpy(options.songtitle_font, DEFAULT_FONT_NAME);

        }

        button_songt_font = gui_stock_label_button(_("Select"), GTK_STOCK_SELECT_FONT);
        gtk_box_pack_start(GTK_BOX(hbox), button_songt_font, FALSE, FALSE, 3);
	g_signal_connect (G_OBJECT (button_songt_font), "clicked",
						G_CALLBACK (songtitle_font_select), NULL);

        /* song info font */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_fonts), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Song info: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

        entry_si_font = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry_si_font, GTK_CAN_FOCUS);
        gtk_box_pack_start(GTK_BOX(hbox), entry_si_font, TRUE, TRUE, 3);
        gtk_editable_set_editable(GTK_EDITABLE(entry_si_font), FALSE);

        if (strlen(options.songinfo_font)) {

	        gtk_entry_set_text(GTK_ENTRY(entry_si_font), options.songinfo_font);

        } else {

                gtk_entry_set_text(GTK_ENTRY(entry_si_font), DEFAULT_FONT_NAME);
                strcpy(options.songinfo_font, DEFAULT_FONT_NAME);

        }

        button_si_font = gui_stock_label_button(_("Select"), GTK_STOCK_SELECT_FONT);
        gtk_box_pack_start(GTK_BOX(hbox), button_si_font, FALSE, FALSE, 3);
	g_signal_connect (G_OBJECT (button_si_font), "clicked",
						G_CALLBACK (songinfo_font_select), NULL);

        /* statusbar font */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_fonts), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Statusbar: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

        entry_sb_font = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(entry_sb_font, GTK_CAN_FOCUS);
        gtk_box_pack_start(GTK_BOX(hbox), entry_sb_font, TRUE, TRUE, 3);
        gtk_editable_set_editable(GTK_EDITABLE(entry_sb_font), FALSE);

        if (strlen(options.statusbar_font)) {

	        gtk_entry_set_text(GTK_ENTRY(entry_sb_font), options.statusbar_font);

        } else {

                gtk_entry_set_text(GTK_ENTRY(entry_sb_font), DEFAULT_FONT_NAME);
                strcpy(options.statusbar_font, DEFAULT_FONT_NAME);

        }

        button_sb_font = gui_stock_label_button(_("Select"), GTK_STOCK_SELECT_FONT);
        gtk_box_pack_start(GTK_BOX(hbox), button_sb_font, FALSE, FALSE, 3);
	g_signal_connect (G_OBJECT (button_sb_font), "clicked",
						G_CALLBACK (statusbar_font_select), NULL);

        /* colors */

	frame_colors = gtk_frame_new(_("Colors"));
	gtk_box_pack_start(GTK_BOX(vbox_appearance), frame_colors, FALSE, TRUE, 5);

	vbox_colors = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_colors), 10);
	gtk_container_add(GTK_CONTAINER(frame_colors), vbox_colors);

        /* active song */

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox_colors), hbox, FALSE, TRUE, 3);

	label = gtk_label_new(_("Active song in playlist: "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
        gtk_size_group_add_widget(label_size, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	hbox_s = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), hbox_s, TRUE, TRUE, 3);

        if (gdk_color_parse(options.activesong_color, &color) == FALSE) {

                color.red = play_list->style->fg[SELECTED].red; 
                color.green = play_list->style->fg[SELECTED].green; 
                color.blue = play_list->style->fg[SELECTED].blue; 
                color.pixel = (gulong)((color.red & 0xff00)*256 + (color.green & 0xff00) + (color.blue & 0xff00)/256); 
        }

        color_picker = gtk_color_button_new_with_color (&color);
        gtk_widget_set_size_request(GTK_WIDGET(color_picker), 60, -1);
        gtk_box_pack_start(GTK_BOX(hbox), color_picker, FALSE, TRUE, 3);
	g_signal_connect (G_OBJECT (color_picker), "color-set",
						G_CALLBACK (color_selected), NULL);


        set_sensitive_part();

        /* end of notebook */

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);
        gtk_box_set_spacing(GTK_BOX(hbuttonbox), 8);
        gtk_container_set_border_width(GTK_CONTAINER(hbuttonbox), 3);

        ok_btn = gtk_button_new_from_stock (GTK_STOCK_OK); 
	g_signal_connect(ok_btn, "clicked", G_CALLBACK(ok), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), ok_btn);   

        cancel_btn = gtk_button_new_from_stock (GTK_STOCK_CANCEL); 
        g_signal_connect(cancel_btn, "clicked", G_CALLBACK(cancel), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), cancel_btn);   

	gtk_widget_show_all(options_window);

        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), current_notebook_page);

        set_sliders_width();    /* MAGIC */
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

