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

#ifndef _SEARCH_H
#define _SEARCH_H

/* search flags */
#define SEARCH_F_CS (1 << 0)    /* case sensitive */
#define SEARCH_F_EM (1 << 1)    /* exact matches only */
#define SEARCH_F_SF (1 << 2)    /* select first and close */
#define SEARCH_F_AN (1 << 3)    /* artist names */
#define SEARCH_F_RT (1 << 4)    /* record titles */
#define SEARCH_F_TT (1 << 5)    /* track titles */
#define SEARCH_F_CO (1 << 6)    /* comments */

void search_dialog(void);


#endif /* _SEARCH_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  
