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


#ifndef _DEC_CDDA_H
#define _DEC_CDDA_H

#ifdef HAVE_CDDB
#define _TMP_HAVE_CDDB 1
#undef HAVE_CDDB
#endif /* HAVE_CDDB */
#include <cdio/cdio.h>
#include <cdio/paranoia.h>
#include <cdio/bytesex.h>
#ifdef HAVE_CDDB
#undef HAVE_CDDB
#endif /* HAVE_CDDB */
#ifdef _TMP_HAVE_CDDB
#define HAVE_CDDB 1
#undef _TMP_HAVE_CDDB
#endif /* _TMP_HAVE_CDDB */

#ifdef _WIN32
#include <glib.h>
#else
#include <pthread.h>
#endif /* _WIN32*/

#include "../cdda.h"
#include "file_decoder.h"


/* size of ringbuffer for decoded CD Audio data (in frames) */
#define RB_CDDA_SIZE (1<<20)

typedef struct _cdda_pdata_t {
	rb_t * rb;
	char device_path[CDDA_MAXLEN];
	track_t track_no;
	CdIo_t * cdio;
	cdrom_drive_t * drive;
	cdrom_paranoia_t * paranoia;
	lsn_t first_lsn;
	lsn_t last_lsn;
	lsn_t disc_last_lsn;
	int overread_sectors;
	lsn_t pos_lsn;
	int is_eos;
	AQUALUNG_THREAD_DECLARE(cdda_reader_id)
	AQUALUNG_MUTEX_DECLARE(cdda_reader_mutex)
	int cdda_reader_status;
	int paranoia_mode;
	int paranoia_maxretries;
} cdda_pdata_t;


decoder_t * cdda_decoder_init(file_decoder_t * fdec);
void cdda_decoder_destroy(decoder_t * dec);
int cdda_decoder_open(decoder_t * dec, char * filename);
void cdda_decoder_send_metadata(decoder_t * dec);
int cdda_decoder_reopen(decoder_t * dec, char * filename);
void cdda_decoder_set_mode(decoder_t * dec, int drive_speed, int paranoia_mode, int paranoia_maxretries);
void cdda_decoder_close(decoder_t * dec);
unsigned int cdda_decoder_read(decoder_t * dec, float * dest, int num);
void cdda_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);


#endif /* _DEC_CDDA_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

