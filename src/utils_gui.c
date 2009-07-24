/*                                                     -*- linux-c -*-
    Copyright (C) 2007 Tom Szilagyi

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

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"
#include "i18n.h"
#include "options.h"
#include "utils.h"
#include "utils_gui.h"


extern options_t options;

#ifdef HAVE_SNDFILE
extern char * valid_extensions_sndfile[];
#endif /* HAVE_SNDFILE */

#ifdef HAVE_MPEG
extern char * valid_extensions_mpeg[];
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
extern char * valid_extensions_mod[];
#endif /* HAVE_MOD */


GSList * toplevel_windows;

typedef struct {
	GtkWidget * window;
	int flag;
} top_win_t;

void
unregister_toplevel_window(GtkWidget * window) {

	GSList * node;

	for (node = toplevel_windows; node; node = node->next) {

		top_win_t * tw = (top_win_t *)node->data;

		if (tw->window == window) {
			toplevel_windows = g_slist_remove(toplevel_windows, tw);
			free(tw);
			break;
		}
	}
}

void
register_toplevel_window(GtkWidget * window, int flag) {

	top_win_t * tw;

	if ((tw = (top_win_t *)calloc(1, sizeof(top_win_t))) == NULL) {
		fprintf(stderr, "register_toplevel_window: calloc error\n");
		return;
	}

	tw->window = window;
	tw->flag = flag;

	unregister_toplevel_window(window);
	toplevel_windows = g_slist_append(toplevel_windows, tw);
}

void
toplevel_window_foreach(int flag, void (* callback)(GtkWidget * window)) {

	GSList * node;

	for (node = toplevel_windows; node; node = node->next) {

		top_win_t * tw = (top_win_t *)node->data;

		if (tw->flag & flag) {
			callback(tw->window);
		}
	}
}


#ifdef HAVE_SYSTRAY
int systray_semaphore = 0;

int
get_systray_semaphore() {

	return systray_semaphore;
}

#endif /* HAVE_SYSTRAY */


gint
aqualung_dialog_run(GtkDialog * dialog) {
#ifdef HAVE_SYSTRAY
	int ret;
	systray_semaphore++;
	gtk_window_present(GTK_WINDOW(dialog));
	ret = gtk_dialog_run(dialog);
	systray_semaphore--;
	return ret;
#else
	gtk_window_present(GTK_WINDOW(dialog));
	return gtk_dialog_run(dialog);
#endif /* HAVE_SYSTRAY */
}

typedef struct {
	GSourceFunc func;
	gpointer data;
	GDestroyNotify destroy;
} threads_dispatch_t;

static gboolean
threads_dispatch(gpointer data) {

	threads_dispatch_t * dispatch = data;
	gboolean ret = FALSE;

	GDK_THREADS_ENTER();

	if (!g_source_is_destroyed(g_main_current_source())) {
		ret = dispatch->func(dispatch->data);
	}

	GDK_THREADS_LEAVE();

	return ret;
}

static void
threads_dispatch_free(gpointer data) {

	threads_dispatch_t * dispatch = data;

	if (dispatch->destroy && dispatch->data) {
		dispatch->destroy(dispatch->data);
	}

	g_slice_free(threads_dispatch_t, data);
}

guint
aqualung_idle_add(GSourceFunc function, gpointer data) {

	threads_dispatch_t * dispatch;

	dispatch = g_slice_new(threads_dispatch_t);
	dispatch->func = function;
	dispatch->data = data;
	dispatch->destroy = NULL;

	return g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
			       threads_dispatch,
			       dispatch,
			       threads_dispatch_free);
}

guint
aqualung_timeout_add(guint interval, GSourceFunc function, gpointer data) {

	threads_dispatch_t * dispatch;

	dispatch = g_slice_new(threads_dispatch_t);
	dispatch->func = function;
	dispatch->data = data;
	dispatch->destroy = NULL;

	return g_timeout_add_full(G_PRIORITY_DEFAULT,
				  interval,
				  threads_dispatch,
				  dispatch,
				  threads_dispatch_free);
}


/* create button with stock item
 *
 * in: label - label for button        (label=NULL  to disable label, label=-1 to disable button relief)
 *     stock - stock icon identifier
 */
