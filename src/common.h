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

#ifndef AQUALUNG_COMMON_H
#define AQUALUNG_COMMON_H


#define MAXLEN 1024

#define MAX_FONTNAME_LEN    256
#define MAX_COLORNAME_LEN   32


/* When built with a C11 compiler, CHAR_ARRAY_SIZE will cause an error if the
   argument is not actually a char[] array. */
#if __STDC_VERSION__ >= 201112L
#define CHAR_ARRAY_SIZE(array) _Generic(&(array), char (*)[]: sizeof(array))
#else
#define CHAR_ARRAY_SIZE(array) sizeof(array)
#endif

/* Since Aqualung uses fixed-size C strings in many places, provide wrappers
   for bounded string operations that use CHAR_ARRAY_SIZE to ensure it's an
   array. */
#define arr_strlcat(dest, src) \
        g_strlcat((dest), (src), CHAR_ARRAY_SIZE(dest))
#define arr_strlcpy(dest, src) \
        g_strlcpy((dest), (src), CHAR_ARRAY_SIZE(dest))
#define arr_snprintf(dest, ...) \
        snprintf((dest), CHAR_ARRAY_SIZE(dest), __VA_ARGS__)


#endif /* AQUALUNG_COMMON_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
