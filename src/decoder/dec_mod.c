/*                                                     -*- linux-c -*-
    Copyright (C) 2005 Tom Szilagyi

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
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif /* HAVE_LIBZ */

#ifdef HAVE_LIBBZ2
#include <bzlib.h>
#endif /* HAVE_LIBBZ2 */

#include "../metadata.h"
#include "dec_mod.h"

extern size_t sample_size;


#ifdef HAVE_MOD

/* list of accepted file extensions */
char * valid_extensions_mod[] = {
	"669", "amf", "ams", "dbm", "dmf", "dsm", "far", "it",
	"j2b", "mdl", "med", "mod", "mt2", "mtm", "okt", "psm",
	"ptm", "s3m", "stm", "ult", "umx", "xm",
#ifdef HAVE_LIBZ
        "gz",
#endif /* HAVE_LIBZ */
#ifdef HAVE_LIBBZ2
        "bz2",
#endif /* HAVE_LIBBZ2 */
        NULL
};

#if defined(HAVE_LIBZ) || defined(HAVE_LIBBZ2)

char unpacked_filename[PATH_MAX];

char *
unpack_file (char *filename, int type) {

	char *pos, *c;
	int i, len = 0;
	char buffer[16384];
#ifdef HAVE_LIBZ
	gzFile *gz_input_file = NULL;
#endif /* HAVE_LIBZ */
#ifdef HAVE_LIBBZ2
	BZFILE *bz2_input_file = NULL;
#endif /* HAVE_LIBBZ2 */
	FILE *output_file;

	unpacked_filename[0] = '\0';

	if ((type == GZ || type == BZ2) && (pos = strrchr(filename, '.')) != NULL) {
		pos++;

		if (type == GZ) {

			if (strcasecmp(pos, "gz") != 0) {
				return NULL;
			}

		} else {

			if (strcasecmp(pos, "bz2") != 0) {
				return NULL;
			}
		}

		strncat(unpacked_filename, "/tmp/", PATH_MAX-1);

		i = 5;      /* strlen("/tmp/") */

		if ((c = strrchr(filename, '/')) != NULL) {
			c++;

			while (c != pos-1) {
				unpacked_filename[i++] = *c;
				c++;
			}
		}

		unpacked_filename[i] = '\0';

		if (type == GZ) {

#ifdef HAVE_LIBZ
			if ((gz_input_file = gzopen (filename, "r")) == NULL) {
				return NULL;
			}
#endif /* HAVE_LIBZ */

		} else {

#ifdef HAVE_LIBBZ2
			if ((bz2_input_file = BZ2_bzopen (filename, "r")) == NULL) {
				return NULL;
			}
#endif /* HAVE_LIBBZ2 */
		}

		if ((output_file = fopen (unpacked_filename, "w")) == NULL) {
			return NULL;
		}

		while (1) {

			if (type == GZ) {
#ifdef HAVE_LIBZ
				len = gzread(gz_input_file, buffer, sizeof(buffer));
#endif /* HAVE_LIBZ */
			} else {
#ifdef HAVE_LIBBZ2
				len = BZ2_bzread(bz2_input_file, buffer, sizeof(buffer));
#endif /* HAVE_LIBBZ2 */
			}

			if (len < 0) {
				return NULL;
			}
			if (len == 0) break;

			if ((int)fwrite(buffer, 1, (unsigned)len, output_file) != len) {
				return NULL;
			}
		}

		if (fclose(output_file)) {
			return NULL;
		}

		if (type == GZ) {
#ifdef HAVE_LIBZ
			if (gzclose(gz_input_file) != Z_OK){
				return NULL;
			}
#endif /* HAVE_LIBZ */
		} else {
#ifdef HAVE_LIBBZ2
			BZ2_bzclose(bz2_input_file);
#endif /* HAVE_LIBBZ2 */
		}

		return unpacked_filename;

	} else {

		return NULL;
	}
}

int
remove_unpacked_file (void) {

	if (unpacked_filename[0]) {
		unlink (unpacked_filename);
		return 1;
	}

	return 0;
}

#endif /* HAVE_LIBZ && HAVE_LIBBZ2 */


/* return 1 if reached end of stream, 0 else */
int
decode_mod(decoder_t * dec) {

	mod_pdata_t * pd = (mod_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	int i;
	long bytes_read;
        float fbuffer[MOD_BUFSIZE/2];
        char buffer[MOD_BUFSIZE];

        if ((bytes_read = ModPlug_Read(pd->mpf, buffer, MOD_BUFSIZE)) > 0) {
                for (i = 0; i < bytes_read/2; i++) {
                        fbuffer[i] = *((short *)(buffer + 2*i)) * fdec->voladj_lin / 32768.f;
		}
                rb_write(pd->rb, (char *)fbuffer, bytes_read/2 * sample_size);
                return 0;
        } else {
		return 1;
	}
}


decoder_t *
mod_decoder_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_mod.c: mod_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(mod_pdata_t))) == NULL) {
                fprintf(stderr, "dec_mod.c: mod_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->init = mod_decoder_init;
	dec->destroy = mod_decoder_destroy;
	dec->open = mod_decoder_open;
	dec->send_metadata = mod_decoder_send_metadata;
	dec->close = mod_decoder_close;
	dec->read = mod_decoder_read;
	dec->seek = mod_decoder_seek;

	return dec;
}


void
mod_decoder_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}

