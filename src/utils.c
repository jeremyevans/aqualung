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

#ifndef HAVE_STRCASESTR

#include <string.h>

char
toupper(char c) {

	if (c >= 'a' && c <= 'z')
		return (c-32);
	return c;
}

char *
strcasestr(char *haystack, char *needle) {

	char *s1 = needle;
	char *s2 = haystack;
	char *sr = NULL;
	int inside = 0;

	while (*s2 != '\0') {
		if (toupper(*s1) == toupper(*s2)) {
			if (inside == 0) {
				sr = s2;
			}
			inside = 1;
			++s1;
			++s2;
			if (*s1 == '\0') {
				break;
			}
		} else {
			inside = 0;
			s1 = needle;
			++s2;
		}
	}
	if (inside == 1)
		return sr;
	return NULL;
}

#endif /* HAVE_STRCASESTR */
