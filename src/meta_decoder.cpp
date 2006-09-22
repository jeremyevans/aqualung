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

#include <config.h>

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <gtk/gtk.h>

#ifdef HAVE_MOD
#include <libmodplug/modplug.h>
#endif /* HAVE_MOD */

#ifdef HAVE_TAGLIB
#include <tag.h>
#include <id3v1tag.h>
#include <id3v2tag.h>
#include <apetag.h>
#include <xiphcomment.h>

#include <fileref.h>
#include <mpegfile.h>
#include <mpcfile.h>
#include <flacfile.h>
#include <vorbisfile.h>
#endif /* HAVE_TAGLIB */

#include "common.h"
#include "core.h"
#include "i18n.h"
#include "options.h"
#include "decoder/file_decoder.h"
#ifdef HAVE_MOD
#include "decoder/dec_mod.h"
#endif /* HAVE_MOD */
#include "meta_decoder.h"


#define META_TITLE   1
#define META_RECORD  2
#define META_ARTIST  3
#define META_YEAR    4
#define META_COMMENT 5


extern options_t options;


int
cut_trailing_whitespace(char * str) {

	int i = strlen(str) - 1;

	while (i >= 0) {
		if ((str[i] == ' ') || (str[i] == '\t')) {
			str[i] = '\0';
		} else {
			break;
		}
		--i;
	}
	return ((i >= 0) ? 1 : 0);
}


#ifdef HAVE_MOD
void
mod_name_filter(char *str) {

        char buffer[MAXLEN];
        int n, k, f, len;

        strncpy(buffer, str, MAXLEN-1);

        k = f = n = 0;
        len = strlen(buffer);

        while (buffer[n] == ' ' || buffer[n] == '\t' ||
               buffer[n] == '_' || buffer[n] == '.') {
                n++;
        }

        for(; n < len; n++) {
                if (isprint(buffer[n])) {
                        if (buffer[n] == '_' || buffer[n] == '.') {
                                buffer[n] = ' ';
                        }
                        str[k++] = buffer[n];
                        f = 1;
                }
        }

        if (f) {
                str[k] = '\0';
        }
}
#endif /* HAVE_MOD */

/* fills in descr based on frameID, ret 1 if found, 0 else */
int
lookup_id3v2_textframe(char * frameID, char * descr) {

        static struct {
                char const * frameID;
                char const * descr;
        } const info[] = {
                { "TALB",  _("Album") },
                { "TBPM",  _("Beats per minute") },
                { "TCOM",  _("Composer") },
                { "TCON",  _("Content type") },
                { "TCOP",  _("Copyright") },
                { "TDEN",  _("Encoding time") },
                { "TDLY",  _("Playlist delay") },
                { "TDOR",  _("Original release time") },
                { "TDRC",  _("Recording time") },
                { "TDRL",  _("Release time") },
                { "TDTG",  _("Tagging time") },
                { "TENC",  _("Encoded by") },
                { "TEXT",  _("Lyricist") },
                { "TFLT",  _("File type") },
                { "TIPL",  _("Involved people") },
                { "TIT1",  _("Content group") },
                { "TIT2",  _("Title") },
                { "TIT3",  _("Subtitle") },
                { "TKEY",  _("Initial key") },
                { "TLAN",  _("Language") },
                { "TLEN",  _("Length (ms)") },
                { "TMCL",  _("Musician credits") },
                { "TMED",  _("Media type") },
                { "TMOO",  _("Mood") },
                { "TOAL",  _("Original album") },
                { "TOFN",  _("Original filename") },
                { "TOLY",  _("Original lyricist") },
                { "TOPE",  _("Original artist") },
                { "TOWN",  _("Owner/licensee") },
                { "TPE1",  _("Artist") },
                { "TPE2",  _("Orchestra") },
                { "TPE3",  _("Conductor") },
                { "TPE4",  _("Interpreted/remixed") },
                { "TPOS",  _("Part of set") },
                { "TPRO",  _("Produced") },
                { "TPUB",  _("Publisher") },
                { "TRCK",  _("Track number") },
                { "TRSN",  _("Radio station name") },
                { "TRSO",  _("Radio station owner") },
                { "TSOA",  _("Album sort order") },
                { "TSOP",  _("Artist sort order") },
                { "TSOT",  _("Title sort order") },
                { "TSRC",  _("ISRC") },
                { "TSSE",  _("Encoding settings") },
                { "TSST",  _("Set subtitle") },
		{ NULL, NULL }
        };

	for (int i = 0; info[i].frameID != NULL; i++) {
		if (strcmp(info[i].frameID, frameID) == 0) {
			strncpy(descr, info[i].descr, MAXLEN-1);
			return 1;
		}
	}
	return 0;
}


