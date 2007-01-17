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


#ifdef __cplusplus
extern "C" {
#endif


#define BUILD_THREAD_BUSY  0
#define BUILD_THREAD_FREE  1

/* Please update when we reach the 22nd century. */
#define YEAR_MIN 1900
#define YEAR_MAX 2100

typedef struct _map_t {

	char str[MAXLEN];
	int count;
	struct _map_t * next;

} map_t;

void map_put(map_t ** map, char * str);
char * map_get_max(map_t * map);
void map_free(map_t * map);


enum {
	DATA_SRC_CDDB = 0,
	DATA_SRC_META,
	DATA_SRC_FILE
};


typedef struct _build_track_t {

	char filename[MAXLEN];
	char d_name[MAXLEN];

	char name[3][MAXLEN];
	char final[MAXLEN];

	char comment[MAXLEN];
	float duration;
	float rva;

	struct _build_track_t * next;

} build_track_t;

typedef struct {

	char d_name[MAXLEN];
	char sort[MAXLEN];

	char name[3][MAXLEN];
	char final[MAXLEN];

	char year[MAXLEN];
	char comment[MAXLEN];

} build_record_t;

typedef struct {

	char d_name[MAXLEN];
	char sort[MAXLEN];

	char name[3][MAXLEN];
	char final[MAXLEN];

} build_artist_t;

typedef struct {

	build_artist_t artist;
	build_record_t record;
	build_track_t * tracks;

	int flag;
	GtkTreeIter iter;

} build_disc_t;


int is_valid_year(long y);
int is_all_wspace(char * str);

void build_store(GtkTreeIter store_iter);

int build_thread_test(int state);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* _BUILD_STORE_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

