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


#ifndef _MUSIC_BROWSER_H
#define _MUSIC_BROWSER_H


void create_music_browser(void);
void show_music_browser(void);
void hide_music_browser(void);

void load_music_store(void);
void save_music_store(void);

void artist__addlist_cb(gpointer data);
void record__addlist_cb(gpointer data);
void track__addlist_cb(gpointer data);

#endif
