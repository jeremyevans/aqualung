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

    $Id: cdda.h 339 2006-09-10 17:37:58Z tszilagyi $
*/

#ifndef _CDDA_H
#define _CDDA_H


#include <config.h>



#include "common.h"

#define CDDA_DRIVES_MAX 16
#define CDDA_MAXLEN 256

typedef struct {

} cdda_disc_t;


typedef struct {
	char device_path[CDDA_MAXLEN];
	char vendor[CDDA_MAXLEN];
	char model[CDDA_MAXLEN];
	char revision[CDDA_MAXLEN];
	cdda_disc_t disc;
} cdda_drive_t;


int cdda_get_drives(cdda_drive_t * drives);

void cdda_test(void);


#endif /* _CDDA_H */
