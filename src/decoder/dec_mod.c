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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "dec_mod.h"


extern size_t sample_size;


#ifdef HAVE_MOD
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
                jack_ringbuffer_write(pd->rb, (char *)fbuffer,
				      bytes_read/2 * sample_size);
                return 0;
        } else {
		return 1;
	}
}


decoder_t *
mod_decoder_new(file_decoder_t * fdec) {

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

	dec->new = mod_decoder_new;
	dec->delete = mod_decoder_delete;
	dec->open = mod_decoder_open;
	dec->close = mod_decoder_close;
	dec->read = mod_decoder_read;
	dec->seek = mod_decoder_seek;

	return dec;
}


void
mod_decoder_delete(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


int
mod_decoder_open(decoder_t * dec, char * filename) {

	mod_pdata_t * pd = (mod_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	

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
	
	pd->is_eos = 0;
	pd->rb = jack_ringbuffer_create(pd->mp_settings.mChannels * sample_size * RB_MOD_SIZE);
	fdec->channels = pd->mp_settings.mChannels;
	fdec->SR = pd->mp_settings.mFrequency;
	fdec->file_lib = MOD_LIB;
	
	fdec->fileinfo.total_samples = ModPlug_GetLength(pd->mpf)
		/ 1000.0f * pd->mp_settings.mFrequency;
	fdec->fileinfo.format_major = FORMAT_MOD;
	fdec->fileinfo.format_minor = 0;
	fdec->fileinfo.bps = pd->st.st_size * 8000.0f /	ModPlug_GetLength(pd->mpf);
	
	return DECODER_OPEN_SUCCESS;
}


void
mod_decoder_close(decoder_t * dec) {

	mod_pdata_t * pd = (mod_pdata_t *)dec->pdata;

	ModPlug_Unload(pd->mpf);
	if (munmap(pd->fdm, pd->st.st_size) == -1)
		fprintf(stderr, "Error while munmap()'ing MOD Audio file mapping\n");
	close(pd->fd);
	jack_ringbuffer_free(pd->rb);
}


unsigned int
mod_decoder_read(decoder_t * dec, float * dest, int num) {

	mod_pdata_t * pd = (mod_pdata_t *)dec->pdata;

	unsigned int numread = 0;
	unsigned int n_avail = 0;


	while ((jack_ringbuffer_read_space(pd->rb) <
		num * pd->mp_settings.mChannels * sample_size) && (!pd->is_eos)) {

		pd->is_eos = decode_mod(dec);
	}

	n_avail = jack_ringbuffer_read_space(pd->rb) /
		(pd->mp_settings.mChannels * sample_size);

	if (n_avail > num)
		n_avail = num;

	jack_ringbuffer_read(pd->rb, (char *)dest, n_avail *
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
	while (jack_ringbuffer_read_space(pd->rb))
		jack_ringbuffer_read(pd->rb, &flush_dest, sizeof(char));
}


#else
decoder_t *
mod_decoder_new(file_decoder_t * fdec) {

        return NULL;
}
#endif /* HAVE_MOD */
