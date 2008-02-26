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

#ifdef HAVE_IFP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <limits.h>
#include <usb.h>
#include <ifp.h>

#include "common.h"
#include "utils_gui.h"
#include "i18n.h"
#include "options.h"
#include "gui_main.h"
#include "ifp_device.h"
#include "playlist.h"
#include "decoder/file_decoder.h"

extern options_t options;
extern GtkWidget * playlist_window;
extern GtkWidget * main_window;
extern GtkTooltips * aqualung_tooltips;

void aifp_close_device(void);
gint aifp_directory_listing(gchar *name);
void aifp_check_size(void);
void aifp_update_info(void);

struct usb_device *dev = NULL;
usb_dev_handle *dh;
struct ifp_device ifpdev;

gchar remote_path[MAXLEN], remote_item[MAXLEN];
gchar dest_dir[MAXLEN], dest_file[MAXLEN];
guint songs_size, number_of_songs;
gint battery_status, capacity, freespace, abort_pressed;
gint valid_files, transfer_active, remote_type;
gint transfer_mode;

GtkWidget * aifp_window = NULL;
GtkWidget * upload_download_button;
GtkWidget * abort_button;
GtkWidget * close_button;   
GtkWidget * mkdir_button;
GtkWidget * rndir_button;
GtkWidget * rmdir_button; 
GtkWidget * local_path_entry;
GtkWidget * local_path_browse_button;
GtkWidget * aifp_close_when_ready_check;
GtkWidget * label_songs;
GtkWidget * label_songs_size;
GtkWidget * label_model;
GtkWidget * progressbar_battery;
GtkWidget * progressbar_freespace;
GtkWidget * progressbar_cf;
GtkWidget * progressbar_op;
GtkWidget * aifp_file_entry;
GtkWidget * list;
GtkListStore * list_store = NULL;

GtkWidget *mkdir_dialog;
GtkWidget *rename_dialog;

/* list of accepted file extensions */
char * valid_extensions_ifp[] = {
        "mp3", "ogg", "wma", "asf", NULL
};


int
aifp_window_close(GtkWidget * widget, gpointer * data) {

        aifp_close_device();

        gtk_window_get_size(GTK_WINDOW(aifp_window), &options.ifpmanager_size_x, &options.ifpmanager_size_y);
        gtk_widget_destroy(aifp_window);
        aifp_window = NULL;
        return 0;
}


void
abort_transfer_cb (GtkButton *button, gpointer user_data) {
        abort_pressed = 1;
}


static int
update_progress (void *context, struct ifp_transfer_status *status) {

        gchar temp[MAXLEN];

        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_cf), (float)status->file_bytes/status->file_total);
        sprintf(temp, _("%.1f MB / %.1f MB"), (float)status->file_bytes/(1024*1024), (float)status->file_total/(1024*1024));
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_cf), temp);

        if (abort_pressed) {
                return 1;
        } else {
                return 0;
        }
}


int
upload_songs_cb_foreach(playlist_t * pl, GtkTreeIter * iter, void * data) {

        int * n = (int *)data;
        char * file;
	char temp[MAXLEN];
	playlist_data_t * pldata;

        if (abort_pressed) {
                return 0;
        }

        gtk_tree_model_get(GTK_TREE_MODEL(pl->store), iter, PL_COL_DATA, &pldata, -1);

        if (!g_file_test(pldata->file, G_FILE_TEST_EXISTS)) {
		return 0;
 	}

	file = g_path_get_basename(pldata->file);

        strncpy(dest_file, remote_path, MAXLEN-1);
        if (strlen(remote_path) != 1) {
                strncat(dest_file, "\\", MAXLEN-1);
        }
        strncat(dest_file, file, MAXLEN-1);

        gtk_entry_set_text(GTK_ENTRY(aifp_file_entry), file);
        gtk_editable_set_position(GTK_EDITABLE(aifp_file_entry), -1);

        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_op),
                                      (float)(*n + 1) / number_of_songs);
        sprintf(temp, _("%d / %d files"), *n + 1, number_of_songs);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR (progressbar_op), temp);

        ifp_upload_file(&ifpdev, pldata->file, dest_file, update_progress, NULL);

        aifp_update_info();

        g_free(file);

        (*n)++;

        return 0;
}


gboolean
download_songs_cb_foreach (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {

        gchar *file;
        gchar temp[MAXLEN];
        int * n = (int *)data;

        if (abort_pressed) {
                return 0;
        }

        gtk_tree_model_get (model, iter, COLUMN_NAME, &file, -1);  

        if (strncmp(file, PARENTDIR, 2)) {
                strncpy(remote_item, remote_path, MAXLEN-1);
                if (strlen(remote_path) != 1) {
                        strncat(remote_item, "\\", MAXLEN-1);
                }
                strncat(remote_item, file, MAXLEN-1);

                strncpy(dest_file, dest_dir, MAXLEN-1);
                strncat(dest_file, "/", MAXLEN-1);
                strncat(dest_file, file, MAXLEN-1);

                gtk_entry_set_text(GTK_ENTRY(aifp_file_entry), file);
                gtk_editable_set_position(GTK_EDITABLE(aifp_file_entry), -1);

                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_op),
                                              (float)(*n + 1) / number_of_songs);

                if (ifp_is_file (&ifpdev, remote_item) == TRUE) {
                        sprintf(temp, _("%d / %d files"), *n + 1, number_of_songs);
                        gtk_progress_bar_set_text(GTK_PROGRESS_BAR (progressbar_op), temp);
                        ifp_download_file (&ifpdev, remote_item, dest_file, update_progress, NULL);
                } else {                      
                        sprintf(temp, _("%d / %d directories"), *n + 1, number_of_songs);
                        gtk_progress_bar_set_text(GTK_PROGRESS_BAR (progressbar_op), temp);
                        ifp_download_dir (&ifpdev, remote_item, dest_file, update_progress, NULL);
                }

                aifp_update_info();
                g_free(file);

                (*n)++;
        }
 
        return TRUE;
}


void
upload_download_songs_cb(GtkButton * button, gpointer user_data) {

        int n = 0;
        playlist_t * pl = playlist_get_current();

        if (transfer_mode == UPLOAD_MODE && pl == NULL) {
                return;
        }

        if (transfer_mode == DOWNLOAD_MODE) {
                if (access(dest_dir, W_OK) == -1) {               
                        message_dialog(_("Error"), aifp_window, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,  NULL, 
                                       _("Cannot write to selected directory. Please select another directory."));
                        return;
                }
        }

        transfer_active = 1;

        gtk_widget_set_sensitive(abort_button, TRUE);
        gtk_widget_set_sensitive(upload_download_button, FALSE);
        gtk_widget_set_sensitive(close_button, FALSE);
        if (transfer_mode == UPLOAD_MODE) {
                gtk_widget_set_sensitive(mkdir_button, FALSE);
        } else {
                gtk_widget_set_sensitive(local_path_browse_button, FALSE);
        }
        gtk_widget_set_sensitive(list, FALSE);
        gtk_widget_set_sensitive(rndir_button, FALSE);
        gtk_widget_set_sensitive(rmdir_button, FALSE);

        if (transfer_mode == UPLOAD_MODE) {
                playlist_foreach_selected(pl, (void *)upload_songs_cb_foreach, &n);
        } else {
                GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
                gtk_tree_selection_selected_foreach(selection, (GtkTreeSelectionForeachFunc)download_songs_cb_foreach, &n);
        }

        gtk_widget_set_sensitive(abort_button, FALSE);
        gtk_widget_set_sensitive(close_button, TRUE);
        gtk_widget_set_sensitive(list, TRUE);

        if (!abort_pressed) {
                gtk_widget_set_sensitive(upload_download_button, FALSE);
                gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar_op), _("Done"));
                gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar_cf), _("Done"));
        } else {
                gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar_op), _("Aborted..."));
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_op), 0.0);
                gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar_cf), _("Aborted..."));
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_cf), 0.0);
                gtk_widget_set_sensitive(upload_download_button, TRUE);
        }

        aifp_update_info();

        gtk_widget_set_sensitive(abort_button, FALSE);
        gtk_widget_set_sensitive(upload_download_button, TRUE);
        gtk_widget_set_sensitive(close_button, TRUE);
        if (transfer_mode == UPLOAD_MODE) {
                gtk_widget_set_sensitive(mkdir_button, TRUE);
        } else {
                gtk_widget_set_sensitive(local_path_browse_button, TRUE);
        }
        gtk_widget_set_sensitive(list, TRUE);
        gtk_widget_set_sensitive(rndir_button, TRUE);
        gtk_widget_set_sensitive(rmdir_button, TRUE);
        gtk_widget_grab_focus(list);

        gtk_entry_set_text(GTK_ENTRY(aifp_file_entry), _("None"));

        aifp_directory_listing(NULL);
        aifp_update_info();
        
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(aifp_close_when_ready_check)) && !abort_pressed) {
                aifp_window_close(NULL, NULL);
        }

        abort_pressed = 0;
        transfer_active = 0;
}


int 
mkdir_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data) {

        if (event->keyval == GDK_Return) {
                gtk_dialog_response(GTK_DIALOG (mkdir_dialog), GTK_RESPONSE_OK);
        } 
        return FALSE;
}


void
aifp_create_directory_cb (GtkButton *button, gpointer user_data) {

        GtkWidget *name_entry;
        gint response;
        gchar temp[MAXLEN];

        mkdir_dialog = gtk_message_dialog_new (GTK_WINDOW(aifp_window),
                                              GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, _("Please enter directory name."));

        gtk_window_set_title(GTK_WINDOW(mkdir_dialog), _("Create directory"));

        name_entry = gtk_entry_new();
        g_signal_connect (G_OBJECT(name_entry), "key_press_event", G_CALLBACK(mkdir_key_press), NULL);
        gtk_entry_set_max_length(GTK_ENTRY(name_entry), 64);
        gtk_widget_set_size_request(GTK_WIDGET(name_entry), 300, -1);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mkdir_dialog)->vbox), name_entry, FALSE, FALSE, 6);
        gtk_widget_show_all (mkdir_dialog);

        response = aqualung_dialog_run(GTK_DIALOG(mkdir_dialog));

        if (response == GTK_RESPONSE_OK) {

                strncpy(temp, remote_path, MAXLEN-1);
                if (strlen(remote_path) != 1) {
                        strncat(temp, "\\", MAXLEN-1);
                }
                strncat(temp, gtk_entry_get_text(GTK_ENTRY(name_entry)), MAXLEN-1);

                if (strlen(temp)) {

                        ifp_mkdir(&ifpdev, temp);
                        aifp_directory_listing(temp);
                        aifp_update_info();
                }
        }

        gtk_widget_destroy(mkdir_dialog);
}


gint 
rename_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data) {

        if (event->keyval == GDK_Return) {
                gtk_dialog_response(GTK_DIALOG (rename_dialog), GTK_RESPONSE_OK);
        }
        return FALSE;
}

void
aifp_rename_item_cb (GtkButton *button, gpointer user_data) {

        GtkWidget *name_entry;
        gchar temp[MAXLEN];
        gint response;
        const gchar * text;

        if (strncmp(remote_item, PARENTDIR, 2)) {

                rename_dialog = gtk_message_dialog_new (GTK_WINDOW(aifp_window),
                                                      GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                      GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, _("Please enter a new name."));

                gtk_window_set_title(GTK_WINDOW(rename_dialog), _("Rename"));

                name_entry = gtk_entry_new();
                g_signal_connect (G_OBJECT(name_entry), "key_press_event", G_CALLBACK(rename_key_press), NULL);
                gtk_entry_set_max_length(GTK_ENTRY(name_entry), 64);
                gtk_widget_set_size_request(GTK_WIDGET(name_entry), 300, -1);
                gtk_entry_set_text(GTK_ENTRY(name_entry), remote_item);

                gtk_box_pack_start(GTK_BOX(GTK_DIALOG(rename_dialog)->vbox), name_entry, FALSE, FALSE, 6);
                gtk_widget_show_all (rename_dialog);

                response = aqualung_dialog_run(GTK_DIALOG(rename_dialog));

                if (response == GTK_RESPONSE_OK) {

                        text = gtk_entry_get_text(GTK_ENTRY(name_entry));

                        strncpy(temp, remote_path, MAXLEN-1);
                        if (strlen(remote_path) != 1) {
                                strncat(temp, "\\", MAXLEN-1);
                        }
                        strncat(temp, text, MAXLEN-1);

                        if (strlen(text)) {
                                strncpy(dest_file, remote_path, MAXLEN-1);
                                if (strlen(remote_path) != 1) {
                                        strncat(dest_file, "\\", MAXLEN-1);
                                }
                                strncat(dest_file, remote_item, MAXLEN-1);

                                ifp_rename(&ifpdev, dest_file, temp);
                                aifp_update_info();
                                aifp_directory_listing (NULL);
                        }
                }

                gtk_widget_destroy(rename_dialog);
        }
}

void
aifp_remove_item_cb (GtkButton *button, gpointer user_data) {

        gchar temp[MAXLEN];
        gint response;

        if (strncmp(remote_item, PARENTDIR, 2)) {

                if (remote_type == TYPE_DIR) {
                        sprintf(temp, _("Directory '%s' will be removed with its entire contents.\n\nDo you want to proceed?"), 
                                remote_item);
                } else {
                        sprintf(temp, _("File '%s' will be removed.\n\nDo you want to proceed?"),
                                remote_item);
                }

                response = message_dialog(_("Remove"), aifp_window, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                                          NULL, temp);

                if (response == GTK_RESPONSE_YES) {   

                        strncpy(temp, remote_path, MAXLEN-1);
                        if (strlen(remote_path) != 1) {
                                strncat(temp, "\\", MAXLEN-1);
                        }
                        strncat(temp, remote_item, MAXLEN-1);

                        if (ifp_is_file (&ifpdev, temp) == TRUE) {
                                ifp_delete (&ifpdev, temp);
                        } else {
                                ifp_delete_dir_recursive(&ifpdev, temp);
                        }
                        aifp_update_info();
                        aifp_check_size();
                        aifp_directory_listing (NULL);
                }

        }
}


void
item_selected (GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {

        gchar *text, *type;
        int * n = (int *)data;

        gtk_tree_model_get (model, iter, COLUMN_NAME, &text, COLUMN_TYPE_SIZE, &type, -1);  
        strncpy(remote_item, text, MAXLEN-1);

        remote_type = TYPE_DIR;

        if(type != NULL) {
                if(strncmp(type, DIRID, strlen(DIRID))) {
                        remote_type = TYPE_FILE;
                }
                if (n != NULL) {
                        (*n)++;
                }
        }
 
        g_free(text);
        g_free(type);
}

gboolean
multiple_items_selected_foreach_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {

        item_selected (model, iter, (int *)data);
        return TRUE;
}

void 
directory_selected_cb (GtkTreeSelection *selection, gpointer data) {

        GtkTreeIter iter;
        GtkTreeModel *model;  

        if (transfer_mode == UPLOAD_MODE) {
                if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                        item_selected (model, &iter, NULL);
                }
        } else {
                number_of_songs = 0;
                gtk_tree_selection_selected_foreach(selection, (GtkTreeSelectionForeachFunc) multiple_items_selected_foreach_cb, &number_of_songs);
        }
}


gint 
aifp_dump_dir(void *context, int type, const char *name, int filesize) {

        GtkTreeIter iter;
        gint i = 0;

        if (type == IFP_DIR) {
                gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store), &iter, NULL, i++);
                gtk_list_store_append(list_store, &iter);
                gtk_list_store_set(list_store, &iter, 0, name, 1, DIRID, -1);
        } 
        return 0;
}


gint 
aifp_dump_files(void *context, int type, const char *name, int filesize) {

        GtkTreeIter iter;
        gchar temp[MAXLEN];

        if (type == IFP_FILE) {
                gtk_list_store_append(list_store, &iter);
                sprintf(temp, "%.1f MB", (double)filesize/(1024*1024));
                gtk_list_store_set(list_store, &iter, 0, name, 1, temp, -1);
        }

        return 0;
}


gint
aifp_directory_listing(gchar *name) {

        GtkTreeIter iter;
        GtkTreePath *path;
        gint d = 0;
        gchar * item_name;

        gtk_list_store_clear(list_store);

        if (strlen(remote_path) != 1) {
                gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store), &iter, NULL, 0);
                gtk_list_store_append(list_store, &iter);
                gtk_list_store_set(list_store, &iter, 0, PARENTDIR, -1);
        }

        if (ifp_list_dirs(&ifpdev, remote_path, aifp_dump_dir, NULL)) {

                fprintf(stderr, "ifp_device.c: aifp_directory_listing(): list dirs failed.\n");
                return -1;
        }

        if (ifp_list_dirs(&ifpdev, remote_path, aifp_dump_files, NULL)) {

                fprintf(stderr, "ifp_device.c: aifp_directory_listing(): list dirs failed.\n");
                return -1;
        }

        if (!name) {
                path = gtk_tree_path_new_first();
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(list), path, NULL, FALSE);
                g_free(path);
        } else {
                while(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store), &iter, NULL, d++)) {

                        gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter, COLUMN_NAME, &item_name, -1);

                        if (!strcmp(item_name, name)) {
                                path = gtk_tree_path_new_from_indices (d-1, -1);
                                gtk_tree_view_set_cursor(GTK_TREE_VIEW(list), path, NULL, FALSE);
                                gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (list), path,
                                              NULL, TRUE, 0.5, 0.0);
                                g_free(path);
                                break;
                        }
                }
        }

        return 0;
}


void
aifp_update_info(void) {

        gchar temp[MAXLEN], tmp[MAXLEN];
        gfloat space;

        if (transfer_mode == UPLOAD_MODE) {
                sprintf(temp, "%d", number_of_songs); 
                sprintf(tmp, _(" (%.1f MB)"), (float)songs_size / (1024*1024)); 
                strncat (temp, tmp, MAXLEN-1);
                gtk_label_set_text(GTK_LABEL(label_songs), temp);
        }

        battery_status = ifp_battery(&ifpdev);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_battery), battery_status / 4.0);

        ifp_model(&ifpdev, temp, sizeof(temp));
        capacity = ifp_capacity(&ifpdev);
        sprintf(tmp, _(" (capacity = %.1f MB)"), (float)capacity / (1024*1024)); 
        strncat (temp, tmp, MAXLEN-1);
        gtk_label_set_text(GTK_LABEL(label_model), temp);
        
        freespace = ifp_freespace(&ifpdev);
        sprintf(temp, _(" Free space (%.1f MB)"), (float)freespace / (1024*1024)); 

        space = (float)freespace/capacity;

        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_freespace), temp);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_freespace), space);
}


int
aifp_get_songs_info_foreach(playlist_t * pl, GtkTreeIter * iter, void * data) {

        struct stat statbuf;
        playlist_data_t * pldata;
        gint * num = (int *)data;

        gtk_tree_model_get(GTK_TREE_MODEL(pl->store), iter, PL_COL_DATA, &pldata, -1);

        if (g_stat(pldata->file, &statbuf) != -1) {
                songs_size += statbuf.st_size;
                (*num)++;
        }

        return 0;
}


gint
aifp_get_number_of_songs(void) {

        gint num = 0;
        playlist_t * pl = playlist_get_current();

        if (pl != NULL) {
                playlist_foreach_selected(pl, (void *)aifp_get_songs_info_foreach, &num);
        }

        return num;
}


int
aifp_check_files_cb_foreach(playlist_t * pl, GtkTreeIter * iter, void * data) {

        struct stat statbuf;
	playlist_data_t * pldata;

        gtk_tree_model_get(GTK_TREE_MODEL(pl->store), iter, PL_COL_DATA, &pldata, -1);

        if (g_stat(pldata->file, &statbuf) != -1) {
                valid_files += is_valid_extension(valid_extensions_ifp, pldata->file, 0);
        }

        return 0;
}

gint 
aifp_check_files(void) {

        playlist_t * pl = playlist_get_current();

        valid_files = 0;

        if (pl != NULL) {
                playlist_foreach_selected(pl, (void *)aifp_check_files_cb_foreach, NULL);
        }

        return valid_files;
}


void
aifp_close_device(void) {

        if (ifp_finalize(&ifpdev)) { 
                fprintf(stderr, "ifp_device.c: aifp_window_close(): finalize failed\n");  
        }

        usb_release_interface(dh, dev->config->interface->altsetting->bInterfaceNumber);

        if (ifp_release_device(dh)) { 
                fprintf(stderr, "ifp_device.c: aifp_window_close(): release_device failed\n"); 
        }
}


gint 
aifp_check_and_init_device(void) {

        usb_init();

        dh = ifp_find_device();
        if (dh == NULL) {
                message_dialog(_("Error"), options.playlist_is_embedded ? GTK_WIDGET(main_window) : GTK_WIDGET(playlist_window),
                               GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, NULL, _("No suitable iRiver iFP device found.\n"
                               "Perhaps it is unplugged or turned off."));
                return -1;
        }

        dev = usb_device(dh);

        if (usb_claim_interface(dh, dev->config->interface->altsetting->bInterfaceNumber)) {

                message_dialog(_("Error"), options.playlist_is_embedded ? GTK_WIDGET(main_window) : GTK_WIDGET(playlist_window),
                               GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, NULL, _("Device is busy.\n(Aqualung was unable to claim its interface.)"));

                if (ifp_release_device(dh)) { 
                        fprintf(stderr, "ifp_device.c: aifp_check_and_init_device(): release_device failed\n");
                }

                return -1;
        }

        if (ifp_init(&ifpdev, dh)) {
 
                message_dialog(_("Error"), options.playlist_is_embedded ? GTK_WIDGET(main_window) : GTK_WIDGET(playlist_window),
                               GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, NULL, _("Device is not responding.\nTry jiggling the handle."));

                usb_release_interface(dh, dev->config->interface->altsetting->bInterfaceNumber);

                if (ifp_release_device(dh)) { 
                        fprintf(stderr, "ifp_device.c: aifp_check_and_init_device(): release_device failed\n");
                }
                return -1;
        }
        return 0;
}


void 
aifp_check_size(void) {
        if (transfer_mode == UPLOAD_MODE) {
                if(songs_size > freespace) {
                        gtk_widget_set_sensitive(upload_download_button, FALSE);
                } else {
                        gtk_widget_set_sensitive(upload_download_button, TRUE);
                }
        }
}

gint
aifp_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent) {

        if (!transfer_active) {

                switch (kevent->keyval) {
                        case GDK_Escape:
                                aifp_window_close (NULL, NULL);
                                return TRUE;
                        case GDK_c:
                                if (transfer_mode == UPLOAD_MODE) {
                                        aifp_create_directory_cb (NULL, NULL);
                                        return TRUE;
                                } else {
                                        break;
                                }
                        case GDK_r:
                                aifp_rename_item_cb (NULL, NULL);
                                return TRUE;
                        case GDK_Delete:
                        case GDK_KP_Delete:
                                aifp_remove_item_cb (NULL, NULL);
                                return TRUE;
                }
        }
        return FALSE;
}

gint
aifp_list_dbclick_cb(GtkWidget * widget, GdkEventButton * event, gpointer func_data) {

gchar *npath;

    if ((event->type==GDK_2BUTTON_PRESS) && (event->button == 1)) {
        
        if (remote_type == TYPE_DIR) {

                if (!strncmp(remote_item, PARENTDIR, 2)) {
            
                        npath = strrchr (remote_path, '\\');

                        if (npath != NULL) {
                                *npath = '\0';
                                if (!strlen(remote_path)) {
                                        strcpy(remote_path, "\\");
                                }
                                aifp_directory_listing(NULL);
                        }
                } else {
                        if (strlen(remote_path) != 1) { 
                                strcat(remote_path, "\\");
                        }
                        strncat(remote_path, remote_item, MAXLEN-1);
                        aifp_directory_listing(NULL);
                }
        }
        return TRUE;
    }
    return FALSE;
}


void
directory_chooser(char * title, GtkWidget * parent, char * directory) {

        GtkWidget * dialog;
	const gchar * selected_directory;

        dialog = gtk_file_chooser_dialog_new(title,
                                             GTK_WINDOW(parent),
                                             GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), directory);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	if (options.show_hidden) {
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);
	}

        if (aqualung_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {

                char * utf8;

                selected_directory = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		utf8 = g_filename_to_utf8(selected_directory, -1, NULL, NULL, NULL);

		if (utf8 == NULL) {
			gtk_widget_destroy(dialog);
		}

                strncpy(directory, selected_directory, MAXLEN-1);
		g_free(utf8);
        }

        gtk_widget_destroy(dialog);
}


void
local_path_selected_cb(GtkButton * button, gpointer data) {
	directory_chooser(_("Please select a local path."), aifp_window, dest_dir);
        gtk_entry_set_text(GTK_ENTRY(local_path_entry), dest_dir);
}


void 
aifp_transfer_files(gint mode) {

        GtkWidget * vbox1;
        GtkWidget * vbox2;
        GtkWidget * vbox3;
        GtkWidget * vbox4;  
        GtkWidget * hbox1;
        GtkWidget * hbox2;
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
        gchar temp[MAXLEN];

        if (aifp_window != NULL) {
                return;
        }

        transfer_mode = mode;
        songs_size = 0;
        transfer_active = 0;
        strcpy(remote_path, "\\");

        if (transfer_mode == UPLOAD_MODE) {
                number_of_songs = aifp_get_number_of_songs();

                if (!number_of_songs) {
                        message_dialog(_("Warning"), options.playlist_is_embedded ? GTK_WIDGET(main_window) : GTK_WIDGET(playlist_window),
                                       GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, NULL, _("Please select at least one valid song from playlist."));
                        return;
                }
        }

        if (aifp_check_and_init_device() == -1) {
                return;
        }

        if (transfer_mode == UPLOAD_MODE) {
                gint k = aifp_check_files();

                if (k != number_of_songs) {
                        if (k) {
                                if((number_of_songs-k) == 1) {
                                        sprintf(temp, _("One song has format unsupported by your player.\n\nDo you want to proceed?"));
                                } else {
                                        sprintf(temp, _("%d of %d songs have format unsupported by your player.\n\nDo you want to proceed?"), number_of_songs-k, number_of_songs);
                                }
                        } else {
                                if (number_of_songs == 1) {
                                        sprintf(temp, _("The selected song has format unsupported by your player.\n\nDo you want to proceed?"));
                                } else {
                                        sprintf(temp, _("None of the selected songs has format supported by your player.\n\nDo you want to proceed?"));
                                }
                        }

                        gint ret = message_dialog(_("Warning"), options.playlist_is_embedded ? GTK_WIDGET(main_window) : GTK_WIDGET(playlist_window),
                                       GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, NULL, temp);

                        if (ret == GTK_RESPONSE_NO || ret == GTK_RESPONSE_DELETE_EVENT) {
                                aifp_close_device();
                                return;
                        }
                }
        }

        aifp_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        if (transfer_mode == UPLOAD_MODE) {
                gtk_window_set_title(GTK_WINDOW(aifp_window), _("iFP device manager (upload mode)"));
        } else {
                gtk_window_set_title(GTK_WINDOW(aifp_window), _("iFP device manager (download mode)"));
        }
        gtk_window_set_position(GTK_WINDOW(aifp_window), GTK_WIN_POS_CENTER_ALWAYS);
        gtk_window_set_transient_for(GTK_WINDOW(aifp_window), 
                                     options.playlist_is_embedded ? GTK_WINDOW(main_window) : GTK_WINDOW(playlist_window));
        gtk_window_set_modal(GTK_WINDOW(aifp_window), TRUE);
        g_signal_connect(G_OBJECT(aifp_window), "delete_event",
                         G_CALLBACK(aifp_window_close), NULL);
        g_signal_connect(G_OBJECT(aifp_window), "key_press_event",
                         G_CALLBACK(aifp_window_key_pressed), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(aifp_window), 5);
        gtk_window_set_default_size(GTK_WINDOW(aifp_window), options.ifpmanager_size_x, options.ifpmanager_size_y);

        vbox1 = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox1);
        gtk_container_add (GTK_CONTAINER (aifp_window), vbox1);

        vbox2 = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox2);
        gtk_box_pack_start (GTK_BOX (vbox1), vbox2, TRUE, TRUE, 0);

        frame = gtk_frame_new (NULL);
        gtk_widget_show (frame);
        gtk_box_pack_start (GTK_BOX (vbox2), frame, FALSE, FALSE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

        alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
        gtk_widget_show (alignment);
        gtk_container_add (GTK_CONTAINER (frame), alignment);
        gtk_container_set_border_width (GTK_CONTAINER (alignment), 4);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);

        if (transfer_mode == UPLOAD_MODE) {

                hbox1 = gtk_hbox_new (FALSE, 0);
                gtk_widget_show (hbox1);
                gtk_container_add (GTK_CONTAINER (alignment), hbox1);

                label = gtk_label_new (_("Selected files:"));
                gtk_widget_show (label);
                gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);
                gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

                label_songs = gtk_label_new (_("label_songs"));
                gtk_widget_show (label_songs);
                gtk_box_pack_start (GTK_BOX (hbox1), label_songs, FALSE, FALSE, 0);
                gtk_misc_set_alignment (GTK_MISC (label_songs), 0, 0.5);
                gtk_misc_set_padding (GTK_MISC (label_songs), 5, 0);

         
                label = gtk_label_new (_("<b>Songs info</b>"));
                gtk_widget_show (label);
                gtk_frame_set_label_widget (GTK_FRAME (frame), label);
                gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

                frame = gtk_frame_new (NULL);
                gtk_widget_show (frame);
                gtk_box_pack_start (GTK_BOX (vbox2), frame, FALSE, FALSE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
                gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

                alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
                gtk_widget_show (alignment);
                gtk_container_add (GTK_CONTAINER (frame), alignment);
                gtk_container_set_border_width (GTK_CONTAINER (alignment), 4);
                gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);
        }

        vbox3 = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox3);
        gtk_container_add (GTK_CONTAINER (alignment), vbox3);

        hbox1 = gtk_hbox_new (FALSE, 0);
        gtk_widget_show (hbox1);
        gtk_box_pack_start (GTK_BOX (vbox3), hbox1, TRUE, TRUE, 2);

        label = gtk_label_new (_("Model:"));
        gtk_widget_show (label);
        gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

        label_model = gtk_label_new (NULL);
        gtk_widget_show (label_model);
        gtk_box_pack_start (GTK_BOX (hbox1), label_model, FALSE, FALSE, 0);
        gtk_misc_set_alignment (GTK_MISC (label_model), 0, 0.5);
        gtk_misc_set_padding (GTK_MISC (label_model), 5, 0);

        hbox1 = gtk_hbox_new (FALSE, 0);
        gtk_widget_show (hbox1);
        gtk_box_pack_start (GTK_BOX (vbox3), hbox1, TRUE, TRUE, 2);

        progressbar_battery = gtk_progress_bar_new ();
        gtk_widget_show (progressbar_battery);
        gtk_box_pack_start (GTK_BOX (hbox1), progressbar_battery, TRUE, TRUE, 2);
        gtk_widget_set_size_request (progressbar_battery, 70, -1);
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_battery), _("Battery"));

        progressbar_freespace = gtk_progress_bar_new ();
        gtk_widget_show (progressbar_freespace);
        gtk_box_pack_start (GTK_BOX (hbox1), progressbar_freespace, TRUE, TRUE, 2);
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

        list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
        list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
        gtk_widget_show (list);
	g_signal_connect(G_OBJECT(list), "button_press_event", 
                     G_CALLBACK(aifp_list_dbclick_cb), NULL);      

        list_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));  
        g_signal_connect(G_OBJECT(list_selection), "changed", G_CALLBACK(directory_selected_cb), NULL);   
        gtk_container_add (GTK_CONTAINER (scrolledwindow), list);
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), TRUE);
        gtk_tree_view_set_enable_search (GTK_TREE_VIEW(list), FALSE);

        if (transfer_mode == DOWNLOAD_MODE) {
                gtk_tree_selection_set_mode(list_selection, GTK_SELECTION_MULTIPLE);
        }

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_("Name"),
                                                          renderer,
                                                          "text", COLUMN_NAME,
                                                          NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
        gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_("Size"),
                                                          renderer,
                                                          "text", COLUMN_TYPE_SIZE,
                                                          NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
        gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

        vseparator = gtk_vseparator_new ();
        gtk_widget_show (vseparator);
        gtk_box_pack_start (GTK_BOX (hbox1), vseparator, FALSE, TRUE, 2);

        vbox4 = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (vbox4);
        gtk_box_pack_start (GTK_BOX (hbox1), vbox4, FALSE, FALSE, 0);

        if (transfer_mode == UPLOAD_MODE) {
                mkdir_button = gui_stock_label_button(NULL, GTK_STOCK_NEW);
                gtk_button_set_relief (GTK_BUTTON (mkdir_button), GTK_RELIEF_NONE); 
                GTK_WIDGET_UNSET_FLAGS(mkdir_button, GTK_CAN_FOCUS);
                gtk_widget_show (mkdir_button);
                g_signal_connect(mkdir_button, "clicked", G_CALLBACK(aifp_create_directory_cb), NULL);
                gtk_box_pack_start (GTK_BOX (vbox4), mkdir_button, FALSE, FALSE, 0);
        }

        rndir_button = gui_stock_label_button(NULL, GTK_STOCK_EDIT);
        GTK_WIDGET_UNSET_FLAGS(rndir_button, GTK_CAN_FOCUS);
        gtk_button_set_relief (GTK_BUTTON (rndir_button), GTK_RELIEF_NONE); 
        gtk_widget_show (rndir_button);
        g_signal_connect(rndir_button, "clicked", G_CALLBACK(aifp_rename_item_cb), NULL);
        gtk_box_pack_start (GTK_BOX (vbox4), rndir_button, FALSE, FALSE, 0);

        rmdir_button = gui_stock_label_button(NULL, GTK_STOCK_DELETE);
        GTK_WIDGET_UNSET_FLAGS(rmdir_button, GTK_CAN_FOCUS);
        gtk_button_set_relief (GTK_BUTTON (rmdir_button), GTK_RELIEF_NONE); 
        gtk_widget_show (rmdir_button);
        g_signal_connect(rmdir_button, "clicked", G_CALLBACK(aifp_remove_item_cb), NULL);
        gtk_box_pack_start (GTK_BOX (vbox4), rmdir_button, FALSE, FALSE, 0);

        if (options.enable_tooltips) {
                if (transfer_mode == UPLOAD_MODE) {
                        gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), mkdir_button, _("Create a new directory"), NULL);
                }
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), rndir_button, _("Rename"), NULL);
                gtk_tooltips_set_tip (GTK_TOOLTIPS (aqualung_tooltips), rmdir_button, _("Remove"), NULL);
        }

        label = gtk_label_new (_("<b>Remote directory</b>"));
        gtk_widget_show (label);
        gtk_frame_set_label_widget (GTK_FRAME (frame), label);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

        frame = gtk_frame_new (NULL);
        gtk_widget_show (frame);
        gtk_box_pack_start (GTK_BOX (vbox2), frame, FALSE, FALSE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

        if (transfer_mode == DOWNLOAD_MODE) {

                label = gtk_label_new (_("<b>Local directory</b>"));
                gtk_widget_show (label);
                gtk_frame_set_label_widget (GTK_FRAME (frame), label);
                gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

                frame = gtk_frame_new (NULL);
                gtk_widget_show (frame);
                gtk_box_pack_start (GTK_BOX (vbox2), frame, FALSE, FALSE, 0);
                gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
                gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

                alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
                gtk_widget_show (alignment);
                gtk_container_add (GTK_CONTAINER (frame), alignment);
                gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);

                hbox2 = gtk_hbox_new(FALSE, FALSE);
                gtk_container_add (GTK_CONTAINER (alignment), hbox2);
                gtk_widget_show (hbox2);

                local_path_entry = gtk_entry_new();
                gtk_widget_show (local_path_entry);
                GTK_WIDGET_UNSET_FLAGS(local_path_entry, GTK_CAN_FOCUS);
                gtk_box_pack_start(GTK_BOX(hbox2), local_path_entry, TRUE, TRUE, 2);
                gtk_editable_set_editable (GTK_EDITABLE(local_path_entry), FALSE);
                strncpy(dest_dir, options.home, MAXLEN-1);
                gtk_entry_set_text(GTK_ENTRY(local_path_entry), dest_dir);

                local_path_browse_button = gui_stock_label_button(_("Browse"), GTK_STOCK_OPEN);
                GTK_WIDGET_UNSET_FLAGS(local_path_browse_button, GTK_CAN_FOCUS);
                gtk_widget_show (local_path_browse_button);
                gtk_container_set_border_width(GTK_CONTAINER(local_path_browse_button), 2);
                g_signal_connect (G_OBJECT(local_path_browse_button), "clicked",
                                  G_CALLBACK(local_path_selected_cb), (gpointer)local_path_entry);
                gtk_box_pack_end(GTK_BOX(hbox2), local_path_browse_button, FALSE, FALSE, 0);
        }

        frame = gtk_frame_new (NULL);
        gtk_widget_show (frame);
        gtk_box_pack_start (GTK_BOX (vbox2), frame, FALSE, FALSE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

        alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
        gtk_widget_show (alignment);
        gtk_container_add (GTK_CONTAINER (frame), alignment);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);

        table = gtk_table_new (3, 2, FALSE);
        gtk_widget_show (table);
        gtk_container_add (GTK_CONTAINER (alignment), table);
        gtk_container_set_border_width (GTK_CONTAINER (table), 8);
        gtk_table_set_row_spacings (GTK_TABLE (table), 4);
        gtk_table_set_col_spacings (GTK_TABLE (table), 4);

        label = gtk_label_new (_("File name: "));
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

        label = gtk_label_new (_("Current file: "));
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

        label = gtk_label_new (_("Overall: "));
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
                          (GtkAttachOptions) (GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

        aifp_file_entry = gtk_entry_new();
        GTK_WIDGET_UNSET_FLAGS(aifp_file_entry, GTK_CAN_FOCUS);
        gtk_widget_show (aifp_file_entry);
        gtk_editable_set_editable(GTK_EDITABLE(aifp_file_entry), FALSE);
        gtk_table_attach (GTK_TABLE (table), aifp_file_entry, 1, 2, 0, 1,
                          (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);
        gtk_entry_set_text(GTK_ENTRY(aifp_file_entry), _("None"));

        progressbar_cf = gtk_progress_bar_new ();
        gtk_widget_show (progressbar_cf);
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_cf), _("Idle"));
        gtk_table_attach (GTK_TABLE (table), progressbar_cf, 1, 2, 1, 2,
                          (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);

        progressbar_op = gtk_progress_bar_new ();
        gtk_widget_show (progressbar_op);
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progressbar_op), _("Idle"));
        gtk_table_attach (GTK_TABLE (table), progressbar_op, 1, 2, 2, 3,
                          (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                          (GtkAttachOptions) (0), 0, 0);

        label = gtk_label_new (_("<b>Transfer progress</b>"));
        gtk_widget_show (label);
        gtk_frame_set_label_widget (GTK_FRAME (frame), label);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

        aifp_close_when_ready_check = gtk_check_button_new_with_label(_("Close window when transfer complete"));
        GTK_WIDGET_UNSET_FLAGS(aifp_close_when_ready_check, GTK_CAN_FOCUS);
        gtk_widget_set_name(aifp_close_when_ready_check, "check_on_window");
        gtk_widget_show(aifp_close_when_ready_check);
        gtk_box_pack_start(GTK_BOX(vbox2), aifp_close_when_ready_check, FALSE, TRUE, 0);

        hseparator = gtk_hseparator_new ();
        gtk_widget_show (hseparator);
        gtk_box_pack_start (GTK_BOX (vbox1), hseparator, FALSE, TRUE, 3);

        hbuttonbox = gtk_hbutton_box_new ();
        gtk_widget_show (hbuttonbox);
        gtk_box_pack_start (GTK_BOX (vbox1), hbuttonbox, FALSE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (hbuttonbox), 5);
        gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_END);
        gtk_box_set_spacing (GTK_BOX (hbuttonbox), 6);

        if (transfer_mode == UPLOAD_MODE) {
                upload_download_button = gui_stock_label_button (_("_Upload"), GTK_STOCK_GO_UP);
        } else {
                upload_download_button = gui_stock_label_button (_("_Download"), GTK_STOCK_GO_DOWN);
        }
        GTK_WIDGET_UNSET_FLAGS(upload_download_button, GTK_CAN_FOCUS);
        gtk_widget_show (upload_download_button);
        g_signal_connect(upload_download_button, "clicked", G_CALLBACK(upload_download_songs_cb), NULL);
        gtk_container_add (GTK_CONTAINER (hbuttonbox), upload_download_button);
        GTK_WIDGET_SET_FLAGS (upload_download_button, GTK_CAN_DEFAULT);

        abort_button = gui_stock_label_button (_("_Abort"), GTK_STOCK_CANCEL);
        GTK_WIDGET_UNSET_FLAGS(abort_button, GTK_CAN_FOCUS);
        gtk_widget_show (abort_button);
        g_signal_connect(abort_button, "clicked", G_CALLBACK(abort_transfer_cb), NULL);
        gtk_container_add (GTK_CONTAINER (hbuttonbox), abort_button);
        GTK_WIDGET_SET_FLAGS (abort_button, GTK_CAN_DEFAULT);

        close_button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
        GTK_WIDGET_UNSET_FLAGS(close_button, GTK_CAN_FOCUS);
        gtk_widget_show (close_button);
        g_signal_connect(close_button, "clicked", G_CALLBACK(aifp_window_close), NULL);
        gtk_container_add (GTK_CONTAINER (hbuttonbox), close_button);
        GTK_WIDGET_SET_FLAGS (close_button, GTK_CAN_DEFAULT);     

        gtk_widget_set_sensitive(abort_button, FALSE);
        gtk_widget_grab_focus(list);

        aifp_update_info();
        aifp_directory_listing (NULL);
        aifp_check_size();

        abort_pressed = 0;
        remote_type = TYPE_DIR;

        gtk_widget_show(aifp_window);
}

#endif /* HAVE_IFP */

// vim: shiftwidth=8:tabstop=8:softtabstop=8:

