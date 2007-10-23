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

#include <glib.h>

#include "common.h"


/* tag (and pseudo-tag) types */
#define META_TAG_NULL            0x00
#define META_TAG_ID3v1           0x01
#define META_TAG_ID3v2           0x02
#define META_TAG_APE             0x04
#define META_TAG_OXC             0x08
#define META_TAG_GEN_STREAM      0x10
#define META_TAG_MPEGSTREAM      0x20
#define META_TAG_MODINFO         0x40

#define META_N_TAGS  6  /* NULL and MODINFO are pseudo-tags */

/* frame types -- string, integer, float, binary */

/* string types (most basic ones) */
#define META_FIELD_TITLE               0x01
#define META_FIELD_ARTIST              0x02
#define META_FIELD_ALBUM               0x03
#define META_FIELD_DATE                0x04
#define META_FIELD_GENRE               0x05
#define META_FIELD_COMMENT             0x06

/* string types added for OXC */
#define META_FIELD_PERFORMER           0x07
#define META_FIELD_DESCRIPTION         0x08
#define META_FIELD_ORGANIZATION        0x09
#define META_FIELD_LOCATION            0x0a
#define META_FIELD_CONTACT             0x0b
#define META_FIELD_LICENSE             0x0c
#define META_FIELD_COPYRIGHT           0x0d
#define META_FIELD_ISRC                0x0e
#define META_FIELD_VERSION             0x0f

/* string types added for APE */
#define META_FIELD_SUBTITLE            0x10
#define META_FIELD_DEBUT_ALBUM         0x11
#define META_FIELD_PUBLISHER           0x12
#define META_FIELD_CONDUCTOR           0x13
#define META_FIELD_COMPOSER            0x14
#define META_FIELD_PRIGHT              0x15
#define META_FIELD_FILE                0x16
#define META_FIELD_ISBN                0x17
#define META_FIELD_CATALOG             0x18
#define META_FIELD_LC                  0x19
#define META_FIELD_RECORD_DATE         0x1a
#define META_FIELD_RECORD_LOC          0x1b
#define META_FIELD_MEDIA               0x1c
#define META_FIELD_INDEX               0x1d
#define META_FIELD_RELATED             0x1e
#define META_FIELD_ABSTRACT            0x1f
#define META_FIELD_LANGUAGE            0x20
#define META_FIELD_BIBLIOGRAPHY        0x21
#define META_FIELD_INTROPLAY           0x22

/* string types added for ID3v2 */
#define META_FIELD_TBPM                0x30
#define META_FIELD_TDEN                0x31
#define META_FIELD_TDLY                0x32
#define META_FIELD_TDOR                0x33
#define META_FIELD_TDRL                0x34
#define META_FIELD_TDTG                0x35
#define META_FIELD_TENC                0x36
#define META_FIELD_T_E_X_T             0x37
#define META_FIELD_TFLT                0x38
#define META_FIELD_TIPL                0x39
#define META_FIELD_TIT1                0x3a
#define META_FIELD_TKEY                0x3b
#define META_FIELD_TLEN                0x3c
#define META_FIELD_TMCL                0x3d
#define META_FIELD_TMOO                0x3e
#define META_FIELD_TOAL                0x3f
#define META_FIELD_TOFN                0x40
#define META_FIELD_TOLY                0x41
#define META_FIELD_TOPE                0x42
#define META_FIELD_TOWN                0x43
#define META_FIELD_TPE2                0x44
#define META_FIELD_TPE4                0x45
#define META_FIELD_TPOS                0x46
#define META_FIELD_TPRO                0x47
#define META_FIELD_TRSN                0x48
#define META_FIELD_TRSO                0x49
#define META_FIELD_TSOA                0x4a
#define META_FIELD_TSOP                0x4b
#define META_FIELD_TSOT                0x4c
#define META_FIELD_TSSE                0x4d
#define META_FIELD_TSST                0x4e
#define META_FIELD_TXXX                0x4f

/* misc. string types */
#define META_FIELD_VENDOR              0x60
#define META_FIELD_ICY_NAME            0x61
#define META_FIELD_ICY_DESCR           0x62
#define META_FIELD_ICY_GENRE           0x63
#define META_FIELD_OTHER               0xff

/* integer types */
#define META_FIELD_TRACKNO           0x0100
#define META_FIELD_DISC              0x0200
#define META_FIELD_EAN_UPC           0x0300

/* float types */
#define META_FIELD_RVA2            0x010000
#define META_FIELD_RG_REFLOUDNESS  0x020000
#define META_FIELD_RG_TRACK_GAIN   0x030000
#define META_FIELD_RG_TRACK_PEAK   0x040000
#define META_FIELD_RG_ALBUM_GAIN   0x050000
#define META_FIELD_RG_ALBUM_PEAK   0x060000

/* binary types */
#define META_FIELD_APIC          0x01000000
#define META_FIELD_GEOB          0x02000000
#define META_FIELD_MODINFO       0x03000000
#define META_FIELD_HIDDEN        0x04000000


#define META_FIELD_TEXT(f)        ((f)&0xff)
#define META_FIELD_INT(f)       ((f)&0xff00)
#define META_FIELD_FLOAT(f)   ((f)&0xff0000)
#define META_FIELD_BIN(f)   ((f)&0xff000000)

/* field flags */
#define META_FIELD_UNIQUE    0x01 /* only one instance is permitted */
#define META_FIELD_MANDATORY 0x02 /* field cannot be removed */
#define META_FIELD_LOCATOR   0x80 /* field_val is only a locator to the actual content */

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


/* data model functions */
char * meta_get_tagname(int tag);
int meta_tag_from_name(char * name);

int meta_get_fieldname(int type, char ** str);
int meta_get_fieldname_embedded(int tag, int type, char ** str);
char * meta_get_field_parsefmt(int type);
char * meta_get_field_renderfmt(int type);
int meta_frame_type_from_name(char * name);
int meta_frame_type_from_embedded_name(int tag, char * name);
GSList * meta_get_possible_fields(int tag);
int meta_get_default_flags(int tag, int type);
void metadata_add_mandatory_frames(metadata_t * meta, int tag);


/* object methods */
metadata_t * metadata_new(void);
void metadata_free(metadata_t * meta);

meta_frame_t * meta_frame_new(void);
void meta_frame_free(meta_frame_t * meta_frame);

void metadata_add_frame(metadata_t * meta, meta_frame_t * frame);
void metadata_remove_frame(metadata_t * meta, meta_frame_t * frame);


/* helper functions */
meta_frame_t * metadata_get_frame_by_type(metadata_t * meta,
					  int type, meta_frame_t * root);
meta_frame_t * metadata_get_frame_by_tag(metadata_t * meta,
					 int tag, meta_frame_t * root);
meta_frame_t * metadata_get_frame_by_tag_and_type(metadata_t * meta,
						  int tag, int type,
						  meta_frame_t * root);
meta_frame_t * metadata_add_frame_from_keyval(metadata_t * meta, int tag,
					      char * key, char * val);


metadata_t * metadata_from_mpeg_stream_data(char * str);

void metadata_make_title_string(metadata_t * meta, char * dest);
void metadata_make_playlist_string(metadata_t * meta, char * dest);


/* low-level utils */
u_int32_t meta_read_int32(unsigned char * buf);
u_int64_t meta_read_int64(unsigned char * buf);
void meta_write_int32(u_int32_t val, unsigned char * buf);
void meta_write_int64(u_int64_t val, unsigned char * buf);


/* debug functions */
void metadata_dump(metadata_t * meta);
void meta_dump_frame(meta_frame_t * frame);


#endif /* _METADATA_H */
