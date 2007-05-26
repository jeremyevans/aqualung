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
#include "core.h"
#include "decoder/file_decoder.h"
#include "music_browser.h"
#include "i18n.h"
#include "volume.h"


#define RMSSIZE 100
#define EPSILON 0.00000000001

typedef struct {
        float buffer[RMSSIZE];
        unsigned int pos;
        float sum;
} rms_env;


extern GtkTreeStore * music_store;
extern GtkWidget* gui_stock_label_button(gchar *blabel, const gchar *bstock);

AQUALUNG_THREAD_DECLARE(volume_thread_id)


GtkWidget * vol_window = NULL;
GtkWidget * progress;
GtkWidget * cancel_button;
GtkWidget * file_entry;

vol_queue_t * vol_queue = NULL;
vol_queue_t * vol_queue_save = NULL;
file_decoder_t * fdec_vol;
unsigned long vol_chunk_size;
unsigned long vol_n_chunks;
unsigned long vol_chunks_read;
float vol_result;
int vol_running;
int vol_cancelled;
int vol_finished;
int vol_index;
int vol_window_visible;

double fraction;

rms_env * rms_vol;


void
voladj2str(float voladj, char * str) {

	if (fabs(voladj) < 0.05f) {
		sprintf(str, " %.1f dB", 0.0f);
	} else {
		sprintf(str, "% .1f dB", voladj);
	}
}


vol_queue_t *
vol_queue_push(vol_queue_t * q, char * file, GtkTreeIter iter) {

	vol_queue_t * q_item = NULL;
	vol_queue_t * q_prev;
	
	if ((q_item = (vol_queue_t *)calloc(1, sizeof(vol_queue_t))) == NULL) {
		fprintf(stderr, "vol_queue_push(): calloc error\n");
		return NULL;
	}
	
	q_item->file = strdup(file);
	q_item->iter = iter;
	q_item->next = NULL;

	if (q != NULL) {
		q_prev = q;
		while (q_prev->next != NULL) {
			q_prev = q_prev->next;
		}
		q_prev->next = q_item;
	}

	return q_item;
}


void
vol_queue_cleanup(vol_queue_t * q) {

	vol_queue_t * p;

	p = q;
	while (p != NULL) {
		q = p->next;
		free(p);
		p = q;
	}
}


inline static float
rms_env_process(rms_env * r, const float x) {

        r->sum -= r->buffer[r->pos];
        r->sum += x;
        r->buffer[r->pos] = x;

	r->pos++;
        if (r->pos == RMSSIZE) {
		r->pos = 0;
	}

        return sqrt(r->sum / (float)RMSSIZE);
}


int
vol_window_close(GtkWidget * widget, gpointer * data) {

        vol_cancelled = 1;
	vol_window = NULL;

        return 0;
}


gboolean
vol_window_destroy(gpointer data) {

	gtk_widget_destroy(vol_window);
	vol_window = NULL;

	return FALSE;
}


gint
cancel_event(GtkWidget * widget, GdkEvent * event, gpointer data) {

        vol_cancelled = 1;
        return TRUE;
}

gboolean
set_filename_text(gpointer data) {

	if (vol_window) {

		char * utf8 = g_filename_display_name((char *)data);

		gtk_entry_set_text(GTK_ENTRY(file_entry), utf8);
		gtk_editable_set_position(GTK_EDITABLE(file_entry), -1);
		gtk_widget_grab_focus(cancel_button);

		g_free(utf8);
	}

	return FALSE;
}

static gboolean
update_progress(gpointer data) {

	if (vol_window) {
		if (vol_window_visible) {
			char str_progress[10];

			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), fraction);
			snprintf(str_progress, 10, "%.0f%%", fraction * 100.0f);
			gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress), str_progress);
		} else {
			char title[MAXLEN];

			snprintf(title, MAXLEN, "%.0f%% - %s", fraction * 100.0f,
				 _("Calculating volume level"));
			gtk_window_set_title(GTK_WINDOW(vol_window), title);
		}
	}

	return FALSE;
}


gboolean
vol_window_state_changed(GtkWidget * widget, GdkEventWindowState * event, gpointer user_data) {

	if ((vol_window_visible = !(event->new_window_state & GDK_WINDOW_STATE_ICONIFIED))) {
		gtk_window_set_title(GTK_WINDOW(vol_window), _("Calculating volume level"));
	}

	return FALSE;
}

