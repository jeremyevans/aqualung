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
#include <time.h>
#include <math.h>

#include "../common.h"
#include "../metadata.h"
#include "../rb.h"
#include "enc_vorbis.h"


#ifdef HAVE_VORBISENC

encoder_t *
vorbisenc_encoder_init(file_encoder_t * fenc) {

        encoder_t * enc = NULL;

        if ((enc = calloc(1, sizeof(encoder_t))) == NULL) {
                fprintf(stderr, "enc_vorbisenc.c: vorbisenc_encoder_new() failed: calloc error\n");
                return NULL;
        }

	enc->fenc = fenc;

        if ((enc->pdata = calloc(1, sizeof(vorbisenc_pencdata_t))) == NULL) {
                fprintf(stderr, "enc_vorbisenc.c: vorbisenc_encoder_new() failed: calloc error\n");
                return NULL;
        }

	enc->init = vorbisenc_encoder_init;
	enc->destroy = vorbisenc_encoder_destroy;
	enc->open = vorbisenc_encoder_open;
	enc->close = vorbisenc_encoder_close;
	enc->write = vorbisenc_encoder_write;

	return enc;
}


void
vorbisenc_encoder_destroy(encoder_t * enc) {

	free(enc->pdata);
	free(enc);
}


int
vorbisenc_encoder_open(encoder_t * enc, encoder_mode_t * mode) {

	vorbisenc_pencdata_t * pd = (vorbisenc_pencdata_t *)enc->pdata;
	int ret;

	pd->out = fopen(mode->filename, "wb");
	if (pd->out == NULL) {
		fprintf(stdout, "vorbisenc_encoder_open(): unable to open file for writing: %s\n",
			mode->filename);
		return -1;
	}

	vorbis_info_init(&pd->vi);

	ret = (vorbis_encode_setup_managed(&pd->vi, mode->channels, mode->sample_rate, -1, mode->bps, -1) ||
	       vorbis_encode_ctl(&pd->vi, OV_ECTL_RATEMANAGE2_SET, NULL) ||
	       vorbis_encode_setup_init(&pd->vi));

	if (ret)
		return -1;

	vorbis_comment_init(&pd->vc);
        if (mode->write_meta) {
		meta_frame_t * frame;
		frame = metadata_get_frame_by_tag(mode->meta, META_TAG_OXC, NULL);
		while (frame != NULL) {
			char * str;
			char * field_val = NULL;
			char fval[MAXLEN];
			char * renderfmt = meta_get_field_renderfmt(frame->type);
			meta_get_fieldname_embedded(META_TAG_OXC, frame->type, &str);

			if (META_FIELD_TEXT(frame->type)) {
				field_val = frame->field_val;
			} else if (META_FIELD_INT(frame->type)) {
				snprintf(fval, MAXLEN-1, renderfmt, frame->int_val);
				field_val = fval;
			} else if (META_FIELD_FLOAT(frame->type)) {
				snprintf(fval, MAXLEN-1, renderfmt, frame->float_val);
				field_val = fval;
			}

			vorbis_comment_add_tag(&pd->vc, str, field_val);
			frame = metadata_get_frame_by_tag(mode->meta, META_TAG_OXC, frame);
		}
        }
	vorbis_analysis_init(&pd->vd, &pd->vi);
	vorbis_block_init(&pd->vd, &pd->vb);

	srand(time(NULL));
	ogg_stream_init(&pd->os, rand());

	{
		ogg_packet header;
		ogg_packet header_comm;
		ogg_packet header_code;

		vorbis_analysis_headerout(&pd->vd, &pd->vc, &header, &header_comm, &header_code);
		ogg_stream_packetin(&pd->os, &header);
		ogg_stream_packetin(&pd->os, &header_comm);
		ogg_stream_packetin(&pd->os, &header_code);

		while (1) {
			int result = ogg_stream_flush(&pd->os, &pd->og);
			if (result == 0)
				break;
			fwrite(pd->og.header, 1, pd->og.header_len, pd->out);
			fwrite(pd->og.body, 1, pd->og.body_len, pd->out);
		}
	}

	pd->rb = rb_create(mode->channels * sizeof(float) * RB_VORBISENC_SIZE);
	pd->channels = mode->channels;
	pd->eos = 0;
	return 0;
}


void
vorbisenc_read_and_analyse(vorbisenc_pencdata_t * pd, int n_frames) {

	int i, j;
	float ** buffer = vorbis_analysis_buffer(&pd->vd, n_frames);
	
	for (i = 0; i < n_frames; i++) {
		for (j = 0; j < pd->channels; j++) {
			rb_read(pd->rb, (char *)(&(buffer[j][i])), sizeof(float));
		}
	}
	vorbis_analysis_wrote(&pd->vd, n_frames);
}


void
vorbisenc_encode_blocks(vorbisenc_pencdata_t * pd) {

	while (vorbis_analysis_blockout(&pd->vd, &pd->vb) == 1) {
		vorbis_analysis(&pd->vb, NULL);
		vorbis_bitrate_addblock(&pd->vb);
		
		while (vorbis_bitrate_flushpacket(&pd->vd, &pd->op)) {
			ogg_stream_packetin(&pd->os, &pd->op);
			while (!pd->eos) {
				int result = ogg_stream_pageout(&pd->os, &pd->og);
				if (result == 0)
					break;
				fwrite(pd->og.header, 1, pd->og.header_len, pd->out);
				fwrite(pd->og.body, 1, pd->og.body_len, pd->out);
				if (ogg_page_eos(&pd->og))
					pd->eos = 1;
			}
		}
	}
}


unsigned int
vorbisenc_encoder_write(encoder_t * enc, float * data, int num) {

	vorbisenc_pencdata_t * pd = (vorbisenc_pencdata_t *)enc->pdata;

	rb_write(pd->rb, (char *)data, num * pd->channels * sizeof(float));
	while (rb_read_space(pd->rb) > VORBISENC_READ * pd->channels * sizeof(float)) {
		vorbisenc_read_and_analyse(pd, VORBISENC_READ);
		vorbisenc_encode_blocks(pd);
	}

	return num;
}


void
vorbisenc_encoder_close(encoder_t * enc) {

	vorbisenc_pencdata_t * pd = (vorbisenc_pencdata_t *)enc->pdata;

	int space = rb_read_space(pd->rb) / pd->channels / sizeof(float);
	vorbisenc_read_and_analyse(pd, space);
	vorbisenc_encode_blocks(pd);
	vorbis_analysis_wrote(&pd->vd, 0);
	vorbisenc_encode_blocks(pd);

	ogg_stream_clear(&pd->os);
	vorbis_block_clear(&pd->vb);
	vorbis_dsp_clear(&pd->vd);
	vorbis_comment_clear(&pd->vc);
	vorbis_info_clear(&pd->vi);

	rb_free(pd->rb);
	fclose(pd->out);
}


#else
encoder_t *
vorbisenc_encoder_init(file_encoder_t * fenc) {

	return NULL;
}
#endif /* HAVE_VORBISENC */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

