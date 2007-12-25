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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <glib.h>

#include "common.h"
#include "utils.h"
#include "i18n.h"
#include "metadata.h"
#include "metadata_api.h"
#include "metadata_id3v2.h"


char *
meta_id3v2_apic_type_to_string(int type) {

	switch (type) {
	case 0x00: return _("Other");
	case 0x01: return _("File icon (32x32 PNG)");
	case 0x02: return _("File icon (other)");
	case 0x03: return _("Front cover");
	case 0x04: return _("Back cover");
	case 0x05: return _("Leaflet page");
	case 0x06: return _("Album image");
	case 0x07: return _("Lead artist/performer");
	case 0x08: return _("Artist/performer");
	case 0x09: return _("Conductor");
	case 0x0a: return _("Band/orchestra");
	case 0x0b: return _("Composer");
	case 0x0c: return _("Lyricist/text writer");
	case 0x0d: return _("Recording location/studio");
	case 0x0e: return _("During recording");
	case 0x0f: return _("During performance");
	case 0x10: return _("Movie/video screen capture");
	case 0x11: return _("A large, coloured fish");
	case 0x12: return _("Illustration");
	case 0x13: return _("Band/artist logotype");
	case 0x14: return _("Publisher/studio logotype");
	default: return NULL;
	}
}


/* strlen() for ID3v2 strings.
 *   enc={0,3} -> terminated by one null-byte
 *   enc={1,2} -> terminated by two null-bytes
 * buf is traversed to a maximum of buf[maxlen].
 */
int
meta_id3v2_strlen(unsigned char * buf, int maxlen, int enc) {

	int i;
	for (i = 0; i < maxlen; i++) {
		if (buf[i] == '\0') {
			if ((enc == 0) || (enc == 3)) {
				return i;
			}
			if ((enc == 1) || (enc == 2)) {
				if ((i > 0) && (buf[i-1] == '\0')) {
					return i;
				}
			}
		}
	}
	return maxlen;
}


u_int32_t
meta_id3v2_read_int(unsigned char * buf) {

	return ((((u_int32_t)buf[0]) << 24) |
		(((u_int32_t)buf[1]) << 16) |
		(((u_int32_t)buf[2]) << 8) |
		  (u_int32_t)buf[3]);
}


u_int32_t
meta_id3v2_read_synchsafe_int(unsigned char * buf) {

	return (((u_int32_t)(buf[0] & 0x7f) << 21) |
		((u_int32_t)(buf[1] & 0x7f) << 14) |
		((u_int32_t)(buf[2] & 0x7f) << 7) |
		 (u_int32_t)(buf[3] & 0x7f));
}


void
meta_id3v2_write_synchsafe_int(unsigned char * buf, u_int32_t val) {

	buf[0] = ((val >> 21) & 0x7f);
	buf[1] = ((val >> 14) & 0x7f);
	buf[2] = ((val >> 7) & 0x7f);
	buf[3] = (val & 0x7f);
}


void
unsynch(unsigned char * inbuf, int inlen,
	unsigned char ** outbuf, int * outlen) {

	int i;
	int outpos = 0;
	*outlen = inlen;
	*outbuf = (unsigned char *)malloc(*outlen);
	if (*outbuf == NULL) {
		fprintf(stderr, "unsynch(): malloc error");
		return;
	}

	for (i = 0; i < inlen; i++) {
		if ((i > 0) &&
		    (inbuf[i-1] == 0xff) &&
		    (((inbuf[i] & 0xe0) == 0xe0) || (inbuf[i] == 0x0))) {
			
			*outlen += 1;
			*outbuf = (unsigned char *)realloc(*outbuf, *outlen);
			if (*outbuf == NULL) {
				fprintf(stderr, "unsynch(): realloc error");
				return;
			}
			(*outbuf)[outpos++] = 0x0;
		}
		(*outbuf)[outpos++] = inbuf[i];
	}
}


int
un_unsynch(unsigned char * buf, int len) {

	int i;
	for (i = 0; i < len-1; i++) {
		if ((buf[i] == 0xff) && (buf[i+1] == 0x00)) {
			memmove(buf+i+1, buf+i+2, len-i-2);
			--len;
		}
	}
	return len;
}


char *
meta_id3v2_to_utf8(unsigned char enc, unsigned char * buf, int len) {

	char * str = NULL;
	char * from;
	GError * error = NULL;

	if (buf[0] == '\0') {
		return strdup("");
	}

	switch (enc) {
	case 0x00: from = "iso-8859-1"; break;
	case 0x01: from = "utf-16"; break;
	case 0x02: from = "utf-16be"; break;
	case 0x03: from = "utf-8"; break;
	default:
		fprintf(stderr, "meta_id3v2_to_utf8: invalid enc = %d\n", enc);
		return NULL;
	}

	str = g_convert_with_fallback((char *)buf, len, "utf-8", from,
				      "?", NULL, NULL, &error);
	if (str != NULL) {
		return str;
	} else {
		fprintf(stderr, "meta_id3v2_to_utf8: error converting '%s': %s\n",
			buf, error->message);
		g_clear_error(&error);
		return NULL;
	}
}


