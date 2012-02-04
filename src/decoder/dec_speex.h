/*                                                     -*- linux-c -*-
    Copyright (C) 2005 Tom Szilagyi

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

#ifndef AQUALUNG_DEC_SPEEX_H
#define AQUALUNG_DEC_SPEEX_H

#include <stdio.h>
#include <oggz/oggz.h>
#include <speex/speex.h>

#include "../rb.h"
#include "file_decoder.h"


/* Decoding buffer size for Speex */
#define SPEEX_BUFSIZE 1280
/* size of ringbuffer for decoded Speex data (in frames) */
#define RB_SPEEX_SIZE 262144


typedef struct _speex_pdata_t {
        rb_t * rb;
        FILE * speex_file;

	OGGZ * oggz;
	SpeexBits bits;
	const SpeexMode * mode;
	void * decoder;

	int sample_rate;
	int channels;
	int vbr;
	int nframes;
	int frame_size;

	int exploring;
	int error;
	long packetno;
	int granulepos;

        int is_eos;
} speex_pdata_t;


decoder_t * speex_dec_init(file_decoder_t * fdec);
void speex_dec_destroy(decoder_t * dec);
int speex_dec_open(decoder_t * dec, char * filename);
void speex_dec_send_metadata(decoder_t * dec);
void speex_dec_close(decoder_t * dec);
unsigned int speex_dec_read(decoder_t * dec, float * dest, int num);
void speex_dec_seek(decoder_t * dec, unsigned long long seek_to_pos);


#endif /* AQUALUNG_DEC_SPEEX_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

