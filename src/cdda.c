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

#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#ifdef HAVE_CDDB
#undef HAVE_CDDB
#endif /* HAVE_CDDB */

#include "common.h"
#include "cdda.h"


cdda_drive_t cdda_drives[CDDA_DRIVES_MAX];


/* fills cdda_drives and returns the number of drives found */
int
cdda_get_drives(cdda_drive_t * cdda_drives) {

	char ** drives = NULL;
	int i;

	printf("cdda_get_drives\n");

	drives = cdio_get_devices(DRIVER_DEVICE);
	if (!drives)
		return 0;

	for (i = 0; (drives[i] != NULL) && (i < CDDA_DRIVES_MAX); ) {

		cdio_hwinfo_t hwinfo;
		CdIo_t * cdio = cdio_open(drives[i], DRIVER_DEVICE);
		track_t tracks;

		if (!cdio)			
			continue;

		if (!cdio_get_hwinfo(cdio, &hwinfo))
			continue;

		printf("Drive %d = %s\n", i, drives[i]);
		printf("%-28s: %s\n%-28s: %s\n%-28s: %s\n",
		       "Vendor"  , hwinfo.psz_vendor,
		       "Model"   , hwinfo.psz_model,
		       "Revision", hwinfo.psz_revision);
		printf("\n");

		strncpy(cdda_drives[i].device_path, drives[i], CDDA_MAXLEN-1);
		strncpy(cdda_drives[i].vendor, hwinfo.psz_vendor, CDDA_MAXLEN-1);
		strncpy(cdda_drives[i].model, hwinfo.psz_model, CDDA_MAXLEN-1);
		strncpy(cdda_drives[i].revision, hwinfo.psz_revision, CDDA_MAXLEN-1);

		tracks = cdio_get_num_tracks(cdio);
		printf("tracks = %d\n", tracks);

		/* see if there is a CD in the drive */
		if (tracks > 0 && tracks < 100) {
			track_t first_track;
			int j;

			/* XXX: it might be any CD at this point, not just CD-DA */
			
			first_track = j = cdio_get_first_track_num(cdio);
			printf("first_track = %d\n", first_track);

			printf("CD-ROM Track List (%i - %i)\n", first_track, first_track + tracks - 1);
			printf("  #:  LSN\n");
			
			for (; j < tracks; j++) {
				lsn_t lsn = cdio_get_track_lsn(cdio, j);
				if (lsn != CDIO_INVALID_LSN) {
					printf("%3d: %06lu\n", (int) i, (long unsigned int) lsn);
				}
			}
			printf("%3X: %06lu  leadout\n", CDIO_CDROM_LEADOUT_TRACK,
			       (long unsigned int) cdio_get_track_lsn(cdio, CDIO_CDROM_LEADOUT_TRACK));
		}

		cdio_destroy(cdio);
		++i;
	}
	cdio_free_device_list(drives);

	printf("%d drive(s) found\n", i);
	return i;
}


/* test function */
void
cdda_test(void) {

	cdda_get_drives(cdda_drives);
}


#endif /* HAVE_CDDA */