GtkWidget*
gui_stock_label_button(gchar *label, const gchar *stock) {

	GtkWidget *button;
	GtkWidget *alignment;
	GtkWidget *hbox;
	GtkWidget *image;

	button = g_object_new (GTK_TYPE_BUTTON, "visible", TRUE, NULL);

        if (label== (gchar *)-1) {
                gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
        }

	alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (alignment), hbox);

	image = gtk_image_new_from_stock (stock, GTK_ICON_SIZE_BUTTON);

        if (image) {
		gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);
        }

        if (label != NULL && label != (gchar *)-1) {
		gtk_box_pack_start (GTK_BOX (hbox),
		g_object_new (GTK_TYPE_LABEL, "label", label, "use_underline", TRUE, NULL),
		FALSE, TRUE, 0);
        }

	gtk_widget_show_all (alignment);
	gtk_container_add (GTK_CONTAINER (button), alignment);

	return button;
}


void
assign_etf_fc_filters(GtkFileChooser *fc) {

        GtkFileFilter *filter_1, *filter_2;

        /* all files filter */
        filter_1 = gtk_file_filter_new();
        gtk_file_filter_add_pattern(filter_1, "*");
        gtk_file_filter_set_name(GTK_FILE_FILTER(filter_1), _("All Files"));
        gtk_file_chooser_add_filter(fc, filter_1);

        /* music store files filter */
        filter_2 = gtk_file_filter_new();
        gtk_file_filter_add_pattern(filter_2, "*.[lL][uU][aA]");
        gtk_file_filter_set_name(GTK_FILE_FILTER(filter_2), _("Extended Title Format Files (*.lua)"));
        gtk_file_chooser_add_filter(fc, filter_2);

        gtk_file_chooser_set_filter(fc, filter_2);
}


void
assign_store_fc_filters(GtkFileChooser *fc) {

        GtkFileFilter *filter_1, *filter_2;

        /* all files filter */
        filter_1 = gtk_file_filter_new();
        gtk_file_filter_add_pattern(filter_1, "*");
        gtk_file_filter_set_name(GTK_FILE_FILTER(filter_1), _("All Files"));
        gtk_file_chooser_add_filter(fc, filter_1);

        /* music store files filter */
        filter_2 = gtk_file_filter_new();
        gtk_file_filter_add_pattern(filter_2, "*.[xX][mM][lL]");
        gtk_file_filter_set_name(GTK_FILE_FILTER(filter_2), _("Music Store Files (*.xml)"));
        gtk_file_chooser_add_filter(fc, filter_2);

        gtk_file_chooser_set_filter(fc, filter_2);
}


void
assign_playlist_fc_filters(GtkFileChooser *fc) {

        gchar *file_filters[] = { 
                _("Aqualung playlist (*.xml)"),      "*.[xX][mM][lL]",
                _("MP3 Playlist (*.m3u)"),           "*.[mM]3[uU]",
                _("Multimedia Playlist (*.pls)"),    "*.[pP][lL][sS]",
        };

        gint i, len;
        GtkFileFilter *filter_1, *filter_2, *filter_3;

        len = sizeof(file_filters)/sizeof(gchar*)/2;

        /* all files filter */
        filter_1 = gtk_file_filter_new();
        gtk_file_filter_add_pattern(filter_1, "*");
        gtk_file_filter_set_name(GTK_FILE_FILTER(filter_1), _("All Files"));
        gtk_file_chooser_add_filter(fc, filter_1);

        /* all playlist files filter */
        filter_2 = gtk_file_filter_new();

        for (i = 0; i < len; i++) {
                gtk_file_filter_add_pattern(filter_2, file_filters[2*i+1]);
        }

        gtk_file_filter_set_name(GTK_FILE_FILTER(filter_2), _("All Playlist Files"));
        gtk_file_chooser_add_filter(fc, filter_2);
        gtk_file_chooser_set_filter(fc, filter_2);

        /* single extensions */
        for (i = 0; i < len; i++) {

                filter_3 = gtk_file_filter_new();
                gtk_file_filter_add_pattern(filter_3, file_filters[2*i+1]);
                gtk_file_filter_set_name(GTK_FILE_FILTER(filter_3), file_filters[2*i]);
                gtk_file_chooser_add_filter(fc, filter_3);
        }
}


void
build_filter_from_extensions(GtkFileFilter * f1, GtkFileFilter * f2, char * extensions[]) {

	int i, j, k;

	for (i = 0; extensions[i]; i++) {
		char buf[32];
		buf[0] = '*';
		buf[1] = '.';
		k = 2;
		for (j = 0; extensions[i][j]; j++) {
			if (isalpha(extensions[i][j]) && k < 28) {
				buf[k++] = '[';
				buf[k++] = tolower(extensions[i][j]);
				buf[k++] = toupper(extensions[i][j]);
				buf[k++] = ']';
			} else if (k < 31) {
				buf[k++] = extensions[i][j];
			}
		}
		buf[k] = '\0';
		gtk_file_filter_add_pattern(f1, buf);
		gtk_file_filter_add_pattern(f2, buf);
	}
}


