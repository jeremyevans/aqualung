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
#include <getopt.h>
#include <sys/stat.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#ifdef HAVE_SNDFILE
#include <sndfile.h>
#endif /* HAVE_SNDFILE */

#ifdef HAVE_SRC
#include <samplerate.h>
#endif /* HAVE_SRC */

/* for OSS support */
#ifdef HAVE_OSS
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/soundcard.h>
#endif /* HAVE_OSS */

/* for JACK support */
#include <jack/jack.h>
#include <jack/ringbuffer.h>

/* for FLAC format support */
#ifdef HAVE_FLAC
#include <FLAC/format.h>
#include <FLAC/file_decoder.h>
#endif /* HAVE_FLAC */

/* for Ogg Vorbis support */
#ifdef HAVE_OGG_VORBIS
#include <vorbis/vorbisfile.h>
#endif /* HAVE_OGG_VORBIS */

/* for MPEG Audio support */
#ifdef HAVE_MPEG
#include <mad.h>
#include <sys/mman.h>
#endif /* HAVE_MPEG */

/* for MOD Audio support */
#ifdef HAVE_MOD
#include <libmodplug/modplug.h>
#include <sys/mman.h>
#endif /* HAVE_MOD */

#include "common.h"
#include "version.h"
#include "transceiver.h"
#include "gui_main.h"
#include "plugin.h"
#include "i18n.h"
#include "core.h"



/* JACK data */
jack_client_t * jack_client;
jack_port_t * out_L_port;
jack_port_t * out_R_port;
char * client_name = NULL;
jack_nframes_t jack_nframes;
int jack_is_shutdown;

const size_t sample_size = sizeof(float);


/* ALSA driver parameters */
#ifdef HAVE_ALSA
int nperiods = 0;
int period = 0;
#endif /* HAVE_ALSA */

/* The name of the output device e.g. "/dev/dsp" or "plughw:0,0" */
char * device_name = NULL;


/***** disk thread stuff *****/
int file_open = 0;
int file_lib;
fileinfo_t fileinfo;
unsigned long long disk_thread_samples_left;
unsigned long long disk_thread_sample_offset;
status_t disk_thread_status;
int output = 0; /* oss/alsa/jack */

#ifdef HAVE_SRC
int src_type = 4;
int src_type_parsed = 0;
#endif /* HAVE_SRC */

/* for sndfile decoding (private to disk thread) */
#ifdef HAVE_SNDFILE
SF_INFO sf_info;
SNDFILE * sf;
#endif /* HAVE_SNDFILE */

/* for FLAC decoding (private to disk thread) */
#ifdef HAVE_FLAC
FLAC__FileDecoder * flac_decoder;
jack_ringbuffer_t * rb_flac;
int flac_channels;
int flac_SR;
unsigned flac_bits_per_sample;
FLAC__uint64 flac_total_samples;
int flac_probing; /* whether we are reading for probing the file, or for real */
int flac_error;
#endif /* HAVE_FLAC */

/* for Ogg Vorbis decoding (private to disk thread) */
#ifdef HAVE_OGG_VORBIS
jack_ringbuffer_t * rb_vorbis;
FILE * vorbis_file;
OggVorbis_File vf;
vorbis_info * vi;
int vorbis_EOS;
#endif /* HAVE_OGG_VORBIS */

/* for MPEG Audio decoding (private to disk thread) */
#ifdef HAVE_MPEG
struct mad_decoder mpeg_decoder;
jack_ringbuffer_t * rb_mpeg;
FILE * mpeg_file;
int mpeg_channels;
int mpeg_SR;
unsigned mpeg_bitrate;
int mpeg_is_exploring; /* 1 if we are doing a decode run to get SR, bitrate, etc. */
int mpeg_again; /* to guard against multiple invocations of mpeg_input_explore() */
int mpeg_err;
int mpeg_EOS;
struct stat mpeg_stat;
int mpeg_fd;
void * mpeg_fdm;
unsigned long mpeg_total_samples_est;
int mpeg_subformat; /* used as v_minor */
struct mad_stream mpeg_stream;
struct mad_frame mpeg_frame;
struct mad_synth mpeg_synth;
#endif /* HAVE_MPEG */

/* for MOD Audio decoding (private to disk thread) */
#ifdef HAVE_MOD
jack_ringbuffer_t * rb_mod;
ModPlug_Settings mp_settings;
ModPlugFile * mpf;
int mod_fd;
void * mod_fdm;
struct stat mod_st;
int mod_EOS;
#endif /* HAVE_MOD */


/* Synchronization between process thread and disk thread */
jack_ringbuffer_t * rb;
pthread_mutex_t disk_thread_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  disk_thread_wake = PTHREAD_COND_INITIALIZER;
jack_ringbuffer_t * rb_disk2out;

/* Communication between gui thread and disk thread */
jack_ringbuffer_t * rb_gui2disk;
jack_ringbuffer_t * rb_disk2gui;

/* Lock critical operations that could interfere with output thread */
pthread_mutex_t output_thread_lock = PTHREAD_MUTEX_INITIALIZER;
double left_gain = 1.0;
double right_gain = 1.0;

/* LADSPA stuff */
int ladspa_is_postfader = 0;
pthread_mutex_t plugin_lock = PTHREAD_MUTEX_INITIALIZER;
unsigned long ladspa_buflen = 0;
LADSPA_Data * l_buf = NULL;
LADSPA_Data * r_buf = NULL;
int n_plugins = 0;
plugin_instance * plugin_vect[MAX_PLUGINS];

/* remote control */
extern int immediate_start;
extern int aqualung_session_id;
extern int aqualung_socket_fd;
extern char aqualung_socket_filename[256];

extern char cwd[MAXLEN];


#ifdef HAVE_FLAC
/* FLAC write callback */
FLAC__StreamDecoderWriteStatus
write_callback(const FLAC__FileDecoder * decoder,
               const FLAC__Frame * frame,
               const FLAC__int32 * const buffer[],
               void * client_data) {

        int i, j;
        long int blocksize;
	long int flac_scale;
        FLAC__int32 buf[2];
        float fbuf[2];

	if (flac_probing)
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

        blocksize = frame->header.blocksize;
        flac_scale = 1 << (flac_bits_per_sample - 1);

        for (i = 0; i < blocksize; i++) {
		for (j = 0; j < flac_channels; j++) {
			buf[j] = *(buffer[j] + i);
			fbuf[j] = (float)buf[j] / flac_scale;
		}
		jack_ringbuffer_write(rb_flac, (char *)fbuf, flac_channels * sample_size);
        }

        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


/* FLAC metadata callback */
void
metadata_callback(const FLAC__FileDecoder * decoder,
                  const FLAC__StreamMetadata * metadata,
                  void * client_data) {

        if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
                flac_SR = metadata->data.stream_info.sample_rate;
                flac_bits_per_sample = metadata->data.stream_info.bits_per_sample;
                flac_channels = metadata->data.stream_info.channels;
		flac_total_samples = metadata->data.stream_info.total_samples;
        } else
                fprintf(stderr, "FLAC metadata callback: ignoring unexpected header\n");
}


/* FLAC error callback */
void
error_callback(const FLAC__FileDecoder * decoder,
               FLAC__StreamDecoderErrorStatus status,
               void * client_data) {

        flac_error = 1;
}
#endif /* HAVE_FLAC */


#ifdef HAVE_OGG_VORBIS
/* Ogg Vorbis: return 1 if reached end of stream, 0 else */
int
decode_vorbis(void) {

	float fbuffer[VORBIS_BUFSIZE / 2];
	char buffer[VORBIS_BUFSIZE];
	int current_section;
	long bytes_read;
	int i;
	
	if ((bytes_read = ov_read(&vf, buffer, VORBIS_BUFSIZE, 0, 2, 1, &current_section)) > 0) {
                for (i = 0; i < bytes_read/2; i++)
                        fbuffer[i] = *((short *)(buffer + 2*i)) / 32768.f;
                jack_ringbuffer_write(rb_vorbis, (char *)fbuffer, bytes_read/2 * sample_size);
		return 0;
        } else
		return 1;
}
#endif /* HAVE_OGG_VORBIS */


#ifdef HAVE_MPEG
/* MPEG Audio decoder input callback for exploring */
static
enum mad_flow
mpeg_input_explore(void * data, struct mad_stream * stream) {

	char file_data[MAD_BUFSIZE];
	int n_read;
	
	if (mpeg_again)
		return MAD_FLOW_STOP;

	mpeg_again = 1;

	fseek(mpeg_file, MAD_BUFSIZE, SEEK_SET);
	if ((n_read = fread(file_data, sizeof(char), MAD_BUFSIZE, mpeg_file)) != MAD_BUFSIZE) {
		mpeg_EOS = 1;
		fclose(mpeg_file);
	}

	if (n_read) {
		mad_stream_buffer(stream, file_data, n_read);
	}
	
	return MAD_FLOW_CONTINUE;
}


/* MPEG Audio decoder output callback for exploring */
static
enum mad_flow
mpeg_output_explore(void * data, struct mad_header const * header, struct mad_pcm * pcm) {

	mpeg_channels = pcm->channels;
	mpeg_SR = pcm->samplerate;
	return MAD_FLOW_STOP;
}


/* MPEG Audio decoder header callback for exploring */
static
enum mad_flow
mpeg_header_explore(void * data, struct mad_header const * header) {

	mpeg_bitrate = header->bitrate;

	mpeg_subformat = 0;
	switch (header->layer) {
	case MAD_LAYER_I:
		mpeg_subformat |= MPEG_LAYER_I;
		break;
	case MAD_LAYER_II:
		mpeg_subformat |= MPEG_LAYER_II;
		break;
	case MAD_LAYER_III:
		mpeg_subformat |= MPEG_LAYER_III;
		break;
	}

	switch (header->mode) {
	case MAD_MODE_SINGLE_CHANNEL:
		mpeg_subformat |= MPEG_MODE_SINGLE;
		break;
	case MAD_MODE_DUAL_CHANNEL:
		mpeg_subformat |= MPEG_MODE_DUAL;
		break;
	case MAD_MODE_JOINT_STEREO:
		mpeg_subformat |= MPEG_MODE_JOINT;
		break;
	case MAD_MODE_STEREO:
		mpeg_subformat |= MPEG_MODE_STEREO;
		break;
	}

	switch (header->emphasis) {
	case MAD_EMPHASIS_NONE:
		mpeg_subformat |= MPEG_EMPH_NONE;
		break;
	case MAD_EMPHASIS_50_15_US:
		mpeg_subformat |= MPEG_EMPH_5015;
		break;
	case MAD_EMPHASIS_CCITT_J_17:
		mpeg_subformat |= MPEG_EMPH_J_17;
		break;
	default:
		mpeg_subformat |= MPEG_EMPH_RES;
		break;
	}

	return MAD_FLOW_CONTINUE;
}


/* MPEG Audio decoder error callback for exploring */
static
enum mad_flow
mpeg_error_explore(void * data, struct mad_stream * stream, struct mad_frame * frame) {

	mpeg_err = 1;

	return MAD_FLOW_CONTINUE;
}


/* MPEG Audio decoder input callback for playback */
static
enum mad_flow
mpeg_input(void * data, struct mad_stream * stream) {
	
	if (fstat(mpeg_fd, &mpeg_stat) == -1 || mpeg_stat.st_size == 0)
		return MAD_FLOW_STOP;
	
	mpeg_fdm = mmap(0, mpeg_stat.st_size, PROT_READ, MAP_SHARED, mpeg_fd, 0);
	if (mpeg_fdm == MAP_FAILED)
		return MAD_FLOW_STOP;
	
	mad_stream_buffer(stream, mpeg_fdm, mpeg_stat.st_size);

	return MAD_FLOW_CONTINUE;
}


