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
#include <string.h>
#include <ogg/ogg.h>
#include <speex/speex_header.h>
#include <speex/speex_callbacks.h>

#include "../rb.h"
#include "dec_speex.h"


extern size_t sample_size;


static int
parse_speex_header(speex_pdata_t * pd, ogg_packet * op) {

	SpeexHeader * header;
	const SpeexMode * mode;
	int enh = 1;

	if ((op->bytes < 8) || (memcmp(op->packet, "Speex   ", 8) != 0)) {
		return -1;
	}

	if ((header = speex_packet_to_header((char *)op->packet, op->bytes)) == NULL) {
		fprintf(stderr, "dec_speex.c: cannot read Speex header\n");
		return -1;
	}

	if ((header->mode < 0) || (header->mode >= SPEEX_NB_MODES) ||
	    ((mode = speex_lib_get_mode(header->mode)) == NULL)) {
		fprintf(stderr, "dec_speex.c: invalid Speex mode %d\n", header->mode);
		speex_header_free(header);
		return -1;
	}

	if (mode->bitstream_version != header->mode_bitstream_version) {
		fprintf(stderr, "dec_speex.c: unsupported bitstream version %d (expected %d)\n",
			header->mode_bitstream_version, mode->bitstream_version);
		speex_header_free(header);
		return -1;
	}

	pd->sample_rate = header->rate;
	pd->channels = header->nb_channels;
	pd->nframes = (header->frames_per_packet > 0) ? header->frames_per_packet : 1;
	pd->extra_headers = (header->extra_headers > 0) ? header->extra_headers : 0;
	speex_header_free(header);

	if ((pd->channels != 1) && (pd->channels != 2)) {
		fprintf(stderr, "dec_speex.c: Ogg Speex with %d channels is unsupported\n",
			pd->channels);
		return -1;
	}

	if (pd->sample_rate <= 0) {
		fprintf(stderr, "dec_speex.c: invalid sample rate %d\n", pd->sample_rate);
		return -1;
	}

	pd->decoder = speex_decoder_init(mode);
	speex_decoder_ctl(pd->decoder, SPEEX_GET_FRAME_SIZE, &pd->frame_size);
	speex_decoder_ctl(pd->decoder, SPEEX_GET_LOOKAHEAD, &pd->lookahead);
	speex_decoder_ctl(pd->decoder, SPEEX_SET_ENH, &enh);

	if (pd->frame_size > SPEEX_MAX_FRAME_SIZE) {
		fprintf(stderr, "dec_speex.c: unsupported frame size %d\n", pd->frame_size);
		return -1;
	}

	/* N.B.: Stereo Speex is a mono stream with in-band intensity
	   stereo parameters, picked up by this handler while decoding and
	   applied by speex_decode_stereo(). */
	pd->stereo = speex_stereo_state_init();
	if (pd->channels == 2) {
		SpeexCallback callback;
		callback.callback_id = SPEEX_INBAND_STEREO;
		callback.func = speex_std_stereo_request_handler;
		callback.data = pd->stereo;
		callback.reserved1 = NULL;
		callback.reserved2 = 0;
		speex_decoder_ctl(pd->decoder, SPEEX_SET_HANDLER, &callback);
	}

	return 0;
}


static void
write_frame(speex_pdata_t * pd, float * frame) {

	long long start = pd->pos;
	long long end = pd->pos + pd->frame_size;
	long long i;

	pd->pos = end;

	if (start < pd->discard_until)
		start = pd->discard_until;
	if (start < 0)
		start = 0;
	if (end > pd->total_samples)
		end = pd->total_samples;
	if (start >= end)
		return;

	frame += (start - (pd->pos - pd->frame_size)) * pd->channels;
	for (i = 0; i < (end - start) * pd->channels; i++) {
		frame[i] /= 32768.0f;
		if (frame[i] > 1.0f) {
			frame[i] = 1.0f;
		} else if (frame[i] < -1.0f) {
			frame[i] = -1.0f;
		}
	}
	rb_write(pd->rb, (char *)frame, (end - start) * pd->channels * sample_size);
}


