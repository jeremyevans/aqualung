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


#ifndef _DEC_MPEG_H
#define _DEC_MPEG_H


#ifdef HAVE_MPEG
#include <sys/mman.h>
#include <mad.h>
#endif /* HAVE_MPEG */


#include "file_decoder.h"


/* size of ringbuffer for decoded MPEG Audio data (in frames) */
#define RB_MAD_SIZE 262144


#ifdef HAVE_MPEG
typedef struct _mpeg_pdata_t {
        struct mad_decoder mpeg_decoder;
        jack_ringbuffer_t * rb;
        FILE * mpeg_file;
        int channels;
        int SR;
        unsigned bitrate;
        int error;
        int is_eos;
        struct stat mpeg_stat;
        long long int filesize;
        int fd;
        int exp_fd;
        void * fdm;
        unsigned long total_samples_est;
        int mpeg_subformat; /* used as v_minor */
        struct mad_stream mpeg_stream;
        struct mad_frame mpeg_frame;
        struct mad_synth mpeg_synth;
} mpeg_pdata_t;
#endif /* HAVE_MPEG */


decoder_t * mpeg_decoder_new(file_decoder_t * fdec);
#ifdef HAVE_MPEG
void mpeg_decoder_delete(decoder_t * dec);
int mpeg_decoder_open(decoder_t * dec, char * filename);
void mpeg_decoder_close(decoder_t * dec);
unsigned int mpeg_decoder_read(decoder_t * dec, float * dest, int num);
void mpeg_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);
#endif /* HAVE_MPEG */


#endif /* _DEC_MPEG_H */
