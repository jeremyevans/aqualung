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

#ifndef AQUALUNG_VOLUME_H
#define AQUALUNG_VOLUME_H

#include <glib.h>
#include <gtk/gtk.h>

#include "athread.h"
#include "decoder/file_decoder.h"


#define RMSSIZE 100

#define VOLUME_SEPARATE 0
#define VOLUME_AVERAGE  1

typedef struct {
        float buffer[RMSSIZE];
        unsigned int pos;
        float sum;
} rms_env_t;


typedef struct {
	GtkTreeIter iter;
	char * file;
} vol_item_t;

typedef struct {

	GtkTreeStore * store;
	GList * queue;
	int update_tag;
	int cancelled;
	int paused;
	int window_visible;
	int type;

	AQUALUNG_THREAD_DECLARE(thread_id)
	AQUALUNG_MUTEX_DECLARE(thread_mutex)
	AQUALUNG_MUTEX_DECLARE(wait_mutex)
	AQUALUNG_COND_DECLARE(thread_wait)

	GtkWidget * slot;
	GtkWidget * progress;
	GtkWidget * pause_button;
	GtkWidget * cancel_button;
	GtkWidget * file_entry;

	vol_item_t * item;
	file_decoder_t * fdec;
	unsigned long chunk_size;
	unsigned long n_chunks;
	unsigned long chunks_read;
	float result;
	rms_env_t * rms;

	float * volumes;
	unsigned int n_volumes;

} volume_t;

volume_t * volume_new(GtkTreeStore * store, int type);
void volume_push(volume_t * vol, char * file, GtkTreeIter iter);
void volume_start(volume_t * vol);

void voladj2str(float voladj, char * str, size_t str_size);

float rva_from_volume(float volume);
float rva_from_replaygain(float rg);
float rva_from_multiple_volumes(int nlevels, float * volumes);


#endif /* AQUALUNG_VOLUME_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
