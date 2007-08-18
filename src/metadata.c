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


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "i18n.h"
#include "utils.h"
#include "options.h"
#include "metadata.h"
#include "metadata_api.h"


extern options_t options;


/* Functions that reflect the data model */

char *
meta_get_tagname(int tag) {

	switch (tag) {
	case META_TAG_NULL: return _("NULL");
	case META_TAG_ID3v1: return _("ID3v1");
	case META_TAG_ID3v2: return _("ID3v2");
	case META_TAG_APE: return _("APE");
	case META_TAG_OXC: return _("Ogg Xiph Comments");
	case META_TAG_GEN_STREAM: return _("Generic StreamMeta");
	case META_TAG_MPEGSTREAM: return _("MPEG StreamMeta");
	case META_TAG_MODINFO: return _("Module info");
	default: return _("Unknown");
	}
}


int
meta_get_fieldname(int field, char ** str) {

	switch (field) {
	case META_FIELD_TITLE: *str = _("Title"); return 1;
	case META_FIELD_ARTIST: *str = _("Artist"); return 1;
	case META_FIELD_ALBUM: *str = _("Album"); return 1;
	case META_FIELD_DATE: *str = _("Date"); return 1;
	case META_FIELD_GENRE: *str = _("Genre"); return 1;
	case META_FIELD_COMMENT: *str = _("Comment"); return 1;

	case META_FIELD_PERFORMER: *str = _("Performer"); return 1;
	case META_FIELD_DESCRIPTION: *str = _("Description"); return 1;
	case META_FIELD_ORGANIZATION: *str = _("Organization"); return 1;
	case META_FIELD_LOCATION: *str = _("Location"); return 1;
	case META_FIELD_CONTACT: *str = _("Contact"); return 1;
	case META_FIELD_LICENSE: *str = _("License"); return 1;
	case META_FIELD_COPYRIGHT: *str = _("Copyright"); return 1;
	case META_FIELD_ISRC: *str = _("ISRC"); return 1;
	case META_FIELD_VERSION: *str = _("Version"); return 1;

	case META_FIELD_SUBTITLE: *str = _("Subtitle"); return 1;
	case META_FIELD_DEBUT_ALBUM: *str = _("Debut Album"); return 1;
	case META_FIELD_PUBLISHER: *str = _("Publisher"); return 1;
	case META_FIELD_CONDUCTOR: *str = _("Conductor"); return 1;
	case META_FIELD_COMPOSER: *str = _("Composer"); return 1;
	case META_FIELD_PRIGHT: *str = _("Publication Right"); return 1;
	case META_FIELD_FILE: *str = _("File"); return 1;
	case META_FIELD_EAN_UPC: *str = _("EAN/UPC"); return 1;
	case META_FIELD_ISBN: *str = _("ISBN"); return 1;
	case META_FIELD_CATALOG: *str = _("Catalog"); return 1;
	case META_FIELD_LC: *str = _("Label Code"); return 1;
	case META_FIELD_RECORD_DATE: *str = _("Record Date"); return 1;
	case META_FIELD_RECORD_LOC: *str = _("Record Location"); return 1;
	case META_FIELD_MEDIA: *str = _("Media"); return 1;
	case META_FIELD_INDEX: *str = _("Index"); return 1;
	case META_FIELD_RELATED: *str = _("Related"); return 1;
	case META_FIELD_ABSTRACT: *str = _("Abstract"); return 1;
	case META_FIELD_LANGUAGE: *str = _("Language"); return 1;
	case META_FIELD_BIBLIOGRAPHY: *str = _("Bibliography"); return 1;
	case META_FIELD_INTROPLAY: *str = _("Introplay"); return 1;

	case META_FIELD_VENDOR: *str = _("Vendor"); return 1;
	case META_FIELD_ICY_NAME: *str = _("Icy-Name"); return 1;
	case META_FIELD_ICY_DESCR: *str = _("Icy-Description"); return 1;
	case META_FIELD_ICY_GENRE: *str = _("Icy-Genre"); return 1;
	case META_FIELD_OTHER: *str = _("Other"); return 1;
	case META_FIELD_TRACKNO: *str = _("Track No."); return 1;
	case META_FIELD_RVA2: *str = _("RVA2"); return 1;
	case META_FIELD_APIC: *str = _("Embedded Picture"); return 1;
	case META_FIELD_GEOB: *str = _("General Encapsulated Object"); return 1;

	default: return 0;
	}
}


