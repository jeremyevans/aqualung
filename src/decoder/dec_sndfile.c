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

#include "dec_sndfile.h"


#ifdef HAVE_SNDFILE

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


	pd->sf_info.format = 0;
        if ((pd->sf = sf_open(filename, SFM_READ, &(pd->sf_info))) == NULL) {
		return DECODER_OPEN_BADLIB;
	}

#ifdef HAVE_NEW_SNDFILE
	/* XXX don't use the FLAC decoder in sndfile, seeking seems to be buggy */
	/* the native FLAC decoder will catch the file instead. */
	if ((pd->sf_info.format & SF_FORMAT_TYPEMASK) == SF_FORMAT_FLAC) {
		sf_close(pd->sf);
		return DECODER_OPEN_BADLIB;
	}
#endif

	if ((pd->sf_info.channels != 1) && (pd->sf_info.channels != 2)) {
		fprintf(stderr,
			"sndfile_decoder_open: sndfile with %d channels is unsupported\n",
			pd->sf_info.channels);
		return DECODER_OPEN_FERROR;
	}
	fdec->channels = pd->sf_info.channels;
	fdec->SR = pd->sf_info.samplerate;
	fdec->file_lib = SNDFILE_LIB;
	
	fdec->fileinfo.total_samples = pd->sf_info.frames;
	fdec->fileinfo.format_major = pd->sf_info.format & SF_FORMAT_TYPEMASK;
	fdec->fileinfo.format_minor = pd->sf_info.format & SF_FORMAT_SUBMASK;
	
	switch (fdec->fileinfo.format_minor) {
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
	fdec->fileinfo.bps *= fdec->SR * fdec->channels;
		
	return DECODER_OPEN_SUCCESS;
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

	for (i = 0; i < numread; i++) {
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


#else
decoder_t *
sndfile_decoder_init(file_decoder_t * fdec) {

	return NULL;
}
#endif /* HAVE_SNDFILE */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