/* MPEG Audio decoder output callback for playback */
static
enum mad_flow
mpeg_output(void * data, struct mad_header const * header, struct mad_pcm * pcm) {

	int i, j;
	unsigned long mpeg_scale = 1 << 28;
        int buf[2];
        float fbuf[2];

        for (i = 0; i < pcm->length; i++) {
		for (j = 0; j < mpeg_channels; j++) {
			buf[j] = mpeg_err ? 0 : *(pcm->samples[j] + i);
			fbuf[j] = (double)buf[j] / mpeg_scale;
		}
		if (jack_ringbuffer_write_space(rb_mpeg) >= mpeg_channels * sample_size)
			jack_ringbuffer_write(rb_mpeg, (char *)fbuf, mpeg_channels * sample_size);
        }
	mpeg_err = 0;

	return MAD_FLOW_CONTINUE;
}


/* MPEG Audio decoder header callback for playback */
static
enum mad_flow
mpeg_header(void * data, struct mad_header const * header) {

	mpeg_bitrate = header->bitrate;
	
	return MAD_FLOW_CONTINUE;
}


/* MPEG Audio decoder error callback for playback */
static
enum mad_flow
mpeg_error(void * data, struct mad_stream * stream, struct mad_frame * frame) {

	mpeg_err = 1;

	return MAD_FLOW_CONTINUE;
}


/* Main MPEG Audio decode loop: return 1 if reached end of stream, 0 else */
int
decode_mpeg() {

	if (mad_header_decode(&mpeg_frame.header, &mpeg_stream) == -1) {
		if (mpeg_stream.error == MAD_ERROR_BUFLEN)
		        return 1;

		if (!MAD_RECOVERABLE(mpeg_stream.error))
			fprintf(stderr, "libMAD: unrecoverable error in MPEG Audio stream\n");

		mpeg_error(NULL, &mpeg_stream, &mpeg_frame);
		mpeg_header(NULL, &mpeg_frame.header);
	}

	if (mad_frame_decode(&mpeg_frame, &mpeg_stream) == -1) {
		if (mpeg_stream.error == MAD_ERROR_BUFLEN)
			return 1;

		if (!MAD_RECOVERABLE(mpeg_stream.error))
			fprintf(stderr, "libMAD: unrecoverable error in MPEG Audio stream\n");

		mpeg_error(NULL, &mpeg_stream, &mpeg_frame);
	}

	mad_synth_frame(&mpeg_synth, &mpeg_frame);
	mpeg_output(NULL, &mpeg_frame.header, &mpeg_synth.pcm);
	
	return 0;
}
#endif /* HAVE_MPEG */


#ifdef HAVE_MOD
/* MOD decoder: return 1 if reached end of stream, 0 else */
int
decode_mod(void) {

        float fbuffer[MOD_BUFSIZE / 2];
        char buffer[MOD_BUFSIZE];
        long bytes_read;
        int i;

        if ((bytes_read = ModPlug_Read(mpf, buffer, MOD_BUFSIZE)) > 0) {
                for (i = 0; i < bytes_read/2; i++)
                        fbuffer[i] = *((short *)(buffer + 2*i)) / 32768.f;
                jack_ringbuffer_write(rb_mod, (char *)fbuffer, bytes_read/2 * sample_size);
                return 0;
        } else
                return 1;
}
#endif /* HAVE_MOD */



int close_file(void * arg);

