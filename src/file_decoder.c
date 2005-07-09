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
#include <pthread.h>
#include <sys/stat.h>
#include <gtk/gtk.h>

#include <jack/jack.h>
#include <jack/ringbuffer.h>

#ifdef HAVE_SRC
#include <samplerate.h>
#endif /* HAVE_SRC */


#include "core.h"
#include "file_decoder.h"


extern size_t sample_size;


#ifdef HAVE_FLAC
/* FLAC write callback */
FLAC__StreamDecoderWriteStatus
write_callback(const FLAC__FileDecoder * decoder,
               const FLAC__Frame * frame,
               const FLAC__int32 * const buffer[],
               void * client_data) {

	file_decoder_t * fdec = (file_decoder_t *) client_data;

	if (fdec->flac_probing)
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

        fdec->flac_blocksize = frame->header.blocksize;
        fdec->flac_scale = 1 << (fdec->flac_bits_per_sample - 1);

        for (fdec->flac_i = 0; fdec->flac_i < fdec->flac_blocksize; fdec->flac_i++) {
		for (fdec->flac_j = 0; fdec->flac_j < fdec->flac_channels; fdec->flac_j++) {
			fdec->flac_buf[fdec->flac_j] = *(buffer[fdec->flac_j] + fdec->flac_i);
			fdec->flac_fbuf[fdec->flac_j] = (float)fdec->flac_buf[fdec->flac_j]
				* fdec->voladj_lin / fdec->flac_scale;
		}
		jack_ringbuffer_write(fdec->rb_flac, (char *)fdec->flac_fbuf,
				      fdec->flac_channels * sample_size);
        }

        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


/* FLAC metadata callback */
void
metadata_callback(const FLAC__FileDecoder * decoder,
                  const FLAC__StreamMetadata * metadata,
                  void * client_data) {

	file_decoder_t * fdec = (file_decoder_t *) client_data;

        if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
                fdec->flac_SR = metadata->data.stream_info.sample_rate;
                fdec->flac_bits_per_sample = metadata->data.stream_info.bits_per_sample;
                fdec->flac_channels = metadata->data.stream_info.channels;
		fdec->flac_total_samples = metadata->data.stream_info.total_samples;
        } else
                fprintf(stderr, "FLAC metadata callback: ignoring unexpected header\n");
}


/* FLAC error callback */
void
error_callback(const FLAC__FileDecoder * decoder,
               FLAC__StreamDecoderErrorStatus status,
               void * client_data) {

	file_decoder_t * fdec = (file_decoder_t *) client_data;
        fdec->flac_error = 1;
}
#endif /* HAVE_FLAC */


#ifdef HAVE_OGG_VORBIS
/* Ogg Vorbis: return 1 if reached end of stream, 0 else */
int
decode_vorbis(file_decoder_t * fdec) {
	
	if ((fdec->oggv_bytes_read = ov_read(&(fdec->vf), fdec->oggv_buffer, VORBIS_BUFSIZE,
					     0, 2, 1, &(fdec->oggv_current_section))) > 0) {
                for (fdec->oggv_i = 0; fdec->oggv_i < fdec->oggv_bytes_read/2; fdec->oggv_i++)
                        fdec->oggv_fbuffer[fdec->oggv_i] =
				*((short *)(fdec->oggv_buffer + 2*fdec->oggv_i))
				* fdec->voladj_lin / 32768.f;
                jack_ringbuffer_write(fdec->rb_vorbis, (char *)fdec->oggv_fbuffer,
				      fdec->oggv_bytes_read/2 * sample_size);
		return 0;
        } else
		return 1;
}
#endif /* HAVE_OGG_VORBIS */


#ifdef HAVE_MPEG
/* MPEG Audio decoder input callback for exploring */
/*
static
enum mad_flow
mpeg_input_explore(void * data, struct mad_stream * stream) {

	file_decoder_t * fdec = (file_decoder_t *) data;

	if (fdec->mpeg_again)
		return MAD_FLOW_STOP;

	fdec->mpeg_again = 1;

	fseek(fdec->mpeg_file, MAD_BUFSIZE, SEEK_SET);
	if ((fdec->mpeg_n_read = fread(fdec->mpeg_file_data, sizeof(char),
				       MAD_BUFSIZE, fdec->mpeg_file)) != MAD_BUFSIZE) {
		fdec->mpeg_EOS = 1;
		fclose(fdec->mpeg_file);
	}

	if (fdec->mpeg_n_read) {
		mad_stream_buffer(stream, fdec->mpeg_file_data, fdec->mpeg_n_read);
	}
	
	return MAD_FLOW_CONTINUE;
}
*/

/* MPEG Audio decoder output callback for exploring */
/*
static
enum mad_flow
mpeg_output_explore(void * data, struct mad_header const * header, struct mad_pcm * pcm) {

	file_decoder_t * fdec = (file_decoder_t *) data;

	fdec->mpeg_channels = pcm->channels;
	fdec->mpeg_SR = pcm->samplerate;
	return MAD_FLOW_STOP;
}
*/

/* MPEG Audio decoder header callback for exploring */
/*
static
enum mad_flow
mpeg_header_explore(void * data, struct mad_header const * header) {

	file_decoder_t * fdec = (file_decoder_t *) data;

	fdec->mpeg_bitrate = header->bitrate;

	fdec->mpeg_subformat = 0;
	switch (header->layer) {
	case MAD_LAYER_I:
		fdec->mpeg_subformat |= MPEG_LAYER_I;
		break;
	case MAD_LAYER_II:
		fdec->mpeg_subformat |= MPEG_LAYER_II;
		break;
	case MAD_LAYER_III:
		fdec->mpeg_subformat |= MPEG_LAYER_III;
		break;
	}

	switch (header->mode) {
	case MAD_MODE_SINGLE_CHANNEL:
		fdec->mpeg_subformat |= MPEG_MODE_SINGLE;
		break;
	case MAD_MODE_DUAL_CHANNEL:
		fdec->mpeg_subformat |= MPEG_MODE_DUAL;
		break;
	case MAD_MODE_JOINT_STEREO:
		fdec->mpeg_subformat |= MPEG_MODE_JOINT;
		break;
	case MAD_MODE_STEREO:
		fdec->mpeg_subformat |= MPEG_MODE_STEREO;
		break;
	}

	switch (header->emphasis) {
	case MAD_EMPHASIS_NONE:
		fdec->mpeg_subformat |= MPEG_EMPH_NONE;
		break;
	case MAD_EMPHASIS_50_15_US:
		fdec->mpeg_subformat |= MPEG_EMPH_5015;
		break;
	case MAD_EMPHASIS_CCITT_J_17:
		fdec->mpeg_subformat |= MPEG_EMPH_J_17;
		break;
	default:
		fdec->mpeg_subformat |= MPEG_EMPH_RES;
		break;
	}

	return MAD_FLOW_CONTINUE;
}
*/

