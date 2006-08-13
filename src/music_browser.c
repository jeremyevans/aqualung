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
extern void set_sliders_width(void);
extern void assign_audio_fc_filters(GtkFileChooser * fc);

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
GtkWidget * store__remove;
GtkWidget * store__separator2;
GtkWidget * store__addart;
GtkWidget * store__separator3;
GtkWidget * store__volume;
GtkWidget * store__volume_menu;
GtkWidget * store__volume_unmeasured;
GtkWidget * store__volume_all;
GtkWidget * store__search;
GtkWidget * store__separator4;
GtkWidget * store__save;

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

GtkWidget * blank_menu;
GtkWidget * blank__add;
GtkWidget * blank__search;
GtkWidget * blank__save;

int drag_info;

GtkWidget * save_button;

GtkWidget * name_entry;
GtkWidget * sort_name_entry;


/* prototypes, when we need them */
void load_music_store(void);

static gboolean music_tree_event_cb(GtkWidget * widget, GdkEvent * event);

static void store__add_cb(gpointer data);
static void store__build_cb(gpointer data);
static void store__edit_cb(gpointer data);
static void store__volume_unmeasured_cb(gpointer data);
static void store__volume_all_cb(gpointer data);
static void store__remove_cb(gpointer data);

static void artist__add_cb(gpointer data);
static void artist__build_cb(gpointer data);
static void artist__edit_cb(gpointer data);
static void artist__volume_unmeasured_cb(gpointer data);
static void artist__volume_all_cb(gpointer data);
static void artist__remove_cb(gpointer data);

static void record__add_cb(gpointer data);
static void record__edit_cb(gpointer data);
static void record__volume_unmeasured_cb(gpointer data);
static void record__volume_all_cb(gpointer data);
static void record__remove_cb(gpointer data);

static void track__add_cb(gpointer data);
static void track__edit_cb(gpointer data);
static void track__volume_unmeasured_cb(gpointer data);
static void track__volume_all_cb(gpointer data);
static void track__remove_cb(gpointer data);
static void track__fileinfo_cb(gpointer data);

static void search_cb(gpointer data);

struct keybinds {
	void (*callback)(gpointer);
	int keyval1;
	int keyval2;
};

struct keybinds blank_keybinds[] = {
        {store__add_cb, GDK_n, GDK_N},
        {store__build_cb, GDK_b, GDK_B},
        {search_cb, GDK_f, GDK_F},
        {NULL, 0}
};

struct keybinds store_keybinds[] = {
	{store__addlist_defmode, GDK_a, GDK_A},
	{store__add_cb, GDK_n, GDK_N},
	{store__build_cb, GDK_b, GDK_B},
	{store__edit_cb, GDK_e, GDK_E},
	{store__volume_unmeasured_cb, GDK_v, GDK_V},
	{store__remove_cb, GDK_Delete, GDK_KP_Delete},
	{artist__add_cb, GDK_plus, GDK_KP_Add},
	{search_cb, GDK_f, GDK_F},
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
	{NULL, 0}
};



static gboolean
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
make_title_string(char * dest, char * template, char * artist, char * record, char * track) {

	int i;
	int j;
	int argc = 0;
	char * arg[3] = { "", "", "" };
	char temp[MAXLEN];

	temp[0] = template[0];

	for (i = j = 1; i < strlen(template) && j < MAXLEN - 1; i++, j++) {
		if (template[i - 1] == '%') {
			if (argc < 3) {
				switch (template[i]) {
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
					temp[j] = template[i];
					break;
				}
			} else {
				temp[j++] = '%';
				temp[j] = template[i];
			}
		} else {
			temp[j] = template[i];
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

        set_sliders_width();    /* MAGIC */

        if (strlen(selected_filename)) {
		char * locale = g_locale_from_utf8(selected_filename, -1, NULL, NULL, NULL);
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), locale);
		g_free(locale);
	} else {
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
	}

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);


        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * locale;

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		locale = g_locale_from_utf8(selected_filename, -1, NULL, NULL, NULL);
		gtk_entry_set_text(GTK_ENTRY(data), selected_filename);

                strncpy(options.currdir, locale, MAXLEN-1);
		g_free(locale);
        }


        gtk_widget_destroy(dialog);

        set_sliders_width();    /* MAGIC */

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

	gtk_widget_grab_focus(name_entry);

	gtk_widget_show_all(dialog);

 display:

	name[0] = '\0';
	file[0] = '\0';
	comment[0] = '\0';

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		const char * pfile = g_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(file_entry)), -1, NULL, NULL, NULL);

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

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
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

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
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

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
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

        set_sliders_width();    /* MAGIC */

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 580, 390);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), options.currdir);
        assign_audio_fc_filters(GTK_FILE_CHOOSER(dialog));

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

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

        set_sliders_width();    /* MAGIC */

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

        set_sliders_width();    /* MAGIC */

 display:

	name[0] = '\0';
	sort_name[0] = '\0';
	comment[0] = '\0';

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
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

        set_sliders_width();    /* MAGIC */

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

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
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

        set_sliders_width();    /* MAGIC */

        if (strlen(selected_filename)) {
		char * locale = g_locale_from_utf8(selected_filename, -1, NULL, NULL, NULL);
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), locale);
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

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * utf8;

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		utf8 = g_locale_to_utf8(selected_filename, -1, NULL, NULL, NULL);
		gtk_entry_set_text(GTK_ENTRY(data), utf8);

                strncpy(options.currdir, selected_filename, MAXLEN-1);
		g_free(utf8);
        }


        gtk_widget_destroy(dialog);

        set_sliders_width();    /* MAGIC */

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
		
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * pfile = g_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(file_entry)), -1, NULL, NULL, NULL);

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

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * pfile = g_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(file_entry)), -1, NULL, NULL, NULL);

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
confirm_dialog(char * title, char * text) {

/*	GtkWidget * dialog;*/
/*        GtkWidget * label;*/
        int ret;

/*        dialog = gtk_dialog_new_with_buttons(title,*/
/*					     GTK_WINDOW(browser_window),*/
/*					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,*/
/*					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,*/
/*					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,*/
/*					     NULL);*/
/*	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);*/
/*        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);*/
/*	gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);*/

/*	label = gtk_label_new(text);*/
/*        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, TRUE, 10);*/
/*	gtk_widget_show(label);*/

/*        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {*/

        dialog = gtk_message_dialog_new(GTK_WINDOW(browser_window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, text);
	gtk_window_set_title(GTK_WINDOW(dialog), title);
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_NO);
        gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES) {
                ret = 1;
        } else {
                ret = 0;
        }

        gtk_widget_destroy(dialog);

        return ret;
}


static int
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


static int
is_store_iter_readonly(GtkTreeIter * i) {

        return is_store_path_readonly(gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), i));
}


static void
set_popup_sensitivity(GtkTreePath * path) {

	gboolean val;
	gboolean val2;

	val = (is_store_path_readonly(path)) ? FALSE : TRUE;
	val2 = (vol_window == NULL) ? TRUE : FALSE;

	gtk_widget_set_sensitive(store__build, val);
	gtk_widget_set_sensitive(store__edit, val);
	gtk_widget_set_sensitive(store__remove, val);
	gtk_widget_set_sensitive(store__addart, val);
	gtk_widget_set_sensitive(store__volume, val2);

	gtk_widget_set_sensitive(artist__add, val);
	gtk_widget_set_sensitive(artist__build, val);
	gtk_widget_set_sensitive(artist__edit, val);
	gtk_widget_set_sensitive(artist__remove, val);
	gtk_widget_set_sensitive(artist__addrec, val);
	gtk_widget_set_sensitive(artist__volume, val2);

	gtk_widget_set_sensitive(record__add, val);
	gtk_widget_set_sensitive(record__edit, val);
	gtk_widget_set_sensitive(record__remove, val);
	gtk_widget_set_sensitive(record__addtrk, val);
#ifdef HAVE_CDDB
	gtk_widget_set_sensitive(record__cddb, val);
#endif /* HAVE_CDDB */
	gtk_widget_set_sensitive(record__volume, val2);

	gtk_widget_set_sensitive(track__add, val);
	gtk_widget_set_sensitive(track__edit, val);
	gtk_widget_set_sensitive(track__remove, val);
	gtk_widget_set_sensitive(track__volume, val2);

	if (
#ifdef HAVE_CDDB
	    cddb_thread_state != 2 ||
#endif /* HAVE_CDDB */
	    build_thread_state != 1) {
		gtk_widget_set_sensitive(store__build, FALSE);
		gtk_widget_set_sensitive(artist__build, FALSE);
#ifdef HAVE_CDDB
		gtk_widget_set_sensitive(record__cddb, FALSE);
#endif /* HAVE_CDDB */
	}
}


static gboolean
music_tree_event_cb(GtkWidget * widget, GdkEvent * event) {

	GtkTreePath * path;
	GtkTreeViewColumn * column;

	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton * bevent = (GdkEventButton *) event;

		if (bevent->button == 3) {

			if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(music_tree), bevent->x, bevent->y,
							  &path, &column, NULL, NULL)) {

				set_popup_sensitivity(path);
				
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(music_tree), path, NULL, FALSE);

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
			
			switch (gtk_tree_path_get_depth(path)) {
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

        char * partist_name;
        char * precord_name;
        char * ptrack_name;
        char * pfile;

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
		
	gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_record, &iter_track);
	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record, 0, &precord_name, -1);
	strncpy(record_name, precord_name, MAXLEN-1);
	g_free(precord_name);

	gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_artist, &iter_record);
	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_artist, 0, &partist_name, -1);
	strncpy(artist_name, partist_name, MAXLEN-1);
	g_free(partist_name);

	if (options.auto_use_meta_artist || options.auto_use_meta_record || options.auto_use_meta_track) {
		meta = meta_new();
		if (!meta_read(meta, file)) {
			meta_free(meta);
			meta = NULL;
		}
	}

	if ((meta != NULL) && options.auto_use_meta_artist) {
		meta_get_artist(meta, artist_name);
	}

	if ((meta != NULL) && options.auto_use_meta_record) {
		meta_get_record(meta, record_name);
	}

	if ((meta != NULL) && options.auto_use_meta_track) {
		meta_get_title(meta, track_name);
	}

	if (meta != NULL) {
		meta_free(meta);
		meta = NULL;
	}
	
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
			music_store_mark_changed();
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
		char * precord_name;
		char * partist_name;
		char name_str[MAXLEN];
		char packed_str[MAXLEN];
		GtkTreeIter iter_artist;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_record, 0, &precord_name, -1);
		gtk_tree_model_iter_parent(GTK_TREE_MODEL(music_store), &iter_artist, &iter_record);
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_artist, 0, &partist_name, -1);

		sprintf(name_str, "%s: %s", partist_name, precord_name);
		pack_strings(partist_name, precord_name, packed_str);

		g_free(precord_name);
		g_free(partist_name);

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
        GtkTreeModel * model;

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_store)) {
		store_addlist_iter(iter_store, (GtkTreeIter *)data, mode);
		playlist_content_changed();
		delayed_playlist_rearrange(100);
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


static void
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
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
		} else {

			char * utf8 = g_locale_to_utf8(file, -1, NULL, NULL, NULL);

			gtk_tree_store_append(music_store, &iter, NULL);
			gtk_tree_store_set(music_store, &iter, 0, name, 2, file, 3, comment, 7, 1.0f/*rw*/, 8, PANGO_WEIGHT_BOLD, -1);

			doc = xmlNewDoc((const xmlChar *) "1.0");
			root = xmlNewNode(NULL, (const xmlChar *) "music_store");
			xmlDocSetRootElement(doc, root);

			xmlNewTextChild(root, NULL, (const xmlChar *) "name", (xmlChar *) name);
			xmlNewTextChild(root, NULL, (const xmlChar *) "comment", (xmlChar *) comment);

			xmlSaveFormatFile(file, doc, 1);

			gtk_list_store_append(ms_pathlist_store, &iter);
			gtk_list_store_set(ms_pathlist_store, &iter, 0, file, 1, utf8, 2, _("rw"), -1);
			g_free(utf8);
		}
	}
}


static void
store__build_cb(gpointer data) {

	GtkTreeIter store_iter;
	GtkTreeModel * model;

	if (gtk_tree_selection_get_selected(music_select, &model, &store_iter)) {
		build_store(store_iter);
	}
}


static void
store__edit_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	char * pname;
	char * pcomment;
	char * pfile = NULL;

	char name[MAXLEN];
	char comment[MAXLEN];
	char file[MAXLEN];
	float state;


	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) return;

                gtk_tree_model_get(model, &iter, 0, &pname, 2, &pfile, 3, &pcomment, 7, &state, -1);

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

		if (edit_store_dialog(name, file, comment)) {
			gtk_tree_store_set(music_store, &iter, 0, name, 3, comment, -1);
			tree_selection_changed_cb(music_select, NULL);
			music_store_mark_changed();
		}
        }
}


static void
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


static void
store__volume_unmeasured_cb(gpointer data) {

	store_volume_calc(1);
}


static void
store__volume_all_cb(gpointer data) {

	store_volume_calc(0);
}


static void
store__remove_cb(gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;
	char * pname;
	char * pfile;
	char name[MAXLEN];
	char file[MAXLEN];
	char text[MAXLEN];
	float state;
	int i = 0;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) return;

                gtk_tree_model_get(model, &iter, 0, &pname, 2, &pfile, 7, &state, -1);

		strncpy(name, pname, MAXLEN-1);
		strncpy(file, pfile, MAXLEN-1);
                g_free(pname);
                g_free(pfile);
		
		snprintf(text, MAXLEN-1, _("Really remove \"%s\" from the Music Store?"), name);
		if (confirm_dialog(_("Remove Store"), text)) {
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


static void
store__save_cb(gpointer data) {

	save_music_store();
}


/****************************************/


/* mode: 0 normal, 1 album mode */

void
artist__addlist_with_mode(int mode, gpointer data) {

        GtkTreeIter iter_artist;
        GtkTreeModel * model;

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_artist)) {
		artist_addlist_iter(iter_artist, (GtkTreeIter *)data, mode);
		playlist_content_changed();
		delayed_playlist_rearrange(100);
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


static void
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
			gtk_tree_store_set(music_store, &iter, 0, name, 1, sort_name, 2, "", 3, comment, -1);
			music_store_mark_changed();
		}
	}
}


static void
artist__build_cb(gpointer data) {

	GtkTreeIter artist_iter;
	GtkTreeModel * model;

	if (gtk_tree_selection_get_selected(music_select, &model, &artist_iter)) {
		build_artist(artist_iter);
	}
}


static void
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
			music_store_mark_changed();
		}
        }
}


static void
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


static void
artist__volume_unmeasured_cb(gpointer data) {

	artist_volume_calc(1);
}


static void
artist__volume_all_cb(gpointer data) {

	artist_volume_calc(0);
}


static void
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
		if (confirm_dialog(_("Remove Artist"), text)) {
			gtk_tree_store_remove(music_store, &iter);
			tree_selection_changed_cb(music_select, NULL);
			music_store_mark_changed();
		}
	}
}

/************************************/


/* mode: 0 normal, 1 album mode */

void
record__addlist_with_mode(int mode, gpointer data) {

        GtkTreeIter iter_record;
        GtkTreeModel * model;

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_record)) {
		record_addlist_iter(iter_record, (GtkTreeIter *)data, mode);
		playlist_content_changed();
		delayed_playlist_rearrange(100);
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


static void
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
								   5, volume, 7, use_rva, -1);
						g_free(utf8);
					}
					free(strings[i]);
				}
				free(strings);
			}
			music_store_mark_changed();
		}
	}
}


static void
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
			music_store_mark_changed();
		}
        }
}


static void
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


static void
record__volume_unmeasured_cb(gpointer data) {

	record_volume_calc(1);
}


static void
record__volume_all_cb(gpointer data) {

	record_volume_calc(0);
}


static void
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
		if (confirm_dialog(_("Remove Record"), text)) {
			gtk_tree_store_remove(music_store, &iter);
			gtk_tree_selection_unselect_all(music_select);
			tree_selection_changed_cb(music_select, NULL);
			music_store_mark_changed();
		}
	}
}


#ifdef HAVE_CDDB

static void
record__cddb_cb(gpointer data) {
	GtkTreeIter iter;
	GtkTreeModel * model;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		if (is_store_iter_readonly(&iter)) return;

		cddb_get();
	}
}

#endif /* HAVE_CDDB */


/************************************/


void
track__addlist_cb(gpointer data) {

        GtkTreeIter iter_track;
        GtkTreeModel * model;

        if (gtk_tree_selection_get_selected(music_select, &model, &iter_track)) {
		track_addlist_iter(iter_track, NULL, (GtkTreeIter *)data, 0.0f, 0);
		playlist_content_changed();
		delayed_playlist_rearrange(100);
	}
}


static void
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
					   3, comment, 4, get_file_duration(file), 5, volume, 7, use_rva, -1);

			music_store_mark_changed();
		}
	}
}


static void
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

			music_store_mark_changed();
                }
        }
}


static void
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


static void
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


static void
track__volume_unmeasured_cb(gpointer data) {

	track_volume_calc(1);
}


static void
track__volume_all_cb(gpointer data) {

	track_volume_calc(0);
}


static void
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
		if (confirm_dialog(_("Remove Track"), text)) {
			gtk_tree_store_remove(music_store, &iter);
			gtk_tree_selection_unselect_all(music_select);
			tree_selection_changed_cb(music_select, NULL);
			music_store_mark_changed();
		}
	}
}



static void
search_cb(gpointer data) {

	search_dialog();
}

static void
collapse_all_items_cb(gpointer data)
{
        gtk_tree_view_collapse_all(GTK_TREE_VIEW(music_tree));  
}

static void
edit_item_cb(gpointer data)
{
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

static void
add_item_cb(gpointer data)
{
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

static void
remove_item_cb(gpointer data)
{
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

/* return:
     0: if iter was newly created
     1: if iter has already existed
*/

int
store_get_iter_for_artist_and_record(GtkTreeIter * store_iter, GtkTreeIter * iter,
				     const char * artist, const char * artist_sort_name,
				     const char * record, const char * record_sort_name,
				     const char * record_comment) {
	int i, j;
	GtkTreeIter artist_iter;
	GtkTreeIter record_iter;
	GtkTreeIter new_artist_iter;
	GtkTreeIter new_record_iter;


	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &artist_iter, store_iter, i++)) {

		char * artist_name;
		char * artist_name_key;
		char * artist_key;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &artist_iter,
				   0, &artist_name, -1);

		artist_key = g_utf8_casefold(artist, -1);
		artist_name_key = g_utf8_casefold(artist_name, -1);

		if (g_utf8_collate(artist_key, artist_name_key)) {
			g_free(artist_key);
			g_free(artist_name_key);
			continue;
		}

		g_free(artist_key);
		g_free(artist_name_key);

		j = 0;
		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
						     &record_iter, &artist_iter, j++)) {
			char * record_name;
			char * record_name_key;
			char * record_key;

			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &record_iter,
					   0, &record_name, -1);

			record_key = g_utf8_casefold(record, -1);
			record_name_key = g_utf8_casefold(record_name, -1);

			if (!g_utf8_collate(record_key, record_name_key)) {

				*iter = record_iter;
				g_free(record_key);
				g_free(record_name_key);
				return 1;
			}

			g_free(record_key);
			g_free(record_name_key);
		}

		/* create record */
		gtk_tree_store_append(music_store, &new_record_iter, &artist_iter);
		gtk_tree_store_set(music_store, &new_record_iter,
				   0, record, 1, record_sort_name,
				   2, "", 3, record_comment, -1);

		*iter = new_record_iter;
		return 0;
	}

	/* create both artist and record */
	gtk_tree_store_append(music_store, &new_artist_iter, store_iter);
	gtk_tree_store_set(music_store, &new_artist_iter,
			   0, artist, 1, artist_sort_name,
			   2, "", 3, "", -1);

	gtk_tree_store_append(music_store, &new_record_iter, &new_artist_iter);
	gtk_tree_store_set(music_store, &new_record_iter,
			   0, record, 1, record_sort_name,
			   2, "", 3, record_comment, -1);

	*iter = new_record_iter;
	return 0;
}

int
artist_get_iter_for_record(GtkTreeIter * artist_iter, GtkTreeIter * iter,
			   const char * record, const char * record_sort_name,
			   const char * record_comment) {
	int i;
	GtkTreeIter record_iter;
	GtkTreeIter new_record_iter;


	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &record_iter, artist_iter, i++)) {

		char * record_name;
		char * record_name_key;
		char * record_key;

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &record_iter,
				   0, &record_name, -1);

		record_key = g_utf8_casefold(record, -1);
		record_name_key = g_utf8_casefold(record_name, -1);

		if (!g_utf8_collate(record_key, record_name_key)) {

			*iter = record_iter;
			g_free(record_key);
			g_free(record_name_key);
			return 1;
		}

		g_free(record_key);
		g_free(record_name_key);
	}

	/* create record */
	gtk_tree_store_append(music_store, &new_record_iter, artist_iter);
	gtk_tree_store_set(music_store, &new_record_iter,
			   0, record, 1, record_sort_name,
			   2, "", 3, record_comment, -1);

	*iter = new_record_iter;
	return 0;
}


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
	unsigned int size = 0;
	
	char duration_str[MAXLEN];
	char str[MAXLEN];

	struct stat statbuf;


	if (!options.enable_mstore_statusbar) return;

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		path = gtk_tree_model_get_path(model, &iter);

		switch (gtk_tree_path_get_depth(path)) {
		case 1:
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
						
						if (stat(file, &statbuf) != -1) {
							size += statbuf.st_size;
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
				if ((size /= 1024 * 1024) < 1024) {
					snprintf(str, MAXLEN-1,
						 ("%d %s, %d %s, %d %s [%s] (%d MB)"),
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
		case 2:
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

					if (stat(file, &statbuf) != -1) {
						size += statbuf.st_size;
					}

					g_free(file);
				}
			}

			if (duration > 0.0f) {
				time2time(duration, duration_str);
			} else {
				strcpy(duration_str, _("time unmeasured"));
			}

			if (options.ms_statusbar_show_size) {
				if ((size /= 1024 * 1024) < 1024) {
					snprintf(str, MAXLEN-1, "%d %s, %d %s [%s] (%d MB)",
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
		case 3:
			k = 0;
			while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_track,
							     &iter, k++)) {
				n_track++;
				gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_track, 2, &file, 4, &len, -1);
				duration += len;

				if (stat(file, &statbuf) != -1) {
					size += statbuf.st_size;
				}

				g_free(file);
			}

			if (duration > 0.0f) {
				time2time(duration, duration_str);
			} else {
				strcpy(duration_str, _("time unmeasured"));
			}

			if (options.ms_statusbar_show_size) {
				if ((size /= 1024 * 1024) < 1024) {
					snprintf(str, MAXLEN-1, "%d %s [%s] (%d MB)",
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
		case 4:
			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, 2, &file, 4, &duration, -1);

			if (duration > 0.0f) {
				time2time(duration, duration_str);
			} else {
				strcpy(duration_str, _("time unmeasured"));
			}

			if (stat(file, &statbuf) != -1) {
				size += statbuf.st_size;
			}

			g_free(file);

			if (options.ms_statusbar_show_size) {
				if ((size /= 1024 * 1024) < 1024) {
					snprintf(str, MAXLEN-1, "1 %s [%s] (%d MB)",
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
        GtkTreeModel * model;

        if (!gtk_tree_selection_get_selected(music_select, &model, &iter2)) {
		set_comment_content();
		music_store_set_status_bar_info();
	}

	return FALSE;
}


void
browser_drag_begin(GtkWidget * widget, GdkDragContext * drag_context, gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	GtkTargetEntry target_table[] = {
		{ "", GTK_TARGET_SAME_APP, 0 }
	};

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {
		target_table[0].info =
			gtk_tree_path_get_depth(gtk_tree_model_get_path(model, &iter));
	}

	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(play_list), FALSE);
	gtk_drag_dest_set(play_list,
			  GTK_DEST_DEFAULT_ALL,
			  target_table,
			  1,
			  GDK_ACTION_COPY);
}


void
browser_drag_data_get(GtkWidget * widget, GdkDragContext * drag_context,
		      GtkSelectionData * data, guint info, guint time, gpointer user_data) {

	gtk_selection_data_set(data, data->target, 8, (const guchar *) "store\0", 6);
}


void
browser_drag_end(GtkWidget * widget, GdkDragContext * drag_context, gpointer data) {

	gtk_drag_dest_unset(play_list);
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
	}

	if (path) {
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
       	GtkWidget * edit_button;
	GtkWidget * add_button;
	GtkWidget * remove_button;
	GtkWidget * search_button;
	GtkWidget * collapse_all_button;

	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTextBuffer * buffer;

	GdkPixbuf * pixbuf;
	char path[MAXLEN];

	GtkTargetEntry target_table[] = {
		{ "", GTK_TARGET_SAME_APP, 0 }
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

                search_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_FIND);
                GTK_WIDGET_UNSET_FLAGS(search_button, GTK_CAN_FOCUS);
                g_signal_connect(G_OBJECT(search_button), "clicked", G_CALLBACK(search_cb), NULL);
                gtk_box_pack_start(GTK_BOX(hbox), search_button, FALSE, TRUE, 3);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), search_button, _("Search..."), NULL);

                collapse_all_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_REFRESH);
                GTK_WIDGET_UNSET_FLAGS(collapse_all_button, GTK_CAN_FOCUS);
                gtk_box_pack_start(GTK_BOX(hbox), collapse_all_button, FALSE, TRUE, 3);
                g_signal_connect(G_OBJECT(collapse_all_button), "pressed", G_CALLBACK(collapse_all_items_cb), NULL);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), collapse_all_button, _("Collapse all items"), NULL);

                edit_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_EDIT);
                GTK_WIDGET_UNSET_FLAGS(edit_button, GTK_CAN_FOCUS);
                gtk_box_pack_start(GTK_BOX(hbox), edit_button, FALSE, TRUE, 3);
                g_signal_connect(G_OBJECT(edit_button), "pressed", G_CALLBACK(edit_item_cb), NULL);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), edit_button, _("Edit item..."), NULL);

                add_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_ADD);
                GTK_WIDGET_UNSET_FLAGS(add_button, GTK_CAN_FOCUS);
                gtk_box_pack_start(GTK_BOX(hbox), add_button, FALSE, TRUE, 3);
                g_signal_connect(G_OBJECT(add_button), "pressed", G_CALLBACK(add_item_cb), NULL);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), add_button, _("Add item..."), NULL);

                remove_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_REMOVE);
                GTK_WIDGET_UNSET_FLAGS(remove_button, GTK_CAN_FOCUS);
                gtk_box_pack_start(GTK_BOX(hbox), remove_button, FALSE, TRUE, 3);
                g_signal_connect(G_OBJECT(remove_button), "pressed", G_CALLBACK(remove_item_cb), NULL);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), remove_button, _("Remove item..."), NULL);

                save_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_SAVE);
                GTK_WIDGET_UNSET_FLAGS(save_button, GTK_CAN_FOCUS);
                gtk_box_pack_start(GTK_BOX(hbox), save_button, FALSE, TRUE, 3);
                g_signal_connect(G_OBJECT(save_button), "pressed", G_CALLBACK(store__save_cb), NULL);
                gtk_widget_set_sensitive(save_button, FALSE);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), save_button, _("Save Music Store"), NULL);
        }


	if (!options.hide_comment_pane) {
		browser_paned = gtk_vpaned_new();
		gtk_box_pack_start(GTK_BOX(vbox), browser_paned, TRUE, TRUE, 0);
	}

	/* create music store tree */
	if (!music_store) {
		music_store = gtk_tree_store_new(9,
						 G_TYPE_STRING,  /* visible name */
						 G_TYPE_STRING,  /* string to sort by (except stores) */
						 G_TYPE_STRING,  /* physical filename (stores&tracks) */
						 G_TYPE_STRING,  /* user comments */
						 G_TYPE_FLOAT,   /* track length in seconds */
						 G_TYPE_FLOAT,   /* track average volume in dBFS */
						 G_TYPE_FLOAT,   /* track manual volume adjustment, dB */
						 G_TYPE_FLOAT,   /* if >= 0: use track manual RVA, 
								    if < 0: auto (compute from avg. loudness);
								    if >= 0: writable store,
								    if < 0: readonly store */
                			         G_TYPE_INT);    /* font weight */
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

        renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Artist / Record / Track",
							  renderer,
							  "text", 0,
                                                          "weight", 8,
							  NULL);
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

	/* setup drag-and-drop */
	gtk_drag_source_set(music_tree,
			    GDK_BUTTON1_MASK,
			    target_table,
			    1,
			    GDK_ACTION_COPY);

	snprintf(path, MAXLEN-1, "%s/drag.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		gtk_drag_source_set_icon_pixbuf(music_tree, pixbuf);
	}	

	g_signal_connect(G_OBJECT(music_tree), "drag_begin", G_CALLBACK(browser_drag_begin), NULL);
	g_signal_connect(G_OBJECT(music_tree), "drag_data_get", G_CALLBACK(browser_drag_data_get), NULL);
	g_signal_connect(G_OBJECT(music_tree), "drag_end", G_CALLBACK(browser_drag_end), NULL);


        /* create popup menu for blank space */
        blank_menu = gtk_menu_new();
        blank__add = gtk_menu_item_new_with_label(_("Create empty store..."));
        blank__search = gtk_menu_item_new_with_label(_("Search..."));
        blank__save = gtk_menu_item_new_with_label(_("Save Music Store"));
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
	store__build = gtk_menu_item_new_with_label(_("Build store from filesystem..."));
	store__edit = gtk_menu_item_new_with_label(_("Edit store..."));
	store__remove = gtk_menu_item_new_with_label(_("Remove store"));
	store__separator2 = gtk_separator_menu_item_new();
	store__addart = gtk_menu_item_new_with_label(_("Add new artist to this store..."));
	store__separator3 = gtk_separator_menu_item_new();
	store__volume = gtk_menu_item_new_with_label(_("Calculate volume (recursive)"));
	store__volume_menu = gtk_menu_new();
	store__volume_unmeasured = gtk_menu_item_new_with_label(_("Unmeasured tracks only"));
	store__volume_all = gtk_menu_item_new_with_label(_("All tracks"));
	store__search = gtk_menu_item_new_with_label(_("Search..."));
	store__separator4 = gtk_separator_menu_item_new();
	store__save = gtk_menu_item_new_with_label(_("Save Music Store"));

	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__addlist);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__addlist_albummode);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__separator1);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__add);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__build);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__edit);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__remove);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__addart);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__separator3);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__volume);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(store__volume), store__volume_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(store__volume_menu), store__volume_unmeasured);
        gtk_menu_shell_append(GTK_MENU_SHELL(store__volume_menu), store__volume_all);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__search);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__separator4);
	gtk_menu_shell_append(GTK_MENU_SHELL(store_menu), store__save);

	g_signal_connect_swapped(G_OBJECT(store__addlist), "activate", G_CALLBACK(store__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__addlist_albummode), "activate", G_CALLBACK(store__addlist_albummode_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__add), "activate", G_CALLBACK(store__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__build), "activate", G_CALLBACK(store__build_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__edit), "activate", G_CALLBACK(store__edit_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__remove), "activate", G_CALLBACK(store__remove_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__addart), "activate", G_CALLBACK(artist__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__volume_unmeasured), "activate", G_CALLBACK(store__volume_unmeasured_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__volume_all), "activate", G_CALLBACK(store__volume_all_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__search), "activate", G_CALLBACK(search_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(store__save), "activate", G_CALLBACK(store__save_cb), NULL);

	gtk_widget_show(store__addlist);
	gtk_widget_show(store__addlist_albummode);
	gtk_widget_show(store__separator1);
	gtk_widget_show(store__add);
	gtk_widget_show(store__build);
	gtk_widget_show(store__edit);
	gtk_widget_show(store__remove);
	gtk_widget_show(store__separator2);
	gtk_widget_show(store__addart);
	gtk_widget_show(store__separator3);
	gtk_widget_show(store__volume);
	gtk_widget_show(store__volume_unmeasured);
	gtk_widget_show(store__volume_all);
	gtk_widget_show(store__search);
	gtk_widget_show(store__separator4);
	gtk_widget_show(store__save);

	/* create popup menu for artist tree items */
	artist_menu = gtk_menu_new();
	artist__addlist = gtk_menu_item_new_with_label(_("Add to playlist"));
	artist__addlist_albummode = gtk_menu_item_new_with_label(_("Add to playlist (Album mode)"));
	artist__separator1 = gtk_separator_menu_item_new();
	artist__add = gtk_menu_item_new_with_label(_("Add new artist..."));
	artist__build = gtk_menu_item_new_with_label(_("Build artist from filesystem..."));
	artist__edit = gtk_menu_item_new_with_label(_("Edit artist..."));
	artist__remove = gtk_menu_item_new_with_label(_("Remove artist"));
	artist__separator2 = gtk_separator_menu_item_new();
	artist__addrec = gtk_menu_item_new_with_label(_("Add new record to this artist..."));
	artist__separator3 = gtk_separator_menu_item_new();
	artist__volume = gtk_menu_item_new_with_label(_("Calculate volume (recursive)"));
	artist__volume_menu = gtk_menu_new();
	artist__volume_unmeasured = gtk_menu_item_new_with_label(_("Unmeasured tracks only"));
	artist__volume_all = gtk_menu_item_new_with_label(_("All tracks"));
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
#endif /* HAVE_CDDB */
	record__separator3 = gtk_separator_menu_item_new();
	record__volume = gtk_menu_item_new_with_label(_("Calculate volume (recursive)"));
	record__volume_menu = gtk_menu_new();
	record__volume_unmeasured = gtk_menu_item_new_with_label(_("Unmeasured tracks only"));
	record__volume_all = gtk_menu_item_new_with_label(_("All tracks"));
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
#endif /* HAVE_CDDB */
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__separator3);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__volume);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(record__volume), record__volume_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(record__volume_menu), record__volume_unmeasured);
        gtk_menu_shell_append(GTK_MENU_SHELL(record__volume_menu), record__volume_all);
	gtk_menu_shell_append(GTK_MENU_SHELL(record_menu), record__search);

	g_signal_connect_swapped(G_OBJECT(record__addlist), "activate", G_CALLBACK(record__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__addlist_albummode), "activate", G_CALLBACK(record__addlist_albummode_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__add), "activate", G_CALLBACK(record__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__edit), "activate", G_CALLBACK(record__edit_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__remove), "activate", G_CALLBACK(record__remove_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__addtrk), "activate", G_CALLBACK(track__add_cb), NULL);
#ifdef HAVE_CDDB
	g_signal_connect_swapped(G_OBJECT(record__cddb), "activate", G_CALLBACK(record__cddb_cb), NULL);
#endif /* HAVE_CDDB */
	g_signal_connect_swapped(G_OBJECT(record__volume_unmeasured), "activate", G_CALLBACK(record__volume_unmeasured_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(record__volume_all), "activate", G_CALLBACK(record__volume_all_cb), NULL);
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
#endif /* HAVE_CDDB */
	gtk_widget_show(record__separator3);
	gtk_widget_show(record__volume);
	gtk_widget_show(record__volume_unmeasured);
	gtk_widget_show(record__volume_all);
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
	track__volume_unmeasured = gtk_menu_item_new_with_label(_("Unmeasured tracks only"));
	track__volume_all = gtk_menu_item_new_with_label(_("All tracks"));
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
	gtk_menu_shell_append(GTK_MENU_SHELL(track_menu), track__search);

	g_signal_connect_swapped(G_OBJECT(track__addlist), "activate", G_CALLBACK(track__addlist_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__add), "activate", G_CALLBACK(track__add_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__edit), "activate", G_CALLBACK(track__edit_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__remove), "activate", G_CALLBACK(track__remove_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__fileinfo), "activate", G_CALLBACK(track__fileinfo_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__volume_unmeasured), "activate", G_CALLBACK(track__volume_unmeasured_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(track__volume_all), "activate", G_CALLBACK(track__volume_all_cb), NULL);
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
	gtk_widget_show(track__search);

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
			};
			xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"sort_name"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				strncpy(sort_name, (char *) key, sizeof(sort_name)-1);
				sort_name[sizeof(sort_name) - 1] = '\0';
			};
			xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"file"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
			    if((converted_temp = g_filename_from_uri((char *) key, NULL, NULL))) {
                                strncpy(file, converted_temp, sizeof(file)-1);
				file[sizeof(file) - 1] = '\0';
			        g_free(converted_temp);
			    } else 
			    //try to read utf8 filename from outdated file
				if((converted_temp = g_locale_from_utf8((char *) key, -1, NULL, NULL, &error))) {
				    strncpy(file, converted_temp, sizeof(file)-1);
				    file[sizeof(file) - 1] = '\0';
                                    g_free(converted_temp);
                                } else {
                                //last try - maybe it's plain locale filename
				    strncpy(file, (char *) key, sizeof(file)-1);
				    file[sizeof(file) - 1] = '\0';
				};
			};
			xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"comment"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (key != NULL) {
				strncpy(comment, (char *) key, sizeof(comment)-1);
				comment[sizeof(comment) - 1] = '\0';
			};
			xmlFree(key);
		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"duration"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL) {
                                duration = convf((char *) key);
                        };
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
	} else {
		if (name[0] == '\0')
			fprintf(stderr, "Error in XML music_store: Track <name> is required, but NULL\n");
		else if (file[0] == '\0')
			fprintf(stderr, "Error in XML music_store: Track <file> is required, but NULL\n");
	}

	return;
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

	return;
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

	return;
}


void
load_music_store(void) {

	char name[MAXLEN];
	char comment[MAXLEN];

	name[0] = '\0';
	comment[0] = '\0';

	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar * key;

	GtkTreeIter iter_store;
	char * store_file;
	int i = 0;

	if (!ms_pathlist_store) {
		ms_pathlist_store = gtk_list_store_new(3,
						       G_TYPE_STRING,     /* path */
						       G_TYPE_STRING,     /* displayed name */
						       G_TYPE_STRING);    /* state (rw, r, unreachable) */
	}

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ms_pathlist_store), &iter_store, NULL, i++)) {
                gtk_tree_model_get(GTK_TREE_MODEL(ms_pathlist_store), &iter_store, 0, &store_file, -1);

		if (access(store_file, R_OK) != 0) continue;

		gtk_tree_store_append(music_store, &iter_store, NULL);
		gtk_tree_store_set(music_store, &iter_store, 0, _("Music Store"), 1, "",
				   2, store_file, 3, "", 7, -1.0f, 8, PANGO_WEIGHT_BOLD, -1);

		if (access(store_file, W_OK) == 0) {
			gtk_tree_store_set(music_store, &iter_store, 7, 1.0f, 8, PANGO_WEIGHT_BOLD, -1);
		}
	
		doc = xmlParseFile(store_file);
		if (doc == NULL) {
			fprintf(stderr, "An XML error occured while parsing %s\n", store_file);
			continue;
		}
	
		cur = xmlDocGetRootElement(doc);
		if (cur == NULL) {
			fprintf(stderr, "load_music_store: empty XML document\n");
			xmlFreeDoc(doc);
			continue;
		}

		if (xmlStrcmp(cur->name, (const xmlChar *)"music_store")) {
			fprintf(stderr, "load_music_store: XML document of the wrong type, "
				"root node != music_store\n");
			xmlFreeDoc(doc);
			continue;
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
                g_free(store_file);
        }

	music_tree_expand_stores();
}

void music_store_mark_changed(void) {

	music_store_changed = 1;
	gtk_window_set_title(GTK_WINDOW(browser_window), _("*Music Store"));
	if (options.enable_mstore_toolbar) {
		gtk_widget_set_sensitive(save_button, TRUE);
	}
}

void music_store_mark_saved(void) {

	music_store_changed = 0;
	gtk_window_set_title(GTK_WINDOW(browser_window), _("Music Store"));
	if (options.enable_mstore_toolbar) {
		gtk_widget_set_sensitive(save_button, FALSE);
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
save_store(xmlDocPtr doc, xmlNodePtr root, GtkTreeIter * iter_store) {

	char * name;
	char * comment;
	GtkTreeIter iter_artist;
	int i = 0;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_store,
			   0, &name, 3, &comment, -1);
	
	if (name[0] == '\0')
		fprintf(stderr, "saving music_store XML: warning: empty <name>\n");
	xmlNewTextChild(root, NULL, (const xmlChar *) "name", (const xmlChar *) name);
	if (comment[0] != '\0')
		xmlNewTextChild(root, NULL, (const xmlChar *) "comment", (const xmlChar *) comment);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_artist, iter_store, i++))
		save_artist(doc, root, &iter_artist);

	g_free(name);
	g_free(comment);
}


void
save_music_store(void) {

	xmlDocPtr doc;
	xmlNodePtr root;
	GtkTreeIter iter_store;
	int i = 0;
	char c, d, diff;
	FILE * fin;
	FILE * fout;
	char tmpname[MAXLEN];
	char * store_file;
	float state;


	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_store, NULL, i++)) {

		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_store, 2, &store_file, 7, &state, -1);

		if (state < 0) continue;

		doc = xmlNewDoc((const xmlChar *) "1.0");
		root = xmlNewNode(NULL, (const xmlChar *) "music_store");
		xmlDocSetRootElement(doc, root);

		save_store(doc, root, &iter_store);

		sprintf(tmpname, "%s.temp", store_file);
		xmlSaveFormatFile(tmpname, doc, 1);
	
		if ((fin = fopen(store_file, "rt")) == NULL) {
			fprintf(stderr, "Error opening file: %s\n", store_file);
			continue;
		}
		if ((fout = fopen(tmpname, "rt")) == NULL) {
			fprintf(stderr, "Error opening file: %s\n", tmpname);
			continue;
		}
	
		c = 0; d = 0; diff = 0;
		while (((c = fgetc(fin)) != EOF) && ((d = fgetc(fout)) != EOF)) {
			if (c != d) {
				diff = 1;
				break;
			}
		}

		fclose(fin);
		fclose(fout);

		if (diff) {
			struct stat st_file;

			if (stat(store_file, &st_file) == 0) {
				chown(tmpname, st_file.st_uid, st_file.st_gid);
				chmod(tmpname, st_file.st_mode);
			}

			unlink(store_file);
			rename(tmpname, store_file);
		} else {
			unlink(tmpname);
		}

		g_free(store_file);
	}

	music_store_mark_saved();
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

