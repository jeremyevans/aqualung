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

#ifdef MPC_OLD_API
#include <mpcdec/mpcdec.h>
#else
#include <mpc/mpcdec.h>
#include <math.h>
#endif /* MPC_OLD_API */

#include "file_decoder.h"


/* Decoding buffer size for musepack */
#define MPC_BUFSIZE 8192
/* size of ringbuffer for decoded Musepack audio data (in frames) */
#define RB_MPC_SIZE 262144


typedef struct _mpc_pdata_t {
        rb_t * rb;
        FILE * mpc_file;
        long int size;
        int seekable;
        int status;
        int is_eos;
#ifdef MPC_OLD_API
        mpc_decoder mpc_d;
        mpc_reader_file mpc_r_f;
#else
        mpc_demux * mpc_d;
        mpc_reader mpc_r_f;
#endif /* MPC_OLD_API */
        mpc_streaminfo mpc_i;
} mpc_pdata_t;


decoder_t * mpc_decoder_init_func(file_decoder_t * fdec);
void mpc_decoder_destroy(decoder_t * dec);
int mpc_decoder_open(decoder_t * dec, char * filename);
void mpc_decoder_send_metadata(decoder_t * dec);
void mpc_decoder_close(decoder_t * dec);
unsigned int mpc_decoder_read(decoder_t * dec, float * dest, int num);
void mpc_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);


#endif /* _DEC_MPC_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

