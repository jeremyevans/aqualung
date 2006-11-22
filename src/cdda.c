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
#include <stdlib.h>
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
#include "rb.h"
#include "i18n.h"
#include "cdda.h"


extern options_t options;
extern GdkPixbuf * icon_store;
extern GdkPixbuf * icon_record;
extern GdkPixbuf * icon_track;
extern GtkTreeStore * music_store;

typedef struct {
	int event_type;
	char * device_path;
} cdda_notify_t;

#define CDDA_NOTIFY_RB_SIZE (16*sizeof(cdda_notify_t))
#define CDDA_TIMEOUT_PERIOD 500

#define CDDA_EVENT_NEW_DRIVE     1
#define CDDA_EVENT_REMOVED_DRIVE 2
#define CDDA_EVENT_CHANGED_DRIVE 3

cdda_drive_t cdda_drives[CDDA_DRIVES_MAX];
AQUALUNG_THREAD_DECLARE(cdda_scanner_id)
AQUALUNG_MUTEX_DECLARE(cdda_mutex)
int cdda_scanner_working;
guint cdda_timeout_tag;
rb_t * cdda_notify_rb;


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
	cdda_notify_t notify;
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
				/* EVENT refresh disc data */
				notify.event_type = CDDA_EVENT_CHANGED_DRIVE;
				notify.device_path = strdup(drives[i]);
				rb_write(cdda_notify_rb, (char *)&notify, sizeof(cdda_notify_t));
			}
		} else { /* no, scan the drive */
			if (cdda_get_first_free_slot(&n) < 0) {
				printf("cdda.c: error: too many CD drives\n");
				return;
			}
			if (cdda_scan_drive(drives[i], cdda_get_drive(n)) >= 0) {
				touched[n] = 1;
				/* EVENT newly discovered drive */
				notify.event_type = CDDA_EVENT_NEW_DRIVE;
				notify.device_path = strdup(drives[i]);
				rb_write(cdda_notify_rb, (char *)&notify, sizeof(cdda_notify_t));
			}
		}
	}
	cdio_free_device_list(drives);

	/* remove all drives that were not touched (those that disappeared) */
	for (i = 0; i < CDDA_DRIVES_MAX; i++) {
		if ((cdda_drives[i].device_path[0] != '\0') && (touched[i] == 0)) {

			/* EVENT removed drive */
			notify.event_type = CDDA_EVENT_REMOVED_DRIVE;
			notify.device_path = strdup(cdda_drives[i].device_path);
			rb_write(cdda_notify_rb, (char *)&notify, sizeof(cdda_notify_t));

			cdio_destroy(cdda_drives[i].cdio);
			memset(cdda_drives + i, 0, sizeof(cdda_drive_t));
		}
	}
}


void *
cdda_scanner(void * arg) {

	int i = 0;

	cdio_log_set_handler(cdda_log_handler);
	AQUALUNG_MUTEX_LOCK(cdda_mutex)
	cdda_scan_all_drives();
	AQUALUNG_MUTEX_UNLOCK(cdda_mutex)

	while (cdda_scanner_working) {

		struct timespec req_time;
		struct timespec rem_time;
		req_time.tv_sec = 0;
		req_time.tv_nsec = 100000000;

		nanosleep(&req_time, &rem_time);

		if (i == 50) { /* scan every 5 seconds */
			AQUALUNG_MUTEX_LOCK(cdda_mutex)
			cdda_scan_all_drives();
			AQUALUNG_MUTEX_UNLOCK(cdda_mutex)
			i = 0;
		}
		++i;
	}

	AQUALUNG_MUTEX_LOCK(cdda_mutex)
	for (i = 0; i < CDDA_DRIVES_MAX; i++) {
		if (cdda_drives[i].cdio != NULL) {
			cdio_destroy(cdda_drives[i].cdio);
			memset(cdda_drives + i, 0, sizeof(cdda_drive_t));
		}
	}
	AQUALUNG_MUTEX_UNLOCK(cdda_mutex)

	return NULL;
}


