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

#ifndef AQUALUNG_DEC_VORBIS_H
#define AQUALUNG_DEC_VORBIS_H

#include <vorbis/codec.h>
#ifdef _WIN32
#undef _WIN32
#include <vorbis/vorbisfile.h>
#define _WIN32
#else
#include <vorbis/vorbisfile.h>
#endif /* _WIN32 */

#include "../httpc.h"
#include "../rb.h"
#include "file_decoder.h"


/* Decoding buffer size for Vorbis */
#define VORBIS_BUFSIZE 4096
/* size of ringbuffer for decoded Vorbis data (in frames) */
#define RB_VORBIS_SIZE 262144


typedef struct _vorbis_pdata_t {
        rb_t * rb;
        FILE * vorbis_file;
        OggVorbis_File vf;
        vorbis_info * vi;
        int is_eos;
	http_session_t * session;
} vorbis_pdata_t;


decoder_t * vorbis_decoder_init(file_decoder_t * fdec);
void vorbis_decoder_destroy(decoder_t * dec);
int vorbis_decoder_open(decoder_t * dec, char * filename);
void vorbis_decoder_send_metadata(decoder_t * dec);
int vorbis_stream_decoder_open(decoder_t * dec, http_session_t * session);
void vorbis_decoder_close(decoder_t * dec);
unsigned int vorbis_decoder_read(decoder_t * dec, float * dest, int num);
void vorbis_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);


#endif /* AQUALUNG_DEC_VORBIS_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

