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

#include "about.h"
#include "version.h"
#include "logo.xpm"

GtkWidget * about_window;
extern GtkWidget * main_window;

static gint
ok(GtkWidget * widget, gpointer data) {

	gtk_widget_destroy(about_window);
	return TRUE;
}


void
create_about_window() {

	GtkWidget * vbox;

	GtkWidget * xpm;
	GdkPixmap * pixmap;
	GdkBitmap * mask;
	GtkStyle * style;

	GtkWidget * viewp;
	GtkWidget * scrolled_win;
	GtkWidget * view;
	GtkTextBuffer * buffer;
	GtkTextIter iter;

	GtkWidget * ok_btn;
	GtkWidget * label;

	GdkColor white = { 0, 65535, 65535, 65535 };
	GdkColor blue1 = { 0, 60000, 60000, 65535 };
	GdkColor blue2 = { 0, 62500, 62500, 65535 };
	GdkColor blue3 = { 0, 63000, 63000, 65535 };

	GtkTextTag * tag;

	PangoFontDescription * font_desc = pango_font_description_new();


	pango_font_description_set_family(font_desc, "Courier");
	pango_font_description_set_weight(font_desc, PANGO_WEIGHT_BOLD);
	pango_font_description_set_size(font_desc, 1);


	about_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_transient_for(GTK_WINDOW(about_window), GTK_WINDOW(main_window));
	gtk_window_set_modal(GTK_WINDOW(about_window), TRUE);
	gtk_widget_set_name(about_window, "");
        gtk_window_set_title(GTK_WINDOW(about_window), "About");
	gtk_window_set_position(GTK_WINDOW(about_window), GTK_WIN_POS_CENTER);
	gtk_widget_set_size_request(about_window, -1, 350);
	gtk_widget_modify_bg(about_window, GTK_STATE_NORMAL, &white);

        vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
        gtk_container_add(GTK_CONTAINER(about_window), vbox);


	label = gtk_label_new("Dismiss");
	gtk_widget_modify_font(label, font_desc);

	ok_btn = gtk_button_new();
	gtk_widget_set_name(ok_btn, "");
	gtk_widget_set_size_request(ok_btn, 50, 30);
	gtk_container_add(GTK_CONTAINER(ok_btn), label);
	g_signal_connect(ok_btn, "clicked", G_CALLBACK(ok), NULL);
	gtk_box_pack_end(GTK_BOX(vbox), ok_btn, FALSE, FALSE, 6);
	gtk_widget_modify_bg(ok_btn, GTK_STATE_NORMAL, &blue1);
	gtk_widget_modify_bg(ok_btn, GTK_STATE_PRELIGHT, &blue2);
	gtk_widget_modify_bg(ok_btn, GTK_STATE_ACTIVE, &blue3);

	
	viewp = gtk_viewport_new(NULL, NULL);
	gtk_widget_set_name(viewp, "");
	gtk_box_pack_end(GTK_BOX(vbox), viewp, TRUE, TRUE, 0);
	
	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_name(scrolled_win, "");
	gtk_widget_set_name(GTK_SCROLLED_WINDOW(scrolled_win)->vscrollbar, "");
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(viewp), scrolled_win);
	gtk_widget_modify_bg(GTK_SCROLLED_WINDOW(scrolled_win)->vscrollbar, GTK_STATE_NORMAL, &blue1);
	gtk_widget_modify_bg(GTK_SCROLLED_WINDOW(scrolled_win)->vscrollbar, GTK_STATE_PRELIGHT, &blue2);
	gtk_widget_modify_bg(GTK_SCROLLED_WINDOW(scrolled_win)->vscrollbar, GTK_STATE_ACTIVE, &blue3);


        view = gtk_text_view_new();
	gtk_widget_set_name(view, "");

	gtk_widget_modify_base(view, GTK_STATE_NORMAL, &blue3);
	gtk_widget_modify_font(view, font_desc);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 3);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
        gtk_text_view_set_buffer(GTK_TEXT_VIEW(view), buffer);

	tag = gtk_text_buffer_create_tag(buffer, NULL, "foreground", "#0000C0", NULL);


	/* insert text */

	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags(buffer, &iter, "Build version: ", -1, tag, NULL);
	gtk_text_buffer_insert_at_cursor(buffer, aqualung_version, -1);


	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags(buffer, &iter, "\n\nHomepage: ", -1, tag, NULL);
	gtk_text_buffer_insert_at_cursor(buffer, "http://aqualung.sf.net\n", -1);

	gtk_text_buffer_insert_at_cursor(buffer, "\nCopyright (C) 2004 Tom Szilagyi\n\n", -1);

	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags(buffer, &iter, "\nAuthors:\n\n", -1, tag, NULL);
	gtk_text_buffer_insert_at_cursor(buffer,
					 "Core design, engineering & programming:\t"
					 "Tom Szilagyi <tszilagyi@users.sourceforge.net>\n"
					 "Skin support, look & feel, GUI hacks:\t"
					 "Peter Szilagyi <szilagyi.peter@index.hu>\n\n", -1);

	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags(buffer, &iter,
					 "\nThis Aqualung binary is compiled with:\n", -1, tag, NULL);

	gtk_text_buffer_insert_at_cursor(buffer, "\n\tFile format support:\n", -1);
					 
	gtk_text_buffer_insert_at_cursor(buffer, "\t\tsndfile (WAV, AIFF, etc.)\t\t\t: ", -1);
#ifdef HAVE_SNDFILE
	gtk_text_buffer_insert_at_cursor(buffer, "yes\n", -1);
#else
	gtk_text_buffer_insert_at_cursor(buffer, "no\n", -1);
#endif /* HAVE_SNDFILE */
	
	gtk_text_buffer_insert_at_cursor(buffer, "\t\tFree Lossless Audio Codec (FLAC)\t\t: ", -1);
#ifdef HAVE_FLAC
	gtk_text_buffer_insert_at_cursor(buffer, "yes\n", -1);
#else
	gtk_text_buffer_insert_at_cursor(buffer, "no\n", -1);
#endif /* HAVE_FLAC */
	
	gtk_text_buffer_insert_at_cursor(buffer, "\t\tOgg Vorbis\t\t\t\t\t: ", -1);
#ifdef HAVE_OGG_VORBIS
	gtk_text_buffer_insert_at_cursor(buffer, "yes\n", -1);
#else
	gtk_text_buffer_insert_at_cursor(buffer, "no\n", -1);
#endif /* HAVE_OGG_VORBIS */
	
	gtk_text_buffer_insert_at_cursor(buffer, "\t\tMPEG Audio (MPEG 1-2.5 Layer I-III)\t\t: ", -1);
#ifdef HAVE_MPEG
	gtk_text_buffer_insert_at_cursor(buffer, "yes\n", -1);
#else
	gtk_text_buffer_insert_at_cursor(buffer, "no\n", -1);
#endif /* HAVE_MPEG */
	
        gtk_text_buffer_insert_at_cursor(buffer, "\t\tMOD Audio (MOD, S3M, XM, IT, etc.)\t\t: ", -1);
#ifdef HAVE_MOD
        gtk_text_buffer_insert_at_cursor(buffer, "yes\n", -1);
#else
        gtk_text_buffer_insert_at_cursor(buffer, "no\n", -1);
#endif /* HAVE_MOD */

	gtk_text_buffer_insert_at_cursor(buffer, "\n\tOutput driver support:\n", -1);
	
	gtk_text_buffer_insert_at_cursor(buffer, "\t\tOSS Audio\t\t\t\t\t: ", -1);
#ifdef HAVE_OSS
	gtk_text_buffer_insert_at_cursor(buffer, "yes\n", -1);
#else
	gtk_text_buffer_insert_at_cursor(buffer, "no\n", -1);
#endif /* HAVE_OSS */
	
	gtk_text_buffer_insert_at_cursor(buffer, "\t\tALSA Audio\t\t\t\t\t: ", -1);
#ifdef HAVE_ALSA
	gtk_text_buffer_insert_at_cursor(buffer, "yes\n", -1);
#else
	gtk_text_buffer_insert_at_cursor(buffer, "no\n", -1);
#endif /* HAVE_ALSA */
	
	gtk_text_buffer_insert_at_cursor(buffer, "\t\tJACK Audio Server\t\t\t\t: yes (always)\n", -1);
	
	gtk_text_buffer_insert_at_cursor(buffer, "\n\tInternal Sample Rate Converter support\t\t\t: ", -1);
#ifdef HAVE_SRC
	gtk_text_buffer_insert_at_cursor(buffer, "yes\n\n", -1);
#else
	gtk_text_buffer_insert_at_cursor(buffer, "no\n\n", -1);
#endif /* HAVE_SRC */


	gtk_text_buffer_insert_at_cursor(buffer,
			 "\nAqualung could not be functional without the following libraries.\n"
			 "We would like to thank the authors of these software packages:\n\n"

			 "* The GIMP Toolkit (GTK+), version 2.0\n"
			 "* libXML2, the XML C parser and toolkit of Gnome\n"
			 "* libJACK, the JACK Audio Connection Kit support library\n"
			 "* liblrdf, a library for manipulating RDF files about LADSPA plugins\n"
			 "* libsamplerate (aka Secret Rabbit Code), a high quality Sample Rate Converter\n"
			 "* libsndfile, a library for accessing audio files containing sampled sound\n"
			 "* libvorbis & libvorbisfile, for decoding Ogg Vorbis audio\n"
			 "* libFLAC, the Free Lossless Audio Codec library\n"
			 "* libMAD, a GPL'ed MPEG Audio Decoder library\n"
			 "* libmodplug, a MOD decoder library in the public domain\n\n", -1);


	gtk_text_buffer_insert_at_cursor(buffer,
				"\nThis program is free software; you can redistribute it and/or modify\n"
				"it under the terms of the GNU General Public License as published by\n"
				"the Free Software Foundation; either version 2 of the License, or\n"
				"(at your option) any later version.\n"

				"\nThis program is distributed in the hope that it will be useful,\n"
				"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
				"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
				"GNU General Public License for more details.\n"

				"\nYou should have received a copy of the GNU General Public License\n"
				"along with this program; if not, write to the Free Software\n"
				"Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.", -1);


	gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);
	gtk_text_buffer_place_cursor(buffer, &iter);

	gtk_container_add(GTK_CONTAINER(scrolled_win), view);

	gtk_widget_show_all(about_window);

	style = gtk_widget_get_style(about_window);
	pixmap = gdk_pixmap_create_from_xpm_d(about_window->window, &mask,
					      &style->bg[GTK_STATE_NORMAL],
					      (gchar **)logo_xpm);
	xpm = gtk_pixmap_new(pixmap, mask);
	gtk_box_pack_end(GTK_BOX(vbox), xpm, FALSE, FALSE, 0);

	gtk_widget_show(xpm);
}
