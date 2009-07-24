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
#include <ctype.h>

#include "dec_lavc.h"


#ifdef HAVE_LAVC

/* uncomment this to get some debug info */
/* #define LAVC_DEBUG */

extern size_t sample_size;

/* return 1 if reached end of stream, 0 else */
int
decode_lavc(decoder_t * dec) {

        lavc_pdata_t * pd = (lavc_pdata_t *)dec->pdata;
        file_decoder_t * fdec = dec->fdec;

	AVPacket packet;
        int16_t samples[AVCODEC_MAX_AUDIO_FRAME_SIZE];
        float fsamples[AVCODEC_MAX_AUDIO_FRAME_SIZE];
        int n_bytes = AVCODEC_MAX_AUDIO_FRAME_SIZE;

	if (av_read_frame(pd->avFormatCtx, &packet) < 0)
		return 1;

	if (packet.stream_index == pd->audioStream) {

		avcodec_decode_audio2(pd->avCodecCtx, samples, &n_bytes, packet.data, packet.size);
		if (n_bytes > 0) {
			int i;
			for (i = 0; i < n_bytes/2; i++) {
				fsamples[i] = samples[i] * fdec->voladj_lin / 32768.f;
			}
			rb_write(pd->rb, (char *)fsamples, n_bytes/2 * sample_size);
		}
	}
	av_free_packet(&packet);
        return 0;
}


decoder_t *
lavc_decoder_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_lavc.c: lavc_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(lavc_pdata_t))) == NULL) {
                fprintf(stderr, "dec_lavc.c: lavc_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->init = lavc_decoder_init;
	dec->destroy = lavc_decoder_destroy;
	dec->open = lavc_decoder_open;
	dec->send_metadata = lavc_decoder_send_metadata;
	dec->close = lavc_decoder_close;
	dec->read = lavc_decoder_read;
	dec->seek = lavc_decoder_seek;

	return dec;
}


void
lavc_decoder_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


int
lavc_decoder_open(decoder_t * dec, char * filename) {

	lavc_pdata_t * pd = (lavc_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	int i;

	if (av_open_input_file(&pd->avFormatCtx, filename, NULL, 0, NULL) != 0)
		return DECODER_OPEN_BADLIB;

	if (av_find_stream_info(pd->avFormatCtx) < 0)
		return DECODER_OPEN_BADLIB;

	/* debug */
#ifdef LAVC_DEBUG
	dump_format(pd->avFormatCtx, 0, filename, 0);
#endif /* LAVC_DEBUG */

	pd->audioStream = -1;
	for (i = 0; i < pd->avFormatCtx->nb_streams; i++) {
		if (pd->avFormatCtx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
			pd->audioStream = i;
			break;
		}
	}
	if (pd->audioStream == -1)
		return DECODER_OPEN_BADLIB;

	pd->avCodecCtx = pd->avFormatCtx->streams[pd->audioStream]->codec;

	pd->time_base = pd->avFormatCtx->streams[pd->audioStream]->time_base;

	pd->avCodec = avcodec_find_decoder(pd->avCodecCtx->codec_id);
	if (pd->avCodec == NULL)
		return DECODER_OPEN_BADLIB;

	if (avcodec_open(pd->avCodecCtx, pd->avCodec) < 0)
		return DECODER_OPEN_BADLIB;

	if ((pd->avCodecCtx->channels != 1) && (pd->avCodecCtx->channels != 2)) {
		fprintf(stderr,
			"lavc_decoder_open: audio stream with %d channels is unsupported\n",
			pd->avCodecCtx->channels);
		return DECODER_OPEN_FERROR;
	}

        pd->is_eos = 0;
        pd->rb = rb_create(pd->avCodecCtx->channels * sample_size * RB_LAVC_SIZE);

	fdec->fileinfo.channels = pd->avCodecCtx->channels;
	fdec->fileinfo.sample_rate = pd->avCodecCtx->sample_rate;
	fdec->fileinfo.bps = pd->avCodecCtx->bit_rate;
	if (pd->avFormatCtx->duration > 0) {
		fdec->fileinfo.total_samples = pd->avFormatCtx->duration
			/ 1000000.0 * pd->avCodecCtx->sample_rate;
	} else {
		fdec->fileinfo.total_samples = 0;
	}

	fdec->file_lib = LAVC_LIB;
	snprintf(dec->format_str, MAXLEN-1, "%s/%s", pd->avFormatCtx->iformat->name, pd->avCodec->name);
	for (i = 0; dec->format_str[i] != '\0'; i++) {
		dec->format_str[i] = toupper(dec->format_str[i]);
	}
	
	return DECODER_OPEN_SUCCESS;
}


void
lavc_decoder_send_metadata(decoder_t * dec) {
}


void
lavc_decoder_close(decoder_t * dec) {

	lavc_pdata_t * pd = (lavc_pdata_t *)dec->pdata;

	avcodec_close(pd->avCodecCtx);
	av_close_input_file(pd->avFormatCtx);
	rb_free(pd->rb);
}


unsigned int
lavc_decoder_read(decoder_t * dec, float * dest, int num) {

	lavc_pdata_t * pd = (lavc_pdata_t *)dec->pdata;

        unsigned int numread = 0;
        unsigned int n_avail = 0;

        while ((rb_read_space(pd->rb) <
                num * pd->avCodecCtx->channels * sample_size) && (!pd->is_eos)) {

                pd->is_eos = decode_lavc(dec);
        }

        n_avail = rb_read_space(pd->rb) / (pd->avCodecCtx->channels * sample_size);

        if (n_avail > num)
                n_avail = num;

        rb_read(pd->rb, (char *)dest, n_avail *	pd->avCodecCtx->channels * sample_size);
        numread = n_avail;
        return numread;
}


void
lavc_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {
	
	lavc_pdata_t * pd = (lavc_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	char flush_dest;

	long long pos = fdec->fileinfo.total_samples - fdec->samples_left;
	int64_t timestamp = (double)seek_to_pos / fdec->fileinfo.sample_rate
		* pd->time_base.den / pd->time_base.num;
	int flags = (pos > seek_to_pos) ? AVSEEK_FLAG_BACKWARD : 0;

	if (av_seek_frame(pd->avFormatCtx, pd->audioStream, timestamp, flags) >= 0) {
		fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
		/* empty lavc decoder ringbuffer */
		while (rb_read_space(pd->rb))
			rb_read(pd->rb, &flush_dest, sizeof(char));
	} else {
		fprintf(stderr, "lavc_decoder_seek: warning: av_seek_frame() failed\n");
	}
}


#else
decoder_t *
lavc_decoder_init(file_decoder_t * fdec) {

	return NULL;
}
#endif /* HAVE_LAVC */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

