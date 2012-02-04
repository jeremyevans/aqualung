/*                                                     -*- linux-c -*-
    Copyright (C) 2007 Tom Szilagyi

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

#ifndef AQUALUNG_DEC_WAVPACK_H
#define AQUALUNG_DEC_WAVPACK_H

#include <wavpack/wavpack.h>

#include "../rb.h"
#include "file_decoder.h"


#define RB_WAVPACK_SIZE 262144
#define WAVPACK_BUFSIZE 4096


typedef struct _wavpack_pdata_t {
	WavpackContext *wpc;
	rb_t * rb;
	int flags;
	char error[100];
	int last_decoded_samples;
	int bits_per_sample;
	int end_of_file;
	float scale_factor_float;
} wavpack_pdata_t;


void wavpack_decoder_destroy(decoder_t * dec);
int wavpack_decoder_open(decoder_t * dec, char * filename);
void wavpack_decoder_send_metadata(decoder_t * dec);
void wavpack_decoder_close(decoder_t * dec);
unsigned int wavpack_decoder_read(decoder_t * dec, float * dest, int num);
void wavpack_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);


decoder_t * wavpack_decoder_init(file_decoder_t * fdec);


#endif /* AQUALUNG_DEC_WAVPACK_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