#ifdef HAVE_MOD
mod_info *
modinfo_new(void) {

	mod_info * new_tag = NULL;

	if ((new_tag = (mod_info *)malloc(sizeof(mod_info))) == NULL) {
		fprintf(stderr, "meta_decoder.c/modinfo_new(): malloc error\n");
		return NULL;
	}

	new_tag->title = NULL;
	new_tag->active = 0;

	return new_tag;
}
#endif /* HAVE_MOD */


metadata *
meta_new(void) {

	metadata * new_tag = NULL;

	if ((new_tag = (metadata *)malloc(sizeof(metadata))) == NULL) {
		fprintf(stderr, "meta_decoder.c/meta_new(): malloc error\n");
		return NULL;
	}

#ifdef HAVE_TAGLIB
	new_tag->taglib_file = NULL;
#endif /* HAVE_TAGLIB */

#ifdef HAVE_MOD
	new_tag->mod_root = modinfo_new();
#endif /* HAVE_MOD */

	return new_tag;
}


#ifdef HAVE_MOD
void
append_mod(metadata * meta, mod_info * mod) {

	if (meta == NULL) {
		fprintf(stderr, "append_mod(): assertion meta != NULL failed\n");
		return;
	}

	if (meta->mod_root == NULL) {
		fprintf(stderr, "append_mod(): assertion meta->mod_root != NULL failed\n");
		return;
	}

	meta->mod_root = mod;
}
#endif /* HAVE_MOD */


#ifdef HAVE_TAGLIB
#ifdef HAVE_FLAC
void *
meta_read_flac(char * file) {

	TagLib::FLAC::File * taglib_flac_file = new TagLib::FLAC::File(file, false);
	return reinterpret_cast<void *>(taglib_flac_file);
}
#endif /* HAVE_FLAC */


#ifdef HAVE_OGG_VORBIS
void *
meta_read_oggv(char * file) {

	TagLib::Ogg::Vorbis::File * taglib_oggv_file = new TagLib::Ogg::Vorbis::File(file, false);
	return reinterpret_cast<void *>(taglib_oggv_file);
}
#endif /* HAVE_OGG_VORBIS */


#ifdef HAVE_MPEG
void *
meta_read_mpeg(char * file) {

	TagLib::MPEG::File * taglib_mpeg_file = new TagLib::MPEG::File(file, false);
	return reinterpret_cast<void *>(taglib_mpeg_file);
}
#endif /* HAVE_MPEG */


#ifdef HAVE_MPC
void *
meta_read_mpc(char * file) {

	TagLib::MPC::File * taglib_mpc_file = new TagLib::MPC::File(file, false);
	return reinterpret_cast<void *>(taglib_mpc_file);
}
#endif /* HAVE_MPC */
#endif /* HAVE_TAGLIB */


