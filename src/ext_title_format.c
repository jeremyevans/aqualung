/*                                                     -*- linux-c -*-
    Copyright (C) 2008-2009 Jeremy Evans

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

#include "config.h"

#ifdef HAVE_LUA

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef LUA_HEADER_lua5_1
#include <lua5.1/lua.h>
#include <lua5.1/lauxlib.h>
#include <lua5.1/lualib.h>
#else
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#endif /* LUA_HEADER_DIR */
#include "metadata.h"
#include "decoder/file_decoder.h"
#include "options.h"

#define AQUALUNG_LUA_TITLE_FORMAT_FUNCTION "playlist_title"
#define AQUALUNG_LUA_METADATA_FUNCTION "m"
#define AQUALUNG_LUA_FILEINFO_FUNCTION "i"

#define FILEINFO_TYPE_FILENAME 0x1
#define FILEINFO_TYPE_SAMPLE_RATE 0x2
#define FILEINFO_TYPE_CHANNELS 0x3
#define FILEINFO_TYPE_BPS 0x4
#define FILEINFO_TYPE_SAMPLES 0x5
#define FILEINFO_TYPE_VOLADJ_DB 0x6
#define FILEINFO_TYPE_VOLADJ_LIN 0x7
#define FILEINFO_TYPE_STREAM 0x8
#define FILEINFO_TYPE_MONO 0x9

extern options_t options;

static lua_State * L = NULL;
static GMutex * l_mutex = NULL;
static const char l_cur_fdec = 'l';

/* Glib hash tables are abused as we store ints instead of pointers for the values */
static GHashTable * metadata_type_hash = NULL;
static GHashTable * fileinfo_type_hash = NULL;

int metadata_type_int(char * type_string) {
	return (int)(long)g_hash_table_lookup(metadata_type_hash, type_string);
}

int fileinfo_type_int(char * type_string) {
	return (int)(long)g_hash_table_lookup(fileinfo_type_hash, type_string);
}

/* Caller is responsible for freeing returned string */
char * metadata_value_all(metadata_t * meta, int type) {
	int l = 0;
	int t = 0;
	char * news;
	char * s = NULL;
	char * tmp_str;
	meta_frame_t * mframe = NULL;
	while ((mframe = metadata_pref_frame_by_type(meta, type, mframe)) != NULL) {
		if (mframe->field_val != NULL) {
			tmp_str = mframe->field_val;
			l = strlen(tmp_str);
		} else {
			if ((l = asprintf(&tmp_str, "%f", mframe->float_val)) == -1) {
				tmp_str = NULL;
				goto metadata_value_all_cleanup;
			} 
			l += 1;
		}
		if (s == NULL) {
			t = l + 1;
			if ((s = (char *)malloc(t)) == NULL) {
				goto metadata_value_all_cleanup;
			}
			s[0] = '\0';
		}
		else {
			t += l + 3;
			if ((news = (char *)realloc(s, t)) == NULL) {
				goto metadata_value_all_cleanup;
			}
			s = news;
			strncat(s, ", ", 2);
		}
		strncat(s, tmp_str, l);
		if (mframe->field_val == NULL) {
			free(tmp_str);
		}
	}
	return s;

	metadata_value_all_cleanup:
	free(s);
	if (mframe->field_val == NULL) {
		free(tmp_str);
	}
	return NULL;
}

static file_decoder_t * l_get_cur_fdec(lua_State * L) {
	file_decoder_t * fdec;
	lua_pushlightuserdata(L, (void *)&l_cur_fdec);
	lua_gettable(L, LUA_REGISTRYINDEX);
	fdec = (file_decoder_t *)lua_touserdata(L, -1);
	if (fdec == NULL) {
		luaL_error(L, "Programmer error: Invalid file decoder registered with lua");
	}
	return fdec;
}

