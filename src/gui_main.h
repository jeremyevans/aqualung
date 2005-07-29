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

/* maximum length of font description */
#define MAX_FONTNAME_LEN 256

void deflicker(void);

void create_gui(int argc, char ** argv, int optind, int enqueue,
		unsigned long rate, unsigned long rb_audio_size);

void run_gui(void);

void sample2time(unsigned long SR, unsigned long long sample, char * str, int sign);
void time2time(float samples, char * str);
void assembly_format_label(char * str, int v_major, int v_minor);

void save_window_position(void);
void restore_window_position(void);
void change_skin(char * skin_path);

void set_src_type_label(int src_type);


#endif /* _GUI_MAIN_H */
