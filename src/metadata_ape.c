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
#include <glib.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "common.h"
#include "metadata_api.h"
#include "metadata_id3v2.h"
#include "metadata_ape.h"


void
meta_ape_free(ape_tag_t * tag) {

	GSList * list = tag->items;
	while (list != NULL) {
		ape_item_t * item = list->data;
		if (item->value != NULL) {
			free(item->value);
		}
		free(item);
		list = g_slist_next(list);
	}
	g_slist_free(tag->items);
}



ape_item_t *
meta_ape_item_new(void) {

	ape_item_t * item = NULL;
	
	if ((item = calloc(1, sizeof(ape_item_t))) == NULL) {
		fprintf(stderr, "metadata_ape.c: meta_ape_item_new() failed: calloc error\n");
		return NULL;
	}

	return item;
}

int
meta_ape_parse(char * filename, ape_tag_t * tag) {

	FILE * file;
	ape_item_t * item = NULL;
	unsigned char buf[32];
	unsigned char * data = NULL;
	long offset = 0L;
	int i;
	long file_size = 0L;
	long id3_length = 0L;
	long tag_data_size = 0L;
	long last_possible_offset = 0L;
	int ret = 0;
	unsigned char * value_start = NULL;
	unsigned char * key_start = NULL;
	int key_length = 0;

	if ((file = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "meta_ape_parse: fopen() failed\n");
		return 0;
	}

	/* Get file size */
	if (fseek(file, 0, SEEK_END) == -1) {
		fprintf(stderr, "meta_ape_parse: fseek() failed\n");
		goto meta_ape_parse_error;
	}
	if ((file_size = ftell(file)) == -1) {
		fprintf(stderr, "meta_ape_parse: ftell() failed\n");
		goto meta_ape_parse_error;
	} 
	
	/* No ape or id3 tag possible in this size */
	if (file_size < APE_MINIMUM_TAG_SIZE) {
		goto meta_ape_parse_error;
	} 
	if (file_size >= 128) {
		/* Check for id3 tag */
		if ((fseek(file, -128, SEEK_END)) != 0) {
			fprintf(stderr, "meta_ape_parse: fseek() failed\n");
			goto meta_ape_parse_error;
		}
		if (fread(buf, 1, 3, file) < 3) {
			fprintf(stderr, "meta_ape_parse: fread() of id3 failed\n");
			goto meta_ape_parse_error;
		}
		if (buf[0] == 'T' && buf[1] == 'A' && buf[2] == 'G' ) {
			id3_length = 128L;
		}
	}
	/* Recheck possibility for ape tag now that id3 presence is known */
	if (file_size < APE_MINIMUM_TAG_SIZE + id3_length) {
		goto meta_ape_parse_error;
	}
	
	/* Check for existence of ape tag footer */
	if (fseek(file, -32-id3_length, SEEK_END) != 0) {
		fprintf(stderr, "meta_ape_parse: fseek() failed\n");
		goto meta_ape_parse_error;
	}
	if (fread(buf, 1, 32, file) < 32) {
		fprintf(stderr, "meta_ape_parse: fread() of tag footer failed\n");
		goto meta_ape_parse_error;
	}
	if (memcmp(APE_PREAMBLE, buf, 8)) {
		goto meta_ape_parse_error;
	}

	tag->footer.version = meta_read_int32(buf+8);
	tag->footer.tag_size = meta_read_int32(buf+12);
	tag->footer.item_count = meta_read_int32(buf+16);
	tag->footer.flags = meta_read_int32(buf+20);
	tag_data_size = tag->footer.tag_size - 32;
	
	/* Check tag footer for validity */
	if (tag->footer.tag_size < APE_MINIMUM_TAG_SIZE) {
		fprintf(stderr, "meta_ape_parse: corrupt tag (too short)\n");
		goto meta_ape_parse_error;
	}
	if (tag->footer.tag_size + id3_length > file_size) {
		fprintf(stderr, "meta_ape_parse: corrupt tag (too large)\n");
		goto meta_ape_parse_error;
	}
	if (tag->footer.item_count > tag_data_size / APE_ITEM_MINIMUM_SIZE) {
		fprintf(stderr, "meta_ape_parse: corrupt tag (item count too large)\n");
		goto meta_ape_parse_error;
	}
	if ((data = (unsigned char *)malloc(tag_data_size)) == NULL) {
		fprintf(stderr, "meta_ape_parse: malloc() failed\n");
		goto meta_ape_parse_error;
	}
	if (fseek(file, -(tag->footer.tag_size + id3_length), SEEK_END) != 0) {
		fprintf(stderr, "meta_ape_parse: fseek() failed\n");
		goto meta_ape_parse_error;
	}
	if (fread(data, 1, tag_data_size, file) < tag_data_size) {
		fprintf(stderr, "meta_ape_parse: fread() of tag data failed\n");
		goto meta_ape_parse_error;
	}
	
	last_possible_offset = tag_data_size - APE_ITEM_MINIMUM_SIZE;

	for (i = 0; i < tag->footer.item_count; i++) {
		if (offset > last_possible_offset) {
			fprintf(stderr, "meta_ape_parse: error: last_possible_offset passed\n");
			goto meta_ape_parse_error;
		}
		
		if ((item = meta_ape_item_new()) == NULL) {
			fprintf(stderr, "meta_ape_parse: malloc() failed\n");
			goto meta_ape_parse_error;
		}

		item->value_size = meta_read_int32(data+offset);
		item->flags = meta_read_int32(data+offset+4);
		key_start = data + offset + 8;
		/* Find and check start of value */
		if (item->value_size + offset + APE_ITEM_MINIMUM_SIZE > tag_data_size) {
			fprintf(stderr, "meta_ape_parse: corrupt tag (bad item length)\n");
			goto meta_ape_parse_error;
		}
		for (value_start=key_start; value_start < key_start+256 && *value_start != '\0'; value_start++) {
			/* Left Blank */
		}
		if (*value_start != '\0') {
			fprintf(stderr, "meta_ape_parse: corrupt tag (key length too large)\n");
			goto meta_ape_parse_error;
		}
		value_start++;
		key_length = value_start - key_start;
		offset += 8 + key_length + item->value_size;
		if (offset < 0 || offset > tag_data_size) {
			fprintf(stderr, "meta_ape_parse: corrupt tag (item length too large)\n");
			goto meta_ape_parse_error;
		}
		
		/* Copy key and value from tag data to item */
		if ((item->value = (unsigned char *)malloc(item->value_size + 1)) == NULL) {
			fprintf(stderr, "meta_ape_parse: malloc() failed\n");
			goto meta_ape_parse_error;
		}
		
		memcpy(item->key, key_start, key_length);
		memcpy(item->value, value_start, item->value_size);
		item->value[item->value_size] = '\0';
		tag->items = g_slist_append(tag->items, (gpointer)item);
	}
	
	if (offset != tag_data_size) {
		fprintf(stderr, "meta_ape_parse: corrupt tag (data remaining)\n");
		goto meta_ape_parse_error;
	}
	ret = 1;
	
  meta_ape_parse_error:
	if (ret == 0) {
		meta_ape_free(tag);
	}
	fclose(file);
	free(data);
	return ret;
}