static int l_metadata_value(lua_State * L) {
	char * s = NULL;
	int i;
	file_decoder_t * fdec;
	s = (char *)lua_tostring(L, 1);
	if((i = metadata_type_int(s)) == 0){
		luaL_error(L, "Invalid tag field: %s", s);
	}
	lua_pop(L, 1);
	fdec = l_get_cur_fdec(L);
	if ((fdec->meta != NULL) && \
	    ((s = metadata_value_all(fdec->meta, i)) != NULL)) {
		lua_pushstring(L, s);
		free(s);
	} else {
		lua_pushstring(L, "");
	}
	return 1;
}

static int l_fileinfo_value(lua_State * L) {
	char * s = NULL;
	int i;
	file_decoder_t * fdec;
	s = (char *)lua_tostring(L, 1);
	if((i = fileinfo_type_int(s)) == 0){
		luaL_error(L, "Invalid tag field: %s", s);
	}
	lua_pop(L, 1);
	fdec = l_get_cur_fdec(L);
	switch (i) {
	case FILEINFO_TYPE_FILENAME:
		lua_pushstring(L, fdec->filename);
		break;
	case FILEINFO_TYPE_SAMPLE_RATE:
		lua_pushinteger(L, fdec->fileinfo.sample_rate);
		break;
	case FILEINFO_TYPE_CHANNELS:
		lua_pushinteger(L, fdec->fileinfo.channels);
		break;
	case FILEINFO_TYPE_BPS:
		lua_pushinteger(L, fdec->fileinfo.bps);
		break;
	case FILEINFO_TYPE_SAMPLES:
		lua_pushinteger(L, fdec->fileinfo.total_samples);
		break;
	case FILEINFO_TYPE_VOLADJ_DB:
		lua_pushnumber(L, fdec->voladj_db);
		break;
	case FILEINFO_TYPE_VOLADJ_LIN:
		lua_pushnumber(L, fdec->voladj_lin);
		break;
	case FILEINFO_TYPE_STREAM:
		lua_pushboolean(L, fdec->is_stream);
		break;
	case FILEINFO_TYPE_MONO:
		lua_pushboolean(L, fdec->fileinfo.is_mono);
		break;
	default:
		luaL_error(L, "Programmer error, fileinfo type not handled: %d", i);
		break;
	}
	return 1;
}

