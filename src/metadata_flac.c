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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <glib.h>

#include "common.h"
#include "i18n.h"
#include "metadata_flac.h"


#ifdef HAVE_FLAC
void
metadata_from_flac_streammeta_vc(metadata_t * meta,
				 FLAC__StreamMetadata_VorbisComment * vc) {

	int i;

	if (!meta || !vc) {
		return;
	}

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
		
		metadata_add_frame_from_keyval(meta, META_TAG_OXC, key, val);
	}

	/* Add Vendor string */
	metadata_add_frame_from_keyval(meta, META_TAG_OXC, "vendor",
				       (vc->vendor_string.length > 0) ?
				       (char *)vc->vendor_string.entry : "");
}


void
metadata_from_flac_streammeta_pic(metadata_t * meta,
				  FLAC__StreamMetadata_Picture * pic) {

	meta_frame_t * frame = meta_frame_new();

	if (frame == NULL) {
		return;
	}

	frame->tag = META_TAG_FLAC_APIC;
	frame->type = META_FIELD_APIC;
	frame->field_name = strdup(pic->mime_type);
	frame->field_val = strdup((char *)pic->description);
	frame->int_val = pic->type;
	frame->length = pic->data_length;
	frame->data = (unsigned char *)malloc(frame->length);
	if (frame->data == NULL) {
		meta_frame_free(frame);
		return;
	}
	memcpy(frame->data, pic->data, frame->length);

	metadata_add_frame(meta, frame);
}


void
meta_entry_from_frame(FLAC__StreamMetadata_VorbisComment_Entry * entry,
		      meta_frame_t * frame) {

	char * key = NULL;
	char * val = NULL;
	char * renderfmt = meta_get_field_renderfmt(frame->type);
	char str[MAXLEN];

	if (!meta_get_fieldname_embedded(META_TAG_OXC, frame->type, &key)) {
		key = g_ascii_strdown(frame->field_name, strlen(frame->field_name));
		free(frame->field_name);
		frame->field_name = key;
	}

	if (META_FIELD_TEXT(frame->type)) {
		val = frame->field_val;
	} else if (META_FIELD_INT(frame->type)) {
		snprintf(str, MAXLEN-1, renderfmt, frame->int_val);
		val = str;
	} else if (META_FIELD_FLOAT(frame->type)) {
		snprintf(str, MAXLEN-1, renderfmt, frame->float_val);
		val = str;
	} else {
		fprintf(stderr, "meta_entry_from_frame: frame type 0x%x is unsupported\n", frame->type);
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


FLAC__StreamMetadata *
metadata_apic_frame_to_smeta(meta_frame_t * frame) {

	FLAC__StreamMetadata * smeta =
		FLAC__metadata_object_new(FLAC__METADATA_TYPE_PICTURE);

	FLAC__metadata_object_picture_set_mime_type(smeta, frame->field_name, true);
	FLAC__metadata_object_picture_set_description(smeta, (unsigned char *)frame->field_val, true);
	FLAC__metadata_object_picture_set_data(smeta, frame->data, frame->length, true);
	smeta->data.picture.type = frame->int_val;

	return smeta;
}

#endif /* HAVE_FLAC */
