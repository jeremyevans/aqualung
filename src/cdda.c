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
#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>
#ifndef _WIN32
#include <pthread.h>
#endif /* !_WIN32 */

#ifdef HAVE_CDDB
#define _TMP_HAVE_CDDB 1
#undef HAVE_CDDB
#endif /* HAVE_CDDB */
#include <cdio/cdio.h>
#include <cdio/cdda.h>
#include <cdio/logging.h>
#ifdef HAVE_CDDB
#undef HAVE_CDDB
#endif /* HAVE_CDDB */
#ifdef _TMP_HAVE_CDDB
#define HAVE_CDDB 1
#undef _TMP_HAVE_CDDB
#endif /* _TMP_HAVE_CDDB */


#include "common.h"
#include "utils_gui.h"
#include "options.h"
#include "music_browser.h"
#include "playlist.h"
#include "gui_main.h"
#include "rb.h"
#include "i18n.h"
#include "cdda.h"


extern options_t options;
extern GdkPixbuf * icon_store;
extern GdkPixbuf * icon_record;
extern GdkPixbuf * icon_track;
extern GdkPixbuf * icon_cdda;
extern GdkPixbuf * icon_cdda_disc;
extern GdkPixbuf * icon_cdda_nodisc;
extern GtkTreeStore * music_store;
extern GtkWidget * browser_window;


#ifdef HAVE_CDDB
extern void cddb_get_cdda(cdda_disc_t * disc, GtkTreeIter iter_drive);
#endif /* HAVE_CDDB */

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

cdda_drive_t *
cdda_get_drive_by_spec_device_path(char * device_path) {

	if (strstr(device_path, "CDDA_DRIVE ") == device_path) {
		return cdda_get_drive_by_device_path(device_path + 11);
	}

	return NULL;
}


/* value should point to a buffer of size at least CDDA_MAXLEN */
/* ret 0 if OK, -1 if readlink error */
#ifndef _WIN32
int
symlink_deref_absolute(char * symlink, char * path) {

	int ret = readlink(symlink, path, CDDA_MAXLEN-1);
	if (ret < 0)
		return -1;
	path[ret] = '\0';
	
	if (!g_path_is_absolute(path)) {
		char tmp[CDDA_MAXLEN];
		char * dir = g_path_get_dirname(symlink);
		snprintf(tmp, CDDA_MAXLEN-1, "%s/%s", dir, path);
		strcpy(path, tmp);
		g_free(dir);
	}
	return 0;
}
#endif /* !_WIN32 */

/* return 1 if drives[i] is a symlink to a drive path
 * also present in the drives[] string array, or the
 * second, third, ... of symlinks pointing to the same
 * physical device path. Return 0 else.
 */
int
cdda_skip_extra_symlink(char ** drives, int i) {

#ifndef _WIN32
	int j;
	char path[CDDA_MAXLEN];

	if (!g_file_test(drives[i], G_FILE_TEST_IS_SYMLINK))
		return 0;

	if ((symlink_deref_absolute(drives[i], path)) != 0)
		return 1;

	/* have the destination of the symlink in our device list */
	for (j = 0; drives[j] != NULL; j++) {
		if (i == j)
			continue;
		if (strcmp(drives[j], path) == 0) {
			return 1;
		}
	}

	/* have another symlink to the same destination with a smaller index */
	for (j = 0; (j < i) && (drives[j] != NULL); j++) {
		char path2[CDDA_MAXLEN];
		if (!g_file_test(drives[j], G_FILE_TEST_IS_SYMLINK))
			continue;
		if ((symlink_deref_absolute(drives[j], path2)) != 0)
			continue;
		if (strcmp(path, path2) == 0)
			return 1;
	}
	
	return 0;
#else
	return 0;
#endif /* !_WIN32 */
}


unsigned long
calc_cdda_hash(cdda_disc_t * disc) {

	unsigned long result = 0;
	unsigned long tmp;
	unsigned long discid;
	int i;

	if (disc->n_tracks == 0) {
		return 0;
	}

	for (i = 0; i < disc->n_tracks; i++) {

		tmp = (disc->toc[i] + 150) / 75;

		do {
			result += tmp % 10;
			tmp /= 10;
		} while (tmp != 0);
	}

        discid = (result % 0xff) << 24 | 
		(disc->toc[disc->n_tracks] / 75) << 8 | 
		disc->n_tracks;

	return discid;
}