/* return: =0 on OK, >0 on error */
int
open_file(void * arg, char * filename) {

        thread_info_t * info = (thread_info_t *)arg;
	int channels = 0;
	unsigned long SR = 0;
	char cmd;
#ifdef HAVE_SRC
	double src_ratio;
#endif /* HAVE_SRC */
#ifdef HAVE_FLAC
	FLAC__FileDecoderState flac_state;
	int tried_flac = 0;
#endif /* HAVE_FLAC */

	file_lib = 0;

#ifdef HAVE_SNDFILE
	/* try opening with libsndfile */
	sf_info.format = 0;
	if ((sf = sf_open(filename, SFM_READ, &sf_info)) != NULL) {
		if ((sf_info.channels != 1) && (sf_info.channels != 2)) {
			fprintf(stderr,
				"open_file: sndfile with %d channels is unsupported\n", sf_info.channels);
			goto no_open;
		}
		channels = sf_info.channels;
		SR = sf_info.samplerate;
		file_lib = SNDFILE_LIB;

		fileinfo.total_samples = sf_info.frames;
		fileinfo.format_major = sf_info.format & SF_FORMAT_TYPEMASK;
		fileinfo.format_minor = sf_info.format & SF_FORMAT_SUBMASK;

		switch (fileinfo.format_minor) {
		case SF_FORMAT_PCM_S8:
		case SF_FORMAT_PCM_U8:
			fileinfo.bps = 8;
			break;
		case SF_FORMAT_PCM_16:
			fileinfo.bps = 16;
			break;
		case SF_FORMAT_PCM_24:
			fileinfo.bps = 24;
			break;
		case SF_FORMAT_PCM_32:
		case SF_FORMAT_FLOAT:
			fileinfo.bps = 32;
			break;
		case SF_FORMAT_DOUBLE:
			fileinfo.bps = 64;
			break;
		default:
			/* XXX libsndfile knows some more formats apart from the ones above,
			   but i don't know their bits/sample... perhaps i'm stupid. */
			fileinfo.bps = 0;
			break;
		}
		fileinfo.bps *= SR * channels;
	}
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
	/* try opening with libFLAC */
 try_flac:
	if (!file_lib) {
		flac_error = 0;
		flac_decoder = FLAC__file_decoder_new();
		FLAC__file_decoder_set_write_callback(flac_decoder,
						      (FLAC__StreamDecoderWriteStatus
						       (*)(const FLAC__FileDecoder *,
							   const FLAC__Frame *,
							   const FLAC__int32 * const [],
							   void *))write_callback);
		FLAC__file_decoder_set_metadata_callback(flac_decoder,
							 (void (*)(const FLAC__FileDecoder *,
								   const FLAC__StreamMetadata *,
								   void *))metadata_callback);
		FLAC__file_decoder_set_error_callback(flac_decoder,
						      (void (*)(const FLAC__FileDecoder *,
								FLAC__StreamDecoderErrorStatus,
								void *))error_callback);
		FLAC__file_decoder_set_filename(flac_decoder, filename);

		if (FLAC__file_decoder_init(flac_decoder)) {
			fprintf(stderr,
				"open_file: nonexistent or non-accessible file: %s\n", filename);
			FLAC__file_decoder_delete(flac_decoder);
			goto no_open;
		}

		FLAC__file_decoder_process_until_end_of_metadata(flac_decoder);
		if ((!flac_error) && (flac_channels > 0)) {
			if ((flac_channels != 1) && (flac_channels != 2)) {
				fprintf(stderr, "open_file: FLAC file with %d channels is unsupported\n",
				       flac_channels);
				goto no_open;
			} else {
				if (!tried_flac) {
					/* we need a real read test (some MP3's get to this point) */
					flac_probing = 1;
					FLAC__file_decoder_process_single(flac_decoder);
					flac_state = FLAC__file_decoder_get_state(flac_decoder);
					
					if ((flac_state != FLAC__FILE_DECODER_OK) &&
					    (flac_state != FLAC__FILE_DECODER_END_OF_FILE)) {
						goto no_flac;
					}

					flac_probing = 0;
					tried_flac = 1;
					FLAC__file_decoder_finish(flac_decoder);
					FLAC__file_decoder_delete(flac_decoder);
					goto try_flac;
				}

				rb_flac = jack_ringbuffer_create(flac_channels * sample_size * RB_FLAC_SIZE);
				channels = flac_channels;
				SR = flac_SR;
				file_lib = FLAC_LIB;
				
				fileinfo.total_samples = flac_total_samples;
				fileinfo.format_major = FORMAT_FLAC;
				fileinfo.format_minor = 0;
				fileinfo.bps = flac_bits_per_sample * SR * channels;
			}
		} else {
			FLAC__file_decoder_finish(flac_decoder);
			FLAC__file_decoder_delete(flac_decoder);
		}
	}
 no_flac:
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
	/* try opening with libvorbis */
	if (!file_lib) {
		if ((vorbis_file = fopen(filename, "rb")) == NULL) {
			fprintf(stderr, "open_file: fopen failed for Ogg Vorbis file\n");
			goto no_open;
		}
		if (ov_open(vorbis_file, &vf, NULL, 0) != 0) {
			/* not an Ogg Vorbis file */
			fclose(vorbis_file);
		} else {
			vi = ov_info(&vf, -1);
			if ((vi->channels != 1) && (vi->channels != 2)) {
				fprintf(stderr,
					"open_file: Ogg Vorbis file with %d channels is unsupported\n",
					vi->channels);
				goto no_open;
			}
			if (ov_streams(&vf) != 1) {
				fprintf(stderr,
					"open_file: This Ogg Vorbis file contains multiple logical streams.\n"
					"Currently such a file is not supported.\n");
				goto no_open;
			}
			
			vorbis_EOS = 0;
			rb_vorbis = jack_ringbuffer_create(vi->channels * sample_size * RB_VORBIS_SIZE);
			channels = vi->channels;
			SR = vi->rate;
			file_lib = VORBIS_LIB;
			
			fileinfo.total_samples = ov_pcm_total(&vf, -1);
			fileinfo.format_major = FORMAT_VORBIS;
			fileinfo.format_minor = 0;
			fileinfo.bps = ov_bitrate(&vf, -1);
		}
	}
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPEG
	/* try opening with libMAD */
	if (!file_lib) {

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

		if ((mpeg_file = fopen(filename, "rb")) == NULL) {
			fprintf(stderr, "open_file: fopen failed for MPEG Audio file\n");
			goto no_open;
		}

		{
			int fd;
			struct stat stat;
			
			fd = open(filename, O_RDONLY);
			fstat(fd, &stat);
			close(fd);
			if (stat.st_size < 2 * MAD_BUFSIZE + 1) {
				fclose(mpeg_file);
				goto no_mpeg;
			}
		}

		mpeg_SR = mpeg_channels = mpeg_bitrate = 0;
		mpeg_err = mpeg_again = 0;
		mad_decoder_init(&mpeg_decoder, NULL, mpeg_input_explore,
				 mpeg_header_explore, 0, mpeg_output_explore, mpeg_error_explore, 0);
		mad_decoder_run(&mpeg_decoder, MAD_DECODER_MODE_SYNC);
		mad_decoder_finish(&mpeg_decoder);

		fclose(mpeg_file);

		if ((mpeg_SR) && (mpeg_channels) && (mpeg_bitrate)) {
			
			if ((mpeg_channels != 1) && (mpeg_channels != 2)) {
				fprintf(stderr,
					"open_file: MPEG Audio file with %d channels is unsupported\n",
					mpeg_channels);
				goto no_open;
			}

			{ /* get a rough estimate of the total decoded length of the file */
			  /* XXX this may as well be considered a hack. (what about VBR ???) */
				int fd;
				struct stat stat;

				fd = open(filename, O_RDONLY);
				fstat(fd, &stat);
				close(fd);
				mpeg_total_samples_est = (double)stat.st_size / mpeg_bitrate * 8 * mpeg_SR;
			}

			/* setup playback */
			mpeg_fd = open(filename, O_RDONLY);
			mad_stream_init(&mpeg_stream);
			mad_frame_init(&mpeg_frame);
			mad_synth_init(&mpeg_synth);

			if (mpeg_input(NULL, &mpeg_stream) == MAD_FLOW_STOP) {
				mad_synth_finish(&mpeg_synth);
				mad_frame_finish(&mpeg_frame);
				mad_stream_finish(&mpeg_stream);
				goto no_open;
			}

			mpeg_err = 0;
			mpeg_EOS = 0;
			rb_mpeg = jack_ringbuffer_create(mpeg_channels * sample_size * RB_MAD_SIZE);
			channels = mpeg_channels;
			SR = mpeg_SR;
			file_lib = MAD_LIB;

			fileinfo.total_samples = mpeg_total_samples_est;
			fileinfo.format_major = FORMAT_MAD;
			fileinfo.format_minor = mpeg_subformat;
			fileinfo.bps = mpeg_bitrate;
		}
	}
 no_mpeg:
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
	/* try opening with libmodplug */
	if (!file_lib) {

		if ((mod_fd = open(filename, O_RDONLY)) == -1) {
			fprintf(stderr, "open_file: nonexistent or non-accessible file: %s\n", filename);
			goto no_open;
		}

		if (fstat(mod_fd, &mod_st) == -1 || mod_st.st_size == 0) {
			fprintf(stderr, "open_file: fstat error or zero-length file: %s\n", filename);
			close(mod_fd);
			goto no_open;
		}

		mod_fdm = mmap(0, mod_st.st_size, PROT_READ, MAP_SHARED, mod_fd, 0);
		if (mod_fdm == MAP_FAILED) {
			fprintf(stderr, "open_file: mmap() failed for MOD file %s\n", filename);
			close(mod_fd);
			goto no_open;
		}

		/* set libmodplug decoder parameters */
		mp_settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING | MODPLUG_ENABLE_NOISE_REDUCTION;
		mp_settings.mChannels = 2;
		mp_settings.mBits = 16;
		mp_settings.mFrequency = 44100;
		mp_settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
		mp_settings.mReverbDepth = 100;
		mp_settings.mReverbDelay = 100;
		mp_settings.mBassAmount = 100;
		mp_settings.mBassRange = 100;
		mp_settings.mSurroundDepth = 100;
		mp_settings.mSurroundDelay = 40;
		mp_settings.mLoopCount = 0;

		ModPlug_SetSettings(&mp_settings);
		if ((mpf = ModPlug_Load(mod_fdm, mod_st.st_size)) != NULL) {

			if (mod_st.st_size * 8000.0f / ModPlug_GetLength(mpf) >= 1000000.0f) {
				fprintf(stderr,
					"MOD bitrate greater than 1 Mbit/s, "
					"very likely not a MOD file: %s\n", filename);

				ModPlug_Unload(mpf);
				if (munmap(mod_fdm, mod_st.st_size) == -1)
					fprintf(stderr, "Error while munmap()'ing MOD Audio file mapping\n");
				close(mod_fd);
			} else {
				
				mod_EOS = 0;
				rb_mod = jack_ringbuffer_create(mp_settings.mChannels * sample_size
								* RB_MOD_SIZE);
				channels = mp_settings.mChannels;
				SR = mp_settings.mFrequency;
				file_lib = MOD_LIB;
				
				fileinfo.total_samples = ModPlug_GetLength(mpf) / 1000.0f *
					mp_settings.mFrequency;
				fileinfo.format_major = FORMAT_MOD;
				fileinfo.format_minor = 0;
				fileinfo.bps = mod_st.st_size * 8000.0f / ModPlug_GetLength(mpf);

			}
		} else {
                        if (munmap(mod_fdm, mod_st.st_size) == -1)
                                fprintf(stderr, "Error while munmap()'ing MOD Audio file mapping\n");
			close(mod_fd);
		}

	}
#endif /* HAVE_MOD */


	if (!file_lib) {
	        goto no_open;
	}


	if (channels == 1) {
		info->is_mono = 1;
		fileinfo.is_mono = 1;
#ifdef HAVE_SRC
		src_ratio = 1.0 * info->out_SR / SR;
		if (!src_is_valid_ratio(src_ratio) || src_ratio > MAX_RATIO || src_ratio < 1.0/MAX_RATIO) {
			fprintf(stderr, "open_file: too big difference between input and "
				"output sample rate!\n");
#else
	        if (info->out_SR != SR) {
			fprintf(stderr,
				"Input file's samplerate (%ld Hz) and output samplerate (%ld Hz) differ, "
				"and\nAqualung is compiled without Sample Rate Converter support. To play "
				"this file,\nyou have to build Aqualung with internal Sample Rate Converter "
				"support,\nor set the playback sample rate to match the file's sample rate."
				"\n", SR, info->out_SR);
#endif /* HAVE_SRC */
			file_open = 1; /* to get close_file() working */
			close_file(arg);
			file_open = 0;
			goto no_open;
		} else
			goto ok_open;

	} else if (channels == 2) {
		info->is_mono = 0;
		fileinfo.is_mono = 0;
#ifdef HAVE_SRC
		src_ratio = 1.0 * info->out_SR / SR;
		if (!src_is_valid_ratio(src_ratio) || src_ratio > MAX_RATIO || src_ratio < 1.0/MAX_RATIO) {
			fprintf(stderr, "open_file: too big difference between input and "
			       "output sample rate!\n");
#else
	        if (info->out_SR != SR) {
			fprintf(stderr,
				"Input file's samplerate (%ld Hz) and output samplerate (%ld Hz) differ, "
				"and\nAqualung is compiled without Sample Rate Converter support. To play "
				"this file,\nyou have to build Aqualung with internal Sample Rate Converter "
				"support,\nor set the playback sample rate to match the file's sample rate."
				"\n", SR, info->out_SR);
#endif /* HAVE_SRC */
			file_open = 1; /* to get close_file() working */
			close_file(arg);
			file_open = 0;
			goto no_open;
		} else
			goto ok_open;

	} else {
		fprintf(stderr, "open_file: programmer error: "
		       "soundfile with %d\n channels is unsupported.\n", channels);
		goto no_open;
	}

 ok_open:
	info->in_SR_prev = info->in_SR;
	info->in_SR = SR;
	fileinfo.sample_rate = SR;
	file_open = 1;
	disk_thread_samples_left = fileinfo.total_samples;
	disk_thread_sample_offset = 0;
	cmd = CMD_FILEINFO;
	jack_ringbuffer_write(rb_disk2gui, &cmd, sizeof(cmd));
	jack_ringbuffer_write(rb_disk2gui, (char *)&fileinfo, sizeof(fileinfo_t));
	return 0;

 no_open:
	fprintf(stderr, "open_file: unable to open %s\n", filename);
	disk_thread_samples_left = 0;
	return 1;
}


int
close_file(void * arg) {

	if (file_open) {
		switch (file_lib) {

#ifdef HAVE_SNDFILE
		case SNDFILE_LIB:
			sf_close(sf);
			file_lib = 0;
			break;
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
		case FLAC_LIB:
			FLAC__file_decoder_finish(flac_decoder);
			FLAC__file_decoder_delete(flac_decoder);
			jack_ringbuffer_free(rb_flac);
			file_lib = 0;
			break;
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
		case VORBIS_LIB:
			ov_clear(&vf);
			jack_ringbuffer_free(rb_vorbis);
			file_lib = 0;
			break;
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPEG
		case MAD_LIB:
			mad_synth_finish(&mpeg_synth);
			mad_frame_finish(&mpeg_frame);
			mad_stream_finish(&mpeg_stream);
			if (munmap(mpeg_fdm, mpeg_stat.st_size) == -1)
				fprintf(stderr, "Error while munmap()'ing MPEG Audio file mapping\n");
			close(mpeg_fd);
			jack_ringbuffer_free(rb_mpeg);
			file_lib = 0;
			break;
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
		case MOD_LIB:
			ModPlug_Unload(mpf);
                        if (munmap(mod_fdm, mod_st.st_size) == -1)
                                fprintf(stderr, "Error while munmap()'ing MOD Audio file mapping\n");
                        close(mod_fd);
                        jack_ringbuffer_free(rb_mod);
                        file_lib = 0;
			break;
#endif /* HAVE_MOD */

		default:
			fprintf(stderr,
				"close_file: programmer error: unexpected value file_lib=%d\n", file_lib);
			break;
		}
	}
	return 0;
}


/* return the number of samples actually placed in dest */
unsigned int read_file(void * arg, float * dest, int num) {

	unsigned int numread = 0;
	unsigned int n_avail;
#ifdef HAVE_FLAC
	FLAC__FileDecoderState flac_state;
#endif /* HAVE_FLAC */

	switch (file_lib) {

#ifdef HAVE_SNDFILE
	case SNDFILE_LIB:
		numread = sf_readf_float(sf, dest, num);
		break;
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
	case FLAC_LIB:
		flac_state = FLAC__file_decoder_get_state(flac_decoder);
		while ((jack_ringbuffer_read_space(rb_flac) < num * flac_channels * sample_size) &&
		       (flac_state == FLAC__FILE_DECODER_OK)){
			FLAC__file_decoder_process_single(flac_decoder);
			flac_state = FLAC__file_decoder_get_state(flac_decoder);
		}

		if ((flac_state != FLAC__FILE_DECODER_OK) &&
		    (flac_state != FLAC__FILE_DECODER_END_OF_FILE)) {
			fprintf(stderr, "read_file/FLAC: decoder error: %s\n",
			       FLAC__FileDecoderStateString[flac_state]);
			return 0; /* this means that a new file will be opened */
		}

		n_avail = jack_ringbuffer_read_space(rb_flac) / (flac_channels * sample_size);
		if (n_avail > num)
			n_avail = num;
		jack_ringbuffer_read(rb_flac, (char *)dest, n_avail * flac_channels * sample_size);
		numread = n_avail;
		break;
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
	case VORBIS_LIB:
		while ((jack_ringbuffer_read_space(rb_vorbis) < num * vi->channels * sample_size) &&
		       (!vorbis_EOS)) {
			vorbis_EOS = decode_vorbis();
		}

		n_avail = jack_ringbuffer_read_space(rb_vorbis) / (vi->channels * sample_size);
		if (n_avail > num)
			n_avail = num;
		jack_ringbuffer_read(rb_vorbis, (char *)dest, n_avail * vi->channels * sample_size);
		numread = n_avail;
		break;
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPEG
	case MAD_LIB:
		while ((jack_ringbuffer_read_space(rb_mpeg) < num * mpeg_channels * sample_size) &&
		       (!mpeg_EOS)) {
			mpeg_EOS = decode_mpeg();
		}

		n_avail = jack_ringbuffer_read_space(rb_mpeg) / (mpeg_channels * sample_size);
		if (n_avail > num)
			n_avail = num;
		jack_ringbuffer_read(rb_mpeg, (char *)dest, n_avail * mpeg_channels * sample_size);
		numread = n_avail;
		break;
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
        case MOD_LIB:
                while ((jack_ringbuffer_read_space(rb_mod) < num * mp_settings.mChannels * sample_size) &&
                       (!mod_EOS)) {
                        mod_EOS = decode_mod();
                }

                n_avail = jack_ringbuffer_read_space(rb_mod) / (mp_settings.mChannels * sample_size);
                if (n_avail > num)
                        n_avail = num;
                jack_ringbuffer_read(rb_mod, (char *)dest, n_avail * mp_settings.mChannels * sample_size);
                numread = n_avail;
                break;
#endif /* HAVE_MOD */

	default:
		fprintf(stderr, "read_file: programmer error: unexpected file_lib=%d value\n", file_lib);
		break;
	}

	return numread;
}



/* seek currently open file to given sample pos */
void
seek_file(void * arg, unsigned long long seek_to_pos) {

#ifdef HAVE_SNDFILE
	unsigned int nframes;
#endif /* HAVE_SNDFILE */

	char flush_dest;

	switch (file_lib) {

#ifdef HAVE_SNDFILE
        case SNDFILE_LIB:
		if ((nframes = sf_seek(sf, seek_to_pos, SEEK_SET)) != -1) {
			disk_thread_samples_left = fileinfo.total_samples - nframes;
		} else {
			fprintf(stderr, "Warning: in seek_file: sf_seek() failed\n");
		}
		break;
#endif /* HAVE_SNDFILE */

#ifdef HAVE_FLAC
        case FLAC_LIB:
		if (seek_to_pos == fileinfo.total_samples)
			--seek_to_pos;
		if (FLAC__file_decoder_seek_absolute(flac_decoder, seek_to_pos)) {
			disk_thread_samples_left = fileinfo.total_samples - seek_to_pos;
			/* empty flac decoder ringbuffer */
			while (jack_ringbuffer_read_space(rb_flac))
				jack_ringbuffer_read(rb_flac, &flush_dest, sizeof(char));
		} else {
			fprintf(stderr, "Warning: in seek_file: FLAC__file_decoder_seek_absolute() failed\n");
		}
		break;
#endif /* HAVE_FLAC */

#ifdef HAVE_OGG_VORBIS
        case VORBIS_LIB:
		if (ov_pcm_seek(&vf, seek_to_pos) == 0) {
			disk_thread_samples_left = fileinfo.total_samples - seek_to_pos;
			/* empty vorbis decoder ringbuffer */
			while (jack_ringbuffer_read_space(rb_vorbis))
				jack_ringbuffer_read(rb_vorbis, &flush_dest, sizeof(char));
		} else {
			fprintf(stderr, "Warning: in seek_file: ov_pcm_seek() failed\n");
		}
		break;
#endif /* HAVE_OGG_VORBIS */

#ifdef HAVE_MPEG
	case MAD_LIB:
		mpeg_stream.next_frame = mpeg_stream.buffer + seek_to_pos * mpeg_bitrate / 8 / mpeg_SR;
		mad_stream_sync(&mpeg_stream);
		mpeg_EOS = decode_mpeg();
		/* report the real position of the decoder */
		disk_thread_samples_left = fileinfo.total_samples -
			(mpeg_stream.next_frame - mpeg_stream.buffer) / mpeg_bitrate * 8 * mpeg_SR;
		/* empty mpeg decoder ringbuffer */
		while (jack_ringbuffer_read_space(rb_mpeg))
			jack_ringbuffer_read(rb_mpeg, &flush_dest, sizeof(char));
		break;
#endif /* HAVE_MPEG */

#ifdef HAVE_MOD
	case MOD_LIB:
                if (seek_to_pos == fileinfo.total_samples)
                        --seek_to_pos;
		ModPlug_Seek(mpf, (double)seek_to_pos / mp_settings.mFrequency * 1000.0f);
		disk_thread_samples_left = fileinfo.total_samples - seek_to_pos;
                /* empty mod decoder ringbuffer */
                while (jack_ringbuffer_read_space(rb_mod))
                        jack_ringbuffer_read(rb_mod, &flush_dest, sizeof(char));
		break;
#endif /* HAVE_MOD */

        default:
                fprintf(stderr, "seek_file: programmer error: unexpected file_lib=%d value\n", file_lib);
                break;
	}
}




void *
disk_thread(void * arg) {

	thread_info_t * info = (thread_info_t *)arg;
	unsigned int n_read = 0;
	unsigned int want_read;
	int n_src = 0;
	int n_src_prev = 0;
	int end_of_file = 0;
	double src_ratio = 1.0;
	void * readbuf = malloc(MAX_RATIO * info->rb_size * 2 * sample_size);
	void * framebuf = malloc(MAX_RATIO * info->rb_size * 2 * sample_size);
	size_t n_space;
	char send_cmd, recv_cmd;
	char filename[RB_CONTROL_SIZE];
	seek_t seek;
	int i;


#ifdef HAVE_SRC	
	int src_type_prev;
	SRC_STATE * src_state;
	SRC_DATA src_data;
	int src_error;

        if ((src_state = src_new(src_type, 2, &src_error)) == NULL) {
		fprintf(stderr, "disk thread: error: src_new() failed: %s.\n", src_strerror(src_error));
		exit(1);
	}
	src_type_prev = src_type;
#endif /* HAVE_SRC */

	if ((!readbuf) || (!framebuf)) {
		fprintf(stderr, "disk thread: malloc error\n");
		exit(1);
	}

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_mutex_lock(&disk_thread_lock);

	filename[0] = '\0';

	while (1) {

		recv_cmd = 0;
		if (jack_ringbuffer_read_space(rb_gui2disk) > 0) {
			jack_ringbuffer_read(rb_gui2disk, &recv_cmd, 1);
			switch (recv_cmd) {
			case CMD_CUE: 
				/* read the string */
				i = 0;
				while ((jack_ringbuffer_read_space(rb_gui2disk)) && (i < RB_CONTROL_SIZE)) {
					jack_ringbuffer_read(rb_gui2disk, filename + i, 1);
					i++;
				}
				filename[i] = '\0';
				if (file_lib != 0)
					close_file(arg);
				if (filename[0] != '\0') {
					if (open_file(arg, filename)) {
						info->is_streaming = 0;
						end_of_file = 1;
						send_cmd = CMD_FILEREQ;
						jack_ringbuffer_write(rb_disk2gui, &send_cmd, 1);
						goto sleep;
					} else {
						info->is_streaming = 1;
						end_of_file = 0;
					}
				} else { /* STOP */
					info->is_streaming = 0;

					/* send a FLUSH command to output thread to stop immediately */
					send_cmd = CMD_FLUSH;
					jack_ringbuffer_write(rb_disk2out, &send_cmd, 1);
					goto sleep;
				}
				break;
			case CMD_STOPWOFL: /* STOP but first flush output ringbuffer. */
				info->is_streaming = 0;
				goto flush;
				break;
			case CMD_PAUSE:
				info->is_streaming = 0;

				/* send a FLUSH command to output thread */
				send_cmd = CMD_FLUSH;
				jack_ringbuffer_write(rb_disk2out, &send_cmd, 1);

				/* roll back sample_offset samples, if possible */
				disk_thread_sample_offset =
					jack_ringbuffer_read_space(rb) / (2 * sample_size) * src_ratio;
				if (disk_thread_sample_offset <
				    fileinfo.total_samples - disk_thread_samples_left)
					seek_file(arg, fileinfo.total_samples - disk_thread_samples_left
						  - disk_thread_sample_offset);
				else
					seek_file(arg, 0);
				break;
			case CMD_RESUME:
				info->is_streaming = 1;
				break;
			case CMD_FINISH:
				goto done;
				break;
			case CMD_SEEKTO:
				while (jack_ringbuffer_read_space(rb_gui2disk) < sizeof(seek_t))
					;
				jack_ringbuffer_read(rb_gui2disk, (char *)&seek, sizeof(seek_t));
				if (file_lib != 0) {
					seek_file(arg, seek.seek_to_pos);
					/* send a FLUSH command to output thread */
					send_cmd = CMD_FLUSH;
					jack_ringbuffer_write(rb_disk2out, &send_cmd, 1);
				} else {
					/* send dummy STATUS to gui, to set pos slider to zero */
					disk_thread_status.samples_left = 0;
					disk_thread_status.sample_offset = 0;
					send_cmd = CMD_STATUS;
					jack_ringbuffer_write(rb_disk2gui, &send_cmd, sizeof(send_cmd));
					jack_ringbuffer_write(rb_disk2gui, (char *)&disk_thread_status,
							      sizeof(status_t));
				}
				break;
			default:
				fprintf(stderr, "disk thread: received unexpected command %d\n", recv_cmd);
				break;
			}

		} else if (end_of_file)
			goto sleep;

		if (!info->is_streaming)
			goto sleep;

		n_space = jack_ringbuffer_write_space(rb) / (2 * sample_size);
		while (n_src < 0.95 * n_space) {
			
			src_ratio = (double)info->out_SR / (double)info->in_SR;
			n_src_prev = n_src;
			want_read = floor((n_space - n_src) / src_ratio);

			if (want_read > MAX_RATIO * info->rb_size)
				want_read = MAX_RATIO * info->rb_size;
			
			n_read = read_file(arg, readbuf, want_read);
			if (n_read < want_read)
				end_of_file = 1;
			
			if (info->is_mono) { /* convert to stereo interleaved */
				for (i = 2*n_read - 1; i >= 0; i--) {
					memcpy(readbuf + i * sample_size,
					       readbuf + i/2 * sample_size,
					       sample_size);
				}
			}
			
			if (info->in_SR == info->out_SR) {
				memcpy(framebuf + n_src_prev * 2*sample_size,
				       readbuf, n_read * 2*sample_size);
				n_src += n_read;

			} else { /* do SRC */
#ifdef HAVE_SRC				
				if ((info->in_SR_prev != info->in_SR) ||
				    (src_type_prev != src_type)) { /* reinit SRC */

					src_state = src_delete(src_state);
					if ((src_state = src_new(src_type, 2, &src_error)) == NULL) {
						fprintf(stderr, "disk thread: error: src_new() failed: "
						       "%s.\n", src_strerror(src_error));
						goto done;
					}
					info->in_SR_prev = info->in_SR;
					src_type_prev = src_type;
				}
				
				src_data.input_frames = n_read;
				src_data.data_in = readbuf;
				src_data.src_ratio = src_ratio;
				src_data.data_out = framebuf + n_src_prev * 2*sample_size;
				src_data.output_frames = n_space - n_src_prev;
				if ((src_error = src_process(src_state, &src_data))) {
					fprintf(stderr, "disk thread: SRC error: %s\n",
					       src_strerror(src_error));
					goto done;
				}
				n_src += src_data.output_frames_gen;
#endif /* HAVE_SRC */
			}
			
			if (end_of_file) {

				close_file(arg);
				file_open = 0;

				/* send request for a new filename */
				send_cmd = CMD_FILEREQ;
				jack_ringbuffer_write(rb_disk2gui, &send_cmd, 1);
				goto sleep;
			}
		}

	flush:
		jack_ringbuffer_write(rb, framebuf, n_src * 2*sample_size);

		/* update & send STATUS */
		disk_thread_samples_left -= n_read;
		disk_thread_sample_offset =
			jack_ringbuffer_read_space(rb) / (2 * sample_size);
		disk_thread_status.samples_left = disk_thread_samples_left;
		disk_thread_status.sample_offset = disk_thread_sample_offset / src_ratio;
		if (disk_thread_status.samples_left < 0) {
			disk_thread_status.samples_left = 0;
		}

		if (!jack_ringbuffer_read_space(rb_gui2disk)) {
			send_cmd = CMD_STATUS;
			jack_ringbuffer_write(rb_disk2gui, &send_cmd, sizeof(send_cmd));
			jack_ringbuffer_write(rb_disk2gui, (char *)&disk_thread_status,
					      sizeof(status_t));
		}

		/* cleanup buffer counters */
		n_src = 0;
		n_src_prev = 0;
		end_of_file = 0;
		
	sleep:
		pthread_cond_wait(&disk_thread_wake, &disk_thread_lock);
	}
 done:
	free(readbuf);
	free(framebuf);
#ifdef HAVE_SRC
	src_state = src_delete(src_state);
#endif /* HAVE_SRC */
	pthread_mutex_unlock(&disk_thread_lock);
	return 0;
}



/* OSS output thread */
#ifdef HAVE_OSS
void *
oss_thread(void * arg) {

        jack_nframes_t i;
        thread_info_t * info = (thread_info_t *)arg;
	int bufsize = 1024;
        int n_avail;
	int ioctl_status;
	char recv_cmd;

	int fd_oss = info->fd_oss;
	short * oss_short_buf;
	jack_nframes_t rb_size = info->rb_size;

	struct timespec req_time;
	struct timespec rem_time;
	req_time.tv_sec = 0;
        req_time.tv_nsec = 100000000;

	pthread_mutex_lock(&output_thread_lock);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	if ((info->oss_short_buf = malloc(2*bufsize * sizeof(short))) == NULL) {
		fprintf(stderr, "oss_thread: malloc error\n");
		exit(1);
	}
	oss_short_buf = info->oss_short_buf;

	if ((l_buf = malloc(bufsize * sizeof(LADSPA_Data))) == NULL) {
		fprintf(stderr, "oss_thread: malloc error\n");
		exit(1);
	}
	if ((r_buf = malloc(bufsize * sizeof(LADSPA_Data))) == NULL) {
		fprintf(stderr, "oss_thread: malloc error\n");
		exit(1);
	}
	ladspa_buflen = bufsize;


	while (1) {
		while (jack_ringbuffer_read_space(rb_disk2out)) {
			jack_ringbuffer_read(rb_disk2out, &recv_cmd, 1);
			switch (recv_cmd) {
			case CMD_FLUSH:
				while ((n_avail = jack_ringbuffer_read_space(rb)) > 0) {
					if (n_avail > 2*bufsize * sizeof(short))
						n_avail = 2*bufsize * sizeof(short);
					jack_ringbuffer_read(rb, (char *)oss_short_buf,
							     2*bufsize * sizeof(short));
				}
				goto oss_wake;
				break;
			default:
				fprintf(stderr, "oss_thread: recv'd unknown command %d\n", recv_cmd);
				break;
			}
		}

		if ((n_avail = jack_ringbuffer_read_space(rb) / (2*sample_size)) == 0) {
			pthread_mutex_unlock(&output_thread_lock);
			nanosleep(&req_time, &rem_time);
			pthread_mutex_lock(&output_thread_lock);
			goto oss_wake;
		}

		if (n_avail > bufsize)
			n_avail = bufsize;
		
		for (i = 0; i < n_avail; i++) {
			jack_ringbuffer_read(rb, (char *)&(l_buf[i]), sample_size);
			jack_ringbuffer_read(rb, (char *)&(r_buf[i]), sample_size);
		}

		if (ladspa_is_postfader) {
			for (i = 0; i < n_avail; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}

		if (n_avail < bufsize) {
			for (i = n_avail; i < bufsize; i++) {
				l_buf[i] = 0.0f;
				r_buf[i] = 0.0f;
			}
		}

		/* plugin processing */
		pthread_mutex_lock(&plugin_lock);
		for (i = 0; i < n_plugins; i++) {
			if (plugin_vect[i]->is_bypassed)
				continue;

			if (plugin_vect[i]->handle) {
				plugin_vect[i]->descriptor->run(plugin_vect[i]->handle, ladspa_buflen);
			}
			if (plugin_vect[i]->handle2) {
				plugin_vect[i]->descriptor->run(plugin_vect[i]->handle2, ladspa_buflen);
			}
		}
		pthread_mutex_unlock(&plugin_lock);

		if (!ladspa_is_postfader) {
			for (i = 0; i < bufsize; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}

		for (i = 0; i < bufsize; i++) {
			if (l_buf[i] > 1.0)
				l_buf[i] = 1.0;
			else if (l_buf[i] < -1.0)
				l_buf[i] = -1.0;

			if (r_buf[i] > 1.0)
				r_buf[i] = 1.0;
			else if (r_buf[i] < -1.0)
				r_buf[i] = -1.0;

			oss_short_buf[2*i] = floorf(32767.0 * l_buf[i]);
			oss_short_buf[2*i+1] = floorf(32767.0 * r_buf[i]);
		}

		/* write data to audio device */
		pthread_mutex_unlock(&output_thread_lock);
		ioctl_status = write(fd_oss, oss_short_buf, 2*n_avail * sizeof(short));
		pthread_mutex_lock(&output_thread_lock);
		if (ioctl_status != 2*n_avail * sizeof(short))
			fprintf(stderr, "oss_thread: Error writing to audio device\n");

		/* wake up disk thread if 1/4 of rb data has been read */
		/* note that 1 frame = 8 bytes so 8 * info->rb_size equals the full data amount */
	oss_wake:
		if (jack_ringbuffer_read_space(rb) < 6 * rb_size) {
			if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
				pthread_cond_signal(&disk_thread_wake);
				pthread_mutex_unlock(&disk_thread_lock);
			}
		}		
	}
        /* NOTREACHED -- this thread will be cancelled from main thread on exit */
	return 0;
}
#endif /* HAVE_OSS */



/* ALSA output thread */
#ifdef HAVE_ALSA
void *
alsa_thread(void * arg) {

        jack_nframes_t i;
        thread_info_t * info = (thread_info_t *)arg;
	snd_pcm_sframes_t n_written = 0;
	int bufsize = 1024;
        int n_avail;
	char recv_cmd;

	int is_output_32bit = info->is_output_32bit;
	short * alsa_short_buf = NULL;
	int * alsa_int_buf = NULL;

	jack_nframes_t rb_size = info->rb_size;
	snd_pcm_t * pcm_handle = info->pcm_handle;

	struct timespec req_time;
	struct timespec rem_time;
	req_time.tv_sec = 0;
        req_time.tv_nsec = 100000000;

	pthread_mutex_lock(&output_thread_lock);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	if (is_output_32bit) {
		if ((info->alsa_int_buf = malloc(2*bufsize * sizeof(int))) == NULL) {
			fprintf(stderr, "alsa_thread: malloc error\n");
			exit(1);
		}
		alsa_int_buf = info->alsa_int_buf;
	} else {
		if ((info->alsa_short_buf = malloc(2*bufsize * sizeof(short))) == NULL) {
			fprintf(stderr, "alsa_thread: malloc error\n");
			exit(1);
		}
		alsa_short_buf = info->alsa_short_buf;
	}

	if ((l_buf = malloc(bufsize * sizeof(LADSPA_Data))) == NULL) {
		fprintf(stderr, "alsa_thread: malloc error\n");
		exit(1);
	}
	if ((r_buf = malloc(bufsize * sizeof(LADSPA_Data))) == NULL) {
		fprintf(stderr, "alsa_thread: malloc error\n");
		exit(1);
	}
	ladspa_buflen = bufsize;


	while (1) {
		while (jack_ringbuffer_read_space(rb_disk2out)) {
			jack_ringbuffer_read(rb_disk2out, &recv_cmd, 1);
			switch (recv_cmd) {
			case CMD_FLUSH:
				if (is_output_32bit) {
					while ((n_avail = jack_ringbuffer_read_space(rb)) > 0) {
						if (n_avail > 2*bufsize * sizeof(int))
							n_avail = 2*bufsize * sizeof(int);
						jack_ringbuffer_read(rb, (char *)alsa_int_buf,
								     2*bufsize * sizeof(int));
					}
				} else {
					while ((n_avail = jack_ringbuffer_read_space(rb)) > 0) {
						if (n_avail > 2*bufsize * sizeof(short))
							n_avail = 2*bufsize * sizeof(short);
						jack_ringbuffer_read(rb, (char *)alsa_short_buf,
								     2*bufsize * sizeof(short));
					}
				}
				goto alsa_wake;
				break;
			default:
				fprintf(stderr, "alsa_thread: recv'd unknown command %d\n", recv_cmd);
				break;
			}
		}

		if ((n_avail = jack_ringbuffer_read_space(rb) / (2*sample_size)) == 0) {
			pthread_mutex_unlock(&output_thread_lock);
			nanosleep(&req_time, &rem_time);
			pthread_mutex_lock(&output_thread_lock);
			goto alsa_wake;
		}

		if (n_avail > bufsize)
			n_avail = bufsize;

		for (i = 0; i < n_avail; i++) {
			jack_ringbuffer_read(rb, (char *)&(l_buf[i]), sample_size);
			jack_ringbuffer_read(rb, (char *)&(r_buf[i]), sample_size);
		}

		if (ladspa_is_postfader) {
			for (i = 0; i < n_avail; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}

		if (n_avail < bufsize) {
			for (i = n_avail; i < bufsize; i++) {
				l_buf[i] = 0.0f;
				r_buf[i] = 0.0f;
			}
		}
		
		/* plugin processing */
		pthread_mutex_lock(&plugin_lock);
		for (i = 0; i < n_plugins; i++) {
			if (plugin_vect[i]->is_bypassed)
				continue;
			
			if (plugin_vect[i]->handle) {
				plugin_vect[i]->descriptor->run(plugin_vect[i]->handle, ladspa_buflen);
			}
			if (plugin_vect[i]->handle2) {
				plugin_vect[i]->descriptor->run(plugin_vect[i]->handle2, ladspa_buflen);
			}
		}
		pthread_mutex_unlock(&plugin_lock);
		
		if (!ladspa_is_postfader) {
			for (i = 0; i < bufsize; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}

		if (is_output_32bit) {
			for (i = 0; i < bufsize; i++) {
				if (l_buf[i] > 1.0)
					l_buf[i] = 1.0;
				else if (l_buf[i] < -1.0)
					l_buf[i] = -1.0;
				
				if (r_buf[i] > 1.0)
					r_buf[i] = 1.0;
				else if (r_buf[i] < -1.0)
					r_buf[i] = -1.0;
				
				alsa_int_buf[2*i] = floorf(2147483647.0 * l_buf[i]);
				alsa_int_buf[2*i+1] = floorf(2147483647.0 * r_buf[i]);
			}

			/* write data to audio device */
			pthread_mutex_unlock(&output_thread_lock);
			if ((n_written = snd_pcm_writei(pcm_handle, alsa_int_buf, n_avail)) != n_avail) {
				snd_pcm_prepare(pcm_handle);
			}
			pthread_mutex_lock(&output_thread_lock);

		} else {
			for (i = 0; i < bufsize; i++) {
				if (l_buf[i] > 1.0)
					l_buf[i] = 1.0;
				else if (l_buf[i] < -1.0)
					l_buf[i] = -1.0;
				
				if (r_buf[i] > 1.0)
					r_buf[i] = 1.0;
				else if (r_buf[i] < -1.0)
					r_buf[i] = -1.0;

				alsa_short_buf[2*i] = floorf(32768.0 * l_buf[i]);
				alsa_short_buf[2*i+1] = floorf(32768.0 * r_buf[i]);
			}

			/* write data to audio device */
			pthread_mutex_unlock(&output_thread_lock);
			if ((n_written = snd_pcm_writei(pcm_handle, alsa_short_buf, n_avail)) != n_avail) {
				snd_pcm_prepare(pcm_handle);
			}
			pthread_mutex_lock(&output_thread_lock);
		}
		
		/* wake up disk thread if 1/4 of rb data has been read */
		/* note that 1 frame = 8 bytes so 8 * info->rb_size equals the full data amount */
	alsa_wake:
		if (jack_ringbuffer_read_space(rb) < 6 * rb_size) {
			if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
				pthread_cond_signal(&disk_thread_wake);
				pthread_mutex_unlock(&disk_thread_lock);
			}
		}		
	}
        /* NOTREACHED -- this thread will be cancelled from main thread on exit */
	return 0;
}
#endif /* HAVE_ALSA */



/* JACK output function */
int
process(jack_nframes_t nframes, void * arg) {

	jack_nframes_t i;
	jack_nframes_t n_avail;
	thread_info_t * info = (thread_info_t *) arg;
	jack_default_audio_sample_t * out1 = jack_port_get_buffer(out_L_port, nframes);
	jack_default_audio_sample_t * out2 = jack_port_get_buffer(out_R_port, nframes);

	int flushing = 0;
	char recv_cmd;

	pthread_mutex_lock(&output_thread_lock);

	if (jack_nframes != nframes) {
		if ((l_buf = realloc(l_buf, nframes * sizeof(LADSPA_Data))) == NULL) {
			fprintf(stderr, "jack process: realloc error\n");
			exit(1);
		}
		if ((r_buf = realloc(r_buf, nframes * sizeof(LADSPA_Data))) == NULL) {
			fprintf(stderr, "jack process: realloc error\n");
			exit(1);
		}
		ladspa_buflen = jack_nframes = nframes;
	}

	while (jack_ringbuffer_read_space(rb_disk2out)) {
		jack_ringbuffer_read(rb_disk2out, &recv_cmd, 1);
		switch (recv_cmd) {
		case CMD_FLUSH:
			flushing = 1;
			break;
		default:
			fprintf(stderr, "jack process(): recv'd unknown command %d\n", recv_cmd);
			break;
		}
	}

 flush_begin:
	n_avail = jack_ringbuffer_read_space(rb) / (2*sample_size);
	if (n_avail > nframes)
		n_avail = nframes;

	for (i = 0; i < n_avail; i++) {
		jack_ringbuffer_read(rb, (char *)&(l_buf[i]), sample_size);
		jack_ringbuffer_read(rb, (char *)&(r_buf[i]), sample_size);
	}

	if (n_avail < nframes) {
		for (i = n_avail; i < nframes; i++) {
			l_buf[i] = 0.0f;
			r_buf[i] = 0.0f;
		}
	}

	if (flushing) {
		for (i = 0; i < n_avail; i++) {
                        l_buf[i] = 0.0f;
                        r_buf[i] = 0.0f;
		}
	} else {
		if (ladspa_is_postfader) {
			for (i = 0; i < n_avail; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}

		/* plugin processing */
		pthread_mutex_lock(&plugin_lock);
		for (i = 0; i < n_plugins; i++) {
			if (plugin_vect[i]->is_bypassed)
				continue;
			
			if (plugin_vect[i]->handle) {
				plugin_vect[i]->descriptor->run(plugin_vect[i]->handle, ladspa_buflen);
			}
			if (plugin_vect[i]->handle2) {
				plugin_vect[i]->descriptor->run(plugin_vect[i]->handle2, ladspa_buflen);
			}
		}
		pthread_mutex_unlock(&plugin_lock);

		if (!ladspa_is_postfader) {
			for (i = 0; i < nframes; i++) {
				l_buf[i] *= left_gain;
				r_buf[i] *= right_gain;
			}
		}
	}

	for (i = 0; i < nframes; i++) {
		out1[i] = l_buf[i];
		out2[i] = r_buf[i];
	}


	if ((flushing) && (jack_ringbuffer_read_space(rb)))
		goto flush_begin;

	pthread_mutex_unlock(&output_thread_lock);

	/* wake up disk thread if 1/4 of rb data has been read */
	/* note that 1 frame = 8 bytes so 8 * info->rb_size equals the full data amount */
	if (jack_ringbuffer_read_space(rb) < 6 * info->rb_size) {
		if (pthread_mutex_trylock(&disk_thread_lock) == 0) {
			pthread_cond_signal(&disk_thread_wake);
			pthread_mutex_unlock(&disk_thread_lock);
		}
	}

	return 0;
}


void
jack_shutdown(void *arg) {

	pthread_mutex_lock(&output_thread_lock);
	jack_is_shutdown = 1;
	pthread_mutex_unlock(&output_thread_lock);
}





#ifdef HAVE_OSS
void
oss_init(thread_info_t * info) {

	int ioctl_arg;
	int ioctl_status;

        if (info->out_SR > MAX_SAMPLERATE) {
                fprintf(stderr, "\nThe sample rate you set (%ld Hz) is higher than MAX_SAMPLERATE.\n",
		       info->out_SR);
                fprintf(stderr, "This is an arbitrary limit, which you may safely enlarge "
                       "if you really need to.\n");
                fprintf(stderr, "Currently MAX_SAMPLERATE = %d Hz.\n", MAX_SAMPLERATE);
                exit(1);
        }

	/* open dsp device */
	info->fd_oss = open(device_name, O_WRONLY, 0);
	if (info->fd_oss < 0) {
		fprintf(stderr, "oss_init: open of %s ", device_name);
		perror("failed");
		exit(1);
	}

	ioctl_arg = 16; /* bitdepth */
	ioctl_status = ioctl(info->fd_oss, SOUND_PCM_WRITE_BITS, &ioctl_arg);
	if (ioctl_status == -1) {
		perror("oss_init: SOUND_PCM_WRITE_BITS ioctl failed");
		exit(1);
	}
	if (ioctl_arg != 16) {
		perror("oss_init: unable to set sample size");
		exit(1);
	}

	ioctl_arg = 2;  /* stereo */
	ioctl_status = ioctl(info->fd_oss, SOUND_PCM_WRITE_CHANNELS, &ioctl_arg);
	if (ioctl_status == -1) {
		perror("oss_init: SOUND_PCM_WRITE_CHANNELS ioctl failed");
		exit(1);
	}
	if (ioctl_arg != 2) {
		perror("oss_init: unable to set number of channels");
		exit(1);
	}

	ioctl_arg = info->out_SR;
	ioctl_status = ioctl(info->fd_oss, SOUND_PCM_WRITE_RATE, &ioctl_arg);
	if (ioctl_status == -1) {
		perror("oss_init: SOUND_PCM_WRITE_RATE ioctl failed");
		exit(1);
	}
	if (ioctl_arg != info->out_SR) {
		fprintf(stderr, "oss_init: unable to set sample_rate to %ld\n", info->out_SR);
	}

	/* start OSS output thread */
	pthread_create(&info->oss_thread_id, NULL, oss_thread, info);
}
#endif /* HAVE_OSS */



#ifdef HAVE_ALSA
void
alsa_init(thread_info_t * info) {

	unsigned rate;
	int dir = 0;


	info->stream = SND_PCM_STREAM_PLAYBACK;
	snd_pcm_hw_params_alloca(&info->hwparams);
	if (snd_pcm_open(&info->pcm_handle, info->pcm_name, info->stream, 0) < 0) {
		fprintf(stderr, "alsa_init: error opening PCM device %s\n", info->pcm_name);
		exit(1);
	}
	
	if (snd_pcm_hw_params_any(info->pcm_handle, info->hwparams) < 0) {
		fprintf(stderr, "alsa_init: cannot configure this PCM device.\n");
		exit(1);
	}

	if (snd_pcm_hw_params_set_periods_integer(info->pcm_handle, info->hwparams) < 0) {
		fprintf(stderr, "alsa_init: cannot set period size to integer value.\n");
		exit(1);
	}

	if (snd_pcm_hw_params_set_access(info->pcm_handle, info->hwparams,
					 SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		fprintf(stderr, "alsa_init: error setting access to SND_PCM_ACCESS_RW_INTERLEAVED.\n");
		exit(1);
	}
  
	info->is_output_32bit = 1;
	if (snd_pcm_hw_params_set_format(info->pcm_handle, info->hwparams, SND_PCM_FORMAT_S32) < 0) {
		fprintf(stderr, "alsa_init: unable to open 32 bit output, falling back to 16 bit...\n");
		if (snd_pcm_hw_params_set_format(info->pcm_handle, info->hwparams, SND_PCM_FORMAT_S16) < 0) {
			fprintf(stderr, "alsa_init: unable to open 16 bit output, exiting.\n");
			exit(1);
		}
		info->is_output_32bit = 0;
	}

	rate = info->out_SR;
	dir = 0;
	if (snd_pcm_hw_params_set_rate_near(info->pcm_handle, info->hwparams, &rate, &dir) < 0) {
		fprintf(stderr, "alsa_init: can't set sample rate to %u.\n", rate);
		exit(1);
	}
	
	if (rate != info->out_SR) {
		fprintf(stderr, "Requested sample rate (%ld Hz) cannot be set, ", info->out_SR);
		fprintf(stderr, "using closest available rate (%d Hz).\n", rate);
		info->out_SR = rate;
	}

	if (snd_pcm_hw_params_set_channels(info->pcm_handle, info->hwparams, 2) < 0) {
		fprintf(stderr, "alsa_init: error setting channels.\n");
		exit(1);
	}

	if (snd_pcm_hw_params_set_periods(info->pcm_handle, info->hwparams, nperiods, 0) < 0) {
		fprintf(stderr, "alsa_init: error setting nperiods to %d.\n", nperiods);
		exit(1);
	}
  
	if (snd_pcm_hw_params_set_buffer_size(info->pcm_handle, info->hwparams,
					      (period * nperiods)>>2) < 0) {
		fprintf(stderr, "alsa_init: error setting buffersize to %d.\n", (period * nperiods)>>2);
		fprintf(stderr, "Parameters were: nperiods = %d, period = %d\n", nperiods, period);
		exit(1);
	}

	if (snd_pcm_hw_params(info->pcm_handle, info->hwparams) < 0) {
		fprintf(stderr, "alsa_init: error setting HW params.\n");
		exit(1);
	}
}
#endif /* HAVE_ALSA */


void jack_init(thread_info_t * info) {

	if (client_name == NULL)
		client_name = strdup("aqualung");

	if ((jack_client = jack_client_new(client_name)) == 0) {
                fprintf(stderr, "\nAqualung couldn't connect to JACK.\n");
                fprintf(stderr, "There are several possible reasons:\n"
                        "\t1) JACK is not running.\n"
                        "\t2) JACK is running as another user, perhaps root.\n"
                        "\t3) There is already another client called '%s'. (Use the -c option!)\n",
			client_name);
                fprintf(stderr, "Please consider the possibilities, and perhaps (re)start JACK.\n");
		exit(1);
	}

	jack_set_process_callback(jack_client, process, info);
	jack_on_shutdown(jack_client, jack_shutdown, info);

        if ((info->out_SR = jack_get_sample_rate(jack_client)) > MAX_SAMPLERATE) {
                fprintf(stderr, "\nThe JACK sample rate (%ld Hz) is higher than MAX_SAMPLERATE.\n",
		       info->out_SR);
                fprintf(stderr, "This is an arbitrary limit, which you may safely enlarge "
                       "if you really need to.\n");
                fprintf(stderr, "Currently MAX_SAMPLERATE = %d Hz.\n", MAX_SAMPLERATE);
                jack_client_close(jack_client);
                exit(1);
        }
        out_L_port = jack_port_register(jack_client, "out_L", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        out_R_port = jack_port_register(jack_client, "out_R", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);


	jack_nframes = 1024;
	if ((l_buf = malloc(jack_nframes * sizeof(LADSPA_Data))) == NULL) {
		fprintf(stderr, "jack_init: malloc error\n");
		exit(1);
	}
	if ((r_buf = malloc(jack_nframes * sizeof(LADSPA_Data))) == NULL) {
		fprintf(stderr, "jack_init: malloc error\n");
		exit(1);
	}
	ladspa_buflen = jack_nframes;
}
 
 
 
void
load_default_cl(int * argc, char *** argv) {

	int i = 0;
	char * home;
	xmlDocPtr doc;
        xmlNodePtr cur;
        xmlNodePtr root;
        xmlChar * key;
        FILE * f;
	char confdir[MAXLEN];
        char config_file[MAXLEN];
	char default_param[MAXLEN];
	char cl[MAXLEN];
	int ret;

        if (!(home = getenv("HOME"))) {
                fprintf(stderr, "Warning: $HOME not set, using \".\" (current directory) as home\n");
                home = ".";
        }

        sprintf(confdir, "%s/.aqualung", home);
        if ((ret = chdir(confdir)) != 0) {
                if (errno == ENOENT) {
                        fprintf(stderr, "Creating directory %s\n", confdir);
                        mkdir(confdir, S_IRUSR | S_IWUSR | S_IXUSR);
                        chdir(confdir);
                } else {
                        fprintf(stderr, "An error occured while attempting chdir(\"%s\"). errno = %d\n",
                                confdir, errno);
                }
        }

        sprintf(config_file, "%s/.aqualung/config.xml", home);
        if ((f = fopen(config_file, "rt")) == NULL) {
                fprintf(stderr, "No config.xml -- creating empty one: %s\n", config_file);
                fprintf(stderr, "Wired-in defaults will be used.\n");
                doc = xmlNewDoc("1.0");
                root = xmlNewNode(NULL, "aqualung_config");
                xmlDocSetRootElement(doc, root);
                xmlSaveFormatFile(config_file, doc, 1);
		*argc = 1;
                return;
        }
        fclose(f);

        doc = xmlParseFile(config_file);
        if (doc == NULL) {
                fprintf(stderr, "An XML error occured while parsing %s\n", config_file);
                return;
        }
        cur = xmlDocGetRootElement(doc);
        if (cur == NULL) {
                fprintf(stderr, "load_config: empty XML document\n");
                xmlFreeDoc(doc);
                return;
        }

        if (xmlStrcmp(cur->name, (const xmlChar *)"aqualung_config")) {
                fprintf(stderr, "load_config: XML document of the wrong type, "
                        "root node != aqualung_config\n");
                xmlFreeDoc(doc);
                return;
        }

        default_param[0] = '\0';

        cur = cur->xmlChildrenNode;
        while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)"default_param"))) {
                        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                        if (key != NULL)
                                strncpy(default_param, key, MAXLEN-1);
                        xmlFree(key);
                }
                cur = cur->next;
        }

        xmlFreeDoc(doc);

	snprintf(cl, MAXLEN-1, "aqualung %s", default_param);
	while (cl[i] != '\0') {
		++(*argc);
		if ((*argv = realloc(*argv, *argc * sizeof(char *))) == NULL) {
			fprintf(stderr, "realloc() failed in load_default_cl()\n");
			exit(1);
		}
		(*argv)[*argc-1] = cl+i;
		
		while ((cl[i] != ' ') && (cl[i] != '\0'))
			++i;
		
		if (cl[i] == ' ')
			cl[i++] = '\0';
	}
	if ((*argv = realloc(*argv, (*argc+1) * sizeof(char *))) == NULL) {
		fprintf(stderr, "realloc() failed in load_default_cl()\n");
		exit(1);
	}
	(*argv)[*argc] = NULL;
	
        return;
}



int
main(int argc, char ** argv) {

	int parse_run;
	int * argc_opt = NULL;
	char *** argv_opt = NULL;
	int argc_def = 0;
	char ** argv_def = NULL;

	thread_info_t thread_info;
	struct sched_param param;

	const char ** ports_out;
	int c;
	int longopt_index = 0;
	extern int optind, opterr;
	int show_version = 0;
	int show_usage = 0;
	char * output_str = NULL;
	int rate = 0;
	int auto_connect = 0;
	int try_realtime = 0;
	int priority = 1;

	char rcmd;
	int no_session = -1;
	int back = 0;
	int play = 0;
	int pause = 0;
	int stop = 0;
	int fwd = 0;
	int enqueue = 0;
	int remote_quit = 0;


	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	setup_app_socket();

	if (getcwd(cwd, MAXLEN) == NULL) {
		fprintf(stderr, "main(): warning: getcwd() returned NULL, using . as cwd\n");
		strcpy(cwd, ".");
	}


	char * optstring = "vho:d:c:n:p:r:aRP:s::N:BLUTFEQ";
	struct option long_options[] = {
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, 'h' },
		{ "output", 1, 0, 'o' },
		{ "device", 1, 0, 'd' },
		{ "client", 1, 0, 'c' },
		{ "nperiods", 1, 0, 'n'},
		{ "period", 1, 0, 'p'},
		{ "rate", 1, 0, 'r' },
		{ "auto", 0, 0, 'a' },
		{ "realtime", 0, 0, 'R' },
		{ "priority", 1, 0, 'P' },
		{ "srctype", 2, 0, 's' },

		{ "session", 1, 0, 'N' },
		{ "back", 0, 0, 'B' },
		{ "play", 0, 0, 'L' },
		{ "pause", 0, 0, 'U' },
		{ "stop", 0, 0, 'T' },
		{ "fwd", 0, 0, 'F' },
		{ "enqueue", 0, 0, 'E' },
		{ "quit", 0, 0, 'Q' },

		{ 0, 0, 0, 0 }
	};


	load_default_cl(&argc_def, &argv_def);

	for (parse_run = 0; parse_run < 2; parse_run++) {

		if ((!parse_run) && (argc_def == 1))
			++parse_run;

		switch (parse_run) {
		case 0:
			argc_opt = &argc_def;
			argv_opt = &argv_def;
			break;
		case 1:
			argc_opt = &argc;
			argv_opt = &argv;
			break;
		}

		optind = opterr = 0;
		longopt_index = opterr = 0;
		while ((c = getopt_long(*argc_opt, *argv_opt, optstring,
					long_options, &longopt_index)) != -1) {
			switch (c) {
			case 1:
				/* getopt signals end of '-' options */
				break;
				
			case 'h':
				show_usage++;
				break;
			case 'v':
				show_version++;
				break;
			case 'o':
				output_str = strdup(optarg);
				if (strcmp(output_str, "oss") == 0) {
#ifdef HAVE_OSS
					output = OSS_DRIVER;
#else
					fprintf(stderr,
						"You selected OSS output, but this instance of Aqualung "
						"is compiled\n"
						"without OSS output support. Type aqualung -v to get a "
						"list of\n"
						"compiled-in features.\n");
					exit(1);
#endif /* HAVE_OSS*/
					break;
				}
				if (strcmp(output_str, "alsa") == 0) {
#ifdef HAVE_ALSA
					output = ALSA_DRIVER;
#else
					fprintf(stderr,
						"You selected ALSA output, but this instance of Aqualung "
						"is compiled\n"
						"without ALSA output support. Type aqualung -v to get a "
						"list of\n"
						"compiled-in features.\n");
					exit(1);
#endif /* HAVE_ALSA */
					break;
				}
				if (strcmp(output_str, "jack") == 0) {
					output = JACK_DRIVER;
					break;
				}
			case 'd':
				device_name = strdup(optarg);
				break;
			case 'c':
				client_name = strdup(optarg);
				break;
			case 'n':
#ifdef HAVE_ALSA
				nperiods = atoi(optarg);
#endif /* HAVE_ALSA */
				break;
			case 'p':
#ifdef HAVE_ALSA
				period = atoi(optarg);
#endif /* HAVE_ALSA */
				break;
			case 'r':
				rate = atoi(optarg);
				break;
			case 'a':
				auto_connect = 1;
				break;
			case 'R':
				try_realtime = 1;
				break;
			case 'P':
				priority = atoi(optarg);
				break;
			case 's':
#ifdef HAVE_SRC
				if (optarg) {
					src_type = atoi(optarg);
					++src_type_parsed;
				} else {
					int i = 0;
					
					fprintf(stderr, "List of available Sample Rate Converters:\n\n");
					while (src_get_name(i)) {
						fprintf(stderr, "Converter #%d: %s\n%s\n\n",
						       i, src_get_name(i), src_get_description(i));
						++i;
					}
					fprintf(stderr,
						"Type aqualung --srctype=n ... or aqualung -sn ... to choose "
						"Converter #n.\n\n");
					exit(0);
				}
#else
				if (optarg) {
					fprintf(stderr,
						"You attempted to set the type of the internal Sample Rate "
						"Converter,"
						"\nbut this instance of Aqualung is compiled without "
						"support for"
						"\ninternal Sample Rate Conversion. Type aqualung -v to get "
						"a list"
						"\nof compiled-in features.\n");
				} else {
					fprintf(stderr,
						"You attempted to list the available types for the internal "
						"Sample"
						"\nRate Converter, but this instance of Aqualung is "
						"compiled without"
						"\nsupport for internal Sample Rate Conversion. Type "
						"aqualung -v to"
						"\nget a list of compiled-in features.\n");
				}
				exit(1);
#endif /* HAVE_SRC */
				break;
				
			case 'N':
				no_session = atoi(optarg);
				break;
			case 'B':
				back++;
				break;
			case 'L':
				play++;
				break;
			case 'U':
				pause++;
				break;
			case 'T':
				stop++;
				break;
			case 'F':
				fwd++;
				break;
			case 'E':
				enqueue++;
				break;
			case 'Q':
				remote_quit++;
				break;

			default:
				show_usage++;
				break;
			}
		}
	}
	
	if (show_version) {
		fprintf(stderr,	"Aqualung -- Music player for GNU/Linux\n");
		fprintf(stderr, "Build version: %s\n", aqualung_version);
		fprintf(stderr,
			"(C) 2004 Tom Szilagyi <tszilagyi@users.sourceforge.net>\n"
			"This is free software, and you are welcome to redistribute it\n"
			"under certain conditions; see the file COPYING for details.\n");

		fprintf(stderr, "\nThis Aqualung binary is compiled with:\n"
			"\n\tFile format support:\n");

		fprintf(stderr, "\t\tsndfile (WAV, AIFF, etc.)           : ");
#ifdef HAVE_SNDFILE
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_SNDFILE */

		fprintf(stderr, "\t\tFree Lossless Audio Codec (FLAC)    : ");
#ifdef HAVE_FLAC
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_FLAC */

		fprintf(stderr, "\t\tOgg Vorbis                          : ");
#ifdef HAVE_OGG_VORBIS
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_OGG_VORBIS */

		fprintf(stderr, "\t\tMPEG Audio (MPEG 1-2.5 Layer I-III) : ");
#ifdef HAVE_MPEG
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_MPEG */

                fprintf(stderr, "\t\tMOD Audio (MOD, S3M, XM, IT, etc.)  : ");
#ifdef HAVE_MOD
                fprintf(stderr, "yes\n");
#else
                fprintf(stderr, "no\n");
#endif /* HAVE_MOD */

		fprintf(stderr, "\n\tOutput driver support:\n");

		fprintf(stderr, "\t\tOSS Audio                           : ");
#ifdef HAVE_OSS
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_OSS */

		fprintf(stderr, "\t\tALSA Audio                          : ");
#ifdef HAVE_ALSA
		fprintf(stderr, "yes\n");
#else
		fprintf(stderr, "no\n");
#endif /* HAVE_ALSA */

		fprintf(stderr, "\t\tJACK Audio Server                   : yes (always)\n");

		fprintf(stderr, "\n\tInternal Sample Rate Converter support      : ");
#ifdef HAVE_SRC
		fprintf(stderr, "yes\n\n");
#else
		fprintf(stderr, "no\n\n");
#endif /* HAVE_SRC */

		exit(1);
	}

	if (show_usage)
		goto show_usage_;


	if (back) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_BACK;
		send_message_to_session(no_session, &rcmd, 1);
		exit(0);
	}
	if (play) {
		if (no_session == -1) {
			immediate_start = 1;
		} else {
			rcmd = RCMD_PLAY;
			send_message_to_session(no_session, &rcmd, 1);
			exit(0);
		}
	}
	if (pause) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_PAUSE;
		send_message_to_session(no_session, &rcmd, 1);
		exit(0);
	}
	if (stop) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_STOP;
		send_message_to_session(no_session, &rcmd, 1);
		exit(0);
	}
	if (fwd) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_FWD;
		send_message_to_session(no_session, &rcmd, 1);
		exit(0);
	}

	if ((remote_quit) && (no_session != -1)) {
		if (no_session == -1)
			no_session = 0;
		rcmd = RCMD_QUIT;
		send_message_to_session(no_session, &rcmd, 1);
		exit(1);
	}

	{
		int i;
		char buffer[MAXLEN];
		char fullname[MAXLEN];
		char * home;
		char * path;

		if (no_session != -1) {
			for (i = optind; argv[i] != NULL; i++) {
				
				switch (argv[i][0]) {
				case '/':
					strcpy(fullname, argv[i]);
					break;
				case '~':
					path = argv[i];
					++path;
					if (!(home = getenv("HOME"))) {
						fprintf(stderr,	"main(): cannot resolve home directory\n");
						return 1;
					}
					snprintf(fullname, MAXLEN-1, "%s/%s", home, path);
					break;
				default:
					snprintf(fullname, MAXLEN-1, "%s/%s", cwd, argv[i]);
					break;
				}

				if ((enqueue) || (i > optind)) {
					buffer[0] = RCMD_ENQUEUE;
					buffer[1] = '\0';
					strncat(buffer, fullname, MAXLEN-1);
					send_message_to_session(no_session, buffer, strlen(buffer));
				} else {
					buffer[0] = RCMD_LOAD;
					buffer[1] = '\0';
					strncat(buffer, fullname, MAXLEN-1);
					send_message_to_session(no_session, buffer, strlen(buffer));
				}
			}
			exit(0);
		}
	}


 show_usage_:
	if (show_usage || (output == 0)) {
		fprintf(stderr,	"Aqualung -- Music player for GNU/Linux\n");
		fprintf(stderr, "Build version: %s\n", aqualung_version);
		fprintf(stderr,
			"(C) 2004 Tom Szilagyi <tszilagyi@users.sourceforge.net>\n"
			"This is free software, and you are welcome to redistribute it\n"
			"under certain conditions; see the file COPYING for details.\n");

		fprintf(stderr,
			"\nInvocation:\n"
			"aqualung --output (oss|alsa|jack) [options] [file1 [file2 ...]]\n"
			"aqualung --help\n"
			"aqualung --version\n"

			"\nOptions relevant to ALSA output:\n"
			"-d, --device <name>: Set the output device (defaults to plughw:0,0).\n"
			"-p, --period <int>: Set ALSA period size (defaults to 8192).\n"
			"-n, --nperiods <int>: Specify the number of periods in hardware buffer (defaults to 2).\n"
			"-r, --rate <int>: Set the output sample rate.\n"
			"-R, --realtime: Try to use realtime (SCHED_FIFO) scheduling for ALSA output thread.\n"
			"-P, --priority <int>: When running --realtime, set scheduler priority to <int> (defaults to 1).\n"

			"\nOptions relevant to OSS output:\n"
			"-d, --device <name>: Set the output device (defaults to /dev/dsp).\n"
			"-r, --rate <int>: Set the output sample rate.\n"
			
			"\nOptions relevant to JACK output:\n"
			"-a, --auto: Auto-connect output ports to first two hardware playback ports.\n"
			"-c, --client <name>: Set client name (needed if you want to run multiple instances of the program).\n"

			"\nOptions relevant to the Sample Rate Converter:\n"
			"-s[<int>], --srctype[=<int>]: Choose the SRC type, or print the list of available\n"
			"types if no number given. The default is SRC type 4 (Linear Interpolator).\n"

			"\nOptions for remote cue control:\n"
			"-N, --session <int>: Number of Aqualung instance to control.\n"
			"-B, --back: Jump to previous track.\n"
			"-F, --fwd: Jump to next track.\n"
			"-L, --play: Start playing.\n"
			"-U, --pause: Pause playback, or resume if already paused.\n"
			"-T, --stop: Stop playback.\n"
			"-Q, --quit: Terminate remote instance.\n"

			"Note that these options default to the 0-th instance when no -N option is given,\n"
			"except for -L which defaults to the present instance (so as to be able to start\n"
			"playback immediately from the command line).\n"

			"\nOptions for file loading:\n"
			"-E, --enqueue: Don't clear the contents of the playlist when adding new items.\n"

			"\nIf you don't specify a session number via --session, the files will be opened by "
			"the new\ninstance, otherwise they will be sent to the already running instance you "
			"specify.\n"

			"\nExamples:\n"
			"$ aqualung -s3 -o alsa -R -r 48000 -d hw:0,0 -p 2048 -n 2\n"
			"$ aqualung --srctype=1 --output oss --rate 96000\n"
			"$ aqualung -o jack -a -E `find ./ledzeppelin/ -name \"*.flac\"`\n");

		fprintf(stderr, 
			"\nDepending on the compile-time options, not all file formats\n"
			"and output drivers may be usable. Type aqualung -v to get a\n"
			"list of all the compiled-in features.\n\n");

		exit (0);
	}

	
	if ((output == JACK_DRIVER) && (rate > 0)) {
		fprintf(stderr,
			"You attempted to set the output rate for the JACK output.\n"
			"We won't do this; please use the --rate option with the\n"
			"oss and alsa outputs only.\n"
			"In case of the JACK output, the (already running) JACK server\n"
			"will determine the output sample rate to use.\n");
		exit(1);
	}

