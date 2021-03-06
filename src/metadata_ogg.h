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

#ifndef AQUALUNG_METADATA_OGG_H
#define AQUALUNG_METADATA_OGG_H

#include <config.h>

#include <glib.h>

#ifdef HAVE_VORBIS
#include <vorbis/codec.h>
#endif /* HAVE_VORBIS */

#include "metadata.h"


#define META_OGG_FRAGM_PACKET 0x01
#define META_OGG_BOS          0x02
#define META_OGG_EOS          0x04

typedef struct {
	unsigned char version;
	char flags;
	guint64 granulepos;
	guint32 serialno;
	guint32 seqno;
	guint32 checksum;
	unsigned char n_segments;
	unsigned char segment_table[256];
	unsigned char * data;
} meta_ogg_page_t;

GSList * meta_ogg_parse(char * filename);
int meta_ogg_render(GSList * slist, char * filename, int n_pages);
void meta_ogg_free(GSList * slist);

unsigned char * meta_ogg_get_vc_packet(GSList * slist,
				       unsigned int * length,
				       unsigned int * n_pages);

unsigned int meta_ogg_get_page_size(GSList * slist, int nth);
unsigned int meta_ogg_vc_get_total_growable(GSList * slist);

GSList * meta_ogg_vc_encapsulate_payload(GSList * slist,
					 unsigned char ** payload, unsigned int length,
					 int * n_pages_to_write);

unsigned char * meta_ogg_vc_render(metadata_t * meta, unsigned int * length);

#ifdef HAVE_VORBIS
metadata_t * metadata_from_vorbis_comment(vorbis_comment * vc);
#endif /* HAVE_VORBIS */


#endif /* AQUALUNG_METADATA_OGG_H */
