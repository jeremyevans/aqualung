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

#ifdef HAVE_FLAC
#include <FLAC/metadata.h>
#endif /* HAVE_FLAC */

#include <glib.h>

#include "common.h"


/* tag (and pseudo-tag) types */
#define META_TAG_NULL            0x00
#define META_TAG_ID3v1           0x01
#define META_TAG_ID3v2           0x02
#define META_TAG_APEv2           0x04
#define META_TAG_OXC             0x08
#define META_TAG_GEN_STREAM      0x10
#define META_TAG_MPEGSTREAM      0x20
#define META_TAG_MODINFO         0x40

/* string types */
#define META_FIELD_TITLE         0x01
#define META_FIELD_ARTIST        0x02
#define META_FIELD_ALBUM         0x03
#define META_FIELD_DATE          0x04
#define META_FIELD_GENRE         0x05
#define META_FIELD_COMMENT       0x06

#define META_FIELD_PERFORMER     0x07
#define META_FIELD_DESCRIPTION   0x08
#define META_FIELD_ORGANIZATION  0x09
#define META_FIELD_LOCATION      0x0a
#define META_FIELD_CONTACT       0x0b
#define META_FIELD_LICENSE       0x0c
#define META_FIELD_COPYRIGHT     0x0d
#define META_FIELD_ISRC          0x0e
#define META_FIELD_VERSION       0x0f

#define META_FIELD_VENDOR        0x10
#define META_FIELD_ICY_NAME      0x11
#define META_FIELD_ICY_DESCR     0x12
#define META_FIELD_ICY_GENRE     0x13
#define META_FIELD_OTHER         0xff

/* integer types */
#define META_FIELD_TRACKNO     0x0100

/* float types */
#define META_FIELD_RVA2      0x010000

/* binary types */
#define META_FIELD_APIC    0x01000000
#define META_FIELD_GEOB    0x02000000
#define META_FIELD_MODINFO 0x04000000


#define META_FIELD_TEXT(f)  ((f)&0xff)
#define META_FIELD_INT(f)   ((f)&0xff00)
#define META_FIELD_FLOAT(f) ((f)&0xff0000)
#define META_FIELD_BIN(f)   ((f)&0xff000000)

/* field flags */
#define META_FIELD_UNIQUE    0x01 /* only one instance is permitted */
#define META_FIELD_MANDATORY 0x02 /* field cannot be removed */


typedef struct _meta_frame_t {
	int tag; /* one of META_TAG_*, owner tag of this frame */
	int type; /* one of META_FIELD_* */
	int flags;
	char * field_name;
	char * field_val; /* UTF8 */
	int int_val;
	float float_val;
	void * data;
	int length;
	void * source; /* source widget in File info dialog */
	struct _meta_frame_t * next;
} meta_frame_t;

typedef struct {
	int writable;
	int valid_tags; /* tags that are valid (but may not be actually present) */
	meta_frame_t * root; /* linked list */
	void * fdec; /* optional; points to the owner fdec */
} metadata_t;


/* type of META_FIELD_MODINFO */
#ifdef HAVE_MOD
typedef struct _mod_info {
	char title[MAXLEN];
        int active;
#ifdef HAVE_MOD_INFO
        int type;
        unsigned int samples;
        unsigned int instruments;
        unsigned int patterns;
        unsigned int channels;
#endif /* HAVE_MOD_INFO */
} mod_info;
#endif /* HAVE_MOD */


meta_frame_t * meta_frame_new(void);
void meta_frame_free(meta_frame_t * meta_frame);

metadata_t * metadata_new(void);
void metadata_free(metadata_t * meta);

void metadata_add_frame(metadata_t * meta, meta_frame_t * frame);
void metadata_remove_frame(metadata_t * meta, meta_frame_t * frame);

void metadata_add_mandatory_frames(metadata_t * meta, int tag);

void metadata_add_textframe_from_keyval(metadata_t * meta, int tag,
					char * key, char * val);

#ifdef HAVE_FLAC
metadata_t * metadata_from_flac_streammeta(FLAC__StreamMetadata_VorbisComment * vc);
FLAC__StreamMetadata * metadata_to_flac_streammeta(metadata_t * meta);
#endif /* HAVE_FLAC */
#ifdef HAVE_OGG_VORBIS
metadata_t * metadata_from_vorbis_comment(vorbis_comment * vc);
#endif /* HAVE_OGG_VORBIS */
metadata_t * metadata_from_mpeg_stream_data(char * str);

meta_frame_t * metadata_get_frame(metadata_t * meta, int type, meta_frame_t * root);
meta_frame_t * metadata_get_frame_by_tag(metadata_t * meta, int tag, meta_frame_t * root);
meta_frame_t * metadata_get_frame_by_tag_and_type(metadata_t * meta, int tag, int type, meta_frame_t * root);
void metadata_make_title_string(metadata_t * meta, char * dest);
void metadata_make_playlist_string(metadata_t * meta, char * dest);

/* data model functions */
char * meta_get_tagname(int tag);
int meta_get_fieldname(int field, char ** str);
int meta_get_fieldname_embedded(int field, char ** str);
int meta_tag_from_name(char * name);
int meta_frame_type_from_name(int tag, char * name);
GSList * meta_get_possible_fields(int tag);
int meta_get_default_flags(int tag, int type);


/* debug functions */
void metadata_dump(metadata_t * meta);
void meta_dump_frame(meta_frame_t * frame);

#endif /* _METADATA_H */
