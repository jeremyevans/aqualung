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


#ifndef _PLAYLIST_H
#define _PLAYLIST_H

#include <config.h>

void voladj2str(float voladj, char * str);
GtkTreePath * get_playing_path(GtkTreeStore * store);
void delayed_playlist_rearrange(int delay);
gint playlist_size_allocate(GtkWidget * widget, GdkEventConfigure * event);
void create_playlist(void);
void show_playlist(void);
void hide_playlist(void);
void set_playlist_color(void);
void save_playlist(char * filename);
void load_playlist(char * filename, int enqueue);
void load_m3u(char * filename, int enqueue);
void load_pls(char * filename, int enqueue);
gint is_playlist(char * filename);
void add_to_playlist(char * filename, int enqueue);
void playlist_content_changed(void);
void playlist_drag_end(GtkWidget * widget, GdkDragContext * drag_context, gpointer data);
gint playlist_window_key_pressed(GtkWidget * widget, GdkEventKey * kevent);
void playlist_foreach_selected(void (* foreach)(GtkTreeIter *, void *), void * data);

#ifdef HAVE_CDDA
void playlist_add_cdda(GtkTreeIter * iter_drive, unsigned long hash);
void playlist_remove_cdda(char * device_path);
#endif /* HAVE_CDDA */

enum {
    COLUMN_TRACK_NAME = 0,          
    COLUMN_PHYSICAL_FILENAME,       
    COLUMN_SELECTION_COLOR,         
    COLUMN_VOLUME_ADJUSTMENT,       
    COLUMN_VOLUME_ADJUSTMENT_DISP,  
    COLUMN_DURATION,                
    COLUMN_DURATION_DISP,           
    COLUMN_FONT_WEIGHT,             

    NUMBER_OF_COLUMNS               /* it must be the last entry here */
};

#endif /* _PLAYLIST_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
