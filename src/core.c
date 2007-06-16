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
#include <getopt.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#ifdef _WIN32
#include <glib.h>
#else
#include <pthread.h>
#endif /* _WIN32 */

#ifdef HAVE_SRC
#include <samplerate.h>
#endif /* HAVE_SRC */

#ifdef HAVE_OSS
#include <sys/ioctl.h>
#include <sys/types.h>
#ifdef __FreeBSD__
#include <sys/soundcard.h>
#else
#include <linux/soundcard.h>
#endif /* __FreeBSD__ */
#endif /* HAVE_OSS */

#ifdef HAVE_JACK
#include <sys/mman.h>
#include <jack/jack.h>
#endif /* HAVE_JACK */

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif /* _WIN32 */

#include "common.h"
#include "utils.h"
#include "version.h"
#include "rb.h"
#include "options.h"
#include "decoder/file_decoder.h"
#include "transceiver.h"
#include "gui_main.h"
#include "plugin.h"
#include "i18n.h"
#include "cdda.h"
#ifdef HAVE_CDDA
#include "decoder/dec_cdda.h"
#endif /* HAVE_CDDA */
#include "core.h"

extern options_t options;

/* JACK data */
#ifdef HAVE_JACK
jack_client_t * jack_client;
jack_port_t * out_L_port;
jack_port_t * out_R_port;
char * client_name = NULL;
u_int32_t jack_nframes;
int jack_is_shutdown;
int auto_connect = 0;
int default_ports = 1;
char * user_port1 = NULL;
char * user_port2 = NULL;
#endif /* HAVE_JACK */

const size_t sample_size = sizeof(float);

gint playlist_state, browser_state;

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

int src_type_parsed = 0;

/* Synchronization between disk thread and output thread */
AQUALUNG_MUTEX_DECLARE_INIT(disk_thread_lock)
AQUALUNG_COND_DECLARE_INIT(disk_thread_wake)
rb_t * rb; /* this is the audio stream carrier ringbuffer */
rb_t * rb_disk2out;

/* Communication between gui thread and disk thread */
rb_t * rb_gui2disk;
rb_t * rb_disk2gui;

/* Lock critical operations that could interfere with output thread */
double left_gain = 1.0;
double right_gain = 1.0;

/* LADSPA stuff */
float * l_buf = NULL;
float * r_buf = NULL;
#ifdef HAVE_LADSPA
volatile int plugin_lock = 0;
unsigned long ladspa_buflen = 0;
int n_plugins = 0;
plugin_instance * plugin_vect[MAX_PLUGINS];
#endif /* HAVE_LADSPA */

/* remote control */
extern char * tab_name;
extern int immediate_start;
extern int aqualung_session_id;
extern int aqualung_socket_fd;
extern char aqualung_socket_filename[256];


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
			"Input file's samplerate (%d Hz) and output samplerate (%d Hz) differ, "
			"and\nAqualung is compiled without Sample Rate Converter support. To play "
			"this file,\nyou have to build Aqualung with internal Sample Rate Converter "
			"support,\nor set the playback sample rate to match the file's sample rate."
			"\n", file_SR, out_SR);             
#endif /* HAVE_SRC */
		return 0;
	}
	return 1;
}


#ifdef HAVE_CDDA
int
same_disc_next_track(char * filename, char * filename_prev) {

	char device[CDDA_MAXLEN];
	char device_prev[CDDA_MAXLEN];
	int track, track_prev;
	long hash, hash_prev;

	if ((strlen(filename) < 4) ||
	    (filename[0] != 'C') ||
	    (filename[1] != 'D') ||
	    (filename[2] != 'D') ||
	    (filename[3] != 'A'))
		return 0;

	if ((strlen(filename_prev) < 4) ||
	    (filename_prev[0] != 'C') ||
	    (filename_prev[1] != 'D') ||
	    (filename_prev[2] != 'D') ||
	    (filename_prev[3] != 'A'))
		return 0;

        if (sscanf(filename, "CDDA %s %lX %u", device, &hash, &track) < 3)
		return 0;

        if (sscanf(filename_prev, "CDDA %s %lX %u", device_prev, &hash_prev, &track_prev) < 3)
		return 0;

	if (strcmp(device, device_prev) != 0)
		return 0;

	if (hash != hash_prev)
		return 0;

	return (track == track_prev + 1) ? 1 : 0;
}
#endif /* HAVE_CDDA */
 

/* roll back sample_offset samples, if possible */
void
rollback(rb_t * rb, file_decoder_t * fdec, double src_ratio) {

	sample_offset = rb_read_space(rb) /
		(2 * sample_size) * src_ratio;
	if (sample_offset < fdec->fileinfo.total_samples - fdec->samples_left)
		file_decoder_seek(fdec, fdec->fileinfo.total_samples - fdec->samples_left - sample_offset);
	else
		file_decoder_seek(fdec, 0);
}


void
send_meta(metadata_t * meta) {

	char send_cmd = CMD_METABLOCK;
	rb_write(rb_disk2gui, &send_cmd, 1);
	rb_write(rb_disk2gui, (char *)&meta, sizeof(metadata_t *));
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
#ifdef HAVE_CDDA
	int flowthrough = 0;
	char filename_prev[RB_CONTROL_SIZE];
#endif /* HAVE_CDDA */
	seek_t seek;
	cue_t cue;
	int i;


#ifdef HAVE_SRC
	int src_type_prev;
	SRC_STATE * src_state;
	SRC_DATA src_data;
	int src_error;

        if ((src_state = src_new(options.src_type, 2, &src_error)) == NULL) {
		fprintf(stderr, "disk thread: error: src_new() failed: %s.\n",
			src_strerror(src_error));
		exit(1);
	}
	src_type_prev = options.src_type;
#endif /* HAVE_SRC */

	if ((fdec = file_decoder_new()) == NULL) {
		fprintf(stderr, "disk thread: error: file_decoder_new() failed\n");
		exit(1);
	}
	file_decoder_set_meta_cb(fdec, send_meta);

	if ((!readbuf) || (!framebuf)) {
		fprintf(stderr, "disk thread: malloc error\n");
		exit(1);
	}

	AQUALUNG_MUTEX_LOCK(disk_thread_lock)

	filename[0] = '\0';
#ifdef HAVE_CDDA
	filename_prev[0] = '\0';
#endif /* HAVE_CDDA */

	while (1) {

		recv_cmd = 0;
		if (rb_read_space(rb_gui2disk) > 0) {
			rb_read(rb_gui2disk, &recv_cmd, 1);
			switch (recv_cmd) {
			case CMD_CUE:
				/* read the string */
				while (rb_read_space(rb_gui2disk) < sizeof(cue_t))
					;

				rb_read(rb_gui2disk, (void *)&cue, sizeof(cue_t));
				
#ifdef HAVE_CDDA
				strcpy(filename_prev, filename);
#endif /* HAVE_CDDA */
				if (cue.filename != NULL) {
					strncpy(filename, cue.filename, MAXLEN-1);
					free(cue.filename);
				} else {
					filename[0] = '\0';
				}

#ifdef HAVE_CDDA
				if (!flowthrough || !same_disc_next_track(filename, filename_prev)) {
					if (fdec->file_lib != 0)
						file_decoder_close(fdec);
				}
#else
				if (fdec->file_lib != 0)
					file_decoder_close(fdec);
#endif /* HAVE_CDDA */

				if (filename[0] != '\0') {
#ifdef HAVE_CDDA
					if (flowthrough &&
					    same_disc_next_track(filename, filename_prev) &&
					    (fdec->pdec != NULL)) {

						decoder_t * dec = (decoder_t *)fdec->pdec;
						
						cdda_decoder_reopen(dec, filename);
						fdec->samples_left = fdec->fileinfo.total_samples;

						file_decoder_set_rva(fdec, cue.voladj);
						fdec->sample_pos = 0;

						sample_offset = 0;

						send_cmd = CMD_FILEINFO;
						rb_write(rb_disk2gui, &send_cmd,
								      sizeof(send_cmd));
						rb_write(rb_disk2gui,
								      (char *)&(fdec->fileinfo),
								      sizeof(fileinfo_t));

						info->is_streaming = 1;
						end_of_file = 0;
					} else {
#endif /* HAVE_CDDA */
					if (file_decoder_open(fdec, filename)) {
						fdec->samples_left = 0;
						info->is_streaming = 0;
						end_of_file = 1;
						send_cmd = CMD_FILEREQ;
						rb_write(rb_disk2gui, &send_cmd, 1);
						goto sleep;
					} else if (!sample_rates_ok(info->out_SR,
								    fdec->fileinfo.sample_rate)) {
						fdec->file_open = 1; /* to get close_file() working */
						file_decoder_close(fdec);
						fdec->file_open = 0;

						fdec->samples_left = 0;
						info->is_streaming = 0;
						end_of_file = 1;
						send_cmd = CMD_FILEREQ;
						rb_write(rb_disk2gui, &send_cmd, 1);
						goto sleep;
					} else {
						file_decoder_set_rva(fdec, cue.voladj);
						info->in_SR_prev = info->in_SR;
						info->in_SR = fdec->fileinfo.sample_rate;
						info->is_mono = fdec->fileinfo.is_mono;
						fdec->sample_pos = 0;
#ifdef HAVE_CDDA
						if (fdec->file_lib == CDDA_LIB) {
							cdda_decoder_set_mode(((decoder_t *)fdec->pdec),
									      options.cdda_drive_speed,
									      options.cdda_paranoia_mode,
									      options.cdda_paranoia_maxretries);
						}
#endif /* HAVE_CDDA */

						sample_offset = 0;

						send_cmd = CMD_FILEINFO;
						rb_write(rb_disk2gui, &send_cmd,
								      sizeof(send_cmd));
						rb_write(rb_disk2gui,
								      (char *)&(fdec->fileinfo),
								      sizeof(fileinfo_t));

						info->is_streaming = 1;
						end_of_file = 0;
					}
#ifdef HAVE_CDDA
					}
					flowthrough = 0;
#endif /* HAVE_CDDA */
				} else { /* STOP */
					info->is_streaming = 0;

					/* send a FLUSH command to output thread to stop immediately */
					send_cmd = CMD_FLUSH;
					rb_write(rb_disk2out, &send_cmd, 1);
					goto sleep;
				}
				break;
			case CMD_STOPWOFL: /* STOP but first flush output ringbuffer. */
				info->is_streaming = 0;
				if (fdec->file_lib != 0)
					file_decoder_close(fdec);
				goto flush;
				break;
			case CMD_PAUSE:
				info->is_streaming = 0;

				/* send a FLUSH command to output thread */
				send_cmd = CMD_FLUSH;
				rb_write(rb_disk2out, &send_cmd, 1);

				rollback(rb, fdec, src_ratio);
				if (fdec->is_stream) {
					file_decoder_pause(fdec);
				}
				break;
			case CMD_RESUME:
				info->is_streaming = 1;
				if (fdec->is_stream) {
					file_decoder_resume(fdec);
				}
				break;
			case CMD_FINISH:
				/* send FINISH to output thread, then goto exit */
				send_cmd = CMD_FINISH;
				rb_write(rb_disk2out, &send_cmd, 1);
				goto done;
				break;
			case CMD_SEEKTO:
				while (rb_read_space(rb_gui2disk) < sizeof(seek_t))
					;
				rb_read(rb_gui2disk, (char *)&seek, sizeof(seek_t));
				if (fdec->file_lib != 0) {
					file_decoder_seek(fdec, seek.seek_to_pos);
					/* send a FLUSH command to output thread */
					send_cmd = CMD_FLUSH;
					rb_write(rb_disk2out, &send_cmd, 1);

					if (fdec->is_stream && !info->is_streaming) {
						file_decoder_pause(fdec);
					}
				} else {
					/* send dummy STATUS to gui, to set pos slider to zero */
					disk_thread_status.samples_left = 0;
					disk_thread_status.sample_offset = 0;
					send_cmd = CMD_STATUS;
					rb_write(rb_disk2gui, &send_cmd, sizeof(send_cmd));
					rb_write(rb_disk2gui, (char *)&disk_thread_status,
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

		n_read = 0;
		n_space = rb_write_space(rb) / (2 * sample_size);
		while (n_src < 0.95 * n_space) {
			
			src_ratio = (double)info->out_SR / (double)info->in_SR;
			n_src_prev = n_src;
			want_read = floor((n_space - n_src) / src_ratio);

			if (want_read == 0)
				break;

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
				    (src_type_prev != options.src_type)) { /* reinit SRC */

					src_state = src_delete(src_state);
					if ((src_state = src_new(options.src_type, 2, &src_error)) == NULL) {
						fprintf(stderr, "disk thread: error: src_new() failed: "
						       "%s.\n", src_strerror(src_error));
						goto done;
					}
					info->in_SR_prev = info->in_SR;
					src_type_prev = options.src_type;
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
				/* send request for a new filename */
#ifdef HAVE_CDDA
				flowthrough = 1;
#endif /* HAVE_CDDA */
				send_cmd = CMD_FILEREQ;
				rb_write(rb_disk2gui, &send_cmd, 1);
				goto sleep;
			}
		}

	flush:
		rb_write(rb, framebuf, n_src * 2*sample_size);

		/* update & send STATUS */
		fdec->sample_pos += n_read;
		if (fdec->samples_left > n_read)
			fdec->samples_left -= n_read;
		sample_offset =	rb_read_space(rb) / (2 * sample_size);
		disk_thread_status.sample_pos = fdec->sample_pos;
		disk_thread_status.samples_left = fdec->samples_left;
		disk_thread_status.sample_offset = sample_offset / src_ratio;
		if (disk_thread_status.samples_left < 0) {
			disk_thread_status.samples_left = 0;
		}

		if (!rb_read_space(rb_gui2disk)) {
			send_cmd = CMD_STATUS;
			rb_write(rb_disk2gui, &send_cmd, sizeof(send_cmd));
			rb_write(rb_disk2gui, (char *)&disk_thread_status,
					      sizeof(status_t));
		}

		/* cleanup buffer counters */
		n_src = 0;
		n_src_prev = 0;
		end_of_file = 0;
		
	sleep:
		{
			/* suspend thread, wake up after 100 ms */
#ifdef _WIN32
			GTimeVal time;
			GTimeVal * timeout = &time;
			g_get_current_time(timeout);
			g_time_val_add(timeout, 100000);
#else
			struct timeval now;
			struct timezone tz;
			struct timespec timeout;
			gettimeofday(&now, &tz);
			timeout.tv_nsec = now.tv_usec * 1000 + 100000000;
			timeout.tv_sec = now.tv_sec;
			while (timeout.tv_nsec > 1000000000) {
				timeout.tv_nsec -= 1000000000;
				timeout.tv_sec += 1;
			}
#endif /* _WIN32 */
			AQUALUNG_COND_TIMEDWAIT(disk_thread_wake, disk_thread_lock, timeout)
		}
	}
 done:
	free(readbuf);
	free(framebuf);
#ifdef HAVE_SRC
	src_state = src_delete(src_state);
#endif /* HAVE_SRC */
	file_decoder_delete(fdec);
	AQUALUNG_MUTEX_UNLOCK(disk_thread_lock)
	return 0;
}



/* OSS output thread */
#ifdef HAVE_OSS
void *
oss_thread(void * arg) {

        u_int32_t i;
        thread_info_t * info = (thread_info_t *)arg;
	int bufsize = 1024;
        int n_avail;
	int ioctl_status;
	char recv_cmd;

	int fd_oss = info->fd_oss;
	short * oss_short_buf;

	struct timespec req_time;
	struct timespec rem_time;
	req_time.tv_sec = 0;
        req_time.tv_nsec = 100000000;

	if ((info->oss_short_buf = malloc(2*bufsize * sizeof(short))) == NULL) {
		fprintf(stderr, "oss_thread: malloc error\n");
		exit(1);
	}
	oss_short_buf = info->oss_short_buf;

	if ((l_buf = malloc(bufsize * sizeof(float))) == NULL) {
		fprintf(stderr, "oss_thread: malloc error\n");
		exit(1);
	}
	if ((r_buf = malloc(bufsize * sizeof(float))) == NULL) {
		fprintf(stderr, "oss_thread: malloc error\n");
		exit(1);
	}
#ifdef HAVE_LADSPA
	ladspa_buflen = bufsize;
#endif /* HAVE_LADSPA */


	while (1) {
	oss_wake:
		while (rb_read_space(rb_disk2out)) {
			rb_read(rb_disk2out, &recv_cmd, 1);
			switch (recv_cmd) {
			case CMD_FLUSH:
				while ((n_avail = rb_read_space(rb)) > 0) {
					if (n_avail > 2*bufsize * sizeof(short))
						n_avail = 2*bufsize * sizeof(short);
					rb_read(rb, (char *)oss_short_buf,
							     2*bufsize * sizeof(short));
				}
				goto oss_wake;
				break;
			case CMD_FINISH:
				goto oss_finish;
				break;
			default:
				fprintf(stderr, "oss_thread: recv'd unknown command %d\n", recv_cmd);
				break;
			}
		}

		if ((n_avail = rb_read_space(rb) / (2*sample_size)) == 0) {
			nanosleep(&req_time, &rem_time);
			goto oss_wake;
		}

		if (n_avail > bufsize)
			n_avail = bufsize;
		
		for (i = 0; i < n_avail; i++) {
			rb_read(rb, (char *)&(l_buf[i]), sample_size);
			rb_read(rb, (char *)&(r_buf[i]), sample_size);
		}

#ifdef HAVE_LADSPA
		if (options.ladspa_is_postfader) {
			for (i = 0; i < n_avail; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}
#else
		for (i = 0; i < n_avail; i++) {
			l_buf[i] *= left_gain;
			r_buf[i] *= right_gain;
		}
#endif /* HAVE_LADSPA */

		if (n_avail < bufsize) {
			for (i = n_avail; i < bufsize; i++) {
				l_buf[i] = 0.0f;
				r_buf[i] = 0.0f;
			}
		}

		/* plugin processing */
#ifdef HAVE_LADSPA
		plugin_lock = 1;
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
		plugin_lock = 0;

		if (!options.ladspa_is_postfader) {
			for (i = 0; i < bufsize; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}
#endif /* HAVE_LADSPA */

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
		ioctl_status = write(fd_oss, oss_short_buf, 2*n_avail * sizeof(short));
		if (ioctl_status != 2*n_avail * sizeof(short))
			fprintf(stderr, "oss_thread: Error writing to audio device\n");

	}
 oss_finish:
	return 0;
}
#endif /* HAVE_OSS */



/* ALSA output thread */
#ifdef HAVE_ALSA
void *
alsa_thread(void * arg) {

        u_int32_t i;
        thread_info_t * info = (thread_info_t *)arg;
	snd_pcm_sframes_t n_written = 0;
	int bufsize = 1024;
        int n_avail;
	char recv_cmd;

	int is_output_32bit = info->is_output_32bit;
	short * alsa_short_buf = NULL;
	int * alsa_int_buf = NULL;

	snd_pcm_t * pcm_handle = info->pcm_handle;

	struct timespec req_time;
	struct timespec rem_time;
	req_time.tv_sec = 0;
        req_time.tv_nsec = 100000000;

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

	if ((l_buf = malloc(bufsize * sizeof(float))) == NULL) {
		fprintf(stderr, "alsa_thread: malloc error\n");
		exit(1);
	}
	if ((r_buf = malloc(bufsize * sizeof(float))) == NULL) {
		fprintf(stderr, "alsa_thread: malloc error\n");
		exit(1);
	}
#ifdef HAVE_LADSPA
	ladspa_buflen = bufsize;
#endif /* HAVE_LADSPA */


	while (1) {
	alsa_wake:
		while (rb_read_space(rb_disk2out)) {
			rb_read(rb_disk2out, &recv_cmd, 1);
			switch (recv_cmd) {
			case CMD_FLUSH:
				if (is_output_32bit) {
					while ((n_avail = rb_read_space(rb)) > 0) {
						if (n_avail > 2*bufsize * sizeof(int))
							n_avail = 2*bufsize * sizeof(int);
						rb_read(rb, (char *)alsa_int_buf,
								     2*bufsize * sizeof(int));
					}
				} else {
					while ((n_avail = rb_read_space(rb)) > 0) {
						if (n_avail > 2*bufsize * sizeof(short))
							n_avail = 2*bufsize * sizeof(short);
						rb_read(rb, (char *)alsa_short_buf,
								     2*bufsize * sizeof(short));
					}
				}
				goto alsa_wake;
				break;
			case CMD_FINISH:
				goto alsa_finish;
				break;
			default:
				fprintf(stderr, "alsa_thread: recv'd unknown command %d\n", recv_cmd);
				break;
			}
		}

		if ((n_avail = rb_read_space(rb) / (2*sample_size)) == 0) {
			nanosleep(&req_time, &rem_time);
			goto alsa_wake;
		}

		if (n_avail > bufsize)
			n_avail = bufsize;

		for (i = 0; i < n_avail; i++) {
			rb_read(rb, (char *)&(l_buf[i]), sample_size);
			rb_read(rb, (char *)&(r_buf[i]), sample_size);
		}

#ifdef HAVE_LADSPA
		if (options.ladspa_is_postfader) {
			for (i = 0; i < n_avail; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}
#else
		for (i = 0; i < n_avail; i++) {
			l_buf[i] *= left_gain;
			r_buf[i] *= right_gain;
		}
#endif /* HAVE_LADSPA */

		if (n_avail < bufsize) {
			for (i = n_avail; i < bufsize; i++) {
				l_buf[i] = 0.0f;
				r_buf[i] = 0.0f;
			}
		}
		
		/* plugin processing */
#ifdef HAVE_LADSPA
		plugin_lock = 1;
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
		plugin_lock = 0;
		
		if (!options.ladspa_is_postfader) {
			for (i = 0; i < bufsize; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}
#endif /* HAVE_LADSPA */

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
			if ((n_written = snd_pcm_writei(pcm_handle, alsa_int_buf, n_avail)) != n_avail) {
				snd_pcm_prepare(pcm_handle);
			}
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
			if ((n_written = snd_pcm_writei(pcm_handle, alsa_short_buf, n_avail)) != n_avail) {
				snd_pcm_prepare(pcm_handle);
			}
		}
	}
 alsa_finish:
	return 0;
}
#endif /* HAVE_ALSA */



/* JACK output function */
#ifdef HAVE_JACK
int
process(u_int32_t nframes, void * arg) {

	u_int32_t i;
	u_int32_t n_avail;
	jack_default_audio_sample_t * out1 = jack_port_get_buffer(out_L_port, nframes);
	jack_default_audio_sample_t * out2 = jack_port_get_buffer(out_R_port, nframes);

	static int flushing = 0;
	static int flushcnt = 0;
	char recv_cmd;

	jack_nframes = nframes;
#ifdef HAVE_LADSPA
	ladspa_buflen = nframes;
#endif /* HAVE_LADSPA */
	
	while (rb_read_space(rb_disk2out)) {
		rb_read(rb_disk2out, &recv_cmd, 1);
		switch (recv_cmd) {
		case CMD_FLUSH:
			flushing = 1;
			flushcnt = rb_read_space(rb)/nframes/
				(2*sample_size) * 1.1f;
			break;
		case CMD_FINISH:
			return 0;
			break;
		default:
			fprintf(stderr, "jack process(): recv'd unknown command %d\n", recv_cmd);
			break;
		}
	}
	
	n_avail = rb_read_space(rb) / (2*sample_size);
	if (n_avail > nframes)
		n_avail = nframes;
	
	for (i = 0; i < n_avail; i++) {
		rb_read(rb, (char *)&(l_buf[i]), sample_size);
		rb_read(rb, (char *)&(r_buf[i]), sample_size);
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
#ifdef HAVE_LADSPA
		if (options.ladspa_is_postfader) {
			for (i = 0; i < n_avail; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}
#else
		for (i = 0; i < n_avail; i++) {
			l_buf[i] *= left_gain;
			r_buf[i] *= right_gain;
		}
#endif /* HAVE_LADSPA */
		
		/* plugin processing */
#ifdef HAVE_LADSPA
		if (n_avail > 0) {
			plugin_lock = 1;
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
			plugin_lock = 0;
		}
		
		if (!options.ladspa_is_postfader) {
			for (i = 0; i < nframes; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}
#endif /* HAVE_LADSPA */
	}
	
	for (i = 0; i < nframes; i++) {
		out1[i] = l_buf[i];
		out2[i] = r_buf[i];
	}
	
	
	if ((flushing) && (!rb_read_space(rb) || (--flushcnt == 0))) {
		flushing = 0;
	}
	return 0;
}


void
jack_shutdown(void * arg) {

	jack_is_shutdown = 1;
}
#endif /* HAVE_JACK */




#ifdef HAVE_OSS
/* return values:
 *  0 : success
 * -1 : device busy
 * -N : unable to start with given params
 */
int
oss_init(thread_info_t * info, int verbose) {

	int ioctl_arg;
	int ioctl_status;

        if (info->out_SR > MAX_SAMPLERATE) {
		if (verbose) {
			fprintf(stderr, "\nThe sample rate you set (%ld Hz) is higher than MAX_SAMPLERATE.\n",
				info->out_SR);
			fprintf(stderr, "This is an arbitrary limit, which you may safely enlarge "
				"if you really need to.\n");
			fprintf(stderr, "Currently MAX_SAMPLERATE = %d Hz.\n", MAX_SAMPLERATE);
		}
                return -2;
        }

	/* open dsp device */
	info->fd_oss = open(device_name, O_WRONLY, 0);
	if (info->fd_oss < 0) {
		if (verbose) {
			fprintf(stderr, "oss_init: open of %s ", device_name);
			perror("failed");
		}
		return -1;
	}

	ioctl_arg = 16; /* bitdepth */
	ioctl_status = ioctl(info->fd_oss, SOUND_PCM_WRITE_BITS, &ioctl_arg);
	if (ioctl_status == -1) {
		if (verbose) {
			perror("oss_init: SOUND_PCM_WRITE_BITS ioctl failed");
		}
		return -3;
	}
	if (ioctl_arg != 16) {
		if (verbose) {
			perror("oss_init: unable to set sample size");
		}
		return -4;
	}

	ioctl_arg = 2;  /* stereo */
	ioctl_status = ioctl(info->fd_oss, SOUND_PCM_WRITE_CHANNELS, &ioctl_arg);
	if (ioctl_status == -1) {
		if (verbose) {
			perror("oss_init: SOUND_PCM_WRITE_CHANNELS ioctl failed");
		}
		return -5;
	}
	if (ioctl_arg != 2) {
		if (verbose) {
			perror("oss_init: unable to set number of channels");
		}
		return -6;
	}

	ioctl_arg = info->out_SR;
	ioctl_status = ioctl(info->fd_oss, SOUND_PCM_WRITE_RATE, &ioctl_arg);
	if (ioctl_status == -1) {
		if (verbose) {
			perror("oss_init: SOUND_PCM_WRITE_RATE ioctl failed");
		}
		return -7;
	}
	if (ioctl_arg != info->out_SR) {
		if (verbose) {
			fprintf(stderr, "oss_init: unable to set sample_rate to %ld\n", info->out_SR);
		}
		return -8;
	}

	/* start OSS output thread */
	AQUALUNG_THREAD_CREATE(info->oss_thread_id, NULL, oss_thread, info)

	return 0;
}
#endif /* HAVE_OSS */



#ifdef HAVE_ALSA
/* return values:
 *  0 : success
 * -1 : device busy
 * -N : unable to start with given params
 */
int
alsa_init(thread_info_t * info, int verbose) {

	unsigned rate;
	int dir = 0;
	int ret;

	info->stream = SND_PCM_STREAM_PLAYBACK;
	snd_pcm_hw_params_alloca(&info->hwparams);
	ret = snd_pcm_open(&info->pcm_handle, info->pcm_name, info->stream, SND_PCM_NONBLOCK);
	if (ret < 0) {
		if (verbose) {
			fprintf(stderr, "alsa_init: error opening PCM device %s: %s\n",
				info->pcm_name, snd_strerror(ret));
		}
		return -1;
	}
	snd_pcm_close(info->pcm_handle);

	/* now that we know the device is available, open it in blocking mode */
	/* this is not deadlock safe. */
	ret = snd_pcm_open(&info->pcm_handle, info->pcm_name, info->stream, 0);
	if (ret < 0) {
		if (verbose) {
			fprintf(stderr, "alsa_init: error reopening PCM device %s: %s\n",
				info->pcm_name, snd_strerror(ret));
		}
		return -2;
	}
	
	if (snd_pcm_hw_params_any(info->pcm_handle, info->hwparams) < 0) {
		if (verbose) {
			fprintf(stderr, "alsa_init: cannot configure this PCM device.\n");
		}
		return -3;
	}

	if (snd_pcm_hw_params_set_periods_integer(info->pcm_handle, info->hwparams) < 0) {
		fprintf(stderr, "alsa_init warning: cannot set period size to integer value.\n");
	}

	if (snd_pcm_hw_params_set_access(info->pcm_handle, info->hwparams,
					 SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		if (verbose) {
			fprintf(stderr, "alsa_init: error setting access to SND_PCM_ACCESS_RW_INTERLEAVED.\n");
		}
		return -4;
	}
  
	info->is_output_32bit = 1;
	if (snd_pcm_hw_params_set_format(info->pcm_handle, info->hwparams, SND_PCM_FORMAT_S32) < 0) {
		if (verbose) {
			fprintf(stderr, "alsa_init: unable to open 32 bit output, falling back to 16 bit...\n");
		}
		if (snd_pcm_hw_params_set_format(info->pcm_handle, info->hwparams, SND_PCM_FORMAT_S16) < 0) {
			if (verbose) {
				fprintf(stderr, "alsa_init: unable to open 16 bit output, exiting.\n");
			}
			return -5;
		}
		info->is_output_32bit = 0;
	}
	
	rate = info->out_SR;
	dir = 0;
	if (snd_pcm_hw_params_set_rate_near(info->pcm_handle, info->hwparams, &rate, &dir) < 0) {
		if (verbose) {
			fprintf(stderr, "alsa_init: can't set sample rate to %u.\n", rate);
		}
		return -6;
	}
	
	if (rate != info->out_SR) {
		if (verbose) {
			fprintf(stderr, "Requested sample rate (%ld Hz) cannot be set, ", info->out_SR);
			fprintf(stderr, "using closest available rate (%d Hz).\n", rate);
		}
		info->out_SR = rate;
	}

	if (snd_pcm_hw_params_set_channels(info->pcm_handle, info->hwparams, 2) < 0) {
		if (verbose) {
			fprintf(stderr, "alsa_init: error setting channels.\n");
		}
		return -7;
	}

	if (snd_pcm_hw_params_set_periods(info->pcm_handle, info->hwparams, nperiods, 0) < 0) {
		if (verbose) {
			fprintf(stderr, "alsa_init warning: error setting nperiods to %d.\n", nperiods);
		}
	}
  
	if (snd_pcm_hw_params_set_buffer_size(info->pcm_handle, info->hwparams,
					      (period * nperiods)>>2) < 0) {
		if (verbose) {
			fprintf(stderr, "alsa_init warning: failed setting buffersize to %d.\n", (period * nperiods)>>2);
			fprintf(stderr, "Parameters were: nperiods = %d, period = %d\n", nperiods, period);
		}
	}

	if (snd_pcm_hw_params(info->pcm_handle, info->hwparams) < 0) {
		if (verbose) {
			fprintf(stderr, "alsa_init: error setting HW params.\n");
		}
		return -8;
	}

	return 0;
}
#endif /* HAVE_ALSA */


#ifdef HAVE_JACK
/* return values:
 *  0 : success
 * -1 : couldn't connect to Jack
 * -N : Jack sample rate (N) out of range
 */
int
jack_init(thread_info_t * info) {

	if (client_name == NULL)
		client_name = strdup("aqualung");

	if ((jack_client = jack_client_new(client_name)) == 0) {
		return -1;
	}

	jack_set_process_callback(jack_client, process, info);
	jack_on_shutdown(jack_client, jack_shutdown, info);

        if ((info->out_SR = jack_get_sample_rate(jack_client)) > MAX_SAMPLERATE) {
		jack_client_close(jack_client);
                return -info->out_SR;
        }
        out_L_port = jack_port_register(jack_client, "out_L", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        out_R_port = jack_port_register(jack_client, "out_R", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	jack_nframes = jack_get_buffer_size(jack_client);
	if ((l_buf = malloc(jack_nframes * sizeof(float))) == NULL) {
		fprintf(stderr, "jack_init: malloc error\n");
		exit(1);
	}
	if ((r_buf = malloc(jack_nframes * sizeof(float))) == NULL) {
		fprintf(stderr, "jack_init: malloc error\n");
		exit(1);
	}
#ifdef HAVE_LADSPA
	ladspa_buflen = jack_nframes;
#endif /* HAVE_LADSPA */
	return 0;
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
#endif /* HAVE_JACK */



#ifdef _WIN32

#define WIN32_BUFFER_LEN (1<<16)

void *
win32_thread(void * arg) {

        u_int32_t i;
        thread_info_t * info = (thread_info_t *)arg;

        WAVEFORMATEX wf;
        MMRESULT error;
        HWAVEOUT hwave;
	short * short_buf = NULL;
	int nbufs = 8;
	int bufcnt;
        WAVEHDR whdr[nbufs];
	int j;
        int n_avail;
	char recv_cmd;
	int bufsize = WIN32_BUFFER_LEN / sizeof(short) / nbufs / 2;


	if ((l_buf = calloc(1, bufsize * sizeof(float))) == NULL) {
		fprintf(stderr, "win32_thread: malloc error\n");
		exit(1);
	}
	if ((r_buf = calloc(1, bufsize * sizeof(float))) == NULL) {
		fprintf(stderr, "win32_thread: malloc error\n");
		exit(1);
	}
#ifdef HAVE_LADSPA
	ladspa_buflen = bufsize;
#endif /* HAVE_LADSPA */

	if ((short_buf = calloc(1, WIN32_BUFFER_LEN)) == NULL) {
		fprintf(stderr, "win32_thread: malloc error\n");
		exit(1);
	}

        wf.nChannels = 2;
        wf.nSamplesPerSec = info->out_SR;
        wf.nBlockAlign = 2 * sizeof(short);

        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.cbSize = 0;
        wf.wBitsPerSample = 16;
        wf.nAvgBytesPerSec = wf.nBlockAlign * wf.nSamplesPerSec;

        error = waveOutOpen(&hwave, WAVE_MAPPER, &wf, 0L, 0L, 0L);
        if (error != MMSYSERR_NOERROR) {
                printf("waveOutOpen failed, error = %d\n", error);
                switch (error) {
		case MMSYSERR_ALLOCATED:
			puts("MMSYSERR_ALLOCATED");
			break;
		case MMSYSERR_BADDEVICEID:
			puts("MMSYSERR_BADDEVICEID");
			break;
		case MMSYSERR_NODRIVER:
			puts("MMSYSERR_NODRIVER");
			break;
		case MMSYSERR_NOMEM:
			puts("MMSYSERR_NOMEM");
			break;
		case WAVERR_BADFORMAT:
			puts("WAVERR_BADFORMAT");
			break;
		case WAVERR_SYNC:
			puts("WAVERR_SYNC");
			break;
                }
                return NULL;
        }

	for (bufcnt = 0; bufcnt < nbufs; bufcnt++) {
		whdr[bufcnt].lpData = (char *)short_buf +
			bufcnt * WIN32_BUFFER_LEN / nbufs;
		whdr[bufcnt].dwBufferLength = WIN32_BUFFER_LEN / nbufs;
		whdr[bufcnt].dwUser = 0L;
		whdr[bufcnt].dwFlags = 0L;
		whdr[bufcnt].dwLoops = 0L;
		
		if ((error = waveOutPrepareHeader(hwave, &(whdr[bufcnt]), sizeof(WAVEHDR)))
		    != MMSYSERR_NOERROR) {
			printf("waveOutPrepareHeader[%d] failed : %08X\n", bufcnt, error);
			waveOutUnprepareHeader(hwave, &(whdr[bufcnt]), sizeof(WAVEHDR));
			waveOutClose(hwave);
			return NULL;
		}
	}

        for (j = 0; j < WIN32_BUFFER_LEN / sizeof(short); j++) {
                short_buf[j] = 0;
        }


	for (bufcnt = 0; bufcnt < nbufs; bufcnt++) {
		if ((error = waveOutWrite(hwave, &(whdr[bufcnt]), sizeof(WAVEHDR))) !=
		    MMSYSERR_NOERROR) {
			printf("waveOutWrite[%d] failed : %08X\n", bufcnt, error);
			waveOutUnprepareHeader(hwave, &(whdr[bufcnt]), sizeof(WAVEHDR));
			waveOutClose(hwave);
			return NULL;
		}
        }

	bufcnt = 0;

	while (1) {
	win32_wake:
		while (rb_read_space(rb_disk2out)) {
			rb_read(rb_disk2out, &recv_cmd, 1);
			switch (recv_cmd) {
			case CMD_FLUSH:
				while ((n_avail = rb_read_space(rb)) > 0) {
					if (n_avail > 2*bufsize * sizeof(short))
						n_avail = 2*bufsize * sizeof(short);
					rb_read(rb, (char *)short_buf,
							     2*bufsize * sizeof(short));
				}
				for (j = 0; j < WIN32_BUFFER_LEN / sizeof(short); j++) {
					short_buf[j] = 0;
				}
				goto win32_wake;
				break;
			case CMD_FINISH:
				goto win32_finish;
				break;
			default:
				fprintf(stderr, "win32_thread: recv'd unknown command %d\n",
					recv_cmd);
				break;
			}
		}

		if ((n_avail = rb_read_space(rb) / (2*sample_size)) == 0) {
			Sleep(100);
			goto win32_wake;
		}

		if (n_avail > bufsize)
			n_avail = bufsize;
		
		for (i = 0; i < n_avail; i++) {
			rb_read(rb, (char *)&(l_buf[i]), sample_size);
			rb_read(rb, (char *)&(r_buf[i]), sample_size);
		}

#ifdef HAVE_LADSPA
		if (options.ladspa_is_postfader) {
			for (i = 0; i < n_avail; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}
#else
		for (i = 0; i < n_avail; i++) {
			l_buf[i] *= left_gain;
			r_buf[i] *= right_gain;
		}
#endif /* HAVE_LADSPA */

		if (n_avail < bufsize) {
			for (i = n_avail; i < bufsize; i++) {
				l_buf[i] = 0.0f;
				r_buf[i] = 0.0f;
			}
		}

		/* plugin processing */
#ifdef HAVE_LADSPA
		plugin_lock = 1;
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
		plugin_lock = 0;
		
		if (!options.ladspa_is_postfader) {
			for (i = 0; i < bufsize; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}
#endif /* HAVE_LADSPA */

		
		while (!(whdr[bufcnt].dwFlags & WHDR_DONE))
			Sleep(1);

		for (i = 0; i < bufsize; i++) {
			if (l_buf[i] > 1.0)
				l_buf[i] = 1.0;
			else if (l_buf[i] < -1.0)
				l_buf[i] = -1.0;

			if (r_buf[i] > 1.0)
				r_buf[i] = 1.0;
			else if (r_buf[i] < -1.0)
				r_buf[i] = -1.0;

			short_buf[2*bufcnt*bufsize + 2*i] = floorf(32767.0 * l_buf[i]);
			short_buf[2*bufcnt*bufsize + 2*i+1] = floorf(32767.0 * r_buf[i]);
		}

		/* write data to audio device */
		if ((error = waveOutWrite(hwave, &(whdr[bufcnt]), sizeof(WAVEHDR)))
		    != MMSYSERR_NOERROR) {
			printf("waveOutWrite[%d] failed : %08X\n", bufcnt, error);
			waveOutUnprepareHeader(hwave, &(whdr[bufcnt]), sizeof(WAVEHDR));
			waveOutClose(hwave);
			return NULL;
		}

		++bufcnt;
		if (bufcnt == nbufs)
			bufcnt = 0;
	}

 win32_finish:
        waveOutPause(hwave);
        waveOutReset(hwave);
	for (bufcnt = 0; bufcnt < nbufs; bufcnt++) {
		waveOutUnprepareHeader(hwave, &(whdr[bufcnt]), sizeof(WAVEHDR));
	}
        waveOutClose(hwave);
	return 0;
}
#endif /* _WIN32 */


 
void
load_default_cl(int * argc, char *** argv) {

	int i = 0;
	xmlDocPtr doc;
        xmlNodePtr cur;
        xmlNodePtr root;
        xmlChar * key;
        FILE * f;
        char config_file[MAXLEN];
	char default_param[MAXLEN];
	char cl[MAXLEN];
	int ret;


        if ((ret = chdir(options.confdir)) != 0) {
                if (errno == ENOENT) {
                        fprintf(stderr, "Creating directory %s\n", options.confdir);
                        if (mkdir(options.confdir, S_IRUSR | S_IWUSR | S_IXUSR) < 0) {
				fprintf(stderr, "fatal error: cannot create config directory.\n");
				fprintf(stderr, "mkdir: %s\n", strerror(errno));
				exit(1);
			} else {
				chdir(options.confdir);
			}
                } else {
                        fprintf(stderr, "An error occured while attempting chdir(\"%s\"). errno = %d\n",
                                options.confdir, errno);
                }
        }

        sprintf(config_file, "%s/config.xml", options.confdir);
        if ((f = fopen(config_file, "rt")) == NULL) {
                fprintf(stderr, "No config.xml -- creating empty one: %s\n", config_file);
                fprintf(stderr, "Wired-in defaults will be used.\n");
                doc = xmlNewDoc((const xmlChar *) "1.0");
                root = xmlNewNode(NULL, (const xmlChar *) "aqualung_config");
                xmlDocSetRootElement(doc, root);
                xmlSaveFormatFile(config_file, doc, 1);
		xmlFreeDoc(doc);
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
 
 
#define V_YES "    [+] "
#define V_NO  "    [ ] "

void
print_version(void) {

	fprintf(stderr,	"Aqualung -- Music player for GNU/Linux\n");
	fprintf(stderr, "Build version: %s\n", AQUALUNG_VERSION);
	fprintf(stderr,
		"(C) 2004-2007 Tom Szilagyi <tszilagyi@users.sourceforge.net>\n"
		"This is free software, and you are welcome to redistribute it\n"
		"under certain conditions; see the file COPYING for details.\n");
	
	fprintf(stderr, "\nThis Aqualung binary is compiled with:\n");

	fprintf(stderr, "\n  Optional features:\n");
	
#ifdef HAVE_LADSPA
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_LADSPA */
	fprintf(stderr, "LADSPA plugin support\n");
	
#ifdef HAVE_CDDA
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_CDDA */
	fprintf(stderr, "CDDA (Audio CD) support\n");
	
#ifdef HAVE_CDDB
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_CDDB */
	fprintf(stderr, "CDDB support\n");
	
#ifdef HAVE_SRC
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_SRC */
	fprintf(stderr, "Sample Rate Converter support\n");

#ifdef HAVE_IFP
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_IFP */
	fprintf(stderr, "iRiver iFP driver support\n");
	
#ifdef HAVE_LOOP
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_LOOP */
	fprintf(stderr, "Loop playback support\n");

#ifdef HAVE_SYSTRAY
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_SYSTRAY */
	fprintf(stderr, "Systray support\n");


	fprintf(stderr, "\n  Decoding support:\n");
	
#ifdef HAVE_SNDFILE
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_SNDFILE */
	fprintf(stderr, "sndfile (WAV, AIFF, etc.)\n");
	
#ifdef HAVE_FLAC
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_FLAC */
	fprintf(stderr, "Free Lossless Audio Codec (FLAC)\n");
	
#ifdef HAVE_OGG_VORBIS
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_OGG_VORBIS */
	fprintf(stderr, "Ogg Vorbis\n");
	
#ifdef HAVE_SPEEX
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_SPEEX */
	fprintf(stderr, "Ogg Speex\n");
	
#ifdef HAVE_MPEG
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_MPEG */
	fprintf(stderr, "MPEG Audio (MPEG 1-2.5 Layer I-III)\n");
	
#ifdef HAVE_MOD
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_MOD */
	fprintf(stderr, "MOD Audio (MOD, S3M, XM, IT, etc.)\n");
	
#ifdef HAVE_MPC
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_MPC */
	fprintf(stderr, "Musepack\n");
	
#ifdef HAVE_MAC
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_MAC */
	fprintf(stderr, "Monkey's Audio Codec\n");
	
#ifdef HAVE_WAVPACK
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_WAVPACK */
	fprintf(stderr, "WavPack\n");
	
#ifdef HAVE_LAVC
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_LAVC */
	fprintf(stderr, "LAVC (AC3, AAC, WavPack, WMA, etc.)\n");
	
#ifdef HAVE_TAGLIB
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_TAGLIB */
	fprintf(stderr, "Metadata (ID3, APE, Ogg comments)\n");
	

	fprintf(stderr, "\n  Encoding support:\n");
	
#ifdef HAVE_SNDFILE
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_SNDFILE */
	fprintf(stderr, "sndfile (WAV)\n");
	
#ifdef HAVE_FLAC
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_FLAC */
	fprintf(stderr, "Free Lossless Audio Codec (FLAC)\n");
	
#ifdef HAVE_VORBISENC
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_VORBISENC */
	fprintf(stderr, "Ogg Vorbis\n");

#ifdef HAVE_LAME
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_LAME */
	fprintf(stderr, "LAME (MP3)\n");
	

	fprintf(stderr, "\n  Output driver support:\n");
	
#ifdef HAVE_OSS
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_OSS */
	fprintf(stderr, "OSS Audio\n");
	
#ifdef HAVE_ALSA
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_ALSA */
	fprintf(stderr, "ALSA Audio\n");
	
#ifdef HAVE_JACK
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* HAVE_JACK */
	fprintf(stderr, "JACK Audio Server\n");
	
#ifdef _WIN32
	fprintf(stderr, V_YES);
#else
	fprintf(stderr, V_NO);
#endif /* _WIN32 */
	fprintf(stderr, "Win32 Sound API\n");

	fprintf(stderr, "\n");
}


void
print_usage(void) {

	fprintf(stderr,	"Aqualung -- Music player for GNU/Linux\n");
	fprintf(stderr, "Build version: %s\n", AQUALUNG_VERSION);
	fprintf(stderr,
		"(C) 2004-2007 Tom Szilagyi <tszilagyi@users.sourceforge.net>\n"
		"This is free software, and you are welcome to redistribute it\n"
		"under certain conditions; see the file COPYING for details.\n");
	
	fprintf(stderr,
		"\nInvocation:\n"
		"aqualung --help\n"
		"aqualung --version\n"
		"aqualung [--output (oss|alsa|jack|win32)] [options] [file1 [file2 ...]]\n"
		
		"\nGeneral options:\n"
		"-D, --disk-realtime: Try to use realtime (SCHED_FIFO) scheduling for disk thread.\n"
		"-Y, --disk-priority <int>: When running -D, set scheduler priority to <int> (defaults to 1).\n"
		
		"\nOptions relevant to ALSA output:\n"
		"-d, --device <name>: Set the output device (defaults to plughw:0,0).\n"
		"-p, --period <int>: Set ALSA period size (defaults to 8192).\n"
		"-n, --nperiods <int>: Specify the number of periods in hardware buffer (defaults to 2).\n"
		"-r, --rate <int>: Set the output sample rate.\n"
		"-R, --realtime: Try to use realtime (SCHED_FIFO) scheduling for ALSA output thread.\n"
		"-P, --priority <int>: When running -R, set scheduler priority to <int> (defaults to 1).\n"
		
		"\nOptions relevant to OSS output:\n"
		"-d, --device <name>: Set the output device (defaults to /dev/dsp).\n"
		"-r, --rate <int>: Set the output sample rate.\n"
		
		"\nOptions relevant to JACK output:\n"
		"-a[<port_L>,<port_R>], --auto[=<port_L>,<port_R>]: Auto-connect output ports to\n"
		"given JACK ports (defaults to first two hardware playback ports).\n"
		"-c, --client <name>: Set client name (needed if you want to run multiple instances of the program).\n"
		
		"\nOptions relevant to WIN32 output:\n"
		"-r, --rate <int>: Set the output sample rate.\n"
		
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
		"-t[<name>], --tab[=<name>]: Add files to the specified playlist tab.\n"
		
		"\nIf you don't specify a session number via --session, the files will be opened by "
		"the new\ninstance, otherwise they will be sent to the already running instance you "
		"specify.\n"
		
		"\nOptions for changing state of Playlist/Music Store windows:\n"
		"-l [yes|no], --show-pl=[yes|no]: Show/hide playlist window.\n"
		"-m [yes|no], --show-ms=[yes|no]: Show/hide music store window.\n"
		
		"\nExamples:\n"
		"$ aqualung -s3 -o alsa -R -r 48000 -d hw:0,0 -p 2048 -n 2\n"
		"$ aqualung --srctype=1 --output oss --rate 96000\n"
		"$ aqualung -o jack -a -E `find ./ledzeppelin/ -name \"*.flac\"`\n");
	
	fprintf(stderr, 
		"\nDepending on the compile-time options, not all file formats\n"
		"and output drivers may be usable. Type aqualung -v to get a\n"
		"list of all the compiled-in features.\n\n");
}


#ifdef HAVE_JACK
void
print_jack_failed_connection(void) {

	fprintf(stderr, "\nAqualung couldn't connect to JACK.\n");
	fprintf(stderr, "There are several possible reasons:\n"
		"\t1) JACK is not running.\n"
		"\t2) JACK is running as another user, perhaps root.\n"
		"\t3) There is already another client called '%s'. (Use the -c option!)\n",
		client_name);
	fprintf(stderr, "Please consider the possibilities, and perhaps (re)start JACK.\n");
}


void
print_jack_SR_out_of_range(int SR) {

	fprintf(stderr, "\nThe JACK sample rate (%d Hz) is higher than MAX_SAMPLERATE.\n", SR);
	fprintf(stderr, "This is an arbitrary limit, which you may safely enlarge "
		"if you really need to.\n");
	fprintf(stderr, "Currently MAX_SAMPLERATE = %d Hz.\n", MAX_SAMPLERATE);
}
#endif /* HAVE_JACK */


void
setup_app_directories(void) {

	char * home = getenv("HOME");
	if (!home) {
		char * homedir = (char *)g_get_home_dir();
		strcpy(options.home, homedir);
		snprintf(options.currdir, MAXLEN-1, "%s/.aqualung", homedir);
		g_free(homedir);
	} else {
		strcpy(options.home, home);
		snprintf(options.currdir, MAXLEN-1, "%s/.aqualung", home);
	}
	strcpy(options.confdir, options.currdir);

	if (getcwd(options.cwd, MAXLEN) == NULL) {
		fprintf(stderr, "setup_app_directories(): warning: getcwd() returned NULL, using . as cwd\n");
		strcpy(options.cwd, ".");
	}
}


int
main(int argc, char ** argv) {

	int parse_run;
	int * argc_opt = NULL;
	char *** argv_opt = NULL;
	int argc_def = 0;
	char ** argv_def = NULL;

	thread_info_t thread_info;
#ifndef _WIN32
	struct sched_param param;
#endif /* !_WIN32 */

	int c;
	int longopt_index = 0;
	extern int optind, opterr;
	int show_version = 0;
	int show_usage = 0;
	char * output_str = NULL;
	int rate = 0;
	int try_realtime = 0;
	int priority = 1;
	int disk_try_realtime = 0;
	int disk_priority = 1;

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

	char * optstring = "vho:d:c:n:p:r:a::RP:DY:s::l:m:N:BLUTFEV:Qt::";
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
		{ "disk-realtime", 0, 0, 'D' },
		{ "disk-priority", 1, 0, 'Y' },
		{ "srctype", 2, 0, 's' },
                { "show-pl", 1, 0, 'l' },
		{ "show-ms", 1, 0, 'm' },
		{ "tab", 2, 0, 't' },

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

#if defined(HAVE_JACK) || defined(HAVE_ALSA) || defined(HAVE_OSS)
	int auto_driver_found = 0;
#endif /* jack || alsa || oss */


#ifndef _WIN32
	/* unlock memory (implicitly locked by libjack when Jack runs realtime) */
	if (munlockall() < 0) {
		fprintf(stderr, "aqualung main(): munlockall() failed\n");
	}
#endif /* !_WIN32 */

#ifdef _WIN32
	g_thread_init(NULL);
	disk_thread_lock = g_mutex_new();
	disk_thread_wake = g_cond_new();
#endif /* _WIN32 */

	file_decoder_init();

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, AQUALUNG_LOCALEDIR);
	textdomain(PACKAGE);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	
	setup_app_socket();

	setup_app_directories();

	load_default_cl(&argc_def, &argv_def);

        playlist_state = browser_state = -1;

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
					free(output_str);
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
					free(output_str);
					break;
				}
				if (strcmp(output_str, "jack") == 0) {
#ifdef HAVE_JACK
					output = JACK_DRIVER;
#else
					fprintf(stderr,
						"You selected JACK output, but this instance of Aqualung "
						"is compiled\n"
						"without JACK output support. Type aqualung -v to get a "
						"list of\n"
						"compiled-in features.\n");
					exit(1);
#endif /* HAVE_JACK */
					free(output_str);
					break;
				}
				if (strcmp(output_str, "win32") == 0) {
#ifdef _WIN32
					output = WIN32_DRIVER;
#else
					fprintf(stderr,
						"You selected WIN32 output, but this instance of Aqualung "
						"is compiled\n"
						"without WIN32 output support. Type aqualung -v to get a "
						"list of\n"
						"compiled-in features.\n");
					exit(1);
#endif /* _WIN32 */
					free(output_str);
					break;
				}
				fprintf(stderr, "Invalid output device: %s\n", output_str);
				free(output_str);
				exit(0);
				break;
			case 'd':
				device_name = strdup(optarg);
				break;
			case 'c':
#ifdef HAVE_JACK
				client_name = strdup(optarg);
#endif /* HAVE_JACK */
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
#ifdef HAVE_JACK
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
#endif /* HAVE_JACK */
				break;
			case 'R':
				try_realtime = 1;
				break;
			case 'P':
				priority = atoi(optarg);
				break;
			case 'D':
				disk_try_realtime = 1;
				break;
			case 'Y':
				disk_priority = atoi(optarg);
				break;
			case 's':
#ifdef HAVE_SRC
				if (optarg) {
					options.src_type = atoi(optarg);
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
                        case 'l':
                               if(!strncmp(optarg, "yes", 3)) {
                                        playlist_state = 1;
                               } else if(!strncmp(optarg, "no", 2)) {
                                        playlist_state = 0;
                               }
                               break;
                        case 'm':
                                if(!strncmp(optarg, "yes", 3)) {
                                        browser_state = 1;
                                } else if(!strncmp(optarg, "no", 2)) {
                                        browser_state = 0;
                                }
                                break;

			case 't':
				if (optarg != NULL) {
					tab_name = strdup(optarg);
				} else {
					tab_name = strdup("");
				}
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
	free(argv_def);
	
	if (show_version) {
		print_version();
		close_app_socket();
		exit(1);
	}

	if (show_usage) {
		print_usage();
		close_app_socket();
		exit(1);
	}


	if (back) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_BACK;
		send_message_to_session(no_session, &rcmd, 1);
		close_app_socket();
		exit(0);
	}
	if (pause) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_PAUSE;
		send_message_to_session(no_session, &rcmd, 1);
		close_app_socket();
		exit(0);
	}
	if (stop) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_STOP;
		send_message_to_session(no_session, &rcmd, 1);
		close_app_socket();
		exit(0);
	}
	if (fwd) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_FWD;
		send_message_to_session(no_session, &rcmd, 1);
		close_app_socket();
		exit(0);
	}

	if (remote_quit) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_QUIT;
		send_message_to_session(no_session, &rcmd, 1);
		close_app_socket();
		exit(1);
	}

	if (voladj_arg) {
		char buf[MAXLEN];

		if (no_session == -1)
			no_session = 0;
		buf[0] = RCMD_VOLADJ;
		buf[1] = '\0';
		strncat(buf, voladj_arg, MAXLEN-2);
		send_message_to_session(no_session, buf, strlen(buf));
		close_app_socket();
		exit(1);
	}

	{
		int i;
		char buffer[MAXLEN];
		char fullname[MAXLEN];

		if ((no_session != -1) && (no_session != aqualung_session_id)) {

			for (i = optind; argv[i] != NULL; i++) {				

				normalize_filename(argv[i], fullname);

				buffer[0] = RCMD_ADD_FILE;
				buffer[1] = '\0';
				strncat(buffer, fullname, MAXLEN-2);
				send_message_to_session(no_session, buffer, strlen(buffer));
			}

			if (argv[optind] != NULL) {

				buffer[0] = RCMD_ADD_COMMIT;
				buffer[1] = (enqueue) ? 1 : 0;
				buffer[2] = (play) ? 1 : 0;
				buffer[3] = (tab_name != NULL) ? 1 : 0;
				buffer[4] = '\0';

				if (tab_name != NULL) {
					strncat(buffer + 4, tab_name, MAXLEN-5);
				}

				send_message_to_session(no_session, buffer, 4 + strlen(buffer + 4));
				close_app_socket();
				exit(0);
			}
		}
	}

	if (play) {
		if ((no_session == -1) || (no_session == aqualung_session_id)) {
			immediate_start = 1;
		} else {
			rcmd = RCMD_PLAY;
			send_message_to_session(no_session, &rcmd, 1);
			close_app_socket();
			exit(0);
		}
	}

#ifdef _WIN32
	if (output == 0) {
		output = WIN32_DRIVER;
	}
#endif /* _WIN32 */


	memset(&thread_info, 0, sizeof(thread_info));

#ifdef HAVE_JACK
	if ((output != JACK_DRIVER) && (rate == 0)) {
		rate = 44100;
	}
#else
	if (rate == 0) {
		rate = 44100;
	}
#endif /* HAVE_JACK */

	/* try to find a suitable output driver */
	if (output == 0) {
		printf("No output driver specified, probing for a usable driver.\n");
	}
#ifdef HAVE_JACK
	if (output == 0) {
		int ret;

		/* probe Jack */
		printf("Probing JACK driver... ");
		ret = jack_init(&thread_info);
		if (ret == -1) {
			printf("JACK server not found\n");
		} else if (ret < 0) {
			printf("Output samplerate (%d Hz) out of range\n", -ret);
		} else {
			output = JACK_DRIVER;
			rate = thread_info.out_SR;
			auto_connect = 1;
			auto_driver_found = 1;
			printf("OK\n");
		}
	}
#endif /* HAVE_JACK */
#ifdef HAVE_ALSA
	if (output == 0) { /* probe ALSA */
		int ret;

		printf("Probing ALSA driver... ");

		period = 8192;
		nperiods = 2;
		device_name = strdup("plughw:0,0");

		thread_info.out_SR = rate;
		thread_info.pcm_name = strdup(device_name);

		ret = alsa_init(&thread_info, 0);
		if (ret == -1) {
			printf("device busy\n");			
		} else if (ret < 0) {
			printf("unable to start with default params\n");
		} else {
			output = ALSA_DRIVER;
			auto_driver_found = 1;
			printf("OK\n");
		}
	}
#endif /* HAVE_ALSA */
#ifdef HAVE_OSS
	if (output == 0) { /* probe OSS */
		int ret;

		printf("Probing OSS driver... ");

		device_name = strdup("/dev/dsp");
		thread_info.out_SR = rate;

		ret = oss_init(&thread_info, 0);
		if (ret == -1) {
			printf("device busy\n");			
		} else if (ret < 0) {
			printf("unable to start with default params\n");
		} else {
			output = OSS_DRIVER;
			auto_driver_found = 1;
			printf("OK\n");
		}
	}
#endif /* HAVE_OSS */

	/* if no output driver was found, give up and exit */
	if (output == 0) {
		fprintf(stderr,
			"No usable output driver was found. Please see aqualung --help\n"
			"and the docs for more info on successfully starting the program.\n");
		close_app_socket();
		exit(1);
	}

	
#ifdef HAVE_JACK
	if ((output == JACK_DRIVER) && (!auto_driver_found) && (rate > 0)) {
		fprintf(stderr,
			"You attempted to set the output rate for the JACK output.\n"
			"We won't do this; please use the --rate option with the\n"
			"oss and alsa outputs only.\n"
			"In case of the JACK output, the (already running) JACK server\n"
			"will determine the output sample rate to use.\n");
		close_app_socket();
		exit(1);
	}
#endif /* HAVE_JACK */

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


#ifdef HAVE_JACK
	if (output == JACK_DRIVER) {
		if (!auto_driver_found) {
			int ret = jack_init(&thread_info);
			if (ret == -1) {
				print_jack_failed_connection();
				close_app_socket();
				exit(1);
			} else if (ret < 0) {
				print_jack_SR_out_of_range(-ret);
				close_app_socket();
				exit(1);
			} else {
				rate = thread_info.out_SR;
			}
		}
	}
#endif /* HAVE_JACK */

#ifdef HAVE_ALSA
	if (output == ALSA_DRIVER) {
		if (!auto_driver_found) {
			int ret;

			thread_info.out_SR = rate;
			thread_info.pcm_name = strdup(device_name);

			ret = alsa_init(&thread_info, 1);
			if (ret < 0) {
				close_app_socket();
				exit(1);
			}
		}
	}
#endif /* HAVE_ALSA */

#ifdef HAVE_OSS
	if (output == OSS_DRIVER) {
		thread_info.out_SR = rate;
	}
#endif /* HAVE_OSS */

#ifdef _WIN32
	if (output == WIN32_DRIVER) {
		thread_info.out_SR = rate;
	}
#endif /* _WIN32 */


	thread_info.rb_size = RB_AUDIO_SIZE * thread_info.out_SR / 44100.0;

        rb = rb_create(2*sample_size * thread_info.rb_size);
	memset(rb->buf, 0, rb->size);


	rb_disk2gui = rb_create(RB_CONTROL_SIZE);
	memset(rb_disk2gui->buf, 0, rb_disk2gui->size);

	rb_gui2disk = rb_create(RB_CONTROL_SIZE);
	memset(rb_gui2disk->buf, 0, rb_gui2disk->size);

	rb_disk2out = rb_create(RB_CONTROL_SIZE);
	memset(rb_disk2out->buf, 0, rb_disk2out->size);

	thread_info.is_streaming = 0;
	thread_info.is_mono = 0;
	thread_info.in_SR = 0;


	/* startup disk thread */
	AQUALUNG_THREAD_CREATE(thread_info.disk_thread_id, NULL, disk_thread, &thread_info)

	if (disk_try_realtime) {
#ifdef _WIN32
		printf("Warning: setting thread priorities is unsupported under Win32.\n");
#else
		int x;
		memset(&param, 0, sizeof(param));
		param.sched_priority = disk_priority;
		if ((x = pthread_setschedparam(thread_info.disk_thread_id,
					       SCHED_FIFO, &param)) != 0) {
			fprintf(stderr,
				"Warning: cannot use real-time scheduling for disk thread (FIFO/%d) "
				"(%d: %s)\n", param.sched_priority, x, strerror(x));
		}
#endif /* _WIN32 */
	}


#ifdef HAVE_OSS
	if (output == OSS_DRIVER) {
		if (!auto_driver_found) {
			int ret = oss_init(&thread_info, 1);
			if (ret < 0) {
				close_app_socket();
				exit(1);
			}
		}
	}
#endif /* HAVE_OSS */

#ifdef HAVE_ALSA
	if (output == ALSA_DRIVER) {
		AQUALUNG_THREAD_CREATE(thread_info.alsa_thread_id, NULL, alsa_thread, &thread_info)

		if (try_realtime) {
#ifdef _WIN32
			printf("Warning: setting thread priorities is unsupported under Win32.\n");
#else
			int x;
			memset(&param, 0, sizeof(param));
			param.sched_priority = priority;
			if ((x = pthread_setschedparam(thread_info.alsa_thread_id,
						       SCHED_FIFO, &param)) != 0) {
				fprintf(stderr,
					"Warning: cannot use real-time scheduling for ALSA output thread (FIFO/%d) "
					"(%d: %s)\n", param.sched_priority, x, strerror(x));
			}
#endif /* _WIN32 */
		}
	}
#endif /* HAVE_ALSA */

#ifdef _WIN32
	if (output == WIN32_DRIVER) {
		AQUALUNG_THREAD_CREATE(thread_info.win32_thread_id, NULL, win32_thread, &thread_info)
		g_thread_set_priority(thread_info.win32_thread_id, G_THREAD_PRIORITY_URGENT);
	}
#endif /* _WIN32 */


	create_gui(argc, argv, optind, enqueue, rate, RB_AUDIO_SIZE * rate / 44100.0);
	run_gui(); /* control stays here until user exits program */

	AQUALUNG_THREAD_JOIN(thread_info.disk_thread_id)

#ifdef HAVE_OSS
	if (output == OSS_DRIVER) {
		AQUALUNG_THREAD_JOIN(thread_info.oss_thread_id)
		free(thread_info.oss_short_buf);
		ioctl(thread_info.fd_oss, SNDCTL_DSP_RESET, 0);
		close(thread_info.fd_oss);
	}
#endif /* HAVE_OSS */

#ifdef HAVE_ALSA
	if (output == ALSA_DRIVER) {
		AQUALUNG_THREAD_JOIN(thread_info.alsa_thread_id)
		free(thread_info.pcm_name);
		snd_pcm_close(thread_info.pcm_handle);
		if (thread_info.is_output_32bit)
			free(thread_info.alsa_int_buf);
		else
			free(thread_info.alsa_short_buf);
	}
#endif /* HAVE_ALSA */

#ifdef HAVE_JACK
	if (output == JACK_DRIVER) {
		jack_client_close(jack_client);
		free(client_name);
	}
#endif /* HAVE_JACK */

#ifdef _WIN32
	if (output == WIN32_DRIVER) {
		AQUALUNG_THREAD_JOIN(thread_info.win32_thread_id)
	}

	g_mutex_free(disk_thread_lock);
	g_cond_free(disk_thread_wake);
#endif /* _WIN32 */

	if (device_name != NULL)
		free(device_name);

	free(l_buf);
	free(r_buf);
	rb_free(rb);
	rb_free(rb_disk2gui);
	rb_free(rb_gui2disk);
	rb_free(rb_disk2out);

	close_app_socket();
	return 0;
}

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