gint
cdda_timeout_callback(gpointer data) {

	cdda_notify_t notify;

	while (rb_read_space(cdda_notify_rb) >= sizeof(cdda_notify_t)) {
		rb_read(cdda_notify_rb, (char *)&notify, sizeof(cdda_notify_t));
		switch (notify.event_type) {
		case CDDA_EVENT_NEW_DRIVE:
			AQUALUNG_MUTEX_LOCK(cdda_mutex)
			insert_cdda_drive_node(notify.device_path);
			AQUALUNG_MUTEX_UNLOCK(cdda_mutex)
			free(notify.device_path);
			break;
		case CDDA_EVENT_CHANGED_DRIVE:
			AQUALUNG_MUTEX_LOCK(cdda_mutex)
			refresh_cdda_drive_node(notify.device_path);
			AQUALUNG_MUTEX_UNLOCK(cdda_mutex)
			free(notify.device_path);
			break;
		case CDDA_EVENT_REMOVED_DRIVE:
			AQUALUNG_MUTEX_LOCK(cdda_mutex)
			remove_cdda_drive_node(notify.device_path);
			AQUALUNG_MUTEX_UNLOCK(cdda_mutex)
			free(notify.device_path);
			break;
		}
	}
	return TRUE;
}


void
cdda_scanner_start(void) {

	if (cdda_scanner_working)
		return;

	cdda_notify_rb = rb_create(CDDA_NOTIFY_RB_SIZE);
#ifdef _WIN32
	cdda_mutex = g_mutex_new();
#endif /* _WIN32 */

	cdda_timeout_tag = g_timeout_add(CDDA_TIMEOUT_PERIOD, cdda_timeout_callback, NULL);

	cdda_scanner_working = 1;
	AQUALUNG_THREAD_CREATE(cdda_scanner_id, NULL, cdda_scanner, NULL)
}


void
cdda_scanner_stop(void) {

	cdda_scanner_working = 0;
	AQUALUNG_THREAD_JOIN(cdda_scanner_id)

	cdda_timeout_callback(NULL); /* cleanup any leftover messages */
	g_source_remove(cdda_timeout_tag);

#ifdef _WIN32
	g_mutex_free(cdda_mutex);
#endif /* _WIN32 */

	rb_free(cdda_notify_rb);
}


/* For some odd reason, drive names on Win32 begin with "\\.\" so eg. D: has a
 * device_path of "\\.\D:" This function gets rid of that. Returns a pointer to
 * the same buffer that is passed in (no mem allocated).
 */
char *
cdda_displayed_device_path(char * device_path) {
#ifdef _WIN32
	if (strstr(device_path, "\\\\.\\") == device_path) {
		return device_path + 4;
	} else {
		return device_path;
	}
#else
	return device_path;
#endif /* _WIN32 */
}


void
update_track_data(cdda_drive_t * drive, GtkTreeIter iter_drive) {

	int i;
	for (i = 0; i < drive->disc.n_tracks; i++) {
		
		GtkTreeIter iter_track;
		char title[MAXLEN];
		char path[CDDA_MAXLEN];
		char sort[16];
		float duration = (drive->disc.toc[i+1] - drive->disc.toc[i]) / 75.0;
		
		snprintf(title, MAXLEN-1, "Track %d", i+1);
		snprintf(path, CDDA_MAXLEN-1, "CDDA %s %d", drive->device_path, i);
		snprintf(sort, 15, "%d", i);
		
		gtk_tree_store_append(music_store, &iter_track, &iter_drive);
		gtk_tree_store_set(music_store, &iter_track,
				   0, title,
				   1, sort,
				   2, path,
				   3, "",
				   4, duration,
				   -1);

		if (options.enable_ms_tree_icons) {
			gtk_tree_store_set(music_store, &iter_track, 9, icon_track, -1);
		}
	}
}


