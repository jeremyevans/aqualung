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


#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <jack/jack.h>

#include "common.h"
#include "ports.h"


#define LIST_WIDTH 200
#define LIST_HEIGHT 100


extern GtkWidget * main_window;

/* JACK data */
extern jack_port_t * out_L_port;
extern jack_port_t * out_R_port;
extern jack_client_t * jack_client;

GtkWidget * ports_window = NULL;
GtkWidget * nb_outs;
GtkWidget * nb_out_labels[MAX_JACK_CLIENTS];

GtkWidget * vbox_dl; /* down-left */
GtkWidget * vbox_dr; /* down-right */

GtkWidget * tree_out_L;
GtkWidget * tree_out_R;
GtkListStore * store_out_L;
GtkListStore * store_out_R;
GtkTreeViewColumn * column_out_L;
GtkTreeViewColumn * column_out_R;

int n_clients;
GtkListStore * store_out_nb[MAX_JACK_CLIENTS];

gint timeout_tag;

int out_selector = 0;


void scan_connections(jack_port_t * port, GtkListStore * store);
void setup_notebook_out(void);



gint
ports_timeout_callback(gpointer data) {

	switch((int)data) {
	case 1:
		gtk_list_store_clear(store_out_L);
		scan_connections(out_L_port, store_out_L);
		break;
	case 2:
		gtk_list_store_clear(store_out_R);
		scan_connections(out_R_port, store_out_R);
		break;
	}
	return 0;
}


int
port_window_close(GtkWidget *widget, gpointer * data) {

	ports_window = NULL;
	return 0;
}


void
clicked_rescan(GtkWidget * widget, gpointer * data) {

	gtk_list_store_clear(store_out_L);
	scan_connections(out_L_port, store_out_L);
	gtk_list_store_clear(store_out_R);
	scan_connections(out_R_port, store_out_R);

	/* re-build notebook */
	gtk_widget_destroy(nb_outs);
	n_clients = 0;

	nb_outs = gtk_notebook_new();
        gtk_box_pack_start(GTK_BOX(vbox_dr), nb_outs, TRUE, TRUE, 2);
	setup_notebook_out();
        gtk_widget_show(nb_outs);
}


void
ports_clicked_close(GtkWidget * widget, gpointer * data) {

	gtk_widget_destroy(ports_window);
	ports_window = NULL;
}


void
set_active(GtkWidget * widget, int sel) {

	GdkColor color_normal;
	GdkColor color_active;
	GdkColor color_prelight;

	if (sel == 0) {
		color_normal.red = 40000;
		color_normal.green = 40000;
		color_normal.blue = 40000;

		color_active.red = 30000;
		color_active.green = 30000;
		color_active.blue = 30000;

		color_prelight.red = 50000;
		color_prelight.green = 50000;
		color_prelight.blue = 50000;

	} else {
		color_normal.red = 40000;
		color_normal.green = 40000;
		color_normal.blue = 65535;

		color_active.red = 30000;
		color_active.green = 30000;
		color_active.blue = 45000;

		color_prelight.red = 50000;
		color_prelight.green = 50000;
		color_prelight.blue = 65535;
	}
	gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &color_normal);
	gtk_widget_modify_bg(widget, GTK_STATE_ACTIVE, &color_active);
	gtk_widget_modify_bg(widget, GTK_STATE_PRELIGHT, &color_prelight);
}


void
clicked_out_L_header(GtkWidget * widget, gpointer * data) {

	out_selector = 0;
	set_active(GTK_WIDGET(column_out_L->button), 1);
	set_active(GTK_WIDGET(column_out_R->button), 0);
}


void
clicked_out_R_header(GtkWidget * widget, gpointer * data) {

	out_selector = 1;
	set_active(GTK_WIDGET(column_out_L->button), 0);
	set_active(GTK_WIDGET(column_out_R->button), 1);
}


