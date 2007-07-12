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
#include <gtk/gtk.h>
#include <math.h>

#ifdef _WIN32
#include <glib.h>
#else
#include <pthread.h>
#endif /* _WIN32 */

#include "common.h"
#include "utils_gui.h"
#include "core.h"
#include "decoder/file_decoder.h"
#include "options.h"
#include "music_browser.h"
#include "playlist.h"
#include "i18n.h"
#include "volume.h"


#define EPSILON 0.00000000001

extern options_t options;

extern GtkTreeStore * music_store;

GtkWidget * vol_window;
int vol_slot_count;


void
voladj2str(float voladj, char * str) {

	if (fabs(voladj) < 0.05f) {
		sprintf(str, " %.1f dB", 0.0f);
	} else {
		sprintf(str, "% .1f dB", voladj);
	}
}


volume_t *
volume_new(GtkTreeStore * store, int type) {

	volume_t * vol = NULL;

	if ((vol = (volume_t *)calloc(1, sizeof(volume_t))) == NULL) {
		fprintf(stderr, "volume_new(): calloc error\n");
		return NULL;
	}

	vol->store = store;
	vol->type = type;

	AQUALUNG_COND_INIT(vol->thread_wait);

#ifdef _WIN32
	vol->thread_mutex = g_mutex_new();
	vol->wait_mutex = g_mutex_new();
	vol->thread_wait = g_cond_new();
#endif

	return vol;
}

void
volume_push(volume_t * vol, char * file, GtkTreeIter iter) {

	vol_item_t * item = NULL;

	if ((item = (vol_item_t *)calloc(1, sizeof(vol_item_t))) == NULL) {
		fprintf(stderr, "volume_push(): calloc error\n");
		return;
	}

	item->file = strdup(file);
	item->iter = iter;

	vol->queue = g_list_append(vol->queue, item);
}

void
vol_item_free(vol_item_t * item) {

	free(item->file);
	free(item);
}

void
volume_free(volume_t * vol) {

#ifdef _WIN32
	g_mutex_free(vol->thread_mutex);
	g_mutex_free(vol->wait_mutex);
	g_cond_free(vol->thread_wait);
#endif

	if (vol->volumes != NULL) {
		free(vol->volumes);
	}

	g_list_free(vol->queue);
	free(vol);
}


inline static float
rms_env_process(rms_env_t * r, const float x) {

        r->sum -= r->buffer[r->pos];
        r->sum += x;
        r->buffer[r->pos] = x;

	r->pos++;
        if (r->pos == RMSSIZE) {
		r->pos = 0;
	}

        return sqrt(r->sum / (float)RMSSIZE);
}

gboolean
vol_window_event(GtkWidget * widget, GdkEvent * event, gpointer * data) {

	if (event->type == GDK_DELETE) {
		gtk_window_iconify(GTK_WINDOW(vol_window));
		return TRUE;
	}

	return FALSE;
}

gboolean
volume_finalize(gpointer data) {

	volume_t * vol = (volume_t *)data;

	gtk_window_resize(GTK_WINDOW(vol_window),
			  vol_window->allocation.width,
			  vol_window->allocation.height - vol->slot->allocation.height);

	gtk_widget_destroy(vol->slot);
	vol->slot = NULL;

	g_source_remove(vol->update_tag);
	volume_free(vol);

	--vol_slot_count;

	if (vol_slot_count == 0) {
		gtk_widget_destroy(vol_window);
		vol_window = NULL;
	}

	return FALSE;
}

void
vol_pause_event(GtkButton * button, gpointer data) {

	volume_t * vol = (volume_t *)data;

        vol->paused = !vol->paused;
	if (vol->paused) {
		gtk_button_set_label(button, _("Resume"));
	} else {
		gtk_button_set_label(button, _("Pause"));
	}
}

void
vol_cancel_event(GtkButton * button, gpointer data) {

	volume_t * vol = (volume_t *)data;

        vol->cancelled = 1;
}


gboolean
vol_set_filename_text(gpointer data) {

	volume_t * vol = (volume_t *)data;

	AQUALUNG_MUTEX_LOCK(vol->wait_mutex);

	if (vol->slot) {

		char * utf8;

		AQUALUNG_MUTEX_LOCK(vol->thread_mutex);
		utf8 = g_filename_display_name(vol->item->file);
		AQUALUNG_MUTEX_UNLOCK(vol->thread_mutex);

		gtk_entry_set_text(GTK_ENTRY(vol->file_entry), utf8);
		gtk_editable_set_position(GTK_EDITABLE(vol->file_entry), -1);

		g_free(utf8);
	}

	AQUALUNG_COND_SIGNAL(vol->thread_wait);
	AQUALUNG_MUTEX_UNLOCK(vol->wait_mutex);

	return FALSE;
}