/* MPEG Audio decoder error callback for exploring */
/*
static
enum mad_flow
mpeg_error_explore(void * data, struct mad_stream * stream, struct mad_frame * frame) {

	file_decoder_t * fdec = (file_decoder_t *) data;

	fdec->mpeg_err = 1;

	return MAD_FLOW_CONTINUE;
}
*/

/* MPEG Audio decoder input callback for playback */
static
enum mad_flow
mpeg_input(void * data, struct mad_stream * stream) {
	
	file_decoder_t * fdec = (file_decoder_t *) data;

	if (fstat(fdec->mpeg_fd, &(fdec->mpeg_stat)) == -1 || fdec->mpeg_stat.st_size == 0)
		return MAD_FLOW_STOP;
	
	fdec->mpeg_fdm = mmap(0, fdec->mpeg_stat.st_size, PROT_READ, MAP_SHARED, fdec->mpeg_fd, 0);
	if (fdec->mpeg_fdm == MAP_FAILED)
		return MAD_FLOW_STOP;
	
	mad_stream_buffer(stream, fdec->mpeg_fdm, fdec->mpeg_stat.st_size);

	return MAD_FLOW_CONTINUE;
}


/* MPEG Audio decoder output callback for playback */
static
enum mad_flow
mpeg_output(void * data, struct mad_header const * header, struct mad_pcm * pcm) {

	file_decoder_t * fdec = (file_decoder_t *) data;

	fdec->mpeg_scale = 322122547; /* (1 << 28) * 1.2 */

        for (fdec->mpeg_i = 0; fdec->mpeg_i < pcm->length; fdec->mpeg_i++) {
		for (fdec->mpeg_j = 0; fdec->mpeg_j < fdec->mpeg_channels; fdec->mpeg_j++) {
			fdec->mpeg_buf[fdec->mpeg_j] =
				fdec->mpeg_err ? 0 : *(pcm->samples[fdec->mpeg_j] + fdec->mpeg_i);
			fdec->mpeg_fbuf[fdec->mpeg_j] =
				(double)fdec->mpeg_buf[fdec->mpeg_j] *
				fdec->voladj_lin / fdec->mpeg_scale;
		}
		if (jack_ringbuffer_write_space(fdec->rb_mpeg) >=
		    fdec->mpeg_channels * sample_size)
			jack_ringbuffer_write(fdec->rb_mpeg, (char *)fdec->mpeg_fbuf,
					      fdec->mpeg_channels * sample_size);
        }
	fdec->mpeg_err = 0;

	return MAD_FLOW_CONTINUE;
}


/* MPEG Audio decoder header callback for playback */
static
enum mad_flow
mpeg_header(void * data, struct mad_header const * header) {

	file_decoder_t * fdec = (file_decoder_t *) data;

	fdec->mpeg_bitrate = header->bitrate;

	return MAD_FLOW_CONTINUE;
}


/* MPEG Audio decoder error callback for playback */
static
enum mad_flow
mpeg_error(void * data, struct mad_stream * stream, struct mad_frame * frame) {

	file_decoder_t * fdec = (file_decoder_t *) data;

	fdec->mpeg_err = 1;

	return MAD_FLOW_CONTINUE;
}


/* MPEG bitrate index tables */
int bri_V1_L1[15] = {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448};
int bri_V1_L2[15] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384};
int bri_V1_L3[15] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
int bri_V2_L1[15] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256};
int bri_V2_L23[15] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};

/* ret: 0 = valid MPEG file, >0: error */
int
mpeg_explore(file_decoder_t * fdec) {

	int buf = 0;
	int pos = 0;
	int byte1 = 0;
	int byte2 = 0;
	int byte3 = 0;
	int ver = 0;
	int layer = 0;
	int bri = 0; /* bitrate index */
	int padding = 0;
	int chmode = 0;
	int emph = 0;

	do {
		do {
			if (read(fdec->mpeg_exp_fd, &buf, 1) != 1) {
				return 1;
			}
			++pos;
			if (pos > 8192) {
				return 1;
			}
		} while (buf != 0xff);
		
		if (read(fdec->mpeg_exp_fd, &buf, 1) != 1) {
			return 1;
		}
		++pos;
	} while ((buf & 0xe0) != 0xe0);

	byte1 = buf;

	if (read(fdec->mpeg_exp_fd, &byte2, 1) != 1) {
		return 1;
	}
	++pos;
	
	if (read(fdec->mpeg_exp_fd, &byte3, 1) != 1) {
		return 1;
	}
	++pos;
	
	ver = (byte1 >> 3) & 0x3;
	
	layer = (byte1 >> 1) & 0x3;
	switch (layer) {
	case 0: return 1;
		break;
	case 1: fdec->mpeg_subformat |= MPEG_LAYER_III;
		break;
	case 2: fdec->mpeg_subformat |= MPEG_LAYER_II;
		break;
	case 3: fdec->mpeg_subformat |= MPEG_LAYER_I;
		break;
	}
	
	bri = (byte2 >> 4) & 0x0f;
	switch (bri) {
	case 0: fdec->mpeg_bitrate = 256000; /* XXX random value so it's not zero */
		printf("Warning: free-format MPEG detected. Be prepared for weird errors.\n");
		break;
	case 15:
		return 1;
		break;
	default:
		switch (layer) {
		case 3:
			switch (ver) {
			case 3:
				fdec->mpeg_bitrate = bri_V1_L1[bri] * 1000;
				break;
			case 0:
			case 2:
				fdec->mpeg_bitrate = bri_V2_L1[bri] * 1000;
				break;
			}
			break;
		case 2:
			switch (ver) {
			case 3:
				fdec->mpeg_bitrate = bri_V1_L2[bri] * 1000;
				break;
			case 0:
			case 2:
				fdec->mpeg_bitrate = bri_V2_L23[bri] * 1000;
				break;
			}
			break;
		case 1:
			switch (ver) {
			case 3:
				fdec->mpeg_bitrate = bri_V1_L3[bri] * 1000;
				break;
			case 0:
			case 2:
				fdec->mpeg_bitrate = bri_V2_L23[bri] * 1000;
				break;
			}
			break;
		}
		break;
	}
	
	
	switch ((byte2 >> 2) & 0x03) {
	case 0:
		switch (ver) {
		case 0: fdec->mpeg_SR = 11025;
			break;
		case 2: fdec->mpeg_SR = 22050;
			break;
		case 3: fdec->mpeg_SR = 44100;
			break;
		}
		break;
	case 1:
		switch (ver) {
		case 0: fdec->mpeg_SR = 12000;
			break;
		case 2: fdec->mpeg_SR = 24000;
			break;
		case 3: fdec->mpeg_SR = 48000;
			break;
		}
		break;
	case 2:
		switch (ver) {
		case 0: fdec->mpeg_SR = 8000;
			break;
		case 2: fdec->mpeg_SR = 16000;
			break;
		case 3: fdec->mpeg_SR = 32000;
			break;
		}
		break;
	case 3:
		return 1;
		break;
	}
	
	padding = (byte2 >> 1) & 0x01;
	
	chmode = (byte3 >> 6) & 0x03;
	switch (chmode) {
	case 0: fdec->mpeg_subformat |= MPEG_MODE_STEREO;
		fdec->mpeg_channels = 2;
		break;
	case 1: fdec->mpeg_subformat |= MPEG_MODE_JOINT;
		fdec->mpeg_channels = 2;
		break;
	case 2: fdec->mpeg_subformat |= MPEG_MODE_DUAL;
		fdec->mpeg_channels = 2;
		break;
	case 3: fdec->mpeg_subformat |= MPEG_MODE_SINGLE;
		fdec->mpeg_channels = 1;
		break;
	}
	
	emph = byte3 & 0x03;
	switch (emph) {
	case 0: fdec->mpeg_subformat |= MPEG_EMPH_NONE;
		break;
	case 1: fdec->mpeg_subformat |= MPEG_EMPH_5015;
		break;
	case 2: fdec->mpeg_subformat |= MPEG_EMPH_RES;
		break;
	case 3: fdec->mpeg_subformat |= MPEG_EMPH_J_17;
		break;
	}

	return 0;
}


