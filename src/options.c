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
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#ifdef HAVE_SRC
#include <samplerate.h>
#endif /* HAVE_SRC */

#include "common.h"
#include "gui_main.h"
#include "options.h"


extern pthread_mutex_t output_thread_lock;

char title_format[MAXLEN];
char default_param[MAXLEN];

#ifdef HAVE_SRC
extern int src_type;
GtkWidget * optmenu_src;
#endif /* HAVE_SRC */

extern int ladspa_is_postfader;
extern GtkWidget * main_window;
extern int auto_save_playlist;
int auto_save_playlist_shadow;

GtkWidget * options_window;
GtkWidget * optmenu_ladspa;
GtkWidget * entry_title;
GtkWidget * entry_param;
GtkWidget * label_src;
GtkWidget * check_autoplsave;


static gint
ok(GtkWidget * widget, gpointer data) {

	strncpy(title_format, gtk_entry_get_text(GTK_ENTRY(entry_title)), MAXLEN - 1);
	strncpy(default_param, gtk_entry_get_text(GTK_ENTRY(entry_param)), MAXLEN - 1);
	auto_save_playlist = auto_save_playlist_shadow;

	gtk_widget_destroy(options_window);
	return TRUE;
}


static gint
cancel(GtkWidget * widget, gpointer data) {

	gtk_widget_destroy(options_window);
	return TRUE;
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
changed_ladspa_prepost(GtkWidget * widget, gpointer * data) {

	int status = gtk_option_menu_get_history(GTK_OPTION_MENU(optmenu_ladspa));
	pthread_mutex_lock(&output_thread_lock);
	ladspa_is_postfader = status;
	pthread_mutex_unlock(&output_thread_lock);
}


#ifdef HAVE_SRC
void
changed_src_type(GtkWidget * widget, gpointer * data) {

	src_type = gtk_option_menu_get_history(GTK_OPTION_MENU(optmenu_src));
	gtk_label_set_text(GTK_LABEL(label_src), src_get_description(src_type));
	set_src_type_label(src_type);
}
#endif /* HAVE_SRC */


void
create_options_window() {

	GtkWidget * vbox;

	GtkWidget * frame_title;
	GtkWidget * frame_param;
	GtkWidget * frame_autoplsave;
	GtkWidget * frame_ladspa;
	GtkWidget * frame_src;

	GtkWidget * vbox_title;
	GtkWidget * vbox_param;
	GtkWidget * vbox_autoplsave;
	GtkWidget * vbox_ladspa;
	GtkWidget * vbox_src;

	GtkWidget * label_title;
	GtkWidget * label_param;

	GtkWidget * hbox;
	GtkWidget * ok_btn;
	GtkWidget * cancel_btn;
	int status;


	options_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_transient_for(GTK_WINDOW(options_window), GTK_WINDOW(main_window));
        gtk_window_set_title(GTK_WINDOW(options_window), "Options");
	gtk_window_set_position(GTK_WINDOW(options_window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(options_window), TRUE);
        gtk_container_set_border_width(GTK_CONTAINER(options_window), 5);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(options_window), vbox);


	frame_title = gtk_frame_new("Title format");
	gtk_box_pack_start(GTK_BOX(vbox), frame_title, TRUE, TRUE, 0);

	vbox_title = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_title), 10);
	gtk_container_add(GTK_CONTAINER(frame_title), vbox_title);

	label_title = gtk_label_new("\nThe template string you enter here will be used to\n"
				    "construct a single title line from an Artist, a Record\n"
				    "and a Track name. These are denoted by %a, %r and %t,\n"
				    "respectively. Everything else you enter here will be\n"
				    "literally copied into the resulting string.\n");
	gtk_box_pack_start(GTK_BOX(vbox_title), label_title, TRUE, TRUE, 0);

	entry_title = gtk_entry_new_with_max_length(MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(entry_title), title_format);
	gtk_box_pack_start(GTK_BOX(vbox_title), entry_title, TRUE, TRUE, 0);


	frame_param = gtk_frame_new("Implicit command line");
	gtk_box_pack_start(GTK_BOX(vbox), frame_param, TRUE, TRUE, 0);

	vbox_param = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_param), 10);
	gtk_container_add(GTK_CONTAINER(frame_param), vbox_param);

	label_param = gtk_label_new("\nThe string you enter here will be parsed as a command\n"
				    "line before parsing the actual command line parameters.\n"
				    "What you enter here will act as a default setting and may\n"
				    "or may not be overrided from the 'real' command line.\n"
				    "Example: enter '-o alsa -R' below to use ALSA output\n"
				    "running realtime as a default.\n");
	gtk_box_pack_start(GTK_BOX(vbox_param), label_param, TRUE, TRUE, 0);

	entry_param = gtk_entry_new_with_max_length(MAXLEN - 1);
	gtk_entry_set_text(GTK_ENTRY(entry_param), default_param);
	gtk_box_pack_start(GTK_BOX(vbox_param), entry_param, TRUE, TRUE, 0);


	frame_autoplsave = gtk_frame_new("Auto-save playlist");
	gtk_box_pack_start(GTK_BOX(vbox), frame_autoplsave, TRUE, TRUE, 0);

        vbox_autoplsave = gtk_vbox_new(FALSE, 3);
        gtk_container_set_border_width(GTK_CONTAINER(vbox_autoplsave), 10);
        gtk_container_add(GTK_CONTAINER(frame_autoplsave), vbox_autoplsave);

	check_autoplsave = gtk_check_button_new_with_label("Save and restore the playlist "
							   "automatically on exit/startup");
	if (auto_save_playlist) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_autoplsave), TRUE);
	}
	auto_save_playlist_shadow = auto_save_playlist;
	g_signal_connect(G_OBJECT(check_autoplsave), "toggled", G_CALLBACK(autoplsave_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox_autoplsave), check_autoplsave, TRUE, TRUE, 0);


	frame_ladspa = gtk_frame_new("LADSPA plugin processing");
	gtk_box_pack_start(GTK_BOX(vbox), frame_ladspa, TRUE, TRUE, 0);

	vbox_ladspa = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_ladspa), 10);
	gtk_container_add(GTK_CONTAINER(frame_ladspa), vbox_ladspa);

	optmenu_ladspa = gtk_option_menu_new();
        gtk_box_pack_start(GTK_BOX(vbox_ladspa), optmenu_ladspa, TRUE, TRUE, 0);

	{
		GtkWidget * menu = gtk_menu_new();
		GtkWidget * item;

		item = gtk_menu_item_new_with_label("Pre Fader (before Volume & Balance)");
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);

		item = gtk_menu_item_new_with_label("Post Fader (after Volume & Balance)");
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);

		gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu_ladspa), menu);
	}
	pthread_mutex_lock(&output_thread_lock);
	status = ladspa_is_postfader;
	pthread_mutex_unlock(&output_thread_lock);
        gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu_ladspa), status);
        g_signal_connect(optmenu_ladspa, "changed", G_CALLBACK(changed_ladspa_prepost), NULL);


	frame_src = gtk_frame_new("Sample Rate Converter type");
	gtk_box_pack_start(GTK_BOX(vbox), frame_src, TRUE, TRUE, 0);

	vbox_src = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_src), 10);
	gtk_container_add(GTK_CONTAINER(frame_src), vbox_src);

	label_src = gtk_label_new("");

#ifdef HAVE_SRC
	optmenu_src = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(vbox_src), optmenu_src, TRUE, TRUE, 0);

	{
		GtkWidget * menu = gtk_menu_new();
		GtkWidget * item;
		int i = 0;

		while (src_get_name(i)) {
			item = gtk_menu_item_new_with_label(src_get_name(i));
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			gtk_widget_show(item);
			++i;
		}

		gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu_src), menu);
	}

	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu_src), src_type);
	g_signal_connect(optmenu_src, "changed", G_CALLBACK(changed_src_type), NULL);

	gtk_label_set_text(GTK_LABEL(label_src), src_get_description(src_type));
#else
	gtk_label_set_text(GTK_LABEL(label_src),
			   "Aqualung is compiled without Sample Rate Converter support.\n"
			   "See the About box and the documentation for details.");

#endif /* HAVE_SRC */

	gtk_box_pack_start(GTK_BOX(vbox_src), label_src, TRUE, TRUE, 0);


	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

	cancel_btn = gtk_button_new_with_label("Cancel");
	gtk_widget_set_size_request(cancel_btn, 60, 30);
	g_signal_connect(cancel_btn, "clicked", G_CALLBACK(cancel), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), cancel_btn, FALSE, FALSE, 6);

	ok_btn = gtk_button_new_with_label("OK");
	gtk_widget_set_size_request(ok_btn, 60, 30);
	g_signal_connect(ok_btn, "clicked", G_CALLBACK(ok), NULL);
	gtk_box_pack_end(GTK_BOX(hbox), ok_btn, FALSE, FALSE, 6);

	gtk_widget_show_all(options_window);
}
