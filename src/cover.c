/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi
              (C) 2006 Tomasz Maka

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
#include <strings.h>
#include <dirent.h>
#include <limits.h>
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

#include "music_browser.h"
#include "store_file.h"
#include "options.h"
#include "cover.h"


extern options_t options;

extern gint cover_show_flag;

extern GtkTreeStore * music_store;

/* maximum 4 chars per extension */

gchar *cover_extensions[] = {
        "jpg", "jpeg", "png", "gif", "bmp", "tif", "tiff"
};

gint n_extensions = sizeof(cover_extensions) / sizeof(gchar*);

GtkWidget *cover_window;
gboolean ext_flag;
gchar temp_filename[PATH_MAX];
gint calculated_width, calculated_height;
gint cover_widths[N_COVER_WIDTHS] = { 50, 100, 200, 300, -1 };       /* widths in pixels */

gchar *
get_song_path(gchar *song_filename) {

        static gchar song_path[PATH_MAX];
        gint i = 0;
        gchar *pos;
        
        if ((pos = strrchr(song_filename, '/'))) {
                for (i=0; song_filename <= pos; i++) {
                        song_path[i] = *song_filename++;
                }
        }
        song_path[i] = '\0';

        return song_path;
}


static gint
entry_filter(const struct dirent *entry) {

        gint i, len;
        gchar *ext, *str1, *str2;

        len = strlen (entry->d_name);

        if (len < 5)                    /* 5 => a.abc */
                return FALSE;

        if ((ext = strrchr(entry->d_name, '.'))) {
 
                ext++; 

                /* filter candidates for cover */

                str1 = g_utf8_casefold(ext, -1);

                for (i = 0; i < n_extensions; i++) {

                        str2 = g_utf8_casefold(cover_extensions[i], -1);
                        
                        if (!g_utf8_collate(str1, str2)) {
                                g_free(str1);
                                g_free(str2);
                                ext_flag = TRUE;
                                arr_strlcpy(temp_filename, entry->d_name);
                                return TRUE;
                        }
                
                        g_free (str2);
                }

                g_free (str1);
        }

	return FALSE;
}

gchar *
find_cover_filename(gchar *song_filename) {

        gchar *cover_filenames[] = {
                "cover", ".cover", 
                "folder", ".folder",
                "front", ".front"
        };

        gint n_templates, n_file_hits, i, j, n, m;
        gchar base_path[PATH_MAX], current_filename[PATH_MAX];
        static gchar cover_filename[PATH_MAX];
        static gint cover_filename_reasonable;
	struct dirent ** d_entry;
        gchar *str1, *str2;


        n_templates = sizeof(cover_filenames) / sizeof(gchar*);
        arr_strlcpy(base_path, get_song_path(song_filename));

        if (strcmp(base_path, get_song_path(cover_filename))) {

                cover_filename[0] = temp_filename[0] = '\0';
                cover_filename_reasonable = FALSE;

                ext_flag = FALSE;
                n_file_hits = scandir(base_path, &d_entry, entry_filter, alphasort);

                for (i = 0; i < n_templates; i++) {

                        for (j = 0; j < n_extensions; j++) {

                                arr_strlcpy(current_filename, cover_filenames[i]);
                                arr_strlcat(current_filename, ".");
                                arr_strlcat(current_filename, cover_extensions[j]);

                                str1 = g_utf8_casefold (current_filename, -1);

                                for (n = 0; n < n_file_hits; n++) {

                                        str2 = g_utf8_casefold(d_entry[n]->d_name, -1);

                                        if (!g_utf8_collate(str1, str2)) {

                                                arr_strlcpy(cover_filename, base_path);
                                                arr_strlcat(cover_filename, d_entry[n]->d_name);

                                                if (g_file_test (cover_filename, G_FILE_TEST_IS_REGULAR) == TRUE) {
                                                        g_free (str1);
                                                        g_free (str2);


                                                        for (m = 0; m < n_file_hits; m++) {
                                                                free(d_entry[m]);
                                                        }
                                                        free(d_entry);

                                                        cover_filename_reasonable = TRUE;

                                                        return cover_filename;
                                                }
                                        }

                                        g_free (str2);
                                }

                                g_free (str1);
                        }
                }

                if (n_file_hits > 0) {
                        for (m = 0; m < n_file_hits; m++) {
                                free(d_entry[m]);
                        }
                        free(d_entry);
                }

                arr_strlcpy(cover_filename, base_path);
 
                if (ext_flag == TRUE) {
                        arr_strlcat(cover_filename, temp_filename);
                        cover_filename_reasonable = TRUE;
                }

        }

        if (cover_filename_reasonable == TRUE) {
                return cover_filename;
        }

        return NULL;
}


void
draw_cover_frame(GdkPixbuf *pixbuf, gint width, gint height, gboolean bevel) {

        gint rowstride, channels;
        gint i, bc1, bc2, bc3, bc4;
        guchar *pixels, *p;
        gchar *c = NULL;

        bc1 = bc2 = bc3 = bc4 = 64;      /* dark edges */

        if (bevel == TRUE) {
                bc2 = bc4 = 160;        /* light edges */
        }

        /* set high contrast bevel if "plain" or "no_skin" is the current skin */

        if ((c = strrchr(options.skin, '/')) != NULL) {
                ++c;
                
                if (strcasecmp(c, "plain") == 0 || strcasecmp(c, "no_skin") == 0) {

                        bc1 = bc2 = bc3 = bc4 = 0;      /* dark edges */

                        if (bevel == TRUE) {
                                bc2 = bc4 = 255;        /* light edges */
                        }
                }
        }

        /* draw frame */

        channels = gdk_pixbuf_get_n_channels (pixbuf);
        rowstride = gdk_pixbuf_get_rowstride (pixbuf);
        pixels = gdk_pixbuf_get_pixels (pixbuf);

        /* horizontal lines */
        for(i=0; i < width; i++) {
                p = pixels + i * channels;
                p[0] = p[1] = p[2] = bc1;
                p = pixels + (height-1) * rowstride + i * channels;
                p[0] = p[1] = p[2] = bc2;
        }

        /* vertical lines */
        for(i=0; i < height; i++) {
                p = pixels + i * rowstride;
                p[0] = p[1] = p[2] = bc3;
                p = pixels + i * rowstride + (width-1) * channels;
                p[0] = p[1] = p[2] = bc4;
        }

}

gboolean
cover_window_close_cb(GtkWidget * widget, GdkEvent * event, gpointer data) {

        gtk_widget_destroy(widget);
	return TRUE;
}

gint
cover_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

	if (kevent->keyval == GDK_Escape) {
                cover_window_close_cb(widget, NULL, NULL);
		return TRUE;
        }
	return FALSE;
}


void
create_zoomed_cover_window(gint * size, GtkWidget * window, GtkWidget ** image_area) {

	*size = cover_widths[options.cover_width];
	if (*size == -1) {
		*size = cover_widths[options.cover_width-1];
	}
	
	cover_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(cover_window), "button_press_event",
			 G_CALLBACK(cover_window_close_cb), NULL);
	g_signal_connect(G_OBJECT(cover_window), "key_press_event",
			 G_CALLBACK(cover_window_key_pressed), NULL);
	gtk_window_set_position(GTK_WINDOW(cover_window), GTK_WIN_POS_MOUSE);
	gtk_widget_set_events(cover_window, GDK_BUTTON_PRESS_MASK);
	gtk_window_set_transient_for(GTK_WINDOW(cover_window), GTK_WINDOW(window));
	gtk_window_set_decorated(GTK_WINDOW(cover_window), FALSE);
	
	*image_area = gtk_image_new();
	gtk_widget_show(*image_area);
	gtk_container_add (GTK_CONTAINER (cover_window), *image_area);
}


void 
display_zoomed_cover(GtkWidget *window, GtkWidget *event_area, gchar *song_filename) {

        GtkWidget * image_area;
        gint size;

        if (g_file_test(song_filename, G_FILE_TEST_IS_REGULAR) != TRUE) {
		return;
	}

	create_zoomed_cover_window(&size, window, &image_area);
	display_cover(image_area, event_area, NULL, size, size, song_filename, FALSE, FALSE);
	gtk_widget_set_size_request(cover_window, calculated_width, calculated_height);
	gtk_widget_show(cover_window);
}


void 
display_zoomed_cover_from_binary(GtkWidget *window, GtkWidget *event_area, void * data, int length) {

        GtkWidget * image_area;
        gint size;

        if (data == NULL) {
		return;
	}

	create_zoomed_cover_window(&size, window, &image_area);
	display_cover_from_binary(image_area, event_area, NULL, size, size, data, length, FALSE, FALSE);
	gtk_widget_set_size_request(cover_window, calculated_width, calculated_height);
	gtk_widget_show(cover_window);
}


void 
display_cover_from_pixbuf(GtkWidget *image_area, GtkWidget *event_area, GtkWidget *align,
			  gint dest_width, gint dest_height,
			  GdkPixbuf * cover_pixbuf, gboolean hide, gboolean bevel) {

        GdkPixbuf * cover_pixbuf_scaled;
	gint width = gdk_pixbuf_get_width(cover_pixbuf);
	gint height = gdk_pixbuf_get_height(cover_pixbuf);
        gint scaled_width, scaled_height;

	if (cover_pixbuf == NULL) {
		return;
	}

        calculated_width = dest_width;
        calculated_height = dest_height;

	/* don't scale when orginal size is smaller than cover defaults */
	
	scaled_width =  dest_width;
	scaled_height = dest_height;
	
	if (width >= height) {
		scaled_height = (height * dest_height) / width;
	} else {
		scaled_width = (width * dest_width) / height;
	}
	
	cover_pixbuf_scaled = gdk_pixbuf_scale_simple(cover_pixbuf,
						      scaled_width, scaled_height,
						      GDK_INTERP_TILES);
	if (cover_pixbuf_scaled == NULL) {
		return;
	}
	
	draw_cover_frame(cover_pixbuf_scaled, scaled_width, scaled_height, bevel);
	
	calculated_width = scaled_width;
	calculated_height = scaled_height;
	
	gtk_image_set_from_pixbuf(GTK_IMAGE(image_area), cover_pixbuf_scaled);
	g_object_unref(cover_pixbuf_scaled);
	
	if (!cover_show_flag && hide == TRUE) {
		cover_show_flag = 1;      
		gtk_widget_show(image_area);
		gtk_widget_show(event_area);
		if (align) {
			gtk_widget_show(align);
		}
	}

	/* don't unref cover_pixbuf that was passed in */
}

void
hide_cover(GtkWidget *image_area, GtkWidget *event_area, GtkWidget *align) {

	cover_show_flag = 0;
	gtk_widget_hide(image_area);
	gtk_widget_hide(event_area);
	if (align) {
		gtk_widget_hide(align);
	}
}

void 
display_cover(GtkWidget *image_area, GtkWidget *event_area, GtkWidget *align,
	      gint dest_width, gint dest_height,
              gchar *song_filename, gboolean hide, gboolean bevel) {

        GdkPixbuf * cover_pixbuf = NULL;
        gchar cover_filename[PATH_MAX];
	char * filename = NULL;

        if (strlen(song_filename) == 0) {
		if (hide == TRUE) {
			hide_cover(image_area, event_area, align);
		}
		return;
	}

	if ((filename = find_cover_filename(song_filename)) == NULL) {
		if (hide == TRUE) {
			hide_cover(image_area, event_area, align);
		}
		return;
	}

	arr_strlcpy(cover_filename, filename);

	if ((cover_pixbuf = gdk_pixbuf_new_from_file(cover_filename, NULL)) != NULL) {
		display_cover_from_pixbuf(image_area, event_area, align,
					  dest_width, dest_height,
					  cover_pixbuf, hide, bevel);
		g_object_unref(cover_pixbuf);
	} else if (hide == TRUE) {
		hide_cover(image_area, event_area, align);
	}
}


void 
display_cover_from_binary(GtkWidget *image_area, GtkWidget *event_area, GtkWidget *align,
			  gint dest_width, gint dest_height,
			  void * data, int length, gboolean hide, gboolean bevel) {

        GdkPixbuf * cover_pixbuf = NULL;
	GdkPixbufLoader * loader;

        if (data == NULL) {
		return;
	}

	loader = gdk_pixbuf_loader_new();
	if (gdk_pixbuf_loader_write(loader, data, length, NULL) != TRUE) {
		fprintf(stderr, "display_cover_from_binary: failed to load image #1\n");
		g_object_unref(loader);
		return;
	}

	if (gdk_pixbuf_loader_close(loader, NULL) != TRUE) {
		fprintf(stderr, "display_cover_from_binary: failed to load image #2\n");
		g_object_unref(loader);
		return;
	}

	if ((cover_pixbuf = gdk_pixbuf_loader_get_pixbuf(loader)) != NULL) {
		display_cover_from_pixbuf(image_area, event_area, align,
					  dest_width, dest_height,
					  cover_pixbuf, hide, bevel);
		/* cover_pixbuf is owned by loader, so don't unref that manually */
		g_object_unref(loader);
	} else if (hide == TRUE) {
		hide_cover(image_area, event_area, align);
        }
}


void
insert_cover(GtkTreeIter * tree_iter, GtkTextIter * text_iter, GtkTextBuffer * buffer) {

        GtkTreePath * path;
        gchar cover_filename[PATH_MAX];
        gint depth;

	track_data_t * data;
	GdkPixbuf * pixbuf;
	gint k;
	gint d_cover_width, d_cover_height;
	char * filename = NULL;

	gint width, height;
	gint scaled_width, scaled_height;
	GdkPixbuf * scaled;


        /* get cover path */

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), tree_iter);
	depth = gtk_tree_path_get_depth(path);
	gtk_tree_path_free(path);

	/* check if we at 3rd level (album) or 4th level (track) */

	if (depth < 3) {
		return;
	}

	if (depth == 3) { /* album selected */
		GtkTreeIter iter;
		if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter, tree_iter, 0)) {
			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &data, -1);
		} else {
			return;
		}
	} else { /* track selected */
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), tree_iter, MS_COL_DATA, &data, -1);
	}

	if ((filename = find_cover_filename(data->file)) == NULL) {
		return;
	}

	arr_strlcpy(cover_filename, filename);

	/* load and display cover */

	k = cover_widths[options.cover_width % N_COVER_WIDTHS];

	if (k == -1) {
		d_cover_width = d_cover_height = options.browser_size_x - SCROLLBAR_WIDTH;      
	} else {
		d_cover_width = d_cover_height = k;
	}

	if ((pixbuf = gdk_pixbuf_new_from_file (cover_filename, NULL)) == NULL) {
		return;
	}

	gdk_pixbuf_get_file_info(cover_filename, &width, &height);

	/* don't scale when orginal size is smaller than cover defaults */

	scaled_width =  d_cover_width;
	scaled_height = d_cover_height;

	if (width > d_cover_width || height > d_cover_height) {

		if (width >= height) {
			scaled_height = (height * d_cover_width) / width;
		} else {
			scaled_width = (width * d_cover_height) / height;
		}

		scaled = gdk_pixbuf_scale_simple (pixbuf, scaled_width, scaled_height, GDK_INTERP_TILES);
		g_object_unref (pixbuf);
		pixbuf = scaled;

	} else {
		if (options.magnify_smaller_images) {
			scaled_height = (height * d_cover_width) / width;
			scaled = gdk_pixbuf_scale_simple (pixbuf, scaled_width, scaled_height, GDK_INTERP_TILES);
			g_object_unref (pixbuf);
			pixbuf = scaled;
		} else {
			scaled_width = width;
			scaled_height = height;
		}                               
	}

	draw_cover_frame(pixbuf, scaled_width, scaled_height, FALSE);

	/* insert picture */

	gtk_text_buffer_insert_pixbuf (buffer, text_iter, pixbuf);
	gtk_text_buffer_insert (buffer, text_iter, "\n\n", -1);

	g_object_unref (pixbuf);
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

