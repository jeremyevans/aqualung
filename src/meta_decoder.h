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

#ifdef HAVE_MOD
#include <libmodplug/modplug.h>
#endif /* HAVE_MOD */


#include "common.h"
#include "decoder/file_decoder.h"


#ifdef __cplusplus
extern "C" {
#endif


#ifdef HAVE_MOD
typedef struct _mod_info {
	char * title;
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

typedef struct _metadata {

	/* audio stream properties */
	int file_lib;
	char format_str[MAXLEN];
	int format_flags;
	unsigned long long total_samples;
	unsigned long sample_rate;
	int is_mono;
	int bps;

#ifdef HAVE_TAGLIB
	/* TagLib::File* cast to void* */
	void * taglib_file;
#endif /* HAVE_TAGLIB */

#ifdef HAVE_MOD
	mod_info * mod_root;
#endif /* HAVE_MOD */

} metadata;


int cut_trailing_whitespace(char * str);

void read_rva2(char * raw_data, unsigned int length, char * id_str, float * voladj);
int lookup_id3v2_textframe(char * frameID, char * descr);


metadata * meta_new(void);
int meta_read(metadata * meta, char * filename);
int meta_read_fdec(metadata * meta, char * filename, file_decoder_t * fdec);
void meta_free(metadata * meta);

int meta_get_rva(metadata * meta, float * fval);
int meta_get_title(metadata * meta, char * str);
int meta_get_record(metadata * meta, char * str);
int meta_get_artist(metadata * meta, char * str);
int meta_get_year(metadata * meta, char * str);
int meta_get_comment(metadata * meta, char * str);

#if defined(HAVE_TAGLIB) && defined(HAVE_METAEDIT)
int meta_update_basic(char * filename, char * title, char * artist, char * album,
		      char * comment, char * genre, char * year, char * track);
#endif /* HAVE_TAGLIB && HAVE_METAEDIT */


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _META_DECODER_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

