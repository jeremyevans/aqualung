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


#ifndef _FILE_DECODER_H
#define _FILE_DECODER_H


#include <sys/stat.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>

#ifdef HAVE_SNDFILE
#include <sndfile.h>
#endif /* HAVE_SNDFILE */

/* for FLAC format support */
#ifdef HAVE_FLAC
#include <FLAC/format.h>
#include <FLAC/file_decoder.h>
#endif /* HAVE_FLAC */

/* for Ogg Vorbis support */
#ifdef HAVE_OGG_VORBIS
#include <vorbis/vorbisfile.h>
#endif /* HAVE_OGG_VORBIS */

/* for MPEG Audio support */
#ifdef HAVE_MPEG
#include <mad.h>
#include <sys/mman.h>
#endif /* HAVE_MPEG */

/* for MOD Audio support */
#ifdef HAVE_MOD
#include <libmodplug/modplug.h>
#include <sys/mman.h>
#endif /* HAVE_MOD */


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


typedef struct _file_decoder_t {

	/* public */

	int file_open;
	int file_lib;
	int channels;
	unsigned long SR;
	fileinfo_t fileinfo;
	unsigned long long samples_left;


	/* private */

	unsigned int numread;
        unsigned int n_avail;
        char flush_dest;

#ifdef HAVE_SRC
        double src_ratio;
#endif /* HAVE_SRC */

#ifdef HAVE_SNDFILE
	SF_INFO sf_info;
	SNDFILE * sf;
        unsigned int nframes;
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
	FLAC__FileDecoder * flac_decoder;
	jack_ringbuffer_t * rb_flac;
	int flac_channels;
	int flac_SR;
	unsigned flac_bits_per_sample;
	FLAC__uint64 flac_total_samples;
	int flac_probing; /* whether we are reading for probing the file, or for real */
	int flac_error;
        FLAC__FileDecoderState flac_state;
        int tried_flac;
	int flac_i, flac_j;
	long int flac_blocksize;
	long int flac_scale;
	FLAC__int32 flac_buf[2];
	float flac_fbuf[2];
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
	jack_ringbuffer_t * rb_vorbis;
	FILE * vorbis_file;
	OggVorbis_File vf;
	vorbis_info * vi;
	int vorbis_EOS;
	float oggv_fbuffer[VORBIS_BUFSIZE / 2];
        char oggv_buffer[VORBIS_BUFSIZE];
        int oggv_current_section;
        long oggv_bytes_read;
        int oggv_i;
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPEG
	struct mad_decoder mpeg_decoder;
	jack_ringbuffer_t * rb_mpeg;
	FILE * mpeg_file;
	int mpeg_channels;
	int mpeg_SR;
	unsigned mpeg_bitrate;
	int mpeg_is_exploring; /* 1 if we are doing a decode run to get SR, bitrate, etc. */
	int mpeg_again; /* to guard against multiple invocations of mpeg_input_explore() */
	int mpeg_err;
	int mpeg_EOS;
	struct stat mpeg_stat;
	int mpeg_fd;
	struct stat mpeg_exp_stat;
	int mpeg_exp_fd;
	void * mpeg_fdm;
	unsigned long mpeg_total_samples_est;
	int mpeg_subformat; /* used as v_minor */
	struct mad_stream mpeg_stream;
	struct mad_frame mpeg_frame;
	struct mad_synth mpeg_synth;
        char mpeg_file_data[MAD_BUFSIZE];
        int mpeg_n_read;
        int mpeg_i, mpeg_j;
        unsigned long mpeg_scale;
        int mpeg_buf[2];
        float mpeg_fbuf[2];
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
	jack_ringbuffer_t * rb_mod;
	ModPlug_Settings mp_settings;
	ModPlugFile * mpf;
	int mod_fd;
	void * mod_fdm;
	struct stat mod_st;
	int mod_EOS;
        float mod_fbuffer[MOD_BUFSIZE / 2];
        char mod_buffer[MOD_BUFSIZE];
        long mod_bytes_read;
        int mod_i;
#endif /* HAVE_MOD */

} file_decoder_t;


file_decoder_t * file_decoder_new(void);
void file_decoder_delete(file_decoder_t * fdec);

int file_decoder_open(file_decoder_t * fdec, char * filename, unsigned int out_SR);
void file_decoder_close(file_decoder_t * fdec);
unsigned int file_decoder_read(file_decoder_t * fdec, float * dest, int num);
void file_decoder_seek(file_decoder_t * fdec, unsigned long long seek_to_pos);

float get_file_duration(char * file);


#endif /* _FILE_DECODER_H */
