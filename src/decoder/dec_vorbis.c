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
#include <unistd.h>
#include <glib.h>
#include <ogg/ogg.h>

#include "../httpc.h"
#include "../metadata.h"
#include "../metadata_api.h"
#include "../metadata_ogg.h"
#include "../rb.h"
#include "file_decoder.h"
#include "dec_vorbis.h"


extern size_t sample_size;


# if 0
void
dump_vc(vorbis_comment * vc) {

	int i;

	if (!vc) {
		printf("vc = NULL\n");
		return;
	}

	for (i = 0; i < vc->comments; i++) {
		printf("[%d] %s\n", i, vc->user_comments[i]);
	}
	printf("[V] %s\n", vc->vendor);
}
#endif

int
vorbis_write_metadata(file_decoder_t * fdec, metadata_t * meta) {

	GSList * slist;

	int n_pages;
	unsigned char * payload;
	unsigned int length;

	slist = meta_ogg_parse(fdec->filename);
	payload = meta_ogg_vc_render(meta, &length);
	if (payload == NULL) {
		return META_ERROR_INTERNAL;
	}
	slist = meta_ogg_vc_encapsulate_payload(slist, &payload, length, &n_pages);
	if (meta_ogg_render(slist, fdec->filename, n_pages) < 0) {
		meta_ogg_free(slist);
		free(payload);
		return META_ERROR_NOT_WRITABLE;
	}
	meta_ogg_free(slist);
	free(payload);
	return META_ERROR_NONE;
}

void
vorbis_send_metadata(file_decoder_t * fdec, vorbis_pdata_t * pd) {

	vorbis_comment * vc = ov_comment(&pd->vf, -1);
	metadata_t * meta = metadata_from_vorbis_comment(vc);
	if (fdec->is_stream) {
		meta->writable = 0;
	} else {
		if (access(fdec->filename, R_OK | W_OK) == 0) {
			meta->writable = 1;
			fdec->meta_write = vorbis_write_metadata;
		} else {
			meta->writable = 0;
		}
	}
	if (fdec->is_stream) {
		httpc_add_headers_meta(pd->session, meta);
	}
	meta->fdec = fdec;
	fdec->meta = meta;
}

