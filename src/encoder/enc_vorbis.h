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


#ifndef _ENC_VORBIS_H
#define _ENC_VORBIS_H

#include <stdio.h>
#ifdef HAVE_VORBISENC
#include <vorbis/vorbisenc.h>
#endif /* HAVE_VORBISENC */

#include "file_encoder.h"


#ifdef HAVE_VORBISENC

#define VORBISENC_READ 1024
#define RB_VORBISENC_SIZE (1<<18)

typedef struct _vorbisenc_pencdata_t {
	FILE * out;
	rb_t * rb;
	int eos;
	int channels;
	ogg_stream_state os;
	ogg_page og;
	ogg_packet op;
	vorbis_info vi;
	vorbis_comment vc;
	vorbis_dsp_state vd;
	vorbis_block vb;
} vorbisenc_pencdata_t;
#endif /* HAVE_VORBISENC */


encoder_t * vorbisenc_encoder_init(file_encoder_t * fenc);
#ifdef HAVE_VORBISENC
void vorbisenc_encoder_destroy(encoder_t * enc);
int vorbisenc_encoder_open(encoder_t * enc, encoder_mode_t * mode);
void vorbisenc_encoder_close(encoder_t * enc);
unsigned int vorbisenc_encoder_write(encoder_t * enc, float * data, int num);
#endif /* HAVE_VORBISENC */


#endif /* _ENC_VORBIS_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