char *
meta_id3v2_latin1_from_utf8(char * buf, int len) {

	char * str = NULL;
	GError * error = NULL;

	if (buf[0] == '\0') {
		return strdup("");
	}

	str = g_convert_with_fallback((char *)buf, len, "iso-8859-1", "utf-8",
				      "?", NULL, NULL, &error);
	if (str != NULL) {
		return str;
	} else {
		fprintf(stderr, "meta_id3v2_latin1_from_utf8: error converting '%s': %s\n",
			buf, error->message);
		g_clear_error(&error);
		return NULL;
	}
}


void
meta_parse_id3v2_txxx(metadata_t * meta, unsigned char * buf, int len) {

	int type;
	int len1;
	char * val1;
	char * val2;

	type = meta_frame_type_from_embedded_name(META_TAG_ID3v2, "TXXX");
	len1 = meta_id3v2_strlen(buf+11, len-1, buf[10]);
	val1 = meta_id3v2_to_utf8(buf[10], buf+11, len1);
	val2 = meta_id3v2_to_utf8(buf[10], buf+12+len1, len-len1-2);

	if ((val1 != NULL) && (val2 != NULL)) {
		meta_frame_t * frame = metadata_add_frame_from_keyval(meta,
				               META_TAG_ID3v2, val1, val2);
		frame->type = META_FIELD_TXXX;
	}
	if (val1 != NULL) {
		g_free(val1);
	}
	if (val2 != NULL) {
		g_free(val2);
	}
}


void
meta_parse_id3v2_t___(metadata_t * meta, unsigned char * buf, int len) {

	char frame_id[5];
	int type;
	char * val;

	memcpy(frame_id, buf, 4);
	frame_id[4] = '\0';

	/* support the reading of some ID3v2.3 fields by mapping them to
	   the respective ID3v2.4 frame */
	if ((strcmp(frame_id, "TDAT") == 0) ||
	    (strcmp(frame_id, "TIME") == 0) ||
	    (strcmp(frame_id, "TRDA") == 0) ||
	    (strcmp(frame_id, "TYER") == 0)) {
		strcpy(frame_id, "TDRC");
	} else if (strcmp(frame_id, "TORY") == 0) {
		strcpy(frame_id, "TDOR");
	}

	type = meta_frame_type_from_embedded_name(META_TAG_ID3v2, frame_id);
	val = meta_id3v2_to_utf8(buf[10], buf+11, len-1);
	if (val != NULL) {
		metadata_add_frame_from_keyval(meta, META_TAG_ID3v2, frame_id, val);
		g_free(val);
	}
}


void
meta_parse_id3v2_wxxx(metadata_t * meta, unsigned char * buf, int len) {

	int type;
	int len1;
	char * val1;
	char * val2;

	type = meta_frame_type_from_embedded_name(META_TAG_ID3v2, "WXXX");
	len1 = meta_id3v2_strlen(buf+11, len-1, buf[10]);
	val1 = meta_id3v2_to_utf8(buf[10], buf+11, len1);
	val2 = meta_id3v2_to_utf8(0x0 /* iso-8859-1 */, buf+12+len1, len-len1-2);

	if ((val1 != NULL) && (val2 != NULL)) {
		meta_frame_t * frame = metadata_add_frame_from_keyval(meta,
				               META_TAG_ID3v2, val1, val2);
		frame->type = META_FIELD_WXXX;
	}
	if (val1 != NULL) {
		g_free(val1);
	}
	if (val2 != NULL) {
		g_free(val2);
	}
}


void
meta_parse_id3v2_w___(metadata_t * meta, unsigned char * buf, int len) {

	char frame_id[5];
	int type;
	char * val;

	memcpy(frame_id, buf, 4);
	frame_id[4] = '\0';

	type = meta_frame_type_from_embedded_name(META_TAG_ID3v2, frame_id);
	val = meta_id3v2_to_utf8(0x0 /* iso-8859-1 */, buf+10, len);
	if (val != NULL) {
		metadata_add_frame_from_keyval(meta, META_TAG_ID3v2, frame_id, val);
		g_free(val);
	}
}


