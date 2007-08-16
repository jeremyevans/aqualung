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

#include "common.h"
#include "i18n.h"
#include "decoder/file_decoder.h"
#include "utils.h"
#include "metadata.h"
#include "metadata_api.h"


int
metadata_get_title(metadata_t * meta, char ** str) {

	meta_frame_t * frame;

	if (meta == NULL)
		return 0;
	frame = metadata_get_frame(meta, META_FIELD_TITLE, NULL);
	if (frame != NULL) {
		*str = frame->field_val;
		return 1;
	} else {
		return 0;
	}
}


int
metadata_get_artist(metadata_t * meta, char ** str) {

	meta_frame_t * frame;

	if (meta == NULL)
		return 0;
	frame = metadata_get_frame(meta, META_FIELD_ARTIST, NULL);
	if (frame != NULL) {
		*str = frame->field_val;
		return 1;
	} else {
		return 0;
	}
}


int
metadata_get_album(metadata_t * meta, char ** str) {

	meta_frame_t * frame;

	if (meta == NULL)
		return 0;
	frame = metadata_get_frame(meta, META_FIELD_ALBUM, NULL);
	if (frame != NULL) {
		*str = frame->field_val;
		return 1;
	} else {
		return 0;
	}
}


int
metadata_get_date(metadata_t * meta, char ** str) {

	meta_frame_t * frame;

	if (meta == NULL)
		return 0;
	frame = metadata_get_frame(meta, META_FIELD_DATE, NULL);
	if (frame != NULL) {
		*str = frame->field_val;
		return 1;
	} else {
		return 0;
	}
}


int
metadata_get_genre(metadata_t * meta, char ** str) {

	meta_frame_t * frame;

	if (meta == NULL)
		return 0;
	frame = metadata_get_frame(meta, META_FIELD_GENRE, NULL);
	if (frame != NULL) {
		*str = frame->field_val;
		return 1;
	} else {
		return 0;
	}
}


int
metadata_get_comment(metadata_t * meta, char ** str) {

	meta_frame_t * frame;

	if (meta == NULL)
		return 0;
	frame = metadata_get_frame(meta, META_FIELD_COMMENT, NULL);
	if (frame != NULL) {
		*str = frame->field_val;
		return 1;
	} else {
		return 0;
	}
}


int
metadata_get_icy_name(metadata_t * meta, char ** str) {

	meta_frame_t * frame;

	if (meta == NULL)
		return 0;
	frame = metadata_get_frame(meta, META_FIELD_ICY_NAME, NULL);
	if (frame != NULL) {
		*str = frame->field_val;
		return 1;
	} else {
		return 0;
	}
}


int
metadata_get_icy_descr(metadata_t * meta, char ** str) {

	meta_frame_t * frame;

	if (meta == NULL)
		return 0;
	frame = metadata_get_frame(meta, META_FIELD_ICY_DESCR, NULL);
	if (frame != NULL) {
		*str = frame->field_val;
		return 1;
	} else {
		return 0;
	}
}


int
metadata_get_tracknum(metadata_t * meta, int * val) {

	meta_frame_t * frame;

	if (meta == NULL)
		return 0;
	frame = metadata_get_frame(meta, META_FIELD_TRACKNO, NULL);
	if (frame != NULL) {
		*val = frame->int_val;
		return 1;
	} else {
		return 0;
	}
}


int
metadata_get_rva(metadata_t * meta, float * fval) {

	meta_frame_t * frame;

	if (meta == NULL)
		return 0;
	frame = metadata_get_frame(meta, META_FIELD_RVA2, NULL);
	if (frame != NULL) {
		*fval = frame->float_val;
		return 1;
	} else {
		return 0;
	}
}


