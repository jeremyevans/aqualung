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

#ifndef AQUALUNG_ENC_FLAC_H
#define AQUALUNG_ENC_FLAC_H

#include <stdio.h>

#ifdef HAVE_FLAC_ENC
#include <FLAC/format.h>
#include <FLAC/stream_encoder.h>
#include <FLAC/metadata.h>
#endif /* HAVE_FLAC_ENC */

#include "file_encoder.h"


#ifdef HAVE_FLAC_ENC
typedef struct _flac_pencdata_t {
	FLAC__StreamEncoder * encoder;
	FLAC__int32 ** buf;
	int bufsize;
	int channels;
	encoder_mode_t mode;
} flac_pencdata_t;
#endif /* HAVE_FLAC_ENC */


encoder_t * flac_encoder_init(file_encoder_t * fenc);
#ifdef HAVE_FLAC_ENC
void flac_encoder_destroy(encoder_t * enc);
int flac_encoder_open(encoder_t * enc, encoder_mode_t * mode);
void flac_encoder_close(encoder_t * enc);
unsigned int flac_encoder_write(encoder_t * enc, float * data, int num);
#endif /* HAVE_FLAC_ENC */


#endif /* AQUALUNG_ENC_FLAC_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

