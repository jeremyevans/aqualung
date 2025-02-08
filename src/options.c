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
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <libxml/globals.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef HAVE_CDDA
#undef HAVE_CDDB
#include "undef_ac_pkg.h"
#ifdef HAVE_CDIO_PARANOIA_PARANOIA_H
#include <cdio/paranoia/paranoia.h>
#else /* !HAVE_CDIO_PARANOIA_PARANOIA_H (assume older, bundled, layout) */
#include <cdio/paranoia.h>
#endif /* !HAVE_CDIO_PARANOIA_PARANOIA_H */
#undef HAVE_CDDB
#include "undef_ac_pkg.h"
#include <config.h>	/* re-establish undefined autoconf macros */
#endif /* HAVE_CDDA */

#ifdef HAVE_JACK
#include <jack/jack.h>
#endif /* HAVE_JACK */

#ifdef HAVE_SRC
#include <samplerate.h>
#endif /* HAVE_SRC */

#include "ext_lua.h"
#include "common.h"
#include "utils.h"
#include "utils_gui.h"
#include "gui_main.h"
#include "music_browser.h"
#include "store_file.h"
#include "search.h"
#include "playlist.h"
#include "i18n.h"
#include "options.h"


options_t options;

static int current_notebook_page = 0;

extern int src_type_parsed;

extern GtkWidget * main_window;
extern GtkWidget * playlist_window;
extern GtkWidget * playlist_color_indicator;

extern GtkWidget * music_tree;
extern GtkTreeStore * music_store;

int rva_is_enabled_shadow;
int rva_env_shadow;
float rva_refvol_shadow;
float rva_steepness_shadow;
int rva_use_averaging_shadow;
int rva_use_linear_thresh_shadow;
float rva_avg_linear_thresh_shadow;
float rva_avg_stddev_thresh_shadow;
float rva_no_rva_voladj_shadow;
char ext_title_format_file_shadow[MAXLEN];

int restart_flag;
int track_name_in_bold_shadow;

GtkWidget * options_window;
GtkWidget * notebook;

GtkWidget * entry_title;
GtkWidget * entry_param;
GtkWidget * check_dark_theme;
GtkWidget * check_enable_tooltips;
GtkWidget * check_buttons_at_the_bottom;
GtkWidget * check_disable_buttons_relief;
GtkWidget * check_combine_play_pause;
GtkWidget * check_main_window_always_on_top;
GtkWidget * check_simple_view_in_fx;
GtkWidget * check_united_minimization;
GtkWidget * check_show_sn_title;
GtkWidget * check_show_hidden;
GtkWidget * check_tags_tab_first;
GtkWidget * combo_cwidth;
GtkWidget * check_magnify_smaller_images;
GtkWidget * check_dont_show_cover;
GtkWidget * check_use_external_cover_first;
GtkWidget * check_show_cover_for_ms_tracks_only;

#ifdef HAVE_SYSTRAY
GtkWidget * check_use_systray;
GtkWidget * check_systray_start_minimized;
GtkWidget * get_mouse_button_window;
GtkWidget * frame_systray_mouse_wheel;
GtkWidget * frame_systray_mouse_buttons;
GtkWidget * combo_systray_mouse_wheel_horizontal;
GtkWidget * combo_systray_mouse_wheel_vertical;
GtkListStore * systray_mouse_buttons_store;
GtkListStore * systray_mouse_buttons_cmds_store;
GtkTreeSelection * systray_mouse_buttons_selection;
#endif /* HAVE_SYSTRAY */

GtkWidget * check_playlist_is_embedded;
GtkWidget * check_autoplsave;
GtkWidget * check_playlist_auto_save;
GtkWidget * spin_playlist_auto_save;
GtkWidget * check_playlist_is_tree;
GtkWidget * check_playlist_always_show_tabs;
GtkWidget * check_show_close_button_in_tab;
GtkWidget * check_album_shuffle_mode;
GtkWidget * check_enable_playlist_statusbar;
GtkWidget * check_pl_statusbar_show_size;
GtkWidget * check_show_rva_in_playlist;
GtkWidget * check_show_length_in_playlist;
GtkWidget * check_show_active_track_name_in_bold;
GtkWidget * check_auto_roll_to_active_track;
GtkWidget * check_enable_pl_rules_hint;
GtkListStore * plistcol_store;

GtkWidget * check_hide_comment_pane;
GtkWidget * check_enable_mstore_toolbar;
GtkWidget * check_enable_mstore_statusbar;
GtkWidget * check_ms_statusbar_show_size;
GtkWidget * check_expand_stores;
GtkWidget * check_enable_ms_rules_hint;
GtkWidget * check_enable_ms_tree_icons;
GtkWidget * check_ms_confirm_removal;
GtkListStore * ms_pathlist_store = NULL;
GtkTreeSelection * ms_pathlist_select;
GtkWidget * entry_ms_pathlist;
#ifdef HAVE_LUA
GtkWidget * entry_ext_title_format_file;
#endif /* HAVE_LUA */

#ifdef HAVE_LADSPA
GtkWidget * combo_ladspa;
#endif /* HAVE_LADSPA */
#ifdef HAVE_SRC
GtkWidget * combo_src;
#endif /* HAVE_SRC */
GtkWidget * label_src;

GtkWidget * check_rva_is_enabled;
GtkWidget * rva_drawing_area;
GtkWidget * rva_viewport;
GtkWidget * combo_listening_env;
GtkWidget * spin_refvol;
GtkWidget * spin_steepness;
GtkWidget * check_rva_use_averaging;
GtkWidget * combo_threshold;
GtkWidget * spin_linthresh;
GtkWidget * spin_stdthresh;
GtkWidget * spin_defvol;
GObject * adj_refvol;
GObject * adj_steepness;
GObject * adj_linthresh;
GObject * adj_stdthresh;
GObject * adj_defvol;
GtkWidget * label_listening_env;
GtkWidget * label_refvol;
GtkWidget * label_steepness;
GtkWidget * label_threshold;
GtkWidget * label_linthresh;
GtkWidget * label_stdthresh;
GtkWidget * label_defvol;

GtkWidget * combo_replaygain;
GtkWidget * check_batch_mpeg_add_id3v1;
GtkWidget * check_batch_mpeg_add_id3v2;
GtkWidget * check_batch_mpeg_add_ape;
GtkWidget * check_meta_use_basename_only;
GtkWidget * check_meta_rm_extension;
GtkWidget * check_meta_us_to_space;
GtkWidget * check_metaedit_auto_clone;
GtkWidget * encode_set_entry;

#ifdef HAVE_CDDA
GtkWidget * cdda_drive_speed_spinner;
GtkWidget * check_cdda_mode_overlap;
GtkWidget * check_cdda_mode_verify;
GtkWidget * check_cdda_mode_neverskip;
GtkWidget * check_cdda_force_drive_rescan;
GtkWidget * check_cdda_add_to_playlist;
GtkWidget * check_cdda_remove_from_playlist;
GtkWidget * label_cdda_maxretries;
GtkWidget * cdda_paranoia_maxretries_spinner;
#endif /* HAVE_CDDA */

#ifdef HAVE_CDDB
GtkWidget * cddb_server_entry;
GtkWidget * cddb_tout_spinner;
GtkWidget * cddb_proto_combo;
GtkWidget * cddb_label_proto;
#endif /* HAVE_CDDB */

GtkWidget * inet_radio_direct;
GtkWidget * inet_radio_proxy;
GtkWidget * inet_frame;
GtkWidget * inet_label_proxy;
GtkWidget * inet_entry_proxy;
GtkWidget * inet_label_proxy_port;
GtkWidget * inet_spinner_proxy_port;
GtkWidget * inet_label_noproxy_domains;
GtkWidget * inet_entry_noproxy_domains;
GtkWidget * inet_help_noproxy_domains;
GtkWidget * inet_spinner_timeout;

void show_restart_info(void);
void restart_active(GtkToggleButton *, gpointer);

#ifdef HAVE_SYSTRAY
void systray_mouse_button_add_clicked(GtkWidget *, gpointer);
void systray_mouse_button_remove_clicked(GtkWidget *, gpointer);

gchar * systray_mb_cmd_names[SYSTRAY_MB_CMD_LAST];
gint systray_mb_col_command_combo_cmd = -1;
int get_mb_window_button;

#endif /* HAVE_SYSTRAY */

void load_systray_options(xmlDocPtr, xmlNodePtr);
void save_systray_options(xmlDocPtr, xmlNodePtr);

#ifdef HAVE_JACK
extern jack_port_t * out_L_port;
extern jack_port_t * out_R_port;
extern GSList * saved_pconns_L;
extern GSList * saved_pconns_R;
void load_jack_connections(xmlDocPtr, xmlNodePtr);
void save_jack_connections(xmlDocPtr, xmlNodePtr);
#endif /* HAVE_JACK */

GtkListStore * restart_list_store = NULL;


int
diff_option_from_toggle(GtkWidget * widget, int opt) {

	gboolean act = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	return (act && !opt) || (!act && opt);
}

void
music_store_remove_unset_stores(void) {
	int i;
	GtkTreeIter iter;
	GtkTreeIter iter2;

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &iter, NULL, i++)) {
		int j;
		int has;
		store_t * data;
		store_data_t * store_data;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &data, -1);

		if (data->type != STORE_TYPE_FILE) {
			continue;
		}

		store_data = (store_data_t *)data;

		j = 0;
		has = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ms_pathlist_store),
						     &iter2, NULL, j++)) {
			char * file;

			gtk_tree_model_get(GTK_TREE_MODEL(ms_pathlist_store), &iter2, 0, &file, -1);
			if (strcmp(store_data->file, file) == 0) {

				if (access(file, R_OK) == 0) {
					has = 1;
				}

				g_free(file);
				break;
			}

			g_free(file);
		}

		if (!has) {
			if (store_data->dirty) {
				int ret;
				char * name;

				gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter,
						   MS_COL_NAME, &name, -1);

				ret = message_dialog(_("Settings"),
						     options_window,
						     GTK_MESSAGE_QUESTION,
						     GTK_BUTTONS_YES_NO,
						     NULL,
						     _("Do you want to save store \"%s\" before "
						       "removing from Music Store?"),
						     name + 1);

				if (ret == GTK_RESPONSE_YES) {
					store_file_save(&iter);
				} else {
					music_store_mark_saved(&iter);
				}

				g_free(name);
			}

			store_file_remove_store(&iter);
			--i;
		}
	}
}

void
music_store_remove_non_existent_stores(void) {
	int i;
	GtkTreeIter iter;

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &iter, NULL, i++)) {
		store_t * data;
		store_data_t * store_data;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &data, -1);

		if (data->type != STORE_TYPE_FILE) {
			continue;
		}

		store_data = (store_data_t *)data;

		if (access(store_data->file, R_OK) != 0) {
			if (!store_data->dirty) {
				store_file_remove_store(&iter);
				--i;
			}
		}
	}
}

void
music_store_add_new_stores() {
	int i;
	GtkTreeIter iter;
	GtkTreeIter iter2;

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ms_pathlist_store),
					     &iter, NULL, i++)) {
		char * file;
		int j;
		int has;
		char sort[16];

		arr_snprintf(sort, "%03d", i+1);

		gtk_tree_model_get(GTK_TREE_MODEL(ms_pathlist_store), &iter, 0, &file, -1);

		j = 0;
		has = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     &iter2, NULL, j++)) {
			store_t * data;
			store_data_t * store_data;

			gtk_tree_model_get(GTK_TREE_MODEL(music_store),
					   &iter2, MS_COL_DATA, &data, -1);

			if (data->type != STORE_TYPE_FILE) {
				continue;
			}

			store_data = (store_data_t *)data;

			if (strcmp(file, store_data->file) == 0) {

				gtk_tree_store_set(music_store, &iter2, MS_COL_SORT, sort, -1);

				if (access(store_data->file, W_OK) == 0) {
					store_data->readonly = 0;
				} else {
					store_data->readonly = 1;
				}

				has = 1;
				break;
			}
		}

		if (!has) {
			store_file_load(file, sort);
		}

		g_free(file);
	}
}

gboolean
options_store_watcher_cb(gpointer data) {

	music_store_remove_non_existent_stores();
	music_store_add_new_stores();
	return TRUE;
}

void
options_store_watcher_start(void) {

	aqualung_timeout_add(5000, options_store_watcher_cb, NULL);
}

void
options_window_accept(void) {

	int i;
	GtkTreeIter iter;
	int title_format_changed = 0;
	int metadata_encoding_changed = 0;

#ifdef HAVE_LUA
	char * ext_title;
	ext_title = (char *)gtk_entry_get_text(GTK_ENTRY(entry_ext_title_format_file));
	if(strncmp(ext_title, options.ext_title_format_file, MAXLEN-1)) {
		arr_strlcpy(options.ext_title_format_file, ext_title);
		setup_extended_title_formatting();
	}
#endif /* HAVE_LUA */


        if (restart_flag) {
                show_restart_info();
        }


	/* General */

	if (diff_option_from_toggle(check_meta_use_basename_only, options.meta_use_basename_only) ||
	    diff_option_from_toggle(check_meta_rm_extension, options.meta_rm_extension) ||
	    diff_option_from_toggle(check_meta_us_to_space, options.meta_us_to_space) ||
	    strcmp(options.title_format, gtk_entry_get_text(GTK_ENTRY(entry_title)))) {
		title_format_changed = 1;
	}

	if (strcmp(options.encode_set, gtk_entry_get_text(GTK_ENTRY(encode_set_entry)))) {
		metadata_encoding_changed = 1;
		title_format_changed = 1;
	}

	arr_strlcpy(options.title_format, gtk_entry_get_text(GTK_ENTRY(entry_title)));
	arr_strlcpy(options.default_param, gtk_entry_get_text(GTK_ENTRY(entry_param)));

	set_option_from_toggle(check_enable_tooltips, &options.enable_tooltips);
	aqualung_tooltips_set_enabled(options.enable_tooltips);

#ifdef HAVE_LADSPA
        set_option_from_toggle(check_simple_view_in_fx, &options.simple_view_in_fx_shadow);
#endif /* HAVE_LADSPA */
	set_option_from_toggle(check_united_minimization, &options.united_minimization);
	set_option_from_toggle(check_show_hidden, &options.show_hidden);
        set_option_from_toggle(check_tags_tab_first, &options.tags_tab_first);

	set_option_from_combo(combo_cwidth, &options.cover_width);
	set_option_from_toggle(check_magnify_smaller_images, &options.magnify_smaller_images);
	set_option_from_toggle(check_dont_show_cover, &options.dont_show_cover);
	set_option_from_toggle(check_use_external_cover_first, &options.use_external_cover_first);
        set_option_from_toggle(check_show_cover_for_ms_tracks_only, &options.show_cover_for_ms_tracks_only);

	options.magnify_smaller_images = !options.magnify_smaller_images;
        if(options.dont_show_cover) {
                hide_cover_thumbnail();
        }

	set_option_from_toggle(check_dark_theme, &options.dark_theme);
	set_option_from_toggle(check_disable_buttons_relief, &options.disable_buttons_relief);
	set_option_from_toggle(check_combine_play_pause, &options.combine_play_pause_shadow);
	set_option_from_toggle(check_main_window_always_on_top, &options.main_window_always_on_top);
	set_option_from_toggle(check_show_sn_title, &options.show_sn_title);

#ifdef HAVE_SYSTRAY
	set_option_from_toggle(check_use_systray, &options.use_systray);
	set_option_from_toggle(check_systray_start_minimized, &options.systray_start_minimized);

	set_option_from_combo(combo_systray_mouse_wheel_horizontal, &options.systray_mouse_wheel_horizontal);
	set_option_from_combo(combo_systray_mouse_wheel_vertical, &options.systray_mouse_wheel_vertical);

	options.systray_mouse_buttons_count = 0;
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(systray_mouse_buttons_store), &iter)) {
		do {
			++options.systray_mouse_buttons_count;
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(systray_mouse_buttons_store), &iter));
	}

	options.systray_mouse_buttons = realloc(options.systray_mouse_buttons,
	        options.systray_mouse_buttons_count * sizeof(mouse_button_command_t));
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(systray_mouse_buttons_store), &iter)) {
		i = 0;
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(systray_mouse_buttons_store), &iter,
				SYSTRAY_MB_COL_BUTTON, &options.systray_mouse_buttons[i].button_nr,
				SYSTRAY_MB_COL_COMMAND, &options.systray_mouse_buttons[i].command, -1);
			++i;
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(systray_mouse_buttons_store), &iter));
	}

#endif /* HAVE_SYSTRAY */

	/* Playlist */
	set_option_from_toggle(check_autoplsave, &options.auto_save_playlist);
	set_option_from_toggle(check_playlist_auto_save, &options.playlist_auto_save);

	playlist_auto_save_reset();

	set_option_from_spin(spin_playlist_auto_save, &options.playlist_auto_save_int);
	set_option_from_toggle(check_playlist_is_embedded, &options.playlist_is_embedded_shadow);
	set_option_from_toggle(check_buttons_at_the_bottom, &options.buttons_at_the_bottom_shadow);
	set_option_from_toggle(check_playlist_is_tree, &options.playlist_is_tree);
	set_option_from_toggle(check_playlist_always_show_tabs, &options.playlist_always_show_tabs);

        set_option_from_toggle(check_show_close_button_in_tab, &options.playlist_show_close_button_in_tab);
        playlist_show_hide_close_buttons(options.playlist_show_close_button_in_tab);

        set_option_from_toggle(check_album_shuffle_mode, &options.album_shuffle_mode);
	set_option_from_toggle(check_enable_playlist_statusbar, &options.enable_playlist_statusbar_shadow);
	set_option_from_toggle(check_pl_statusbar_show_size, &options.pl_statusbar_show_size);
	set_option_from_toggle(check_show_rva_in_playlist, &options.show_rva_in_playlist);
	set_option_from_toggle(check_show_length_in_playlist, &options.show_length_in_playlist);
	set_option_from_toggle(check_show_active_track_name_in_bold, &options.show_active_track_name_in_bold);
	if (!options.show_active_track_name_in_bold) {
                playlist_disable_bold_font();
	}

	set_option_from_toggle(check_auto_roll_to_active_track, &options.auto_roll_to_active_track);
	set_option_from_toggle(check_enable_pl_rules_hint, &options.enable_pl_rules_hint);

	/* Music Store */

	set_option_from_toggle(check_hide_comment_pane, &options.hide_comment_pane_shadow);


        set_option_from_toggle(check_enable_mstore_toolbar, &options.enable_mstore_toolbar_shadow);
	set_option_from_toggle(check_enable_mstore_statusbar, &options.enable_mstore_statusbar_shadow);
	set_option_from_toggle(check_ms_statusbar_show_size, &options.ms_statusbar_show_size);
	set_option_from_toggle(check_expand_stores, &options.autoexpand_stores);

	set_option_from_toggle(check_enable_ms_rules_hint, &options.enable_ms_rules_hint);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(music_tree), options.enable_ms_rules_hint);

	set_option_from_toggle(check_enable_ms_tree_icons, &options.enable_ms_tree_icons_shadow);
	set_option_from_toggle(check_ms_confirm_removal, &options.ms_confirm_removal);


	/* RVA */

	options.rva_is_enabled = rva_is_enabled_shadow;
	options.rva_env = rva_env_shadow;
	options.rva_refvol = rva_refvol_shadow;
	options.rva_steepness = rva_steepness_shadow;
	options.rva_use_averaging = rva_use_averaging_shadow;
	options.rva_use_linear_thresh = rva_use_linear_thresh_shadow;
	options.rva_avg_linear_thresh = rva_avg_linear_thresh_shadow;
	options.rva_avg_stddev_thresh = rva_avg_stddev_thresh_shadow;
	options.rva_no_rva_voladj = rva_no_rva_voladj_shadow;


	/* Metadata */

	set_option_from_combo(combo_replaygain, &options.replaygain_tag_to_use);

	set_option_from_toggle(check_batch_mpeg_add_id3v1, &options.batch_mpeg_add_id3v1);
	set_option_from_toggle(check_batch_mpeg_add_id3v2, &options.batch_mpeg_add_id3v2);
	set_option_from_toggle(check_batch_mpeg_add_ape, &options.batch_mpeg_add_ape);

	set_option_from_entry(encode_set_entry, options.encode_set, CHAR_ARRAY_SIZE(options.encode_set));

	set_option_from_toggle(check_metaedit_auto_clone, &options.metaedit_auto_clone);
	set_option_from_toggle(check_meta_use_basename_only, &options.meta_use_basename_only);
	set_option_from_toggle(check_meta_rm_extension, &options.meta_rm_extension);
	set_option_from_toggle(check_meta_us_to_space, &options.meta_us_to_space);

	if (metadata_encoding_changed) {
		playlist_update_metadata();
	}

	if (title_format_changed) {
		playlist_reset_display_names();
	}

	/* CDDA */
#ifdef HAVE_CDDA
	set_option_from_spin(cdda_drive_speed_spinner, &options.cdda_drive_speed);
	options.cdda_paranoia_mode =
		(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_cdda_mode_overlap)) ? PARANOIA_MODE_OVERLAP : 0) |
		(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_cdda_mode_verify)) ? PARANOIA_MODE_VERIFY : 0) |
		(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_cdda_mode_neverskip)) ? PARANOIA_MODE_NEVERSKIP : 0);
	set_option_from_spin(cdda_paranoia_maxretries_spinner, &options.cdda_paranoia_maxretries);
	set_option_from_toggle(check_cdda_force_drive_rescan, &options.cdda_force_drive_rescan);
	set_option_from_toggle(check_cdda_add_to_playlist, &options.cdda_add_to_playlist);
	set_option_from_toggle(check_cdda_remove_from_playlist, &options.cdda_remove_from_playlist);
#endif /* HAVE_CDDA */


	/* CDDB */
#ifdef HAVE_CDDB
	set_option_from_entry(cddb_server_entry, options.cddb_server, CHAR_ARRAY_SIZE(options.cddb_server));
	set_option_from_spin(cddb_tout_spinner, &options.cddb_timeout);
	set_option_from_combo(cddb_proto_combo, &options.cddb_use_http);
#endif /* HAVE_CDDB */


	/* Internet */

        set_option_from_toggle(inet_radio_proxy, &options.inet_use_proxy);
	set_option_from_entry(inet_entry_proxy, options.inet_proxy, CHAR_ARRAY_SIZE(options.inet_proxy));
	set_option_from_spin(inet_spinner_proxy_port, &options.inet_proxy_port);
	set_option_from_entry(inet_entry_noproxy_domains, options.inet_noproxy_domains, CHAR_ARRAY_SIZE(options.inet_noproxy_domains));
	set_option_from_spin(inet_spinner_timeout, &options.inet_timeout);

	music_store_remove_unset_stores();

	music_store_add_new_stores();

	set_buttons_relief();
	refresh_displays();
	gtk_window_set_keep_above(GTK_WINDOW(main_window), options.main_window_always_on_top);

	music_store_selection_changed(STORE_TYPE_ALL);
	playlist_content_changed(playlist_get_current());
	playlist_selection_changed(playlist_get_current());

	save_config();
}


#ifdef HAVE_LADSPA
void
changed_ladspa_prepost(GtkWidget * widget, gpointer * data) {

	int status = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_ladspa));
	options.ladspa_is_postfader = status;
}
#endif /* HAVE_LADSPA */


#ifdef HAVE_SRC
void
changed_src_type(GtkWidget * widget, gpointer * data) {

	options.src_type = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_src));
	gtk_label_set_text(GTK_LABEL(label_src), src_get_description(options.src_type));
	set_src_type_label(options.src_type);
}
#endif /* HAVE_SRC */


void
check_rva_is_enabled_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_rva_is_enabled))) {
		rva_is_enabled_shadow = 1;
		gtk_widget_set_sensitive(combo_listening_env, TRUE);
		gtk_widget_set_sensitive(label_listening_env, TRUE);
		gtk_widget_set_sensitive(spin_refvol, TRUE);
		gtk_widget_set_sensitive(label_refvol, TRUE);
		gtk_widget_set_sensitive(spin_steepness, TRUE);
		gtk_widget_set_sensitive(label_steepness, TRUE);

		gtk_widget_set_sensitive(check_rva_use_averaging, TRUE);

		gtk_widget_set_sensitive(spin_defvol, TRUE);
		gtk_widget_set_sensitive(label_defvol, TRUE);

		if (rva_use_averaging_shadow) {
			gtk_widget_set_sensitive(combo_threshold, TRUE);
			gtk_widget_set_sensitive(label_threshold, TRUE);
			gtk_widget_set_sensitive(spin_linthresh, TRUE);
			gtk_widget_set_sensitive(label_linthresh, TRUE);
			gtk_widget_set_sensitive(spin_stdthresh, TRUE);
			gtk_widget_set_sensitive(label_stdthresh, TRUE);
		}
	} else {
		rva_is_enabled_shadow = 0;
		gtk_widget_set_sensitive(combo_listening_env, FALSE);
		gtk_widget_set_sensitive(label_listening_env, FALSE);
		gtk_widget_set_sensitive(spin_refvol, FALSE);
		gtk_widget_set_sensitive(label_refvol, FALSE);
		gtk_widget_set_sensitive(spin_steepness, FALSE);
		gtk_widget_set_sensitive(label_steepness, FALSE);

		gtk_widget_set_sensitive(check_rva_use_averaging, FALSE);
		gtk_widget_set_sensitive(combo_threshold, FALSE);
		gtk_widget_set_sensitive(label_threshold, FALSE);
		gtk_widget_set_sensitive(spin_linthresh, FALSE);
		gtk_widget_set_sensitive(label_linthresh, FALSE);
		gtk_widget_set_sensitive(spin_stdthresh, FALSE);
		gtk_widget_set_sensitive(label_stdthresh, FALSE);
		gtk_widget_set_sensitive(spin_defvol, FALSE);
		gtk_widget_set_sensitive(label_defvol, FALSE);
	}

	gtk_widget_queue_draw(rva_drawing_area);
}


void
check_rva_use_averaging_toggled(GtkWidget * widget, gpointer * data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_rva_use_averaging))) {
		rva_use_averaging_shadow = 1;
		gtk_widget_set_sensitive(combo_threshold, TRUE);
		gtk_widget_set_sensitive(label_threshold, TRUE);
		gtk_widget_set_sensitive(spin_linthresh, TRUE);
		gtk_widget_set_sensitive(label_linthresh, TRUE);
		gtk_widget_set_sensitive(spin_stdthresh, TRUE);
		gtk_widget_set_sensitive(label_stdthresh, TRUE);
	} else {
		rva_use_averaging_shadow = 0;
		gtk_widget_set_sensitive(combo_threshold, FALSE);
		gtk_widget_set_sensitive(label_threshold, FALSE);
		gtk_widget_set_sensitive(spin_linthresh, FALSE);
		gtk_widget_set_sensitive(label_linthresh, FALSE);
		gtk_widget_set_sensitive(spin_stdthresh, FALSE);
		gtk_widget_set_sensitive(label_stdthresh, FALSE);
	}
}


void
changed_listening_env(GtkWidget * widget, gpointer * data) {

	rva_env_shadow = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_listening_env));

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


gboolean
rva_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {

	int width = gtk_widget_get_allocated_width (widget);
	int height = gtk_widget_get_allocated_height (widget);
	int dw = width / 24;
	int dh = height / 24;
	int xoffs = (width - 24*dw) / 2 - 1;
	int yoffs = (height - 24*dh) / 2 - 1;
	float volx, voly;
	int px1, py1, px2, py2;
	int i;

	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_paint (cr);

	cairo_set_line_width (cr, 1.0);

	if (rva_is_enabled_shadow) {
		cairo_set_source_rgb (cr, 10000 / 65536.0, 10000 / 65536.0, 10000 / 65536.0);
	} else {
		cairo_set_source_rgb (cr, 5000 / 65536.0, 5000 / 65536.0, 5000 / 65536.0);
	}

	cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);

	for (i = 0; i <= 24; i++) {
		cairo_move_to (cr, xoffs + i * dw, yoffs);
		cairo_line_to (cr, xoffs + i * dw, yoffs + 24 * dh);
		cairo_move_to (cr, xoffs, yoffs + i * dh);
		cairo_line_to (cr, xoffs + 24 * dw, yoffs + i * dh);
	}
	cairo_stroke (cr);

	cairo_set_antialias (cr, CAIRO_ANTIALIAS_DEFAULT);

	if (rva_is_enabled_shadow) {
		cairo_set_source_rgb (cr, 0.0, 0.0, 1.0);
	} else {
		cairo_set_source_rgb (cr, 0.0, 0.0, 30000 / 65536.0);
	}

	cairo_move_to (cr, xoffs, yoffs + 24 * dh);
	cairo_line_to (cr, xoffs + 24 * dw, yoffs);
	cairo_stroke (cr);

	if (rva_is_enabled_shadow) {
		cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
	} else {
		cairo_set_source_rgb (cr, 30000 / 65536.0, 0.0, 0.0);
	}

	volx = -24.0f;
	voly = volx + (volx - rva_refvol_shadow) * (rva_steepness_shadow - 1.0f);
	px1 = xoffs;
	py1 = yoffs - (voly * dh);

	volx = 0.0f;
	voly = volx + (volx - rva_refvol_shadow) * (rva_steepness_shadow - 1.0f);
	px2 = xoffs + 24*dw;
	py2 = yoffs - (voly * dh);

	cairo_move_to (cr, px1, py1);
	cairo_line_to (cr, px2, py2);
	cairo_stroke (cr);
}


void
refvol_changed(GtkWidget * widget, gpointer * data) {

	rva_refvol_shadow = gtk_adjustment_get_value(GTK_ADJUSTMENT(widget));
	gtk_widget_queue_draw(rva_drawing_area);
}


void
steepness_changed(GtkWidget * widget, gpointer * data) {

	rva_steepness_shadow = gtk_adjustment_get_value(GTK_ADJUSTMENT(widget));
	gtk_widget_queue_draw(rva_drawing_area);
}


void
changed_threshold(GtkWidget * widget, gpointer * data) {

	rva_use_linear_thresh_shadow =
	        gtk_combo_box_get_active(GTK_COMBO_BOX(combo_threshold));
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
defvol_changed(GtkWidget * widget, gpointer * data) {

	rva_no_rva_voladj_shadow = gtk_adjustment_get_value(GTK_ADJUSTMENT(widget));
}

void
show_restart_info(void) {

	GtkWidget * list;
	GtkWidget * viewport;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

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

        message_dialog(_("Settings"),
		       options_window,
		       GTK_MESSAGE_INFO,
		       GTK_BUTTONS_OK,
		       viewport,
		       _("You will need to restart Aqualung for the following changes to take effect:"));
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

GtkWidget *
create_notebook_tab(char * text, char * imgfile) {

	GtkWidget * vbox;
        GdkPixbuf * pixbuf;
        GtkWidget * image;
	GtkWidget * label;

        char path[MAXLEN];

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	label = gtk_label_new(text);
	gtk_box_pack_end(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	arr_snprintf(path, "%s/%s", AQUALUNG_DATADIR, imgfile);

        pixbuf = gdk_pixbuf_new_from_file(path, NULL);

	if (pixbuf) {
		image = gtk_image_new_from_pixbuf(pixbuf);
		gtk_box_pack_end(GTK_BOX(vbox), image, FALSE, FALSE, 0);
	}

	gtk_widget_show_all(vbox);

	return vbox;
}


void
refresh_ms_pathlist_clicked(GtkWidget * widget, gpointer * data) {

	GtkTreeIter iter;
	char * path;
	int i = 0;

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
}

#ifdef HAVE_LUA
void
browse_ext_title_format_file_clicked(GtkButton * button, gpointer data) {

	file_chooser_with_entry(_("Please select a Lua Extension File."),
				options_window,
				GTK_FILE_CHOOSER_ACTION_OPEN,
				FILE_CHOOSER_FILTER_ETF,
				(GtkWidget *)data,
				ext_title_format_file_shadow,
				CHAR_ARRAY_SIZE(ext_title_format_file_shadow));
}
#endif /* HAVE_LUA */

void
browse_ms_pathlist_clicked(GtkButton * button, gpointer data) {

	file_chooser_with_entry(_("Please select a Music Store database."),
				options_window,
				GTK_FILE_CHOOSER_ACTION_OPEN,
				FILE_CHOOSER_FILTER_STORE,
				(GtkWidget *)data,
				options.storedir, CHAR_ARRAY_SIZE(options.storedir));
}


void
append_ms_pathlist(char * path, char * name) {

	GtkTreeIter iter;

	gtk_list_store_append(ms_pathlist_store, &iter);
	gtk_list_store_set(ms_pathlist_store, &iter, 0, path, 1, name, -1);

	refresh_ms_pathlist_clicked(NULL, NULL);
}


void
add_ms_pathlist_clicked(GtkButton * button, gpointer * data) {

	const char * pname;
	char name[MAXLEN];
	char * path;
	GtkTreeIter iter;
	int i;
	struct stat st_file;

	pname = gtk_entry_get_text(GTK_ENTRY(entry_ms_pathlist));

	if (pname[0] == '\0') {
		browse_ms_pathlist_clicked(button, entry_ms_pathlist);
		return;
	}

	if (pname[0] == '~') {
		arr_snprintf(name, "%s%s", options.home, pname + 1);
	} else if (pname[0] == '/') {
		arr_strlcpy(name, pname);
	} else {
		message_dialog(_("Warning"),
			       options_window,
			       GTK_MESSAGE_WARNING,
			       GTK_BUTTONS_OK,
			       NULL,
			       _("Paths must either be absolute or starting with a tilde."));
		return;
	}

	if ((path = g_filename_from_utf8(name, -1, NULL, NULL, NULL)) == NULL) {
		return;
	}

	if (stat(path, &st_file) != -1 && S_ISDIR(st_file.st_mode)) {
		return;
	}

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ms_pathlist_store), &iter, NULL, i++)) {
		char * p;

		gtk_tree_model_get(GTK_TREE_MODEL(ms_pathlist_store), &iter, 0, &p, -1);

		if (!strcmp(p, path)) {
			message_dialog(_("Warning"),
				       options_window,
				       GTK_MESSAGE_WARNING,
				       GTK_BUTTONS_OK,
				       NULL,
				       _("The specified store has already been added to the list."));
			g_free(p);
			g_free(path);
			return;
		}

		g_free(p);
	}


	gtk_entry_set_text(GTK_ENTRY(entry_ms_pathlist), "");

	append_ms_pathlist(path, name);
	arr_strlcpy(options.storedir, name);

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
check_playlist_auto_save_cb(GtkWidget * widget, gpointer data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_playlist_auto_save))) {
		gtk_widget_set_sensitive(spin_playlist_auto_save, TRUE);
	} else {
		gtk_widget_set_sensitive(spin_playlist_auto_save, FALSE);
	}
}


void
display_help(char * text) {

	message_dialog(_("Help"),
		       options_window,
		       GTK_MESSAGE_INFO,
		       GTK_BUTTONS_OK,
		       NULL,
		       text);
}

void
display_title_format_help(void) {

	display_help(_("\nThe template string you enter here will be used to "
		       "construct a single title line from an Artist, a Record "
		       "and a Track name. These are denoted by %%a, %%r and %%t, "
		       "respectively.\n"));
}

void
display_meta_encode_help(void) {

	display_help(_("\nIf you have non-standard ID3v1/ID3v2 character encoding MPEG audio file, "
		       "please enter suitable character encoding in this entry (see below). "
		       "If this entry is blank, the standard character encoding (ISO-8859-1) is used.\n\n"
		       "For example:\n"
		       "  Japanese: CP932 (SHIFT_JIS variant)\n"
		       "  Korean: EUC-KR\n"
		       "  Simp. Chinese: GB2312\n"
		       "  Trad. Chinese: BIG5\n"
		       "  Greek: ISO-8859-7\n"
		       "  Hebrew: ISO-8859-8\n"
		       "  Cyrillic: CP1251\n"
		       "  Thai: ISO-8859-11\n"
		       "  Arabic: CP1256\n"
		       "  Turkish: ISO-8859-9\n"
		       "  Latin Extended: ISO-8859-2\n"
		       "  Central European: CP1250\n"
		       "  Latin1 (standard): ISO-8859-1\n\n"
		       "NOTE: There might be a more suitable character encoding for your language."));
}

#ifdef HAVE_LUA
void
display_ext_title_format_help(void) {

	display_help(_("\nThe file you enter/choose here will set the Lua program to use "
		       "to extend Aqualung. See the Aqualung manual for details. "
		       "Here is a quick example of what you can use in the file:\n\n"
		       "function playlist_title()\n"
		       "  return m('artist') .. '-' .. m('title') .. ' (' .. m('album') .. ') (' .. i('filename') .. ')'\n"
		       "end"));
}
#endif /* HAVE_LUA */



void
display_implict_command_line_help(void) {

	display_help(_("\nThe string you enter here will be parsed as a command "
		       "line before parsing the actual command line parameters. "
		       "What you enter here will act as a default setting and may "
		       "or may not be overridden from the 'real' command line.\n\n"
		       "Example: enter '-o alsa -R' below to use ALSA output "
		       "running realtime as a default.\n"));
}


void
display_pathlist_help(void) {

	display_help(_("Paths must either be absolute or starting with a tilde, which will be expanded to the user's home directory.\n\n"
		       "Drag and drop entries in the list to set the store order in the Music Store."));
}

#ifdef HAVE_CDDA
void
display_cdda_drive_speed_help(void) {

	display_help(_("\nSet the drive speed for CD playing in CD-ROM speed units. "
		       "One speed unit equals to 176 kBps raw data reading speed. "
		       "Warning: not all drives honor this setting.\n\n"

		       "Lower speed usually means less drive noise. However, "
		       "when using Paranoia error correction modes for increased "
		       "accuracy, generally much larger speeds are required to "
		       "prevent buffer underruns (and thus audible drop-outs).\n\n"

		       "Please note that these settings do not apply to CD Ripping, "
		       "which always happens with maximum available speed and "
		       "with error correction modes manually set before every run."));
}

void
display_cdda_force_drive_rescan_help(void) {

	display_help(_("\nMost drives let Aqualung know when a CD has been inserted "
		       "or removed by providing a 'media changed' flag. However, "
		       "some drives don't set this flag properly, and thus it may "
		       "happen that a newly inserted CD remains unnoticed to "
		       "Aqualung. In such cases, enabling this option should help."));
}

void
cdda_toggled(GtkWidget * widget, gpointer * data) {

	gtk_widget_set_sensitive(cdda_paranoia_maxretries_spinner,
				 !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_cdda_mode_neverskip)));
	gtk_widget_set_sensitive(label_cdda_maxretries,
				 !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_cdda_mode_neverskip)));
}
#endif /* HAVE_CDDA */


void
inet_radio_direct_toggled(GtkWidget * widget, gpointer * data) {

	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(inet_radio_direct));

	gtk_widget_set_sensitive(inet_frame, !state);
}

void
display_inet_help_noproxy_domains(void) {

	display_help(_("\nHere you should enter a comma-separated list of domains\n"
		       "that should be accessed without using the proxy set above.\n"
		       "Example: localhost, .localdomain, .my.domain.com"));
}

void
playlist_embedded_cb(GtkToggleButton * togglebutton, gpointer data) {

	gtk_widget_set_sensitive(GTK_WIDGET(data),
				 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton)));

	restart_active(togglebutton, _("Embed playlist into main window"));
}

#ifdef HAVE_SYSTRAY
void
systray_support_cb(GtkToggleButton * togglebutton, gpointer data) {

	gboolean systray_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton));

	gtk_widget_set_sensitive(GTK_WIDGET(check_systray_start_minimized), systray_active);
	gtk_widget_set_sensitive(GTK_WIDGET(frame_systray_mouse_wheel), systray_active);
	gtk_widget_set_sensitive(GTK_WIDGET(frame_systray_mouse_buttons), systray_active);

	restart_active(togglebutton, _("Enable systray"));
}

GtkWidget *
setup_combo_with_label(GtkBox * box, char * label_text) {

	GtkWidget * hbox;
	GtkWidget * hbox_s;
	GtkWidget * label;
	GtkWidget * combo;

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(box, hbox, FALSE, TRUE, 5);

	label = gtk_label_new(label_text);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	hbox_s = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), hbox_s, TRUE, TRUE, 3);

	combo = gtk_combo_box_text_new ();
	gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, FALSE, 0);

	return combo;
}

void
fill_mouse_wheel_combo(GtkComboBoxText * combo) {

	gtk_combo_box_text_append_text (combo, _("Do nothing"));
	gtk_combo_box_text_append_text (combo, _("Change volume"));
	gtk_combo_box_text_append_text (combo, _("Change balance"));
	gtk_combo_box_text_append_text (combo, _("Change song position"));
	gtk_combo_box_text_append_text (combo, _("Change current song"));

}

void
get_mouse_button_text(int button_number, char * text_buffer, int buffer_size) {

	switch (button_number) {
		case 1:
			g_snprintf(text_buffer, buffer_size, _("Left button"));
			break;
		case 2:
			g_snprintf(text_buffer, buffer_size, _("Middle button"));
			break;
		case 3:
			g_snprintf(text_buffer, buffer_size, _("Right button"));
			break;
		default:
			g_snprintf(text_buffer, buffer_size, "%s %d", _("Button"), button_number);
			break;
	}
}

gboolean
get_mouse_button_button_press_cb(GtkWidget * widget, GdkEventButton * event, gpointer user_data) {

	GtkTreeIter iter;
	gint button_number;
	gboolean set_button = TRUE;
	int i = 0;
	gchar buf[30];

	// Don't allow setting Left or Right mouse buttons (they are reserved).
	if (event->button == 1 || event->button == 3) {
		message_dialog(_("Warning"),
			       get_mouse_button_window,
			       GTK_MESSAGE_WARNING,
			       GTK_BUTTONS_OK,
			       NULL,
			       _("Left and right mouse buttons are reserved."));
		set_button = FALSE;
	} else {
		// Don't allow duplicate buttons.
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(systray_mouse_buttons_store), &iter, NULL, i++)) {

			gtk_tree_model_get(GTK_TREE_MODEL(systray_mouse_buttons_store),
				&iter, SYSTRAY_MB_COL_BUTTON, &button_number, -1);

			if (button_number == event->button) {
				message_dialog(_("Warning"),
					       get_mouse_button_window,
					       GTK_MESSAGE_WARNING,
					       GTK_BUTTONS_OK,
					       NULL,
					       _("This button is already assigned."));

				set_button = FALSE;
				break;
			}
		}
	}

	if (set_button) {
		get_mouse_button_text(event->button, buf, sizeof(buf));
		gtk_label_set_text(GTK_LABEL(user_data), buf);
		get_mb_window_button = event->button;
	}

	return set_button;
}

void
setup_get_mouse_button_window(void) {

	GtkWidget * vbox_main;
	GtkWidget * vbox;
	GtkWidget * frame;
	GtkWidget * eventbox;
	GtkWidget * label;
	GtkWidget * combo;
	GtkWidget * alignment;
	GtkTreeIter iter;
	int i;
	int ret;

	get_mouse_button_window = gtk_dialog_new_with_buttons(
	        _("Add mouse button command"),
		GTK_WINDOW(options_window),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		NULL);

	gtk_window_set_position(GTK_WINDOW(get_mouse_button_window), GTK_WIN_POS_CENTER);
	gtk_dialog_set_default_response(GTK_DIALOG(get_mouse_button_window), GTK_RESPONSE_ACCEPT);
	gtk_container_set_border_width(GTK_CONTAINER(get_mouse_button_window), 5);

	vbox_main = gtk_dialog_get_content_area(GTK_DIALOG(get_mouse_button_window));
	gtk_box_set_spacing(GTK_BOX(vbox_main), 8);

	eventbox = gtk_event_box_new();
	gtk_event_box_set_above_child(GTK_EVENT_BOX(eventbox), TRUE);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(eventbox), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_main), eventbox, FALSE, TRUE, 0);

	frame = gtk_frame_new(_("Mouse button"));
	gtk_container_add(GTK_CONTAINER(eventbox), frame);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, TRUE, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 30, 30, 20, 20);

	label = gtk_label_new(_("Click here to set mouse button"));
	gtk_container_add(GTK_CONTAINER(alignment), label);

	g_signal_connect(G_OBJECT(eventbox), "button-press-event",
	        G_CALLBACK(get_mouse_button_button_press_cb), label);

	frame = gtk_frame_new(_("Command"));
	gtk_box_pack_start(GTK_BOX(vbox_main), frame, FALSE, TRUE, 0);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	combo = gtk_combo_box_text_new ();
	gtk_container_add(GTK_CONTAINER(vbox), combo);

	for (i = 0; i < SYSTRAY_MB_CMD_LAST; i++) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(combo), systray_mb_cmd_names[i]);
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);


	gtk_widget_show_all (get_mouse_button_window);

	get_mb_window_button = 0;

	ret = aqualung_dialog_run(GTK_DIALOG(get_mouse_button_window));

	if (ret == GTK_RESPONSE_ACCEPT) {

		i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

		if (get_mb_window_button > 0 && i >= 0) {
			gtk_list_store_append (systray_mouse_buttons_store, &iter);
			gtk_list_store_set (systray_mouse_buttons_store, &iter,
					    SYSTRAY_MB_COL_BUTTON, get_mb_window_button,
					    SYSTRAY_MB_COL_COMMAND, i,
					    -1);
		}
	}

	gtk_widget_destroy(get_mouse_button_window);
}

void
systray_mouse_button_add_clicked(GtkWidget * widget, gpointer data) {

	setup_get_mouse_button_window();
}

void
systray_mouse_button_remove_clicked(GtkWidget * widget, gpointer data) {

	GtkTreeIter iter;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(systray_mouse_buttons_store), &iter, NULL, i++)) {

		if (gtk_tree_selection_iter_is_selected(systray_mouse_buttons_selection, &iter)) {
			gtk_list_store_remove(systray_mouse_buttons_store, &iter);
			--i;
		}
	}
}

void
systray_mb_col_button_datafunc(GtkTreeViewColumn *tree_column,
			       GtkCellRenderer *cell_renderer,
			       GtkTreeModel *tree_model,
			       GtkTreeIter *iter,
			       gpointer data) {

	gchar buf[30];
	int button_number;

	gtk_tree_model_get(tree_model, iter, SYSTRAY_MB_COL_BUTTON, &button_number, -1);
	get_mouse_button_text(button_number, buf, sizeof(buf));
	g_object_set(cell_renderer, "text", buf, NULL);
}

void
systray_mb_col_command_datafunc(GtkTreeViewColumn *tree_column,
				GtkCellRenderer *cell_renderer,
				GtkTreeModel *tree_model,
				GtkTreeIter *iter,
				gpointer data)
{
	gint command;

	gtk_tree_model_get(tree_model, iter, SYSTRAY_MB_COL_COMMAND, &command, -1);

	if (command >= 0 && command < SYSTRAY_MB_CMD_LAST) {
		g_object_set(cell_renderer, "text", systray_mb_cmd_names[command], NULL);
	}
}

void
systray_mb_col_command_combo_changed_cb(GtkCellRendererCombo * combo,
        				gchar * path_string,
					GtkTreeIter * new_iter,
					gpointer user_data) {

	// Get integer command number from the combobox.
	gtk_tree_model_get(GTK_TREE_MODEL(systray_mouse_buttons_cmds_store), new_iter,
			   1, &systray_mb_col_command_combo_cmd, -1);
}

void
systray_mb_col_command_combo_edited_cb(GtkCellRendererText * cell_renderer,
        			       gchar * path,
	        		       gchar * selection,
        			       gpointer user_data) {
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(systray_mouse_buttons_store), &iter, path)) {

		if (systray_mb_col_command_combo_cmd >= 0 &&
		    systray_mb_col_command_combo_cmd < SYSTRAY_MB_CMD_LAST) {
			gtk_list_store_set (systray_mouse_buttons_store, &iter,
					    SYSTRAY_MB_COL_COMMAND, systray_mb_col_command_combo_cmd, -1);
		}
	}

	systray_mb_col_command_combo_cmd = -1;
}

#endif /* HAVE_SYSTRAY */


gboolean
options_dialog_close(GtkWidget * widget, GdkEvent * event, gpointer data) {

        current_notebook_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));

	gtk_widget_destroy(options_window);
	options_window = NULL;

#ifdef HAVE_SYSTRAY
	gtk_list_store_clear(systray_mouse_buttons_store);
	gtk_list_store_clear(systray_mouse_buttons_cmds_store);
#endif /* HAVE_SYSTRAY */

	return TRUE;
}

gboolean
options_dialog_response(GtkDialog * dialog, gint response_id, gpointer data) {

	int ret;
	char buf[MAXLEN];
	char * format = (char *)gtk_entry_get_text(GTK_ENTRY(entry_title));
	char * encode = (char *)gtk_entry_get_text(GTK_ENTRY(encode_set_entry));

	if (response_id != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(options_window);
		options_window = NULL;
		return TRUE;
	}

        current_notebook_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));

	if ((ret = make_string_va(buf, CHAR_ARRAY_SIZE(buf), format,
				  'a', "a", 'r', "r", 't', "t", 0)) != 0) {
		make_string_strerror(ret, buf, CHAR_ARRAY_SIZE(buf));
		message_dialog(_("Error in title format string"),
			       options_window,
			       GTK_MESSAGE_ERROR,
			       GTK_BUTTONS_OK,
			       NULL,
			       buf);
		return FALSE;
	}
	
	if (strlen(encode)) {
		GError * error = NULL;
		gchar * str = g_convert("", -1, "utf-8", encode, NULL, NULL, &error);
		if (str != NULL) {
			g_free(str);
		} else {
			message_dialog(_("Error invalid character encoding"),
				       options_window,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK,
				       NULL,
				       "%s", error->message);
			g_clear_error(&error);
			gtk_entry_set_text(GTK_ENTRY(encode_set_entry), "");
			return FALSE;
		}
	}

	options_window_accept();
	gtk_widget_destroy(options_window);
	options_window = NULL;

#ifdef HAVE_SYSTRAY
	gtk_list_store_clear(systray_mouse_buttons_store);
	gtk_list_store_clear(systray_mouse_buttons_cmds_store);
#endif /* HAVE_SYSTRAY */

	return TRUE;
}

void
create_options_window(void) {

	GtkWidget * vbox_general;
	GtkWidget * nbook_general;
	GtkWidget * frame_title;
	GtkWidget * frame_param;
	GtkWidget * frame_misc;
	GtkWidget * frame_cart;
	GtkWidget * hbox_param;
	GtkWidget * vbox_misc;
	GtkWidget * vbox_cart;
	GtkWidget * vbox_appearance;
	GtkWidget * vbox_miscellaneous_tab;
#ifdef HAVE_SYSTRAY
	GtkWidget * vbox_systray;
	GtkWidget * systray_mb_list;
	GtkWidget * button;
#endif /* HAVE_SYSTRAY */

#ifdef HAVE_LUA
	GtkWidget * browse_ext_title_format_file;
       	GtkWidget * help_btn_ext_title;
#endif /* HAVE_LUA */

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
        GtkWidget * table_fonts;
	GtkWidget * vbox_colors;

	GtkWidget * vbox_rva;
	GtkWidget * table_rva;
        GtkWidget * label_cwidth;
	GtkWidget * hbox_cwidth;

	GtkWidget * vbox_meta;
	GtkWidget * hbox_meta;
	GtkWidget * frame_meta;
	GtkWidget * vbox_meta2;
	GtkWidget * help_btn_meta_encode;

#ifdef HAVE_CDDA
	GtkWidget * table_cdda;
	GtkWidget * help_btn_cdda_drive_speed;
	GtkWidget * help_btn_cdda_force_drive_rescan;
	GtkWidget * frame_cdda;
	GtkWidget * vbox_cdda;
	GtkWidget * hbox_cdda;
#endif /* HAVE_CDDA */

#ifdef HAVE_CDDB
	GtkWidget * table_cddb;
#endif /* HAVE_CDDB */

	GtkWidget * vbox_inet;
	GtkWidget * table_inet;
	GtkWidget * inet_hbox_timeout;
	GtkWidget * inet_label_timeout;

        GtkSizeGroup * label_size;

	GtkWidget * hbox;
	GtkWidget * vbox;
	GtkWidget * hbox_s;
       	GtkWidget * help_btn_title;
	GtkWidget * help_btn_param;
	GtkWidget * help_pathlist;

	GtkWidget * alignment;

#ifdef HAVE_LADSPA
	int status;
#endif /* HAVE_LADSPA */
	int i;

	if (options_window != NULL) {
		return;
	}

        restart_flag = 0;
	if (options.ext_title_format_file[0] == '\0') {
		arr_strlcpy(ext_title_format_file_shadow, options.confdir);
	} else {
		arr_strlcpy(ext_title_format_file_shadow, options.ext_title_format_file);
	}

	if (!restart_list_store) {
		restart_list_store = gtk_list_store_new(1, G_TYPE_STRING);
	} else {
		gtk_list_store_clear(restart_list_store);
	}

	refresh_ms_pathlist_clicked(NULL, NULL);

        options_window = gtk_dialog_new_with_buttons(_("Settings"),
                                            GTK_WINDOW(main_window),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                            NULL);

	gtk_window_set_position(GTK_WINDOW(options_window), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(options_window), GTK_RESPONSE_ACCEPT);
        gtk_container_set_border_width(GTK_CONTAINER(options_window), 5);

	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(options_window))),
			  notebook);

	g_signal_connect(G_OBJECT(options_window), "response",
			 G_CALLBACK(options_dialog_response), NULL);
	g_signal_connect(G_OBJECT(options_window), "delete_event",
			 G_CALLBACK(options_dialog_close), NULL);

        label_size = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);

	/* "General" notebook page */

	vbox_general = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_general), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_general, create_notebook_tab(_("General"), "general.png"));

	nbook_general = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox_general), nbook_general, FALSE, TRUE, 0);

	/* General/Main window page */

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(nbook_general), vbox, gtk_label_new(_("Main window")));

        check_dark_theme =
		gtk_check_button_new_with_label(_("Dark theme"));
	gtk_widget_set_name(check_dark_theme, "check_on_notebook");
	if (options.dark_theme) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_dark_theme), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox), check_dark_theme, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (check_dark_theme), "toggled",
			  G_CALLBACK (restart_active), _("Dark theme"));

        check_disable_buttons_relief =
		gtk_check_button_new_with_label(_("Disable control buttons relief"));
	gtk_widget_set_name(check_disable_buttons_relief, "check_on_notebook");
	if (options.disable_buttons_relief) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_disable_buttons_relief), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox), check_disable_buttons_relief, FALSE, FALSE, 0);

	check_combine_play_pause = gtk_check_button_new_with_label(_("Combine play and pause buttons"));
	gtk_widget_set_name(check_combine_play_pause, "check_on_notebook");
	if (options.combine_play_pause_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_combine_play_pause), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox), check_combine_play_pause, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (check_combine_play_pause), "toggled",
			  G_CALLBACK (restart_active), _("Combine play and pause buttons"));

        check_main_window_always_on_top = gtk_check_button_new_with_label(_("Keep main window always on top"));
	gtk_widget_set_name(check_main_window_always_on_top, "check_on_notebook");
	if (options.main_window_always_on_top) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_main_window_always_on_top), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox), check_main_window_always_on_top, FALSE, FALSE, 0);

	check_show_sn_title =
		gtk_check_button_new_with_label(_("Show song name in the main window's title"));
        gtk_widget_set_name(check_show_sn_title, "check_on_notebook");
	if (options.show_sn_title) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_sn_title), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox), check_show_sn_title, FALSE, FALSE, 0);

	frame_title = gtk_frame_new(_("Title Format"));
	gtk_box_pack_start(GTK_BOX(vbox), frame_title, FALSE, TRUE, 5);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
	gtk_container_add(GTK_CONTAINER(frame_title), hbox);

	entry_title = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(entry_title), MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(entry_title), options.title_format);
	gtk_box_pack_start(GTK_BOX(hbox), entry_title, TRUE, TRUE, 0);

        help_btn_title = gtk_button_new_from_stock (GTK_STOCK_HELP);
	g_signal_connect(help_btn_title, "clicked", G_CALLBACK(display_title_format_help), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), help_btn_title, FALSE, FALSE, 5);

#ifdef HAVE_LUA
	frame_title = gtk_frame_new(_("Lua extension file"));
	gtk_box_pack_start(GTK_BOX(vbox), frame_title, FALSE, TRUE, 5);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
	gtk_container_add(GTK_CONTAINER(frame_title), hbox);

	entry_ext_title_format_file = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(entry_ext_title_format_file), MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(entry_ext_title_format_file), options.ext_title_format_file);
	gtk_box_pack_start(GTK_BOX(hbox), entry_ext_title_format_file, TRUE, TRUE, 2);

	browse_ext_title_format_file = gui_stock_label_button(_("Browse"), GTK_STOCK_OPEN);
        gtk_container_set_border_width(GTK_CONTAINER(browse_ext_title_format_file), 2);
	g_signal_connect (G_OBJECT(browse_ext_title_format_file), "clicked",
			  G_CALLBACK(browse_ext_title_format_file_clicked), (gpointer)entry_ext_title_format_file);
	gtk_box_pack_start(GTK_BOX(hbox), browse_ext_title_format_file, FALSE, FALSE, 5);

        help_btn_ext_title = gtk_button_new_from_stock(GTK_STOCK_HELP);
	g_signal_connect(help_btn_ext_title, "clicked", G_CALLBACK(display_ext_title_format_help), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), help_btn_ext_title, FALSE, FALSE, 5);
#endif /* HAVE_LUA */

	/* General/Systray page */

#ifdef HAVE_SYSTRAY
	vbox_systray = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_systray), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(nbook_general), vbox_systray, gtk_label_new(_("Systray")));

	check_use_systray = gtk_check_button_new_with_label(_("Enable systray"));
	gtk_widget_set_name(check_use_systray, "check_on_notebook");
	if (options.use_systray) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_use_systray), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_systray), check_use_systray, FALSE, FALSE, 0);

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 16, 0);
	gtk_box_pack_start (GTK_BOX (vbox_systray), alignment, FALSE, TRUE, 0);

	check_systray_start_minimized = gtk_check_button_new_with_label(_("Start minimized"));
	gtk_widget_set_name(check_systray_start_minimized, "check_on_notebook");
	if (options.systray_start_minimized) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_systray_start_minimized), TRUE);
	}
	gtk_container_add (GTK_CONTAINER (alignment), check_systray_start_minimized);

	g_signal_connect (G_OBJECT (check_use_systray), "toggled",
			  G_CALLBACK (systray_support_cb), NULL);

	systray_mb_cmd_names[SYSTRAY_MB_CMD_PLAY_STOP_SONG]  = _("Play/Stop song");
	systray_mb_cmd_names[SYSTRAY_MB_CMD_PLAY_PAUSE_SONG] = _("Play/Pause song");
	systray_mb_cmd_names[SYSTRAY_MB_CMD_PREV_SONG]       = _("Previous song");
	systray_mb_cmd_names[SYSTRAY_MB_CMD_NEXT_SONG]       = _("Next song");

	frame_systray_mouse_wheel = gtk_frame_new(_("Mouse wheel"));
	gtk_box_pack_start(GTK_BOX(vbox_systray), frame_systray_mouse_wheel, FALSE, TRUE, 5);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
	gtk_container_add(GTK_CONTAINER(frame_systray_mouse_wheel), vbox);

	combo_systray_mouse_wheel_vertical =
		setup_combo_with_label(GTK_BOX(vbox), _("Vertical mouse wheel:"));
	fill_mouse_wheel_combo(GTK_COMBO_BOX_TEXT (combo_systray_mouse_wheel_vertical));
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_systray_mouse_wheel_vertical),
	        options.systray_mouse_wheel_vertical);

	combo_systray_mouse_wheel_horizontal =
		setup_combo_with_label(GTK_BOX(vbox), _("Horizontal mouse wheel:"));
	fill_mouse_wheel_combo(GTK_COMBO_BOX_TEXT (combo_systray_mouse_wheel_horizontal));
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_systray_mouse_wheel_horizontal),
	        options.systray_mouse_wheel_horizontal);

	frame_systray_mouse_buttons = gtk_frame_new(_("Mouse buttons"));
	gtk_box_pack_start(GTK_BOX(vbox_systray), frame_systray_mouse_buttons, FALSE, TRUE, 5);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
	gtk_container_add(GTK_CONTAINER(frame_systray_mouse_buttons), vbox);

	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(scrolled_win, 100, 100);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled_win, FALSE, TRUE, 5);

	/* Mouse buttons table */
	systray_mouse_buttons_store = gtk_list_store_new (SYSTRAY_MB_LAST,
							  G_TYPE_INT,   /* Button nr */
	        					  G_TYPE_INT);  /* Command index */

	for (i = 0; i < options.systray_mouse_buttons_count; i++) {
		gtk_list_store_append (systray_mouse_buttons_store, &iter);
		gtk_list_store_set (systray_mouse_buttons_store, &iter,
				    SYSTRAY_MB_COL_BUTTON, options.systray_mouse_buttons[i].button_nr,
				    SYSTRAY_MB_COL_COMMAND, options.systray_mouse_buttons[i].command,
				    -1);
	}

	systray_mb_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(systray_mouse_buttons_store));
	systray_mouse_buttons_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(systray_mb_list));
	gtk_tree_selection_set_mode(systray_mouse_buttons_selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(systray_mb_list), FALSE);
	gtk_widget_set_can_focus(GTK_WIDGET(systray_mb_list), FALSE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_win), systray_mb_list);

	// First mouse buttons column.
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Button"), renderer, NULL);
	gtk_tree_view_column_set_cell_data_func(
	        column, renderer, systray_mb_col_button_datafunc, NULL, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(systray_mb_list), column);

	// Second mouse buttons column.
	renderer = gtk_cell_renderer_combo_new();
	systray_mouse_buttons_cmds_store = gtk_list_store_new (
	        2,
	        G_TYPE_STRING,  /* Command description */
	        G_TYPE_INT);	/* Command index */

	for (i = 0; i < SYSTRAY_MB_CMD_LAST; i++) {
		gtk_list_store_append (systray_mouse_buttons_cmds_store, &iter);
		gtk_list_store_set (systray_mouse_buttons_cmds_store, &iter,
				    0, systray_mb_cmd_names[i], 1, i, -1);
	}

	g_object_set(G_OBJECT (renderer),
	             "model", systray_mouse_buttons_cmds_store,
	             "text-column", 0,
	             "editable", TRUE,
	             "has-entry", FALSE,
	             NULL);

	g_signal_connect(G_OBJECT(renderer), "changed",
	        	 G_CALLBACK(systray_mb_col_command_combo_changed_cb), NULL);
	g_signal_connect(G_OBJECT(renderer), "edited",
	        	 G_CALLBACK(systray_mb_col_command_combo_edited_cb), NULL);

	column = gtk_tree_view_column_new_with_attributes(_("Command"), renderer, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(systray_mb_list), column);
	gtk_tree_view_column_set_cell_data_func(
	        column, renderer, systray_mb_col_command_datafunc, NULL, NULL);


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = gui_stock_label_button(_("Add"), GTK_STOCK_ADD);
	g_signal_connect (G_OBJECT(button), "clicked",
			  G_CALLBACK(systray_mouse_button_add_clicked), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = gui_stock_label_button(_("Remove"), GTK_STOCK_REMOVE);
	g_signal_connect (G_OBJECT(button), "clicked",
			  G_CALLBACK(systray_mouse_button_remove_clicked), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	gtk_widget_set_sensitive(check_systray_start_minimized, options.use_systray);
	gtk_widget_set_sensitive(frame_systray_mouse_wheel, options.use_systray);
	gtk_widget_set_sensitive(frame_systray_mouse_buttons, options.use_systray);

#endif /* HAVE_SYSTRAY */

	/* General/Miscellaneous page */

	vbox_miscellaneous_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_miscellaneous_tab), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(nbook_general), vbox_miscellaneous_tab, gtk_label_new(_("Miscellaneous")));

	frame_param = gtk_frame_new(_("Implicit command line"));
	gtk_box_pack_start(GTK_BOX(vbox_miscellaneous_tab), frame_param, FALSE, TRUE, 5);

        hbox_param = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(hbox_param), 5);
	gtk_container_add(GTK_CONTAINER(frame_param), hbox_param);

	entry_param = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(entry_param), MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(entry_param), options.default_param);
	gtk_box_pack_start(GTK_BOX(hbox_param), entry_param, TRUE, TRUE, 0);

        help_btn_param = gtk_button_new_from_stock (GTK_STOCK_HELP);
	g_signal_connect(help_btn_param, "clicked", G_CALLBACK(display_implict_command_line_help), NULL);
	gtk_box_pack_start(GTK_BOX(hbox_param), help_btn_param, FALSE, FALSE, 5);


	frame_cart = gtk_frame_new(_("Cover art"));
	gtk_box_pack_start(GTK_BOX(vbox_miscellaneous_tab), frame_cart, FALSE, TRUE, 5);

	vbox_cart = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_cart), 10);
	gtk_container_add(GTK_CONTAINER(frame_cart), vbox_cart);

        hbox_cwidth = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_cart), hbox_cwidth, FALSE, FALSE, 0);
        label_cwidth = gtk_label_new(_("Default cover width:"));
        gtk_box_pack_start(GTK_BOX(hbox_cwidth), label_cwidth, FALSE, FALSE, 0);

        hbox_s = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(hbox_cwidth), hbox_s, TRUE, TRUE, 3);

	combo_cwidth = gtk_combo_box_text_new ();
        gtk_box_pack_start(GTK_BOX(hbox_cwidth), combo_cwidth, FALSE, FALSE, 0);
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_cwidth), _("50 pixels"));
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_cwidth), _("100 pixels"));
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_cwidth), _("200 pixels"));
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_cwidth), _("300 pixels"));
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_cwidth), _("use browser window width"));

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_cwidth), options.cover_width);

        check_magnify_smaller_images =
		gtk_check_button_new_with_label(_("Do not magnify images with smaller width"));
	gtk_widget_set_name(check_magnify_smaller_images, "check_on_notebook");
	if (!options.magnify_smaller_images) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_magnify_smaller_images), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_cart), check_magnify_smaller_images, FALSE, FALSE, 0);

        check_show_cover_for_ms_tracks_only = gtk_check_button_new_with_label(_("Show cover thumbnail for Music Store tracks only"));
	gtk_widget_set_name(check_show_cover_for_ms_tracks_only, "check_on_notebook");
	if (options.show_cover_for_ms_tracks_only) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_cover_for_ms_tracks_only), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_cart), check_show_cover_for_ms_tracks_only, FALSE, FALSE, 0);

        check_dont_show_cover = gtk_check_button_new_with_label(_("Don't show cover thumbnail in the main window"));
	gtk_widget_set_name(check_dont_show_cover, "check_on_notebook");
	if (options.dont_show_cover) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_dont_show_cover), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_cart), check_dont_show_cover, FALSE, FALSE, 0);

        check_use_external_cover_first = gtk_check_button_new_with_label(_("Use cover from external graphics file first"));
	gtk_widget_set_name(check_use_external_cover_first, "check_on_notebook");
	if (options.use_external_cover_first) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_use_external_cover_first), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_cart), check_use_external_cover_first, FALSE, FALSE, 0);

	frame_misc = gtk_frame_new(_("Miscellaneous"));
	gtk_box_pack_start(GTK_BOX(vbox_miscellaneous_tab), frame_misc, FALSE, TRUE, 0);

	vbox_misc = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_misc), 8);
	gtk_container_add(GTK_CONTAINER(frame_misc), vbox_misc);

	check_enable_tooltips =	gtk_check_button_new_with_label(_("Enable tooltips"));
        gtk_widget_set_name(check_enable_tooltips, "check_on_notebook");
	if (options.enable_tooltips) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_tooltips), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_enable_tooltips, FALSE, FALSE, 0);

#ifdef HAVE_LADSPA
	check_simple_view_in_fx =
		gtk_check_button_new_with_label(_("Simple view in LADSPA patch builder"));
	gtk_widget_set_name(check_simple_view_in_fx, "check_on_notebook");
	if (options.simple_view_in_fx_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_simple_view_in_fx), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_simple_view_in_fx, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (check_simple_view_in_fx), "toggled",
						G_CALLBACK (restart_active), _("Simple view in LADSPA patch builder"));
#endif /* HAVE_LADSPA */

	check_united_minimization =
		gtk_check_button_new_with_label(_("United windows minimization"));
        gtk_widget_set_name(check_united_minimization, "check_on_notebook");
	if (options.united_minimization) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_united_minimization), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_misc), check_united_minimization, FALSE, FALSE, 0);


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

	vbox_pl = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_pl), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_pl, create_notebook_tab(_("Playlist"), "playlist.png"));

        check_playlist_is_embedded =
		gtk_check_button_new_with_label(_("Embed playlist into main window"));
	gtk_widget_set_name(check_playlist_is_embedded, "check_on_notebook");
	if (options.playlist_is_embedded_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_playlist_is_embedded), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_pl), check_playlist_is_embedded, FALSE, TRUE, 0);

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_box_pack_start (GTK_BOX (vbox_pl), alignment, FALSE, TRUE, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 16, 0);

	check_buttons_at_the_bottom =
		gtk_check_button_new_with_label(_("Put control buttons at the bottom of playlist"));
	gtk_widget_set_name(check_buttons_at_the_bottom, "check_on_notebook");
	if (options.buttons_at_the_bottom_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_buttons_at_the_bottom), TRUE);
	}
	if (options.playlist_is_embedded_shadow) {
		gtk_widget_set_sensitive(check_buttons_at_the_bottom, TRUE);
	} else {
		gtk_widget_set_sensitive(check_buttons_at_the_bottom, FALSE);
	}
	gtk_container_add (GTK_CONTAINER (alignment), check_buttons_at_the_bottom);
	g_signal_connect (G_OBJECT (check_buttons_at_the_bottom), "toggled",
			  G_CALLBACK (restart_active), _("Put control buttons at the bottom of playlist"));

	g_signal_connect (G_OBJECT (check_playlist_is_embedded), "toggled",
		G_CALLBACK (playlist_embedded_cb), check_buttons_at_the_bottom);


	check_autoplsave =
	    gtk_check_button_new_with_label(_("Save and restore the playlist on exit/startup"));
	gtk_widget_set_name(check_autoplsave, "check_on_notebook");
	if (options.auto_save_playlist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_autoplsave), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_autoplsave, FALSE, TRUE, 0);


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_pl), hbox, FALSE, TRUE, 0);

	check_playlist_auto_save =
		gtk_check_button_new_with_label(_("Save playlist periodically [min]:"));
	gtk_widget_set_name(check_playlist_auto_save, "check_on_notebook");
	g_signal_connect(G_OBJECT(check_playlist_auto_save), "toggled",
			 G_CALLBACK(check_playlist_auto_save_cb), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), check_playlist_auto_save, FALSE, TRUE, 0);

	spin_playlist_auto_save = gtk_spin_button_new_with_range(1, 60, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_playlist_auto_save), options.playlist_auto_save_int);
        gtk_box_pack_start(GTK_BOX(hbox), spin_playlist_auto_save, FALSE, TRUE, 10);

	if (options.playlist_auto_save) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_playlist_auto_save), TRUE);
	} else {
		gtk_widget_set_sensitive(spin_playlist_auto_save, FALSE);
	}


        check_playlist_is_tree =
		gtk_check_button_new_with_label(_("Album mode is the default when adding entire records"));
	gtk_widget_set_name(check_playlist_is_tree, "check_on_notebook");
	if (options.playlist_is_tree) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_playlist_is_tree), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_pl), check_playlist_is_tree, FALSE, TRUE, 0);

        check_playlist_always_show_tabs =
		gtk_check_button_new_with_label(_("Always show the tab bar"));
	gtk_widget_set_name(check_playlist_always_show_tabs, "check_on_notebook");
	if (options.playlist_always_show_tabs) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_playlist_always_show_tabs), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_pl), check_playlist_always_show_tabs, FALSE, TRUE, 0);

        check_show_close_button_in_tab =
		gtk_check_button_new_with_label(_("Show close button in tab"));
	gtk_widget_set_name(check_show_close_button_in_tab, "check_on_notebook");
	if (options.playlist_show_close_button_in_tab) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_close_button_in_tab), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_pl), check_show_close_button_in_tab, FALSE, TRUE, 0);

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
	g_signal_connect(G_OBJECT(check_enable_playlist_statusbar), "toggled",
			 G_CALLBACK(restart_active), _("Enable statusbar in playlist"));
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_enable_playlist_statusbar, FALSE, TRUE, 0);

	check_pl_statusbar_show_size =
		gtk_check_button_new_with_label(_("Show soundfile size in statusbar"));
	gtk_widget_set_name(check_pl_statusbar_show_size, "check_on_notebook");
	if (options.pl_statusbar_show_size) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_pl_statusbar_show_size), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_pl_statusbar_show_size, FALSE, TRUE, 0);

        check_show_rva_in_playlist =
		gtk_check_button_new_with_label(_("Show RVA values"));
	gtk_widget_set_name(check_show_rva_in_playlist, "check_on_notebook");
	if (options.show_rva_in_playlist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_rva_in_playlist), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_show_rva_in_playlist, FALSE, TRUE, 0);

	check_show_length_in_playlist =
		gtk_check_button_new_with_label(_("Show track lengths"));
	gtk_widget_set_name(check_show_length_in_playlist, "check_on_notebook");
	if (options.show_length_in_playlist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_length_in_playlist), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_show_length_in_playlist, FALSE, TRUE, 0);

	check_show_active_track_name_in_bold =
		gtk_check_button_new_with_label(_("Show active track name in bold"));
	gtk_widget_set_name(check_show_active_track_name_in_bold, "check_on_notebook");
	track_name_in_bold_shadow = options.show_active_track_name_in_bold;
	if (options.show_active_track_name_in_bold) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_show_active_track_name_in_bold), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(vbox_pl), check_show_active_track_name_in_bold, FALSE, TRUE, 0);

	check_auto_roll_to_active_track =
		gtk_check_button_new_with_label(_("Automatically roll to active track"));
	gtk_widget_set_name(check_auto_roll_to_active_track, "check_on_notebook");
	if (options.auto_roll_to_active_track) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_roll_to_active_track), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_pl), check_auto_roll_to_active_track, FALSE, TRUE, 0);

	check_enable_pl_rules_hint =
		gtk_check_button_new_with_label(_("Enable rules hint"));
        gtk_widget_set_name(check_enable_pl_rules_hint, "check_on_notebook");
	if (options.enable_pl_rules_hint) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_pl_rules_hint), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_pl), check_enable_pl_rules_hint, FALSE, TRUE, 0);

        frame_plistcol = gtk_frame_new(_("Playlist column order"));
        gtk_box_pack_start(GTK_BOX(vbox_pl), frame_plistcol, FALSE, TRUE, 5);

        vbox_plistcol = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_plistcol), 8);
        gtk_container_add(GTK_CONTAINER(frame_plistcol), vbox_plistcol);

	label_plistcol = gtk_label_new(_("Drag and drop entries in the list below \n"
					 "to set the column order in the Playlist."));
        gtk_box_pack_start(GTK_BOX(vbox_plistcol), label_plistcol, FALSE, TRUE, 5);


	plistcol_store = gtk_list_store_new(2,
					    G_TYPE_STRING,   /* Column name */
					    G_TYPE_INT);     /* Column index */

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
					   0, _("Track titles"), 1, 0, -1);
			break;
		case 1:
			gtk_list_store_append(plistcol_store, &iter);
			gtk_list_store_set(plistcol_store, &iter,
					   0, _("RVA values"), 1, 1, -1);
			break;
		case 2:
			gtk_list_store_append(plistcol_store, &iter);
			gtk_list_store_set(plistcol_store, &iter,
					   0, _("Track lengths"), 1, 2, -1);
			break;
		}
	}


        /* "Music Store" notebook page */

	vbox_ms = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
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
	g_signal_connect(G_OBJECT(check_enable_mstore_toolbar), "toggled",
			 G_CALLBACK(restart_active), _("Enable toolbar in Music Store"));
        gtk_box_pack_start(GTK_BOX(vbox_ms), check_enable_mstore_toolbar, FALSE, TRUE, 0);

	check_enable_mstore_statusbar =
		gtk_check_button_new_with_label(_("Enable statusbar"));
	gtk_widget_set_name(check_enable_mstore_statusbar, "check_on_notebook");
	if (options.enable_mstore_statusbar_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_mstore_statusbar), TRUE);
	}
	g_signal_connect(G_OBJECT(check_enable_mstore_statusbar), "toggled",
			 G_CALLBACK(restart_active), _("Enable statusbar in Music Store"));
        gtk_box_pack_start(GTK_BOX(vbox_ms), check_enable_mstore_statusbar, FALSE, TRUE, 0);

	check_ms_statusbar_show_size =
		gtk_check_button_new_with_label(_("Show soundfile size in statusbar"));
	gtk_widget_set_name(check_ms_statusbar_show_size, "check_on_notebook");
	if (options.ms_statusbar_show_size) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_ms_statusbar_show_size), TRUE);
	}
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
	gtk_box_pack_start(GTK_BOX(vbox_ms), check_enable_ms_rules_hint, FALSE, TRUE, 0);

	check_enable_ms_tree_icons =
		gtk_check_button_new_with_label(_("Enable tree node icons"));
        gtk_widget_set_name(check_enable_ms_tree_icons, "check_on_notebook");
	if (options.enable_ms_tree_icons_shadow) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_enable_ms_tree_icons), TRUE);
	}
	g_signal_connect (G_OBJECT (check_enable_ms_tree_icons), "toggled",
			  G_CALLBACK (restart_active), _("Enable Music Store tree node icons"));
	gtk_box_pack_start(GTK_BOX(vbox_ms), check_enable_ms_tree_icons, FALSE, TRUE, 0);

	check_ms_confirm_removal = gtk_check_button_new_with_label(_("Ask for confirmation when removing items"));
	gtk_widget_set_name(check_ms_confirm_removal, "check_on_notebook");
	if (options.ms_confirm_removal) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_ms_confirm_removal), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_ms), check_ms_confirm_removal, FALSE, FALSE, 0);

	frame_ms_pathlist = gtk_frame_new(_("Paths to Music Store databases"));
	gtk_box_pack_start(GTK_BOX(vbox_ms), frame_ms_pathlist, FALSE, TRUE, 5);

	vbox_ms_pathlist = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_ms_pathlist), 8);
	gtk_container_add(GTK_CONTAINER(frame_ms_pathlist), vbox_ms_pathlist);

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

	hbox_ms_pathlist = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox_ms_pathlist), hbox_ms_pathlist, FALSE, FALSE, 0);

	entry_ms_pathlist = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox_ms_pathlist), entry_ms_pathlist, TRUE, TRUE, 2);

	browse_ms_pathlist = gui_stock_label_button(_("Browse"), GTK_STOCK_OPEN);
        gtk_container_set_border_width(GTK_CONTAINER(browse_ms_pathlist), 2);
	g_signal_connect (G_OBJECT(browse_ms_pathlist), "clicked",
			  G_CALLBACK(browse_ms_pathlist_clicked), (gpointer)entry_ms_pathlist);
	gtk_box_pack_end(GTK_BOX(hbox_ms_pathlist), browse_ms_pathlist, FALSE, FALSE, 0);

	hbox_ms_pathlist_2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
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

	vbox_dsp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_dsp), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_dsp, create_notebook_tab(_("DSP"), "dsp.png"));

	frame_ladspa = gtk_frame_new(_("LADSPA plugin processing"));
	gtk_box_pack_start(GTK_BOX(vbox_dsp), frame_ladspa, FALSE, TRUE, 0);

	vbox_ladspa = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_ladspa), 10);
	gtk_container_add(GTK_CONTAINER(frame_ladspa), vbox_ladspa);

#ifdef HAVE_LADSPA
        combo_ladspa = gtk_combo_box_text_new ();
        gtk_box_pack_start(GTK_BOX(vbox_ladspa), combo_ladspa, TRUE, TRUE, 0);

	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_ladspa), _("Pre Fader (before Volume & Balance)"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_ladspa), _("Post Fader (after Volume & Balance)"));

	status = options.ladspa_is_postfader;
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_ladspa), status);
        g_signal_connect(combo_ladspa, "changed", G_CALLBACK(changed_ladspa_prepost), NULL);
#else
	{
		GtkWidget * label = gtk_label_new(_("Aqualung is compiled without LADSPA plugin support.\n"
						    "See the About box and the documentation for details."));
		gtk_box_pack_start(GTK_BOX(vbox_ladspa), label, FALSE, TRUE, 5);
	}
#endif /* HAVE_LADSPA */


	frame_src = gtk_frame_new(_("Sample Rate Converter type"));
	gtk_box_pack_start(GTK_BOX(vbox_dsp), frame_src, FALSE, TRUE, 5);

	vbox_src = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_src), 10);
	gtk_container_add(GTK_CONTAINER(frame_src), vbox_src);

	label_src = gtk_label_new("");

#ifdef HAVE_SRC
	combo_src = gtk_combo_box_text_new ();
	gtk_box_pack_start(GTK_BOX(vbox_src), combo_src, TRUE, TRUE, 0);

	{
		int i = 0;

		while (src_get_name(i)) {
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_src), src_get_name(i));
			++i;
		}

	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_src), options.src_type);
	g_signal_connect(combo_src, "changed", G_CALLBACK(changed_src_type), NULL);

	gtk_label_set_text(GTK_LABEL(label_src), src_get_description(options.src_type));
#else
	gtk_label_set_text(GTK_LABEL(label_src),
			   _("Aqualung is compiled without Sample Rate Converter support.\n"
			     "See the About box and the documentation for details."));

#endif /* HAVE_SRC */

	gtk_box_pack_start(GTK_BOX(vbox_src), label_src, TRUE, TRUE, 0);


	/* "Playback RVA" notebook page */

	vbox_rva = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_rva), 10);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_rva, create_notebook_tab(_("Playback RVA"), "rva.png"));

	check_rva_is_enabled = gtk_check_button_new_with_label(_("Enable playback RVA"));
	gtk_widget_set_name(check_rva_is_enabled, "check_on_notebook");
	if (options.rva_is_enabled) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_rva_is_enabled), TRUE);
	}
	rva_is_enabled_shadow = options.rva_is_enabled;
	g_signal_connect(G_OBJECT(check_rva_is_enabled), "toggled",
			 G_CALLBACK(check_rva_is_enabled_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_rva), check_rva_is_enabled, FALSE, TRUE, 0);


	table_rva = gtk_table_new(8, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox_rva), table_rva, TRUE, TRUE, 5);

	rva_viewport = gtk_viewport_new(NULL, NULL);
	gtk_widget_set_size_request(rva_viewport, 244, 244);
        gtk_table_attach(GTK_TABLE(table_rva), rva_viewport, 0, 2, 0, 1,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 2);

	rva_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(GTK_WIDGET(rva_drawing_area), 240, 240);
	gtk_container_add(GTK_CONTAINER(rva_viewport), rva_drawing_area);

	g_signal_connect(G_OBJECT(rva_drawing_area), "draw",
			 G_CALLBACK(rva_draw), NULL);

        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        label_listening_env = gtk_label_new(_("Listening environment:"));
        gtk_box_pack_start(GTK_BOX(hbox), label_listening_env, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox, 0, 1, 1, 2,
                         GTK_FILL, GTK_FILL, 5, 2);

	combo_listening_env = gtk_combo_box_text_new ();
        gtk_table_attach(GTK_TABLE(table_rva), combo_listening_env, 1, 2, 1, 2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 2);


	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_listening_env), _("Audiophile"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_listening_env), _("Living room"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_listening_env), _("Office"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_listening_env), _("Noisy workshop"));

	rva_env_shadow = options.rva_env;
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_listening_env), options.rva_env);
	g_signal_connect(combo_listening_env, "changed", G_CALLBACK(changed_listening_env), NULL);

        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        label_refvol = gtk_label_new(_("Reference volume [dBFS] :"));
        gtk_box_pack_start(GTK_BOX(hbox), label_refvol, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox, 0, 1, 2, 3,
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

        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        label_steepness = gtk_label_new(_("Steepness [dB/dB] :"));
        gtk_box_pack_start(GTK_BOX(hbox), label_steepness, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox, 0, 1, 3, 4,
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



        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        label_defvol = gtk_label_new(_("RVA for Unmeasured Files [dB] :"));
        gtk_box_pack_start(GTK_BOX(hbox), label_defvol, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox, 0, 1, 4, 5,
                         GTK_FILL, GTK_FILL, 5, 2);

	rva_no_rva_voladj_shadow = options.rva_no_rva_voladj;
        adj_defvol = gtk_adjustment_new(options.rva_no_rva_voladj, -10.0f, 10.0f, 0.1f, 1.0f, 0.0f);
        spin_defvol = gtk_spin_button_new(GTK_ADJUSTMENT(adj_defvol), 0.1, 1);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_defvol), TRUE);
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_defvol), FALSE);
	g_signal_connect(G_OBJECT(adj_defvol), "value_changed",
			 G_CALLBACK(defvol_changed), NULL);
        gtk_table_attach(GTK_TABLE(table_rva), spin_defvol, 1, 2, 4, 5,
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
        gtk_table_attach(GTK_TABLE(table_rva), check_rva_use_averaging, 0, 2, 5, 6,
                         GTK_FILL, GTK_FILL, 5, 2);

        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        label_threshold = gtk_label_new(_("Drop statistical aberrations based on"));
        gtk_box_pack_start(GTK_BOX(hbox), label_threshold, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox, 0, 1, 6, 7,
                         GTK_FILL, GTK_FILL, 5, 2);

	combo_threshold = gtk_combo_box_text_new ();
        gtk_table_attach(GTK_TABLE(table_rva), combo_threshold, 1, 2, 6, 7,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 2);

        /* xgettext:no-c-format */
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_threshold), _("% of standard deviation"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_threshold), _("Linear threshold [dB]"));

	rva_use_linear_thresh_shadow = options.rva_use_linear_thresh;
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_threshold), options.rva_use_linear_thresh);
	g_signal_connect(combo_threshold, "changed", G_CALLBACK(changed_threshold), NULL);

        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        label_linthresh = gtk_label_new(_("Linear threshold [dB] :"));
        gtk_box_pack_start(GTK_BOX(hbox), label_linthresh, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox, 0, 1, 7, 8,
                         GTK_FILL, GTK_FILL, 5, 2);

	rva_avg_linear_thresh_shadow = options.rva_avg_linear_thresh;
        adj_linthresh = gtk_adjustment_new(options.rva_avg_linear_thresh, 0.0f, 60.0f, 0.1f, 1.0f, 0.0f);
        spin_linthresh = gtk_spin_button_new(GTK_ADJUSTMENT(adj_linthresh), 0.1, 1);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_linthresh), TRUE);
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_linthresh), FALSE);
	g_signal_connect(G_OBJECT(adj_linthresh), "value_changed",
			 G_CALLBACK(linthresh_changed), NULL);
        gtk_table_attach(GTK_TABLE(table_rva), spin_linthresh, 1, 2, 7, 8,
                         GTK_FILL, GTK_FILL, 5, 2);

        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        /* xgettext:no-c-format */
        label_stdthresh = gtk_label_new(_("% of standard deviation :"));
        gtk_box_pack_start(GTK_BOX(hbox), label_stdthresh, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_rva), hbox, 0, 1, 8, 9,
                         GTK_FILL, GTK_FILL, 5, 2);

	rva_avg_stddev_thresh_shadow = options.rva_avg_stddev_thresh;
        adj_stdthresh = gtk_adjustment_new(options.rva_avg_stddev_thresh * 100.0f,
					   0.0f, 500.0f, 1.0f, 10.0f, 0.0f);
        spin_stdthresh = gtk_spin_button_new(GTK_ADJUSTMENT(adj_stdthresh), 0.5, 0);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_stdthresh), TRUE);
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_stdthresh), FALSE);
	g_signal_connect(G_OBJECT(adj_stdthresh), "value_changed",
			 G_CALLBACK(stdthresh_changed), NULL);
        gtk_table_attach(GTK_TABLE(table_rva), spin_stdthresh, 1, 2, 8, 9,
                         GTK_FILL, GTK_FILL, 5, 2);

	if (!rva_use_averaging_shadow) {
		gtk_widget_set_sensitive(combo_threshold, FALSE);
		gtk_widget_set_sensitive(label_threshold, FALSE);
		gtk_widget_set_sensitive(spin_linthresh, FALSE);
		gtk_widget_set_sensitive(label_linthresh, FALSE);
		gtk_widget_set_sensitive(spin_stdthresh, FALSE);
		gtk_widget_set_sensitive(label_stdthresh, FALSE);
	}

	if (!rva_is_enabled_shadow) {
		gtk_widget_set_sensitive(combo_listening_env, FALSE);
		gtk_widget_set_sensitive(label_listening_env, FALSE);
		gtk_widget_set_sensitive(spin_refvol, FALSE);
		gtk_widget_set_sensitive(label_refvol, FALSE);
		gtk_widget_set_sensitive(spin_steepness, FALSE);
		gtk_widget_set_sensitive(label_steepness, FALSE);
		gtk_widget_set_sensitive(check_rva_use_averaging, FALSE);
		gtk_widget_set_sensitive(combo_threshold, FALSE);
		gtk_widget_set_sensitive(label_threshold, FALSE);
		gtk_widget_set_sensitive(spin_linthresh, FALSE);
		gtk_widget_set_sensitive(label_linthresh, FALSE);
		gtk_widget_set_sensitive(spin_stdthresh, FALSE);
		gtk_widget_set_sensitive(label_stdthresh, FALSE);
		gtk_widget_set_sensitive(spin_defvol, FALSE);
		gtk_widget_set_sensitive(label_defvol, FALSE);
	}

	/* "Metadata" notebook page */

	vbox_meta = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_meta), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_meta, create_notebook_tab(_("Metadata"), "metadata.png"));


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 5);

	label = gtk_label_new(_("ReplayGain tag to use (with fallback to the other): "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox, FALSE, TRUE, 0);

	combo_replaygain = gtk_combo_box_text_new ();
        gtk_box_pack_start(GTK_BOX(hbox), combo_replaygain, FALSE, FALSE, 35);

	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_replaygain), _("Replaygain_track_gain"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_replaygain), _("Replaygain_album_gain"));

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_replaygain), options.replaygain_tag_to_use);


	hbox_meta = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox_meta, FALSE, TRUE, 8);

	frame_meta = gtk_frame_new(_("Adding files to Playlist"));
        gtk_box_pack_start(GTK_BOX(hbox_meta), frame_meta, TRUE, TRUE, 0);
	vbox_meta2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(frame_meta), vbox_meta2);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta2), hbox, FALSE, TRUE, 3);
	check_meta_use_basename_only =
		gtk_check_button_new_with_label(_("Use basename only instead of full path\n"
						  "if no metadata is available."));
	gtk_widget_set_name(check_meta_use_basename_only, "check_on_notebook");
	if (options.meta_use_basename_only) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_meta_use_basename_only), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_meta_use_basename_only, FALSE, TRUE, 35);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta2), hbox, FALSE, TRUE, 3);
	check_meta_rm_extension = gtk_check_button_new_with_label(_("Remove file extension"));
	gtk_widget_set_name(check_meta_rm_extension, "check_on_notebook");
	if (options.meta_rm_extension) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_meta_rm_extension), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_meta_rm_extension, FALSE, TRUE, 35);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta2), hbox, FALSE, TRUE, 3);
	check_meta_us_to_space = gtk_check_button_new_with_label(_("Convert underscore to space"));
	gtk_widget_set_name(check_meta_us_to_space, "check_on_notebook");
	if (options.meta_us_to_space) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_meta_us_to_space), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_meta_us_to_space, FALSE, TRUE, 35);


	hbox_meta = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox_meta, FALSE, TRUE, 8);

	frame_meta = gtk_frame_new(_("Metadata editor (File info dialog)"));
        gtk_box_pack_start(GTK_BOX(hbox_meta), frame_meta, TRUE, TRUE, 0);
	vbox_meta2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(frame_meta), vbox_meta2);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta2), hbox, FALSE, TRUE, 3);
	check_metaedit_auto_clone =
		gtk_check_button_new_with_label(_("When adding new frames, "
		"try to set their contents\nfrom another tag's equivalent frame."));
	gtk_widget_set_name(check_metaedit_auto_clone, "check_on_notebook");
	if (options.metaedit_auto_clone) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_metaedit_auto_clone), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_metaedit_auto_clone, FALSE, TRUE, 35);

	hbox_meta = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_box_pack_start(GTK_BOX(vbox_meta), hbox_meta, FALSE, TRUE, 8);

	frame_meta = gtk_frame_new(_("MPEG audio ID3v1/ID3v2 character encoding"));
	gtk_box_pack_start(GTK_BOX(hbox_meta), frame_meta, TRUE, TRUE, 0);
	vbox_meta2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(frame_meta), vbox_meta2);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox_meta2), hbox, FALSE, TRUE, 3);

	encode_set_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(encode_set_entry), MAXLEN - 1);
	gtk_box_pack_start(GTK_BOX(hbox), encode_set_entry, TRUE, TRUE, 3);
	gtk_entry_set_text(GTK_ENTRY(encode_set_entry), options.encode_set);

	help_btn_meta_encode = gtk_button_new_from_stock (GTK_STOCK_HELP);
	g_signal_connect(help_btn_meta_encode, "clicked", G_CALLBACK(display_meta_encode_help), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), help_btn_meta_encode, FALSE, FALSE, 5);

	hbox_meta = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(vbox_meta), hbox_meta, FALSE, TRUE, 0);

	frame_meta = gtk_frame_new(_("Batch update & encode (mass tagger, CD ripper, file export)"));
        gtk_box_pack_start(GTK_BOX(hbox_meta), frame_meta, TRUE, TRUE, 0);
	vbox_meta2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(frame_meta), vbox_meta2);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta2), hbox, FALSE, TRUE, 5);

	label = gtk_label_new(_("Tags to add when creating or updating MPEG audio files:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta2), hbox, FALSE, TRUE, 0);
	check_batch_mpeg_add_id3v1 = gtk_check_button_new_with_label(_("ID3v1"));
	gtk_widget_set_name(check_batch_mpeg_add_id3v1, "check_on_notebook");
	if (options.batch_mpeg_add_id3v1) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_batch_mpeg_add_id3v1), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_batch_mpeg_add_id3v1, FALSE, TRUE, 35);


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta2), hbox, FALSE, TRUE, 0);
	check_batch_mpeg_add_id3v2 = gtk_check_button_new_with_label(_("ID3v2"));
	gtk_widget_set_name(check_batch_mpeg_add_id3v2, "check_on_notebook");
	if (options.batch_mpeg_add_id3v2) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_batch_mpeg_add_id3v2), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_batch_mpeg_add_id3v2, FALSE, TRUE, 35);


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta2), hbox, FALSE, TRUE, 0);
	check_batch_mpeg_add_ape = gtk_check_button_new_with_label(_("APE"));
	gtk_widget_set_name(check_batch_mpeg_add_ape, "check_on_notebook");
	if (options.batch_mpeg_add_ape) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_batch_mpeg_add_ape), TRUE);
	}
        gtk_box_pack_start(GTK_BOX(hbox), check_batch_mpeg_add_ape, FALSE, TRUE, 35);


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(vbox_meta2), hbox, FALSE, TRUE, 5);

	label = gtk_label_new(_("Note: pre-existing tags will be updated even though they\n"
				"might not be checked here."));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);


	/* CDDA notebook page */
#ifdef HAVE_CDDA
	table_cdda = gtk_table_new(5, 3, FALSE);
        gtk_container_set_border_width(GTK_CONTAINER(table_cdda), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table_cdda, create_notebook_tab(_("CD Audio"), "cdda.png"));

	label = gtk_label_new(_("CD drive speed:"));
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_cdda), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 3);

	cdda_drive_speed_spinner = gtk_spin_button_new_with_range(1, 99, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(cdda_drive_speed_spinner), options.cdda_drive_speed);
        gtk_table_attach(GTK_TABLE(table_cdda), cdda_drive_speed_spinner, 1, 2, 0, 1,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 3);

        help_btn_cdda_drive_speed = gtk_button_new_from_stock(GTK_STOCK_HELP);
	g_signal_connect(help_btn_cdda_drive_speed, "clicked", G_CALLBACK(display_cdda_drive_speed_help), NULL);
        gtk_table_attach(GTK_TABLE(table_cdda), help_btn_cdda_drive_speed, 2, 3, 0, 1, GTK_FILL, GTK_FILL, 5, 3);

	frame_cdda = gtk_frame_new(_("Paranoia error correction"));
        gtk_table_attach(GTK_TABLE(table_cdda), frame_cdda, 0, 3, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 3);

	vbox_cdda = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_cdda), 8);
	gtk_container_add(GTK_CONTAINER(frame_cdda), vbox_cdda);

	check_cdda_mode_overlap = gtk_check_button_new_with_label(_("Perform overlapped reads"));
        gtk_widget_set_name(check_cdda_mode_overlap, "check_on_notebook");
	gtk_box_pack_start(GTK_BOX(vbox_cdda), check_cdda_mode_overlap, FALSE, FALSE, 0);

	check_cdda_mode_verify = gtk_check_button_new_with_label(_("Verify data integrity"));
        gtk_widget_set_name(check_cdda_mode_verify, "check_on_notebook");
	gtk_box_pack_start(GTK_BOX(vbox_cdda), check_cdda_mode_verify, FALSE, FALSE, 0);

	check_cdda_mode_neverskip = gtk_check_button_new_with_label(_("Unlimited retry on failed reads (never skip)"));
        gtk_widget_set_name(check_cdda_mode_neverskip, "check_on_notebook");
	if (options.cdda_paranoia_mode & PARANOIA_MODE_NEVERSKIP) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_cdda_mode_neverskip), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox_cdda), check_cdda_mode_neverskip, FALSE, FALSE, 0);
	g_signal_connect(check_cdda_mode_neverskip, "toggled", G_CALLBACK(cdda_toggled), NULL);

        hbox_cdda = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_box_pack_start(GTK_BOX(vbox_cdda), hbox_cdda, FALSE, FALSE, 0);

	label_cdda_maxretries = gtk_label_new(_("\tMaximum number of retries:"));
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label_cdda_maxretries, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox_cdda), hbox, FALSE, FALSE, 0);

	cdda_paranoia_maxretries_spinner = gtk_spin_button_new_with_range(1, 50, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(cdda_paranoia_maxretries_spinner), options.cdda_paranoia_maxretries);
	gtk_box_pack_start(GTK_BOX(hbox_cdda), cdda_paranoia_maxretries_spinner, FALSE, FALSE, 5);

	check_cdda_force_drive_rescan =
		gtk_check_button_new_with_label(_("Force TOC re-read on every drive scan"));
        gtk_widget_set_name(check_cdda_force_drive_rescan, "check_on_notebook");
        gtk_table_attach(GTK_TABLE(table_cdda), check_cdda_force_drive_rescan, 0, 2, 2, 3,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 3);

        help_btn_cdda_force_drive_rescan = gtk_button_new_from_stock(GTK_STOCK_HELP);
	g_signal_connect(help_btn_cdda_force_drive_rescan, "clicked", G_CALLBACK(display_cdda_force_drive_rescan_help), NULL);
        gtk_table_attach(GTK_TABLE(table_cdda), help_btn_cdda_force_drive_rescan, 2, 3, 2, 3, GTK_FILL, GTK_FILL, 5, 3);

	check_cdda_add_to_playlist =
		gtk_check_button_new_with_label(_("Automatically add CDs to Playlist"));
        gtk_widget_set_name(check_cdda_add_to_playlist, "check_on_notebook");
        gtk_table_attach(GTK_TABLE(table_cdda), check_cdda_add_to_playlist, 0, 3, 3, 4,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 3);

	check_cdda_remove_from_playlist =
		gtk_check_button_new_with_label(_("Automatically remove CDs from Playlist"));
        gtk_widget_set_name(check_cdda_remove_from_playlist, "check_on_notebook");
        gtk_table_attach(GTK_TABLE(table_cdda), check_cdda_remove_from_playlist, 0, 3, 4, 5,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 3);

	if (options.cdda_force_drive_rescan) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_cdda_force_drive_rescan), TRUE);
	}

	if (options.cdda_paranoia_mode & PARANOIA_MODE_OVERLAP) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_cdda_mode_overlap), TRUE);
	}

	if (options.cdda_paranoia_mode & PARANOIA_MODE_VERIFY) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_cdda_mode_verify), TRUE);
	}

	if (options.cdda_paranoia_mode & PARANOIA_MODE_NEVERSKIP) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_cdda_mode_neverskip), TRUE);
		gtk_widget_set_sensitive(cdda_paranoia_maxretries_spinner, FALSE);
		gtk_widget_set_sensitive(label_cdda_maxretries, FALSE);
	}

	if (options.cdda_add_to_playlist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_cdda_add_to_playlist), TRUE);
	}

	if (options.cdda_remove_from_playlist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_cdda_remove_from_playlist), TRUE);
	}

#endif /* HAVE_CDDA */


	/* CDDB notebook page */

#ifdef HAVE_CDDB
	table_cddb = gtk_table_new(8, 2, FALSE);
        gtk_container_set_border_width(GTK_CONTAINER(table_cddb), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table_cddb, create_notebook_tab(_("CDDB"), "cddb.png"));

	label = gtk_label_new(_("CDDB server:"));
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_cddb), hbox, 0, 1, 0, 1,
                         GTK_FILL, GTK_FILL, 5, 3);

	cddb_server_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(cddb_server_entry), options.cddb_server);
        gtk_table_attach(GTK_TABLE(table_cddb), cddb_server_entry, 1, 2, 0, 1,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 3);

	label = gtk_label_new(_("Connection timeout [sec]:"));
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_cddb), hbox, 0, 1, 1, 2,
                         GTK_FILL, GTK_FILL, 5, 3);

	cddb_tout_spinner = gtk_spin_button_new_with_range(1, 60, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(cddb_tout_spinner), options.cddb_timeout);
        gtk_table_attach(GTK_TABLE(table_cddb), cddb_tout_spinner, 1, 2, 1, 2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 3);

	cddb_label_proto = gtk_label_new(_("Protocol for querying (direct connection only):"));
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), cddb_label_proto, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_cddb), hbox, 0, 1, 5, 6,
                         GTK_FILL, GTK_FILL, 5, 3);

	cddb_proto_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cddb_proto_combo), _("CDDBP (port 8880)"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cddb_proto_combo), _("HTTP (port 80)"));
	if (options.cddb_use_http) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(cddb_proto_combo), 1);
	} else {
		gtk_combo_box_set_active(GTK_COMBO_BOX(cddb_proto_combo), 0);
	}
        gtk_table_attach(GTK_TABLE(table_cddb), cddb_proto_combo, 1, 2, 5, 6,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 3);

#endif /* HAVE_CDDB */

	/* Internet notebook page */

	vbox_inet = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_inet), 8);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_inet, create_notebook_tab(_("Internet"), "inet.png"));

	inet_radio_direct = gtk_radio_button_new_with_label(NULL, _("Direct connection to the Internet"));
	gtk_widget_set_name(inet_radio_direct, "check_on_notebook");
	gtk_box_pack_start(GTK_BOX(vbox_inet), inet_radio_direct, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(inet_radio_direct), "toggled",
			 G_CALLBACK(inet_radio_direct_toggled), NULL);

	inet_radio_proxy = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(inet_radio_direct),
								       _("Connect via HTTP proxy"));
	gtk_widget_set_name(inet_radio_proxy, "check_on_notebook");
	gtk_box_pack_start(GTK_BOX(vbox_inet), inet_radio_proxy, FALSE, FALSE, 0);

	inet_frame = gtk_frame_new(_("Proxy settings"));
	gtk_box_pack_start(GTK_BOX(vbox_inet), inet_frame, FALSE, FALSE, 0);

	table_inet = gtk_table_new(2/*row*/, 4, FALSE);
        gtk_container_set_border_width(GTK_CONTAINER(table_inet), 8);
	gtk_container_add(GTK_CONTAINER(inet_frame), table_inet);


	inet_label_proxy = gtk_label_new(_("Proxy host:"));
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), inet_label_proxy, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_inet), hbox, 0, 1, 0, 1,
                         GTK_FILL, GTK_FILL, 5, 3);

	inet_entry_proxy = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(inet_entry_proxy), options.inet_proxy);
        gtk_table_attach(GTK_TABLE(table_inet), inet_entry_proxy, 1, 2, 0, 1,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 3);

	inet_label_proxy_port = gtk_label_new(_("Port:"));
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), inet_label_proxy_port, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_inet), hbox, 2, 3, 0, 1,
                         GTK_FILL, GTK_FILL, 5, 3);

	inet_spinner_proxy_port = gtk_spin_button_new_with_range(0, 65535, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(inet_spinner_proxy_port), options.inet_proxy_port);
        gtk_table_attach(GTK_TABLE(table_inet), inet_spinner_proxy_port, 3, 4, 0, 1,
                         GTK_FILL, GTK_FILL, 5, 3);

	inet_label_noproxy_domains = gtk_label_new(_("No proxy for:"));
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), inet_label_noproxy_domains, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table_inet), hbox, 0, 1, 1, 2,
                         GTK_FILL, GTK_FILL, 5, 3);

	inet_entry_noproxy_domains = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(inet_entry_noproxy_domains), options.inet_noproxy_domains);
        gtk_table_attach(GTK_TABLE(table_inet), inet_entry_noproxy_domains, 1, 3, 1, 2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 3);

        inet_help_noproxy_domains = gtk_button_new_from_stock(GTK_STOCK_HELP);
	g_signal_connect(inet_help_noproxy_domains, "clicked", G_CALLBACK(display_inet_help_noproxy_domains), NULL);
        gtk_table_attach(GTK_TABLE(table_inet), inet_help_noproxy_domains, 3, 4, 1, 2, GTK_FILL, GTK_FILL, 5, 3);


	inet_hbox_timeout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox_inet), inet_hbox_timeout, FALSE, FALSE, 5);

	inet_label_timeout = gtk_label_new(_("Timeout for socket I/O:"));
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), inet_label_timeout, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(inet_hbox_timeout), hbox, FALSE, FALSE, 5);

	inet_spinner_timeout = gtk_spin_button_new_with_range(1, 300, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(inet_spinner_timeout), options.inet_timeout);
	gtk_box_pack_start(GTK_BOX(inet_hbox_timeout), inet_spinner_timeout, FALSE, FALSE, 5);

	inet_label_timeout = gtk_label_new(_("seconds"));
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), inet_label_timeout, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(inet_hbox_timeout), hbox, FALSE, FALSE, 5);

	if (options.inet_use_proxy) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(inet_radio_direct), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(inet_radio_proxy), TRUE);
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(inet_radio_direct), TRUE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(inet_radio_proxy), FALSE);
		gtk_widget_set_sensitive(inet_frame, FALSE);
	}


        /* end of notebook */

	gtk_widget_show_all(options_window);
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), current_notebook_page);

	return;
}


#define SAVE_STR(Var) \
        xmlNewTextChild(root, NULL, (const xmlChar *) #Var, (xmlChar *) options.Var);

#define SAVE_FONT(Font) \
	arr_snprintf(str, "%s", options.Font); \
        xmlNewTextChild(root, NULL, (const xmlChar *) #Font, (xmlChar *) str);

#define SAVE_COLOR(Color) \
	arr_snprintf(str, "%s", options.Color); \
        xmlNewTextChild(root, NULL, (const xmlChar *) #Color, (xmlChar *) str);

#define SAVE_INT(Var) \
	arr_snprintf(str, "%d", options.Var); \
        xmlNewTextChild(root, NULL, (const xmlChar *) #Var, (xmlChar *) str);

#define SAVE_INT_ARRAY(Var, Idx) \
        arr_snprintf(str, "%d", options.Var[Idx]); \
        xmlNewTextChild(root, NULL, (const xmlChar *) #Var "_" #Idx, (xmlChar *) str);

#define SAVE_INT_SH(Var) \
	arr_snprintf(str, "%d", options.Var##_shadow); \
        xmlNewTextChild(root, NULL, (const xmlChar *) #Var, (xmlChar *) str);

#define SAVE_FLOAT(Var) \
        arr_snprintf(str, "%f", options.Var); \
        xmlNewTextChild(root, NULL, (const xmlChar *) #Var, (xmlChar *) str);


void
save_config(void) {

        xmlDocPtr doc;
        xmlNodePtr root;
        int c, d;
        FILE * fin;
        FILE * fout;
        char tmpname[MAXLEN];
        char config_file[MAXLEN];
	char str[MAXLEN];

	GtkTreeIter iter;
	int i = 0;
	char * path;

        arr_snprintf(config_file, "%s/config.xml", options.confdir);

        doc = xmlNewDoc((const xmlChar *) "1.0");
        root = xmlNewNode(NULL, (const xmlChar *) "aqualung_config");
        xmlDocSetRootElement(doc, root);


	SAVE_STR(audiodir);
	SAVE_STR(currdir);
	SAVE_STR(exportdir);
	SAVE_STR(plistdir);
	SAVE_STR(podcastdir);
	SAVE_STR(ripdir);
	SAVE_STR(storedir);
	SAVE_STR(default_param);
	SAVE_STR(title_format);
	SAVE_INT(src_type);
	SAVE_INT(ladspa_is_postfader);
	SAVE_INT(auto_save_playlist);
	SAVE_INT(playlist_auto_save);
	SAVE_INT(playlist_auto_save_int);
	SAVE_INT(show_rva_in_playlist);
	SAVE_INT(pl_statusbar_show_size);
	SAVE_INT(ms_statusbar_show_size);
	SAVE_INT(show_length_in_playlist);
	SAVE_INT(show_active_track_name_in_bold);
	SAVE_INT(auto_roll_to_active_track);
	SAVE_INT(enable_pl_rules_hint);
	SAVE_INT(enable_ms_rules_hint);
	SAVE_INT_SH(enable_ms_tree_icons);
	SAVE_INT(ms_confirm_removal);
	SAVE_INT(batch_mpeg_add_id3v1);
	SAVE_INT(batch_mpeg_add_id3v2);
	SAVE_INT(batch_mpeg_add_ape);
	SAVE_STR(encode_set);
	SAVE_INT(metaedit_auto_clone);
	SAVE_INT(meta_use_basename_only);
	SAVE_INT(meta_rm_extension);
	SAVE_INT(meta_us_to_space);
	SAVE_INT(enable_tooltips);
	SAVE_INT(dark_theme);
	SAVE_INT(disable_buttons_relief);
	SAVE_INT_SH(combine_play_pause);
	SAVE_INT_SH(simple_view_in_fx);
	SAVE_INT(show_sn_title);
	SAVE_INT(united_minimization);
	SAVE_INT(magnify_smaller_images);
	SAVE_INT(cover_width);
	SAVE_INT(dont_show_cover);
	SAVE_INT(use_external_cover_first);
	SAVE_INT(show_cover_for_ms_tracks_only);
	SAVE_INT(use_systray);
	SAVE_INT(systray_start_minimized);
	SAVE_INT_SH(hide_comment_pane);
	SAVE_INT_SH(enable_mstore_toolbar);
	SAVE_INT_SH(enable_mstore_statusbar);
	SAVE_INT(autoexpand_stores);
	SAVE_INT(show_hidden);
	SAVE_INT(main_window_always_on_top);
	SAVE_INT(tags_tab_first);
	SAVE_INT(replaygain_tag_to_use);
	SAVE_FLOAT(vol);
	SAVE_FLOAT(bal);
	SAVE_INT(rva_is_enabled);
	SAVE_INT(rva_env);
	SAVE_FLOAT(rva_refvol);
	SAVE_FLOAT(rva_steepness);
	SAVE_INT(rva_use_averaging);
	SAVE_INT(rva_use_linear_thresh);
	SAVE_FLOAT(rva_avg_linear_thresh);
	SAVE_FLOAT(rva_avg_stddev_thresh);
	SAVE_FLOAT(rva_no_rva_voladj);
	SAVE_INT(main_pos_x);
	SAVE_INT(main_pos_y);
	SAVE_INT(main_size_x);

	if (options.playlist_is_embedded && !options.playlist_is_embedded_shadow && options.playlist_on) {
		GtkAllocation allocation;
		gtk_widget_get_allocation(playlist_window, &allocation);
		arr_snprintf(str, "%d", options.main_size_y - allocation.height - 6);
	} else {
		arr_snprintf(str, "%d", options.main_size_y);
	}
        xmlNewTextChild(root, NULL, (const xmlChar *) "main_size_y", (xmlChar *) str);

	SAVE_INT(browser_pos_x);
	SAVE_INT(browser_pos_y);
	SAVE_INT(browser_size_x);
	SAVE_INT(browser_size_y);
	SAVE_INT(browser_on);
	SAVE_INT(browser_paned_pos);
	SAVE_INT(playlist_pos_x);
	SAVE_INT(playlist_pos_y);
	SAVE_INT(playlist_size_x);
	SAVE_INT(playlist_size_y);
	SAVE_INT(playlist_on);
	SAVE_INT_SH(playlist_is_embedded);
	SAVE_INT_SH(buttons_at_the_bottom);
	SAVE_INT(playlist_is_tree);
	SAVE_INT(playlist_always_show_tabs);
	SAVE_INT(playlist_show_close_button_in_tab);
	SAVE_INT(album_shuffle_mode);
	SAVE_INT_SH(enable_playlist_statusbar);
	SAVE_INT(ifpmanager_size_x);
	SAVE_INT(ifpmanager_size_y);
	SAVE_INT(repeat_on);
	SAVE_INT(repeat_all_on);
	SAVE_INT(shuffle_on);
	SAVE_INT_ARRAY(time_idx, 0);
	SAVE_INT_ARRAY(time_idx, 1);
	SAVE_INT_ARRAY(time_idx, 2);
	SAVE_INT_ARRAY(plcol_idx, 0);
	SAVE_INT_ARRAY(plcol_idx, 1);
	SAVE_INT_ARRAY(plcol_idx, 2);
	SAVE_INT(search_pl_flags);
	SAVE_INT(search_ms_flags);
	SAVE_INT(cdda_drive_speed);
	SAVE_INT(cdda_paranoia_mode);
	SAVE_INT(cdda_paranoia_maxretries);
	SAVE_INT(cdda_force_drive_rescan);
	SAVE_INT(cdda_add_to_playlist);
	SAVE_INT(cdda_remove_from_playlist);
	SAVE_STR(cddb_server);
	SAVE_INT(cddb_timeout);
	SAVE_INT(cddb_use_http);
	SAVE_INT(inet_use_proxy);
	SAVE_STR(inet_proxy);
	SAVE_INT(inet_proxy_port);
	SAVE_STR(inet_noproxy_domains);
	SAVE_INT(inet_timeout);
	SAVE_FLOAT(loop_range_start);
	SAVE_FLOAT(loop_range_end);
	SAVE_INT(wm_systray_warn);
	SAVE_INT(podcasts_autocheck);
	SAVE_INT(cdrip_deststore);
	SAVE_INT(cdrip_file_format);
	SAVE_INT(cdrip_bitrate);
	SAVE_INT(cdrip_vbr);
	SAVE_INT(cdrip_metadata);
	SAVE_STR(export_template);
	SAVE_INT(export_subdir_artist);
	SAVE_INT(export_subdir_album);
	SAVE_INT(export_subdir_limit);
	SAVE_INT(export_file_format);
	SAVE_INT(export_bitrate);
	SAVE_INT(export_vbr);
	SAVE_INT(export_metadata);
	SAVE_INT(export_filter_same);
	SAVE_INT(export_excl_enabled);
	SAVE_STR(export_excl_pattern);
	SAVE_INT(batch_tag_flags);
	SAVE_STR(ext_title_format_file);

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ms_pathlist_store), &iter, NULL, i++)) {
		char * utf8;
		gtk_tree_model_get(GTK_TREE_MODEL(ms_pathlist_store), &iter, 0, &path, -1);
		utf8 = g_filename_to_utf8(path, -1, NULL, NULL, NULL);
		xmlNewTextChild(root, NULL, (const xmlChar *) "music_store", (xmlChar *) utf8);
		g_free(path);
		g_free(utf8);
	}

	save_systray_options(doc, root);
#ifdef HAVE_JACK
	save_jack_connections(doc, root);
#endif /* HAVE_JACK */

        arr_snprintf(tmpname, "%s/config.xml.temp", options.confdir);
        xmlSaveFormatFile(tmpname, doc, 1);
	xmlFreeDoc(doc);

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


#define LOAD_STR(Var) \
                if ((!xmlStrcmp(cur->name, (const xmlChar *) #Var))) { \
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1); \
                        if (key != NULL) \
                                arr_strlcpy(options.Var, (char *) key); \
                        xmlFree(key); \
                }

#define LOAD_INT(Var) \
                if ((!xmlStrcmp(cur->name, (const xmlChar *) #Var))) { \
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1); \
                        if (key != NULL) \
				sscanf((char *) key, "%d", &options.Var); \
                        xmlFree(key); \
                }

#define LOAD_INT_ARRAY(Var, Idx) \
                if ((!xmlStrcmp(cur->name, (const xmlChar *) #Var "_" #Idx))) { \
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1); \
                        if (key != NULL) \
				sscanf((char *) key, "%d", options.Var + Idx); \
                        xmlFree(key); \
                }

#define LOAD_INT_SH(Var) \
                if ((!xmlStrcmp(cur->name, (const xmlChar *) #Var))) { \
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1); \
                        if (key != NULL) { \
				sscanf((char *) key, "%d", &options.Var); \
				options.Var##_shadow = options.Var; \
			} \
                        xmlFree(key); \
                }

#define LOAD_FLOAT(Var) \
                if ((!xmlStrcmp(cur->name, (const xmlChar *) #Var))) { \
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1); \
                        if (key != NULL) { \
				options.Var = convf((char *) key); \
			} \
                        xmlFree(key); \
                }


void
load_config(void) {

        xmlDocPtr doc;
        xmlNodePtr cur;
        xmlNodePtr root;
	xmlChar * key;
        char config_file[MAXLEN];
        FILE * f;


        arr_snprintf(config_file, "%s/config.xml", options.confdir);

        if ((f = fopen(config_file, "rt")) == NULL) {
		/* no warning -- done that in core.c::load_default_cl() */
                doc = xmlNewDoc((const xmlChar *) "1.0");
                root = xmlNewNode(NULL, (const xmlChar *) "aqualung_config");
                xmlDocSetRootElement(doc, root);
                xmlSaveFormatFile(config_file, doc, 1);
		xmlFreeDoc(doc);
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


	if (!src_type_parsed) {
		options.src_type = 4;
	}

	options.vol = 0.0f;
	options.bal = 0.0f;

	options.repeat_on = 0;
	options.repeat_all_on = 0;
	options.shuffle_on = 0;

	options.loop_range_start = 0.0f;
	options.loop_range_end = 1.0f;

	options.wm_systray_warn = 1;
	options.podcasts_autocheck = 1;

	options.search_pl_flags = 0;
	options.search_ms_flags = SEARCH_F_AN | SEARCH_F_RT | SEARCH_F_TT | SEARCH_F_CO;

	options.browser_paned_pos = 400;

	options.default_param[0] = '\0';
	options.title_format[0] = '\0';
        options.enable_tooltips = 1;
        options.show_sn_title = 1;
        options.united_minimization = 1;
        options.buttons_at_the_bottom = options.buttons_at_the_bottom_shadow = 0;
        options.combine_play_pause = options.combine_play_pause_shadow = 0;
	options.playlist_is_embedded = options.playlist_is_embedded_shadow = 1;
	options.playlist_is_tree = 1;
        options.playlist_show_close_button_in_tab = 1;

	options.enable_mstore_statusbar = options.enable_mstore_statusbar_shadow = 1;
       	options.enable_mstore_toolbar = options.enable_mstore_toolbar_shadow = 1;
        options.enable_ms_tree_icons = options.enable_ms_tree_icons_shadow = 1;
	options.ms_statusbar_show_size = 1;
	options.ms_confirm_removal = 1;

        options.cover_width = 2;

	options.autoexpand_stores = 1;

	options.auto_save_playlist = 1;
	options.playlist_auto_save = 0;
	options.playlist_auto_save_int = 5;
	options.show_length_in_playlist = 1;
	options.enable_playlist_statusbar = options.enable_playlist_statusbar_shadow = 1;
	options.pl_statusbar_show_size = 1;

	options.rva_refvol = -12.0f;
	options.rva_steepness = 1.0f;
	options.rva_use_averaging = 1;
	options.rva_use_linear_thresh = 0;
	options.rva_avg_linear_thresh = 3.0f;
	options.rva_avg_stddev_thresh = 2.0f;

	options.batch_mpeg_add_id3v1 = 1;
	options.batch_mpeg_add_id3v2 = 1;
	options.batch_mpeg_add_ape = 0;
	options.encode_set[0] = '\0';
	options.metaedit_auto_clone = 0;

	options.cdda_drive_speed = 4;
	options.cdda_paranoia_mode = 0; /* no paranoia */
	options.cdda_paranoia_maxretries = 20;
	options.cdda_force_drive_rescan = 0;

	options.cddb_server[0] = '\0';
	options.cddb_local[0] = '\0';
	options.cddb_timeout = 10;

	options.inet_use_proxy = 0;
	options.inet_proxy[0] = '\0';
	options.inet_proxy_port = 8080;
	options.inet_noproxy_domains[0] = '\0';
	options.inet_timeout = 5;

	options.time_idx[0] = 0;
	options.time_idx[1] = 1;
	options.time_idx[2] = 2;

	options.plcol_idx[0] = 0;
	options.plcol_idx[1] = 1;
	options.plcol_idx[2] = 2;

        options.cdrip_bitrate = 256;
        options.cdrip_vbr = 1;
        options.cdrip_metadata = 1;

	options.use_systray = 1;
	options.systray_start_minimized = 0;
	options.systray_mouse_wheel_horizontal = SYSTRAY_MW_CMD_DO_NOTHING;
	options.systray_mouse_wheel_vertical = SYSTRAY_MW_CMD_DO_NOTHING;
	options.systray_mouse_buttons_count = 0;
	options.systray_mouse_buttons = NULL;

	arr_strlcpy(options.export_template, "track%i.%x");
	options.export_subdir_limit = 16;
        options.export_bitrate = 256;
        options.export_vbr = 1;
        options.export_metadata = 1;
	options.export_filter_same = 1;
	options.export_excl_pattern[0] = '\0';

	options.ext_title_format_file[0] = '\0';

	options.batch_tag_flags = BATCH_TAG_TITLE | BATCH_TAG_ARTIST | BATCH_TAG_ALBUM |
		BATCH_TAG_YEAR | BATCH_TAG_COMMENT | BATCH_TAG_TRACKNO;

	ms_pathlist_store = gtk_list_store_new(3,
					       G_TYPE_STRING,   /* path */
					       G_TYPE_STRING,   /* displayed name */
					       G_TYPE_STRING);  /* state (rw, r, unreachable) */

        cur = cur->xmlChildrenNode;
        while (cur != NULL) {
		LOAD_STR(audiodir);
		LOAD_STR(currdir);
		LOAD_STR(exportdir);
		LOAD_STR(plistdir);
		LOAD_STR(podcastdir);
		LOAD_STR(ripdir);
		LOAD_STR(storedir);
		LOAD_STR(default_param);
		LOAD_STR(title_format);

		if (!src_type_parsed) {
			LOAD_INT(src_type);
		}

		LOAD_INT(ladspa_is_postfader);
		LOAD_INT(auto_save_playlist);
		LOAD_INT(playlist_auto_save);
		LOAD_INT(playlist_auto_save_int);
		LOAD_INT(batch_mpeg_add_id3v1);
		LOAD_INT(batch_mpeg_add_id3v2);
		LOAD_INT(batch_mpeg_add_ape);
		LOAD_STR(encode_set);
		LOAD_INT(metaedit_auto_clone);
		LOAD_INT(meta_use_basename_only);
		LOAD_INT(meta_rm_extension);
		LOAD_INT(meta_us_to_space);
		LOAD_INT(show_rva_in_playlist);
		LOAD_INT(pl_statusbar_show_size);
		LOAD_INT(ms_statusbar_show_size);
		LOAD_INT(show_length_in_playlist);
		LOAD_INT(show_active_track_name_in_bold);
		LOAD_INT(auto_roll_to_active_track);
		LOAD_INT(enable_pl_rules_hint);
		LOAD_INT(enable_ms_rules_hint);
		LOAD_INT_SH(enable_ms_tree_icons);
		LOAD_INT(ms_confirm_removal);
		LOAD_INT(enable_tooltips);
		LOAD_INT(dark_theme);
		LOAD_INT(disable_buttons_relief);
		LOAD_INT_SH(combine_play_pause);
		LOAD_INT_SH(simple_view_in_fx);
		LOAD_INT(show_sn_title);
		LOAD_INT(united_minimization);
		LOAD_INT(magnify_smaller_images);
		LOAD_INT(cover_width);
		LOAD_INT(dont_show_cover);
		LOAD_INT(use_external_cover_first);
		LOAD_INT(show_cover_for_ms_tracks_only);
		LOAD_INT(use_systray);
		LOAD_INT(systray_start_minimized);
		LOAD_INT_SH(hide_comment_pane);
		LOAD_INT_SH(enable_mstore_toolbar);
		LOAD_INT_SH(enable_mstore_statusbar);
		LOAD_INT(autoexpand_stores);
		LOAD_INT(show_hidden);
		LOAD_INT(main_window_always_on_top);
		LOAD_INT(tags_tab_first);
		LOAD_INT(replaygain_tag_to_use);
		LOAD_FLOAT(vol);
		LOAD_FLOAT(bal);
		LOAD_INT(rva_is_enabled);
		LOAD_INT(rva_env);
		LOAD_FLOAT(rva_refvol);
		LOAD_FLOAT(rva_steepness);
		LOAD_INT(rva_use_averaging);
		LOAD_INT(rva_use_linear_thresh);
		LOAD_FLOAT(rva_avg_linear_thresh);
		LOAD_FLOAT(rva_avg_stddev_thresh);
		LOAD_FLOAT(rva_no_rva_voladj);
		LOAD_INT(main_pos_x);
		LOAD_INT(main_pos_y);
		LOAD_INT(main_size_x);
		LOAD_INT(main_size_y);
		LOAD_INT(browser_pos_x);
		LOAD_INT(browser_pos_y);
		LOAD_INT(browser_size_x);
		LOAD_INT(browser_size_y);
		LOAD_INT(browser_on);
		LOAD_INT(browser_paned_pos);
		LOAD_INT(playlist_pos_x);
		LOAD_INT(playlist_pos_y);
		LOAD_INT(playlist_size_x);
		LOAD_INT(playlist_size_y);
		LOAD_INT(playlist_on);
		LOAD_INT_SH(playlist_is_embedded);
		LOAD_INT_SH(buttons_at_the_bottom);
		LOAD_INT(playlist_is_tree);
		LOAD_INT(playlist_always_show_tabs);
		LOAD_INT(playlist_show_close_button_in_tab);
		LOAD_INT(album_shuffle_mode);
		LOAD_INT_SH(enable_playlist_statusbar);
		LOAD_INT(ifpmanager_size_x);
		LOAD_INT(ifpmanager_size_y);
		LOAD_INT(repeat_on);
		LOAD_INT(repeat_all_on);
		LOAD_INT(shuffle_on);
		LOAD_INT_ARRAY(time_idx, 0);
		LOAD_INT_ARRAY(time_idx, 1);
		LOAD_INT_ARRAY(time_idx, 2);
		LOAD_INT_ARRAY(plcol_idx, 0);
		LOAD_INT_ARRAY(plcol_idx, 1);
		LOAD_INT_ARRAY(plcol_idx, 2);
		LOAD_INT(search_pl_flags);
		LOAD_INT(search_ms_flags);
		LOAD_INT(cdda_drive_speed);
		LOAD_INT(cdda_paranoia_mode);
		LOAD_INT(cdda_paranoia_maxretries);
		LOAD_INT(cdda_force_drive_rescan);
		LOAD_INT(cdda_add_to_playlist);
		LOAD_INT(cdda_remove_from_playlist);
		LOAD_STR(cddb_server);
		LOAD_INT(cddb_timeout);
		LOAD_INT(cddb_use_http);
		LOAD_INT(inet_use_proxy);
		LOAD_STR(inet_proxy);
		LOAD_INT(inet_proxy_port);
		LOAD_STR(inet_noproxy_domains);
		LOAD_INT(inet_timeout);
		LOAD_FLOAT(loop_range_start);
		LOAD_FLOAT(loop_range_end);
		LOAD_INT(wm_systray_warn);
		LOAD_INT(podcasts_autocheck);
		LOAD_INT(cdrip_deststore);
		LOAD_INT(cdrip_file_format);
		LOAD_INT(cdrip_bitrate);
		LOAD_INT(cdrip_vbr);
		LOAD_INT(cdrip_metadata);
		LOAD_STR(export_template);
		LOAD_INT(export_subdir_artist);
		LOAD_INT(export_subdir_album);
		LOAD_INT(export_subdir_limit);
		LOAD_INT(export_file_format);
		LOAD_INT(export_bitrate);
		LOAD_INT(export_vbr);
		LOAD_INT(export_metadata);
		LOAD_INT(export_filter_same);
		LOAD_INT(export_excl_enabled);
		LOAD_STR(export_excl_pattern);
		LOAD_INT(batch_tag_flags);
		LOAD_STR(ext_title_format_file);

                if ((!xmlStrcmp(cur->name, (const xmlChar *)"music_store"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {

				char path[MAXLEN];
				char * ppath;

				arr_snprintf(path, "%s", (char *)key);
				ppath = g_filename_from_utf8(path, -1, NULL, NULL, NULL);

				append_ms_pathlist(ppath, path);

				g_free(ppath);
			}

                        xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "systray"))) {
			load_systray_options(doc, cur);
#ifdef HAVE_JACK
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "jack_connections"))) {
			load_jack_connections(doc, cur);
#endif /* HAVE_JACK */
		}

		cur = cur->next;
        }

	{
		char buf[MAXLEN];
		if (!options.title_format[0] ||
		    make_string_va(buf, CHAR_ARRAY_SIZE(buf), options.title_format,
				   'a', "a", 'r', "r", 't', "t", 0) != 0) {
			arr_strlcpy(options.title_format, "%a?ar|at{ :: }%r?rt{ :: }%t");
		}
	}

	setup_extended_title_formatting();
	xmlFreeDoc(doc);
	return;
}

void
save_systray_options(xmlDocPtr doc, xmlNodePtr root) {

	xmlNodePtr cur;
	char str[MAXLEN];
	int i;

	if (root == NULL)
		return;

	cur = xmlNewNode(NULL, (const xmlChar *) "systray");
	xmlAddChild(root, cur);
	root = cur;

	SAVE_INT(systray_mouse_wheel_horizontal);
	SAVE_INT(systray_mouse_wheel_vertical);

	for (i = 0; i < options.systray_mouse_buttons_count; i++) {

		cur = xmlNewNode(NULL, (const xmlChar *) "mouse_button");
		xmlAddChild(root, cur);

		arr_snprintf(str, "%d", options.systray_mouse_buttons[i].button_nr);
		xmlSetProp(cur, (const xmlChar *) "number", (const xmlChar *) str);

		arr_snprintf(str, "%d", options.systray_mouse_buttons[i].command);
		xmlSetProp(cur, (const xmlChar *) "command", (const xmlChar *) str);
	}
}

void
load_systray_options(xmlDocPtr doc, xmlNodePtr cur) {

	xmlChar * key;
	int button_nr, command;

	if (cur == NULL)
		return;

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {

		LOAD_INT(systray_mouse_wheel_horizontal);
		LOAD_INT(systray_mouse_wheel_vertical);

		if ((!xmlStrcmp(cur->name, (const xmlChar *) "mouse_button"))) {

			key = xmlGetProp(cur, (const xmlChar *) "number");
			if (key != NULL) {
				sscanf((char *) key, "%d", &button_nr);
				xmlFree(key);

				key = xmlGetProp(cur, (const xmlChar *) "command");
				if (key != NULL) {
					sscanf((char *) key, "%d", &command);
					xmlFree(key);

					// Disallow Left and Right mouse button (reserved).
					if (button_nr != 1 && button_nr != 3 &&
					    command >= 0 && command < SYSTRAY_MB_CMD_LAST) {

						++options.systray_mouse_buttons_count;
						options.systray_mouse_buttons = realloc(options.systray_mouse_buttons,
											options.systray_mouse_buttons_count * sizeof(mouse_button_command_t));
						options.systray_mouse_buttons[options.systray_mouse_buttons_count-1].button_nr = button_nr;
						options.systray_mouse_buttons[options.systray_mouse_buttons_count-1].command = command;
					}
				}
			}
		}

		cur = cur->next;
	}
}

void
finalize_options(void) {

	options.systray_mouse_buttons_count = 0;
	free(options.systray_mouse_buttons);
	options.systray_mouse_buttons = NULL;
}

#ifdef HAVE_JACK
void
save_jack_connections(xmlDocPtr doc, xmlNodePtr root) {

	xmlNodePtr port_list_node;
	xmlNodePtr cur;
	const char ** ports;

	if (root == NULL)
		return;

	cur = xmlNewNode(NULL, (const xmlChar *) "jack_connections");
	xmlAddChild(root, cur);
	root = cur;

	port_list_node = xmlNewNode(NULL, (const xmlChar *) "out_L");
	xmlAddChild(root, port_list_node);

	if (out_L_port && (ports = jack_port_get_connections(out_L_port))) {
		int i = 0;
		while (ports[i] != NULL) {
			cur = xmlNewNode(NULL, (const xmlChar *) "port");
			xmlAddChild(port_list_node, cur);
			xmlSetProp(cur, (const xmlChar *) "label", (const xmlChar *) ports[i]);
			i++;
		}
		free(ports);
	}

	port_list_node = xmlNewNode(NULL, (const xmlChar *) "out_R");
	xmlAddChild(root, port_list_node);

	if (out_R_port && (ports = jack_port_get_connections(out_R_port))) {
		int i = 0;
		while (ports[i] != NULL) {
			cur = xmlNewNode(NULL, (const xmlChar *) "port");
			xmlAddChild(port_list_node, cur);
			xmlSetProp(cur, (const xmlChar *) "label", (const xmlChar *) ports[i]);
			i++;
		}
		free(ports);
	}
}

void
load_jack_connections(xmlDocPtr doc, xmlNodePtr cur) {

	xmlChar * key;
	xmlNodePtr port_list_node;

	if (cur == NULL)
		return;

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *) "out_L"))) {
			port_list_node = cur->xmlChildrenNode;
			while (port_list_node != NULL) {
				key = xmlGetProp(port_list_node, (const xmlChar *) "label");
				if (key != NULL) {
					saved_pconns_L = g_slist_prepend(saved_pconns_L, strdup((const char *)key));
					xmlFree(key);
				}
				port_list_node = port_list_node->next;
			}
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *) "out_R"))) {
			port_list_node = cur->xmlChildrenNode;
			while (port_list_node != NULL) {
				key = xmlGetProp(port_list_node, (const xmlChar *) "label");
				if (key != NULL) {
					saved_pconns_R = g_slist_prepend(saved_pconns_R, strdup((const char *)key));
					xmlFree(key);
				}
				port_list_node = port_list_node->next;
			}
		}
		cur = cur->next;
	}
	saved_pconns_L = g_slist_reverse(saved_pconns_L);
	saved_pconns_R = g_slist_reverse(saved_pconns_R);
}
#endif /* HAVE_JACK */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :

