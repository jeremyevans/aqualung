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

#include "common.h"
#include "core.h"
#include "i18n.h"
#include "file_decoder.h"
#include "meta_decoder.h"


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

	new->id3_root = id3_new();
	new->flac_root = oggv_new();
	new->oggv_root = oggv_new();

	return new;
}


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
			id3->utf8 = strdup(str);
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
				id3->utf8 = strdup(utf8);
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
			id3->utf8 = strdup(utf8);
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

		if (strcmp(field, "Replaygain_track_gain:") == 0) {
			oggv->fval = convf(str);
		} else {
			oggv->fval = 0.0f;
		}

		append_flac(meta, oggv);
        }

        for (j = 0; j < flacmeta->data.vorbis_comment.vendor_string.length; j++) {

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
/* expects filename in UTF8 */
int
meta_read(metadata * meta, char * file) {

        file_decoder_t * fdec = NULL;
        char str[MAXLEN];


        if ((fdec = file_decoder_new()) == NULL) {
                fprintf(stderr, "meta_read: error: file_decoder_new() returned NULL\n");
		file_decoder_delete(fdec);
                return 0;
        }

        if (file_decoder_open(fdec, g_locale_from_utf8(file, -1, NULL, NULL, NULL), 44100)) {
                fprintf(stderr, "file_decoder_open() failed on %s\n",
                        g_locale_from_utf8(file, -1, NULL, NULL, NULL));
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

                if ((id3file = id3_file_open(g_locale_from_utf8(file, -1, NULL, NULL, NULL),
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

                vorbis_comment * vc = ov_comment(&(fdec->vf), -1);
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

			if (strcmp(field, "Replaygain_track_gain:") == 0) {
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
		        g_locale_from_utf8(file, -1, NULL, NULL, NULL), true, true)) {

                        fprintf(stderr,
                                "meta_read(): error: "
                                "FLAC__metadata_simple_iterator_init() failed on %s\n",
                                g_locale_from_utf8(file, -1, NULL, NULL, NULL));

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

        file_decoder_close(fdec);
        file_decoder_delete(fdec);

	return 1;
}


void
meta_free(metadata * meta) {

	id3_tag_data * id3;
	id3_tag_data * id3_next;
	oggv_comment * oggv;
	oggv_comment * oggv_next;


	id3 = meta->id3_root;
	while (id3 != NULL) {
		id3_next = id3->next;
		free(id3->label);
		free(id3->utf8);
		free(id3);
		id3 = id3_next;
	}

	oggv = meta->flac_root;
	while (oggv != NULL) {
		oggv_next = oggv->next;
		free(oggv->label);
		free(oggv->str);
		free(oggv);
		oggv = oggv_next;
	}

	oggv = meta->oggv_root;
	while (oggv != NULL) {
		oggv_next = oggv->next;
		free(oggv->label);
		free(oggv->str);
		free(oggv);
		oggv = oggv_next;
	}

	free(meta);
}


/* for DEBUG purposes */
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


/* ret: 1 if found, 0 if no RVA data */
/* can be called with fval == NULL only to see if RVA is available */
int
meta_get_rva(metadata * meta, float * fval) {

	id3_tag_data * id3;
	oggv_comment * oggv;


	if (meta == NULL) {
		fprintf(stderr, "meta_get_rva(): assertion meta != NULL failed\n");
		return 0;
	}

	oggv = meta->flac_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		while (oggv != NULL) {

			if (strcmp(oggv->label, "Replaygain_track_gain:") == 0) {
				if (fval != NULL) {
					*fval = oggv->fval;
				}
				return 1;
			}
			oggv = oggv->next;
		}
	}

	oggv = meta->oggv_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		while (oggv != NULL) {

			if (strcmp(oggv->label, "Replaygain_track_gain:") == 0) {
				if (fval != NULL) {
					*fval = oggv->fval;
				}
				return 1;
			}
			oggv = oggv->next;
		}
	}

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

	return 0;
}


/* ret: 1 if found, 0 if no Title data */
/* can be called with str == NULL only to see if data is found */
int
meta_get_title(metadata * meta, char * str) {

	id3_tag_data * id3;
	oggv_comment * oggv;


	if (meta == NULL) {
		fprintf(stderr, "meta_get_title(): assertion meta != NULL failed\n");
		return 0;
	}

	oggv = meta->flac_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		while (oggv != NULL) {

			if (strcmp(oggv->label, "Title:") == 0) {
				if (str != NULL) {
					strncpy(str, oggv->str, MAXLEN-1);
				}
				return 1;
			}
			oggv = oggv->next;
		}
	}

	oggv = meta->oggv_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		while (oggv != NULL) {

			if (strcmp(oggv->label, "Title:") == 0) {
				if (str != NULL) {
					strncpy(str, oggv->str, MAXLEN-1);
				}
				return 1;
			}
			oggv = oggv->next;
		}
	}

	id3 = meta->id3_root;
	if (id3->next != NULL) {
		id3 = id3->next;
		while (id3 != NULL) {

			if (strcmp(id3->id, ID3_FRAME_TITLE) == 0) {
				if (str != NULL) {
					strncpy(str, id3->utf8, MAXLEN-1);
				}
				return 1;
			}
			id3 = id3->next;
		}
	}

	return 0;
}


/* ret: 1 if found, 0 if no Record data */
/* can be called with str == NULL only to see if data is found */
int
meta_get_record(metadata * meta, char * str) {

	id3_tag_data * id3;
	oggv_comment * oggv;


	if (meta == NULL) {
		fprintf(stderr, "meta_get_record(): assertion meta != NULL failed\n");
		return 0;
	}

	oggv = meta->flac_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		while (oggv != NULL) {

			if (strcmp(oggv->label, "Record:") == 0) {
				if (str != NULL) {
					strncpy(str, oggv->str, MAXLEN-1);
				}
				return 1;
			}
			oggv = oggv->next;
		}
	}

	oggv = meta->oggv_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		while (oggv != NULL) {

			if (strcmp(oggv->label, "Record:") == 0) {
				if (str != NULL) {
					strncpy(str, oggv->str, MAXLEN-1);
				}
				return 1;
			}
			oggv = oggv->next;
		}
	}

	id3 = meta->id3_root;
	if (id3->next != NULL) {
		id3 = id3->next;
		while (id3 != NULL) {

			if (strcmp(id3->id, ID3_FRAME_ALBUM) == 0) {
				if (str != NULL) {
					strncpy(str, id3->utf8, MAXLEN-1);
				}
				return 1;
			}
			id3 = id3->next;
		}
	}

	return 0;
}


/* ret: 1 if found, 0 if no Artist data */
/* can be called with str == NULL only to see if data is found */
int
meta_get_artist(metadata * meta, char * str) {

	id3_tag_data * id3;
	oggv_comment * oggv;


	if (meta == NULL) {
		fprintf(stderr, "meta_get_artist(): assertion meta != NULL failed\n");
		return 0;
	}

	oggv = meta->flac_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		while (oggv != NULL) {

			if (strcmp(oggv->label, "Artist:") == 0) {
				if (str != NULL) {
					strncpy(str, oggv->str, MAXLEN-1);
				}
				return 1;
			}
			oggv = oggv->next;
		}
	}

	oggv = meta->oggv_root;
	if (oggv->next != NULL) {
		oggv = oggv->next;
		while (oggv != NULL) {

			if (strcmp(oggv->label, "Artist:") == 0) {
				if (str != NULL) {
					strncpy(str, oggv->str, MAXLEN-1);
				}
				return 1;
			}
			oggv = oggv->next;
		}
	}

	id3 = meta->id3_root;
	if (id3->next != NULL) {
		id3 = id3->next;
		while (id3 != NULL) {

			if (strcmp(id3->id, ID3_FRAME_ARTIST) == 0) {
				if (str != NULL) {
					strncpy(str, id3->utf8, MAXLEN-1);
				}
				return 1;
			}
			id3 = id3->next;
		}
	}

	return 0;
}
