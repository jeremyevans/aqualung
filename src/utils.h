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

#ifndef _UTILS_H
#define _UTILS_H

#include <config.h>

void make_title_string(char * dest, char * templ,
		       char * artist, char * record, char * track);

void pack_strings(char * str1, char * str2, char * result);
void unpack_strings(char * packed, char * str1, char * str2);
void normalize_filename(const char * in, char * out);

#ifndef HAVE_STRCASESTR
char * strcasestr(char * haystack, char * needle);
#endif /* HAVE_STRCASESTR */


#endif /* _UTILS_H */
