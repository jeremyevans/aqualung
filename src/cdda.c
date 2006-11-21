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

#ifdef HAVE_CDDA

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#ifndef _WIN32
#include <pthread.h>
#endif /* _WIN32 */

#include <cdio/cdio.h>
#include <cdio/cdda.h>
#include <cdio/logging.h>
#ifdef HAVE_CDDB
#undef HAVE_CDDB
#endif /* HAVE_CDDB */

#include "common.h"
#include "options.h"
#include "i18n.h"
#include "cdda.h"


extern options_t options;
extern GdkPixbuf * icon_store;

cdda_drive_t cdda_drives[CDDA_DRIVES_MAX];
AQUALUNG_THREAD_DECLARE(cdda_scanner_id)
int cdda_scanner_working;


static void
cdda_log_handler(cdio_log_level_t level, const char message[]) {

	switch(level) {
	case CDIO_LOG_DEBUG:
	case CDIO_LOG_INFO:
	case CDIO_LOG_WARN:
		return;
	default:
		printf("cdio %d: %s\n", level, message);
	}
}


int
cdda_get_first_free_slot(int * n) {

	int i;
	for (i = 0; i < CDDA_DRIVES_MAX; i++) {
		if (cdda_drives[i].device_path[0] == '\0') {
			*n = i;
			return 0;
		}
	}
	return -1;
}


cdda_drive_t *
cdda_get_drive(int n) {
	
	if (n >= 0 && n < CDDA_DRIVES_MAX)
		return &cdda_drives[n];
	else
		return NULL;
}

