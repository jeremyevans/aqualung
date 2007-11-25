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
#include <string.h>

#include "../i18n.h"
#include "../metadata.h"
#include "../metadata_flac.h"
#include "enc_flac.h"


#ifdef HAVE_FLAC

#ifdef HAVE_FLAC_7

/* Compression level presets to match the interface
 * of the command-line flac 1.1.2 encoder.
 */
typedef struct {
	FLAC__bool ms;
	FLAC__bool ms_loose;
	int lpc_order;
	int blocksize;
	FLAC__bool exhaustive;
	int min_res;
	int max_res;
} flac_preset_t;

flac_preset_t flac_presets[] = {
	{ false, false, 0,  1152, false, 2, 2 },
	{ false, true,  0,  1152, false, 2, 2 },
	{ true,  false, 0,  1152, false, 0, 3 },
	{ false, false, 6,  4608, false, 3, 3 },
	{ false, true,  8,  4608, false, 3, 3 },
	{ true,  false, 8,  4608, false, 3, 3 },
	{ true,  false, 8,  4608, false, 0, 4 },
	{ true,  false, 8,  4608, true,  0, 6 },
	{ true,  false, 12, 4608, true,  0, 6 },
};
#endif /* HAVE_FLAC_7 */

encoder_t *
flac_encoder_init(file_encoder_t * fenc) {

        encoder_t * enc = NULL;

        if ((enc = calloc(1, sizeof(encoder_t))) == NULL) {
                fprintf(stderr, "enc_flac.c: flac_encoder_new() failed: calloc error\n");
                return NULL;
        }

	enc->fenc = fenc;

        if ((enc->pdata = calloc(1, sizeof(flac_pencdata_t))) == NULL) {
                fprintf(stderr, "enc_flac.c: flac_encoder_new() failed: calloc error\n");
                return NULL;
        }

	enc->init = flac_encoder_init;
	enc->destroy = flac_encoder_destroy;
	enc->open = flac_encoder_open;
	enc->close = flac_encoder_close;
	enc->write = flac_encoder_write;

	return enc;
}


void
flac_encoder_destroy(encoder_t * enc) {

	free(enc->pdata);
	free(enc);
}


int
flac_encoder_open(encoder_t * enc, encoder_mode_t * mode) {

	flac_pencdata_t * pd = (flac_pencdata_t *)enc->pdata;
	int i;
#ifdef HAVE_FLAC_8
	FLAC__StreamEncoderInitStatus state;

	pd->encoder = FLAC__stream_encoder_new();
	if (pd->encoder == NULL)
		return -1;
	
	FLAC__stream_encoder_set_bits_per_sample(pd->encoder, 16);
	FLAC__stream_encoder_set_channels(pd->encoder, mode->channels);
	FLAC__stream_encoder_set_sample_rate(pd->encoder, mode->sample_rate);
	FLAC__stream_encoder_set_verify(pd->encoder, true);

	FLAC__stream_encoder_set_compression_level(pd->encoder, mode->clevel);

	state = FLAC__stream_encoder_init_file(pd->encoder, mode->filename, NULL, NULL);
	if (state != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		fprintf(stderr, "FLAC__stream_encoder_init_file returned error: %s\n",
			FLAC__StreamEncoderInitStatusString[state]);
		return -1;
	}
#endif /* HAVE_FLAC_8 */
#ifdef HAVE_FLAC_7
	FLAC__FileEncoderState state;

	pd->encoder = FLAC__file_encoder_new();
	if (pd->encoder == NULL)
		return -1;
	
	FLAC__file_encoder_set_bits_per_sample(pd->encoder, 16);
	FLAC__file_encoder_set_channels(pd->encoder, mode->channels);
	FLAC__file_encoder_set_sample_rate(pd->encoder, mode->sample_rate);
	FLAC__file_encoder_set_verify(pd->encoder, true);

	if (mode->channels == 2) {
		FLAC__file_encoder_set_do_mid_side_stereo(pd->encoder, flac_presets[mode->clevel].ms);
		FLAC__file_encoder_set_loose_mid_side_stereo(pd->encoder, flac_presets[mode->clevel].ms_loose);
	}
	FLAC__file_encoder_set_max_lpc_order(pd->encoder, flac_presets[mode->clevel].lpc_order);
	FLAC__file_encoder_set_blocksize(pd->encoder, flac_presets[mode->clevel].blocksize);
	FLAC__file_encoder_set_do_exhaustive_model_search(pd->encoder, flac_presets[mode->clevel].exhaustive);
	FLAC__file_encoder_set_min_residual_partition_order(pd->encoder, flac_presets[mode->clevel].min_res);
	FLAC__file_encoder_set_max_residual_partition_order(pd->encoder, flac_presets[mode->clevel].max_res);

	FLAC__file_encoder_set_filename(pd->encoder, mode->filename);
	if ((state = FLAC__file_encoder_init(pd->encoder)) != FLAC__FILE_ENCODER_OK) {
		fprintf(stderr, "FLAC__file_encoder_init returned error: %s\n",
			FLAC__FileEncoderStateString[state]);
		return -1;
	}
#endif /* HAVE_FLAC_7 */

	pd->mode = *mode;

	pd->buf = (FLAC__int32 **)calloc(mode->channels, sizeof(FLAC__int32 *));
	for (i = 0; i < mode->channels; i++) {
		pd->buf[i] = NULL;
	}
	pd->bufsize = 0;
	pd->channels = mode->channels;
	return 0;
}


