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

#include "../rb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* input libs */
#define NULL_LIB    0
#define SNDFILE_LIB 1
#define FLAC_LIB    2
#define VORBIS_LIB  3
#define SPEEX_LIB   4
#define MPC_LIB     5
#define MAD_LIB     6
#define MOD_LIB     7
#define MAC_LIB     8

#define N_DECODERS  9


/* formats other than libsndfile internal formats */
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

#define MPEG_VBR         0x1000
#define MPEG_UBR         0x2000
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
#define FORMAT_MOD   0x8000000
#endif /* HAVE_MOD */

#ifdef HAVE_MPC
#define FORMAT_MPC   0x10000000
#endif /* HAVE_MPC */

#ifdef HAVE_MAC
#define FORMAT_MAC   0x20000000
/* Monkey's Audio subformats */
#define MAC_COMP_FAST   1
#define MAC_COMP_NORMAL 2
#define MAC_COMP_HIGH   3
#define MAC_COMP_EXTRA  4
#define MAC_COMP_INSANE 5
#endif /* HAVE_MAC */

#ifdef HAVE_SPEEX
#define FORMAT_SPEEX 0x40000000
#endif /* HAVE_SPEEX */


typedef struct _fileinfo_t {
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
	float voladj_db;
	float voladj_lin;

	/* private */
	void * pdec; /* actually, it's (decoder_t *) */

} file_decoder_t;


typedef struct _decoder_t {

	file_decoder_t * fdec;
	void * pdata; /* opaque pointer to decoder-dependent struct */

	struct _decoder_t * (* init)(file_decoder_t * fdec);
	void (* destroy)(struct _decoder_t * dec);
	int (* open)(struct _decoder_t * dec, char * filename);
	void (* set_rva)(struct _decoder_t * dec, float voladj);
	void (* close)(struct _decoder_t * dec);
	unsigned int (* read)(struct _decoder_t * dec, float * dest, int num);
	void (* seek)(struct _decoder_t * dec, unsigned long long seek_to_pos);
	
} decoder_t;


/* return values from decoder_t.open() -- see dec_null.c for explanation */
#define DECODER_OPEN_SUCCESS 0
#define DECODER_OPEN_BADLIB  1
#define DECODER_OPEN_FERROR  2


int is_valid_extension(char ** valid_extensions, char * filename);

file_decoder_t * file_decoder_new(void);
void file_decoder_delete(file_decoder_t * fdec);

int file_decoder_open(file_decoder_t * fdec, char * filename);
void file_decoder_set_rva(file_decoder_t * fdec, float voladj);
void file_decoder_close(file_decoder_t * fdec);
unsigned int file_decoder_read(file_decoder_t * fdec, float * dest, int num);
void file_decoder_seek(file_decoder_t * fdec, unsigned long long seek_to_pos);

float get_file_duration(char * file);


#define db2lin(x) ((x) > -90.0f ? powf(10.0f, (x) * 0.05f) : 0.0f)


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _FILE_DECODER_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

