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


#ifndef _DEC_MOD_H
#define _DEC_MOD_H

#ifdef HAVE_MOD
#include <sys/mman.h>
#include <libmodplug/modplug.h>
#endif /* HAVE_MOD */

#include "file_decoder.h"


/* Decoding buffer size for libmodplug */
#define MOD_BUFSIZE 8192
/* size of ringbuffer for decoded MOD Audio data (in frames) */
#define RB_MOD_SIZE 262144


#ifdef HAVE_MOD
typedef struct _mod_pdata_t {
        rb_t * rb;
        ModPlug_Settings mp_settings;
        ModPlugFile * mpf;
        int fd;
        void * fdm;
        struct stat st;
        int is_eos;
} mod_pdata_t;
#endif /* HAVE_MOD */


decoder_t * mod_decoder_init(file_decoder_t * fdec);
#ifdef HAVE_MOD
void mod_decoder_destroy(decoder_t * dec);
int is_valid_mod_extension(char * filename);
int mod_decoder_open(decoder_t * dec, char * filename);
void mod_decoder_close(decoder_t * dec);
unsigned int mod_decoder_read(decoder_t * dec, float * dest, int num);
void mod_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);
#endif /* HAVE_MOD */


#endif /* _DEC_MOD_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

