/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi

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


#ifndef _FILE_ENCODER_H
#define _FILE_ENCODER_H

#include "../common.h"
#include "../rb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* output libs */
#define ENC_SNDFILE_LIB 0
#define ENC_FLAC_LIB    1
#define ENC_VORBIS_LIB  2
#define ENC_LAME_LIB    3

#define N_ENCODERS      4


typedef struct _encoder_mode_t {
	int file_lib;
	char filename[MAXLEN];
        unsigned long sample_rate;
	int channels;
        int bps; /* meaningful only with Vorbis and LAME */
	int vbr; /* meaningful only with LAME */
} encoder_mode_t;


typedef struct _file_encoder_t {
	int file_open;
	void * penc; /* actually, it's (encoder_t *) */
} file_encoder_t;


typedef struct _encoder_t {

	file_encoder_t * fenc;
	void * pdata; /* opaque pointer to encoder-dependent struct */

	struct _encoder_t * (* init)(file_encoder_t * fenc);
	void (* destroy)(struct _encoder_t * enc);
	int (* open)(struct _encoder_t * enc, encoder_mode_t * mode);
	void (* close)(struct _encoder_t * enc);
	unsigned int (* write)(struct _encoder_t * enc, float * data, int num);

} encoder_t;


file_encoder_t * file_encoder_new(void);
void file_encoder_delete(file_encoder_t * fenc);

int file_encoder_open(file_encoder_t * fenc, encoder_mode_t * mode);
void file_encoder_close(file_encoder_t * fenc);
unsigned int file_encoder_write(file_encoder_t * fenc, float * data, int num);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _FILE_ENCODER_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