void
volume_window(void) {

	GtkWidget * table;
	GtkWidget * label;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * hbuttonbox;
	GtkWidget * hseparator;

	vol_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(vol_window), _("Calculating volume level"));
        gtk_window_set_position(GTK_WINDOW(vol_window), GTK_WIN_POS_CENTER);
        gtk_window_resize(GTK_WINDOW(vol_window), 430, 110);
        g_signal_connect(G_OBJECT(vol_window), "delete_event",
                         G_CALLBACK(vol_window_close), NULL);
        g_signal_connect(G_OBJECT(vol_window), "window_state_event",
                         G_CALLBACK(vol_window_state_changed), NULL);
        gtk_container_set_border_width(GTK_CONTAINER(vol_window), 5);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(vol_window), vbox);

	table = gtk_table_new(2, 2, FALSE);
        gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("File:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
			 GTK_FILL, GTK_FILL, 5, 5);

        file_entry = gtk_entry_new();
        gtk_editable_set_editable(GTK_EDITABLE(file_entry), FALSE);
	gtk_table_attach(GTK_TABLE(table), file_entry, 1, 2, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 5);

        hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Progress:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
			 GTK_FILL, GTK_FILL, 5, 5);

	progress = gtk_progress_bar_new();
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 0.0f);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress), "0.0%");
	gtk_table_attach(GTK_TABLE(table), progress, 1, 2, 1, 2,
			 GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 5);

        hseparator = gtk_hseparator_new ();
        gtk_widget_show (hseparator);
        gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, TRUE, 5);

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(vbox), hbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);

        cancel_button = gui_stock_label_button (_("Abort"), GTK_STOCK_CANCEL); 
        g_signal_connect(cancel_button, "clicked", G_CALLBACK(cancel_event), NULL);
  	gtk_container_add(GTK_CONTAINER(hbuttonbox), cancel_button);   

        gtk_widget_grab_focus(cancel_button);

        gtk_widget_show_all(vol_window);
}


/* if returns 1, file will be skipped */
int
process_volume_setup(vol_queue_t * q) {

	char msg[MAXLEN];


	if (q == NULL) {
		fprintf(stderr, "programmer error: process_volume_setup() called on NULL\n");
		return 1;
	}
	
        if ((fdec_vol = file_decoder_new()) == NULL) {
                fprintf(stderr, "calculate_volume: error: file_decoder_new() returned NULL\n");
                return 1;
        }

        if (file_decoder_open(fdec_vol, q->file)) {
                fprintf(stderr, "file_decoder_open() failed on %s\n",
                        q->file);
                return 1;
        }

	if ((rms_vol = (rms_env *)calloc(1, sizeof(rms_env))) == NULL) {
		fprintf(stderr, "calculate_volume(): calloc error\n");
		return 1;
	}

	vol_chunk_size = fdec_vol->fileinfo.sample_rate / 100;
	vol_n_chunks = fdec_vol->fileinfo.total_samples / vol_chunk_size + 1;
	vol_chunks_read = 0;
	
	strncpy(msg, q->file, MAXLEN-1);
	g_idle_add(set_filename_text, (gpointer)q->file);

	vol_running = 1;
	vol_result = 0.0f;

	return 0;
}