int
cdda_hash_matches(char * filename, unsigned long hash) {

	char device_path[CDDA_MAXLEN];
	unsigned long fhash;
	unsigned int track;

	if (sscanf(filename, "CDDA %s %lX %u", device_path, &fhash, &track) < 3) {
		return 0;
	}

	return fhash == hash;
}


/* return 0 if OK, -1 if drive is apparently not available */
int
cdda_scan_drive(char * device_path, cdda_drive_t * cdda_drive) {

	track_t tracks;
	track_t first_track;
	track_t last_track;
	track_t skipped = 0;
	int i, n = 0;
	cdrom_drive_t * d;

	if (cdda_drive->cdio == NULL) {
		cdda_drive->cdio = cdio_open(device_path, DRIVER_DEVICE);
		if (!cdda_drive->cdio) {
			return -1;
		}		
		strncpy(cdda_drive->device_path, device_path, CDDA_MAXLEN-1);
	}
	
	cdda_drive->disc.n_tracks = 0;
	tracks = cdio_get_num_tracks(cdda_drive->cdio);

	if (tracks < 1 || tracks >= 100) {
		cdda_drive->disc.hash_prev = cdda_drive->disc.hash;
		cdda_drive->disc.hash = 0L;
		return 0;
	}

	d = cdio_cddap_identify_cdio(cdda_drive->cdio, 0, NULL);
	if (d == NULL) {
		return -1;
	}
		
	first_track = cdio_get_first_track_num(cdda_drive->cdio);
	last_track = cdio_get_last_track_num(cdda_drive->cdio);

	for (i = first_track; i <= last_track; i++) {
		if (!cdio_cddap_track_audiop(d, i)) {
			skipped = i;
			last_track = i-1;
			break;
		}
	}

	cdio_cddap_close_no_free_cdio(d);
	
	if (last_track < first_track) {
		return 0;
	}

	cdda_drive->disc.n_tracks = last_track - first_track + 1;
	for (i = first_track; i <= last_track; i++) {
		lsn_t lsn = cdio_get_track_lsn(cdda_drive->cdio, i);
		if (lsn != CDIO_INVALID_LSN) {
			cdda_drive->disc.toc[n++] = lsn;
		}
	}
	if (skipped == 0) {
		cdda_drive->disc.toc[n] = cdio_get_track_lsn(cdda_drive->cdio, CDIO_CDROM_LEADOUT_TRACK);
	} else {
		cdda_drive->disc.toc[n] = cdio_get_track_lsn(cdda_drive->cdio, skipped);
		if ((cdda_drive->disc.toc[n] - cdda_drive->disc.toc[n-1]) >= 11400) {
			/* compensate end of track with multisession offset */
			cdda_drive->disc.toc[n] -= 11400;
		}
	}

	cdda_drive->disc.hash_prev = cdda_drive->disc.hash;
	cdda_drive->disc.hash = calc_cdda_hash(&cdda_drive->disc);

	if (cdda_drive->disc.hash != cdda_drive->disc.hash_prev) {
		strncpy(cdda_drive->disc.artist_name, _("Unknown Artist"), MAXLEN-1);
		strncpy(cdda_drive->disc.record_name, _("Unknown Record"), MAXLEN-1);

		cdda_drive->swap_bytes = -1; /* unknown */
	}

	return 0;
}


void
cdda_send_event(int event_type, char * drive) {

	cdda_notify_t notify;

	/* If there is no space for the message, it is dropped.
	 * This may happen if GUI gets blocked (unable to run cdda_timeout_callback()
	 * for several seconds), however it is not anticipated to happen very often.
	 * In such case these events are not very much of use, anyway.
	 */
	if (rb_write_space(cdda_notify_rb) >= sizeof(cdda_notify_t)) {
		notify.event_type = event_type;
		notify.device_path = strdup(drive);
		rb_write(cdda_notify_rb, (char *)&notify, sizeof(cdda_notify_t));
	}
}


void
cdda_scan_all_drives(void) {

	char ** drives = NULL;
	char touched[CDDA_DRIVES_MAX];
	int i;

	for (i = 0; i < CDDA_DRIVES_MAX; i++) {
		if (cdda_drives[i].cdio != NULL) {
			cdda_drives[i].media_changed = options.cdda_force_drive_rescan ?
				1 : cdio_get_media_changed(cdda_drives[i].cdio);
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
				if ((cdda_drives[n].disc.hash == 0L) ||
				    (cdda_drives[n].disc.hash != cdda_drives[n].disc.hash_prev)) {
					/* EVENT refresh disc data */
					cdda_send_event(CDDA_EVENT_CHANGED_DRIVE, drives[i]);
				}
			}
		} else { /* no, scan the drive */
			if (cdda_skip_extra_symlink(drives, i))
				continue;
			if (cdda_get_first_free_slot(&n) < 0) {
				printf("cdda.c: error: too many CD drives\n");
				return;
			}
			if (cdda_scan_drive(drives[i], cdda_get_drive(n)) >= 0) {
				touched[n] = 1;
				/* EVENT newly discovered drive */
				cdda_send_event(CDDA_EVENT_NEW_DRIVE, drives[i]);
			}
		}
	}
	cdio_free_device_list(drives);

	/* remove all drives that were not touched (those that disappeared) */
	for (i = 0; i < CDDA_DRIVES_MAX; i++) {
		if ((cdda_drives[i].device_path[0] != '\0') && (touched[i] == 0)) {

			/* EVENT removed drive */
			cdda_send_event(CDDA_EVENT_REMOVED_DRIVE, cdda_drives[i].device_path);

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
	update_cdda_status_bar();
	return TRUE;
}


void
cdda_timeout_start(void) {

	cdda_timeout_tag = g_timeout_add(CDDA_TIMEOUT_PERIOD, cdda_timeout_callback, NULL);
}


void
cdda_timeout_stop(void) {

	g_source_remove(cdda_timeout_tag);
}


void
cdda_scanner_start(void) {

	if (cdda_scanner_working)
		return;

	cdda_notify_rb = rb_create(CDDA_NOTIFY_RB_SIZE);
#ifdef _WIN32
	cdda_mutex = g_mutex_new();
#endif /* _WIN32 */

	cdda_timeout_start();

	cdda_scanner_working = 1;
	AQUALUNG_THREAD_CREATE(cdda_scanner_id, NULL, cdda_scanner, NULL)
}


void
cdda_scanner_stop(void) {

	cdda_scanner_working = 0;
	AQUALUNG_THREAD_JOIN(cdda_scanner_id)

	cdda_timeout_callback(NULL); /* cleanup any leftover messages */
	cdda_timeout_stop();

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


/* Returns 0 on success, 1 on failure */
int
update_track_data_from_cdtext(cdda_drive_t * drive, GtkTreeIter * iter_drive) {

	CdIo_t * cdio;
	cdtext_t * cdtext;
	GtkTreeIter iter;
	int i;
	int ret = 0;

	cdio = cdio_open(drive->device_path, DRIVER_UNKNOWN);
	cdtext = cdio_get_cdtext(cdio, 0);

	if (cdtext == NULL) {
		ret = 1;
	} else {
		char tmp[MAXLEN];

		tmp[0] = '\0';

		if (cdtext->field[CDTEXT_PERFORMER] != NULL && *(cdtext->field[CDTEXT_PERFORMER]) != '\0') {
			strncat(tmp, cdtext->field[CDTEXT_PERFORMER], MAXLEN-1);
			strncpy(drive->disc.artist_name, cdtext->field[CDTEXT_PERFORMER], MAXLEN-1);
		} else {
			ret = 1;
		}

		strncat(tmp, ": ", MAXLEN - strlen(tmp) - 1);

		if (cdtext->field[CDTEXT_TITLE] != NULL && *(cdtext->field[CDTEXT_TITLE]) != '\0') {
			strncat(tmp, cdtext->field[CDTEXT_TITLE], MAXLEN - strlen(tmp) - 1);
			strncpy(drive->disc.record_name, cdtext->field[CDTEXT_TITLE], MAXLEN-1);
		} else {
			ret = 1;
		}

		if (ret == 0)
			gtk_tree_store_set(music_store, iter_drive, 0, tmp, -1);
	}


	i = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter, iter_drive, i++)) {

		cdtext = cdio_get_cdtext(cdio, i);

		if (cdtext == NULL) {
			ret = 1;
			continue;
		}

		if (cdtext->field[CDTEXT_TITLE] != NULL && *(cdtext->field[CDTEXT_TITLE]) != '\0') {
			gtk_tree_store_set(music_store, &iter, 0, cdtext->field[CDTEXT_TITLE], -1);
		} else {
			ret = 1;
		}
	}

	cdio_destroy(cdio);

	return ret;
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
		snprintf(path, CDDA_MAXLEN-1, "CDDA %s %lX %d", drive->device_path, drive->disc.hash, i+1);
		snprintf(sort, 15, "%02d", i+1);

		gtk_tree_store_append(music_store, &iter_track, &iter_drive);
		gtk_tree_store_set(music_store, &iter_track,
				   0, title,
				   1, sort,
				   2, path,
				   3, "",
				   4, duration,
				   5, 1.0f, /* volume - unmeasured */
				   -1);

		if (options.enable_ms_tree_icons) {
			gtk_tree_store_set(music_store, &iter_track, 9, icon_track, -1);
		}
	}

	if (update_track_data_from_cdtext(drive, &iter_drive) != 0) {
#ifdef HAVE_CDDB
		cddb_get_cdda(&drive->disc, iter_drive);
#else
		if (options.cdda_add_to_playlist) {
			playlist_add_cdda(&iter_drive, drive->disc.hash);
		}
#endif /* HAVE_CDDB */
	} else if (options.cdda_add_to_playlist) {
		playlist_add_cdda(&iter_drive, drive->disc.hash);
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
		gtk_tree_store_set(music_store, &iter, 9, icon_cdda, -1);
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

	if (drive == NULL) {
		return;
	}

	if (drive->disc.n_tracks > 0) {
		snprintf(str_title, MAXLEN-1, "%s [%s]",
			 _("Unknown disc"), cdda_displayed_device_path(device_path));
	} else {
		snprintf(str_title, MAXLEN-1, "%s [%s]",
			 _("No disc"), cdda_displayed_device_path(device_path));
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
		if (drive->disc.n_tracks > 0) {
			gtk_tree_store_set(music_store, &iter_drive, 9, icon_cdda_disc, -1);
		} else {
			gtk_tree_store_set(music_store, &iter_drive, 9, icon_cdda_nodisc, -1);
		}
	}

	if (drive->disc.n_tracks > 0) {
		update_track_data(drive, iter_drive);
	}
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

	if (drive == NULL)
		return;

	if (drive->disc.n_tracks > 0) {
		snprintf(str_title, MAXLEN-1, "%s [%s]", _("Unknown disc"),
			 cdda_displayed_device_path(device_path));
	} else {
		snprintf(str_title, MAXLEN-1, "%s [%s]", _("No disc"),
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

	if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter_drive) > 0) {

		if (options.cdda_remove_from_playlist) {
			playlist_remove_cdda(device_path);
		}

		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_tmp, &iter_drive, 0)) {
			gtk_tree_store_remove(music_store, &iter_tmp);
		}
	}

	if (options.enable_ms_tree_icons) {
		if (drive->disc.n_tracks > 0) {
			gtk_tree_store_set(music_store, &iter_drive, 9, icon_cdda_disc, -1);
		} else {
			gtk_tree_store_set(music_store, &iter_drive, 9, icon_cdda_nodisc, -1);
		}
	}

	if (drive->disc.n_tracks > 0) {
		update_track_data(drive, iter_drive);
	}
}


void
cdda_info_row(char * text, int yes, GtkWidget * table, int * cnt) {

	GtkWidget * image;
	GtkWidget * label;
	GtkWidget * hbox;

	image = gtk_image_new_from_stock(yes ? GTK_STOCK_APPLY : GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON);
	label = gtk_label_new(text);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), image, 0, 1, *cnt, *cnt+1, GTK_FILL, GTK_FILL, 2, 1);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, *cnt, *cnt+1, GTK_FILL, GTK_FILL, 5, 1);
	(*cnt)++;
}


void
cdda_drive_info(char * device_path) {

	cdda_drive_t * drive = cdda_get_drive_by_device_path(device_path);
	CdIo_t * cdio;
	cdio_hwinfo_t hwinfo;
	cdio_drive_read_cap_t read_cap;
	cdio_drive_write_cap_t write_cap;
	cdio_drive_misc_cap_t misc_cap;

        GtkWidget * dialog;
	GtkWidget * label;
	GtkWidget * hbox;
	GtkWidget * vbox;
	GtkWidget * notebook;
	GtkWidget * table;
	char str[MAXLEN];

	if (!drive)
		return;

	cdio = cdio_open(device_path, DRIVER_UNKNOWN);
	if (!cdio_get_hwinfo(cdio, &hwinfo)) {
		cdio_destroy(cdio);
		return;
	}
	cdio_get_drive_cap(cdio, &read_cap, &write_cap, &misc_cap);
	cdio_destroy(cdio);

	snprintf(str, MAXLEN-1, "%s [%s]", _("Drive info"), cdda_displayed_device_path(device_path));

        dialog = gtk_dialog_new_with_buttons(str,
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT |
					     GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
					     NULL);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox, FALSE, FALSE, 4);

	table = gtk_table_new(4, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 2);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Device path:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 4, 1);
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(cdda_displayed_device_path(device_path));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 4, 1);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Vendor:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 4, 1);
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(hwinfo.psz_vendor);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 4, 1);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Model:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 4, 1);
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(hwinfo.psz_model);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 2, 3, GTK_FILL, GTK_FILL, 4, 1);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Revision:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 4, 1);
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(hwinfo.psz_revision);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 3, 4, GTK_FILL, GTK_FILL, 4, 1);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("The information below is reported by the drive, and\n"
				"may not reflect the actual capabilities of the device."));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 10);

	if ((misc_cap == CDIO_DRIVE_CAP_ERROR) &&
	    (read_cap == CDIO_DRIVE_CAP_ERROR) &&
	    (write_cap == CDIO_DRIVE_CAP_ERROR)) {

		goto cdda_info_finish;
	}

	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), notebook, TRUE, TRUE, 0);

	if (misc_cap != CDIO_DRIVE_CAP_ERROR) {
		int cnt = 0;
		label = gtk_label_new(_("General"));
		table = gtk_table_new(8, 2, FALSE);
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table, label);

		cdda_info_row(_("Eject"), misc_cap & CDIO_DRIVE_CAP_MISC_EJECT, table, &cnt);
		cdda_info_row(_("Close tray"), misc_cap & CDIO_DRIVE_CAP_MISC_CLOSE_TRAY, table, &cnt);
		cdda_info_row(_("Disable manual eject"), misc_cap & CDIO_DRIVE_CAP_MISC_LOCK, table, &cnt);
		cdda_info_row(_("Select juke-box disc"), misc_cap & CDIO_DRIVE_CAP_MISC_SELECT_DISC, table, &cnt);
		cdda_info_row(_("Set drive speed"), misc_cap & CDIO_DRIVE_CAP_MISC_SELECT_SPEED, table, &cnt);
		cdda_info_row(_("Detect media change"), misc_cap & CDIO_DRIVE_CAP_MISC_MEDIA_CHANGED, table, &cnt);
		cdda_info_row(_("Read multiple sessions"), misc_cap & CDIO_DRIVE_CAP_MISC_MULTI_SESSION, table, &cnt);
		cdda_info_row(_("Hard reset device"), misc_cap & CDIO_DRIVE_CAP_MISC_RESET, table, &cnt);
	}

	if (read_cap != CDIO_DRIVE_CAP_ERROR) {
		int cnt = 0;
		label = gtk_label_new(_("Reading"));
		table = gtk_table_new(16, 2, FALSE);
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table, label);

		cdda_info_row(_("Play CD Audio"), read_cap & CDIO_DRIVE_CAP_READ_AUDIO, table, &cnt);
		cdda_info_row(_("Read CD-DA"), read_cap & CDIO_DRIVE_CAP_READ_CD_DA, table, &cnt);
		cdda_info_row(_("Read CD+G"), read_cap & CDIO_DRIVE_CAP_READ_CD_G, table, &cnt);
		cdda_info_row(_("Read CD-R"), read_cap & CDIO_DRIVE_CAP_READ_CD_R, table, &cnt);
		cdda_info_row(_("Read CD-RW"), read_cap & CDIO_DRIVE_CAP_READ_CD_RW, table, &cnt);
		cdda_info_row(_("Read DVD-R"), read_cap & CDIO_DRIVE_CAP_READ_DVD_R, table, &cnt);
		cdda_info_row(_("Read DVD+R"), read_cap & CDIO_DRIVE_CAP_READ_DVD_PR, table, &cnt);
		cdda_info_row(_("Read DVD-RW"), read_cap & CDIO_DRIVE_CAP_READ_DVD_RW, table, &cnt);
		cdda_info_row(_("Read DVD+RW"), read_cap & CDIO_DRIVE_CAP_READ_DVD_RPW, table, &cnt);
		cdda_info_row(_("Read DVD-RAM"), read_cap & CDIO_DRIVE_CAP_READ_DVD_RAM, table, &cnt);
		cdda_info_row(_("Read DVD-ROM"), read_cap & CDIO_DRIVE_CAP_READ_DVD_ROM, table, &cnt);
		cdda_info_row(_("C2 Error Correction"), read_cap & CDIO_DRIVE_CAP_READ_C2_ERRS, table, &cnt);
		cdda_info_row(_("Read Mode 2 Form 1"), read_cap & CDIO_DRIVE_CAP_READ_MODE2_FORM1, table, &cnt);
		cdda_info_row(_("Read Mode 2 Form 2"), read_cap & CDIO_DRIVE_CAP_READ_MODE2_FORM2, table, &cnt);
		cdda_info_row(_("Read MCN"), read_cap & CDIO_DRIVE_CAP_READ_MCN, table, &cnt);
		cdda_info_row(_("Read ISRC"), read_cap & CDIO_DRIVE_CAP_READ_ISRC, table, &cnt);
	}

	if (write_cap != CDIO_DRIVE_CAP_ERROR) {
		int cnt = 0;
		label = gtk_label_new(_("Writing"));
		table = gtk_table_new(9, 2, FALSE);
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table, label);

		cdda_info_row(_("Write CD-R"), write_cap & CDIO_DRIVE_CAP_WRITE_CD_R, table, &cnt);
		cdda_info_row(_("Write CD-RW"), write_cap & CDIO_DRIVE_CAP_WRITE_CD_RW, table, &cnt);
		cdda_info_row(_("Write DVD-R"), write_cap & CDIO_DRIVE_CAP_WRITE_DVD_R, table, &cnt);
		cdda_info_row(_("Write DVD+R"), write_cap & CDIO_DRIVE_CAP_WRITE_DVD_PR, table, &cnt);
		cdda_info_row(_("Write DVD-RW"), write_cap & CDIO_DRIVE_CAP_WRITE_DVD_RW, table, &cnt);
		cdda_info_row(_("Write DVD+RW"), write_cap & CDIO_DRIVE_CAP_WRITE_DVD_RPW, table, &cnt);
		cdda_info_row(_("Write DVD-RAM"), write_cap & CDIO_DRIVE_CAP_WRITE_DVD_RAM, table, &cnt);
		cdda_info_row(_("Mount Rainier"), write_cap & CDIO_DRIVE_CAP_WRITE_MT_RAINIER, table, &cnt);
		cdda_info_row(_("Burn Proof"), write_cap & CDIO_DRIVE_CAP_WRITE_BURN_PROOF, table, &cnt);
	}

 cdda_info_finish:
	gtk_widget_show_all(dialog);
        aqualung_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


