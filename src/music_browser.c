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
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "common.h"
#include "build_store.h"
#include "cddb_lookup.h"
#include "core.h"
#include "cover.h"
#include "file_info.h"
#include "decoder/file_decoder.h"
#include "meta_decoder.h"
#include "gui_main.h"
#include "options.h"
#include "volume.h"
#include "playlist.h"
#include "search.h"
#include "cdda.h"
#include "i18n.h"
#include "music_browser.h"


extern options_t options;

extern volatile int build_thread_state;

#ifdef HAVE_CDDB
extern volatile int cddb_thread_state;
#endif /* HAVE_CDDB */

extern GtkWidget * vol_window;
extern GtkWidget * play_list;
extern GtkWidget * musicstore_toggle;

extern GtkTreeStore * play_store;
extern GtkListStore * ms_pathlist_store;

extern PangoFontDescription *fd_browser;
extern PangoFontDescription *fd_statusbar;

extern char pl_color_active[14];
extern char pl_color_inactive[14];

extern GtkWidget * gui_stock_label_button(gchar * label, const gchar * stock);
extern void assign_audio_fc_filters(GtkFileChooser * fc);

extern void iw_show_info (GtkWidget *dialog, GtkWindow *transient, gchar *message);
extern void iw_hide_info (void);

extern GtkTooltips * aqualung_tooltips;

GtkWidget * browser_window;
GtkWidget * dialog;
gint browser_pos_x;
gint browser_pos_y;
gint browser_size_x;
gint browser_size_y;
gint browser_on;
gint browser_paned_pos;

gint music_store_changed = 0;

GtkWidget * music_tree;
GtkTreeStore * music_store = NULL;
GtkTreeSelection * music_select;

GtkWidget * comment_view;
GtkWidget * browser_paned;
GtkWidget * statusbar_ms;

/* popup menus for tree items */
GtkWidget * store_menu;
GtkWidget * store__addlist;
GtkWidget * store__addlist_albummode;
GtkWidget * store__separator1;
GtkWidget * store__add;
GtkWidget * store__build;
GtkWidget * store__edit;
GtkWidget * store__save;
GtkWidget * store__remove;
GtkWidget * store__separator2;
GtkWidget * store__addart;
GtkWidget * store__separator3;
GtkWidget * store__volume;
GtkWidget * store__volume_menu;
GtkWidget * store__volume_unmeasured;
GtkWidget * store__volume_all;
GtkWidget * store__search;

GtkWidget * artist_menu;
GtkWidget * artist__addlist;
GtkWidget * artist__addlist_albummode;
GtkWidget * artist__separator1;
GtkWidget * artist__add;
GtkWidget * artist__build;
GtkWidget * artist__edit;
GtkWidget * artist__remove;
GtkWidget * artist__separator2;
GtkWidget * artist__addrec;
GtkWidget * artist__separator3;
GtkWidget * artist__volume;
GtkWidget * artist__volume_menu;
GtkWidget * artist__volume_unmeasured;
GtkWidget * artist__volume_all;
GtkWidget * artist__search;

GtkWidget * record_menu;
GtkWidget * record__addlist;
GtkWidget * record__addlist_albummode;
GtkWidget * record__separator1;
GtkWidget * record__add;
GtkWidget * record__edit;
GtkWidget * record__remove;
GtkWidget * record__separator2;
GtkWidget * record__addtrk;
#ifdef HAVE_CDDB
GtkWidget * record__cddb;
GtkWidget * record__cddb_submit;
#endif /* HAVE_CDDB */
GtkWidget * record__separator3;
GtkWidget * record__volume;
GtkWidget * record__volume_menu;
GtkWidget * record__volume_unmeasured;
GtkWidget * record__volume_all;
GtkWidget * record__search;

GtkWidget * track_menu;
GtkWidget * track__addlist;
GtkWidget * track__separator1;
GtkWidget * track__add;
GtkWidget * track__edit;
GtkWidget * track__remove;
GtkWidget * track__separator2;
GtkWidget * track__fileinfo;
GtkWidget * track__separator3;
GtkWidget * track__volume;
GtkWidget * track__volume_menu;
GtkWidget * track__volume_unmeasured;
GtkWidget * track__volume_all;
GtkWidget * track__search;

#ifdef HAVE_CDDA
GtkWidget * cdda_track_menu;
GtkWidget * cdda_track__addlist;

GtkWidget * cdda_record_menu;
GtkWidget * cdda_record__addlist;
GtkWidget * cdda_record__addlist_albummode;
GtkWidget * cdda_record__separator1;
#ifdef HAVE_CDDB
GtkWidget * cdda_record__cddb;
GtkWidget * cdda_record__cddb_submit;
#endif /* HAVE_CDDB */
GtkWidget * cdda_record__rip;
GtkWidget * cdda_record__separator2;
GtkWidget * cdda_record__disc_info;
GtkWidget * cdda_record__drive_info;

GtkWidget * cdda_store_menu;
GtkWidget * cdda_store__addlist;
GtkWidget * cdda_store__addlist_albummode;
#endif /* HAVE_CDDA */

GtkWidget * blank_menu;
GtkWidget * blank__add;
GtkWidget * blank__search;
GtkWidget * blank__save;

GtkWidget * toolbar_save_button;
GtkWidget * toolbar_edit_button;
GtkWidget * toolbar_add_button;
GtkWidget * toolbar_remove_button;

GtkWidget * name_entry;
GtkWidget * sort_name_entry;

GdkPixbuf * icon_artist;
GdkPixbuf * icon_record;
GdkPixbuf * icon_store;
GdkPixbuf * icon_track;
GdkPixbuf * icon_cdda;
GdkPixbuf * icon_cdda_disc;
GdkPixbuf * icon_cdda_nodisc;

#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
GtkWidget * store__tag;
GtkWidget * artist__tag;
GtkWidget * record__tag;
GtkWidget * track__tag;

typedef struct _batch_tag_t {

	char filename[MAXLEN];
	char title[MAXLEN];
	char artist[MAXLEN];
	char album[MAXLEN];
	char comment[MAXLEN];
	char year[MAXLEN];
	char track[MAXLEN];

	struct _batch_tag_t * next;

} batch_tag_t;

batch_tag_t * batch_tag_root = NULL;
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */


/* prototypes, when we need them */

gboolean music_tree_event_cb(GtkWidget * widget, GdkEvent * event);

void store__add_cb(gpointer data);
void store__build_cb(gpointer data);
void store__edit_cb(gpointer data);
void store__save_cb(gpointer data);
void store__volume_unmeasured_cb(gpointer data);
void store__volume_all_cb(gpointer data);
void store__remove_cb(gpointer data);

void artist__add_cb(gpointer data);
void artist__build_cb(gpointer data);
void artist__edit_cb(gpointer data);
void artist__volume_unmeasured_cb(gpointer data);
void artist__volume_all_cb(gpointer data);
void artist__remove_cb(gpointer data);

void record__add_cb(gpointer data);
void record__edit_cb(gpointer data);
void record__volume_unmeasured_cb(gpointer data);
void record__volume_all_cb(gpointer data);
void record__remove_cb(gpointer data);

void track__add_cb(gpointer data);
void track__edit_cb(gpointer data);
void track__volume_unmeasured_cb(gpointer data);
void track__volume_all_cb(gpointer data);
void track__remove_cb(gpointer data);
void track__fileinfo_cb(gpointer data);

void search_cb(gpointer data);
void collapse_all_items_cb(gpointer data);

struct keybinds {
	void (*callback)(gpointer);
	int keyval1;
	int keyval2;
};

struct keybinds blank_keybinds[] = {
        {store__add_cb, GDK_n, GDK_N},
        {store__build_cb, GDK_b, GDK_B},
        {search_cb, GDK_f, GDK_F},
        {collapse_all_items_cb, GDK_w, GDK_W},
        {NULL, 0}
};

struct keybinds store_keybinds[] = {
	{store__addlist_defmode, GDK_a, GDK_A},
	{store__add_cb, GDK_n, GDK_N},
	{store__build_cb, GDK_b, GDK_B},
	{store__edit_cb, GDK_e, GDK_E},
	{store__save_cb, GDK_s, GDK_S},
	{store__volume_unmeasured_cb, GDK_v, GDK_V},
	{store__remove_cb, GDK_Delete, GDK_KP_Delete},
	{artist__add_cb, GDK_plus, GDK_KP_Add},
	{search_cb, GDK_f, GDK_F},
        {collapse_all_items_cb, GDK_w, GDK_W},
	{NULL, 0}
};

struct keybinds artist_keybinds[] = {
	{artist__addlist_defmode, GDK_a, GDK_A},
	{artist__add_cb, GDK_n, GDK_N},
	{artist__build_cb, GDK_b, GDK_B},
	{artist__edit_cb, GDK_e, GDK_E},
	{artist__volume_unmeasured_cb, GDK_v, GDK_V},
	{artist__remove_cb, GDK_Delete, GDK_KP_Delete},
	{record__add_cb, GDK_plus, GDK_KP_Add},
	{search_cb, GDK_f, GDK_F},
        {collapse_all_items_cb, GDK_w, GDK_W},
	{NULL, 0}
};

struct keybinds record_keybinds[] = {
	{record__addlist_defmode, GDK_a, GDK_A},
	{record__add_cb, GDK_n, GDK_N},
	{record__edit_cb, GDK_e, GDK_E},
	{record__volume_unmeasured_cb, GDK_v, GDK_V},
	{record__remove_cb, GDK_Delete, GDK_KP_Delete},
	{track__add_cb, GDK_plus, GDK_KP_Add},
	{search_cb, GDK_f, GDK_F},
        {collapse_all_items_cb, GDK_w, GDK_W},
	{NULL, 0}
};

struct keybinds track_keybinds[] = {
	{track__addlist_cb, GDK_a, GDK_A},
	{track__add_cb, GDK_n, GDK_N},
	{track__edit_cb, GDK_e, GDK_E},
	{track__volume_unmeasured_cb, GDK_v, GDK_V},
	{track__remove_cb, GDK_Delete, GDK_KP_Delete},
	{track__fileinfo_cb, GDK_i, GDK_I},
	{search_cb, GDK_f, GDK_F},
        {collapse_all_items_cb, GDK_w, GDK_W},
	{NULL, 0}
};

#ifdef HAVE_CDDA
struct keybinds cdda_store_keybinds[] = {
	{artist__addlist_defmode, GDK_a, GDK_A},
        {collapse_all_items_cb, GDK_w, GDK_W},
	{NULL, 0}
};

struct keybinds cdda_record_keybinds[] = {
	{record__addlist_defmode, GDK_a, GDK_A},
        {collapse_all_items_cb, GDK_w, GDK_W},
	{NULL, 0}
};

struct keybinds cdda_track_keybinds[] = {
	{track__addlist_cb, GDK_a, GDK_A},
        {collapse_all_items_cb, GDK_w, GDK_W},
	{NULL, 0}
};
#endif /* HAVE_CDDA */



gboolean
browser_window_close(GtkWidget * widget, GdkEvent * event, gpointer data) {

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(musicstore_toggle), FALSE);

	return TRUE;
}

gint
browser_window_key_pressed(GtkWidget * widget, GdkEventKey * event) {

	switch (event->keyval) {
	case GDK_q:
	case GDK_Q:
	case GDK_Escape:
		browser_window_close(NULL, NULL, NULL);
		return TRUE;
		break;
	}

	return FALSE;
}


void
make_title_string(char * dest, char * templ, char * artist, char * record, char * track) {

	int i;
	int j;
	int argc = 0;
	char * arg[3] = { "", "", "" };
	char temp[MAXLEN];

	temp[0] = templ[0];

	for (i = j = 1; i < strlen(templ) && j < MAXLEN - 1; i++, j++) {
		if (templ[i - 1] == '%') {
			if (argc < 3) {
				switch (templ[i]) {
				case 'a':
					arg[argc++] = artist;
					temp[j] = 's';
					break;
				case 'r':
					arg[argc++] = record;
					temp[j] = 's';
					break;
				case 't':
					arg[argc++] = track;
					temp[j] = 's';
					break;
				default:
					temp[j++] = '%';
					temp[j] = templ[i];
					break;
				}
			} else {
				temp[j++] = '%';
				temp[j] = templ[i];
			}
		} else {
			temp[j] = templ[i];
		}
	}

	temp[j] = '\0';
	
	snprintf(dest, MAXLEN - 1, temp, arg[0], arg[1], arg[2]);
}


int
browse_button_store_clicked(GtkWidget * widget, gpointer * data) {

        GtkWidget * dialog;
	const gchar * selected_filename = gtk_entry_get_text(GTK_ENTRY(data));

        dialog = gtk_file_chooser_dialog_new(_("Please select the xml file for this store."),
                                             GTK_WINDOW(browser_window),
                                             GTK_FILE_CHOOSER_ACTION_SAVE,
                                             GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        deflicker();

        if (strlen(selected_filename)) {
		char * locale = g_locale_from_utf8(selected_filename, -1, NULL, NULL, NULL);
		char tmp[MAXLEN];
		tmp[0] = '\0';

		if (locale == NULL) {
			gtk_widget_destroy(dialog);
			return 0;
		}

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

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);


        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * locale;

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		locale = g_locale_from_utf8(selected_filename, -1, NULL, NULL, NULL);

		if (locale == NULL) {
			gtk_widget_destroy(dialog);
			return 0;
		}

		gtk_entry_set_text(GTK_ENTRY(data), selected_filename);

                strncpy(options.currdir, locale, MAXLEN-1);
		g_free(locale);
        }

        gtk_widget_destroy(dialog);

	return 0;
}


