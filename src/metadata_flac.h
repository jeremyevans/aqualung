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


#ifndef _METADATA_FLAC_H
#define _METADATA_FLAC_H

#include <config.h>

#ifdef HAVE_FLAC
#include <FLAC/metadata.h>
#endif /* HAVE_FLAC */

#include <glib.h>

#include "common.h"
#include "metadata.h"

#ifdef HAVE_FLAC
metadata_t * metadata_from_flac_streammeta(FLAC__StreamMetadata_VorbisComment * vc);
FLAC__StreamMetadata * metadata_to_flac_streammeta(metadata_t * meta);
#endif /* HAVE_FLAC */


#endif /* _METADATA_FLAC_H */
