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


#ifndef _GUI_MAIN_H
#define _GUI_MAIN_H


#include <gtk/gtk.h>

#include "core.h"


#ifdef __cplusplus
extern "C" {
#endif


#define PLAY  1
#define PAUSE 2


gint aqualung_dialog_run(GtkDialog * dialog);
void deflicker(void);

void pack_strings(char * str1, char * str2, char * result);
void unpack_strings(char * packed, char * str1, char * str2);
void normalize_filename(const char * in, char * out);

void try_waking_disk_thread(void);
void toggle_noeffect(int id, int state);
void cue_track_for_playback(GtkTreeStore * store, GtkTreeIter * piter, cue_t * cue);

void create_gui(int argc, char ** argv, int optind, int enqueue,
		unsigned long rate, unsigned long rb_audio_size);

void run_gui(void);

void sample2time(unsigned long SR, unsigned long long sample, char * str, int sign);
void time2time(float samples, char * str);
void time2time_na(float seconds, char * str);
void format_bps_label(int bps, int format_flags, char * str);

void refresh_displays(void);

void save_window_position(void);
void restore_window_position(void);
void change_skin(char * skin_path);

void set_src_type_label(int src_type);

gint scroll_btn_pressed(GtkWidget * widget, GdkEventButton * event);
gint scroll_btn_released(GtkWidget * widget, GdkEventButton * event, gpointer * win);
gint scroll_motion_notify(GtkWidget * widget, GdkEventMotion * event, gpointer * win);

GtkWidget* gui_stock_label_button(gchar *label, const gchar *stock);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _GUI_MAIN_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
