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


extern options_t options;


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
	
	p = meta->root->next;
	while (p != NULL) {
		q = p->next;
		meta_frame_free(p);
		p = q;
	}
	free(meta->root);
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


void
metadata_dump(metadata_t * meta) {

	meta_frame_t * frame = meta->root;
	printf("\nMetadata block dump, writable = %d\n", meta->writable);
	while (frame) {
		meta_dump_frame(frame);
		frame = frame->next;
	}
}


void
meta_dump_frame(meta_frame_t * frame) {
	printf("  Type %4d  '%s'  '%s'  ",
	       frame->type, frame->field_name, frame->field_val);
	printf("int %d  float %f  ptr %p  len %d\n",
	       frame->int_val, frame->float_val, frame->data, frame->length);
}


void
metadata_add_textframe_from_keyval(metadata_t * meta, char * key, char * val) {

	if (strcmp("Title", key) == 0) {
		meta_frame_t * frame = meta_frame_new();
		frame->type = META_FIELD_TITLE;
		frame->field_name = strdup(key);
		frame->field_val = strdup(val);
		metadata_add_frame(meta, frame);
	} else if (strcmp("Artist", key) == 0) {
		meta_frame_t * frame = meta_frame_new();
		frame->type = META_FIELD_ARTIST;
		frame->field_name = strdup(key);
		frame->field_val = strdup(val);
		metadata_add_frame(meta, frame);
	} else if (strcmp("Album", key) == 0) {
		meta_frame_t * frame = meta_frame_new();
		frame->type = META_FIELD_ALBUM;
		frame->field_name = strdup(key);
		frame->field_val = strdup(val);
		metadata_add_frame(meta, frame);
	} else if (strcmp("Icy-Name", key) == 0) {
		meta_frame_t * frame = meta_frame_new();
		frame->type = META_FIELD_ICY_NAME;
		frame->field_name = strdup(key);
		frame->field_val = strdup(val);
		metadata_add_frame(meta, frame);
	} else if (strcmp("Icy-Genre", key) == 0) {
		meta_frame_t * frame = meta_frame_new();
		frame->type = META_FIELD_ICY_GENRE;
		frame->field_name = strdup(key);
		frame->field_val = strdup(val);
		metadata_add_frame(meta, frame);
	} else if (strcmp("Icy-Description", key) == 0) {
		meta_frame_t * frame = meta_frame_new();
		frame->type = META_FIELD_ICY_DESCR;
		frame->field_name = strdup(key);
		frame->field_val = strdup(val);
		metadata_add_frame(meta, frame);
	} else {
		meta_frame_t * frame = meta_frame_new();
		frame->type = META_FIELD_OTHER;
		frame->field_name = strdup(key);
		frame->field_val = strdup(val);
		metadata_add_frame(meta, frame);
	}
}


#ifdef HAVE_OGG_VORBIS
metadata_t *
metadata_from_vorbis_comment(vorbis_comment * vc) {

	int i;
	metadata_t * meta;

	if (!vc) {
		return NULL;
	}

	meta = metadata_new();
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
		
		metadata_add_textframe_from_keyval(meta, key, val);
	}

	{ /* Add Vendor string */
		meta_frame_t * frame = meta_frame_new();
		frame->type = META_FIELD_VENDOR;
		frame->field_name = strdup(_("Vendor"));
		frame->field_val = strdup(vc->vendor);
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

		metadata_add_textframe_from_keyval(meta, key, val);

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


char *
metadata_get_title(metadata_t * meta) {

	meta_frame_t * frame = metadata_get_frame(meta, META_FIELD_TITLE, NULL);
	if (frame != NULL) {
		return frame->field_val;
	} else {
		return "";
	}
}


char *
metadata_get_artist(metadata_t * meta) {

	meta_frame_t * frame = metadata_get_frame(meta, META_FIELD_ARTIST, NULL);
	if (frame != NULL) {
		return frame->field_val;
	} else {
		return "";
	}
}


char *
metadata_get_album(metadata_t * meta) {

	meta_frame_t * frame = metadata_get_frame(meta, META_FIELD_ALBUM, NULL);
	if (frame != NULL) {
		return frame->field_val;
	} else {
		return "";
	}
}


char *
metadata_get_icy_name(metadata_t * meta) {

	meta_frame_t * frame = metadata_get_frame(meta, META_FIELD_ICY_NAME, NULL);
	if (frame != NULL) {
		return frame->field_val;
	} else {
		return "";
	}
}


char *
metadata_get_icy_descr(metadata_t * meta) {

	meta_frame_t * frame = metadata_get_frame(meta, META_FIELD_ICY_DESCR, NULL);
	if (frame != NULL) {
		return frame->field_val;
	} else {
		return "";
	}
}


void
metadata_make_title_string(metadata_t * meta, char * dest) {

	char dest1[MAXLEN];
	char * artist = metadata_get_artist(meta);
	char * album = metadata_get_album(meta);
	char * title = metadata_get_title(meta);
	char * icy_name = metadata_get_icy_name(meta);

	if (title[0] != '\0' && artist[0] != '\0' && album[0] != '\0') {
		make_title_string(dest1, options.title_format,
				  artist, album, title);
	} else if (title[0] != '\0' && artist[0] != '\0') {
		make_title_string_no_album(dest1, options.title_format_no_album,
					   artist, title);
	} else if (title[0] != '\0') {
		strncpy(dest1, metadata_get_title(meta), MAXLEN-1);
	} else {
		if (icy_name[0] == '\0') {
			dest[0] = '\0';
		} else {
			strncpy(dest, icy_name, MAXLEN-1);
		}
		return;
	}

	if (icy_name[0] == '\0') {
		strncpy(dest, dest1, MAXLEN-1);
	} else {
		snprintf(dest, MAXLEN-1, "%s (%s)", dest1, icy_name);
	}
}


void
metadata_make_playlist_string(metadata_t * meta, char * dest) {

	char * name = metadata_get_icy_name(meta);
	char * descr = metadata_get_icy_descr(meta);

	if (name[0] == '\0') {
		metadata_make_title_string(meta, dest);
	} else {
		if (descr[0] != '\0') {
			snprintf(dest, MAXLEN-1, "%s (%s)", name, descr);
		} else {
			strncpy(dest, name, MAXLEN-1);
		}
	}
}