/* ret: 1 if successful, 0 if error */
int
meta_read(metadata * meta, char * file) {

        file_decoder_t * fdec = NULL;

        if ((fdec = file_decoder_new()) == NULL) {
                fprintf(stderr, "meta_read(): error: file_decoder_new() returned NULL\n");
		file_decoder_delete(fdec);
                return 0;
        }

        if (file_decoder_open(fdec, file)) {
                fprintf(stderr, "file_decoder_open() failed on %s\n", file);
		file_decoder_delete(fdec);
                return 0;
        }

	meta->format_major = fdec->fileinfo.format_major;
	meta->format_minor = fdec->fileinfo.format_minor;
	meta->total_samples = fdec->fileinfo.total_samples;
	meta->sample_rate = fdec->fileinfo.sample_rate;
	meta->is_mono = fdec->fileinfo.is_mono;
	meta->bps = fdec->fileinfo.bps;

#ifdef HAVE_TAGLIB
	switch (meta->format_major) {
#ifdef HAVE_FLAC
	case FORMAT_FLAC:
		meta->taglib_file = meta_read_flac(file);
		break;
#endif /* HAVE_FLAC */
#ifdef HAVE_OGG_VORBIS
	case FORMAT_VORBIS:
		meta->taglib_file = meta_read_oggv(file);
		break;
#endif /* HAVE_OGG_VORBIS */
#ifdef HAVE_MPEG
	case FORMAT_MAD:
		meta->taglib_file = meta_read_mpeg(file);
		break;
#endif /* HAVE_MPEG */
#ifdef HAVE_MPC
	case FORMAT_MPC:
		meta->taglib_file = meta_read_mpc(file);
		break;
#endif /* HAVE_MPC */
	}
#endif /* HAVE_TAGLIB */


#ifdef HAVE_MOD
        if (fdec->file_lib == MOD_LIB) {

                decoder_t * dec = (decoder_t *)(fdec->pdec);
                mod_pdata_t * pd = (mod_pdata_t *)dec->pdata;
		mod_info * mi = modinfo_new();
                
		mi->title = strdup(ModPlug_GetName(pd->mpf));
		mod_name_filter(mi->title);
                mi->active = 1;
#ifdef HAVE_MOD_INFO
                mi->type = ModPlug_GetModuleType(pd->mpf);
                mi->samples = ModPlug_NumSamples(pd->mpf);
                mi->instruments = ModPlug_NumInstruments(pd->mpf);
                mi->patterns = ModPlug_NumPatterns(pd->mpf);
                mi->channels = ModPlug_NumChannels(pd->mpf);
#endif /* HAVE_MOD_INFO */
                append_mod(meta, mi);
        }
#endif /* HAVE_MOD */

        file_decoder_close(fdec);
        file_decoder_delete(fdec);

	return 1;
}


#ifdef HAVE_TAGLIB
#ifdef HAVE_FLAC
void
meta_free_flac(metadata * meta) {

	TagLib::FLAC::File * taglib_flac_file =
		reinterpret_cast<TagLib::FLAC::File *>(meta->taglib_file);
	taglib_flac_file->~File();
}
#endif /* HAVE_FLAC */


#ifdef HAVE_OGG_VORBIS
void
meta_free_oggv(metadata * meta) {

	TagLib::Ogg::Vorbis::File * taglib_oggv_file =
		reinterpret_cast<TagLib::Ogg::Vorbis::File *>(meta->taglib_file);
	taglib_oggv_file->~File();
}
#endif /* HAVE_OGG_VORBIS */


#ifdef HAVE_MPEG
void
meta_free_mpeg(metadata * meta) {

	TagLib::MPEG::File * taglib_mpeg_file =
		reinterpret_cast<TagLib::MPEG::File *>(meta->taglib_file);
	taglib_mpeg_file->~File();
}
#endif /* HAVE_MPEG */


#ifdef HAVE_MPC
void
meta_free_mpc(metadata * meta) {

	TagLib::MPC::File * taglib_mpc_file =
		reinterpret_cast<TagLib::MPC::File *>(meta->taglib_file);
	taglib_mpc_file->~File();
}
#endif /* HAVE_MPC */
#endif /* HAVE_TAGLIB */


void
meta_free(metadata * meta) {

#ifdef HAVE_MOD
	mod_info * mi;
#endif /* HAVE_MOD */


#ifdef HAVE_TAGLIB
	switch (meta->format_major) {
#ifdef HAVE_FLAC
	case FORMAT_FLAC:
		meta_free_flac(meta);
		break;
#endif /* HAVE_FLAC */
#ifdef HAVE_OGG_VORBIS
	case FORMAT_VORBIS:
		meta_free_oggv(meta);
		break;
#endif /* HAVE_OGG_VORBIS */
#ifdef HAVE_MPEG
	case FORMAT_MAD:
		meta_free_mpeg(meta);
		break;
#endif /* HAVE_MPEG */
#ifdef HAVE_MPC
	case FORMAT_MPC:
		meta_free_mpc(meta);
		break;
#endif /* HAVE_MPC */
	}
#endif /* HAVE_TAGLIB */


#ifdef HAVE_MOD
	mi = meta->mod_root;
        if(mi != NULL) {
                if(mi->title != NULL) {                   
                        free(mi->title);
                }
                free(mi);
        }
#endif /* HAVE_MOD */

	free(meta);
}


