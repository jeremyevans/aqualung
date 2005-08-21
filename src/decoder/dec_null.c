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

#include "dec_null.h"


decoder_t *
null_decoder_new(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_null.c: null_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(null_pdata_t))) == NULL) {
                fprintf(stderr, "dec_null.c: null_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->new = null_decoder_new;
	dec->delete = null_decoder_delete;
	dec->open = null_decoder_open;
	dec->close = null_decoder_close;
	dec->read = null_decoder_read;
	dec->seek = null_decoder_seek;

	return dec;
}


void
null_decoder_delete(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


int
null_decoder_open(decoder_t * dec, char * filename) {
	/* declarations for easy data access:
	null_pdata_t * pd = (null_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	*/

	/* do whatever is needed to open the file and
	   determine whether this decoder is able to
	   handle it.

	   return one of the following:

	   DECODER_OPEN_SUCCESS : this decoder is able to deal with
	       the file and opening was successful.

	   DECODER_OPEN_BADLIB : this decoder is not appropriate
	       for this file.

	   DECODER_OPEN_FERROR : file nonexistent/nonaccessible, so
	       trying other decoders is pointless.
	*/

	return DECODER_OPEN_BADLIB;
}


void
null_decoder_close(decoder_t * dec) {
	/*
	null_pdata_t * pd = (null_pdata_t *)dec->pdata;
	*/
	/* close the file */
}


unsigned int
null_decoder_read(decoder_t * dec, float * dest, int num) {
	/*
	null_pdata_t * pd = (null_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	*/
	/* read num frames (mono or stereo) from the file.
	   return the number of frames actually read.
	 */

	return 0;
}


void
null_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {
	/*	
	null_pdata_t * pd = (null_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	*/
	/* seek to seek_to_pos sample position in the file. */
}