void
tree_out_nb_selection_changed(GtkObject * tree, gpointer * data) {

	GtkTreeIter iter;
        GtkTreeModel * model;
	GtkTreeSelection * selection;
        gchar * str;
	const gchar * label;
	char fullname[MAXLEN];

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

                gtk_tree_model_get(model, &iter, 0, &str, -1);
		label = gtk_label_get_text(GTK_LABEL(nb_out_labels[(int)data]));
		sprintf(fullname, "%s:%s", label, str);
		g_free(str);

		if (out_selector == 0) {
			if (jack_connect(jack_client, jack_port_name(out_L_port), fullname)) {
				fprintf(stderr, "Cannot connect %s to out_L. "
					"These ports are probably already connected.\n", fullname);
			} else {
				gtk_list_store_clear(store_out_L);
				scan_connections(out_L_port, store_out_L);
				out_selector = 1;
				set_active(GTK_WIDGET(column_out_L->button), 0);
				set_active(GTK_WIDGET(column_out_R->button), 1);
			}
		} else {
			if (jack_connect(jack_client, jack_port_name(out_R_port), fullname)) {
				fprintf(stderr, "Cannot connect %s to out_R. "
					"These ports are probably already connected.\n", fullname);
			} else {
				gtk_list_store_clear(store_out_R);
				scan_connections(out_R_port, store_out_R);
				out_selector = 0;
				set_active(GTK_WIDGET(column_out_L->button), 1);
				set_active(GTK_WIDGET(column_out_R->button), 0);
			}
		}
        }
}


void
tree_out_L_selection_changed(GtkTreeSelection * selection, gpointer * data) {

	GtkTreeIter iter;
        GtkTreeModel * model;
        gchar * str;
	int res;

        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

                gtk_tree_model_get(model, &iter, 0, &str, -1);
		if ((res = jack_disconnect(jack_client, jack_port_name(out_L_port), str)) != 0) {
			fprintf(stderr, "ERROR: jack_disconnect() returned %d\n", res);
		}
		g_free(str);
		timeout_tag = gtk_timeout_add(100, ports_timeout_callback, (gpointer)1);
        }
}


void
tree_out_R_selection_changed(GtkTreeSelection *selection, gpointer * data) {

	GtkTreeIter iter;
        GtkTreeModel * model;
        gchar * str;
	int res;

        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

                gtk_tree_model_get(model, &iter, 0, &str, -1);
		if ((res = jack_disconnect(jack_client, jack_port_name(out_R_port), str)) != 0) {
			fprintf(stderr, "ERROR: jack_disconnect() returned %d\n", res);
		}
		g_free(str);
		timeout_tag = gtk_timeout_add(100, ports_timeout_callback, (gpointer)2);
        }
}


void
clear_outs(GtkWidget * widget, gpointer * data) {

	const char ** ports;
	int i = 0;
	int res;

	ports = jack_port_get_connections(out_L_port);
	if (ports) {
		while (ports[i] != NULL) {
			if ((res = jack_disconnect(jack_client, jack_port_name(out_L_port), ports[i])) != 0) {
				fprintf(stderr, "ERROR: jack_disconnect() returned %d\n", res);
			}
			i++;
		}
		free(ports);
	}
	i = 0;
	ports = jack_port_get_connections(out_R_port);
	if (ports) {
		while (ports[i] != NULL) {
			if ((res = jack_disconnect(jack_client, jack_port_name(out_R_port), ports[i])) != 0) {
				fprintf(stderr, "ERROR: jack_disconnect() returned %d\n", res);
			}
			i++;
		}
		free(ports);
	}
	gtk_list_store_clear(store_out_L);
	gtk_list_store_clear(store_out_R);
}


void
scan_connections(jack_port_t * port, GtkListStore * store) {

	GtkTreeIter iter;
	const char ** ports;
	int i = 0;

	ports = jack_port_get_connections(port);

	if (!ports)
		return;
	
	while (ports[i] != NULL) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, ports[i], -1);
		i++;
	}
	free(ports);
}


GtkWidget *
setup_tree_out(void) {

	GtkWidget * tree;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkTreeSelection * select;
	GtkWidget * scrwin;

	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_out_nb[n_clients]));
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("inputs", renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(tree), "cursor-changed", G_CALLBACK(tree_out_nb_selection_changed),
			 (gpointer *)n_clients);

	scrwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrwin), tree);
	gtk_widget_set_size_request(GTK_WIDGET(scrwin), LIST_WIDTH, -1);
	
	gtk_widget_show(tree);
	gtk_widget_show(scrwin);

	return scrwin;
}


void
setup_notebook_out(void) {
	
	const char ** ports_out;
	int i, j, k;
	char client_name[MAXLEN];
	char client_name_prev[MAXLEN];
	char port_name[MAXLEN];
	GtkTreeIter iter;

        ports_out = jack_get_ports(jack_client, NULL, NULL, JackPortIsInput);

	for (j = 0; j < MAXLEN; j++) {
		client_name[j] = '\0';
		client_name_prev[j] = '\0';
	}

	i = 0;
	n_clients = -1;
	if (ports_out) {
		while (ports_out[i] != NULL) {
			/* get the client name */
			j = 0;
			while ((ports_out[i][j] != ':') && (ports_out[i][j] != '\0')) {
				client_name[j] = ports_out[i][j];
				j++;
			}
			client_name[j] = '\0';

			/* create a new notebook page if needed */
			if (strcmp(client_name, client_name_prev) != 0) {
				n_clients++;
				store_out_nb[n_clients] = gtk_list_store_new(1, G_TYPE_STRING);
				nb_out_labels[n_clients] = gtk_label_new(client_name);
				gtk_widget_show(nb_out_labels[n_clients]);
				gtk_notebook_insert_page(GTK_NOTEBOOK(nb_outs), GTK_WIDGET(setup_tree_out()),
							 GTK_WIDGET(nb_out_labels[n_clients]), n_clients);
			}
			/* add the port to the list */
			j = 0;
			while ((ports_out[i][j] != ':') && (ports_out[i][j] != '\0'))
				j++;
			if (ports_out[i][j] == '\0')
				fprintf(stderr, "ERROR: bad JACK port string: %s\n", ports_out[i]);
			else {
				k = 0;
				j++;
				while (ports_out[i][j] != '\0')
					port_name[k++] = ports_out[i][j++];
				port_name[k] = '\0';
				gtk_list_store_append(store_out_nb[n_clients], &iter);
				gtk_list_store_set(store_out_nb[n_clients], &iter, 0, port_name, -1);
			}
			strcpy(client_name_prev, client_name);
			i++;
		}
		free(ports_out);
	}
}


