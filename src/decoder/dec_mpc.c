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
#include <unistd.h>

#include "../i18n.h"
#include "../metadata_ape.h"
#include "dec_mpc.h"


extern size_t sample_size;


/* return 1 if reached end of stream, 0 else */
int
decode_mpc(decoder_t * dec) {

	mpc_pdata_t * pd = (mpc_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;

	int n;
	float fval;
        MPC_SAMPLE_FORMAT buffer[MPC_DECODER_BUFFER_LENGTH];


#ifdef MPC_OLD_API
        pd->status = mpc_decoder_decode(&pd->mpc_d, buffer, NULL, NULL);
	if (pd->status == (unsigned)(-1)) {
		fprintf(stderr, "decode_mpc: mpc decoder reported an error\n");
		return 1; /* ignore the rest of the stream */
	} else if (pd->status == 0) {
		return 1; /* end of stream */
	}
#else
	mpc_frame_info frame;
	mpc_status err;

	frame.buffer = buffer;
	err = mpc_demux_decode(pd->mpc_d, &frame);
	if (err != MPC_STATUS_OK) {
		fprintf(stderr, "decode_mpc: mpc decoder reported an error\n");
		return 1; /* ignore the rest of the stream */
	} else if (frame.bits == -1) {
		return 1; /* end of stream */
	}
	pd->status = frame.samples;
#endif /* MPC_OLD_API */
	
	for (n = 0; n < pd->status * pd->mpc_i.channels; n++) {
#ifdef MPC_FIXED_POINT
                fval = buffer[n] / (double)MPC_FIXED_POINT_SCALE * fdec->voladj_lin;
#else
                fval = buffer[n] * fdec->voladj_lin;
#endif /* MPC_FIXED_POINT */
		
                if (fval < -1.0f) {
                        fval = -1.0f;
                } else if (fval > 1.0f) {
                        fval = 1.0f;
                }
		
                rb_write(pd->rb, (char *)&fval, sample_size);
	}
	return 0;
}


decoder_t *
mpc_decoder_init_func(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_mpc.c: mpc_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(mpc_pdata_t))) == NULL) {
                fprintf(stderr, "dec_mpc.c: mpc_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->init = mpc_decoder_init_func;
	dec->destroy = mpc_decoder_destroy;
	dec->open = mpc_decoder_open;
	dec->send_metadata = mpc_decoder_send_metadata;
	dec->close = mpc_decoder_close;
	dec->read = mpc_decoder_read;
	dec->seek = mpc_decoder_seek;

	return dec;
}


void
mpc_decoder_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


void
mpc_add_rg_frame(metadata_t * meta, int type, float fval) {

	meta_frame_t * frame = meta_frame_new();
	char * str;

	if (frame == NULL) {
		return;
	}

	frame->tag = META_TAG_MPC_RGDATA;
	frame->type = type;
	frame->flags = META_FIELD_UNIQUE | META_FIELD_MANDATORY;
	meta_get_fieldname(type, &str);
	frame->field_name = strdup(str);
	frame->float_val = fval;
	metadata_add_frame(meta, frame);
}


void
mpc_add_rg_meta(metadata_t * meta, mpc_streaminfo * si) {

#ifdef MPC_OLD_API
	float track_gain = si->gain_title / 100.0;
	float album_gain = si->gain_album / 100.0;
	float track_peak = si->peak_title / 32768.0;
	float album_peak = si->peak_album / 32768.0;
#else
	/* These conversions are inverted from streaminfo_read_header_sv7()
	 * from the following file on top of what aqualung does for OLD_API:
	 * http://svn.musepack.net/libmpc/trunk/libmpcdec/streaminfo.c
	 */
	float track_gain = MPC_OLD_GAIN_REF - (si->gain_title / 256.0);
	float album_gain = MPC_OLD_GAIN_REF - (si->gain_album / 256.0);
	float track_peak = pow(10, si->peak_title / 256.0 / 20.0) / 32768.0;
	float album_peak = pow(10, si->peak_album / 256.0 / 20.0) / 32768.0;
#endif

	if (meta == NULL) {
		return;
	}

	/* XXX TODO FIXME:
	 * This is commented just yet so as not to offer this tag
	 * for creation since we don't support saving it anyway.
	 */
	/* meta->valid_tags |= META_TAG_MPC_RGDATA; */

	if ((track_gain == 0.0f) && (album_gain == 0.0f) &&
	    (track_peak == 0.0f) && (album_peak == 0.0f)) {
		return;
	}

	mpc_add_rg_frame(meta, META_FIELD_RG_TRACK_GAIN, track_gain);
	mpc_add_rg_frame(meta, META_FIELD_RG_ALBUM_GAIN, album_gain);
	mpc_add_rg_frame(meta, META_FIELD_RG_TRACK_PEAK, track_peak);
	mpc_add_rg_frame(meta, META_FIELD_RG_ALBUM_PEAK, album_peak);
}


int
mpc_decoder_open(decoder_t * dec, char * filename) {

	mpc_pdata_t * pd = (mpc_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	metadata_t * meta;
	
	
	if ((pd->mpc_file = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "mpc_decoder_open: fopen() failed for Musepack file\n");
		return DECODER_OPEN_FERROR;
	}
	pd->seekable = 1;
	fseek(pd->mpc_file, 0, SEEK_END);
	pd->size = ftell(pd->mpc_file);
	fseek(pd->mpc_file, 0, SEEK_SET);
	
#ifdef MPC_OLD_API
	mpc_reader_setup_file_reader(&pd->mpc_r_f, pd->mpc_file);
	
	mpc_streaminfo_init(&pd->mpc_i);
	if (mpc_streaminfo_read(&pd->mpc_i, &pd->mpc_r_f.reader) != ERROR_CODE_OK) {
		fclose(pd->mpc_file);
		return DECODER_OPEN_BADLIB;
	}
	
	mpc_decoder_setup(&pd->mpc_d, &pd->mpc_r_f.reader);
	if (!mpc_decoder_initialize(&pd->mpc_d, &pd->mpc_i)) {
		fclose(pd->mpc_file);
		return DECODER_OPEN_BADLIB;
	}
#else
	mpc_reader_init_stdio_stream(&pd->mpc_r_f, pd->mpc_file);

	pd->mpc_d = mpc_demux_init(&pd->mpc_r_f);
	if (!pd->mpc_d) {
		fclose(pd->mpc_file);
		return DECODER_OPEN_BADLIB;
	}
	mpc_demux_get_info(pd->mpc_d, &pd->mpc_i);
#endif /* MPC_OLD_API */
	
	pd->is_eos = 0;
	pd->rb = rb_create(pd->mpc_i.channels * sample_size * RB_MPC_SIZE);
	
	fdec->fileinfo.channels = pd->mpc_i.channels;
	fdec->fileinfo.sample_rate = pd->mpc_i.sample_freq;
	fdec->fileinfo.total_samples = mpc_streaminfo_get_length_samples(&pd->mpc_i);
	fdec->fileinfo.bps = pd->mpc_i.average_bitrate;

	fdec->file_lib = MPC_LIB;
	strcpy(dec->format_str, "Musepack");

#ifdef MPC_OLD_API
	switch (pd->mpc_i.profile) {
#else
	switch ((int) pd->mpc_i.profile) {
#endif /* MPC_OLD_API */
	case 7:
		sprintf(dec->format_str, "%s (%s)", dec->format_str, _("Profile: Telephone"));
		break;
	case 8:
		sprintf(dec->format_str, "%s (%s)", dec->format_str, _("Profile: Thumb"));
		break;
	case 9:
		sprintf(dec->format_str, "%s (%s)", dec->format_str, _("Profile: Radio"));
		break;
	case 10:
		sprintf(dec->format_str, "%s (%s)", dec->format_str, _("Profile: Standard"));
		break;
	case 11:
		sprintf(dec->format_str, "%s (%s)", dec->format_str, _("Profile: Xtreme"));
		break;
	case 12:
		sprintf(dec->format_str, "%s (%s)", dec->format_str, _("Profile: Insane"));
		break;
	case 13:
		sprintf(dec->format_str, "%s (%s)", dec->format_str, _("Profile: Braindead"));
		break;
	}

	meta = metadata_new();
	mpc_add_rg_meta(meta, &pd->mpc_i);
	meta_ape_send_metadata(meta, fdec);
	return DECODER_OPEN_SUCCESS;
}


void
mpc_decoder_send_metadata(decoder_t * dec) {
}


void
mpc_decoder_close(decoder_t * dec) {

	mpc_pdata_t * pd = (mpc_pdata_t *)dec->pdata;

	rb_free(pd->rb);
	fclose(pd->mpc_file);
}


unsigned int
mpc_decoder_read(decoder_t * dec, float * dest, int num) {

	mpc_pdata_t * pd = (mpc_pdata_t *)dec->pdata;

	unsigned int numread = 0;
	unsigned int n_avail = 0;


	while ((rb_read_space(pd->rb) <
		num * pd->mpc_i.channels * sample_size) && (!pd->is_eos)) {

		pd->is_eos = decode_mpc(dec);
	}

	n_avail = rb_read_space(pd->rb) / (pd->mpc_i.channels * sample_size);

	if (n_avail > num)
		n_avail = num;

	rb_read(pd->rb, (char *)dest, n_avail *
			     pd->mpc_i.channels * sample_size);

	numread = n_avail;
	return numread;
}


void
mpc_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {
	
	mpc_pdata_t * pd = (mpc_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	char flush_dest;


#ifdef MPC_OLD_API
	if (mpc_decoder_seek_sample(&pd->mpc_d, seek_to_pos)) {
#else
	if (mpc_demux_seek_sample(pd->mpc_d, seek_to_pos) == MPC_STATUS_OK) {
#endif /* MPC_OLD_API */
		fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
		/* empty musepack decoder ringbuffer */
		while (rb_read_space(pd->rb))
			rb_read(pd->rb, &flush_dest, sizeof(char));
	} else {
		fprintf(stderr,
			"mpc_decoder_seek: warning: mpc_decoder_seek_sample() failed\n");
	}
}



// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

