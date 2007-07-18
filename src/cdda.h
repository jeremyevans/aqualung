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

#ifndef _CDDA_H
#define _CDDA_H

#include <config.h>

#ifdef HAVE_CDDA

#include <gtk/gtk.h>

#ifdef HAVE_CDDB
#define _TMP_HAVE_CDDB 1
#undef HAVE_CDDB
#endif /* HAVE_CDDB */
#include <cdio/cdio.h>
#include <cdio/paranoia.h>
#ifdef HAVE_CDDB
#undef HAVE_CDDB
#endif /* HAVE_CDDB */
#ifdef _TMP_HAVE_CDDB
#define HAVE_CDDB 1
#undef _TMP_HAVE_CDDB
#endif /* _TMP_HAVE_CDDB */

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CDDA_DRIVES_MAX 16
#define CDDA_MAXLEN 256

typedef struct {
	int n_tracks;
	lsn_t toc[100];
	unsigned long hash;
	unsigned long hash_prev;
	char record_name[MAXLEN];
	char artist_name[MAXLEN];
	char genre[MAXLEN];
	char year[MAXLEN];
} cdda_disc_t;

typedef struct {
	CdIo_t * cdio;
	int media_changed;
	int is_used;
	int swap_bytes;
	char device_path[CDDA_MAXLEN];
	cdda_disc_t disc;
} cdda_drive_t;


char * cdda_displayed_device_path(char * device_path);

void cdda_scanner_start(void);
void cdda_scanner_stop(void);

void cdda_timeout_start(void);
void cdda_timeout_stop(void);

void cdda_shutdown(void);

void create_cdda_node(void);
void insert_cdda_drive_node(char * device_path);
void remove_cdda_drive_node(char * device_path);
void refresh_cdda_drive_node(char * device_path);

void cdda_drive_info(cdda_drive_t * drive);
void cdda_disc_info(cdda_drive_t * drive);

unsigned long calc_cdda_hash(cdda_disc_t * disc);
int cdda_hash_matches(char * filename, unsigned long hash);

int cdda_get_n(char * device_path);
cdda_drive_t * cdda_get_drive_by_device_path(char * device_path);
cdda_drive_t * cdda_get_drive_by_spec_device_path(char * device_path);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HAVE_CDDA */

#endif /* _CDDA_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
