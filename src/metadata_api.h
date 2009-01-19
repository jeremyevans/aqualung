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

/* Aqualung metadata internal API usage:
 *   - open a file_decoder on file;
 *   - fdec->meta is the metablock of the file;
 *   - access/change it with functions in this module;
 *   - call fdec->meta_write to write it back to the file
 *     (only possible if fdec->meta->writable is true);
 *   - close the file_decoder.
 */

#ifndef _METADATA_API_H
#define _METADATA_API_H

#include <config.h>

#include "metadata.h"


/* Query frames of a given type, with respect to preference order
   between tags. */
meta_frame_t * metadata_pref_frame_by_type(metadata_t * meta, int type, meta_frame_t * root);


int metadata_get_string_field(metadata_t * meta, int type, char ** str);
/* High-level accessor functions
 *
 * Return value: 1 if found, 0 if not found.
 * Note that in case of text strings, if a match is returned,
 * it is still owned by the meta object (only the pointer is
 * passed back), so it should not be freed by the caller.
 * Number data will be copied to the supplied pointer.
 */
int metadata_get_title(metadata_t * meta, char ** str);
int metadata_get_artist(metadata_t * meta, char ** str);
int metadata_get_album(metadata_t * meta, char ** str);
int metadata_get_date(metadata_t * meta, char ** str);
int metadata_get_genre(metadata_t * meta, char ** str);
int metadata_get_comment(metadata_t * meta, char ** str);
int metadata_get_icy_name(metadata_t * meta, char ** str);
int metadata_get_icy_descr(metadata_t * meta, char ** str);
int metadata_get_tracknum(metadata_t * meta, int * val);
int metadata_get_rva(metadata_t * meta, float * fval);


/* Return values of meta_update_basic() */
#define META_ERROR_NONE                  0
#define META_ERROR_NOMEM                -1
#define META_ERROR_OPEN                 -2
#define META_ERROR_NO_METASUPPORT       -3
#define META_ERROR_NOT_WRITABLE         -4
#define META_ERROR_INVALID_TRACKNO      -5
#define META_ERROR_INVALID_GENRE        -6
#define META_ERROR_INVALID_CODING       -7
#define META_ERROR_INTERNAL             -8


/* Update basic metadata fields of a file. Used for mass-tagging.
 *
 * Any input string may be NULL, in which case that field won't
 * be updated. Set trackno to -1 to leave it unaffected.
 * Existing metadata not updated will be retained.
 *
 * filename should be locale encoded, text fields should be UTF8.
 *
 * Return 0 if OK, < 0 else.
 * Use metadata_strerror() to get an error string from the
 * return value.
 */
int
meta_update_basic(char * filename,
		  char * title, char * artist, char * album,
		  char * comment, char * genre, char * date, int trackno);

const char * metadata_strerror(int error);


#endif /* _METADATA_API_H */