int
is_valid_mod_extension(char * filename) {
	return is_valid_extension(valid_extensions_mod, filename, 1);
}

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


void
mod_filename_filter(char *str, char **valid_extensions) {

        char buffer[MAXLEN];
        char *pos;
        int i;

        strncpy(buffer, str, MAXLEN-1);

        if ((pos = strrchr(buffer, '/')) != NULL) {

            pos++;
            i = 0;

            while(pos[i]) {
                str[i] = pos[i];
                i++;
            }
            str[i] = '\0';
        }

        if ((pos = strrchr(str, '.')) != NULL) {

            if (strcasecmp(pos+1, "gz") == 0 || strcasecmp (pos+1, "bz2") == 0) {
                *pos = '\0';
            }
        }

        if ((pos = strrchr(str, '.')) != NULL) {

            i = 0;
            while (valid_extensions[i] != NULL) {

                    if (strcasecmp(pos+1, valid_extensions[i]) == 0) {
                            *pos = '\0';
                    }
                    ++i;
            }
        }

        strncpy(buffer, str, MAXLEN-1);

        if ((pos = strchr(buffer, '.')) != NULL) {
            *pos = '\0';

            i = 0;
            while (valid_extensions[i] != NULL) {

                if (strcasecmp(buffer, valid_extensions[i]) == 0) {

		    pos++;
                    i = 0;

                    while (pos[i] != '\0') {
                        str[i] = pos[i];
                        i++;
                    }
                    str[i] = '\0';
                    break;
                }
                ++i;
            }

        }
}