void
meta_parse_id3v2_comm(metadata_t * meta, unsigned char * buf, int len) {

	char enc = buf[10];
	char lang[4];
	int len1;
	char * descr;
	char * comment = NULL;

	memcpy(lang, buf+11, 3);
	lang[3] = '\0';
	len1 = meta_id3v2_strlen(buf+14, len-4, enc);
	if (len1 > len - 4) {
		len1 = len-4;
		fprintf(stderr, "warning: COMM description field too large, truncating\n");
	}
	descr = meta_id3v2_to_utf8(enc, buf+14, len1);

	if (len - len1 - 4 > 0) {
		comment = meta_id3v2_to_utf8(enc, buf+15+len1, len-len1-5);
	}

	if (descr != NULL) {
		g_free(descr);
	}
	if (comment != NULL) {
		metadata_add_frame_from_keyval(meta, META_TAG_ID3v2, "COMM", comment);
		g_free(comment);
	}
}


void
meta_parse_id3v2_apic(metadata_t * meta, unsigned char * buf, int len) {

	char enc = buf[10];
	char * mime_type;
	char * descr;
	char pic_type;
	int len1;
	int len2;

	len1 = meta_id3v2_strlen(buf+11, len-1, 0x0/*ascii*/);
	if (len1 > len-1) {
		len1 = len-1;
		fprintf(stderr, "warning: APIC mime-type field too large, truncating\n");
	}

	mime_type = meta_id3v2_to_utf8(0x0/*ascii*/, buf+11, len1);
	pic_type = buf[12+len1];
	len2 = meta_id3v2_strlen(buf+13+len1, len-3-len1, enc);
	descr = meta_id3v2_to_utf8(enc, buf+13+len1, len2);

	if ((mime_type != NULL) && (descr != NULL)) {
		meta_frame_t * frame = meta_frame_new();
		frame->tag = META_TAG_ID3v2;
		frame->type = META_FIELD_APIC;
		frame->field_name = strdup(mime_type);
		frame->field_val = strdup(descr);
		frame->int_val = pic_type;
		frame->length = len - (4+len1+len2);
		if (frame->length > 0) {
			frame->data = malloc(frame->length);
			if (frame->data == NULL) {
				fprintf(stderr, "meta_parse_id3v2_apic: malloc error\n");
				return;
			}
			memcpy(frame->data, buf+14+len1+len2, frame->length);
		}
		metadata_add_frame(meta, frame);
	}

	if (mime_type != NULL) {
		g_free(mime_type);
	}
	if (descr != NULL) {
		g_free(descr);
	}
}


void
meta_parse_id3v2_rva2(metadata_t * meta, unsigned char * buf, int len) {

	char * id;
	int len1;
	int pos;

	len1 = meta_id3v2_strlen(buf+10, len, 0x0/*ascii*/);
	if (len1 > len) {
		len1 = len;
		fprintf(stderr, "warning: RVA2 identification field too large, truncating\n");
	}

	id = meta_id3v2_to_utf8(0x0/*ascii*/, buf+10, len1);

	pos = 11 + len1;
	len -= len1 + 1;
	while (len >= 4) {
		unsigned int peak_bytes = (buf[pos+3] + 7) / 8;
		if (4 + peak_bytes > len)
			break;
		
		if (buf[pos] == 0x01 /* MasterVolume */) {
			signed int voladj_fixed;
			double voladj_float;
			char * field_name;
			char str[MAXLEN];
			meta_frame_t * frame;
			
			voladj_fixed = (buf[pos+1] << 8) | (buf[pos+2] << 0);
			voladj_fixed |= -(voladj_fixed & 0x8000);
			voladj_float = (double) voladj_fixed / 512.0;

			frame = meta_frame_new();
			frame->tag = META_TAG_ID3v2;
			frame->type = META_FIELD_RVA2;
			meta_get_fieldname(META_FIELD_RVA2, &field_name);
			snprintf(str, MAXLEN-1, "%s (%s)", field_name, id);
			frame->field_name = strdup(str);
			frame->field_val = strdup(id);
 			frame->float_val = voladj_float;
			metadata_add_frame(meta, frame);

			if (id != NULL) {
				g_free(id);
			}
			return;
		}
		pos += 4 + peak_bytes;
		len -= 4 + peak_bytes;
	}
	
	if (id != NULL) {
		g_free(id);
	}
}


void
meta_parse_id3v2_hidden(metadata_t * meta, unsigned char * buf, int len) {

	meta_frame_t * frame = meta_frame_new();
	frame->tag = META_TAG_ID3v2;
	frame->type = META_FIELD_HIDDEN;
	frame->length = len+10;
	frame->data = malloc(frame->length);
	if (frame->data == NULL) {
		fprintf(stderr, "meta_parse_id3v2_hidden: malloc error\n");
		return;
	}
	memcpy(frame->data, buf, frame->length);
	metadata_add_frame(meta, frame);
}


int
is_frame_char(char c) {
	return (((c >= 'A') && (c <= 'Z')) ||
		((c >= '0') && (c <= '9')));
}


int
is_frame_id(char * buf) {

	return (is_frame_char(buf[0]) &&
		is_frame_char(buf[1]) &&
		is_frame_char(buf[2]) &&
		is_frame_char(buf[3]));
}


