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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "enc_flac.h"
#include "enc_lame.h"
#include "enc_vorbis.h"
#include "enc_sndfile.h"
#include "file_encoder.h"

extern size_t sample_size;


typedef encoder_t * encoder_init_t(file_encoder_t * fenc);

/* positions in this array have to agree with the ENC_*_LIB values in file_encoder.h */
encoder_init_t * encoder_init_v[N_ENCODERS] = {
	sndfile_encoder_init,
	flac_encoder_init,
	vorbisenc_encoder_init,
	lame_encoder_init
};


file_encoder_t *
file_encoder_new(void) {

	file_encoder_t * fenc = NULL;
	
	if ((fenc = calloc(1, sizeof(file_encoder_t))) == NULL) {
		fprintf(stderr, "file_encoder.c: file_encoder_new() failed: calloc error\n");
		return NULL;
	}
	
	fenc->file_open = 0;
	fenc->penc = NULL;

	return fenc;
}


void
file_encoder_delete(file_encoder_t * fenc) {
	
	if (fenc->file_open) {
		file_encoder_close(fenc);
	}

	free(fenc);
}


/* return: 0 is OK, >0 is error */
int
file_encoder_open(file_encoder_t * fenc, encoder_mode_t * mode) {

	encoder_t * enc;

	if (mode->filename == NULL) {
		fprintf(stderr, "Warning: filename == NULL passed to file_encoder_open()\n");
		return 1;
	}

	enc = encoder_init_v[mode->file_lib](fenc);
	if (!enc) {
		fprintf(stderr, "Warning: error initializing encoder %d.\n", mode->file_lib);
		return 1;
	}

	if (enc->open(enc, mode) != 0) {
		fprintf(stderr, "Warning: error opening encoder %d.\n", mode->file_lib);
		return 1;
	}
	
	enc->mode = mode;
	fenc->penc = (void *)enc;
	fenc->file_open = 1;
	return 0;
}


void
file_encoder_close(file_encoder_t * fenc) {

	encoder_t * enc;

	if (!fenc->file_open) {
		return;
	}

	enc = (encoder_t *)(fenc->penc);
	enc->close(enc);
	enc->destroy(enc);
	fenc->penc = NULL;
	fenc->file_open = 0;
}


/* data should point to (num * channels) number of float values */
unsigned int
file_encoder_write(file_encoder_t * fenc, float * data, int num) {

	encoder_t * enc = (encoder_t *)(fenc->penc);

	return enc->write(enc, data, num);
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

