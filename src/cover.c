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
#include <dirent.h>
#include <gtk/gtk.h>
#include <gdk/gdkcursor.h>
#include <gdk/gdkkeysyms.h>

#include "common.h"
#include "cover.h"
#include "gui_main.h"
#include "music_browser.h"
#include "store_file.h"
#include "i18n.h"
#include "options.h"

extern options_t options;

extern gint cover_show_flag;

extern GtkWidget * main_window;
extern GtkTreeStore * music_store;

/* maximum 4 chars per extension */

gchar *cover_extensions[] = {
        "jpg", "jpeg", "png", "gif", "bmp", "tif", "tiff"
};

gint n_extensions = sizeof(cover_extensions) / sizeof(gchar*);

GtkWidget *cover_window;
gchar temp_filename[PATH_MAX];
gint ext_flag;
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
                                strncpy(temp_filename, entry->d_name, PATH_MAX-1);
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

        gint n_files, i, j, n;
        gchar base_path[PATH_MAX], current_filename[PATH_MAX];
        static gchar cover_filename[PATH_MAX];
	struct dirent ** d_entry;
        gchar *str1, *str2;

        n_files = sizeof(cover_filenames) / sizeof(gchar*);
        strcpy(base_path, get_song_path(song_filename));

        cover_filename[0] = temp_filename[0] = '\0';

        for (i = 0; i < n_files; i++) {

                for (j = 0; j < n_extensions; j++) {

			int n_files;

                        strcpy (current_filename, cover_filenames[i]);
                        strcat (current_filename, ".");
                        strcat (current_filename, cover_extensions[j]);

                        ext_flag = FALSE;
                        str1 = g_utf8_casefold (current_filename, -1);

			n_files = scandir(base_path, &d_entry, entry_filter, alphasort);
	                for (n = 0; n < n_files; n++) {

                                str2 = g_utf8_casefold(d_entry[n]->d_name, -1);

                                if (!g_utf8_collate(str1, str2)) {

                                        strcpy (cover_filename, base_path);
                                        strcat (cover_filename, d_entry[n]->d_name);

                                        if (g_file_test (cover_filename, G_FILE_TEST_IS_REGULAR) == TRUE) {
                                                g_free (str1);
                                                g_free (str2);

						while (n < n_files) {
							free(d_entry[n]);
							++n;
						}
						
						free(d_entry);

                                                return cover_filename;
                                        }
                                }

                                g_free (str2);
				free(d_entry[n]);
                        }

                        g_free (str1);
			if (n_files > 0) {
				free(d_entry);
			}
                }
        }

        if (ext_flag == TRUE) {
                strcpy (cover_filename, base_path);
                strcat (cover_filename, temp_filename);
        }

        return cover_filename;
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
display_zoomed_cover(GtkWidget *window, GtkWidget *event_area, gchar *song_filename) {

        GtkWidget * image_area;
        gint size;

        if (g_file_test (song_filename, G_FILE_TEST_IS_REGULAR) == TRUE) {

                size = cover_widths[options.cover_width];
                if (size == -1) {
                        size = cover_widths[options.cover_width-1];
                }

		cover_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
                g_signal_connect(G_OBJECT(cover_window), "button_press_event",
                                 G_CALLBACK(cover_window_close_cb), NULL);
                g_signal_connect(G_OBJECT(cover_window), "key_press_event",
                                 G_CALLBACK(cover_window_key_pressed), NULL);
                gtk_window_set_position(GTK_WINDOW(cover_window), GTK_WIN_POS_MOUSE);
	        gtk_widget_set_events(cover_window, GDK_BUTTON_PRESS_MASK);
               	gtk_window_set_modal(GTK_WINDOW(cover_window), TRUE);
        	gtk_window_set_transient_for(GTK_WINDOW(cover_window), GTK_WINDOW(window));
                gtk_window_set_decorated(GTK_WINDOW(cover_window), FALSE);

                image_area = gtk_image_new();
                gtk_widget_show(image_area);
                gtk_container_add (GTK_CONTAINER (cover_window), image_area);

                display_cover(image_area, event_area, NULL, size, size, song_filename, FALSE, FALSE);

                gtk_widget_set_size_request(cover_window, calculated_width, calculated_height);
                gtk_widget_show(cover_window);

        }
}


void 
display_cover(GtkWidget *image_area, GtkWidget *event_area, GtkWidget *align,
	      gint dest_width, gint dest_height,
              gchar *song_filename, gboolean hide, gboolean bevel) {

        GdkPixbuf * cover_pixbuf = NULL;
        GdkPixbuf * cover_pixbuf_scaled;
        GdkPixbufFormat * format;
        gint width, height;
        gint scaled_width, scaled_height;
        gchar cover_filename[PATH_MAX];

        calculated_width = dest_width;
        calculated_height = dest_height;


        if (strlen(song_filename)) {

                strcpy(cover_filename, find_cover_filename(song_filename));

                cover_pixbuf = gdk_pixbuf_new_from_file (cover_filename, NULL);

                if (cover_pixbuf != NULL) {

                        format = gdk_pixbuf_get_file_info(cover_filename, &width, &height);

                        /* don't scale when orginal size is smaller than cover defaults */

                        scaled_width =  dest_width;
                        scaled_height = dest_height;

                        if (width >= height) {

                                scaled_height = (height * (dest_height)) / width;

                        } else {

                                scaled_width = (width * (dest_width)) / height;
                        }

                        cover_pixbuf_scaled = gdk_pixbuf_scale_simple (cover_pixbuf, 
                                                                       scaled_width, scaled_height, 
                                                                       GDK_INTERP_TILES);
                        g_object_unref (cover_pixbuf);

                        if (cover_pixbuf_scaled != NULL) {
                                cover_pixbuf = cover_pixbuf_scaled;

                                draw_cover_frame(cover_pixbuf, scaled_width, scaled_height, bevel);

                                calculated_width = scaled_width;
                                calculated_height = scaled_height;

                                gtk_image_set_from_pixbuf (GTK_IMAGE(image_area), cover_pixbuf);
                                g_object_unref (cover_pixbuf);

                                if (!cover_show_flag && hide == TRUE) {
                                        cover_show_flag = 1;      
                                        gtk_widget_show(image_area);
                                        gtk_widget_show(event_area);
                                        if (align) {
                                                gtk_widget_show(align);
                                        }
                                }
                        }
                } else {
 
                        if (hide == TRUE) {
                                cover_show_flag = 0;      
                                gtk_widget_hide(image_area);
                                gtk_widget_hide(event_area);
				if (align) {
					gtk_widget_hide(align);
				}
                        }

                }

        }

}

void
insert_cover(GtkTreeIter * tree_iter, GtkTextIter * text_iter, GtkTextBuffer * buffer) {

        GtkTreePath * path;
        gchar cover_filename[PATH_MAX];
        gint depth;

        /* get cover path */
        
	path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), tree_iter);
	depth = gtk_tree_path_get_depth(path);

	/* check if we at 3rd level (album) or 4th level (track) */

	if (depth == 3 || depth == 4) {

		track_data_t * data;
		GdkPixbuf * pixbuf;
		gint k;
		gint d_cover_width, d_cover_height;

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

		strcpy(cover_filename, find_cover_filename(data->file));

		/* load and display cover */

		k = cover_widths[options.cover_width % N_COVER_WIDTHS];

		if (k == -1) {
			d_cover_width = d_cover_height = options.browser_size_x - SCROLLBAR_WIDTH;      
		} else {
			d_cover_width = d_cover_height = k;
		}

		if ((pixbuf = gdk_pixbuf_new_from_file (cover_filename, NULL)) != NULL) {

			gint width, height;
			gint scaled_width, scaled_height;
			GdkPixbuf * scaled;

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

				scaled = gdk_pixbuf_scale_simple (pixbuf, 
								  scaled_width, scaled_height, 
								  GDK_INTERP_TILES);
				g_object_unref (pixbuf);
				pixbuf = scaled;

			} else {
				if (options.magnify_smaller_images) {

					scaled_height = (height * d_cover_width) / width;

					scaled = gdk_pixbuf_scale_simple (pixbuf, 
									  scaled_width, scaled_height, 
									  GDK_INTERP_TILES);
					g_object_unref (pixbuf);
					pixbuf = scaled;
				} else {
					scaled_width = width;
					scaled_height = height;
				}                               
			}

                        if (pixbuf != NULL) {
                                draw_cover_frame(pixbuf, scaled_width, scaled_height, FALSE);

                                /* insert picture */

                                gtk_text_buffer_insert_pixbuf (buffer, text_iter, pixbuf);
                                gtk_text_buffer_insert (buffer, text_iter, "\n\n", -1);

                                g_object_unref (pixbuf);
                        }
		}

                gtk_tree_path_free(path);
        }
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

