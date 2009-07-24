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
#include <unistd.h>

#include "../metadata_flac.h"
#include "dec_flac.h"

extern size_t sample_size;


#ifdef HAVE_FLAC_8
/* FLAC write callback */
FLAC__StreamDecoderWriteStatus
write_callback(const FLAC__StreamDecoder * decoder,
               const FLAC__Frame * frame,
               const FLAC__int32 * const buffer[],
               void * client_data) {

	decoder_t * dec = (decoder_t *) client_data;
	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	int i, j;
	long int scale, blocksize;
        FLAC__int32 buf[2];
        float fbuf[2];


        if (pd->probing)
                return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

        blocksize = frame->header.blocksize;
        scale = 1 << (pd->bits_per_sample - 1);

        for (i = 0; i < blocksize; i++) {
                for (j = 0; j < pd->channels; j++) {
                        buf[j] = *(buffer[j] + i);
                        fbuf[j] = (float)buf[j] * fdec->voladj_lin / scale;
                }
                rb_write(pd->rb, (char *)fbuf,
                                      pd->channels * sample_size);
        }

        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


/* FLAC metadata callback */
void
metadata_callback(const FLAC__StreamDecoder * decoder,
                  const FLAC__StreamMetadata * metadata,
                  void * client_data) {

	decoder_t * dec = (decoder_t *)client_data;
	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;

        if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
                pd->SR = metadata->data.stream_info.sample_rate;
                pd->bits_per_sample = metadata->data.stream_info.bits_per_sample;
                pd->channels = metadata->data.stream_info.channels;
                pd->total_samples = metadata->data.stream_info.total_samples;
        } else {
                fprintf(stderr, "FLAC metadata callback: ignoring unexpected header\n");
	}
}


/* FLAC error callback */
void
error_callback(const FLAC__StreamDecoder * decoder,
               FLAC__StreamDecoderErrorStatus status,
               void * client_data) {

	decoder_t * dec = (decoder_t *) client_data;
	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;

        pd->error = 1;
}
#endif /* HAVE_FLAC_8 */


#ifdef HAVE_FLAC_7
/* FLAC write callback */
FLAC__StreamDecoderWriteStatus
write_callback(const FLAC__FileDecoder * decoder,
               const FLAC__Frame * frame,
               const FLAC__int32 * const buffer[],
               void * client_data) {

	decoder_t * dec = (decoder_t *) client_data;
	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	int i, j;
	long int scale, blocksize;
        FLAC__int32 buf[2];
        float fbuf[2];


        if (pd->probing)
                return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

        blocksize = frame->header.blocksize;
        scale = 1 << (pd->bits_per_sample - 1);

        for (i = 0; i < blocksize; i++) {
                for (j = 0; j < pd->channels; j++) {
                        buf[j] = *(buffer[j] + i);
                        fbuf[j] = (float)buf[j] * fdec->voladj_lin / scale;
                }
                rb_write(pd->rb, (char *)fbuf,
                                      pd->channels * sample_size);
        }

        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


/* FLAC metadata callback */
void
metadata_callback(const FLAC__FileDecoder * decoder,
                  const FLAC__StreamMetadata * metadata,
                  void * client_data) {

	decoder_t * dec = (decoder_t *)client_data;
	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;


        if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
                pd->SR = metadata->data.stream_info.sample_rate;
                pd->bits_per_sample = metadata->data.stream_info.bits_per_sample;
                pd->channels = metadata->data.stream_info.channels;
                pd->total_samples = metadata->data.stream_info.total_samples;
        } else {
                fprintf(stderr, "FLAC metadata callback: ignoring unexpected header\n");
	}
}


/* FLAC error callback */
void
error_callback(const FLAC__FileDecoder * decoder,
               FLAC__StreamDecoderErrorStatus status,
               void * client_data) {

	decoder_t * dec = (decoder_t *) client_data;
	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;

        pd->error = 1;
}
#endif /* HAVE_FLAC_7 */