int
meta_get_field_from_string(char * str) {

	if (strcasecmp("Title", str) == 0) { return META_FIELD_TITLE; }
	if (strcasecmp("Artist", str) == 0) { return META_FIELD_ARTIST; }
	if (strcasecmp("Album", str) == 0) { return META_FIELD_ALBUM; }
	if (strcasecmp("Date", str) == 0) { return META_FIELD_DATE; }
	if (strcasecmp("Year", str) == 0) { return META_FIELD_DATE; }
	if (strcasecmp("Genre", str) == 0) { return META_FIELD_GENRE; }
	if (strcasecmp("Comment", str) == 0) { return META_FIELD_COMMENT; }
	if (strcasecmp("Track", str) == 0) { return META_FIELD_TRACKNO; }
	if (strcasecmp("Tracknumber", str) == 0) { return META_FIELD_TRACKNO; }

	if (strcasecmp("Performer", str) == 0) { return META_FIELD_PERFORMER; }
	if (strcasecmp("Description", str) == 0) { return META_FIELD_DESCRIPTION; }
	if (strcasecmp("Organization", str) == 0) { return META_FIELD_ORGANIZATION; }
	if (strcasecmp("Location", str) == 0) { return META_FIELD_LOCATION; } 
	if (strcasecmp("Contact", str) == 0) { return META_FIELD_CONTACT; }
	if (strcasecmp("License", str) == 0) { return META_FIELD_LICENSE; }
	if (strcasecmp("Copyright", str) == 0) { return META_FIELD_COPYRIGHT; }
	if (strcasecmp("ISRC", str) == 0) { return META_FIELD_ISRC; }
	if (strcasecmp("Version", str) == 0) { return META_FIELD_VERSION; }

	if (strcasecmp("Subtitle", str) == 0) { return META_FIELD_SUBTITLE; }
	if (strcasecmp("Debut Album", str) == 0) { return META_FIELD_DEBUT_ALBUM; }
	if (strcasecmp("Publisher", str) == 0) { return META_FIELD_PUBLISHER; }
	if (strcasecmp("Conductor", str) == 0) { return META_FIELD_CONDUCTOR; }
	if (strcasecmp("Composer", str) == 0) { return META_FIELD_COMPOSER; }
	if (strcasecmp("Publicationright", str) == 0) { return META_FIELD_PRIGHT; }
	if (strcasecmp("File", str) == 0) { return META_FIELD_FILE; }
	if (strcasecmp("EAN/UPC", str) == 0) { return META_FIELD_EAN_UPC; }
	if (strcasecmp("ISBN", str) == 0) { return META_FIELD_ISBN; }
	if (strcasecmp("Catalog", str) == 0) { return META_FIELD_CATALOG; }
	if (strcasecmp("LC", str) == 0) { return META_FIELD_LC; }
	if (strcasecmp("Record Date", str) == 0) { return META_FIELD_RECORD_DATE; }
	if (strcasecmp("Record Location", str) == 0) { return META_FIELD_RECORD_LOC; }
	if (strcasecmp("Media", str) == 0) { return META_FIELD_MEDIA; }
	if (strcasecmp("Index", str) == 0) { return META_FIELD_INDEX; }
	if (strcasecmp("Related", str) == 0) { return META_FIELD_RELATED; }
	if (strcasecmp("Abstract", str) == 0) { return META_FIELD_ABSTRACT; }
	if (strcasecmp("Language", str) == 0) { return META_FIELD_LANGUAGE; }
	if (strcasecmp("Bibliography", str) == 0) { return META_FIELD_BIBLIOGRAPHY; }
	if (strcasecmp("Introplay", str) == 0) { return META_FIELD_INTROPLAY; }
	
	if (strcasecmp("Icy-Name", str) == 0) { return META_FIELD_ICY_NAME; }
	if (strcasecmp("Icy-Genre", str) == 0) { return META_FIELD_ICY_GENRE; }
	if (strcasecmp("Icy-Description", str) == 0) { return META_FIELD_ICY_DESCR; }
	
	return META_FIELD_OTHER; 
}


