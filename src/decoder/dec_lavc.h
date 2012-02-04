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

#ifndef AQUALUNG_DEC_LAVC_H
#define AQUALUNG_DEC_LAVC_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "../rb.h"
#include "file_decoder.h"


#define RB_LAVC_SIZE (3*AVCODEC_MAX_AUDIO_FRAME_SIZE)


typedef struct _lavc_pdata_t {
        rb_t * rb;
	AVFormatContext * avFormatCtx;
	AVCodecContext * avCodecCtx;
	AVCodec * avCodec;
	AVRational time_base;
	int audioStream;
	int is_eos;
} lavc_pdata_t;


decoder_t * lavc_decoder_init(file_decoder_t * fdec);
void lavc_decoder_destroy(decoder_t * dec);
int lavc_decoder_open(decoder_t * dec, char * filename);
void lavc_decoder_send_metadata(decoder_t * dec);
void lavc_decoder_close(decoder_t * dec);
unsigned int lavc_decoder_read(decoder_t * dec, float * dest, int num);
void lavc_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);


#endif /* AQUALUNG_DEC_LAVC_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

