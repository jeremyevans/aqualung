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


#include <config.h>

#include <stdio.h>

#include "spinlock.h"

#define SL_MASK_SLAVE  0x01
#define SL_MASK_MASTER 0x02
#define SL_MASK_TURN   0x04


void
spin_waitlock_m(volatile int * sl) {

	(*sl) |= SL_MASK_MASTER;
	(*sl) |= SL_MASK_TURN;

	while (((*sl) & SL_MASK_SLAVE) && ((*sl) & SL_MASK_TURN))
		;

}


void
spin_waitlock_s(volatile int * sl) {

	(*sl) |= SL_MASK_SLAVE;
	(*sl) &= !SL_MASK_TURN;

	while (((*sl) & SL_MASK_MASTER) && (!((*sl) & SL_MASK_TURN)))
		;

}


void
spin_unlock_m(volatile int * sl) {

	(*sl) &= !SL_MASK_MASTER;
}


void
spin_unlock_s(volatile int * sl) {

	(*sl) &= !SL_MASK_SLAVE;
}


int
spin_islocked_m(volatile int * sl) {

	return (((*sl) & SL_MASK_MASTER) &&
		((!((*sl) & SL_MASK_SLAVE)) || (!((*sl) & SL_MASK_TURN))));
}


int
spin_islocked_s(volatile int * sl) {

	return (((*sl) & SL_MASK_SLAVE) &&
		((!((*sl) & SL_MASK_SLAVE)) || (((*sl) & SL_MASK_TURN))));
}