void setup_extended_title_formatting(void) {
	int error;
	if (options.ext_title_format_file[0] == '\0') {
		options.use_ext_title_format = 0;
		return;
	}

	l_mutex = g_mutex_new();

	metadata_type_hash = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(metadata_type_hash, "title", (gpointer)META_FIELD_TITLE);	
	g_hash_table_insert(metadata_type_hash, "artist", (gpointer)META_FIELD_ARTIST);	
	g_hash_table_insert(metadata_type_hash, "album", (gpointer)META_FIELD_ALBUM);	
	g_hash_table_insert(metadata_type_hash, "date", (gpointer)META_FIELD_DATE);	
	g_hash_table_insert(metadata_type_hash, "genre", (gpointer)META_FIELD_GENRE);	
	g_hash_table_insert(metadata_type_hash, "trackno", (gpointer)META_FIELD_TRACKNO);	
	g_hash_table_insert(metadata_type_hash, "comment", (gpointer)META_FIELD_COMMENT);	
	g_hash_table_insert(metadata_type_hash, "disc", (gpointer)META_FIELD_DISC);	
	g_hash_table_insert(metadata_type_hash, "performer", (gpointer)META_FIELD_PERFORMER);	
	g_hash_table_insert(metadata_type_hash, "description", (gpointer)META_FIELD_DESCRIPTION);	
	g_hash_table_insert(metadata_type_hash, "organization", (gpointer)META_FIELD_ORGANIZATION);	
	g_hash_table_insert(metadata_type_hash, "location", (gpointer)META_FIELD_LOCATION);	
	g_hash_table_insert(metadata_type_hash, "contact", (gpointer)META_FIELD_CONTACT);	
	g_hash_table_insert(metadata_type_hash, "license", (gpointer)META_FIELD_LICENSE);	
	g_hash_table_insert(metadata_type_hash, "copyright", (gpointer)META_FIELD_COPYRIGHT);	
	g_hash_table_insert(metadata_type_hash, "isrc", (gpointer)META_FIELD_ISRC);	
	g_hash_table_insert(metadata_type_hash, "version", (gpointer)META_FIELD_VERSION);	
	g_hash_table_insert(metadata_type_hash, "subtitle", (gpointer)META_FIELD_SUBTITLE);	
	g_hash_table_insert(metadata_type_hash, "debut_album", (gpointer)META_FIELD_DEBUT_ALBUM);	
	g_hash_table_insert(metadata_type_hash, "publisher", (gpointer)META_FIELD_PUBLISHER);	
	g_hash_table_insert(metadata_type_hash, "conductor", (gpointer)META_FIELD_CONDUCTOR);	
	g_hash_table_insert(metadata_type_hash, "composer", (gpointer)META_FIELD_COMPOSER);	
	g_hash_table_insert(metadata_type_hash, "publication_right", (gpointer)META_FIELD_PRIGHT);	
	g_hash_table_insert(metadata_type_hash, "file", (gpointer)META_FIELD_FILE);	
	g_hash_table_insert(metadata_type_hash, "ean_upc", (gpointer)META_FIELD_EAN_UPC);	
	g_hash_table_insert(metadata_type_hash, "isbn", (gpointer)META_FIELD_ISBN);	
	g_hash_table_insert(metadata_type_hash, "catalog", (gpointer)META_FIELD_CATALOG);	
	g_hash_table_insert(metadata_type_hash, "label_code", (gpointer)META_FIELD_LC);	
	g_hash_table_insert(metadata_type_hash, "record_date", (gpointer)META_FIELD_RECORD_DATE);	
	g_hash_table_insert(metadata_type_hash, "record_location", (gpointer)META_FIELD_RECORD_LOC);	
	g_hash_table_insert(metadata_type_hash, "media", (gpointer)META_FIELD_MEDIA);	
	g_hash_table_insert(metadata_type_hash, "index", (gpointer)META_FIELD_INDEX);	
	g_hash_table_insert(metadata_type_hash, "related", (gpointer)META_FIELD_RELATED);	
	g_hash_table_insert(metadata_type_hash, "abstract", (gpointer)META_FIELD_ABSTRACT);	
	g_hash_table_insert(metadata_type_hash, "language", (gpointer)META_FIELD_LANGUAGE);	
	g_hash_table_insert(metadata_type_hash, "bibliography", (gpointer)META_FIELD_BIBLIOGRAPHY);	
	g_hash_table_insert(metadata_type_hash, "introplay", (gpointer)META_FIELD_INTROPLAY);	
	g_hash_table_insert(metadata_type_hash, "bpm", (gpointer)META_FIELD_TBPM);	
	g_hash_table_insert(metadata_type_hash, "encoding_time", (gpointer)META_FIELD_TDEN);	
	g_hash_table_insert(metadata_type_hash, "playlist_delay", (gpointer)META_FIELD_TDLY);	
	g_hash_table_insert(metadata_type_hash, "original_release_time", (gpointer)META_FIELD_TDOR);	
	g_hash_table_insert(metadata_type_hash, "release_time", (gpointer)META_FIELD_TDRL);	
	g_hash_table_insert(metadata_type_hash, "tagging_time", (gpointer)META_FIELD_TDTG);	
	g_hash_table_insert(metadata_type_hash, "encoded_by", (gpointer)META_FIELD_TENC);	
	g_hash_table_insert(metadata_type_hash, "lyricist", (gpointer)META_FIELD_T_E_X_T);	
	g_hash_table_insert(metadata_type_hash, "filetype", (gpointer)META_FIELD_TFLT);	
	g_hash_table_insert(metadata_type_hash, "involved_people", (gpointer)META_FIELD_TIPL);	
	g_hash_table_insert(metadata_type_hash, "content_group", (gpointer)META_FIELD_TIT1);	
	g_hash_table_insert(metadata_type_hash, "initial_key", (gpointer)META_FIELD_TKEY);	
	g_hash_table_insert(metadata_type_hash, "length", (gpointer)META_FIELD_TLEN);	
	g_hash_table_insert(metadata_type_hash, "musician_credits", (gpointer)META_FIELD_TMCL);	
	g_hash_table_insert(metadata_type_hash, "mood", (gpointer)META_FIELD_TMOO);	
	g_hash_table_insert(metadata_type_hash, "original_album", (gpointer)META_FIELD_TOAL);	
	g_hash_table_insert(metadata_type_hash, "original_filename", (gpointer)META_FIELD_TOFN);	
	g_hash_table_insert(metadata_type_hash, "original_lyricist", (gpointer)META_FIELD_TOLY);	
	g_hash_table_insert(metadata_type_hash, "original_artist", (gpointer)META_FIELD_TOPE);	
	g_hash_table_insert(metadata_type_hash, "file_owner", (gpointer)META_FIELD_TOWN);	
	g_hash_table_insert(metadata_type_hash, "band", (gpointer)META_FIELD_TPE2);	
	g_hash_table_insert(metadata_type_hash, "remixed", (gpointer)META_FIELD_TPE4);	
	g_hash_table_insert(metadata_type_hash, "set_part", (gpointer)META_FIELD_TPOS);	
	g_hash_table_insert(metadata_type_hash, "produced", (gpointer)META_FIELD_TPRO);	
	g_hash_table_insert(metadata_type_hash, "station_name", (gpointer)META_FIELD_TRSN);	
	g_hash_table_insert(metadata_type_hash, "station_owner", (gpointer)META_FIELD_TRSO);	
	g_hash_table_insert(metadata_type_hash, "album_order", (gpointer)META_FIELD_TSOA);	
	g_hash_table_insert(metadata_type_hash, "performer_order", (gpointer)META_FIELD_TSOP);	
	g_hash_table_insert(metadata_type_hash, "title_order", (gpointer)META_FIELD_TSOT);	
	g_hash_table_insert(metadata_type_hash, "software", (gpointer)META_FIELD_TSSE);	
	g_hash_table_insert(metadata_type_hash, "set_subtitle", (gpointer)META_FIELD_TSST);	
	g_hash_table_insert(metadata_type_hash, "user_text", (gpointer)META_FIELD_TXXX);	
	g_hash_table_insert(metadata_type_hash, "commercial_info", (gpointer)META_FIELD_WCOM);	
	g_hash_table_insert(metadata_type_hash, "legal_info", (gpointer)META_FIELD_WCOP);	
	g_hash_table_insert(metadata_type_hash, "file_website", (gpointer)META_FIELD_WOAF);	
	g_hash_table_insert(metadata_type_hash, "artist_website", (gpointer)META_FIELD_WOAR);	
	g_hash_table_insert(metadata_type_hash, "source_website", (gpointer)META_FIELD_WOAS);	
	g_hash_table_insert(metadata_type_hash, "station_website", (gpointer)META_FIELD_WORS);	
	g_hash_table_insert(metadata_type_hash, "payment", (gpointer)META_FIELD_WPAY);	
	g_hash_table_insert(metadata_type_hash, "publisher_website", (gpointer)META_FIELD_WPUB);	
	g_hash_table_insert(metadata_type_hash, "user_url", (gpointer)META_FIELD_WXXX);	
	g_hash_table_insert(metadata_type_hash, "vendor", (gpointer)META_FIELD_VENDOR);	
	g_hash_table_insert(metadata_type_hash, "rg_loudness", (gpointer)META_FIELD_RG_REFLOUDNESS);	
	g_hash_table_insert(metadata_type_hash, "track_gain", (gpointer)META_FIELD_RG_TRACK_GAIN);	
	g_hash_table_insert(metadata_type_hash, "track_peak", (gpointer)META_FIELD_RG_TRACK_PEAK);	
	g_hash_table_insert(metadata_type_hash, "album_gain", (gpointer)META_FIELD_RG_ALBUM_GAIN);	
	g_hash_table_insert(metadata_type_hash, "album_peak", (gpointer)META_FIELD_RG_ALBUM_PEAK);	
	g_hash_table_insert(metadata_type_hash, "icy_name", (gpointer)META_FIELD_ICY_NAME);	
	g_hash_table_insert(metadata_type_hash, "icy_descr", (gpointer)META_FIELD_ICY_DESCR);	
	g_hash_table_insert(metadata_type_hash, "icy_genre", (gpointer)META_FIELD_ICY_GENRE);	
	g_hash_table_insert(metadata_type_hash, "rva", (gpointer)META_FIELD_RVA2);	

	fileinfo_type_hash = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(fileinfo_type_hash, "filename", (gpointer)FILEINFO_TYPE_FILENAME);	
	g_hash_table_insert(fileinfo_type_hash, "sample_rate", (gpointer)FILEINFO_TYPE_SAMPLE_RATE);	
	g_hash_table_insert(fileinfo_type_hash, "channels", (gpointer)FILEINFO_TYPE_CHANNELS);	
	g_hash_table_insert(fileinfo_type_hash, "bps", (gpointer)FILEINFO_TYPE_BPS);	
	g_hash_table_insert(fileinfo_type_hash, "samples", (gpointer)FILEINFO_TYPE_SAMPLES);	
	g_hash_table_insert(fileinfo_type_hash, "voladj_db", (gpointer)FILEINFO_TYPE_VOLADJ_DB);	
	g_hash_table_insert(fileinfo_type_hash, "voladj_lin", (gpointer)FILEINFO_TYPE_VOLADJ_LIN);	
	g_hash_table_insert(fileinfo_type_hash, "stream", (gpointer)FILEINFO_TYPE_STREAM);	
	g_hash_table_insert(fileinfo_type_hash, "mono", (gpointer)FILEINFO_TYPE_MONO);	

	L = lua_open();
	luaopen_base(L);
	luaopen_table(L);
	luaopen_string(L);
	luaopen_math(L);

	lua_pushcfunction(L, l_metadata_value);
	lua_setglobal(L, AQUALUNG_LUA_METADATA_FUNCTION);
	lua_pushcfunction(L, l_fileinfo_value);
	lua_setglobal(L, AQUALUNG_LUA_FILEINFO_FUNCTION);

	error = luaL_loadfile(L, options.ext_title_format_file) || lua_pcall(L, 0, 0, 0);
	if (error) {
		fprintf(stderr, "Error in setup_extended_title_formatting: %s\n", lua_tostring(L, -1));
		options.use_ext_title_format = 0;
		lua_pop(L, 1);
	}
}

char * extended_title_format(file_decoder_t * fdec) {
	int error;
	char * s = NULL; 
	if (options.use_ext_title_format) {
		g_mutex_lock(l_mutex);
		lua_pushlightuserdata(L, (void *)&l_cur_fdec);
		lua_pushlightuserdata(L, (void *)fdec);
		lua_settable(L, LUA_REGISTRYINDEX);
		lua_getglobal(L, AQUALUNG_LUA_TITLE_FORMAT_FUNCTION);
		error = lua_pcall(L, 0, 1, 0);
		if (error) {
			fprintf(stderr, "Error in extended_title_format: %s", lua_tostring(L, -1));
		} else {
			s = strdup(lua_tostring(L, -1));
		}
		lua_pop(L, 1);
		g_mutex_unlock(l_mutex);
	}
	return s;
}

#endif /* HAVE_LUA */
