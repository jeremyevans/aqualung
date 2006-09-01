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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <gtk/gtk.h>

#ifdef HAVE_ID3
#include <id3tag.h>
#endif /* HAVE_ID3 */

#ifdef HAVE_FLAC
#include <FLAC/format.h>
#include <FLAC/metadata.h>
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
#ifdef _WIN32
#undef _WIN32
#include <vorbis/vorbisfile.h>
#define _WIN32
#else
#include <vorbis/vorbisfile.h>
#endif /* _WIN32 */
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MOD
#include <libmodplug/modplug.h>
#endif /* HAVE_MOD */

#include "common.h"
#include "core.h"
#include "i18n.h"
#include "options.h"
#include "decoder/file_decoder.h"
#ifdef HAVE_OGG_VORBIS
#include "decoder/dec_vorbis.h"
#endif /* HAVE_OGG_VORBIS */
#ifdef HAVE_MOD
#include "decoder/dec_mod.h"
#endif /* HAVE_MOD */
#include "meta_decoder.h"


extern options_t options;


#ifdef HAVE_ID3
id3_tag_data *
id3_new(void) {

	id3_tag_data * new = NULL;

	if ((new = (id3_tag_data *)malloc(sizeof(id3_tag_data))) == NULL) {
		fprintf(stderr, "meta_decoder.c/id3_new(): malloc error\n");
		return NULL;
	}

	new->id[0] = '\0';
	new->label = NULL;
	new->utf8 = NULL;
	new->fval = 0.0f;
	new->next = NULL;

	return new;
}
#endif /* HAVE_ID3 */

#ifdef HAVE_MOD
mod_info *
modinfo_new(void) {

	mod_info * new = NULL;

	if ((new = (mod_info *)malloc(sizeof(mod_info))) == NULL) {
		fprintf(stderr, "meta_decoder.c/modinfo_new(): malloc error\n");
		return NULL;
	}

	new->title = NULL;
	new->active = 0;

	return new;
}
#endif /* HAVE_MOD */

oggv_comment *
oggv_new(void) {

	oggv_comment * new = NULL;

	if ((new = (oggv_comment *)malloc(sizeof(oggv_comment))) == NULL) {
		fprintf(stderr, "meta_decoder.c/oggv_new(): malloc error\n");
		return NULL;
	}

	new->label = NULL;
	new->str = NULL;
	new->fval = 0.0f;
	new->next = NULL;

	return new;
}


metadata *
meta_new(void) {

	metadata * new = NULL;

	if ((new = (metadata *)malloc(sizeof(metadata))) == NULL) {
		fprintf(stderr, "meta_decoder.c/meta_new(): malloc error\n");
		return NULL;
	}

#ifdef HAVE_ID3
	new->id3_root = id3_new();
#endif /* HAVE_ID3 */
#ifdef HAVE_FLAC
	new->flac_root = oggv_new();
#endif /* HAVE_FLAC */
#ifdef HAVE_OGG_VORBIS
	new->oggv_root = oggv_new();
#endif /* HAVE_OGG_VORBIS */
#ifdef HAVE_MOD
	new->mod_root = modinfo_new();
#endif /* HAVE_MOD */

	return new;
}


#ifdef HAVE_ID3
void
append_id3(metadata * meta, id3_tag_data * id3) {

	id3_tag_data * id3_prev;

	if (meta == NULL) {
		fprintf(stderr, "append_id3(): assertion meta != NULL failed\n");
		return;
	}

	if (meta->id3_root == NULL) {
		fprintf(stderr, "append_id3(): assertion meta->id3_root != NULL failed\n");
		return;
	}

	id3_prev = meta->id3_root;
	while (id3_prev->next != NULL) {
		id3_prev = id3_prev->next;
	}
	id3_prev->next = id3;
}
#endif /* HAVE_ID3 */


#ifdef HAVE_FLAC
void
append_flac(metadata * meta, oggv_comment * oggv) {

	oggv_comment * oggv_prev;

	if (meta == NULL) {
		fprintf(stderr, "append_flac(): assertion meta != NULL failed\n");
		return;
	}

	if (meta->flac_root == NULL) {
		fprintf(stderr, "append_flac(): assertion meta->flac_root != NULL failed\n");
		return;
	}

	oggv_prev = meta->flac_root;
	while (oggv_prev->next != NULL) {
		oggv_prev = oggv_prev->next;
	}
	oggv_prev->next = oggv;
}
#endif /* HAVE_FLAC */


#ifdef HAVE_OGG_VORBIS
void
append_oggv(metadata * meta, oggv_comment * oggv) {

	oggv_comment * oggv_prev;

	if (meta == NULL) {
		fprintf(stderr, "append_oggv(): assertion meta != NULL failed\n");
		return;
	}

	if (meta->oggv_root == NULL) {
		fprintf(stderr, "append_oggv(): assertion meta->oggv_root != NULL failed\n");
		return;
	}

	oggv_prev = meta->oggv_root;
	while (oggv_prev->next != NULL) {
		oggv_prev = oggv_prev->next;
	}
	oggv_prev->next = oggv;
}
#endif /* HAVE_OGG_VORBIS */

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


#ifdef HAVE_ID3
void
meta_read_rva2(metadata * meta, struct id3_tag * tag) {

        struct id3_frame * frame;
        char str[MAXLEN];

	id3_latin1_t const * id;
	id3_byte_t const * data;
	id3_length_t length;

	enum {
		CHANNEL_OTHER         = 0x00,
		CHANNEL_MASTER_VOLUME = 0x01,
		CHANNEL_FRONT_RIGHT   = 0x02,
		CHANNEL_FRONT_LEFT    = 0x03,
		CHANNEL_BACK_RIGHT    = 0x04,
		CHANNEL_BACK_LEFT     = 0x05,
		CHANNEL_FRONT_CENTRE  = 0x06,
		CHANNEL_BACK_CENTRE   = 0x07,
		CHANNEL_SUBWOOFER     = 0x08
	};


        frame = id3_tag_findframe(tag, "RVA2", 0);
        if (!frame) {
		return;
	}


	id = id3_field_getlatin1(id3_frame_field(frame, 0));
	data = id3_field_getbinarydata(id3_frame_field(frame, 1), &length);

	assert(id && data);
	while (length >= 4) {
		unsigned int peak_bytes;
		
		peak_bytes = (data[3] + 7) / 8;
		if (4 + peak_bytes > length)
			break;
		
		if (data[0] == CHANNEL_MASTER_VOLUME) {
			signed int voladj_fixed;
			double voladj_float;
			id3_tag_data * id3 = id3_new();
			
			voladj_fixed = (data[1] << 8) | (data[2] << 0);
			voladj_fixed |= -(voladj_fixed & 0x8000);
			
			voladj_float = (double) voladj_fixed / 512;
			
			snprintf(str, MAXLEN-1, "%+.1f dB (tagged by %s)",
				 voladj_float, id);

			strcpy(id3->id, "RVA2");
			id3->label = strdup(_("Relative Volume"));
			id3->utf8 = (signed char *) strdup((char *) str);
			id3->fval = voladj_float;
			append_id3(meta, id3);

			break;
		}
		
		data   += 4 + peak_bytes;
		length -= 4 + peak_bytes;
	}
}


void
meta_read_id3(metadata * meta, struct id3_tag * tag) {

        unsigned int i;
        struct id3_frame const * frame;
        id3_ucs4_t const * ucs4;
        id3_utf8_t * utf8;
        char str[MAXLEN];

        static struct {
                char const * id;
                char const * label;
        } const info[] = {
                { ID3_FRAME_TITLE,  X_("Title") },
                { "TIT3",           X_("Subtitle") },
                { "TCOP",           0 },  /* Copyright */
                { "TPRO",           0 },  /* Produced */
                { "TCOM",           X_("Composer") },
                { ID3_FRAME_ARTIST, X_("Artist") },
                { "TPE2",           X_("Orchestra") },
                { "TPE3",           X_("Conductor") },
                { "TEXT",           X_("Lyricist") },
                { ID3_FRAME_ALBUM,  X_("Album") },
                { ID3_FRAME_TRACK,  X_("Track") },
                { ID3_FRAME_YEAR,   X_("Year") },
                { "TPUB",           X_("Publisher") },
                { ID3_FRAME_GENRE,  X_("Genre") },
                { "TRSN",           X_("Station") },
                { "TENC",           X_("Encoder") }
        };

        /* text information */

        for (i = 0; i < sizeof(info) / sizeof(info[0]); ++i) {

                union id3_field const * field;
                unsigned int nstrings, j;

                frame = id3_tag_findframe(tag, info[i].id, 0);
                if (frame == 0)
                        continue;

                field = id3_frame_field(frame, 1);
                nstrings = id3_field_getnstrings(field);

                for (j = 0; j < nstrings; ++j) {
                        ucs4 = id3_field_getstrings(field, j);
                        assert(ucs4);

                        if (strcmp(info[i].id, ID3_FRAME_GENRE) == 0)
                                ucs4 = id3_genre_name(ucs4);

                        utf8 = id3_ucs4_utf8duplicate(ucs4);
                        if (utf8 == 0)
                                goto fail;

                        if (j == 0 && info[i].label) {
                                snprintf(str, MAXLEN-1, "%s:", gettext(info[i].label));

                        } else if (strcmp(info[i].id, "TCOP") == 0 ||
				   strcmp(info[i].id, "TPRO") == 0) {

                                        snprintf(str, MAXLEN-1, "%s",
                                                 (info[i].id[1] == 'C') ?
						 _("Copyright (C)") : _("Produced (P)"));

			} else {
				snprintf(str, MAXLEN-1, "%s:", info[i].id);
			}

			{
				id3_tag_data * id3 = id3_new();

				strcpy(id3->id, info[i].id);
				id3->label = strdup(str);
				id3->utf8 = (signed char *) strdup((char *) utf8);
				append_id3(meta, id3);
			}

                        free(utf8);
                }
        }

        /* comments */

        i = 0;
        while ((frame = id3_tag_findframe(tag, ID3_FRAME_COMMENT, i++))) {
                ucs4 = id3_field_getstring(id3_frame_field(frame, 2));
                assert(ucs4);

                if (*ucs4)
                        continue;

                ucs4 = id3_field_getfullstring(id3_frame_field(frame, 3));
                assert(ucs4);

                utf8 = id3_ucs4_utf8duplicate(ucs4);
                if (utf8 == 0)
                        goto fail;

                sprintf(str, "%s:", _("Comment"));

		{
			id3_tag_data * id3 = id3_new();
			
			strcpy(id3->id, "XCOM");
			id3->label = strdup(str);
			id3->utf8 = (signed char *) strdup((char *) utf8);
			append_id3(meta, id3);
		}

                free(utf8);
                break;
        }

        if (0) {
        fail:
                fprintf(stderr, "meta_read_id3(): error: memory allocation error in libid3\n");
        }
}
#endif /* HAVE_ID3 */


#ifdef HAVE_FLAC
void
meta_read_flac(metadata * meta, FLAC__StreamMetadata * flacmeta) {

        char field[MAXLEN];
        char str[MAXLEN];
        int i,j,k;


        for (i = 0; i < flacmeta->data.vorbis_comment.num_comments; i++) {

		oggv_comment * oggv = oggv_new();


                for (j = 0; (flacmeta->data.vorbis_comment.comments[i].entry[j] != '=') &&
                             (j < MAXLEN-1); j++) {

                        field[j] = (j == 0) ?
                                toupper(flacmeta->data.vorbis_comment.comments[i].entry[j]) :
                                tolower(flacmeta->data.vorbis_comment.comments[i].entry[j]);
                }
                field[j++] = ':';
                field[j] = '\0';

                for (k = 0; (j < flacmeta->data.vorbis_comment.comments[i].length) &&
                             (k < MAXLEN-1); j++) {

                        str[k++] = flacmeta->data.vorbis_comment.comments[i].entry[j];
                }
                str[k] = '\0';

		
		oggv->label = strdup(field);
		oggv->str = strdup(str);

		if ((strcmp(field, "Replaygain_track_gain:") == 0) ||
		    (strcmp(field, "Replaygain_album_gain:") == 0)) {

			oggv->fval = convf(str);
		} else {
			oggv->fval = 0.0f;
		}

		append_flac(meta, oggv);
        }

        for (j = 0; j < flacmeta->data.vorbis_comment.vendor_string.length && j < MAXLEN-1; j++) {

                field[j] = flacmeta->data.vorbis_comment.vendor_string.entry[j];
        }
        field[j] = '\0';
	{
		oggv_comment * oggv = oggv_new();
		oggv->label = strdup(_("Vendor:"));
		oggv->str = strdup(field);
		append_flac(meta, oggv);
	}
}
#endif /* HAVE_FLAC */


