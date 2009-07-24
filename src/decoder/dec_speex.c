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

#include "dec_speex.h"


extern size_t sample_size;


#ifdef HAVE_SPEEX
static int
read_ogg_packet(OGGZ * oggz, ogg_packet * op, long serialno, void * user_data) {

	decoder_t * dec = (decoder_t *)user_data;
	speex_pdata_t * pd = (speex_pdata_t *)dec->pdata;

	if (pd->exploring && (pd->packetno == 0)) {
		/* Speex header */

		int i;
                int ptr = 0;
                char speex_string[9];
                char speex_version[21];
		int enh = 1;

                SpeexHeader * header;


                for (i = 0; i < 8; i++) {
                        speex_string[i] = op->packet[ptr++];
                }
                speex_string[i] = '\0';

                for (i = 0; i < 20; i++) {
                        speex_version[i] = op->packet[ptr++];
                }
                speex_version[i] = '\0';


                if (strcmp(speex_string, "Speex   ") != 0) {
			printf("read_ogg_packet(): Not a Speex stream\n");
                        pd->error = 1;
			return 0;
                }

                speex_bits_init(&(pd->bits));

                header = speex_packet_to_header((char *)op->packet, op->bytes);
                if (!header) {
                        printf("Cannot read Speex header\n");
			pd->error = 1;
                        return 0;
                }

                pd->mode = speex_mode_list[header->mode];

                if (pd->mode->bitstream_version > header->mode_bitstream_version) {
                        fprintf(stderr, "Unknown bitstream version! The file was encoded with an older version of Speex.\n"
                                "You need to downgrade Speex in order to play it.\n");
			pd->error = 1;
                        return 0;
                }

                if (pd->mode->bitstream_version < header->mode_bitstream_version) {
                        fprintf(stderr, "Unknown bitstream version! The file was encoded with a newer version of Speex.\n"
                                "You need to upgrade Speex in order to play it.\n");
			pd->error = 1;
                        return 0;
                }

		pd->sample_rate = header->rate;
		pd->channels = header->nb_channels;
		pd->vbr = header->vbr;

                if (header->frames_per_packet != 0)
                        pd->nframes = header->frames_per_packet;

                pd->decoder = speex_decoder_init(pd->mode);
                speex_decoder_ctl(pd->decoder, SPEEX_GET_FRAME_SIZE, &(pd->frame_size));
                speex_decoder_ctl(pd->decoder, SPEEX_SET_ENH, &enh);

	} else if (pd->packetno >= 2) {

		int j;
		float output_frame[SPEEX_BUFSIZE];

		pd->granulepos = op->granulepos;

		if (!pd->exploring) {
			speex_bits_read_from(&(pd->bits), (char *)op->packet, op->bytes);
			
			for (j = 0; j < pd->nframes; j++) {
				
				int k;
				
				speex_decode(pd->decoder, &(pd->bits), output_frame);
				
				for (k = 0; k < pd->frame_size * pd->channels; k++) {
					output_frame[k] /= 32768.0f;
					if (output_frame[k] > 1.0f) {
						output_frame[k] = 1.0f;
					} else if (output_frame[k] < -1.0f) {
						output_frame[k] = -1.0f;
					}
				}
				
				rb_write(pd->rb, (char *)output_frame,
						      pd->channels * pd->frame_size * sample_size);
			}
		}
	}
	
	++pd->packetno;
	return 0;
}


