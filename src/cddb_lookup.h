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

void cddb_start_query(GtkTreeIter * record_iter, int ntracks, int * frames, int length);
void cddb_start_submit(GtkTreeIter * iter_record, int ntracks, int * frames, int length);
void cddb_auto_query_cdda(GtkTreeIter * drive_iter, int ntracks, int * frames, int length);

void cddb_query_batch(int ntracks, int * frames, int length,
		      char * artist, char * record, int * year, char ** tracks);

#endif /* _CDDB_LOOKUP_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
