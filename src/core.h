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


#ifndef _CORE_H
#define _CORE_H


#include <sys/types.h>

#ifdef _WIN32
#include <glib.h>
#else
#include <pthread.h>
#endif

#include "common.h"

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif /* HAVE_ALSA */

#ifdef HAVE_JACK
#include <jack/jack.h>
#endif /* HAVE_JACK */

/* output drivers */
#ifdef HAVE_OSS
#define OSS_DRIVER  1
#endif /* HAVE_OSS */

#ifdef HAVE_ALSA
#define ALSA_DRIVER 2
#endif /* HAVE_ALSA */

#ifdef HAVE_JACK
#define JACK_DRIVER 3
#endif /* HAVE_JACK */

#ifdef _WIN32
#define WIN32_DRIVER 4
#endif /* _WIN32 */


#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SAMPLERATE 96000


/* audio ringbuffer size in stereo frames */
#define RB_AUDIO_SIZE 32768

/* control ringbuffer size in bytes */
#define RB_CONTROL_SIZE 32768


/* SRC settings */
#ifdef HAVE_SRC
#define MAX_RATIO 24
#else
#define MAX_RATIO 2
#endif /* HAVE_SRC */


typedef struct _thread_info {
	AQUALUNG_THREAD_DECLARE(disk_thread_id)

#ifdef HAVE_OSS	
	AQUALUNG_THREAD_DECLARE(oss_thread_id)
	int fd_oss;
	short * oss_short_buf;
#endif /* HAVE_OSS */

#ifdef HAVE_ALSA
	AQUALUNG_THREAD_DECLARE(alsa_thread_id)
	char * pcm_name;
	snd_pcm_t * pcm_handle;
	snd_pcm_stream_t stream;
	snd_pcm_hw_params_t * hwparams;
	int is_output_32bit;
	short * alsa_short_buf;
	int * alsa_int_buf;
#endif /* HAVE_ALSA */	

#ifdef _WIN32
	AQUALUNG_THREAD_DECLARE(win32_thread_id)
#endif /* _WIN32 */

	u_int32_t rb_size;
	unsigned long in_SR;
	unsigned long in_SR_prev;
	unsigned long out_SR;
	volatile int is_streaming;
	volatile int is_mono;

} thread_info_t;



/* command numbers from gui to disk */
#define CMD_CUE         1
#define CMD_PAUSE       2
#define CMD_RESUME      3
#define CMD_FINISH      4
#define CMD_SEEKTO      5
#define CMD_STOPWOFL    6
/* command numbers from disk to gui */
#define CMD_FILEREQ     7
#define CMD_FILEINFO    8
#define CMD_STATUS      9
/* command numbers from disk to output */
#define CMD_FLUSH      10


typedef struct _cue_t {
	char * filename;
	float voladj;
} cue_t;

typedef struct _status_t {
	long long sample_pos;
	long long samples_left;
	long long sample_offset;
} status_t;


typedef struct _seek_t {
	long long seek_to_pos;
} seek_t;


float convf(char * s);
void jack_client_start(void);

#define db2lin(x) ((x) > -90.0f ? powf(10.0f, (x) * 0.05f) : 0.0f)


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _CORE_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