/* return 1 if reached end of stream, 0 else */
int
decode_vorbis(decoder_t * dec) {

	vorbis_pdata_t * pd = (vorbis_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	int i;
	long bytes_read;
        float fbuffer[VORBIS_BUFSIZE/2];
        char buffer[VORBIS_BUFSIZE];
	int current_section;


	bytes_read = ov_read(&(pd->vf), buffer, VORBIS_BUFSIZE, bigendianp(), 2, 1, &current_section);

	switch (bytes_read) {
	case 0:
		/* end of file */
                return 1;
		break;
	case OV_HOLE:
		if (fdec->is_stream) {
			vorbis_send_metadata(fdec, pd);
			vorbis_decoder_send_metadata(dec);
			break;
		} else {
			printf("dec_vorbis.c/decode_vorbis(): ov_read() returned OV_HOLE\n");
			printf("This indicates an interruption in the Vorbis data (one of:\n");
			printf("garbage between Ogg pages, loss of sync, or corrupt page).\n");
		}
		break;
	case OV_EBADLINK:
		printf("dec_vorbis.c/decode_vorbis(): ov_read() returned OV_EBADLINK\n");
		printf("An invalid stream section was supplied to libvorbisfile.\n");
		break;
	default:
                for (i = 0; i < bytes_read/2; i++) {
                        fbuffer[i] = *((short *)(buffer + 2*i)) * fdec->voladj_lin / 32768.f;
		}
                rb_write(pd->rb, (char *)fbuffer,
                                      bytes_read/2 * sample_size);
		break;
	}
	return 0;
}


decoder_t *
vorbis_decoder_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_vorbis.c: vorbis_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(vorbis_pdata_t))) == NULL) {
                fprintf(stderr, "dec_vorbis.c: vorbis_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->init = vorbis_decoder_init;
	dec->destroy = vorbis_decoder_destroy;
	dec->open = vorbis_decoder_open;
	dec->send_metadata = vorbis_decoder_send_metadata;
	dec->close = vorbis_decoder_close;
	dec->read = vorbis_decoder_read;
	dec->seek = vorbis_decoder_seek;

	return dec;
}


void
vorbis_decoder_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


size_t
read_vorbis_stream(void *ptr, size_t size, size_t nmemb, void * datasource) {

	http_session_t * session = (http_session_t *) datasource;
	return httpc_read(session, (char *)ptr, size*nmemb);
}


int
seek_vorbis_stream(void * datasource, ogg_int64_t offset, int whence) {

	http_session_t * session = (http_session_t *) datasource;

	if (session->type != HTTPC_SESSION_NORMAL)
		return -1; /* HTTP stream is not seekable */

	return httpc_seek(session, offset, whence);
}

long
tell_vorbis_stream(void * datasource) {

	http_session_t * session = (http_session_t *) datasource;
	return httpc_tell(session);
}


int
close_vorbis_stream(void * datasource) {

	http_session_t * session = (http_session_t *) datasource;
	session->fdec->is_stream = 0;
	httpc_close(session);
	httpc_del(session);
	return 0;
}


void
pause_vorbis_stream(decoder_t * dec) {

	vorbis_pdata_t * pd = (vorbis_pdata_t *)dec->pdata;
	char flush_dest;

	httpc_close(pd->session);

	if (pd->session->type == HTTPC_SESSION_STREAM) {
		/* empty vorbis decoder ringbuffer */
		while (rb_read_space(pd->rb))
			rb_read(pd->rb, &flush_dest, sizeof(char));
	}
}


void
resume_vorbis_stream(decoder_t * dec) {

	vorbis_pdata_t * pd = (vorbis_pdata_t *)dec->pdata;
	httpc_reconnect(pd->session);
}


int
vorbis_decoder_finish_open(decoder_t * dec) {

	vorbis_pdata_t * pd = (vorbis_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	pd->vi = ov_info(&(pd->vf), -1);
	if ((pd->vi->channels != 1) && (pd->vi->channels != 2)) {
		fprintf(stderr,
			"vorbis_decoder_open: Ogg Vorbis file with %d channels "
			"is unsupported\n", pd->vi->channels);
		return DECODER_OPEN_FERROR;
	}
	if (ov_streams(&(pd->vf)) != 1) {
		fprintf(stderr,
			"vorbis_decoder_open: This Ogg Vorbis file contains "
			"multiple logical streams.\n"
			"Currently such a file is not supported.\n");
		return DECODER_OPEN_FERROR;
	}

	pd->is_eos = 0;
	pd->rb = rb_create(pd->vi->channels * sample_size * RB_VORBIS_SIZE);
	fdec->fileinfo.channels = pd->vi->channels;
	fdec->fileinfo.sample_rate = pd->vi->rate;
	if (fdec->is_stream && pd->session->type != HTTPC_SESSION_NORMAL) {
		fdec->fileinfo.total_samples = 0;
	} else {
		fdec->fileinfo.total_samples = ov_pcm_total(&(pd->vf), -1);
	}
	fdec->fileinfo.bps = ov_bitrate(&(pd->vf), -1);

	fdec->file_lib = VORBIS_LIB;
	strcpy(dec->format_str, "Ogg Vorbis");

	vorbis_send_metadata(fdec, pd);
	vorbis_decoder_send_metadata(dec);

	return DECODER_OPEN_SUCCESS;
}


int
vorbis_stream_decoder_open(decoder_t * dec, http_session_t * session) {

	vorbis_pdata_t * pd = (vorbis_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	ov_callbacks ov_cb;
        ov_cb.read_func = read_vorbis_stream;
        ov_cb.seek_func = seek_vorbis_stream;
        ov_cb.close_func = close_vorbis_stream;
        ov_cb.tell_func = tell_vorbis_stream;

	if (ov_open_callbacks((void *)session, &(pd->vf), NULL, 0, ov_cb) != 0) {
		/* not an Ogg Vorbis stream */
		return DECODER_OPEN_BADLIB;
	}

	fdec->is_stream = 1;

	pd->session = session;
	dec->pause = pause_vorbis_stream;
	dec->resume = resume_vorbis_stream;

	return vorbis_decoder_finish_open(dec);
}


int
vorbis_decoder_open(decoder_t * dec, char * filename) {

	vorbis_pdata_t * pd = (vorbis_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	if ((pd->vorbis_file = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "vorbis_decoder_open: fopen() failed\n");
		return DECODER_OPEN_FERROR;
	}
	if (ov_open(pd->vorbis_file, &(pd->vf), NULL, 0) != 0) {
		/* not an Ogg Vorbis file */
		fclose(pd->vorbis_file);
		return DECODER_OPEN_BADLIB;
	}

	fdec->is_stream = 0;
	return vorbis_decoder_finish_open(dec);
}


void
vorbis_decoder_send_metadata(decoder_t * dec) {

        file_decoder_t * fdec = dec->fdec;

	if (fdec->meta != NULL && fdec->meta_cb != NULL) {
		fdec->meta_cb(fdec->meta, fdec->meta_cbdata);
	}
}


void
vorbis_decoder_close(decoder_t * dec) {

	vorbis_pdata_t * pd = (vorbis_pdata_t *)dec->pdata;

	ov_clear(&(pd->vf));
	rb_free(pd->rb);
}


unsigned int
vorbis_decoder_read(decoder_t * dec, float * dest, int num) {

	vorbis_pdata_t * pd = (vorbis_pdata_t *)dec->pdata;

	unsigned int numread = 0;
	unsigned int n_avail = 0;


	while ((rb_read_space(pd->rb) <
		num * pd->vi->channels * sample_size) && (!pd->is_eos)) {

		pd->is_eos = decode_vorbis(dec);
	}

	n_avail = rb_read_space(pd->rb) /
		(pd->vi->channels * sample_size);

	if (n_avail > num)
		n_avail = num;

	rb_read(pd->rb, (char *)dest, n_avail *
			     pd->vi->channels * sample_size);
	numread = n_avail;
	return numread;
}


void
vorbis_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {

	vorbis_pdata_t * pd = (vorbis_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	char flush_dest;

	if (fdec->is_stream && pd->session->type != HTTPC_SESSION_NORMAL)
		return;

	if (ov_pcm_seek(&(pd->vf), seek_to_pos) == 0) {
		fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
		/* empty vorbis decoder ringbuffer */
		while (rb_read_space(pd->rb))
			rb_read(pd->rb, &flush_dest, sizeof(char));
	} else {
		fprintf(stderr, "vorbis_decoder_seek: warning: ov_pcm_seek() failed\n");
	}
}



// vim: shiftwidth=8:tabstop=8:softtabstop=8 :