#ifdef HAVE_TAGLIB
/* fuction to work around buggy TagLib::ID3v2::RelativeVolumeFrame */
void
read_rva2(char * raw_data, unsigned int length, char * id_str, float * voladj) {

       int i;
       char id[MAXLEN];
       char str[MAXLEN];
       int id_len;
       char * data;

       id[0] = '\0';
       str[0] = '\0';

       /* parse text string */
       strncpy(id, raw_data, MAXLEN-1);
       id_len = strlen(id);

       /* skipping zero bytes */
       for (i = id_len; raw_data[i] == '\0'; i++) {

               /* TagLib puts extra zero bytes after the end of the
                  text, but do not count them into the frame size. */
               if (i != id_len) {
                       length++;
               }
       }

       length -= i;
       data = raw_data + i;

       /* now parse the binary data */
       while (length >= 4) {
               unsigned int peak_bytes;

               peak_bytes = (data[3] + 7) / 8;
               if (4 + peak_bytes > length)
                       break;

               if (data[0] == 0x01 /* MasterVolume */) {
                       signed int voladj_fixed;
                       double voladj_float;

                       voladj_fixed = (data[1] << 8) | (data[2] << 0);
                       voladj_fixed |= -(voladj_fixed & 0x8000);
                       voladj_float = (double) voladj_fixed / 512;

                       snprintf(id_str, MAXLEN-1, "%s", id);
		       *voladj = voladj_float;
		       return;
               }
               data   += 4 + peak_bytes;
               length -= 4 + peak_bytes;
       }
       id_str[0] = '\0';
       *voladj = 0.0f;
}


int
meta_get_rva_from_id3v2(TagLib::ID3v2::Tag * id3v2_tag, float * fval) {

	if (id3v2_tag == NULL) return 0;
	if (id3v2_tag->isEmpty()) return 0;

	TagLib::ID3v2::FrameList l = id3v2_tag->frameList();
	std::list<TagLib::ID3v2::Frame*>::iterator i;

	for (i = l.begin(); i != l.end(); ++i) {
		TagLib::ID3v2::Frame * frame = *i;
		char frameID[5];
		
		for(int j = 0; j < 4; j++) {
			frameID[j] = frame->frameID().data()[j];
		}
		frameID[4] = '\0';

		if (strcmp(frameID, "RVA2") == 0) {
			char id_str[MAXLEN];
			read_rva2(frame->render().data() + 10, frame->size(), id_str, fval);
		}
	}
	return 0;
}


int
meta_get_rva_from_oxc(TagLib::Ogg::XiphComment * oxc, float * fval) {

	char replaygain_label[MAXLEN];

	if (oxc == NULL) return 0;
	if (oxc->isEmpty()) return 0;

	switch (options.replaygain_tag_to_use) {
	case 0:
		strcpy(replaygain_label, "Replaygain_track_gain:");
		break;
	case 1:
		strcpy(replaygain_label, "Replaygain_album_gain:");
		break;
	}

	TagLib::Ogg::FieldListMap m = oxc->fieldListMap();
	for (TagLib::Ogg::FieldListMap::Iterator i = m.begin(); i != m.end(); ++i) {
		for (TagLib::StringList::Iterator j = (*i).second.begin(); j != (*i).second.end(); ++j) {
			char key[MAXLEN];
			char val[MAXLEN];
			char c;
			int k;
			
			for (k = 0; ((c = (*i).first.toCString(true)[k]) != '\0') && (k < MAXLEN-1); k++) {
				key[k] = (k == 0) ? toupper(c) : tolower(c);
			}
			key[k++] = ':';
			key[k] = '\0';
			
			for (k = 0; ((c = (*j).toCString(true)[k]) != '\0') && (k < MAXLEN-1); k++) {
				val[k] = c;
			}
			val[k] = '\0';
			
			if (strcmp(key, replaygain_label) == 0) {
				*fval = convf(val);
				return 1;
			}
		}
	}
	return 0;
}
#endif /* HAVE_TAGLIB */