static void
decode_packet(speex_pdata_t * pd, ogg_packet * op) {

	float frame[2 * SPEEX_MAX_FRAME_SIZE];
	int j;

	speex_bits_read_from(&pd->bits, (char *)op->packet, op->bytes);

	for (j = 0; j < pd->nframes; j++) {

		int ret = speex_decode(pd->decoder, &pd->bits, frame);

		if (ret == -1) {
			break;
		}
		if (ret == -2) {
			fprintf(stderr, "dec_speex.c: decoding error, corrupt stream?\n");
			break;
		}
		if (speex_bits_remaining(&pd->bits) < 0) {
			fprintf(stderr, "dec_speex.c: decoding overflow, corrupt stream?\n");
			break;
		}

		if (pd->channels == 2) {
			speex_decode_stereo(frame, pd->frame_size, pd->stereo);
		}
		if (pd->synced) {
			write_frame(pd, frame);
		}
	}
}


static int
add_seek_point(speex_pdata_t * pd, long long granulepos, oggz_off_t offset) {

	if (pd->n_seek_points == pd->seek_points_size) {
		size_t size = pd->seek_points_size ? 2 * pd->seek_points_size : 1024;
		speex_seek_point_t * p = realloc(pd->seek_points, size * sizeof(speex_seek_point_t));
		if (p == NULL) {
			fprintf(stderr, "dec_speex.c: add_seek_point(): realloc error\n");
			return -1;
		}
		pd->seek_points = p;
		pd->seek_points_size = size;
	}
	pd->seek_points[pd->n_seek_points].granulepos = granulepos;
	pd->seek_points[pd->n_seek_points].offset = offset;
	++pd->n_seek_points;
	return 0;
}


static int
read_ogg_packet(OGGZ * oggz, oggz_packet * zp, long serialno, void * user_data) {

	ogg_packet * op = &zp->op;
	speex_pdata_t * pd = (speex_pdata_t *)user_data;

	if (pd->exploring && (pd->packetno == 0)) {
		pd->serialno = serialno;
		if (!op->b_o_s || (parse_speex_header(pd, op) != 0)) {
			pd->error = 1;
			return OGGZ_STOP_ERR;
		}
		++pd->packetno;
		return OGGZ_CONTINUE;
	}

	if (serialno != pd->serialno) {
		return OGGZ_CONTINUE;
	}

	if (op->b_o_s) {
		pd->packetno = 0;
	}

	if (pd->packetno++ < 2 + pd->extra_headers) {
		return OGGZ_CONTINUE;
	}

	if (pd->exploring) {
		pd->raw_samples += pd->nframes * pd->frame_size;
		if (op->granulepos != -1) {
			if (pd->first_granulepos < 0) {
				pd->first_granulepos = op->granulepos;
				pd->first_granulepos_raw = pd->raw_samples;
			}
			pd->last_granulepos = op->granulepos;
			if (add_seek_point(pd, op->granulepos, oggz_tell(oggz)) != 0) {
				pd->error = 1;
				return OGGZ_STOP_ERR;
			}
		}
		return OGGZ_CONTINUE;
	}

	decode_packet(pd, op);

	/* N.B.: The granulepos of a packet is the stream position of its
	   last sample. Decoder output lags by the decoder lookahead. */
	if (!pd->synced && (op->granulepos != -1)) {
		pd->synced = 1;
		pd->pos = op->granulepos - pd->lookahead;
		if (pd->pos > pd->discard_until) {
			/* seeked past the target, need to start over */
			pd->synced = 0;
			return OGGZ_STOP_OK;
		}
	}

	return OGGZ_CONTINUE;
}


static void
flush_rb(speex_pdata_t * pd) {

	rb_read_advance(pd->rb, rb_read_space(pd->rb));
}