int
meta_parse_id3v2_frame(metadata_t * meta, unsigned char * buf, int len,
		       int version, int unsynch_all) {

	char frame_id[5];
	int frame_size = 0;
	int pay_len = 0;

	/* detect padding/footer, consume rest of payload */
	if ((buf[0] == '\0') ||
	    ((buf[0] == '3') && (buf[1] == 'D') && (buf[2] == 'I'))) {
		return len;
	}

	if (len < 10) {
		return len;
	}

	memcpy(frame_id, buf, 4);
	frame_id[4] = '\0';
	
	if (!is_frame_id(frame_id)) {
		fprintf(stderr, "meta_parse_id3v2_frame: Frame ID expected, got 0x%x%x%x%x\n",
			(unsigned char)frame_id[0],
			(unsigned char)frame_id[1],
			(unsigned char)frame_id[2],
			(unsigned char)frame_id[3]);
		return len;
	}

	if (version == 0x03) {
		frame_size = pay_len = meta_id3v2_read_int(buf+4);
	} else if (version == 0x04) {
		frame_size = pay_len = meta_id3v2_read_synchsafe_int(buf+4);
		if (unsynch_all || (buf[9] & 0x02)) { /* unsynch-ed frame */
			pay_len = un_unsynch(buf+10, frame_size);
		}
	}

	if (frame_id[0] == 'T') {
		if ((frame_id[1] == 'X') &&
		    (frame_id[2] == 'X') &&
		    (frame_id[3] == 'X')) {
			meta_parse_id3v2_txxx(meta, buf, pay_len);
		} else {
			meta_parse_id3v2_t___(meta, buf, pay_len);
		}
	} else if (frame_id[0] == 'W') {
		if ((frame_id[1] == 'X') &&
		    (frame_id[2] == 'X') &&
		    (frame_id[3] == 'X')) {
			meta_parse_id3v2_wxxx(meta, buf, pay_len);
		} else {
			meta_parse_id3v2_w___(meta, buf, pay_len);
		}
	} else if (strcmp(frame_id, "COMM") == 0) {
		meta_parse_id3v2_comm(meta, buf, pay_len);
	} else if (strcmp(frame_id, "APIC") == 0) {
		meta_parse_id3v2_apic(meta, buf, pay_len);
	} else if (strcmp(frame_id, "RVA2") == 0) {
		meta_parse_id3v2_rva2(meta, buf, pay_len);
	} else {
		/* save the data in a hidden frame to preserve it for write-back */
		meta_parse_id3v2_hidden(meta, buf, pay_len);
	}

	return frame_size+10;
}


int
metadata_from_id3v2(metadata_t * meta, unsigned char * buf, int length) {

	int pos = 10;
	int payload_length = 0;

	if ((buf[3] != 0x3) && (buf[3] != 0x4)) {
		/* ID3v2 version not 2.3 or 2.4, not supported */
		return 0;
	}

	/* In ID3v2.3 first unsynch the whole tag (after header),
	   then skip extended header. In ID3v2.4 frames are
	   individually unsynch-ed. */
	if (buf[3] == 0x03) {
		if (buf[5] & 0x80) {
			payload_length = un_unsynch(buf+pos, length-pos);
		} else {
			payload_length = length - pos;
		}
		if (buf[5] & 0x40) {
			int ext_len = meta_id3v2_read_int(buf+10) + 4;
			pos += ext_len;
			payload_length -= ext_len;
		}
	} else if (buf[3] == 0x04) {
		payload_length = length - pos;
		if (buf[5] & 0x40) {
			int ext_len = meta_id3v2_read_synchsafe_int(buf+10);
			pos += ext_len;
			payload_length -= ext_len;
		}
	}

	while (length > pos) {
		pos += meta_parse_id3v2_frame(meta, buf+pos, payload_length,
					      buf[3], buf[5] & 0x80);
	}

	return 1;
}



void
meta_render_append_frame(unsigned char * data, int length,
			 unsigned char ** buf, int * size) {

	unsigned char * payload;
	int pay_len;

	unsynch(data+10, length-10, &payload, &pay_len);
	*buf = realloc(*buf, *size+10+pay_len);
	memcpy(*buf+*size, data, 10);
	(*buf)[*size+8] = 0x0;
	(*buf)[*size+9] = 0x2; /* set unsynchronization flag */
	meta_id3v2_write_synchsafe_int(*buf+*size+4, pay_len);
	memcpy(*buf+*size+10, payload, pay_len);
	*size += 10 + pay_len;
	free(payload);
}


void
meta_render_id3v2_txxx(meta_frame_t * frame, unsigned char ** buf, int * size) {

	unsigned char * data;
	int length;
	char * frame_id;
	int len1 = strlen(frame->field_name);
	int len2 = strlen(frame->field_val);

	length = 12 + len1 + len2;
	data = (unsigned char *)malloc(length);
	if (data == NULL) {
		fprintf(stderr, "meta_render_id3v2_txxx: malloc error\n");
		return;
	}
	meta_get_fieldname_embedded(META_TAG_ID3v2, frame->type, &frame_id);
	memcpy(data, frame_id, 4);
	data[10] = 0x03; /* text encoding: UTF-8 */
	memcpy(data+11, frame->field_name, len1);
	data[11 + len1] = '\0';
	memcpy(data + 12 + len1, frame->field_val, len2);

	meta_render_append_frame(data, length, buf, size);
	free(data);
}


void
meta_render_id3v2_t___(meta_frame_t * frame, unsigned char ** buf, int * size) {

	unsigned char * data;
	int length;
	char * frame_id;
	
	char fval[MAXLEN];
	char * field_val = NULL;
	int field_len = 0;
	char * renderfmt = meta_get_field_renderfmt(frame->type);
	
	if (META_FIELD_TEXT(frame->type)) {
		field_val = frame->field_val;
		field_len = strlen(field_val);
	} else if (META_FIELD_INT(frame->type)) {
		snprintf(fval, MAXLEN-1, renderfmt, frame->int_val);
		field_val = fval;
		field_len = strlen(field_val);
	} else if (META_FIELD_FLOAT(frame->type)) {
		snprintf(fval, MAXLEN-1, renderfmt, frame->float_val);
		field_val = fval;
		field_len = strlen(field_val);
	}

	length = 11+field_len;
	data = (unsigned char *)malloc(length);
	if (data == NULL) {
		fprintf(stderr, "meta_render_id3v2_t___: malloc error\n");
		return;
	}
	meta_get_fieldname_embedded(META_TAG_ID3v2, frame->type, &frame_id);
	memcpy(data, frame_id, 4);
	data[10] = 0x03; /* text encoding: UTF-8 */
	memcpy(data+11, field_val, field_len);

	meta_render_append_frame(data, length, buf, size);
	free(data);
}


void
meta_render_id3v2_wxxx(meta_frame_t * frame, unsigned char ** buf, int * size) {

	unsigned char * data;
	int length;
	char * frame_id;
	int len1 = strlen(frame->field_name);
	int len2 = strlen(frame->field_val);
	char * link = meta_id3v2_latin1_from_utf8(frame->field_val, len2);

	if (link == NULL) {
		fprintf(stderr, "meta_render_id3v2_wxxx: URL not convertible to iso-8859-1.\n");
		return;
	}
	len2 = strlen(link);

	length = 12 + len1 + len2;
	data = (unsigned char *)malloc(length);
	if (data == NULL) {
		fprintf(stderr, "meta_render_id3v2_wxxx: malloc error\n");
		return;
	}
	meta_get_fieldname_embedded(META_TAG_ID3v2, frame->type, &frame_id);
	memcpy(data, frame_id, 4);
	data[10] = 0x03; /* text encoding: UTF-8 */
	memcpy(data+11, frame->field_name, len1);
	data[11 + len1] = '\0';
	memcpy(data + 12 + len1, link, len2);

	meta_render_append_frame(data, length, buf, size);
	free(data);
}


void
meta_render_id3v2_w___(meta_frame_t * frame, unsigned char ** buf, int * size) {

	unsigned char * data;
	int length;
	char * frame_id;	
	int field_len = strlen(frame->field_val);
	char * link = meta_id3v2_latin1_from_utf8(frame->field_val, field_len);
	if (link == NULL) {
		fprintf(stderr, "meta_render_id3v2_w___: URL not convertible to iso-8859-1.\n");
		return;
	}
	field_len = strlen(link);
	
	length = 10+field_len;
	data = (unsigned char *)malloc(length);
	if (data == NULL) {
		fprintf(stderr, "meta_render_id3v2_w___: malloc error\n");
		return;
	}
	meta_get_fieldname_embedded(META_TAG_ID3v2, frame->type, &frame_id);
	memcpy(data, frame_id, 4);
	memcpy(data+10, link, field_len);

	meta_render_append_frame(data, length, buf, size);
	free(data);
}


void
meta_render_id3v2_comm(meta_frame_t * frame, unsigned char ** buf, int * size) {

	unsigned char * data;
	int length;
	char * frame_id;

	length = 15+strlen(frame->field_val);
	data = (unsigned char *)malloc(length);
	if (data == NULL) {
		fprintf(stderr, "meta_render_id3v2_comm: malloc error\n");
		return;
	}
	meta_get_fieldname_embedded(META_TAG_ID3v2, frame->type, &frame_id);
	memcpy(data, frame_id, 4);
	data[10] = 0x03; /* text encoding: UTF-8 */
	memset(data+11, 0x0, 4); /* language and short content descr. is empty */
	memcpy(data+15, frame->field_val, strlen(frame->field_val));

	meta_render_append_frame(data, length, buf, size);
	free(data);
}


