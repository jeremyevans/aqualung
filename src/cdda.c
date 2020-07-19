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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

#undef HAVE_CDDB
#include "undef_ac_pkg.h"
#include <cdio/cdio.h>
#include <cdio/logging.h>
#ifdef HAVE_CDIO_PARANOIA_CDDA_H
#include <cdio/paranoia/cdda.h>
#else /* !HAVE_CDIO_PARANOIA_CDDA_H (assume older, bundled, layout) */
#include <cdio/cdda.h>
#endif /* !HAVE_CDIO_PARANOIA_CDDA_H */
#undef HAVE_CDDB
#include "undef_ac_pkg.h"
#include <config.h>	/* re-establish undefined autoconf macros */

#include "athread.h"
#include "common.h"
#include "utils_gui.h"
#include "options.h"
#include "music_browser.h"
#include "rb.h"
#include "i18n.h"
#include "store_cdda.h"
#include "cdda.h"


#if CDIO_API_VERSION < 6
#define CDTEXT_FIELD_PERFORMER CDTEXT_PERFORMER
#define CDTEXT_FIELD_TITLE     CDTEXT_TITLE
#endif /* CDIO_API_VERSION < 6 */


extern options_t options;
extern GdkPixbuf * icon_track;
extern GdkPixbuf * icon_cdda_disc;
extern GdkPixbuf * icon_cdda_nodisc;
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


gchar *
cdda_get_cdtext(CdIo_t * cdio, cdtext_field_t field, track_t track) {

	const gchar * value;
	cdtext_t * cdtext;

	if (cdio == NULL)
		return NULL;

#if CDIO_API_VERSION >= 6
	if (NULL == (cdtext = cdio_get_cdtext(cdio)))
		return NULL;

	value = cdtext_get_const(cdtext, field, track);
#else /* CDIO_API_VERSION < 6 */
	if (NULL == (cdtext = cdio_get_cdtext(cdio, track)))
		return NULL;

	value = cdtext_get_const(field, cdtext);
#endif /* CDIO_API_VERSION < 6 */
	if (value && !g_utf8_validate(value, -1, NULL)) {
		g_debug("cdda_get_cdtext: invalid UTF-8 in %s field, track %d",
			cdtext_field2str(field), track);
		return NULL;
	}

	return g_strdup(value);
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
	
	if (n >= 0 && n < CDDA_DRIVES_MAX) {
		return &cdda_drives[n];
	} else {
		return NULL;
	}
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
		arr_snprintf(tmp, "%s/%s", dir, path);
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
		g_usleep(100000);

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
cdda_timeout_start(void) {

	cdda_timeout_tag = aqualung_timeout_add(CDDA_TIMEOUT_PERIOD, cdda_timeout_callback, NULL);
}


void
cdda_timeout_stop(void) {

	g_source_remove(cdda_timeout_tag);
}


void
cdda_scanner_start(void) {

	if (cdda_scanner_working) {
		return;
	}

	cdda_notify_rb = rb_create(CDDA_NOTIFY_RB_SIZE);
#ifndef HAVE_LIBPTHREAD
	cdda_mutex = g_mutex_new();
#endif /* !HAVE_LIBPTHREAD */

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

#ifndef HAVE_LIBPTHREAD
	g_mutex_free(cdda_mutex);
#endif /* !HAVE_LIBPTHREAD */

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
	gchar * title;
	GtkTreeIter iter;
	track_t i = 0;
	int ret = 0;

	cdio = cdio_open(drive->device_path, DRIVER_UNKNOWN);
	if (cdio == NULL) {
		return 1;
	} else {
		gchar * performer, * disc_name;

		performer = cdda_get_cdtext(cdio, CDTEXT_FIELD_PERFORMER, i);
		if (performer && *performer != '\0') {
			g_strlcpy(drive->disc.artist_name, performer, MAXLEN);
		} else {
			ret = 1;
		}

		title = cdda_get_cdtext(cdio, CDTEXT_FIELD_TITLE, i);
		if (title && *title != '\0') {
			g_strlcpy(drive->disc.record_name, title, MAXLEN);
		} else {
			ret = 1;
		}

		if (ret == 0) {
			disc_name = g_strconcat(performer, ": ", title, NULL);
			gtk_tree_store_set(music_store, iter_drive,
					   MS_COL_NAME, disc_name, -1);
			g_free(disc_name);
		}
		g_free(performer);
		g_free(title);
	}

	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter, iter_drive, i++)) {

		title = cdda_get_cdtext(cdio, CDTEXT_FIELD_TITLE, i);

		if (title && *title != '\0') {
			gtk_tree_store_set(music_store, &iter,
					   MS_COL_NAME, title, -1);
		} else {
			ret = 1;
		}
		g_free(title);
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

		cdda_track_t * data;

		arr_snprintf(title, "Track %d", i+1);
		arr_snprintf(path, "CDDA %s %lX %d", drive->device_path, drive->disc.hash, i+1);
		arr_snprintf(sort, "%02d", i+1);

		if ((data = (cdda_track_t *)malloc(sizeof(cdda_track_t))) == NULL) {
			fprintf(stderr, "update_track_data: malloc error\n");
			return;
		}

		data->path = strdup(path);
		data->duration = (drive->disc.toc[i+1] - drive->disc.toc[i]) / 75.0;

		gtk_tree_store_append(music_store, &iter_track, &iter_drive);
		gtk_tree_store_set(music_store, &iter_track,
				   MS_COL_NAME, title,
				   MS_COL_SORT, sort,
				   MS_COL_DATA, data, -1);

		if (options.enable_ms_tree_icons) {
			gtk_tree_store_set(music_store, &iter_track, MS_COL_ICON, icon_track, -1);
		}
	}

	if (update_track_data_from_cdtext(drive, &iter_drive) != 0) {
#ifdef HAVE_CDDB
		cdda_record_auto_query_cddb(&iter_drive);
#else
		if (options.cdda_add_to_playlist) {
			cdda_add_to_playlist(&iter_drive, drive->disc.hash);
		}
#endif /* HAVE_CDDB */
	} else if (options.cdda_add_to_playlist) {
		cdda_add_to_playlist(&iter_drive, drive->disc.hash);
	}
}


