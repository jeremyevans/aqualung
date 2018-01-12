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
#include "dec_sndfile.h"


/* list of accepted file extensions */
char * valid_extensions_sndfile[] = {
        "wav", "aiff", "au", "w64", "voc", "xi", "htk", "svx", NULL
};

decoder_t *
sndfile_decoder_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_sndfile.c: sndfile_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(sndfile_pdata_t))) == NULL) {
                fprintf(stderr, "dec_sndfile.c: sndfile_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->init = sndfile_decoder_init;
	dec->destroy = sndfile_decoder_destroy;
	dec->open = sndfile_decoder_open;
	dec->send_metadata = sndfile_decoder_send_metadata;
	dec->close = sndfile_decoder_close;
	dec->read = sndfile_decoder_read;
	dec->seek = sndfile_decoder_seek;

	return dec;
}


void
sndfile_decoder_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


int
sndfile_decoder_open(decoder_t * dec, char * filename) {

	sndfile_pdata_t * pd = (sndfile_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	const char * format_type = "?";
	const char * format_sub1 = NULL;
	const char * format_sub2 = NULL;


	pd->sf_info.format = 0;
        if ((pd->sf = sf_open(filename, SFM_READ, &(pd->sf_info))) == NULL) {
		return DECODER_OPEN_BADLIB;
	}

	/* XXX don't use the FLAC decoder in sndfile, seeking seems to be buggy */
	/* the native FLAC decoder will catch the file instead. */
	if ((pd->sf_info.format & SF_FORMAT_TYPEMASK) == SF_FORMAT_FLAC) {
		sf_close(pd->sf);
		return DECODER_OPEN_BADLIB;
	}

#ifdef HAVE_SNDFILE_1_0_18
	/* XXX don't use the Ogg decoder in sndfile. */
	if ((pd->sf_info.format & SF_FORMAT_SUBMASK) == SF_FORMAT_VORBIS) {
		sf_close(pd->sf);
		return DECODER_OPEN_BADLIB;
	}
#endif /* HAVE_SNDFILE_1_0_18 */

	if ((pd->sf_info.channels != 1) && (pd->sf_info.channels != 2)) {
		fprintf(stderr,
			"sndfile_decoder_open: sndfile with %d channels is unsupported\n",
			pd->sf_info.channels);
		return DECODER_OPEN_FERROR;
	}
	fdec->fileinfo.channels = pd->sf_info.channels;
	fdec->fileinfo.sample_rate = pd->sf_info.samplerate;
	fdec->fileinfo.total_samples = pd->sf_info.frames;

	fdec->file_lib = SNDFILE_LIB;

	switch (pd->sf_info.format & SF_FORMAT_SUBMASK) {
	case SF_FORMAT_PCM_S8:
	case SF_FORMAT_PCM_U8:
		fdec->fileinfo.bps = 8;
		break;
	case SF_FORMAT_PCM_16:
		fdec->fileinfo.bps = 16;
		break;
	case SF_FORMAT_PCM_24:
		fdec->fileinfo.bps = 24;
		break;
	case SF_FORMAT_PCM_32:
	case SF_FORMAT_FLOAT:
		fdec->fileinfo.bps = 32;
		break;
	case SF_FORMAT_DOUBLE:
		fdec->fileinfo.bps = 64;
		break;
	default:
		/* XXX libsndfile knows some more formats apart from the ones above,
		   but i don't know their bits/sample... perhaps i'm stupid. --tszilagyi */
		fdec->fileinfo.bps = 0;
		break;
	}
	fdec->fileinfo.bps *= fdec->fileinfo.sample_rate * fdec->fileinfo.channels;


	switch (pd->sf_info.format & SF_FORMAT_TYPEMASK) {
	case SF_FORMAT_WAV:
		format_type = "Microsoft WAV";
		break;
	case SF_FORMAT_AIFF:
		format_type = "Apple/SGI AIFF";
		break;
	case SF_FORMAT_AU:
		format_type = "Sun/NeXT AU";
		break;
	case SF_FORMAT_PAF:
		format_type = "Ensoniq PARIS";
		break;
	case SF_FORMAT_SVX:
		format_type = "Amiga IFF / SVX8 / SV16";
		break;
	case SF_FORMAT_NIST:
		format_type = "Sphere NIST";
		break;
	case SF_FORMAT_VOC:
		format_type = "VOC";
		break;
	case SF_FORMAT_IRCAM:
		format_type = "Berkeley/IRCAM/CARL";
		break;
	case SF_FORMAT_W64:
		format_type = "Sonic Foundry 64 bit RIFF/WAV";
		break;
	case SF_FORMAT_MAT4:
	case SF_FORMAT_MAT5:
		format_type = "Matlab (tm) / GNU Octave";
		break;
	case SF_FORMAT_PVF:
		format_type = "Portable Voice Format";
		break;
	case SF_FORMAT_XI:
		format_type = "Fasttracker 2 Extended Instrument";
		break;
	case SF_FORMAT_HTK:
		format_type = "HMM Tool Kit";
		break;
	case SF_FORMAT_SDS:
		format_type = "Midi Sample Dump Standard";
		break;
	case SF_FORMAT_AVR:
		format_type = "Audio Visual Research";
		break;
	case SF_FORMAT_WAVEX:
		format_type = "MS WAVE with WAVEFORMATEX";
		break;
	case SF_FORMAT_SD2:
		format_type = "Sound Designer 2";
		break;
	case SF_FORMAT_FLAC:
		format_type = "FLAC";
		break;
	case SF_FORMAT_CAF:
		format_type = "Core Audio File";
		break;
#ifdef HAVE_SNDFILE_1_0_18
	case SF_FORMAT_MPC2K:
		format_type = "Akai MPC 2000 Sample";
		break;
	case SF_FORMAT_RF64:
		format_type = "RF64 WAV";
		break;
#endif /* HAVE_SNDFILE_1_0_18 */
	}

	switch (pd->sf_info.format & SF_FORMAT_SUBMASK) {
	case SF_FORMAT_PCM_S8:
		format_sub1 = "8 ";
		format_sub2 = _("bit signed");
		break;
	case SF_FORMAT_PCM_U8:
		format_sub1 = "16 ";
		format_sub2 = _("bit unsigned");
		break;
	case SF_FORMAT_PCM_16:
		format_sub1 = "16 ";
		format_sub2 = _("bit signed");
		break;
	case SF_FORMAT_PCM_24:
		format_sub1 = "24 ";
		format_sub2 = _("bit signed");
		break;
	case SF_FORMAT_PCM_32:
		format_sub1 = "32 ";
		format_sub2 = _("bit signed");
		break;
	case SF_FORMAT_FLOAT:
		format_sub1 = "32 ";
		format_sub2 = _("bit float");
		break;
	case SF_FORMAT_DOUBLE:
		format_sub1 = "64 ";
		format_sub2 = _("bit double");
		break;
	case SF_FORMAT_ULAW:
		format_sub1 = "u-Law ";
		format_sub2 = _("encoding");
		break;
	case SF_FORMAT_ALAW:
		format_sub1 = "A-Law ";
		format_sub2 = _("encoding");
		break;
	case SF_FORMAT_IMA_ADPCM:
		format_sub1 = "IMA ADPCM ";
		format_sub2 = _("encoding");
		break;
	case SF_FORMAT_MS_ADPCM:
		format_sub1 = "Microsoft ADPCM ";
		format_sub2 = _("encoding");
		break;
	case SF_FORMAT_GSM610:
		format_sub1 = "GSM 6.10 ";
		format_sub2 = _("encoding");
		break;
	case SF_FORMAT_VOX_ADPCM:
		format_sub1 = "Oki Dialogic ADPCM ";
		format_sub2 = _("encoding");
		break;
	case SF_FORMAT_G721_32:
		format_sub1 = "32kbps G721 ADPCM ";
		format_sub2 = _("encoding");
		break;
	case SF_FORMAT_G723_24:
		format_sub1 = "24kbps G723 ADPCM ";
		format_sub2 = _("encoding");
		break;
	case SF_FORMAT_G723_40:
		format_sub1 = "40kbps G723 ADPCM ";
		format_sub2 = _("encoding");
		break;
	case SF_FORMAT_DWVW_12:
		format_sub1 = "12 bit DWVW ";
		format_sub2 = _("encoding");
		break;
	case SF_FORMAT_DWVW_16:
		format_sub1 = "16 bit DWVW ";
		format_sub2 = _("encoding");
		break;
	case SF_FORMAT_DWVW_24:
		format_sub1 = "24 bit DWVW ";
		format_sub2 = _("encoding");
		break;
	case SF_FORMAT_DWVW_N:
		format_sub1 = "N bit DWVW ";
		format_sub2 = _("encoding");
		break;
	}

	sprintf(dec->format_str, "%s%s%s%s%s", format_type, (format_sub1 != NULL) ? " (" : "", format_sub1, format_sub2, (format_sub1 != NULL) ? ")" : "");

	return DECODER_OPEN_SUCCESS;
}


void
sndfile_decoder_send_metadata(decoder_t * dec) {
}


void
sndfile_decoder_close(decoder_t * dec) {

	sndfile_pdata_t * pd = (sndfile_pdata_t *)dec->pdata;

	sf_close(pd->sf);
}


unsigned int
sndfile_decoder_read(decoder_t * dec, float * dest, int num) {

	sndfile_pdata_t * pd = (sndfile_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	int i;
	unsigned int numread = 0;

	numread = sf_readf_float(pd->sf, dest, num);

	for (i = 0; i < pd->sf_info.channels * numread; i++) {
		dest[i] *= fdec->voladj_lin;
	}

	return numread;
}


void
sndfile_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {

	sndfile_pdata_t * pd = (sndfile_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	if ((pd->nframes = sf_seek(pd->sf, seek_to_pos, SEEK_SET)) != -1) {
		fdec->samples_left = fdec->fileinfo.total_samples - pd->nframes;
	} else {
		fprintf(stderr, "sndfile_decoder_seek: warning: sf_seek() failed\n");
	}
}



// vim: shiftwidth=8:tabstop=8:softtabstop=8 :