/* return 1 if reached end of stream, 0 else */
int
decode_speex(decoder_t * dec) {

	speex_pdata_t * pd = (speex_pdata_t *)dec->pdata;


	if (oggz_read(pd->oggz, 1024) > 0) {
		return 0;
	} else {
		return 1;
	}
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


int
speex_dec_open(decoder_t * dec, char * filename) {

	speex_pdata_t * pd = (speex_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	int enh = 1;
	char ogg_sig[4];
	long length_in_bytes = 0;
	long length_in_samples = 0;


	if ((pd->speex_file = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "speex_decoder_open: fopen() failed\n");
		return DECODER_OPEN_FERROR;
	}

	if (fread(ogg_sig, 1, 4, pd->speex_file) != 4) {
		fprintf(stderr, "couldn't read OGG signature from %s\n", filename);
		return DECODER_OPEN_FERROR;
	}

	if ((ogg_sig[0] != 'O') ||
	    (ogg_sig[1] != 'g') ||
	    (ogg_sig[2] != 'g') ||
	    (ogg_sig[3] != 'S')) {
		/* not an OGG stream */
		fclose(pd->speex_file);
		return DECODER_OPEN_BADLIB;
	}

        if ((pd->oggz = oggz_open(filename, OGGZ_READ | OGGZ_AUTO)) == NULL) {
                printf("nonexistent or unaccessible file %s\n", filename);
                return DECODER_OPEN_FERROR;
        }

        oggz_set_read_callback(pd->oggz, -1, read_ogg_packet, dec);

	pd->packetno = 0;
	pd->exploring = 1;
	pd->error = 0;
	while (pd->packetno < 2) { /* process Speex header and comments */
		oggz_read(pd->oggz, 1024);
	}

	if (pd->error != 0) {
		printf("Error opening Speex\n");
		oggz_close(pd->oggz);
		return DECODER_OPEN_BADLIB;
	}

	/* parse ogg packets till eof to get the last granulepos */
	while (oggz_read(pd->oggz, 1024) > 0)
		;

	length_in_bytes = oggz_tell(pd->oggz);

	oggz_close(pd->oggz);
	speex_bits_destroy(&(pd->bits));
        speex_decoder_destroy(pd->decoder);
	
	if ((pd->channels != 1) && (pd->channels != 2)) {
		printf("Sorry, Ogg Speex with %d channels is unsupported\n", pd->channels);
		return DECODER_OPEN_FERROR;
	}
	
	pd->packetno = 0;
	pd->exploring = 0;
	pd->error = 0;
        pd->oggz = oggz_open(filename, OGGZ_READ | OGGZ_AUTO);
	oggz_set_read_callback(pd->oggz, -1, read_ogg_packet, dec);
	speex_bits_init(&(pd->bits));
	pd->decoder = speex_decoder_init(pd->mode);
	speex_decoder_ctl(pd->decoder, SPEEX_SET_ENH, &enh);

	pd->is_eos = 0;
	pd->rb = rb_create(pd->channels * sample_size * RB_SPEEX_SIZE);
	fdec->fileinfo.channels = pd->channels;
	fdec->fileinfo.sample_rate = pd->sample_rate;
	length_in_samples = pd->granulepos + pd->nframes - 1;
	fdec->fileinfo.total_samples = length_in_samples;
	fdec->fileinfo.bps = 8 * length_in_bytes / (length_in_samples / pd->sample_rate);

	fdec->file_lib = SPEEX_LIB;
	strcpy(dec->format_str, "Ogg Speex");

	return DECODER_OPEN_SUCCESS;
}


void
speex_dec_send_metadata(decoder_t * dec) {
}


void
speex_dec_close(decoder_t * dec) {

	speex_pdata_t * pd = (speex_pdata_t *)dec->pdata;

	oggz_close(pd->oggz);
	speex_bits_destroy(&(pd->bits));
        speex_decoder_destroy(pd->decoder);
	rb_free(pd->rb);
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
	char flush_dest;

	if (seek_to_pos == fdec->fileinfo.total_samples)
		--seek_to_pos;

	if (oggz_seek_units(pd->oggz, seek_to_pos / (float)pd->sample_rate * 1000.0f, SEEK_SET) != -1) {

		if (seek_to_pos == 0) {
			pd->packetno = 0;
		}

		fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
		/* empty speex decoder ringbuffer */
		while (rb_read_space(pd->rb))
			rb_read(pd->rb, &flush_dest, sizeof(char));
	} else {
		fprintf(stderr, "speex_dec_seek(): warning: oggz_seek_units() returned -1\n");
	}
}


#else
decoder_t *
speex_dec_init(file_decoder_t * fdec) {

        return NULL;
}
#endif /* HAVE_SPEEX */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