/* Main MPEG Audio decode loop: return 1 if reached end of stream, 0 else */
int
decode_mpeg(file_decoder_t * fdec) {

	if (mad_header_decode(&(fdec->mpeg_frame.header), &(fdec->mpeg_stream)) == -1) {
		if (fdec->mpeg_stream.error == MAD_ERROR_BUFLEN)
		        return 1;

		if (!MAD_RECOVERABLE(fdec->mpeg_stream.error))
			fprintf(stderr, "libMAD: unrecoverable error in MPEG Audio stream\n");

		mpeg_error((void *)fdec, &(fdec->mpeg_stream), &(fdec->mpeg_frame));
	}
	mpeg_header((void *)fdec, &(fdec->mpeg_frame.header));

	if (mad_frame_decode(&(fdec->mpeg_frame), &(fdec->mpeg_stream)) == -1) {
		if (fdec->mpeg_stream.error == MAD_ERROR_BUFLEN)
			return 1;

		if (!MAD_RECOVERABLE(fdec->mpeg_stream.error))
			fprintf(stderr, "libMAD: unrecoverable error in MPEG Audio stream\n");

		mpeg_error((void *)fdec, &(fdec->mpeg_stream), &(fdec->mpeg_frame));
	}

	mad_synth_frame(&(fdec->mpeg_synth), &(fdec->mpeg_frame));
	mpeg_output((void *)fdec, &(fdec->mpeg_frame.header), &(fdec->mpeg_synth.pcm));
	
	return 0;
}
#endif /* HAVE_MPEG */


#ifdef HAVE_MOD
/* MOD decoder: return 1 if reached end of stream, 0 else */
int
decode_mod(file_decoder_t * fdec) {

        if ((fdec->mod_bytes_read = ModPlug_Read(fdec->mpf, fdec->mod_buffer, MOD_BUFSIZE)) > 0) {
                for (fdec->mod_i = 0; fdec->mod_i < fdec->mod_bytes_read/2; fdec->mod_i++)
                        fdec->mod_fbuffer[fdec->mod_i] =
				*((short *)(fdec->mod_buffer + 2*fdec->mod_i)) *
				fdec->voladj_lin / 32768.f;
                jack_ringbuffer_write(fdec->rb_mod, (char *)fdec->mod_fbuffer,
				      fdec->mod_bytes_read/2 * sample_size);
                return 0;
        } else
                return 1;
}
#endif /* HAVE_MOD */


#ifdef HAVE_MPC
/* Musepack decoder: return 1 if reached end of stream, 0 else */
int
decode_mpc(file_decoder_t * fdec) {

	fdec->mpc_status = mpc_decoder_decode(&fdec->mpc_d, fdec->mpc_sample_buffer, NULL, NULL);
        if (fdec->mpc_status == (unsigned)(-1)) {
		fprintf(stderr, "file_decoder: decode_mpc reported mpc decoder error\n");
		return 1; /* ignore the rest of the stream */
        } else if (fdec->mpc_status == 0) {
		return 1; /* end of stream */
        }

        for (fdec->mpc_n = 0; fdec->mpc_n < fdec->mpc_status * fdec->mpc_i.channels;
	     fdec->mpc_n++) {
#ifdef MPC_FIXED_POINT
		fdec->mpc_fval = fdec->mpc_sample_buffer[fdec->mpc_n]
			/ (double)MPC_FIXED_POINT_SCALE;
#else
		fdec->mpc_fval = fdec->mpc_sample_buffer[fdec->mpc_n];
#endif /* MPC_FIXED_POINT */

		if (fdec->mpc_fval < -1.0f) {
			fdec->mpc_fval = -1.0f;
		} else if (fdec->mpc_fval > 1.0f) {
			fdec->mpc_fval = 1.0f;
		}

                jack_ringbuffer_write(fdec->rb_mpc, (char *)&fdec->mpc_fval, sample_size);
        }
	return 0;
}
#endif /* HAVE_MPC */


file_decoder_t * file_decoder_new(void) {

	file_decoder_t * fdec;
	
	if ((fdec = calloc(1, sizeof(file_decoder_t))) == NULL) {
		fprintf(stderr, "file_decoder.c: file_decoder_new() failed: calloc error\n");
		return NULL;
	}
	
	fdec->file_open = 0;

	fdec->voladj_db = 0.0f;
	fdec->voladj_lin = 1.0f;

	return fdec;
}