void
cdda_disc_info(char * device_path) {

	CdIo_t * cdio;
	cdtext_t * cdtext;
	track_t itrack;
	track_t ntracks;
	track_t track_last;
	int i;

        GtkWidget * dialog;
	GtkWidget * table;
	GtkWidget * label;
	GtkWidget * vbox;
	GtkWidget * hbox;

	GtkListStore * list;
	GtkWidget * view;
	GtkWidget * viewport;
	GtkWidget * scrolled_win;
	GtkCellRenderer * cell;

	GType types[MAX_CDTEXT_FIELDS + 1];
	GtkTreeViewColumn * columns[MAX_CDTEXT_FIELDS + 1];
	int visible[MAX_CDTEXT_FIELDS + 1];
	int has_some_cdtext = 0;


        dialog = gtk_dialog_new_with_buttons(_("Disc info"),
					     GTK_WINDOW(browser_window),
					     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT |
					     GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
					     NULL);

	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_widget_set_size_request(GTK_WIDGET(dialog), 500, 400);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox, TRUE, TRUE, 4);


	cdio = cdio_open(device_path, DRIVER_UNKNOWN);
	cdtext = cdio_get_cdtext(cdio, 0);

	if (cdtext != NULL) {
		table = gtk_table_new(MAX_CDTEXT_FIELDS, 2, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 2);


		for (i = 0; i < MAX_CDTEXT_FIELDS; i++) {

			if (cdtext->field[i] == NULL || *(cdtext->field[i]) == '\0') {
				continue;
			}

			has_some_cdtext = 1;

			hbox = gtk_hbox_new(FALSE, 0);
			label = gtk_label_new(cdtext_field2str(i));
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
			gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, i, i + 1, GTK_FILL, GTK_FILL, 4, 1);

			hbox = gtk_hbox_new(FALSE, 0);
			label = gtk_label_new(cdtext->field[i]);
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
			gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, i, i + 1, GTK_FILL, GTK_FILL, 4, 1);
		}
	}


	types[0] = G_TYPE_INT;
	for (i = 1; i <= MAX_CDTEXT_FIELDS; i++) {
		types[i] = G_TYPE_STRING;
		visible[i] = 0;
	}

	list = gtk_list_store_newv(MAX_CDTEXT_FIELDS + 1,
				   types);

        view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list));
        viewport = gtk_viewport_new(NULL, NULL);
        scrolled_win = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
                                       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), viewport, TRUE, TRUE, 2);
        gtk_container_add(GTK_CONTAINER(viewport), scrolled_win);
        gtk_container_add(GTK_CONTAINER(scrolled_win), view);

        cell = gtk_cell_renderer_text_new();
	g_object_set((gpointer)cell, "xalign", 1.0, NULL);
        columns[0] = gtk_tree_view_column_new_with_attributes(_("No."), cell, "text", 0, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(view), GTK_TREE_VIEW_COLUMN(columns[0]));

	for (i = 0; i < MAX_CDTEXT_FIELDS; i++) {

		cell = gtk_cell_renderer_text_new();
		columns[i + 1] = gtk_tree_view_column_new_with_attributes(cdtext_field2str(i),
									  cell, "text", i + 1, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), GTK_TREE_VIEW_COLUMN(columns[i + 1]));
	}

	itrack = cdio_get_first_track_num(cdio);
	ntracks = cdio_get_num_tracks(cdio);
	track_last = itrack + ntracks;

	for (; itrack < track_last; itrack++) {

		GtkTreeIter iter;

		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter, 0, itrack, -1);

		cdtext = cdio_get_cdtext(cdio, itrack);

		if (cdtext == NULL) {
			continue;
		}

		for (i = 0; i < MAX_CDTEXT_FIELDS; i++) {

			if (cdtext->field[i] != NULL && *(cdtext->field[i]) != '\0') {
				gtk_list_store_set(list, &iter, i + 1, cdtext->field[i], -1);
				visible[i + 1] = 1;
				has_some_cdtext = 1;
			}
		}
	}

	for (i = 1; i <= MAX_CDTEXT_FIELDS; i++) {
		gtk_tree_view_column_set_visible(columns[i], visible[i]);
	}

	cdio_destroy(cdio);

	gtk_widget_show_all(dialog);

	if (has_some_cdtext) {
		aqualung_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	} else {
		gtk_widget_destroy(dialog);

		message_dialog(_("Disc info"),
			       browser_window,
			       GTK_MESSAGE_INFO,
			       GTK_BUTTONS_OK,
			       NULL,
			       _("This CD does not contain CD-Text information."));
	}
}


#endif /* HAVE_CDDA */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

