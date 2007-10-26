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


#ifndef _METADATA_ID3V2_H
#define _METADATA_ID3V2_H

#include <config.h>

#include <glib.h>

#include "common.h"
#include "metadata.h"


char * meta_id3v2_apic_type_to_string(int type);

u_int32_t meta_id3v2_read_int(unsigned char * buf);
u_int32_t meta_id3v2_read_synchsafe_int(unsigned char * buf);

int metadata_from_id3v2(metadata_t * meta, unsigned char * buf, int length);
int metadata_to_id3v2(metadata_t * meta, unsigned char ** data, int * length);

char * meta_id3v2_to_utf8(unsigned char enc, unsigned char * buf, int len);

int meta_id3v2_delete(char * filename);
int meta_id3v2_rewrite(char * filename, unsigned char * buf, int len);

#endif /* _METADATA_ID3V2_H */
