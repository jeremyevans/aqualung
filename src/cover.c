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

#include "common.h"
#include "cover.h"
#include "music_browser.h"
#include "i18n.h"
#include "options.h"

extern options_t options;

extern gint cover_show_flag;

extern gint browser_size_x;
extern GtkWidget * comment_view;
extern GtkTreeSelection * music_select;

/* maximum 4 chars per extension */

gchar *cover_extensions[] = {
        "jpg", "jpeg", "png", "gif", "bmp", "tif", "tiff"
};

gint n_extensions = sizeof(cover_extensions) / sizeof(gchar*);


gchar *
get_song_path(gchar *song_filename) {

        static gchar song_path[PATH_MAX];
        gint i, k;


        k = strlen(song_filename) - 1;
        while (song_filename[k--] != '/');

        for (i = 0; k != -2; i++, k--)
                song_path[i] = song_filename[i];

        song_path[i] = '\0';

        return song_path;
}


static gint
entry_filter(const struct dirent *entry) {

        gint i, j, len;
        gchar ext[5], c, *str1, *str2;
   

        len = strlen (entry->d_name);

        if (len < 5)                    /* 5 => a.abc */
                return FALSE;

        /* extract extension */

        i = j = 0;

        while(i < 4) {
                c = entry->d_name[len - 4 + i];
                if (c != '.') {
                        ext[j++] = c;
                }
                i++;
        }

        ext[j] = 0;

        /* filter candidates for cover */

        str1 = g_utf8_casefold(ext, -1);

        for (i = 0; i < n_extensions; i++) {

                str2 = g_utf8_casefold(cover_extensions[i], -1);
                
                if (!g_utf8_collate(str1, str2)) {
                        g_free(str1);
                        g_free(str2);
                        return TRUE;
                }
        
                g_free (str2);
        }

        g_free (str1);

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

        for (i = 0; i < n_files; i++) {

                for (j = 0; j < n_extensions; j++) {

                        strcpy (current_filename, cover_filenames[i]);
                        strcat (current_filename, ".");
                        strcat (current_filename, cover_extensions[j]);

                        str1 = g_utf8_casefold (current_filename, -1);

	                for (n = 0; n < scandir(base_path, &d_entry, entry_filter, alphasort); n++) {

                                str2 = g_utf8_casefold(d_entry[n]->d_name, -1);

                                if (!g_utf8_collate(str1, str2)) {

                                        strcpy (cover_filename, base_path);
                                        strcat (cover_filename, d_entry[n]->d_name);

                                        if (g_file_test (cover_filename, G_FILE_TEST_IS_REGULAR) == TRUE) {
                                                g_free (str1);
                                                g_free (str2);
                                                return cover_filename;
                                        }
                                }

                                g_free (str2);
                        }

                        g_free (str1);
                }
        }

        return cover_filename;
}


void
draw_cover_frame(GdkPixbuf *pixbuf, gint width, gint height, gboolean bevel) {

        gint rowstride, channels;
        gint i, bc1, bc2, bc3, bc4;
        guchar *pixels, *p;

        bc1 = bc2 = bc3 = bc4 = 0;      /* black edges */

        if (bevel == TRUE) {
                bc2 = bc4 = 255;        /* white edges */
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

void 
display_cover(GtkWidget *image_area, gint dest_width, gint dest_height, gchar *song_filename, gboolean hide) {

        GdkPixbuf * cover_pixbuf;
        GdkPixbuf * cover_pixbuf_scaled;
        GdkPixbufFormat * format;
        gint width, height;
        gint scaled_width, scaled_height;
        gchar cover_filename[PATH_MAX];


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
                        cover_pixbuf = cover_pixbuf_scaled;

                        draw_cover_frame(cover_pixbuf, scaled_width, scaled_height, TRUE);

                        gtk_image_set_from_pixbuf (GTK_IMAGE(image_area), cover_pixbuf);

                        if (!cover_show_flag && hide == TRUE) {
                                cover_show_flag = 1;      
                                gtk_widget_show(image_area);
                        }

                } else {
 
                        if (hide == TRUE) {
                                cover_show_flag = 0;      
                                gtk_widget_hide(image_area);
                        }

                }

        }

}

void
insert_cover(GtkTextIter * iter) {

        GtkTreeModel * model;
        GtkTreeIter r_iter, t_iter;
        GtkTreePath * path;
        GtkTextBuffer * buffer;
        GdkPixbuf *pixbuf;
        GdkPixbuf *scaled;
        GdkPixbufFormat *format;
        gchar *song_filename, cover_filename[PATH_MAX];
        gint k, i;
        gint width, height;
        gint d_cover_width, d_cover_height;
        gint scaled_width, scaled_height;
        gint cover_widths[5] = { 50, 100, 200, 300, -1 };       /* widths in pixels */

        buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));

        /* get cover path */
        
        if (gtk_tree_selection_get_selected(music_select, &model, &r_iter)) {

                path = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &r_iter);
                i = gtk_tree_path_get_depth(path);

                /* check if we at 3rd level (album) or 4th level (track) */

                if (i == 3 || i == 4) {

                        if (i == 3) { /* album selected */

                                if (gtk_tree_model_iter_nth_child(model, &t_iter, &r_iter, 0)) {
                                        gtk_tree_model_get(GTK_TREE_MODEL(model), &t_iter, 2, &song_filename, -1);
                                } else {
                                        return;
                                }

                        } else { /* track selected */
 
                                gtk_tree_model_get(GTK_TREE_MODEL(model), &r_iter, 2, &song_filename, -1);
 
                        }

                        strcpy(cover_filename, find_cover_filename(song_filename));

                        /* load and display cover */

                        k = cover_widths[options.cover_width % 5];

                        if (k == -1) {

                                d_cover_width = d_cover_height = browser_size_x - SCROLLBAR_WIDTH;      

                        } else {

                                d_cover_width = d_cover_height = k;
                        }

                        pixbuf = gdk_pixbuf_new_from_file (cover_filename, NULL);

                        if (pixbuf != NULL) {

                                format = gdk_pixbuf_get_file_info(cover_filename, &width, &height);

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


                                draw_cover_frame(pixbuf, scaled_width, scaled_height, FALSE);

                                /* insert picture */

                                gtk_text_buffer_insert_pixbuf (buffer, iter, pixbuf);
                                gtk_text_buffer_insert (buffer, iter, "\n\n", -1);

                                g_object_unref (pixbuf);
                        }
                }

                gtk_tree_path_free(path);
        }
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