/* create toplevel Music Store node for CD Audio */
void
create_cdda_node(void) {

	GtkTreeIter iter;

	gtk_tree_store_insert(music_store, &iter, NULL, 0);
	gtk_tree_store_set(music_store, &iter,
			   0, _("CD Audio"),
			   1, "",
			   2, "CDDA_STORE",
			   3, "",
			   8, PANGO_WEIGHT_BOLD,
			   -1);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &iter, 9, icon_store, -1);
	}
}


void
insert_cdda_drive_node(char * device_path) {

	GtkTreeIter iter_cdda;
	GtkTreeIter iter_drive;
	cdda_drive_t * drive = cdda_get_drive_by_device_path(device_path);
	char str_title[MAXLEN];
	char str_path[CDDA_MAXLEN];
	char str_sort[16];

	if (drive->disc.n_tracks > 0) {
		snprintf(str_title, MAXLEN-1, "Unknown disc [%s]",
			 cdda_displayed_device_path(device_path));
	} else {
		snprintf(str_title, MAXLEN-1, "No disc [%s]",
			 cdda_displayed_device_path(device_path));
	}

	snprintf(str_path, CDDA_MAXLEN-1, "CDDA_DRIVE %s", device_path);
	snprintf(str_sort, 15, "%d", cdda_get_n(device_path));

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(music_store), &iter_cdda);

	gtk_tree_store_append(music_store, &iter_drive, &iter_cdda);
	gtk_tree_store_set(music_store, &iter_drive,
			   0, str_title,
			   1, str_sort,
			   2, str_path,
			   3, "",
			   -1);

	if (options.enable_ms_tree_icons) {
		gtk_tree_store_set(music_store, &iter_drive, 9, icon_record, -1);
	}

	if (drive->disc.n_tracks > 0) {
		update_track_data(drive, iter_drive);
	}

	printf("discovered drive %s\n", device_path);
}


void
remove_cdda_drive_node(char * device_path) {

	GtkTreeIter iter_cdda;
	GtkTreeIter iter_drive;
	char str_path[CDDA_MAXLEN];
	int i, found;

	snprintf(str_path, CDDA_MAXLEN-1, "CDDA_DRIVE %s", device_path);

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(music_store), &iter_cdda);

	i = found = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_drive, &iter_cdda, i++)) {
		gchar * tmp;
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_drive, 2, &tmp, -1);
		if (strcmp(str_path, tmp) == 0) {
			found = 1;
			g_free(tmp);
			break;
		}
		g_free(tmp);
	}

	if (!found)
		return;

	gtk_tree_store_remove(music_store, &iter_drive);
	printf("drive %s does not exist anymore\n", device_path);
}


void
refresh_cdda_drive_node(char * device_path) {

	GtkTreeIter iter_cdda;
	GtkTreeIter iter_drive;
	GtkTreeIter iter_tmp;
	cdda_drive_t * drive = cdda_get_drive_by_device_path(device_path);
	char str_title[MAXLEN];
	char str_path[CDDA_MAXLEN];
	int i, found;

	if (drive->disc.n_tracks > 0) {
		snprintf(str_title, MAXLEN-1, "Unknown disc [%s]",
			 cdda_displayed_device_path(device_path));
	} else {
		snprintf(str_title, MAXLEN-1, "No disc [%s]",
			 cdda_displayed_device_path(device_path));
	}

	snprintf(str_path, CDDA_MAXLEN-1, "CDDA_DRIVE %s", device_path);

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(music_store), &iter_cdda);

	i = found = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_drive, &iter_cdda, i++)) {
		gchar * tmp;
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_drive, 2, &tmp, -1);
		if (strcmp(str_path, tmp) == 0) {
			found = 1;
			g_free(tmp);
			break;
		}
		g_free(tmp);
	}

	if (!found)
		return;

	gtk_tree_store_set(music_store, &iter_drive, 0, str_title, -1);

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_tmp, &iter_drive, 0)) {
		gtk_tree_store_remove(music_store, &iter_tmp);
	}

	if (drive->disc.n_tracks > 0) {
		update_track_data(drive, iter_drive);
	}

	printf("changed drive %s\n", device_path);
}


#endif /* HAVE_CDDA */