int
add_store_dialog(char * name, char * file, char * comment) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * name_label;
	GtkWidget * hbox2;
	GtkWidget * file_entry;
	GtkWidget * file_label;
	GtkWidget * browse_button;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Create empty store"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

        gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, 300);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, TRUE, 2);

	file_label = gtk_label_new(_("Filename:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), file_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox2, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

        file_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(file_entry), MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(file_entry), "~/music_store.xml");
        gtk_box_pack_start(GTK_BOX(hbox2), file_entry, TRUE, TRUE, 0);


	name_label = gtk_label_new(_("Name:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 5);

        name_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(name_entry), MAXLEN - 1);
	gtk_table_attach(GTK_TABLE(table), name_entry, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);


	browse_button = gui_stock_label_button(_("_Browse..."), GTK_STOCK_OPEN);
        gtk_box_pack_start(GTK_BOX(hbox2), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked",
			 G_CALLBACK(browse_button_store_clicked), (gpointer *)file_entry);
	

	comment_label = gtk_label_new(_("Comments:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), comment_label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 2);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);

        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(file_entry);

	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	file[0] = '\0';
	comment[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		const char * pfile = g_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(file_entry)), -1, NULL, NULL, NULL);

		if (pfile == NULL) {
			gtk_widget_grab_focus(file_entry);
			goto display;
		}

		if (pfile[0] == '~') {
			snprintf(file, MAXLEN-1, "%s%s", options.home, pfile + 1);
		} else if (pfile[0] == '/') {
			strncpy(file, pfile, MAXLEN-1);
		} else if (pfile[0] != '\0') {
			snprintf(file, MAXLEN-1, "%s/%s", options.cwd, pfile);
		}

                strncpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)), MAXLEN-1);

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));

		if ((name[0] == '\0') || (file[0] == '\0')) {
			goto display;
		}

		ret = 1;

        } else {
                name[0] = '\0';
                file[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
edit_store_dialog(char * name, char * file, char * comment) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * name_label;
	GtkWidget * file_entry;
	GtkWidget * file_label;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

	char * utf8;
        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Edit Store"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 350, 250);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, TRUE, 2);

	name_label = gtk_label_new(_("Name:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

        name_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
	gtk_table_attach(GTK_TABLE(table), name_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	file_label = gtk_label_new(_("Filename:"));
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), file_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 5);

	file_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(file_entry), MAXLEN - 1);

	utf8 = g_locale_to_utf8(file, -1, NULL, NULL, NULL);
	gtk_entry_set_text(GTK_ENTRY(file_entry), utf8);
	g_free(utf8);

	gtk_editable_set_editable(GTK_EDITABLE(file_entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), file_entry, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	comment_label = gtk_label_new(_("Comments:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), comment_label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 2);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);


        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	comment[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));
		if (name[0] == '\0') {
			goto display;
		}

		ret = 1;

        } else {
                name[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}

int add_entry_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Return) {

                gtk_entry_set_text(GTK_ENTRY(sort_name_entry), gtk_entry_get_text(GTK_ENTRY(name_entry)));
	        gtk_widget_grab_focus(sort_name_entry);
        } 
        
	return FALSE;
}


int
add_artist_dialog(char * name, char * sort_name, char * comment) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * name_label;
	GtkWidget * sort_name_label;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Add Artist"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 350, 250);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, TRUE, 2);

	name_label = gtk_label_new(_("Visible name:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

        name_entry = gtk_entry_new();
        g_signal_connect (G_OBJECT(name_entry), "key_press_event", G_CALLBACK(add_entry_key_press), NULL);
        gtk_entry_set_max_length(GTK_ENTRY(name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
	gtk_table_attach(GTK_TABLE(table), name_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	sort_name_label = gtk_label_new(_("Name to sort by:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), sort_name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 5);

        sort_name_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(sort_name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(sort_name_entry), sort_name);
	gtk_table_attach(GTK_TABLE(table), sort_name_entry, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	comment_label = gtk_label_new(_("Comments:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), comment_label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 2);


	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);

        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
 
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
                strcpy(sort_name, gtk_entry_get_text(GTK_ENTRY(sort_name_entry)));

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));
		if (name[0] == '\0') {
			goto display;
		}

		ret = 1;

        } else {
                name[0] = '\0';
                sort_name[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
edit_artist_dialog(char * name, char * sort_name, char * comment) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * name_label;
	GtkWidget * sort_name_label;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Edit Artist"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 350, 250);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, TRUE, 2);

	name_label = gtk_label_new(_("Visible name:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

        name_entry = gtk_entry_new();
        g_signal_connect (G_OBJECT(name_entry), "key_press_event", G_CALLBACK(add_entry_key_press), NULL);
        gtk_entry_set_max_length(GTK_ENTRY(name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
	gtk_table_attach(GTK_TABLE(table), name_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	sort_name_label = gtk_label_new(_("Name to sort by:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), sort_name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 5);

        sort_name_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(sort_name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(sort_name_entry), sort_name);
	gtk_table_attach(GTK_TABLE(table), sort_name_entry, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	comment_label = gtk_label_new(_("Comments:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), comment_label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 2);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);


        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
                strcpy(sort_name, gtk_entry_get_text(GTK_ENTRY(sort_name_entry)));

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));
		if (name[0] == '\0') {
			goto display;
		}

		ret = 1;

        } else {
                name[0] = '\0';
                sort_name[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
browse_button_record_clicked(GtkWidget * widget, gpointer * data) {

        GtkWidget * dialog;
        const gchar * selected_filename;
	GtkListStore * model = (GtkListStore *)data;
	GtkTreeIter iter;
        GSList *lfiles, *node;


        dialog = gtk_file_chooser_dialog_new(_("Please select the audio files for this record."), 
						    NULL,
						    GTK_FILE_CHOOSER_ACTION_OPEN,
						    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
						    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						    NULL);

        deflicker();

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        assign_audio_fc_filters(GTK_FILE_CHOOSER(dialog));


        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                strncpy(options.currdir, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
                                                                         MAXLEN-1);

                lfiles = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));

                node = lfiles;

                while (node) {

                        selected_filename = (char *) node->data;

			if (selected_filename[strlen(selected_filename)-1] != '/') {

				char * utf8 = g_locale_to_utf8(selected_filename, -1,
							       NULL, NULL, NULL);
				gtk_list_store_append(model, &iter);
				gtk_list_store_set(model, &iter, 0, utf8, -1);
				g_free(utf8);
			}

                        g_free(node->data);

                        node = node->next;
                }

                g_slist_free(lfiles);
        }
                                                         
        gtk_widget_destroy(dialog);

	return 0;
}


void
clicked_tracklist_header(GtkWidget * widget, gpointer * data) {

	GtkListStore * model = (GtkListStore *)data;

	gtk_list_store_clear(model);
}


int
add_record_dialog(char * name, char * sort_name, char *** strings, char * comment) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * name_label;
	GtkWidget * sort_name_label;
	GtkWidget * list_label;

	GtkWidget * viewport1;
	GtkWidget * scrolled_win;
	GtkWidget * tracklist_tree;
	GtkListStore * model;
	GtkCellRenderer * cell;
	GtkTreeViewColumn * column;
	GtkTreeIter iter;
	gchar * str;

	GtkWidget * browse_button;
	GtkWidget * viewport2;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;
	int n, i;

        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Add Record"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, 400);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, TRUE, 2);

	name_label = gtk_label_new(_("Visible name:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

        name_entry = gtk_entry_new();
        g_signal_connect (G_OBJECT(name_entry), "key_press_event", G_CALLBACK(add_entry_key_press), NULL);
        gtk_entry_set_max_length(GTK_ENTRY(name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
	gtk_table_attach(GTK_TABLE(table), name_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	sort_name_label = gtk_label_new(_("Name to sort by:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), sort_name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 5);

        sort_name_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(sort_name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(sort_name_entry), sort_name);
	gtk_table_attach(GTK_TABLE(table), sort_name_entry, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);


	list_label = gtk_label_new(_("Auto-create tracks from these files:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), list_label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 2);

	viewport1 = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport1, TRUE, TRUE, 2);

	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport1), scrolled_win);

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
        g_signal_connect(G_OBJECT(column->button), "clicked", G_CALLBACK(clicked_tracklist_header),
			 (gpointer *)model);
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), 0, GTK_SORT_ASCENDING);

	browse_button = gui_stock_label_button(_("_Add files..."), GTK_STOCK_ADD);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked", G_CALLBACK(browse_button_record_clicked),
			 (gpointer *)model);

	comment_label = gtk_label_new(_("Comments:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), comment_label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 2);


	viewport2 = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport2, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport2), scrolled_window);

        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
                strcpy(sort_name, gtk_entry_get_text(GTK_ENTRY(sort_name_entry)));

		n = 0;
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter)) {
			/* count the number of list items */
			n = 1;
			while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter)) {
				n++;
			}
		}
		if ((n) && (name[0] != '\0')) {

			gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter);
			if (!(*strings = calloc(n + 1, sizeof(char *)))) {
				fprintf(stderr, "add_record_dialog(): calloc error\n");
				exit(1);
			}
			for (i = 0; i < n; i++) {

				char * locale;

				gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 0, &str, -1);
				gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);

				locale = g_locale_from_utf8(str, -1, NULL, NULL, NULL);

				if (!((*strings)[i] = calloc(strlen(locale)+1, sizeof(char)))) {
					fprintf(stderr, "add_record_dialog(): calloc error\n");
					exit(1);
				}

				strcpy((*strings)[i], locale);
				g_free(str);
				g_free(locale);
			}
			(*strings)[i] = NULL;
		}

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));

		if (name[0] == '\0') {
			goto display;
		}

		ret = 1;
        } else {
                name[0] = '\0';
                sort_name[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
edit_record_dialog(char * name, char * sort_name, char * comment) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * name_label;
	GtkWidget * sort_name_label;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Edit Record"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 350, 250);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, TRUE, 2);

	name_label = gtk_label_new(_("Visible name:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

        name_entry = gtk_entry_new();
        g_signal_connect (G_OBJECT(name_entry), "key_press_event", G_CALLBACK(add_entry_key_press), NULL);
        gtk_entry_set_max_length(GTK_ENTRY(name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
	gtk_table_attach(GTK_TABLE(table), name_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	sort_name_label = gtk_label_new(_("Name to sort by:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), sort_name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 5);

        sort_name_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(sort_name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(sort_name_entry), sort_name);
	gtk_table_attach(GTK_TABLE(table), sort_name_entry, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	comment_label = gtk_label_new(_("Comments:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), comment_label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 2);


	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);

        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
                strcpy(sort_name, gtk_entry_get_text(GTK_ENTRY(sort_name_entry)));

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));
		if (name[0] == '\0') {
			goto display;
		}

		ret = 1;

        } else {
                name[0] = '\0';
                sort_name[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
browse_button_track_clicked(GtkWidget * widget, gpointer * data) {

        GtkWidget * dialog;
	const gchar * selected_filename = gtk_entry_get_text(GTK_ENTRY(data));

        dialog = gtk_file_chooser_dialog_new(_("Please select the audio file for this track."), 
                                             GTK_WINDOW(browser_window), 
                                             GTK_FILE_CHOOSER_ACTION_OPEN, 
                                             GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, 
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                             NULL);

        deflicker();

        if (strlen(selected_filename)) {
		char * locale = g_locale_from_utf8(selected_filename, -1, NULL, NULL, NULL);
		char tmp[MAXLEN];
		tmp[0] = '\0';

		if (locale == NULL) {
			gtk_widget_destroy(dialog);
			return 0;
		}

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

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
        assign_audio_fc_filters(GTK_FILE_CHOOSER(dialog));

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * utf8;

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		utf8 = g_locale_to_utf8(selected_filename, -1, NULL, NULL, NULL);

		if (utf8 == NULL) {
			gtk_widget_destroy(dialog);
			return 0;
		}

		gtk_entry_set_text(GTK_ENTRY(data), utf8);

                strncpy(options.currdir, selected_filename, MAXLEN-1);
		g_free(utf8);
	}

        gtk_widget_destroy(dialog);

	return 0;
}


int
add_track_dialog(char * name, char * sort_name, char * file, char * comment) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * name_label;
	GtkWidget * sort_name_label;
	GtkWidget * hbox2;
	GtkWidget * file_entry;
	GtkWidget * file_label;
	GtkWidget * browse_button;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;

        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Add Track"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, 300);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	table = gtk_table_new(3, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, TRUE, 2);

	name_label = gtk_label_new(_("Visible name:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

        name_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
	gtk_table_attach(GTK_TABLE(table), name_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	sort_name_label = gtk_label_new(_("Name to sort by:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), sort_name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 5);

        sort_name_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(sort_name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(sort_name_entry), sort_name);
	gtk_table_attach(GTK_TABLE(table), sort_name_entry, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	file_label = gtk_label_new(_("Filename:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), file_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 5);

	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox2, 1, 2, 2, 3,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

        file_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(file_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(file_entry), file);
        gtk_box_pack_start(GTK_BOX(hbox2), file_entry, TRUE, TRUE, 0);


	browse_button = gui_stock_label_button(_("_Browse..."), GTK_STOCK_OPEN);
        gtk_box_pack_start(GTK_BOX(hbox2), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked", G_CALLBACK(browse_button_track_clicked),
			 (gpointer *)file_entry);
	

	comment_label = gtk_label_new(_("Comments:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), comment_label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 2);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);

        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	sort_name[0] = '\0';
	file[0] = '\0';
	comment[0] = '\0';
		
        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * pfile = g_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(file_entry)), -1, NULL, NULL, NULL);

		if (pfile == NULL) {
			gtk_widget_grab_focus(file_entry);
			goto display;
		}

                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
                strcpy(sort_name, gtk_entry_get_text(GTK_ENTRY(sort_name_entry)));
                strcpy(file, pfile);

		g_free(pfile);

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));

		if ((name[0] == '\0') || (file[0] == '\0')) {
			goto display;
		}

		ret = 1;

        } else {
                name[0] = '\0';
                sort_name[0] = '\0';
                file[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


void
use_rva_check_button_cb(GtkWidget * widget, gpointer data) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		gtk_widget_set_sensitive((GtkWidget *)data, TRUE);
    	} else {
		gtk_widget_set_sensitive((GtkWidget *)data, FALSE);
	}
}


int
edit_track_dialog(char * name, char * sort_name, char * file, char * comment,
		  float duration, float volume, float * rva, float * use_rva) {

	GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * hbox;
	GtkWidget * name_label;
	GtkWidget * sort_name_label;
	GtkWidget * hbox2;
	GtkWidget * file_entry;
	GtkWidget * file_label;
	GtkWidget * browse_button;
	GtkWidget * viewport;
        GtkWidget * scrolled_window;
	GtkWidget * comment_label;
	GtkWidget * comment_view;
        GtkTextBuffer * buffer;
	GtkTextIter iter_start;
	GtkTextIter iter_end;
	GtkWidget * table2;
	GtkWidget * duration_label;
	GtkWidget * duration_hbox;
	GtkWidget * duration_entry;
	GtkWidget * volume_label;
	GtkWidget * volume_hbox;
	GtkWidget * volume_entry;
	GtkWidget * check_button;
	GtkObject * adj_manual_rva;
	GtkWidget * spin_button;

	char str[MAXLEN];
	char * utf8;
        int ret;

        dialog = gtk_dialog_new_with_buttons(_("Edit Track"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);
        gtk_widget_set_size_request(GTK_WIDGET(dialog), 400, 400);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);

	table = gtk_table_new(3, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, TRUE, 2);

	name_label = gtk_label_new(_("Visible name:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 5);

        name_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(name_entry), name);
	gtk_table_attach(GTK_TABLE(table), name_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	sort_name_label = gtk_label_new(_("Name to sort by:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), sort_name_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 5);

        sort_name_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(sort_name_entry), MAXLEN - 1);
        gtk_entry_set_text(GTK_ENTRY(sort_name_entry), sort_name);
	gtk_table_attach(GTK_TABLE(table), sort_name_entry, 1, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	file_label = gtk_label_new(_("Filename:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), file_label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 5);


	hbox2 = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox2, 1, 2, 2, 3,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

        file_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(file_entry), MAXLEN - 1);

	utf8 = g_locale_to_utf8(file, -1, NULL, NULL, NULL);
        gtk_entry_set_text(GTK_ENTRY(file_entry), utf8);
	g_free(utf8);

        gtk_box_pack_start(GTK_BOX(hbox2), file_entry, TRUE, TRUE, 0);


	browse_button = gui_stock_label_button(_("_Browse..."), GTK_STOCK_OPEN);
        gtk_box_pack_start(GTK_BOX(hbox2), browse_button, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(browse_button), "clicked",
			 G_CALLBACK(browse_button_track_clicked), (gpointer *)file_entry);
	

	comment_label = gtk_label_new(_("Comments:"));
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), comment_label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 2);

	viewport = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), viewport, TRUE, TRUE, 2);

        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_window);

        comment_view = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), TRUE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), comment, -1);
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
        gtk_container_add(GTK_CONTAINER(scrolled_window), comment_view);


        table = gtk_table_new(3, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, FALSE, 4);

	duration_label = gtk_label_new(_("Duration:"));
        duration_hbox = gtk_hbox_new(FALSE, 3);
        gtk_box_pack_start(GTK_BOX(duration_hbox), duration_label, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table), duration_hbox, 0, 1, 0, 1,
                         GTK_FILL, GTK_EXPAND | GTK_FILL, 5, 3);

        duration_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(duration_entry), MAXLEN - 1);
	time2time(duration, str);
        gtk_entry_set_text(GTK_ENTRY(duration_entry), str);
        gtk_editable_set_editable(GTK_EDITABLE(duration_entry), FALSE);
        gtk_table_attach(GTK_TABLE(table), duration_entry, 1, 2, 0, 1,
                         GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 3);


	volume_label = gtk_label_new(_("Volume level:"));
        volume_hbox = gtk_hbox_new(FALSE, 3);
        gtk_box_pack_start(GTK_BOX(volume_hbox), volume_label, FALSE, FALSE, 0);
        gtk_table_attach(GTK_TABLE(table), volume_hbox, 0, 1, 1, 2,
                         GTK_FILL, GTK_EXPAND | GTK_FILL, 5, 3);

        volume_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(volume_entry), MAXLEN - 1);

	if (volume <= 0.1f) {
		snprintf(str, MAXLEN-1, "%.1f dBFS", volume);
	} else {
		snprintf(str, MAXLEN-1, _("Unmeasured"));
	}

        gtk_entry_set_text(GTK_ENTRY(volume_entry), str);
        gtk_editable_set_editable(GTK_EDITABLE(volume_entry), FALSE);
        gtk_table_attach(GTK_TABLE(table), volume_entry, 1, 2, 1, 2,
                         GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 3);


        table2 = gtk_table_new(1, 2, FALSE);
        gtk_table_attach(GTK_TABLE(table), table2, 0, 2, 2, 3,
                         GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	check_button = gtk_check_button_new_with_label(_("Use manual RVA value [dB]"));
	gtk_widget_set_name(check_button, "check_on_window");
        gtk_table_attach(GTK_TABLE(table2), check_button, 0, 1, 0, 1,
                         GTK_FILL, GTK_FILL, 5, 3);

	adj_manual_rva = gtk_adjustment_new(*rva, -70.0f, 20.0f, 0.1f, 1.0f, 0.0f);
	spin_button = gtk_spin_button_new(GTK_ADJUSTMENT(adj_manual_rva), 0.3, 1);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_button), TRUE);
	gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_button), FALSE);
        gtk_table_attach(GTK_TABLE(table2), spin_button, 1, 2, 0, 1,
                         GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 3);

	g_signal_connect(G_OBJECT(check_button), "toggled", G_CALLBACK(use_rva_check_button_cb),
			 (gpointer)spin_button);

	if (*use_rva >= 0.0f) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), TRUE);
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), FALSE);
		gtk_widget_set_sensitive(spin_button, FALSE);
	}

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	sort_name[0] = '\0';
	file[0] = '\0';
	comment[0] = '\0';

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * pfile = g_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(file_entry)), -1, NULL, NULL, NULL);

		if (pfile == NULL) {
			gtk_widget_grab_focus(file_entry);
			goto display;
		}

                strcpy(name, gtk_entry_get_text(GTK_ENTRY(name_entry)));
                strcpy(sort_name, gtk_entry_get_text(GTK_ENTRY(sort_name_entry)));
                strcpy(file, pfile);

		g_free(pfile);

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_start, 0);
		gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter_end, -1);
		strcpy(comment, gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer),
							 &iter_start, &iter_end, TRUE));

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_button))) {
			*use_rva = 1.0f;
			*rva = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_manual_rva));
		} else {
			*use_rva = -1.0f;
			*rva = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj_manual_rva));
		}

		if ((name[0] == '\0') || (file[0] == '\0')) {
			goto display;
		}

		ret = 1;

        } else {
                name[0] = '\0';
                sort_name[0] = '\0';
                file[0] = '\0';
                comment[0] = '\0';
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
confirm_dialog(char * title, char * text, GtkResponseType resp) {

        int ret;

        dialog = gtk_message_dialog_new(GTK_WINDOW(browser_window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, text);
	gtk_window_set_title(GTK_WINDOW(dialog), title);
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), resp);
        gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES) {
                ret = 1;
        } else {
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


int
is_store_path_readonly(GtkTreePath * p) {

	GtkTreeIter iter;
        GtkTreePath * path;
        float state;

	path = gtk_tree_path_copy(p);

        while (gtk_tree_path_get_depth(path) > 1) {
                gtk_tree_path_up(path);
        }

        gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, 7, &state, -1);
	gtk_tree_path_free(path);

        return state < 0;
}


int
is_store_iter_readonly(GtkTreeIter * i) {

	GtkTreePath * path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), i);
	int ret = is_store_path_readonly(path);
	gtk_tree_path_free(path);
        return ret;
}


#ifdef HAVE_CDDA
int
is_store_path_cdda(GtkTreePath * p) {

	int i;
	GtkTreePath * path = gtk_tree_path_copy(p);

	while (gtk_tree_path_get_depth(path) > 1) {
		gtk_tree_path_up(path);
	}

	i = gtk_tree_path_get_indices(path)[0];
	gtk_tree_path_free(path);

	return (i == 0);
}

int
is_store_iter_cdda(GtkTreeIter * i) {

	GtkTreePath * path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), i);
	int ret = is_store_path_cdda(path);
	gtk_tree_path_free(path);
        return ret;
}
#endif /* HAVE_CDDA */


void
set_popup_sensitivity(GtkTreePath * path) {

	gboolean writable = !is_store_path_readonly(path);
	gboolean vol_free = (vol_window == NULL);
	gboolean build_free = (build_thread_state == BUILD_THREAD_FREE);

#ifdef HAVE_CDDB
	gboolean cddb_free = (cddb_thread_state == CDDB_THREAD_FREE);
#endif /* HAVE_CDDB */

#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	gboolean tag_free = (batch_tag_root == NULL);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */


#ifdef HAVE_CDDA
	if (is_store_path_cdda(path) && (gtk_tree_path_get_depth(path) == 2)) {
		gboolean val_cdda;
		GtkTreeIter iter;

		gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter, path);
		val_cdda = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter) > 0;

		gtk_widget_set_sensitive(cdda_record__addlist, val_cdda);
		gtk_widget_set_sensitive(cdda_record__addlist_albummode, val_cdda);
#ifdef HAVE_CDDB
		gtk_widget_set_sensitive(cdda_record__cddb, val_cdda && cddb_free && build_free);
		gtk_widget_set_sensitive(cdda_record__cddb_submit, val_cdda && cddb_free && build_free);
#endif /* HAVE_CDDB */
		gtk_widget_set_sensitive(cdda_record__rip, val_cdda);
		gtk_widget_set_sensitive(cdda_record__disc_info, val_cdda);
	}
