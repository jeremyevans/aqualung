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

#ifndef _PLUGIN_H
#define _PLUGIN_H

#include <gtk/gtk.h>

#include <ladspa.h>

#define MAX_PLUGINS 128
#define MAX_KNOBS 128

typedef struct {
	char filename[MAXLEN];
	int index;
	void * library;
	int is_restored;
	int is_mono;
	int is_bypassed;
	int shift_pressed;
	const LADSPA_Descriptor * descriptor;
	LADSPA_Handle * handle;
	LADSPA_Handle * handle2;
	GtkWidget * window;
	GtkWidget * bypass_button;
	gint timeout;
	GtkAdjustment * adjustments[MAX_KNOBS];
	LADSPA_Data knobs[MAX_KNOBS];
} plugin_instance;




void create_fxbuilder(void);
void show_fxbuilder(void);
void hide_fxbuilder(void);
void save_plugin_data(void);
void load_plugin_data(void);


#endif /* _PLUGIN_H */