unsigned int
flac_encoder_write(encoder_t * enc, float * data, int num) {

	flac_pencdata_t * pd = (flac_pencdata_t *)enc->pdata;
	int i, n = 0;
	FLAC__bool b;

	if (pd->bufsize < num) {
		int k;
		for (k = 0; k < pd->channels; k++) {
			pd->buf[k] = realloc(pd->buf[k], num * sizeof(FLAC__int32));
		}
		pd->bufsize = num;
	}

	for (i = 0; i < num; i++) {
		int k;
		for (k = 0; k < pd->channels; k++) {
			pd->buf[k][i] = data[n] * (1<<15);
			++n;
		}
	}

#ifdef HAVE_FLAC_8
	b = FLAC__stream_encoder_process(pd->encoder, (const FLAC__int32 **)pd->buf, num);
	if (b != true) {
		fprintf(stderr, "FLAC__stream_encoder_process returned error: %s\n",
			FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(pd->encoder)]);
	}
#endif /* HAVE_FLAC_8 */
#ifdef HAVE_FLAC_7
	b = FLAC__file_encoder_process(pd->encoder, (const FLAC__int32 **)pd->buf, num);
	if (b != true) {
		fprintf(stderr, "FLAC__file_encoder_process returned error: %s\n",
			FLAC__FileEncoderStateString[FLAC__file_encoder_get_state(pd->encoder)]);	
	}
#endif /* HAVE_FLAC_7 */

	return num;
}


int
flac_encoder_write_meta_vc(metadata_t * meta, FLAC__Metadata_SimpleIterator * iter) {

	int ret;
	FLAC__StreamMetadata * smeta = metadata_to_flac_streammeta(meta);

	if (FLAC__metadata_simple_iterator_get_block_type(iter) ==
	    FLAC__METADATA_TYPE_VORBIS_COMMENT) {

		ret = FLAC__metadata_simple_iterator_set_block(iter, smeta, true);
		if (ret == false) {
			fprintf(stderr, "error: FLAC metadata write failed!\n");
			FLAC__metadata_simple_iterator_delete(iter);
			return -1;
		}
	} else {
		ret = FLAC__metadata_simple_iterator_insert_block_after(iter, smeta, true);
		if (ret == false) {
			fprintf(stderr, "error: FLAC metadata write failed!\n");
			FLAC__metadata_object_delete(smeta);
			FLAC__metadata_simple_iterator_delete(iter);
			return -1;
		}
	}
	FLAC__metadata_object_delete(smeta);
	return 0;
}


#ifdef HAVE_FLAC_8
int
flac_encoder_write_meta_pics(metadata_t * meta, FLAC__Metadata_SimpleIterator * iter) {

	meta_frame_t * frame = metadata_get_frame_by_tag(meta, META_TAG_FLAC_APIC, NULL);
	while (frame) {
		FLAC__StreamMetadata * smeta = metadata_apic_frame_to_smeta(frame);
		int ret = FLAC__metadata_simple_iterator_insert_block_after(iter, smeta, true);
		if (ret == false) {
			fprintf(stderr, "error: FLAC metadata write failed!\n");
			FLAC__metadata_object_delete(smeta);
			FLAC__metadata_simple_iterator_delete(iter);
			return -1;
		}

		FLAC__metadata_object_delete(smeta);
		frame = metadata_get_frame_by_tag(meta, META_TAG_FLAC_APIC, frame);
	}
	return 0;
}
#endif /* HAVE_FLAC_8 */


void
flac_encoder_write_meta(encoder_t * enc) {

	flac_pencdata_t * pd = (flac_pencdata_t *)enc->pdata;
	FLAC__Metadata_SimpleIterator * iter = FLAC__metadata_simple_iterator_new();
	if (iter == NULL)
		return;
	
	if (FLAC__metadata_simple_iterator_init(iter, pd->mode.filename, false, false) != true) {
		fprintf(stderr, "FLAC__metadata_simple_iterator_init returned error: %s\n",
			FLAC__Metadata_SimpleIteratorStatusString[FLAC__metadata_simple_iterator_status(iter)]);
		FLAC__metadata_simple_iterator_delete(iter);
		return;
	}
	FLAC__metadata_simple_iterator_next(iter);

	if (metadata_get_frame_by_tag(enc->mode->meta, META_TAG_OXC, NULL) != NULL) {
		if (flac_encoder_write_meta_vc(enc->mode->meta, iter) < 0) {
			return;
		}
	}

#ifdef HAVE_FLAC_8
	if (flac_encoder_write_meta_pics(enc->mode->meta, iter) < 0) {
		return;
	}
#endif /* HAVE_FLAC_8 */

	FLAC__metadata_simple_iterator_delete(iter);
}


void
flac_encoder_close(encoder_t * enc) {

	flac_pencdata_t * pd = (flac_pencdata_t *)enc->pdata;

#ifdef HAVE_FLAC_8
	FLAC__stream_encoder_finish(pd->encoder);
	FLAC__stream_encoder_delete(pd->encoder);
#endif /* HAVE_FLAC_8 */
#ifdef HAVE_FLAC_7
	FLAC__file_encoder_finish(pd->encoder);
	FLAC__file_encoder_delete(pd->encoder);
#endif /* HAVE_FLAC_7 */

	if (pd->bufsize > 0) {
		int k;
		for (k = 0; k < pd->channels; k++) {
			free(pd->buf[k]);
		}
	}
	free(pd->buf);

	if (pd->mode.write_meta) {
		flac_encoder_write_meta(enc);
	}
}


#else
encoder_t *
flac_encoder_init(file_encoder_t * fenc) {

	return NULL;
}
#endif /* HAVE_FLAC */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

