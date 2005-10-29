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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/stat.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#ifdef HAVE_SRC
#include <samplerate.h>
#endif /* HAVE_SRC */

/* for OSS support */
#ifdef HAVE_OSS
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/soundcard.h>
#endif /* HAVE_OSS */

/* for JACK support */
#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include "common.h"
#include "version.h"
#include "decoder/file_decoder.h"
#include "transceiver.h"
#include "gui_main.h"
#include "plugin.h"
#include "i18n.h"
#include "core.h"


/* JACK data */
jack_client_t * jack_client;
jack_port_t * out_L_port;
jack_port_t * out_R_port;
char * client_name = NULL;
jack_nframes_t jack_nframes;
int jack_is_shutdown;
int auto_connect = 0;
int default_ports = 1;
char * user_port1 = NULL;
char * user_port2 = NULL;

const size_t sample_size = sizeof(float);


/* ALSA driver parameters */
#ifdef HAVE_ALSA
int nperiods = 0;
int period = 0;
#endif /* HAVE_ALSA */

/* The name of the output device e.g. "/dev/dsp" or "plughw:0,0" */
char * device_name = NULL;


/***** disk thread stuff *****/
unsigned long long sample_offset;
status_t disk_thread_status;
int output = 0; /* oss/alsa/jack */

#ifdef HAVE_SRC
int src_type = 4;
int src_type_parsed = 0;
#endif /* HAVE_SRC */

/* Synchronization between disk thread and output thread */
jack_ringbuffer_t * rb;
pthread_mutex_t disk_thread_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  disk_thread_wake = PTHREAD_COND_INITIALIZER;
jack_ringbuffer_t * rb_disk2out;

/* Communication between gui thread and disk thread */
jack_ringbuffer_t * rb_gui2disk;
jack_ringbuffer_t * rb_disk2gui;

/* Lock critical operations that could interfere with output thread */
pthread_mutex_t output_thread_lock = PTHREAD_MUTEX_INITIALIZER;
double left_gain = 1.0;
double right_gain = 1.0;

/* LADSPA stuff */
int ladspa_is_postfader = 0;
pthread_mutex_t plugin_lock = PTHREAD_MUTEX_INITIALIZER;
unsigned long ladspa_buflen = 0;
LADSPA_Data * l_buf = NULL;
LADSPA_Data * r_buf = NULL;
int n_plugins = 0;
plugin_instance * plugin_vect[MAX_PLUGINS];

/* remote control */
extern int immediate_start;
extern int aqualung_session_id;
extern int aqualung_socket_fd;
extern char aqualung_socket_filename[256];

extern char cwd[MAXLEN];


float convf(char * s) {

        float val, pow;
        int i, sign;

        for (i = 0; s[i] == ' ' || s[i] == '\n' || s[i] == '\t'; i++);
        sign = 1;
        if (s[i] == '+' || s[i] == '-')
                sign = (s[i++] == '+') ? 1 : -1;
        for (val = 0; s[i] >= '0' && s[i] <= '9'; i++)
                val = 10 * val + s[i] - '0';
        if ((s[i] == '.') || (s[i] == ','))
                i++;
        for (pow = 1; s[i] >= '0' && s[i] <= '9'; i++) {
                val = 10 * val + s[i] - '0';
                pow *= 10;
        }
        return(sign * val / pow);
}


