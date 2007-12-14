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

#ifndef _CD_RIPPER_H
#define _CD_RIPPER_H

#include <config.h>

#if defined(HAVE_CDDA) && (defined(HAVE_SNDFILE) || defined(HAVE_FLAC) || defined(HAVE_VORBISENC) || defined(HAVE_LAME))
#define HAVE_CD_RIPPER

#include <gtk/gtk.h>

#include "cdda.h"

void cd_ripper(cdda_drive_t * drive, GtkTreeIter * iter);

#endif /* HAVE_CDDA && ... */

#endif /* _CD_RIPPER_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

