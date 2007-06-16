/*                                                     -*- linux-c -*-
    Copyright (C) 2007 Tom Szilagyi

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


#ifndef _METADATA_H
#define _METADATA_H

#include <config.h>

#ifdef HAVE_OGG_VORBIS
#ifdef _WIN32
#undef _WIN32
#include <vorbis/vorbisfile.h>
#define _WIN32
#else
#include <vorbis/vorbisfile.h>
#endif /* _WIN32 */
#endif /* HAVE_OGG_VORBIS */


#include "common.h"

/* string types */
#define META_FIELD_TITLE     0x01
#define META_FIELD_ARTIST    0x02
#define META_FIELD_ALBUM     0x03
#define META_FIELD_DATE      0x04
#define META_FIELD_GENRE     0x05
#define META_FIELD_COMMENT   0x06
#define META_FIELD_VENDOR    0x07
#define META_FIELD_ICY_NAME  0x08
#define META_FIELD_ICY_DESCR 0x09
#define META_FIELD_ICY_GENRE 0x0a
#define META_FIELD_OTHER     0xff

/* integer types */
#define META_FIELD_TRACKNO 0x0100

/* float types */
#define META_FIELD_RVA2 0x010000

/* binary types */
#define META_FIELD_APIC 0x01000000
#define META_FIELD_GEOB 0x02000000

#define META_FIELD_TEXT  (f) ((f & 0xff))
#define META_FIELD_INT   (f) ((f & 0xff00))
#define META_FIELD_FLOAT (f) ((f & 0xff0000))
#define META_FIELD_BIN   (f) ((f & 0xff000000))


typedef struct _meta_frame_t {
	int type; /* one of META_FIELD_* */
	char * field_name;
	char * field_val; /* UTF8 */
	int int_val;
	float float_val;
	void * data;
	int length;
	struct _meta_frame_t * next;
} meta_frame_t;

typedef struct {
	int writable;
	meta_frame_t * root; /* linked list */
} metadata_t;


meta_frame_t * meta_frame_new(void);
void meta_frame_free(meta_frame_t * meta_frame);

metadata_t * metadata_new(void);
void metadata_free(metadata_t * meta);

void metadata_add_frame(metadata_t * meta, meta_frame_t * frame);

#ifdef HAVE_OGG_VORBIS
metadata_t * metadata_from_vorbis_comment(vorbis_comment * vc);
#endif /* HAVE_OGG_VORBIS */
metadata_t * metadata_from_mpeg_stream_data(char * str);
void metadata_add_textframe_from_keyval(metadata_t * meta, char * key, char * val);

meta_frame_t * metadata_get_frame(metadata_t * meta, int type, meta_frame_t * root);
char * metadata_get_title(metadata_t * meta);
char * metadata_get_artist(metadata_t * meta);
char * metadata_get_album(metadata_t * meta);
char * metadata_get_icy_name(metadata_t * meta);
char * metadata_get_icy_descr(metadata_t * meta);

void metadata_make_title_string(metadata_t * meta, char * dest);
void metadata_make_playlist_string(metadata_t * meta, char * dest);

/* debug functions */
void metadata_dump(metadata_t * meta);
void meta_dump_frame(meta_frame_t * frame);

#endif /* _METADATA_H */