/* ret: 1 if found, 0 if no RVA data */
/* can be called with fval == NULL only to see if RVA is available */
int
meta_get_rva(metadata * meta, float * fval) {

	int ret = 0;

	if (meta == NULL) {
		fprintf(stderr, "meta_get_rva(): assertion meta != NULL failed\n");
		return 0;
	}

#ifdef HAVE_TAGLIB
	TagLib::FLAC::File * flac_file;
	TagLib::Ogg::Vorbis::File * oggv_file;
	TagLib::MPEG::File * mpeg_file;

	switch (meta->format_major) {
#ifdef HAVE_FLAC
	case FORMAT_FLAC:
		flac_file = reinterpret_cast<TagLib::FLAC::File *>(meta->taglib_file);
		ret = meta_get_rva_from_id3v2(flac_file->ID3v2Tag(), fval);
		if (!ret) {
			ret = meta_get_rva_from_oxc(flac_file->xiphComment(), fval);
		}
		break;
#endif /* HAVE_FLAC */
#ifdef HAVE_OGG_VORBIS
	case FORMAT_VORBIS:
		oggv_file = reinterpret_cast<TagLib::Ogg::Vorbis::File *>(meta->taglib_file);
		ret = meta_get_rva_from_oxc(oggv_file->tag(), fval);
		break;
#endif /* HAVE_OGG_VORBIS */
#ifdef HAVE_MPEG
	case FORMAT_MAD:
		mpeg_file = reinterpret_cast<TagLib::MPEG::File *>(meta->taglib_file);
		ret = meta_get_rva_from_id3v2(mpeg_file->ID3v2Tag(), fval);
		break;
#endif /* HAVE_MPEG */
	}
#endif /* HAVE_TAGLIB */

	return ret;
}


#ifdef HAVE_TAGLIB
int
meta_lookup(char * str, int which, TagLib::Tag * tag) {

	switch (which) {
	case META_TITLE:
		if (tag->title().toCString(true)[0] != '\0') {
			strncpy(str, tag->title().toCString(true), MAXLEN-1);
			return cut_trailing_whitespace(str);
		}
		break;
	case META_RECORD:
		if (tag->album().toCString(true)[0] != '\0') {
			strncpy(str, tag->album().toCString(true), MAXLEN-1);
			return cut_trailing_whitespace(str);
		}
		break;
	case META_ARTIST:
		if (tag->artist().toCString(true)[0] != '\0') {
			strncpy(str, tag->artist().toCString(true), MAXLEN-1);
			return cut_trailing_whitespace(str);
		}
		break;
	case META_YEAR:
		if (tag->year() != 0) {
			snprintf(str, MAXLEN-1, "%d", tag->year());
			return cut_trailing_whitespace(str);
		}
		break;
	case META_COMMENT:
		if (tag->comment().toCString(true)[0] != '\0') {
			strncpy(str, tag->comment().toCString(true), MAXLEN-1);
			return cut_trailing_whitespace(str);
		}
		break;
	}
	return 0;
}
#endif /* HAVE_TAGLIB */


#ifdef HAVE_MOD
int
meta_get_title_mod(metadata * meta, char * str) {

	mod_info * mi;

	mi = meta->mod_root;
        if (mi != NULL) {
		if (str != NULL && mi->title != NULL) {
			strncpy(str, (char *) mi->title, MAXLEN-1);
		}
		return cut_trailing_whitespace(str);
        }
	return 0;
}
#endif /* HAVE_MOD */


int
meta_get_title(metadata * meta, char * str) {

	int ret = 0;

	if (meta == NULL) {
		fprintf(stderr, "meta_get_title(): assertion meta != NULL failed\n");
		return 0;
	}

#ifdef HAVE_TAGLIB
	if (meta->taglib_file != NULL) {
		TagLib::File * file = reinterpret_cast<TagLib::File *>(meta->taglib_file);
		ret = meta_lookup(str, META_TITLE, file->tag());
	}
	if (ret)
		return ret;
#endif /* HAVE_TAGLIB */

#ifdef HAVE_MOD
	if (meta->format_major == FORMAT_MOD) {
		ret = meta_get_title_mod(meta, str);
	}
#endif /* HAVE_MOD */
	return ret;
}


int
meta_get_record(metadata * meta, char * str) {

	int ret = 0;

	if (meta == NULL) {
		fprintf(stderr, "meta_get_title(): assertion meta != NULL failed\n");
		return 0;
	}

#ifdef HAVE_TAGLIB
	if (meta->taglib_file != NULL) {
		TagLib::File * file = reinterpret_cast<TagLib::File *>(meta->taglib_file);
		ret = meta_lookup(str, META_RECORD, file->tag());
	}
#endif /* HAVE_TAGLIB */
	return ret;
}


int
meta_get_artist(metadata * meta, char * str) {

	int ret = 0;

	if (meta == NULL) {
		fprintf(stderr, "meta_get_title(): assertion meta != NULL failed\n");
		return 0;
	}

#ifdef HAVE_TAGLIB
	if (meta->taglib_file != NULL) {
		TagLib::File * file = reinterpret_cast<TagLib::File *>(meta->taglib_file);
		ret = meta_lookup(str, META_ARTIST, file->tag());
	}
#endif /* HAVE_TAGLIB */
	return ret;
}


int
meta_get_year(metadata * meta, char * str) {

	int ret = 0;

	if (meta == NULL) {
		fprintf(stderr, "meta_get_title(): assertion meta != NULL failed\n");
		return 0;
	}

#ifdef HAVE_TAGLIB
	if (meta->taglib_file != NULL) {
		TagLib::File * file = reinterpret_cast<TagLib::File *>(meta->taglib_file);
		ret = meta_lookup(str, META_YEAR, file->tag());
	}
#endif /* HAVE_TAGLIB */
	return ret;
}


int
meta_get_comment(metadata * meta, char * str) {

	int ret = 0;

	if (meta == NULL) {
		fprintf(stderr, "meta_get_title(): assertion meta != NULL failed\n");
		return 0;
	}

#ifdef HAVE_TAGLIB
	if (meta->taglib_file != NULL) {
		TagLib::File * file = reinterpret_cast<TagLib::File *>(meta->taglib_file);
		ret = meta_lookup(str, META_COMMENT, file->tag());
	}
#endif /* HAVE_TAGLIB */
	return ret;
}



#ifdef HAVE_TAGLIB

/* Update basic metadata fields of a file. Used for mass-tagging.
 * Note that this method is lightweight (does not need an underlying file_decoder),
 * and does not need explicit filetype information either.
 *
 * Any input string may be NULL, in which case that field won't be updated.
 * Existing metadata not updated will be retained.
 *
 * filename should be locale encoded, fields should be UTF8.
 *
 * Return 1 if OK, < 0 else.
 */
int
meta_update_basic(char * filename, char * title, char * artist, char * album,
		  char * comment, char * genre, char * year, char * track) {

	TagLib::FileRef f(filename);
	char buf[MAXLEN];
	int save = 0;
	
	if (f.isNull() || !f.tag()) {
		return -1;
	}

	TagLib::Tag * t = f.tag();
	TagLib::String str;

	if (title) {
		strncpy(buf, title, MAXLEN-1);
		cut_trailing_whitespace(buf);
		str = TagLib::String(buf, TagLib::String::UTF8);
		t->setTitle(str);
		save = 1;
	}
	if (artist) {
		strncpy(buf, artist, MAXLEN-1);
		cut_trailing_whitespace(buf);
		str = TagLib::String(buf, TagLib::String::UTF8);
		t->setArtist(str);
		save = 1;
	}
	if (album) {
		strncpy(buf, album, MAXLEN-1);
		cut_trailing_whitespace(buf);
		str = TagLib::String(buf, TagLib::String::UTF8);
		t->setAlbum(str);
		save = 1;
	}
	if (comment) {
		strncpy(buf, comment, MAXLEN-1);
		cut_trailing_whitespace(buf);
		str = TagLib::String(buf, TagLib::String::UTF8);
		t->setComment(str);
		save = 1;
	}
	if (genre) {
		strncpy(buf, genre, MAXLEN-1);
		cut_trailing_whitespace(buf);
		str = TagLib::String(buf, TagLib::String::UTF8);
		t->setGenre(str);
		save = 1;
	}
	if (year) {
		int i;
		strncpy(buf, year, MAXLEN-1);
		cut_trailing_whitespace(buf);
		if (sscanf(buf, "%d", &i) < 1) {
			i = 0;
		}
		if ((i < 0) || (i > 9999)) {
			i = 0;
		}
		t->setYear(i);
		save = 1;
	}
	if (track) {
		int i;
		strncpy(buf, track, MAXLEN-1);
		cut_trailing_whitespace(buf);
		if (sscanf(buf, "%d", &i) < 1) {
			i = 0;
		}
		if ((i < 0) || (i > 9999)) {
			i = 0;
		}
		t->setTrack(i);
		save = 1;
	}

	if (save) {
		f.file()->save();
	}

	return 1;
}

#endif /* HAVE_TAGLIB */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

