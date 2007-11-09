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


#ifndef _METADATA_APE_H
#define _METADATA_APE_H

#include <config.h>

#include <glib.h>

#include "common.h"
#include "decoder/file_decoder.h"
#include "metadata.h"

#define APE_FLAG_READONLY      1
#define APE_FLAG_TEXT          0x00
#define APE_FLAG_BINARY        0x02
#define APE_FLAG_LOCATOR       0x04
#define APE_FLAG_IS_TEXT(x)    (((x)&0x06)==APE_FLAG_TEXT)
#define APE_FLAG_IS_BINARY(x)  (((x)&0x06)==APE_FLAG_BINARY)
#define APE_FLAG_IS_LOCATOR(x) (((x)&0x06)==APE_FLAG_LOCATOR)
#define APE_FLAG_HEADER        (1<<29)
#define APE_FLAG_HAS_NO_FOOTER (1<<30)
#define APE_FLAG_HAS_HEADER    (1<<31)

#define APE_MINIMUM_TAG_SIZE   32
#define APE_ITEM_MINIMUM_SIZE  11
#define APE_PREAMBLE "APETAGEX"

typedef struct {
	u_int32_t flags;
	unsigned char key[256];
	u_int32_t value_size;
	unsigned char * value;
} ape_item_t;

typedef struct {
	u_int32_t version;
	u_int32_t tag_size;
	u_int32_t item_count;
	u_int32_t flags;
} ape_header_t;

typedef struct {
	ape_header_t header;
	ape_header_t footer;
	GSList * items; /* items of type: ape_item_t */
} ape_tag_t;


int meta_ape_parse(char * filename, ape_tag_t * tag);
void meta_ape_free(ape_tag_t * tag);

void metadata_from_ape_tag(metadata_t * meta, ape_tag_t * tag);
void metadata_to_ape_tag(metadata_t * meta, ape_tag_t * tag);
void meta_ape_render(ape_tag_t * tag, unsigned char * data);

int meta_ape_delete(char * filename);
int meta_ape_replace_or_append(char * filename, ape_tag_t * tag);

/* for direct use by decoders */
int meta_ape_write_metadata(file_decoder_t * fdec, metadata_t * meta);
void meta_ape_send_metadata(file_decoder_t * fdec);


#endif /* _METADATA_APE_H */
