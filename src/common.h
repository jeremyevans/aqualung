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


/* Moving towards Gtk3...

   While still on Gtk2, compile Aqualung with

   make CFLAGS+="-DGTK_DISABLE_SINGLE_INCLUDES -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"

   Note, adding "-DGSEAL_ENABLE" breaks this...
*/
#define TVCOL_BUTTON(tvcol) tvcol->button

/* ... and this is what we'd want to use, but Gtk2 does not yet have that.
   So, when we build with Gtk3, we will do this:
*/
//#define TVCOL_BUTTON(tvcol) gtk_tree_view_column_get_button(tvcol)

#endif /* AQUALUNG_COMMON_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
