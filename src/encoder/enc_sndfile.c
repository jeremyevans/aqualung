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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include "enc_sndfile.h"


#ifdef HAVE_SNDFILE_ENC

encoder_t *
sndfile_encoder_init(file_encoder_t * fenc) {

        encoder_t * enc = NULL;

        if ((enc = calloc(1, sizeof(encoder_t))) == NULL) {
                fprintf(stderr, "enc_sndfile.c: sndfile_encoder_new() failed: calloc error\n");
                return NULL;
        }

	enc->fenc = fenc;

        if ((enc->pdata = calloc(1, sizeof(sndfile_pencdata_t))) == NULL) {
                fprintf(stderr, "enc_sndfile.c: sndfile_encoder_new() failed: calloc error\n");
                return NULL;
        }

	enc->init = sndfile_encoder_init;
	enc->destroy = sndfile_encoder_destroy;
	enc->open = sndfile_encoder_open;
	enc->close = sndfile_encoder_close;
	enc->write = sndfile_encoder_write;

	return enc;
}


void
sndfile_encoder_destroy(encoder_t * enc) {

	free(enc->pdata);
	free(enc);
}


int
sndfile_encoder_open(encoder_t * enc, encoder_mode_t * mode) {

	sndfile_pencdata_t * pd = (sndfile_pencdata_t *)enc->pdata;

	pd->sf_info.samplerate = mode->sample_rate;
	pd->sf_info.channels = mode->channels;
	pd->sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
        if ((pd->sf = sf_open(mode->filename, SFM_WRITE, &(pd->sf_info))) == NULL) {
		return -1;
	}

	return 0;
}


void
sndfile_encoder_close(encoder_t * enc) {

	sndfile_pencdata_t * pd = (sndfile_pencdata_t *)enc->pdata;

	sf_close(pd->sf);
}


unsigned int
sndfile_encoder_write(encoder_t * enc, float * data, int num) {

	sndfile_pencdata_t * pd = (sndfile_pencdata_t *)enc->pdata;
	unsigned int numwritten = 0;

	numwritten = sf_writef_float(pd->sf, data, num);

	return numwritten;
}


#else
encoder_t *
sndfile_encoder_init(file_encoder_t * fenc) {

	return NULL;
}
#endif /* HAVE_SNDFILE_ENC */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