int
cdda_get_n(char * device_path) {

	int i;
	for (i = 0; i < CDDA_DRIVES_MAX; i++) {
		if (!strcmp(device_path, cdda_drives[i].device_path))
			return i;
	}
	return -1;
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

/* return 0 if OK, -1 if drive is apparently not available */
int
cdda_scan_drive(char * device_path, cdda_drive_t * cdda_drive) {

	cdio_hwinfo_t hwinfo;
	track_t tracks;
	track_t first_track;
	track_t last_track;
	int i, n = 0;
	cdrom_drive_t * d;

	/* if the drive is being used (currently playing), we should not access it */
	if (cdda_drive->is_used) {
		return 0;
	}

#ifndef _WIN32
	if (g_file_test(device_path, G_FILE_TEST_IS_SYMLINK)) {
		return -1;
	}
#endif /* !_WIN32 */

	if (cdda_drive->cdio == NULL) {
		cdda_drive->cdio = cdio_open(device_path, DRIVER_DEVICE);
		if (!cdda_drive->cdio) {
			return -1;
		}
		
		if (!cdio_get_hwinfo(cdda_drive->cdio, &hwinfo)) {
			cdio_destroy(cdda_drive->cdio);
			cdda_drive->cdio = NULL;
			return -1;
		}
		
		strncpy(cdda_drive->device_path, device_path, CDDA_MAXLEN-1);
		strncpy(cdda_drive->vendor, hwinfo.psz_vendor, CDDA_MAXLEN-1);
		strncpy(cdda_drive->model, hwinfo.psz_model, CDDA_MAXLEN-1);
		strncpy(cdda_drive->revision, hwinfo.psz_revision, CDDA_MAXLEN-1);
	}
	
	cdda_drive->disc.n_tracks = 0;
	tracks = cdio_get_num_tracks(cdda_drive->cdio);

	if (tracks < 1 || tracks >= 100)
		return 0;

	d = cdio_cddap_identify_cdio(cdda_drive->cdio, 0, NULL);
	if (d == NULL) {
		return -1;
	}
		
	first_track = cdio_get_first_track_num(cdda_drive->cdio);
	last_track = cdio_get_last_track_num(cdda_drive->cdio);

	for (i = first_track; i <= last_track; i++) {
		if (!cdio_cddap_track_audiop(d, i)) {
			last_track = i-1;
			break;
		}
	}

	cdio_cddap_close_no_free_cdio(d);
	
	if (last_track < first_track) {
		return 0;
	}

	cdda_drive->disc.n_tracks = tracks;
	for (i = first_track; i <= last_track; i++) {
		lsn_t lsn = cdio_get_track_lsn(cdda_drive->cdio, i);
		if (lsn != CDIO_INVALID_LSN) {
			cdda_drive->disc.toc[n++] = lsn;
		}
	}
	cdda_drive->disc.toc[n] = cdio_get_track_lsn(cdda_drive->cdio, CDIO_CDROM_LEADOUT_TRACK);

	return 0;
}


void
cdda_scan_all_drives(void) {

	char ** drives = NULL;
	char touched[CDDA_DRIVES_MAX];
	int i;

	for (i = 0; i < CDDA_DRIVES_MAX; i++) {
		if (cdda_drives[i].cdio != NULL) {
			cdda_drives[i].media_changed = cdio_get_media_changed(cdda_drives[i].cdio);
			if (cdda_drives[i].media_changed) {
				cdio_destroy(cdda_drives[i].cdio);
				cdda_drives[i].cdio = NULL;
			}
		}
	}

	drives = cdio_get_devices(DRIVER_DEVICE);
	if (!drives)
		return;

	for (i = 0; i < CDDA_DRIVES_MAX; i++)
		touched[i] = 0;

	for (i = 0; (drives[i] != NULL) && (i < CDDA_DRIVES_MAX); i++) {
		int n;
		/* see if drives[i] is already known to us... */
		cdda_drive_t * d = cdda_get_drive_by_device_path(drives[i]);
		if (d != NULL) { /* yes */
			n = cdda_get_n(drives[i]);
			touched[n] = 1;
			if (cdda_drives[n].media_changed) {
				cdda_scan_drive(drives[i], cdda_get_drive(n));
				/* EVENT do something to refresh disc data */
			}
		} else { /* no, scan the drive */
			if (cdda_get_first_free_slot(&n) < 0) {
				printf("cdda.c: error: too many drives\n");
				return;
			}
			if (cdda_scan_drive(drives[i], cdda_get_drive(n)) >= 0) {
				touched[n] = 1;
				/* EVENT do something to insert newly discovered drive */
				printf("discovered drive %s\n", drives[i]);
			}
		}
	}
	cdio_free_device_list(drives);

	/* remove all drives that were not touched (those that disappeared) */
	for (i = 0; i < CDDA_DRIVES_MAX; i++) {
		if ((cdda_drives[i].device_path[0] != '\0') && (touched[i] == 0)) {

			/* EVENT do something to remove drive */

			printf("drive %s does not exist anymore, removing.\n", cdda_drives[i].device_path);
			cdio_destroy(cdda_drives[i].cdio);
			memset(cdda_drives + i, 0, sizeof(cdda_drive_t));
		}
	}
}


void *
cdda_scanner(void * arg) {

	int i = 0;

	cdio_log_set_handler(cdda_log_handler);
	cdda_scan_all_drives();

	while (cdda_scanner_working) {

		struct timespec req_time;
		struct timespec rem_time;
		req_time.tv_sec = 0;
		req_time.tv_nsec = 100000000;

		nanosleep(&req_time, &rem_time);

		if (i == 50) {
			cdda_scan_all_drives();
			i = 0;
		}
		++i;
	}

	for (i = 0; i < CDDA_DRIVES_MAX; i++) {
		if (cdda_drives[i].cdio != NULL) {
			cdio_destroy(cdda_drives[i].cdio);
			memset(cdda_drives + i, 0, sizeof(cdda_drive_t));
		}
	}

	return NULL;
}


void
cdda_scanner_start(void) {

	if (cdda_scanner_working)
		return;

	cdda_scanner_working = 1;
	AQUALUNG_THREAD_CREATE(cdda_scanner_id, NULL, cdda_scanner, NULL)
}


void
cdda_scanner_stop(void) {

	cdda_scanner_working = 0;
	AQUALUNG_THREAD_JOIN(cdda_scanner_id)
}


/* create toplevel Music Store node for CD Audio */
void
create_cdda_node(GtkTreeStore * store) {

	GtkTreeIter iter;

	gtk_tree_store_insert(store, &iter, NULL, 0);
	gtk_tree_store_set(store, &iter,
			   0, _("CD Audio"),
			   1, "",
			   2, "CDDA_STORE",
			   3, "",
			   8, PANGO_WEIGHT_BOLD,
			   -1);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(store, &iter, 9, icon_store, -1);
	}
}


#endif /* HAVE_CDDA */
