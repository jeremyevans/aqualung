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
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

#include "common.h"
#include "version.h"
#include "i18n.h"
#include "about.h"


#define BUILT_IN "    [+] "
#define LEFT_OUT "    [ ] "
#ifndef HAVE_LADSPA
#define BUILT_W_LADSPA LEFT_OUT
#else /* HAVE_LADSPA */
#define BUILT_W_LADSPA BUILT_IN
#endif /* HAVE_LADSPA */
#ifndef HAVE_CDDA
#define BUILT_W_CDDA LEFT_OUT
#else /* HAVE_CDDA */
#define BUILT_W_CDDA BUILT_IN
#endif /* HAVE_CDDA */
#ifndef HAVE_CDDB
#define BUILT_W_CDDB LEFT_OUT
#else /* HAVE_CDDB */
#define BUILT_W_CDDB BUILT_IN
#endif /* HAVE_CDDB */
#ifndef HAVE_SRC
#define BUILT_W_SRC LEFT_OUT
#else /* HAVE_SRC */
#define BUILT_W_SRC BUILT_IN
#endif /* HAVE_SRC */
#ifndef HAVE_IFP
#define BUILT_W_IFP LEFT_OUT
#else /* HAVE_IFP */
#define BUILT_W_IFP BUILT_IN
#endif /* HAVE_IFP */
#ifndef HAVE_JACK_MGMT
#define BUILT_W_JACK_MGMT LEFT_OUT
#else /* HAVE_JACK_MGMT */
#define BUILT_W_JACK_MGMT BUILT_IN
#endif /* HAVE_JACK_MGMT */
#ifndef HAVE_LOOP
#define BUILT_W_LOOP LEFT_OUT
#else /* HAVE_LOOP */
#define BUILT_W_LOOP BUILT_IN
#endif /* HAVE_LOOP */
#ifndef HAVE_SYSTRAY
#define BUILT_W_SYSTRAY LEFT_OUT
#else /* HAVE_SYSTRAY */
#define BUILT_W_SYSTRAY BUILT_IN
#endif /* HAVE_SYSTRAY */
#ifndef HAVE_PODCAST
#define BUILT_W_PODCAST LEFT_OUT
#else /* HAVE_PODCAST */
#define BUILT_W_PODCAST BUILT_IN
#endif /* HAVE_PODCAST */
#ifndef HAVE_LUA
#define BUILT_W_LUA LEFT_OUT
#else /* HAVE_LUA */
#define BUILT_W_LUA BUILT_IN
#endif /* HAVE_LUA */
#ifndef HAVE_SNDFILE
#define BUILT_W_SNDFILE LEFT_OUT
#else /* HAVE_SNDFILE */
#define BUILT_W_SNDFILE BUILT_IN
#endif /* HAVE_SNDFILE */
#ifndef HAVE_FLAC
#define BUILT_W_FLAC LEFT_OUT
#else /* HAVE_FLAC */
#define BUILT_W_FLAC BUILT_IN
#endif /* HAVE_FLAC */
#ifndef HAVE_VORBIS
#define BUILT_W_VORBIS LEFT_OUT
#else /* HAVE_VORBIS */
#define BUILT_W_VORBIS BUILT_IN
#endif /* HAVE_VORBIS */
#ifndef HAVE_SPEEX
#define BUILT_W_SPEEX LEFT_OUT
#else /* HAVE_SPEEX */
#define BUILT_W_SPEEX BUILT_IN
#endif /* HAVE_SPEEX */
#ifndef HAVE_MPEG
#define BUILT_W_MPEG LEFT_OUT
#else /* HAVE_MPEG */
#define BUILT_W_MPEG BUILT_IN
#endif /* HAVE_MPEG */
#ifndef HAVE_MOD
#define BUILT_W_MOD LEFT_OUT
#else /* HAVE_MOD */
#define BUILT_W_MOD BUILT_IN
#endif /* HAVE_MOD */
#ifndef HAVE_MPC
#define BUILT_W_MPC LEFT_OUT
#else /* HAVE_MPC */
#define BUILT_W_MPC BUILT_IN
#endif /* HAVE_MPC */
#ifndef HAVE_MAC
#define BUILT_W_MAC LEFT_OUT
#else /* HAVE_MAC */
#define BUILT_W_MAC BUILT_IN
#endif /* HAVE_MAC */
#ifndef HAVE_WAVPACK
#define BUILT_W_WAVPACK LEFT_OUT
#else /* HAVE_WAVPACK */
#define BUILT_W_WAVPACK BUILT_IN
#endif /* HAVE_WAVPACK */
#ifndef HAVE_LAVC
#define BUILT_W_LAVC LEFT_OUT
#else /* HAVE_LAVC */
#define BUILT_W_LAVC BUILT_IN
#endif /* HAVE_LAVC */
#ifndef HAVE_SNDFILE_ENC
#define BUILT_W_SNDFILE_ENC LEFT_OUT
#else /* HAVE_SNDFILE_ENC */
#define BUILT_W_SNDFILE_ENC BUILT_IN
#endif /* HAVE_SNDFILE_ENC */
#ifndef HAVE_FLAC_ENC
#define BUILT_W_FLAC_ENC LEFT_OUT
#else /* HAVE_FLAC_ENC */
#define BUILT_W_FLAC_ENC BUILT_IN
#endif /* HAVE_FLAC_ENC */
#ifndef HAVE_VORBISENC
#define BUILT_W_VORBISENC LEFT_OUT
#else /* HAVE_VORBISENC */
#define BUILT_W_VORBISENC BUILT_IN
#endif /* HAVE_VORBISENC */
#ifndef HAVE_LAME
#define BUILT_W_LAME LEFT_OUT
#else /* HAVE_LAME */
#define BUILT_W_LAME BUILT_IN
#endif /* HAVE_LAME */
#ifndef HAVE_SNDIO
#define BUILT_W_SNDIO LEFT_OUT
#else /* HAVE_SNDIO */
#define BUILT_W_SNDIO BUILT_IN
#endif /* HAVE_SNDIO */
#ifndef HAVE_OSS
#define BUILT_W_OSS LEFT_OUT
#else /* HAVE_OSS */
#define BUILT_W_OSS BUILT_IN
#endif /* HAVE_OSS */
#ifndef HAVE_ALSA
#define BUILT_W_ALSA LEFT_OUT
#else /* HAVE_ALSA */
#define BUILT_W_ALSA BUILT_IN
#endif /* HAVE_ALSA */
#ifndef HAVE_JACK
#define BUILT_W_JACK LEFT_OUT
#else /* HAVE_JACK */
#define BUILT_W_JACK BUILT_IN
#endif /* HAVE_JACK */
#ifndef HAVE_PULSE
#define BUILT_W_PULSE LEFT_OUT
#else /* HAVE_PULSE */
#define BUILT_W_PULSE BUILT_IN
#endif /* HAVE_PULSE */
#ifndef HAVE_WINMM
#define BUILT_W_WINMM LEFT_OUT
#else /* HAVE_WINMM */
#define BUILT_W_WINMM BUILT_IN
#endif /* HAVE_WINMM */