void
insert_cdda_drive_node(char * device_path) {

	GtkTreeIter iter_cdda;
	GtkTreeIter iter_drive;
	cdda_drive_t * drive = cdda_get_drive_by_device_path(device_path);
	char str_title[MAXLEN];
	char str_sort[16];

	if (drive == NULL) {
		return;
	}

	if (drive->disc.n_tracks > 0) {
		arr_snprintf(str_title, "%s [%s]",
			     _("Unknown disc"), cdda_displayed_device_path(device_path));
	} else {
		arr_snprintf(str_title, "%s [%s]",
			     _("No disc"), cdda_displayed_device_path(device_path));
	}

	arr_snprintf(str_sort, "%d", cdda_get_n(device_path));

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(music_store), &iter_cdda);

	gtk_tree_store_append(music_store, &iter_drive, &iter_cdda);
	gtk_tree_store_set(music_store, &iter_drive,
			   MS_COL_NAME, str_title,
			   MS_COL_SORT, str_sort,
			   MS_COL_DATA, drive, -1);

	if (options.enable_ms_tree_icons) {
		if (drive->disc.n_tracks > 0) {
			gtk_tree_store_set(music_store, &iter_drive, MS_COL_ICON, icon_cdda_disc, -1);
		} else {
			gtk_tree_store_set(music_store, &iter_drive, MS_COL_ICON, icon_cdda_nodisc, -1);
		}
	}

	if (drive->disc.n_tracks > 0) {
		update_track_data(drive, iter_drive);
	}

	music_store_selection_changed(STORE_TYPE_CDDA);
}

void
remove_cdda_drive_node(char * device_path) {

	GtkTreeIter iter_cdda;
	GtkTreeIter iter_drive;
	cdda_drive_t * drive;
	int i, found;

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(music_store), &iter_cdda);

	i = found = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_drive, &iter_cdda, i++)) {
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_drive, MS_COL_DATA, &drive, -1);
		if (strcmp(drive->device_path, device_path) == 0) {
			found = 1;
			break;
		}
	}

	if (found) {
		store_cdda_remove_record(&iter_drive);
	}

	music_store_selection_changed(STORE_TYPE_CDDA);
}


void
refresh_cdda_drive_node(char * device_path) {

	GtkTreeIter iter_cdda;
	GtkTreeIter iter_drive;
	GtkTreeIter iter_tmp;
	cdda_drive_t * drive;
	char str_title[MAXLEN];
	int i, found;

	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(music_store), &iter_cdda);

	i = found = 0;
	while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_drive, &iter_cdda, i++)) {
		gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_drive, MS_COL_DATA, &drive, -1);
		if (strcmp(drive->device_path, device_path) == 0) {
			found = 1;
			break;
		}
	}

	if (!found) {
		return;
	}

	if (drive->disc.n_tracks > 0) {
		arr_snprintf(str_title, "%s [%s]", _("Unknown disc"),
			     cdda_displayed_device_path(device_path));
	} else {
		arr_snprintf(str_title, "%s [%s]", _("No disc"),
			     cdda_displayed_device_path(device_path));
	}

	gtk_tree_store_set(music_store, &iter_drive, MS_COL_NAME, str_title, -1);

	if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter_drive) > 0) {

		if (options.cdda_remove_from_playlist) {
			cdda_remove_from_playlist(drive);
		}

		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_tmp, &iter_drive, 0)) {
			store_cdda_remove_track(&iter_tmp);
		}
	}

	if (options.enable_ms_tree_icons) {
		if (drive->disc.n_tracks > 0) {
			gtk_tree_store_set(music_store, &iter_drive, MS_COL_ICON, icon_cdda_disc, -1);
		} else {
			gtk_tree_store_set(music_store, &iter_drive, MS_COL_ICON, icon_cdda_nodisc, -1);
		}
	}

	if (drive->disc.n_tracks > 0) {
		update_track_data(drive, iter_drive);
	}

	music_store_selection_changed(STORE_TYPE_CDDA);
}

void
cdda_shutdown(void) {

	if (options.cdda_remove_from_playlist) {

		GtkTreeIter iter_cdda;
		GtkTreeIter iter_drive;
		cdda_drive_t * drive;
		int i = 0;

		gtk_tree_model_get_iter_first(GTK_TREE_MODEL(music_store), &iter_cdda);

		while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(music_store), &iter_drive, &iter_cdda, i++)) {

			gtk_tree_model_get(GTK_TREE_MODEL(music_store), &iter_drive, MS_COL_DATA, &drive, -1);

			if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_store), &iter_drive) > 0) {
				cdda_remove_from_playlist(drive);
			}
		}
	}

	cdda_scanner_stop();
}


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
