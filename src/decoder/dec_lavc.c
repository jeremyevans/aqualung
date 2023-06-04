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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <libavutil/avutil.h>
#include <libavcodec/version.h>

#include "../common.h"
#include "../rb.h"
#include "dec_lavc.h"


/* uncomment this to get some debug info */
/* #define LAVC_DEBUG */

extern size_t sample_size;

/* interleaved: */
void conv_fmt_u8(int n_samples, int sample_size, float * fsamples, AVFrame * frame) {
	int i;
	for (i = 0; i < n_samples; i++) {
		*fsamples++ = (*((uint8_t*)(frame->extended_data[0] + i*sample_size)) - 128) / 256.f;
	}
}
void conv_fmt_s16(int n_samples, int sample_size, float * fsamples, AVFrame * frame) {
	int i;
	for (i = 0; i < n_samples; i++) {
		*fsamples++ = *((int16_t*)(frame->extended_data[0] + i*sample_size)) / 32768.f;
	}
}
void conv_fmt_s32(int n_samples, int sample_size, float * fsamples, AVFrame * frame) {
	int i;
	for (i = 0; i < n_samples; i++) {
		*fsamples++ = *((int32_t*)(frame->extended_data[0] + i*sample_size)) / 2147483648.f;
	}
}
void conv_fmt_flt(int n_samples, int sample_size, float * fsamples, AVFrame * frame) {
	int i;
	for (i = 0; i < n_samples; i++) {
		*fsamples++ = *((float*)(frame->extended_data[0] + i*sample_size));
	}
}
void conv_fmt_dbl(int n_samples, int sample_size, float * fsamples, AVFrame * frame) {
	int i;
	for (i = 0; i < n_samples; i++) {
		*fsamples++ = *((double*)(frame->extended_data[0] + i*sample_size));
	}
}

/* planar: */
void conv_fmt_u8p(int n_samples, int channels, int sample_size, float * fsamples, AVFrame * frame) {
	int i, ch;
	for (i = 0; i < n_samples; i++) {
		for (ch = 0; ch < channels; ch++) {
			*fsamples++ = (*((uint8_t*)(frame->extended_data[ch] + i*sample_size)) - 128) / 256.f;
		}
	}
}
void conv_fmt_s16p(int n_samples, int channels, int sample_size, float * fsamples, AVFrame * frame) {
	int i, ch;
	for (i = 0; i < n_samples; i++) {
		for (ch = 0; ch < channels; ch++) {
			*fsamples++ = *((int16_t*)(frame->extended_data[ch] + i*sample_size)) / 32768.f;
		}
	}
}
void conv_fmt_s32p(int n_samples, int channels, int sample_size, float * fsamples, AVFrame * frame) {
	int i, ch;
	for (i = 0; i < n_samples; i++) {
		for (ch = 0; ch < channels; ch++) {
			*fsamples++ = *((int32_t*)(frame->extended_data[ch] + i*sample_size)) / 2147483648.f;
		}
	}
}
void conv_fmt_fltp(int n_samples, int channels, int sample_size, float * fsamples, AVFrame * frame) {
	int i, ch;
	for (i = 0; i < n_samples; i++) {
		for (ch = 0; ch < channels; ch++) {
			*fsamples++ = *((float*)(frame->extended_data[ch] + i*sample_size));
		}
	}
}
void conv_fmt_dblp(int n_samples, int channels, int sample_size, float * fsamples, AVFrame * frame) {
	int i, ch;
	for (i = 0; i < n_samples; i++) {
		for (ch = 0; ch < channels; ch++) {
			*fsamples++ = *((double*)(frame->extended_data[ch] + i*sample_size));
		}
	}
}

#if defined(LIBAVCODEC_VERSION_MAJOR) && LIBAVCODEC_VERSION_MAJOR >= 59
#define FFMPEG5
#endif

#ifdef FFMPEG5
#define codec_channels(ctx) (ctx)->ch_layout.nb_channels
#define stream_codec(stream) (stream)->codecpar
#else
#define codec_channels(ctx) (ctx)->channels
#define stream_codec(stream) (stream)->codec
#endif

