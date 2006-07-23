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


#ifndef _BUILD_STORE_H
#define _BUILD_STORE_H

typedef struct _map_t {

	char str[MAXLEN];
	int count;
	struct _map_t * next;

} map_t;

void map_put(map_t ** map, char * str);
char * map_get_max(map_t * map);
void map_free(map_t * map);


typedef struct _track_t {

	char filename[MAXLEN];
	char d_name[MAXLEN];
	char name[MAXLEN];
	float duration;
	int valid;

	struct _track_t * next;

} track_t;

void build_store(GtkTreeIter store_iter);

#endif /* _BUILD_STORE_H */
