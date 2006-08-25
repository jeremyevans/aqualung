/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi

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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <gtk/gtk.h>

#ifdef _WIN32
#include <glib.h>
#else
#include <pthread.h>
#endif /* _WIN32 */


#include "file_decoder.h"
#include "dec_null.h"
#include "dec_sndfile.h"
#include "dec_flac.h"
#include "dec_vorbis.h"
#include "dec_speex.h"
#include "dec_mpc.h"
#include "dec_mpeg.h"
#include "dec_mod.h"
#include "dec_mac.h"


extern size_t sample_size;


typedef decoder_t * decoder_init_t(file_decoder_t * fdec);

/* this controls the order in which decoders are probed for a file */
decoder_init_t * decoder_init_v[N_DECODERS] = {
	null_decoder_init,
	sndfile_decoder_init,
	flac_decoder_init,
	vorbis_decoder_init,
	speex_dec_init,
	mpc_decoder_init,
	mac_decoder_init,
	mpeg_decoder_init,
	mod_decoder_init
};


file_decoder_t *
file_decoder_new(void) {

	file_decoder_t * fdec = NULL;
	
	if ((fdec = calloc(1, sizeof(file_decoder_t))) == NULL) {
		fprintf(stderr, "file_decoder.c: file_decoder_new() failed: calloc error\n");
		return NULL;
	}
	
	fdec->file_open = 0;

	fdec->voladj_db = 0.0f;
	fdec->voladj_lin = 1.0f;

	fdec->pdec = NULL;

	return fdec;
}


void
file_decoder_delete(file_decoder_t * fdec) {
	
	if (fdec->file_open) {
		file_decoder_close(fdec);
	}

	free(fdec);
}


/* return: 0 is OK, >0 is error */
int
file_decoder_open(file_decoder_t * fdec, char * filename) {

	int i, ret;
	decoder_t * dec;

	if (filename == NULL) {
		fprintf(stderr, "Warning: filename == NULL passed to file_decoder_open()\n");
		fprintf(stderr, "This is likely to be a programmer error, please report.\n");
		return 1;
	}

	for (i = 0; i < N_DECODERS; i++) {
		dec = decoder_init_v[i](fdec);
		if (!dec) {
			continue;
		}
		ret = dec->open(dec, filename);
		if (ret == DECODER_OPEN_FERROR) {
			goto no_open;
		} else if (ret == DECODER_OPEN_BADLIB) {
			continue;
		} else if (ret != DECODER_OPEN_SUCCESS) {
			printf("programmer error, please report: "
			       "illegal retvalue %d from dec->open() at %d\n", ret, i);
			return 1;
		}

		fdec->pdec = (void *)dec;
		break;
	}

	if (i == N_DECODERS) {
	        goto no_open;
	}

	if (fdec->channels == 1) {
		fdec->fileinfo.is_mono = 1;
		goto ok_open;

	} else if (fdec->channels == 2) {
		fdec->fileinfo.is_mono = 0;
		goto ok_open;

	} else {
		fprintf(stderr, "file_decoder_open: programmer error: "
			"soundfile with %d\n channels is unsupported.\n", fdec->channels);
		goto no_open;
	}

 ok_open:
	fdec->fileinfo.sample_rate = fdec->SR;
	fdec->file_open = 1;
	fdec->samples_left = fdec->fileinfo.total_samples;
	return 0;

 no_open:
	fprintf(stderr, "file_decoder_open: unable to open %s\n", filename);
	return 1;
}


void
file_decoder_set_rva(file_decoder_t * fdec, float voladj) {

	fdec->voladj_db = voladj;
	fdec->voladj_lin = db2lin(voladj);
}


void
file_decoder_close(file_decoder_t * fdec) {

	decoder_t * dec;

	if (!fdec->file_open) {
		return;
	}

	dec = (decoder_t *)(fdec->pdec);
	dec->close(dec);
	dec->destroy(dec);
	fdec->pdec = NULL;
	fdec->file_open = 0;
	fdec->file_lib = 0;
}


unsigned int
file_decoder_read(file_decoder_t * fdec, float * dest, int num) {

	decoder_t * dec = (decoder_t *)(fdec->pdec);

	return dec->read(dec, dest, num);
}


void
file_decoder_seek(file_decoder_t * fdec, unsigned long long seek_to_pos) {

	decoder_t * dec = (decoder_t *)(fdec->pdec);

	dec->seek(dec, seek_to_pos);
}
 

float
get_file_duration(char * file) {

	file_decoder_t * fdec;
	float duration;

	if ((fdec = file_decoder_new()) == NULL) {
                fprintf(stderr, "get_file_duration: error: file_decoder_new() returned NULL\n");
                return 0.0f;
        }

        if (file_decoder_open(fdec, file)) {
                fprintf(stderr, "file_decoder_open() failed on %s\n", file);
                return 0.0f;
        }

	duration = (float)fdec->fileinfo.total_samples / fdec->fileinfo.sample_rate;

        file_decoder_close(fdec);
        file_decoder_delete(fdec);

	return duration;
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

