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


#ifndef _DEC_SNDFILE_H
#define _DEC_SNDFILE_H

#ifdef HAVE_SNDFILE
#include <sndfile.h>
#endif /* HAVE_SNDFILE */

#include "file_decoder.h"


#ifdef HAVE_SNDFILE
typedef struct _sndfile_pdata_t {
	SF_INFO sf_info;
	SNDFILE * sf;
	unsigned int nframes;
} sndfile_pdata_t;
#endif /* HAVE_SNDFILE */


decoder_t * sndfile_decoder_init(file_decoder_t * fdec);
#ifdef HAVE_SNDFILE
void sndfile_decoder_destroy(decoder_t * dec);
int sndfile_decoder_open(decoder_t * dec, char * filename);
void sndfile_decoder_close(decoder_t * dec);
unsigned int sndfile_decoder_read(decoder_t * dec, float * dest, int num);
void sndfile_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);
#endif /* HAVE_SNDFILE */


#endif /* _DEC_SNDFILE_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