void
assign_audio_fc_filters(GtkFileChooser * fc) {

        GtkFileFilter * filter;
        GtkFileFilter * filter_all;

        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("All Files"));
        gtk_file_filter_add_pattern(filter, "*");
        gtk_file_chooser_add_filter(fc, filter);

        filter_all = gtk_file_filter_new();
        gtk_file_filter_set_name(filter_all, _("All Audio Files"));
        gtk_file_chooser_add_filter(fc, filter_all);
        gtk_file_chooser_set_filter(fc, filter_all);

#ifdef HAVE_SNDFILE
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("Sound Files (*.wav, *.aiff, *.au, ...)"));
	build_filter_from_extensions(filter, filter_all, valid_extensions_sndfile);
        gtk_file_chooser_add_filter(fc, filter);
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("Free Lossless Audio Codec (*.flac)"));
        gtk_file_filter_add_pattern(filter, "*.[fF][lL][aA][cC]");
        gtk_file_filter_add_pattern(filter_all, "*.[fF][lL][aA][cC]");
        gtk_file_chooser_add_filter(fc, filter);
#endif /* HAVE_FLAC */

#ifdef HAVE_MPEG
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("MPEG Audio (*.mp3, *.mpa, *.mpega, ...)"));
	build_filter_from_extensions(filter, filter_all, valid_extensions_mpeg);
        gtk_file_chooser_add_filter(fc, filter);
#endif /* HAME_MPEG */

#ifdef HAVE_OGG_VORBIS
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("Ogg Vorbis (*.ogg)"));
        gtk_file_filter_add_pattern(filter, "*.[oO][gG][gG]");
        gtk_file_filter_add_pattern(filter_all, "*.[oO][gG][gG]");
        gtk_file_chooser_add_filter(fc, filter);
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_SPEEX
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("Ogg Speex (*.spx)"));
        gtk_file_filter_add_pattern(filter, "*.[sS][pP][xX]");
        gtk_file_filter_add_pattern(filter_all, "*.[sS][pP][xX]");
        gtk_file_chooser_add_filter(fc, filter);
#endif /* HAVE_SPEEX */

#ifdef HAVE_MPC
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("Musepack (*.mpc)"));
        gtk_file_filter_add_pattern(filter, "*.[mM][pP][cC]");
        gtk_file_filter_add_pattern(filter_all, "*.[mM][pP][cC]");
        gtk_file_chooser_add_filter(fc, filter);
#endif /* HAVE_MPC */

#ifdef HAVE_MAC
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("Monkey's Audio Codec (*.ape)"));
        gtk_file_filter_add_pattern(filter, "*.[aA][pP][eE]");
        gtk_file_filter_add_pattern(filter_all, "*.[aA][pP][eE]");
        gtk_file_chooser_add_filter(fc, filter);
#endif /* HAVE_MAC */

#ifdef HAVE_MOD
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("Modules (*.xm, *.mod, *.it, *.s3m, ...)"));
	build_filter_from_extensions(filter, filter_all, valid_extensions_mod);
        gtk_file_chooser_add_filter(fc, filter);
#endif /* HAVE_MOD */

#if defined(HAVE_MOD) && (defined(HAVE_LIBZ) || defined(HAVE_LIBBZ2))
        filter = gtk_file_filter_new();
#if defined(HAVE_LIBZ) && defined(HAVE_LIBBZ2)
        gtk_file_filter_set_name(filter, _("Compressed modules (*.gz, *.bz2)"));
#elif defined(HAVE_LIBZ)
        gtk_file_filter_set_name(filter, _("Compressed modules (*.gz)"));
#elif defined(HAVE_LIBBZ2)
        gtk_file_filter_set_name(filter, _("Compressed modules (*.bz2)"));
#endif /* HAVE_LIBZ, HAVE_LIBBZ2 */

#ifdef HAVE_LIBZ
        gtk_file_filter_add_pattern(filter, "*.[gG][zZ]");
        gtk_file_filter_add_pattern(filter_all, "*.[gG][zZ]");
#endif /* HAVE_LIBZ */
#ifdef HAVE_LIBBZ2
        gtk_file_filter_add_pattern(filter, "*.[bB][zZ]2");
        gtk_file_filter_add_pattern(filter_all, "*.[bB][zZ]2");
#endif /* HAVE_LIBBZ2 */
        gtk_file_chooser_add_filter(fc, filter);
#endif /* (HAVE_MOD && HAVE LIBZ) */

#ifdef HAVE_WAVPACK
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("WavPack (*.wv)"));
        gtk_file_filter_add_pattern(filter, "*.[wW][vV]");
        gtk_file_filter_add_pattern(filter_all, "*.[wW][vV]");
        gtk_file_chooser_add_filter(fc, filter);
#endif /* HAVE_WAVPACK */

#ifdef HAVE_LAVC
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, _("LAVC audio/video files"));
	{
		char * valid_ext_lavc[] = {
			"aac", "ac3", "asf", "avi", "mpeg", "mpg", "mp3", "ra",
			"wav", "wma", "wv", NULL };
		build_filter_from_extensions(filter, filter_all, valid_ext_lavc);
	}
        gtk_file_chooser_add_filter(fc, filter);
#endif /* HAVE_LAVC */
}


void
assign_fc_filters(GtkFileChooser * fc, int filter) {

        gtk_widget_realize(GTK_WIDGET(fc));

	if (filter == FILE_CHOOSER_FILTER_AUDIO) {
		assign_audio_fc_filters(fc);
	}
	if (filter == FILE_CHOOSER_FILTER_PLAYLIST) {
		assign_playlist_fc_filters(fc);
	}
	if (filter == FILE_CHOOSER_FILTER_STORE) {
		assign_store_fc_filters(fc);
	}
	if (filter == FILE_CHOOSER_FILTER_ETF) {
		assign_etf_fc_filters(fc);
	}
}

GSList *
file_chooser(char * title, GtkWidget * parent, GtkFileChooserAction action, int filter,
	     gint multiple, char * destpath) {

        GtkWidget * dialog;
	GSList * files = NULL;

        dialog = gtk_file_chooser_dialog_new(title,
                                             GTK_WINDOW(parent),
                                             action,
					     (action == GTK_FILE_CHOOSER_ACTION_SAVE) ? GTK_STOCK_SAVE : GTK_STOCK_OPEN,
					     GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), multiple);
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), destpath);

	if (action == GTK_FILE_CHOOSER_ACTION_SAVE) {
		char * bname = g_path_get_basename(destpath);
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), bname);
		g_free(bname);
	}

        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	assign_fc_filters(GTK_FILE_CHOOSER(dialog), filter);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		strncpy(destpath,
			gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)),
			MAXLEN-1);

		files = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
        }

        gtk_widget_destroy(dialog);

	return files;
}

void
file_chooser_with_entry(char * title, GtkWidget * parent, GtkFileChooserAction action, int filter,
			GtkWidget * entry, char * destpath) {

        GtkWidget * dialog;
	const gchar * selected_filename = gtk_entry_get_text(GTK_ENTRY(entry));
	char path[MAXLEN];

	path[0] = '\0';

        dialog = gtk_file_chooser_dialog_new(title,
                                             GTK_WINDOW(parent),
					     action,
                                             GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), options.show_hidden);
	}

        if (strlen(selected_filename)) {
      		char * filename = g_filename_from_utf8(selected_filename, -1, NULL, NULL, NULL);

		if (filename == NULL) {
			gtk_widget_destroy(dialog);
			return;
		}

		normalize_filename(filename, path);
		g_free(filename);
	} else {
		strncpy(path, destpath, MAXLEN-1);
	}

	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), path);

	if (action == GTK_FILE_CHOOSER_ACTION_SAVE) {
		char * bname = g_path_get_basename(path);
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), bname);
		g_free(bname);
	}

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	assign_fc_filters(GTK_FILE_CHOOSER(dialog), filter);

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

		char * utf8;

                selected_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		utf8 = g_filename_to_utf8(selected_filename, -1, NULL, NULL, NULL);

		if (utf8 == NULL) {
			gtk_widget_destroy(dialog);
		}

		gtk_entry_set_text(GTK_ENTRY(entry), utf8);

                strncpy(destpath, selected_filename, MAXLEN-1);
		g_free(utf8);
        }

        gtk_widget_destroy(dialog);
}

int
message_dialog(char * title, GtkWidget * parent, GtkMessageType type, GtkButtonsType buttons,
	       GtkWidget * extra, char * text, ...) {

	GtkWidget * dialog;
	va_list args;
	gchar * msg = NULL;
	int res;


	va_start(args, text);
	msg = g_strdup_vprintf(text, args);
	va_end(args);

	dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					type,
					buttons,
					"%s",
					msg);
	g_free(msg);

	if (extra != NULL) {
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), extra, FALSE, FALSE, 5);
		gtk_widget_show_all(extra);
	}

	gtk_window_set_title(GTK_WINDOW(dialog), title);
	res = aqualung_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	return res;
}

void
insert_label_entry(GtkWidget * table, char * ltext, GtkWidget ** entry, char * etext,
		   int y1, int y2, gboolean editable) {

	GtkWidget * label;
	GtkWidget * hbox;

	label = gtk_label_new(ltext);
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, y1, y2, GTK_FILL, GTK_FILL, 5, 5);

        *entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(*entry), MAXLEN-1);
        gtk_editable_set_editable(GTK_EDITABLE (*entry), editable ? TRUE : FALSE);
	if (etext != NULL) {
		gtk_entry_set_text(GTK_ENTRY(*entry), etext);
	}
	gtk_table_attach(GTK_TABLE(table), *entry, 1, 2, y1, y2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);
}

void
insert_label_entry_button(GtkWidget * table, char * ltext, GtkWidget ** entry, char * etext,
			  GtkWidget * button, int y1, int y2) {

	GtkWidget * label;
	GtkWidget * hbox;

	label = gtk_label_new(ltext);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, y1, y2, GTK_FILL, GTK_FILL, 5, 5);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, y1, y2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	*entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(*entry), MAXLEN-1);
	if (etext != NULL) {
		gtk_entry_set_text(GTK_ENTRY(*entry), etext);
	}
	gtk_box_pack_start(GTK_BOX(hbox), *entry, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 5);
}

void
insert_label_entry_browse(GtkWidget * table, char * ltext, GtkWidget ** entry, char * etext,
			  int y1, int y2, 
			  void (* browse_cb)(GtkButton * button, gpointer data)) {

	GtkWidget * button = gui_stock_label_button(_("_Browse..."), GTK_STOCK_OPEN);
	insert_label_entry_button(table, ltext, entry, etext, button, y1, y2);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(browse_cb), *entry);
}

void
insert_label_progbar_button(GtkWidget * table, char * ltext, GtkWidget ** progbar,
			    GtkWidget * button, int y1, int y2) {

	GtkWidget * label;
	GtkWidget * hbox;

	label = gtk_label_new(ltext);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, y1, y2, GTK_FILL, GTK_FILL, 5, 5);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, y1, y2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 5);

	*progbar = gtk_progress_bar_new();
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(*progbar), 0.0f);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(*progbar), "0%");
	gtk_box_pack_start(GTK_BOX(hbox), *progbar, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 5);
}

void
insert_label_spin(GtkWidget * table, char * ltext, GtkWidget ** spin, int spinval,
		   int y1, int y2) {

	GtkWidget * label;
	GtkWidget * hbox;

	label = gtk_label_new(ltext);
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, y1, y2, GTK_FILL, GTK_FILL, 5, 5);

	*spin = gtk_spin_button_new_with_range(YEAR_MIN, YEAR_MAX, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(*spin), spinval);
        gtk_table_attach(GTK_TABLE(table), *spin, 1, 2, y1, y2,
                         GTK_EXPAND | GTK_FILL, GTK_FILL, 2, 5);
}

void
insert_label_spin_with_limits(GtkWidget * table, char * ltext, GtkWidget ** spin, double spinval,
			      double min, double max, int y1, int y2) {


	insert_label_spin(table, ltext, spin, spinval, y1, y2);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(*spin), min, max);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(*spin), spinval);
}


void
set_option_from_toggle(GtkWidget * widget, int * opt) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		*opt = 1;
	} else {
		*opt = 0;
	}
}

void
set_option_from_combo(GtkWidget * widget, int * opt) {

	*opt = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
}

void
set_option_from_spin(GtkWidget * widget, int * opt) {

	*opt = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
}

void
set_option_from_entry(GtkWidget * widget, char * opt, int n) {

	strncpy(opt, gtk_entry_get_text(GTK_ENTRY(widget)), n-1);
}

void
set_option_bit_from_toggle(GtkWidget * toggle, int * opt, int bit) {

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle))) {
		*opt |= bit;
	} else {
		*opt &= ~bit;
	}
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8:

