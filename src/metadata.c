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

#include "common.h"
#include "i18n.h"
#include "utils.h"
#include "options.h"
#include "metadata.h"
#include "metadata_api.h"


extern options_t options;

/* the data model */

struct {
	int type;
	char * name; /* name displayed to the user */
	char * parse_fmt; /* format string for conversion from int/float types */
	char * render_fmt; /* format string for conversion to int/float types */
	int tags; /* tags this frame can be present in */
	struct {
		int tag;
		int flags;
		char * name; /* name as it is written in tag */
	} const emb_names[META_N_TAGS];
} const meta_model[] = {

	{META_FIELD_TITLE, X_("Title"), "", "",
	     META_TAG_APE | META_TAG_OXC | META_TAG_ID3v1 | META_TAG_ID3v2 | META_TAG_MPEGSTREAM,
	     {{META_TAG_APE, 0, "Title"},
	      {META_TAG_OXC, 0, "title"},
	      {META_TAG_ID3v1, META_FIELD_UNIQUE | META_FIELD_MANDATORY, "Title"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TIT2"},
	      {META_TAG_MPEGSTREAM, 0, "Title"}}},
	{META_FIELD_ARTIST, X_("Artist"), "", "",
	     META_TAG_APE | META_TAG_OXC | META_TAG_ID3v1 | META_TAG_ID3v2 | META_TAG_MPEGSTREAM,
	     {{META_TAG_APE, 0, "Artist"},
	      {META_TAG_OXC, 0, "artist"},
	      {META_TAG_ID3v1, META_FIELD_UNIQUE | META_FIELD_MANDATORY, "Artist"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TPE1"},
	      {META_TAG_MPEGSTREAM, 0, "Artist"}}},
	{META_FIELD_ALBUM, X_("Album"), "", "",
	     META_TAG_APE | META_TAG_OXC | META_TAG_ID3v1 | META_TAG_ID3v2 | META_TAG_MPEGSTREAM,
	     {{META_TAG_APE, 0, "Album"},
	      {META_TAG_OXC, 0, "album"},
	      {META_TAG_ID3v1, META_FIELD_UNIQUE | META_FIELD_MANDATORY, "Album"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TALB"},
	      {META_TAG_MPEGSTREAM, 0, "Album"}}},
	{META_FIELD_DATE, X_("Date"), "", "",
	     META_TAG_APE | META_TAG_OXC | META_TAG_ID3v1 | META_TAG_ID3v2,
	     {{META_TAG_APE, META_FIELD_UNIQUE, "Year"},
	      {META_TAG_OXC, META_FIELD_UNIQUE, "date"},
	      {META_TAG_ID3v1, META_FIELD_UNIQUE | META_FIELD_MANDATORY, "Year"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TDRC"}}},
	{META_FIELD_GENRE, X_("Genre"), "", "",
	     META_TAG_APE | META_TAG_OXC | META_TAG_ID3v1 | META_TAG_ID3v2,
	     {{META_TAG_APE, META_FIELD_UNIQUE, "Genre"},
	      {META_TAG_OXC, META_FIELD_UNIQUE, "genre"},
	      {META_TAG_ID3v1, META_FIELD_UNIQUE | META_FIELD_MANDATORY, "Genre"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TCON"}}},
	{META_FIELD_TRACKNO, X_("Track No."), "%d", "%02d",
	     META_TAG_APE | META_TAG_OXC | META_TAG_ID3v1 | META_TAG_ID3v2,
	     {{META_TAG_APE, META_FIELD_UNIQUE, "Track"},
	      {META_TAG_OXC, META_FIELD_UNIQUE, "tracknumber"},
	      {META_TAG_ID3v1, META_FIELD_UNIQUE, "Track"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TRCK"}}},
	{META_FIELD_COMMENT, X_("Comment"), "", "",
	     META_TAG_APE | META_TAG_OXC | META_TAG_ID3v1 | META_TAG_ID3v2,
	     {{META_TAG_APE, 0, "Comment"},
	      {META_TAG_OXC, 0, "comment"},
	      {META_TAG_ID3v1, META_FIELD_UNIQUE | META_FIELD_MANDATORY, "Comment"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "COMM"}}},

	{META_FIELD_DISC, X_("Disc"), "%d", "%d",
	     META_TAG_APE | META_TAG_OXC,
	     {{META_TAG_APE, 0, "Disc"},
	      {META_TAG_OXC, 0, "disc"}}},
	{META_FIELD_PERFORMER, X_("Performer"), "", "",
	     META_TAG_OXC,
	     {{META_TAG_OXC, 0, "performer"}}},
	{META_FIELD_DESCRIPTION, X_("Description"), "", "",
	     META_TAG_OXC,
	     {{META_TAG_OXC, 0, "description"}}},
	{META_FIELD_ORGANIZATION, X_("Organization"), "", "",
	     META_TAG_OXC,
	     {{META_TAG_OXC, 0, "organization"}}},
	{META_FIELD_LOCATION, X_("Location"), "", "",
	     META_TAG_OXC,
	     {{META_TAG_OXC, 0, "location"}}},
	{META_FIELD_CONTACT, X_("Contact"), "", "",
	     META_TAG_OXC,
	     {{META_TAG_OXC, 0, "contact"}}},
	{META_FIELD_LICENSE, X_("License"), "", "",
	     META_TAG_OXC,
	     {{META_TAG_OXC, 0, "license"}}},
	{META_FIELD_COPYRIGHT, X_("Copyright"), "", "",
	     META_TAG_APE | META_TAG_OXC | META_TAG_ID3v2,
	     {{META_TAG_APE, 0, "Copyright"},
	      {META_TAG_OXC, 0, "copyright"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TCOP"}}},
	{META_FIELD_ISRC, X_("ISRC"), "", "",
	     META_TAG_APE | META_TAG_OXC | META_TAG_ID3v2,
	     {{META_TAG_APE, 0, "ISRC"},
	      {META_TAG_OXC, 0, "isrc"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TSRC"}}},
	{META_FIELD_VERSION, X_("Version"), "", "",
	     META_TAG_OXC,
	     {{META_TAG_OXC, 0, "Version"}}},

	{META_FIELD_SUBTITLE, X_("Subtitle"), "", "",
	     META_TAG_APE | META_TAG_ID3v2,
	     {{META_TAG_APE, 0, "Subtitle"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TIT3"}}},
	{META_FIELD_DEBUT_ALBUM, X_("Debut Album"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "Debut album"}}},
	{META_FIELD_PUBLISHER, X_("Publisher"), "", "",
	     META_TAG_APE | META_TAG_ID3v2,
	     {{META_TAG_APE, 0, "Publisher"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TPUB"}}},
	{META_FIELD_CONDUCTOR, X_("Conductor"), "", "",
	     META_TAG_APE | META_TAG_ID3v2,
	     {{META_TAG_APE, 0, "Conductor"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TCON"}}},
	{META_FIELD_COMPOSER, X_("Composer"), "", "",
	     META_TAG_APE | META_TAG_ID3v2,
	     {{META_TAG_APE, 0, "Composer"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TCOM"}}},
	{META_FIELD_PRIGHT, X_("Publication Right"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "Publicationright"}}},
	{META_FIELD_FILE, X_("File"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "File"}}},
	{META_FIELD_EAN_UPC, X_("EAN/UPC"), "%d", "%d",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "EAN/UPC"}}},
	{META_FIELD_ISBN, X_("ISBN"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "ISBN"}}},
	{META_FIELD_CATALOG, X_("Catalog Number"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "Catalog"}}},
	{META_FIELD_LC, X_("Label Code"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "LC"}}},
	{META_FIELD_RECORD_DATE, X_("Record Date"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "Record Date"}}},
	{META_FIELD_RECORD_LOC, X_("Record Location"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "Record Location"}}},
	{META_FIELD_MEDIA, X_("Media"), "", "",
	     META_TAG_APE | META_TAG_ID3v2,
	     {{META_TAG_APE, 0, "Media"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TMED"}}},
	{META_FIELD_INDEX, X_("Index"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "Index"}}},
	{META_FIELD_RELATED, X_("Related"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "Related"}}},
	{META_FIELD_ABSTRACT, X_("Abstract"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "Abstract"}}},
	{META_FIELD_LANGUAGE, X_("Language"), "", "",
	     META_TAG_APE | META_TAG_ID3v2,
	     {{META_TAG_APE, 0, "Language"},
	      {META_TAG_ID3v2, META_FIELD_UNIQUE, "TLAN"}}},
	{META_FIELD_BIBLIOGRAPHY, X_("Bibliography"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "Bibliography"}}},
	{META_FIELD_INTROPLAY, X_("Introplay"), "", "",
	     META_TAG_APE,
	     {{META_TAG_APE, 0, "Introplay"}}},


	{META_FIELD_TBPM, X_("BPM"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TBPM"}}},
	{META_FIELD_TDEN, X_("Encoding Time"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TDEN"}}},
	{META_FIELD_TDLY, X_("Playlist Delay"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TDLY"}}},
	{META_FIELD_TDOR, X_("Original Release Time"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TDOR"}}},
	{META_FIELD_TDRL, X_("Release Time"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TDRL"}}},
	{META_FIELD_TDTG, X_("Tagging Time"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TDTG"}}},
	{META_FIELD_TENC, X_("Encoded by"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TENC"}}},
	{META_FIELD_T_E_X_T, X_("Lyricist/Text Writer"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TEXT"}}},
	{META_FIELD_TFLT, X_("File Type"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TFLT"}}},
	{META_FIELD_TIPL, X_("Involved People"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TIPL"}}},
	{META_FIELD_TIT1, X_("Content Group"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TIT1"}}},
	{META_FIELD_TKEY, X_("Initial key"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TKEY"}}},
	{META_FIELD_TLEN, X_("Length"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TLEN"}}},
	{META_FIELD_TMCL, X_("Musician Credits"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TMCL"}}},
	{META_FIELD_TMOO, X_("Mood"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TMOO"}}},
	{META_FIELD_TOAL, X_("Original Album"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TOAL"}}},
	{META_FIELD_TOFN, X_("Original Filename"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TOFN"}}},
	{META_FIELD_TOLY, X_("Original Lyricist"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TOLY"}}},
	{META_FIELD_TOPE, X_("Original Artist"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TOPE"}}},
	{META_FIELD_TOWN, X_("File Owner"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TOWN"}}},
	{META_FIELD_TPE2, X_("Band/Orchestra"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TPE2"}}},
	{META_FIELD_TPE4, X_("Interpreted/Remixed"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TPE4"}}},
	{META_FIELD_TPOS, X_("Part Of A Set"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TPOS"}}},
	{META_FIELD_TPRO, X_("Produced"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TPRO"}}},
	{META_FIELD_TRSN, X_("Internet Radio Station Name"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TRSN"}}},
	{META_FIELD_TRSO, X_("Internet Radio Station Owner"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TRSO"}}},
	{META_FIELD_TSOA, X_("Album Sort Order"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TSOA"}}},
	{META_FIELD_TSOP, X_("Performer Sort Order"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TSOP"}}},
	{META_FIELD_TSOT, X_("Title Sort Order"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TSOT"}}},
	{META_FIELD_TSSE, X_("Software"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TSSE"}}},
	{META_FIELD_TSST, X_("Set Subtitle"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "TSST"}}},
	{META_FIELD_TXXX, X_("User Defined Text"), "", "",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, 0, "TXXX"}}},

	{META_FIELD_VENDOR, X_("Vendor"), "", "",
	     META_TAG_OXC,
	     {{META_TAG_OXC, META_FIELD_UNIQUE | META_FIELD_MANDATORY, "vendor"}}},

	{META_FIELD_RG_REFLOUDNESS, X_("ReplayGain Reference Loudness"), "%f", "%02.1f dB",
	     META_TAG_APE | META_TAG_OXC,
	     {{META_TAG_APE, META_FIELD_UNIQUE, "Replaygain_reference_loudness"},
	      {META_TAG_OXC, META_FIELD_UNIQUE, "replaygain_reference_loudness"}}},
	{META_FIELD_RG_TRACK_GAIN, X_("ReplayGain Track Gain"), "%f", "%1.2f dB",
	     META_TAG_APE | META_TAG_OXC,
	     {{META_TAG_APE, META_FIELD_UNIQUE, "Replaygain_track_gain"},
	      {META_TAG_OXC, META_FIELD_UNIQUE, "replaygain_track_gain"}}},
	{META_FIELD_RG_TRACK_PEAK, X_("ReplayGain Track Peak"), "%f", "%f",
	     META_TAG_APE | META_TAG_OXC,
	     {{META_TAG_APE, META_FIELD_UNIQUE, "Replaygain_track_peak"},
	      {META_TAG_OXC, META_FIELD_UNIQUE, "replaygain_track_peak"}}},
	{META_FIELD_RG_ALBUM_GAIN, X_("ReplayGain Album Gain"), "%f dB", "%1.2f dB",
	     META_TAG_APE | META_TAG_OXC,
	     {{META_TAG_APE, META_FIELD_UNIQUE, "Replaygain_album_gain"},
	      {META_TAG_OXC, META_FIELD_UNIQUE, "replaygain_album_gain"}}},
	{META_FIELD_RG_ALBUM_PEAK, X_("ReplayGain Album Peak"), "%f", "%f",
	     META_TAG_APE | META_TAG_OXC,
	     {{META_TAG_APE, META_FIELD_UNIQUE, "Replaygain_album_peak"},
	      {META_TAG_OXC, META_FIELD_UNIQUE, "replaygain_album_peak"}}},

	{META_FIELD_ICY_NAME, X_("Icy-Name"), "", "",
	     META_TAG_GEN_STREAM,
	     {{META_TAG_GEN_STREAM, 0, "Icy-Name"}}},
	{META_FIELD_ICY_DESCR, X_("Icy-Description"), "", "",
	     META_TAG_GEN_STREAM,
	     {{META_TAG_GEN_STREAM, 0, "Icy-Description"}}},
	{META_FIELD_ICY_GENRE, X_("Icy-Genre"), "", "",
	     META_TAG_GEN_STREAM,
	     {{META_TAG_GEN_STREAM, 0, "Icy-Genre"}}},
	{META_FIELD_RVA2, X_("RVA"), "%f dB", "%2.2f dB",
	     META_TAG_ID3v2,
	     {{META_TAG_ID3v2, META_FIELD_UNIQUE, "RVA2"}}},
	{META_FIELD_APIC, X_("Attached Picture"), "", "",
	     META_TAG_ID3v2 | META_TAG_FLAC_APIC,
	     {{META_TAG_ID3v2, 0, "APIC"}, {META_TAG_FLAC_APIC, 0, "APIC"}}},
	{META_FIELD_GEOB, X_("Binary Object"), "", "",
	     0,
	     {}},

	{-1}};


/* data model functions */

char *
meta_get_tagname(int tag) {

	switch (tag) {
	case META_TAG_NULL: return _("NULL");
	case META_TAG_ID3v1: return _("ID3v1");
	case META_TAG_ID3v2: return _("ID3v2");
	case META_TAG_APE: return _("APE");
	case META_TAG_OXC: return _("Ogg Xiph Comments");
	case META_TAG_FLAC_APIC: return _("FLAC Pictures");
	case META_TAG_GEN_STREAM: return _("Generic StreamMeta");
	case META_TAG_MPEGSTREAM: return _("MPEG StreamMeta");
	case META_TAG_MODINFO: return _("Module info");
	default: return _("Unknown");
	}
}

int
meta_tag_from_name(char * name) {

	int i = 1;
	if (strcmp(name, meta_get_tagname(0)) == 0)
		return 0;

	while (1) {
		if (strcmp(name, meta_get_tagname(i)) == 0) {
			return i;
		}
		i <<= 1;
	}
	fprintf(stderr, "meta_get_tagname: programmer error\n");
	return -1;
}


int
meta_get_fieldname(int type, char ** str) {

	int i;
	for (i = 0; meta_model[i].type != -1; i++) {
		if (type == meta_model[i].type) {
			*str = _(meta_model[i].name);
			return 1;
		}
	}
	return 0;
}


int
meta_get_fieldname_embedded(int tag, int type, char ** str) {

	int i;
	for (i = 0; meta_model[i].type != -1; i++) {
		if (type == meta_model[i].type) {
			int j;
			for (j = 0; j < META_N_TAGS; j++) {
				if (tag == meta_model[i].emb_names[j].tag) {
					*str = meta_model[i].emb_names[j].name;
					return 1;
				}
			}
			return 0;
		}
	}
	return 0;
}


char *
meta_get_field_parsefmt(int type) {

	int i;
	for (i = 0; meta_model[i].type != -1; i++) {
		if (type == meta_model[i].type) {
			return meta_model[i].parse_fmt;
		}
	}
	if (META_FIELD_INT(type)) {
		return "%d";
	}
	if (META_FIELD_FLOAT(type)) {
		return "%f";
	}
	return "";
}


char *
meta_get_field_renderfmt(int type) {

	int i;
	for (i = 0; meta_model[i].type != -1; i++) {
		if (type == meta_model[i].type) {
			return meta_model[i].render_fmt;
		}
	}
	if (META_FIELD_INT(type)) {
		return "%d";
	}
	if (META_FIELD_FLOAT(type)) {
		return "%f";
	}
	return "";
}


int
meta_frame_type_from_name(char * name) {

	int i;
	for (i = 0; meta_model[i].type != -1; i++) {
		if (strcmp(name, _(meta_model[i].name)) == 0) {
			return meta_model[i].type;
		}
	}
	return META_FIELD_OTHER;
}


int
meta_frame_type_from_embedded_name(int tag, char * name) {

	int i;
	for (i = 0; meta_model[i].type != -1; i++) {
		int j;
		for (j = 0; j < META_N_TAGS; j++) {
			if ((tag == meta_model[i].emb_names[j].tag) &&
			    (strcasecmp(name, meta_model[i].emb_names[j].name) == 0)) {
				return meta_model[i].type;
			}
		}
	}
	return META_FIELD_OTHER;
}


GSList *
meta_get_possible_fields(int tag) {

	GSList * list = NULL;
	int i;
	for (i = 0; meta_model[i].type != -1; i++) {
		if ((tag & meta_model[i].tags) != 0) {
			list = g_slist_append(list, GINT_TO_POINTER(meta_model[i].type));
		}
	}
	return list;
}


int
meta_get_default_flags(int tag, int type) {

	int i;
	for (i = 0; meta_model[i].type != -1; i++) {
		if (type == meta_model[i].type) {
			int j;
			for (j = 0; j < META_N_TAGS; j++) {
				if (tag == meta_model[i].emb_names[j].tag) {
					return meta_model[i].emb_names[j].flags;
				}
			}
			return 0;
		}
	}
	return 0;
}


meta_frame_t *
metadata_find_companion_frame(metadata_t * meta, meta_frame_t * frame) {

	meta_frame_t * f;
	int tag = META_TAG_MAX;

	while (tag > 0) {
		if (tag != frame->tag) {
			f = metadata_get_frame_by_tag_and_type(meta, tag, frame->type, NULL);
			if (f) {
				return f;
			}
		}
		tag >>= 1;
	}

	return NULL;
}


/* search for frames other than the given frame itself
   that are of the same type and copy their contents */
void
metadata_clone_frame(metadata_t * meta, meta_frame_t * frame) {

	meta_frame_t * companion = metadata_find_companion_frame(meta, frame);

	if (companion == NULL) {
		return;
	}
	
	if (companion->field_name != NULL) {
		if (frame->field_name != NULL) {
			free(frame->field_name);
		}
		frame->field_name = strdup(companion->field_name);
	}

	if (companion->field_val != NULL) {
		frame->field_val = strdup(companion->field_val);
	}
	
	frame->int_val = companion->int_val;
	frame->float_val = companion->float_val;

	if (companion->length > 0) {
		frame->length = companion->length;
		frame->data = (unsigned char *)malloc(frame->length);
		if (frame->data == NULL) {
			fprintf(stderr, "metadata_clone_frame: malloc error\n");
			return;
		}
		memcpy(frame->data, companion->data, frame->length);
	}
}


void
metadata_add_mandatory_frames(metadata_t * meta, int tag) {

	int i;
	for (i = 0; meta_model[i].type != -1; i++) {
		int j;
		for (j = 0; j < META_N_TAGS; j++) {
			if ((tag == meta_model[i].emb_names[j].tag) &&
			    ((meta_model[i].emb_names[j].flags & META_FIELD_MANDATORY) != 0)) {
				meta_frame_t * frame = meta_frame_new();
				char * str;
				frame->tag = tag;
				frame->type = meta_model[i].type;
				frame->flags = meta_get_default_flags(tag, frame->type);
				if (meta_get_fieldname(frame->type, &str)) {
					if (options.metaedit_auto_clone) {
						metadata_clone_frame(meta, frame);
					}
					if (frame->field_name == NULL) {
						frame->field_name = strdup(str);
					}
				} else {
					fprintf(stderr, "metadata_add_mandatory_frames: programmer error\n");
					meta_frame_free(frame);
					return;
				}
				if (frame->field_val == NULL) {
					frame->field_val = strdup("");
				}
				metadata_add_frame(meta, frame);
			}
		}
	}
}


/* object methods */

metadata_t *
metadata_new(void) {

	metadata_t * meta = NULL;
	
	if ((meta = calloc(1, sizeof(metadata_t))) == NULL) {
		fprintf(stderr, "metadata.c: metadata_new() failed: calloc error\n");
		return NULL;
	}

	return meta;
}


void
metadata_free(metadata_t * meta) {

	meta_frame_t * p;
	meta_frame_t * q;
	
	if (meta->root == NULL) {
		free(meta);
		return;
	}
	
	p = meta->root;
	while (p != NULL) {
		q = p->next;
		meta_frame_free(p);
		p = q;
	}
	free(meta);
}


meta_frame_t *
meta_frame_new(void) {

	meta_frame_t * meta_frame = NULL;
	
	if ((meta_frame = calloc(1, sizeof(meta_frame_t))) == NULL) {
		fprintf(stderr, "metadata.c: meta_frame_new() failed: calloc error\n");
		return NULL;
	}

	return meta_frame;
}


void
meta_frame_free(meta_frame_t * meta_frame) {

	if (meta_frame->field_name != NULL)
		free(meta_frame->field_name);
	if (meta_frame->field_val != NULL)
		free(meta_frame->field_val);
	if (meta_frame->data != NULL)
		free(meta_frame->data);
	free(meta_frame);
}


void
metadata_add_frame(metadata_t * meta, meta_frame_t * frame) {

	frame->next = NULL;

	if (meta->root == NULL) {
		meta->root = frame;
	} else {
		meta_frame_t * prev = meta->root;
		while (prev->next != NULL) {
			prev = prev->next;
		}
		prev->next = frame;
	}
}

/* take frame out of meta; does not free frame! */
void
metadata_remove_frame(metadata_t * meta, meta_frame_t * frame) {

	meta_frame_t * prev;

	if (meta->root == frame) {
		meta->root = frame->next;
		return;
	}

	prev = meta->root;
	while (prev->next != frame) {
		prev = prev->next;
	}
	
	prev->next = frame->next;	
}


/* helper functions */


/* Search for frame of given type. Passing NULL as root will search from
 * the beginning of the frame list. Passing the previous return value of
 * this function as root allows getting multiple frames of the same type.
 * Returns NULL if there are no (more) frames of the specified type.
 */
meta_frame_t *
metadata_get_frame_by_type(metadata_t * meta, int type, meta_frame_t * root) {

	meta_frame_t * frame;

	if (root == NULL) {
		frame = meta->root;
	} else {
		frame = root->next;
	}

	if (frame == NULL) {
		return NULL;
	}

	while (frame->type != type) {
		frame = frame->next;
		if (frame == NULL) {
			return NULL;
		}
	}

	return frame;
}


/* Search for frame belonging to tag. Passing NULL as root will search from
 * the beginning of the frame list. Passing the previous return value of
 * this function as root allows getting multiple frames of the same tag.
 * Returns NULL if there are no (more) frames of the specified tag.
 */
meta_frame_t *
metadata_get_frame_by_tag(metadata_t * meta, int tag, meta_frame_t * root) {

	meta_frame_t * frame;

	if (root == NULL) {
		frame = meta->root;
	} else {
		frame = root->next;
	}

	if (frame == NULL) {
		return NULL;
	}

	while (frame->tag != tag) {
		frame = frame->next;
		if (frame == NULL) {
			return NULL;
		}
	}

	return frame;
}


/* Search for frame of given type, belonging to tag. Passing NULL as
 * root will search from the beginning of the frame list. Passing the
 * previous return value of this function as root allows getting
 * multiple frames of the same tag.  Returns NULL if there are no
 * (more) frames of the specified tag.
 */
meta_frame_t *
metadata_get_frame_by_tag_and_type(metadata_t * meta, int tag, int type,
				   meta_frame_t * root) {

	meta_frame_t * frame;

	if (root == NULL) {
		frame = meta->root;
	} else {
		frame = root->next;
	}

	if (frame == NULL) {
		return NULL;
	}

	while (frame->tag != tag || frame->type != type) {
		frame = frame->next;
		if (frame == NULL) {
			return NULL;
		}
	}

	return frame;
}


meta_frame_t *
metadata_add_frame_from_keyval(metadata_t * meta, int tag, char * key, char * val) {

	meta_frame_t * frame = meta_frame_new();
	char * str;
	char * parsefmt;

	frame->tag = tag;
	frame->type = meta_frame_type_from_embedded_name(tag, key);
	if (meta_get_fieldname(frame->type, &str)) {
		frame->field_name = strdup(str);
	} else {
		frame->field_name = strdup(key);
	}
	frame->field_val = strdup(val);
	frame->flags = meta_get_default_flags(tag, frame->type);

	parsefmt = meta_get_field_parsefmt(frame->type);
	if (META_FIELD_INT(frame->type)) {
		if (sscanf(val, parsefmt, &frame->int_val) < 1) {
			fprintf(stderr, "warning: couldn't parse integer value from '%s' using format string '%s'\n", val, parsefmt);
		}
	} else if (META_FIELD_FLOAT(frame->type)) {
		if (sscanf(val, parsefmt, &frame->float_val) < 1) {
			fprintf(stderr, "warning: couldn't parse float value from '%s' using format string '%s'\n", val, parsefmt);
		}
	}

	metadata_add_frame(meta, frame);
	return frame;
}


metadata_t *
metadata_from_mpeg_stream_data(char * str) {

	metadata_t * meta = metadata_new();
	char * s;

	meta->writable = 0;

	s = strtok(str, ";");
	while (s) {
		char key[MAXLEN];
		char val[MAXLEN];
		char c;
		int k, n = 0;

		for (k = 0; ((c = s[n]) != '\0') &&
			     (c != '=') && (k < MAXLEN-1); k++) {
			key[k] = c;
			++n;
		}
		key[k] = '\0';
		++n;

		if (strstr(key, "Stream") == key) {
			memmove(key, key+6, sizeof(key)-6);
		}

		if (str[n] == '\'') {
			++n;
		}

		for (k = 0; ((c = s[n]) != '\0') &&
			     (c != '\'') && (k < MAXLEN-1); k++) {
			val[k] = c;
			++n;
		}
		val[k] = '\0';

		metadata_add_frame_from_keyval(meta, META_TAG_MPEGSTREAM, key, val);

		s = strtok(NULL, ";");
	}
	return meta;
}


void
metadata_make_title_string(metadata_t * meta, char * dest) {

	char dest1[MAXLEN];
	char * artist = NULL;
	char * album = NULL;
	char * title = NULL;
	char * icy_name = NULL;

	metadata_get_artist(meta, &artist);
	metadata_get_album(meta, &album);
	metadata_get_title(meta, &title);
	metadata_get_icy_name(meta, &icy_name);

	if (title != NULL && artist != NULL && album != NULL) {
		make_title_string(dest1, options.title_format, artist, album, title);
	} else if (title != NULL && artist != NULL) {
		make_title_string_no_album(dest1, options.title_format_no_album,
					   artist, title);
	} else if (title != NULL) {
		strncpy(dest1, title, MAXLEN-1);
	} else {
		if (icy_name == NULL) {
			dest[0] = '\0';
		} else {
			strncpy(dest, icy_name, MAXLEN-1);
		}
		return;
	}

	if (icy_name == NULL) {
		strncpy(dest, dest1, MAXLEN-1);
	} else {
		snprintf(dest, MAXLEN-1, "%s (%s)", dest1, icy_name);
	}
}


void
metadata_make_playlist_string(metadata_t * meta, char * dest) {

	char * name = NULL;
	char * descr = NULL;

	metadata_get_icy_name(meta, &name);
	metadata_get_icy_descr(meta, &descr);

	if (name == NULL) {
		metadata_make_title_string(meta, dest);
	} else {
		if (descr != NULL) {
			snprintf(dest, MAXLEN-1, "%s (%s)", name, descr);
		} else {
			strncpy(dest, name, MAXLEN-1);
		}
	}
}


/* low-level utils */

u_int32_t
meta_read_int32(unsigned char * buf) {

	return ((((u_int32_t)buf[0]))       |
		(((u_int32_t)buf[1]) << 8)  |
		(((u_int32_t)buf[2]) << 16) |
		(((u_int32_t)buf[3]) << 24));
}

u_int64_t
meta_read_int64(unsigned char * buf) {

	return ((((u_int64_t)buf[0]))       |
		(((u_int64_t)buf[1]) << 8)  |
		(((u_int64_t)buf[2]) << 16) |
		(((u_int64_t)buf[3]) << 24) |
		(((u_int64_t)buf[4]) << 32) |
		(((u_int64_t)buf[5]) << 40) |
		(((u_int64_t)buf[6]) << 48) |
		(((u_int64_t)buf[7]) << 56));
}

void
meta_write_int32(u_int32_t val, unsigned char * buf) {

	buf[0] = ((val & 0xff));
	buf[1] = ((val >> 8)  & 0xff);
	buf[2] = ((val >> 16) & 0xff);
	buf[3] = ((val >> 24) & 0xff);
}

void
meta_write_int64(u_int64_t val, unsigned char * buf) {

	buf[0] = ((val & 0xff));
	buf[1] = ((val >> 8)  & 0xff);
	buf[2] = ((val >> 16) & 0xff);
	buf[3] = ((val >> 24) & 0xff);
	buf[4] = ((val >> 32) & 0xff);
	buf[5] = ((val >> 40) & 0xff);
	buf[6] = ((val >> 48) & 0xff);
	buf[7] = ((val >> 56) & 0xff);
}


/* debug functions */

void
metadata_dump(metadata_t * meta) {

	meta_frame_t * frame = meta->root;
	printf("\nMetadata block dump, writable = %d, valid_tags = %d\n",
	       meta->writable, meta->valid_tags);
	while (frame) {
		meta_dump_frame(frame);
		frame = frame->next;
	}
}


void
meta_dump_frame(meta_frame_t * frame) {
	printf("  Tag %2d Type %4d  F=0x%04x  '%s'  '%s'  ",
	       frame->tag, frame->type, frame->flags, frame->field_name, frame->field_val);
	printf("int %d  float %f  ptr %p  len %d\n",
	       frame->int_val, frame->float_val, frame->data, frame->length);
}
