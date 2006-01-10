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

*/


#include <config.h>

#ifdef HAVE_IFP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <pthread.h>
#include <limits.h>
#include <usb.h>
#include <ifp.h>

#include "common.h"
#include "i18n.h"
#include "options.h"

#define MBYTES  1048576         /* bytes per MB */
#define ROOTDIR _("<root>")

extern options_t options;
extern GtkListStore * play_store;
extern GtkTreeSelection * play_select;
extern GtkWidget * playlist_window;
extern GtkWidget * main_window;
extern GtkTooltips * aqualung_tooltips;

extern void set_sliders_width(void);
void deflicker(void);
gint aifp_directory_listing(void);
extern GtkWidget* gui_stock_label_button(gchar *blabel, const gchar *bstock);

struct usb_device *dev = NULL;
usb_dev_handle *dh;
struct ifp_device ifpdev;

gchar remote_directory[MAXLEN];
gchar temp[MAXLEN];
gchar dest_file[MAXLEN];

guint songs_size, number_of_songs;
gint battery_status, capacity, freespace, abort_pressed;

GtkWidget * aifp_window;
GtkWidget * upload_button;
GtkWidget * abort_button;
GtkWidget * close_button;   
GtkWidget * mkdir_button;
GtkWidget * rndir_button;
GtkWidget * rmdir_button; 
GtkWidget * label_songs;
GtkWidget * label_songs_size;
GtkWidget * label_model;
GtkWidget * label_capacity;
GtkWidget * label_freespace;
GtkWidget * progressbar_battery;
GtkWidget * progressbar_freespace;
GtkWidget * progressbar_cf;
GtkWidget * progressbar_op;
GtkWidget * list;
GtkListStore * list_store = NULL;

void aifp_update_info(void);

/* upload songs handler */

void
abort_transfer_cb (GtkButton *button, gpointer user_data) {

        abort_pressed = 1;
}

int
update_progress (void *context, struct ifp_transfer_status *status) {

        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_cf), (double)status->file_bytes/status->file_total);
        sprintf(temp, _("%d / %d bytes"), status->file_bytes, status->file_total);
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_cf), temp);
        deflicker();

        return 0;

}

void
upload_songs_cb (GtkButton *button, gpointer user_data) {

	GtkTreeIter iter;
        gint i, j, k, n;
        gchar *str;
        gchar dest_dir[MAXLEN], file[MAXLEN];

        gtk_widget_set_sensitive(abort_button, TRUE);
        gtk_widget_set_sensitive(upload_button, FALSE);
        gtk_widget_set_sensitive(close_button, FALSE);
        gtk_widget_set_sensitive(mkdir_button, FALSE);
        gtk_widget_set_sensitive(rndir_button, FALSE);
        gtk_widget_set_sensitive(rmdir_button, FALSE);
        gtk_widget_set_sensitive(list, FALSE);
        deflicker();

        strncpy(dest_dir, "\\", MAXLEN-1);

        if(strcmp(remote_directory, ROOTDIR))
                strncat(dest_dir, remote_directory, MAXLEN-1);

        i = n = 0;

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

                if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {

			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 1, &str, -1);

                        j = strlen(str);
                        k = 0;

                        while(str[--j]!='/');
                        while(str[++j])
                            file[k++] = str[j];
                        file[k] = '\0';

                        strncpy(dest_file, dest_dir, MAXLEN-1);
                        if(strcmp(remote_directory, ROOTDIR))
                                strncat(dest_file, "\\", MAXLEN-1);
                        strncat(dest_file, file, MAXLEN-1);

                        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_op), (double)(n+1)/number_of_songs);
                        sprintf(temp, _("%d / %d files"), n+1, number_of_songs);
                        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_op), temp);
                        deflicker();

                        k = ifp_upload_file(&ifpdev, str, dest_file, update_progress, NULL);

                        aifp_update_info();
                        deflicker();

                        n++;
                }

                if(abort_pressed) break;
	}

        gtk_widget_set_sensitive(abort_button, FALSE);
        gtk_widget_set_sensitive(close_button, TRUE);
        gtk_widget_set_sensitive(list, TRUE);

        if(!abort_pressed) {
                gtk_widget_set_sensitive(upload_button, FALSE);
                gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_op), _("Done."));
        } else {
                gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_op), _("Aborted..."));
                gtk_widget_set_sensitive(upload_button, TRUE);
        }

        aifp_update_info();
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_cf), _("Done."));
        deflicker();

        abort_pressed = 0;
}

