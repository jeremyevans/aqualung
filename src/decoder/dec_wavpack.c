/*                                                     -*- linux-c -*-
    Copyright (C) 2007 Tom Szilagyi

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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>

#include "../metadata.h"
#include "../metadata_ape.h"
#include "../rb.h"
#include "file_decoder.h"
#include "dec_wavpack.h"


extern size_t sample_size;


int
decode_wavpack(decoder_t * dec) {

	file_decoder_t * fdec = dec->fdec;
	wavpack_pdata_t * pd = (wavpack_pdata_t *)dec->pdata;

	int32_t buffer[WAVPACK_BUFSIZE];
	uint i, j;

	float fval[fdec->fileinfo.channels];

	pd->last_decoded_samples = WavpackUnpackSamples(pd->wpc, buffer,
							WAVPACK_BUFSIZE / fdec->fileinfo.channels);

	/* Actual floating point data is output into an integer, will have to deal with that */
	if (WavpackGetMode (pd->wpc) & MODE_FLOAT) {

		int num_samples = pd->last_decoded_samples * fdec->fileinfo.channels;
		int32_t * buffer_pointer = buffer;

		while (num_samples--) {
			float data = * (float*) buffer_pointer;
			data *= pd->scale_factor_float;

			if (data > pd->scale_factor_float)
				data = pd->scale_factor_float;
			if (data < -(pd->scale_factor_float))
				data = -(pd->scale_factor_float);

			*buffer_pointer++ = (int32_t) data;
		}
	}

	if (pd->last_decoded_samples == 0)
		return 1;

	for (i = 0; i < pd->last_decoded_samples * fdec->fileinfo.channels; i += fdec->fileinfo.channels) {
		for (j = 0; j < fdec->fileinfo.channels; j++) {

			fval[j] = buffer[i+j] * fdec->voladj_lin / pd->scale_factor_float;

			if (fval[j] < -1.0f) {
				fval[j] = -1.0f;
			} else if (fval[j] > 1.0f) {
				fval[j] = 1.0f;
			}
		}
		rb_write(pd->rb, (char *)fval, fdec->fileinfo.channels * sample_size);
	}

	return 0;
}


decoder_t *
wavpack_decoder_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_wavpack.c: wavpack_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(wavpack_pdata_t))) == NULL) {
                fprintf(stderr, "dec_wavpack.c: wavpack_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->init = wavpack_decoder_init;
	dec->destroy = wavpack_decoder_destroy;
	dec->open = wavpack_decoder_open;
	dec->send_metadata = wavpack_decoder_send_metadata;
	dec->close = wavpack_decoder_close;
	dec->read = wavpack_decoder_read;
	dec->seek = wavpack_decoder_seek;

	return dec;
}


void
wavpack_decoder_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


int
wavpack_decoder_open(decoder_t * dec, char * filename) {

	file_decoder_t * fdec = dec->fdec;
	wavpack_pdata_t * pd = (wavpack_pdata_t *)dec->pdata;
	int i, corrected_bits_per_sample;
	metadata_t * meta;

	/* More than 2 channels doesn't work */
	/* Normalize is for floating point data only, it gets scaled to -1.0 and 1.0,
	   not replaygain related or anything */
	/* Opening hybrid correction file if possible */
	pd->flags = OPEN_2CH_MAX | OPEN_TAGS | OPEN_NORMALIZE | OPEN_WVC;

	strcpy(pd->error, "No Error");
	pd->wpc = WavpackOpenFileInput(filename, pd->error, pd->flags, 0);

	/* The decoder can actually do something with the file */
	if (pd->wpc != NULL) {
		fdec->fileinfo.channels = WavpackGetReducedChannels(pd->wpc);
		fdec->fileinfo.sample_rate = WavpackGetSampleRate(pd->wpc);
		fdec->fileinfo.total_samples = WavpackGetNumSamples(pd->wpc);
		fdec->fileinfo.bps = WavpackGetBitsPerSample(pd->wpc) * fdec->fileinfo.sample_rate 
					* fdec->fileinfo.channels;
		pd->bits_per_sample = WavpackGetBitsPerSample(pd->wpc);

		pd->rb = rb_create(fdec->fileinfo.channels * sample_size * RB_WAVPACK_SIZE);

		pd->end_of_file = 0;

		/* It's best to calculate the scale factor in advance and store it */
		pd->scale_factor_float = 1;
		/* Anything other than 8, 16, 24 or 32 bits is padded to the nearest one */
		if (WavpackGetMode (pd->wpc) & MODE_FLOAT) {
			corrected_bits_per_sample = 32;
		} else {
			corrected_bits_per_sample = ceil(pd->bits_per_sample/8.0f) * 8;
		}

		for (i = 1; i < corrected_bits_per_sample; i++) {
			pd->scale_factor_float *= 2;
		}

		strcpy(dec->format_str, "WavPack");
		fdec->file_lib = WAVPACK_LIB;

		meta = metadata_new();
		meta_ape_send_metadata(meta, fdec);
		return DECODER_OPEN_SUCCESS;
	}

	return DECODER_OPEN_BADLIB;
}


void
wavpack_decoder_send_metadata(decoder_t * dec) {
}


void
wavpack_decoder_close(decoder_t * dec) {
	wavpack_pdata_t * pd = (wavpack_pdata_t *)dec->pdata;

	WavpackCloseFile(pd->wpc);
	rb_free(pd->rb);
}


unsigned int
wavpack_decoder_read(decoder_t * dec, float * dest, int num) {

	file_decoder_t * fdec = dec->fdec;
	wavpack_pdata_t * pd = (wavpack_pdata_t *)dec->pdata;

	while ((rb_read_space(pd->rb) < num * fdec->fileinfo.channels * sample_size) &&
	       (pd->end_of_file == 0)) {

			pd->end_of_file = decode_wavpack(dec);
	}

	uint actual_num = 0;
	uint n_avail = rb_read_space(pd->rb) / (fdec->fileinfo.channels * sample_size);

	if ( num > n_avail ) {
		actual_num = n_avail;
	} else {
		actual_num = num;
	}

	rb_read(pd->rb, (char *)dest, actual_num * sample_size * fdec->fileinfo.channels);

	return actual_num;
}


void
wavpack_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {

	file_decoder_t * fdec = dec->fdec;
	wavpack_pdata_t * pd = (wavpack_pdata_t *)dec->pdata;
	char flush_dest;

	if (WavpackSeekSample(pd->wpc, seek_to_pos) == 1) {
		fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
		/* Empty ringbuffer */
		while (rb_read_space(pd->rb))
			rb_read(pd->rb, &flush_dest, sizeof(char));
	} else {
		fprintf(stderr, "wavpack_decoder_seek: warning: WavpackSeekSample() failed\n");
	}
}



// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