gboolean
process_volume(float * volumes) {

	float * samples = NULL;
	unsigned long numread;
	float chunk_power = 0.0f;
	float rms;
	unsigned long i;
	int runs;

	if (!vol_running) {

		if (vol_queue->next != NULL) {
			vol_queue = vol_queue->next;
			if (process_volume_setup(vol_queue)) {
				vol_running = 0;
				return TRUE;
			}
		} else {
			vol_queue_cleanup(vol_queue_save);
			vol_queue_save = vol_queue = NULL;
			vol_finished = 1;

			g_idle_add(vol_window_destroy, NULL);
			return FALSE;
		}
	}

	if (vol_cancelled) {
		vol_queue_cleanup(vol_queue_save);
		vol_queue_save = vol_queue = NULL;
		vol_finished = 1;
		file_decoder_close(fdec_vol);
		file_decoder_delete(fdec_vol);
		if (vol_window != NULL) {
			g_idle_add(vol_window_destroy, NULL);
			free(rms_vol);
		}

		return FALSE;
	}

	if ((samples = (float *)malloc(vol_chunk_size * fdec_vol->fileinfo.channels * sizeof(float))) == NULL) {

		fprintf(stderr, "process_volume(): error: malloc() returned NULL\n");
		vol_queue_cleanup(vol_queue_save);
		vol_queue_save = vol_queue = NULL;
		vol_finished = 1;
		file_decoder_close(fdec_vol);
		file_decoder_delete(fdec_vol);
		g_idle_add(vol_window_destroy, NULL);
		free(rms_vol);
		return FALSE;
	}


	/* just to make things faster */
	for (runs = 0; runs < 100; runs++) {

		numread = file_decoder_read(fdec_vol, samples, vol_chunk_size);
		++vol_chunks_read;

		if (runs == 0) {
			fraction = (double)vol_chunks_read / vol_n_chunks;
			if (fraction > 1.0f) {
				fraction = 1.0f;
			}

			g_idle_add(update_progress, NULL);
		}

		
		/* calculate signal power of chunk and feed it in the rms envelope */
		if (numread > 0) {
			for (i = 0; i < numread * fdec_vol->fileinfo.channels; i++) {
				chunk_power += samples[i] * samples[i];
			}
			chunk_power /= numread * fdec_vol->fileinfo.channels;
			
			rms = rms_env_process(rms_vol, chunk_power);

			if (rms > vol_result) {
				vol_result = rms;
			}
		}
		
		if ((numread < vol_chunk_size) || (vol_cancelled)) {

			if (!vol_cancelled) {
				
				vol_result = 20.0f * log10f(vol_result);

#ifdef HAVE_MPEG
				/* compensate for anti-clip vol.reduction in dec_mpeg.c/mpeg_output() */
				if (fdec_vol->file_lib == MAD_LIB) {
					vol_result += 1.8f;
				}
#endif /* HAVE_MPEG */

				if (volumes == NULL) {

					/* seems to works from this thread */
					gtk_tree_store_set(music_store, &(vol_queue->iter), 5, vol_result, -1);
					music_store_mark_changed(&(vol_queue->iter));
				} else {
					volumes[vol_index++] = vol_result;
				}
			}
			file_decoder_close(fdec_vol);
			file_decoder_delete(fdec_vol);
			free(rms_vol);
			free(samples);
			vol_running = 0;
			return TRUE;
		}
	}
	free(samples);
	return TRUE;
}


void *
volume_thread(void * arg) {

	float * volumes = (float *)arg;

	AQUALUNG_THREAD_DETACH()

	while (process_volume(volumes) == TRUE);

	return NULL;
}

/* volumes == NULL: store in music_store, else: store in result vector */
void
calculate_volume(vol_queue_t * q, float * volumes) {

	if (q == NULL) {
		return;
	}
	
	vol_queue_save = vol_queue = q;
	volume_window();
	vol_index = 0;
	vol_finished = 0;
	vol_cancelled = 0;
	process_volume_setup(vol_queue);
	AQUALUNG_THREAD_CREATE(volume_thread_id, NULL, volume_thread, volumes)
}

float
rva_from_volume(float volume, float rva_refvol, float rva_steepness) {

	return ((volume - rva_refvol) * (rva_steepness - 1.0f));
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
rva_from_replaygain(float volume, float rva_refvol, float rva_steepness) {

	return rva_from_volume(volume_from_replaygain(volume), rva_refvol, rva_steepness);
}

float
rva_from_multiple_volumes(int nlevels, float * volumes,
			  int use_lin_thresh, float lin_thresh, float stddev_thresh,
			  float rva_refvol, float rva_steepness) {

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

	if (!use_lin_thresh) { /* use stddev_thresh */

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

		threshold = stddev_thresh * std_dev;
	} else {
		threshold = lin_thresh;
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
		return rva_from_volume(20 * log10(mean_level), rva_refvol, rva_steepness);
	}

	level = sum / files_to_avg;
	return rva_from_volume(20 * log10(level), rva_refvol, rva_steepness);
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

