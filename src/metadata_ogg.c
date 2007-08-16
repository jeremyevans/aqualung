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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <glib.h>

#include "metadata_ogg.h"


static const unsigned int crc_table[256] = {

	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

/* CRC for computing Ogg page checksums.
 * initial value and final XOR = 0, generator polynomial=0x04c11db7.
 */
unsigned int
meta_ogg_crc(unsigned char * data, int length) {

	unsigned int sum = 0;
	int i;
	for (i = 0; i < length; ++i) {
		sum = (sum << 8) ^ crc_table[((sum >> 24) & 0xff) ^ data[i]];
	}
	return sum;
}


meta_ogg_page_t *
meta_ogg_page_new(void) {

	meta_ogg_page_t * page = NULL;
	
	if ((page = calloc(1, sizeof(meta_ogg_page_t))) == NULL) {
		fprintf(stderr, "metadata_ogg.c: meta_ogg_page_new() failed: calloc error\n");
		return NULL;
	}

	return page;
}

void
meta_ogg_page_free(meta_ogg_page_t * page) {

	if (page->data != NULL) {
		free(page->data);
	}
	free(page);
}

void
meta_ogg_page_dump(meta_ogg_page_t * page) {

	int i;
	printf("\nOgg page\n");
	printf("  version=0x%02x  flags=0x%02x\n", page->version, page->flags);
	printf("  granulepos=0x%016llx\n", page->granulepos);
	printf("  serialno  =0x%08x\n", page->serialno);
	printf("  seqno     =0x%08x\n", page->seqno);
	printf("  checksum  =0x%08x\n", page->checksum);
	printf("  n_segments=%d\n", page->n_segments);
	for (i = 0; i < page->n_segments; i++)
		printf(" %02x", page->segment_table[i]);
	printf("\n");
}


unsigned int
meta_ogg_read_int32(unsigned char * buf) {

	return ((((unsigned int)buf[0]))       |
		(((unsigned int)buf[1]) << 8)  |
		(((unsigned int)buf[2]) << 16) |
		(((unsigned int)buf[3]) << 24));
}

unsigned long long
meta_ogg_read_int64(unsigned char * buf) {

	return ((((unsigned long long)buf[0]))       |
		(((unsigned long long)buf[1]) << 8)  |
		(((unsigned long long)buf[2]) << 16) |
		(((unsigned long long)buf[3]) << 24) |
		(((unsigned long long)buf[4]) << 32) |
		(((unsigned long long)buf[5]) << 40) |
		(((unsigned long long)buf[6]) << 48) |
		(((unsigned long long)buf[7]) << 56));
}

void
meta_ogg_write_int32(unsigned int val, unsigned char * buf) {

	buf[0] = ((val & 0xff));
	buf[1] = ((val >> 8)  & 0xff);
	buf[2] = ((val >> 16) & 0xff);
	buf[3] = ((val >> 24) & 0xff);
}

void
meta_ogg_write_int64(unsigned long long val, unsigned char * buf) {

	buf[0] = ((val & 0xff));
	buf[1] = ((val >> 8)  & 0xff);
	buf[2] = ((val >> 16) & 0xff);
	buf[3] = ((val >> 24) & 0xff);
	buf[4] = ((val >> 32) & 0xff);
	buf[5] = ((val >> 40) & 0xff);
	buf[6] = ((val >> 48) & 0xff);
	buf[7] = ((val >> 56) & 0xff);
}

unsigned char *
meta_ogg_render_page(meta_ogg_page_t * page, unsigned int * length) {

	int i;
	unsigned int total_length = 27;
	unsigned int data_length = 0;
	unsigned int crc;
	unsigned char * data;

	total_length += page->n_segments;
	for (i = 0; i < page->n_segments; i++) {
		total_length += page->segment_table[i];
		data_length += page->segment_table[i];
	}

	data = (unsigned char *)calloc(1, total_length);
	if (data == NULL) {
		fprintf(stderr, "meta_ogg_render_page: calloc error\n");
		return NULL;
	}

	/* Render Ogg header */
	data[0] = 'O'; data[1] = 'g'; data[2] = 'g'; data[3] = 'S';
	data[4] = page->version;
	data[5] = page->flags;
	meta_ogg_write_int64(page->granulepos, data + 6);
	meta_ogg_write_int32(page->serialno, data + 14);
	meta_ogg_write_int32(page->seqno, data + 18);
	meta_ogg_write_int32(0x00000000, data + 22);
	data[26] = page->n_segments;

	/* Render segment table */
	for (i = 0; i < page->n_segments; i++) {
		data[27 + i] = page->segment_table[i];
	}

	/* Render packet data */
	memcpy(data + 27 + page->n_segments, page->data, data_length);

	/* Update CRC */
	crc = meta_ogg_crc(data, total_length);
	meta_ogg_write_int32(crc, data + 22);

	if (length != NULL) {
		*length = total_length;
	}
	return data;
}


/* read the next page from an Ogg file */
meta_ogg_page_t *
meta_ogg_read_page(FILE * file) {

	unsigned char buf[27];
	meta_ogg_page_t * page = NULL;
	unsigned int data_size = 0;
	int i;

	if (fread(buf, 1, 27, file) != 27) {
		if (feof(file)) {
			return NULL;
		}
		fprintf(stderr, "meta_ogg_read_page: error reading file (Ogg header)\n");
	}

	if ((buf[0] != 'O') ||
	    (buf[1] != 'g') ||
	    (buf[2] != 'g') ||
	    (buf[3] != 'S')) {

		fprintf(stderr, "meta_ogg_read_page: page sync error\n");
		return NULL;
	}

	page = meta_ogg_page_new();
	if (page == NULL) {
		return NULL;
	}

	page->version = buf[4];
	page->flags = buf[5];
	page->granulepos = meta_ogg_read_int64(buf + 6);
	page->serialno = meta_ogg_read_int32(buf + 14);
	page->seqno = meta_ogg_read_int32(buf + 18);
	page->checksum  = meta_ogg_read_int32(buf + 22);
	page->n_segments = buf[26];

	for (i = 0; i < page->n_segments; i++) {
		unsigned char val;
		if (fread(&val, 1, 1, file) != 1) {
			if (feof(file)) {
				fprintf(stderr, "meta_ogg_read_page: premature end of file\n");
			} else {
				fprintf(stderr, "meta_ogg_read_page: error reading file (Segment table)\n");
			}
			meta_ogg_page_free(page);
			return NULL;
		}
		page->segment_table[i] = val;
		data_size += val;
	}

	page->data = (unsigned char *)calloc(1, data_size);
	if (page->data == NULL) {
		fprintf(stderr, "meta_ogg_read_page: calloc error\n");
		meta_ogg_page_free(page);
		return NULL;
	}
	if (fread(page->data, 1, data_size, file) != data_size) {
			if (feof(file)) {
				fprintf(stderr, "meta_ogg_read_page: premature end of file\n");
			} else {
				fprintf(stderr, "meta_ogg_read_page: error reading file (Packet data)\n");
			}
		meta_ogg_page_free(page);
		return NULL;
	}

	/* XXX debug only */
	//meta_ogg_page_dump(page);

	return page;
}


/* parse an Ogg stream into a list of pages */
GSList *
meta_ogg_parse(char * filename) {

	FILE * file;
	GSList * slist = NULL;
	meta_ogg_page_t * page;

	if ((file = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "meta_ogg_parse: fopen() failed\n");
		return NULL;
	}

	while (1) {
		page = meta_ogg_read_page(file);
		if (page != NULL) {
			slist = g_slist_prepend(slist, (gpointer)page);
		} else {
			break;
		}
	}
	fclose(file);
	return g_slist_reverse(slist);
}

/* render list of pages to an Ogg stream */
int
meta_ogg_render(GSList * slist, char * filename, int n_pages) {

	FILE * file;
	meta_ogg_page_t * page;
	unsigned char * data;
	unsigned int length;
	unsigned long long total_length = 0L;
	int page_count = 0;
	
	if ((file = fopen(filename, (n_pages == -1) ? "wb" : "r+b")) == NULL) {
		fprintf(stderr, "meta_ogg_render: fopen() failed\n");
		return -1;
	}

	while ((slist != NULL) && ((n_pages == -1) || (page_count < n_pages))) {
		page = (meta_ogg_page_t *)slist->data;
		data = meta_ogg_render_page(page, &length);
		if (data != NULL) {
			if (fwrite(data, 1, length, file) != length) {
				fprintf(stderr, "meta_ogg_render: fwrite() failed\n");
				return -1;
			}
			free(data);
		} else {
			fprintf(stderr, "meta_ogg_render: rendering page failed\n");
			return -1;
		}
		slist = g_slist_next(slist);
		total_length += length;
		++page_count;
	}

	/* XXX debug only */
	printf("meta_ogg_render: rendered %d pages, %llu bytes OK\n",
	       page_count, total_length);

	fclose(file);
	return 0;
}

void
meta_ogg_free(GSList * slist) {

	GSList * s = slist;
	while (slist != NULL) {
		meta_ogg_page_t * page = (meta_ogg_page_t *)slist->data;
		meta_ogg_page_free(page);
		slist = g_slist_next(slist);
	}
	g_slist_free(s);
}


/* returns packet data size without header and segment table overhead */
unsigned int
meta_ogg_get_page_size(GSList * slist, int nth) {

	meta_ogg_page_t * page = (meta_ogg_page_t *)g_slist_nth_data(slist, nth);
	int i;
	unsigned int size = 0;
	meta_ogg_page_dump(page);
	for (i = 0; i < page->n_segments; i++) {
		size += page->segment_table[i];
	}
	return size;
}


unsigned int
meta_ogg_vc_last_page_growable(meta_ogg_page_t * page) {

	int i;
	unsigned char last_lace = 255;
	unsigned int growable = 0;

	for (i = 0; page->segment_table[i] == 255; i++);
	last_lace = page->segment_table[i];
	growable += 255 * (255 - page->n_segments); /* add new segments */
	growable += (254 - last_lace); /* increase last lace of OXC to 254 */
	return growable;
}

/* Total number of bytes the Vorbis comment packet is
 * growable without the need for inserting new pages. */
unsigned int
meta_ogg_vc_get_total_growable(GSList * slist) {

	unsigned char * vc_packet;
	unsigned int vc_length;
	unsigned int n_pages;
	unsigned int total_growable = 0;
	meta_ogg_page_t * page;
	int i;

	vc_packet = meta_ogg_get_vc_packet(slist, &vc_length, &n_pages);
	free(vc_packet);
	printf("vc_packet vc_length = %d\n", vc_length);
	printf("          n_pages   = %d\n", n_pages);

	for (i = 0; i < n_pages-1; i++) {
		/* OXC-only pages to max size */
		total_growable += 255*255 - meta_ogg_get_page_size(slist, i+1);
	}

	/* current last page of OXC is special */
	page = (meta_ogg_page_t *)g_slist_nth_data(slist, n_pages);
	total_growable += meta_ogg_vc_last_page_growable(page);
	return total_growable;
}


/* reads packet starting on the first page of slist */
/* n_pages: number of pages the packet spans */
unsigned char *
meta_ogg_read_packet(GSList * slist, unsigned int * length, unsigned int * n_pages) {

	meta_ogg_page_t * page;
	int n = 1;
	int packet_length = 0;
	int fragment_length; /* length of packet fragment on this page */
	int stored_length = 0;
	unsigned char * data = NULL;
	int i;

	while (1) {
		page = (meta_ogg_page_t *)slist->data;

		fragment_length = 0;
		for (i = 0; i < page->n_segments; i++) {
			packet_length += page->segment_table[i];
			fragment_length += page->segment_table[i];
			if (page->segment_table[i] < 255) {
				break;
			}
		}

		data = realloc(data, packet_length);
		if (data == NULL) {
			fprintf(stderr, "meta_ogg_read_packet: realloc() error\n");
			return NULL;
		}
		memcpy(data + stored_length, page->data, fragment_length);
		stored_length += fragment_length;
		
		if (i == page->n_segments) {
			/* continue with next page */
			slist = g_slist_next(slist);
			++n;
		} else {
			break;
		}
	}

	if (length != NULL) {
		*length = packet_length;
	}
	if (n_pages != NULL) {
		*n_pages = n;
	}
	return data;
}

unsigned char *
meta_ogg_get_vc_packet(GSList * slist, unsigned int * length, unsigned int * n_pages) {

	/* second Ogg page always begins with Vorbis comment packet */
	return meta_ogg_read_packet(g_slist_next(slist), length, n_pages);
}


int
page_data_size(meta_ogg_page_t * page) {

	int size = 0;
	int j;
	for (j = 0; j < page->n_segments; j++) {
		size += page->segment_table[j];
		if (page->segment_table[j] < 255)
			break;
	}
	return size;
}

GSList *
meta_ogg_vc_in_place_ovwr(GSList * slist, int n_pages,
			  unsigned char * payload,
			  int * n_pages_to_write) {

	int i;
	int data_pos = 0;
	for (i = 0; i < n_pages; i++) {
		meta_ogg_page_t * page = (meta_ogg_page_t *)g_slist_nth_data(slist, i+1);
		int size = page_data_size(page);
		/* XXX */printf("meta_ogg_vc_in_place_ovwr: page_data_size = %d on page %d\n", size, i);
		memcpy(page->data, payload + data_pos, size);
		data_pos += size;
	}
	
	*n_pages_to_write = n_pages+1;
	return slist;
}

GSList *
meta_ogg_vc_expander_encaps(GSList * slist, int n_pages,
			    unsigned int new_length, unsigned int old_length,
			    unsigned char * payload, int * n_pages_to_write) {

	int i;
	int data_pos = 0;
	int expansion = new_length - old_length;

	/* XXX debug */
	printf("n_pages = %d\n", n_pages);
	printf("new_length = %d\n", new_length);
	printf("old_length = %d\n", old_length);
	printf("expansion  = %d\n", expansion);

	for (i = 0; i < n_pages; i++) {
		meta_ogg_page_t * page = (meta_ogg_page_t *)g_slist_nth_data(slist, i+1);
		int is_last_page = (i == n_pages-1) ? 1 : 0;
		int total_size = meta_ogg_get_page_size(slist, i+1);
		int size = page_data_size(page);
		int growable = is_last_page ?
			meta_ogg_vc_last_page_growable(page) :
			(255*255 - total_size);
		int new_size;
		int j;

		if (growable == 0) {
			/* this page is already full */
			continue;
		}

		if (!is_last_page) {
			growable -= growable % 255;
		}

		if (growable > expansion) {
			growable = expansion;
		}
		new_size = size + growable;
		expansion -= growable;

		/* XXX debug */
		printf("  total_size = %d\n", total_size);
		printf("  size = %d\n", size);
		printf("  growable = %d\n", growable);
		printf("  new_size = %d\n", new_size);

		/* move data */
		page->data = realloc(page->data, total_size + growable);
		memmove(page->data + new_size, page->data + size, total_size - size);
		memcpy(page->data, payload + data_pos, new_size);
		data_pos += new_size;

		/* XXX debug */
		printf("nsegments(%d) = %d\n", size, size/255+1);
		printf("nsegments(%d) = %d\n", new_size, new_size/255+1);
		printf("nsegments(%d) = %d\n", total_size-size, (total_size-size)/255+1);

		/* update segment_table accordingly */
		memmove(page->segment_table + new_size/255+1,
			page->segment_table + size/255+1,
			page->n_segments - (size/255+1));
		for (j = 0; j < new_size/255; j++)
			page->segment_table[j] = 255;
		page->segment_table[j] = new_size % 255;
		page->n_segments += (new_size/255 - size/255);

		meta_ogg_page_dump(page);
	}

	*n_pages_to_write = -1;
	return slist;
}

GSList *
meta_ogg_vc_paginator_encaps(GSList * slist, int n_pages,
			     unsigned char * payload, unsigned int length) {

	meta_ogg_page_t * page;
	meta_ogg_page_t * succ_page;
	int total_size;
	int size;
	int i;
	int data_pos = 0;
	GSList * tail;

	succ_page = (meta_ogg_page_t *)g_slist_nth_data(slist, n_pages);
	total_size = meta_ogg_get_page_size(slist, n_pages);
	size = page_data_size(succ_page);

	/* XXX debug */
	printf("size = %d\n", size);
	printf("total_size = %d\n", total_size);
	printf("total pages = %d\n", g_slist_length(slist));

	if (size == total_size) {
		slist = g_slist_remove(slist, succ_page);
		meta_ogg_page_free(succ_page);
		succ_page = (meta_ogg_page_t *)g_slist_nth_data(slist, n_pages+1);
	} else {
		succ_page->flags &= ~META_OGG_FRAGM_PACKET;
		memmove(succ_page->data, succ_page->data + size, total_size - size);
		succ_page->data = realloc(succ_page->data, total_size - size);
		/* update segment_table accordingly */
		memmove(succ_page->segment_table,
			succ_page->segment_table + size/255+1,
			succ_page->n_segments - (size/255+1));
		succ_page->n_segments -= size/255+1;

		/* XXX debug */
		printf("*** page after OXC removal ***\n");
		meta_ogg_page_dump(succ_page);
	}

	/* remove pages 1..n_pages-1 */
	for (i = n_pages-1; i >= 1; i--) {
		/* XXX */printf("paginator_encaps: removing page\n");
		page = (meta_ogg_page_t *)g_slist_nth_data(slist, i);
		slist = g_slist_remove(slist, page);
		meta_ogg_page_free(page);
	}
	/* XXX */printf("total pages = %d\n", g_slist_length(slist));

	/* insert new pages for OXC packet */
	i = 0;
	while (length > 0) {
		int j;
		int ins_len = 255*255;
		int n_segments;
		if (length < ins_len) {
			ins_len = length;
		}

		/* new page with ins_len amount of bytes starting from data_pos */
		page = meta_ogg_page_new();
		if (i > 0) {
			page->flags |= META_OGG_FRAGM_PACKET;
		}
		page->version = succ_page->version;
		page->granulepos = 0L;
		page->serialno = succ_page->serialno;
		page->seqno = i+1; /* There is 1 preceding Ogg page (stream init) */
		
		page->data = calloc(ins_len, 1);
		if (page->data == NULL) {
			fprintf(stderr, "meta_ogg_vc_paginator_encaps(): malloc error\n");
			return slist;
		}
		memcpy(page->data, payload + data_pos, ins_len);

		n_segments = ins_len/255+1;
		/* XXX debug */
		printf("ins_len = %d\n", ins_len);
		printf("n_segments = %d\n", n_segments);
		for (j = 0; j < n_segments-1; j++) {
			page->segment_table[j] = 255;
		}
		page->segment_table[j] = ins_len % 255;
		/* XXX */printf("table[%d] = %d\n", j, page->segment_table[j]);

		if (length > ins_len) {
			page->n_segments = n_segments - 1;
		} else {
			page->n_segments = n_segments;
		}

		slist = g_slist_insert(slist, page, i+1);

		/* XXX debug */
		printf("paginator_encaps: inserted page:\n");
		meta_ogg_page_dump(page);

		data_pos += ins_len;
		length -= ins_len;
		++i;
	}

	/* XXX */printf("\n");
	/* Renumber remaining Ogg pages */
	tail = g_slist_nth(slist, i);
	while (tail != NULL) {
		page = (meta_ogg_page_t *)tail->data;
		page->seqno = i++;
		tail = g_slist_next(tail);
		/* XXX */printf(".");
	}
	/* XXX */printf("\n");

	return slist;
}

GSList *
meta_ogg_vc_encapsulate_payload(GSList * slist,
				unsigned char ** payload, unsigned int length,
				int * n_pages_to_write) {

	unsigned char * vc_packet;
	unsigned int vc_length;
	unsigned int n_pages;
	unsigned int total_growable;

	vc_packet = meta_ogg_get_vc_packet(slist, &vc_length, &n_pages);
	free(vc_packet);

	if (length <= vc_length) {
		/* In-place overwrite, padding with zeroes */
		if (length < vc_length) {
			*payload = realloc(*payload, vc_length);
			memset(*payload + length, 0x00, vc_length - length);
		}
		return meta_ogg_vc_in_place_ovwr(slist, n_pages,
						 *payload, n_pages_to_write);
	}
	
	total_growable = meta_ogg_vc_get_total_growable(slist);
	/* XXX */printf("total_growable = %d\n", total_growable);
	if (length <= vc_length + total_growable) {
		/* Expand existing pages */
		return meta_ogg_vc_expander_encaps(slist, n_pages,
						   length, vc_length,
						   *payload, n_pages_to_write);
	}

	*n_pages_to_write = -1; /* re-render the whole file */
	return meta_ogg_vc_paginator_encaps(slist, n_pages, *payload, length);
}


unsigned char *
meta_ogg_vc_render(metadata_t * meta, unsigned int * length) {

	unsigned char * payload;
	unsigned int len = 16;
	unsigned int n_comments = 0;
	int n_comments_pos;
	meta_frame_t * frame;

	payload = (unsigned char *)calloc(len, 1);
	if (payload == NULL) {
		fprintf(stderr, "meta_ogg_vc_render(): calloc error\n");
		return NULL;
	}
	payload[0] = 0x03;
	payload[1] = 'v'; payload[2] = 'o'; payload[3] = 'r';
	payload[4] = 'b'; payload[5] = 'i'; payload[6] = 's';

	if ((frame = metadata_get_frame_by_tag(meta, META_TAG_OXC, NULL)) == NULL) {
		/* no Ogg Xiph comment in this metablock */
		/* we cannot really remove it, only delete all of its contents. */
		len = 16;
		payload[len-1] = 0x01; /* last framing bit */
		*length = len;
		return payload;
	} else { /* vendor string */
		frame = metadata_get_frame(meta, META_FIELD_VENDOR, NULL);
		unsigned int str_len;

		if (frame == NULL) {
			fprintf(stderr, "meta_ogg_vc_render(): programmer error: "
				"no Vendor string in metablock\n");
			free(payload);
			return NULL;
		}

		str_len = strlen(frame->field_val);
		meta_ogg_write_int32(str_len, payload + 7);

		payload = realloc(payload, len + str_len);
		memcpy(payload + 11, frame->field_val, str_len);
		len += str_len;

		payload = realloc(payload, len + 4);
		n_comments_pos = 11 + str_len;
		meta_ogg_write_int32(0, payload + n_comments_pos); /* n_comments */
		len = n_comments_pos + 4;

		payload[len] = 0x01;
	}

	/* all else */
	frame = metadata_get_frame_by_tag(meta, META_TAG_OXC, NULL);
	while (frame != NULL) {
		char * vc_entry;
		int vc_len;
		int field_len;
		char * field_val;
		char fval[MAXLEN];
		char * str;
		int i;

		if (frame->type == META_FIELD_VENDOR) {
			frame = metadata_get_frame_by_tag(meta, META_TAG_OXC, frame);
			continue;
		}

		if (!meta_get_fieldname_embedded(frame->type, &str)) {
			str = frame->field_name;
		}

		vc_entry = calloc(strlen(str) + 2, 1);
		strcpy(vc_entry, str);
		for (i = 0; vc_entry[i] != '\0'; i++) {
			vc_entry[i] = tolower(vc_entry[i]);
		}
		strcat(vc_entry, "=");
		vc_len = strlen(vc_entry);

		if (META_FIELD_TEXT(frame->type)) {
			field_val = frame->field_val;
			field_len = strlen(frame->field_val);
		} else if (META_FIELD_INT(frame->type)) {
			snprintf(fval, MAXLEN-1, "%d", frame->int_val);
			field_val = fval;
			field_len = strlen(field_val);
		} else if (META_FIELD_FLOAT(frame->type)) {
			snprintf(fval, MAXLEN-1, "%f", frame->float_val);
			field_val = fval;
			field_len = strlen(field_val);
		} else {
			fval[0] = '\0';
			field_val = fval;
			field_len = 0;
		}
		vc_entry = realloc(vc_entry, vc_len + field_len);
		memcpy(vc_entry + vc_len, field_val, field_len);
		vc_len += field_len;

		payload = realloc(payload, len + 4);
		meta_ogg_write_int32(vc_len, payload + len);
		len += 4;
		
		payload = realloc(payload, len + vc_len);
		memcpy(payload + len, vc_entry, vc_len);
		len += vc_len;

		++n_comments;
		free(vc_entry);
		frame = metadata_get_frame_by_tag(meta, META_TAG_OXC, frame);
	}

	meta_ogg_write_int32(n_comments, payload + n_comments_pos);

	if (n_comments > 0) {
		/* XXX */printf("meta_ogg_vc_render: len = %d\n", len);
		payload = realloc(payload, len+1);
		payload[len] = 0x01;
		++len;
	}
	*length = len;
	return payload;
}
