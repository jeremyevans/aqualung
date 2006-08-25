/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi

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

#ifndef _META_DECODER_H
#define _META_DECODER_H

#include <config.h>

#ifdef HAVE_ID3
#include <id3tag.h>
#endif /* HAVE_ID3 */

#ifdef HAVE_FLAC
#include <FLAC/format.h>
#include <FLAC/metadata.h>
#endif /* HAVE_FLAC */

#include "decoder/file_decoder.h"


#ifdef HAVE_ID3
typedef struct _id3_tag_data {
	char id[5];
	char * label;
	id3_utf8_t * utf8;
	float fval;
	struct _id3_tag_data * next;
} id3_tag_data;
#endif /* HAVE_ID3 */


/* we use this for both FLAC and Ogg */
typedef struct _oggv_comment {
	char * label;
	char * str;
	float fval;
	struct _oggv_comment * next;
} oggv_comment;


typedef struct _metadata {

	/* audio stream properties */
	int format_major;
	int format_minor;
	unsigned long long total_samples;
	unsigned long sample_rate;
	int is_mono;
	int bps;

#ifdef HAVE_ID3
	id3_tag_data * id3_root;
#endif /* HAVE_ID3 */

#ifdef HAVE_FLAC
	oggv_comment * flac_root;
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
	oggv_comment * oggv_root;
#endif /* HAVE_OGG_VORBIS */

} metadata;


metadata * meta_new(void);
int meta_read(metadata * meta, char * filename);
void meta_free(metadata * meta);
/*void meta_list(metadata * meta);*/ /* for DEBUG purposes */

int meta_get_rva(metadata * meta, float * fval);
int meta_get_title(metadata * meta, char * str);
int meta_get_record(metadata * meta, char * str);
int meta_get_artist(metadata * meta, char * str);
int meta_get_year(metadata * meta, char * str);
int meta_get_comment(metadata * meta, char * str);


#endif /* _META_DECODER_H */
