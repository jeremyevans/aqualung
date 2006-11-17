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

    $Id: dec_cdda.h 238 2006-08-03 01:52:33Z tszilagyi $
*/


#ifndef _DEC_CDDA_H
#define _DEC_CDDA_H

#ifdef HAVE_CDDA
#include <cdio/cdio.h>
#include <cdio/paranoia.h>
#ifdef HAVE_CDDB
#undef HAVE_CDDB
#endif /* HAVE_CDDB */

#include "../cdda.h"
#endif /* HAVE_CDDA */

#include "file_decoder.h"


#ifdef HAVE_CDDA
typedef struct _cdda_pdata_t {
	char device_path[CDDA_MAXLEN];
	track_t track_no;
	cdrom_drive_t * drive;
	cdrom_paranoia_t * paranoia;
	lsn_t first_lsn;
	lsn_t last_lsn;
	lsn_t current_lsn;

	float buf[CDIO_CD_FRAMESIZE_RAW / 2];
	int bufptr;
} cdda_pdata_t;
#endif /* HAVE_CDDA */


decoder_t * cdda_decoder_init(file_decoder_t * fdec);
#ifdef HAVE_CDDA
void cdda_decoder_destroy(decoder_t * dec);
int cdda_decoder_open(decoder_t * dec, char * filename);
void cdda_decoder_close(decoder_t * dec);
unsigned int cdda_decoder_read(decoder_t * dec, float * dest, int num);
void cdda_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);
#endif /* HAVE_CDDA */


#endif /* _DEC_CDDA_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

