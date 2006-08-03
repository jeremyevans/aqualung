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

#include "dec_mpc.h"


extern size_t sample_size;


#ifdef HAVE_MPC

/* return 1 if reached end of stream, 0 else */
int
decode_mpc(decoder_t * dec) {

	mpc_pdata_t * pd = (mpc_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	int n;
	float fval;
        MPC_SAMPLE_FORMAT buffer[MPC_DECODER_BUFFER_LENGTH];


        pd->status = mpc_decoder_decode(&pd->mpc_d, buffer, NULL, NULL);
	if (pd->status == (unsigned)(-1)) {
		fprintf(stderr, "decode_mpc: mpc decoder reported an error\n");
		return 1; /* ignore the rest of the stream */
	} else if (pd->status == 0) {
		return 1; /* end of stream */
	}
	
	for (n = 0; n < pd->status * pd->mpc_i.channels; n++) {
#ifdef MPC_FIXED_POINT
                fval = buffer[n] / (double)MPC_FIXED_POINT_SCALE * fdec->voladj_lin;
#else
                fval = buffer[n] * fdec->voladj_lin;
#endif /* MPC_FIXED_POINT */
		
                if (fval < -1.0f) {
                        fval = -1.0f;
                } else if (fval > 1.0f) {
                        fval = 1.0f;
                }
		
                jack_ringbuffer_write(pd->rb, (char *)&fval, sample_size);
	}
	return 0;
}


decoder_t *
mpc_decoder_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_mpc.c: mpc_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(mpc_pdata_t))) == NULL) {
                fprintf(stderr, "dec_mpc.c: mpc_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->init = mpc_decoder_init;
	dec->destroy = mpc_decoder_destroy;
	dec->open = mpc_decoder_open;
	dec->close = mpc_decoder_close;
	dec->read = mpc_decoder_read;
	dec->seek = mpc_decoder_seek;

	return dec;
}


void
mpc_decoder_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


int
mpc_decoder_open(decoder_t * dec, char * filename) {

	mpc_pdata_t * pd = (mpc_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	
	
	if ((pd->mpc_file = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "mpc_decoder_open: fopen() failed for Musepack file\n");
		return DECODER_OPEN_FERROR;
	}
	pd->seekable = 1;
	fseek(pd->mpc_file, 0, SEEK_END);
	pd->size = ftell(pd->mpc_file);
	fseek(pd->mpc_file, 0, SEEK_SET);
	
	mpc_reader_setup_file_reader(&pd->mpc_r_f, pd->mpc_file);
	
	mpc_streaminfo_init(&pd->mpc_i);
	if (mpc_streaminfo_read(&pd->mpc_i, &pd->mpc_r_f.reader) != ERROR_CODE_OK) {
		fclose(pd->mpc_file);
		return DECODER_OPEN_BADLIB;
	}
	
	mpc_decoder_setup(&pd->mpc_d, &pd->mpc_r_f.reader);
	if (!mpc_decoder_initialize(&pd->mpc_d, &pd->mpc_i)) {
		fclose(pd->mpc_file);
		return DECODER_OPEN_BADLIB;
	}
	
	pd->is_eos = 0;
	pd->rb = jack_ringbuffer_create(pd->mpc_i.channels * sample_size * RB_MPC_SIZE);
	
	fdec->channels = pd->mpc_i.channels;
	fdec->SR = pd->mpc_i.sample_freq;
	fdec->file_lib = MPC_LIB;
	
	fdec->fileinfo.total_samples = mpc_streaminfo_get_length_samples(&pd->mpc_i);
	fdec->fileinfo.format_major = FORMAT_MPC;
	fdec->fileinfo.format_minor = pd->mpc_i.profile;
	fdec->fileinfo.bps = pd->mpc_i.average_bitrate;
	
	return DECODER_OPEN_SUCCESS;
}


void
mpc_decoder_close(decoder_t * dec) {

	mpc_pdata_t * pd = (mpc_pdata_t *)dec->pdata;

	jack_ringbuffer_free(pd->rb);
	fclose(pd->mpc_file);
}


unsigned int
mpc_decoder_read(decoder_t * dec, float * dest, int num) {

	mpc_pdata_t * pd = (mpc_pdata_t *)dec->pdata;

	unsigned int numread = 0;
	unsigned int n_avail = 0;


	while ((jack_ringbuffer_read_space(pd->rb) <
		num * pd->mpc_i.channels * sample_size) && (!pd->is_eos)) {

		pd->is_eos = decode_mpc(dec);
	}

	n_avail = jack_ringbuffer_read_space(pd->rb) / (pd->mpc_i.channels * sample_size);

	if (n_avail > num)
		n_avail = num;

	jack_ringbuffer_read(pd->rb, (char *)dest, n_avail *
			     pd->mpc_i.channels * sample_size);

	numread = n_avail;
	return numread;
}


void
mpc_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {
	
	mpc_pdata_t * pd = (mpc_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	char flush_dest;


	if (mpc_decoder_seek_sample(&pd->mpc_d, seek_to_pos)) {
		fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
		/* empty musepack decoder ringbuffer */
		while (jack_ringbuffer_read_space(pd->rb))
			jack_ringbuffer_read(pd->rb, &flush_dest, sizeof(char));
	} else {
		fprintf(stderr,
			"mpc_decoder_seek: warning: mpc_decoder_seek_sample() failed\n");
	}
}


#else
decoder_t *
mpc_decoder_init(file_decoder_t * fdec) {

        return NULL;
}
#endif /* HAVE_MPC */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

