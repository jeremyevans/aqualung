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
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <dirent.h>
#include <lrdf.h>
#include <pthread.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>


#include "common.h"
#include "i18n.h"
#include "trashlist.h"
#include "plugin.h"


extern pthread_mutex_t plugin_lock;

extern char confdir[MAXLEN];
extern int n_plugins;
extern plugin_instance * plugin_vect[MAX_PLUGINS];

extern unsigned long ladspa_buflen;
extern LADSPA_Data * l_buf;
extern LADSPA_Data * r_buf;

extern unsigned long out_SR;

int fxbuilder_on;

GtkWidget * fxbuilder_window;
GtkWidget * avail_list;
GtkListStore * avail_store = NULL;
GtkTreeSelection * avail_select;
GtkWidget * running_list;
GtkListStore * running_store = NULL;
GtkTreeSelection * running_select;

GtkWidget * add_button;
GtkWidget * remove_button;
GtkWidget * conf_button;

extern GtkWidget * plugin_toggle;

typedef struct {
	plugin_instance * instance;
	int index;
} optdata_t;

typedef struct {
	plugin_instance * instance;
	float start;
} btnpdata_t;

int added_plugin = 0;


static int
rdf_filter(const struct dirent64 * de) {

	if (de->d_type != DT_UNKNOWN && de->d_type != DT_REG && de->d_type != DT_LNK)
		return 0;

	if (de->d_name[0] == '.')
		return 0;

	return (((strlen(de->d_name) >= 4) && (strcmp(de->d_name + strlen(de->d_name) - 3, ".n3") == 0)) ||
		((strlen(de->d_name) >= 5) && (strcmp(de->d_name + strlen(de->d_name) - 4, ".rdf") == 0)) ||
		((strlen(de->d_name) >= 6) && (strcmp(de->d_name + strlen(de->d_name) - 5, ".rdfs") == 0)));
}

static int
so_filter(const struct dirent64 * de) {

	if (de->d_type != DT_UNKNOWN && de->d_type != DT_REG && de->d_type != DT_LNK)
		return 0;

	if (de->d_name[0] == '.')
		return 0;

	return ((strlen(de->d_name) >= 4) && (strcmp(de->d_name + strlen(de->d_name) - 3, ".so") == 0));
}

void
parse_lrdf_data(void) {


	char * lrdf_path = NULL;
	char rdf_path[MAXLEN];
	char fileuri[MAXLEN];
	int i, j = 0;
	struct dirent64 ** de;
	int n;

	if (!(lrdf_path = getenv("LADSPA_RDF_PATH"))) {
                lrdf_path = strdup("/usr/local/share/ladspa/rdf:/usr/share/ladspa/rdf");
	}

	strcat(lrdf_path, ":");
	for (i = 0; lrdf_path[i] != '\0'; i++) {
		if (lrdf_path[i] == ':') {
			rdf_path[j] = '\0';
			j = 0;

			n = scandir64(rdf_path, &de, rdf_filter, alphasort64);
			if (n >= 0) {
				int c;
				
				for (c = 0; c < n; ++c) {
					snprintf(fileuri, MAXLEN-1, "file://%s/%s", rdf_path, de[c]->d_name);
					if (lrdf_read_file(fileuri)) {
						fprintf(stderr,
							"warning: could not parse RDF file: %s\n", fileuri);
					}
				}
			}
		} else {
			rdf_path[j++] = lrdf_path[i];
		}
	}
}


void
get_ladspa_category(unsigned long plugin_id, char * str) {

        char buf[256];
        lrdf_statement pattern;
	lrdf_statement * matches1;
	lrdf_statement * matches2;

        snprintf(buf, 255, "%s%lu", LADSPA_BASE, plugin_id);
        pattern.subject = buf;
        pattern.predicate = RDF_TYPE;
        pattern.object = 0;
        pattern.object_type = lrdf_uri;

        matches1 = lrdf_matches(&pattern);

        if (!matches1) {
                strncpy(str, "Unknown", MAXLEN-1);
		return;
        }

        pattern.subject = matches1->object;
        pattern.predicate = LADSPA_BASE "hasLabel";
        pattern.object = 0;
        pattern.object_type = lrdf_literal;

        matches2 = lrdf_matches (&pattern);
        lrdf_free_statements(matches1);

        if (!matches2) {
                strncpy(str, "Unknown", MAXLEN-1);
                return;
        }

        strncpy(str, matches2->object, MAXLEN-1);
        lrdf_free_statements(matches2);
}


static void
find_plugins(char * path_entry) {

	void * library = NULL;
	char lib_name[MAXLEN];
	LADSPA_Descriptor_Function descriptor_fn;
	const LADSPA_Descriptor * descriptor;
	struct dirent64 ** de;
	int n, k, c;
	long int port, n_ins, n_outs;
	GtkTreeIter iter;
	char id_str[32];
	char n_ins_str[32];
	char n_outs_str[32];
	char c_str[32];
	char category[MAXLEN];

	n = scandir64(path_entry, &de, so_filter, alphasort64);
	if (n >= 0) {
		for (c = 0; c < n; ++c) {
			snprintf(lib_name, MAXLEN-1, "%s/%s", path_entry, de[c]->d_name);
			library = dlopen(lib_name, RTLD_LAZY);
			if (library == NULL) {
				continue;
			}
			descriptor_fn = dlsym(library, "ladspa_descriptor");
			if (descriptor_fn == NULL) {
				dlclose(library);
				continue;
			}

			for (k = 0; ; ++k) {
				descriptor = descriptor_fn(k);
				if (descriptor == NULL) {
					break;
				}

				for (n_ins = n_outs = port = 0; port < descriptor->PortCount; ++port) {
					if (LADSPA_IS_PORT_AUDIO(descriptor->PortDescriptors[port])) {
						if (LADSPA_IS_PORT_INPUT(descriptor->PortDescriptors[port]))
							++n_ins;
						if (LADSPA_IS_PORT_OUTPUT(descriptor->PortDescriptors[port]))
							++n_outs;
					}
				}
				get_ladspa_category(descriptor->UniqueID, category);
				snprintf(id_str, 31, "%ld", descriptor->UniqueID);
				snprintf(n_ins_str, 31, "%ld", n_ins);
				snprintf(n_outs_str, 31, "%ld", n_outs);
				snprintf(c_str, 31, "%d", k);

                                gtk_list_store_append(avail_store, &iter);
                                gtk_list_store_set(avail_store, &iter, 0, id_str, 1, descriptor->Name,
						   2, category, 3, n_ins_str, 4, n_outs_str,
						   5, lib_name, 6, c_str, -1);
				
			}
			dlclose(library);
		}
	}
}


static void
find_all_plugins(void) {

	char * ladspa_path;
	char * directory;

	if (!(ladspa_path = getenv("LADSPA_PATH"))) {
		find_plugins("/usr/lib/ladspa");
		find_plugins("/usr/local/lib/ladspa");
	} else {
		ladspa_path = strdup(ladspa_path);
		directory = strtok(ladspa_path, ":");
		while (directory != NULL) {
			find_plugins(directory);
			directory = strtok(NULL, ":");
		}
		free(ladspa_path);
	}
}



static gboolean
fxbuilder_close(GtkWidget * widget, GdkEvent * event, gpointer data) {

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plugin_toggle), FALSE);
        return TRUE;
}

void
show_fxbuilder(void) {

	gtk_widget_show_all(fxbuilder_window);
	fxbuilder_on = 1;
}


void
hide_fxbuilder(void) {

	gtk_widget_hide(fxbuilder_window);
	fxbuilder_on = 0;
}


gint
fxbuilder_key_pressed(GtkWidget * widget, GdkEventKey * event, gpointer * data) {

        switch (event->keyval) {
	case GDK_q:
	case GDK_Q:
	case GDK_Escape:
		fxbuilder_close(NULL, NULL, NULL);
		return TRUE;
	};

	return FALSE;
}


/* we need this because the default gtk sort func doesn't obey spaces in strings
   eg. "ABCE" gets in between "ABC D" and "ABC F" and not after them.
*/
gint
compare_func(GtkTreeModel * model, GtkTreeIter * a, GtkTreeIter * b, gpointer user_data) {

	int col = (int) user_data;
	char * sa;
	char * sb;
	int ret;

	gtk_tree_model_get(model, a, col, &sa, -1);
	gtk_tree_model_get(model, b, col, &sb, -1);

	ret = strcmp(sa, sb);

	g_free(sa);
	g_free(sb);

	return ret;
}