#endif /* HAVE_CDDA */

	gtk_widget_set_sensitive(store__build, writable &&
#ifdef HAVE_CDDB
				 cddb_free &&
#endif /* HAVE_CDDB */
				 build_free);
	gtk_widget_set_sensitive(store__edit, writable);
	gtk_widget_set_sensitive(store__save, writable);
	gtk_widget_set_sensitive(store__addart, writable);
	gtk_widget_set_sensitive(store__volume, vol_free && writable);

	gtk_widget_set_sensitive(artist__add, writable);
	gtk_widget_set_sensitive(artist__build, writable &&
#ifdef HAVE_CDDB
				 cddb_free &&
#endif /* HAVE_CDDB */
				 build_free);
	gtk_widget_set_sensitive(artist__edit, writable);
	gtk_widget_set_sensitive(artist__remove, writable);
	gtk_widget_set_sensitive(artist__addrec, writable);
	gtk_widget_set_sensitive(artist__volume, vol_free && writable);

	gtk_widget_set_sensitive(record__add, writable);
	gtk_widget_set_sensitive(record__edit, writable);
	gtk_widget_set_sensitive(record__remove, writable);
	gtk_widget_set_sensitive(record__addtrk, writable);
#ifdef HAVE_CDDB
	gtk_widget_set_sensitive(record__cddb, writable && cddb_free && build_free);
	gtk_widget_set_sensitive(record__cddb_submit, cddb_free && build_free);
#endif /* HAVE_CDDB */
	gtk_widget_set_sensitive(record__volume, vol_free && writable);

	gtk_widget_set_sensitive(track__add, writable);
	gtk_widget_set_sensitive(track__edit, writable);
	gtk_widget_set_sensitive(track__remove, writable);
	gtk_widget_set_sensitive(track__volume, vol_free && writable);

#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	gtk_widget_set_sensitive(store__tag, tag_free);
	gtk_widget_set_sensitive(artist__tag, tag_free);
	gtk_widget_set_sensitive(record__tag, tag_free);
	gtk_widget_set_sensitive(track__tag, tag_free);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
}


gboolean
music_tree_event_cb(GtkWidget * widget, GdkEvent * event) {

	GtkTreePath * path;
	GtkTreeViewColumn * column;

	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton * bevent = (GdkEventButton *) event;

		if (bevent->button == 1 && options.enable_mstore_toolbar) {
                        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(music_tree), bevent->x, bevent->y,
                                                          &path, &column, NULL, NULL)) {
                          
#ifdef HAVE_CDDA
                                if (is_store_path_cdda(path)) {
                                        gtk_widget_set_sensitive(toolbar_edit_button, FALSE);
                                        gtk_widget_set_sensitive(toolbar_add_button, FALSE);
                                        gtk_widget_set_sensitive(toolbar_remove_button, FALSE);
                                } else {
                                        gtk_widget_set_sensitive(toolbar_edit_button, TRUE);
                                        gtk_widget_set_sensitive(toolbar_add_button, TRUE);
                                        gtk_widget_set_sensitive(toolbar_remove_button, TRUE);
                                }
#endif /* HAVE_CDDA */
				gtk_tree_path_free(path);
                        }
                }

                if (bevent->button == 3) {

			if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(music_tree), bevent->x, bevent->y,
							  &path, &column, NULL, NULL)) {

				set_popup_sensitivity(path);
				
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(music_tree), path, NULL, FALSE);

#ifdef HAVE_CDDA
				if (is_store_path_cdda(path)) {
				switch (gtk_tree_path_get_depth(path)) {
				case 1:
					gtk_menu_popup(GTK_MENU(cdda_store_menu), NULL, NULL, NULL, NULL,
						       bevent->button, bevent->time);
					break;
				case 2:
					gtk_menu_popup(GTK_MENU(cdda_record_menu), NULL, NULL, NULL, NULL,
						       bevent->button, bevent->time);
					break;
				case 3:
					gtk_menu_popup(GTK_MENU(cdda_track_menu), NULL, NULL, NULL, NULL,
						       bevent->button, bevent->time);
					break;
				}
				} else {
#endif /* HAVE_CDDA */
				switch (gtk_tree_path_get_depth(path)) {
				case 1:
					gtk_menu_popup(GTK_MENU(store_menu), NULL, NULL, NULL, NULL,
						       bevent->button, bevent->time);
					break;
				case 2:
					gtk_menu_popup(GTK_MENU(artist_menu), NULL, NULL, NULL, NULL,
						       bevent->button, bevent->time);
					break;
				case 3:
					gtk_menu_popup(GTK_MENU(record_menu), NULL, NULL, NULL, NULL,
						       bevent->button, bevent->time);
					break;
				case 4:
					gtk_menu_popup(GTK_MENU(track_menu), NULL, NULL, NULL, NULL,
						       bevent->button, bevent->time);
					break;
				}
#ifdef HAVE_CDDA
				}
#endif /* HAVE_CDDA */
				gtk_tree_path_free(path);
			} else {
				gtk_menu_popup(GTK_MENU(blank_menu), NULL, NULL, NULL, NULL,
                                               bevent->button, bevent->time);
			}
			return TRUE;
		}
		return FALSE;
	} 

	if (event->type == GDK_KEY_PRESS) {
		GdkEventKey * kevent = (GdkEventKey *) event;

		GtkTreeIter iter;
		GtkTreeModel * model;

		if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {
			GtkTreePath * path = gtk_tree_model_get_path(model, &iter);
			int i;

			set_popup_sensitivity(path);
			
			switch (kevent->keyval) {
			case GDK_KP_Enter:
			case GDK_Return:
				if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(music_tree), path)) {
					gtk_tree_view_collapse_row(GTK_TREE_VIEW(music_tree), path);
				} else {
					gtk_tree_view_expand_row(GTK_TREE_VIEW(music_tree), path, FALSE);
				}
				break;
			}

#ifdef HAVE_CDDA
			if (is_store_path_cdda(path)) {
			switch (gtk_tree_path_get_depth(path)) {
			case 1:
				for (i = 0; cdda_store_keybinds[i].callback; ++i)
					if (kevent->keyval == cdda_store_keybinds[i].keyval1 ||
					    kevent->keyval == cdda_store_keybinds[i].keyval2)
						(cdda_store_keybinds[i].callback)(NULL);
				break;
			case 2:
				for (i = 0; cdda_record_keybinds[i].callback; ++i)
					if (kevent->keyval == cdda_record_keybinds[i].keyval1 ||
					    kevent->keyval == cdda_record_keybinds[i].keyval2)
						(cdda_record_keybinds[i].callback)(NULL);
				break;
			case 3:
				for (i = 0; cdda_track_keybinds[i].callback; ++i)
					if (kevent->keyval == cdda_track_keybinds[i].keyval1 ||
					    kevent->keyval == cdda_track_keybinds[i].keyval2)
						(cdda_track_keybinds[i].callback)(NULL);
				break;
			}
			} else {
#endif /* HAVE_CDDA */
			switch (gtk_tree_path_get_depth(path)) {
			case 1: 
				for (i = 0; store_keybinds[i].callback; ++i)
					if (kevent->keyval == store_keybinds[i].keyval1 ||
					    kevent->keyval == store_keybinds[i].keyval2)
						(store_keybinds[i].callback)(NULL);
				break;
			case 2:
				for (i = 0; artist_keybinds[i].callback; ++i)
					if (kevent->keyval == artist_keybinds[i].keyval1 ||
					    kevent->keyval == artist_keybinds[i].keyval2)
						(artist_keybinds[i].callback)(NULL);
				break;
			case 3:
				for (i = 0; record_keybinds[i].callback; ++i)
					if (kevent->keyval == record_keybinds[i].keyval1 ||
					    kevent->keyval == record_keybinds[i].keyval2)
						(record_keybinds[i].callback)(NULL);
				break;
			case 4:
				for (i = 0; track_keybinds[i].callback; ++i)
					if (kevent->keyval == track_keybinds[i].keyval1 ||
					    kevent->keyval == track_keybinds[i].keyval2)
						(track_keybinds[i].callback)(NULL);
				break;
			}
#ifdef HAVE_CDDA
			}
#endif /* HAVE_CDDA */
			gtk_tree_path_free(path);
		} else {
			int i;
                        for (i = 0; blank_keybinds[i].callback; ++i)
                                if (kevent->keyval == blank_keybinds[i].keyval1 ||
                                    kevent->keyval == blank_keybinds[i].keyval2)
                                        (blank_keybinds[i].callback)(NULL);
		}
		return FALSE;
	}
	return FALSE;
}


gint
dblclick_handler(GtkWidget * widget, GdkEventButton * event, gpointer func_data) {

        GtkTreePath * path;
        GtkTreeViewColumn * column;

        if ((event->type==GDK_2BUTTON_PRESS) && (event->button == 1)) {

		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(music_tree), event->x, event->y,
						  &path, &column, NULL, NULL)) {
			
			int depth = gtk_tree_path_get_depth(path);
#ifdef HAVE_CDDA
			if (is_store_path_cdda(path))
				depth += 1;
#endif /* HAVE_CDDA */

			if (!gtk_tree_selection_path_is_selected(music_select, path)) {
				return FALSE;
			}

			switch (depth) {
			case 1:
				store__addlist_defmode(NULL);
				return TRUE;
				break;
			case 2:
				artist__addlist_defmode(NULL);
				return TRUE;
				break;
			case 3:
				record__addlist_defmode(NULL);
				return TRUE;
				break;
			case 4:
				track__addlist_cb(NULL);
				return TRUE;
				break;
			default:
				return FALSE;
			}
		}
		return FALSE;
	}
	return FALSE;
}


/****************************************/


/* returns the duration of the track */
float
track_addlist_iter(GtkTreeIter iter_track, GtkTreeIter * parent, GtkTreeIter * dest,
		   float avg_voladj, int use_avg_voladj) {

        GtkTreeIter iter_artist;
        GtkTreeIter iter_record;
	GtkTreeIter list_iter;

        char * pfile;
	char * ptrack_name;

        char artist_name[MAXLEN];
        char record_name[MAXLEN];
        char track_name[MAXLEN];
        char file[MAXLEN];
	char list_str[MAXLEN];

	float duration;
	float volume;
	float rva;
	float use_rva;
	float voladj = 0.0f;
	char voladj_str[32];
	char duration_str[MAXLEN];

	metadata * meta = NULL;


	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track, 0, &ptrack_name, 2, &pfile,
			   4, &duration, 5, &volume, 6, &rva, 7, &use_rva, -1);
	strncpy(track_name, ptrack_name, MAXLEN-1);
	strncpy(file, pfile, MAXLEN-1);
	g_free(ptrack_name);
	g_free(pfile);


	if (parent == NULL) {
#ifdef HAVE_CDDA
		if (is_store_iter_cdda(&iter_track)) {

			char * device_path = NULL;
			cdda_disc_t * disc = NULL;

			gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store),
						   &iter_record, &iter_track);
			gtk_tree_model_get(GTK_TREE_MODEL(music_store),
					   &iter_record, 2, &device_path, -1);

			disc = &cdda_get_drive_by_spec_device_path(device_path)->disc;

			strncpy(artist_name, disc->artist_name, MAXLEN-1);
			strncpy(record_name, disc->record_name, MAXLEN-1);

			g_free(device_path);
		} else
#endif /* HAVE_CDDA */
		{
			char * partist_name;
			char * precord_name;

			gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store),
						   &iter_record, &iter_track);
			gtk_tree_model_get(GTK_TREE_MODEL(music_store),
					   &iter_record, 0, &precord_name, -1);
			strncpy(record_name, precord_name, MAXLEN-1);
			g_free(precord_name);

			gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store),
						   &iter_artist, &iter_record);
			gtk_tree_model_get(GTK_TREE_MODEL(music_store),
					   &iter_artist, 0, &partist_name, -1);
			strncpy(artist_name, partist_name, MAXLEN-1);
			g_free(partist_name);
		}
	}

#ifdef HAVE_CDDA
	if (!is_store_iter_cdda(&iter_track)) {
#endif /* HAVE_CDDA */
	if (options.auto_use_meta_artist || options.auto_use_meta_record || options.auto_use_meta_track) {
		meta = meta_new();
		if (!meta_read(meta, file)) {
			meta_free(meta);
			meta = NULL;
		}
	}

	if (parent == NULL) {
		if ((meta != NULL) && options.auto_use_meta_artist) {
			meta_get_artist(meta, artist_name);
		}

		if ((meta != NULL) && options.auto_use_meta_record) {
			meta_get_record(meta, record_name);
		}
	}

	if ((meta != NULL) && options.auto_use_meta_track) {
		meta_get_title(meta, track_name);
	}

	if (meta != NULL) {
		meta_free(meta);
		meta = NULL;
	}
#ifdef HAVE_CDDA
	}
#endif /* HAVE_CDDA */
	
	if (parent != NULL) {
		strcpy(list_str, track_name);
	} else {
		make_title_string(list_str, options.title_format,
				  artist_name, record_name, track_name);
	}

	if (duration == 0.0f) {
		duration = get_file_duration(file);
		if (!is_store_iter_readonly(&iter_track) && duration > 0.0f) {
			gtk_tree_store_set(music_store, &iter_track, 4, duration, -1);
			music_store_mark_changed(&iter_track);
		}
	}
	time2time(duration, duration_str);
	
	if (options.rva_is_enabled) {
		if (options.rva_use_averaging && use_avg_voladj) {
			voladj = avg_voladj;
		} else {
			if (use_rva >= 0.0f) {
				voladj = rva;
			} else {
				if (volume <= 0.1f) {
					voladj = rva_from_volume(volume,
								 options.rva_refvol,
								 options.rva_steepness);
				} else { /* unmeasured, see if there is RVA data in the file */
					metadata * meta = meta_new();
					if (meta_read(meta, file)) {
						if (!meta_get_rva(meta, &voladj)) {
							voladj = 0.0f;
						}
					} else {
						voladj = 0.0f;
					}
					meta_free(meta);
				}
			}
		}
	}
	
	/* either parent or dest should be set, but not both */
	gtk_tree_store_insert_before(play_store, &list_iter, parent, dest);
	
	voladj2str(voladj, voladj_str);
	
	gtk_tree_store_set(play_store, &list_iter, 0, list_str, 1, file,
			   2, pl_color_inactive, 3, voladj, 4, voladj_str,
			   5, duration, 6, duration_str, -1);

	return duration;
}


void
record_addlist_iter(GtkTreeIter iter_record, GtkTreeIter * dest, int album_mode) {

        GtkTreeIter iter_track;
	GtkTreeIter list_iter;
	GtkTreeIter * plist_iter;
	
	int i;
	int nlevels;

	float volume;
	float voladj = 0.0f;

	float record_duration = 0.0f;


	if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter_record) == 0)
		return;

	if (options.rva_is_enabled && options.rva_use_averaging) { /* save track volumes */

		float * volumes = NULL;
		i = 0;
		nlevels = 0;
		
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track,
						     &iter_record, i++)) {
			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track, 5, &volume, -1);
			
			if (volume > 0.1f) { /* unmeasured */
				volume = options.rva_refvol;
			}
			
			nlevels++;
			if ((volumes = realloc(volumes, nlevels * sizeof(float))) == NULL) {
				fprintf(stderr, "record__addlist_cb: realloc error\n");
				return;
			}
			volumes[nlevels-1] = volume;
		}
		
		voladj = rva_from_multiple_volumes(nlevels, volumes,
						   options.rva_use_linear_thresh,
						   options.rva_avg_linear_thresh,
						   options.rva_avg_stddev_thresh,
						   options.rva_refvol, options.rva_steepness);
		free(volumes);
	}
	
	if (album_mode) {

		char name_str[MAXLEN];
		char packed_str[MAXLEN];

#ifdef HAVE_CDDA
		if (is_store_iter_cdda(&iter_record)) {

			char * device_path = NULL;
			cdda_disc_t * disc = NULL;

			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record, 2, &device_path, -1);

			disc = &cdda_get_drive_by_spec_device_path(device_path)->disc;

			snprintf(name_str, MAXLEN-1, "%s: %s", disc->artist_name, disc->record_name);
			pack_strings(disc->artist_name, disc->record_name, packed_str);

			g_free(device_path);

		} else

#endif /* HAVE_CDDA */

		{

			GtkTreeIter iter_artist;
			char * precord_name;
			char * partist_name;

			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record, 0, &precord_name, -1);
			gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_artist, &iter_record);
			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_artist, 0, &partist_name, -1);

			snprintf(name_str, MAXLEN-1, "%s: %s", partist_name, precord_name);
			pack_strings(partist_name, precord_name, packed_str);

			g_free(precord_name);
			g_free(partist_name);
		}

		gtk_tree_store_insert_before(play_store, &list_iter, NULL, dest);
		gtk_tree_store_set(play_store, &list_iter, 0, name_str, 1, packed_str,
				   2, pl_color_inactive, 3, 0.0f/*voladj*/, 4, ""/*voladj_str*/,
				   5, 0.0f/*duration*/, 6, "00:00"/*duration_str*/, -1);
		plist_iter = &list_iter;
		dest = NULL;
	} else {
		plist_iter = NULL;
	}

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track, &iter_record, i++)) {
		record_duration += track_addlist_iter(iter_track, plist_iter, dest, voladj,
						      options.rva_use_averaging);
	}

	if (album_mode) {
		char record_duration_str[MAXLEN];
		time2time(record_duration, record_duration_str);
		gtk_tree_store_set(play_store, &list_iter, 5, record_duration, 6, record_duration_str, -1);
	}
}


void
artist_addlist_iter(GtkTreeIter iter_artist, GtkTreeIter * dest, int album_mode) {

	GtkTreeIter iter_record;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_record, &iter_artist, i++)) {
		record_addlist_iter(iter_record, dest, album_mode);
	}

}


void
store_addlist_iter(GtkTreeIter iter_store, GtkTreeIter * dest, int album_mode) {

	GtkTreeIter iter_artist;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_artist, &iter_store, i++)) {
		artist_addlist_iter(iter_artist, dest, album_mode);
	}
}


/****************************************/


/* mode: 0 normal, 1 album mode */

void
store__addlist_with_mode(int mode, gpointer data) {

        GtkTreeIter iter_store;

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_store)) {

                iw_show_info (NULL, GTK_WINDOW(browser_window), _("Adding songs..."));
		store_addlist_iter(iter_store, (GtkTreeIter *)data, mode);
		playlist_content_changed();
		delayed_playlist_rearrange(100);
                iw_hide_info ();
	}
}

void
store__addlist_defmode(gpointer data) {

	store__addlist_with_mode(options.playlist_is_tree, data);
}


void
store__addlist_albummode_cb(gpointer data) {

	store__addlist_with_mode(1, data);
}


