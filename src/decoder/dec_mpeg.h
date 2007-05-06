/*                                                     -*- linux-c -*-
    Copyright (C) 2005 Tom Szilagyi

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


#ifndef _DEC_MPEG_H
#define _DEC_MPEG_H


#ifdef HAVE_MPEG
#include <sys/mman.h>
#include <mad.h>
#endif /* HAVE_MPEG */


#include "../common.h"
#include "file_decoder.h"


/* size of ringbuffer for decoded MPEG Audio data (in frames) */
#define RB_MAD_SIZE 262144


#ifdef HAVE_MPEG


#define MPEG_VERSION1   0
#define MPEG_VERSION2   1
#define MPEG_VERSION2_5 2

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


typedef struct {
	/* Standard MP3 frame header fields */
	int version;
	int layer;
	int protection;
	int bitrate;
	long frequency;
	int padding;
	int channel_mode;
	int mode_extension;
	int emphasis;
	int frame_size;   /* Frame size in bytes */
	int frame_samples; /* Samples per frame */
	int ft_num;       /* Numerator of frametime in milliseconds */
	int ft_den;       /* Denominator of frametime in milliseconds */
	
	int is_vbr;      /* True if the file is VBR */
	int has_toc;     /* True if there is a VBR header in the file */
	int is_xing_vbr; /* True if the VBR header is of Xing type */
	int is_vbri_vbr; /* True if the VBR header is of VBRI type */
	unsigned char toc[100];
	unsigned long frame_count; /* Number of frames in the file (if VBR) */
	unsigned long byte_count;  /* File size in bytes */
	unsigned long file_time;   /* Length of the whole file in milliseconds */
	unsigned long vbr_header_pos;
	int enc_delay;    /* Encoder delay, fetched from LAME header */
	int enc_padding;  /* Padded samples added to last frame. LAME header */

	/* first valid(!) frame */
	unsigned long start_byteoffset;
} mp3info_t;

/* Xing header information */
#define VBR_FRAMES_FLAG  0x01
#define VBR_BYTES_FLAG   0x02
#define VBR_TOC_FLAG     0x04
#define VBR_QUALITY_FLAG 0x08

#define MAX_XING_HEADER_SIZE 576

unsigned long find_next_frame(int fd, long *offset, long max_offset,
                              unsigned long last_header, int is_ubr_allowed);
int get_mp3file_info(int fd, mp3info_t *info);

typedef struct {
	int frame; /* number of mpeg frame */
	unsigned long long sample; /* number of audio samples since beginning of file */
	unsigned long offset; /* byte offset from beginning of file */
} mpeg_seek_table_t;

typedef struct _mpeg_pdata_t {
        struct mad_decoder mpeg_decoder;
        rb_t * rb;
        FILE * mpeg_file;
        int channels;
        int SR;
        unsigned bitrate;
        int error;
        int is_eos;
        struct stat mpeg_stat;
        long long int filesize;
	long skip_bytes;
       	long delay_frames;
        int fd;
        void * fdm;
        unsigned long total_samples_est;
        int mpeg_subformat; /* used as v_minor */
	mp3info_t mp3info;
	int seek_table_built;
	AQUALUNG_THREAD_DECLARE(seek_builder_id)
        int builder_thread_running;
	mpeg_seek_table_t seek_table[100];
	unsigned long frame_counter;
	long last_frames[2]; /* [0] is the last frame's byte offset, [1] the last-but-one */
        struct mad_stream mpeg_stream;
        struct mad_frame mpeg_frame;
        struct mad_synth mpeg_synth;
} mpeg_pdata_t;
#endif /* HAVE_MPEG */


decoder_t * mpeg_decoder_init(file_decoder_t * fdec);
#ifdef HAVE_MPEG
void mpeg_decoder_destroy(decoder_t * dec);
int mpeg_decoder_open(decoder_t * dec, char * filename);
void mpeg_decoder_close(decoder_t * dec);
unsigned int mpeg_decoder_read(decoder_t * dec, float * dest, int num);
void mpeg_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos);
#endif /* HAVE_MPEG */


#endif /* _DEC_MPEG_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