gboolean
vol_update_progress(gpointer data) {

	volume_t * vol = (volume_t *)data;

	if (vol->slot) {

		float fraction = 0.0f;
		char str_progress[10];

		AQUALUNG_MUTEX_LOCK(vol->thread_mutex);
		if (vol->n_chunks != 0) {
			fraction = (float)vol->chunks_read / vol->n_chunks;
		}
		AQUALUNG_MUTEX_UNLOCK(vol->thread_mutex);

		if (fraction < 0 || fraction > 1.0f) {
			fraction = 0.0f;
		}

		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(vol->progress), fraction);
		snprintf(str_progress, 10, "%.0f%%", fraction * 100.0f);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(vol->progress), str_progress);
	}

	return TRUE;
}

void
vol_store_voladj(GtkTreeStore * store, GtkTreeIter * iter, float voladj) {

	if (!gtk_tree_store_iter_is_valid(store, iter)) {
		return;
	}

	if (store == music_store) { /* music store */
		gtk_tree_store_set(store, iter, 5, voladj, -1);
		music_store_mark_changed(iter);

	} else { /* playlist */
		
		char str[32];
		voladj2str(voladj, str);
		gtk_tree_store_set(store, iter,
				   PL_COL_VOLUME_ADJUSTMENT, voladj,
				   PL_COL_VOLUME_ADJUSTMENT_DISP, str, -1);
	}
}

gboolean
vol_store_result_sep(gpointer data) {

	volume_t * vol = (volume_t *)data;

	AQUALUNG_MUTEX_LOCK(vol->wait_mutex);

	vol_store_voladj(vol->store, &vol->item->iter,
			 (vol->store == music_store) ? vol->result : rva_from_volume(vol->result));

	AQUALUNG_COND_SIGNAL(vol->thread_wait);
	AQUALUNG_MUTEX_UNLOCK(vol->wait_mutex);

	return FALSE;
}

gboolean
vol_store_result_avg(gpointer data) {

	volume_t * vol = (volume_t *)data;
	GList * node = NULL;
	float voladj;

	AQUALUNG_MUTEX_LOCK(vol->wait_mutex);

	voladj = rva_from_multiple_volumes(vol->n_volumes, vol->volumes);

	for (node = vol->queue; node; node = node->next) {
		vol_item_t * item = (vol_item_t *)node->data;
		vol_store_voladj(vol->store, &item->iter, voladj);
	}

	AQUALUNG_COND_SIGNAL(vol->thread_wait);
	AQUALUNG_MUTEX_UNLOCK(vol->wait_mutex);

	return FALSE;
}

void
create_volume_window() {

	GtkWidget * vbox;

	vol_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(vol_window), _("Calculating volume level"));
        gtk_window_set_position(GTK_WINDOW(vol_window), GTK_WIN_POS_CENTER);
        gtk_window_resize(GTK_WINDOW(vol_window), 480, 110);
        g_signal_connect(G_OBJECT(vol_window), "event",
                         G_CALLBACK(vol_window_event), NULL);

        gtk_container_set_border_width(GTK_CONTAINER(vol_window), 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(vol_window), vbox);

        gtk_widget_show_all(vol_window);
}

void
vol_create_gui(volume_t * vol) {

	GtkWidget * table;
	GtkWidget * label;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * hseparator;

	++vol_slot_count;

	if (vol_window == NULL) {
		create_volume_window();
	}

	vbox = gtk_bin_get_child(GTK_BIN(vol_window));

	vol->slot = table = gtk_table_new(2, 5, FALSE);
        gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

        hseparator = gtk_hseparator_new();
	gtk_table_attach(GTK_TABLE(table), hseparator, 0, 3, 0, 1,
			 GTK_FILL, GTK_FILL, 5, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("File:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 5);

        vol->file_entry = gtk_entry_new();
        gtk_editable_set_editable(GTK_EDITABLE(vol->file_entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), vol->file_entry, 1, 2, 1, 2,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 5);

	vol->pause_button = gtk_button_new_with_label(_("Pause"));
	g_signal_connect(vol->pause_button, "clicked", G_CALLBACK(vol_pause_event), vol);
	gtk_table_attach(GTK_TABLE(table), vol->pause_button, 2, 3, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Progress:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3,
			 GTK_FILL, GTK_FILL, 5, 5);

	vol->progress = gtk_progress_bar_new();
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(vol->progress), 0.0f);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(vol->progress), "0.0%");
	gtk_table_attach(GTK_TABLE(table), vol->progress, 1, 2, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 5);

	vol->cancel_button = gui_stock_label_button (_("Abort"), GTK_STOCK_CANCEL); 
	g_signal_connect(vol->cancel_button, "clicked", G_CALLBACK(vol_cancel_event), vol);
	gtk_table_attach(GTK_TABLE(table), vol->cancel_button, 2, 3, 2, 3,
			 GTK_FILL, GTK_FILL, 5, 5);

        hseparator = gtk_hseparator_new();
	gtk_table_attach(GTK_TABLE(table), hseparator, 0, 3, 3, 4,
			 GTK_FILL, GTK_FILL, 5, 5);

	gtk_widget_show_all(vol->slot);
}


/* if returns 1, file will be skipped */
int
process_volume_setup(volume_t * vol) {

        if ((vol->fdec = file_decoder_new()) == NULL) {
                fprintf(stderr, "calculate_volume: error: file_decoder_new() returned NULL\n");
                return 1;
        }

        if (file_decoder_open(vol->fdec, vol->item->file)) {
                fprintf(stderr, "file_decoder_open() failed on %s\n",
                        vol->item->file);
                return 1;
        }

	if ((vol->rms = (rms_env_t *)calloc(1, sizeof(rms_env_t))) == NULL) {
		fprintf(stderr, "calculate_volume(): calloc error\n");
		return 1;
	}

	vol->chunks_read = 0;
	vol->chunk_size = vol->fdec->fileinfo.sample_rate / 100;
	vol->n_chunks = vol->fdec->fileinfo.total_samples / vol->chunk_size + 1;

	AQUALUNG_MUTEX_LOCK(vol->wait_mutex);
	g_idle_add(vol_set_filename_text, vol);
	AQUALUNG_COND_WAIT(vol->thread_wait, vol->wait_mutex);
	AQUALUNG_MUTEX_UNLOCK(vol->wait_mutex);

	vol->result = 0.0f;

	return 0;
}


void *
volume_thread(void * arg) {

	volume_t * vol = (volume_t *)arg;

	GList * node;

	float * samples = NULL;
	unsigned long numread;
	float chunk_power = 0.0f;
	float rms;
	unsigned long i;

	struct timespec req_time;
	struct timespec rem_time;

	req_time.tv_sec = 0;
        req_time.tv_nsec = 500000000;


	AQUALUNG_THREAD_DETACH();

	for (node = vol->queue; node; node = node->next) {

		vol->item = (vol_item_t *)node->data;

		if (process_volume_setup(vol)) {
			continue;
		}

		if ((samples = (float *)malloc(vol->chunk_size * vol->fdec->fileinfo.channels * sizeof(float))) == NULL) {

			fprintf(stderr, "volume_thread(): malloc() error\n");
			file_decoder_close(vol->fdec);
			file_decoder_delete(vol->fdec);
			free(vol->rms);
			break;
		}

		do {
			numread = file_decoder_read(vol->fdec, samples, vol->chunk_size);
			vol->chunks_read++;
			
			/* calculate signal power of chunk and feed it in the rms envelope */
			if (numread > 0) {
				for (i = 0; i < numread * vol->fdec->fileinfo.channels; i++) {
					chunk_power += samples[i] * samples[i];
				}
				chunk_power /= numread * vol->fdec->fileinfo.channels;
				
				rms = rms_env_process(vol->rms, chunk_power);
				
				if (rms > vol->result) {
					vol->result = rms;
				}
			}
			
			while (vol->paused && !vol->cancelled) {
				nanosleep(&req_time, &rem_time);
			}

		} while (numread == vol->chunk_size && !vol->cancelled);

		if (!vol->cancelled) {
					
			vol->result = 20.0f * log10f(vol->result);
					
#ifdef HAVE_MPEG
			/* compensate for anti-clip vol.reduction in dec_mpeg.c/mpeg_output() */
			if (vol->fdec->file_lib == MAD_LIB) {
				vol->result += 1.8f;
			}
#endif /* HAVE_MPEG */

			if (vol->type == VOLUME_SEPARATE) {

				AQUALUNG_MUTEX_LOCK(vol->wait_mutex);
				g_idle_add(vol_store_result_sep, vol);
				AQUALUNG_COND_WAIT(vol->thread_wait, vol->wait_mutex);
				AQUALUNG_MUTEX_UNLOCK(vol->wait_mutex);

			} else if (vol->type == VOLUME_AVERAGE) {

				vol->n_volumes++;
				if ((vol->volumes = realloc(vol->volumes, vol->n_volumes * sizeof(float))) == NULL) {
					fprintf(stderr, "volume_thread(): realloc error\n");
					return NULL;
				}
				vol->volumes[vol->n_volumes - 1] = vol->result;
			}
		}

		file_decoder_close(vol->fdec);
		file_decoder_delete(vol->fdec);
		free(vol->rms);

		free(samples);

		if (vol->cancelled) {
			break;
		}
	}

	if (!vol->cancelled && vol->type == VOLUME_AVERAGE) {
		AQUALUNG_MUTEX_LOCK(vol->wait_mutex);
		g_idle_add(vol_store_result_avg, vol);
		AQUALUNG_COND_WAIT(vol->thread_wait, vol->wait_mutex);
		AQUALUNG_MUTEX_UNLOCK(vol->wait_mutex);
	}

	for (node = vol->queue; node; node = node->next) {
		vol_item_free((vol_item_t *)node->data);
	}

	g_idle_add(volume_finalize, vol);


	return NULL;
}


void
volume_start(volume_t * vol) {

	if (vol->queue == NULL) {
		return;
	}

	vol_create_gui(vol);

	AQUALUNG_THREAD_CREATE(vol->thread_id, NULL, volume_thread, vol);

	vol->update_tag = g_timeout_add(250, vol_update_progress, vol);
}

float
rva_from_volume(float volume) {

	return ((volume - options.rva_refvol) * (options.rva_steepness - 1.0f));
}

/* The reference signal for so called 89 dB SPL replaygain is -14 dBFS pink noise */
/* The actual offset value is slightly different, based on emperical findings */
/* The conditions for calculation are probably slightly different */
/* I tried a few samples, and it usually varied +/- 0.5 dBFS */
/* If someone knows the exact way to match this against rva, please change it */
/* It is possible to do replaygain with a different reference level, this was not taken into account */
float
volume_from_replaygain(float replaygain) {
	return (-(replaygain)-15.7)/0.97;
}

float
rva_from_replaygain(float volume) {

	return rva_from_volume(volume_from_replaygain(volume));
}


float
rva_from_multiple_volumes(int nlevels, float * volumes) {

	int i, files_to_avg;
	char * badlevels;
	double sum, level, mean_level, variance, std_dev;
	double level_difference, threshold;

	if ((badlevels = (char *)calloc(nlevels, sizeof(char))) == NULL) {
		fprintf(stderr, "rva_from_multiple_volumes() : calloc error\n");
		return 0.0f;
	}

	sum = 0.0;
	for (i = 0; i < nlevels; i++) {
		sum += db2lin(volumes[i]);
	}
	mean_level = sum / nlevels;

	if (!options.rva_use_linear_thresh) { /* use stddev_thresh */

		sum = 0;
		for (i = 0; i < nlevels; i++) {
			double tmp = 20.0 * log10(db2lin(volumes[i]) / mean_level);
			sum += tmp * tmp;
		}
		variance = sum / nlevels;

		/* get standard deviation */
		if (variance < EPSILON) {
			std_dev = 0.0;
		} else {
			std_dev = sqrt(variance);
		}

		threshold = options.rva_avg_stddev_thresh * std_dev;
	} else {
		threshold = options.rva_avg_linear_thresh;
	}


	if (threshold > EPSILON && nlevels > 1) {

		for (i = 0; i < nlevels; i++) {
			level_difference = fabs(20.0 * log10(mean_level / db2lin(volumes[i])));
			if (level_difference > threshold) {
				badlevels[i] = 1;
			}
		}
	}

	/* throw out the levels marked as bad */
	files_to_avg = 0;
	sum = 0;
	for (i = 0; i < nlevels; i++) {
		if (!badlevels[i]) {
			sum += db2lin(volumes[i]);
			files_to_avg++;
		}
	}
	free(badlevels);

	if (files_to_avg == 0) {
		fprintf(stderr,
			"rva_from_multiple_volumes: all files ignored, using mean value.\n");
		return rva_from_volume(20 * log10(mean_level));
	}

	level = sum / files_to_avg;
	return rva_from_volume(20 * log10(level));
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

