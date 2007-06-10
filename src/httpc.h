/*                                                     -*- linux-c -*-
    Copyright (C) 2007 Tom Szilagyi

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


#ifndef _HTTPC_H
#define _HTTPC_H

#define HTTPC_OK                0
#define HTTPC_URL_ERROR        -1
#define HTTPC_CONNECTION_ERROR -2
#define HTTPC_HEADER_ERROR     -3
#define HTTPC_REDIRECT_ERROR   -4

#define HTTPC_SESSION_NORMAL  1
#define HTTPC_SESSION_CHUNKED 2
#define HTTPC_SESSION_STREAM  3

#include "decoder/file_decoder.h"

typedef struct {
	char * status;
	char * location;
	int content_length;
	char * content_type;
	char * transfer_encoding;
	int icy_metaint;
	int icy_br;
	char * icy_genre;
	char * icy_name;
	char * icy_description;
} http_header_t;

typedef struct {
	/* original session parameters */
	char * URL;
	int use_proxy;
	char * proxy;
	int proxy_port;
	char * noproxy_domains;
	
	int sock;
	int is_active;
	http_header_t headers;
	
	int type; /* one of HTTPC_SESSION_* */
	
	/* variables for normal download: */
	long long byte_pos;
	
	/* variables for chunked download: */
	char * chunk_buf;
	int chunk_size;
	int chunk_pos;
	int end_of_data;

	/* variables for stream download: */
	int metapos;

	/* file decoder that uses us - if that is the case */
	file_decoder_t * fdec;
} http_session_t;


int httpc_is_url(const char * str);

http_session_t * httpc_new(void);
void httpc_del(http_session_t * session);

/*  Initiate a HTTP/1.1 request.
 *    URL: string containing location including protocol (http://),
 *         host, port (optional), path.
 *    proxy_URL: proxy URL (NULL if none used)
 *    proxy_port : integer
 *    start_byte : first byte of content we are interested in,
 *                 should be 0L for most cases.
 *
 *  Return: one of HTTPC_*
 */
int httpc_init(http_session_t * session, char * URL, int use_proxy, char * proxy,
	       int proxy_port, char * noproxy_domains, long long start_byte);

int httpc_read(http_session_t * session, char * buf, int num);
int httpc_seek(http_session_t * session, long long offset, int whence);
long long httpc_tell(http_session_t * session);
void httpc_close(http_session_t * session);

int httpc_reconnect(http_session_t * session);

void httpc_add_headers_meta(http_session_t * session, metadata_t * meta);


#endif /* _HTTPC_H */


// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