void
store__addlist_cb(gpointer data) {

	store__addlist_with_mode(0, data);
}


void
store__add_cb(gpointer data) {

	GtkTreeIter iter;

	char name[MAXLEN];
	char file[MAXLEN];
	char comment[MAXLEN];

	xmlDocPtr doc;
	xmlNodePtr root;


	name[0] = '\0';
	file[0] = '\0';
	comment[0] = '\0';

	if (add_store_dialog(name, file, comment)) {

		if (access(file, F_OK) == 0) {

			GtkWidget * dialog;

			dialog = gtk_message_dialog_new(GTK_WINDOW (browser_window),
							GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
							GTK_MESSAGE_INFO,
							GTK_BUTTONS_OK,
							_("The store '%s' already exists.\nAdd it on the Settings/Music Store tab."),
							file);
			gtk_widget_show_all(dialog);
			aqualung_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
		} else {

			char * utf8 = g_locale_to_utf8(file, -1, NULL, NULL, NULL);

			gtk_tree_store_append(music_store, &iter, NULL);

			gtk_tree_store_set(music_store, &iter, 0, name, 2, file,
					   3, comment, 7, 1.0f/*rw*/, 8, PANGO_WEIGHT_BOLD, -1);
                        if (options.enable_ms_tree_icons) {
                                gtk_tree_store_set(music_store, &iter, 9, icon_store, -1);
                        }

			doc = xmlNewDoc((const xmlChar *) "1.0");
			root = xmlNewNode(NULL, (const xmlChar *) "music_store");
			xmlDocSetRootElement(doc, root);

			xmlNewTextChild(root, NULL, (const xmlChar *) "name", (xmlChar *) name);
			xmlNewTextChild(root, NULL, (const xmlChar *) "comment", (xmlChar *) comment);

			xmlSaveFormatFile(file, doc, 1);
			xmlFreeDoc(doc);

			gtk_list_store_append(ms_pathlist_store, &iter);
			gtk_list_store_set(ms_pathlist_store, &iter, 0, file, 1, utf8, 2, _("rw"), -1);
			g_free(utf8);
		}
	}
}


void
store__build_cb(gpointer data) {

	GtkTreeIter store_iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &store_iter)) {
		build_store(store_iter);
	}
}


void
store__edit_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	char * pname;
	char * pcomment;
	char * pfile = NULL;

	char name[MAXLEN+1];
	char comment[MAXLEN];
	char file[MAXLEN];
	float state;
	float dirty;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) return;

                gtk_tree_model_get(model, &iter, 0, &pname, 2, &pfile, 3, &pcomment, 6, &dirty, 7, &state, -1);

		if (state < 0) {
			g_free(pname);
			g_free(pcomment);
			g_free(pfile);
			return;
		}

		strncpy(name, pname, MAXLEN-1);
		strncpy(comment, pcomment, MAXLEN-1);
		strncpy(file, pfile, MAXLEN-1);

		g_free(pname);
                g_free(pcomment);
		g_free(pfile);

		if (edit_store_dialog(name + ((dirty < 0) ? 1 : 0), file, comment)) {
			gtk_tree_store_set(music_store, &iter, 0, name, 3, comment, -1);
			tree_selection_changed_cb(music_select, NULL);
			music_store_mark_changed(&iter);
		}
        }
}


void
store_volume_calc(int unmeasured) {

        GtkTreeIter iter_store;
        GtkTreeIter iter_artist;
        GtkTreeIter iter_record;
        GtkTreeIter iter_track;
        GtkTreeModel * model;

        char * pfile;
        char file[MAXLEN];
	int h, i, j;

	vol_queue_t * q = NULL;


	if (vol_window != NULL) {
		return;
	}

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_store)) {

		if (is_store_iter_readonly(&iter_store)) return;

		h = 0;
		while (gtk_tree_model_iter_nth_child(model, &iter_artist, &iter_store, h++)) {

			i = 0;
			while (gtk_tree_model_iter_nth_child(model, &iter_record, &iter_artist, i++)) {

				j = 0;
				while (gtk_tree_model_iter_nth_child(model, &iter_track, &iter_record, j++)) {
					float vol;

					gtk_tree_model_get(model, &iter_track, 2, &pfile, 5, &vol, -1);
					strncpy(file, pfile, MAXLEN-1);
					g_free(pfile);

					if (!unmeasured || vol > 0.1f) {
						if (q == NULL) {
							q = vol_queue_push(NULL, file, iter_track);
						} else {
							vol_queue_push(q, file, iter_track);
						}
					}
				}
			}
		}

		calculate_volume(q, NULL);
	}
}


void
store__volume_unmeasured_cb(gpointer data) {

	store_volume_calc(1);
}


void
store__volume_all_cb(gpointer data) {

	store_volume_calc(0);
}


void
store__remove_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;
	char * pname;
	char * pfile;
	char name[MAXLEN];
	char file[MAXLEN];
	char text[MAXLEN];
	float dirty;
	int i = 0;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

                gtk_tree_model_get(model, &iter, 0, &pname, 2, &pfile, 6, &dirty, -1);

		strncpy(name, pname, MAXLEN-1);
		strncpy(file, pfile, MAXLEN-1);
                g_free(pname);
                g_free(pfile);
		
		snprintf(text, MAXLEN-1, _("Really remove \"%s\" from the Music Store?"),
			 (dirty < 0) ? (name + 1) : (name));
		if (confirm_dialog(_("Remove Store"), text, GTK_RESPONSE_NO)) {

			if (dirty < 0) {
				if (confirm_dialog(_("Remove Store"),
					   _("Do you want to save the store before removing?"),
					   GTK_RESPONSE_YES)) {
					save_music_store(&iter);
				} else {
					music_store_mark_saved(&iter);
				}
			}

			gtk_tree_store_remove(music_store, &iter);
			tree_selection_changed_cb(music_select, NULL);
			
			i = 0;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ms_pathlist_store),
							     &iter, NULL, i++)) {
				gtk_tree_model_get(GTK_TREE_MODEL(ms_pathlist_store), &iter, 0, &pname, -1);
				if (!strcmp(file, pname)) {
					gtk_list_store_remove(ms_pathlist_store, &iter);
				}
			}
		}
	}
}


void
store__save_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {
		save_music_store(&iter);
	}
}


void
store__save_all_cb(gpointer data) {

	save_all_music_store();
}


/****************************************/


/* mode: 0 normal, 1 album mode */

void
artist__addlist_with_mode(int mode, gpointer data) {

        GtkTreeIter iter_artist;

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_artist)) {
                iw_show_info (NULL, GTK_WINDOW(browser_window), _("Adding songs..."));
		artist_addlist_iter(iter_artist, (GtkTreeIter *)data, mode);
		playlist_content_changed();
		delayed_playlist_rearrange(100);
                iw_hide_info ();
	}
}

void
artist__addlist_defmode(gpointer data) {

	artist__addlist_with_mode(options.playlist_is_tree, data);
}

void
artist__addlist_albummode_cb(gpointer data) {

	artist__addlist_with_mode(1, data);
}


void
artist__addlist_cb(gpointer data) {

	artist__addlist_with_mode(0, data);
}


void
artist__add_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeIter parent_iter;
	GtkTreePath * parent_path;
	GtkTreeModel * model;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char comment[MAXLEN];

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';


	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		if (is_store_iter_readonly(&parent_iter)) return;

		/* get iter to music store (parent) */
		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		if (gtk_tree_path_get_depth(parent_path) == 2)
			gtk_tree_path_up(parent_path);
		gtk_tree_model_get_iter(model, &parent_iter, parent_path);


		if (add_artist_dialog(name, sort_name, comment)) {
			gtk_tree_store_append(music_store, &iter, &parent_iter);
			gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name, 2,
					   "", 3, comment, -1);
                        if (options.enable_ms_tree_icons) {
			        gtk_tree_store_set(music_store, &iter, 9, icon_artist, -1);
                        }
			music_store_mark_changed(&iter);
		}
	}
}


void
artist__build_cb(gpointer data) {

	GtkTreeIter artist_iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &artist_iter)) {
		build_artist(artist_iter);
	}
}


void
artist__edit_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	char * pname;
	char * psort_name;
	char * pcomment;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char comment[MAXLEN];
	
	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) return;

                gtk_tree_model_get(model, &iter, 0, &pname, 1, &psort_name, 3, &pcomment, -1);

		strncpy(name, pname, MAXLEN-1);
		strncpy(sort_name, psort_name, MAXLEN-1);
		strncpy(comment, pcomment, MAXLEN-1);

                g_free(pname);
                g_free(psort_name);
                g_free(pcomment);

		if (edit_artist_dialog(name, sort_name, comment)) {

			gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name, 2, "", 3, comment, -1);
			tree_selection_changed_cb(music_select, NULL);
			music_store_mark_changed(&iter);
		}
        }
}


void
artist_volume_calc(int unmeasured) {

        GtkTreeIter iter_artist;
        GtkTreeIter iter_record;
        GtkTreeIter iter_track;
        GtkTreeModel * model;

        char * pfile;
        char file[MAXLEN];
	int i, j;

	vol_queue_t * q = NULL;


	if (vol_window != NULL) {
		return;
	}

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_artist)) {

		if (is_store_iter_readonly(&iter_artist)) return;

		i = 0;
		while (gtk_tree_model_iter_nth_child(model, &iter_record, &iter_artist, i++)) {

			j = 0;
			while (gtk_tree_model_iter_nth_child(model, &iter_track, &iter_record, j++)) {
				float vol;				

				gtk_tree_model_get(model, &iter_track, 2, &pfile, 5, &vol, -1);
				strncpy(file, pfile, MAXLEN-1);
				g_free(pfile);

				if (!unmeasured || vol > 0.1f) {
					if (q == NULL) {
						q = vol_queue_push(NULL, file, iter_track);
					} else {
						vol_queue_push(q, file, iter_track);
					}
				}
			}
		}

		calculate_volume(q, NULL);
	}
}


void
artist__volume_unmeasured_cb(gpointer data) {

	artist_volume_calc(1);
}


void
artist__volume_all_cb(gpointer data) {

	artist_volume_calc(0);
}


void
artist__remove_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;
	char * pname;
	char name[MAXLEN];
	char text[MAXLEN];

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) return;

                gtk_tree_model_get(model, &iter, 0, &pname, -1);
		strncpy(name, pname, MAXLEN-1);
                g_free(pname);
		
		snprintf(text, MAXLEN-1, _("Really remove \"%s\" from the Music Store?"), name);
		if (confirm_dialog(_("Remove Artist"), text, GTK_RESPONSE_NO)) {
			music_store_mark_changed(&iter);
			gtk_tree_store_remove(music_store, &iter);
			tree_selection_changed_cb(music_select, NULL);
		}
	}
}

/************************************/


/* mode: 0 normal, 1 album mode */

void
record__addlist_with_mode(int mode, gpointer data) {

        GtkTreeIter iter_record;

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_record)) {
                iw_show_info (NULL, GTK_WINDOW(browser_window), _("Adding songs..."));
		record_addlist_iter(iter_record, (GtkTreeIter *)data, mode);
		playlist_content_changed();
		delayed_playlist_rearrange(100);
                iw_hide_info ();
	}
}

void
record__addlist_defmode(gpointer data) {

	record__addlist_with_mode(options.playlist_is_tree, data);
}

void
record__addlist_albummode_cb(gpointer data) {

	record__addlist_with_mode(1, data);
}


void
record__addlist_cb(gpointer data) {

	record__addlist_with_mode(0, data);
}


void
record__add_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeIter parent_iter;
	GtkTreeIter child_iter;
	GtkTreePath * parent_path;
	GtkTreeModel * model;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char ** strings = NULL;
	char * str;
	char comment[MAXLEN];
	char str_n[16];

	int i;

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		if (is_store_iter_readonly(&parent_iter)) return;

		/* get iter to artist (parent) */
		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		if (gtk_tree_path_get_depth(parent_path) == 3)
			gtk_tree_path_up(parent_path);
		gtk_tree_model_get_iter(model, &parent_iter, parent_path);
		
		if (add_record_dialog(name, sort_name, &strings, comment)) {
			
			gtk_tree_store_append(music_store, &iter, &parent_iter);
			gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name,
					   2, "", 3, comment, -1);
                        if (options.enable_ms_tree_icons) {
                                gtk_tree_store_set(music_store, &iter, 9, icon_record, -1);
                        }

			music_store_mark_changed(&iter);

			if (strings) {
				for (i = 0; strings[i] != NULL; i++) {
					sprintf(str_n, "%02d", i+1);
					str = strings[i];
					while (strstr(str, "/")) {
						str = strstr(str, "/") + 1;
					}
					if (str) {
						float volume = 1.0f;
						float use_rva = -1.0f;
						char * utf8 = g_locale_to_utf8(str, -1, NULL, NULL, NULL);

						gtk_tree_store_append(music_store, &child_iter, &iter);
						gtk_tree_store_set(music_store, &child_iter,
								   0, utf8, 1, str_n,
								   2, strings[i], 3, "",
								   4, get_file_duration(strings[i]),
								   5, volume, 7, use_rva, 
								   9, icon_track, -1);
                                                if (options.enable_ms_tree_icons) {
                                                        gtk_tree_store_set(music_store, &child_iter,
                                                                           9, icon_track, -1);
						}
						g_free(utf8);
					}
					free(strings[i]);
				}
				free(strings);
			}
		}
	}
}


void
record__edit_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	char * pname;
	char * psort_name;
	char * pcomment;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char comment[MAXLEN];
	
	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) return;

                gtk_tree_model_get(model, &iter, 0, &pname, 1, &psort_name, 3, &pcomment, -1);

		strncpy(name, pname, MAXLEN-1);
		strncpy(sort_name, psort_name, MAXLEN-1);
		strncpy(comment, pcomment, MAXLEN-1);

                g_free(pname);
                g_free(psort_name);
                g_free(pcomment);

		if (edit_record_dialog(name, sort_name, comment)) {

			gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name, 2, "", 3, comment, -1);
			tree_selection_changed_cb(music_select, NULL);
			music_store_mark_changed(&iter);
		}
        }
}


void
record_volume_calc(int unmeasured) {

        GtkTreeIter iter_record;
        GtkTreeIter iter_track;
        GtkTreeModel * model;

	vol_queue_t * q = NULL;

	int i;
        char * pfile;
        char file[MAXLEN];


	if (vol_window != NULL) {
		return;
	}

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_record)) {

		if (is_store_iter_readonly(&iter_record)) return;

		i = 0;
		while (gtk_tree_model_iter_nth_child(model, &iter_track, &iter_record, i++)) {
		
			float vol;

			gtk_tree_model_get(model, &iter_track, 2, &pfile, 5, &vol, -1);
			strncpy(file, pfile, MAXLEN-1);
			g_free(pfile);
		
			if (!unmeasured || vol > 0.1f) {
				if (q == NULL) {
					q = vol_queue_push(NULL, file, iter_track);
				} else {
					vol_queue_push(q, file, iter_track);
				}
			}
		}
	}
	
	calculate_volume(q, NULL);
}


void
record__volume_unmeasured_cb(gpointer data) {

	record_volume_calc(1);
}


void
record__volume_all_cb(gpointer data) {

	record_volume_calc(0);
}


void
record__remove_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;
	char * pname;
	char name[MAXLEN];
	char text[MAXLEN];

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) return;

                gtk_tree_model_get(model, &iter, 0, &pname, -1);
		strncpy(name, pname, MAXLEN-1);
                g_free(pname);
		
		snprintf(text, MAXLEN-1, _("Really remove \"%s\" from the Music Store?"), name);
		if (confirm_dialog(_("Remove Record"), text, GTK_RESPONSE_NO)) {
			music_store_mark_changed(&iter);
			gtk_tree_store_remove(music_store, &iter);
			gtk_tree_selection_unselect_all(music_select);
			tree_selection_changed_cb(music_select, NULL);
		}
	}
}


#ifdef HAVE_CDDB

void
record__cddb_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {
		if (!is_store_iter_readonly(&iter)) {
			cddb_get(&iter);
		}
	}
}

void
record__cddb_submit_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {
		cddb_submit(&iter);
	}
}

#endif /* HAVE_CDDB */


/************************************/


void
track__addlist_cb(gpointer data) {

        GtkTreeIter iter_track;

        if (gtk_tree_selection_get_selected(music_select, NULL, &iter_track)) {
		track_addlist_iter(iter_track, NULL, (GtkTreeIter *)data, 0.0f, 0);
		playlist_content_changed();
		delayed_playlist_rearrange(100);
	}
}


void
track__add_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeIter parent_iter;
	GtkTreePath * parent_path;
	GtkTreeModel * model;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char file[MAXLEN];
	char comment[MAXLEN];

	float volume = 1.0f;
	float use_rva = -1.0f;

	name[0] = '\0';
	sort_name[0] = '\0';
	file[0] = '\0';
	comment[0] = '\0';

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		if (is_store_iter_readonly(&parent_iter)) return;

		/* get iter to artist (parent) */
		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		if (gtk_tree_path_get_depth(parent_path) == 4)
			gtk_tree_path_up(parent_path);
		gtk_tree_model_get_iter(model, &parent_iter, parent_path);
		
		if (add_track_dialog(name, sort_name, file, comment)) {
			
			gtk_tree_store_append(music_store, &iter, &parent_iter);
			gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name, 2, file,
					   3, comment, 4, get_file_duration(file), 5, volume, 
					   7, use_rva, -1);
	                if (options.enable_ms_tree_icons) {
                                gtk_tree_store_set(music_store, &iter, 9, icon_track, -1);
                        }
			music_store_mark_changed(&iter);
		}
	}
}


void
track__edit_cb(gpointer data) {

        GtkTreeIter iter;
        GtkTreeModel * model;

        char * pname;
        char * psort_name;
	char * pfile;
        char * pcomment;

        char name[MAXLEN];
        char sort_name[MAXLEN];
	char file[MAXLEN];
        char comment[MAXLEN];
	float duration;
	float volume;
	float rva;
	float use_rva;

        if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) return;

                gtk_tree_model_get(model, &iter,
				   0, &pname, 1, &psort_name, 2, &pfile, 3, &pcomment,
				   4, &duration, 5, &volume, 6, &rva, 7, &use_rva, -1);

                strncpy(name, pname, MAXLEN-1);
                strncpy(sort_name, psort_name, MAXLEN-1);
                strncpy(file, pfile, MAXLEN-1);
                strncpy(comment, pcomment, MAXLEN-1);

                g_free(pname);
                g_free(psort_name);
                g_free(pfile);
                g_free(pcomment);


		if (duration == 0.0f) {
			duration = get_file_duration(file);
			gtk_tree_store_set(music_store, &iter, 4, duration, -1);
		}

                if (edit_track_dialog(name, sort_name, file, comment, duration, volume, &rva, &use_rva)) {

                        gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name,
					   2, file, 3, comment, 6, rva, 7, use_rva, -1);
                        tree_selection_changed_cb(music_select, NULL);

			music_store_mark_changed(&iter);
                }
        }
}


void
track__fileinfo_cb(gpointer data) {

        GtkTreeIter iter_track;
        GtkTreeIter iter_record;
        GtkTreeIter iter_artist;
        GtkTreeModel * model;

        char * ptrack_name;
        char * precord_name;
        char * partist_name;
	char * pfile;

        char track_name[MAXLEN];
        char record_name[MAXLEN];
        char artist_name[MAXLEN];
	char file[MAXLEN];

	char list_str[MAXLEN];

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_track)) {

                gtk_tree_model_get(model, &iter_track, 0, &ptrack_name, 2, &pfile, -1);
                strncpy(track_name, ptrack_name, MAXLEN-1);
                strncpy(file, pfile, MAXLEN-1);
                g_free(ptrack_name);
                g_free(pfile);
		
		gtk_tree_model_iter_parent(model, &iter_record, &iter_track);
                gtk_tree_model_get(model, &iter_record, 0, &precord_name, -1);
                strncpy(record_name, precord_name, MAXLEN-1);
                g_free(precord_name);

		gtk_tree_model_iter_parent(model, &iter_artist, &iter_record);
                gtk_tree_model_get(model, &iter_artist, 0, &partist_name, -1);
                strncpy(artist_name, partist_name, MAXLEN-1);
                g_free(partist_name);

		make_title_string(list_str, options.title_format,
				  artist_name, record_name, track_name);

		if (is_store_iter_readonly(&iter_track)) {
			show_file_info(list_str, file, 0, model, iter_track);
		} else {
			show_file_info(list_str, file, 1, model, iter_track);
		}
        }
}


void
track_volume_calc(int unmeasured) {

        GtkTreeIter iter_track;
        GtkTreeModel * model;
	char * pfile;
	char file[MAXLEN];
	float vol;
	
	vol_queue_t * q;

	if (vol_window != NULL) {
		return;
	}

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_track)) {

		if (is_store_iter_readonly(&iter_track)) return;

                gtk_tree_model_get(model, &iter_track, 2, &pfile, 5, &vol, -1);
                strncpy(file, pfile, MAXLEN-1);
                g_free(pfile);

		if (!unmeasured || vol > 0.1f) {
			q = vol_queue_push(NULL, file, iter_track);
			calculate_volume(q, NULL);
		}
        }
}


void
track__volume_unmeasured_cb(gpointer data) {

	track_volume_calc(1);
}


void
track__volume_all_cb(gpointer data) {

	track_volume_calc(0);
}


void
track__remove_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;
	char * pname;
	char name[MAXLEN];
	char text[MAXLEN];

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) return;

                gtk_tree_model_get(model, &iter, 0, &pname, -1);
		strncpy(name, pname, MAXLEN-1);
                g_free(pname);
		
		snprintf(text, MAXLEN-1, _("Really remove \"%s\" from the Music Store?"), name);
		if (confirm_dialog(_("Remove Track"), text, GTK_RESPONSE_NO)) {
			music_store_mark_changed(&iter);
			gtk_tree_store_remove(music_store, &iter);
			gtk_tree_selection_unselect_all(music_select);
			tree_selection_changed_cb(music_select, NULL);
		}
	}
}


/************************************/

#ifdef HAVE_CDDA
void
cdda_record__drive_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {
		gchar * sort_name;
		char device_path[CDDA_MAXLEN];
                gtk_tree_model_get(model, &iter, 2, &sort_name, -1);

		sscanf(sort_name, "CDDA_DRIVE %s", device_path);
		g_free(sort_name);

		cdda_drive_info(device_path);
	}
}
#endif /* HAVE_CDDA */

/************************************/

void
search_cb(gpointer data) {

	search_dialog();
}

void
collapse_all_items_cb(gpointer data) {

        gtk_tree_view_collapse_all(GTK_TREE_VIEW(music_tree));  
}

void
edit_item_cb(gpointer data) {

	GtkTreeModel * model;
	GtkTreeIter parent_iter;
	GtkTreePath * parent_path;
        gint level;

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		if (is_store_iter_readonly(&parent_iter)) return;

		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		level = gtk_tree_path_get_depth(parent_path);

                switch(level) {

                        case 1: /* store */
                                store__edit_cb(NULL);
                                break;
                        case 2: /* artist */
                                artist__edit_cb(NULL);
                                break;
                        case 3: /* album */
                                record__edit_cb(NULL);
                                break;
                        case 4: /* track */
                                track__edit_cb(NULL);
                                break;

                        default:
                                break;
                }
        }

}

void
add_item_cb(gpointer data) {

	GtkTreeModel * model;
	GtkTreeIter parent_iter;
	GtkTreePath * parent_path;
        gint level;

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		if (is_store_iter_readonly(&parent_iter)) return;

		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		level = gtk_tree_path_get_depth(parent_path);

                switch(level) {

                        case 1: /* store */
                                store__add_cb(NULL);
                                break;
                        case 2: /* artist */
                                artist__add_cb(NULL);
                                break;
                        case 3: /* album */
                                record__add_cb(NULL);
                                break;
                        case 4: /* track */
                                track__add_cb(NULL);
                                break;

                        default:
                                break;
                }
        }

}

void
remove_item_cb(gpointer data) {

	GtkTreeModel * model;
	GtkTreeIter parent_iter;
	GtkTreePath * parent_path;
        gint level;

	if (gtk_tree_selection_get_selected(music_select, &model, &parent_iter)) {

		if (is_store_iter_readonly(&parent_iter)) return;

		parent_path = gtk_tree_model_get_path(model, &parent_iter);
		level = gtk_tree_path_get_depth(parent_path);

                switch(level) {

                        case 1: /* store */
                                store__remove_cb(NULL);
                                break;
                        case 2: /* artist */
                                artist__remove_cb(NULL);
                                break;
                        case 3: /* album */
                                record__remove_cb(NULL);
                                break;
                        case 4: /* track */
                                track__remove_cb(NULL);
                                break;

                        default:
                                break;
                }
        }

}


/************************************/


#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)

AQUALUNG_THREAD_DECLARE(tag_thread_id)
volatile int batch_tag_cancelled;

#define TAG_F_TITLE   (1 << 0)
#define TAG_F_ARTIST  (1 << 1)
#define TAG_F_ALBUM   (1 << 2)
#define TAG_F_COMMENT (1 << 3)
#define TAG_F_YEAR    (1 << 5)
#define TAG_F_TRACKNO (1 << 6)

int tag_flags = TAG_F_TITLE | TAG_F_ARTIST | TAG_F_ALBUM | TAG_F_YEAR | TAG_F_COMMENT;

GtkTreeIter store_tag_iter;
GtkTreeIter artist_tag_iter;
GtkTreeIter record_tag_iter;
GtkTreeIter track_tag_iter;

char artist_tag[MAXLEN];
char album_tag[MAXLEN];
char year_tag[MAXLEN];

batch_tag_t * batch_tag_curr = NULL;

GtkWidget * tag_prog_window;
GtkWidget * tag_prog_file_entry;
GtkWidget * tag_prog_cancel_button;
GtkListStore * tag_error_list;

int
create_tag_dialog() {

	GtkWidget * dialog;
	GtkWidget * check_artist;
	GtkWidget * check_record;
	GtkWidget * check_track;
	GtkWidget * check_comment;
	GtkWidget * check_trackno;
	GtkWidget * check_year;


        dialog = gtk_dialog_new_with_buttons(_("Update file metadata"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     NULL);

	check_artist = gtk_check_button_new_with_label(_("Artist name"));
	check_record = gtk_check_button_new_with_label(_("Record name"));
	check_track = gtk_check_button_new_with_label(_("Track name"));
	check_comment = gtk_check_button_new_with_label(_("Track comment"));
	check_trackno = gtk_check_button_new_with_label(_("Track number"));
	check_year = gtk_check_button_new_with_label(_("Year"));

	gtk_widget_set_name(check_artist, "check_on_window");
	gtk_widget_set_name(check_record, "check_on_window");
	gtk_widget_set_name(check_track, "check_on_window");
	gtk_widget_set_name(check_comment, "check_on_window");
	gtk_widget_set_name(check_trackno, "check_on_window");
	gtk_widget_set_name(check_year, "check_on_window");

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), check_artist, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), check_record, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), check_track, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), check_comment, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), check_trackno, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), check_year, FALSE, FALSE, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_artist),
				     tag_flags & TAG_F_ARTIST);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_record),
				     tag_flags & TAG_F_ALBUM);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_track),
				     tag_flags & TAG_F_TITLE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_comment),
				     tag_flags & TAG_F_COMMENT);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_trackno),
				     tag_flags & TAG_F_TRACKNO);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_year),
				     tag_flags & TAG_F_YEAR);

	gtk_widget_show_all(dialog);

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		tag_flags =
			(TAG_F_ARTIST *
			 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_artist))) |
			(TAG_F_ALBUM *
			 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_record))) |
			(TAG_F_TITLE *
			 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_track))) |
			(TAG_F_COMMENT *
			 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_comment))) |
			(TAG_F_TRACKNO *
			 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_trackno))) |
			(TAG_F_YEAR *
			 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_year)));

		gtk_widget_destroy(dialog);
		return 1;
	}

        gtk_widget_destroy(dialog);
	return 0;
}

void
tag_prog_window_close(GtkWidget * widget, gpointer data) {

	batch_tag_cancelled = 1;
	batch_tag_root = batch_tag_curr = NULL;

	if (tag_prog_window) {
		gtk_widget_destroy(tag_prog_window);
		tag_prog_window = NULL;
	}
}

void
cancel_batch_tag(GtkWidget * widget, gpointer data) {

	tag_prog_window_close(NULL, NULL);
}

void
create_tag_prog_window(void) {


	GtkWidget * table;
	GtkWidget * label;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * hbuttonbox;
	GtkWidget * hseparator;
	GtkWidget * viewport;
	GtkWidget * scrollwin;
	GtkWidget * tag_error_view;
	GtkTreeViewColumn * column;
	GtkCellRenderer * renderer;

	tag_prog_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(tag_prog_window), _("Update file metadata"));
        gtk_window_set_position(GTK_WINDOW(tag_prog_window), GTK_WIN_POS_CENTER);
        gtk_window_resize(GTK_WINDOW(tag_prog_window), 500, 300);
        g_signal_connect(G_OBJECT(tag_prog_window), "delete_event",
                         G_CALLBACK(tag_prog_window_close), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(tag_prog_window), 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(tag_prog_window), vbox);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("File:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 5, 5);

        tag_prog_file_entry = gtk_entry_new();
        gtk_editable_set_editable(GTK_EDITABLE(tag_prog_file_entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), tag_prog_file_entry, 1, 2, 0, 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);

	tag_error_list = gtk_list_store_new(1, G_TYPE_STRING);
        tag_error_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tag_error_list));
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tag_error_view), FALSE);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Failed to set metadata at the following files:"),
							  renderer,
							  "text", 0,
							  NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tag_error_view), column);

	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        viewport = gtk_viewport_new(NULL, NULL);
	gtk_table_attach(GTK_TABLE(table), viewport, 0, 2, 1, 2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);
	gtk_container_add(GTK_CONTAINER(viewport), scrollwin);
        gtk_container_add(GTK_CONTAINER(scrollwin), tag_error_view);

        hseparator = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, TRUE, 5);

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        tag_prog_cancel_button = gui_stock_label_button (_("Abort"), GTK_STOCK_CANCEL); 
        g_signal_connect(tag_prog_cancel_button, "clicked", G_CALLBACK(cancel_batch_tag), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), tag_prog_cancel_button);   

        gtk_widget_grab_focus(tag_prog_cancel_button);

        gtk_widget_show_all(tag_prog_window);
}

gboolean
set_tag_prog_file_entry(gpointer data) {

	if (tag_prog_window) {

		char * utf8 = g_filename_display_name((char *)data);
		gtk_entry_set_text(GTK_ENTRY(tag_prog_file_entry), utf8);
		gtk_widget_grab_focus(tag_prog_cancel_button);
		g_free(utf8);
	}

	return FALSE;
}

gboolean
batch_tag_finish(gpointer data) {

	if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(tag_error_list), NULL) > 0) {
		gtk_entry_set_text(GTK_ENTRY(tag_prog_file_entry), "");
		gtk_button_set_label(GTK_BUTTON(tag_prog_cancel_button), GTK_STOCK_CLOSE);
		gtk_button_set_use_stock(GTK_BUTTON(tag_prog_cancel_button), TRUE);
	} else {
		tag_prog_window_close(NULL, NULL);
	}

	return FALSE;
}

gboolean
batch_tag_append_error(gpointer data) {

	GtkTreeIter iter;

	gtk_list_store_append(tag_error_list, &iter);
	gtk_list_store_set(tag_error_list, &iter, 0, (char *)data, -1);

	return FALSE;
}


void *
update_tag_thread(void * args) {

	batch_tag_t * ptag = (batch_tag_t *)args;
	batch_tag_t * _ptag = ptag;

	AQUALUNG_THREAD_DETACH()

	while (ptag) {

		if (batch_tag_cancelled) {

			while (ptag) {
				ptag = ptag->next;
				free(_ptag);
				_ptag = ptag;
			}			

			g_idle_add(batch_tag_finish, NULL);

			return NULL;
		}

		g_idle_add(set_tag_prog_file_entry, (gpointer)ptag->filename);

		if (meta_update_basic(ptag->filename,
				      (tag_flags & TAG_F_TITLE) ? ptag->title : NULL,
				      (tag_flags & TAG_F_ARTIST) ? ptag->artist : NULL,
				      (tag_flags & TAG_F_ALBUM) ? ptag->album : NULL,
				      (tag_flags & TAG_F_COMMENT) ? ptag->comment : NULL,
				      NULL /* genre */,
				      (tag_flags & TAG_F_YEAR) ? ptag->year : NULL,
				      (tag_flags & TAG_F_TRACKNO) ? ptag->track : NULL) < 0) {

			g_idle_add(batch_tag_append_error, (gpointer)ptag->filename);
		}

		ptag = ptag->next;
		free(_ptag);
		_ptag = ptag;
	}

	g_idle_add(batch_tag_finish, NULL);

	return NULL;
}

gboolean
track_batch_tag(gpointer data) {

	char * filename;
	char * title;
	char * comment;
	char * track;

	batch_tag_t * ptag;


	if ((ptag = (batch_tag_t *)calloc(1, sizeof(batch_tag_t))) == NULL) {
		fprintf(stderr, "music_store.c: track_batch_tag(): calloc error");
		return FALSE;
	}

	if (batch_tag_root == NULL) {
		batch_tag_root = batch_tag_curr = ptag;
	} else {
		batch_tag_curr->next = ptag;
		batch_tag_curr = ptag;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &track_tag_iter, 0, &title, 1, &track,
			   2, &filename, 3, &comment, -1);

	strncpy(ptag->artist, artist_tag, MAXLEN-1);
	strncpy(ptag->album, album_tag, MAXLEN-1);
	strncpy(ptag->year, year_tag, MAXLEN-1);

	strncpy(ptag->filename, filename, MAXLEN-1);
	strncpy(ptag->title, title, MAXLEN-1);
	strncpy(ptag->track, track, MAXLEN-1);
	strncpy(ptag->comment, comment, MAXLEN-1);

	g_free(filename);
	g_free(title);
	g_free(comment);
	g_free(track);

	if (data) {
		batch_tag_cancelled = 0;
		create_tag_prog_window();
		AQUALUNG_THREAD_CREATE(tag_thread_id, NULL, update_tag_thread, batch_tag_root)
	}

	return FALSE;
}

void
record_batch_tag_set_from_iter(GtkTreeIter * iter) {

	char * str;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, 0, &str, -1);
	strncpy(album_tag, str, MAXLEN-1);
	g_free(str);

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, 1, &str, -1);
	strncpy(year_tag, str, MAXLEN-1);
	g_free(str);
}

gboolean
record_batch_tag(gpointer data) {

	GtkTreeIter iter_track;
	int i = 0;

	record_batch_tag_set_from_iter(&record_tag_iter);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &iter_track, &record_tag_iter, i++)) {
		track_tag_iter = iter_track;
		track_batch_tag(NULL);
	}

	if (data) {
		batch_tag_cancelled = 0;
		create_tag_prog_window();
		AQUALUNG_THREAD_CREATE(tag_thread_id, NULL, update_tag_thread, batch_tag_root)
	}

	return FALSE;
}

void
artist_batch_tag_set_from_iter(GtkTreeIter * iter) {

	char * str;
	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter, 0, &str, -1);
	strncpy(artist_tag, str, MAXLEN-1);
	g_free(str);
}

gboolean
artist_batch_tag(gpointer data) {

	GtkTreeIter iter_record;
	int i = 0;

	artist_batch_tag_set_from_iter(&artist_tag_iter);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &iter_record, &artist_tag_iter, i++)) {
		record_tag_iter = iter_record;
		record_batch_tag(NULL);
	}

	if (data) {
		batch_tag_cancelled = 0;
		create_tag_prog_window();
		AQUALUNG_THREAD_CREATE(tag_thread_id, NULL, update_tag_thread, batch_tag_root)
	}

	return FALSE;
}

gboolean
store_batch_tag(gpointer data) {

	GtkTreeIter iter_artist;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &iter_artist, &store_tag_iter, i++)) {
		artist_tag_iter = iter_artist;
		artist_batch_tag(NULL);
	}

	if (data) {
		batch_tag_cancelled = 0;
		create_tag_prog_window();
		AQUALUNG_THREAD_CREATE(tag_thread_id, NULL, update_tag_thread, batch_tag_root)
	}

	return FALSE;
}

void
track__tag_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		if (create_tag_dialog()) {

			GtkTreeIter iter_record;
			GtkTreeIter iter_artist;

			gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store),
						   &iter_record, &iter);
			record_batch_tag_set_from_iter(&iter_record);

			gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store),
						   &iter_artist, &iter_record);
			artist_batch_tag_set_from_iter(&iter_artist);

			track_tag_iter = iter;
			g_idle_add(track_batch_tag, (gpointer)1);
		}
	}
}

void
record__tag_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		if (create_tag_dialog()) {

			GtkTreeIter iter_artist;
			gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store),
						   &iter_artist, &iter);

			artist_batch_tag_set_from_iter(&iter_artist);

			record_tag_iter = iter;
			g_idle_add(record_batch_tag, (gpointer)1);
		}
	}
}

void
artist__tag_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		if (create_tag_dialog()) {
			artist_tag_iter = iter;
			g_idle_add(artist_batch_tag, (gpointer)1);
		}
	}
}

void
store__tag_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		if (create_tag_dialog()) {
			store_tag_iter = iter;
			g_idle_add(store_batch_tag, (gpointer)1);
		}
	}
}


#endif /* HAVE_TAGLIB && HAVE_METAEDIT */


/************************************/


void
set_comment_text(GtkTextIter * iter) {

	GtkTreeModel * model;
	GtkTreeIter iter_tree;
	GtkTextBuffer * buffer;
	char * comment;


	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_tree)) {
		gtk_tree_model_get(model, &iter_tree, 3, &comment, -1);
		if (comment[0] != '\0') {
			gtk_text_buffer_insert(buffer, iter, comment, -1);
		} else {
			gtk_text_buffer_insert(buffer, iter, _("(no comment)"), -1);
		}
                g_free(comment);
	} else {
		gtk_text_buffer_insert(buffer, iter, "", -1);
	}

	gtk_text_view_set_buffer(GTK_TEXT_VIEW(comment_view), buffer);
}

void
set_comment_content(void) {

	GtkTextBuffer * buffer;
        GtkTextIter iter, a_iter, b_iter;


	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));

        gtk_text_buffer_get_bounds(buffer, &a_iter, &b_iter);
	gtk_text_buffer_delete(buffer, &a_iter, &b_iter);  
        gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);

	insert_cover(&iter);
	set_comment_text(&iter);
}


#ifdef HAVE_CDDA
void
update_cdda_status_bar(void) {

	GtkTreeModel * model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {
		if (is_store_iter_cdda(&iter)) {
			music_store_set_status_bar_info();
		}
	}
}
#endif /* HAVE_CDDA */


void
music_store_set_status_bar_info(void) {

	GtkTreePath * path;
	GtkTreeModel * model;
	GtkTreeIter iter;
	GtkTreeIter iter_artist;
	GtkTreeIter iter_record;
	GtkTreeIter iter_track;

	int n_artist = 0;
	int n_record = 0;
	int n_track = 0;

	int i, j, k;

	float len = 0.0f;
	float duration = 0.0f;
	char * file;
	float size = 0;
	
	char duration_str[MAXLEN];
	char str[MAXLEN];

	struct stat statbuf;


	if (!options.enable_mstore_statusbar) return;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		int depth;
		path = gtk_tree_model_get_path(model, &iter);
		depth = gtk_tree_path_get_depth(path);

#ifdef HAVE_CDDA
		if (is_store_path_cdda(path))
			depth += 1;
#endif /* HAVE_CDDA */

		switch (depth) {
		case 1: /* music store */
			i = 0;

			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_artist, &iter, i++)) {
				n_artist++;

				j = 0;
				while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_record,
								     &iter_artist, j++)) {
					n_record++;

					k = 0;
					while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track,
									     &iter_record, k++)) {
						n_track++;
                                                gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track, 2, &file, 4, &len, -1);
						duration += len;
						
			                        if (options.ms_statusbar_show_size) {
                                                        if (stat(file, &statbuf) != -1) {
                                                                size += statbuf.st_size / 1024.0;
                                                        } 
                                                }

                                                g_free(file);
					}
				}
			}

			if (duration > 0.0f) {
				time2time(duration, duration_str);
			} else {
				strcpy(duration_str, _("time unmeasured"));
			}

			if (options.ms_statusbar_show_size) {
				if ((size /= 1024.0) < 1024) {
					snprintf(str, MAXLEN-1,
						 ("%d %s, %d %s, %d %s [%s] (%.1f MB)"),
						 n_artist,
						 (n_artist > 1) ? _("artists") : _("artist"),
						 n_record,
						 (n_record > 1) ? _("records") : _("record"),
						 n_track,
						 (n_track > 1) ? _("tracks") : _("track"),
						 duration_str, size);
				} else {
					snprintf(str, MAXLEN-1,
						 ("%d %s, %d %s, %d %s [%s] (%.1f GB)"),
						 n_artist,
						 (n_artist > 1) ? _("artists") : _("artist"),
						 n_record,
						 (n_record > 1) ? _("records") : _("record"),
						 n_track,
						 (n_track > 1) ? _("tracks") : _("track"),
						 duration_str, size / 1024.0f);
				}
			} else {
				snprintf(str, MAXLEN-1,
					 ("%d %s, %d %s, %d %s [%s] "),
					 n_artist, (n_artist > 1) ? _("artists") : _("artist"),
					 n_record, (n_record > 1) ? _("records") : _("record"),
					 n_track, (n_track > 1) ? _("tracks") : _("track"),
					 duration_str);
			}

			break;
		case 2: /* artist */
			j = 0;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_record,
							     &iter, j++)) {
				n_record++;
				
				k = 0;
				while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track,
								     &iter_record, k++)) {
					n_track++;
					gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track, 2, &file, 4, &len, -1);
					duration += len;

                                        if (options.ms_statusbar_show_size) {
                                                if (stat(file, &statbuf) != -1) {
                                                        size += statbuf.st_size / 1024.0;
                                                }
                                        }

                                        g_free(file);
				}
			}

			if (duration > 0.0f) {
				time2time(duration, duration_str);
			} else {
				strcpy(duration_str, _("time unmeasured"));
			}

#ifdef HAVE_CDDA
			if (options.ms_statusbar_show_size && !is_store_path_cdda(path)) {
#else
			if (options.ms_statusbar_show_size) {
#endif /* HAVE_CDDA */
				if ((size /= 1024.0) < 1024) {
					snprintf(str, MAXLEN-1, "%d %s, %d %s [%s] (%.1f MB)",
						 n_record,
						 (n_record > 1) ? _("records") : _("record"),
						 n_track,
						 (n_track > 1) ? _("tracks") : _("track"),
						 duration_str, size);
				} else {
					snprintf(str, MAXLEN-1, "%d %s, %d %s [%s] (%.1f GB)",
						 n_record,
						 (n_record > 1) ? _("records") : _("record"),
						 n_track,
						 (n_track > 1) ? _("tracks") : _("track"),
						 duration_str, size / 1024.0f);
				}
			} else {
				snprintf(str, MAXLEN-1, "%d %s, %d %s [%s] ",
					 n_record, (n_record > 1) ? _("records") : _("record"),
					 n_track, (n_track > 1) ? _("tracks") : _("track"),
					 duration_str);
			}

			break;
		case 3: /* record */
			k = 0;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track,
							     &iter, k++)) {
				n_track++;
				gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track, 2, &file, 4, &len, -1);
				duration += len;

			        if (options.ms_statusbar_show_size) {
                                        if (stat(file, &statbuf) != -1) {
                                                size += statbuf.st_size / 1024.0;
                                        }
                                }

                                g_free(file);
			}

			if (duration > 0.0f) {
				time2time(duration, duration_str);
			} else {
				strcpy(duration_str, _("time unmeasured"));
			}

#ifdef HAVE_CDDA
			if (options.ms_statusbar_show_size && !is_store_path_cdda(path)) {
#else
			if (options.ms_statusbar_show_size) {
#endif /* HAVE_CDDA */
				if ((size /= 1024.0) < 1024) {
					snprintf(str, MAXLEN-1, "%d %s [%s] (%.1f MB)",
						 n_track,
						 (n_track > 1) ? _("tracks") : _("track"),
						 duration_str, size);
				} else {
					snprintf(str, MAXLEN-1, "%d %s [%s] (%.1f GB)",
						 n_track,
						 (n_track > 1) ? _("tracks") : _("track"),
						 duration_str, size / 1024.0f);
				}
			} else {
				snprintf(str, MAXLEN-1, "%d %s [%s] ",
					 n_track, (n_track > 1) ? _("tracks") : _("track"),
					 duration_str);
			}

			break;
		case 4: /* track */
			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, 2, &file, 4, &duration, -1);

			if (duration > 0.0f) {
				time2time(duration, duration_str);
			} else {
				strcpy(duration_str, _("time unmeasured"));
			}

			if (options.ms_statusbar_show_size) {
                                if (stat(file, &statbuf) != -1) {
                                        size += statbuf.st_size / 1024.0;
                                }
                        }

                        g_free(file);

#ifdef HAVE_CDDA
			if (options.ms_statusbar_show_size && !is_store_path_cdda(path)) {
#else
			if (options.ms_statusbar_show_size) {
#endif /* HAVE_CDDA */
				if ((size /= 1024.0) < 1024) {
					snprintf(str, MAXLEN-1, "1 %s [%s] (%.1f MB)",
						 _("track"), duration_str, size);
				} else {
					snprintf(str, MAXLEN-1, "1 %s [%s] (%.1f GB)",
						 _("track"), duration_str, size / 1024.0f);
				}
			} else {
				snprintf(str, MAXLEN-1, "1 %s [%s] ", _("track"), duration_str);
			}

			break;
		}
		gtk_tree_path_free(path);

	} else {
		str[0] = '\0';
	}

	gtk_label_set_text(GTK_LABEL(statusbar_ms), str);
}


void
tree_selection_changed_cb(GtkTreeSelection * selection, gpointer data) {

	set_comment_content();
	music_store_set_status_bar_info();
}


gint
row_collapsed_cb(GtkTreeView * view, GtkTreeIter * iter1, GtkTreePath * path1) {

        GtkTreeIter iter2;

        if (!gtk_tree_selection_get_selected(music_select, NULL, &iter2)) {
		set_comment_content();
		music_store_set_status_bar_info();
	}

	return FALSE;
}


void
browser_drag_data_get(GtkWidget * widget, GdkDragContext * drag_context,
		      GtkSelectionData * data, guint info, guint time, gpointer user_data) {

	gtk_selection_data_set(data, data->target, 8, (const guchar *) "store\0", 6);
}


void
browser_drag_end(GtkWidget * widget, GdkDragContext * drag_context, gpointer data) {

	playlist_drag_end(widget, drag_context, data);
}


void
music_tree_expand_stores(void) {

	GtkTreeIter iter_store;
	GtkTreePath * path = NULL;
	int i = 0;

	if (!options.autoexpand_stores) return;

	i = 0;
        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_store, NULL, i++)) {
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), &iter_store);
		gtk_tree_view_expand_row(GTK_TREE_VIEW(music_tree), path, FALSE);
		gtk_tree_path_free(path);
	}
}


void
create_music_browser(void) {
	
	GtkWidget * vbox;
        GtkWidget * hbox;
	GtkWidget * viewport1;
	GtkWidget * viewport2;
	GtkWidget * scrolled_win1;
	GtkWidget * scrolled_win2;
	GtkWidget * statusbar_viewport;
	GtkWidget * statusbar_scrolledwin;
	GtkWidget * statusbar_hbox;
	GtkWidget * toolbar_search_button;
	GtkWidget * toolbar_collapse_all_button;

	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTextBuffer * buffer;

	GdkPixbuf * pixbuf;
	char path[MAXLEN];

	GtkTargetEntry target_table[] = {
		{ "STRING", GTK_TARGET_SAME_APP, 0 }
	};


	/* window creating stuff */
	browser_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(browser_window), _("Music Store"));
	g_signal_connect(G_OBJECT(browser_window), "delete_event", G_CALLBACK(browser_window_close), NULL);
	g_signal_connect(G_OBJECT(browser_window), "key_press_event",
			 G_CALLBACK(browser_window_key_pressed), NULL);
	gtk_container_set_border_width(GTK_CONTAINER(browser_window), 2);
        gtk_widget_set_size_request(browser_window, 200, 300);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(browser_window), vbox);

        if (options.enable_mstore_toolbar) {

                hbox = gtk_hbox_new(FALSE, 0);
                gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 3);

                toolbar_search_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_FIND);
                GTK_WIDGET_UNSET_FLAGS(toolbar_search_button, GTK_CAN_FOCUS);
                g_signal_connect(G_OBJECT(toolbar_search_button), "clicked", G_CALLBACK(search_cb), NULL);
                gtk_box_pack_start(GTK_BOX(hbox), toolbar_search_button, FALSE, TRUE, 3);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), toolbar_search_button, _("Search..."), NULL);

                toolbar_collapse_all_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_REFRESH);
                GTK_WIDGET_UNSET_FLAGS(toolbar_collapse_all_button, GTK_CAN_FOCUS);
                gtk_box_pack_start(GTK_BOX(hbox), toolbar_collapse_all_button, FALSE, TRUE, 3);
                g_signal_connect(G_OBJECT(toolbar_collapse_all_button), "pressed", G_CALLBACK(collapse_all_items_cb), NULL);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), toolbar_collapse_all_button, _("Collapse all items"), NULL);

                toolbar_edit_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_EDIT);
                GTK_WIDGET_UNSET_FLAGS(toolbar_edit_button, GTK_CAN_FOCUS);
                gtk_box_pack_start(GTK_BOX(hbox), toolbar_edit_button, FALSE, TRUE, 3);
                g_signal_connect(G_OBJECT(toolbar_edit_button), "pressed", G_CALLBACK(edit_item_cb), NULL);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), toolbar_edit_button, _("Edit item..."), NULL);

                toolbar_add_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_ADD);
                GTK_WIDGET_UNSET_FLAGS(toolbar_add_button, GTK_CAN_FOCUS);
                gtk_box_pack_start(GTK_BOX(hbox), toolbar_add_button, FALSE, TRUE, 3);
                g_signal_connect(G_OBJECT(toolbar_add_button), "pressed", G_CALLBACK(add_item_cb), NULL);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), toolbar_add_button, _("Add item..."), NULL);

                toolbar_remove_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_REMOVE);
                GTK_WIDGET_UNSET_FLAGS(toolbar_remove_button, GTK_CAN_FOCUS);
                gtk_box_pack_start(GTK_BOX(hbox), toolbar_remove_button, FALSE, TRUE, 3);
                g_signal_connect(G_OBJECT(toolbar_remove_button), "pressed", G_CALLBACK(remove_item_cb), NULL);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), toolbar_remove_button, _("Remove item..."), NULL);

                toolbar_save_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_SAVE);
                GTK_WIDGET_UNSET_FLAGS(toolbar_save_button, GTK_CAN_FOCUS);
                gtk_box_pack_start(GTK_BOX(hbox), toolbar_save_button, FALSE, TRUE, 3);
                g_signal_connect(G_OBJECT(toolbar_save_button), "pressed", G_CALLBACK(store__save_all_cb), NULL);
                gtk_widget_set_sensitive(toolbar_save_button, FALSE);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), toolbar_save_button, _("Save all stores"), NULL);
        }


	if (!options.hide_comment_pane) {
		browser_paned = gtk_vpaned_new();
		gtk_box_pack_start(GTK_BOX(vbox), browser_paned, TRUE, TRUE, 0);
	}

	/* load tree icons */
        if (options.enable_ms_tree_icons) {
                sprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-store.png");
                icon_store = gdk_pixbuf_new_from_file (path, NULL);
                sprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-artist.png");
                icon_artist = gdk_pixbuf_new_from_file (path, NULL);
                sprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-record.png");
                icon_record = gdk_pixbuf_new_from_file (path, NULL);
                sprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-track.png");
                icon_track = gdk_pixbuf_new_from_file (path, NULL);
                sprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-cdda.png");
                icon_cdda = gdk_pixbuf_new_from_file (path, NULL);
                sprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-cdda-disk.png");
                icon_cdda_disc = gdk_pixbuf_new_from_file (path, NULL);
                sprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-cdda-nodisk.png");
                icon_cdda_nodisc = gdk_pixbuf_new_from_file (path, NULL);
        }

	/* create music store tree */
	if (!music_store) {
		music_store = gtk_tree_store_new(10,
						 G_TYPE_STRING,  /* visible name */
						 G_TYPE_STRING,  /* string to sort by */
						 G_TYPE_STRING,  /* physical filename (stores&tracks) */
						 G_TYPE_STRING,  /* user comments */
						 G_TYPE_FLOAT,   /* track length in seconds */
						 G_TYPE_FLOAT,   /* track average volume in dBFS */
						 G_TYPE_FLOAT,   /* track manual volume adjustment, dB
								    if >= 0: saved store
								    if < 0: changed store */
						 G_TYPE_FLOAT,   /* if >= 0: use track manual RVA, 
								    if < 0: auto (compute from avg. loudness);
								    if >= 0: writable store,
								    if < 0: readonly store */
                			         G_TYPE_INT,     /* font weight */
                                                 GDK_TYPE_PIXBUF);    /* icon */
	}

	music_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(music_store));
	gtk_widget_set_name(music_tree, "music_tree");
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(music_tree), FALSE);

        if (options.override_skin_settings) {
                gtk_widget_modify_font(music_tree, fd_browser);
        }

        if (options.enable_ms_rules_hint) {
                gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(music_tree), TRUE);
        }

        column = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title (column, "Artist / Record / Track");
        renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_tree_view_column_pack_start (column, renderer, FALSE);
        gtk_tree_view_column_add_attribute (column, renderer, "pixbuf", 9);
        renderer = gtk_cell_renderer_text_new ();
        gtk_tree_view_column_pack_start (column, renderer, TRUE);
        gtk_tree_view_column_add_attribute (column, renderer, "text", 0);
        gtk_tree_view_column_add_attribute (column, renderer, "weight", 8);
        g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(music_tree), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(music_tree), FALSE);

	viewport1 = gtk_viewport_new(NULL, NULL);
	if (!options.hide_comment_pane) {
		gtk_paned_pack1(GTK_PANED(browser_paned), viewport1, TRUE, TRUE);
	} else {
		gtk_box_pack_start(GTK_BOX(vbox), viewport1, TRUE, TRUE, 0);
	}

	scrolled_win1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrolled_win1, -1, 1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win1),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(viewport1), scrolled_win1);

	gtk_container_add(GTK_CONTAINER(scrolled_win1), music_tree);

	music_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(music_tree));
	gtk_tree_selection_set_mode(music_select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(music_select), "changed", G_CALLBACK(tree_selection_changed_cb), NULL);
	g_signal_connect(G_OBJECT(music_tree), "row_collapsed", G_CALLBACK(row_collapsed_cb), NULL);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(music_store), 1, GTK_SORT_ASCENDING);

	music_tree_expand_stores();

	/* setup drag and drop */
	gtk_drag_source_set(music_tree,
			    GDK_BUTTON1_MASK,
			    target_table,
			    1,
			    GDK_ACTION_COPY);

	snprintf(path, MAXLEN-1, "%s/drag.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		gtk_drag_source_set_icon_pixbuf(music_tree, pixbuf);
	}	

	g_signal_connect(G_OBJECT(music_tree), "drag_data_get", G_CALLBACK(browser_drag_data_get), NULL);
	g_signal_connect(G_OBJECT(music_tree), "drag_end", G_CALLBACK(browser_drag_end), NULL);


        /* create popup menu for blank space */
        blank_menu = gtk_menu_new();
        blank__add = gtk_menu_item_new_with_label(_("Create empty store..."));
        blank__search = gtk_menu_item_new_with_label(_("Search..."));
        blank__save = gtk_menu_item_new_with_label(_("Save all stores"));
        gtk_menu_shell_append(GTK_MENU_SHELL(blank_menu), blank__add);
        gtk_menu_shell_append(GTK_MENU_SHELL(blank_menu), blank__search);
        gtk_menu_shell_append(GTK_MENU_SHELL(blank_menu), blank__save);
        g_signal_connect_swapped(G_OBJECT(blank__add), "activate", G_CALLBACK(store__add_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(blank__search), "activate", G_CALLBACK(search_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(blank__save), "activate", G_CALLBACK(store__save_cb), NULL);
        gtk_widget_show(blank__add);
        gtk_widget_show(blank__search);
        gtk_widget_show(blank__save);

	/* create popup menu for music store tree items */
	store_menu = gtk_menu_new();
	store__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	store__addlist_albummode = gtk_menu_item_new_with_label(_("Add to playlist (Album mode)"));
	store__separator1 = gtk_separator_menu_item_new();
	store__add = gtk_menu_item_new_with_label(_("Create empty store..."));
	store__build = gtk_menu_item_new_with_label(_("Build/Update store from filesystem..."));
	store__edit = gtk_menu_item_new_with_label(_("Edit store..."));
	store__save = gtk_menu_item_new_with_label(_("Save store"));
	store__remove = gtk_menu_item_new_with_label(_("Remove store"));
	store__separator2 = gtk_separator_menu_item_new();
	store__addart = gtk_menu_item_new_with_label(_("Add new artist to this store..."));
	store__separator3 = gtk_separator_menu_item_new();
	store__volume = gtk_menu_item_new_with_label(_("Calculate volume (recursive)"));
	store__volume_menu = gtk_menu_new();
	store__volume_unmeasured = gtk_menu_item_new_with_label(_("Unmeasured tracks only"));
	store__volume_all = gtk_menu_item_new_with_label(_("All tracks"));
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	store__tag = gtk_menu_item_new_with_label(_("Batch-update file metadata..."));
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	store__search = gtk_menu_item_new_with_label(_("Search..."));

	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__addlist_albummode);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__add);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__build);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__edit);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__save);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__remove);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__addart);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__separator3);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__volume);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(store__volume), store__volume_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(store__volume_menu), store__volume_unmeasured);
        gtk_menu_shell_append(GTK_MENU_SHELL(store__volume_menu), store__volume_all);
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__tag);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__search);

	g_signal_connect_swapped(G_OBJECT(store__addlist), "activate", G_CALLBACK(store__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__addlist_albummode), "activate", G_CALLBACK(store__addlist_albummode_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__add), "activate", G_CALLBACK(store__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__build), "activate", G_CALLBACK(store__build_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__edit), "activate", G_CALLBACK(store__edit_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__save), "activate", G_CALLBACK(store__save_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__remove), "activate", G_CALLBACK(store__remove_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__addart), "activate", G_CALLBACK(artist__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__volume_unmeasured), "activate", G_CALLBACK(store__volume_unmeasured_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__volume_all), "activate", G_CALLBACK(store__volume_all_cb), NULL);
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	g_signal_connect_swapped(G_OBJECT(store__tag), "activate", G_CALLBACK(store__tag_cb), NULL);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	g_signal_connect_swapped(G_OBJECT(store__search), "activate", G_CALLBACK(search_cb), NULL);

	gtk_widget_show(store__addlist);
	gtk_widget_show(store__addlist_albummode);
	gtk_widget_show(store__separator1);
	gtk_widget_show(store__add);
	gtk_widget_show(store__build);
	gtk_widget_show(store__edit);
	gtk_widget_show(store__save);
	gtk_widget_show(store__remove);
	gtk_widget_show(store__separator2);
	gtk_widget_show(store__addart);
	gtk_widget_show(store__separator3);
	gtk_widget_show(store__volume);
	gtk_widget_show(store__volume_unmeasured);
	gtk_widget_show(store__volume_all);
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	gtk_widget_show(store__tag);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	gtk_widget_show(store__search);

	/* create popup menu for artist tree items */
	artist_menu = gtk_menu_new();
	artist__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	artist__addlist_albummode = gtk_menu_item_new_with_label(_("Add to playlist (Album mode)"));
	artist__separator1 = gtk_separator_menu_item_new();
	artist__add = gtk_menu_item_new_with_label(_("Add new artist..."));
	artist__build = gtk_menu_item_new_with_label(_("Build/Update artist from filesystem..."));
	artist__edit = gtk_menu_item_new_with_label(_("Edit artist..."));
	artist__remove = gtk_menu_item_new_with_label(_("Remove artist"));
	artist__separator2 = gtk_separator_menu_item_new();
	artist__addrec = gtk_menu_item_new_with_label(_("Add new record to this artist..."));
	artist__separator3 = gtk_separator_menu_item_new();
	artist__volume = gtk_menu_item_new_with_label(_("Calculate volume (recursive)"));
	artist__volume_menu = gtk_menu_new();
	artist__volume_unmeasured = gtk_menu_item_new_with_label(_("Unmeasured tracks only"));
	artist__volume_all = gtk_menu_item_new_with_label(_("All tracks"));
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	artist__tag = gtk_menu_item_new_with_label(_("Batch-update file metadata..."));
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	artist__search = gtk_menu_item_new_with_label(_("Search..."));

	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__addlist_albummode);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__add);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__build);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__edit);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__remove);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__addrec);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__separator3);
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__volume);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(artist__volume), artist__volume_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(artist__volume_menu), artist__volume_unmeasured);
        gtk_menu_shell_append(GTK_MENU_SHELL(artist__volume_menu), artist__volume_all);
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__tag);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	gtk_menu_shell_append(GTK_MENU_SHELL(artist_menu), artist__search);

	g_signal_connect_swapped(G_OBJECT(artist__addlist), "activate", G_CALLBACK(artist__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__addlist_albummode), "activate", G_CALLBACK(artist__addlist_albummode_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__add), "activate", G_CALLBACK(artist__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__build), "activate", G_CALLBACK(artist__build_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__edit), "activate", G_CALLBACK(artist__edit_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__remove), "activate", G_CALLBACK(artist__remove_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__addrec), "activate", G_CALLBACK(record__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__volume_unmeasured), "activate", G_CALLBACK(artist__volume_unmeasured_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(artist__volume_all), "activate", G_CALLBACK(artist__volume_all_cb), NULL);
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	g_signal_connect_swapped(G_OBJECT(artist__tag), "activate", G_CALLBACK(artist__tag_cb), NULL);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	g_signal_connect_swapped(G_OBJECT(artist__search), "activate", G_CALLBACK(search_cb), NULL);

	gtk_widget_show(artist__addlist);
	gtk_widget_show(artist__addlist_albummode);
	gtk_widget_show(artist__separator1);
	gtk_widget_show(artist__add);
	gtk_widget_show(artist__build);
	gtk_widget_show(artist__edit);
	gtk_widget_show(artist__remove);
	gtk_widget_show(artist__separator2);
	gtk_widget_show(artist__addrec);
	gtk_widget_show(artist__separator3);
	gtk_widget_show(artist__volume);
	gtk_widget_show(artist__volume_unmeasured);
	gtk_widget_show(artist__volume_all);
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	gtk_widget_show(artist__tag);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	gtk_widget_show(artist__search);

	/* create popup menu for record tree items */
	record_menu = gtk_menu_new();
	record__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	record__addlist_albummode = gtk_menu_item_new_with_label(_("Add to playlist (Album mode)"));
	record__separator1 = gtk_separator_menu_item_new();
	record__add = gtk_menu_item_new_with_label(_("Add new record..."));
	record__edit = gtk_menu_item_new_with_label(_("Edit record..."));
	record__remove = gtk_menu_item_new_with_label(_("Remove record"));
	record__separator2 = gtk_separator_menu_item_new();
	record__addtrk = gtk_menu_item_new_with_label(_("Add new track to this record..."));
#ifdef HAVE_CDDB
	record__cddb = gtk_menu_item_new_with_label(_("CDDB query for this record..."));
	record__cddb_submit = gtk_menu_item_new_with_label(_("Submit record to CDDB database..."));
#endif /* HAVE_CDDB */
	record__separator3 = gtk_separator_menu_item_new();
	record__volume = gtk_menu_item_new_with_label(_("Calculate volume (recursive)"));
	record__volume_menu = gtk_menu_new();
	record__volume_unmeasured = gtk_menu_item_new_with_label(_("Unmeasured tracks only"));
	record__volume_all = gtk_menu_item_new_with_label(_("All tracks"));
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	record__tag = gtk_menu_item_new_with_label(_("Batch-update file metadata..."));
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	record__search = gtk_menu_item_new_with_label(_("Search..."));

	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__addlist_albummode);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__add);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__edit);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__remove);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__addtrk);
#ifdef HAVE_CDDB
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__cddb);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__cddb_submit);
#endif /* HAVE_CDDB */
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__separator3);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__volume);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(record__volume), record__volume_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(record__volume_menu), record__volume_unmeasured);
        gtk_menu_shell_append(GTK_MENU_SHELL(record__volume_menu), record__volume_all);
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__tag);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__search);

	g_signal_connect_swapped(G_OBJECT(record__addlist), "activate", G_CALLBACK(record__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__addlist_albummode), "activate", G_CALLBACK(record__addlist_albummode_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__add), "activate", G_CALLBACK(record__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__edit), "activate", G_CALLBACK(record__edit_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__remove), "activate", G_CALLBACK(record__remove_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__addtrk), "activate", G_CALLBACK(track__add_cb), NULL);
#ifdef HAVE_CDDB
	g_signal_connect_swapped(G_OBJECT(record__cddb), "activate", G_CALLBACK(record__cddb_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__cddb_submit), "activate", G_CALLBACK(record__cddb_submit_cb), NULL);
#endif /* HAVE_CDDB */
	g_signal_connect_swapped(G_OBJECT(record__volume_unmeasured), "activate", G_CALLBACK(record__volume_unmeasured_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__volume_all), "activate", G_CALLBACK(record__volume_all_cb), NULL);
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	g_signal_connect_swapped(G_OBJECT(record__tag), "activate", G_CALLBACK(record__tag_cb), NULL);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	g_signal_connect_swapped(G_OBJECT(record__search), "activate", G_CALLBACK(search_cb), NULL);

	gtk_widget_show(record__addlist);
	gtk_widget_show(record__addlist_albummode);
	gtk_widget_show(record__separator1);
	gtk_widget_show(record__add);
	gtk_widget_show(record__edit);
	gtk_widget_show(record__remove);
	gtk_widget_show(record__separator2);
	gtk_widget_show(record__addtrk);
#ifdef HAVE_CDDB
	gtk_widget_show(record__cddb);
	gtk_widget_show(record__cddb_submit);
#endif /* HAVE_CDDB */
	gtk_widget_show(record__separator3);
	gtk_widget_show(record__volume);
	gtk_widget_show(record__volume_unmeasured);
	gtk_widget_show(record__volume_all);
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	gtk_widget_show(record__tag);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	gtk_widget_show(record__search);

	/* create popup menu for track tree items */
	track_menu = gtk_menu_new();
	track__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	track__separator1 = gtk_separator_menu_item_new();
	track__add = gtk_menu_item_new_with_label(_("Add new track..."));
	track__edit = gtk_menu_item_new_with_label(_("Edit track..."));
	track__remove = gtk_menu_item_new_with_label(_("Remove track"));
	track__separator2 = gtk_separator_menu_item_new();
	track__fileinfo = gtk_menu_item_new_with_label(_("File info..."));
	track__separator3 = gtk_separator_menu_item_new();
	track__volume = gtk_menu_item_new_with_label(_("Calculate volume"));
	track__volume_menu = gtk_menu_new();
	track__volume_unmeasured = gtk_menu_item_new_with_label(_("Only if unmeasured"));
	track__volume_all = gtk_menu_item_new_with_label(_("In any case"));
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	track__tag = gtk_menu_item_new_with_label(_("Update file metadata..."));
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	track__search = gtk_menu_item_new_with_label(_("Search..."));

	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__add);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__edit);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__remove);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__fileinfo);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__separator3);
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__volume);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(track__volume), track__volume_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(track__volume_menu), track__volume_unmeasured);
        gtk_menu_shell_append(GTK_MENU_SHELL(track__volume_menu), track__volume_all);
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__tag);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__search);

	g_signal_connect_swapped(G_OBJECT(track__addlist), "activate", G_CALLBACK(track__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__add), "activate", G_CALLBACK(track__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__edit), "activate", G_CALLBACK(track__edit_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__remove), "activate", G_CALLBACK(track__remove_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__fileinfo), "activate", G_CALLBACK(track__fileinfo_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__volume_unmeasured), "activate", G_CALLBACK(track__volume_unmeasured_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__volume_all), "activate", G_CALLBACK(track__volume_all_cb), NULL);
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	g_signal_connect_swapped(G_OBJECT(track__tag), "activate", G_CALLBACK(track__tag_cb), NULL);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	g_signal_connect_swapped(G_OBJECT(track__search), "activate", G_CALLBACK(search_cb), NULL);

	gtk_widget_show(track__addlist);
	gtk_widget_show(track__separator1);
	gtk_widget_show(track__add);
	gtk_widget_show(track__edit);
	gtk_widget_show(track__remove);
	gtk_widget_show(track__separator2);
	gtk_widget_show(track__fileinfo);
	gtk_widget_show(track__separator3);
	gtk_widget_show(track__volume);
	gtk_widget_show(track__volume_unmeasured);
	gtk_widget_show(track__volume_all);
#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
	gtk_widget_show(track__tag);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */
	gtk_widget_show(track__search);

#ifdef HAVE_CDDA
	/* create popup menu for cdda_record tree items */
	cdda_record_menu = gtk_menu_new();
	cdda_record__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	cdda_record__addlist_albummode = gtk_menu_item_new_with_label(_("Add to playlist (Album mode)"));
	cdda_record__separator1 = gtk_separator_menu_item_new();
#ifdef HAVE_CDDB
	cdda_record__cddb = gtk_menu_item_new_with_label(_("CDDB query for this CD..."));
	cdda_record__cddb_submit = gtk_menu_item_new_with_label(_("Submit CD to CDDB database..."));
#endif /* HAVE_CDDB */
	cdda_record__rip = gtk_menu_item_new_with_label(_("Rip CD..."));
	cdda_record__disc_info = gtk_menu_item_new_with_label(_("Disc info..."));
	cdda_record__separator2 = gtk_separator_menu_item_new();
	cdda_record__drive_info = gtk_menu_item_new_with_label(_("Drive info..."));

	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__addlist_albummode);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__separator1);
#ifdef HAVE_CDDB
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__cddb);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__cddb_submit);
#endif /* HAVE_CDDB */
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__rip);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__disc_info);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_record_menu), cdda_record__drive_info);

 	g_signal_connect_swapped(G_OBJECT(cdda_record__addlist), "activate", G_CALLBACK(record__addlist_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(cdda_record__addlist_albummode), "activate", G_CALLBACK(record__addlist_albummode_cb), NULL);
#ifdef HAVE_CDDB
	g_signal_connect_swapped(G_OBJECT(cdda_record__cddb), "activate", G_CALLBACK(record__cddb_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(cdda_record__cddb_submit), "activate", G_CALLBACK(record__cddb_submit_cb), NULL);
#endif /* HAVE_CDDB */
/* 	g_signal_connect_swapped(G_OBJECT(cdda_record__rip), "activate", G_CALLBACK(cdda_record__rip_cb), NULL); */
/* 	g_signal_connect_swapped(G_OBJECT(cdda_record__disc_info), "activate", G_CALLBACK(cdda_record__disc_info_cb), NULL); */
 	g_signal_connect_swapped(G_OBJECT(cdda_record__drive_info), "activate", G_CALLBACK(cdda_record__drive_cb), NULL);

	gtk_widget_show(cdda_record__addlist);
	gtk_widget_show(cdda_record__addlist_albummode);
	gtk_widget_show(cdda_record__separator1);
#ifdef HAVE_CDDB
	gtk_widget_show(cdda_record__cddb);
	gtk_widget_show(cdda_record__cddb_submit);
#endif /* HAVE_CDDB */
	gtk_widget_show(cdda_record__rip);
	gtk_widget_show(cdda_record__disc_info);
	gtk_widget_show(cdda_record__separator2);
	gtk_widget_show(cdda_record__drive_info);

	/* create popup menu for cdda_track tree items */
	cdda_track_menu = gtk_menu_new();
	cdda_track__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_track_menu), cdda_track__addlist);
 	g_signal_connect_swapped(G_OBJECT(cdda_track__addlist), "activate", G_CALLBACK(track__addlist_cb), NULL);
	gtk_widget_show(cdda_track__addlist);

	/* create popup menu for cdda_store tree items */
	cdda_store_menu = gtk_menu_new();
	cdda_store__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	cdda_store__addlist_albummode = gtk_menu_item_new_with_label(_("Add to playlist (Album mode)"));

	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_store_menu), cdda_store__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(cdda_store_menu), cdda_store__addlist_albummode);

 	g_signal_connect_swapped(G_OBJECT(cdda_store__addlist), "activate", G_CALLBACK(artist__addlist_cb), NULL);
 	g_signal_connect_swapped(G_OBJECT(cdda_store__addlist_albummode), "activate", G_CALLBACK(artist__addlist_albummode_cb), NULL);

	gtk_widget_show(cdda_store__addlist);
	gtk_widget_show(cdda_store__addlist_albummode);
#endif /* HAVE_CDDA */


	/* attach event handler that will popup the menus */
	g_signal_connect_swapped(G_OBJECT(music_tree), "event", G_CALLBACK(music_tree_event_cb), NULL);

	/* event handler for double-clicking */
	g_signal_connect(G_OBJECT(music_tree), "button_press_event", G_CALLBACK(dblclick_handler), NULL);

	/* create text widget for comments */
	comment_view = gtk_text_view_new();
	gtk_widget_set_name(comment_view, "comment_view");
	gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), FALSE);
	gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));

	scrolled_win2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrolled_win2, -1, 1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win2),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	viewport2 = gtk_viewport_new(NULL, NULL);
	if (!options.hide_comment_pane) {
		gtk_paned_pack2(GTK_PANED(browser_paned), viewport2, FALSE, TRUE);
	}
	gtk_container_add(GTK_CONTAINER(viewport2), scrolled_win2);
	gtk_container_add(GTK_CONTAINER(scrolled_win2), comment_view);

	if (!options.hide_comment_pane) {
		gtk_paned_set_position(GTK_PANED(browser_paned), browser_paned_pos);
	}

	if (options.enable_mstore_statusbar) {

		statusbar_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
		gtk_widget_set_size_request(statusbar_scrolledwin, 1, -1);    /* MAGIC */
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(statusbar_scrolledwin),
					       GTK_POLICY_NEVER, GTK_POLICY_NEVER);

		statusbar_viewport = gtk_viewport_new(NULL, NULL);
		gtk_widget_set_name(statusbar_viewport, "info_viewport");

		gtk_container_add(GTK_CONTAINER(statusbar_scrolledwin), statusbar_viewport);
		gtk_box_pack_start(GTK_BOX(vbox), statusbar_scrolledwin, FALSE, TRUE, 2);

		gtk_widget_set_events(statusbar_viewport,
				      GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

		g_signal_connect(G_OBJECT(statusbar_viewport), "button_press_event",
				 G_CALLBACK(scroll_btn_pressed), NULL);
		g_signal_connect(G_OBJECT(statusbar_viewport), "button_release_event",
				 G_CALLBACK(scroll_btn_released), (gpointer)statusbar_scrolledwin);
		g_signal_connect(G_OBJECT(statusbar_viewport), "motion_notify_event",
				 G_CALLBACK(scroll_motion_notify), (gpointer)statusbar_scrolledwin);

		statusbar_hbox = gtk_hbox_new(FALSE, 0);
		gtk_container_set_border_width(GTK_CONTAINER(statusbar_hbox), 1);
		gtk_container_add(GTK_CONTAINER(statusbar_viewport), statusbar_hbox);

		statusbar_ms = gtk_label_new("");
		gtk_widget_set_name(statusbar_ms, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar_hbox), statusbar_ms, FALSE, FALSE, 0);

                if (options.override_skin_settings) {
                        gtk_widget_modify_font (statusbar_ms, fd_statusbar);
                }
	}
}


void
show_music_browser(void) {

	browser_on = 1;
	gtk_window_move(GTK_WINDOW(browser_window), browser_pos_x, browser_pos_y);
	gtk_window_resize(GTK_WINDOW(browser_window), browser_size_x, browser_size_y);
	gtk_widget_show_all(browser_window);
	if (!options.hide_comment_pane) {
		gtk_paned_set_position(GTK_PANED(browser_paned), browser_paned_pos);
	}
}


void
hide_music_browser(void) {

	browser_on = 0;
	gtk_window_get_position(GTK_WINDOW(browser_window), &browser_pos_x, &browser_pos_y);
	gtk_window_get_size(GTK_WINDOW(browser_window), &browser_size_x, &browser_size_y);
	if (!options.hide_comment_pane) {
		browser_paned_pos = gtk_paned_get_position(GTK_PANED(browser_paned));
	}
	gtk_widget_hide(browser_window);
}

/*********************************************************************************/

void
parse_track(xmlDocPtr doc, xmlNodePtr cur, GtkTreeIter * iter_record) {

	GtkTreeIter iter_track;
	xmlChar * key;
	gchar *converted_temp;
	GError *error=NULL;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char file[MAXLEN];
	char comment[MAXLEN];
	float duration = 0.0f;
	float volume = 1.0f;
	float rva = 0.0f;
	float use_rva = -1.0f;

	name[0] = '\0';
	sort_name[0] = '\0';
	file[0] = '\0';
	comment[0] = '\0';

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				strncpy(name, (char *) key, sizeof(name)-1);
				name[sizeof(name) - 1] = '\0';
			}
			xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"sort_name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				strncpy(sort_name, (char *) key, sizeof(sort_name)-1);
				sort_name[sizeof(sort_name) - 1] = '\0';
			}
			xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"file"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				if ((converted_temp = g_filename_from_uri((char *) key, NULL, NULL))) {
					strncpy(file, converted_temp, sizeof(file)-1);
					file[sizeof(file) - 1] = '\0';
					g_free(converted_temp);
				} else {
					/* try to read utf8 filename from outdated file */
					if ((converted_temp = g_locale_from_utf8((char *) key, -1, NULL, NULL, &error))) {
						strncpy(file, converted_temp, sizeof(file)-1);
						file[sizeof(file) - 1] = '\0';
						g_free(converted_temp);
					} else {
						/* last try - maybe it's plain locale filename */
						strncpy(file, (char *) key, sizeof(file)-1);
						file[sizeof(file) - 1] = '\0';
					}
				}
			}
			xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"comment"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				strncpy(comment, (char *) key, sizeof(comment)-1);
				comment[sizeof(comment) - 1] = '\0';
			}
			xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"duration"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                duration = convf((char *) key);
                        }
                        xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"volume"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                volume = convf((char *) key);
                        }
                        xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"rva"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                rva = convf((char *) key);
                        }
                        xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"use_rva"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                use_rva = convf((char *) key);
                        }
                        xmlFree(key);
                }
                
		cur = cur->next;
	}

	if ((name[0] != '\0') && (file != '\0')) {
		gtk_tree_store_append(music_store, &iter_track, iter_record);
		gtk_tree_store_set(music_store, &iter_track,
				   0, name,
				   1, sort_name,
				   2, file,
				   3, comment,
				   4, duration,
				   5, volume,
				   6, rva,
				   7, use_rva,
				   -1);
                if (options.enable_ms_tree_icons) {
                        gtk_tree_store_set(music_store, &iter_track, 9, icon_track, -1);
		}
	} else {
		if (name[0] == '\0')
			fprintf(stderr, "Error in XML music_store: Track <name> is required, but NULL\n");
		else if (file[0] == '\0')
			fprintf(stderr, "Error in XML music_store: Track <file> is required, but NULL\n");
	}
}


void
parse_record(xmlDocPtr doc, xmlNodePtr cur, GtkTreeIter * iter_artist) {

	GtkTreeIter iter_record;
	xmlChar * key;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char comment[MAXLEN];

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';

	cur = cur->xmlChildrenNode;
	gtk_tree_store_append(music_store, &iter_record, iter_artist);
	gtk_tree_store_set(music_store, &iter_record, 0, "", 1, "", 2, "", 3, "", -1);
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(name, (char *) key, MAXLEN-1);
			xmlFree(key);
			if (name[0] == '\0') {
				fprintf(stderr, "Error in XML music_store: "
				       "Record <name> is required, but NULL\n");
			}
			gtk_tree_store_set(music_store, &iter_record, 0, name, -1);
                        if (options.enable_ms_tree_icons) {
			        gtk_tree_store_set(music_store, &iter_record, 9, icon_record, -1);
                        }
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"sort_name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(sort_name, (char *) key, MAXLEN-1);
			xmlFree(key);
			gtk_tree_store_set(music_store, &iter_record, 1, sort_name, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"comment"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(comment, (char *) key, MAXLEN-1);
			xmlFree(key);
			gtk_tree_store_set(music_store, &iter_record, 3, comment, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"track"))) {
			parse_track(doc, cur, &iter_record);
		}
		cur = cur->next;
	}
}


void
parse_artist(xmlDocPtr doc, xmlNodePtr cur, GtkTreeIter * iter_store) {

	GtkTreeIter iter_artist;
	xmlChar * key;

	char name[MAXLEN];
	char sort_name[MAXLEN];
	char comment[MAXLEN];

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';

	cur = cur->xmlChildrenNode;
	gtk_tree_store_append(music_store, &iter_artist, iter_store);
	gtk_tree_store_set(music_store, &iter_artist, 0, "", 1, "", 2, "", 3, "", -1);
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(name, (char *) key, MAXLEN-1);
			xmlFree(key);
			if (name[0] == '\0') {
				fprintf(stderr, "Error in XML music_store: "
				       "Artist <name> is required, but NULL\n");
			}
			gtk_tree_store_set(music_store, &iter_artist, 0, name, -1);
                        if (options.enable_ms_tree_icons) {
                                gtk_tree_store_set(music_store, &iter_artist, 9, icon_artist, -1);
                        }
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"sort_name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(sort_name, (char *) key, MAXLEN-1);
			xmlFree(key);
			gtk_tree_store_set(music_store, &iter_artist, 1, sort_name, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"comment"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(comment, (char *) key, MAXLEN-1);
			xmlFree(key);
			gtk_tree_store_set(music_store, &iter_artist, 3, comment, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"record"))) {
			parse_record(doc, cur, &iter_artist);
		}
		cur = cur->next;
	}
}


void
load_music_store(char * store_file, char * sort) {

	GtkTreeIter iter_store;

	char name[MAXLEN];
	char comment[MAXLEN];

	name[0] = '\0';
	comment[0] = '\0';

	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar * key;


	if (access(store_file, R_OK) != 0) {
		return;
	}

	gtk_tree_store_append(music_store, &iter_store, NULL);

	gtk_tree_store_set(music_store, &iter_store, 0, _("Music Store"), 1, sort,
			   2, store_file, 3, "", 6, 1.0f, 8, PANGO_WEIGHT_BOLD, -1);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &iter_store, 9, icon_store, -1);
	}

	if (access(store_file, W_OK) == 0) {
		gtk_tree_store_set(music_store, &iter_store, 7, 1.0f, -1);
	} else {
		gtk_tree_store_set(music_store, &iter_store, 7, -1.0f, -1);
	}
	
	doc = xmlParseFile(store_file);
	if (doc == NULL) {
		fprintf(stderr, "An XML error occured while parsing %s\n", store_file);
		return;
	}
	
	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		fprintf(stderr, "load_music_store: empty XML document\n");
		xmlFreeDoc(doc);
		return;
	}

	if (xmlStrcmp(cur->name, (const xmlChar *)"music_store")) {
		fprintf(stderr, "load_music_store: XML document of the wrong type, "
			"root node != music_store\n");
		xmlFreeDoc(doc);
		return;
	}
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(name, (char *) key, MAXLEN-1);
			xmlFree(key);
			if (name[0] == '\0') {
				fprintf(stderr, "Error in XML music_store: "
					"Music Store <name> is required, but NULL\n");
			}
			gtk_tree_store_set(music_store, &iter_store, 0, name, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"comment"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL)
				strncpy(comment, (char *) key, MAXLEN-1);
			xmlFree(key);
			gtk_tree_store_set(music_store, &iter_store, 3, comment, -1);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"artist"))) {
			parse_artist(doc, cur, &iter_store);
		}
		cur = cur->next;
	}

	xmlFreeDoc(doc);
}

void
load_all_music_store(void) {

	GtkTreeIter iter_store;
	char * store_file;
	int i = 0;


        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ms_pathlist_store),
					     &iter_store, NULL, i++)) {

                gtk_tree_model_get(GTK_TREE_MODEL(ms_pathlist_store), &iter_store,
				   0, &store_file, -1);

		load_music_store(store_file, "0");

		g_free(store_file);
        }

	music_tree_expand_stores();
}

void
music_store_mark_changed(GtkTreeIter * iter) {

	GtkTreeIter iter_store;
	GtkTreePath * path;
	float dirty;
	char name[MAXLEN];
	char * pname;

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), iter);

        while (gtk_tree_path_get_depth(path) > 1) {
                gtk_tree_path_up(path);
        }

#ifdef HAVE_CDDA
	if (is_store_path_cdda(path)) {
		gtk_tree_path_free(path);
		return;
	}
#endif /* HAVE_CDDA */

	gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter_store, path);
	gtk_tree_path_free(path);

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_store, 6, &dirty, -1);

	if (dirty < 0) {
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_store, 0, &pname, -1);

	name[0] = '*';
	name[1] = '\0';
	strncat(name, pname, MAXLEN-2);
	g_free(pname);

	gtk_tree_store_set(music_store, &iter_store, 0, name, 6, -1.0f, -1);

	music_store_changed = 1;
	gtk_window_set_title(GTK_WINDOW(browser_window), _("*Music Store"));
	if (options.enable_mstore_toolbar) {
		gtk_widget_set_sensitive(toolbar_save_button, TRUE);
	}
}

void
music_store_mark_saved(GtkTreeIter * iter_store) {

	GtkTreeIter iter;
	int i;
	float dirty;
	char * pname;

#ifdef HAVE_CDDA
	if (is_store_iter_cdda(iter_store)) {
		return;
	}
#endif /* HAVE_CDDA */

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_store, 6, &dirty, -1);

	if (dirty > 0) {
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_store, 0, &pname, -1);
	gtk_tree_store_set(music_store, iter_store, 0, pname + 1, 6, 1.0f, -1);
	g_free(pname);

	i = 0;
        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter, NULL, i++)) {
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, 6, &dirty, -1);

		if (dirty < 0) {
			return;
		}
	}

	music_store_changed = 0;
	gtk_window_set_title(GTK_WINDOW(browser_window), _("Music Store"));
	if (options.enable_mstore_toolbar) {
		gtk_widget_set_sensitive(toolbar_save_button, FALSE);
	}
}

/**********************************************************************************/


void
save_track(xmlDocPtr doc, xmlNodePtr node_track, GtkTreeIter * iter_track) {

	xmlNodePtr node;
	gchar *converted_temp;
	
	char * name;
	char * sort_name;
	char * file;
	char * comment;
	float duration;
	float volume;
	float rva;
	float use_rva;
	char str[32];

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_track,
			   0, &name, 1, &sort_name, 2, &file, 3, &comment,
			   4, &duration, 5, &volume, 6, &rva, 7, &use_rva,
			   -1);
	
	node = xmlNewTextChild(node_track, NULL, (const xmlChar *) "track", NULL);
	if (name[0] == '\0')
		fprintf(stderr, "saving music_store XML: warning: track node with empty <name>\n");
	xmlNewTextChild(node, NULL, (const xmlChar *) "name", (const xmlChar *) name);
	if (sort_name[0] != '\0')
		xmlNewTextChild(node, NULL, (const xmlChar *) "sort_name", (const xmlChar *) sort_name);
	if (file[0] == '\0')
		fprintf(stderr, "saving music_store XML: warning: track node with empty <file>\n");
	converted_temp = g_filename_to_uri(file, NULL, NULL);
	xmlNewTextChild(node, NULL, (const xmlChar *) "file", (const xmlChar*) converted_temp);
	g_free(converted_temp);
	if (comment[0] != '\0')
		xmlNewTextChild(node, NULL, (const xmlChar *) "comment", (const xmlChar *) comment);

	if (duration != 0.0f) {
		snprintf(str, 31, "%.1f", duration);
		xmlNewTextChild(node, NULL, (const xmlChar *) "duration", (const xmlChar *) str);
	}

	if (volume <= 0.1f) {
		snprintf(str, 31, "%.1f", volume);
		xmlNewTextChild(node, NULL, (const xmlChar *) "volume", (const xmlChar *) str);
	}

	if (rva != 0.0f) {
		snprintf(str, 31, "%.1f", rva);
		xmlNewTextChild(node, NULL, (const xmlChar *) "rva", (const xmlChar *) str);
	}

	if (use_rva >= 0.0f) {
		snprintf(str, 31, "%.1f", use_rva);
		xmlNewTextChild(node, NULL, (const xmlChar *) "use_rva", (const xmlChar *) str);
	}

	g_free(name);
	g_free(sort_name);
	g_free(file);
	g_free(comment);
}


void
save_record(xmlDocPtr doc, xmlNodePtr node_record, GtkTreeIter * iter_record) {

	xmlNodePtr node;
	char * name;
	char * sort_name;
	char * comment;
	GtkTreeIter iter_track;
	int i = 0;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_record,
			   0, &name, 1, &sort_name, 3, &comment, -1);
	
	node = xmlNewTextChild(node_record, NULL, (const xmlChar *) "record", NULL);
	if (name[0] == '\0')
		fprintf(stderr, "saving music_store XML: warning: record node with empty <name>\n");
	xmlNewTextChild(node, NULL, (const xmlChar *) "name", (const xmlChar *) name);
	if (sort_name[0] != '\0')
		xmlNewTextChild(node, NULL, (const xmlChar *) "sort_name", (const xmlChar *) sort_name);
	if (comment[0] != '\0')
		xmlNewTextChild(node, NULL, (const xmlChar *) "comment", (const xmlChar *) comment);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track, iter_record, i++))
		save_track(doc, node, &iter_track);

	g_free(name);
	g_free(sort_name);
	g_free(comment);
}


void
save_artist(xmlDocPtr doc, xmlNodePtr root, GtkTreeIter * iter_artist) {

	xmlNodePtr node;
	char * name;
	char * sort_name;
	char * comment;
	GtkTreeIter iter_record;
	int i = 0;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_artist,
			   0, &name, 1, &sort_name, 3, &comment, -1);
	
	node = xmlNewTextChild(root, NULL, (const xmlChar *) "artist", NULL);
	if (name[0] == '\0')
		fprintf(stderr, "saving music_store XML: warning: artist node with empty <name>\n");
	xmlNewTextChild(node, NULL, (const xmlChar *) "name", (const xmlChar *) name);
	if (sort_name[0] != '\0')
		xmlNewTextChild(node, NULL, (const xmlChar *) "sort_name", (const xmlChar *) sort_name);
	if (comment[0] != '\0')
		xmlNewTextChild(node, NULL, (const xmlChar *) "comment", (const xmlChar *) comment);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_record, iter_artist, i++))
		save_record(doc, node, &iter_record);

	g_free(name);
	g_free(sort_name);
	g_free(comment);
}


void
save_music_store(GtkTreeIter * iter_store) {

	xmlDocPtr doc;
	xmlNodePtr root;
	char * store_file;
	char * name;
	char * comment;
	float dirty;
	int i;
	GtkTreeIter iter_artist;


	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_store, 6, &dirty, -1);

	if (dirty > 0) {
		return;
	}

	music_store_mark_saved(iter_store);

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_store,
			   0, &name, 2, &store_file, 3, &comment, -1);

	if (strcmp(store_file, "CDDA_STORE") == 0) {
		g_free(name);
		g_free(comment);
		g_free(store_file);
		return;
	}

	doc = xmlNewDoc((const xmlChar *) "1.0");
	root = xmlNewNode(NULL, (const xmlChar *) "music_store");
	xmlDocSetRootElement(doc, root);

	if (name[0] == '\0') {
		fprintf(stderr, "saving music_store XML: warning: empty <name>\n");
	}
	xmlNewTextChild(root, NULL, (const xmlChar *) "name", (const xmlChar *) name);
	if (comment[0] != '\0') {
		xmlNewTextChild(root, NULL, (const xmlChar *) "comment", (const xmlChar *) comment);
	}

	g_free(name);
	g_free(comment);

	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_artist, iter_store, i++)) {
		save_artist(doc, root, &iter_artist);
	}

	xmlSaveFormatFile(store_file, doc, 1);
	xmlFreeDoc(doc);
	g_free(store_file);
}


void
save_all_music_store(void) {

	GtkTreeIter iter_store;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &iter_store, NULL, i++)) {
		save_music_store(&iter_store);
	}
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