static void
reset_decoder(speex_pdata_t * pd) {

	speex_decoder_ctl(pd->decoder, SPEEX_RESET_STATE, NULL);
	speex_stereo_state_reset(pd->stereo);
	pd->is_eos = 0;
	flush_rb(pd);
}


/* Rewind to the start of the stream. Returns 0 on success. */
static int
restart_stream(speex_pdata_t * pd) {

	if (oggz_seek(pd->oggz, 0, SEEK_SET) != 0) {
		return -1;
	}
	reset_decoder(pd);
	pd->packetno = 0;
	pd->pos = -pd->skip;
	pd->synced = 1;
	return 0;
}


/* return 1 if reached end of stream, 0 else */
static int
decode_speex(decoder_t * dec) {

	speex_pdata_t * pd = (speex_pdata_t *)dec->pdata;
	long ret = oggz_read(pd->oggz, 4096);

	if (!pd->synced && (pd->pos > pd->discard_until)) {
		if (restart_stream(pd) != 0) {
			return 1;
		}
		return 0;
	}

	return (ret > 0) ? 0 : 1;
}


decoder_t *
speex_dec_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_speex.c: speex_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(speex_pdata_t))) == NULL) {
                fprintf(stderr, "dec_speex.c: speex_decoder_new() failed: calloc error\n");
		free(dec);
                return NULL;
        }

	dec->init = speex_dec_init;
	dec->destroy = speex_dec_destroy;
	dec->open = speex_dec_open;
	dec->send_metadata = speex_dec_send_metadata;
	dec->close = speex_dec_close;
	dec->read = speex_dec_read;
	dec->seek = speex_dec_seek;

	return dec;
}


void
speex_dec_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


static void
free_pdata_members(speex_pdata_t * pd) {

	if (pd->oggz != NULL) {
		oggz_close(pd->oggz);
		pd->oggz = NULL;
	}
	if (pd->decoder != NULL) {
		speex_decoder_destroy(pd->decoder);
		pd->decoder = NULL;
	}
	if (pd->stereo != NULL) {
		speex_stereo_state_destroy(pd->stereo);
		pd->stereo = NULL;
	}
	speex_bits_destroy(&pd->bits);
	if (pd->rb != NULL) {
		rb_free(pd->rb);
		pd->rb = NULL;
	}
	free(pd->seek_points);
	pd->seek_points = NULL;
	pd->n_seek_points = pd->seek_points_size = 0;
}