/* ret: 1 if successful, 0 if error */
int
meta_read(metadata * meta, char * file) {

        file_decoder_t * fdec = NULL;
#ifdef HAVE_OGG_VORBIS
        char str[MAXLEN];
#endif /* HAVE_OGG_VORBIS */

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

#ifdef HAVE_ID3
	{
		struct id3_file * id3file;
                struct id3_tag * id3tag;

                if ((id3file = id3_file_open(file,
                                             ID3_FILE_MODE_READONLY)) != NULL) {
                        if ((id3tag = id3_file_tag(id3file)) != NULL) {

                                meta_read_id3(meta, id3tag);
                                meta_read_rva2(meta, id3tag);
                        }
                }
		id3_file_close(id3file);
	}
#endif /* HAVE_ID3 */

#ifdef HAVE_OGG_VORBIS
        if (fdec->file_lib == VORBIS_LIB) {

		decoder_t * dec = (decoder_t *)(fdec->pdec);
		vorbis_pdata_t * pd = (vorbis_pdata_t *)dec->pdata;
                vorbis_comment * vc = ov_comment(&(pd->vf), -1);
                char field[MAXLEN];
                int cnt;
                int i, j;


                for (cnt = 0; cnt < vc->comments; cnt++) {

			oggv_comment * oggv = oggv_new();

                        for (i = 0; (vc->user_comments[cnt][i] != '=') &&
                                     (vc->user_comments[cnt][i] != '\0') &&
                                     (i < MAXLEN-2); i++) {

                                field[i] = (i == 0) ? toupper(vc->user_comments[cnt][i]) :
                                        tolower(vc->user_comments[cnt][i]);
                        }
                        field[i++] = ':';
                        field[i] = '\0';
                        for (j = 0; (vc->user_comments[cnt][i] != '\0') && (j < MAXLEN-1); i++) {
                                str[j++] = vc->user_comments[cnt][i];
                        }
                        str[j] = '\0';

			oggv->label = strdup(field);
			oggv->str = strdup(str);

			if ((strcmp(field, "Replaygain_track_gain:") == 0) ||
			    (strcmp(field, "Replaygain_album_gain:") == 0)) {

				oggv->fval = convf(str);
			} else {
				oggv->fval = 0.0f;
			}

			append_oggv(meta, oggv);
		}

                strncpy(str, vc->vendor, MAXLEN-1);
		{
			oggv_comment * oggv = oggv_new();
			oggv->label = strdup(_("Vendor:"));
			oggv->str = strdup(str);
			append_oggv(meta, oggv);
		}
	}
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_FLAC
        if (fdec->file_lib == FLAC_LIB) {

                FLAC__Metadata_SimpleIterator * iter = FLAC__metadata_simple_iterator_new();
                FLAC__StreamMetadata * flacmeta = NULL;

                if (!FLAC__metadata_simple_iterator_init(iter,
		        file, true, true)) {

                        fprintf(stderr,
                                "meta_read(): error: "
                                "FLAC__metadata_simple_iterator_init() failed on %s\n",
                                file);

			fprintf(stderr, "%s\n",
				FLAC__Metadata_SimpleIteratorStatusString[FLAC__metadata_simple_iterator_status(iter)]);
			FLAC__metadata_simple_iterator_delete(iter);

			file_decoder_close(fdec);
			file_decoder_delete(fdec);
                        return 0;
                }

                while (1) {
                        flacmeta = FLAC__metadata_simple_iterator_get_block(iter);
                        if (flacmeta == NULL)
                                break;

                        /* process a VORBIS_COMMENT metadata block */
                        if (flacmeta->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {

				meta_read_flac(meta, flacmeta);
                        }

                        FLAC__metadata_object_delete(flacmeta);
                        if (!FLAC__metadata_simple_iterator_next(iter))
                                break;
                }

                FLAC__metadata_simple_iterator_delete(iter);
        }
#endif /* HAVE_FLAC */

#ifdef HAVE_MOD
        if (fdec->file_lib == MOD_LIB) {

                decoder_t * dec = (decoder_t *)(fdec->pdec);
                mod_pdata_t * pd = (mod_pdata_t *)dec->pdata;
		mod_info * mi = modinfo_new();
                
		mi->title = strdup(ModPlug_GetName(pd->mpf));
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


void
meta_free(metadata * meta) {

#ifdef HAVE_ID3
	id3_tag_data * id3;
	id3_tag_data * id3_next;
#endif /* HAVE_ID3 */
	oggv_comment * oggv;
	oggv_comment * oggv_next;
#ifdef HAVE_MOD
	mod_info * mi;
#endif /* HAVE_MOD */


#ifdef HAVE_ID3
	id3 = meta->id3_root;
	while (id3 != NULL) {
		id3_next = id3->next;
		free(id3->label);
		free(id3->utf8);
		free(id3);
		id3 = id3_next;
	}
#endif /* HAVE_ID3 */

#ifdef HAVE_FLAC
	oggv = meta->flac_root;
	while (oggv != NULL) {
		oggv_next = oggv->next;
		free(oggv->label);
		free(oggv->str);
		free(oggv);
		oggv = oggv_next;
	}
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
	oggv = meta->oggv_root;
	while (oggv != NULL) {
		oggv_next = oggv->next;
		free(oggv->label);
		free(oggv->str);
		free(oggv);
		oggv = oggv_next;
	}
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MOD
	mi = meta->mod_root;
        if(mi != NULL) {
                if(mi->title)                   
                        free(mi->title);
                free(mi);
        }
#endif /* HAVE_MOD */

	free(meta);
}


/* for DEBUG purposes */
/*
void
meta_list(metadata * meta) {

	id3_tag_data * id3;
	oggv_comment * oggv;


	printf("\nMetadata list\n");

	printf("format_major = %d\n", meta->format_major);
	printf("format_minor = %d\n", meta->format_minor);
	printf("total_samples = %lld\n", meta->total_samples);
	printf("sample_rate = %ld\n", meta->sample_rate);
	printf("is_mono = %d\n", meta->is_mono);
	printf("bps = %d\n", meta->bps);

	
	id3 = meta->id3_root;
	if (id3->next != NULL) {
		id3 = id3->next;
		printf("\nID3 tags\n");
		while (id3 != NULL) {
			printf("id: %s  label: %s  utf8: %s  fval=%f\n",
			       id3->id, id3->label, id3->utf8, id3->fval);
			id3 = id3->next;
		}
	}


	oggv = meta->flac_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		printf("\nFLAC meta\n");
		while (oggv != NULL) {
			printf("label: %s  str: %s  fval=%f\n",
			       oggv->label, oggv->str, oggv->fval);
			oggv = oggv->next;
		}
	}


	oggv = meta->oggv_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		printf("\nOggV meta\n");
		while (oggv != NULL) {
			printf("label: %s  str: %s  fval=%f\n",
			       oggv->label, oggv->str, oggv->fval);
			oggv = oggv->next;
		}
	}
}
*/


/* ret: 1 if found, 0 if no RVA data */
/* can be called with fval == NULL only to see if RVA is available */
int
meta_get_rva(metadata * meta, float * fval) {

#ifdef HAVE_ID3
	id3_tag_data * id3;
#endif /* HAVE_ID3 */
	oggv_comment * oggv;

	char replaygain_label[MAXLEN];


	if (meta == NULL) {
		fprintf(stderr, "meta_get_rva(): assertion meta != NULL failed\n");
		return 0;
	}

	switch (options.replaygain_tag_to_use) {
	case 0:
		strcpy(replaygain_label, "Replaygain_track_gain:");
		break;
	case 1:
		strcpy(replaygain_label, "Replaygain_album_gain:");
		break;
	default:
		fprintf(stderr, "meta_decoder.c: illegal replaygain_tag_to_use value -- "
			"please see the programmers\n");
	}

#ifdef HAVE_FLAC
	oggv = meta->flac_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		while (oggv != NULL) {

			if (strcmp(oggv->label, replaygain_label) == 0) {
				if (fval != NULL) {
					*fval = oggv->fval;
				}
				return 1;
			}
			oggv = oggv->next;
		}
	}
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
	oggv = meta->oggv_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		while (oggv != NULL) {

			if (strcmp(oggv->label, replaygain_label) == 0) {
				if (fval != NULL) {
					*fval = oggv->fval;
				}
				return 1;
			}
			oggv = oggv->next;
		}
	}
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_ID3
	id3 = meta->id3_root;
	if (id3->next != NULL) {
		id3 = id3->next;
		while (id3 != NULL) {

			if (strcmp(id3->id, "RVA2") == 0) {
				if (fval != NULL) {
					*fval = id3->fval;
				}
				return 1;
			}
			id3 = id3->next;
		}
	}
#endif /* HAVE_ID3 */

	return 0;
}


/* ret: 1 if found, 0 if no data */
/* can be called with str == NULL only to see if data is found */
int
meta_get_abstract(metadata * meta, char * str, char * tag_flac, char * tag_ogg, char * tag_id3, char * tag_mod) {

#ifdef HAVE_ID3
	id3_tag_data * id3;
#endif /* HAVE_ID3 */
	oggv_comment * oggv;
#ifdef HAVE_MOD
	mod_info * mi;
#endif /* HAVE_MOD */


	if (meta == NULL) {
		fprintf(stderr, "meta_get_title(): assertion meta != NULL failed\n");
		return 0;
	}

#ifdef HAVE_FLAC
	oggv = meta->flac_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		while (oggv != NULL) {

			if (strcmp(oggv->label, tag_flac) == 0) {
				if (str != NULL) {
					strncpy(str, oggv->str, MAXLEN-1);
				}
				return 1;
			}
			oggv = oggv->next;
		}
	}
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
	oggv = meta->oggv_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		while (oggv != NULL) {

			if (strcmp(oggv->label, tag_ogg) == 0) {
				if (str != NULL) {
					strncpy(str, oggv->str, MAXLEN-1);
				}
				return 1;
			}
			oggv = oggv->next;
		}
	}
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_ID3
	id3 = meta->id3_root;
	if (id3->next != NULL) {
		id3 = id3->next;
		while (id3 != NULL) {

			if (strcmp(id3->id, tag_id3) == 0) {
				if (str != NULL) {
					strncpy(str, (char *) id3->utf8, MAXLEN-1);
				}
				return 1;
			}
			id3 = id3->next;
		}
	}