#ifdef HAVE_OSS
	if ((output == OSS_DRIVER) && (rate == 0)) {
		rate = 44100;
	}
#endif /* HAVE_OSS */

#ifdef HAVE_ALSA
	if ((output == ALSA_DRIVER) && (rate == 0)) {
		rate = 44100;
	}
#endif /* HAVE_ALSA */


	if (device_name == NULL) {
#ifdef HAVE_OSS
		if (output == OSS_DRIVER) {
			device_name = strdup("/dev/dsp");
		}
#endif /* HAVE_OSS */
#ifdef HAVE_ALSA
		if (output == ALSA_DRIVER) {
			device_name = strdup("plughw:0,0");
		}
#endif /* HAVE_ALSA */
	}

#ifdef HAVE_ALSA
	if (output == ALSA_DRIVER) {
		if (period == 0) {
			period = 8192;
		}
		if (nperiods == 0) {
			nperiods = 2;
		}
	}
#endif /* HAVE_ALSA */


	memset(&thread_info, 0, sizeof(thread_info));

	/* JACK */
	if (output == JACK_DRIVER) {
		jack_init(&thread_info);
		rate = thread_info.out_SR;
	}


	/* ALSA */
#ifdef HAVE_ALSA
	if (output == ALSA_DRIVER) {
		thread_info.out_SR = rate;
		thread_info.pcm_name = strdup(device_name);
		alsa_init(&thread_info);
	}
#endif /* HAVE_ALSA */


	/* OSS */
#ifdef HAVE_OSS
	if (output == OSS_DRIVER) {
		thread_info.out_SR = rate;
	}
#endif /* HAVE_OSS */


	thread_info.rb_size = RB_AUDIO_SIZE * thread_info.out_SR / 44100.0;

        rb = jack_ringbuffer_create(2*sample_size * thread_info.rb_size);
	memset(rb->buf, 0, rb->size);


	rb_disk2gui = jack_ringbuffer_create(RB_CONTROL_SIZE);
	memset(rb_disk2gui->buf, 0, rb_disk2gui->size);

	rb_gui2disk = jack_ringbuffer_create(RB_CONTROL_SIZE);
	memset(rb_gui2disk->buf, 0, rb_gui2disk->size);

	rb_disk2out = jack_ringbuffer_create(RB_CONTROL_SIZE);
	memset(rb_disk2out->buf, 0, rb_disk2out->size);

	thread_info.is_streaming = 0;
	thread_info.is_mono = 0;
	thread_info.in_SR = 0;


	/* startup disk thread */
	pthread_create(&thread_info.disk_thread_id, NULL, disk_thread, &thread_info);


	/* OSS */
#ifdef HAVE_OSS
	if (output == OSS_DRIVER) {
		oss_init(&thread_info);
	}
