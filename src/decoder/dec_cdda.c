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

    $Id: dec_cdda.c 397 2006-11-07 11:21:56Z tszilagyi $
*/


#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../i18n.h"
#include "../cdda.h"
#include "dec_cdda.h"

#ifdef HAVE_CDDA

decoder_t *
cdda_decoder_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_cdda.c: cdda_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(cdda_pdata_t))) == NULL) {
                fprintf(stderr, "dec_cdda.c: cdda_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->init = cdda_decoder_init;
	dec->destroy = cdda_decoder_destroy;
	dec->open = cdda_decoder_open;
	dec->close = cdda_decoder_close;
	dec->read = cdda_decoder_read;
	dec->seek = cdda_decoder_seek;

	return dec;
}


void
cdda_decoder_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


/* filename must be of format "CDDA <device_path> <track_no>"
 * e.g. "CDDA /dev/cdrom 1"
 */
int
cdda_decoder_open(decoder_t * dec, char * filename) {

	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	unsigned int track;

	if (strlen(filename) < 4) {
		return DECODER_OPEN_BADLIB;
	}

	if ((filename[0] != 'C') ||
	    (filename[1] != 'D') ||
	    (filename[2] != 'D') ||
	    (filename[3] != 'A')) {
		return DECODER_OPEN_BADLIB;
	}

	if (sscanf(filename, "CDDA %s %u", pd->device_path, &track) < 2) {
		return DECODER_OPEN_BADLIB;
	}
	pd->track_no = track;

	printf("=== CDDA file_decoder, device=%s track=%u\n", pd->device_path, pd->track_no);

	pd->drive = cdio_cddap_identify(pd->device_path, 1, NULL);
	if (!pd->drive) {
		printf("Couldn't open drive\n");
		return DECODER_OPEN_BADLIB;
	}

	cdio_cddap_verbose_set(pd->drive, CDDA_MESSAGE_PRINTIT, CDDA_MESSAGE_PRINTIT);

	if (cdio_cddap_open(pd->drive) != 0) {
		printf("Unable to open disc.\n");
		return DECODER_OPEN_BADLIB;
	}

	pd->paranoia = cdio_paranoia_init(pd->drive);
	pd->first_lsn = cdio_cddap_track_firstsector(pd->drive, pd->track_no);
	pd->last_lsn = cdio_cddap_track_lastsector(pd->drive, pd->track_no);

	pd->bufptr = 0;

	printf("first = %u  last = %u  nsectors = %d\n", pd->first_lsn, pd->last_lsn, pd->drive->nsectors);

	cdio_paranoia_modeset(pd->paranoia, PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP);
	//cdio_paranoia_modeset(pd->paranoia, PARANOIA_MODE_DISABLE);
	cdio_paranoia_seek(pd->paranoia, pd->first_lsn, SEEK_SET);
	pd->current_lsn = pd->first_lsn;

	fdec->fileinfo.channels = 2;
	fdec->fileinfo.sample_rate = 44100;
	fdec->fileinfo.total_samples = (pd->last_lsn - pd->first_lsn + 1) * 588;

	fdec->file_lib = CDDA_LIB;
	fdec->fileinfo.bps = 2 * 16 * 44100;
	strcpy(dec->format_str, "Audio CD");

	/* TODO mark drive as used */
	return DECODER_OPEN_SUCCESS;
}


void
cdda_decoder_close(decoder_t * dec) {

	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;

	paranoia_free(pd->paranoia);
	cdda_close(pd->drive);
	/* TODO mark drive as unused */
}


unsigned int
cdda_decoder_read(decoder_t * dec, float * dest, int num) {

	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	int i, j = 0;
	unsigned int numread = 0;

	/* empty buffer (leftover from prev read) */
	for (i = 0; i < pd->bufptr; i++)
		dest[j++] = pd->buf[i];
	numread += pd->bufptr;
	pd->bufptr = 0;

	while (num - numread >= 588) {
		int16_t * readbuf = cdio_paranoia_read(pd->paranoia, NULL);
		++pd->current_lsn;
		for (i = 0; i < CDIO_CD_FRAMESIZE_RAW / 2; i++) {
			dest[j++] = readbuf[i] / 65536.0 * fdec->voladj_lin;
		}
		numread += 588;
	}

	if (num - numread > 0) {
		int k = 0;
		int16_t * readbuf = cdio_paranoia_read(pd->paranoia, NULL);
		++pd->current_lsn;
		for (i = 0; i < num - numread; i++) {
			dest[j++] = readbuf[i] / 65536.0 * fdec->voladj_lin;
		}
		for (i = num - numread; i < CDIO_CD_FRAMESIZE_RAW / 2; i++) {
			pd->buf[k++] = readbuf[i] / 65536.0 * fdec->voladj_lin;
		}
		pd->bufptr = k;
		numread = num;
	}

	return numread;
}


void
cdda_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {
/*	
	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	if ((pd->nframes = sf_seek(pd->sf, seek_to_pos, SEEK_SET)) != -1) {
		fdec->samples_left = fdec->fileinfo.total_samples - pd->nframes;
	} else {
		fprintf(stderr, "cdda_decoder_seek: warning: sf_seek() failed\n");
	}
*/
}


#else
decoder_t *
cdda_decoder_init(file_decoder_t * fdec) {

	return NULL;
}
#endif /* HAVE_CDDA */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