void
meta_update_frame(metadata_t * meta, int type, char * str, int val, float fval) {

	int tag = 1;

	while (tag) {
		if ((meta->valid_tags & tag) != 0) {
			meta_frame_t * frame;
			frame = metadata_get_frame_by_tag_and_type(meta, tag, type, NULL);
			if (frame == NULL) {
				/* add new frame */
				frame = meta_frame_new();
				if (frame == NULL) {
					fprintf(stderr, "meta_update_frame: calloc error\n");
					return;
				}
				frame->tag = tag;
				frame->type = type;
				frame->flags = meta_get_default_flags(tag, type);

				if (META_FIELD_TEXT(type)) {
					frame->field_val = strdup(str);
				} else if (META_FIELD_INT(type)) {
					frame->int_val = val;
				} else if (META_FIELD_FLOAT(type)) {
					frame->float_val = fval;
				} else { /* no binary frames in update_basic mode */
					fprintf(stderr, "meta_update_frame: #1: programmer error\n");
				}
				metadata_add_frame(meta, frame);
			} else {
				/* update frame */
				if (META_FIELD_TEXT(type)) {
					if (frame->field_val != NULL) {
						free(frame->field_val);
					}
					frame->field_val = strdup(str);
				} else if (META_FIELD_INT(type)) {
					frame->int_val = val;
				} else if (META_FIELD_FLOAT(type)) {
					frame->float_val = fval;
				} else { /* no binary frames in update_basic mode */
					fprintf(stderr, "meta_update_frame: #2: programmer error\n");
				}
			}
		}
		tag <<= 1;
	}
}


int
meta_update_basic(char * filename,
		  char * title, char * artist, char * album,
		  char * comment, char * genre, char * date, int trackno) {

	file_decoder_t * fdec = file_decoder_new();

	if (fdec == NULL) {
		return META_UPDATE_ERROR_NOMEM;
	}

	if (file_decoder_open(fdec, filename) != 0) {
		file_decoder_delete(fdec);
		return META_UPDATE_ERROR_OPEN;
	}

	if (fdec->meta == NULL) {
		file_decoder_close(fdec);
		file_decoder_delete(fdec);
		return META_UPDATE_ERROR_NO_METASUPPORT;
	}

	if (!fdec->meta->writable) {
		file_decoder_close(fdec);
		file_decoder_delete(fdec);
		return META_UPDATE_ERROR_NOT_WRITABLE;
	}
	

	if (fdec->meta_write == NULL) {
		file_decoder_close(fdec);
		file_decoder_delete(fdec);
		return META_UPDATE_ERROR_INTERNAL;
	}

	if (title != NULL && !is_all_wspace(title)) {
		cut_trailing_whitespace(title);
		meta_update_frame(fdec->meta, META_FIELD_TITLE, title, 0, 0.0f);
	}
	if (artist != NULL && !is_all_wspace(artist)) {
		cut_trailing_whitespace(artist);
		meta_update_frame(fdec->meta, META_FIELD_ARTIST, artist, 0, 0.0f);
	}
	if (album != NULL && !is_all_wspace(album)) {
		cut_trailing_whitespace(album);
		meta_update_frame(fdec->meta, META_FIELD_ALBUM, album, 0, 0.0f);
	}
	if (trackno != -1) {
		meta_update_frame(fdec->meta, META_FIELD_TRACKNO, NULL, trackno, 0.0f);
	}
	if (date != NULL && !is_all_wspace(date)) {
		cut_trailing_whitespace(date);
		meta_update_frame(fdec->meta, META_FIELD_DATE, date, 0, 0.0f);
	}
	if (genre != NULL && !is_all_wspace(genre)) {
		cut_trailing_whitespace(genre);
		meta_update_frame(fdec->meta, META_FIELD_GENRE, genre, 0, 0.0f);
	}
	if (comment != NULL && !is_all_wspace(comment)) {
		cut_trailing_whitespace(comment);
		meta_update_frame(fdec->meta, META_FIELD_COMMENT, comment, 0, 0.0f);
	}

	printf("dump before write:\n");
	metadata_dump(fdec->meta);
	fdec->meta_write(fdec, fdec->meta);

	file_decoder_close(fdec);
	file_decoder_delete(fdec);

	return META_UPDATE_ERROR_NONE;
}

const char *
meta_update_strerror(int error) {

	switch (error) {
	case META_UPDATE_ERROR_NONE: return _("Success");
	case META_UPDATE_ERROR_NOMEM: return _("Memory allocation error");
	case META_UPDATE_ERROR_OPEN: return _("Unable to open file");
	case META_UPDATE_ERROR_NO_METASUPPORT: return _("No metadata support for this format");
	case META_UPDATE_ERROR_NOT_WRITABLE: return _("File is not writable");
	case META_UPDATE_ERROR_INTERNAL: return _("Internal error");
	default: return _("Unknown error");
	}
}
