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
#include "../metadata_ape.h"
#include "../metadata_id3v1.h"
#include "../metadata_id3v2.h"
#include "enc_lame.h"


#ifdef HAVE_LAME

extern options_t options;

void
lame_encoder_printf(const char * format, va_list ap) {

	(void)vfprintf(stdout, format, ap);
}


encoder_t *
lame_encoder_init(file_encoder_t * fenc) {

        encoder_t * enc = NULL;

        if ((enc = calloc(1, sizeof(encoder_t))) == NULL) {
                fprintf(stderr, "enc_lame.c: lame_encoder_new() failed: calloc error\n");
                return NULL;
        }

	enc->fenc = fenc;

        if ((enc->pdata = calloc(1, sizeof(lame_pencdata_t))) == NULL) {
                fprintf(stderr, "enc_lame.c: lame_encoder_new() failed: calloc error\n");
                return NULL;
        }

	enc->init = lame_encoder_init;
	enc->destroy = lame_encoder_destroy;
	enc->open = lame_encoder_open;
	enc->close = lame_encoder_close;
	enc->write = lame_encoder_write;

	return enc;
}


void
lame_encoder_destroy(encoder_t * enc) {

	free(enc->pdata);
	free(enc);
}


int
lame_encoder_validate_bitrate(int requested, int idx_offset) {

	int n_rates = 14;
	int rates[] = { 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
	int idx = 0;
	int i;

	for (i = 0; i < n_rates-1; i++) {
		if (requested <= (rates[i] + rates[i+1]) / 2) {
			break;
		}
	}
	idx = i + idx_offset;
	if (idx < 0)
		idx = 0;
	if (idx > n_rates - 1)
		idx = n_rates - 1;

	return rates[idx];
}


int
lame_encoder_open(encoder_t * enc, encoder_mode_t * mode) {

	lame_pencdata_t * pd = (lame_pencdata_t *)enc->pdata;
	int ret;

	pd->out = fopen(mode->filename, "wb+");
	if (pd->out == NULL) {
		fprintf(stdout, "lame_decoder_open(): unable to open file for writing: %s\n",
			mode->filename);
		return -1;
	}

	pd->gf = lame_init();
	if (pd->gf == NULL)
		return -1;

	pd->channels = mode->channels;
	pd->rb = rb_create(mode->channels * sizeof(float) * RB_LAME_SIZE);

	lame_set_num_channels(pd->gf, mode->channels);
	lame_set_in_samplerate(pd->gf, mode->sample_rate);
	lame_set_mode(pd->gf, (mode->channels == 2) ? JOINT_STEREO : MONO);
	lame_set_quality(pd->gf, 2); /* algorithm quality selection: 2=high 5=medium 7=low */

	if (mode->vbr) {
		lame_set_VBR(pd->gf, vbr_abr);
		lame_set_VBR_mean_bitrate_kbps(pd->gf, lame_encoder_validate_bitrate(mode->bps/1000, 0));
		lame_set_VBR_min_bitrate_kbps(pd->gf, lame_encoder_validate_bitrate(mode->bps/1000, -14));
		lame_set_VBR_max_bitrate_kbps(pd->gf, lame_encoder_validate_bitrate(mode->bps/1000, 2));
	} else {
		lame_set_brate(pd->gf, lame_encoder_validate_bitrate(mode->bps/1000, 0));
	}

	lame_set_errorf(pd->gf, lame_encoder_printf);
	lame_set_debugf(pd->gf, lame_encoder_printf);
	lame_set_msgf(pd->gf, lame_encoder_printf);

	if (mode->write_meta && options.batch_mpeg_add_id3v2) {
		unsigned char * buf;
		int length;
		int padding_size;
		
		if (mode->meta != NULL) {
			metadata_to_id3v2(mode->meta, &buf, &length);
			padding_size = meta_id3v2_padding_size(length);
			meta_id3v2_pad(&buf, &length, padding_size);
			meta_id3v2_write_tag(pd->out, buf, length);
		}
	}

	ret = lame_init_params(pd->gf);
	return (ret >= 0) ? 0 : -1;
}


void
lame_encode_block(lame_pencdata_t * pd) {

	int i;
	short int l[LAME_READ];
	short int r[LAME_READ];
	unsigned char mp3buf[LAME_BUFSIZE];
	int n_encoded;
	int n_avail = rb_read_space(pd->rb) / pd->channels / sizeof(float);

	if (n_avail > LAME_READ)
		n_avail = LAME_READ;

	for (i = 0; i < n_avail; i++) {
		float f;

		rb_read(pd->rb, (char *)&f, sizeof(float));
		l[i] = 32768.0 * f;
		if (pd->channels == 2) {
			rb_read(pd->rb, (char *)&f, sizeof(float));
			r[i] = 32768.0 * f;
		} else {
			r[i] = l[i];
		}
	}

	n_encoded = lame_encode_buffer(pd->gf, l, r, n_avail, mp3buf, LAME_BUFSIZE);

	if (n_encoded < 0) {
		printf("enc_lame.c: encoding error\n");
		return;
	}

	if (fwrite(mp3buf, 1, n_encoded, pd->out) != n_encoded) {
		printf("enc_lame.c: file write error\n");
		return;
	}
}


unsigned int
lame_encoder_write(encoder_t * enc, float * data, int num) {

	lame_pencdata_t * pd = (lame_pencdata_t *)enc->pdata;

	rb_write(pd->rb, (char *)data, num * pd->channels * sizeof(float));
	while (rb_read_space(pd->rb) > LAME_READ * pd->channels * sizeof(float)) {
		lame_encode_block(pd);
	}

	return num;
}


void
lame_encoder_close(encoder_t * enc) {

	lame_pencdata_t * pd = (lame_pencdata_t *)enc->pdata;

	unsigned char mp3buf[LAME_BUFSIZE];
	int n_encoded;

	lame_encode_block(pd);
	
	n_encoded = lame_encode_flush(pd->gf, mp3buf, LAME_BUFSIZE);

	if (n_encoded < 0) {
		printf("enc_lame.c: encoding error\n");
		return;
	}

	if (fwrite(mp3buf, 1, n_encoded, pd->out) != n_encoded) {
		printf("enc_lame.c: file write error\n");
		return;
	}

	fflush(pd->out);
	lame_mp3_tags_fid(pd->gf, pd->out);
	lame_close(pd->gf);

	fseek(pd->out, 0L, SEEK_END);

	if (enc->mode->write_meta && options.batch_mpeg_add_ape) {
		ape_tag_t tag;
		memset(&tag, 0x00, sizeof(ape_tag_t));
		metadata_to_ape_tag(enc->mode->meta, &tag);

		if (tag.header.item_count > 0) {
			int length = tag.header.tag_size + 32;
			unsigned char * data = calloc(1, length);
			if (data == NULL) {
				fprintf(stderr, "enc_lame.c: calloc error\n");
				return;
			}
			meta_ape_render(&tag, data);
			if (data != NULL && length > 0) {
				if (fwrite(data, 1, length, pd->out) != length) {
					fprintf(stderr, "enc_lame.c: fwrite() failed\n");
					fclose(pd->out);
					return;
				}
			}
			free(data);
		}
		meta_ape_free(&tag);
	}

	if (enc->mode->write_meta && options.batch_mpeg_add_id3v1) {
		if (metadata_get_frame_by_tag(enc->mode->meta, META_TAG_ID3v1, NULL) != NULL) {
			int ret;
			unsigned char id3v1[128];
			ret = metadata_to_id3v1(enc->mode->meta, id3v1);
			if (ret != META_ERROR_NONE) {
				fprintf(stderr, "enc_lame.c: metadata_to_id3v1() returned %d\n", ret);
				return;
			}

			if (fwrite(id3v1, 1, 128, pd->out) != 128) {
				fprintf(stderr, "enc_lame.c: fwrite() failed\n");
				fclose(pd->out);
				return;
			}
		}
	}

	rb_free(pd->rb);
	fclose(pd->out);
}


#else
encoder_t *
lame_encoder_init(file_encoder_t * fenc) {

	return NULL;
}
#endif /* HAVE_LAME */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

