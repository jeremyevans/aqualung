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
#include <glib.h>


/* expand this to nothing so there's no error when including MACLib.h */
/* -- talkin' about cross-platform libraries? */
#define DLLEXPORT

#include "../undef_ac_pkg.h"
#include <MAC/All.h>
#include <MAC/MACLib.h>
#include <MAC/CharacterHelper.h>
#include "../undef_ac_pkg.h"
#include <config.h>	/* re-establish undefined autoconf macros */

#include "../i18n.h"
#include "../metadata.h"
#include "../metadata_ape.h"
#include "../rb.h"
#include "file_decoder.h"
#include "dec_mac.h"


extern size_t sample_size;


#define BLOCKS_PER_READ 2048


#define SWAP_16(x) ((((x)&0x00ff)<<8) | (((x)&0xff00)>>8))
#define SWAP_32(x) ((((x)&0xff)<<24) | (((x)&0xff00)<<8) | (((x)&0xff0000)>>8) | (((x)&0xff000000)>>24))

/* return 1 if reached end of stream, 0 else */
int
decode_mac(decoder_t * dec) {

	mac_pdata_t * pd = (mac_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	APE::IAPEDecompress * pdecompress = (APE::IAPEDecompress *)pd->decompress;

	APE::int64 act_read = 0;
	unsigned long scale = 1 << (pd->bits_per_sample - 1);
	float fbuf[2 * BLOCKS_PER_READ];
	int n = 0;

	switch (pd->bits_per_sample) {
	case 8:
		unsigned char data8[2 * BLOCKS_PER_READ];
		pdecompress->GetData(data8, BLOCKS_PER_READ, &act_read);
		if (!act_read) {
			return 1;
		}
		for (int i = 0; i < act_read; i++) {
			for (unsigned int j = 0; j < pd->channels; j++) {
				fbuf[n] = (float)(data8[n] * fdec->voladj_lin / scale);
				++n;
			}
		}
		break;

	case 16:
		short data16[2 * BLOCKS_PER_READ];
		pdecompress->GetData((unsigned char *)data16, BLOCKS_PER_READ, &act_read);
		if (!act_read) {
			return 1;
		}
		for (int i = 0; i < act_read; i++) {
			for (unsigned int j = 0; j < pd->channels; j++) {
				if (pd->swap_bytes)
					data16[n] = SWAP_16(data16[n]);
				fbuf[n] = (float)(data16[n] * fdec->voladj_lin / scale);
				++n;
			}
		}
		break;

	case 32:
		int data32[2 * BLOCKS_PER_READ];
		pdecompress->GetData((unsigned char *)data32, BLOCKS_PER_READ, &act_read);
		if (!act_read) {
			return 1;
		}
		for (int i = 0; i < act_read; i++) {
			for (unsigned int j = 0; j < pd->channels; j++) {
				if (pd->swap_bytes)
					data32[n] = SWAP_32(data32[n]);
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

	rb_write(pd->rb, (char *)fbuf,
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
	dec->send_metadata = mac_decoder_send_metadata;
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
	metadata_t * meta;
	APE::IAPEDecompress * pdecompress = (APE::IAPEDecompress *)pd->decompress;
	const char * comp_level = NULL;


	int ret = 0;
#ifdef __OpenBSD__
        wchar_t * pUTF16 = GetUTF16FromANSI(filename);
        pdecompress = CreateIAPEDecompress(pUTF16, &ret);
        free(pUTF16);
#else
        gunichar2 * pUTF16 = g_utf8_to_utf16(filename, -1, NULL, NULL, NULL);
        pdecompress = CreateIAPEDecompress((wchar_t *)pUTF16, &ret, FALSE, FALSE, FALSE);
        g_free(pUTF16);
#endif

        if (!pdecompress || ret != ERROR_SUCCESS) {
                return DECODER_OPEN_BADLIB;
        }

	pd->decompress = (void *)pdecompress;
        pd->sample_rate = pdecompress->GetInfo(APE::IAPEDecompress::APE_INFO_SAMPLE_RATE);
        pd->bits_per_sample = pdecompress->GetInfo(APE::IAPEDecompress::APE_INFO_BITS_PER_SAMPLE);
        pd->bitrate = pdecompress->GetInfo(APE::IAPEDecompress::APE_DECOMPRESS_AVERAGE_BITRATE);
        pd->channels = pdecompress->GetInfo(APE::IAPEDecompress::APE_INFO_CHANNELS);
        pd->length_in_ms = pdecompress->GetInfo(APE::IAPEDecompress::APE_DECOMPRESS_LENGTH_MS);
        pd->block_align = pdecompress->GetInfo(APE::IAPEDecompress::APE_INFO_BLOCK_ALIGN);
	pd->compression_level = pdecompress->GetInfo(APE::IAPEDecompress::APE_INFO_COMPRESSION_LEVEL);

	if ((pd->channels != 1) && (pd->channels != 2)) {
		printf("Sorry, MAC file with %d channels is not supported.\n", pd->channels);
		return DECODER_OPEN_BADLIB;
	}

	if ((pd->bits_per_sample != 8) && (pd->bits_per_sample != 16) && (pd->bits_per_sample != 32)) {
		printf("Sorry, MAC file with %d bits per sample is not supported.\n", pd->bits_per_sample);
		return DECODER_OPEN_BADLIB;
	}

	pd->swap_bytes = bigendianp();

	pd->is_eos = 0;
	pd->rb = rb_create(pd->channels * sample_size * RB_MAC_SIZE);
	fdec->fileinfo.channels = pd->channels;
	fdec->fileinfo.sample_rate = pd->sample_rate;
	fdec->fileinfo.total_samples = (unsigned long long)(pd->sample_rate / 1000.0f * pd->length_in_ms);
	fdec->fileinfo.bps = pd->bitrate * 1000;

	fdec->file_lib = MAC_LIB;

	switch (pd->compression_level) {
	case APE_COMPRESSION_LEVEL_FAST:
		comp_level = _("Compression: Fast");
		break;
	case APE_COMPRESSION_LEVEL_NORMAL:
		comp_level = _("Compression: Normal");
		break;
	case APE_COMPRESSION_LEVEL_HIGH:
		comp_level = _("Compression: High");
		break;
	case APE_COMPRESSION_LEVEL_EXTRA_HIGH:
		comp_level = _("Compression: Extra High");
		break;
	case APE_COMPRESSION_LEVEL_INSANE:
		comp_level = _("Compression: Insane");
		break;
	default:
		printf("Unknown MAC compression level %d\n", pd->compression_level);
		break;
	}

	arr_snprintf(dec->format_str, "Monkey's Audio%s%s%s", (comp_level != NULL) ? " (" : "", comp_level, (comp_level != NULL) ? ")" : "");

	meta = metadata_new();
	meta_ape_send_metadata(meta, fdec);
	return DECODER_OPEN_SUCCESS;
}


void
mac_decoder_send_metadata(decoder_t * dec) {
}


void
mac_decoder_close(decoder_t * dec) {

	mac_pdata_t * pd = (mac_pdata_t *)dec->pdata;
	APE::IAPEDecompress * pdecompress = (APE::IAPEDecompress *)pd->decompress;

	delete(pdecompress);
	rb_free(pd->rb);
}


unsigned int
mac_decoder_read(decoder_t * dec, float * dest, int num) {

	mac_pdata_t * pd = (mac_pdata_t *)dec->pdata;

	unsigned int numread = 0;
	int n_avail = 0;


	while ((rb_read_space(pd->rb) <
		num * pd->channels * sample_size) && (!pd->is_eos)) {

		pd->is_eos = decode_mac(dec);
	}

	n_avail = rb_read_space(pd->rb) /
		(pd->channels * sample_size);

	if (n_avail > num)
		n_avail = num;

	rb_read(pd->rb, (char *)dest, n_avail * pd->channels * sample_size);
	numread = n_avail;
	return numread;
}


void
mac_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {
	
	mac_pdata_t * pd = (mac_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	APE::IAPEDecompress * pdecompress = (APE::IAPEDecompress *)pd->decompress;
	char flush_dest;

	pdecompress->Seek(seek_to_pos);
	fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;

	/* empty mac decoder ringbuffer */
	while (rb_read_space(pd->rb))
		rb_read(pd->rb, &flush_dest, sizeof(char));
}



// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