/* Loosely based on avcodec_decode_audio3() implementation found at:
 * https://raw.github.com/FFmpeg/FFmpeg/master/libavcodec/utils.c
 */
int decode_audio(AVCodecContext *avctx, float *fsamples, int *frame_size_ptr, AVPacket *avpkt) {
	int ret;
	AVFrame frame = { { 0 } };

#ifdef FFMPEG5
	ret = avcodec_send_packet(avctx, avpkt);
	if (ret < 0) {
		*frame_size_ptr = 0;
		return ret;
	}

	while (ret >= 0) {
		ret = avcodec_receive_frame(avctx, &frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			return 0;
		}
		else if (ret < 0) {
			*frame_size_ptr = 0;
			return ret;
		}
#else
	int got_frame = 0;

	ret = avcodec_decode_audio4(avctx, &frame, &got_frame, avpkt);
	if (ret >= 0 && got_frame) {
#endif
		int64_t channels = codec_channels(avctx);
		int plane_size;
		int planar = av_sample_fmt_is_planar(avctx->sample_fmt);
		int sample_size = av_get_bytes_per_sample(avctx->sample_fmt);
		int data_size = av_samples_get_buffer_size(&plane_size, channels,
							   frame.nb_samples, avctx->sample_fmt, 1);
		int n_samples = plane_size / sample_size;
		if (MAX_AUDIO_FRAME_SIZE < *frame_size_ptr + data_size) {
			av_log(avctx, AV_LOG_ERROR, "output buffer size is too small for "
			       "the current frame (%d < %d)\n", *frame_size_ptr, data_size);
			return AVERROR(EINVAL);
		}

		switch (avctx->sample_fmt) {
		/* interleaved: */
		case AV_SAMPLE_FMT_U8: conv_fmt_u8(n_samples, sample_size, fsamples, &frame); break;
		case AV_SAMPLE_FMT_S16: conv_fmt_s16(n_samples, sample_size, fsamples, &frame); break;
		case AV_SAMPLE_FMT_S32: conv_fmt_s32(n_samples, sample_size, fsamples, &frame); break;
		case AV_SAMPLE_FMT_FLT: conv_fmt_flt(n_samples, sample_size, fsamples, &frame); break;
		case AV_SAMPLE_FMT_DBL: conv_fmt_dbl(n_samples, sample_size, fsamples, &frame); break;
		/* planar: */
		case AV_SAMPLE_FMT_U8P: conv_fmt_u8p(n_samples, channels, sample_size, fsamples, &frame); break;
		case AV_SAMPLE_FMT_S16P: conv_fmt_s16p(n_samples, channels, sample_size, fsamples, &frame); break;
		case AV_SAMPLE_FMT_S32P: conv_fmt_s32p(n_samples, channels, sample_size, fsamples, &frame); break;
		case AV_SAMPLE_FMT_FLTP: conv_fmt_fltp(n_samples, channels, sample_size, fsamples, &frame); break;
		case AV_SAMPLE_FMT_DBLP: conv_fmt_dblp(n_samples, channels, sample_size, fsamples, &frame); break;
		default:
			fprintf(stderr, "Fatal error: dec_lavc.c: decode_audio #1: invalid sample format %s\n",
				av_get_sample_fmt_name(avctx->sample_fmt));
			exit(1); // no, we really don't want to handle this gracefully
		}
		*frame_size_ptr += planar ? n_samples * channels : n_samples;
	}
	return ret;
}

/* return 1 if reached end of stream, 0 else */
int
decode_lavc(decoder_t * dec) {

        lavc_pdata_t * pd = (lavc_pdata_t *)dec->pdata;
        file_decoder_t * fdec = dec->fdec;

	AVPacket packet;
        float fsamples[MAX_AUDIO_FRAME_SIZE];
	int n_samples = 0;
	int i;

	if (av_read_frame(pd->avFormatCtx, &packet) < 0)
		return 1;

	if (!(packet.stream_index == pd->audioStream))
		goto end;

	decode_audio(pd->avCodecCtx, fsamples, &n_samples, &packet);
	if (n_samples <= 0) goto end;
	for (i = 0; i < n_samples; i++) {
		fsamples[i] *= fdec->voladj_lin;
	}

	rb_write(pd->rb, (char *)fsamples, n_samples * sample_size);
end:
#ifdef FFMPEG5
	av_packet_unref(&packet);
#else
	av_free_packet(&packet);
#endif
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
	int channels;

	if (avformat_open_input(&pd->avFormatCtx, filename, NULL, NULL) != 0)
	{
		return DECODER_OPEN_BADLIB;
	}

	if (avformat_find_stream_info(pd->avFormatCtx, NULL) < 0)
	{
		return DECODER_OPEN_BADLIB;
	}

	/* debug */
#ifdef LAVC_DEBUG
#ifdef FFMPEG5
	av_dump_format(pd->avFormatCtx, 0, filename, 0);
#else
	dump_format(pd->avFormatCtx, 0, filename, 0);
#endif
#endif /* LAVC_DEBUG */

	pd->audioStream = -1;
	for (i = 0; i < pd->avFormatCtx->nb_streams; i++) {
		if (stream_codec(pd->avFormatCtx->streams[i])->codec_type == AVMEDIA_TYPE_AUDIO) {
			pd->audioStream = i;
			break;
		}
	}
	if (pd->audioStream == -1)
		return DECODER_OPEN_BADLIB;


#ifdef FFMPEG5
	pd->avCodec = (AVCodec *)avcodec_find_decoder(pd->avFormatCtx->streams[pd->audioStream]->codecpar->codec_id);
	pd->avCodecCtx = avcodec_alloc_context3(pd->avCodec);
#else
	pd->avCodecCtx = pd->avFormatCtx->streams[pd->audioStream]->codec;
	pd->avCodec = avcodec_find_decoder(pd->avCodecCtx->codec_id);
#endif

	pd->time_base = pd->avFormatCtx->streams[pd->audioStream]->time_base;

	if (pd->avCodec == NULL)
		return DECODER_OPEN_BADLIB;

#ifdef FFMPEG5
	avcodec_parameters_to_context(pd->avCodecCtx, pd->avFormatCtx->streams[pd->audioStream]->codecpar);
#endif
	if (avcodec_open2(pd->avCodecCtx, pd->avCodec, NULL) < 0)
	{
		return DECODER_OPEN_BADLIB;
	}

	channels = codec_channels(pd->avCodecCtx);
	if (channels != 1 && channels != 2) {
		fprintf(stderr,
			"lavc_decoder_open: audio stream with %d channels is unsupported\n",
			channels);
		return DECODER_OPEN_FERROR;
	}

        pd->is_eos = 0;
        pd->rb = rb_create(channels * sample_size * RB_LAVC_SIZE);

	fdec->fileinfo.channels = channels;
	fdec->fileinfo.sample_rate = pd->avCodecCtx->sample_rate;
	fdec->fileinfo.bps = pd->avCodecCtx->bit_rate;
	if (pd->avFormatCtx->duration > 0) {
		fdec->fileinfo.total_samples = pd->avFormatCtx->duration
			/ 1000000.0 * pd->avCodecCtx->sample_rate;
	} else {
		fdec->fileinfo.total_samples = 0;
	}

	fdec->file_lib = LAVC_LIB;
	arr_snprintf(dec->format_str, "%s/%s", pd->avFormatCtx->iformat->name, pd->avCodec->name);
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

	avformat_close_input(&pd->avFormatCtx);

	rb_free(pd->rb);
}


unsigned int
lavc_decoder_read(decoder_t * dec, float * dest, int num) {

	lavc_pdata_t * pd = (lavc_pdata_t *)dec->pdata;

        unsigned int numread = 0;
        unsigned int n_avail = 0;
	int64_t channels = codec_channels(pd->avCodecCtx);

        while ((rb_read_space(pd->rb) <
                num * channels * sample_size) && (!pd->is_eos)) {

                pd->is_eos = decode_lavc(dec);
        }

        n_avail = rb_read_space(pd->rb) / (channels * sample_size);

        if (n_avail > num)
                n_avail = num;

        rb_read(pd->rb, (char *)dest, n_avail *	channels * sample_size);
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



// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