int
meta_get_vc_fieldname_embedded(int field, char ** str) {

	switch (field) {
	case META_FIELD_TITLE: *str = "title"; return 1;
	case META_FIELD_ARTIST: *str = "artist"; return 1;
	case META_FIELD_ALBUM: *str = "album"; return 1;
	case META_FIELD_DATE: *str = "date"; return 1;
	case META_FIELD_GENRE: *str = "genre"; return 1;
	case META_FIELD_COMMENT: *str = "comment"; return 1;
	case META_FIELD_TRACKNO: *str = "tracknumber"; return 1;

	case META_FIELD_PERFORMER: *str = _("performer"); return 1;
	case META_FIELD_DESCRIPTION: *str = _("description"); return 1;
	case META_FIELD_ORGANIZATION: *str = _("organization"); return 1;
	case META_FIELD_LOCATION: *str = _("location"); return 1;
	case META_FIELD_CONTACT: *str = _("contact"); return 1;
	case META_FIELD_LICENSE: *str = _("license"); return 1;
	case META_FIELD_COPYRIGHT: *str = _("copyright"); return 1;
	case META_FIELD_ISRC: *str = _("isrc"); return 1;
	case META_FIELD_VERSION: *str = _("version"); return 1;

	default: return 0;
	}
}


int
meta_get_ape_fieldname_embedded(int field, char ** str) {

	switch (field) {
	case META_FIELD_TITLE: *str = "Title"; return 1;
	case META_FIELD_ARTIST: *str = "Artist"; return 1;
	case META_FIELD_ALBUM: *str = "Album"; return 1;
	case META_FIELD_DATE: *str = "Year"; return 1;
	case META_FIELD_GENRE: *str = "Genre"; return 1;
	case META_FIELD_COMMENT: *str = "Comment"; return 1;
	case META_FIELD_TRACKNO: *str = "Track"; return 1;

	case META_FIELD_SUBTITLE: *str = "Subtitle"; return 1;
	case META_FIELD_DEBUT_ALBUM: *str = "Debut album"; return 1;
	case META_FIELD_PUBLISHER: *str = "Publisher"; return 1;
	case META_FIELD_CONDUCTOR: *str = "Conductor"; return 1;
	case META_FIELD_COMPOSER: *str = "Composer"; return 1;
	case META_FIELD_COPYRIGHT: *str = "Copyright"; return 1;
	case META_FIELD_PRIGHT: *str = "Publicationright"; return 1;
	case META_FIELD_FILE: *str = "File"; return 1;
	case META_FIELD_EAN_UPC: *str = "EAN/UPC"; return 1;
	case META_FIELD_ISBN: *str = "ISBN"; return 1;
	case META_FIELD_ISRC: *str = "ISRC"; return 1;
	case META_FIELD_CATALOG: *str = "Catalog"; return 1;
	case META_FIELD_LC: *str = "LC"; return 1;
	case META_FIELD_RECORD_DATE: *str = "Record Date"; return 1;
	case META_FIELD_RECORD_LOC: *str = "Record Location"; return 1;
	case META_FIELD_MEDIA: *str = "Media"; return 1;
	case META_FIELD_INDEX: *str = "Index"; return 1;
	case META_FIELD_RELATED: *str = "Related"; return 1;
	case META_FIELD_ABSTRACT: *str = "Abstract"; return 1;
	case META_FIELD_LANGUAGE: *str = "Language"; return 1;
	case META_FIELD_BIBLIOGRAPHY: *str = "Bibliography"; return 1;
	case META_FIELD_INTROPLAY: *str = "Introplay"; return 1;

	default: return 0;
	}
}