#endif /* HAVE_ID3 */

#ifdef HAVE_MOD 
	mi = meta->mod_root;

        if (mi != NULL) {
                if (strcmp(tag_mod, "Title") == 0) {
                        if (str != NULL && mi->title != NULL) {
                                strncpy(str, (char *) mi->title, MAXLEN-1);
                        }
                        return 1;
                }
        }
#endif /* HAVE_MOD */

	return 0;
}


int
meta_get_title(metadata * meta, char * str) {

#ifdef HAVE_ID3
	return meta_get_abstract(meta, str, "Title:", "Title:", ID3_FRAME_TITLE, "Title");
#else
	return meta_get_abstract(meta, str, "Title:", "Title:", "dummy", "Title");
#endif /* HAVE_ID3 */
}


int
meta_get_record(metadata * meta, char * str) {

#ifdef HAVE_ID3
	return meta_get_abstract(meta, str, "Album:", "Album:", ID3_FRAME_ALBUM, "dummy");
#else
	return meta_get_abstract(meta, str, "Album:", "Album:", "dummy", "dummy");
#endif /* HAVE_ID3 */
}


int
meta_get_artist(metadata * meta, char * str) {

#ifdef HAVE_ID3
	return meta_get_abstract(meta, str, "Artist:", "Artist:", ID3_FRAME_ARTIST, "dummy");
#else
	return meta_get_abstract(meta, str, "Artist:", "Artist:", "dummy", "dummy");
#endif /* HAVE_ID3 */
}


int
meta_get_year(metadata * meta, char * str) {

#ifdef HAVE_ID3
	return meta_get_abstract(meta, str, "Date:", "Date:", ID3_FRAME_YEAR, "dummy");
#else
	return meta_get_abstract(meta, str, "Date:", "Date:", "dummy", "dummy");
#endif /* HAVE_ID3 */
}


int
meta_get_comment(metadata * meta, char * str) {

	return meta_get_abstract(meta, str, "Comment:", "Comment:", "XCOM", "dummy");
}


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

