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

#ifndef AQUALUNG_CORE_H
#define AQUALUNG_CORE_H

#include <sys/types.h>

#ifdef HAVE_ALSA
#define AlSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#endif /* HAVE_ALSA */

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

#ifdef HAVE_WINMM
#define WIN32_DRIVER 4
#endif /* HAVE_WINMM */

#ifdef HAVE_SNDIO
#define SNDIO_DRIVER 5
#include <sndio.h>
#endif /* HAVE_SNDIO */

#ifdef HAVE_PULSE
#define PULSE_DRIVER 6
#include <pulse/simple.h>
#endif /* HAVE_PULSE */

#include "common.h"


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

#ifdef HAVE_SNDIO
	AQUALUNG_THREAD_DECLARE(sndio_thread_id)
	struct sio_hdl * sndio_hdl;
	short * sndio_short_buf;
#endif /* HAVE_SNDIO */

#ifdef HAVE_OSS	
	AQUALUNG_THREAD_DECLARE(oss_thread_id)
	int fd_oss;
	short * oss_short_buf;
#endif /* HAVE_OSS */

#ifdef HAVE_ALSA
	AQUALUNG_THREAD_DECLARE(alsa_thread_id)
	char * pcm_name;
	snd_pcm_t * pcm_handle;
	snd_pcm_hw_params_t * hwparams;
	snd_pcm_uframes_t n_frames;
	int is_output_32bit;
	short * alsa_short_buf;
	int * alsa_int_buf;
#endif /* HAVE_ALSA */	

#ifdef HAVE_WINMM
	AQUALUNG_THREAD_DECLARE(win32_thread_id)
#endif /* HAVE_WINMM */

#ifdef HAVE_PULSE
	AQUALUNG_THREAD_DECLARE(pulse_thread_id)
	pa_simple *pa;
	pa_sample_spec pa_spec;
	short * pa_short_buf;
#endif /* HAVE_PULSE */

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
#define CMD_METABLOCK  10
/* command numbers from disk to output */
#define CMD_FLUSH      11


typedef struct _cue_t {
	char * filename;
	float voladj;
} cue_t;

typedef struct _status_t {
	long long sample_pos;
	long long samples_left;
	long long sample_offset;
	unsigned long sample_rate;
} status_t;


typedef struct _seek_t {
	long long seek_to_pos;
} seek_t;


void jack_client_start(void);


#endif /* AQUALUNG_CORE_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