GSList *
meta_get_possible_fields(int tag) {

	GSList * list = NULL;
	switch (tag) {
	case META_TAG_NULL: 
	case META_TAG_GEN_STREAM:
	case META_TAG_MPEGSTREAM:
		return NULL;
	case META_TAG_ID3v1:
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_TITLE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ARTIST));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ALBUM));
		/* TODO */
		return list;
	case META_TAG_ID3v2:
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_TITLE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ARTIST));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ALBUM));
		/* TODO */
		return list;
	case META_TAG_APE:
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_TITLE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ARTIST));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ALBUM));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_DATE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_GENRE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_TRACKNO));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_COMMENT));

		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_SUBTITLE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_DEBUT_ALBUM));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_PUBLISHER));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_CONDUCTOR));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_COMPOSER));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_COPYRIGHT));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_PRIGHT));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_FILE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_EAN_UPC));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ISBN));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ISRC));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_CATALOG));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_LC));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_RECORD_DATE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_RECORD_LOC));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_MEDIA));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_INDEX));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_RELATED));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ABSTRACT));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_LANGUAGE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_BIBLIOGRAPHY));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_INTROPLAY));
		return list;
	case META_TAG_OXC:
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_TITLE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ARTIST));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ALBUM));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_DATE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_GENRE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_TRACKNO));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_COMMENT));

		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_PERFORMER));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_DESCRIPTION));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ORGANIZATION));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_LOCATION));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_CONTACT));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_LICENSE));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_COPYRIGHT));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_ISRC));
		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_VERSION));

		list = g_slist_append(list, GINT_TO_POINTER(META_FIELD_VENDOR));
		return list;
	default: return NULL;
	}
}


int
meta_get_default_flags(int tag, int type) {

	switch (type) {
		case META_FIELD_DATE:
		case META_FIELD_GENRE:
		case META_FIELD_TRACKNO:
			return META_FIELD_UNIQUE;
	}

	switch (tag) {
	case META_TAG_NULL: 
	case META_TAG_GEN_STREAM:
	case META_TAG_MPEGSTREAM:
		return 0;
	case META_TAG_ID3v1: return 0;
	case META_TAG_ID3v2: return 0;
	case META_TAG_APE: return 0;
	case META_TAG_OXC:
		switch (type) {
		case META_FIELD_VENDOR:
			return META_FIELD_MANDATORY | META_FIELD_UNIQUE;
		default: return 0;
		}
	default: fprintf(stderr, "meta_get_default_flags: unknown tag=%d\n", tag);
		return 0;
	}
}


void
metadata_add_mandatory_frames(metadata_t * meta, int tag) {

	meta_frame_t * frame;

	switch (tag) {
	case META_TAG_NULL: 
	case META_TAG_GEN_STREAM:
	case META_TAG_MPEGSTREAM:
		return;
	case META_TAG_ID3v1:
		/* TODO */
		return;
	case META_TAG_ID3v2:
		/* TODO */
		return;
	case META_TAG_APE:
		/* TODO */
		return;
	case META_TAG_OXC:
		/* Vendor string is mandatory */
		frame = meta_frame_new();
		frame->tag = tag;
		frame->type = META_FIELD_VENDOR;
		frame->flags = meta_get_default_flags(tag, META_FIELD_VENDOR);
		frame->field_name = strdup("Vendor");
		frame->field_val = strdup("");
		metadata_add_frame(meta, frame);
		return;
	default: return;
	}
}

/* Generic functions */

metadata_t *
metadata_new(void) {

	metadata_t * meta = NULL;
	
	if ((meta = calloc(1, sizeof(metadata_t))) == NULL) {
		fprintf(stderr, "metadata.c: metadata_new() failed: calloc error\n");
		return NULL;
	}

	return meta;
}


void
metadata_free(metadata_t * meta) {

	meta_frame_t * p;
	meta_frame_t * q;
	
	if (meta->root == NULL) {
		free(meta);
		return;
	}
	
	p = meta->root;
	while (p != NULL) {
		q = p->next;
		meta_frame_free(p);
		p = q;
	}
	free(meta);
}


meta_frame_t *
meta_frame_new(void) {

	meta_frame_t * meta_frame = NULL;
	
	if ((meta_frame = calloc(1, sizeof(meta_frame_t))) == NULL) {
		fprintf(stderr, "metadata.c: meta_frame_new() failed: calloc error\n");
		return NULL;
	}

	return meta_frame;
}