/* return 1 if conversion is possible, 0 if not */
int
sample_rates_ok(int out_SR, int file_SR) {

#ifdef HAVE_SRC
	float src_ratio;

	src_ratio = 1.0 * out_SR / file_SR;
	if (!src_is_valid_ratio(src_ratio) ||
	    src_ratio > MAX_RATIO || src_ratio < 1.0/MAX_RATIO) {
		fprintf(stderr, "core.c/sample_rates_ok(): too big difference between input and "
			"output sample rate!\n");
#else
        if (out_SR != file_SR) {
		fprintf(stderr,
			"Input file's samplerate (%ld Hz) and output samplerate (%ld Hz) differ, "
			"and\nAqualung is compiled without Sample Rate Converter support. To play "
			"this file,\nyou have to build Aqualung with internal Sample Rate Converter "
			"support,\nor set the playback sample rate to match the file's sample rate."
			"\n", file_SR, out_SR);             
#endif /* HAVE_SRC */
		return 0;
	}
	return 1;
}


void *
disk_thread(void * arg) {

	thread_info_t * info = (thread_info_t *)arg;
	file_decoder_t * fdec = NULL;
	unsigned int n_read = 0;
	unsigned int want_read;
	int n_src = 0;
	int n_src_prev = 0;
	int end_of_file = 0;
	double src_ratio = 1.0;
	void * readbuf = malloc(MAX_RATIO * info->rb_size * 2 * sample_size);
	void * framebuf = malloc(MAX_RATIO * info->rb_size * 2 * sample_size);
	size_t n_space;
	char send_cmd, recv_cmd;
	char filename[RB_CONTROL_SIZE];
	seek_t seek;
	cue_t cue;
	int i;


#ifdef HAVE_SRC
	int src_type_prev;
	SRC_STATE * src_state;
	SRC_DATA src_data;
	int src_error;

        if ((src_state = src_new(src_type, 2, &src_error)) == NULL) {
		fprintf(stderr, "disk thread: error: src_new() failed: %s.\n",
			src_strerror(src_error));
		exit(1);
	}
	src_type_prev = src_type;
#endif /* HAVE_SRC */

	if ((fdec = file_decoder_new()) == NULL) {
		fprintf(stderr, "disk thread: error: file_decoder_new() failed\n");
		exit(1);
	}

	if ((!readbuf) || (!framebuf)) {
		fprintf(stderr, "disk thread: malloc error\n");
		exit(1);
	}

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_mutex_lock(&disk_thread_lock);

	filename[0] = '\0';

	while (1) {

		recv_cmd = 0;
		if (jack_ringbuffer_read_space(rb_gui2disk) > 0) {
			jack_ringbuffer_read(rb_gui2disk, &recv_cmd, 1);
			switch (recv_cmd) {
			case CMD_CUE:
				/* read the string */
				while (jack_ringbuffer_read_space(rb_gui2disk) < sizeof(cue_t))
					;

				jack_ringbuffer_read(rb_gui2disk, (void *)&cue, sizeof(cue_t));
				
				if (cue.filename != NULL) {
					strncpy(filename, cue.filename, MAXLEN-1);
					free(cue.filename);
				} else {
					filename[0] = '\0';
				}

				if (fdec->file_lib != 0)
					file_decoder_close(fdec);
				if (filename[0] != '\0') {
					if (file_decoder_open(fdec, filename)) {
						fdec->samples_left = 0;
						info->is_streaming = 0;
						end_of_file = 1;
						send_cmd = CMD_FILEREQ;
						jack_ringbuffer_write(rb_disk2gui, &send_cmd, 1);
						goto sleep;
					} else if (!sample_rates_ok(info->out_SR, fdec->SR)) {
						fdec->file_open = 1; /* to get close_file() working */
						file_decoder_close(fdec);
						fdec->file_open = 0;

						fdec->samples_left = 0;
						info->is_streaming = 0;
						end_of_file = 1;
						send_cmd = CMD_FILEREQ;
						jack_ringbuffer_write(rb_disk2gui, &send_cmd, 1);
						goto sleep;
					} else {
						file_decoder_set_rva(fdec, cue.voladj);
						info->in_SR_prev = info->in_SR;
						info->in_SR = fdec->SR;
						info->is_mono = (fdec->channels == 1) ? 1 : 0;

						sample_offset = 0;

						send_cmd = CMD_FILEINFO;
						jack_ringbuffer_write(rb_disk2gui, &send_cmd,
								      sizeof(send_cmd));
						jack_ringbuffer_write(rb_disk2gui,
								      (char *)&(fdec->fileinfo),
								      sizeof(fileinfo_t));

						info->is_streaming = 1;
						end_of_file = 0;
					}
				} else { /* STOP */
					info->is_streaming = 0;

					/* send a FLUSH command to output thread to stop immediately */
					send_cmd = CMD_FLUSH;
					jack_ringbuffer_write(rb_disk2out, &send_cmd, 1);
					goto sleep;
				}
				break;
			case CMD_STOPWOFL: /* STOP but first flush output ringbuffer. */
				info->is_streaming = 0;
				goto flush;
				break;
			case CMD_PAUSE:
				info->is_streaming = 0;

				/* send a FLUSH command to output thread */
				send_cmd = CMD_FLUSH;
				jack_ringbuffer_write(rb_disk2out, &send_cmd, 1);

				/* roll back sample_offset samples, if possible */
				sample_offset = jack_ringbuffer_read_space(rb) /
					(2 * sample_size) * src_ratio;
				if (sample_offset <
				    fdec->fileinfo.total_samples - fdec->samples_left)
					file_decoder_seek(fdec,
					   fdec->fileinfo.total_samples - fdec->samples_left
					   - sample_offset);
				else
				        file_decoder_seek(fdec, 0);
				break;
			case CMD_RESUME:
				info->is_streaming = 1;
				break;
			case CMD_FINISH:
				goto done;
				break;
			case CMD_SEEKTO:
				while (jack_ringbuffer_read_space(rb_gui2disk) < sizeof(seek_t))
					;
				jack_ringbuffer_read(rb_gui2disk, (char *)&seek, sizeof(seek_t));
				if (fdec->file_lib != 0) {
					file_decoder_seek(fdec, seek.seek_to_pos);
					/* send a FLUSH command to output thread */
					send_cmd = CMD_FLUSH;
					jack_ringbuffer_write(rb_disk2out, &send_cmd, 1);
				} else {
					/* send dummy STATUS to gui, to set pos slider to zero */
					disk_thread_status.samples_left = 0;
					disk_thread_status.sample_offset = 0;
					send_cmd = CMD_STATUS;
					jack_ringbuffer_write(rb_disk2gui, &send_cmd, sizeof(send_cmd));
					jack_ringbuffer_write(rb_disk2gui, (char *)&disk_thread_status,
							      sizeof(status_t));
				}
				break;
			default:
				fprintf(stderr, "disk thread: received unexpected command %d\n", recv_cmd);
				break;
			}

		} else if (end_of_file)
			goto sleep;

		if (!info->is_streaming)
			goto sleep;

		n_space = jack_ringbuffer_write_space(rb) / (2 * sample_size);
		while (n_src < 0.95 * n_space) {
			
			src_ratio = (double)info->out_SR / (double)info->in_SR;
			n_src_prev = n_src;
			want_read = floor((n_space - n_src) / src_ratio);

			if (want_read > MAX_RATIO * info->rb_size)
				want_read = MAX_RATIO * info->rb_size;
			
			n_read = file_decoder_read(fdec, readbuf, want_read);
			if (n_read < want_read)
				end_of_file = 1;
			
			if (info->is_mono) { /* convert to stereo interleaved */
				for (i = 2*n_read - 1; i >= 0; i--) {
					memcpy(readbuf + i * sample_size,
					       readbuf + i/2 * sample_size,
					       sample_size);
				}
			}
			
			if (info->in_SR == info->out_SR) {
				memcpy(framebuf + n_src_prev * 2*sample_size,
				       readbuf, n_read * 2*sample_size);
				n_src += n_read;

			} else { /* do SRC */
#ifdef HAVE_SRC				
				if ((info->in_SR_prev != info->in_SR) ||
				    (src_type_prev != src_type)) { /* reinit SRC */

					src_state = src_delete(src_state);
					if ((src_state = src_new(src_type, 2, &src_error)) == NULL) {
						fprintf(stderr, "disk thread: error: src_new() failed: "
						       "%s.\n", src_strerror(src_error));
						goto done;
					}
					info->in_SR_prev = info->in_SR;
					src_type_prev = src_type;
				}
				
				src_data.input_frames = n_read;
				src_data.data_in = readbuf;
				src_data.src_ratio = src_ratio;
				src_data.data_out = framebuf + n_src_prev * 2*sample_size;
				src_data.output_frames = n_space - n_src_prev;
				if ((src_error = src_process(src_state, &src_data))) {
					fprintf(stderr, "disk thread: SRC error: %s\n",
					       src_strerror(src_error));
					goto done;
				}
				n_src += src_data.output_frames_gen;
#endif /* HAVE_SRC */
			}
			
			if (end_of_file) {

				file_decoder_close(fdec);

				/* send request for a new filename */
				send_cmd = CMD_FILEREQ;
				jack_ringbuffer_write(rb_disk2gui, &send_cmd, 1);
				goto sleep;
			}
		}

	flush:
		jack_ringbuffer_write(rb, framebuf, n_src * 2*sample_size);

		/* update & send STATUS */
		fdec->samples_left -= n_read;
		sample_offset =	jack_ringbuffer_read_space(rb) / (2 * sample_size);
		disk_thread_status.samples_left = fdec->samples_left;
		disk_thread_status.sample_offset = sample_offset / src_ratio;
		if (disk_thread_status.samples_left < 0) {
			disk_thread_status.samples_left = 0;
		}

		if (!jack_ringbuffer_read_space(rb_gui2disk)) {
			send_cmd = CMD_STATUS;
			jack_ringbuffer_write(rb_disk2gui, &send_cmd, sizeof(send_cmd));
			jack_ringbuffer_write(rb_disk2gui, (char *)&disk_thread_status,
					      sizeof(status_t));
		}

		/* cleanup buffer counters */
		n_src = 0;
		n_src_prev = 0;
		end_of_file = 0;
		
	sleep:
		pthread_cond_wait(&disk_thread_wake, &disk_thread_lock);
	}
 done:
	free(readbuf);
	free(framebuf);
#ifdef HAVE_SRC
	src_state = src_delete(src_state);
#endif /* HAVE_SRC */
	file_decoder_delete(fdec);
	pthread_mutex_unlock(&disk_thread_lock);
	return 0;
}



/* OSS output thread */
#ifdef HAVE_OSS
void *
oss_thread(void * arg) {

        jack_nframes_t i;
        thread_info_t * info = (thread_info_t *)arg;
	int bufsize = 1024;
        int n_avail;
	int ioctl_status;
	char recv_cmd;

	int fd_oss = info->fd_oss;
	short * oss_short_buf;
	jack_nframes_t rb_size = info->rb_size;

	struct timespec req_time;
	struct timespec rem_time;
	req_time.tv_sec = 0;
        req_time.tv_nsec = 100000000;

	pthread_mutex_lock(&output_thread_lock);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	if ((info->oss_short_buf = malloc(2*bufsize * sizeof(short))) == NULL) {
		fprintf(stderr, "oss_thread: malloc error\n");
		exit(1);
	}
	oss_short_buf = info->oss_short_buf;

	if ((l_buf = malloc(bufsize * sizeof(LADSPA_Data))) == NULL) {
		fprintf(stderr, "oss_thread: malloc error\n");
		exit(1);
	}
	if ((r_buf = malloc(bufsize * sizeof(LADSPA_Data))) == NULL) {
		fprintf(stderr, "oss_thread: malloc error\n");
		exit(1);
	}
	ladspa_buflen = bufsize;


	while (1) {
		while (jack_ringbuffer_read_space(rb_disk2out)) {
			jack_ringbuffer_read(rb_disk2out, &recv_cmd, 1);
			switch (recv_cmd) {
			case CMD_FLUSH:
				while ((n_avail = jack_ringbuffer_read_space(rb)) > 0) {
					if (n_avail > 2*bufsize * sizeof(short))
						n_avail = 2*bufsize * sizeof(short);
					jack_ringbuffer_read(rb, (char *)oss_short_buf,
							     2*bufsize * sizeof(short));
				}
				goto oss_wake;
				break;
			default:
				fprintf(stderr, "oss_thread: recv'd unknown command %d\n", recv_cmd);
				break;
			}
		}

		if ((n_avail = jack_ringbuffer_read_space(rb) / (2*sample_size)) == 0) {
			pthread_mutex_unlock(&output_thread_lock);
			nanosleep(&req_time, &rem_time);
			pthread_mutex_lock(&output_thread_lock);
			goto oss_wake;
		}

		if (n_avail > bufsize)
			n_avail = bufsize;
		
		for (i = 0; i < n_avail; i++) {
			jack_ringbuffer_read(rb, (char *)&(l_buf[i]), sample_size);
			jack_ringbuffer_read(rb, (char *)&(r_buf[i]), sample_size);
		}

		if (ladspa_is_postfader) {
			for (i = 0; i < n_avail; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}

		if (n_avail < bufsize) {
			for (i = n_avail; i < bufsize; i++) {
				l_buf[i] = 0.0f;
				r_buf[i] = 0.0f;
			}
		}

		/* plugin processing */
		pthread_mutex_lock(&plugin_lock);
		for (i = 0; i < n_plugins; i++) {
			if (plugin_vect[i]->is_bypassed)
				continue;

			if (plugin_vect[i]->handle) {
				plugin_vect[i]->descriptor->run(plugin_vect[i]->handle, ladspa_buflen);
			}
			if (plugin_vect[i]->handle2) {
				plugin_vect[i]->descriptor->run(plugin_vect[i]->handle2, ladspa_buflen);
			}
		}
		pthread_mutex_unlock(&plugin_lock);

		if (!ladspa_is_postfader) {
			for (i = 0; i < bufsize; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}

		for (i = 0; i < bufsize; i++) {
			if (l_buf[i] > 1.0)
				l_buf[i] = 1.0;
			else if (l_buf[i] < -1.0)
				l_buf[i] = -1.0;

			if (r_buf[i] > 1.0)
				r_buf[i] = 1.0;
			else if (r_buf[i] < -1.0)
				r_buf[i] = -1.0;

			oss_short_buf[2*i] = floorf(32767.0 * l_buf[i]);
			oss_short_buf[2*i+1] = floorf(32767.0 * r_buf[i]);
		}

		/* write data to audio device */
		pthread_mutex_unlock(&output_thread_lock);
		ioctl_status = write(fd_oss, oss_short_buf, 2*n_avail * sizeof(short));
		pthread_mutex_lock(&output_thread_lock);
		if (ioctl_status != 2*n_avail * sizeof(short))
			fprintf(stderr, "oss_thread: Error writing to audio device\n");

		/* wake up disk thread if 1/4 of rb data has been read */
		/* note that 1 frame = 8 bytes so 8 * info->rb_size equals the full data amount */
	oss_wake:
		if (jack_ringbuffer_read_space(rb) < 6 * rb_size) {
			if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
				pthread_cond_signal(&disk_thread_wake);
				pthread_mutex_unlock(&disk_thread_lock);
			}
		}		
	}
        /* NOTREACHED -- this thread will be cancelled from main thread on exit */
	return 0;
}
#endif /* HAVE_OSS */



/* ALSA output thread */
#ifdef HAVE_ALSA
void *
alsa_thread(void * arg) {

        jack_nframes_t i;
        thread_info_t * info = (thread_info_t *)arg;
	snd_pcm_sframes_t n_written = 0;
	int bufsize = 1024;
        int n_avail;
	char recv_cmd;

	int is_output_32bit = info->is_output_32bit;
	short * alsa_short_buf = NULL;
	int * alsa_int_buf = NULL;

	jack_nframes_t rb_size = info->rb_size;
	snd_pcm_t * pcm_handle = info->pcm_handle;

	struct timespec req_time;
	struct timespec rem_time;
	req_time.tv_sec = 0;
        req_time.tv_nsec = 100000000;

	pthread_mutex_lock(&output_thread_lock);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	if (is_output_32bit) {
		if ((info->alsa_int_buf = malloc(2*bufsize * sizeof(int))) == NULL) {
			fprintf(stderr, "alsa_thread: malloc error\n");
			exit(1);
		}
		alsa_int_buf = info->alsa_int_buf;
	} else {
		if ((info->alsa_short_buf = malloc(2*bufsize * sizeof(short))) == NULL) {
			fprintf(stderr, "alsa_thread: malloc error\n");
			exit(1);
		}
		alsa_short_buf = info->alsa_short_buf;
	}

	if ((l_buf = malloc(bufsize * sizeof(LADSPA_Data))) == NULL) {
		fprintf(stderr, "alsa_thread: malloc error\n");
		exit(1);
	}
	if ((r_buf = malloc(bufsize * sizeof(LADSPA_Data))) == NULL) {
		fprintf(stderr, "alsa_thread: malloc error\n");
		exit(1);
	}
	ladspa_buflen = bufsize;


	while (1) {
		while (jack_ringbuffer_read_space(rb_disk2out)) {
			jack_ringbuffer_read(rb_disk2out, &recv_cmd, 1);
			switch (recv_cmd) {
			case CMD_FLUSH:
				if (is_output_32bit) {
					while ((n_avail = jack_ringbuffer_read_space(rb)) > 0) {
						if (n_avail > 2*bufsize * sizeof(int))
							n_avail = 2*bufsize * sizeof(int);
						jack_ringbuffer_read(rb, (char *)alsa_int_buf,
								     2*bufsize * sizeof(int));
					}
				} else {
					while ((n_avail = jack_ringbuffer_read_space(rb)) > 0) {
						if (n_avail > 2*bufsize * sizeof(short))
							n_avail = 2*bufsize * sizeof(short);
						jack_ringbuffer_read(rb, (char *)alsa_short_buf,
								     2*bufsize * sizeof(short));
					}
				}
				goto alsa_wake;
				break;
			default:
				fprintf(stderr, "alsa_thread: recv'd unknown command %d\n", recv_cmd);
				break;
			}
		}

		if ((n_avail = jack_ringbuffer_read_space(rb) / (2*sample_size)) == 0) {
			pthread_mutex_unlock(&output_thread_lock);
			nanosleep(&req_time, &rem_time);
			pthread_mutex_lock(&output_thread_lock);
			goto alsa_wake;
		}

		if (n_avail > bufsize)
			n_avail = bufsize;

		for (i = 0; i < n_avail; i++) {
			jack_ringbuffer_read(rb, (char *)&(l_buf[i]), sample_size);
			jack_ringbuffer_read(rb, (char *)&(r_buf[i]), sample_size);
		}

		if (ladspa_is_postfader) {
			for (i = 0; i < n_avail; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}

		if (n_avail < bufsize) {
			for (i = n_avail; i < bufsize; i++) {
				l_buf[i] = 0.0f;
				r_buf[i] = 0.0f;
			}
		}
		
		/* plugin processing */
		pthread_mutex_lock(&plugin_lock);
		for (i = 0; i < n_plugins; i++) {
			if (plugin_vect[i]->is_bypassed)
				continue;
			
			if (plugin_vect[i]->handle) {
				plugin_vect[i]->descriptor->run(plugin_vect[i]->handle, ladspa_buflen);
			}
			if (plugin_vect[i]->handle2) {
				plugin_vect[i]->descriptor->run(plugin_vect[i]->handle2, ladspa_buflen);
			}
		}
		pthread_mutex_unlock(&plugin_lock);
		
		if (!ladspa_is_postfader) {
			for (i = 0; i < bufsize; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}

		if (is_output_32bit) {
			for (i = 0; i < bufsize; i++) {
				if (l_buf[i] > 1.0)
					l_buf[i] = 1.0;
				else if (l_buf[i] < -1.0)
					l_buf[i] = -1.0;

				if (r_buf[i] > 1.0)
					r_buf[i] = 1.0;
				else if (r_buf[i] < -1.0)
					r_buf[i] = -1.0;
				
				alsa_int_buf[2*i] = floorf(2147483647.0 * l_buf[i]);
				alsa_int_buf[2*i+1] = floorf(2147483647.0 * r_buf[i]);
			}

			/* write data to audio device */
			pthread_mutex_unlock(&output_thread_lock);
			if ((n_written = snd_pcm_writei(pcm_handle, alsa_int_buf, n_avail)) != n_avail) {
				snd_pcm_prepare(pcm_handle);
			}
			pthread_mutex_lock(&output_thread_lock);

		} else {
			for (i = 0; i < bufsize; i++) {
				if (l_buf[i] > 1.0)
					l_buf[i] = 1.0;
				else if (l_buf[i] < -1.0)
					l_buf[i] = -1.0;
				
				if (r_buf[i] > 1.0)
					r_buf[i] = 1.0;
				else if (r_buf[i] < -1.0)
					r_buf[i] = -1.0;

				alsa_short_buf[2*i] = floorf(32767.0 * l_buf[i]);
				alsa_short_buf[2*i+1] = floorf(32767.0 * r_buf[i]);
			}

			/* write data to audio device */
			pthread_mutex_unlock(&output_thread_lock);
			if ((n_written = snd_pcm_writei(pcm_handle, alsa_short_buf, n_avail)) != n_avail) {
				snd_pcm_prepare(pcm_handle);
			}
			pthread_mutex_lock(&output_thread_lock);
		}
		
		/* wake up disk thread if 1/4 of rb data has been read */
		/* note that 1 frame = 8 bytes so 8 * info->rb_size equals the full data amount */
	alsa_wake:
		if (jack_ringbuffer_read_space(rb) < 6 * rb_size) {
			if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
				pthread_cond_signal(&disk_thread_wake);
				pthread_mutex_unlock(&disk_thread_lock);
			}
		}		
	}
        /* NOTREACHED -- this thread will be cancelled from main thread on exit */
	return 0;
}
#endif /* HAVE_ALSA */



/* JACK output function */
int
process(jack_nframes_t nframes, void * arg) {

	jack_nframes_t i;
	jack_nframes_t n_avail;
	thread_info_t * info = (thread_info_t *) arg;
	jack_default_audio_sample_t * out1 = jack_port_get_buffer(out_L_port, nframes);
	jack_default_audio_sample_t * out2 = jack_port_get_buffer(out_R_port, nframes);

	static int flushing = 0;
	static int flushcnt = 0;
	char recv_cmd;

	pthread_mutex_lock(&output_thread_lock);
	
	ladspa_buflen = jack_nframes = nframes;
	
	while (jack_ringbuffer_read_space(rb_disk2out)) {
		jack_ringbuffer_read(rb_disk2out, &recv_cmd, 1);
		switch (recv_cmd) {
		case CMD_FLUSH:
			flushing = 1;
			flushcnt = jack_ringbuffer_read_space(rb)/nframes/
				(2*sample_size) * 1.1f;
			break;
		default:
			fprintf(stderr, "jack process(): recv'd unknown command %d\n", recv_cmd);
			break;
		}
	}
	
	n_avail = jack_ringbuffer_read_space(rb) / (2*sample_size);
	if (n_avail > nframes)
		n_avail = nframes;
	
	for (i = 0; i < n_avail; i++) {
		jack_ringbuffer_read(rb, (char *)&(l_buf[i]), sample_size);
		jack_ringbuffer_read(rb, (char *)&(r_buf[i]), sample_size);
	}
	
	if (n_avail < nframes) {
		for (i = n_avail; i < nframes; i++) {
			l_buf[i] = 0.0f;
			r_buf[i] = 0.0f;
		}
	}
	
	if (flushing) {
		for (i = 0; i < n_avail; i++) {
			l_buf[i] = 0.0f;
			r_buf[i] = 0.0f;
		}
	} else {
		if (ladspa_is_postfader) {
			for (i = 0; i < n_avail; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}
		
		/* plugin processing */
		pthread_mutex_lock(&plugin_lock);
		for (i = 0; i < n_plugins; i++) {
			if (plugin_vect[i]->is_bypassed)
				continue;
			
			if (plugin_vect[i]->handle) {
				plugin_vect[i]->descriptor->run(plugin_vect[i]->handle, ladspa_buflen);
			}
			if (plugin_vect[i]->handle2) {
				plugin_vect[i]->descriptor->run(plugin_vect[i]->handle2, ladspa_buflen);
			}
		}
		pthread_mutex_unlock(&plugin_lock);
		
		if (!ladspa_is_postfader) {
			for (i = 0; i < nframes; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}
	}
	
	for (i = 0; i < nframes; i++) {
		out1[i] = l_buf[i];
		out2[i] = r_buf[i];
	}
	
	
	if ((flushing) && (!jack_ringbuffer_read_space(rb) || (--flushcnt == 0))) {
		flushing = 0;
	}
	
	pthread_mutex_unlock(&output_thread_lock);

	/* wake up disk thread if 1/4 of rb data has been read */
	/* note that 1 frame = 8 bytes so 8 * info->rb_size equals the full data amount */
	if (jack_ringbuffer_read_space(rb) < 6 * info->rb_size) {
		if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
			pthread_cond_signal(&disk_thread_wake);
			pthread_mutex_unlock(&disk_thread_lock);
		}
	}

	return 0;
}


void
jack_shutdown(void *arg) {

	pthread_mutex_lock(&output_thread_lock);
	jack_is_shutdown = 1;
	pthread_mutex_unlock(&output_thread_lock);
}





#ifdef HAVE_OSS
void
oss_init(thread_info_t * info) {

	int ioctl_arg;
	int ioctl_status;

        if (info->out_SR > MAX_SAMPLERATE) {
                fprintf(stderr, "\nThe sample rate you set (%ld Hz) is higher than MAX_SAMPLERATE.\n",
		       info->out_SR);
                fprintf(stderr, "This is an arbitrary limit, which you may safely enlarge "
                       "if you really need to.\n");
                fprintf(stderr, "Currently MAX_SAMPLERATE = %d Hz.\n", MAX_SAMPLERATE);
                exit(1);
        }

	/* open dsp device */
	info->fd_oss = open(device_name, O_WRONLY, 0);
	if (info->fd_oss < 0) {
		fprintf(stderr, "oss_init: open of %s ", device_name);
		perror("failed");
		exit(1);
	}

	ioctl_arg = 16; /* bitdepth */
	ioctl_status = ioctl(info->fd_oss, SOUND_PCM_WRITE_BITS, &ioctl_arg);
	if (ioctl_status == -1) {
		perror("oss_init: SOUND_PCM_WRITE_BITS ioctl failed");
		exit(1);
	}
	if (ioctl_arg != 16) {
		perror("oss_init: unable to set sample size");
		exit(1);
	}

	ioctl_arg = 2;  /* stereo */
	ioctl_status = ioctl(info->fd_oss, SOUND_PCM_WRITE_CHANNELS, &ioctl_arg);
	if (ioctl_status == -1) {
		perror("oss_init: SOUND_PCM_WRITE_CHANNELS ioctl failed");
		exit(1);
	}
	if (ioctl_arg != 2) {
		perror("oss_init: unable to set number of channels");
		exit(1);
	}

	ioctl_arg = info->out_SR;
	ioctl_status = ioctl(info->fd_oss, SOUND_PCM_WRITE_RATE, &ioctl_arg);
	if (ioctl_status == -1) {
		perror("oss_init: SOUND_PCM_WRITE_RATE ioctl failed");
		exit(1);
	}
	if (ioctl_arg != info->out_SR) {
		fprintf(stderr, "oss_init: unable to set sample_rate to %ld\n", info->out_SR);
	}

	/* start OSS output thread */
	pthread_create(&info->oss_thread_id, NULL, oss_thread, info);
}
#endif /* HAVE_OSS */



#ifdef HAVE_ALSA
void
alsa_init(thread_info_t * info) {

	unsigned rate;
	int dir = 0;

	info->stream = SND_PCM_STREAM_PLAYBACK;
	snd_pcm_hw_params_alloca(&info->hwparams);
	if (snd_pcm_open(&info->pcm_handle, info->pcm_name, info->stream, 0) < 0) {
		fprintf(stderr, "alsa_init: error opening PCM device %s\n", info->pcm_name);
		exit(1);
	}
	
	if (snd_pcm_hw_params_any(info->pcm_handle, info->hwparams) < 0) {
		fprintf(stderr, "alsa_init: cannot configure this PCM device.\n");
		exit(1);
	}

	if (snd_pcm_hw_params_set_periods_integer(info->pcm_handle, info->hwparams) < 0) {
		fprintf(stderr, "alsa_init: cannot set period size to integer value.\n");
		exit(1);
	}

	if (snd_pcm_hw_params_set_access(info->pcm_handle, info->hwparams,
					 SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		fprintf(stderr, "alsa_init: error setting access to SND_PCM_ACCESS_RW_INTERLEAVED.\n");
		exit(1);
	}
  
	info->is_output_32bit = 1;
	if (snd_pcm_hw_params_set_format(info->pcm_handle, info->hwparams, SND_PCM_FORMAT_S32) < 0) {
		fprintf(stderr, "alsa_init: unable to open 32 bit output, falling back to 16 bit...\n");
		if (snd_pcm_hw_params_set_format(info->pcm_handle, info->hwparams, SND_PCM_FORMAT_S16) < 0) {
			fprintf(stderr, "alsa_init: unable to open 16 bit output, exiting.\n");
			exit(1);
		}
		info->is_output_32bit = 0;
	}

	rate = info->out_SR;
	dir = 0;
	if (snd_pcm_hw_params_set_rate_near(info->pcm_handle, info->hwparams, &rate, &dir) < 0) {
		fprintf(stderr, "alsa_init: can't set sample rate to %u.\n", rate);
		exit(1);
	}
	
	if (rate != info->out_SR) {
		fprintf(stderr, "Requested sample rate (%ld Hz) cannot be set, ", info->out_SR);
		fprintf(stderr, "using closest available rate (%d Hz).\n", rate);
		info->out_SR = rate;
	}

	if (snd_pcm_hw_params_set_channels(info->pcm_handle, info->hwparams, 2) < 0) {
		fprintf(stderr, "alsa_init: error setting channels.\n");
		exit(1);
	}

	if (snd_pcm_hw_params_set_periods(info->pcm_handle, info->hwparams, nperiods, 0) < 0) {
		fprintf(stderr, "alsa_init: error setting nperiods to %d.\n", nperiods);
		exit(1);
	}
  
	if (snd_pcm_hw_params_set_buffer_size(info->pcm_handle, info->hwparams,
					      (period * nperiods)>>2) < 0) {
		fprintf(stderr, "alsa_init: error setting buffersize to %d.\n", (period * nperiods)>>2);
		fprintf(stderr, "Parameters were: nperiods = %d, period = %d\n", nperiods, period);
		exit(1);
	}

	if (snd_pcm_hw_params(info->pcm_handle, info->hwparams) < 0) {
		fprintf(stderr, "alsa_init: error setting HW params.\n");
		exit(1);
	}
}
#endif /* HAVE_ALSA */


void
jack_init(thread_info_t * info) {

	if (client_name == NULL)
		client_name = strdup("aqualung");

	if ((jack_client = jack_client_new(client_name)) == 0) {
                fprintf(stderr, "\nAqualung couldn't connect to JACK.\n");
                fprintf(stderr, "There are several possible reasons:\n"
                        "\t1) JACK is not running.\n"
                        "\t2) JACK is running as another user, perhaps root.\n"
                        "\t3) There is already another client called '%s'. (Use the -c option!)\n",
			client_name);
                fprintf(stderr, "Please consider the possibilities, and perhaps (re)start JACK.\n");
		exit(1);
	}

	jack_set_process_callback(jack_client, process, info);
	jack_on_shutdown(jack_client, jack_shutdown, info);

        if ((info->out_SR = jack_get_sample_rate(jack_client)) > MAX_SAMPLERATE) {
                fprintf(stderr, "\nThe JACK sample rate (%ld Hz) is higher than MAX_SAMPLERATE.\n",
		       info->out_SR);
                fprintf(stderr, "This is an arbitrary limit, which you may safely enlarge "
                       "if you really need to.\n");
                fprintf(stderr, "Currently MAX_SAMPLERATE = %d Hz.\n", MAX_SAMPLERATE);
                jack_client_close(jack_client);
                exit(1);
        }
        out_L_port = jack_port_register(jack_client, "out_L", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        out_R_port = jack_port_register(jack_client, "out_R", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	jack_nframes = jack_get_buffer_size(jack_client);
	if ((l_buf = malloc(jack_nframes * sizeof(LADSPA_Data))) == NULL) {
		fprintf(stderr, "jack_init: malloc error\n");
		exit(1);
	}
	if ((r_buf = malloc(jack_nframes * sizeof(LADSPA_Data))) == NULL) {
		fprintf(stderr, "jack_init: malloc error\n");
		exit(1);
	}
	ladspa_buflen = jack_nframes;
}


void
jack_client_start(void) {

	const char ** ports_out;

	if (jack_activate(jack_client)) {
		fprintf(stderr, "Cannot activate JACK client.\n");
		exit(1);
	}
	if (auto_connect) {
		if (default_ports) {
			if ((ports_out = jack_get_ports(jack_client, NULL, NULL,
							JackPortIsPhysical|JackPortIsInput)) == NULL) {
				fprintf(stderr, "Cannot find any physical playback ports.\n");
			} else {
				if (jack_connect(jack_client, jack_port_name(out_L_port),
						 ports_out[0])) {
					fprintf(stderr, "Cannot connect out_L port to %s.\n",
						ports_out[0]);
				} else {
					fprintf(stderr, "Connected out_L to %s\n", ports_out[0]);
				}
				if (jack_connect(jack_client, jack_port_name(out_R_port),
						 ports_out[1])) {
					fprintf(stderr, "Cannot connect out_R port to %s.\n",
						ports_out[1]);
				} else {
					fprintf(stderr, "Connected out_R to %s\n", ports_out[1]);
				}
				free(ports_out);
			}
		} else {
			if (jack_connect(jack_client, jack_port_name(out_L_port),
					 user_port1)) {
				fprintf(stderr, "Cannot connect out_L port to %s.\n", user_port1);
			} else {
				fprintf(stderr, "Connected out_L to %s\n", user_port1);
			}
			if (jack_connect(jack_client, jack_port_name(out_R_port),
					 user_port2)) {
				fprintf(stderr, "Cannot connect out_R port to %s.\n", user_port2);
			} else {
				fprintf(stderr, "Connected out_R to %s\n", user_port2);
			}
		}
	}
}
 
 
void
load_default_cl(int * argc, char *** argv) {

	int i = 0;
	char * home;
	xmlDocPtr doc;
        xmlNodePtr cur;
        xmlNodePtr root;
        xmlChar * key;
        FILE * f;
	char confdir[MAXLEN];
        char config_file[MAXLEN];
	char default_param[MAXLEN];
	char cl[MAXLEN];
	int ret;

        if (!(home = getenv("HOME"))) {
                fprintf(stderr, "Warning: $HOME not set, using \".\" (current directory) as home\n");
                home = ".";
        }

        sprintf(confdir, "%s/.aqualung", home);
        if ((ret = chdir(confdir)) != 0) {
                if (errno == ENOENT) {
                        fprintf(stderr, "Creating directory %s\n", confdir);
                        mkdir(confdir, S_IRUSR | S_IWUSR | S_IXUSR);
                        chdir(confdir);
                } else {
                        fprintf(stderr, "An error occured while attempting chdir(\"%s\"). errno = %d\n",
                                confdir, errno);
                }
        }

        sprintf(config_file, "%s/.aqualung/config.xml", home);
        if ((f = fopen(config_file, "rt")) == NULL) {
                fprintf(stderr, "No config.xml -- creating empty one: %s\n", config_file);
                fprintf(stderr, "Wired-in defaults will be used.\n");
                doc = xmlNewDoc((const xmlChar *) "1.0");
                root = xmlNewNode(NULL, (const xmlChar *) "aqualung_config");
                xmlDocSetRootElement(doc, root);
                xmlSaveFormatFile(config_file, doc, 1);
		*argc = 1;
                return;
        }
        fclose(f);

        doc = xmlParseFile(config_file);
        if (doc == NULL) {
                fprintf(stderr, "An XML error occured while parsing %s\n", config_file);
                return;
        }
        cur = xmlDocGetRootElement(doc);
        if (cur == NULL) {
                fprintf(stderr, "load_config: empty XML document\n");
                xmlFreeDoc(doc);
                return;
        }

        if (xmlStrcmp(cur->name, (const xmlChar *)"aqualung_config")) {
                fprintf(stderr, "load_config: XML document of the wrong type, "
                        "root node != aqualung_config\n");
                xmlFreeDoc(doc);
                return;
        }

        default_param[0] = '\0';

        cur = cur->xmlChildrenNode;
        while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"default_param"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(default_param, (char *) key, MAXLEN-1);
                        xmlFree(key);
                }
                cur = cur->next;
        }

        xmlFreeDoc(doc);

	snprintf(cl, MAXLEN-1, "aqualung %s", default_param);
	while (cl[i] != '\0') {
		++(*argc);
		if ((*argv = realloc(*argv, *argc * sizeof(char *))) == NULL) {
			fprintf(stderr, "realloc() failed in load_default_cl()\n");
			exit(1);
		}
		(*argv)[*argc-1] = cl+i;
		
		while ((cl[i] != ' ') && (cl[i] != '\0'))
			++i;
		
		if (cl[i] == ' ')
			cl[i++] = '\0';
	}
	if ((*argv = realloc(*argv, (*argc+1) * sizeof(char *))) == NULL) {
		fprintf(stderr, "realloc() failed in load_default_cl()\n");
		exit(1);
	}
	(*argv)[*argc] = NULL;
	
        return;
}



int
main(int argc, char ** argv) {

	int parse_run;
	int * argc_opt = NULL;
	char *** argv_opt = NULL;
	int argc_def = 0;
	char ** argv_def = NULL;

	thread_info_t thread_info;
	struct sched_param param;

	int c;
	int longopt_index = 0;
	extern int optind, opterr;
	int show_version = 0;
	int show_usage = 0;
	char * output_str = NULL;
	int rate = 0;
	int try_realtime = 0;
	int priority = 1;

	char rcmd;
	int no_session = -1;
	int back = 0;
	int play = 0;
	int pause = 0;
	int stop = 0;
	int fwd = 0;
	int enqueue = 0;
	int remote_quit = 0;
	char * voladj_arg = NULL;

	char * optstring = "vho:d:c:n:p:r:a::RP:s::N:BLUTFEV:Q";
	struct option long_options[] = {
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, 'h' },
		{ "output", 1, 0, 'o' },
		{ "device", 1, 0, 'd' },
		{ "client", 1, 0, 'c' },
		{ "nperiods", 1, 0, 'n'},
		{ "period", 1, 0, 'p'},
		{ "rate", 1, 0, 'r' },
		{ "auto", 2, 0, 'a' },
		{ "realtime", 0, 0, 'R' },
		{ "priority", 1, 0, 'P' },
		{ "srctype", 2, 0, 's' },

		{ "session", 1, 0, 'N' },
		{ "back", 0, 0, 'B' },
		{ "play", 0, 0, 'L' },
		{ "pause", 0, 0, 'U' },
		{ "stop", 0, 0, 'T' },
		{ "fwd", 0, 0, 'F' },
		{ "enqueue", 0, 0, 'E' },
		{ "volume", 1, 0, 'V' },
		{ "quit", 0, 0, 'Q' },

		{ 0, 0, 0, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	
	setup_app_socket();

	if (getcwd(cwd, MAXLEN) == NULL) {
		fprintf(stderr, "main(): warning: getcwd() returned NULL, using . as cwd\n");
		strcpy(cwd, ".");
	}

	load_default_cl(&argc_def, &argv_def);

	for (parse_run = 0; parse_run < 2; parse_run++) {

		if ((!parse_run) && (argc_def == 1))
			++parse_run;

		switch (parse_run) {
		case 0:
			argc_opt = &argc_def;
			argv_opt = &argv_def;
			break;
		case 1:
			argc_opt = &argc;
			argv_opt = &argv;
			break;
		}

		optind = opterr = 0;
		longopt_index = opterr = 0;
		while ((c = getopt_long(*argc_opt, *argv_opt, optstring,
					long_options, &longopt_index)) != -1) {
			switch (c) {
			case 1:
				/* getopt signals end of '-' options */
				break;
				
			case 'h':
				show_usage++;
				break;
			case 'v':
				show_version++;
				break;
			case 'o':
				output_str = strdup(optarg);
				if (strcmp(output_str, "oss") == 0) {
#ifdef HAVE_OSS
					output = OSS_DRIVER;
#else
					fprintf(stderr,
						"You selected OSS output, but this instance of Aqualung "
						"is compiled\n"
						"without OSS output support. Type aqualung -v to get a "
						"list of\n"
						"compiled-in features.\n");
					exit(1);
#endif /* HAVE_OSS*/
					break;
				}
				if (strcmp(output_str, "alsa") == 0) {
#ifdef HAVE_ALSA
					output = ALSA_DRIVER;
#else
					fprintf(stderr,
						"You selected ALSA output, but this instance of Aqualung "
						"is compiled\n"
						"without ALSA output support. Type aqualung -v to get a "
						"list of\n"
						"compiled-in features.\n");
					exit(1);
#endif /* HAVE_ALSA */
					break;
				}
				if (strcmp(output_str, "jack") == 0) {
					output = JACK_DRIVER;
					break;
				}
			case 'd':
				device_name = strdup(optarg);
				break;
			case 'c':
				client_name = strdup(optarg);
				break;
			case 'n':
#ifdef HAVE_ALSA
				nperiods = atoi(optarg);
#endif /* HAVE_ALSA */
				break;
			case 'p':
#ifdef HAVE_ALSA
				period = atoi(optarg);
#endif /* HAVE_ALSA */
				break;
			case 'r':
				rate = atoi(optarg);
				break;
			case 'a':
				auto_connect = 1;
				if (optarg) {
					char * s;

					if (strstr(optarg, ",") == NULL) {
						fprintf(stderr, "Invalid ports specification: %s\n", optarg);
						exit(0);
					}

					s = strtok(optarg, ",");
					if (s != NULL) {
						user_port1 = strdup(s);
					} else {
						fprintf(stderr, "Invalid ports specification: argument "
							"contains too few ports\n");
						exit(0);
					}

					s = strtok(NULL, ",");
					if (s != NULL) {
						user_port2 = strdup(s);
					} else {
						fprintf(stderr, "Invalid ports specification: argument "
							"contains too few ports\n");
						exit(0);
					}

					default_ports = 0;
				}

				break;
			case 'R':
				try_realtime = 1;
				break;
			case 'P':
				priority = atoi(optarg);
				break;
			case 's':
#ifdef HAVE_SRC
				if (optarg) {
					src_type = atoi(optarg);
					++src_type_parsed;
				} else {
					int i = 0;
					
					fprintf(stderr, "List of available Sample Rate Converters:\n\n");
					while (src_get_name(i)) {
						fprintf(stderr, "Converter #%d: %s\n%s\n\n",
						       i, src_get_name(i), src_get_description(i));
						++i;
					}
					fprintf(stderr,
						"Type aqualung --srctype=n ... or aqualung -sn ... to choose "
						"Converter #n.\n\n");
					exit(0);
				}
#else
				if (optarg) {
					fprintf(stderr,
						"You attempted to set the type of the internal Sample Rate "
						"Converter,"
						"\nbut this instance of Aqualung is compiled without "
						"support for"
						"\ninternal Sample Rate Conversion. Type aqualung -v to get "
						"a list"
						"\nof compiled-in features.\n");
				} else {
					fprintf(stderr,
						"You attempted to list the available types for the internal "
						"Sample"
						"\nRate Converter, but this instance of Aqualung is "
						"compiled without"
						"\nsupport for internal Sample Rate Conversion. Type "
						"aqualung -v to"
						"\nget a list of compiled-in features.\n");
				}
				exit(1);
#endif /* HAVE_SRC */
				break;
				
			case 'N':
				no_session = atoi(optarg);
				break;
			case 'B':
				back++;
				break;
			case 'L':
				play++;
				break;
			case 'U':
				pause++;
				break;
			case 'T':
				stop++;
				break;
			case 'F':
				fwd++;
				break;
			case 'E':
				enqueue++;
				break;
			case 'V':
				voladj_arg = strdup(optarg);
				break;
			case 'Q':
				remote_quit++;
				break;

			default:
				show_usage++;
				break;
			}
		}
	}
	
	if (show_version) {
		fprintf(stderr,	"Aqualung -- Music player for GNU/Linux\n");
		fprintf(stderr, "Build version: %s\n", aqualung_version);
		fprintf(stderr,
			"(C) 2004-2005 Tom Szilagyi <tszilagyi@users.sourceforge.net>\n"
			"This is free software, and you are welcome to redistribute it\n"
			"under certain conditions; see the file COPYING for details.\n");

		fprintf(stderr, "\nThis Aqualung binary is compiled with:\n"
			"\n\tFile format support:\n");

		fprintf(stderr, "\t\tsndfile (WAV, AIFF, etc.)           : ");
#ifdef HAVE_SNDFILE
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_SNDFILE */

		fprintf(stderr, "\t\tFree Lossless Audio Codec (FLAC)    : ");
#ifdef HAVE_FLAC
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_FLAC */

		fprintf(stderr, "\t\tOgg Vorbis                          : ");
#ifdef HAVE_OGG_VORBIS
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_OGG_VORBIS */

		fprintf(stderr, "\t\tOgg Speex                           : ");
#ifdef HAVE_SPEEX
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_SPEEX */

		fprintf(stderr, "\t\tMPEG Audio (MPEG 1-2.5 Layer I-III) : ");
#ifdef HAVE_MPEG
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_MPEG */

                fprintf(stderr, "\t\tMOD Audio (MOD, S3M, XM, IT, etc.)  : ");
#ifdef HAVE_MOD
                fprintf(stderr, "yes\n");
#else
                fprintf(stderr, "no\n");
#endif /* HAVE_MOD */

                fprintf(stderr, "\t\tMusepack                            : ");
#ifdef HAVE_MPC
                fprintf(stderr, "yes\n");
#else
                fprintf(stderr, "no\n");
#endif /* HAVE_MPC */

                fprintf(stderr, "\t\tMonkey's Audio Codec                : ");
#ifdef HAVE_MAC
                fprintf(stderr, "yes\n");
#else
                fprintf(stderr, "no\n");
#endif /* HAVE_MAC */

                fprintf(stderr, "\t\tID3 tags                            : ");
#ifdef HAVE_ID3
                fprintf(stderr, "yes\n");
#else
                fprintf(stderr, "no\n");
#endif /* HAVE_ID3 */

		fprintf(stderr, "\n\tOutput driver support:\n");

		fprintf(stderr, "\t\tOSS Audio                           : ");
#ifdef HAVE_OSS
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_OSS */

		fprintf(stderr, "\t\tALSA Audio                          : ");
#ifdef HAVE_ALSA
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_ALSA */

		fprintf(stderr, "\t\tJACK Audio Server                   : yes (always)\n");

		fprintf(stderr, "\n\tInternal Sample Rate Converter support      : ");
#ifdef HAVE_SRC
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_SRC */

		fprintf(stderr, "\tCDDB support                                : ");
#ifdef HAVE_CDDB
		fprintf(stderr, "yes\n\n");
#else
		fprintf(stderr, "no\n\n");
#endif /* HAVE_CDDB */

		exit(1);
	}

	if (show_usage)
		goto show_usage_;


	if (back) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_BACK;
		send_message_to_session(no_session, &rcmd, 1);
		exit(0);
	}
	if (pause) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_PAUSE;
		send_message_to_session(no_session, &rcmd, 1);
		exit(0);
	}
	if (stop) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_STOP;
		send_message_to_session(no_session, &rcmd, 1);
		exit(0);
	}
	if (fwd) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_FWD;
		send_message_to_session(no_session, &rcmd, 1);
		exit(0);
	}

	if (remote_quit) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_QUIT;
		send_message_to_session(no_session, &rcmd, 1);
		exit(1);
	}

	if (voladj_arg) {
		char buf[MAXLEN];

		if (no_session == -1)
			no_session = 0;
		buf[0] = RCMD_VOLADJ;
		buf[1] = '\0';
		strncat(buf, voladj_arg, MAXLEN-1);
		send_message_to_session(no_session, buf, strlen(buf));
		exit(1);
	}

	{
		int i;
		char buffer[MAXLEN];
		char fullname[MAXLEN];
		char * home;
		char * path;

		if ((no_session != -1) && (no_session != aqualung_session_id)) {
			for (i = optind; argv[i] != NULL; i++) {
				
				switch (argv[i][0]) {
				case '/':
					strcpy(fullname, argv[i]);
					break;
				case '~':
					path = argv[i];
					++path;
					if (!(home = getenv("HOME"))) {
						fprintf(stderr,	"main(): cannot resolve home directory\n");
						return 1;
					}
					snprintf(fullname, MAXLEN-1, "%s/%s", home, path);
					break;
				default:
					snprintf(fullname, MAXLEN-1, "%s/%s", cwd, argv[i]);
					break;
				}

				if ((enqueue) || (i > optind)) {
					buffer[0] = RCMD_ENQUEUE;
					buffer[1] = '\0';
					strncat(buffer, fullname, MAXLEN-1);
					send_message_to_session(no_session, buffer, strlen(buffer));
				} else {
					buffer[0] = RCMD_LOAD;
					buffer[1] = '\0';
					strncat(buffer, fullname, MAXLEN-1);
					send_message_to_session(no_session, buffer, strlen(buffer));
				}
			}
			if (play) {
				rcmd = RCMD_PLAY;
				send_message_to_session(no_session, &rcmd, 1);
			}
			exit(0);
		}
	}

	if (play) {
		if ((no_session == -1) || (no_session == aqualung_session_id)) {
			immediate_start = 1;
		} else {
			rcmd = RCMD_PLAY;
			send_message_to_session(no_session, &rcmd, 1);
			exit(0);
		}
	}


 show_usage_:
	if (show_usage || (output == 0)) {
		fprintf(stderr,	"Aqualung -- Music player for GNU/Linux\n");
		fprintf(stderr, "Build version: %s\n", aqualung_version);
		fprintf(stderr,
			"(C) 2004-2005 Tom Szilagyi <tszilagyi@users.sourceforge.net>\n"
			"This is free software, and you are welcome to redistribute it\n"
			"under certain conditions; see the file COPYING for details.\n");

		fprintf(stderr,
			"\nInvocation:\n"
			"aqualung --output (oss|alsa|jack) [options] [file1 [file2 ...]]\n"
			"aqualung --help\n"
			"aqualung --version\n"

			"\nOptions relevant to ALSA output:\n"
			"-d, --device <name>: Set the output device (defaults to plughw:0,0).\n"
			"-p, --period <int>: Set ALSA period size (defaults to 8192).\n"
			"-n, --nperiods <int>: Specify the number of periods in hardware buffer (defaults to 2).\n"
			"-r, --rate <int>: Set the output sample rate.\n"
			"-R, --realtime: Try to use realtime (SCHED_FIFO) scheduling for ALSA output thread.\n"
			"-P, --priority <int>: When running --realtime, set scheduler priority to <int> (defaults to 1).\n"

			"\nOptions relevant to OSS output:\n"
			"-d, --device <name>: Set the output device (defaults to /dev/dsp).\n"
			"-r, --rate <int>: Set the output sample rate.\n"
			
			"\nOptions relevant to JACK output:\n"
			"-a[<port_L>,<port_R>], --auto[=<port_L>,<port_R>]: Auto-connect output ports to\n"
			"given JACK ports (defaults to first two hardware playback ports).\n"
			"-c, --client <name>: Set client name (needed if you want to run multiple instances of the program).\n"

			"\nOptions relevant to the Sample Rate Converter:\n"
			"-s[<int>], --srctype[=<int>]: Choose the SRC type, or print the list of available\n"
			"types if no number given. The default is SRC type 4 (Linear Interpolator).\n"

			"\nOptions for remote cue control:\n"
			"-N, --session <int>: Number of Aqualung instance to control.\n"
			"-B, --back: Jump to previous track.\n"
			"-F, --fwd: Jump to next track.\n"
			"-L, --play: Start playing.\n"
			"-U, --pause: Pause playback, or resume if already paused.\n"
			"-T, --stop: Stop playback.\n"
			"-V, --volume [m|M]|[=]<val>: Set, adjust or mute volume.\n"
			"-Q, --quit: Terminate remote instance.\n"

			"Note that these options default to the 0-th instance when no -N option is given,\n"
			"except for -L which defaults to the present instance (so as to be able to start\n"
			"playback immediately from the command line).\n"

			"\nOptions for file loading:\n"
			"-E, --enqueue: Don't clear the contents of the playlist when adding new items.\n"

			"\nIf you don't specify a session number via --session, the files will be opened by "
			"the new\ninstance, otherwise they will be sent to the already running instance you "
			"specify.\n"

			"\nExamples:\n"
			"$ aqualung -s3 -o alsa -R -r 48000 -d hw:0,0 -p 2048 -n 2\n"
			"$ aqualung --srctype=1 --output oss --rate 96000\n"
			"$ aqualung -o jack -a -E `find ./ledzeppelin/ -name \"*.flac\"`\n");

		fprintf(stderr, 
			"\nDepending on the compile-time options, not all file formats\n"
			"and output drivers may be usable. Type aqualung -v to get a\n"
			"list of all the compiled-in features.\n\n");

		exit (0);
	}

	
	if ((output == JACK_DRIVER) && (rate > 0)) {
		fprintf(stderr,
			"You attempted to set the output rate for the JACK output.\n"
			"We won't do this; please use the --rate option with the\n"
			"oss and alsa outputs only.\n"
			"In case of the JACK output, the (already running) JACK server\n"
			"will determine the output sample rate to use.\n");
		exit(1);
	}

#ifdef HAVE_OSS
	if ((output == OSS_DRIVER) && (rate == 0)) {
		rate = 44100;
	}
#endif /* HAVE_OSS */

#ifdef HAVE_ALSA
	if ((output == ALSA_DRIVER) && (rate == 0)) {
		rate = 44100;
	}
#endif /* HAVE_ALSA */


	if (device_name == NULL) {
#ifdef HAVE_OSS
		if (output == OSS_DRIVER) {
			device_name = strdup("/dev/dsp");
		}
#endif /* HAVE_OSS */
#ifdef HAVE_ALSA
		if (output == ALSA_DRIVER) {
			device_name = strdup("plughw:0,0");
		}
#endif /* HAVE_ALSA */
	}

#ifdef HAVE_ALSA
	if (output == ALSA_DRIVER) {
		if (period == 0) {
			period = 8192;
		}
		if (nperiods == 0) {
			nperiods = 2;
		}
	}
#endif /* HAVE_ALSA */


	memset(&thread_info, 0, sizeof(thread_info));

	/* JACK */
	if (output == JACK_DRIVER) {
		jack_init(&thread_info);
		rate = thread_info.out_SR;
	}


	/* ALSA */
#ifdef HAVE_ALSA
	if (output == ALSA_DRIVER) {
		thread_info.out_SR = rate;
		thread_info.pcm_name = strdup(device_name);
		alsa_init(&thread_info);
	}
#endif /* HAVE_ALSA */


	/* OSS */
#ifdef HAVE_OSS
	if (output == OSS_DRIVER) {
		thread_info.out_SR = rate;
	}
#endif /* HAVE_OSS */


	thread_info.rb_size = RB_AUDIO_SIZE * thread_info.out_SR / 44100.0;

        rb = jack_ringbuffer_create(2*sample_size * thread_info.rb_size);
	memset(rb->buf, 0, rb->size);


	rb_disk2gui = jack_ringbuffer_create(RB_CONTROL_SIZE);
	memset(rb_disk2gui->buf, 0, rb_disk2gui->size);

	rb_gui2disk = jack_ringbuffer_create(RB_CONTROL_SIZE);
	memset(rb_gui2disk->buf, 0, rb_gui2disk->size);

	rb_disk2out = jack_ringbuffer_create(RB_CONTROL_SIZE);
	memset(rb_disk2out->buf, 0, rb_disk2out->size);

	thread_info.is_streaming = 0;
	thread_info.is_mono = 0;
	thread_info.in_SR = 0;


	/* startup disk thread */
	pthread_create(&thread_info.disk_thread_id, NULL, disk_thread, &thread_info);


	/* OSS */
#ifdef HAVE_OSS
	if (output == OSS_DRIVER) {
		oss_init(&thread_info);
	}
#endif /* HAVE_OSS */


	/* ALSA */
#ifdef HAVE_ALSA
	if (output == ALSA_DRIVER) {
		pthread_create(&thread_info.alsa_thread_id, NULL, alsa_thread, &thread_info);

		if (try_realtime) {
			int x;
			memset(&param, 0, sizeof(param));
			param.sched_priority = priority;
			if ((x = pthread_setschedparam(thread_info.alsa_thread_id,
						       SCHED_FIFO, &param)) != 0) {
				fprintf(stderr,
					"Cannot use real-time scheduling for ALSA output thread (FIFO/%d) "
					"(%d: %s)\n", param.sched_priority, x, strerror(x));
				exit(1);
			}
		}
	}
#endif /* HAVE_ALSA */


	create_gui(argc, argv, optind, enqueue, rate, RB_AUDIO_SIZE * rate / 44100.0);
	run_gui(); /* control stays here until user exits program */

	pthread_join(thread_info.disk_thread_id, NULL);

	/* OSS */
#ifdef HAVE_OSS
	if (output == OSS_DRIVER) {
		pthread_cancel(thread_info.oss_thread_id);
		free(thread_info.oss_short_buf);
		ioctl(thread_info.fd_oss, SNDCTL_DSP_RESET, 0);
		close(thread_info.fd_oss);
	}
#endif /* HAVE_OSS */


	/* ALSA */
#ifdef HAVE_ALSA
	if (output == ALSA_DRIVER) {
		pthread_cancel(thread_info.alsa_thread_id);
		snd_pcm_close(thread_info.pcm_handle);
		if (thread_info.is_output_32bit)
			free(thread_info.alsa_int_buf);
		else
			free(thread_info.alsa_short_buf);
	}
#endif /* HAVE_ALSA */


	/* JACK */
	if (output == JACK_DRIVER) {
		jack_client_close(jack_client);
		free(client_name);
	}


	if (device_name != NULL)
		free(device_name);

	free(l_buf);
	free(r_buf);
	jack_ringbuffer_free(rb);
	jack_ringbuffer_free(rb_disk2gui);
	jack_ringbuffer_free(rb_gui2disk);
	jack_ringbuffer_free(rb_disk2out);
	return 0;
}