int
meta_ape_pictype_from_string(unsigned char * str) {

	char * key;
	int i = 0;

	if (strstr((char *)str, "Cover Art (") == (char *)str) {
		key = (char *)str+11;
	} else {
		key = (char *)str;
	}
	key[strlen(key)-1] = '\0';

	while (1) {
		char * pic_type = meta_id3v2_apic_type_to_string(i);
		if (pic_type == NULL) {
			return 0;
		}
		if (strcasestr(pic_type, key) == pic_type) {
			return i;
		}
		++i;
	}
	return 0; /* type: other */
}


void
meta_ape_add_pic_frame(metadata_t * meta, ape_item_t * item) {

	meta_frame_t * frame;
	GdkPixbufLoader * loader;
	GdkPixbufFormat * format;
	gchar ** mime_types;
	int len1 = strlen((char *)item->value);
	if (len1 > item->value_size) {
		fprintf(stderr, "meta_ape_add_pic_frame: filename too long, discarding\n");
		return;
	}

	frame = meta_frame_new();
	if (frame == NULL) {
		fprintf(stderr, "meta_ape_add_pic_frame: malloc error\n");
		return;
	}

	frame->tag = META_TAG_APE;
	frame->type = META_FIELD_APIC;
	frame->int_val = meta_ape_pictype_from_string(item->key);
	frame->field_val = strdup((char *)item->value);
	frame->length = item->value_size - len1 - 1;
	frame->data = (unsigned char *)malloc(frame->length);
	if (frame->data == NULL) {
		fprintf(stderr, "meta_ape_add_pic_frame: malloc error\n");
		meta_frame_free(frame);
		return;
	}
	memcpy(frame->data, item->value + len1 + 1, frame->length);

	loader = gdk_pixbuf_loader_new();
	if (gdk_pixbuf_loader_write(loader, frame->data, frame->length, NULL) != TRUE) {
		fprintf(stderr, "meta_ape_add_apic_frame: failed to load image #1\n");
		meta_frame_free(frame);
		g_object_unref(loader);
		return;
	}

	if (gdk_pixbuf_loader_close(loader, NULL) != TRUE) {
		fprintf(stderr, "meta_ape_add_apic_frame: failed to load image #2\n");
		meta_frame_free(frame);
		g_object_unref(loader);
		return;
	}

	format = gdk_pixbuf_loader_get_format(loader);
	if (format == NULL) {
		fprintf(stderr, "meta_ape_add_apic_frame: failed to load image #3\n");
		meta_frame_free(frame);
		g_object_unref(loader);
		return;
	}

	mime_types = gdk_pixbuf_format_get_mime_types(format);
	if (mime_types[0] != NULL) {
		frame->field_name = strdup(mime_types[0]);
		g_strfreev(mime_types);
	} else {
		fprintf(stderr, "meta_ape_add_apic_frame: error: no mime type for image\n");
		g_strfreev(mime_types);
		meta_frame_free(frame);
		g_object_unref(loader);
		return;
	}

	g_object_unref(loader);
	metadata_add_frame(meta, frame);
}


