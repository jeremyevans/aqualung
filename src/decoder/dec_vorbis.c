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

#include "dec_vorbis.h"


extern size_t sample_size;


#ifdef HAVE_OGG_VORBIS
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
		printf("dec_vorbis.c/decode_vorbis(): ov_read() returned OV_HOLE\n");
		printf("This indicates an interruption in the Vorbis data (one of:\n");
		printf("garbage between Ogg pages, loss of sync, or corrupt page).\n");
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
	fdec->fileinfo.total_samples = ov_pcm_total(&(pd->vf), -1);
	fdec->fileinfo.bps = ov_bitrate(&(pd->vf), -1);
	
	fdec->file_lib = VORBIS_LIB;
	strcpy(dec->format_str, "Ogg Vorbis");
	
	return DECODER_OPEN_SUCCESS;
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

	if (ov_pcm_seek(&(pd->vf), seek_to_pos) == 0) {
		fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
		/* empty vorbis decoder ringbuffer */
		while (rb_read_space(pd->rb))
			rb_read(pd->rb, &flush_dest, sizeof(char));
	} else {
		fprintf(stderr, "vorbis_decoder_seek: warning: ov_pcm_seek() failed\n");
	}
}


#else
decoder_t *
vorbis_decoder_init(file_decoder_t * fdec) {

        return NULL;
}
#endif /* HAVE_OGG_VORBIS */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