void file_decoder_delete(file_decoder_t * fdec) {
	
	if (fdec->file_open) {
		file_decoder_close(fdec);
	}

	free(fdec);
}


/* return: 0 is OK, >0 is error */
int file_decoder_open(file_decoder_t * fdec, char * filename, unsigned int out_SR) {

#ifdef HAVE_FLAC
	fdec->tried_flac = 0;
#endif /* HAVE_FLAC */
	fdec->file_lib = 0;

#ifdef HAVE_SNDFILE
	/* try opening with libsndfile */
	fdec->sf_info.format = 0;
	if ((fdec->sf = sf_open(filename, SFM_READ, &(fdec->sf_info))) != NULL) {
		if ((fdec->sf_info.channels != 1) && (fdec->sf_info.channels != 2)) {
			fprintf(stderr,
				"file_decoder_open: sndfile with %d channels is unsupported\n",
				fdec->sf_info.channels);
			goto no_open;
		}
		fdec->channels = fdec->sf_info.channels;
		fdec->SR = fdec->sf_info.samplerate;
		fdec->file_lib = SNDFILE_LIB;

		fdec->fileinfo.total_samples = fdec->sf_info.frames;
		fdec->fileinfo.format_major = fdec->sf_info.format & SF_FORMAT_TYPEMASK;
		fdec->fileinfo.format_minor = fdec->sf_info.format & SF_FORMAT_SUBMASK;

		switch (fdec->fileinfo.format_minor) {
		case SF_FORMAT_PCM_S8:
		case SF_FORMAT_PCM_U8:
			fdec->fileinfo.bps = 8;
			break;
		case SF_FORMAT_PCM_16:
			fdec->fileinfo.bps = 16;
			break;
		case SF_FORMAT_PCM_24:
			fdec->fileinfo.bps = 24;
			break;
		case SF_FORMAT_PCM_32:
		case SF_FORMAT_FLOAT:
			fdec->fileinfo.bps = 32;
			break;
		case SF_FORMAT_DOUBLE:
			fdec->fileinfo.bps = 64;
			break;
		default:
			/* XXX libsndfile knows some more formats apart from the ones above,
			   but i don't know their bits/sample... perhaps i'm stupid. */
			fdec->fileinfo.bps = 0;
			break;
		}
		fdec->fileinfo.bps *= fdec->SR * fdec->channels;
	}
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
	/* try opening with libFLAC */
 try_flac:
	if (!fdec->file_lib) {
		fdec->flac_error = 0;
		fdec->flac_decoder = FLAC__file_decoder_new();
		FLAC__file_decoder_set_client_data(fdec->flac_decoder, (void *)fdec);
		FLAC__file_decoder_set_write_callback(fdec->flac_decoder,
						      (FLAC__StreamDecoderWriteStatus
						       (*)(const FLAC__FileDecoder *,
							   const FLAC__Frame *,
							   const FLAC__int32 * const [],
							   void *))write_callback);
		FLAC__file_decoder_set_metadata_callback(fdec->flac_decoder,
							 (void (*)(const FLAC__FileDecoder *,
								   const FLAC__StreamMetadata *,
								   void *))metadata_callback);
		FLAC__file_decoder_set_error_callback(fdec->flac_decoder,
						      (void (*)(const FLAC__FileDecoder *,
								FLAC__StreamDecoderErrorStatus,
								void *))error_callback);
		FLAC__file_decoder_set_filename(fdec->flac_decoder, filename);

		if (FLAC__file_decoder_init(fdec->flac_decoder)) {
			fprintf(stderr,
				"file_decoder_open: nonexistent or non-accessible file: %s\n",
				filename);
			fprintf(stderr, "FLAC decoder status: %s\n",
				FLAC__FileDecoderStateString[FLAC__file_decoder_get_state(fdec->flac_decoder)]);
			FLAC__file_decoder_delete(fdec->flac_decoder);
			goto no_open;
		}

		FLAC__file_decoder_process_until_end_of_metadata(fdec->flac_decoder);
		if ((!fdec->flac_error) && (fdec->flac_channels > 0)) {
			if ((fdec->flac_channels != 1) && (fdec->flac_channels != 2)) {
				fprintf(stderr,
					"file_decoder_open: FLAC file with %d channels is "
					"unsupported\n", fdec->flac_channels);
				goto no_open;
			} else {
				if (!fdec->tried_flac) {
					/* we need a real read test (some MP3's get to this point) */
					fdec->flac_probing = 1;
					FLAC__file_decoder_process_single(fdec->flac_decoder);
					fdec->flac_state =
						FLAC__file_decoder_get_state(fdec->flac_decoder);
					
					if ((fdec->flac_state != FLAC__FILE_DECODER_OK) &&
					    (fdec->flac_state != FLAC__FILE_DECODER_END_OF_FILE)) {
						goto no_flac;
					}

					fdec->flac_probing = 0;
					fdec->tried_flac = 1;
					FLAC__file_decoder_finish(fdec->flac_decoder);
					FLAC__file_decoder_delete(fdec->flac_decoder);
					goto try_flac;
				}

				fdec->rb_flac = jack_ringbuffer_create(fdec->flac_channels *
								       sample_size * RB_FLAC_SIZE);
				fdec->channels = fdec->flac_channels;
				fdec->SR = fdec->flac_SR;
				fdec->file_lib = FLAC_LIB;
				
				fdec->fileinfo.total_samples = fdec->flac_total_samples;
				fdec->fileinfo.format_major = FORMAT_FLAC;
				fdec->fileinfo.format_minor = 0;
				fdec->fileinfo.bps = fdec->flac_bits_per_sample *
					fdec->SR * fdec->channels;
			}
		} else {
			FLAC__file_decoder_finish(fdec->flac_decoder);
			FLAC__file_decoder_delete(fdec->flac_decoder);
		}
	}
 no_flac:
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
	/* try opening with libvorbis */
	if (!fdec->file_lib) {
		if ((fdec->vorbis_file = fopen(filename, "rb")) == NULL) {
			fprintf(stderr, "file_decoder_open: fopen() failed for Ogg Vorbis file\n");
			goto no_open;
		}
		if (ov_open(fdec->vorbis_file, &(fdec->vf), NULL, 0) != 0) {
			/* not an Ogg Vorbis file */
			fclose(fdec->vorbis_file);
		} else {
			fdec->vi = ov_info(&(fdec->vf), -1);
			if ((fdec->vi->channels != 1) && (fdec->vi->channels != 2)) {
				fprintf(stderr,
					"file_decoder_open: Ogg Vorbis file with %d channels "
					"is unsupported\n", fdec->vi->channels);
				goto no_open;
			}
			if (ov_streams(&(fdec->vf)) != 1) {
				fprintf(stderr,
					"file_decoder_open: This Ogg Vorbis file contains "
					"multiple logical streams.\n"
					"Currently such a file is not supported.\n");
				goto no_open;
			}
			
			fdec->vorbis_EOS = 0;
			fdec->rb_vorbis = jack_ringbuffer_create(fdec->vi->channels * sample_size
								 * RB_VORBIS_SIZE);
			fdec->channels = fdec->vi->channels;
			fdec->SR = fdec->vi->rate;
			fdec->file_lib = VORBIS_LIB;
			
			fdec->fileinfo.total_samples = ov_pcm_total(&(fdec->vf), -1);
			fdec->fileinfo.format_major = FORMAT_VORBIS;
			fdec->fileinfo.format_minor = 0;
			fdec->fileinfo.bps = ov_bitrate(&(fdec->vf), -1);
		}
	}
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPC
	if (!fdec->file_lib) {
		if ((fdec->mpc_file = fopen(filename, "rb")) == NULL) {
			fprintf(stderr, "file_decoder_open: fopen() failed for Musepack file\n");
			goto no_open;
		}
		fdec->mpc_seekable = 1;
		fseek(fdec->mpc_file, 0, SEEK_END);
		fdec->mpc_size = ftell(fdec->mpc_file);
		fseek(fdec->mpc_file, 0, SEEK_SET);

		mpc_reader_setup_file_reader(&fdec->mpc_r, fdec->mpc_file);

		mpc_streaminfo_init(&fdec->mpc_i);
		if (mpc_streaminfo_read(&fdec->mpc_i, &fdec->mpc_r) != ERROR_CODE_OK) {
			goto no_mpc;
		}

		mpc_decoder_setup(&fdec->mpc_d, &fdec->mpc_r);
		if (!mpc_decoder_initialize(&fdec->mpc_d, &fdec->mpc_i)) {
			goto no_mpc;
		}

		fdec->mpc_EOS = 0;
		fdec->rb_mpc = jack_ringbuffer_create(fdec->mpc_i.channels * sample_size
						      * RB_VORBIS_SIZE);

		fdec->channels = fdec->mpc_i.channels;
		fdec->SR = fdec->mpc_i.sample_freq;
		fdec->file_lib = MPC_LIB;
			
		fdec->fileinfo.total_samples =
			mpc_streaminfo_get_length_samples(&fdec->mpc_i);
		fdec->fileinfo.format_major = FORMAT_MPC;
		fdec->fileinfo.format_minor = fdec->mpc_i.profile;
		fdec->fileinfo.bps = fdec->mpc_i.average_bitrate;
	}
 no_mpc:
#endif /* HAVE_MPC */

#ifdef HAVE_MPEG
	/* try opening with libMAD */
	if (!fdec->file_lib) {

#ifndef HAVE_SNDFILE
		/* some wav files get recognized as valid, so quick hack based on file extension. */
		/* this may only happen if compiled without libsndfile support */
		if (strrchr(filename, '.')) {
			if ((strcmp(strrchr(filename, '.'), ".wav") == 0) ||
			    (strcmp(strrchr(filename, '.'), ".WAV") == 0)) {
				goto no_open;
			}
		}
#endif /* !HAVE_SNDFILE */
/*
		if ((fdec->mpeg_file = fopen(filename, "rb")) == NULL) {
			fprintf(stderr, "file_decoder_open: fopen() failed for MPEG Audio file\n");
			goto no_open;
		}
*/
		if ((fdec->mpeg_exp_fd = open(filename, O_RDONLY)) == 0) {
			fprintf(stderr, "file_decoder_open: open() failed for MPEG Audio file\n");
			goto no_open;
		}

		fstat(fdec->mpeg_exp_fd, &(fdec->mpeg_exp_stat));
		fdec->mpeg_filesize = fdec->mpeg_exp_stat.st_size;
		fdec->mpeg_SR = fdec->mpeg_channels = fdec->mpeg_bitrate = fdec->mpeg_subformat = 0;
		fdec->mpeg_err = fdec->mpeg_again = 0;
		if (mpeg_explore(fdec) != 0) {
			close(fdec->mpeg_exp_fd);
			goto no_mpeg;
		}
		close(fdec->mpeg_exp_fd);
/*
		if (fdec->mpeg_exp_stat.st_size < 2 * MAD_BUFSIZE + 1) {
			fclose(fdec->mpeg_file);
			goto no_mpeg;
		}

		mad_decoder_init(&(fdec->mpeg_decoder), (void *)fdec, mpeg_input_explore,
				 mpeg_header_explore, 0, mpeg_output_explore,
				 mpeg_error_explore, 0);
		mad_decoder_run(&(fdec->mpeg_decoder), MAD_DECODER_MODE_SYNC);
		mad_decoder_finish(&(fdec->mpeg_decoder));

		fclose(fdec->mpeg_file);
*/

		if ((fdec->mpeg_SR) && (fdec->mpeg_channels) && (fdec->mpeg_bitrate)) {
			
			if ((fdec->mpeg_channels != 1) && (fdec->mpeg_channels != 2)) {
				fprintf(stderr,
					"file_decoder_open: MPEG Audio file with %d channels "
					"is unsupported\n", fdec->mpeg_channels);
				goto no_open;
			}

			/* get a rough estimate of the total decoded length of the file */
			/* XXX this may as well be considered a hack. (what about VBR ???) */
/*
			fdec->mpeg_exp_fd = open(filename, O_RDONLY);
			fstat(fdec->mpeg_exp_fd, &(fdec->mpeg_exp_stat));
			close(fdec->mpeg_exp_fd);
*/
			fdec->mpeg_total_samples_est =
				(double)fdec->mpeg_filesize / fdec->mpeg_bitrate * 8 * fdec->mpeg_SR;

			/* setup playback */
			fdec->mpeg_fd = open(filename, O_RDONLY);
			mad_stream_init(&(fdec->mpeg_stream));
			mad_frame_init(&(fdec->mpeg_frame));
			mad_synth_init(&(fdec->mpeg_synth));

			if (mpeg_input((void *)fdec, &(fdec->mpeg_stream)) == MAD_FLOW_STOP) {
				mad_synth_finish(&(fdec->mpeg_synth));
				mad_frame_finish(&(fdec->mpeg_frame));
				mad_stream_finish(&(fdec->mpeg_stream));
				goto no_open;
			}

			fdec->mpeg_err = 0;
			fdec->mpeg_EOS = 0;
			fdec->rb_mpeg = jack_ringbuffer_create(fdec->mpeg_channels * sample_size
							       * RB_MAD_SIZE);
			fdec->channels = fdec->mpeg_channels;
			fdec->SR = fdec->mpeg_SR;
			fdec->file_lib = MAD_LIB;

			fdec->fileinfo.total_samples = fdec->mpeg_total_samples_est;
			fdec->fileinfo.format_major = FORMAT_MAD;
			fdec->fileinfo.format_minor = fdec->mpeg_subformat;
			fdec->fileinfo.bps = fdec->mpeg_bitrate;
		}
	}
 no_mpeg:
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
	/* try opening with libmodplug */
	if (!fdec->file_lib) {

		if ((fdec->mod_fd = open(filename, O_RDONLY)) == -1) {
			fprintf(stderr, 
				"file_decoder_open: nonexistent or non-accessible file (tried MOD opening): %s\n",
				filename);
			goto no_open;
		}

		if (fstat(fdec->mod_fd, &(fdec->mod_st)) == -1 || fdec->mod_st.st_size == 0) {
			fprintf(stderr,
				"file_decoder_open: fstat() error or zero-length file: %s\n",
				filename);
			close(fdec->mod_fd);
			goto no_open;
		}

		fdec->mod_fdm = mmap(0, fdec->mod_st.st_size, PROT_READ, MAP_SHARED,
				     fdec->mod_fd, 0);
		if (fdec->mod_fdm == MAP_FAILED) {
			fprintf(stderr,
				"file_decoder_open: mmap() failed for MOD file %s\n", filename);
			close(fdec->mod_fd);
			goto no_open;
		}

		/* set libmodplug decoder parameters */
		fdec->mp_settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING | MODPLUG_ENABLE_NOISE_REDUCTION;
		fdec->mp_settings.mChannels = 2;
		fdec->mp_settings.mBits = 16;
		fdec->mp_settings.mFrequency = 44100;
		fdec->mp_settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
		fdec->mp_settings.mReverbDepth = 100;
		fdec->mp_settings.mReverbDelay = 100;
		fdec->mp_settings.mBassAmount = 100;
		fdec->mp_settings.mBassRange = 100;
		fdec->mp_settings.mSurroundDepth = 100;
		fdec->mp_settings.mSurroundDelay = 40;
		fdec->mp_settings.mLoopCount = 0;

		ModPlug_SetSettings(&(fdec->mp_settings));
		if ((fdec->mpf = ModPlug_Load(fdec->mod_fdm, fdec->mod_st.st_size)) != NULL) {

			if (fdec->mod_st.st_size * 8000.0f / ModPlug_GetLength(fdec->mpf)
			    >= 1000000.0f) {
				fprintf(stderr,
					"MOD bitrate greater than 1 Mbit/s, "
					"very likely not a MOD file: %s\n", filename);

				ModPlug_Unload(fdec->mpf);
				if (munmap(fdec->mod_fdm, fdec->mod_st.st_size) == -1)
					fprintf(stderr,	"Error while munmap()'ing MOD Audio file mapping\n");
				close(fdec->mod_fd);
			} else {
				
				fdec->mod_EOS = 0;
				fdec->rb_mod = jack_ringbuffer_create(fdec->mp_settings.mChannels
								      * sample_size * RB_MOD_SIZE);
				fdec->channels = fdec->mp_settings.mChannels;
				fdec->SR = fdec->mp_settings.mFrequency;
				fdec->file_lib = MOD_LIB;
				
				fdec->fileinfo.total_samples = ModPlug_GetLength(fdec->mpf)
					/ 1000.0f * fdec->mp_settings.mFrequency;
				fdec->fileinfo.format_major = FORMAT_MOD;
				fdec->fileinfo.format_minor = 0;
				fdec->fileinfo.bps = fdec->mod_st.st_size * 8000.0f /
					ModPlug_GetLength(fdec->mpf);
			}
		} else {
                        if (munmap(fdec->mod_fdm, fdec->mod_st.st_size) == -1)
                                fprintf(stderr, "Error while munmap()'ing MOD Audio file mapping\n");
			close(fdec->mod_fd);
		}

	}
#endif /* HAVE_MOD */


	if (!fdec->file_lib) {
	        goto no_open;
	}


	if (fdec->channels == 1) {
		fdec->fileinfo.is_mono = 1;
#ifdef HAVE_SRC
		fdec->src_ratio = 1.0 * out_SR / fdec->SR;
		if (!src_is_valid_ratio(fdec->src_ratio) ||
		    fdec->src_ratio > MAX_RATIO || fdec->src_ratio < 1.0/MAX_RATIO) {
			fprintf(stderr, "file_decoder_open: too big difference between input and "
				"output sample rate!\n");
#else
	        if (out_SR != fdec->SR) {
			fprintf(stderr,
				"Input file's samplerate (%ld Hz) and output samplerate (%ld Hz) differ, "
				"and\nAqualung is compiled without Sample Rate Converter support. To play "
				"this file,\nyou have to build Aqualung with internal Sample Rate Converter "
				"support,\nor set the playback sample rate to match the file's sample rate."
				"\n", fdec->SR, out_SR);
#endif /* HAVE_SRC */
			fdec->file_open = 1; /* to get close_file() working */
			file_decoder_close(fdec);
			fdec->file_open = 0;
			goto no_open;
		} else
			goto ok_open;

	} else if (fdec->channels == 2) {
		fdec->fileinfo.is_mono = 0;
#ifdef HAVE_SRC
		fdec->src_ratio = 1.0 * out_SR / fdec->SR;
		if (!src_is_valid_ratio(fdec->src_ratio) ||
		    fdec->src_ratio > MAX_RATIO || fdec->src_ratio < 1.0/MAX_RATIO) {
			fprintf(stderr, "file_decoder_open: too big difference between input and "
			       "output sample rate!\n");
#else
	        if (out_SR != fdec->SR) {
			fprintf(stderr,
				"Input file's samplerate (%ld Hz) and output samplerate (%ld Hz) differ, "
				"and\nAqualung is compiled without Sample Rate Converter support. To play "
				"this file,\nyou have to build Aqualung with internal Sample Rate Converter "
				"support,\nor set the playback sample rate to match the file's sample rate."
				"\n", fdec->SR, out_SR);
#endif /* HAVE_SRC */
			fdec->file_open = 1; /* to get close_file() working */
			file_decoder_close(fdec);
			fdec->file_open = 0;
			goto no_open;
		} else
			goto ok_open;

	} else {
		fprintf(stderr, "file_decoder_open: programmer error: "
			"soundfile with %d\n channels is unsupported.\n", fdec->channels);
		goto no_open;
	}

 ok_open:
	fdec->fileinfo.sample_rate = fdec->SR;
	fdec->file_open = 1;
	fdec->samples_left = fdec->fileinfo.total_samples;
	return 0;

 no_open:
	fprintf(stderr, "file_decoder_open: unable to open %s\n", filename);
	return 1;
}


void file_decoder_set_rva(file_decoder_t * fdec, float voladj) {

	fdec->voladj_db = voladj;
	fdec->voladj_lin = db2lin(voladj);
}


void file_decoder_close(file_decoder_t * fdec) {

	if (fdec->file_open) {
		switch (fdec->file_lib) {

#ifdef HAVE_SNDFILE
		case SNDFILE_LIB:
			sf_close(fdec->sf);
			fdec->file_open = 0;
			fdec->file_lib = 0;
			break;
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
		case FLAC_LIB:
			FLAC__file_decoder_finish(fdec->flac_decoder);
			FLAC__file_decoder_delete(fdec->flac_decoder);
			jack_ringbuffer_free(fdec->rb_flac);
			fdec->file_open = 0;
			fdec->file_lib = 0;
			break;
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
		case VORBIS_LIB:
			ov_clear(&(fdec->vf));
			jack_ringbuffer_free(fdec->rb_vorbis);
			fdec->file_open = 0;
			fdec->file_lib = 0;
			break;
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPC
		case MPC_LIB:
			jack_ringbuffer_free(fdec->rb_mpc);
			fclose(fdec->mpc_file);
			fdec->file_open = 0;
			fdec->file_lib = 0;
			break;
#endif /* HAVE_MPC */

#ifdef HAVE_MPEG
		case MAD_LIB:
			mad_synth_finish(&(fdec->mpeg_synth));
			mad_frame_finish(&(fdec->mpeg_frame));
			mad_stream_finish(&(fdec->mpeg_stream));
			if (munmap(fdec->mpeg_fdm, fdec->mpeg_stat.st_size) == -1)
				fprintf(stderr, "Error while munmap()'ing MPEG Audio file mapping\n");
			close(fdec->mpeg_fd);
			jack_ringbuffer_free(fdec->rb_mpeg);
			fdec->file_open = 0;
			fdec->file_lib = 0;
			break;
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
		case MOD_LIB:
			ModPlug_Unload(fdec->mpf);
                        if (munmap(fdec->mod_fdm, fdec->mod_st.st_size) == -1)
                                fprintf(stderr, "Error while munmap()'ing MOD Audio file mapping\n");
                        close(fdec->mod_fd);
                        jack_ringbuffer_free(fdec->rb_mod);
			fdec->file_open = 0;
                        fdec->file_lib = 0;
			break;
#endif /* HAVE_MOD */
			
		default:
			fprintf(stderr,	"file_decoder_close: programmer error: "
				"unexpected value file_lib=%d\n", fdec->file_lib);
			break;
		}
	}
}


unsigned int file_decoder_read(file_decoder_t * fdec, float * dest, int num) {

	fdec->numread = 0;
	fdec->n_avail = 0;

	switch (fdec->file_lib) {

#ifdef HAVE_SNDFILE
	case SNDFILE_LIB:
		fdec->numread = sf_readf_float(fdec->sf, dest, num);
		break;
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
	case FLAC_LIB:
		fdec->flac_state = FLAC__file_decoder_get_state(fdec->flac_decoder);
		while ((jack_ringbuffer_read_space(fdec->rb_flac) < num * fdec->flac_channels
			* sample_size) && (fdec->flac_state == FLAC__FILE_DECODER_OK)) {
			FLAC__file_decoder_process_single(fdec->flac_decoder);
			fdec->flac_state = FLAC__file_decoder_get_state(fdec->flac_decoder);
		}

		if ((fdec->flac_state != FLAC__FILE_DECODER_OK) &&
		    (fdec->flac_state != FLAC__FILE_DECODER_END_OF_FILE)) {
			fprintf(stderr, "file_decoder_read() / FLAC: decoder error: %s\n",
			       FLAC__FileDecoderStateString[fdec->flac_state]);
			return 0; /* this means that a new file will be opened */
		}

		fdec->n_avail = jack_ringbuffer_read_space(fdec->rb_flac) /
			(fdec->flac_channels * sample_size);
		if (fdec->n_avail > num)
			fdec->n_avail = num;
		jack_ringbuffer_read(fdec->rb_flac, (char *)dest, fdec->n_avail *
				     fdec->flac_channels * sample_size);
		fdec->numread = fdec->n_avail;
		break;
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
	case VORBIS_LIB:
		while ((jack_ringbuffer_read_space(fdec->rb_vorbis) <
			num * fdec->vi->channels * sample_size) && (!fdec->vorbis_EOS)) {

			fdec->vorbis_EOS = decode_vorbis(fdec);
		}

		fdec->n_avail = jack_ringbuffer_read_space(fdec->rb_vorbis) /
			(fdec->vi->channels * sample_size);
		if (fdec->n_avail > num)
			fdec->n_avail = num;
		jack_ringbuffer_read(fdec->rb_vorbis, (char *)dest, fdec->n_avail *
				     fdec->vi->channels * sample_size);
		fdec->numread = fdec->n_avail;
		break;
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPC
	case MPC_LIB:
		while ((jack_ringbuffer_read_space(fdec->rb_mpc) <
			num * fdec->mpc_i.channels * sample_size) && (!fdec->mpc_EOS)) {

			fdec->mpc_EOS = decode_mpc(fdec);
		}

		fdec->n_avail = jack_ringbuffer_read_space(fdec->rb_mpc) /
			(fdec->mpc_i.channels * sample_size);
		if (fdec->n_avail > num)
			fdec->n_avail = num;
		jack_ringbuffer_read(fdec->rb_mpc, (char *)dest, fdec->n_avail *
				     fdec->mpc_i.channels * sample_size);
		fdec->numread = fdec->n_avail;
		break;
#endif /* HAVE_MPC */

#ifdef HAVE_MPEG
	case MAD_LIB:
		while ((jack_ringbuffer_read_space(fdec->rb_mpeg) < num * fdec->mpeg_channels *
			sample_size) && (!fdec->mpeg_EOS)) {

			fdec->mpeg_EOS = decode_mpeg(fdec);
		}

		fdec->n_avail = jack_ringbuffer_read_space(fdec->rb_mpeg) /
			(fdec->mpeg_channels * sample_size);
		if (fdec->n_avail > num)
			fdec->n_avail = num;
		jack_ringbuffer_read(fdec->rb_mpeg, (char *)dest, fdec->n_avail *
				     fdec->mpeg_channels * sample_size);
		fdec->numread = fdec->n_avail;

		break;
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
        case MOD_LIB:
                while ((jack_ringbuffer_read_space(fdec->rb_mod) <
			num * fdec->mp_settings.mChannels * sample_size) && (!fdec->mod_EOS)) {

                        fdec->mod_EOS = decode_mod(fdec);
                }

                fdec->n_avail = jack_ringbuffer_read_space(fdec->rb_mod) /
			(fdec->mp_settings.mChannels * sample_size);
                if (fdec->n_avail > num)
                        fdec->n_avail = num;
                jack_ringbuffer_read(fdec->rb_mod, (char *)dest, fdec->n_avail *
				     fdec->mp_settings.mChannels * sample_size);
                fdec->numread = fdec->n_avail;
                break;
#endif /* HAVE_MOD */

	default:
		fprintf(stderr,
			"file_decoder_read: programmer error: unexpected file_lib=%d value\n",
			fdec->file_lib);
		break;
	}
	
	return fdec->numread;
}


void file_decoder_seek(file_decoder_t * fdec, unsigned long long seek_to_pos) {

	switch (fdec->file_lib) {

#ifdef HAVE_SNDFILE
        case SNDFILE_LIB:
		if ((fdec->nframes = sf_seek(fdec->sf, seek_to_pos, SEEK_SET)) != -1) {
			fdec->samples_left = fdec->fileinfo.total_samples - fdec->nframes;
		} else {
			fprintf(stderr, "file_decoder_seek: warning: sf_seek() failed\n");
		}
		break;
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
        case FLAC_LIB:
		if (seek_to_pos == fdec->fileinfo.total_samples)
			--seek_to_pos;
		if (FLAC__file_decoder_seek_absolute(fdec->flac_decoder, seek_to_pos)) {
			fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
			/* empty flac decoder ringbuffer */
			while (jack_ringbuffer_read_space(fdec->rb_flac))
				jack_ringbuffer_read(fdec->rb_flac, &(fdec->flush_dest),
						     sizeof(char));
		} else {
			fprintf(stderr, "file_decoder_seek: warning: "
				"FLAC__file_decoder_seek_absolute() failed\n");
		}
		break;
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
        case VORBIS_LIB:
		if (ov_pcm_seek(&(fdec->vf), seek_to_pos) == 0) {
                        fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
			/* empty vorbis decoder ringbuffer */
			while (jack_ringbuffer_read_space(fdec->rb_vorbis))
				jack_ringbuffer_read(fdec->rb_vorbis, &(fdec->flush_dest),
						     sizeof(char));
		} else {
			fprintf(stderr, "file_decoder_seek: warning: ov_pcm_seek() failed\n");
		}
		break;
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPC
        case MPC_LIB:
		if (mpc_decoder_seek_sample(&fdec->mpc_d, seek_to_pos)) {
                        fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
			/* empty musepack decoder ringbuffer */
			while (jack_ringbuffer_read_space(fdec->rb_mpc))
				jack_ringbuffer_read(fdec->rb_mpc, &(fdec->flush_dest),
						     sizeof(char));
		} else {
			fprintf(stderr,
				"file_decoder_seek: warning: mpc_decoder_seek_sample() failed\n");
		}
		break;
#endif /* HAVE_MPC */

#ifdef HAVE_MPEG
	case MAD_LIB:
		fdec->mpeg_stream.next_frame = fdec->mpeg_stream.buffer;
		mad_stream_sync(&(fdec->mpeg_stream));
		mad_stream_skip(&(fdec->mpeg_stream),
				fdec->mpeg_filesize *
				(double)seek_to_pos / fdec->mpeg_total_samples_est);
		mad_stream_sync(&(fdec->mpeg_stream));
/*
		fdec->mpeg_stream.next_frame = fdec->mpeg_stream.buffer +
			seek_to_pos * fdec->mpeg_bitrate / 8 / fdec->mpeg_SR;
		mad_stream_sync(&(fdec->mpeg_stream));
*/
		fdec->mpeg_EOS = decode_mpeg(fdec);
		/* report the real position of the decoder */
		fdec->samples_left = fdec->fileinfo.total_samples -
			(fdec->mpeg_stream.next_frame - fdec->mpeg_stream.buffer)
			/ fdec->mpeg_bitrate * 8 * fdec->mpeg_SR;
		/* empty mpeg decoder ringbuffer */
		while (jack_ringbuffer_read_space(fdec->rb_mpeg))
			jack_ringbuffer_read(fdec->rb_mpeg, &(fdec->flush_dest), sizeof(char));
		break;
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
	case MOD_LIB:
                if (seek_to_pos == fdec->fileinfo.total_samples)
                        --seek_to_pos;
		ModPlug_Seek(fdec->mpf, (double)seek_to_pos / fdec->mp_settings.mFrequency
			     * 1000.0f);
		fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
                /* empty mod decoder ringbuffer */
                while (jack_ringbuffer_read_space(fdec->rb_mod))
                        jack_ringbuffer_read(fdec->rb_mod, &(fdec->flush_dest), sizeof(char));
		break;
#endif /* HAVE_MOD */

        default:
                fprintf(stderr,
			"file_decoder_seek: programmer error: unexpected file_lib=%d value\n",
			fdec->file_lib);
                break;
	}
}
 

/* expects to get an UTF8 filename */ 
float
get_file_duration(char * file) {

	file_decoder_t * fdec;
	float duration;

	if ((fdec = file_decoder_new()) == NULL) {
                fprintf(stderr, "get_file_duration: error: file_decoder_new() returned NULL\n");
                return 0.0f;
        }

        if (file_decoder_open(fdec, g_locale_from_utf8(file, -1, NULL, NULL, NULL), 44100)) {
                fprintf(stderr, "file_decoder_open() failed on %s\n",
                        g_locale_from_utf8(file, -1, NULL, NULL, NULL));
                return 0.0f;
        }

	duration = (float)fdec->fileinfo.total_samples / fdec->fileinfo.sample_rate;

        file_decoder_close(fdec);
        file_decoder_delete(fdec);

	return duration;
}