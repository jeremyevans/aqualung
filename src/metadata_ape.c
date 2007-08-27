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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <glib.h>

#include "utils.h"
#include "options.h"
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
meta_ape_read_item(FILE * file, ape_tag_t * tag) {

	int i;
	unsigned char buf[8];
	ape_item_t * item = meta_ape_item_new();
	if (item == NULL) {
		return -1;
	}

	if (fread(buf, 1, 8, file) != 8) {
		fprintf(stderr, "meta_ape_read_item: fread() failed\n");
		free(item);
		return -1;
	}

	/* catch corrupt files with wrong item_count */
	if ((buf[0] == 'A') && (buf[1] == 'P') && (buf[2] == 'E') && (buf[3] == 'T') &&
	    (buf[4] == 'A') && (buf[5] == 'G') && (buf[6] == 'E') && (buf[7] == 'X')) {

		fprintf(stderr, "warning: corrupt APE tag: number of items too large\n");
		return -1;
	}

	item->value_size = meta_read_int32(buf);
	item->flags = meta_read_int32(buf+4);
	
	for (i = 0; i < 256; i++) {
		if (fread(item->key+i, 1, 1, file) != 1) {
			fprintf(stderr, "meta_ape_read_item: fread() failed\n");
			free(item);
			return -1;
		}
		if (item->key[i] == '\0') {
			break;
		}
	}

	item->value = malloc(item->value_size+1);
	if (item->value == NULL) {
		fprintf(stderr, "meta_ape_read_item: malloc error\n");
		free(item);
		return -1;
	}
	if (fread(item->value, 1, item->value_size, file) != item->value_size) {
		fprintf(stderr, "meta_ape_read_item: fread() failed\n");
		free(item);
		return -1;
	}
	item->value[item->value_size] = '\0';

	tag->items = g_slist_append(tag->items, (gpointer)item);
	return 0;
}

int
meta_ape_parse(char * filename, ape_tag_t * tag) {

	FILE * file;
	unsigned char buf[32];
	long offset = 0L;
	int has_id3v1 = 0;
	int i;

	if ((file = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "meta_ape_parse: fopen() failed\n");
		return 0;
	}

	/* At the moment we only support APE tags located at the end of the file. */
	/* However, there may be an ID3v1 tag after the APE tag. */
 seek_to_footer:
	if (fseek(file, -32L + offset, SEEK_END) != 0) {
		fprintf(stderr, "meta_ape_parse: fseek() failed\n");
		fclose(file);
		return 0;
	}

	if (fread(buf, 1, 32, file) != 32) {
		fprintf(stderr, "meta_ape_parse: fread() failed\n");
		fclose(file);
		return 0;
	}

	if ((buf[0] != 'A') || (buf[1] != 'P') || (buf[2] != 'E') ||
	    (buf[3] != 'T') || (buf[4] != 'A') || (buf[5] != 'G') ||
	    (buf[6] != 'E') || (buf[7] != 'X')) {
		if (has_id3v1 == 0) {
			offset = -128L;
			has_id3v1 = 1;
			goto seek_to_footer;
		} else {
			fclose(file);
			return 0;
		}
	}

	tag->footer.version = meta_read_int32(buf+8);
	tag->footer.tag_size = meta_read_int32(buf+12);
	tag->footer.item_count = meta_read_int32(buf+16);
	tag->footer.flags = meta_read_int32(buf+20);

	if (fseek(file, -tag->footer.tag_size + offset, SEEK_END) != 0) {
		fprintf(stderr, "meta_ape_parse: fseek() failed\n");
		fclose(file);
		return 0;
	}

	for (i = 0; i < tag->footer.item_count; i++) {
		if (meta_ape_read_item(file, tag) < 0) {
			break;
		}
	}

	fclose(file);
	return 1;
}


void
meta_add_frame_from_ape_item(metadata_t * meta, ape_item_t * item) {

	int i;
	meta_frame_t * frame;

	if (APE_FLAG_IS_BINARY(item->flags)) {
		/* TODO */
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
			} else if (META_FIELD_FLOAT(frame->type)) {
				snprintf(fval, MAXLEN-1, renderfmt, frame->float_val);
				field_val = fval;
				field_len = strlen(field_val);
			} else {
				item->flags = APE_FLAG_BINARY;
				/* TODO */
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
					goto truncate_ape;
				}
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
	pos = -tag_size + offset;
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

	/* XXX */printf("meta_ape_rewrite: truncating file at 0x%08x\n", (unsigned int)pos);
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
meta_ape_send_metadata(file_decoder_t * fdec) {

	ape_tag_t tag;
	metadata_t * meta;

	memset(&tag, 0x00, sizeof(ape_tag_t));

	meta = metadata_new();
	if (meta == NULL) {
		return;
	}
	if (meta_ape_parse(fdec->filename, &tag)) {
		metadata_from_ape_tag(meta, &tag);
		meta_ape_free(&tag);
	}

	meta->valid_tags = META_TAG_APE;

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