void
meta_ape_add_hidden_frame(metadata_t * meta, ape_item_t * item) {

	meta_frame_t * frame = meta_frame_new();
	frame->tag = META_TAG_APE;
	frame->type = META_FIELD_HIDDEN;
	if (APE_FLAG_IS_LOCATOR(item->flags)) {
		frame->flags |= META_FIELD_LOCATOR;
	}
	frame->field_val = strdup((char *)item->key);
	frame->length = item->value_size;
	frame->data = malloc(frame->length);
	if (frame->data == NULL) {
		fprintf(stderr, "meta_ape_add_hidden_frame: malloc error\n");
		return;
	}
	memcpy(frame->data, item->value, frame->length);
	metadata_add_frame(meta, frame);
}


void
meta_add_frame_from_ape_item(metadata_t * meta, ape_item_t * item) {

	int i;
	meta_frame_t * frame;

	if (APE_FLAG_IS_BINARY(item->flags)) {
		if (strcasestr((char *)item->key, "cover art") == (char *)item->key) {
			meta_ape_add_pic_frame(meta, item);
		} else {
			meta_ape_add_hidden_frame(meta, item);
		}
		return;
	}

	frame = metadata_add_frame_from_keyval(meta, META_TAG_APE,
					       (char *)item->key,
					       (char *)item->value);
	if (APE_FLAG_IS_LOCATOR(item->flags)) {
		frame->flags |= META_FIELD_LOCATOR;
	}
	/* handle list of values */
	for (i = 1; i < item->value_size; i++) {
		if (item->value[i-1] == 0x00) {
			frame = metadata_add_frame_from_keyval(meta, META_TAG_APE,
							       (char *)item->key,
							       (char *)item->value+i);
			if (APE_FLAG_IS_LOCATOR(item->flags)) {
				frame->flags |= META_FIELD_LOCATOR;
			}
		}
	}
}


void
metadata_from_ape_tag(metadata_t * meta, ape_tag_t * tag) {

	GSList * list = tag->items;

	while (list != NULL) {
		ape_item_t * item = (ape_item_t *)list->data;
		meta_add_frame_from_ape_item(meta, item);
		list = g_slist_next(list);
	}
}


