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


#ifndef _DEC_NULL_H
#define _DEC_NULL_H

#include "file_decoder.h"

/* private data of the decoder */
typedef struct _null_pdata_t {
	int dummy;
} null_pdata_t;



decoder_t * null_decoder_init(file_decoder_t * fdec);
void null_decoder_destroy(decoder_t * dec);
int null_decoder_open(decoder_t * dec, char * filename);
void null_decoder_close(decoder_t * dec);
unsigned int null_decoder_read(decoder_t * dec, float * dest, int num);
void null_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);


#endif /* _DEC_NULL_H */
