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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "dec_mpeg.h"


extern size_t sample_size;


#ifdef HAVE_MPEG

/* MPEG bitrate index tables */
int bri_V1_L1[15] = {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448};
int bri_V1_L2[15] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384};
int bri_V1_L3[15] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
int bri_V2_L1[15] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256};
int bri_V2_L23[15] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};


/* ret == 0 -> valid MPEG file, ret != 0 -> error */
int
mpeg_explore(decoder_t * dec) {

	mpeg_pdata_t * pd = (mpeg_pdata_t *)dec->pdata;

        int buf = 0;
        int pos = 0;
        int byte1 = 0;
        int byte2 = 0;
        int byte3 = 0;
        int ver = 0;
        int layer = 0;
        int bri = 0; /* bitrate index */
        int padding = 0;
        int chmode = 0;
        int emph = 0;

        do {
                do {
                        if (read(pd->exp_fd, &buf, 1) != 1) {
                                return 1;
                        }
                        ++pos;
                        if (pos > 8192) {
                                return 1;
                        }
                } while (buf != 0xff);

                if (read(pd->exp_fd, &buf, 1) != 1) {
                        return 1;
                }
                ++pos;
        } while ((buf & 0xe0) != 0xe0);

        byte1 = buf;

        if (read(pd->exp_fd, &byte2, 1) != 1) {
                return 1;
        }
        ++pos;

        if (read(pd->exp_fd, &byte3, 1) != 1) {
                return 1;
        }
        ++pos;

        ver = (byte1 >> 3) & 0x3;

        layer = (byte1 >> 1) & 0x3;
        switch (layer) {
        case 0: return 1;
                break;
        case 1: pd->mpeg_subformat |= MPEG_LAYER_III;
                break;
        case 2: pd->mpeg_subformat |= MPEG_LAYER_II;
                break;
        case 3: pd->mpeg_subformat |= MPEG_LAYER_I;
                break;
        }

        bri = (byte2 >> 4) & 0x0f;
        switch (bri) {
        case 0: pd->bitrate = 256000; /* XXX random value so it's not zero */
                printf("Warning: free-format MPEG detected. Be prepared for weird errors.\n");
                break;
        case 15:
                return 1;
                break;
        default:
                switch (layer) {
                case 3:
                        switch (ver) {
                        case 3:
                                pd->bitrate = bri_V1_L1[bri] * 1000;
                                break;
                        case 0:
                        case 2:
                                pd->bitrate = bri_V2_L1[bri] * 1000;
                                break;
                        }
                        break;
                case 2:
                        switch (ver) {
                        case 3:
                                pd->bitrate = bri_V1_L2[bri] * 1000;
                                break;
                        case 0:
                        case 2:
                                pd->bitrate = bri_V2_L23[bri] * 1000;
                                break;
                        }
                        break;
                case 1:
                        switch (ver) {
                        case 3:
                                pd->bitrate = bri_V1_L3[bri] * 1000;
                                break;
                        case 0:
                        case 2:
                                pd->bitrate = bri_V2_L23[bri] * 1000;
                                break;
                        }
                        break;
                }
                break;
        }


        switch ((byte2 >> 2) & 0x03) {
        case 0:
                switch (ver) {
                case 0: pd->SR = 11025;
                        break;
                case 2: pd->SR = 22050;
                        break;
                case 3: pd->SR = 44100;
                        break;
                }
                break;
        case 1:
                switch (ver) {
                case 0: pd->SR = 12000;
                        break;
                case 2: pd->SR = 24000;
                        break;
                case 3: pd->SR = 48000;
                        break;
                }
                break;
        case 2:
                switch (ver) {
                case 0: pd->SR = 8000;
                        break;
                case 2: pd->SR = 16000;
                        break;
                case 3: pd->SR = 32000;
                        break;
                }
                break;
        case 3:
                return 1;
                break;
        }

        padding = (byte2 >> 1) & 0x01;

        chmode = (byte3 >> 6) & 0x03;
        switch (chmode) {
        case 0: pd->mpeg_subformat |= MPEG_MODE_STEREO;
                pd->channels = 2;
                break;
        case 1: pd->mpeg_subformat |= MPEG_MODE_JOINT;
                pd->channels = 2;
                break;
        case 2: pd->mpeg_subformat |= MPEG_MODE_DUAL;
                pd->channels = 2;
                break;
        case 3: pd->mpeg_subformat |= MPEG_MODE_SINGLE;
                pd->channels = 1;
                break;
        }

        emph = byte3 & 0x03;
        switch (emph) {
        case 0: pd->mpeg_subformat |= MPEG_EMPH_NONE;
                break;
        case 1: pd->mpeg_subformat |= MPEG_EMPH_5015;
                break;
        case 2: pd->mpeg_subformat |= MPEG_EMPH_RES;
                break;
        case 3: pd->mpeg_subformat |= MPEG_EMPH_J_17;
                break;
        }

        return 0;
}


/* MPEG input callback */
static
enum mad_flow
mpeg_input(void * data, struct mad_stream * stream) {

        decoder_t * dec = (decoder_t *)data;
	mpeg_pdata_t * pd = (mpeg_pdata_t *)dec->pdata;

        if (fstat(pd->fd, &(pd->mpeg_stat)) == -1 || pd->mpeg_stat.st_size == 0)
                return MAD_FLOW_STOP;
	
        pd->fdm = mmap(0, pd->mpeg_stat.st_size, PROT_READ, MAP_SHARED, pd->fd, 0);
        if (pd->fdm == MAP_FAILED)
                return MAD_FLOW_STOP;

        mad_stream_buffer(stream, pd->fdm, pd->mpeg_stat.st_size);

        return MAD_FLOW_CONTINUE;
}


/* MPEG output callback */
static
enum mad_flow
mpeg_output(void * data, struct mad_header const * header, struct mad_pcm * pcm) {

        decoder_t * dec = (decoder_t *)data;
	mpeg_pdata_t * pd = (mpeg_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	int i, j;
	unsigned long scale = 322122547; /* (1 << 28) * 1.2 */
        int buf[2];
        float fbuf[2];

        for (i = 0; i < pcm->length; i++) {
                for (j = 0; j < pd->channels; j++) {
                        buf[j] = pd->error ? 0 : *(pcm->samples[j] + i);
                        fbuf[j] = (double)buf[j] * fdec->voladj_lin / scale;
                }

                if (jack_ringbuffer_write_space(pd->rb) >=
                    pd->channels * sample_size) {

                        jack_ringbuffer_write(pd->rb, (char *)fbuf,
                                              pd->channels * sample_size);
		}
        }
        pd->error = 0;

        return MAD_FLOW_CONTINUE;
}


/* MPEG header callback */
static
enum mad_flow
mpeg_header(void * data, struct mad_header const * header) {

        decoder_t * dec = (decoder_t *)data;
	mpeg_pdata_t * pd = (mpeg_pdata_t *)dec->pdata;

        pd->bitrate = header->bitrate;

        return MAD_FLOW_CONTINUE;
}


/* MPEG error callback */
static
enum mad_flow
mpeg_error(void * data, struct mad_stream * stream, struct mad_frame * frame) {

        decoder_t * dec = (decoder_t *)data;
	mpeg_pdata_t * pd = (mpeg_pdata_t *)dec->pdata;

        pd->error = 1;

        return MAD_FLOW_CONTINUE;
}


/* Main decode loop: return 1 if reached end of stream, 0 else */
int
decode_mpeg(decoder_t * dec) {

	mpeg_pdata_t * pd = (mpeg_pdata_t *)dec->pdata;

        if (mad_header_decode(&(pd->mpeg_frame.header), &(pd->mpeg_stream)) == -1) {
                if (pd->mpeg_stream.error == MAD_ERROR_BUFLEN)
                        return 1;

                if (!MAD_RECOVERABLE(pd->mpeg_stream.error))
                        fprintf(stderr, "libMAD: unrecoverable error in MPEG Audio stream\n");

                mpeg_error((void *)dec, &(pd->mpeg_stream), &(pd->mpeg_frame));
        }
        mpeg_header((void *)dec, &(pd->mpeg_frame.header));

        if (mad_frame_decode(&(pd->mpeg_frame), &(pd->mpeg_stream)) == -1) {
                if (pd->mpeg_stream.error == MAD_ERROR_BUFLEN)
                        return 1;

                if (!MAD_RECOVERABLE(pd->mpeg_stream.error))
                        fprintf(stderr, "libMAD: unrecoverable error in MPEG Audio stream\n");

                mpeg_error((void *)dec, &(pd->mpeg_stream), &(pd->mpeg_frame));
        }

        mad_synth_frame(&(pd->mpeg_synth), &(pd->mpeg_frame));
        mpeg_output((void *)dec, &(pd->mpeg_frame.header), &(pd->mpeg_synth.pcm));

        return 0;
}


decoder_t *
mpeg_decoder_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_mpeg.c: mpeg_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(mpeg_pdata_t))) == NULL) {
                fprintf(stderr, "dec_mpeg.c: mpeg_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->init = mpeg_decoder_init;
	dec->destroy = mpeg_decoder_destroy;
	dec->open = mpeg_decoder_open;
	dec->close = mpeg_decoder_close;
	dec->read = mpeg_decoder_read;
	dec->seek = mpeg_decoder_seek;

	return dec;
}


void
mpeg_decoder_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


int
mpeg_decoder_open(decoder_t * dec, char * filename) {

	mpeg_pdata_t * pd = (mpeg_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	struct stat exp_stat;


	if ((pd->exp_fd = open(filename, O_RDONLY)) == 0) {
		fprintf(stderr, "mpeg_decoder_open: open() failed for MPEG Audio file\n");
		return DECODER_OPEN_FERROR;
	}
	
	fstat(pd->exp_fd, &exp_stat);
	pd->filesize = exp_stat.st_size;
	pd->SR = pd->channels = pd->bitrate = pd->mpeg_subformat = 0;
	pd->error = 0;
	if (mpeg_explore(dec) != 0) {
		close(pd->exp_fd);
		return DECODER_OPEN_BADLIB;
	}
	close(pd->exp_fd);

	if ((!pd->SR) || (!pd->channels) || (!pd->bitrate)) {
		return DECODER_OPEN_BADLIB;
	}

	if ((pd->channels != 1) && (pd->channels != 2)) {
		fprintf(stderr,
			"mpeg_decoder_open: MPEG Audio file with %d channels "
			"is unsupported\n", pd->channels);
		return DECODER_OPEN_FERROR;
	}
	
	/* get a rough estimate of the total decoded length of the file */
	/* XXX this may as well be considered a hack. (what about VBR ???) */
	pd->total_samples_est =
		(double)pd->filesize / pd->bitrate * 8 * pd->SR;
	
	/* setup playback */
	pd->fd = open(filename, O_RDONLY);
	mad_stream_init(&(pd->mpeg_stream));
	mad_frame_init(&(pd->mpeg_frame));
	mad_synth_init(&(pd->mpeg_synth));
	
	if (mpeg_input((void *)dec, &(pd->mpeg_stream)) == MAD_FLOW_STOP) {
		mad_synth_finish(&(pd->mpeg_synth));
		mad_frame_finish(&(pd->mpeg_frame));
		mad_stream_finish(&(pd->mpeg_stream));
		return DECODER_OPEN_FERROR;
	}
	
	pd->error = 0;
	pd->is_eos = 0;
	pd->rb = jack_ringbuffer_create(pd->channels * sample_size * RB_MAD_SIZE);
	fdec->channels = pd->channels;
	fdec->SR = pd->SR;
	fdec->file_lib = MAD_LIB;
	
	fdec->fileinfo.total_samples = pd->total_samples_est;
	fdec->fileinfo.format_major = FORMAT_MAD;
	fdec->fileinfo.format_minor = pd->mpeg_subformat;
	fdec->fileinfo.bps = pd->bitrate;

	return DECODER_OPEN_SUCCESS;
}


void
mpeg_decoder_close(decoder_t * dec) {

	mpeg_pdata_t * pd = (mpeg_pdata_t *)dec->pdata;

	mad_synth_finish(&(pd->mpeg_synth));
	mad_frame_finish(&(pd->mpeg_frame));
	mad_stream_finish(&(pd->mpeg_stream));
	if (munmap(pd->fdm, pd->mpeg_stat.st_size) == -1)
		fprintf(stderr, "Error while munmap()'ing MPEG Audio file mapping\n");
	close(pd->fd);
	jack_ringbuffer_free(pd->rb);
}


unsigned int
mpeg_decoder_read(decoder_t * dec, float * dest, int num) {

	mpeg_pdata_t * pd = (mpeg_pdata_t *)dec->pdata;

	unsigned int numread = 0;
	unsigned int n_avail = 0;


	while ((jack_ringbuffer_read_space(pd->rb) < num * pd->channels *
		sample_size) && (!pd->is_eos)) {
		
		pd->is_eos = decode_mpeg(dec);
	}
	
	n_avail = jack_ringbuffer_read_space(pd->rb) / (pd->channels * sample_size);

	if (n_avail > num)
		n_avail = num;

	jack_ringbuffer_read(pd->rb, (char *)dest, n_avail * pd->channels * sample_size);

	numread = n_avail;
	return numread;
}


void
mpeg_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {
	
	mpeg_pdata_t * pd = (mpeg_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	char flush_dest;

	pd->mpeg_stream.next_frame = pd->mpeg_stream.buffer;
	mad_stream_sync(&(pd->mpeg_stream));
	mad_stream_skip(&(pd->mpeg_stream), pd->filesize *
			(double)seek_to_pos / pd->total_samples_est);
	mad_stream_sync(&(pd->mpeg_stream));

	pd->is_eos = decode_mpeg(dec);
	/* report the real position of the decoder */
	fdec->samples_left = fdec->fileinfo.total_samples -
		(pd->mpeg_stream.next_frame - pd->mpeg_stream.buffer)
		/ pd->bitrate * 8 * pd->SR;
	/* empty mpeg decoder ringbuffer */
	while (jack_ringbuffer_read_space(pd->rb))
		jack_ringbuffer_read(pd->rb, &flush_dest, sizeof(char));
}


#else
decoder_t *
mpeg_decoder_init(file_decoder_t * fdec) {

        return NULL;
}
#endif /* HAVE_MPEG */