int
speex_dec_open(decoder_t * dec, char * filename) {

	speex_pdata_t * pd = (speex_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	FILE * f;
	char ogg_sig[4];
	long length_in_bytes;
	long ret;


	if ((f = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "speex_decoder_open: fopen() failed\n");
		return DECODER_OPEN_FERROR;
	}
	if (fread(ogg_sig, 1, 4, f) != 4) {
		fclose(f);
		return DECODER_OPEN_BADLIB;
	}
	fclose(f);
	if (memcmp(ogg_sig, "OggS", 4) != 0) {
		return DECODER_OPEN_BADLIB;
	}

        if ((pd->oggz = oggz_open(filename, OGGZ_READ)) == NULL) {
                fprintf(stderr, "dec_speex.c: nonexistent or unaccessible file %s\n", filename);
                return DECODER_OPEN_FERROR;
        }
        oggz_set_read_callback(pd->oggz, -1, read_ogg_packet, pd);
	speex_bits_init(&pd->bits);

	/* parse the whole stream to get its length */
	pd->exploring = 1;
	pd->packetno = 0;
	pd->error = 0;
	pd->first_granulepos = -1;
	pd->last_granulepos = -1;
	do {
		ret = oggz_read(pd->oggz, 65536);
	} while ((ret > 0) && !pd->error);

	if (pd->error || (pd->decoder == NULL)) {
		free_pdata_members(pd);
		return DECODER_OPEN_BADLIB;
	}

	length_in_bytes = oggz_tell(pd->oggz);

	if (pd->first_granulepos >= 0) {
		pd->skip = pd->first_granulepos_raw - pd->first_granulepos + pd->lookahead;
		if (pd->skip < 0)
			pd->skip = 0;
		pd->total_samples = pd->last_granulepos;
	} else {
		pd->skip = pd->lookahead;
		pd->total_samples = pd->raw_samples;
	}
	if (pd->total_samples > pd->raw_samples - pd->skip)
		pd->total_samples = pd->raw_samples - pd->skip;
	if (pd->total_samples <= 0) {
		fprintf(stderr, "dec_speex.c: no audio in %s\n", filename);
		free_pdata_members(pd);
		return DECODER_OPEN_FERROR;
	}

	pd->exploring = 0;
	pd->rb = rb_create(pd->channels * sample_size * RB_SPEEX_SIZE);
	pd->discard_until = 0;
	if (restart_stream(pd) != 0) {
		fprintf(stderr, "dec_speex.c: cannot rewind %s\n", filename);
		free_pdata_members(pd);
		return DECODER_OPEN_FERROR;
	}

	fdec->fileinfo.channels = pd->channels;
	fdec->fileinfo.sample_rate = pd->sample_rate;
	fdec->fileinfo.total_samples = pd->total_samples;
	fdec->fileinfo.bps = 8.0 * length_in_bytes * pd->sample_rate / pd->total_samples;

	fdec->file_lib = SPEEX_LIB;
	arr_strlcpy(dec->format_str, "Ogg Speex");

	return DECODER_OPEN_SUCCESS;
}


void
speex_dec_send_metadata(decoder_t * dec) {
}


void
speex_dec_close(decoder_t * dec) {

	free_pdata_members((speex_pdata_t *)dec->pdata);
}


unsigned int
speex_dec_read(decoder_t * dec, float * dest, int num) {

	speex_pdata_t * pd = (speex_pdata_t *)dec->pdata;

	unsigned int numread = 0;
	unsigned int n_avail = 0;


	while ((rb_read_space(pd->rb) <
		num * pd->channels * sample_size) && (!pd->is_eos)) {

		pd->is_eos = decode_speex(dec);
	}

	n_avail = rb_read_space(pd->rb) /
		(pd->channels * sample_size);

	if (n_avail > num)
		n_avail = num;

	rb_read(pd->rb, (char *)dest, n_avail *
			     pd->channels * sample_size);

	numread = n_avail;
	return numread;
}


void
speex_dec_seek(decoder_t * dec, unsigned long long seek_to_pos) {

	speex_pdata_t * pd = (speex_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	long long target;
	size_t lo, hi;

	if (seek_to_pos > (unsigned long long)pd->total_samples)
		seek_to_pos = pd->total_samples;
	target = seek_to_pos;

	fdec->samples_left = pd->total_samples - target;
	pd->discard_until = target;

	/* find the last page ending at or before the target */
	lo = 0;
	hi = pd->n_seek_points;
	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		if (pd->seek_points[mid].granulepos - pd->lookahead <= target)
			lo = mid + 1;
		else
			hi = mid;
	}

	/* N.B.: Decoding resumes from the page before that one. Its
	   packets warm up the decoder, and the stream position is known
	   once its last packet (carrying a granulepos) is decoded. */
	if ((lo >= 2) &&
	    (oggz_seek(pd->oggz, pd->seek_points[lo - 2].offset, SEEK_SET) >= 0)) {
		reset_decoder(pd);
		pd->packetno = 2 + pd->extra_headers;
		pd->synced = 0;
		pd->pos = 0;
	} else if (restart_stream(pd) != 0) {
		fprintf(stderr, "speex_dec_seek(): seeking failed\n");
		pd->is_eos = 1;
	}
}



// vim: shiftwidth=8:tabstop=8:softtabstop=8 :