void
meta_ape_render_apic(meta_frame_t * frame, ape_item_t * item) {

	int len1 = strlen(frame->field_val);
	item->value_size = frame->length + len1 + 1;
	item->value = (unsigned char *)malloc(item->value_size);
	memcpy(item->value, frame->field_val, len1);
	(item->value)[len1] = '\0';
	memcpy(item->value + len1 + 1, frame->data, frame->length);
}


void
meta_ape_render_bin_frames(metadata_t * meta, ape_tag_t * tag, int type,
			   u_int32_t * item_count, u_int32_t * total_size) {

	meta_frame_t * frame;

	frame = metadata_get_frame_by_tag_and_type(meta, META_TAG_APE, type, NULL);
	while (frame) {
		char key[255];
		ape_item_t * item = meta_ape_item_new();
		item->flags = APE_FLAG_BINARY;
		if ((frame->flags & META_FIELD_LOCATOR) != 0) {
			item->flags = APE_FLAG_LOCATOR;
		}

		switch (type) {
		case META_FIELD_APIC:
			snprintf(key, 254, "Cover Art (%s)",
				 meta_id3v2_apic_type_to_string(frame->int_val));
			strncpy((char *)item->key, key, 255);
			meta_ape_render_apic(frame, item);
			break;
		case META_FIELD_HIDDEN:
			strncpy((char *)item->key, frame->field_val, 255);
			item->value_size = frame->length;
			item->value = (unsigned char *)malloc(item->value_size);
			memcpy(item->value, frame->data, item->value_size);
			break;
		default:
			fprintf(stderr, "meta_ape_render_bin_frames: programmer error: unknown frame type %d\n", type);
			free(item);
			return;
		}

		*total_size += 9 + strlen((char *)item->key) + item->value_size;
		++*item_count;
		tag->items = g_slist_append(tag->items, item);
		frame = metadata_get_frame_by_tag_and_type(meta, META_TAG_APE, type, frame);
	}	
}


void
metadata_to_ape_tag(metadata_t * meta, ape_tag_t * tag) {

	GSList * pfields = meta_get_possible_fields(META_TAG_APE);
	GSList * _pfields = pfields;
	u_int32_t item_count = 0;
	u_int32_t total_size = 0;

	while (pfields != NULL) {
		int type = GPOINTER_TO_INT(pfields->data);
		ape_item_t * item;
		meta_frame_t * frame;
		char * str;

		if (META_FIELD_BIN(type)) {
			meta_ape_render_bin_frames(meta, tag, type,
						   &item_count, &total_size);
			pfields = g_slist_next(pfields);
			continue;
		}

		frame = metadata_get_frame_by_tag_and_type(meta, META_TAG_APE, type, NULL);
		if (frame == NULL) {
			pfields = g_slist_next(pfields);
			continue;
		}

		item = meta_ape_item_new();
		if ((frame->flags & META_FIELD_LOCATOR) != 0) {
			item->flags = APE_FLAG_LOCATOR;
		}
		if (meta_get_fieldname_embedded(META_TAG_APE, type, &str)) {
			strncpy((char *)item->key, str, 255);
		} else {
			strncpy((char *)item->key, frame->field_name, 255);
		}
		while (frame != NULL) {
			char fval[MAXLEN];
			char * field_val = NULL;
			int field_len = 0;
			char * renderfmt = meta_get_field_renderfmt(type);

			if (META_FIELD_TEXT(type)) {
				field_val = frame->field_val;
				field_len = strlen(frame->field_val);
			} else if (META_FIELD_INT(type)) {
				snprintf(fval, MAXLEN-1, renderfmt, frame->int_val);
				field_val = fval;
				field_len = strlen(field_val);
			} else if (META_FIELD_FLOAT(type)) {
				snprintf(fval, MAXLEN-1, renderfmt, frame->float_val);
				field_val = fval;
				field_len = strlen(field_val);
			}

			item->value = realloc(item->value, item->value_size + field_len + 1);
			memcpy(item->value + item->value_size, field_val, field_len);
			item->value[item->value_size + field_len] = 0x00;
			item->value_size += field_len + 1;

			frame = metadata_get_frame_by_tag_and_type(meta, META_TAG_APE, type, frame);
		}
		--item->value_size; /* cut the last terminating zero */
		++item_count;
		total_size += 9 + strlen((char *)item->key) + item->value_size;
		tag->items = g_slist_append(tag->items, item);
		pfields = g_slist_next(pfields);
	}

	tag->header.version = 2000;
	tag->header.tag_size = total_size + 32; /* don't count the header */
	tag->header.item_count = item_count;
	tag->header.flags = APE_FLAG_HEADER | APE_FLAG_HAS_HEADER;

	tag->footer = tag->header;
	tag->footer.flags &= ~APE_FLAG_HEADER;

	g_slist_free(_pfields);
}


