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


#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif /* HAVE_ALSA */


/* input libs */
#ifdef HAVE_SNDFILE
#define SNDFILE_LIB 1
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
#define FLAC_LIB    2
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
#define VORBIS_LIB  3
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPEG
#define MAD_LIB     4
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
#define MOD_LIB     5
#endif /* HAVE_MOD */

/* output drivers */
#ifdef HAVE_OSS
#define OSS_DRIVER  1
#endif /* HAVE_OSS */

#ifdef HAVE_ALSA
#define ALSA_DRIVER 2
#endif /* HAVE_ALSA */

/* you can't compile without this */
#define JACK_DRIVER 3


#define MAX_SAMPLERATE 96000


/* audio ringbuffer size in stereo frames */
#define RB_AUDIO_SIZE 32768

/* control ringbuffer size in bytes */
#define RB_CONTROL_SIZE 32768


#ifdef HAVE_FLAC
/* size of ringbuffer for decoded FLAC data (in frames) */
#define RB_FLAC_SIZE 262144
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
/* Decoding buffer size for Vorbis */
#define VORBIS_BUFSIZE 4096
/* size of ringbuffer for decoded Vorbis data (in frames) */
#define RB_VORBIS_SIZE 262144
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPEG
/* Decoding buffer size for libMAD */
#define MAD_BUFSIZE 8192
/* size of ringbuffer for decoded MPEG Audio data (in frames) */
#define RB_MAD_SIZE 262144
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
/* Decoding buffer size for libmodplug */
#define MOD_BUFSIZE 8192
/* size of ringbuffer for decoded MOD Audio data (in frames) */
#define RB_MOD_SIZE 262144
#endif /* HAVE_MOD */


/* SRC settings */
#ifdef HAVE_SRC
#define MAX_RATIO 24
#else
#define MAX_RATIO 2
#endif /* HAVE_SRC */


typedef struct _thread_info {
	pthread_t disk_thread_id;

#ifdef HAVE_OSS	
	/* OSS */
	pthread_t oss_thread_id;
	int fd_oss;
	short * oss_short_buf;
#endif /* HAVE_OSS */

#ifdef HAVE_ALSA	
	/* ALSA */
	pthread_t alsa_thread_id;
	char * pcm_name;
	snd_pcm_t * pcm_handle;
	snd_pcm_stream_t stream;
	snd_pcm_hw_params_t * hwparams;
	int is_output_32bit;
	short * alsa_short_buf;
	int * alsa_int_buf;
#endif /* HAVE_ALSA */	

	jack_nframes_t rb_size;
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


/* formats that are not supported by libsndfile */
#ifdef HAVE_FLAC
#define FORMAT_FLAC   0x1000000
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
#define FORMAT_VORBIS 0x2000000
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPEG
#define FORMAT_MAD    0x4000000
/* MPEG Audio subformats */
#define MPEG_LAYER_I     0x001
#define MPEG_LAYER_II    0x002
#define MPEG_LAYER_III   0x004
#define MPEG_LAYER_MASK  0x007
#define MPEG_MODE_SINGLE 0x010
#define MPEG_MODE_DUAL   0x020
#define MPEG_MODE_JOINT  0x040
#define MPEG_MODE_STEREO 0x080
#define MPEG_MODE_MASK   0x0F0
#define MPEG_EMPH_NONE   0x100
#define MPEG_EMPH_5015   0x200
#define MPEG_EMPH_J_17   0x400
#define MPEG_EMPH_RES    0x800
#define MPEG_EMPH_MASK   0xF00
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
#define FORMAT_MOD   0x8000000
#endif /* HAVE_MOD */


typedef struct _fileinfo {
	unsigned long long total_samples;
	unsigned long sample_rate;
	int is_mono;
	int format_major;
	int format_minor;
	int bps;
} fileinfo_t;


typedef struct _status {
	long long samples_left;
	long long sample_offset;
} status_t;


typedef struct _seek {
	long long seek_to_pos;
} seek_t;


#define db2lin(x) ((x) > -90.0f ? powf(10.0f, (x) * 0.05f) : 0.0f)


#endif /* _CORE_H */
