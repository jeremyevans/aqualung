/*                                                     -*- linux-c -*-
    Copyright (C) 2006 Tom Szilagyi

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

#ifndef _SPINLOCK_H
#define _SPINLOCK_H

/* Spinlocks are simple integer objects to use instead of pthread mutexes.
 * This implementation allows two threads (master, slave) to share a spinlock.
 */

void spin_waitlock_m(int * sl);
void spin_waitlock_s(int * sl);
void spin_unlock_m(int * sl);
void spin_unlock_s(int * sl);

/* Should be used from the opposite thread -- e.g. spin_islocked_m() should
 * be called from the slave thread to know if the master thread currently
 * holds the lock.
 */
int spin_islocked_m(int * sl);
int spin_islocked_s(int * sl);


#endif /* _SPINLOCK_H */
