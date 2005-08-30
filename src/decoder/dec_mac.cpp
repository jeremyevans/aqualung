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


#ifdef HAVE_MAC
/* expand this to nothing so there's no error when including MACLib.h */
/* -- talkin' about cross-platform libraries? */
#define DLLEXPORT

/* more cross-platform compatibility... */
#include <mac/NoWindows.h>

#include <mac/All.h>
#include <mac/MACLib.h>
#include <mac/CharacterHelper.h>
#endif /* HAVE_MAC */


#include "dec_mac.h"


extern size_t sample_size;


#define SAMPLES_PER_READ 2048


#ifdef HAVE_MAC
/* return 1 if reached end of stream, 0 else */
int
decode_mac(decoder_t * dec) {

	mac_pdata_t * pd = (mac_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	IAPEDecompress * pdecompress = (IAPEDecompress *)pd->decompress;

        int blocks_to_read = SAMPLES_PER_READ;
        int bytes = blocks_to_read * pd->block_align;
	int act_read, act_bytes;
	unsigned long scale = 1 << (pd->bits_per_sample - 1);
	float fbuf[2 * SAMPLES_PER_READ];
	int n = 0;

	act_read = blocks_to_read;
	act_bytes = bytes;

	switch (pd->bits_per_sample) {
	case 8:
		char data8[2 * SAMPLES_PER_READ];
		pdecompress->GetData(data8, blocks_to_read, &act_read);
		if (!act_read) {
			return 1;
		}
		for (int i = 0; i < act_read; i++) {
			for (int j = 0; j < pd->channels; j++) {
				fbuf[n] = (float)(data8[n] * fdec->voladj_lin / scale);
				++n;
			}
		}
		break;

	case 16:
		short data16[2 * SAMPLES_PER_READ];

		pdecompress->GetData((char *)data16, blocks_to_read, &act_read);
		if (!act_read) {
			return 1;
		}
		for (int i = 0; i < act_read; i++) {
			for (int j = 0; j < pd->channels; j++) {
				fbuf[n] = (float)(data16[n] * fdec->voladj_lin / scale);
				++n;
			}
		}
		break;

	case 32:
		int data32[2 * SAMPLES_PER_READ];
		pdecompress->GetData((char *)data32, blocks_to_read, &act_read);
		if (!act_read) {
			return 1;
		}
		for (int i = 0; i < act_read; i++) {
			for (int j = 0; j < pd->channels; j++) {
				fbuf[n] = (float)(data32[n] * fdec->voladj_lin / scale);
				++n;
			}
		}
		break;

	default:
		printf("programmer error, please report: invalid bits_per_sample = %d "
		       "in decode_mac()\n", pd->bits_per_sample);
		break;
	}

	jack_ringbuffer_write(pd->rb, (char *)fbuf,
			      act_read * pd->channels * sample_size);

	return 0;
}


decoder_t *
mac_decoder_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = (decoder_t *)calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_mac.c: mac_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(mac_pdata_t))) == NULL) {
                fprintf(stderr, "dec_mac.c: mac_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->init = mac_decoder_init;
	dec->destroy = mac_decoder_destroy;
	dec->open = mac_decoder_open;
	dec->close = mac_decoder_close;
	dec->read = mac_decoder_read;
	dec->seek = mac_decoder_seek;

	return dec;
}


void
mac_decoder_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


int
mac_decoder_open(decoder_t * dec, char * filename) {

	mac_pdata_t * pd = (mac_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	IAPEDecompress * pdecompress = (IAPEDecompress *)pd->decompress;


	int ret = 0;
        wchar_t * pUTF16 = GetUTF16FromANSI(filename);
        pdecompress = CreateIAPEDecompress(pUTF16, &ret);
        free(pUTF16);

        if (!pdecompress || ret != ERROR_SUCCESS) {
                return DECODER_OPEN_BADLIB;
        }

	pd->decompress = (void *)pdecompress;
        pd->sample_rate = pdecompress->GetInfo(APE_INFO_SAMPLE_RATE);
        pd->bits_per_sample = pdecompress->GetInfo(APE_INFO_BITS_PER_SAMPLE);
        pd->bitrate = pdecompress->GetInfo(APE_DECOMPRESS_AVERAGE_BITRATE);
        pd->channels = pdecompress->GetInfo(APE_INFO_CHANNELS);
        pd->length_in_ms = pdecompress->GetInfo(APE_DECOMPRESS_LENGTH_MS);
        pd->block_align = pdecompress->GetInfo(APE_INFO_BLOCK_ALIGN);
	pd->compression_level = pdecompress->GetInfo(APE_INFO_COMPRESSION_LEVEL);

	if ((pd->channels != 1) && (pd->channels != 2)) {
		printf("Sorry, MAC file with %d channels is not supported.\n", pd->channels);
		return DECODER_OPEN_BADLIB;
	}

	if ((pd->bits_per_sample != 8) && (pd->bits_per_sample != 16) && (pd->bits_per_sample != 32)) {
		printf("Sorry, MAC file with %d bits per sample is not supported.\n", pd->bits_per_sample);
		return DECODER_OPEN_BADLIB;
	}

	pd->is_eos = 0;
	pd->rb = jack_ringbuffer_create(pd->channels * sample_size * RB_MAC_SIZE);
	fdec->channels = pd->channels;
	fdec->SR = pd->sample_rate;
	fdec->file_lib = MAC_LIB;

	switch (pd->compression_level) {
	case COMPRESSION_LEVEL_FAST:
		fdec->fileinfo.format_minor = MAC_COMP_FAST;
		break;
	case COMPRESSION_LEVEL_NORMAL:
		fdec->fileinfo.format_minor = MAC_COMP_NORMAL;
		break;
	case COMPRESSION_LEVEL_HIGH:
		fdec->fileinfo.format_minor = MAC_COMP_HIGH;
		break;
	case COMPRESSION_LEVEL_EXTRA_HIGH:
		fdec->fileinfo.format_minor = MAC_COMP_EXTRA;
		break;
	case COMPRESSION_LEVEL_INSANE:
		fdec->fileinfo.format_minor = MAC_COMP_INSANE;
		break;
	default:
		printf("Unknown MAC compression level %d\n", pd->compression_level);
		fdec->fileinfo.format_minor = 0;
		break;
	}
	
	fdec->fileinfo.total_samples = (unsigned long long)(pd->sample_rate / 1000.0f * pd->length_in_ms);
	fdec->fileinfo.format_major = FORMAT_MAC;
	fdec->fileinfo.bps = pd->bitrate * 1000;

	return DECODER_OPEN_SUCCESS;
}


void
mac_decoder_close(decoder_t * dec) {

	mac_pdata_t * pd = (mac_pdata_t *)dec->pdata;
	IAPEDecompress * pdecompress = (IAPEDecompress *)pd->decompress;

	delete(pdecompress);
	jack_ringbuffer_free(pd->rb);
}


unsigned int
mac_decoder_read(decoder_t * dec, float * dest, int num) {

	mac_pdata_t * pd = (mac_pdata_t *)dec->pdata;

	unsigned int numread = 0;
	unsigned int n_avail = 0;


	while ((jack_ringbuffer_read_space(pd->rb) <
		num * pd->channels * sample_size) && (!pd->is_eos)) {

		pd->is_eos = decode_mac(dec);
	}

	n_avail = jack_ringbuffer_read_space(pd->rb) /
		(pd->channels * sample_size);

	if (n_avail > num)
		n_avail = num;

	jack_ringbuffer_read(pd->rb, (char *)dest, n_avail * pd->channels * sample_size);
	numread = n_avail;
	return numread;
}


void
mac_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {
	
	mac_pdata_t * pd = (mac_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	IAPEDecompress * pdecompress = (IAPEDecompress *)pd->decompress;
	char flush_dest;

	pdecompress->Seek(seek_to_pos);
	fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;

	/* empty mac decoder ringbuffer */
	while (jack_ringbuffer_read_space(pd->rb))
		jack_ringbuffer_read(pd->rb, &flush_dest, sizeof(char));
}


#else
decoder_t *
mac_decoder_init(file_decoder_t * fdec) {

        return NULL;
}
#endif /* HAVE_MAC */
