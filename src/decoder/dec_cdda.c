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


int cdda_read_error[CDDA_DRIVES_MAX];

/* This mess is mandated by the error callback of cdio_paranoia_read
 * not having a context argument. We have to use a different callback
 * for each possible decoder, with the context hard-coded. ARGH!
 */
#define DEFINE_ERROR_CB(n) \
void \
cdda_paranoia_callback_##n(long int inpos, paranoia_cb_mode_t function) { \
\
	switch (function) { \
	case PARANOIA_CB_READERR: \
		cdda_read_error[n] = 1; \
		break; \
	default: \
		break; \
	} \
}

#define ERROR_CB(n) cdda_paranoia_callback_##n

/* These have to agree with the value of CDDA_DRIVES_MAX */
DEFINE_ERROR_CB(0)
DEFINE_ERROR_CB(1)
DEFINE_ERROR_CB(2)
DEFINE_ERROR_CB(3)
DEFINE_ERROR_CB(4)
DEFINE_ERROR_CB(5)
DEFINE_ERROR_CB(6)
DEFINE_ERROR_CB(7)
DEFINE_ERROR_CB(8)
DEFINE_ERROR_CB(9)
DEFINE_ERROR_CB(10)
DEFINE_ERROR_CB(11)
DEFINE_ERROR_CB(12)
DEFINE_ERROR_CB(13)
DEFINE_ERROR_CB(14)
DEFINE_ERROR_CB(15)


void
cdda_reader_on_error(cdda_pdata_t * pd) {

	char flush_dest;
	cdda_drive_t * cdda_drive;
	
	pd->is_eos = 1;
	pd->pos_lsn = pd->last_lsn;
	
	while (rb_read_space(pd->rb))
		rb_read(pd->rb, &flush_dest, sizeof(char));

	cdda_drive = cdda_get_drive_by_device_path(pd->device_path);
	if (cdda_drive == NULL) {
		return;
	}
	cdda_drive->disc.hash_prev = cdda_drive->disc.hash;
	cdda_drive->disc.hash = 0L;
}


void *
cdda_reader_thread(void * arg) {

        decoder_t * dec = (decoder_t *)arg;
	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	cdda_drive_t * cdda_drive;

	int16_t * readbuf;
	int i;

	int n = cdda_get_n(pd->device_path);
	void(*callback)(long int, paranoia_cb_mode_t) = NULL;

	cdda_drive = cdda_get_drive_by_device_path(pd->device_path);
	if ((n < 0) || (cdda_drive == NULL)) {
		cdda_reader_on_error(pd);
		pd->cdda_reader_status = CDDA_READER_FREE;
		AQUALUNG_THREAD_DETACH()
		return NULL;
	}
	if (cdda_drive->disc.hash == 0L) { /* disc has been removed */
		cdda_reader_on_error(pd);
		pd->cdda_reader_status = CDDA_READER_FREE;
		AQUALUNG_THREAD_DETACH()
		return NULL;
	}

	switch (n) {
        /* These have to agree with the value of CDDA_DRIVES_MAX */
	case 0: callback = ERROR_CB(0); break;
	case 1: callback = ERROR_CB(1); break;
	case 2: callback = ERROR_CB(2); break;
	case 3: callback = ERROR_CB(3); break;
	case 4: callback = ERROR_CB(4); break;
	case 5: callback = ERROR_CB(5); break;
	case 6: callback = ERROR_CB(6); break;
	case 7: callback = ERROR_CB(7); break;
	case 8: callback = ERROR_CB(8); break;
	case 9: callback = ERROR_CB(9); break;
	case 10: callback = ERROR_CB(10); break;
	case 11: callback = ERROR_CB(11); break;
	case 12: callback = ERROR_CB(12); break;
	case 13: callback = ERROR_CB(13); break;
	case 14: callback = ERROR_CB(14); break;
	case 15: callback = ERROR_CB(15); break;
	}

	pd->paranoia = cdio_paranoia_init(pd->drive);
	cdio_paranoia_modeset(pd->paranoia, pd->paranoia_mode);
	cdio_paranoia_seek(pd->paranoia, pd->pos_lsn, SEEK_SET);

	while ((rb_write_space(pd->rb) > CDIO_CD_FRAMESIZE_RAW * 2) &&
	       (pd->pos_lsn < pd->disc_last_lsn)) {

		AQUALUNG_MUTEX_LOCK(pd->cdda_reader_mutex)
		if (pd->cdda_reader_status == CDDA_READER_FREE) {
			AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)
			pd->overread_sectors = 0;
			cdio_paranoia_free(pd->paranoia);
			return NULL;
		}
		AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)

		if (pd->paranoia_mode & PARANOIA_MODE_NEVERSKIP) {
			readbuf = cdio_paranoia_read(pd->paranoia, callback);
		} else {
			readbuf = cdio_paranoia_read_limited(pd->paranoia, callback,
							     pd->paranoia_maxretries);
		}

		if (cdda_read_error[n]) {
			cdda_reader_on_error(pd);
			pd->cdda_reader_status = CDDA_READER_FREE;
			AQUALUNG_THREAD_DETACH()
		}

		if (pd->pos_lsn > pd->last_lsn)
			++pd->overread_sectors;
		++pd->pos_lsn;

		if (cdda_drive->bigendian) {
			for (i = 0; i < CDIO_CD_FRAMESIZE_RAW / 2; i++) {
				readbuf[i] = UINT16_SWAP_LE_BE_C(readbuf[i]);
			}
		}

		for (i = 0; i < CDIO_CD_FRAMESIZE_RAW / 2; i++) {
			float f = readbuf[i] / 32768.0 * fdec->voladj_lin;
			rb_write(pd->rb, (char *)&f, sample_size);
		}
		pd->is_eos = (pd->pos_lsn >= pd->last_lsn ? 1 : 0);
	}

	cdio_paranoia_free(pd->paranoia);

	AQUALUNG_MUTEX_LOCK(pd->cdda_reader_mutex)
	pd->cdda_reader_status = CDDA_READER_FREE;
	AQUALUNG_THREAD_DETACH()
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


/* filename must be of format "CDDA <device_path> <disc hash> <track_no>"
 * e.g. "CDDA /dev/cdrom 6809B809 1"
 */
int
cdda_decoder_open(decoder_t * dec, char * filename) {

	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	unsigned int track;
	unsigned long hash;
	cdda_drive_t * drive;

	if (strlen(filename) < 4) {
		return DECODER_OPEN_BADLIB;
	}

	if ((filename[0] != 'C') ||
	    (filename[1] != 'D') ||
	    (filename[2] != 'D') ||
	    (filename[3] != 'A')) {
		return DECODER_OPEN_BADLIB;
	}

	if (sscanf(filename, "CDDA %s %lX %u", pd->device_path, &hash, &track) < 3) {
		return DECODER_OPEN_BADLIB;
	}
	pd->track_no = track;

	drive = cdda_get_drive_by_device_path(pd->device_path);
	if (drive == NULL) {
		return DECODER_OPEN_FERROR;
	}
	if (drive->disc.hash != hash) {
		return DECODER_OPEN_FERROR;
	}
	if (drive->is_used) {
		fprintf(stderr, "cdda_decoder_open: drive %s is already in use\n", pd->device_path);
		return DECODER_OPEN_FERROR;
	}
	drive->is_used = 1;

	pd->cdio = cdio_open(pd->device_path, DRIVER_DEVICE);
	if (!pd->cdio) {
		printf("cdda_decoder_open: couldn't open cdio device %s\n", pd->device_path);
		drive->is_used = 0;
		return DECODER_OPEN_FERROR;
	}

	pd->drive = cdio_cddap_identify_cdio(pd->cdio, 0, NULL);
	if (!pd->drive) {
		printf("cdda_decoder_open: couldn't open drive %s\n", pd->device_path);
		drive->is_used = 0;
		cdio_destroy(pd->cdio);
		pd->cdio = NULL;
		return DECODER_OPEN_FERROR;
	}

	cdio_cddap_verbose_set(pd->drive, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

	if (cdio_cddap_open(pd->drive) != 0) {
		printf("cdda_decoder_open: unable to open disc.\n");
		drive->is_used = 0;
		cdio_cddap_close_no_free_cdio(pd->drive);
		pd->drive = NULL;
		cdio_destroy(pd->cdio);
		pd->cdio = NULL;
		return DECODER_OPEN_FERROR;
	}

	pd->first_lsn = cdio_cddap_track_firstsector(pd->drive, pd->track_no);
	pd->last_lsn = cdio_cddap_track_lastsector(pd->drive, pd->track_no);
	pd->disc_last_lsn = cdio_cddap_disc_lastsector(pd->drive);
	pd->pos_lsn = pd->first_lsn;
	pd->overread_sectors = 0;
	pd->is_eos = 0;

	if (drive->bigendian == -1) {
		drive->bigendian = data_bigendianp(pd->drive);
	}
	if (drive->bigendian == -1) {
		printf("cdda_decoder_open: unable to determine endianness of drive.\n");
		drive->is_used = 0;
		cdio_cddap_close_no_free_cdio(pd->drive);
		pd->drive = NULL;
		cdio_destroy(pd->cdio);
		pd->cdio = NULL;
		return DECODER_OPEN_FERROR;
	}

	/* use this default in case cdda_decoder_set_mode() won't be called */
	pd->paranoia_mode = PARANOIA_MODE_DISABLE;

	pd->rb = rb_create(2 * sample_size * RB_CDDA_SIZE);

	fdec->fileinfo.channels = 2;
	fdec->fileinfo.sample_rate = 44100;
	fdec->fileinfo.total_samples = (pd->last_lsn - pd->first_lsn + 1) * 588;

	fdec->file_lib = CDDA_LIB;
	fdec->fileinfo.bps = 2 * 16 * 44100;
	strcpy(dec->format_str, "Audio CD");

	return DECODER_OPEN_SUCCESS;
}


/* set the next (pre-buffered) track on the same CD without closing */
int
cdda_decoder_reopen(decoder_t * dec, char * filename) {

	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	unsigned int track;
	unsigned long hash;
	cdda_drive_t * drive;

	if (strlen(filename) < 4) {
		return DECODER_OPEN_BADLIB;
	}

	if ((filename[0] != 'C') ||
	    (filename[1] != 'D') ||
	    (filename[2] != 'D') ||
	    (filename[3] != 'A')) {
		return DECODER_OPEN_BADLIB;
	}

	if (sscanf(filename, "CDDA %s %lX %u", pd->device_path, &hash, &track) < 3) {
		return DECODER_OPEN_BADLIB;
	}
	pd->track_no = track;

	drive = cdda_get_drive_by_device_path(pd->device_path);
	if (drive == NULL) {
		return DECODER_OPEN_FERROR;
	}
	if (drive->disc.hash != hash) {
		drive->is_used = 0;
		return DECODER_OPEN_FERROR;
	}

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
cdda_decoder_set_mode(decoder_t * dec, int drive_speed,
		      int paranoia_mode, int paranoia_maxretries) {

	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;

	if (pd->cdio == NULL)
		return;

	if (cdio_set_speed(pd->cdio, drive_speed) != DRIVER_OP_SUCCESS) {
		fprintf(stderr, "Warning: setting CD drive speed seems to be unsupported.\n");
	}

	pd->paranoia_mode = paranoia_mode;
	pd->paranoia_maxretries = paranoia_maxretries;
}


void
cdda_decoder_close(decoder_t * dec) {

	cdda_pdata_t * pd = (cdda_pdata_t *)dec->pdata;
	cdda_drive_t * drive;

	AQUALUNG_MUTEX_LOCK(pd->cdda_reader_mutex)
	if (pd->cdda_reader_status == CDDA_READER_BUSY) {
		pd->cdda_reader_status = CDDA_READER_FREE;
		AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)
		AQUALUNG_THREAD_JOIN(pd->cdda_reader_id);
	} else {
		AQUALUNG_MUTEX_UNLOCK(pd->cdda_reader_mutex)
	}

	rb_free(pd->rb);
	cdio_cddap_close_no_free_cdio(pd->drive);
	pd->drive = NULL;
	cdio_set_speed(pd->cdio, 100);
	cdio_destroy(pd->cdio);
	pd->cdio = NULL;

	drive = cdda_get_drive_by_device_path(pd->device_path);
	if (drive == NULL) {
		return;
	}
	cdda_read_error[cdda_get_n(pd->device_path)] = 0;
	drive->is_used = 0;
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

