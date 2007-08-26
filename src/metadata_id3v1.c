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
#include "metadata_id3v1.h"


char *
lookup_id3v1_genre(int code) {

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


void
meta_add_id3v1_frame(metadata_t * meta, char * key, char * val) {

	char * str = NULL;
	GError * error = NULL;

	/* try to use string as utf8, convert from iso-8859-1 if that fails. */
	str = g_convert(val, -1, "utf8", "utf8", NULL, NULL, &error);
	if (str != NULL) {
		metadata_add_frame_from_keyval(meta, META_TAG_ID3v1, key, str);
		g_free(str);
		str = NULL;
	} else {
		g_clear_error(&error);

		str = g_convert(val, -1, "utf8", "iso-8859-1", NULL, NULL, &error);
		if (str != NULL) {
			metadata_add_frame_from_keyval(meta, META_TAG_ID3v1, key, str);
			g_free(str);
			str = NULL;
		} else {
			fprintf(stderr,
				"metadata_id3v1.c: error converting '%s' to utf8: %s\n",
				val, error->message);
			g_clear_error(&error);
		}
	}
}


int
metadata_from_id3v1(metadata_t * meta, char * buf) {

	char raw[31];
	char * genre;

	if ((buf[0] != 'T') || (buf[1] != 'A') || (buf[2] != 'G')) {
		return 0;
	}

	memcpy(raw, buf+3, 30);
	raw[30] = '\0';
	cut_trailing_whitespace(raw);
	meta_add_id3v1_frame(meta, "Title", raw);

	memcpy(raw, buf+33, 30);
	raw[30] = '\0';
	cut_trailing_whitespace(raw);
	meta_add_id3v1_frame(meta, "Artist", raw);

	memcpy(raw, buf+63, 30);
	raw[30] = '\0';
	cut_trailing_whitespace(raw);
	meta_add_id3v1_frame(meta, "Album", raw);

	memcpy(raw, buf+93, 4);
	raw[4] = '\0';
	cut_trailing_whitespace(raw);
	meta_add_id3v1_frame(meta, "Year", raw);

	memcpy(raw, buf+97, 30);
	raw[30] = '\0';
	if ((raw[28] == '\0') && (raw[29] != '\0')) { /* ID3v1.1 */
		char track[16];
		snprintf(track, 15, "%u", (unsigned char)raw[29]);
		meta_add_id3v1_frame(meta, "Track", track);
	}

	genre = lookup_id3v1_genre(buf[127]);
	if (genre != NULL) {
		meta_add_id3v1_frame(meta, "Genre", genre);
	}

	cut_trailing_whitespace(raw);
	meta_add_id3v1_frame(meta, "Comment", raw);

	return 1;
}


int
metadata_to_id3v1(metadata_t * meta, char * buf) {

	/* TODO */
	return 0;
}
