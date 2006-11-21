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
#include <time.h>

#include "../i18n.h"
#include "../cdda.h"
#include "dec_cdda.h"

#ifdef HAVE_CDDA

extern size_t sample_size;

#define CDDA_READER_FREE   0
#define CDDA_READER_BUSY   1

void *
cdda_reader_thread(void * arg) {

        decoder_t * dec = (decoder_t *)arg;
	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	int16_t * readbuf;
	int i;

	printf("START cdda_reader_thread\n");

	pd->paranoia = cdio_paranoia_init(pd->drive);
	cdio_paranoia_modeset(pd->paranoia, PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP);
	//cdio_paranoia_modeset(pd->paranoia, PARANOIA_MODE_DISABLE);
	cdio_paranoia_seek(pd->paranoia, pd->pos_lsn, SEEK_SET);

	while ((rb_write_space(pd->rb) > CDIO_CD_FRAMESIZE_RAW * 2) &&
	       (pd->pos_lsn < pd->disc_last_lsn)) {

		AQUALUNG_MUTEX_LOCK(pd->cdda_reader_mutex)
		if (pd->cdda_reader_status == CDDA_READER_FREE) {
			AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)
			printf("CANCELLED cdda_reader_thread\n");
			pd->overread_sectors = 0;
			paranoia_free(pd->paranoia);
			return NULL;
		}
		AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)

		readbuf = cdio_paranoia_read(pd->paranoia, NULL);
		if (pd->pos_lsn > pd->last_lsn)
			++pd->overread_sectors;
		++pd->pos_lsn;
		for (i = 0; i < CDIO_CD_FRAMESIZE_RAW / 2; i++) {
			float f = readbuf[i] / 32768.0 * fdec->voladj_lin;
			rb_write(pd->rb, (char *)&f, sample_size);
		}
		pd->is_eos = (pd->pos_lsn >= pd->last_lsn ? 1 : 0);
	}

	if (pd->is_eos)
		printf("end of track, overread = %d\n", pd->overread_sectors);

	paranoia_free(pd->paranoia);
	printf("FINISH cdda_reader_thread\n");

	AQUALUNG_MUTEX_LOCK(pd->cdda_reader_mutex)
	pd->cdda_reader_status = CDDA_READER_FREE;
	AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)

	return NULL;
}


decoder_t *
cdda_decoder_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;
#ifdef _WIN32
	cdda_pdata_t * pd = NULL;
#endif /* _WIN32 */

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
#ifdef _WIN32
	pd = (cdda_pdata_t *)dec->pdata;
	pd->cdda_reader_mutex = g_mutex_new();
#endif /* _WIN32 */

	return dec;
}


void
cdda_decoder_destroy(decoder_t * dec) {

#ifdef _WIN32
	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;
	g_mutex_free(pd->cdda_reader_mutex);
#endif /* _WIN32 */
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

	pd->drive = cdio_cddap_identify(pd->device_path, 0, NULL);
	if (!pd->drive) {
		printf("dec_cdda.c: Couldn't open drive %s\n", pd->device_path);
		return DECODER_OPEN_BADLIB;
	}

	cdio_cddap_verbose_set(pd->drive, CDDA_MESSAGE_PRINTIT, CDDA_MESSAGE_FORGETIT);

	if (cdio_cddap_open(pd->drive) != 0) {
		printf("dec_cdda.c: Unable to open disc.\n");
		return DECODER_OPEN_BADLIB;
	}

	pd->first_lsn = cdio_cddap_track_firstsector(pd->drive, pd->track_no);
	pd->last_lsn = cdio_cddap_track_lastsector(pd->drive, pd->track_no);
	pd->disc_last_lsn = cdio_cddap_disc_lastsector(pd->drive);
	pd->pos_lsn = pd->first_lsn;
	pd->overread_sectors = 0;
	pd->is_eos = 0;

	pd->rb = rb_create(2 * sample_size * RB_CDDA_SIZE);

	fdec->fileinfo.channels = 2;
	fdec->fileinfo.sample_rate = 44100;
	fdec->fileinfo.total_samples = (pd->last_lsn - pd->first_lsn + 1) * 588;

	fdec->file_lib = CDDA_LIB;
	fdec->fileinfo.bps = 2 * 16 * 44100;
	strcpy(dec->format_str, "Audio CD");

	/* TODO mark drive as used */
	return DECODER_OPEN_SUCCESS;
}


/* set a new track on the same CD without closing -- this is fast */
int
cdda_decoder_reopen(decoder_t * dec, char * filename) {

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

	printf("=== CDDA REOPEN for track=%u\n", pd->track_no);

	if (pd->overread_sectors > 0) {
		char flush_dest;
		while (rb_read_space(pd->rb) > pd->overread_sectors * CDIO_CD_FRAMESIZE_RAW * 2)
			rb_read(pd->rb, &flush_dest, sizeof(char));
	}

	pd->first_lsn = cdio_cddap_track_firstsector(pd->drive, pd->track_no);
	pd->last_lsn = cdio_cddap_track_lastsector(pd->drive, pd->track_no);
	pd->pos_lsn = pd->first_lsn + pd->overread_sectors;
	pd->overread_sectors = 0;
	pd->is_eos = 0;

	fdec->fileinfo.total_samples = (pd->last_lsn - pd->first_lsn + 1) * 588;

	return DECODER_OPEN_SUCCESS;
}


void
cdda_decoder_close(decoder_t * dec) {

	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;

	AQUALUNG_MUTEX_LOCK(pd->cdda_reader_mutex)
	if (pd->cdda_reader_status == CDDA_READER_BUSY) {
		pd->cdda_reader_status = CDDA_READER_FREE;
		AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)
		AQUALUNG_THREAD_JOIN(pd->cdda_reader_id);
	} else {
		AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)
	}

	rb_free(pd->rb);
	cdda_close(pd->drive);
	/* TODO mark drive as unused */

	printf("=== CDDA file_decoder CLOSE\n");
}


unsigned int
cdda_decoder_read(decoder_t * dec, float * dest, int num) {

	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;

        unsigned int numread = 0;
        unsigned int n_avail = 0;

	if ((rb_read_space(pd->rb) / sample_size < RB_CDDA_SIZE >> 2) && (!pd->is_eos)) {
		AQUALUNG_MUTEX_LOCK(pd->cdda_reader_mutex)
		if (pd->cdda_reader_status != CDDA_READER_BUSY) {
			pd->cdda_reader_status = CDDA_READER_BUSY;
			AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)
			AQUALUNG_THREAD_CREATE(pd->cdda_reader_id, NULL, cdda_reader_thread, dec)
		} else {
			AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)
		}
	}

        while ((rb_read_space(pd->rb) - (pd->overread_sectors * CDIO_CD_FRAMESIZE_RAW * 2) <
		num * 2 * sample_size) && (!pd->is_eos)) {
		struct timespec req;
		struct timespec rem;
		req.tv_sec = 0;
		req.tv_nsec = 100000000; /* 100 ms */
		printf("cdda_decoder_read: data not ready, waiting...\n");
		nanosleep(&req, &rem);
	}

        n_avail = (rb_read_space(pd->rb) - pd->overread_sectors * CDIO_CD_FRAMESIZE_RAW * 2)
		/ (2 * sample_size);

        if (n_avail > num)
                n_avail = num;

        rb_read(pd->rb, (char *)dest, n_avail * 2 * sample_size);
        numread = n_avail;
        return numread;
}


void
cdda_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {

	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	char flush_dest;

	AQUALUNG_MUTEX_LOCK(pd->cdda_reader_mutex)
	if (pd->cdda_reader_status == CDDA_READER_BUSY) {
		pd->cdda_reader_status = CDDA_READER_FREE;
		AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)
		AQUALUNG_THREAD_JOIN(pd->cdda_reader_id)
	} else {
		AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)
	}

	pd->pos_lsn = pd->first_lsn + seek_to_pos / 588;
	fdec->samples_left = fdec->fileinfo.total_samples - (seek_to_pos / 588) * 588;

	while (rb_read_space(pd->rb))
		rb_read(pd->rb, &flush_dest, sizeof(char));
	pd->overread_sectors = 0;

	AQUALUNG_MUTEX_LOCK(pd->cdda_reader_mutex)
	if ((pd->pos_lsn < pd->last_lsn) && (pd->cdda_reader_status == CDDA_READER_FREE)) {
		pd->cdda_reader_status = CDDA_READER_BUSY;
		AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)
		pd->is_eos = 0;
		AQUALUNG_THREAD_CREATE(pd->cdda_reader_id, NULL, cdda_reader_thread, dec)
	} else {
		AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)
	}
}


#else
decoder_t *
cdda_decoder_init(file_decoder_t * fdec) {

	return NULL;
}
#endif /* HAVE_CDDA */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

