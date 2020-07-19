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

#ifndef AQUALUNG_UTILS_H
#define AQUALUNG_UTILS_H

#include <config.h>

#include "common.h"


/* Please update when we reach the 22nd century. */
#define YEAR_MIN 1900
#define YEAR_MAX 2100


float convf(char * s);
int cut_trailing_whitespace(char * str);
void escape_percents(char * in, char * out);

int make_string_va(char * buf, char * format, ...);
void make_title_string(char * dest, char * templ,
		       char * artist, char * record, char * track);
void make_string_strerror(int ret, char * buf);

void sample2time(unsigned long SR, unsigned long long sample, char * str, size_t str_size, int sign);
void time2time(float samples, char * str, size_t str_size);
void time2time_na(float seconds, char * str, size_t str_size);

void normalize_filename(const char * in, char * out, size_t out_size);

void free_strdup(char ** s, const char * str);
int is_valid_year(int y);
int is_all_wspace(char * str);

int is_dir(char * path);

#ifndef HAVE_STRNDUP
char * strndup(char * str, size_t len);
#endif /* HAVE_STRNDUP */

#ifndef HAVE_STRCASESTR
char * strcasestr(char * haystack, char * needle);
#endif /* HAVE_STRCASESTR */


typedef struct _map_t {

	char str[MAXLEN];
	int count;
	struct _map_t * next;

} map_t;

void map_put(map_t ** map, char * str);
char * map_get_max(map_t * map);
void map_free(map_t * map);


#endif /* AQUALUNG_UTILS_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8:

