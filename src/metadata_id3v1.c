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
#include "metadata_id3v1.h"
#include "options.h"

extern options_t options;

char *
id3v1_genre_str_from_code(int code) {

	switch (code) {
	/* Standard ID3v1 */
	case 0: return "Blues";
	case 1: return "Classic Rock";
	case 2: return "Country";
	case 3: return "Dance";
	case 4: return "Disco";
	case 5: return "Funk";
	case 6: return "Grunge";
	case 7: return "Hip-Hop";
	case 8: return "Jazz";
	case 9: return "Metal";
	case 10: return "New Age";
	case 11: return "Oldies";
	case 12: return "Other";
	case 13: return "Pop";
	case 14: return "R&B";
	case 15: return "Rap";
	case 16: return "Reggae";
	case 17: return "Rock";
	case 18: return "Techno";
	case 19: return "Industrial";
	case 20: return "Alternative";
	case 21: return "Ska";
	case 22: return "Death Metal";
	case 23: return "Pranks";
	case 24: return "Soundtrack";
	case 25: return "Euro-Techno";
	case 26: return "Ambient";
	case 27: return "Trip-Hop";
	case 28: return "Vocal";
	case 29: return "Jazz+Funk";
	case 30: return "Fusion";
	case 31: return "Trance";
	case 32: return "Classical";
	case 33: return "Instrumental";
	case 34: return "Acid";
	case 35: return "House";
	case 36: return "Game";
	case 37: return "Sound Clip";
	case 38: return "Gospel";
	case 39: return "Noise";
	case 40: return "AlternRock";
	case 41: return "Bass";
	case 42: return "Soul";
	case 43: return "Punk";
	case 44: return "Space";
	case 45: return "Meditative";
	case 46: return "Instrumental Pop";
	case 47: return "Instrumental Rock";
	case 48: return "Ethnic";
	case 49: return "Gothic";
	case 50: return "Darkwave";
	case 51: return "Techno-Industrial";
	case 52: return "Electronic";
	case 53: return "Pop-Folk";
	case 54: return "Eurodance";
	case 55: return "Dream";
	case 56: return "Southern Rock";
	case 57: return "Comedy";
	case 58: return "Cult";
	case 59: return "Gangsta";
	case 60: return "Top 40";
	case 61: return "Christian Rap";
	case 62: return "Pop/Funk";
	case 63: return "Jungle";
	case 64: return "Native American";
	case 65: return "Cabaret";
	case 66: return "New Wave";
	case 67: return "Psychadelic";
	case 68: return "Rave";
	case 69: return "Showtunes";
	case 70: return "Trailer";
	case 71: return "Lo-Fi";
	case 72: return "Tribal";
	case 73: return "Acid Punk";
	case 74: return "Acid Jazz";
	case 75: return "Polka";
	case 76: return "Retro";
	case 77: return "Musical";
	case 78: return "Rock & Roll";
	case 79: return "Hard Rock";

	/* Winamp extensions */
	case 80: return "Folk";
	case 81: return "Folk-Rock";
	case 82: return "National Folk";
	case 83: return "Swing";
	case 84: return "Fast Fusion";
	case 85: return "Bebob";
	case 86: return "Latin";
	case 87: return "Revival";
	case 88: return "Celtic";
	case 89: return "Bluegrass";
	case 90: return "Avantgarde";
	case 91: return "Gothic Rock";
	case 92: return "Progressive Rock";
	case 93: return "Psychedelic Rock";
	case 94: return "Symphonic Rock";
	case 95: return "Slow Rock";
	case 96: return "Big Band";
	case 97: return "Chorus";
	case 98: return "Easy Listening";
	case 99: return "Acoustic";
	case 100: return "Humour";
	case 101: return "Speech";
	case 102: return "Chanson";
	case 103: return "Opera";
	case 104: return "Chamber Music";
	case 105: return "Sonata";
	case 106: return "Symphony";
	case 107: return "Booty Bass";
	case 108: return "Primus";
	case 109: return "Porn Groove";
	case 110: return "Satire";
	case 111: return "Slow Jam";
	case 112: return "Club";
	case 113: return "Tango";
	case 114: return "Samba";
	case 115: return "Folklore";
	case 116: return "Ballad";
	case 117: return "Power Ballad";
	case 118: return "Rhythmic Soul";
	case 119: return "Freestyle";
	case 120: return "Duet";
	case 121: return "Punk Rock";
	case 122: return "Drum Solo";
	case 123: return "A capella";
	case 124: return "Euro-House";
	case 125: return "Dance Hall";
	case 126: return "Goa";
	case 127: return "Drum & Bass";
	case 128: return "Club-House";
	case 129: return "Hardcore";
	case 130: return "Terror";
	case 131: return "Indie";
	case 132: return "BritPop";
	case 133: return "Negerpunk";
	case 134: return "Polsk Punk";
	case 135: return "Beat";
	case 136: return "Christian";
	case 137: return "Heavy Metal";
	case 138: return "Black Metal";
	case 139: return "Crossover";
	case 140: return "Contemporary";
	case 141: return "Christian Rock";
	case 142: return "Merengue";
	case 143: return "Salsa";
	case 144: return "Thrash Metal";
	case 145: return "Anime";
	case 146: return "JPop";
	case 147: return "Synthpop";

	default: return NULL;
	}
}