#ifdef HAVE_FLAC
decoder_t *
flac_decoder_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_flac.c: flac_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(flac_pdata_t))) == NULL) {
                fprintf(stderr, "dec_flac.c: flac_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->init = flac_decoder_init;
	dec->destroy = flac_decoder_destroy;
	dec->open = flac_decoder_open;
	dec->send_metadata = flac_decoder_send_metadata;
	dec->close = flac_decoder_close;
	dec->read = flac_decoder_read;
	dec->seek = flac_decoder_seek;

	return dec;
}


void
flac_decoder_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


int
flac_meta_vc_replace_or_append(file_decoder_t * fdec, FLAC__StreamMetadata * smeta) {

	FLAC__Metadata_SimpleIterator * iter;
	FLAC__bool ret;
	int found = 0;

	iter = FLAC__metadata_simple_iterator_new();
	if (iter == NULL) {
		return META_ERROR_NOMEM;
	}

	ret = FLAC__metadata_simple_iterator_init(iter, fdec->filename, false, false);
	if (!ret) {
		fprintf(stderr, "dec_flac.c/flac_meta_replace_or_append: error: %s\n",
			FLAC__Metadata_SimpleIteratorStatusString[
			  FLAC__metadata_simple_iterator_status(iter)]);

		FLAC__metadata_simple_iterator_delete(iter);
		return META_ERROR_INTERNAL;
	}

	if (FLAC__metadata_simple_iterator_is_writable(iter) == false) {
		fprintf(stderr, "error: FLAC meta not writable!\n");
		FLAC__metadata_simple_iterator_delete(iter);
		return META_ERROR_NOT_WRITABLE;
	}

	do {
		switch (FLAC__metadata_simple_iterator_get_block_type(iter)) {
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			ret = FLAC__metadata_simple_iterator_set_block(iter, smeta, true);
			if (ret == false) {
				fprintf(stderr, "error: FLAC metadata write failed!\n");
				FLAC__metadata_simple_iterator_delete(iter);
				return META_ERROR_INTERNAL;
			}
			found = 1;
			break;
		default:
			break;
		}
	} while (FLAC__metadata_simple_iterator_next(iter));

	if (!found) {
		do { /* rewind to STREAMINFO and insert after that */
			if (FLAC__metadata_simple_iterator_get_block_type(iter) ==
			    FLAC__METADATA_TYPE_STREAMINFO) {
				break;
			}
		} while (FLAC__metadata_simple_iterator_prev(iter));

		ret = FLAC__metadata_simple_iterator_insert_block_after(iter, smeta, true);
		if (ret == false) {
			fprintf(stderr, "error: FLAC metadata write failed!\n");
			FLAC__metadata_simple_iterator_delete(iter);
			return META_ERROR_INTERNAL;
		}
	}

	FLAC__metadata_simple_iterator_delete(iter);
	return META_ERROR_NONE;
}


#ifdef HAVE_FLAC_8
int
flac_meta_append_pics(file_decoder_t * fdec, metadata_t * meta) {

	FLAC__Metadata_SimpleIterator * iter;
	FLAC__bool ret;
	meta_frame_t * frame;
	int found_vc = 0;

	iter = FLAC__metadata_simple_iterator_new();
	if (iter == NULL) {
		return META_ERROR_NOMEM;
	}

	ret = FLAC__metadata_simple_iterator_init(iter, fdec->filename, false, false);
	if (!ret) {
		fprintf(stderr, "dec_flac.c/flac_meta_append_pics: error: %s\n",
			FLAC__Metadata_SimpleIteratorStatusString[
			  FLAC__metadata_simple_iterator_status(iter)]);

		FLAC__metadata_simple_iterator_delete(iter);
		return META_ERROR_INTERNAL;
	}

	if (FLAC__metadata_simple_iterator_is_writable(iter) == false) {
		fprintf(stderr, "error: FLAC meta not writable!\n");
		FLAC__metadata_simple_iterator_delete(iter);
		return META_ERROR_NOT_WRITABLE;
	}

	/* try to insert pictures after the Vorbis Comment, or if
	   there is none, after STREAMINFO. */
	do {
		if (FLAC__metadata_simple_iterator_get_block_type(iter) ==
		    FLAC__METADATA_TYPE_VORBIS_COMMENT) {
			found_vc = 1;
			break;
		}
	} while (FLAC__metadata_simple_iterator_next(iter));

	if (!found_vc) {
		do { /* rewind to STREAMINFO and insert after that */
			if (FLAC__metadata_simple_iterator_get_block_type(iter) ==
			    FLAC__METADATA_TYPE_STREAMINFO) {
				break;
			}
		} while (FLAC__metadata_simple_iterator_prev(iter));
	}

	frame = metadata_get_frame_by_tag(meta, META_TAG_FLAC_APIC, NULL);
	while (frame) {
		FLAC__StreamMetadata * smeta = metadata_apic_frame_to_smeta(frame);
		ret = FLAC__metadata_simple_iterator_insert_block_after(iter, smeta, true);
		if (ret == false) {
			fprintf(stderr, "error: FLAC metadata write failed!\n");
			FLAC__metadata_object_delete(smeta);
			FLAC__metadata_simple_iterator_delete(iter);
			return META_ERROR_INTERNAL;
		}

		FLAC__metadata_object_delete(smeta);
		frame = metadata_get_frame_by_tag(meta, META_TAG_FLAC_APIC, frame);
	}

	FLAC__metadata_simple_iterator_delete(iter);
	return META_ERROR_NONE;
}
#endif /* HAVE_FLAC_8 */


int
flac_meta_delete(file_decoder_t * fdec, int del_vc) {

	FLAC__Metadata_SimpleIterator * iter;
	FLAC__bool ret;

	iter = FLAC__metadata_simple_iterator_new();
	if (iter == NULL) {
		return META_ERROR_NOMEM;
	}

	ret = FLAC__metadata_simple_iterator_init(iter, fdec->filename, false, false);
	if (!ret) {
		fprintf(stderr, "dec_flac.c/flac_meta_delete: error: %s\n",
			FLAC__Metadata_SimpleIteratorStatusString[
			  FLAC__metadata_simple_iterator_status(iter)]);

		FLAC__metadata_simple_iterator_delete(iter);
		return META_ERROR_INTERNAL;
	}

	if (FLAC__metadata_simple_iterator_is_writable(iter) == false) {
		fprintf(stderr, "error: FLAC meta not writable!\n");
		FLAC__metadata_simple_iterator_delete(iter);
		return META_ERROR_NOT_WRITABLE;
	}

	do {
		switch (FLAC__metadata_simple_iterator_get_block_type(iter)) {
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			if (!del_vc)
				break;
			ret = FLAC__metadata_simple_iterator_delete_block(iter, true);
			if (ret == false) {
				fprintf(stderr, "error: FLAC metadata delete failed!\n");
				FLAC__metadata_simple_iterator_delete(iter);
				return META_ERROR_INTERNAL;
			}
			break;
#ifdef HAVE_FLAC_8
		case FLAC__METADATA_TYPE_PICTURE:
			ret = FLAC__metadata_simple_iterator_delete_block(iter, true);
			if (ret == false) {
				fprintf(stderr, "error: FLAC metadata delete failed!\n");
				FLAC__metadata_simple_iterator_delete(iter);
				return META_ERROR_INTERNAL;
			}
			break;
#endif /* HAVE_FLAC_8 */
		default:
			break;
		}
	} while (FLAC__metadata_simple_iterator_next(iter));
	FLAC__metadata_simple_iterator_delete(iter);
	return META_ERROR_NONE;
}

int
flac_write_metadata(file_decoder_t * fdec, metadata_t * meta) {

	int ret;
	int del_vc = 0;

	if (metadata_get_frame_by_tag(meta, META_TAG_OXC, NULL) == NULL) {
		/* no Ogg Xiph comment in this metablock -- remove it from file */
		del_vc = 1;
	}
	ret = flac_meta_delete(fdec, del_vc);
	if (ret != META_ERROR_NONE) {
		return ret;
	}

	if (!del_vc) {
		FLAC__StreamMetadata * smeta = metadata_to_flac_streammeta(meta);
		ret = flac_meta_vc_replace_or_append(fdec, smeta);
		if (ret != META_ERROR_NONE) {
			return ret;
		}
		FLAC__metadata_object_delete(smeta);
	}

#ifdef HAVE_FLAC_8
	if (metadata_get_frame_by_tag(meta, META_TAG_FLAC_APIC, NULL) != NULL) {
		ret = flac_meta_append_pics(fdec, meta);
		if (ret != META_ERROR_NONE) {
			return ret;
		}
	}
#endif /* HAVE_FLAC_8 */

	return META_ERROR_NONE;
}

void
flac_send_metadata(decoder_t * dec) {

	file_decoder_t * fdec = dec->fdec;
	metadata_t * meta;
	int found = 0;
	int writable;
	FLAC__Metadata_SimpleIterator * iter;
	FLAC__bool ret;

	iter = FLAC__metadata_simple_iterator_new();
	if (iter == NULL) {
		return;
	}

	writable = (access(fdec->filename, R_OK | W_OK) == 0) ? 1 : 0;
	ret = FLAC__metadata_simple_iterator_init(iter, fdec->filename, writable ? false : true, false);
	if (!ret) {
		fprintf(stderr, "dec_flac.c/flac_send_metadata: error: %s\n",
			FLAC__Metadata_SimpleIteratorStatusString[
			  FLAC__metadata_simple_iterator_status(iter)]);

		FLAC__metadata_simple_iterator_delete(iter);
		return;
	}

	meta = metadata_new();
	meta->fdec = fdec;
	fdec->meta = meta;
#ifdef HAVE_FLAC_8
	meta->valid_tags = META_TAG_OXC | META_TAG_FLAC_APIC;
#else
	meta->valid_tags = META_TAG_OXC;
#endif /* HAVE_FLAC_8 */
	if (writable && FLAC__metadata_simple_iterator_is_writable(iter) == true) {
		meta->writable = 1;
		fdec->meta_write = flac_write_metadata;
	}

	do {
		FLAC__StreamMetadata * smeta;
		switch (FLAC__metadata_simple_iterator_get_block_type(iter)) {
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			smeta = FLAC__metadata_simple_iterator_get_block(iter);
			FLAC__StreamMetadata_VorbisComment vc =
				FLAC__metadata_simple_iterator_get_block(iter)->data.vorbis_comment;
			metadata_from_flac_streammeta_vc(meta, &vc);
			found = 1;

			FLAC__metadata_object_delete(smeta);
			break;
#ifdef HAVE_FLAC_8
		case FLAC__METADATA_TYPE_PICTURE:
			smeta = FLAC__metadata_simple_iterator_get_block(iter);
			FLAC__StreamMetadata_Picture pic =
				FLAC__metadata_simple_iterator_get_block(iter)->data.picture;
			metadata_from_flac_streammeta_pic(meta, &pic);
			found = 1;

			FLAC__metadata_object_delete(smeta);
			break;
#endif /* HAVE_FLAC_8 */
		default:
			break;
		}
	} while (FLAC__metadata_simple_iterator_next(iter));
	FLAC__metadata_simple_iterator_delete(iter);

        if (!found && !meta->writable) {
                fdec->meta = NULL;
                metadata_free(meta);
        }
}
#endif /* HAVE_FLAC */


#ifdef HAVE_FLAC_8
int
flac_decoder_open(decoder_t * dec, char * filename) {

	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	int tried_flac = 0;

 try_flac:
	pd->error = 0;
	pd->flac_decoder = FLAC__stream_decoder_new();

	if ((pd->state = FLAC__stream_decoder_init_file(pd->flac_decoder, filename,
							write_callback, metadata_callback,
							error_callback, (void *)dec))
	    != FLAC__STREAM_DECODER_INIT_STATUS_OK) {

		FLAC__stream_decoder_delete(pd->flac_decoder);
		return DECODER_OPEN_FERROR;
	}

	FLAC__stream_decoder_process_until_end_of_metadata(pd->flac_decoder);

	if ((!pd->error) && (pd->channels > 0)) {
		if ((pd->channels != 1) && (pd->channels != 2)) {
			fprintf(stderr,
				"flac_decoder_open: FLAC file with %d channels is "
				"unsupported\n", pd->channels);
			return DECODER_OPEN_FERROR;
		} else {
			if (!tried_flac) {
				/* we need a real read test (some MP3's get to this point) */
				pd->probing = 1;
				FLAC__stream_decoder_process_single(pd->flac_decoder);
				pd->state = FLAC__stream_decoder_get_state(pd->flac_decoder);

				if ((pd->state != FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC) &&
				    (pd->state != FLAC__STREAM_DECODER_SEARCH_FOR_METADATA) &&
				    (pd->state != FLAC__STREAM_DECODER_READ_METADATA) &&
				    (pd->state != FLAC__STREAM_DECODER_READ_FRAME)) {
					return DECODER_OPEN_BADLIB;
				}

				pd->probing = 0;
				tried_flac = 1;
				FLAC__stream_decoder_finish(pd->flac_decoder);
				FLAC__stream_decoder_delete(pd->flac_decoder);
				goto try_flac;
			}

			pd->rb = rb_create(pd->channels * sample_size * RB_FLAC_SIZE);

			fdec->fileinfo.channels = pd->channels;
			fdec->fileinfo.sample_rate = pd->SR;
			fdec->fileinfo.total_samples = pd->total_samples;
			fdec->fileinfo.bps = pd->bits_per_sample * fdec->fileinfo.sample_rate
				* fdec->fileinfo.channels;

			fdec->file_lib = FLAC_LIB;
			strcpy(dec->format_str, "FLAC");

			flac_send_metadata(dec);
			return DECODER_OPEN_SUCCESS;
		}
	} else {
		FLAC__stream_decoder_finish(pd->flac_decoder);
		FLAC__stream_decoder_delete(pd->flac_decoder);

		return DECODER_OPEN_BADLIB;
	}
}
#endif /* HAVE_FLAC_8 */


#ifdef HAVE_FLAC_7
int
flac_decoder_open(decoder_t * dec, char * filename) {

	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	int tried_flac = 0;

 try_flac:
	pd->error = 0;
	pd->flac_decoder = FLAC__file_decoder_new();
	FLAC__file_decoder_set_client_data(pd->flac_decoder, (void *)dec);
	FLAC__file_decoder_set_write_callback(pd->flac_decoder, write_callback);
	FLAC__file_decoder_set_metadata_callback(pd->flac_decoder, metadata_callback);
	FLAC__file_decoder_set_error_callback(pd->flac_decoder, error_callback);
	FLAC__file_decoder_set_filename(pd->flac_decoder, filename);

	if (FLAC__file_decoder_init(pd->flac_decoder)) {
		FLAC__file_decoder_delete(pd->flac_decoder);
		return DECODER_OPEN_FERROR;
	}

	FLAC__file_decoder_process_until_end_of_metadata(pd->flac_decoder);
	if ((!pd->error) && (pd->channels > 0)) {
		if ((pd->channels != 1) && (pd->channels != 2)) {
			fprintf(stderr,
				"flac_decoder_open: FLAC file with %d channels is "
				"unsupported\n", pd->channels);
			return DECODER_OPEN_FERROR;
		} else {
			if (!tried_flac) {
				/* we need a real read test (some MP3's get to this point) */
				pd->probing = 1;
				FLAC__file_decoder_process_single(pd->flac_decoder);
				pd->state = FLAC__file_decoder_get_state(pd->flac_decoder);

				if ((pd->state != FLAC__FILE_DECODER_OK) &&
				    (pd->state != FLAC__FILE_DECODER_END_OF_FILE)) {
					return DECODER_OPEN_BADLIB;
				}

				pd->probing = 0;
				tried_flac = 1;
				FLAC__file_decoder_finish(pd->flac_decoder);
				FLAC__file_decoder_delete(pd->flac_decoder);
				goto try_flac;
			}

			pd->rb = rb_create(pd->channels * sample_size * RB_FLAC_SIZE);
			fdec->fileinfo.channels = pd->channels;
			fdec->fileinfo.sample_rate = pd->SR;
			fdec->fileinfo.total_samples = pd->total_samples;
			fdec->fileinfo.bps = pd->bits_per_sample * fdec->fileinfo.sample_rate
				* fdec->fileinfo.channels;

			fdec->file_lib = FLAC_LIB;
			strcpy(dec->format_str, "FLAC");

			flac_send_metadata(dec);
			return DECODER_OPEN_SUCCESS;
		}
	} else {
		FLAC__file_decoder_finish(pd->flac_decoder);
		FLAC__file_decoder_delete(pd->flac_decoder);

		return DECODER_OPEN_BADLIB;
	}
}
#endif /* HAVE_FLAC_7 */


void
flac_decoder_send_metadata(decoder_t * dec) {

        file_decoder_t * fdec = dec->fdec;

        if (fdec->meta != NULL && fdec->meta_cb != NULL) {
                fdec->meta_cb(fdec->meta, fdec->meta_cbdata);
        }
}


#ifdef HAVE_FLAC_8
void
flac_decoder_close(decoder_t * dec) {

	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;

	FLAC__stream_decoder_finish(pd->flac_decoder);
	FLAC__stream_decoder_delete(pd->flac_decoder);
	rb_free(pd->rb);
}
#endif /* HAVE_FLAC_8 */


#ifdef HAVE_FLAC_7
void
flac_decoder_close(decoder_t * dec) {

	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;

	FLAC__file_decoder_finish(pd->flac_decoder);
	FLAC__file_decoder_delete(pd->flac_decoder);
	rb_free(pd->rb);
}
#endif /* HAVE_FLAC_7 */


#ifdef HAVE_FLAC_8
unsigned int
flac_decoder_read(decoder_t * dec, float * dest, int num) {

	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;

	unsigned int numread = 0;
	unsigned int n_avail = 0;

	pd->state = FLAC__stream_decoder_get_state(pd->flac_decoder);
	while ((rb_read_space(pd->rb) < num * pd->channels * sample_size) &&
	       (pd->state == FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC ||
		pd->state == FLAC__STREAM_DECODER_SEARCH_FOR_METADATA ||
		pd->state == FLAC__STREAM_DECODER_READ_METADATA ||
		pd->state == FLAC__STREAM_DECODER_READ_FRAME)) {
		FLAC__stream_decoder_process_single(pd->flac_decoder);
		pd->state = FLAC__stream_decoder_get_state(pd->flac_decoder);
	}

	/* Have i chosen the right conditions? */
	if ((pd->state != FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC) &&
	    (pd->state != FLAC__STREAM_DECODER_SEARCH_FOR_METADATA) &&
	    (pd->state != FLAC__STREAM_DECODER_READ_METADATA) &&
	    (pd->state != FLAC__STREAM_DECODER_READ_FRAME) &&
	    (pd->state != FLAC__STREAM_DECODER_END_OF_STREAM)) {
		fprintf(stderr, "file_decoder_read() / FLAC: decoder error: %s\n",
			FLAC__StreamDecoderStateString[pd->state]);
		return 0; /* this means that a new file will be opened */
	}

	n_avail = rb_read_space(pd->rb) /
		(pd->channels * sample_size);

	if (n_avail > num) {
		n_avail = num;
	}

	rb_read(pd->rb, (char *)dest, n_avail * pd->channels * sample_size);
	numread = n_avail;
	return numread;
}
#endif /* HAVE_FLAC_8 */


#ifdef HAVE_FLAC_7
unsigned int
flac_decoder_read(decoder_t * dec, float * dest, int num) {

	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;

	unsigned int numread = 0;
	unsigned int n_avail = 0;


	pd->state = FLAC__file_decoder_get_state(pd->flac_decoder);
	while ((rb_read_space(pd->rb) < num * pd->channels
		* sample_size) && (pd->state == FLAC__FILE_DECODER_OK)) {
		FLAC__file_decoder_process_single(pd->flac_decoder);
		pd->state = FLAC__file_decoder_get_state(pd->flac_decoder);
	}

	if ((pd->state != FLAC__FILE_DECODER_OK) &&
	    (pd->state != FLAC__FILE_DECODER_END_OF_FILE)) {
		fprintf(stderr, "file_decoder_read() / FLAC: decoder error: %s\n",
			FLAC__FileDecoderStateString[pd->state]);
		return 0; /* this means that a new file will be opened */
	}

	n_avail = rb_read_space(pd->rb) /
		(pd->channels * sample_size);

	if (n_avail > num) {
		n_avail = num;
	}

	rb_read(pd->rb, (char *)dest, n_avail *
			     pd->channels * sample_size);
	numread = n_avail;
	return numread;
}
#endif /* HAVE_FLAC_7 */


#ifdef HAVE_FLAC_8
void
flac_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {

	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	char flush_dest;


	if (seek_to_pos == fdec->fileinfo.total_samples) {
		--seek_to_pos;
	}

	if (FLAC__stream_decoder_seek_absolute(pd->flac_decoder, seek_to_pos)) {
		fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;

		/* empty flac decoder ringbuffer */
		while (rb_read_space(pd->rb))
			rb_read(pd->rb, &flush_dest, sizeof(char));
	} else {
		fprintf(stderr, "flac_decoder_seek: warning: "
			"FLAC__file_decoder_seek_absolute() failed\n");
	}
}
#endif /* HAVE_FLAC_8 */


#ifdef HAVE_FLAC_7
void
flac_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {

	flac_pdata_t * pd = (flac_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	char flush_dest;


	if (seek_to_pos == fdec->fileinfo.total_samples) {
		--seek_to_pos;
	}

	if (FLAC__file_decoder_seek_absolute(pd->flac_decoder, seek_to_pos)) {
		fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;

		/* empty flac decoder ringbuffer */
		while (rb_read_space(pd->rb))
			rb_read(pd->rb, &flush_dest, sizeof(char));
	} else {
		fprintf(stderr, "stream_decoder_seek: warning: "
			"FLAC__stream_decoder_seek_absolute() failed\n");
	}
}
#endif /* HAVE_FLAC_7 */


#ifndef HAVE_FLAC
decoder_t *
flac_decoder_init(file_decoder_t * fdec) {

        return NULL;
}
#endif /* !HAVE_FLAC */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :
