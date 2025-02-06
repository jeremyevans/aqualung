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
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

#ifdef HAVE_CDDA
#include "store_cdda.h"
#endif /* HAVE_CDDA */

#ifdef HAVE_PODCAST
#include "store_podcast.h"
#endif /* HAVE_PODCAST */

#include "common.h"
#include "utils_gui.h"
#include "gui_main.h"
#include "options.h"
#include "playlist.h"
#include "search.h"
#include "i18n.h"
#include "store_file.h"
#include "music_browser.h"


extern options_t options;

extern GtkWidget * musicstore_toggle;

extern GtkListStore * ms_pathlist_store;

extern PangoFontDescription *fd_browser;
extern PangoFontDescription *fd_statusbar;

GtkWidget * browser_window;

int music_store_changed = 0;

GtkWidget * music_tree;
GtkTreeStore * music_store = NULL;
GtkTreeSelection * music_select;

GtkWidget * comment_view;
GtkWidget * browser_paned;
GtkWidget * statusbar_ms;

GtkWidget * toolbar_save_button;
GtkWidget * toolbar_edit_button;
GtkWidget * toolbar_add_button;
GtkWidget * toolbar_remove_button;

GtkWidget * blank_menu;
GtkWidget * blank__add;
GtkWidget * blank__search;
GtkWidget * blank__save;

GdkPixbuf * icon_store;

gboolean music_tree_event_cb(GtkWidget * widget, GdkEvent * event, gpointer user_data);

void toolbar__collapse_cb(gpointer data);
void toolbar__search_cb(gpointer data);


gboolean
browser_window_close(GtkWidget * widget, GdkEvent * event, gpointer data) {

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(musicstore_toggle), FALSE);

	return TRUE;
}

gint
browser_window_key_pressed(GtkWidget * widget, GdkEventKey * event) {

	switch (event->keyval) {
	case GDK_KEY_q:
	case GDK_KEY_Q:
	case GDK_KEY_Escape:
		browser_window_close(NULL, NULL, NULL);
		return TRUE;
	}

	return FALSE;
}


int
path_get_store_type(GtkTreePath * p) {

	GtkTreeIter iter;
        GtkTreePath * path;
	store_t * data;

	path = gtk_tree_path_copy(p);

        while (gtk_tree_path_get_depth(path) > 1) {
                gtk_tree_path_up(path);
        }

        gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &data, -1);
	gtk_tree_path_free(path);

	return data->type;
}

int
iter_get_store_type(GtkTreeIter * i) {

	GtkTreePath * path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), i);
	int ret = path_get_store_type(path);
	gtk_tree_path_free(path);
        return ret;
}


int
music_store_iter_is_track(GtkTreeIter * iter) {

	switch (iter_get_store_type(iter)) {
	case STORE_TYPE_FILE:
		return store_file_iter_is_track(iter);
#ifdef HAVE_CDDA
	case STORE_TYPE_CDDA:
		return store_cdda_iter_is_track(iter);
#endif /* HAVE_CDDA */

#ifdef HAVE_PODCAST
	case STORE_TYPE_PODCAST:
		return store_podcast_iter_is_track(iter);
#endif /* HAVE_PODCAST */
	}

	return 0;
}

void
music_store_iter_addlist_defmode(GtkTreeIter * ms_iter, GtkTreeIter * pl_iter, int new_tab) {

	switch (iter_get_store_type(ms_iter)) {
	case STORE_TYPE_FILE:
		store_file_iter_addlist_defmode(ms_iter, pl_iter, new_tab);
		break;
#ifdef HAVE_CDDA
	case STORE_TYPE_CDDA:
		store_cdda_iter_addlist_defmode(ms_iter, pl_iter, new_tab);
		break;
#endif /* HAVE_CDDA */

#ifdef HAVE_PODCAST
	case STORE_TYPE_PODCAST:
		store_podcast_iter_addlist_defmode(ms_iter, pl_iter, new_tab);
		break;
#endif /* HAVE_PODCAST */
	}
}

gboolean
music_tree_event_cb(GtkWidget * widget, GdkEvent * event, gpointer user_data) {

	GtkTreeIter iter;
	GtkTreeModel * model;

	if (event->type != GDK_KEY_PRESS &&
	    event->type != GDK_KEY_RELEASE &&
	    event->type != GDK_BUTTON_PRESS &&
	    event->type != GDK_2BUTTON_PRESS) {
		return FALSE;
	}

	/* global handlers */

	if (event->type == GDK_2BUTTON_PRESS) {
		if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {
			music_store_iter_addlist_defmode(&iter, NULL, 0/*new_tab*/);
		}
		return TRUE;
	}

	if (event->type == GDK_KEY_PRESS) {

		GdkEventKey * kevent = (GdkEventKey *)event;

		switch (kevent->keyval) {
		case GDK_KEY_KP_Enter:
		case GDK_KEY_Return:
			if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

				GtkTreePath * path = gtk_tree_model_get_path(model, &iter);

				if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(music_tree), path)) {
					gtk_tree_view_collapse_row(GTK_TREE_VIEW(music_tree), path);
				} else {
					gtk_tree_view_expand_row(GTK_TREE_VIEW(music_tree), path, FALSE);
				}
				gtk_tree_path_free(path);
			}
			return TRUE;
		case GDK_KEY_w:
		case GDK_KEY_W:
			toolbar__collapse_cb(NULL);
			return TRUE;
		case GDK_KEY_f:
		case GDK_KEY_F:
			toolbar__search_cb(NULL);
			return TRUE;
		}
	}

	if (event->type == GDK_BUTTON_PRESS) {

		GtkTreePath * path = NULL;
		GdkEventButton * bevent = (GdkEventButton *)event;

		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(music_tree), bevent->x, bevent->y,
						  &path, NULL, NULL, NULL)) {
			gtk_tree_selection_select_path(music_select, path);
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(music_tree), path, NULL, FALSE);
			gtk_tree_path_free(path);

			if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {
				if (bevent->button == 2) {
					music_store_iter_addlist_defmode(&iter, NULL, 1/*new_tab*/);
					return TRUE;
				}
			}

		} else {
			if (bevent->button == 3) {
				gtk_menu_popup(GTK_MENU(blank_menu), NULL, NULL, NULL, NULL,
					       bevent->button, bevent->time);
			}
			return TRUE;
		}
	}

	/* pass event to the selected store */

	if (gtk_tree_selection_get_selected(music_select, &model, &iter)) {

		GtkTreePath * path = gtk_tree_model_get_path(model, &iter);

		switch (iter_get_store_type(&iter)) {
		case STORE_TYPE_FILE:
			store_file_event_cb(event, &iter, path);
			break;
#ifdef HAVE_CDDA
		case STORE_TYPE_CDDA:
			store_cdda_event_cb(event, &iter, path);
			break;
#endif /* HAVE_CDDA */

#ifdef HAVE_PODCAST
		case STORE_TYPE_PODCAST:
			store_podcast_event_cb(event, &iter, path);
			break;
#endif /* HAVE_PODCAST */
		}

		gtk_tree_path_free(path);
	}

	return FALSE;
}


/****************************************/

void
music_store_search(void) {

	search_dialog();
}


void
toolbar__search_cb(gpointer data) {

	music_store_search();
}

void
toolbar__collapse_cb(gpointer data) {

        GtkTreeIter iter;
        GtkTreePath * path;

        gtk_tree_view_collapse_all(GTK_TREE_VIEW(music_tree));

        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter, NULL, 0);
        path = gtk_tree_model_get_path (GTK_TREE_MODEL(music_store), &iter);
        if (path) {
                gtk_tree_view_set_cursor (GTK_TREE_VIEW (music_tree), path, NULL, TRUE);
                gtk_tree_path_free(path);
        }
}

void
toolbar__edit_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		switch (iter_get_store_type(&iter)) {
		case STORE_TYPE_FILE:
			store_file_toolbar__edit_cb(data);
			break;
#ifdef HAVE_PODCAST
		case STORE_TYPE_PODCAST:
			store_podcast_toolbar__edit_cb(data);
			break;
#endif /* HAVE_PODCAST */
		}
	}
}

void
toolbar__add_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		switch (iter_get_store_type(&iter)) {
		case STORE_TYPE_FILE:
			store_file_toolbar__add_cb(data);
			break;
#ifdef HAVE_PODCAST
		case STORE_TYPE_PODCAST:
			store_podcast_toolbar__add_cb(data);
			break;
#endif /* HAVE_PODCAST */
		}
	}
}

void
toolbar__remove_cb(gpointer data) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter)) {

		switch (iter_get_store_type(&iter)) {
		case STORE_TYPE_FILE:
			store_file_toolbar__remove_cb(data);
			break;
#ifdef HAVE_PODCAST
		case STORE_TYPE_PODCAST:
			store_podcast_toolbar__remove_cb(data);
			break;
#endif /* HAVE_PODCAST */
		}
	}
}

void
toolbar__save_cb(gpointer data) {

	music_store_save_all();
}

void
music_store_selection_changed(int store_type) {

	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(music_select, NULL, &iter) &&
	    (store_type == STORE_TYPE_ALL || iter_get_store_type(&iter) == store_type)) {

		gtk_tree_selection_unselect_iter(music_select, &iter);
		gtk_tree_selection_select_iter(music_select, &iter);
	}
}

void
tree_selection_changed_cb(GtkTreeSelection * selection, gpointer data) {

	GtkTreeIter iter;
	GtkTreeModel * model;
	GtkTextBuffer * buffer;
	GtkTextIter a_iter, b_iter;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(comment_view));
        gtk_text_buffer_get_bounds(buffer, &a_iter, &b_iter);
	gtk_text_buffer_delete(buffer, &a_iter, &b_iter);

	gtk_label_set_text(GTK_LABEL(statusbar_ms), "");

	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

		switch (iter_get_store_type(&iter)) {
		case STORE_TYPE_FILE:
			if (options.enable_mstore_toolbar) {
				store_file_set_toolbar_sensitivity(&iter,
								   toolbar_edit_button,
								   toolbar_add_button,
								   toolbar_remove_button);
			}
			store_file_selection_changed(&iter,
						     buffer,
						     GTK_LABEL(statusbar_ms));
			break;
#ifdef HAVE_CDDA
		case STORE_TYPE_CDDA:
			if (options.enable_mstore_toolbar) {
				gtk_widget_set_sensitive(toolbar_edit_button, FALSE);
				gtk_widget_set_sensitive(toolbar_add_button, FALSE);
				gtk_widget_set_sensitive(toolbar_remove_button, FALSE);
			}
			store_cdda_selection_changed(&iter,
						     buffer,
						     GTK_LABEL(statusbar_ms));
			break;
#endif /* HAVE_CDDA */

#ifdef HAVE_PODCAST
		case STORE_TYPE_PODCAST:
			if (options.enable_mstore_toolbar) {
				store_podcast_set_toolbar_sensitivity(&iter,
								      toolbar_edit_button,
								      toolbar_add_button,
								      toolbar_remove_button);
			}
			store_podcast_selection_changed(&iter,
							buffer,
							GTK_LABEL(statusbar_ms));
			break;
#endif /* HAVE_PODCAST */
		}
	}
}

void
browser_drag_data_get(GtkWidget * widget, GdkDragContext * drag_context,
		      GtkSelectionData * data, guint info, guint time, gpointer user_data) {

	gtk_selection_data_set(data, gtk_selection_data_get_target(data), 8,
			       (const guchar *) "store\0", 6);
}


void
browser_drag_end(GtkWidget * widget, GdkDragContext * drag_context, gpointer data) {

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
	GtkWidget * toolbar_search_button;
	GtkWidget * toolbar_collapse_all_button;

	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;

	GdkPixbuf * pixbuf;
	char path[MAXLEN];

	GtkTargetEntry target_table[] = {
		{ "aqualung-browser-list", GTK_TARGET_SAME_APP, 1 }
	};


	/* window creating stuff */
	browser_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(browser_window), _("Music Store"));
	g_signal_connect(G_OBJECT(browser_window), "delete_event", G_CALLBACK(browser_window_close), NULL);
	g_signal_connect(G_OBJECT(browser_window), "key_press_event",
			 G_CALLBACK(browser_window_key_pressed), NULL);
	gtk_container_set_border_width(GTK_CONTAINER(browser_window), 2);
        gtk_widget_set_size_request(browser_window, 200, 300);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(browser_window), vbox);

        if (options.enable_mstore_toolbar) {

                hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 3);

                toolbar_search_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_FIND);
		gtk_widget_set_can_focus(toolbar_search_button, FALSE);
                g_signal_connect(G_OBJECT(toolbar_search_button), "clicked", G_CALLBACK(toolbar__search_cb), NULL);
                gtk_box_pack_start(GTK_BOX(hbox), toolbar_search_button, FALSE, TRUE, 3);
                aqualung_widget_set_tooltip_text(toolbar_search_button, _("Search..."));

                toolbar_collapse_all_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_REFRESH);
		gtk_widget_set_can_focus(toolbar_collapse_all_button, FALSE);
                gtk_box_pack_start(GTK_BOX(hbox), toolbar_collapse_all_button, FALSE, TRUE, 3);
                g_signal_connect(G_OBJECT(toolbar_collapse_all_button), "pressed", G_CALLBACK(toolbar__collapse_cb), NULL);
                aqualung_widget_set_tooltip_text(toolbar_collapse_all_button, _("Collapse all items"));

		toolbar_edit_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_EDIT);
		gtk_widget_set_can_focus(toolbar_edit_button, FALSE);
		gtk_box_pack_start(GTK_BOX(hbox), toolbar_edit_button, FALSE, TRUE, 3);
		g_signal_connect(G_OBJECT(toolbar_edit_button), "pressed", G_CALLBACK(toolbar__edit_cb), NULL);
		aqualung_widget_set_tooltip_text(toolbar_edit_button, _("Edit item..."));

		toolbar_add_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_ADD);
		gtk_widget_set_can_focus(toolbar_add_button, FALSE);
		gtk_box_pack_start(GTK_BOX(hbox), toolbar_add_button, FALSE, TRUE, 3);
		g_signal_connect(G_OBJECT(toolbar_add_button), "pressed", G_CALLBACK(toolbar__add_cb), NULL);
		aqualung_widget_set_tooltip_text(toolbar_add_button, _("Add item..."));

		toolbar_remove_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_REMOVE);
		gtk_widget_set_can_focus(toolbar_remove_button, FALSE);
		gtk_box_pack_start(GTK_BOX(hbox), toolbar_remove_button, FALSE, TRUE, 3);
		g_signal_connect(G_OBJECT(toolbar_remove_button), "pressed", G_CALLBACK(toolbar__remove_cb), NULL);
		aqualung_widget_set_tooltip_text(toolbar_remove_button, _("Remove item..."));

		toolbar_save_button = gui_stock_label_button((gchar *)-1, GTK_STOCK_SAVE);
		gtk_widget_set_can_focus(toolbar_save_button, FALSE);
		gtk_box_pack_start(GTK_BOX(hbox), toolbar_save_button, FALSE, TRUE, 3);
		g_signal_connect(G_OBJECT(toolbar_save_button), "pressed", G_CALLBACK(toolbar__save_cb), NULL);
		gtk_widget_set_sensitive(toolbar_save_button, FALSE);
		aqualung_widget_set_tooltip_text(toolbar_save_button, _("Save all stores"));
        }


	if (!options.hide_comment_pane) {
		browser_paned = gtk_vpaned_new();
		gtk_box_pack_start(GTK_BOX(vbox), browser_paned, TRUE, TRUE, 0);
	}

	/* load tree icons */
        if (options.enable_ms_tree_icons) {
		arr_snprintf(path, "%s/%s", AQUALUNG_DATADIR, "ms-store.png");
		icon_store = gdk_pixbuf_new_from_file (path, NULL);
		store_file_load_icons();
#ifdef HAVE_CDDA
		store_cdda_load_icons();
#endif /* HAVE_CDDA */

#ifdef HAVE_PODCAST
		store_podcast_load_icons();
#endif /* HAVE_PODCAST */
        }

	/* create music store tree */
	if (!music_store) {
		music_store = gtk_tree_store_new(MS_COL_COUNT,
						 G_TYPE_STRING,   /* visible name */
						 G_TYPE_STRING,   /* string to sort by */
                			         G_TYPE_INT,      /* font weight */
                                                 GDK_TYPE_PIXBUF, /* icon */
						 G_TYPE_POINTER); /* data */
	}

	music_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(music_store));
	gtk_widget_set_name(music_tree, "music_tree");
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(music_tree), FALSE);

	g_signal_connect(G_OBJECT(music_tree), "event", G_CALLBACK(music_tree_event_cb), NULL);

        if (options.enable_ms_rules_hint) {
                gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(music_tree), TRUE);
        }

        column = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(column, "name");
        renderer = gtk_cell_renderer_pixbuf_new();
        gtk_tree_view_column_pack_start(column, renderer, FALSE);
        gtk_tree_view_column_add_attribute(column, renderer, "pixbuf", MS_COL_ICON);
        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(column, renderer, TRUE);
        gtk_tree_view_column_add_attribute(column, renderer, "text", MS_COL_NAME);
        gtk_tree_view_column_add_attribute(column, renderer, "weight", MS_COL_FONT);
        g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

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

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(music_store), MS_COL_SORT, GTK_SORT_ASCENDING);

	music_tree_expand_stores();

	/* setup drag and drop */
	gtk_drag_source_set(music_tree,
			    GDK_BUTTON1_MASK,
			    target_table,
			    sizeof(target_table) / sizeof(GtkTargetEntry),
			    GDK_ACTION_COPY);

	arr_snprintf(path, "%s/drag.png", AQUALUNG_DATADIR);
	if ((pixbuf = gdk_pixbuf_new_from_file(path, NULL)) != NULL) {
		gtk_drag_source_set_icon_pixbuf(music_tree, pixbuf);
	}

	g_signal_connect(G_OBJECT(music_tree), "drag_data_get", G_CALLBACK(browser_drag_data_get), NULL);
	g_signal_connect(G_OBJECT(music_tree), "drag_end", G_CALLBACK(browser_drag_end), NULL);


        /* create popup menu for blank space */
        blank_menu = gtk_menu_new();
	register_toplevel_window(blank_menu, TOP_WIN_SKIN);

        blank__add = gtk_menu_item_new_with_label(_("Create empty store..."));
        blank__search = gtk_menu_item_new_with_label(_("Search..."));
        blank__save = gtk_menu_item_new_with_label(_("Save all stores"));
        gtk_menu_shell_append(GTK_MENU_SHELL(blank_menu), blank__add);
        gtk_menu_shell_append(GTK_MENU_SHELL(blank_menu), blank__search);
        gtk_menu_shell_append(GTK_MENU_SHELL(blank_menu), blank__save);
        g_signal_connect_swapped(G_OBJECT(blank__add), "activate", G_CALLBACK(store__add_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(blank__search), "activate", G_CALLBACK(toolbar__search_cb), NULL);
        g_signal_connect_swapped(G_OBJECT(blank__save), "activate", G_CALLBACK(toolbar__save_cb), NULL);
        gtk_widget_show(blank__add);
        gtk_widget_show(blank__search);
        gtk_widget_show(blank__save);

	store_file_create_popup_menu();

#ifdef HAVE_CDDA
	store_cdda_create_popup_menu();
#endif /* HAVE_CDDA */

#ifdef HAVE_PODCAST
	store_podcast_create_popup_menu();
#endif /* HAVE_PODCAST */

	/* create text widget for comments */
	comment_view = gtk_text_view_new();
	gtk_widget_set_name(comment_view, "comment_view");
	gtk_text_view_set_editable(GTK_TEXT_VIEW(comment_view), FALSE);
	gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(comment_view), 3);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(comment_view), 3);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(comment_view), GTK_WRAP_WORD);

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
		gtk_paned_set_position(GTK_PANED(browser_paned), options.browser_paned_pos);
	}

	store_file_insert_progress_bar(vbox);

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

		statusbar_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_container_set_border_width(GTK_CONTAINER(statusbar_hbox), 1);
		gtk_container_add(GTK_CONTAINER(statusbar_viewport), statusbar_hbox);

		statusbar_ms = gtk_label_new("");
		gtk_widget_set_name(statusbar_ms, "label_info");
		gtk_box_pack_end(GTK_BOX(statusbar_hbox), statusbar_ms, FALSE, FALSE, 0);
	}
}


void
show_music_browser(void) {

	options.browser_on = 1;
	gtk_window_move(GTK_WINDOW(browser_window), options.browser_pos_x, options.browser_pos_y);
	gtk_window_resize(GTK_WINDOW(browser_window), options.browser_size_x, options.browser_size_y);
	gtk_widget_show_all(browser_window);
	register_toplevel_window(browser_window, TOP_WIN_SKIN | TOP_WIN_TRAY);
	if (!options.hide_comment_pane) {
		gtk_paned_set_position(GTK_PANED(browser_paned), options.browser_paned_pos);
	}
}

void
hide_music_browser(void) {

	options.browser_on = 0;
	gtk_window_get_position(GTK_WINDOW(browser_window), &options.browser_pos_x, &options.browser_pos_y);
	gtk_window_get_size(GTK_WINDOW(browser_window), &options.browser_size_x, &options.browser_size_y);
	if (!options.hide_comment_pane) {
		options.browser_paned_pos = gtk_paned_get_position(GTK_PANED(browser_paned));
	}
	gtk_widget_hide(browser_window);
	register_toplevel_window(browser_window, TOP_WIN_SKIN);
}


void
music_store_mark_changed(GtkTreeIter * iter) {

	GtkTreeIter iter_store;
	GtkTreePath * path;
	char name[MAXLEN];
	char * pname;
	store_t * data;

	music_store_selection_changed(STORE_TYPE_FILE);

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(music_store), iter);

        while (gtk_tree_path_get_depth(path) > 1) {
                gtk_tree_path_up(path);
        }

	gtk_tree_model_get_iter(GTK_TREE_MODEL(music_store), &iter_store, path);
	gtk_tree_path_free(path);

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_store, MS_COL_DATA, &data, -1);

	switch (data->type) {
	case STORE_TYPE_FILE:
		{
			store_data_t * store_data = (store_data_t *)data;
			if (store_data->dirty) {
				return;
			}
			store_data->dirty = 1;
		}
		break;
	default:
		/* skip */
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_store, MS_COL_NAME, &pname, -1);

	name[0] = '*';
	name[1] = '\0';
	arr_strlcat(name, pname);
	g_free(pname);

	gtk_tree_store_set(music_store, &iter_store, MS_COL_NAME, name, -1);

	music_store_changed = 1;
	gtk_window_set_title(GTK_WINDOW(browser_window), _("*Music Store"));
	if (options.enable_mstore_toolbar) {
		gtk_widget_set_sensitive(toolbar_save_button, TRUE);
	}
}

void
music_store_mark_saved(GtkTreeIter * iter_store) {

	GtkTreeIter iter;
	int i;
	char * pname;
	store_t * data;

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_store, MS_COL_DATA, &data, -1);

	switch (data->type) {
	case STORE_TYPE_FILE:
		{
			store_data_t * store_data = (store_data_t *)data;
			if (!store_data->dirty) {
				return;
			}
			store_data->dirty = 0;
		}
		break;
	default:
		/* skip */
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(music_store), iter_store, MS_COL_NAME, &pname, -1);
	gtk_tree_store_set(music_store, iter_store, MS_COL_NAME, pname + 1, -1);
	g_free(pname);

	i = 0;
        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter, NULL, i++)) {
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter, MS_COL_DATA, &data, -1);

		switch (data->type) {
		case STORE_TYPE_FILE:
			{
				store_data_t * store_data = (store_data_t *)data;
				if (store_data->dirty) {
					return;
				}
			}
			break;
		default:
			/* skip */
			break;
		}
	}

	music_store_changed = 0;
	gtk_window_set_title(GTK_WINDOW(browser_window), _("Music Store"));
	if (options.enable_mstore_toolbar) {
		gtk_widget_set_sensitive(toolbar_save_button, FALSE);
	}

	music_store_selection_changed(STORE_TYPE_FILE);
}


int
music_store_get_type(char * filename) {

	/* dummy implementation */
	return STORE_TYPE_FILE;
}


void
music_store_load_all(void) {

	GtkTreeIter iter_store;
	char * store_file;
	char sort[16];
	int i = 0;

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ms_pathlist_store),
					     &iter_store, NULL, i++)) {

                gtk_tree_model_get(GTK_TREE_MODEL(ms_pathlist_store), &iter_store,
				   0, &store_file, -1);

		switch (music_store_get_type(store_file)) {
		case STORE_TYPE_FILE:
			arr_snprintf(sort, "%03d", i+1);
			store_file_load(store_file, sort);
			break;
		}

		g_free(store_file);
        }

	music_tree_expand_stores();
}

void
music_store_save_all(void) {

	GtkTreeIter iter_store;
	int i = 0;

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store),
					     &iter_store, NULL, i++)) {

		switch (iter_get_store_type(&iter_store)) {
		case STORE_TYPE_FILE:
			store_file_save(&iter_store);
			break;
		}
	}
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :

