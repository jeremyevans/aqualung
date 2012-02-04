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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>

#include "../httpc.h"
#include "../metadata.h"
#include "../options.h"
#include "dec_null.h"
#ifdef HAVE_CDDA
#include "dec_cdda.h"
#endif /* HAVE_CDDA */
#ifdef HAVE_SNDFILE
#include "dec_sndfile.h"
#endif /* HAVE_SNDFILE */
#ifdef HAVE_FLAC
#include "dec_flac.h"
#endif /* HAVE_FLAC */
#ifdef HAVE_VORBIS
#include "dec_vorbis.h"
#endif /* HAVE_VORBIS */
#ifdef HAVE_SPEEX
#include "dec_speex.h"
#endif /* HAVE_SPEEX */
#ifdef HAVE_MPC
#include "dec_mpc.h"
#endif /* HAVE_MPC */
#ifdef HAVE_MPEG
#include "dec_mpeg.h"
#endif /* HAVE_MPEG */
#ifdef HAVE_MOD
#include "dec_mod.h"
#endif /* HAVE_MOD */
#ifdef HAVE_MAC
#include "dec_mac.h"
#endif /* HAVE_MAC */
#ifdef HAVE_LAVC
#include <libavformat/avformat.h>
#include "dec_lavc.h"
#endif /* HAVE_LAVC */
#ifdef HAVE_WAVPACK
#include "dec_wavpack.h"
#endif /* HAVE_WAVPACK */
#include "file_decoder.h"


extern options_t options;

typedef decoder_t * decoder_init_t(file_decoder_t * fdec);

/* this controls the order in which decoders are probed for a file */
static decoder_init_t * decoder_init_v[] = {
	null_decoder_init,
#ifdef HAVE_CDDA
	cdda_decoder_init,
#endif /* HAVE_CDDA */
#ifdef HAVE_SNDFILE
	sndfile_decoder_init,
#endif /* HAVE_SNDFILE */
#ifdef HAVE_FLAC
	flac_decoder_init,
#endif /* HAVE_FLAC */
#ifdef HAVE_VORBIS
	vorbis_decoder_init,
#endif /* HAVE_VORBIS */
#ifdef HAVE_SPEEX
	speex_dec_init,
#endif /* HAVE_SPEEX */
#ifdef HAVE_MPC
	mpc_decoder_init_func,
#endif /* HAVE_MPC */
#ifdef HAVE_MAC
	mac_decoder_init,
#endif /* HAVE_MAC */
#ifdef HAVE_MPEG
	mpeg_decoder_init,
#endif /* HAVE_MPEG */
#ifdef HAVE_WAVPACK
	wavpack_decoder_init,
#endif /* HAVE_WAVPACK */
#ifdef HAVE_MOD
	mod_decoder_init,
#endif /* HAVE_MOD */
#ifdef HAVE_LAVC
	lavc_decoder_init,
#endif /* HAVE_LAVC */
	NULL
};


/* utility function used by some decoders to check file extension */
int
is_valid_extension(char ** valid_extensions, char * filename, int module) {

	int i;
	char * c = NULL, * d = NULL;
        char *ext;

        /* post ext */
        i = 0;

	if ((c = strrchr(filename, '.')) != NULL) {

                ++c;

                while (valid_extensions[i] != NULL) {

                        if (strcasecmp(c, valid_extensions[i]) == 0) {
                                return 1;
                        }
                        ++i;
                }
	}

        if (module) {      /* checking mod pre file extension */
                           /* lots of amiga modules has EXT.NAME filename format */

                /* pre ext */
                i = 0;
                ext = strdup(filename);

                if (ext && (c = strrchr(ext, '/')) != NULL) {

                        ++c;

                        if ((d = strchr(ext, '.')) != NULL) {

                                *d = '\0';

                                while (valid_extensions[i] != NULL) {

                                        if (strcasecmp(c, valid_extensions[i]) == 0) {
                                                free(ext);
                                                return 1;
                                        }
                                        ++i;
                                }
                        }
                }

                free(ext);
        }

	return 0;
}


/* call this first before using file_decoder in program */
void
file_decoder_init(void) {

#ifdef HAVE_LAVC
	av_register_all();
#endif /* HAVE_LAVC */
}


file_decoder_t *
file_decoder_new(void) {

	file_decoder_t * fdec = NULL;
	
	if ((fdec = calloc(1, sizeof(file_decoder_t))) == NULL) {
		fprintf(stderr, "file_decoder.c: file_decoder_new() failed: calloc error\n");
		return NULL;
	}
	
	fdec->file_open = 0;

	fdec->voladj_db = 0.0f;
	fdec->voladj_lin = 1.0f;

	fdec->pdec = NULL;

	return fdec;
}


void
file_decoder_delete(file_decoder_t * fdec) {
	
	if (fdec->file_open) {
		file_decoder_close(fdec);
	}

	free(fdec);
}


int
file_decoder_finalize_open(file_decoder_t * fdec, decoder_t * dec, char * filename) {

	if (fdec->fileinfo.channels == 1) {
		fdec->fileinfo.is_mono = 1;
		goto ok_open;

	} else if (fdec->fileinfo.channels == 2) {
		fdec->fileinfo.is_mono = 0;
		goto ok_open;

	} else {
		fprintf(stderr, "file_decoder_open: programmer error: "
			"soundfile with %d\n channels is unsupported.\n",
			fdec->fileinfo.channels);
		goto no_open;
	}

 ok_open:
	fdec->file_open = 1;
	fdec->samples_left = fdec->fileinfo.total_samples;
	fdec->fileinfo.format_str = dec->format_str;
	fdec->fileinfo.format_flags = dec->format_flags;
	return 0;

 no_open:
	fprintf(stderr, "file_decoder_open: unable to open %s\n", filename);
	return 1;
}


int
stream_decoder_open(file_decoder_t * fdec, char * URL) {

	int ret;
	decoder_t * dec;
	http_session_t * session = httpc_new();
	
    	if ((ret = httpc_init(session, fdec, URL,
			      options.inet_use_proxy,
			      options.inet_proxy,
			      options.inet_proxy_port,
			      options.inet_noproxy_domains, 0L)) != HTTPC_OK) {
		fprintf(stderr, "stream_decoder_open: httpc_init failed, ret = %d\n", ret);
		httpc_del(session);
		return DECODER_OPEN_FERROR;
	}

#ifdef HAVE_MPEG
	if ((session->headers.content_type == NULL) ||
	    (strcasecmp(session->headers.content_type, "audio/mp3") == 0) ||
	    (strcasecmp(session->headers.content_type, "audio/mpeg") == 0) ||
	    (strcasecmp(session->headers.content_type, "application/mp3") == 0) ||
	    (strcasecmp(session->headers.content_type, "application/mpeg") == 0)) {
		int ret;

		if (session->headers.content_type == NULL) {
			fprintf(stderr, "Warning: no Content-Type, assuming audio/mpeg\n");
		}

		dec = mpeg_decoder_init(fdec);
		if (!dec)
			return DECODER_OPEN_FERROR;

		ret = mpeg_stream_decoder_open(dec, session);
		if (ret == DECODER_OPEN_FERROR) {
			dec->destroy(dec);
			return ret;
		}
		fdec->pdec = (void *)dec;
		return file_decoder_finalize_open(fdec, dec, URL);
	}
#else
	if (session->headers.content_type == NULL) {
		fprintf(stderr, "stream_decoder_open: error: no Content-Type found\n");
		httpc_close(session);
		httpc_del(session);
		return DECODER_OPEN_FERROR;
	}
#endif /* HAVE_MPEG */

#ifdef HAVE_VORBIS
	if ((strcasecmp(session->headers.content_type, "application/ogg") == 0) ||
	    (strcasecmp(session->headers.content_type, "audio/ogg") == 0) ||
	    (strcasecmp(session->headers.content_type, "audio/x-vorbis") == 0)) {

		int ret;

		dec = vorbis_decoder_init(fdec);
		if (!dec)
			return DECODER_OPEN_FERROR;

		ret = vorbis_stream_decoder_open(dec, session);
		if (ret == DECODER_OPEN_FERROR) {
			dec->destroy(dec);
			return ret;
		}
		fdec->pdec = (void *)dec;
		return file_decoder_finalize_open(fdec, dec, URL);
	}
#endif /* HAVE_VORBIS */

	fprintf(stderr, "Sorry, no handler for Content-Type: %s\n",
		session->headers.content_type);
	httpc_close(session);
	httpc_del(session);
	return 1;
}


/* return: 0 is OK, >0 is error */
int
file_decoder_open(file_decoder_t * fdec, char * filename) {

	int i, ret;
	decoder_t * dec;

	if (filename == NULL) {
		fprintf(stderr, "Warning: filename == NULL passed to file_decoder_open()\n");
		fprintf(stderr, "This is likely to be a programmer error, please report.\n");
		return 1;
	}
	fdec->filename = strdup(filename);

	if (httpc_is_url(filename))
		return stream_decoder_open(fdec, filename);

	for (i = 0; decoder_init_v[i] != NULL; i++) {
		dec = decoder_init_v[i](fdec);
		if (!dec) {
			continue;
		}
		fdec->pdec = (void *)dec;
		ret = dec->open(dec, filename);
		if (ret == DECODER_OPEN_FERROR) {
			dec->destroy(dec);
			goto no_open;
		} else if (ret == DECODER_OPEN_BADLIB) {
			dec->destroy(dec);
			continue;
		} else if (ret != DECODER_OPEN_SUCCESS) {
			printf("programmer error, please report: "
			       "illegal retvalue %d from dec->open() at %d\n", ret, i);
			return 1;
		}

		break;
	}

	if (decoder_init_v[i] == NULL) {
	        goto no_open;
	}

	if (fdec->fileinfo.channels == 1) {
		fdec->fileinfo.is_mono = 1;
		goto ok_open;

	} else if (fdec->fileinfo.channels == 2) {
		fdec->fileinfo.is_mono = 0;
		goto ok_open;

	} else {
		fprintf(stderr, "file_decoder_open: programmer error: "
			"soundfile with %d\n channels is unsupported.\n",
			fdec->fileinfo.channels);
		goto no_open;
	}

 ok_open:
	fdec->file_open = 1;
	fdec->samples_left = fdec->fileinfo.total_samples;
	fdec->fileinfo.format_str = dec->format_str;
	fdec->fileinfo.format_flags = dec->format_flags;
	return 0;

 no_open:
	fprintf(stderr, "file_decoder_open: unable to open %s\n", filename);
	return 1;
}


void
file_decoder_send_metadata(file_decoder_t * fdec) {

	decoder_t * dec = (decoder_t *)(fdec->pdec);

	return dec->send_metadata(dec);
}

void
file_decoder_set_rva(file_decoder_t * fdec, float voladj) {

	fdec->voladj_db = voladj;
	fdec->voladj_lin = db2lin(voladj);
}


void
file_decoder_set_meta_cb(file_decoder_t * fdec,
			 void (* meta_cb)(metadata_t * meta, void * data),
			 void * data) {

	fdec->meta_cb = meta_cb;
	fdec->meta_cbdata = data;
}


void
file_decoder_close(file_decoder_t * fdec) {

	decoder_t * dec;

	if (!fdec->file_open) {
		return;
	}

	dec = (decoder_t *)(fdec->pdec);
	dec->close(dec);
	dec->destroy(dec);
	fdec->pdec = NULL;
	fdec->file_open = 0;
	fdec->file_lib = 0;
	if (fdec->filename != NULL) {
		free(fdec->filename);
		fdec->filename = NULL;
	}
	if (fdec->meta != NULL) {
		metadata_free(fdec->meta);
		fdec->meta = NULL;
	}
}


unsigned int
file_decoder_read(file_decoder_t * fdec, float * dest, int num) {

	decoder_t * dec = (decoder_t *)(fdec->pdec);

	return dec->read(dec, dest, num);
}


void
file_decoder_seek(file_decoder_t * fdec, unsigned long long seek_to_pos) {

	decoder_t * dec = (decoder_t *)(fdec->pdec);

	dec->seek(dec, seek_to_pos);
}
 

void
file_decoder_pause(file_decoder_t * fdec) {

	decoder_t * dec = (decoder_t *)(fdec->pdec);
	if (dec->pause != NULL)
		dec->pause(dec);
}


void
file_decoder_resume(file_decoder_t * fdec) {

	decoder_t * dec = (decoder_t *)(fdec->pdec);
	if (dec->resume != NULL)
		dec->resume(dec);
}


float
get_file_duration(char * file) {

	file_decoder_t * fdec;
	float duration;

	if ((fdec = file_decoder_new()) == NULL) {
                fprintf(stderr, "get_file_duration: error: file_decoder_new() returned NULL\n");
                return -1.0f;
        }

        if (file_decoder_open(fdec, file)) {
                fprintf(stderr, "file_decoder_open() failed on %s\n", file);
		file_decoder_delete(fdec);
                return -1.0f;
        }

	duration = (float)fdec->fileinfo.total_samples / fdec->fileinfo.sample_rate;

        file_decoder_close(fdec);
        file_decoder_delete(fdec);

	return duration;
}

/* taken from cdparanoia source */
int
bigendianp(void) {

	int test=1;
	char *hack=(char *)(&test);
	if(hack[0])return(0);
	return(1);
}


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

