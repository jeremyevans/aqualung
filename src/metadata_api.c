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
#include "options.h"
#include "decoder/file_decoder.h"
#include "utils.h"
#include "metadata.h"
#include "metadata_api.h"


extern options_t options;

/* get frame of given type, with preference between different tags.
   preference order: MPEGSTREAM > GEN_STREAM > OXC > APE > ID3v2 > ID3v1 */
meta_frame_t *
metadata_pref_frame_by_type(metadata_t * meta, int type, meta_frame_t * root) {

	meta_frame_t * frame;
	int tag = META_TAG_MAX;

	while (tag > 0) {
		frame = metadata_get_frame_by_tag_and_type(meta, tag, type, root);
		if (frame) {
			return frame;
		}
		tag >>= 1;
	}

	return NULL;
}


int
metadata_get_title(metadata_t * meta, char ** str) {

	meta_frame_t * frame;

	if (meta == NULL)
		return 0;

#ifdef HAVE_MOD
	frame = metadata_get_frame_by_tag_and_type(meta, META_TAG_MODINFO, META_FIELD_MODINFO, NULL);
	if (frame != NULL) {
		mod_info * mi = (mod_info *)frame->data;
		*str = mi->title;
		return 1;
	}
#endif /* HAVE_MOD */

	frame = metadata_pref_frame_by_type(meta, META_FIELD_TITLE, NULL);
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
	frame = metadata_pref_frame_by_type(meta, META_FIELD_ARTIST, NULL);
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
	frame = metadata_pref_frame_by_type(meta, META_FIELD_ALBUM, NULL);
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
	frame = metadata_pref_frame_by_type(meta, META_FIELD_DATE, NULL);
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
	frame = metadata_pref_frame_by_type(meta, META_FIELD_GENRE, NULL);
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
	frame = metadata_pref_frame_by_type(meta, META_FIELD_COMMENT, NULL);
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
	frame = metadata_pref_frame_by_type(meta, META_FIELD_ICY_NAME, NULL);
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
	frame = metadata_pref_frame_by_type(meta, META_FIELD_ICY_DESCR, NULL);
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
	frame = metadata_pref_frame_by_type(meta, META_FIELD_TRACKNO, NULL);
	if (frame != NULL) {
		*val = frame->int_val;
		return 1;
	} else {
		return 0;
	}
}


int
metadata_get_rva(metadata_t * meta, float * fval) {

	int rva_type;
	meta_frame_t * frame;

	if (meta == NULL)
		return 0;

	switch (options.replaygain_tag_to_use) {
	case 0: rva_type = META_FIELD_RG_TRACK_GAIN;
		break;
	default: rva_type = META_FIELD_RG_ALBUM_GAIN;
		break;
	}

	frame = metadata_pref_frame_by_type(meta, rva_type, NULL);
	if (frame != NULL) {
		*fval = frame->float_val;
		return 1;
	} else {
		frame = metadata_pref_frame_by_type(meta, META_FIELD_RVA2, NULL);
		if (frame != NULL) {
			*fval = frame->float_val;
			return 1;
		}
	}
	return 0;
}


void
meta_update_frame_data(meta_frame_t * frame, char * str, int val, float fval) {

	if (META_FIELD_TEXT(frame->type)) {
		frame->field_val = strdup(str);
	} else if (META_FIELD_INT(frame->type)) {
		frame->int_val = val;
	} else if (META_FIELD_FLOAT(frame->type)) {
		frame->float_val = fval;
	} else { /* no binary frames in update_basic mode */
		fprintf(stderr, "meta_update_frame_data: programmer error\n");
	}
}


void
meta_update_frame(metadata_t * meta, int add_tags, int type,
		  char * str, int val, float fval) {

	int tag = 1;
	while (tag) {
		meta_frame_t * frame;

		if ((meta->valid_tags & tag) == 0) {
			tag <<= 1;
			continue;
		}

		frame = metadata_get_frame_by_tag_and_type(meta, tag, type, NULL);
		if ((frame == NULL) && ((add_tags & tag) != 0)) {
			/* add new frame */
			frame = meta_frame_new();
			if (frame == NULL) {
				fprintf(stderr, "meta_update_frame: calloc error\n");
				return;
			}
			frame->tag = tag;
			frame->type = type;
			frame->flags = meta_get_default_flags(tag, type);
			meta_update_frame_data(frame, str, val, fval);
			metadata_add_frame(meta, frame);
		} else if (frame != NULL) {
			meta_update_frame_data(frame, str, val, fval);
		}
		tag <<= 1;
	}
}


int
meta_update_basic(char * filename,
		  char * title, char * artist, char * album,
		  char * comment, char * genre, char * date, int trackno) {

	file_decoder_t * fdec = file_decoder_new();
	int add_tags;
	int ret;

	if (fdec == NULL) {
		return META_ERROR_NOMEM;
	}

	if (file_decoder_open(fdec, filename) != 0) {
		file_decoder_delete(fdec);
		return META_ERROR_OPEN;
	}

	if (fdec->meta == NULL) {
		file_decoder_close(fdec);
		file_decoder_delete(fdec);
		return META_ERROR_NO_METASUPPORT;
	}

	if (!fdec->meta->writable) {
		file_decoder_close(fdec);
		file_decoder_delete(fdec);
		return META_ERROR_NOT_WRITABLE;
	}

	if (fdec->file_lib == MAD_LIB) {
		add_tags = 0;
		if (options.batch_mpeg_add_id3v1) {
			add_tags |= META_TAG_ID3v1;
		}
		if (options.batch_mpeg_add_id3v2) {
			add_tags |= META_TAG_ID3v2;
		}
		if (options.batch_mpeg_add_ape) {
			add_tags |= META_TAG_APE;
		}
	} else {
		add_tags = fdec->meta->valid_tags;
	}

	if (fdec->meta_write == NULL) {
		file_decoder_close(fdec);
		file_decoder_delete(fdec);
		return META_ERROR_INTERNAL;
	}

	if (title != NULL && !is_all_wspace(title)) {
		cut_trailing_whitespace(title);
		meta_update_frame(fdec->meta, add_tags, META_FIELD_TITLE, title, 0, 0.0f);
	}
	if (artist != NULL && !is_all_wspace(artist)) {
		cut_trailing_whitespace(artist);
		meta_update_frame(fdec->meta, add_tags, META_FIELD_ARTIST, artist, 0, 0.0f);
	}
	if (album != NULL && !is_all_wspace(album)) {
		cut_trailing_whitespace(album);
		meta_update_frame(fdec->meta, add_tags, META_FIELD_ALBUM, album, 0, 0.0f);
	}
	if (trackno != -1) {
		meta_update_frame(fdec->meta, add_tags, META_FIELD_TRACKNO, NULL, trackno, 0.0f);
	}
	if (date != NULL && !is_all_wspace(date)) {
		cut_trailing_whitespace(date);
		meta_update_frame(fdec->meta, add_tags, META_FIELD_DATE, date, 0, 0.0f);
	}
	if (genre != NULL && !is_all_wspace(genre)) {
		cut_trailing_whitespace(genre);
		meta_update_frame(fdec->meta, add_tags, META_FIELD_GENRE, genre, 0, 0.0f);
	}
	if (comment != NULL && !is_all_wspace(comment)) {
		cut_trailing_whitespace(comment);
		meta_update_frame(fdec->meta, add_tags, META_FIELD_COMMENT, comment, 0, 0.0f);
	}

	/* XXX debug */
	printf("dump before write:\n");
	metadata_dump(fdec->meta);

	ret = fdec->meta_write(fdec, fdec->meta);
	file_decoder_close(fdec);
	file_decoder_delete(fdec);

	return ret;
}

const char *
metadata_strerror(int error) {

	switch (error) {
	case META_ERROR_NONE: return _("Success");
	case META_ERROR_NOMEM: return _("Memory allocation error");
	case META_ERROR_OPEN: return _("Unable to open file");
	case META_ERROR_NO_METASUPPORT: return _("No metadata support for this format");
	case META_ERROR_NOT_WRITABLE: return _("File is not writable");
	case META_ERROR_INVALID_TRACKNO: return _("Invalid 'Track no.' field value");
	case META_ERROR_INVALID_GENRE: return _("Invalid 'Genre' field value");
	case META_ERROR_INVALID_CODING: return _("Conversion to target charset failed");
	case META_ERROR_INTERNAL: return _("Internal error");
	default: return _("Unknown error");
	}
}
