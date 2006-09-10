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

#ifndef _CDDB_LOOKUP_H
#define _CDDB_LOOKUP_H

#define CDDB_THREAD_ERROR   -1
#define CDDB_THREAD_BUSY     0
#define CDDB_THREAD_SUCCESS  1
#define CDDB_THREAD_FREE     2

void cddb_get(GtkTreeIter * iter_record);
void cddb_get_batch(build_record_t * record, int cddb_title, int cddb_artist, int cddb_record);
void cddb_submit(GtkTreeIter * iter_record);

#endif /* _CDDB_LOOKUP_H */