void
meta_frame_free(meta_frame_t * meta_frame) {

	if (meta_frame->field_name != NULL)
		free(meta_frame->field_name);
	if (meta_frame->field_val != NULL)
		free(meta_frame->field_val);
	if (meta_frame->data != NULL)
		free(meta_frame->data);
	free(meta_frame);
}


void
metadata_add_frame(metadata_t * meta, meta_frame_t * frame) {

	frame->next = NULL;

	if (meta->root == NULL) {
		meta->root = frame;
	} else {
		meta_frame_t * prev = meta->root;
		while (prev->next != NULL) {
			prev = prev->next;
		}
		prev->next = frame;
	}
}

/* take frame out of meta; does not free frame! */
void
metadata_remove_frame(metadata_t * meta, meta_frame_t * frame) {

	meta_frame_t * prev;

	if (meta->root == frame) {
		meta->root = frame->next;
		return;
	}

	prev = meta->root;
	while (prev->next != frame) {
		prev = prev->next;
	}
	
	prev->next = frame->next;	
}


int
meta_tag_from_name(char * name) {

	int i = 1;
	if (strcmp(name, meta_get_tagname(0)) == 0)
		return 0;

	while (1) {
		if (strcmp(name, meta_get_tagname(i)) == 0) {
			return i;
		}
		i <<= 1;
	}
}

int
meta_frame_type_from_name(int tag, char * name) {

	int i;
	int type = -1;
	GSList * slist = meta_get_possible_fields(tag);
	int length = g_slist_length(slist);
	for (i = 0; i < length; i++) {
		int t = GPOINTER_TO_INT(g_slist_nth_data(slist, i));
		char * str;
		if (meta_get_fieldname(t, &str)) {
			if (strcmp(str, name) == 0) {
				type = t;
				break;
			}
		} else {
			fprintf(stderr, "meta_frame_type_from_name(%d, %s): programmer error\n", tag, name);
			return -1;
		}
	}
	g_slist_free(slist);
	return type;
}


/* Debug functions */

void
metadata_dump(metadata_t * meta) {

	meta_frame_t * frame = meta->root;
	printf("\nMetadata block dump, writable = %d, valid_tags = %d\n",
	       meta->writable, meta->valid_tags);
	while (frame) {
		meta_dump_frame(frame);
		frame = frame->next;
	}
}


void
meta_dump_frame(meta_frame_t * frame) {
	printf("  Tag %2d Type %4d  F=0x%04x  '%s'  '%s'  ",
	       frame->tag, frame->type, frame->flags, frame->field_name, frame->field_val);
	printf("int %d  float %f  ptr %p  len %d\n",
	       frame->int_val, frame->float_val, frame->data, frame->length);
}


/* Utility functions */

u_int32_t
meta_read_int32(unsigned char * buf) {

	return ((((u_int32_t)buf[0]))       |
		(((u_int32_t)buf[1]) << 8)  |
		(((u_int32_t)buf[2]) << 16) |
		(((u_int32_t)buf[3]) << 24));
}

u_int64_t
meta_read_int64(unsigned char * buf) {

	return ((((u_int64_t)buf[0]))       |
		(((u_int64_t)buf[1]) << 8)  |
		(((u_int64_t)buf[2]) << 16) |
		(((u_int64_t)buf[3]) << 24) |
		(((u_int64_t)buf[4]) << 32) |
		(((u_int64_t)buf[5]) << 40) |
		(((u_int64_t)buf[6]) << 48) |
		(((u_int64_t)buf[7]) << 56));
}

void
meta_write_int32(u_int32_t val, unsigned char * buf) {

	buf[0] = ((val & 0xff));
	buf[1] = ((val >> 8)  & 0xff);
	buf[2] = ((val >> 16) & 0xff);
	buf[3] = ((val >> 24) & 0xff);
}

