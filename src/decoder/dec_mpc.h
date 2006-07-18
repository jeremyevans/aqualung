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


#ifndef _DEC_MPC_H
#define _DEC_MPC_H

#ifdef HAVE_MPC
#include <mpcdec/mpcdec.h>
#endif /* HAVE_MPC */

#include "file_decoder.h"


/* Decoding buffer size for musepack */
#define MPC_BUFSIZE 8192
/* size of ringbuffer for decoded Musepack audio data (in frames) */
#define RB_MPC_SIZE 262144


#ifdef HAVE_MPC
typedef struct _mpc_pdata_t {
        jack_ringbuffer_t * rb;
        FILE * mpc_file;
        long int size;
        int seekable;
        int status;
        int is_eos;
        mpc_decoder mpc_d;
        mpc_reader_file mpc_r_f;
        mpc_streaminfo mpc_i;
} mpc_pdata_t;
#endif /* HAVE_MPC */


decoder_t * mpc_decoder_init(file_decoder_t * fdec);
#ifdef HAVE_MPC
void mpc_decoder_destroy(decoder_t * dec);
int mpc_decoder_open(decoder_t * dec, char * filename);
void mpc_decoder_close(decoder_t * dec);
unsigned int mpc_decoder_read(decoder_t * dec, float * dest, int num);
void mpc_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);
#endif /* HAVE_MPC */


#endif /* _DEC_MPC_H */