int
meta_ape_render_header(ape_header_t * header, unsigned char * data) {

	data[0] = 'A'; data[1] = 'P'; data[2] = 'E'; data[3] = 'T';
	data[4] = 'A'; data[5] = 'G'; data[6] = 'E'; data[7] = 'X';
	meta_write_int32(header->version, data+8);
	meta_write_int32(header->tag_size, data+12);
	meta_write_int32(header->item_count, data+16);
	meta_write_int32(header->flags, data+20);
	memset(data+24, 0x00, 8);
	return 32;
}

int
meta_ape_render_item(ape_item_t * item, unsigned char * data) {

	u_int32_t slen = strlen((char *)item->key);
	meta_write_int32(item->value_size, data);
	meta_write_int32(item->flags, data+4);
	memcpy(data+8, item->key, slen);
	data[8 + slen] = 0x00;
	memcpy(data+slen+9, item->value, item->value_size);
	return 9 + slen + item->value_size;
}

void
meta_ape_render(ape_tag_t * tag, unsigned char * data) {

	u_int32_t pos = 0;
	GSList * items = tag->items;
	pos += meta_ape_render_header(&tag->header, data);
	while (items != NULL) {
		ape_item_t * item = (ape_item_t *)items->data;
		pos += meta_ape_render_item(item, data + pos);
		items = g_slist_next(items);
	}
	meta_ape_render_header(&tag->footer, data + pos);	
}