/* ret: -1 if not found */
int
id3v1_genre_code_from_str(char * str) {

	int i;
	for (i = 0; i < 256; i++) {
		char * lookup = id3v1_genre_str_from_code(i);
		if (lookup == NULL) {
			return -1;
		}
		if (strcmp(lookup, str) == 0) {
			return i;
		}
	}
	return -1;
}


char *
meta_id3v1_utf8_to_tagenc(char * utf8) {

	char * str = NULL;
	char * to = NULL;
	GError * error = NULL;

	if (strlen(options.encode_set))
		to = options.encode_set;
	else
		to = "iso-8859-1";

	str = g_convert_with_fallback(utf8, -1, to, "utf-8", "?", NULL, NULL, &error);
	if (str != NULL) {
		return str;
	} else {
		fprintf(stderr,
			"meta_id3v1_utf8_to_tagenc: error converting '%s': %s\n",
			utf8, error->message);
		g_clear_error(&error);
		return NULL;
	}
}


char *
meta_id3v1_utf8_from_tagenc(char * tagenc) {

	char * str = NULL;
	GError * error = NULL;

	/* try to use string as utf8, convert from iso-8859-1 if that fails. */
	if (g_utf8_validate(tagenc, -1, NULL)) {
		return g_strdup(tagenc);
	} else {
		char * from = NULL;

		g_clear_error(&error);

		if (strlen(options.encode_set))
			from = options.encode_set;
		else
			from = "iso-8859-1";

		str = g_convert_with_fallback(tagenc, -1, "utf-8", from, "?", NULL, NULL, &error);
		if (str != NULL) {
			return str;
		} else {
			fprintf(stderr,
				"meta_id3v1_utf8_from_tagenc: error converting '%s': %s\n",
				tagenc, error->message);
			g_clear_error(&error);
			return NULL;
		}
	}
}


void
meta_add_id3v1_frame(metadata_t * meta, char * key, char * val) {

	char * str = meta_id3v1_utf8_from_tagenc(val);

	if (str == NULL) {
		return;
	}

	metadata_add_frame_from_keyval(meta, META_TAG_ID3v1, key, str);
	g_free(str);
}