GtkWidget * about_window;
extern GtkWidget * main_window;


static gint
ok(GtkWidget * widget, gpointer data) {

	gtk_widget_destroy(about_window);
	return TRUE;
}


gint
about_key_pressed(GtkWidget * widget, GdkEventKey * event, gpointer * data) {

        switch (event->keyval) {
	case GDK_q:
	case GDK_Q:
	case GDK_Escape:
		ok(NULL, NULL);
		return TRUE;
	};

	return FALSE;
}

gchar *
about_build(void) {

	return g_strconcat(
		"\n  ", _("Optional features:"), "\n",
		BUILT_W_LADSPA, _("LADSPA plugin support\n"),
		BUILT_W_CDDA, _("CDDA (Audio CD) support\n"),
		BUILT_W_CDDB, _("CDDB support\n"),
		BUILT_W_SRC, _("Sample Rate Converter support\n"),
		BUILT_W_IFP, _("iRiver iFP driver support\n"),
		BUILT_W_JACK_MGMT, _("JACK port management support\n"),
		BUILT_W_LOOP, _("Loop playback support\n"),
		BUILT_W_SYSTRAY, _("Systray support\n"),
		BUILT_W_PODCAST, _("Podcast support\n"),
		BUILT_W_LUA, _("Lua extension support\n"),
		"\n  ", _("Decoding support:"), "\n",
		BUILT_W_SNDFILE, _("sndfile (WAV, AIFF, etc.)\n"),
		BUILT_W_FLAC, _("Free Lossless Audio Codec (FLAC)\n"),
		BUILT_W_VORBIS, _("Ogg Vorbis\n"),
		BUILT_W_SPEEX, _("Ogg Speex\n"),
		BUILT_W_MPEG, _("MPEG Audio (MPEG 1-2.5 Layer I-III)\n"),
		BUILT_W_MOD, _("MOD Audio (MOD, S3M, XM, IT, etc.)\n"),
		BUILT_W_MPC, _("Musepack\n"),
		BUILT_W_MAC, _("Monkey's Audio Codec\n"),
		BUILT_W_WAVPACK, _("WavPack\n"),
		BUILT_W_LAVC, _("LAVC (AC3, AAC, WavPack, WMA, etc.)\n"),
		"\n  ", _("Encoding support:"), "\n",
		BUILT_W_SNDFILE_ENC, _("sndfile (WAV)\n"),
		BUILT_W_FLAC_ENC, _("Free Lossless Audio Codec (FLAC)\n"),
		BUILT_W_VORBISENC, _("Ogg Vorbis\n"),
		BUILT_W_LAME, _("LAME (MP3)\n"),
		"\n  ", _("Output driver support:"), "\n",
		BUILT_W_SNDIO, _("sndio Audio\n"),
		BUILT_W_OSS, _("OSS Audio\n"),
		BUILT_W_ALSA, _("ALSA Audio\n"),
		BUILT_W_JACK, _("JACK Audio Server\n"),
		BUILT_W_PULSE, _("PulseAudio\n"),
		BUILT_W_WINMM, _("Win32 Sound API\n"),
		NULL);
}

void
create_about_window() {

	GtkWidget * vbox0;
	GtkWidget * vbox;

	GtkWidget * xpm;
	GdkPixbuf * pixbuf;

	GtkWidget * frame;
	GtkWidget * scrolled_win;
	GtkWidget * vscrollbar;
	GtkWidget * view;
	GtkTextBuffer * buffer;
	GtkTextIter iter;

	GtkWidget * hbuttonbox;
	GtkWidget * ok_btn;

	GdkColor white = { 0, 49152, 51118, 52429 };
	GdkColor blue1 = { 0, 41288, 47841, 55050 };
	GdkColor blue2 = { 0, 45288, 51841, 60050 };
	GdkColor blue3 = { 0, 55552, 56832,  57600};

	GtkTextTag * tag;
	GtkTextTag * tag2;

	char path[MAXLEN];
	gchar * build_info = about_build();

	about_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_transient_for(GTK_WINDOW(about_window), GTK_WINDOW(main_window));
	gtk_widget_set_name(about_window, "");
        gtk_window_set_title(GTK_WINDOW(about_window), _("About"));
        gtk_widget_set_size_request(about_window, 483, 430);
	gtk_window_set_position(GTK_WINDOW(about_window), GTK_WIN_POS_CENTER);
	gtk_widget_modify_bg(about_window, GTK_STATE_NORMAL, &white);
        g_signal_connect(G_OBJECT(about_window), "key_press_event", G_CALLBACK(about_key_pressed), NULL);

        vbox0 = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(about_window), vbox0);

        vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_box_pack_end(GTK_BOX(vbox0), vbox, TRUE, TRUE, 0);

	hbuttonbox = gtk_hbutton_box_new();
	gtk_widget_set_name(hbuttonbox, "");
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        ok_btn = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_widget_set_name(ok_btn, "");
        g_signal_connect(ok_btn, "clicked", G_CALLBACK(ok), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), ok_btn);
	gtk_widget_modify_bg(ok_btn, GTK_STATE_NORMAL, &blue1);
	gtk_widget_modify_bg(ok_btn, GTK_STATE_PRELIGHT, &blue2);
	gtk_widget_modify_bg(ok_btn, GTK_STATE_ACTIVE, &blue2);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_name(scrolled_win, "");
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	vscrollbar = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(scrolled_win));
	gtk_widget_set_name(vscrollbar, "");
	gtk_widget_modify_bg(vscrollbar, GTK_STATE_NORMAL, &blue1);
	gtk_widget_modify_bg(vscrollbar, GTK_STATE_PRELIGHT, &blue2);
	gtk_widget_modify_bg(vscrollbar, GTK_STATE_ACTIVE, &blue3);
	gtk_widget_modify_bg(vscrollbar, GTK_STATE_INSENSITIVE, &blue2);


        view = gtk_text_view_new();
	gtk_widget_set_name(view, "");

	gtk_widget_modify_base(view, GTK_STATE_NORMAL, &blue3);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(view), buffer);

	tag = gtk_text_buffer_create_tag(buffer, NULL, "foreground", "#0000C0", NULL);
	tag2 = gtk_text_buffer_create_tag(buffer, NULL, "family", "monospace", NULL);


	/* insert text */

	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags(buffer, &iter, _("Build version: "), -1, tag, NULL);
	gtk_text_buffer_insert_at_cursor(buffer, AQUALUNG_VERSION, -1);

	gtk_text_buffer_insert_at_cursor(buffer, "\n\n", -1);
	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags(buffer, &iter, _("Homepage:"), -1, tag, NULL);
	gtk_text_buffer_insert_at_cursor(buffer, " http://aqualung.factorial.hu\n", -1);

	gtk_text_buffer_insert_at_cursor(buffer, "\nCopyright (C) 2004-2014 Tom Szilagyi\n\n\n", -1);

	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags(buffer, &iter, _("Authors:"), -1, tag, NULL);
	gtk_text_buffer_insert_at_cursor(buffer, "\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Core design, engineering & programming:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tTom Szilagyi <tszilagyi@users.sourceforge.net>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Skin support, look & feel, GUI hacks:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tPeter Szilagyi <peterszilagyi@users.sourceforge.net>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Programming, GUI engineering:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tTomasz Maka <pasp@users.sourceforge.net>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("OpenBSD compatibility, metadata tweaks:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tJeremy Evans <code@jeremyevans.net>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Miscellaneous, minimalism:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tJamie Heilman <jamie@audible.transient.net>\n\n\n", -1);

	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags(buffer, &iter, _("Translators:"), -1, tag, NULL);
	gtk_text_buffer_insert_at_cursor(buffer, "\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Chinese (simplified):\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tYinghua Wang <wantinghard@gmail.com>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Dutch:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tIvo van Poorten <ivop@free.fr>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("French:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tJulien Lavergne <gilir@ubuntu.com>\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tLouis Opter <kalessin@kalessin.fr>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("German:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tWolfgang St\303\266ggl <c72578@yahoo.de>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Hungarian:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tPeter Szilagyi <peterszilagyi@users.sourceforge.net>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Italian:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tMichele Petrecca <michelinux@alice.it>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Japanese:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tNorihiro Yoneda <aoba@avis.ne.jp>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Polish:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tRobert Wojewódzki <robwoj44@poczta.onet.pl>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Russian:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tAlexander Ilyashov <alexander.ilyashov@gmail.com>\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tVladimir Smolyar <v_2e@ukr.net>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Spanish:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tJorge Andrés Alvarez Oré <winninglero@gmail.com>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Swedish:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tNiklas 'Nixon' Grahn <terra.unknown@yahoo.com>\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Ukrainian:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tSergiy Niskorodov <sgh_punk@users.sourceforge.net>\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tVladimir Smolyar <v_2e@ukr.net>\n\n\n", -1);

	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags(buffer, &iter, _("Graphics:"), -1, tag, NULL);
	gtk_text_buffer_insert_at_cursor(buffer, "\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer, _("Logo, icons:\n"), -1);
	gtk_text_buffer_insert_at_cursor(buffer, "\tMaja Kocon <ironya@pinky-babble.org>\n\n\n", -1);


	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags(buffer, &iter,
					 _("This Aqualung binary is compiled with:"), -1, tag, NULL);
	gtk_text_buffer_insert_at_cursor(buffer, "\n", -1);
	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags(buffer, &iter, build_info, -1,
					 tag2, NULL);
	g_free(build_info);


	gtk_text_buffer_insert_at_cursor(buffer, "\n\n", -1);
	gtk_text_buffer_insert_at_cursor(buffer,
		       _("This program is free software; you can redistribute it and/or modify "
			 "it under the terms of the GNU General Public License as published by "
			 "the Free Software Foundation; either version 2 of the License, or "
			 "(at your option) any later version.\n\n"
			 "This program is distributed in the hope that it will be useful, "
			 "but WITHOUT ANY WARRANTY; without even the implied warranty of "
			 "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the "
			 "GNU General Public License for more details.\n\n"
			 "You should have received a copy of the GNU General Public License "
			 "along with this program; if not, write to the Free Software "
			 "Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA."), -1);

	gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);
	gtk_text_buffer_place_cursor(buffer, &iter);

	gtk_box_pack_end(GTK_BOX(vbox), frame, TRUE, TRUE, 6);
	gtk_container_add(GTK_CONTAINER(frame), scrolled_win);
	gtk_container_add(GTK_CONTAINER(scrolled_win), view);

	sprintf(path, "%s/logo.png", AQUALUNG_DATADIR);
        if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		xpm = gtk_image_new_from_pixbuf (pixbuf);
		gtk_box_pack_start(GTK_BOX(vbox0), xpm, FALSE, FALSE, 0);
		gtk_widget_show(xpm);
	}

	gtk_widget_show_all(about_window);

        gtk_widget_grab_focus(ok_btn);
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :

