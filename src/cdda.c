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

    $Id: cdda.c 378 2006-09-26 12:46:20Z tszilagyi $
*/

#include <config.h>

#ifdef HAVE_CDDA

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <cdio/cdio.h>
#ifdef HAVE_CDDB
#undef HAVE_CDDB
#endif /* HAVE_CDDB */

#include "common.h"
#include "cdda.h"


cdda_drive_t cdda_drives[CDDA_DRIVES_MAX];


cdda_drive_t *
cdda_get_drive(int n) {
	
	if (n >= 0 && n < CDDA_DRIVES_MAX)
		return &cdda_drives[n];
	else
		return NULL;
}

cdda_drive_t *
cdda_get_drive_by_device_path(char * device_path) {

	int i;
	for (i = 0; i < CDDA_DRIVES_MAX; i++) {
		if (!strcmp(device_path, cdda_drives[i].device_path))
			return &cdda_drives[i];
	}
	return NULL;
}

/* returns the number of tracks found (maybe 0), or -1 if drive is not valid */
int
cdda_scan_drive(char * device_path, cdda_drive_t * cdda_drive) {

	cdio_hwinfo_t hwinfo;
	CdIo_t * cdio = NULL;
	track_t tracks;

	/* if the drive is being used (currently playing back), we should
	   not access it -- return stored data instead. */
	if (cdda_drive->is_used) {
		return cdda_drive->disc.n_tracks;
	}

#ifndef _WIN32
	if (g_file_test(device_path, G_FILE_TEST_IS_SYMLINK)) {
		return -1;
	}
#endif /* !_WIN32 */

	cdio = cdio_open(device_path, DRIVER_DEVICE);
	if (!cdio) {
		return -1;
	}

	if (!cdio_get_hwinfo(cdio, &hwinfo)) {
		cdio_destroy(cdio);
		return -1;
	}

	printf("\nDrive = %s\n", device_path);
	printf("%-28s: %s\n%-28s: %s\n%-28s: %s\n",
	       "Vendor"  , hwinfo.psz_vendor,
	       "Model"   , hwinfo.psz_model,
	       "Revision", hwinfo.psz_revision);
	printf("\n");

	strncpy(cdda_drive->device_path, device_path, CDDA_MAXLEN-1);
	strncpy(cdda_drive->vendor, hwinfo.psz_vendor, CDDA_MAXLEN-1);
	strncpy(cdda_drive->model, hwinfo.psz_model, CDDA_MAXLEN-1);
	strncpy(cdda_drive->revision, hwinfo.psz_revision, CDDA_MAXLEN-1);

	tracks = cdio_get_num_tracks(cdio);
	printf("tracks = %d\n", tracks);

	/* see if there is a CD in the drive */
	if (tracks > 0 && tracks < 100) {
		track_t first_track;
		track_t last_track;
		int j, n_track = 0;

		/* XXX: it might be any CD at this point, not just CD-DA */
		
		first_track = cdio_get_first_track_num(cdio);
		last_track = cdio_get_last_track_num(cdio);
		
		printf("first_track = %d\n", first_track);
		printf("last_track = %d\n", last_track);
		printf("CD-ROM Track List (%i - %i)\n", first_track, last_track);
		printf("  #:  LSN\n");
		
		cdda_drive->disc.n_tracks = tracks;
		for (j = first_track; j <= last_track; j++) {
			lsn_t lsn = cdio_get_track_lsn(cdio, j);
			if (lsn != CDIO_INVALID_LSN) {
				printf("%3d: %06lu\n", (int) j, (long unsigned int) lsn);
				cdda_drive->disc.toc[n_track++] = lsn;
			}
		}
		cdda_drive->disc.toc[n_track] =
			cdio_get_track_lsn(cdio, CDIO_CDROM_LEADOUT_TRACK);
		printf("%3X: %06lu  leadout\n", CDIO_CDROM_LEADOUT_TRACK,
		       (long unsigned int) cdio_get_track_lsn(cdio, CDIO_CDROM_LEADOUT_TRACK));
	} else {
		cdda_drive->disc.n_tracks = 0;
	}
	
	cdio_destroy(cdio);
	return cdda_drive->disc.n_tracks;
}

/* fills cdda_drives and returns the number of drives found */
int
cdda_scan_all_drives(void) {

	char ** drives = NULL;
	int i, n = 0;

	drives = cdio_get_devices(DRIVER_DEVICE);
	if (!drives)
		return 0;

	for (i = 0; (drives[i] != NULL) && (i < CDDA_DRIVES_MAX); i++) {
		if (cdda_scan_drive(drives[i], cdda_get_drive(n)) >= 0)
			++n;
	}
	cdio_free_device_list(drives);

	printf("%d drive(s) found\n", n);
	return n;
}


#endif /* HAVE_CDDA */