plugin_instance *
instantiate(char * filename, int index) {

	LADSPA_Descriptor_Function descriptor_fn;
	plugin_instance * instance;
	int n_ins, n_outs, n_ctrl, port;

	if ((instance = calloc(1, sizeof(plugin_instance))) == NULL) {
		fprintf(stderr, "plugin.c: instantiate(): calloc error\n");
		return NULL;
	}

	strncpy(instance->filename, filename, MAXLEN-1);
	instance->index = index;

	instance->library = dlopen(filename, RTLD_NOW);
	if (instance->library == NULL) {
		fprintf(stderr, "dlopen() failed on %s -- is it a valid shared library file?\n", filename);
		free(instance);
		return NULL;
	}
	descriptor_fn = dlsym(instance->library, "ladspa_descriptor");
	if (descriptor_fn == NULL) {
		fprintf(stderr,
			"dlsym() failed to load symbol 'ladspa_descriptor'. "
			"Possibly a bug in %s\n", filename);
		dlclose(instance->library);
		free(instance);
		return NULL;
	}
	instance->descriptor = descriptor_fn(index);

	if (LADSPA_IS_INPLACE_BROKEN(instance->descriptor->Properties)) {
		fprintf(stderr,
			"%s (%s) is INPLACE_BROKEN and thus unusable in "
			"Aqualung at this time.\n", instance->descriptor->Label, instance->descriptor->Name);
		dlclose(instance->library);
		free(instance);
		return NULL;
	}

	for (n_ins = n_outs = n_ctrl = port = 0; port < instance->descriptor->PortCount; ++port) {
		if (LADSPA_IS_PORT_AUDIO(instance->descriptor->PortDescriptors[port])) {
			if (LADSPA_IS_PORT_INPUT(instance->descriptor->PortDescriptors[port]))
				++n_ins;
			if (LADSPA_IS_PORT_OUTPUT(instance->descriptor->PortDescriptors[port]))
				++n_outs;
		} else {
			++n_ctrl;
		}
	}

	if (n_ctrl > MAX_KNOBS) {
                fprintf(stderr,
			"%s (%s) has more than %d input knobs; "
			"Aqualung cannot use it.\n",
			instance->descriptor->Label, instance->descriptor->Name, MAX_KNOBS);
                dlclose(instance->library);
                free(instance);
                return NULL;
	}

	if ((n_ins == 1) && (n_outs == 1)) {
		instance->is_mono = 1;
		instance->handle = instance->descriptor->instantiate(instance->descriptor, out_SR);
		instance->handle2 = instance->descriptor->instantiate(instance->descriptor, out_SR);
	} else {
		instance->is_mono = 0;
		instance->handle = instance->descriptor->instantiate(instance->descriptor, out_SR);
		instance->handle2 = NULL;
	}

	instance->is_restored = 0;
	instance->is_bypassed = 1;
	instance->shift_pressed = 0;
	instance->window = NULL;
	instance->bypass_button = NULL;
	instance->trashlist = trashlist_new();

	return instance;
}


void
connect_port(plugin_instance * instance) {

	unsigned long port;
	unsigned long inputs = 0, outputs = 0;
	const LADSPA_Descriptor * plugin = instance->descriptor;

	for (port = 0; port < plugin->PortCount; ++port) {

		if (LADSPA_IS_PORT_CONTROL(plugin->PortDescriptors[port])) {
			if (port < MAX_KNOBS) {
				plugin->connect_port(instance->handle, port,
						     &(instance->knobs[port]));
				if (instance->handle2)
					plugin->connect_port(instance->handle2, port,
							     &(instance->knobs[port]));
			} else {
				fprintf(stderr, "impossible: control port count out of range\n");
			}

		} else if (LADSPA_IS_PORT_AUDIO(plugin->PortDescriptors[port])) {

			if (LADSPA_IS_PORT_INPUT(plugin->PortDescriptors[port])) {
				if (inputs == 0) {
					plugin->connect_port(instance->handle, port, l_buf);
					if (instance->handle2)
						plugin->connect_port(instance->handle2, port, r_buf);
				} else if (inputs == 1 && !instance->is_mono) {
					plugin->connect_port(instance->handle, port, r_buf);
				} else {
					fprintf(stderr, "impossible: input port count out of range\n");
				}
				inputs++;

			} else if (LADSPA_IS_PORT_OUTPUT(plugin->PortDescriptors[port])) {
				if (outputs == 0) {
					plugin->connect_port(instance->handle, port, l_buf);
					if (instance->handle2)
						plugin->connect_port(instance->handle2, port, r_buf);
				} else if (outputs == 1 && !instance->is_mono) {
					plugin->connect_port(instance->handle, port, r_buf);
				} else {
					fprintf(stderr, "impossible: output port count out of range\n");
				}
				outputs++;
			}
		}
	}
}


void
activate(plugin_instance * instance) {

	const LADSPA_Descriptor * descriptor = instance->descriptor;

	if (descriptor->activate) {
		descriptor->activate(instance->handle);
		if (instance->handle2) {
			descriptor->activate(instance->handle2);
		}
	}
}


void
get_bypassed_name(plugin_instance * instance, char * str) {

	if (instance->is_bypassed) {
		snprintf(str, MAXLEN-1, "(%s)", instance->descriptor->Name);
	} else {
		strncpy(str, instance->descriptor->Name, MAXLEN-1);
	}
}


void
refresh_plugin_vect(void) {
	
	int i = 0;
	GtkTreeIter iter;
	gpointer gp_instance;

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(running_store), &iter, NULL, i) &&
		i < MAX_PLUGINS) {

		gtk_tree_model_get(GTK_TREE_MODEL(running_store), &iter, 1, &gp_instance, -1);
		plugin_vect[i] = (plugin_instance *) gp_instance;
		++i;
	}
}


static gboolean
close_plugin_window(GtkWidget * widget, GdkEvent * event, gpointer data) {

        gtk_widget_hide(widget);
        return TRUE;
}


void
plugin_bypassed(GtkWidget * widget, gpointer data) {

        int i = 0;
        GtkTreeIter iter;
	char bypassed_name[MAXLEN];
	char * name;
        gpointer gp_instance;
	plugin_instance * instance = (plugin_instance *) data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		pthread_mutex_lock(&plugin_lock);
		instance->is_bypassed = 1;
		pthread_mutex_unlock(&plugin_lock);
	} else {
		pthread_mutex_lock(&plugin_lock);
		instance->is_bypassed = 0;
		pthread_mutex_unlock(&plugin_lock);
	}

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(running_store), &iter, NULL, i)) {

                gtk_tree_model_get(GTK_TREE_MODEL(running_store), &iter, 0, &name, 1, &gp_instance, -1);
		if (instance == (plugin_instance *)gp_instance) {
			get_bypassed_name(instance, bypassed_name);
			gtk_list_store_set(running_store, &iter, 0, bypassed_name, -1);
			return;
		}
		++i;
        }
}


void
plugin_btn_toggled(GtkWidget * widget, gpointer data) {

	LADSPA_Data * plugin_data = (LADSPA_Data *) data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		pthread_mutex_lock(&plugin_lock);
		*plugin_data = 1.0f;
		pthread_mutex_unlock(&plugin_lock);
	} else {
		pthread_mutex_lock(&plugin_lock);
		*plugin_data = -1.0f;
		pthread_mutex_unlock(&plugin_lock);
	}
}


gint
update_plugin_outputs(gpointer data) {

	plugin_instance * instance = (plugin_instance *) data;
	unsigned long k;

	pthread_mutex_lock(&plugin_lock);
	for (k = 0; k < MAX_KNOBS && k < instance->descriptor->PortCount; ++k) {
		if (LADSPA_IS_PORT_OUTPUT(instance->descriptor->PortDescriptors[k])
		    && LADSPA_IS_PORT_CONTROL(instance->descriptor->PortDescriptors[k])) {

			instance->adjustments[k]->value = instance->knobs[k];
		}
	}
	pthread_mutex_unlock(&plugin_lock);

	for (k = 0; k < MAX_KNOBS && k < instance->descriptor->PortCount; ++k) {
		if (LADSPA_IS_PORT_OUTPUT(instance->descriptor->PortDescriptors[k])
		    && LADSPA_IS_PORT_CONTROL(instance->descriptor->PortDescriptors[k])) {
			
			gtk_adjustment_value_changed(instance->adjustments[k]);
		}
	}
	return TRUE;
}


void
plugin_value_changed(GtkAdjustment * adj, gpointer data) {

	LADSPA_Data * plugin_data = (LADSPA_Data *) data;

	pthread_mutex_lock(&plugin_lock);
	*plugin_data = (LADSPA_Data) gtk_adjustment_get_value(adj);
	pthread_mutex_unlock(&plugin_lock);
}


void
changed_combo(GtkWidget * widget, gpointer * data) {

	optdata_t * optdata = (optdata_t *) data;
	plugin_instance * instance = optdata->instance;
	int k = optdata->index;
	const char * str = gtk_entry_get_text(GTK_ENTRY(widget));
	int i;
	LADSPA_Data value = 0.0f;

	lrdf_defaults * defs = lrdf_get_scale_values(instance->descriptor->UniqueID, k);
	for (i = 0; i < defs->count; i++) {
		if (strcmp(str, defs->items[i].label) == 0) {
			value = defs->items[i].value;
			break;
		}
	}
	lrdf_free_setting_values(defs);

	pthread_mutex_lock(&plugin_lock);
	instance->knobs[k] = value;
	pthread_mutex_unlock(&plugin_lock);

}


gint
plugin_window_key_pressed(GtkWidget * widget, GdkEventKey * event, gpointer * data) {

	plugin_instance * instance = (plugin_instance *) data;

        switch (event->keyval) {
        case GDK_Shift_L:
        case GDK_Shift_R:
                instance->shift_pressed = 1;
                break;
        }

	return FALSE;
}


gint
plugin_window_key_released(GtkWidget * widget, GdkEventKey * event, gpointer * data) {

	plugin_instance * instance = (plugin_instance *) data;

        switch (event->keyval) {
        case GDK_Shift_L:
        case GDK_Shift_R:
                instance->shift_pressed = 0;
                break;
        }

	return FALSE;
}


gint
plugin_window_focus_out(GtkWidget * widget, GdkEventKey * event, gpointer * data) {

	plugin_instance * instance = (plugin_instance *) data;

	instance->shift_pressed = 0;

	return FALSE;
}


gint
plugin_scale_btn_pressed(GtkWidget * widget, GdkEventButton * event, gpointer * data) {

	btnpdata_t * btnpdata = (btnpdata_t *) data;
	GtkAdjustment * adj;

        if (event->button != 1)
                return FALSE;

	if (!btnpdata->instance->shift_pressed)
		return FALSE;

	adj = gtk_range_get_adjustment(GTK_RANGE(widget));
	pthread_mutex_lock(&plugin_lock);
	adj->value = btnpdata->start;
	pthread_mutex_unlock(&plugin_lock);
	gtk_adjustment_value_changed(adj);

        return TRUE;
}


void
build_plugin_window(plugin_instance * instance) {

	const LADSPA_Descriptor * plugin = instance->descriptor;
	const LADSPA_PortRangeHint * hints = plugin->PortRangeHints;
	LADSPA_Data fact, min, max, step, start, default_val;
	lrdf_defaults * defs;
	int dp;
	unsigned long k;
	int n_outs = 0;
	int n_ins = 0;
	int n_toggled = 0;
	int n_untoggled = 0;
	int n_outctl = 0;
	int n_outlat = 0;
	int n_rows = 0;
	int i = 0;

	char str_inout[32];
	char str_n[16];
	char * c;
	char maker[MAXLEN];
	GtkWidget * widget;
	GtkWidget * hbox;
	GtkWidget * vbox;
	GtkWidget * upper_hbox;
	GtkWidget * upper_vbox;

	GtkWidget * scrwin;
	GtkWidget * inner_vbox;
	GtkWidget * table = NULL; /* gcc rulez, eh? */
	GtkWidget * hseparator;
	GtkObject * adjustment;
	GtkWidget * combo;
	GList * glist;

	optdata_t * optdata;
	btnpdata_t * btnpdata;
	int j;

	GtkRequisition req;
	int max_width = 0;
	int height = 0;


	instance->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(instance->window), plugin->Name);
	gtk_window_set_position(GTK_WINDOW(instance->window), GTK_WIN_POS_CENTER);
	gtk_window_set_transient_for(GTK_WINDOW(instance->window), GTK_WINDOW(fxbuilder_window));
        g_signal_connect(G_OBJECT(instance->window), "key_press_event",
			 G_CALLBACK(plugin_window_key_pressed), instance);
        g_signal_connect(G_OBJECT(instance->window), "key_release_event",
                         G_CALLBACK(plugin_window_key_released), instance);
        g_signal_connect(G_OBJECT(instance->window), "focus_out_event",
                         G_CALLBACK(plugin_window_focus_out), instance);
        gtk_widget_set_events(instance->window,
			      GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
        gtk_container_set_border_width(GTK_CONTAINER(instance->window), 5);

	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_add(GTK_CONTAINER(instance->window), vbox);

	upper_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), upper_hbox, FALSE, TRUE, 2);

	upper_vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(upper_hbox), upper_vbox, FALSE, FALSE, 2);

	widget = gtk_label_new(plugin->Name);
	gtk_widget_set_name(widget, "plugin_name");
	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(upper_vbox), hbox, FALSE, FALSE, 2);

	strncpy(maker, plugin->Maker, MAXLEN-1);
	if ((c = strchr(maker, '<')) != NULL)
		*c = '\0';
	widget = gtk_label_new(maker);
	gtk_widget_set_name(widget, "plugin_maker");
	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(upper_vbox), hbox, FALSE, FALSE, 2);

	/* count audio I/O ports */
	for (k = 0; k < MAX_KNOBS && k < plugin->PortCount; ++k) {
		if (LADSPA_IS_PORT_CONTROL(plugin->PortDescriptors[k]))
			continue;
		if (LADSPA_IS_PORT_OUTPUT(plugin->PortDescriptors[k])) {
			++n_outs;
		} else {
			++n_ins;
		}
	}

	strcpy(str_inout, "[ ");
	if (n_ins == 1) {
		strcat(str_inout, "1 in");
	} else {
		snprintf(str_n, 15, "%d ins", n_ins);
		strcat(str_inout, str_n);
	}
	strcat(str_inout, " | ");
	if (n_outs == 1) {
		strcat(str_inout, "1 out");
	} else {
		snprintf(str_n, 15, "%d outs", n_outs);
		strcat(str_inout, str_n);
	}
	strcat(str_inout, " ]");
 
	widget = gtk_label_new(str_inout);
	hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(upper_vbox), hbox, FALSE, FALSE, 2);

	upper_vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(upper_hbox), upper_vbox, FALSE, FALSE, 2);


	widget = gtk_toggle_button_new_with_label("BYPASS");
	gtk_widget_set_name(widget, "plugin_bypass_button");
	instance->bypass_button = widget;
	gtk_box_pack_start(GTK_BOX(upper_vbox), widget, FALSE, FALSE, 2);
	g_signal_connect(G_OBJECT(widget), "toggled", G_CALLBACK(plugin_bypassed), instance);
	if (instance->is_restored) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), instance->is_bypassed);
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	}


	/* count control I/O ports */
	for (k = 0; k < MAX_KNOBS && k < plugin->PortCount; ++k) {
		if (LADSPA_IS_PORT_AUDIO(plugin->PortDescriptors[k]))
			continue;
		if (LADSPA_IS_PORT_INPUT(plugin->PortDescriptors[k])) {
			if (LADSPA_IS_HINT_TOGGLED(hints[k].HintDescriptor)) {
				++n_toggled;
			} else {
				++n_untoggled;
			}
		} else {
			if (strcmp(plugin->PortNames[k], "latency") == 0) {
				++n_outlat;
			} else {
				++n_outctl;
			}
		}
		++n_rows;
	}

	if ((n_toggled) && (n_untoggled))
		++n_rows;
	if (((n_toggled) || (n_untoggled)) && (n_outctl))
		++n_rows;
	if (((n_toggled) || (n_untoggled) || (n_outctl)) && (n_outlat))
		++n_rows;


	scrwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_name(scrwin, "plugin_scrwin");
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), scrwin, TRUE, TRUE, 2);

	inner_vbox = gtk_vbox_new(FALSE, 0);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrwin), inner_vbox);

	if ((n_toggled) || (n_untoggled) || (n_outctl) || (n_outlat)) {
		table = gtk_table_new(n_rows, 3, FALSE);
		gtk_box_pack_start(GTK_BOX(inner_vbox), table, TRUE, TRUE, 2);
	}

	if (n_toggled) {
		for (k = 0; k < MAX_KNOBS && k < plugin->PortCount; ++k) {
			int max_height = 0;
			
			if (!LADSPA_IS_PORT_CONTROL(plugin->PortDescriptors[k]))
				continue;
			if (LADSPA_IS_PORT_OUTPUT(plugin->PortDescriptors[k]))
				continue;
			if (!LADSPA_IS_HINT_TOGGLED(hints[k].HintDescriptor))
				continue;
			
			widget = gtk_label_new(plugin->PortNames[k]);
			hbox = gtk_hbox_new(FALSE, 0);
			gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
			gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, i, i+1,
					 GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);
			gtk_widget_size_request(widget, &req);
			req.height += 2;
			if (req.width > max_width)
				max_width = req.width;
			if (req.height > max_height)
				max_height = req.height;
			
			widget = gtk_toggle_button_new();
			gtk_widget_set_name(widget, "plugin_toggled");
			gtk_widget_set_size_request(widget, 14, 14);
			gtk_table_attach(GTK_TABLE(table), widget, 2, 3, i, i+1, 0, 0, 0, 0);
			gtk_widget_size_request(widget, &req);
			if (req.height > max_height)
				max_height = req.height;
			height += max_height;
			++i;
			
			g_signal_connect(G_OBJECT(widget), "toggled", G_CALLBACK(plugin_btn_toggled),
					 &(instance->knobs[k]));
			
			if (((instance->is_restored) && (instance->knobs[k] > 0.0f)) ||
			    ((!instance->is_restored) &&
			     (LADSPA_IS_HINT_DEFAULT_1(hints[k].HintDescriptor)))) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
			}
		}
	}

	if ((n_toggled) && (n_untoggled)) {
		hseparator = gtk_hseparator_new();
		gtk_table_attach(GTK_TABLE(table), hseparator, 0, 3, i, i+1, GTK_FILL, GTK_FILL, 2, 2);
		++i;
		gtk_widget_size_request(hseparator, &req);
		height += req.height + 5;
	}

	if (n_untoggled) {
		for (k = 0; k < MAX_KNOBS && k < plugin->PortCount; ++k) {
			int max_height = 0;

			if (!LADSPA_IS_PORT_CONTROL(plugin->PortDescriptors[k]))
				continue;
			if (LADSPA_IS_PORT_OUTPUT(plugin->PortDescriptors[k]))
				continue;
			if (LADSPA_IS_HINT_TOGGLED(hints[k].HintDescriptor))
				continue;
			
			widget = gtk_label_new(plugin->PortNames[k]);
                        hbox = gtk_hbox_new(FALSE, 0);
                        gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
			gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, i, i+1,
					 GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);
			gtk_widget_size_request(widget, &req);
			req.height += 2;
			if (req.width > max_width)
				max_width = req.width;
			
			
			if (LADSPA_IS_HINT_SAMPLE_RATE(hints[k].HintDescriptor)) {
				fact = out_SR;
			} else {
				fact = 1.0f;
			}
			
			if (LADSPA_IS_HINT_BOUNDED_BELOW(hints[k].HintDescriptor)) {
				min = hints[k].LowerBound * fact;
			} else {
				min = -10000.0f;
			}
			
			if (LADSPA_IS_HINT_BOUNDED_ABOVE(hints[k].HintDescriptor)) {
				max = hints[k].UpperBound * fact;
			} else {
				max = 10000.0f;
			}
			
			/* infinity */
			if (10000.0f <= max - min) {
				dp = 1;
				step = 5.0f;
				
			/* 100.0 ... lots */
			} else if (100.0f < max - min) {
				dp = 0;
				step = 1.0f;
				
			/* 10.0 ... 100.0 */
			} else if (10.0f < max - min) {
				dp = 1;
				step = 0.1f;
				
			/* 1.0 ... 10.0 */
			} else if (1.0f < max - min) {
				dp = 2;
				step = 0.01f;
				
			/* 0.0 ... 1.0 */
			} else {
				dp = 3;
				step = 0.001f;
			}
			
			if (LADSPA_IS_HINT_INTEGER(hints[k].HintDescriptor)) {
				dp = 0;
				if (step < 1.0f) step = 1.0f;
			}
			
			if (LADSPA_IS_HINT_DEFAULT_MINIMUM(hints[k].HintDescriptor)) {
				default_val = min;
			} else if (LADSPA_IS_HINT_DEFAULT_LOW(hints[k].HintDescriptor)) {
				default_val = min * 0.75f + max * 0.25f;
			} else if (LADSPA_IS_HINT_DEFAULT_MIDDLE(hints[k].HintDescriptor)) {
				default_val = min * 0.5f + max * 0.5f;
			} else if (LADSPA_IS_HINT_DEFAULT_HIGH(hints[k].HintDescriptor)) {
				default_val = min * 0.25f + max * 0.75f;
			} else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(hints[k].HintDescriptor)) {
				default_val = max;
			} else if (LADSPA_IS_HINT_DEFAULT_0(hints[k].HintDescriptor)) {
				default_val = 0.0f;
			} else if (LADSPA_IS_HINT_DEFAULT_1(hints[k].HintDescriptor)) {
				default_val = 1.0f;
			} else if (LADSPA_IS_HINT_DEFAULT_100(hints[k].HintDescriptor)) {
				default_val = 100.0f;
			} else if (LADSPA_IS_HINT_DEFAULT_440(hints[k].HintDescriptor)) {
				default_val = 440.0f;
			} else if (LADSPA_IS_HINT_INTEGER(hints[k].HintDescriptor)) {
				default_val = min;
			} else if (max >= 0.0f && min <= 0.0f) {
				default_val = 0.0f;
			} else {
				default_val = min * 0.5f + max * 0.5f;
			}
			
			if (instance->is_restored) {
				start = instance->knobs[k];
			} else {
				instance->knobs[k] = start = default_val;
			}

			defs = lrdf_get_scale_values(plugin->UniqueID, k);
			if ((defs) && (defs->count > 0)) { /* have scale values */

				combo = gtk_combo_new();
				gtk_widget_set_name(combo, "plugin_combo");
				glist = NULL;
				gtk_table_attach(GTK_TABLE(table), combo, 1, 3, i, i+1,
						 GTK_FILL | GTK_EXPAND, GTK_FILL, 2, 2);

				for (j = 0; j < defs->count; j++) {
					glist = g_list_append(glist, defs->items[j].label);
				}
				gtk_combo_set_popdown_strings(GTK_COMBO(combo), glist);

				gtk_widget_size_request(combo, &req);
				req.height += 2;
				if (req.height > max_height)
					max_height = req.height;

				/* now if we have an option that corresponds to 'start', choose that. */
				for (j = 0; j < defs->count; j++) {
					if (defs->items[j].value == start) {
						gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry),
								   defs->items[j].label);
						break;
					}
				}

				if ((optdata = malloc(sizeof(optdata_t))) == NULL) {
					fprintf(stderr, "plugin.c: build_plugin_window(): malloc error\n");
					return;
				}
				trashlist_add(instance->trashlist, optdata);

				optdata->instance = instance;
				optdata->index = k;
				g_signal_connect(GTK_COMBO(combo)->entry, "changed",
						 G_CALLBACK(changed_combo), optdata);

			} else { /* no scale values */

				adjustment = gtk_adjustment_new(start, min, max, step, step * 50.0, 0.0);
				instance->adjustments[k] = GTK_ADJUSTMENT(adjustment);
				g_signal_connect(G_OBJECT(adjustment), "value_changed",
						 G_CALLBACK(plugin_value_changed), &(instance->knobs[k]));
				
				if (!LADSPA_IS_HINT_INTEGER(hints[k].HintDescriptor)) {
					widget = gtk_hscale_new(GTK_ADJUSTMENT(adjustment));
					gtk_widget_set_name(widget, "plugin_scale");
					gtk_widget_set_size_request(widget, 200, -1);
					gtk_scale_set_digits(GTK_SCALE(widget), dp);
					gtk_table_attach(GTK_TABLE(table), widget, 1, 2, i, i+1,
							 GTK_FILL | GTK_EXPAND, GTK_FILL, 2, 2);
					gtk_scale_set_draw_value(GTK_SCALE(widget), FALSE);
					gtk_widget_size_request(widget, &req);
					req.height += 2;
					if (req.height > max_height)
						max_height = req.height;

					if ((btnpdata = malloc(sizeof(btnpdata_t))) == NULL) {
						fprintf(stderr,
							"plugin.c: build_plugin_window(): malloc error\n");
						return;
					}
					trashlist_add(instance->trashlist, btnpdata);
					btnpdata->instance = instance;
					btnpdata->start = default_val;
					g_signal_connect(G_OBJECT(widget), "button_press_event",
							 G_CALLBACK(plugin_scale_btn_pressed),
							 (gpointer) btnpdata);
				}
				
				widget = gtk_spin_button_new(GTK_ADJUSTMENT(adjustment), step, dp);
				gtk_widget_set_size_request(widget, 70, -1);
				gtk_table_attach(GTK_TABLE(table), widget, 2, 3, i, i+1,
						 GTK_FILL, GTK_FILL, 2, 2);
				gtk_widget_size_request(widget, &req);
				req.height += 2;
				if (req.height > max_height)
					max_height = req.height;
			}
			height += max_height;
			++i;
			lrdf_free_setting_values(defs);
		}
	}


	if (((n_toggled) || (n_untoggled)) && (n_outctl)) {
		hseparator = gtk_hseparator_new();
		gtk_table_attach(GTK_TABLE(table), hseparator, 0, 3, i, i+1, GTK_FILL, GTK_FILL, 2, 2);
		++i;
		gtk_widget_size_request(hseparator, &req);
		height += req.height + 5;
	}

	if (n_outctl) {
		for (k = 0; k < MAX_KNOBS && k < plugin->PortCount; ++k) {
			int max_height = 0;

			if (!LADSPA_IS_PORT_CONTROL(plugin->PortDescriptors[k]))
				continue;
			if (LADSPA_IS_PORT_INPUT(plugin->PortDescriptors[k]))
				continue;
			if (LADSPA_IS_HINT_TOGGLED(hints[k].HintDescriptor))
				continue;
			
			widget = gtk_label_new(plugin->PortNames[k]);
                        hbox = gtk_hbox_new(FALSE, 0);
                        gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
			gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, i, i+1,
					 GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);
			gtk_widget_size_request(widget, &req);
			req.height += 2;
			if (req.width > max_width)
				max_width = req.width;
			if (req.height > max_height)
				max_height = req.height;

			if (LADSPA_IS_HINT_SAMPLE_RATE(hints[k].HintDescriptor)) {
				fact = out_SR;
			} else {
				fact = 1.0f;
			}
			
			if (LADSPA_IS_HINT_BOUNDED_BELOW(hints[k].HintDescriptor)) {
				min = hints[k].LowerBound * fact;
			} else {
				min = -10000.0f;
			}
			
			if (LADSPA_IS_HINT_BOUNDED_ABOVE(hints[k].HintDescriptor)) {
				max = hints[k].UpperBound * fact;
			} else {
				max = 10000.0f;
			}
			
			/* infinity */
			if (10000.0f <= max - min) {
				dp = 1;
				step = 5.0f;
				
			/* 100.0 ... lots */
			} else if (100.0f < max - min) {
				dp = 0;
				step = 1.0f;
				
			/* 10.0 ... 100.0 */
			} else if (10.0f < max - min) {
				dp = 1;
				step = 0.1f;
				
			/* 1.0 ... 10.0 */
			} else if (1.0f < max - min) {
				dp = 2;
				step = 0.01f;
				
			/* 0.0 ... 1.0 */
			} else {
				dp = 3;
				step = 0.001f;
			}
			
			if (LADSPA_IS_HINT_INTEGER(hints[k].HintDescriptor)) {
				dp = 0;
				if (step < 1.0f) step = 1.0f;
			}
			
			if (LADSPA_IS_HINT_DEFAULT_MINIMUM(hints[k].HintDescriptor)) {
				start = min;
			} else if (LADSPA_IS_HINT_DEFAULT_LOW(hints[k].HintDescriptor)) {
				start = min * 0.75f + max * 0.25f;
			} else if (LADSPA_IS_HINT_DEFAULT_MIDDLE(hints[k].HintDescriptor)) {
				start = min * 0.5f + max * 0.5f;
			} else if (LADSPA_IS_HINT_DEFAULT_HIGH(hints[k].HintDescriptor)) {
				start = min * 0.25f + max * 0.75f;
			} else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(hints[k].HintDescriptor)) {
				start = max;
			} else if (LADSPA_IS_HINT_DEFAULT_0(hints[k].HintDescriptor)) {
				start = 0.0f;
			} else if (LADSPA_IS_HINT_DEFAULT_1(hints[k].HintDescriptor)) {
				start = 1.0f;
			} else if (LADSPA_IS_HINT_DEFAULT_100(hints[k].HintDescriptor)) {
				start = 100.0f;
			} else if (LADSPA_IS_HINT_DEFAULT_440(hints[k].HintDescriptor)) {
				start = 440.0f;
			} else if (LADSPA_IS_HINT_INTEGER(hints[k].HintDescriptor)) {
				start = min;
			} else if (max >= 0.0f && min <= 0.0f) {
				start = 0.0f;
			} else {
				start = min * 0.5f + max * 0.5f;
			}
			
			instance->knobs[k] = start;
			
			adjustment = gtk_adjustment_new(start, min, max, step, step * 50.0, 0.0);
			instance->adjustments[k] = GTK_ADJUSTMENT(adjustment);
			
			if (!LADSPA_IS_HINT_INTEGER(hints[k].HintDescriptor)) {
				widget = gtk_hscale_new(GTK_ADJUSTMENT(adjustment));
				gtk_widget_set_name(widget, "plugin_scale");
				gtk_widget_set_size_request(widget, 200, -1);
				gtk_scale_set_digits(GTK_SCALE(widget), dp);
				gtk_table_attach(GTK_TABLE(table), widget, 1, 2, i, i+1,
						 GTK_FILL | GTK_EXPAND, GTK_FILL, 2, 2);
				gtk_scale_set_draw_value(GTK_SCALE(widget), FALSE);
				gtk_widget_set_sensitive(widget, FALSE);
				gtk_widget_size_request(widget, &req);
				req.height += 2;
				if (req.height > max_height)
					max_height = req.height;
			}

			widget = gtk_spin_button_new(GTK_ADJUSTMENT(adjustment), step, dp);
			gtk_widget_set_size_request(widget, 70, -1);
			gtk_widget_set_sensitive(widget, FALSE);
			gtk_table_attach(GTK_TABLE(table), widget, 2, 3, i, i+1,
					 GTK_FILL, GTK_FILL, 2, 2);
			gtk_widget_size_request(widget, &req);
			req.height += 2;
			if (req.height > max_height)
				max_height = req.height;
			height += max_height;
			++i;
		}
	}


	if (((n_toggled) || (n_untoggled) || (n_outctl)) && (n_outlat)) {
		hseparator = gtk_hseparator_new();
		gtk_table_attach(GTK_TABLE(table), hseparator, 0, 3, i, i+1, GTK_FILL, GTK_FILL, 2, 2);
		++i;
		gtk_widget_size_request(hseparator, &req);
		height += req.height + 5;
	}

	if (n_outlat) {
		for (k = 0; k < MAX_KNOBS && k < plugin->PortCount; ++k) {
			int max_height = 0;

			if (!LADSPA_IS_PORT_CONTROL(plugin->PortDescriptors[k]))
				continue;
			if (LADSPA_IS_PORT_INPUT(plugin->PortDescriptors[k]))
				continue;
			if (LADSPA_IS_HINT_TOGGLED(hints[k].HintDescriptor))
				continue;
			
			widget = gtk_label_new(plugin->PortNames[k]);
                        hbox = gtk_hbox_new(FALSE, 0);
                        gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
			gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, i, i+1,
					 GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);
			gtk_widget_size_request(widget, &req);
			req.height += 2;
			if (req.width > max_width)
				max_width = req.width;
			if (req.height > max_height)
				max_height = req.height;

			if (LADSPA_IS_HINT_SAMPLE_RATE(hints[k].HintDescriptor)) {
				fact = out_SR;
			} else {
				fact = 1.0f;
			}
			
			if (LADSPA_IS_HINT_BOUNDED_BELOW(hints[k].HintDescriptor)) {
				min = hints[k].LowerBound * fact;
			} else {
				min = -10000.0f;
			}
			
			if (LADSPA_IS_HINT_BOUNDED_ABOVE(hints[k].HintDescriptor)) {
				max = hints[k].UpperBound * fact;
			} else {
				max = 10000.0f;
			}
			
			/* infinity */
			if (10000.0f <= max - min) {
				dp = 1;
				step = 5.0f;
				
			/* 100.0 ... lots */
			} else if (100.0f < max - min) {
				dp = 0;
				step = 1.0f;
				
			/* 10.0 ... 100.0 */
			} else if (10.0f < max - min) {
				dp = 1;
				step = 0.1f;
				
			/* 1.0 ... 10.0 */
			} else if (1.0f < max - min) {
				dp = 2;
				step = 0.01f;
				
			/* 0.0 ... 1.0 */
			} else {
				dp = 3;
				step = 0.001f;
			}
			
			if (LADSPA_IS_HINT_INTEGER(hints[k].HintDescriptor)) {
				dp = 0;
				if (step < 1.0f) step = 1.0f;
			}
			
			if (LADSPA_IS_HINT_DEFAULT_MINIMUM(hints[k].HintDescriptor)) {
				start = min;
			} else if (LADSPA_IS_HINT_DEFAULT_LOW(hints[k].HintDescriptor)) {
				start = min * 0.75f + max * 0.25f;
			} else if (LADSPA_IS_HINT_DEFAULT_MIDDLE(hints[k].HintDescriptor)) {
				start = min * 0.5f + max * 0.5f;
			} else if (LADSPA_IS_HINT_DEFAULT_HIGH(hints[k].HintDescriptor)) {
				start = min * 0.25f + max * 0.75f;
			} else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(hints[k].HintDescriptor)) {
				start = max;
			} else if (LADSPA_IS_HINT_DEFAULT_0(hints[k].HintDescriptor)) {
				start = 0.0f;
			} else if (LADSPA_IS_HINT_DEFAULT_1(hints[k].HintDescriptor)) {
				start = 1.0f;
			} else if (LADSPA_IS_HINT_DEFAULT_100(hints[k].HintDescriptor)) {
				start = 100.0f;
			} else if (LADSPA_IS_HINT_DEFAULT_440(hints[k].HintDescriptor)) {
				start = 440.0f;
			} else if (LADSPA_IS_HINT_INTEGER(hints[k].HintDescriptor)) {
				start = min;
			} else if (max >= 0.0f && min <= 0.0f) {
				start = 0.0f;
			} else {
				start = min * 0.5f + max * 0.5f;
			}
			
			instance->knobs[k] = start;
			
			adjustment = gtk_adjustment_new(start, min, max, step, step * 50.0, 0.0);
			instance->adjustments[k] = GTK_ADJUSTMENT(adjustment);
			
			if (!LADSPA_IS_HINT_INTEGER(hints[k].HintDescriptor)) {
				widget = gtk_hscale_new(GTK_ADJUSTMENT(adjustment));
				gtk_widget_set_name(widget, "plugin_scale");
				gtk_widget_set_size_request(widget, 200, -1);
				gtk_scale_set_digits(GTK_SCALE(widget), dp);
				gtk_table_attach(GTK_TABLE(table), widget, 1, 2, i, i+1,
						 GTK_FILL | GTK_EXPAND, GTK_FILL, 2, 2);
				gtk_scale_set_draw_value(GTK_SCALE(widget), FALSE);
				gtk_widget_set_sensitive(widget, FALSE);
				gtk_widget_size_request(widget, &req);
				req.height += 2;
				if (req.height > max_height)
					max_height = req.height;
			}

			widget = gtk_spin_button_new(GTK_ADJUSTMENT(adjustment), step, dp);
			gtk_widget_set_size_request(widget, 70, -1);
			gtk_widget_set_sensitive(widget, FALSE);
			gtk_table_attach(GTK_TABLE(table), widget, 2, 3, i, i+1,
					 GTK_FILL, GTK_FILL, 2, 2);
			gtk_widget_size_request(widget, &req);
			req.height += 2;
			if (req.height > max_height)
				max_height = req.height;
			height += max_height;
			++i;
		}
	}


	if ((!n_toggled) && (!n_untoggled) && (!n_outctl) && (!n_outlat)) {
		widget = gtk_label_new("This LADSPA plugin has no user controls");
		gtk_box_pack_start(GTK_BOX(inner_vbox), widget, TRUE, TRUE, 2);
		gtk_widget_size_request(widget, &req);
		gtk_widget_set_size_request(scrwin, req.width + 20, req.height + 20);
	} else {
		gtk_widget_set_size_request(scrwin, (max_width + 280) * 1.1,
					    (height > 500) ? 500 : height * 1.1 + 10);
	}

	if ((n_outctl) || (n_outlat)) {
		instance->timeout = g_timeout_add(100, update_plugin_outputs, instance);
	} else {
		instance->timeout = 0;
	}

        g_signal_connect(G_OBJECT(instance->window), "delete_event", G_CALLBACK(close_plugin_window), NULL);
}


void
foreach_plugin_to_add(GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, gpointer data) {

	GtkTreeIter running_iter;
	int n_ins;
	int n_outs;
	char filename[MAXLEN];
	int index;
	char * str_n_ins;
	char * str_n_outs;
	char * str_filename;
	char * str_index;
	plugin_instance * instance;
	char bypassed_name[MAXLEN];
	

	gtk_tree_model_get(GTK_TREE_MODEL(avail_store), iter, 3, &str_n_ins,
			   4, &str_n_outs, 5, &str_filename, 6, &str_index, -1);
	
	sscanf(str_n_ins, "%d", &n_ins);
	sscanf(str_n_outs, "%d", &n_outs);
	strncpy(filename, str_filename, MAXLEN-1);
	sscanf(str_index, "%d", &index);
	
	if (((n_ins == 1) && (n_outs == 1)) ||
	    ((n_ins == 2) && (n_outs == 2))) {
		
		if (n_plugins >= MAX_PLUGINS) {
			fprintf(stderr,
				"Maximum number of running plugin instances (%d) reached; "
				"cannot add more.\n", MAX_PLUGINS);
			return;
		}
		instance = instantiate(filename, index);
		if (instance) {
			connect_port(instance);
			activate(instance);
			build_plugin_window(instance);
			
			get_bypassed_name(instance, bypassed_name);
			added_plugin = 1; /* so resort handler will not do any harm */
			gtk_list_store_append(running_store, &running_iter);
			gtk_list_store_set(running_store, &running_iter,
					   0, bypassed_name, 1, (gpointer)instance, -1);

			pthread_mutex_lock(&plugin_lock);
			++n_plugins;
			refresh_plugin_vect();
			pthread_mutex_unlock(&plugin_lock);
		}
	} else {
		fprintf(stderr,
			"cannot add %s:%d -- it has %d ins and %d outs.\n",
			filename, index, n_ins, n_outs);
	}
}


gint
add_clicked(GtkWidget * widget, GdkEvent * event, gpointer data) {

	gtk_tree_selection_selected_foreach(avail_select, foreach_plugin_to_add, NULL);

	return TRUE;
}


gint
remove_clicked(GtkWidget * widget, GdkEvent * event, gpointer data) {

	GtkTreeIter iter;
	gpointer gp_instance;
	plugin_instance * instance;

	if (gtk_tree_selection_get_selected(running_select, NULL, &iter)) {

                gtk_tree_model_get(GTK_TREE_MODEL(running_store), &iter, 1, &gp_instance, -1);
		gtk_list_store_remove(running_store, &iter);
		pthread_mutex_lock(&plugin_lock);
		--n_plugins;
		refresh_plugin_vect();
		pthread_mutex_unlock(&plugin_lock);

		instance = (plugin_instance *) gp_instance;
		if (instance->handle) {
			if (instance->descriptor->deactivate) {
				instance->descriptor->deactivate(instance->handle);
			}
			instance->descriptor->cleanup(instance->handle);
			instance->handle = NULL;
		}
		if (instance->handle2) {
			if (instance->descriptor->deactivate) {
				instance->descriptor->deactivate(instance->handle2);
			}
			instance->descriptor->cleanup(instance->handle2);
			instance->handle2 = NULL;
		}
		if (instance->timeout)
			g_source_remove(instance->timeout);
		if (instance->window)
			gtk_widget_destroy(instance->window);

		trashlist_free(instance->trashlist);
		free(instance);
	}
	return TRUE;
}


gint
conf_clicked(GtkWidget * widget, GdkEvent * event, gpointer data) {

        GtkTreeIter iter;
        gpointer gp_instance;
        plugin_instance * instance;

        if (gtk_tree_selection_get_selected(running_select, NULL, &iter)) {

                gtk_tree_model_get(GTK_TREE_MODEL(running_store), &iter, 1, &gp_instance, -1);
                instance = (plugin_instance *) gp_instance;
		if (instance->window)
			gtk_widget_show_all(instance->window);
	}

	return TRUE;
}



gint
running_list_key_pressed(GtkWidget * widget, GdkEventKey * event) {

	switch (event->keyval) {
	case GDK_Delete:
	case GDK_KP_Delete:
		remove_clicked(NULL, NULL, NULL);
		return TRUE;
		break;
	}

	return FALSE;
}


gint
running_list_button_pressed(GtkWidget * widget, GdkEventButton * event) {

	GtkTreeIter iter;
        GtkTreePath * path;
        GtkTreeViewColumn * column;
	gpointer gp_instance;
	plugin_instance * instance;

	if (event->type == GDK_BUTTON_PRESS && event->button == 2) {

                if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(running_list), event->x, event->y,
                                                  &path, &column, NULL, NULL)) {

			gtk_tree_view_set_cursor(GTK_TREE_VIEW(running_list), path, NULL, FALSE);
			gtk_tree_selection_get_selected(running_select, NULL, &iter);

			gtk_tree_model_get(GTK_TREE_MODEL(running_store), &iter, 1, &gp_instance, -1);
			instance = (plugin_instance *) gp_instance;
			if (instance->bypass_button) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(instance->bypass_button),
				       !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
									     instance->bypass_button)));
			}
		}
	}

	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		conf_clicked(NULL, NULL, NULL);
	}

	return FALSE;
}


gint
refresh_on_list_changed_cb(gpointer data) {

	pthread_mutex_lock(&plugin_lock);
	refresh_plugin_vect();
	pthread_mutex_unlock(&plugin_lock);

	return FALSE;
}


void
running_list_row_inserted(GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter) {

	if (added_plugin) {
		added_plugin = 0;
		return;
	}
	g_timeout_add(100, refresh_on_list_changed_cb, NULL);
}


gint
avail_key_pressed(GtkWidget * widget, GdkEventKey * event) {

	switch (event->keyval) {
	case GDK_a:
	case GDK_A:
		add_clicked(NULL, NULL, NULL);
		return TRUE;
		break;
	}

	return FALSE;
}


gint
avail_dblclicked(GtkWidget * widget, GdkEventButton * event) {

	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		add_clicked(NULL, NULL, NULL);
		return TRUE;
	}

	return FALSE;
}


void
create_fxbuilder(void) {

	GtkWidget * hbox;
	GtkWidget * vbox;
	GtkWidget * frame_avail;
	GtkWidget * viewport_avail;
	GtkWidget * scrolled_win_avail;

	GtkWidget * frame_running;
	GtkWidget * viewport_running;
	GtkWidget * scrolled_win_running;

	GtkWidget * hbox_buttons;

        GtkCellRenderer * renderer;
        GtkTreeViewColumn * column;

        /* window creating stuff */
        fxbuilder_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(fxbuilder_window), _("LADSPA patch builder"));
	gtk_window_set_position(GTK_WINDOW(fxbuilder_window), GTK_WIN_POS_CENTER);
        g_signal_connect(G_OBJECT(fxbuilder_window), "delete_event", G_CALLBACK(fxbuilder_close), NULL);
        g_signal_connect(G_OBJECT(fxbuilder_window), "key_press_event", G_CALLBACK(fxbuilder_key_pressed), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(fxbuilder_window), 2);

	hbox = gtk_hbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(fxbuilder_window), hbox);

        frame_avail = gtk_frame_new(_("Available plugins"));
        gtk_box_pack_start(GTK_BOX(hbox), frame_avail, TRUE, TRUE, 5);

	vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);
        gtk_container_add(GTK_CONTAINER(frame_avail), vbox);

	viewport_avail = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), viewport_avail, TRUE, TRUE, 3);
	
	add_button = gtk_button_new_with_label(_("Add"));
        g_signal_connect(add_button, "clicked", G_CALLBACK(add_clicked), NULL);
        gtk_box_pack_start(GTK_BOX(vbox), add_button, FALSE, TRUE, 3);

	scrolled_win_avail = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win_avail),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(viewport_avail), scrolled_win_avail);

	/* create store of available plugins */
	if (!avail_store) {
		avail_store = gtk_list_store_new(7,
						 G_TYPE_STRING,  /* 0: ID */
						 G_TYPE_STRING,  /* 1: Name */
						 G_TYPE_STRING,  /* 2: category */
						 G_TYPE_STRING,  /* 3: n_ins */
						 G_TYPE_STRING,  /* 4: n_outs */
						 G_TYPE_STRING,  /* 5: filename */
						 G_TYPE_STRING); /* 6: index */

		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(avail_store), 1, GTK_SORT_ASCENDING);

		gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(avail_store), 0, compare_func,
						(gpointer) 0, NULL);
		gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(avail_store), 1, compare_func,
						(gpointer) 1, NULL);
		gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(avail_store), 2, compare_func,
						(gpointer) 2, NULL);
		gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(avail_store), 3, compare_func,
						(gpointer) 3, NULL);
		gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(avail_store), 4, compare_func,
						(gpointer) 4, NULL);

		/* fill avail_store with data */
		parse_lrdf_data();
		find_all_plugins();
	}


	avail_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(avail_store));
        gtk_widget_set_size_request(avail_list, 400, 300);
	gtk_container_add(GTK_CONTAINER(scrolled_win_avail), avail_list);

        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(avail_list), TRUE);

	g_signal_connect(G_OBJECT(avail_list), "key_press_event", G_CALLBACK(avail_key_pressed), NULL);
	g_signal_connect(G_OBJECT(avail_list), "button_press_event", G_CALLBACK(avail_dblclicked), NULL);


        avail_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(avail_list));
        gtk_tree_selection_set_mode(avail_select, GTK_SELECTION_MULTIPLE);

	renderer = gtk_cell_renderer_text_new();

	column = gtk_tree_view_column_new_with_attributes(_("ID"), renderer, "text", 0, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(avail_list), column);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(column), 0);

	column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer, "text", 1, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(avail_list), column);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(column), 1);

	column = gtk_tree_view_column_new_with_attributes(_("Category"), renderer, "text", 2, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(avail_list), column);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(column), 2);

	column = gtk_tree_view_column_new_with_attributes(_("Inputs"), renderer, "text", 3, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(avail_list), column);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(column), 3);

	column = gtk_tree_view_column_new_with_attributes(_("Outputs"), renderer, "text", 4, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(avail_list), column);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(column), 4);


        frame_running = gtk_frame_new(_("Running plugins"));
        gtk_box_pack_start(GTK_BOX(hbox), frame_running, TRUE, TRUE, 5);

	vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);
        gtk_container_add(GTK_CONTAINER(frame_running), vbox);

	viewport_running = gtk_viewport_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), viewport_running, TRUE, TRUE, 3);

	hbox_buttons = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_buttons, FALSE, TRUE, 3);

	remove_button = gtk_button_new_with_label(_("Remove"));
        g_signal_connect(remove_button, "clicked", G_CALLBACK(remove_clicked), NULL);
	gtk_box_pack_start(GTK_BOX(hbox_buttons), remove_button, TRUE, TRUE, 0);

	conf_button = gtk_button_new_with_label(_("Configure"));
        g_signal_connect(conf_button, "clicked", G_CALLBACK(conf_clicked), NULL);
	gtk_box_pack_start(GTK_BOX(hbox_buttons), conf_button, TRUE, TRUE, 0);

	scrolled_win_running = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win_running),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(viewport_running), scrolled_win_running);

	/* create store of running plugins */
	if (!running_store) {
		running_store = gtk_list_store_new(2,
						   G_TYPE_STRING,   /* Name */
						   G_TYPE_POINTER); /* instance */

		g_signal_connect(G_OBJECT(running_store), "row_inserted",
				 G_CALLBACK(running_list_row_inserted), NULL);
	}

	running_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(running_store));
        gtk_widget_set_size_request(running_list, 200, 300);
	gtk_container_add(GTK_CONTAINER(scrolled_win_running), running_list);

        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(running_list), TRUE);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(running_list), TRUE);

        g_signal_connect(G_OBJECT(running_list), "key_press_event",
			 G_CALLBACK(running_list_key_pressed), NULL);
        g_signal_connect(G_OBJECT(running_list), "button_press_event",
			 G_CALLBACK(running_list_button_pressed), NULL);
	

        running_select = gtk_tree_view_get_selection(GTK_TREE_VIEW(running_list));
        gtk_tree_selection_set_mode(running_select, GTK_SELECTION_SINGLE);

	renderer = gtk_cell_renderer_text_new();

	column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer, "text", 0, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(running_list), column);

}


void
save_plugin_data(void) {

        int i = 0;
	int k;
        GtkTreeIter iter;
        gpointer gp_instance;
	plugin_instance * instance;
        xmlDocPtr doc;
        xmlNodePtr root;
        xmlNodePtr plugin_node;
        xmlNodePtr port_node;
        char c, d;
        FILE * fin;
        FILE * fout;
        char tmpname[MAXLEN];
        char plugin_file[MAXLEN];
        char str[32];


        sprintf(plugin_file, "%s/plugin.xml", confdir);

        doc = xmlNewDoc("1.0");
        root = xmlNewNode(NULL, "aqualung_plugin");
        xmlDocSetRootElement(doc, root);

        while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(running_store), &iter, NULL, i)) {

                gtk_tree_model_get(GTK_TREE_MODEL(running_store), &iter, 1, &gp_instance, -1);
                instance = (plugin_instance *) gp_instance;

		plugin_node = xmlNewTextChild(root, NULL, "plugin", NULL);

		xmlNewTextChild(plugin_node, NULL, "filename", instance->filename);

		snprintf(str, 31, "%d", instance->index);
		xmlNewTextChild(plugin_node, NULL, "index", str);

		snprintf(str, 31, "%d", instance->is_bypassed);
		xmlNewTextChild(plugin_node, NULL, "is_bypassed", str);

                for (k = 0; k < MAX_KNOBS && k < instance->descriptor->PortCount; ++k) {

                        if (!LADSPA_IS_PORT_CONTROL(instance->descriptor->PortDescriptors[k]))
                                continue;
                        if (LADSPA_IS_PORT_OUTPUT(instance->descriptor->PortDescriptors[k]))
                                continue;

			port_node = xmlNewTextChild(plugin_node, NULL, "port", NULL);

			snprintf(str, 31, "%d", k);
			xmlNewTextChild(port_node, NULL, "index", str);

			snprintf(str, 31, "%f", instance->knobs[k]);
			xmlNewTextChild(port_node, NULL, "value", str);
		}
                ++i;
        }

        sprintf(tmpname, "%s/plugin.xml.temp", confdir);
        xmlSaveFormatFile(tmpname, doc, 1);

        if ((fin = fopen(plugin_file, "rt")) == NULL) {
                fprintf(stderr, "Error opening file: %s\n", plugin_file);
                return;
        }
        if ((fout = fopen(tmpname, "rt")) == NULL) {
                fprintf(stderr, "Error opening file: %s\n", tmpname);
                return;
        }

        c = 0; d = 0;
        while (((c = fgetc(fin)) != EOF) && ((d = fgetc(fout)) != EOF)) {
                if (c != d) {
                        fclose(fin);
                        fclose(fout);
                        unlink(plugin_file);
                        rename(tmpname, plugin_file);
                        return;
                }
        }

        fclose(fin);
        fclose(fout);
        unlink(tmpname);
}


void
parse_plugin(xmlDocPtr doc, xmlNodePtr cur) {

        xmlChar * key;
	int k;
        char filename[MAXLEN];
        int index = -1;
        int is_bypassed;
	GtkTreeIter running_iter;
	plugin_instance * instance = NULL;
        char bypassed_name[MAXLEN];
        LADSPA_Data knobs[MAX_KNOBS];

        filename[0] = '\0';

        cur = cur->xmlChildrenNode;
        while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"filename"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(filename, key, MAXLEN-1);
                        xmlFree(key);
                        if (filename[0] == '\0') {
                                fprintf(stderr, "Error in XML aqualung_plugin: "
                                       "plugin <filename> is required, but NULL\n");
                        }
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"index"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf(key, "%d", &index);
                        xmlFree(key);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"is_bypassed"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                sscanf(key, "%d", &is_bypassed);
                        xmlFree(key);
                } else if ((!xmlStrcmp(cur->name, (const xmlChar *)"port"))) {
			int port_index = -1;
			float port_value = 0.0f;
			xmlNodePtr port_node = cur->xmlChildrenNode;

			while (port_node != NULL) {
				if ((!xmlStrcmp(port_node->name, (const xmlChar *)"index"))) {
					key = xmlNodeListGetString(doc, port_node->xmlChildrenNode, 1);
					if (key != NULL)
						sscanf(key, "%d", &port_index);
					xmlFree(key);
				} else if ((!xmlStrcmp(port_node->name, (const xmlChar *)"value"))) {
					key = xmlNodeListGetString(doc, port_node->xmlChildrenNode, 1);
					if (key != NULL)
						sscanf(key, "%f", &port_value);
					xmlFree(key);
				}
				port_node = port_node->next;
			}

			if ((port_index >= 0) && (port_index < MAX_KNOBS)) {
				knobs[port_index] = port_value;
			}
                }
                cur = cur->next;
        }

	
	if ((filename[0] != '\0') && (index >= 0)) { /* create plugin, restore settings */
		
		if (n_plugins >= MAX_PLUGINS) {
			fprintf(stderr,
				"Maximum number of running plugin instances (%d) reached; "
				"cannot add more.\n", MAX_PLUGINS);
			return;
		}
		instance = instantiate(filename, index);
		if (instance) {
			connect_port(instance);
			activate(instance);
			for (k = 0; k < MAX_KNOBS && k < instance->descriptor->PortCount; ++k) {
				if (!LADSPA_IS_PORT_CONTROL(instance->descriptor->PortDescriptors[k]))
					continue;
				if (LADSPA_IS_PORT_OUTPUT(instance->descriptor->PortDescriptors[k]))
					continue;
				instance->knobs[k] = knobs[k];
			}
			instance->is_restored = 1;
			instance->is_bypassed = is_bypassed;
			build_plugin_window(instance);
			
			get_bypassed_name(instance, bypassed_name);
			added_plugin = 1; /* so resort handler will not do any harm */
			gtk_list_store_append(running_store, &running_iter);
			gtk_list_store_set(running_store, &running_iter,
					   0, bypassed_name, 1, (gpointer)instance, -1);

			pthread_mutex_lock(&plugin_lock);
			++n_plugins;
			refresh_plugin_vect();
			pthread_mutex_unlock(&plugin_lock);
		}
	}
	return;
}


void
load_plugin_data(void) {

        xmlDocPtr doc;
        xmlNodePtr cur;
        xmlNodePtr root;
        char plugin_file[MAXLEN];
        FILE * f;

        sprintf(plugin_file, "%s/plugin.xml", confdir);

        if ((f = fopen(plugin_file, "rt")) == NULL) {
                fprintf(stderr, "No plugin.xml -- creating empty one: %s\n", plugin_file);
                doc = xmlNewDoc("1.0");
                root = xmlNewNode(NULL, "aqualung_plugin");
                xmlDocSetRootElement(doc, root);
                xmlSaveFormatFile(plugin_file, doc, 1);
                return;
        }
        fclose(f);

        doc = xmlParseFile(plugin_file);
        if (doc == NULL) {
                fprintf(stderr, "An XML error occured while parsing %s\n", plugin_file);
                return;
        }

        cur = xmlDocGetRootElement(doc);
        if (cur == NULL) {
                fprintf(stderr, "load_config: empty XML document\n");
                xmlFreeDoc(doc);
                return;
        }

        if (xmlStrcmp(cur->name, (const xmlChar *)"aqualung_plugin")) {
                fprintf(stderr,
			"load_config: XML document of the wrong type, "
                        "root node != aqualung_plugin\n");
                xmlFreeDoc(doc);
                return;
        }

        cur = cur->xmlChildrenNode;
        while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"plugin"))) {
			parse_plugin(doc, cur);
                }
                cur = cur->next;
        }

        xmlFreeDoc(doc);
        return;
}