int
metadata_from_id3v1(metadata_t * meta, unsigned char * buf) {

	char raw[31];
	char * genre;

	if ((buf[0] != 'T') || (buf[1] != 'A') || (buf[2] != 'G')) {
		return 0;
	}

	raw[30] = '\0';
	if(buf[3] != '\0') {
		memcpy(raw, buf+3, 30);
		cut_trailing_whitespace(raw);
		meta_add_id3v1_frame(meta, "Title", raw);
	}

	if(buf[33] != '\0') {
		memcpy(raw, buf+33, 30);
		cut_trailing_whitespace(raw);
		meta_add_id3v1_frame(meta, "Artist", raw);
	}

	if(buf[63] != '\0') {
		memcpy(raw, buf+63, 30);
		cut_trailing_whitespace(raw);
		meta_add_id3v1_frame(meta, "Album", raw);
	}

	if(buf[93] != '\0') {
		memcpy(raw, buf+93, 4);
		raw[4] = '\0';
		cut_trailing_whitespace(raw);
		meta_add_id3v1_frame(meta, "Year", raw);
	}

	memcpy(raw, buf+97, 30);
	if ((raw[28] == '\0') && (raw[29] != '\0')) { /* ID3v1.1 */
		char track[4];
		snprintf(track, 4, "%u", (unsigned char)raw[29]);
		meta_add_id3v1_frame(meta, "Track", track);
	}

	genre = id3v1_genre_str_from_code(buf[127]);
	if (genre != NULL) {
		meta_add_id3v1_frame(meta, "Genre", genre);
	}

	if(buf[97] != '\0') {
		cut_trailing_whitespace(raw);
		meta_add_id3v1_frame(meta, "Comment", raw);
	}

	return 1;
}


int
metadata_to_id3v1(metadata_t * meta, unsigned char * buf) {

	meta_frame_t * frame;
	int has_trackno = 0;

	buf[0] = 'T'; buf[1] = 'A'; buf[2] = 'G';
	memset(buf+3, 0x00, 125);

	frame = metadata_get_frame_by_tag_and_type(meta, META_TAG_ID3v1,
						   META_FIELD_TITLE, NULL);
	if (frame != NULL) {
		char * str = meta_id3v1_utf8_to_tagenc(frame->field_val);
		if (str != NULL) {
			strncpy((char *)buf+3, str, 30);
			g_free(str);
		} else {
			return META_ERROR_INVALID_CODING;
		}
	}

	frame = metadata_get_frame_by_tag_and_type(meta, META_TAG_ID3v1,
						   META_FIELD_ARTIST, NULL);
	if (frame != NULL) {
		char * str = meta_id3v1_utf8_to_tagenc(frame->field_val);
		if (str != NULL) {
			strncpy((char *)buf+33, str, 30);
			g_free(str);
		} else {
			return META_ERROR_INVALID_CODING;
		}
	}

	frame = metadata_get_frame_by_tag_and_type(meta, META_TAG_ID3v1,
						   META_FIELD_ALBUM, NULL);
	if (frame != NULL) {
		char * str = meta_id3v1_utf8_to_tagenc(frame->field_val);
		if (str != NULL) {
			strncpy((char *)buf+63, str, 30);
			g_free(str);
		} else {
			return META_ERROR_INVALID_CODING;
		}
	}

	frame = metadata_get_frame_by_tag_and_type(meta, META_TAG_ID3v1,
						   META_FIELD_DATE, NULL);
	if (frame != NULL) {
		char * str = meta_id3v1_utf8_to_tagenc(frame->field_val);
		if (str != NULL) {
			strncpy((char *)buf+93, str, 4);
			g_free(str);
		} else {
			return META_ERROR_INVALID_CODING;
		}
	}

	frame = metadata_get_frame_by_tag_and_type(meta, META_TAG_ID3v1,
						   META_FIELD_TRACKNO, NULL);
	if (frame != NULL) {
		if (frame->int_val == 0) {
			fprintf(stderr, "error: Track no. 0 in ID3v1 is not possible.\n");
			return META_ERROR_INVALID_TRACKNO;
		} else if (frame->int_val > 255) {
			fprintf(stderr, "error: Track no. %d > 255 in ID3v1 is not possible (does not fit on one byte).\n", frame->int_val);

			return META_ERROR_INVALID_TRACKNO;
		} else {
			buf[126] = (unsigned char)frame->int_val;
			has_trackno = 1;
		}
	}

	frame = metadata_get_frame_by_tag_and_type(meta, META_TAG_ID3v1,
						   META_FIELD_GENRE, NULL);
	if (frame != NULL) {
		int genre = id3v1_genre_code_from_str(frame->field_val);
		if (genre != -1) {
			buf[127] = genre;
		} else {
			fprintf(stderr, "error: Genre '%s' is not supported by ID3v1.\n", frame->field_val);
			return META_ERROR_INVALID_GENRE;
		}
	}

	frame = metadata_get_frame_by_tag_and_type(meta, META_TAG_ID3v1,
						   META_FIELD_COMMENT, NULL);
	if (frame != NULL) {
		char * str = meta_id3v1_utf8_to_tagenc(frame->field_val);
		if (str != NULL) {
			strncpy((char *)buf+97, str, has_trackno ? 28 : 30);
			g_free(str);
		} else {
			return META_ERROR_INVALID_CODING;
		}
	}

	return META_ERROR_NONE;
}


int
meta_id3v1_delete(char * filename) {

	FILE * file;
	unsigned char id3v1_file[128];
	long pos;

	if ((file = fopen(filename, "r+b")) == NULL) {
		fprintf(stderr, "meta_id3v1_delete: fopen() failed\n");
		return META_ERROR_NOT_WRITABLE;
	}

	if (fseek(file, -128L, SEEK_END) != 0) {
		fprintf(stderr, "meta_id3v1_delete: fseek() failed\n");
		fclose(file);
		return META_ERROR_INTERNAL;
	}
	if (fread(id3v1_file, 1, 128, file) != 128) {
		fprintf(stderr, "meta_id3v1_delete: fread() failed\n");
		fclose(file);
		return META_ERROR_INTERNAL;
	}
	
	if ((id3v1_file[0] == 'T') && (id3v1_file[1] == 'A') && (id3v1_file[2] == 'G')) {
		/* seek back to beginning of tag */
		if (fseek(file, -128L, SEEK_END) != 0) {
			fprintf(stderr, "meta_id3v1_delete: fseek() failed\n");
			fclose(file);
			return META_ERROR_INTERNAL;
		}
	} else {
		/* seek to the end of file */
		if (fseek(file, 0L, SEEK_END) != 0) {
			fprintf(stderr, "meta_id3v1_delete: fseek() failed\n");
			fclose(file);
			return META_ERROR_INTERNAL;
		}
	}

	pos = ftell(file);
	fclose(file);

	if (truncate(filename, pos) < 0) {
		fprintf(stderr, "meta_ape_rewrite: truncate() failed on %s\n", filename);
		return META_ERROR_INTERNAL;
	}

	return META_ERROR_NONE;
}

int
meta_id3v1_rewrite(char * filename, unsigned char * id3v1) {

	FILE * file;
	unsigned char id3v1_file[128];

	if ((file = fopen(filename, "r+b")) == NULL) {
		fprintf(stderr, "meta_id3v1_rewrite: fopen() failed\n");
		return META_ERROR_NOT_WRITABLE;
	}

	if (fseek(file, -128L, SEEK_END) != 0) {
		fprintf(stderr, "meta_id3v1_rewrite: fseek() failed\n");
		fclose(file);
		return META_ERROR_INTERNAL;
	}
	if (fread(id3v1_file, 1, 128, file) != 128) {
		fprintf(stderr, "meta_id3v1_rewrite: fread() failed\n");
		fclose(file);
		return META_ERROR_INTERNAL;
	}
	
	if ((id3v1_file[0] == 'T') && (id3v1_file[1] == 'A') && (id3v1_file[2] == 'G')) {
		/* seek back to beginning of tag so it gets overwritten */
		if (fseek(file, -128L, SEEK_END) != 0) {
			fprintf(stderr, "meta_id3v1_rewrite: fseek() failed\n");
			fclose(file);
			return META_ERROR_INTERNAL;
		}		
	} else {
		/* seek to the end of file to append tag */
		if (fseek(file, 0L, SEEK_END) != 0) {
			fprintf(stderr, "meta_id3v1_rewrite: fseek() failed\n");
			fclose(file);
			return META_ERROR_INTERNAL;
		}
	}

	/* write new tag */
	if (fwrite(id3v1, 1, 128, file) != 128) {
		fprintf(stderr, "meta_id3v1_rewrite: fwrite() failed\n");
		fclose(file);
		return META_ERROR_INTERNAL;
	}

	fclose(file);
	return META_ERROR_NONE;
}