void
port_setup_dialog(void) {

	GtkWidget * vbox;
	GtkWidget * table;
	GtkWidget * button_rescan;
	GtkWidget * button_close;
	GtkWidget * button_clear_outs;
	GtkWidget * frame_dl;
	GtkWidget * frame_dr;

	GtkCellRenderer * renderer_out_L;
	GtkCellRenderer * renderer_out_R;
	GtkTreeSelection * select_out_L;
	GtkTreeSelection * select_out_R;

	GtkWidget * viewp_out_L;
	GtkWidget * viewp_out_R;

	GtkWidget * hbox_L;
	GtkWidget * hbox_R;
	
	GtkWidget * label_L;
	GtkWidget * label_R;

	GdkColor color = { 0, 0, 0, 0 };


	store_out_L = gtk_list_store_new(1, G_TYPE_STRING);
	store_out_R = gtk_list_store_new(1, G_TYPE_STRING);

	ports_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(ports_window), "JACK Port Setup");
        gtk_window_set_position(GTK_WINDOW(ports_window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(ports_window), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(ports_window), GTK_WINDOW(main_window));
        g_signal_connect(G_OBJECT(ports_window), "delete_event", G_CALLBACK(port_window_close), NULL);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(ports_window), vbox);

        table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 2);

        button_rescan = gtk_button_new_with_label("Rescan");
        gtk_table_attach(GTK_TABLE(table), button_rescan, 0, 1, 1, 2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);
        g_signal_connect(G_OBJECT(button_rescan), "clicked", G_CALLBACK(clicked_rescan), NULL);

        button_close = gtk_button_new_with_label("Close");
        gtk_table_attach(GTK_TABLE(table), button_close, 1, 2, 1, 2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL, 5, 5);
        g_signal_connect(G_OBJECT(button_close), "clicked", G_CALLBACK(ports_clicked_close), NULL);
	
	frame_dl = gtk_frame_new("Outputs");
        gtk_table_attach(GTK_TABLE(table), frame_dl, 0, 1, 0, 1,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);

	frame_dr = gtk_frame_new("Available connections");
        gtk_table_attach(GTK_TABLE(table), frame_dr, 1, 2, 0, 1,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);

        vbox_dl = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_dl), 8);
	gtk_container_add(GTK_CONTAINER(frame_dl), vbox_dl);

        vbox_dr = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox_dr), 8);
	gtk_container_add(GTK_CONTAINER(frame_dr), vbox_dr);

	button_clear_outs = gtk_button_new_with_label("Clear connections");
        gtk_box_pack_start(GTK_BOX(vbox_dl), button_clear_outs, FALSE, TRUE, 2);
        g_signal_connect(G_OBJECT(button_clear_outs), "clicked", G_CALLBACK(clear_outs), NULL);
	
	nb_outs = gtk_notebook_new();
        gtk_box_pack_start(GTK_BOX(vbox_dr), nb_outs, TRUE, TRUE, 2);

	scan_connections(out_L_port, store_out_L);
	scan_connections(out_R_port, store_out_R);

	tree_out_L = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_out_L));
	tree_out_R = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_out_R));
	renderer_out_L = gtk_cell_renderer_text_new();
	renderer_out_R = gtk_cell_renderer_text_new();
	column_out_L = gtk_tree_view_column_new_with_attributes(NULL, renderer_out_L, "text", 0, NULL);
	column_out_R = gtk_tree_view_column_new_with_attributes(NULL, renderer_out_R, "text", 0, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_out_L), column_out_L);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_out_R), column_out_R);

	g_signal_connect(G_OBJECT(column_out_L->button), "clicked", G_CALLBACK(clicked_out_L_header), NULL);
	g_signal_connect(G_OBJECT(column_out_R->button), "clicked", G_CALLBACK(clicked_out_R_header), NULL);
	
	gtk_widget_set_name(column_out_L->button, "nostyle");
	gtk_widget_set_name(column_out_R->button, "nostyle");
	
	select_out_L = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_out_L));
	gtk_tree_selection_set_mode(select_out_L, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select_out_L), "changed", G_CALLBACK(tree_out_L_selection_changed), NULL);

	select_out_R = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_out_R));
	gtk_tree_selection_set_mode(select_out_R, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select_out_R), "changed", G_CALLBACK(tree_out_R_selection_changed), NULL);

	gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tree_out_L), TRUE);
	gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tree_out_R), TRUE);

	viewp_out_L = gtk_viewport_new(NULL, NULL);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewp_out_L), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(viewp_out_L), tree_out_L);
	gtk_widget_set_size_request(GTK_WIDGET(viewp_out_L), LIST_WIDTH, LIST_HEIGHT);

	viewp_out_R = gtk_viewport_new(NULL, NULL);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewp_out_R), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(viewp_out_R), tree_out_R);
	gtk_widget_set_size_request(GTK_WIDGET(viewp_out_R), LIST_WIDTH, LIST_HEIGHT);

        gtk_box_pack_start(GTK_BOX(vbox_dl), viewp_out_L, TRUE, TRUE, 2);
        gtk_box_pack_start(GTK_BOX(vbox_dl), viewp_out_R, TRUE, TRUE, 2);

	setup_notebook_out();

	set_active(GTK_WIDGET(column_out_L->button), TRUE);
	set_active(GTK_WIDGET(column_out_R->button), FALSE);

	gtk_widget_show_all(ports_window);

	gtk_widget_destroy(GTK_BIN(column_out_L->button)->child);
	gtk_widget_destroy(GTK_BIN(column_out_R->button)->child);

	hbox_L = gtk_hbox_new(FALSE, 0);
	hbox_R = gtk_hbox_new(FALSE, 0);

	label_L = gtk_label_new(" out L");
	label_R = gtk_label_new(" out R");

	gtk_container_add(GTK_CONTAINER(column_out_L->button), hbox_L);
	gtk_container_add(GTK_CONTAINER(column_out_R->button), hbox_R);

	gtk_box_pack_start(GTK_BOX(hbox_L), label_L, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_R), label_R, FALSE, FALSE, 0);

	gtk_widget_modify_fg(label_L, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_fg(label_L, GTK_STATE_PRELIGHT, &color);
	gtk_widget_modify_fg(label_L, GTK_STATE_ACTIVE, &color);

	gtk_widget_modify_fg(label_R, GTK_STATE_NORMAL, &color);
	gtk_widget_modify_fg(label_R, GTK_STATE_PRELIGHT, &color);
	gtk_widget_modify_fg(label_R, GTK_STATE_ACTIVE, &color);

	gtk_widget_show_all(hbox_L);
	gtk_widget_show_all(hbox_R);
}
