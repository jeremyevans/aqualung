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


#ifndef _ENC_LAME_H
#define _ENC_LAME_H

#include <stdio.h>
#ifdef HAVE_LAME
#include <lame/lame.h>
#endif /* HAVE_LAME */

#include "file_encoder.h"


#ifdef HAVE_LAME

#define LAME_READ 1024
#define LAME_BUFSIZE (LAME_READ + LAME_READ/4 + 7200)
#define RB_LAME_SIZE (1<<18)

typedef struct _lame_pencdata_t {
	FILE * out;
	rb_t * rb;
	lame_global_flags * gf;
	int channels;
} lame_pencdata_t;
#endif /* HAVE_LAME */


encoder_t * lame_encoder_init(file_encoder_t * fenc);
#ifdef HAVE_LAME
void lame_encoder_destroy(encoder_t * enc);
int lame_encoder_open(encoder_t * enc, encoder_mode_t * mode);
void lame_encoder_close(encoder_t * enc);
unsigned int lame_encoder_write(encoder_t * enc, float * data, int num);
int lame_encoder_validate_bitrate(int requested, int idx_offset);
#endif /* HAVE_LAME */


#endif /* _ENC_LAME_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

