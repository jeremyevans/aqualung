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

#ifndef _EXPORT_H
#define _EXPORT_H

#include "common.h"

typedef struct {

	AQUALUNG_THREAD_DECLARE(thread_id);
	AQUALUNG_MUTEX_DECLARE(mutex);

	GSList * slist;

	char outdir[MAXLEN];
	char template[MAXLEN];
	int dir_for_artist;
	int dir_for_album;
	int dir_len_limit;

	int format;
	int bitrate;
	int vbr;
	int write_meta;

	int cancelled;
	int progbar_tag;
	char file1[MAXLEN];
	char file2[MAXLEN];
	double ratio;

	GtkWidget * dialog;
	GtkWidget * format_combo;
	GtkWidget * check_dir_artist;
	GtkWidget * check_dir_album;
	GtkWidget * dirlen_spin;
	GtkWidget * bitrate_scale;
	GtkWidget * bitrate_label;
	GtkWidget * bitrate_value_label;
	GtkWidget * vbr_check;
	GtkWidget * meta_check;

	GtkWidget * prog_window;
	GtkWidget * prog_file_entry1;
	GtkWidget * prog_file_entry2;
	GtkWidget * prog_cancel_button;
	GtkWidget * progbar;

} export_t;

export_t * export_new(void);
void export_append_item(export_t * export, char * infile,
			char * artist, char * album, char * title, int year, int no);
void export_start(export_t * export);

#endif /* _EXPORT_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