/* directory operation handlers */

void
create_directory_cb (GtkButton *button, gpointer user_data) {

        GtkWidget *info_dialog;
        GtkWidget *name_entry;
        gint response, k;

        info_dialog = gtk_message_dialog_new (GTK_WINDOW(aifp_window),
                                              GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, _("Please enter directory name"));

        gtk_window_set_title(GTK_WINDOW(info_dialog), _("Create directory"));

        name_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(info_dialog)->vbox), name_entry, FALSE, FALSE, 6);
        gtk_entry_set_max_length(GTK_ENTRY(name_entry), 64);
        gtk_widget_set_size_request(GTK_WIDGET(name_entry), 300, -1);

        gtk_widget_show_all (info_dialog);

        response = gtk_dialog_run (GTK_DIALOG (info_dialog));     


        if (response == GTK_RESPONSE_OK) {   

                strncpy(temp, gtk_entry_get_text(GTK_ENTRY(name_entry)), MAXLEN-1);

                if(strlen(temp)) {

                        k = ifp_mkdir(&ifpdev, temp);

                        aifp_update_info();
                        aifp_directory_listing();
                }
        }

        gtk_widget_destroy(info_dialog);
}

void
rename_directory_cb (GtkButton *button, gpointer user_data) {

        GtkWidget *info_dialog;
        GtkWidget *name_entry;
        gint response, k;
        const gchar * text;

        if(strcmp(remote_directory, ROOTDIR)) {

                info_dialog = gtk_message_dialog_new (GTK_WINDOW(aifp_window),
                                                      GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                      GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, _("Please enter a new name"));

                gtk_window_set_title(GTK_WINDOW(info_dialog), _("Rename directory"));

                name_entry = gtk_entry_new();
                gtk_box_pack_start(GTK_BOX(GTK_DIALOG(info_dialog)->vbox), name_entry, FALSE, FALSE, 6);
                gtk_entry_set_max_length(GTK_ENTRY(name_entry), 64);
                gtk_widget_set_size_request(GTK_WIDGET(name_entry), 300, -1);

                gtk_entry_set_text(GTK_ENTRY(name_entry), remote_directory);

                gtk_widget_show_all (info_dialog);

                response = gtk_dialog_run (GTK_DIALOG (info_dialog));     

                if (response == GTK_RESPONSE_OK) {   

                        text = gtk_entry_get_text(GTK_ENTRY(name_entry));

                        strncpy(temp, "\\", MAXLEN-1);
                        strncat(temp, text, MAXLEN-1);

                        if(strlen(text)) {

                                strncpy(dest_file, "\\", MAXLEN-1);
                                strncat(dest_file, remote_directory, MAXLEN-1);

                                k = ifp_rename(&ifpdev, dest_file, temp);

                                aifp_update_info();
                                aifp_directory_listing();
                        }
                }

                gtk_widget_destroy(info_dialog);
        }
}

void
remove_directory_cb (GtkButton *button, gpointer user_data) {

        GtkWidget *info_dialog;
        gint response, k;

        if(strcmp(remote_directory, ROOTDIR)) {

                sprintf(temp, _("Directory '%s' will be removed with its entire contents.\n\nAre you sure ?"), remote_directory);

                info_dialog = gtk_message_dialog_new (GTK_WINDOW(aifp_window),
                                                      GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, temp);

                gtk_window_set_title(GTK_WINDOW(info_dialog), _("Remove directory"));
                gtk_widget_show (info_dialog);

                response = gtk_dialog_run (GTK_DIALOG (info_dialog));     

                gtk_widget_destroy(info_dialog);

                if (response == GTK_RESPONSE_YES) {   

                        strncpy(temp, "\\", MAXLEN-1);
                        strncat(temp, remote_directory, MAXLEN-1);

                        k = ifp_delete_dir_recursive(&ifpdev, temp);

                        aifp_update_info();
                        aifp_directory_listing();
                }

                set_sliders_width();    /* MAGIC */
                
        }
}


/* "directory selected" handler */
void 
directory_selected_cb (GtkTreeSelection *selection, gpointer data) {

        GtkTreeIter iter;
        GtkTreeModel *model;  
        gchar *text;
        
        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {

                gtk_tree_model_get (model, &iter, 0, &text, -1);  
                strncpy(remote_directory, text, MAXLEN-1);

                g_free(text);

        }

}

/* display root directory of iFP device */

gint 
aifp_dump_dir(void *context, int type, const char *name, int filesize) {

        GtkTreeIter iter;
        gint i;

        i = 1;  /* 0 is for ROOTDIR */

        if(type == IFP_DIR) {

                gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store), &iter, NULL, i++);
                gtk_list_store_append(list_store, &iter);
	        gtk_list_store_set(list_store, &iter, 0, name, -1);

        }

        return 0;
}

gint
aifp_directory_listing(void) {

        GtkTreeIter iter;
        GtkTreePath *path;

        gtk_list_store_clear(list_store);

        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store), &iter, NULL, 0);
	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set(list_store, &iter, 0, ROOTDIR, -1);

        if(ifp_list_dirs(&ifpdev, "\\", aifp_dump_dir, NULL)) {

                fprintf(stderr, "warning: list dirs failed.\n");
                return -1;
        }

        path = gtk_tree_path_new_first();
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(list), path, NULL, FALSE);

        g_free(path);

        return 0;
}


/* update status information */

void
aifp_update_info(void) {

        gdouble space;

        sprintf(temp, "%d", number_of_songs); 
        gtk_label_set_text(GTK_LABEL(label_songs), temp);
        sprintf(temp, "%d bytes (%d MB)", songs_size, songs_size/MBYTES); 
        gtk_label_set_text(GTK_LABEL(label_songs_size), temp);

        battery_status = ifp_battery(&ifpdev);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_battery), battery_status/4.0);

        ifp_model(&ifpdev, temp, sizeof(temp));
        gtk_label_set_text(GTK_LABEL(label_model), temp);
        
        capacity = ifp_capacity(&ifpdev);
        sprintf(temp, "%d bytes (%d MB)", capacity, capacity/MBYTES); 
        gtk_label_set_text(GTK_LABEL(label_capacity), temp);

        freespace = ifp_freespace(&ifpdev);
        sprintf(temp, "%d bytes (%d MB)", freespace, freespace/MBYTES); 
        gtk_label_set_text(GTK_LABEL(label_freespace), temp);

        space = (double)freespace/capacity;

        sprintf(temp, "Free space (%d%%)", (gint)(space*100)); 
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_freespace), temp);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_freespace), space);

}


/* get the number of songs to send and overall data size to be transmitted */

gint
aifp_get_songs_info(void) {

	GtkTreeIter iter;
	gchar * str;
	gint i, k;
        struct stat statbuf;

        i = k = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(play_store), &iter, NULL, i++)) {

                if (gtk_tree_selection_iter_is_selected(play_select, &iter)) {

			gtk_tree_model_get(GTK_TREE_MODEL(play_store), &iter, 1, &str, -1);

                        if(g_stat(str, &statbuf) != -1) {

                                songs_size += statbuf.st_size;
                                k++;
                        }
                }
	}

        return k;
}


void 
aifp_show_message(gint type, gchar *message) {

        GtkWidget *info_dialog;

        info_dialog = gtk_message_dialog_new (options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window), 
                                              GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                              type, GTK_BUTTONS_CLOSE, message);

        gtk_widget_show (info_dialog);
        gtk_dialog_run(GTK_DIALOG(info_dialog));
        gtk_widget_destroy(info_dialog);

        set_sliders_width();    /* MAGIC */
}


gint 
aifp_check_and_init_device(void) {

        gint i, e;

        usb_init();

        dh = ifp_find_device();
        if (dh == NULL) {
                aifp_show_message(GTK_MESSAGE_ERROR, _("A suitable iRiver iFP device couldn't be found.\n"
                                  "Perhaps it's unplugged or turned off."));
                return -1;
        }

        dev = usb_device(dh);

        /* "must be called" written in the libusb documentation */
        if (usb_claim_interface(dh, dev->config->interface->altsetting->bInterfaceNumber)) {

                aifp_show_message(GTK_MESSAGE_ERROR, _("Device is busy.\n(I was unable to claim its"
                        " interface.)"));

                e = ifp_release_device(dh);
                if(e) { 
                        fprintf(stderr, "warning: release_device failed, i=%d\n",e);
                }

                return -1;
        }

        i = ifp_init(&ifpdev, dh);

        if(i) {
                
                sprintf(temp, _("Device isn't responding..\ntry jiggling the handle. (error %d)"),i);
                aifp_show_message(GTK_MESSAGE_ERROR, temp);

                usb_release_interface(dh, dev->config->interface->altsetting->bInterfaceNumber);

                e = ifp_release_device(dh);
                if(e) { 
                        fprintf(stderr, "warning: release_device failed, i=%d\n",e);
                }

                return -1;
        }

        return 0;
}


int
aifp_window_close(GtkWidget * widget, gpointer * data) {

        gint e;

        e = ifp_finalize(&ifpdev);

        if(e) { 
                fprintf(stderr, "warning: finalize failed, i=%d\n",e);  
        }

        usb_release_interface(dh, dev->config->interface->altsetting->bInterfaceNumber);

        e = ifp_release_device(dh);

        if(e) { 
                fprintf(stderr, "warning: release_device failed, i=%d\n",e); 
        }

        gtk_widget_destroy(aifp_window);
        aifp_window = NULL;
        set_sliders_width();
        return 0;
}



void aifp_transfer_files(void) {

        GtkWidget * vbox1;
        GtkWidget * vbox2;
        GtkWidget * vbox3;
        GtkWidget * vbox4;  
        GtkWidget * hbox1;
        GtkWidget * alignment;
        GtkWidget * table;
        GtkWidget * label;
        GtkWidget * frame;
        GtkWidget * scrolledwindow;
        GtkWidget * hseparator;
        GtkWidget * vseparator;
        GtkWidget * hbuttonbox;
        GtkTreeSelection * list_selection; 
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
  

        if (aifp_window != NULL) {
		return;
        }

        songs_size = 0;

        number_of_songs = aifp_get_songs_info();

        if(!number_of_songs) {
                aifp_show_message(GTK_MESSAGE_WARNING, _("Please select at least one song from playlist..."));
                return;
        }

        if(aifp_check_and_init_device() == -1) return;

        aifp_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(aifp_window), _("iFP device manager"));
        gtk_window_set_position(GTK_WINDOW(aifp_window), GTK_WIN_POS_CENTER);
	gtk_window_set_transient_for(GTK_WINDOW(aifp_window), 
                                     options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window));
	gtk_window_set_modal(GTK_WINDOW(aifp_window), TRUE);
        g_signal_connect(G_OBJECT(aifp_window), "delete_event",
                         G_CALLBACK(aifp_window_close), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(aifp_window), 5);
        gtk_window_set_default_size(GTK_WINDOW(aifp_window), 300, 530);

        vbox1 = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox1);
        gtk_container_add (GTK_CONTAINER (aifp_window), vbox1);

        vbox2 = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox2);
        gtk_box_pack_start (GTK_BOX (vbox1), vbox2, TRUE, TRUE, 0);

        frame = gtk_frame_new (NULL);
        gtk_widget_show (frame);
        gtk_box_pack_start (GTK_BOX (vbox2), frame, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

        alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
        gtk_widget_show (alignment);
        gtk_container_add (GTK_CONTAINER (frame), alignment);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);

        table = gtk_table_new (2, 2, FALSE);
        gtk_widget_show (table);
        gtk_container_add (GTK_CONTAINER (alignment), table);
        gtk_container_set_border_width (GTK_CONTAINER (table), 8);
        gtk_table_set_row_spacings (GTK_TABLE (table), 4);

        label = gtk_label_new (_("Selected files:"));
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

        label = gtk_label_new (_("Total size:"));
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

        label_songs = gtk_label_new (_("label_songs"));
        gtk_widget_show (label_songs);
        gtk_table_attach (GTK_TABLE (table), label_songs, 1, 2, 0, 1,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label_songs), 0, 0.5);
        gtk_misc_set_padding (GTK_MISC (label_songs), 5, 0);

        label_songs_size = gtk_label_new (_("label_songs_size"));
        gtk_widget_show (label_songs_size);
        gtk_table_attach (GTK_TABLE (table), label_songs_size, 1, 2, 1, 2,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label_songs_size), 0, 0.5);
        gtk_misc_set_padding (GTK_MISC (label_songs_size), 5, 0);

        label = gtk_label_new (_("<b>Songs info</b>"));
        gtk_widget_show (label);
        gtk_frame_set_label_widget (GTK_FRAME (frame), label);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

        frame = gtk_frame_new (NULL);
        gtk_widget_show (frame);
        gtk_box_pack_start (GTK_BOX (vbox2), frame, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

        alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
        gtk_widget_show (alignment);
        gtk_container_add (GTK_CONTAINER (frame), alignment);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);

        vbox3 = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox3);
        gtk_container_add (GTK_CONTAINER (alignment), vbox3);

        table = gtk_table_new (3, 2, FALSE);
        gtk_widget_show (table);
        gtk_box_pack_start (GTK_BOX (vbox3), table, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (table), 8);
        gtk_table_set_row_spacings (GTK_TABLE (table), 4);         

        label = gtk_label_new (_("Model:"));
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

        label = gtk_label_new (_("Capacity:"));
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

        label = gtk_label_new (_("Free space:"));
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

        label_model = gtk_label_new (_("label_model"));
        gtk_widget_show (label_model);
        gtk_table_attach (GTK_TABLE (table), label_model, 1, 2, 0, 1,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label_model), 0, 0.5);
        gtk_misc_set_padding (GTK_MISC (label_model), 5, 0);

        label_capacity = gtk_label_new (_("label_capacity"));
        gtk_widget_show (label_capacity);
        gtk_table_attach (GTK_TABLE (table), label_capacity, 1, 2, 1, 2,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label_capacity), 0, 0.5);
        gtk_misc_set_padding (GTK_MISC (label_capacity), 5, 0);

        label_freespace = gtk_label_new (_("label_freespace"));
        gtk_widget_show (label_freespace);
        gtk_table_attach (GTK_TABLE (table), label_freespace, 1, 2, 2, 3,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label_freespace), 0, 0.5);
        gtk_misc_set_padding (GTK_MISC (label_freespace), 5, 0);             

        table = gtk_table_new (1, 2, FALSE);
        gtk_widget_show (table);
        gtk_box_pack_start (GTK_BOX (vbox3), table, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (table), 8);
        gtk_table_set_col_spacings (GTK_TABLE (table), 8);

        progressbar_battery = gtk_progress_bar_new ();
        gtk_widget_show (progressbar_battery);
        gtk_table_attach (GTK_TABLE (table), progressbar_battery, 0, 1, 0, 1,
                          (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_battery), _("Battery"));

        progressbar_freespace = gtk_progress_bar_new ();
        gtk_widget_show (progressbar_freespace);
        gtk_table_attach (GTK_TABLE (table), progressbar_freespace, 1, 2, 0, 1,
                          (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_freespace), _("Free space"));

        label = gtk_label_new (_("<b>Device status</b>"));
        gtk_widget_show (label);
        gtk_frame_set_label_widget (GTK_FRAME (frame), label);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

        frame = gtk_frame_new (NULL);
        gtk_widget_show (frame);
        gtk_box_pack_start (GTK_BOX (vbox2), frame, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

        alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
        gtk_widget_show (alignment);
        gtk_container_add (GTK_CONTAINER (frame), alignment);
        gtk_container_set_border_width (GTK_CONTAINER (alignment), 4);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);

        hbox1 = gtk_hbox_new (FALSE, 0);
        gtk_widget_show (hbox1);
        gtk_container_add (GTK_CONTAINER (alignment), hbox1);   

        scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
        gtk_widget_show (scrolledwindow);
        gtk_box_pack_start (GTK_BOX (hbox1), scrolledwindow, TRUE, TRUE, 0);    
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_SHADOW_IN);

        list_store = gtk_list_store_new(1, G_TYPE_STRING);
        list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
        gtk_widget_show (list);
        list_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));  
        g_signal_connect(G_OBJECT(list_selection), "changed", G_CALLBACK(directory_selected_cb), NULL);   
        gtk_widget_set_size_request(GTK_WIDGET(list), -1, 140);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("",
							  renderer,
							  "text", 0,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
        gtk_container_add (GTK_CONTAINER (scrolledwindow), list);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);

        vseparator = gtk_vseparator_new ();
        gtk_widget_show (vseparator);
        gtk_box_pack_start (GTK_BOX (hbox1), vseparator, FALSE, TRUE, 6);

        vbox4 = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox4);
        gtk_box_pack_start (GTK_BOX (hbox1), vbox4, FALSE, FALSE, 0);

        mkdir_button = gui_stock_label_button(NULL, GTK_STOCK_NEW);
        gtk_widget_show (mkdir_button);
        g_signal_connect(mkdir_button, "clicked", G_CALLBACK(create_directory_cb), NULL);
        gtk_box_pack_start (GTK_BOX (vbox4), mkdir_button, FALSE, FALSE, 0);

        rndir_button = gui_stock_label_button(NULL, GTK_STOCK_EDIT);
        gtk_widget_show (rndir_button);
        g_signal_connect(rndir_button, "clicked", G_CALLBACK(rename_directory_cb), NULL);
        gtk_box_pack_start (GTK_BOX (vbox4), rndir_button, FALSE, FALSE, 0);

        rmdir_button = gui_stock_label_button(NULL, GTK_STOCK_DELETE);
        gtk_widget_show (rmdir_button);
        g_signal_connect(rmdir_button, "clicked", G_CALLBACK(remove_directory_cb), NULL);
        gtk_box_pack_start (GTK_BOX (vbox4), rmdir_button, FALSE, FALSE, 0);

        if (options.enable_tooltips) {
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), mkdir_button, _("Create a new directory"), NULL);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), rndir_button, _("Rename directory"), NULL);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), rmdir_button, _("Remove directory and its entire contents"), NULL);
        }

        label = gtk_label_new (_("<b>Destanation directory</b>"));
        gtk_widget_show (label);
        gtk_frame_set_label_widget (GTK_FRAME (frame), label);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

        frame = gtk_frame_new (NULL);
        gtk_widget_show (frame);
        gtk_box_pack_start (GTK_BOX (vbox2), frame, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

        alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
        gtk_widget_show (alignment);
        gtk_container_add (GTK_CONTAINER (frame), alignment);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);

        table = gtk_table_new (2, 2, FALSE);
        gtk_widget_show (table);
        gtk_container_add (GTK_CONTAINER (alignment), table);
        gtk_container_set_border_width (GTK_CONTAINER (table), 8);
        gtk_table_set_row_spacings (GTK_TABLE (table), 4);
        gtk_table_set_col_spacings (GTK_TABLE (table), 4);

        label = gtk_label_new (_("Current file: "));
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

        label = gtk_label_new (_("Overall: "));
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

        progressbar_cf = gtk_progress_bar_new ();
        gtk_widget_show (progressbar_cf);
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_cf), _("Idle."));
        gtk_table_attach (GTK_TABLE (table), progressbar_cf, 1, 2, 0, 1,
                          (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);

        progressbar_op = gtk_progress_bar_new ();
        gtk_widget_show (progressbar_op);
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_op), _("Idle."));
        gtk_table_attach (GTK_TABLE (table), progressbar_op, 1, 2, 1, 2,
                          (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);

        label = gtk_label_new (_("<b>Uploading progress</b>"));
        gtk_widget_show (label);
        gtk_frame_set_label_widget (GTK_FRAME (frame), label);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

        hseparator = gtk_hseparator_new ();
        gtk_widget_show (hseparator);
        gtk_box_pack_start (GTK_BOX (vbox1), hseparator, FALSE, TRUE, 3);

        hbuttonbox = gtk_hbutton_box_new ();
        gtk_widget_show (hbuttonbox);
        gtk_box_pack_start (GTK_BOX (vbox1), hbuttonbox, FALSE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (hbuttonbox), 5);
        gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_END);
        gtk_box_set_spacing (GTK_BOX (hbuttonbox), 6);

        upload_button = gui_stock_label_button (_("Upload songs"), GTK_STOCK_GO_UP);
        gtk_widget_show (upload_button);
        g_signal_connect(upload_button, "clicked", G_CALLBACK(upload_songs_cb), NULL);
        gtk_container_add (GTK_CONTAINER (hbuttonbox), upload_button);
        GTK_WIDGET_SET_FLAGS (upload_button, GTK_CAN_DEFAULT);

        abort_button = gui_stock_label_button (_("Abort"), GTK_STOCK_CANCEL);
        gtk_widget_show (abort_button);
        g_signal_connect(abort_button, "clicked", G_CALLBACK(abort_transfer_cb), NULL);
        gtk_container_add (GTK_CONTAINER (hbuttonbox), abort_button);
        GTK_WIDGET_SET_FLAGS (abort_button, GTK_CAN_DEFAULT);

        close_button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
        gtk_widget_show (close_button);
        g_signal_connect(close_button, "clicked", G_CALLBACK(aifp_window_close), NULL);
        gtk_container_add (GTK_CONTAINER (hbuttonbox), close_button);
        GTK_WIDGET_SET_FLAGS (close_button, GTK_CAN_DEFAULT);     

        gtk_widget_set_sensitive(abort_button, FALSE);
        gtk_widget_grab_focus(close_button);

        /* update info and directory listing */

        aifp_update_info();

        aifp_directory_listing();
        
        if(songs_size > freespace)
                gtk_widget_set_sensitive(upload_button, FALSE);
        else
                gtk_widget_set_sensitive(upload_button, TRUE);

        abort_pressed = 0;

        gtk_widget_show(aifp_window);
        set_sliders_width();
}

#endif /* HAVE_IFP */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