#endif /* HAVE_OSS */


	/* ALSA */
#ifdef HAVE_ALSA
	if (output == ALSA_DRIVER) {
		pthread_create(&thread_info.alsa_thread_id, NULL, alsa_thread, &thread_info);

		if (try_realtime) {
			int x;
			memset(&param, 0, sizeof(param));
			param.sched_priority = priority;
			if ((x = pthread_setschedparam(thread_info.alsa_thread_id,
						       SCHED_FIFO, &param)) != 0) {
				fprintf(stderr,
					"Cannot use real-time scheduling for ALSA output thread (FIFO/%d) "
					"(%d: %s)\n", param.sched_priority, x, strerror(x));
				exit(1);
			}
		}
	}
#endif /* HAVE_ALSA */


	/* JACK */
	if (output == JACK_DRIVER) {
		if (jack_activate(jack_client)) {
			fprintf(stderr, "Cannot activate JACK client.\n");
		}
		
		if (auto_connect) {
			if ((ports_out = jack_get_ports(jack_client, NULL, NULL,
							JackPortIsPhysical|JackPortIsInput)) == NULL) {
				fprintf(stderr, "Cannot find any physical playback ports.\n");
			} else {
				if (jack_connect(jack_client, jack_port_name(out_L_port),
						 ports_out[0])) {
					fprintf(stderr, "Cannot connect out_L port.\n");
				} else {
					fprintf(stderr, "Connected out_L to %s\n", ports_out[0]);
				}
				if (jack_connect(jack_client, jack_port_name(out_R_port),
						 ports_out[1])) {
					fprintf(stderr, "Cannot connect out_R port.\n");
				} else {
					fprintf(stderr, "Connected out_R to %s\n", ports_out[1]);
				}
				free(ports_out);
			}
		}
	}


	create_gui(argc, argv, optind, enqueue, rate, RB_AUDIO_SIZE * rate / 44100.0);
	run_gui(); /* control stays here until user exits program */

	pthread_join(thread_info.disk_thread_id, NULL);

	/* OSS */
#ifdef HAVE_OSS
	if (output == OSS_DRIVER) {
		pthread_cancel(thread_info.oss_thread_id);
		free(thread_info.oss_short_buf);
		close(thread_info.fd_oss);
	}
#endif /* HAVE_OSS */


	/* ALSA */
#ifdef HAVE_ALSA
	if (output == ALSA_DRIVER) {
		pthread_cancel(thread_info.alsa_thread_id);
		snd_pcm_close(thread_info.pcm_handle);
		if (thread_info.is_output_32bit)
			free(thread_info.alsa_int_buf);
		else
			free(thread_info.alsa_short_buf);
	}
#endif /* HAVE_ALSA */


	/* JACK */
	if (output == JACK_DRIVER) {
		jack_client_close(jack_client);
		free(client_name);
	}


	if (device_name != NULL)
		free(device_name);

	free(l_buf);
	free(r_buf);
	jack_ringbuffer_free(rb);
	jack_ringbuffer_free(rb_disk2gui);
	jack_ringbuffer_free(rb_gui2disk);
	jack_ringbuffer_free(rb_disk2out);
	return 0;
}