void
meta_render_id3v2_apic(meta_frame_t * frame, unsigned char ** buf, int * size) {

	unsigned char * data;
	int length;
	char * frame_id;
	int len1 = strlen(frame->field_name);
	int len2 = strlen(frame->field_val);

	length = 14 + len1 + len2 + frame->length;
	data = (unsigned char *)malloc(length);
	if (data == NULL) {
		fprintf(stderr, "meta_render_id3v2_apic: malloc error\n");
		return;
	}
	meta_get_fieldname_embedded(META_TAG_ID3v2, frame->type, &frame_id);
	memcpy(data, frame_id, 4);
	data[10] = 0x03; /* text encoding: UTF-8 */
	memcpy(data+11, frame->field_name, len1);
	data[11+len1] = '\0';
	data[12+len1] = frame->int_val; /* picture type */
	memcpy(data+13+len1, frame->field_val, len2);
	data[13+len1+len2] = '\0';
	memcpy(data+14+len1+len2, frame->data, frame->length);

	meta_render_append_frame(data, length, buf, size);
	free(data);
}


void
meta_render_id3v2_rva2(meta_frame_t * frame, unsigned char ** buf, int * size) {

	unsigned char * data;
	int length;
	char * frame_id;
	int len1;
	signed int voladj_fixed;

	if (frame->field_val[0] == '\0') {
		free(frame->field_val);
		frame->field_val = strdup("Aqualung");
	}
	len1 = strlen(frame->field_val);
	length = 15 + len1;
	data = (unsigned char *)malloc(length);
	if (data == NULL) {
		fprintf(stderr, "meta_render_id3v2_apic: malloc error\n");
		return;
	}
	meta_get_fieldname_embedded(META_TAG_ID3v2, frame->type, &frame_id);
	memcpy(data, frame_id, 4);

	memcpy(data+10, frame->field_val, len1);
	data[10+len1] = '\0';
	data[11+len1] = 0x01; /* Master volume */

	voladj_fixed = frame->float_val * 512;
	data[12+len1] = (voladj_fixed & 0xff00) >> 8;
	data[13+len1] = voladj_fixed & 0xff;
	data[14+len1] = 0x00; /* no peak volume */

	meta_render_append_frame(data, length, buf, size);
	free(data);
}


void
meta_render_id3v2_hidden(meta_frame_t * frame, unsigned char ** buf, int * size) {

	unsigned char * data;
	int length;

	unsynch(frame->data+10, frame->length-10, &data, &length);
	*buf = realloc(*buf, *size+10+length);
	memcpy(*buf+*size, frame->data, 10);
	(*buf)[*size+9] |= 0x2; /* set unsynchronization flag */
	memcpy(*buf+*size+10, data, length);
	*size += 10 + length;
	free(data);
}


void
metadata_render_id3v2_frame(meta_frame_t * frame, unsigned char ** buf, int * size) {

	char * frame_id;

	if (frame->type == META_FIELD_HIDDEN) {
		meta_render_id3v2_hidden(frame, buf, size);
		return;
	}

	meta_get_fieldname_embedded(META_TAG_ID3v2, frame->type, &frame_id);

	if (frame_id[0] == 'T') {
		if ((frame_id[1] == 'X') &&
		    (frame_id[2] == 'X') &&
		    (frame_id[3] == 'X')) {
			meta_render_id3v2_txxx(frame, buf, size);
		} else {
			meta_render_id3v2_t___(frame, buf, size);
		}
	} else if (frame_id[0] == 'W') {
		if ((frame_id[1] == 'X') &&
		    (frame_id[2] == 'X') &&
		    (frame_id[3] == 'X')) {
			meta_render_id3v2_wxxx(frame, buf, size);
		} else {
			meta_render_id3v2_w___(frame, buf, size);
		}
	} else if (strcmp(frame_id, "COMM") == 0) {
		meta_render_id3v2_comm(frame, buf, size);
	} else if (strcmp(frame_id, "APIC") == 0) {
		meta_render_id3v2_apic(frame, buf, size);
	} else if (strcmp(frame_id, "RVA2") == 0) {
		meta_render_id3v2_rva2(frame, buf, size);
	} else {
		meta_render_id3v2_hidden(frame, buf, size);
	}
}


/* Render metadata to ID3v2 byte array.
 * Returns META_ERROR_*.
 * On success (META_ERROR_NONE) data and length
 * are set to a pointer of the newly allocated buffer
 * and the length of the data.
 */
