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

#ifndef AQUALUNG_METADATA_ID3V1_H
#define AQUALUNG_METADATA_ID3V1_H

#include "metadata.h"


char * id3v1_genre_str_from_code(int code);
int id3v1_genre_code_from_str(char * str);

/* these return a newly allocated string, or NULL */
char * meta_id3v1_utf8_to_tagenc(char * utf8);
char * meta_id3v1_utf8_from_tagenc(char * tagenc);

int metadata_from_id3v1(metadata_t * meta, unsigned char * buf);
int metadata_to_id3v1(metadata_t * meta, unsigned char * buf);

int meta_id3v1_delete(char * filename);
int meta_id3v1_rewrite(char * filename, unsigned char * id3v1);


#endif /* AQUALUNG_METADATA_ID3V1_H */