int
meta_ape_rewrite(char * filename, unsigned char * data, unsigned int length) {

	FILE * file;
	unsigned char buf[32];
	u_int32_t tag_size, flags;
	long pos;

	long offset = 0L;
	int has_id3v1 = 0;
	unsigned char id3v1[128];

	if ((file = fopen(filename, "r+b")) == NULL) {
		fprintf(stderr, "meta_ape_rewrite: fopen() failed\n");
		return META_ERROR_NOT_WRITABLE;
	}

 seek_to_footer:
	if (fseek(file, -32L + offset, SEEK_END) != 0) {
		fprintf(stderr, "meta_ape_rewrite: fseek() failed\n");
		fclose(file);
		return META_ERROR_INTERNAL;
	}

	if (fread(buf, 1, 32, file) != 32) {
		fprintf(stderr, "meta_ape_rewrite: fread() failed\n");
		fclose(file);
		return META_ERROR_INTERNAL;
	}

	if ((buf[0] != 'A') || (buf[1] != 'P') || (buf[2] != 'E') ||
	    (buf[3] != 'T') || (buf[4] != 'A') || (buf[5] != 'G') ||
	    (buf[6] != 'E') || (buf[7] != 'X')) {

		if (has_id3v1 == 0) {
			/* read last 128 bytes of file */
			if (fseek(file, -128L, SEEK_END) != 0) {
				fprintf(stderr, "meta_ape_rewrite: fseek() failed\n");
				fclose(file);
				return META_ERROR_INTERNAL;
			}
			if (fread(id3v1, 1, 128, file) != 128) {
				fprintf(stderr, "meta_ape_rewrite: fread() failed\n");
				fclose(file);
				return META_ERROR_INTERNAL;
			}

			if ((id3v1[0] == 'T') && (id3v1[1] == 'A') && (id3v1[2] == 'G')) {
				has_id3v1 = 1;
				offset = -128L;
				goto seek_to_footer;
			} else {
				/* no ID3v1, no APE -> append APE */
				fseek(file, 0L, SEEK_END);
				if (data != NULL && length > 0) {
					if (fwrite(data, 1, length, file) != length) {
						fprintf(stderr, "meta_ape_rewrite: fwrite() failed\n");
						fclose(file);
						return META_ERROR_INTERNAL;
					}
				}
				goto truncate_ape;
			}
		} else {
			/* we have ID3v1, but no APE -> write APE and append ID3v1 */
			fseek(file, -128L, SEEK_END);
			if (data != NULL && length > 0) {
				if (fwrite(data, 1, length, file) != length) {
					fprintf(stderr, "meta_ape_rewrite: fwrite() failed\n");
					fclose(file);
					return META_ERROR_INTERNAL;
				}
			}
			if (fwrite(id3v1, 1, 128, file) != 128) {
				fprintf(stderr, "meta_ape_rewrite: fwrite() failed\n");
				fclose(file);
				return META_ERROR_INTERNAL;
			}
			goto truncate_ape;
		}
	}

	/* overwrite existing APE tag, and append ID3v1 if we have it */

	tag_size = meta_read_int32(buf+12);
	flags = meta_read_int32(buf+20);
	pos = offset -(long)tag_size;
	if ((flags & APE_FLAG_HAS_HEADER) != 0) {
		pos -= 32;
	}

	if (fseek(file, pos, SEEK_END) != 0) {
		fprintf(stderr, "meta_ape_rewrite: fseek() failed\n");
		fclose(file);
		return META_ERROR_INTERNAL;
	}

	if (data != NULL && length > 0) {
		if (fwrite(data, 1, length, file) != length) {
			fprintf(stderr, "meta_ape_rewrite: fwrite() failed\n");
			fclose(file);
			return META_ERROR_INTERNAL;
		}
	}
	if (has_id3v1) {
		if (fwrite(id3v1, 1, 128, file) != 128) {
			fprintf(stderr, "meta_ape_rewrite: fwrite() failed\n");
			fclose(file);
			return META_ERROR_INTERNAL;
		}
	}
 truncate_ape:
	pos = ftell(file);
	fclose(file);

	if (truncate(filename, pos) < 0) {
		fprintf(stderr, "meta_ape_rewrite: truncate() failed on %s\n", filename);
		return META_ERROR_INTERNAL;
	}
	
	return META_ERROR_NONE;
}


int
meta_ape_delete(char * filename) {

	return meta_ape_rewrite(filename, NULL, 0);
}

int
meta_ape_replace_or_append(char * filename, ape_tag_t * tag) {

	int ret;
	unsigned char * data = calloc(1, tag->header.tag_size + 32);
	if (data == NULL) {
		fprintf(stderr, "meta_ape_replace_or_append: calloc error\n");
	}
	meta_ape_render(tag, data);
	ret = meta_ape_rewrite(filename, data, tag->header.tag_size + 32);
	free(data);
	return ret;
}


int
meta_ape_write_metadata(file_decoder_t * fdec, metadata_t * meta) {

	int ret;
	ape_tag_t tag;

	memset(&tag, 0x00, sizeof(ape_tag_t));
	metadata_to_ape_tag(meta, &tag);

	if (tag.header.item_count > 0) {
		ret = meta_ape_replace_or_append(fdec->filename, &tag);
	} else {
		ret = meta_ape_delete(fdec->filename);
	}
	meta_ape_free(&tag);
	return ret;
}

void
meta_ape_send_metadata(metadata_t * meta, file_decoder_t * fdec) {

	ape_tag_t tag;

	if (meta == NULL) {
		return;
	}

	memset(&tag, 0x00, sizeof(ape_tag_t));

	if (meta_ape_parse(fdec->filename, &tag)) {
		metadata_from_ape_tag(meta, &tag);
		meta_ape_free(&tag);
	}

	meta->valid_tags |= META_TAG_APE;

	if (access(fdec->filename, R_OK | W_OK) == 0) {
		meta->writable = 1;
		fdec->meta_write = meta_ape_write_metadata;
	} else {
		meta->writable = 0;
	}

	meta->fdec = fdec;
	fdec->meta = meta;
	if (fdec->meta_cb != NULL) {
		fdec->meta_cb(meta, fdec->meta_cbdata);
	}
}