int
metadata_to_id3v2(metadata_t * meta, unsigned char ** data, int * length) {

	meta_frame_t * frame;
	unsigned char * buf;
	int size = 10;

	buf = (unsigned char *)calloc(1, 10);
	if (buf == NULL) {
		return META_ERROR_NOMEM;
	}

	buf[0] = 'I'; buf[1] = 'D'; buf[2] = '3';
	buf[3] = 0x4;  /* ID3v2.4 */
	buf[5] = 0x80; /* Unsynchronize all frames */

	frame = metadata_get_frame_by_tag(meta, META_TAG_ID3v2, NULL);
	while (frame != NULL) {
		metadata_render_id3v2_frame(frame, &buf, &size);
		frame = metadata_get_frame_by_tag(meta, META_TAG_ID3v2, frame);
	}

	meta_id3v2_write_synchsafe_int(buf+6, size);

	*data = buf;
	*length = size;
	return META_ERROR_NONE;
}


int
meta_id3v2_padding_size(int size) {

	/* pad the size of the tag to be an integer multiple of 2K,
	   added padding is between 2K and 4K. */
	return 2048 * ((size / 2048) + 2);
}


void
meta_id3v2_pad(unsigned char ** buf, int * size, int padded_size) {

	*buf = (unsigned char *)realloc(*buf, padded_size);
	if (*buf == NULL) {
		fprintf(stderr, "meta_id3v2_pad: realloc error\n");
		return;
	}
	memset(*buf+*size, 0x0, padded_size - *size);
	meta_id3v2_write_synchsafe_int(*buf+6, padded_size-10);
	*size = padded_size;
}


/* move backward the whole contents of the file with length bytes,
   so file will be length bytes shorter (delete from beginning). */
int
meta_id3v2_pull_file(char * filename, FILE * file, int length) {

	u_int32_t pos;
	u_int32_t len;
	int bufsize = 1024*1024;
	unsigned char * buf;
	int eof = 0;

	buf = malloc(bufsize);
	if (buf == NULL) {
		fprintf(stderr, "meta_id3v2_pull_file: malloc error\n");
		fclose(file);
		return META_ERROR_NOMEM;
	}
	
	pos = length;
	while (!eof) {
		fseek(file, pos, SEEK_SET);
		len = fread(buf, 1, bufsize, file);
		if (len < bufsize) {
			if (feof(file)) {
				eof = 1;
			} else {
				fprintf(stderr, "meta_id3v2_pull_file: fread error\n");
				free(buf);
				fclose(file);
				return META_ERROR_INTERNAL;
			}
		}
		fseek(file, pos - length, SEEK_SET);
		if (fwrite(buf, 1, len, file) != len) {
			fprintf(stderr, "meta_id3v2_pull_file: fwrite error\n");
			free(buf);
			fclose(file);
			return META_ERROR_INTERNAL;
		}
		pos += len;
	}
	pos -= length;

	if (truncate(filename, pos) < 0) {
		fprintf(stderr, "meta_id3v2_pull_file: truncate() failed on %s\n", filename);
		free(buf);
		fclose(file);
		return META_ERROR_INTERNAL;
	}

	free(buf);
	return META_ERROR_NONE;
}


/* move forward the whole contents of the file with length bytes,
   so file will be length bytes longer (add to beginning). */
int
meta_id3v2_push_file(FILE * file, int length) {

	u_int32_t pos;
	u_int32_t len;
	int bufsize = 1024*1024;
	unsigned char * buf;

	fseek(file, 0L, SEEK_END);
	pos = ftell(file);

	buf = malloc(bufsize);
	if (buf == NULL) {
		fprintf(stderr, "meta_id3v2_push_file: malloc error\n");
		fclose(file);
		return META_ERROR_NOMEM;
	}

	while (pos > 0) {
		if (pos >= bufsize) {
			len = bufsize;
			pos -= len;
		} else {
			len = pos;
			pos = 0;
		}

		fseek(file, pos, SEEK_SET);
		if (fread(buf, 1, len, file) != len) {
			fprintf(stderr, "meta_id3v2_push_file: fread() failed\n");
			free(buf);
			fclose(file);
			return META_ERROR_INTERNAL;
		}

		fseek(file, pos+length, SEEK_SET);
		if (fwrite(buf, 1, len, file) != len) {
			fprintf(stderr, "meta_id3v2_push_file: fwrite error\n");
			free(buf);
			fclose(file);
			return META_ERROR_INTERNAL;
		}
	}

	free(buf);
	return META_ERROR_NONE;
}


int
meta_id3v2_write_tag(FILE * file, unsigned char * buf, int len) {

	/* write the tag to the beginning of the file */
	fseek(file, 0L, SEEK_SET);
	if (fwrite(buf, 1, len, file) != len) {
		fprintf(stderr, "meta_id3v2_write_tag: fwrite error\n");
		fclose(file);
		return META_ERROR_INTERNAL;
	}
	return META_ERROR_NONE;
}


int
meta_id3v2_rewrite(char * filename, unsigned char ** buf, int * len) {

	FILE * file;
	unsigned char buffer[12];
	u_int32_t file_size;
	u_int32_t id3v2_length;
	int ret;

	if ((file = fopen(filename, "r+b")) == NULL) {
		fprintf(stderr, "meta_id3v2_rewrite: fopen() failed\n");
		return META_ERROR_NOT_WRITABLE;
	}

	fseek(file, 0L, SEEK_END);
	file_size = ftell(file);
	fseek(file, 0L, SEEK_SET);

	if (file_size < 21) { /* 10 bytes ID3v2 header + 10 bytes frame header + 1 */
		/* no ID3v2 tag found, prepend tag with padding */
		int padding_size = meta_id3v2_padding_size(*len);
		meta_id3v2_pad(buf, len, padding_size);
		ret = meta_id3v2_push_file(file, padding_size);
		if (ret != META_ERROR_NONE) {
			return ret;
		}

		ret = meta_id3v2_write_tag(file, *buf, *len);
		fclose(file);
		return ret;
	} else {
		if (fread(buffer, 1, 10, file) != 10) {
			fprintf(stderr, "meta_id3v2_delete: fread() failed\n");
			fclose(file);
			return META_ERROR_INTERNAL;
		}
		
		if ((buffer[0] != 'I') || (buffer[1] != 'D') || (buffer[2] != '3')) {
			/* no ID3v2 tag found, prepend tag with padding */
			int padding_size = meta_id3v2_padding_size(*len);
			meta_id3v2_pad(buf, len, padding_size);
			ret = meta_id3v2_push_file(file, padding_size);
			if (ret != META_ERROR_NONE) {
				fclose(file);
				return ret;
			}
			
			ret = meta_id3v2_write_tag(file, *buf, *len);
			fclose(file);
			return ret;
		} else {
			id3v2_length = meta_id3v2_read_synchsafe_int(buffer+6);
			id3v2_length += 10; /* add 10 byte header */
			if (id3v2_length < *len) {
				/* rewrite whole file: remove old tag,
				   write new tag with padding */
				int padding_size = meta_id3v2_padding_size(*len);
				meta_id3v2_pad(buf, len, padding_size);
				ret = meta_id3v2_push_file(file, padding_size - id3v2_length);
				if (ret != META_ERROR_NONE) {
					fclose(file);
					return ret;
				}
				
				ret = meta_id3v2_write_tag(file, *buf, *len);
				fclose(file);
				return ret;
			} else if (*len + 32*1024 < id3v2_length) {
				/* if new tag is more than 32K shorter than the old,
				   rewrite file to shrink it. */
				int padding_size = meta_id3v2_padding_size(*len);
				meta_id3v2_pad(buf, len, padding_size);
				ret = meta_id3v2_pull_file(filename, file, id3v2_length - padding_size);
				if (ret != META_ERROR_NONE) {
					return ret;
				}
				ret = meta_id3v2_write_tag(file, *buf, *len);
				fclose(file);
				return ret;
			} else {
				/* write new tag, with remaining space as padding */
				meta_id3v2_pad(buf, len, id3v2_length);
				ret = meta_id3v2_write_tag(file, *buf, *len);
				fclose(file);
				return ret;
			}
		}
	}
}


int
meta_id3v2_delete(char * filename) {

	FILE * file;
	unsigned char buffer[12];
	u_int32_t file_size;
	u_int32_t id3v2_length;
	int ret;

	if ((file = fopen(filename, "r+b")) == NULL) {
		fprintf(stderr, "meta_id3v2_delete: fopen() failed\n");
		return META_ERROR_NOT_WRITABLE;
	}

	fseek(file, 0L, SEEK_END);
	file_size = ftell(file);
	fseek(file, 0L, SEEK_SET);

	if (file_size < 21) { /* 10 bytes ID3v2 header + 10 bytes frame header + 1 */
		fclose(file);
		return META_ERROR_NONE;
	}

	if (fread(buffer, 1, 10, file) != 10) {
		fprintf(stderr, "meta_id3v2_delete: fread() failed\n");
		fclose(file);
		return META_ERROR_INTERNAL;
	}

	if ((buffer[0] != 'I') || (buffer[1] != 'D') || (buffer[2] != '3')) {
		/* no ID3v2 tag found -- we're done */
		fclose(file);
		return META_ERROR_NONE;
	} else {
		id3v2_length = meta_id3v2_read_synchsafe_int(buffer+6);
		id3v2_length += 10; /* add 10 byte header */

		ret = meta_id3v2_pull_file(filename, file, id3v2_length);
		if (ret != META_ERROR_NONE) {
			return ret;
		}

		fclose(file);
	}

	return META_ERROR_NONE;
}