void
meta_write_int64(u_int64_t val, unsigned char * buf) {

	buf[0] = ((val & 0xff));
	buf[1] = ((val >> 8)  & 0xff);
	buf[2] = ((val >> 16) & 0xff);
	buf[3] = ((val >> 24) & 0xff);
	buf[4] = ((val >> 32) & 0xff);
	buf[5] = ((val >> 40) & 0xff);
	buf[6] = ((val >> 48) & 0xff);
	buf[7] = ((val >> 56) & 0xff);
}


meta_frame_t *
metadata_add_textframe_from_keyval(metadata_t * meta, int tag, char * key, char * val) {

	meta_frame_t * frame = meta_frame_new();
	char * str;

	frame->tag = tag;
	frame->field_val = strdup(val);
	frame->type = meta_get_field_from_string(key);
	if (meta_get_fieldname(frame->type, &str)) {
		frame->field_name = strdup(str);
	} else {
		frame->field_name = strdup(key);
	}
	frame->flags = meta_get_default_flags(tag, frame->type);
	metadata_add_frame(meta, frame);
	return frame;
}


void
metadata_add_frame_from_oxc_keyval(metadata_t * meta, char * key, char * val) {

	char replaygain_label[MAXLEN];
	switch (options.replaygain_tag_to_use) {
	case 0: strcpy(replaygain_label, "Replaygain_track_gain");
		break;
	case 1: strcpy(replaygain_label, "Replaygain_album_gain");
		break;
	}
	if (strcmp(key, replaygain_label) == 0) {
		meta_frame_t * frame = meta_frame_new();
		frame->tag = META_TAG_OXC;
		frame->type = META_FIELD_RVA2;
		frame->flags = meta_get_default_flags(frame->tag, frame->type);
		frame->field_name = strdup(key);
		frame->float_val = convf(val);
		metadata_add_frame(meta, frame);
	} else if (strcmp(key, "Tracknumber") == 0) {
		meta_frame_t * frame = meta_frame_new();
		char * str;
		frame->tag = META_TAG_OXC;
		frame->type = META_FIELD_TRACKNO;
		frame->flags = meta_get_default_flags(frame->tag, frame->type);
		if (meta_get_fieldname(frame->type, &str)) {
			frame->field_name = strdup(str);
		} else {
			frame->field_name = strdup(key);
		}
		sscanf(val, "%d", &frame->int_val);
		metadata_add_frame(meta, frame);
	} else {
		metadata_add_textframe_from_keyval(meta, META_TAG_OXC, key, val);
	}
}


#ifdef HAVE_FLAC
metadata_t *
metadata_from_flac_streammeta(FLAC__StreamMetadata_VorbisComment * vc) {

	int i;
	metadata_t * meta;

	if (!vc) {
		return NULL;
	}

	meta = metadata_new();
	meta->valid_tags = META_TAG_OXC;
	for (i = 0; i < vc->num_comments; i++) {
		char key[MAXLEN];
		char val[MAXLEN];
		char c;
		int k, n = 0;

		for (k = 0; ((c = vc->comments[i].entry[n]) != '=') &&
			     (n < vc->comments[i].length) &&
			     (k < MAXLEN-1); k++) {
			key[k] = (k == 0) ? toupper(c) : tolower(c);
			++n;
		}
		key[k] = '\0';
		++n;
		
		for (k = 0; (n < vc->comments[i].length) && (k < MAXLEN-1); k++) {
			val[k] = vc->comments[i].entry[n];
			++n;
		}
		val[k] = '\0';
		
		metadata_add_frame_from_oxc_keyval(meta, key, val);
	}

	{ /* Add Vendor string */
		meta_frame_t * frame = meta_frame_new();
		frame->tag = META_TAG_OXC;
		frame->type = META_FIELD_VENDOR;
		frame->flags = meta_get_default_flags(frame->tag, frame->type);
		frame->field_name = strdup(_("Vendor"));
		frame->field_val = (vc->vendor_string.length > 0) ?
			strdup((char *)vc->vendor_string.entry) : strdup("");
		metadata_add_frame(meta, frame);
	}

	return meta;
}

void
meta_entry_from_frame(FLAC__StreamMetadata_VorbisComment_Entry * entry,
		      meta_frame_t * frame) {

	char * key;
	char * val;
	char str[MAXLEN];

	if (!meta_get_vc_fieldname_embedded(frame->type, &key)) {
		key = frame->field_name;
	}
	if (META_FIELD_TEXT(frame->type)) {
		val = frame->field_val;
	} else if (META_FIELD_INT(frame->type)) {
		snprintf(str, MAXLEN-1, "%d", frame->int_val);
		val = str;
	} else if (META_FIELD_FLOAT(frame->type)) {
		snprintf(str, MAXLEN-1, "%f", frame->float_val);
		val = str;
	} else {
		printf("meta_entry_from_frame: frame type 0x%x is unsupported\n", frame->type);
	}

	FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(
		entry, key, val);
}

FLAC__StreamMetadata *
metadata_to_flac_streammeta(metadata_t * meta) {

	FLAC__StreamMetadata * smeta =
		FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
	meta_frame_t * frame = metadata_get_frame_by_tag(meta, META_TAG_OXC, NULL);

	while (frame != NULL) {
		if (frame->type == META_FIELD_VENDOR) {
			FLAC__StreamMetadata_VorbisComment_Entry entry;
			entry.entry = (unsigned char *)strdup(frame->field_val);
			entry.length = strlen(frame->field_val);
			FLAC__metadata_object_vorbiscomment_set_vendor_string(
			        smeta, entry, false);
		} else {
			FLAC__StreamMetadata_VorbisComment_Entry entry;
			meta_entry_from_frame(&entry, frame);
			FLAC__metadata_object_vorbiscomment_insert_comment(
				smeta, smeta->data.vorbis_comment.num_comments,
				entry, false);
		}
		frame = metadata_get_frame_by_tag(meta, META_TAG_OXC, frame);
	}
	return smeta;
}
#endif /* HAVE_FLAC */


#ifdef HAVE_OGG_VORBIS
metadata_t *
metadata_from_vorbis_comment(vorbis_comment * vc) {

	int i;
	metadata_t * meta;

	if (!vc) {
		return NULL;
	}

	meta = metadata_new();
	meta->valid_tags = META_TAG_OXC;
	for (i = 0; i < vc->comments; i++) {
		char key[MAXLEN];
		char val[MAXLEN];
		char c;
		int k, n = 0;

		for (k = 0; ((c = vc->user_comments[i][n]) != '\0') &&
			     (c != '=') && (k < MAXLEN-1); k++) {
			key[k] = (k == 0) ? toupper(c) : tolower(c);
			++n;
		}
		key[k] = '\0';
		++n;
		
		for (k = 0; ((c = vc->user_comments[i][n]) != '\0') &&
			     (k < MAXLEN-1); k++) {
			val[k] = c;
			++n;
		}
		val[k] = '\0';

		metadata_add_frame_from_oxc_keyval(meta, key, val);
	}

	{ /* Add Vendor string */
		meta_frame_t * frame = meta_frame_new();
		frame->tag = META_TAG_OXC;
		frame->type = META_FIELD_VENDOR;
		frame->flags = meta_get_default_flags(frame->tag, frame->type);
		frame->field_name = strdup(_("Vendor"));
		frame->field_val = (vc->vendor != NULL) ?
			strdup(vc->vendor) : strdup("");
		metadata_add_frame(meta, frame);
	}

	return meta;
}
#endif /* HAVE_OGG_VORBIS */


metadata_t *
metadata_from_mpeg_stream_data(char * str) {

	metadata_t * meta = metadata_new();
	char * s;

	meta->writable = 0;

	s = strtok(str, ";");
	while (s) {
		char key[MAXLEN];
		char val[MAXLEN];
		char c;
		int k, n = 0;

		for (k = 0; ((c = str[n]) != '\0') &&
			     (c != '=') && (k < MAXLEN-1); k++) {
			key[k] = c;
			++n;
		}
		key[k] = '\0';
		++n;

		if (strstr(key, "Stream") == key) {
			memmove(key, key+6, sizeof(key)-6);
		}

		if (str[n] == '\'') {
			++n;
		}

		for (k = 0; ((c = str[n]) != '\0') &&
			     (c != '\'') && (k < MAXLEN-1); k++) {
			val[k] = c;
			++n;
		}
		val[k] = '\0';

		metadata_add_textframe_from_keyval(meta, META_TAG_MPEGSTREAM, key, val);

		s = strtok(NULL, ";");
	}
	return meta;
}


/* Search for frame of given type. Passing NULL as root will search from
 * the beginning of the frame list. Passing the previous return value of
 * this function as root allows getting multiple frames of the same type.
 * Returns NULL if there are no (more) frames of the specified type.
 */
meta_frame_t *
metadata_get_frame(metadata_t * meta, int type, meta_frame_t * root) {

	meta_frame_t * frame;

	if (root == NULL) {
		frame = meta->root;
	} else {
		frame = root->next;
	}

	if (frame == NULL) {
		return NULL;
	}

	while (frame->type != type) {
		frame = frame->next;
		if (frame == NULL) {
			return NULL;
		}
	}

	return frame;
}


/* Search for frame belonging to tag. Passing NULL as root will search from
 * the beginning of the frame list. Passing the previous return value of
 * this function as root allows getting multiple frames of the same tag.
 * Returns NULL if there are no (more) frames of the specified tag.
 */
meta_frame_t *
metadata_get_frame_by_tag(metadata_t * meta, int tag, meta_frame_t * root) {

	meta_frame_t * frame;

	if (root == NULL) {
		frame = meta->root;
	} else {
		frame = root->next;
	}

	if (frame == NULL) {
		return NULL;
	}

	while (frame->tag != tag) {
		frame = frame->next;
		if (frame == NULL) {
			return NULL;
		}
	}

	return frame;
}


/* Search for frame of given type, belonging to tag. Passing NULL as
 * root will search from the beginning of the frame list. Passing the
 * previous return value of this function as root allows getting
 * multiple frames of the same tag.  Returns NULL if there are no
 * (more) frames of the specified tag.
 */
meta_frame_t *
metadata_get_frame_by_tag_and_type(metadata_t * meta, int tag, int type,
				   meta_frame_t * root) {

	meta_frame_t * frame;

	if (root == NULL) {
		frame = meta->root;
	} else {
		frame = root->next;
	}

	if (frame == NULL) {
		return NULL;
	}

	while (frame->tag != tag || frame->type != type) {
		frame = frame->next;
		if (frame == NULL) {
			return NULL;
		}
	}

	return frame;
}

void
metadata_make_title_string(metadata_t * meta, char * dest) {

	char dest1[MAXLEN];
	char * artist = NULL;
	char * album = NULL;
	char * title = NULL;
	char * icy_name = NULL;

	metadata_get_artist(meta, &artist);
	metadata_get_album(meta, &album);
	metadata_get_title(meta, &title);
	metadata_get_icy_name(meta, &icy_name);

	if (title != NULL && artist != NULL && album != NULL) {
		make_title_string(dest1, options.title_format, artist, album, title);
	} else if (title != NULL && artist != NULL) {
		make_title_string_no_album(dest1, options.title_format_no_album,
					   artist, title);
	} else if (title != NULL) {
		strncpy(dest1, title, MAXLEN-1);
	} else {
		if (icy_name == NULL) {
			dest[0] = '\0';
		} else {
			strncpy(dest, icy_name, MAXLEN-1);
		}
		return;
	}

	if (icy_name == NULL) {
		strncpy(dest, dest1, MAXLEN-1);
	} else {
		snprintf(dest, MAXLEN-1, "%s (%s)", dest1, icy_name);
	}
}


void
metadata_make_playlist_string(metadata_t * meta, char * dest) {

	char * name = NULL;
	char * descr = NULL;

	metadata_get_icy_name(meta, &name);
	metadata_get_icy_descr(meta, &descr);

	if (name == NULL) {
		metadata_make_title_string(meta, dest);
	} else {
		if (descr != NULL) {
			snprintf(dest, MAXLEN-1, "%s (%s)", name, descr);
		} else {
			strncpy(dest, name, MAXLEN-1);
		}
	}
}