void
mod_send_metadata(decoder_t * dec) {

	mod_pdata_t * pd = (mod_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	metadata_t * meta = metadata_new();
	meta_frame_t * frame = meta_frame_new();

	mod_info mi;

	if (meta == NULL) {
		fprintf(stderr, "mod_send_metadata: metadata_new() failed\n");
		return;
	}
	if (frame == NULL) {
		fprintf(stderr, "mod_send_metadata: meta_frame_new() failed\n");
		return;
	}

	memset(&mi, 0x00, sizeof(mod_info));

	strncpy(mi.title, ModPlug_GetName(pd->mpf), MAXLEN-1);
	mod_name_filter(mi.title);

	if (!strlen(mi.title)) {
		strncpy(mi.title, fdec->filename, MAXLEN-1);
		mod_filename_filter(mi.title, valid_extensions_mod);
		mod_name_filter(mi.title);
	}

	mi.active = 1;
#ifdef HAVE_MOD_INFO
	mi.type = ModPlug_GetModuleType(pd->mpf);
	mi.samples = ModPlug_NumSamples(pd->mpf);
	mi.instruments = ModPlug_NumInstruments(pd->mpf);
	mi.patterns = ModPlug_NumPatterns(pd->mpf);
	mi.channels = ModPlug_NumChannels(pd->mpf);
#endif /* HAVE_MOD_INFO */

	metadata_add_frame(meta, frame);
	frame->tag = META_TAG_MODINFO;
	frame->type = META_FIELD_MODINFO;
	frame->data = calloc(sizeof(mod_info), 1);
	frame->length = sizeof(mod_info);

	if (frame->data == NULL) {
		fprintf(stderr, "mod_send_metadata: calloc() error\n");
		metadata_free(meta);
		return;
	}

	memcpy(frame->data, &mi, sizeof(mod_info));

	meta->fdec = fdec;
	fdec->meta = meta;
}

int
mod_decoder_open(decoder_t * dec, char * mod_filename) {

char *filename = NULL;

	mod_pdata_t * pd = (mod_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

#ifdef HAVE_LIBZ
        filename = unpack_file(mod_filename, GZ);
#endif /* HAVE_LIBZ */

#ifdef HAVE_LIBBZ2
        if (filename == NULL) {
                filename = unpack_file(mod_filename, BZ2);
        }
#endif /* HAVE_LIBBZ2 */

        if (filename == NULL) {
                filename = mod_filename;
        }

	if (!is_valid_mod_extension(filename)) {
		return DECODER_OPEN_BADLIB;
	}

	if ((pd->fd = open(filename, O_RDONLY)) == -1) {
		fprintf(stderr,
			"mod_decoder_open: nonexistent or non-accessible file: %s\n",
			filename);
		return DECODER_OPEN_FERROR;
	}

	if (fstat(pd->fd, &(pd->st)) == -1 || pd->st.st_size == 0) {
		fprintf(stderr,
			"mod_decoder_open: fstat() error or zero-length file: %s\n",
			filename);
		close(pd->fd);
		return DECODER_OPEN_FERROR;
	}

	pd->fdm = mmap(0, pd->st.st_size, PROT_READ, MAP_SHARED, pd->fd, 0);
	if (pd->fdm == MAP_FAILED) {
		fprintf(stderr,
			"mod_decoder_open: mmap() failed %s\n", filename);
		close(pd->fd);
		return DECODER_OPEN_FERROR;
	}

	if ((pd->mpf = ModPlug_Load(pd->fdm, pd->st.st_size)) == NULL) {
		if (munmap(pd->fdm, pd->st.st_size) == -1)
			fprintf(stderr, "Error while munmap()'ing MOD Audio file mapping\n");
		close(pd->fd);
		return DECODER_OPEN_BADLIB;
	}

	if (pd->st.st_size * 8000.0f / ModPlug_GetLength(pd->mpf) >= 1000000.0f) {
		fprintf(stderr,
			"mod_decoder_open: MOD bitrate greater than 1 Mbit/s, "
			"very likely not a MOD file: %s\n", filename);

		ModPlug_Unload(pd->mpf);
		if (munmap(pd->fdm, pd->st.st_size) == -1)
			fprintf(stderr,
				"Error while munmap()'ing MOD Audio file mapping\n");
		close(pd->fd);
		return DECODER_OPEN_BADLIB;
	}

        /* set libmodplug decoder parameters */
	pd->mp_settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING | MODPLUG_ENABLE_NOISE_REDUCTION;
        pd->mp_settings.mChannels = 2;
	pd->mp_settings.mBits = 16;
	pd->mp_settings.mFrequency = 44100;
	pd->mp_settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
	pd->mp_settings.mReverbDepth = 100;
	pd->mp_settings.mReverbDelay = 100;
	pd->mp_settings.mBassAmount = 100;
	pd->mp_settings.mBassRange = 100;
	pd->mp_settings.mSurroundDepth = 100;
	pd->mp_settings.mSurroundDelay = 40;
	pd->mp_settings.mLoopCount = 0;

	ModPlug_SetSettings(&(pd->mp_settings));

	pd->is_eos = 0;
	pd->rb = rb_create(pd->mp_settings.mChannels * sample_size * RB_MOD_SIZE);
	fdec->fileinfo.channels = pd->mp_settings.mChannels;
	fdec->fileinfo.sample_rate = pd->mp_settings.mFrequency;
	fdec->file_lib = MOD_LIB;
	strcpy(dec->format_str, "MOD Audio");

	fdec->fileinfo.total_samples = ModPlug_GetLength(pd->mpf)
		/ 1000.0f * pd->mp_settings.mFrequency;
	fdec->fileinfo.bps = pd->st.st_size * 8000.0f /	ModPlug_GetLength(pd->mpf);

	mod_send_metadata(dec);
	return DECODER_OPEN_SUCCESS;
}


void
mod_decoder_send_metadata(decoder_t * dec) {

        file_decoder_t* fdec = dec->fdec;

	if (fdec->meta != NULL && fdec->meta_cb != NULL) {
		fdec->meta_cb(fdec->meta, fdec->meta_cbdata);
	}
}


void
mod_decoder_close(decoder_t * dec) {

	mod_pdata_t * pd = (mod_pdata_t *)dec->pdata;

	ModPlug_Unload(pd->mpf);

#if defined(HAVE_LIBZ) || defined(HAVE_LIBBZ2)
        remove_unpacked_file();
#endif /* HAVE_LIBZ, HAVE_LIBBZ2 */

        if (munmap(pd->fdm, pd->st.st_size) == -1)
		fprintf(stderr, "Error while munmap()'ing MOD Audio file mapping\n");
	close(pd->fd);
	rb_free(pd->rb);
}


unsigned int
mod_decoder_read(decoder_t * dec, float * dest, int num) {

	mod_pdata_t * pd = (mod_pdata_t *)dec->pdata;

	unsigned int numread = 0;
	unsigned int n_avail = 0;

	while ((rb_read_space(pd->rb) <
		num * pd->mp_settings.mChannels * sample_size) && (!pd->is_eos)) {

		pd->is_eos = decode_mod(dec);
	}

	n_avail = rb_read_space(pd->rb) /
		(pd->mp_settings.mChannels * sample_size);

	if (n_avail > num)
		n_avail = num;

	rb_read(pd->rb, (char *)dest, n_avail *
			     pd->mp_settings.mChannels * sample_size);

	numread = n_avail;
	return numread;
}


void
mod_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {

	mod_pdata_t * pd = (mod_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	char flush_dest;

	if (seek_to_pos == fdec->fileinfo.total_samples) {
		--seek_to_pos;
	}

	ModPlug_Seek(pd->mpf, (double)seek_to_pos / pd->mp_settings.mFrequency * 1000.0f);
	fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
	/* empty mod decoder ringbuffer */
	while (rb_read_space(pd->rb))
		rb_read(pd->rb, &flush_dest, sizeof(char));
}


#else
decoder_t *
mod_decoder_init(file_decoder_t * fdec) {

        return NULL;
}
#endif /* HAVE_MOD */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :

