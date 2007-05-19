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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h> 

#include "version.h"
#include "common.h"
#include "options.h"
#include "httpc.h"

extern options_t options;

int httpc_is_url(const char * str) {

	if (strlen(str) < 8)
		return 0;
	
	if (str[0] != 'h' && str[0] != 'H') return 0;
	if (str[1] != 't' && str[1] != 'T') return 0;
	if (str[2] != 't' && str[2] != 'T') return 0;
	if (str[3] != 'p' && str[3] != 'P') return 0;
	if (str[4] != ':') return 0;
	if (str[5] != '/') return 0;
	if (str[6] != '/') return 0;

	return 1;
}


void
free_headers(http_header_t * headers) {
	if (headers == NULL)
		return;
	
	if (headers->status != NULL)
		free(headers->status);
	if (headers->location != NULL)
		free(headers->location);
	if (headers->content_type != NULL)
		free(headers->content_type);
	if (headers->transfer_encoding != NULL)
		free(headers->transfer_encoding);
	if (headers->icy_genre != NULL)
		free(headers->icy_genre);
	if (headers->icy_name != NULL)
		free(headers->icy_name);
	if (headers->icy_description != NULL)
		free(headers->icy_description);
}

int
open_socket(char * hostname, unsigned short portnum) {
	
	struct sockaddr_in sa;
	struct hostent * hp;
	int s;
	
	if ((hp = gethostbyname(hostname)) == NULL) {
		printf("open_socket: gethostbyname() failed on %s\n", hostname);
		return -1;
	}
	
	memset(&sa, 0, sizeof(sa));
	memcpy((char *)&sa.sin_addr, hp->h_addr, hp->h_length);
	sa.sin_family = hp->h_addrtype;
	sa.sin_port = htons((u_short)portnum);
	
	if ((s = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0) {
		printf("open_socket: socket(): %s\n", strerror(errno));
		return -1;
	}

	if (connect(s, (struct sockaddr *)&sa, sizeof (sa)) < 0) {
		printf("open_socket: connect(): %s\n", strerror(errno));
		close(s);
		return -1;
	}
	
	printf("socket successfully opened to %s:%d\n", hostname, portnum);
	return s;
}

int
read_socket(int s, char * buf, int n) {
	
	int bcount;
	int br;
	
	bcount = 0;
	br = 0;
	
	while (bcount < n) {
		if ((br = read(s, buf, n-bcount)) > 0) {
			bcount += br;
			buf += br;
		} else if (br < 0) {
			return -1;
		}
	}
	return bcount;
}

int
read_sock_line(int s, char * buf, int n) {
	
	int k;
	for (k = 0; k < n; k++) {
		if (read(s, buf+k, 1) < 0)
			return -1;
		if (buf[k] == '\n') {
			if (k > 0 && buf[k-1] == '\r') {
				buf[k-1] = '\0';
			} else {
				buf[k] = '\0';
			}
			break;
		}
	}
	return strlen(buf);
}

int
write_socket(int s, char * buf, int n) {
	
	int bcount;
	int bw;
	
	bcount = 0;
	bw = 0;
	
	while (bcount < n) {
		if ((bw = write(s, buf, n-bcount)) > 0) {
			bcount += bw;
			buf += bw;
		} else if (bw < 0) {
			return -1;
		}
	}
	return bcount;
}

char *
strip_whitespace(char * str) {
	
	int i, j;
	for (i = 0; str[i] != '\0' && (str[i] == ' ' || str[i] == '\t'); i++);
	if (str[i] == '\0') {
		str[0] = '\0';
		return str;
	}
	for (j = strlen(str)-1; str[j] == ' ' || str[j] == '\t'; j--);
	memmove(str, str+i, j-i+1);
	str[j-i+1] = '\0';
	return str;
}

int
parse_field(char * line, char * name, char * value) {
	
	if (line[0] == ' ' || line[0] == '\t') {
		name[0] = '\0';
		strcpy(value, strip_whitespace(line));
	} else {
		char * c = strstr(line, ":");
		if (c == NULL)
			return -1;
		strncpy(name, line, c-line);
		name[c-line] = '\0';
		strncpy(value, c+1, strlen(c)-1);
		value[strlen(c)-1] = '\0';
		strip_whitespace(name);
		strip_whitespace(value);
	}
	return 0;
}

int
parse_http_headers(http_session_t * session) {
	
	char line[1024];
	char name[1024];
	char value[1024];
	int s = session->sock;
	http_header_t * header = &session->headers;
	
	do {
		read_sock_line(s, line, sizeof(line));
		printf("line = '%s'\n", line);
		
		if (strstr(line, "HTTP/1.1 4") != NULL) {
			header->status = strdup(line);
			return -1;
		}
		if (strstr(line, "HTTP/1.1 5") != NULL) {
			header->status = strdup(line);
			return -2;
		}
	} while ((strstr(line, "HTTP/1.1 20") != NULL) &&
		 (strstr(line, "HTTP/1.1 30") != NULL));
	
	header->status = strdup(line);
	
	while (1) {
		char new_name[1024];
		char new_value[1024];
		read_sock_line(s, line, sizeof(line));
		if (line[0] == '\0')
			break;
		
		if (parse_field(line, new_name, new_value) == -1)
			return -3;
		
		if (new_name[0] == '\0') {
			snprintf(value, sizeof(value), "%s %s", value, new_value);
		} else {
			strcpy(name, new_name);
			strcpy(value, new_value);
		}
		
		if (strcasecmp(name, "location") == 0) {
			header->location = strdup(value);
		} else if (strcasecmp(name, "content-length") == 0) {
			int l;
			if (sscanf(value, "%d", &l) != 1) {
				printf("sscanf error (content-length)\n");
				return -3;
			} else {
				header->content_length = l;
			}
		} else if (strcasecmp(name, "content-type") == 0) {
			header->content_type = strdup(value);
		} else if (strcasecmp(name, "transfer-encoding") == 0) {
			header->transfer_encoding = strdup(value);
		} else if (strcasecmp(name, "icy-metaint") == 0) {
			int l;
			if (sscanf(value, "%d", &l) != 1) {
				printf("sscanf error (icy-metaint)\n");
				return -3;
			} else {
				header->icy_metaint = l;
			}
		} else if (strcasecmp(name, "icy-br") == 0) {
			int l;
			if (sscanf(value, "%d", &l) != 1) {
				if (sscanf(value, "Quality %d", &l) != 1) {
					printf("sscanf error (icy-br)\n");
					return -3;
				} else {
					header->icy_br = l;
				}
			} else {
				header->icy_br = l;
			}
		} else if (strcasecmp(name, "icy_genre") == 0) {
			header->icy_genre = strdup(value);
		} else if (strcasecmp(name, "icy_name") == 0) {
			header->icy_name = strdup(value);
		} else if (strcasecmp(name, "icy_description") == 0) {
			header->icy_description = strdup(value);
		}
		printf("name = '%s'  value = '%s'\n", name, value);
	}
	return 0;
}

int
parse_chunk_size(char * line) {
	
	int ret;
	char * p;
	if ((p = strstr(line, ";")) != NULL) {
		*p = '\0';
	}
	if (sscanf(line, "%x", &ret) != 1) {
		return -1;
	}
	return ret;
}

void
make_http_request_text(char * host, int port, char * path,
		       int use_proxy, char * proxy,
		       long long start_byte, char * msg, int msg_len) {

	char extra_header[1024];
	
	if (start_byte != 0) {
		snprintf(extra_header, sizeof(extra_header), "Range: bytes=%lld-\r\n", start_byte);
	} else {
		extra_header[0] = '\0';
	}

	if (!use_proxy) {
		if (port == 80) {
			snprintf(msg, msg_len,
				 "GET %s HTTP/1.1\r\n"
				 "Host: %s\r\n"
				 "User-Agent: Aqualung/%s\r\n"
				 "icy-metadata: 1\r\n"
				 "%s"
				 "Connection: close\r\n\r\n",
				 path, host, AQUALUNG_VERSION, extra_header);
		} else {
			snprintf(msg, msg_len,
				 "GET %s HTTP/1.1\r\n"
				 "Host: %s:%d\r\n"
				 "User-Agent: Aqualung/%s\r\n"
				 "icy-metadata: 1\r\n"
				 "%s"
				 "Connection: close\r\n\r\n",
				 path, host, port, AQUALUNG_VERSION, extra_header);
		}
	} else {
		if (port == 80) {
			snprintf(msg, msg_len,
				 "GET http://%s%s HTTP/1.1\r\n"
				 "Host: %s\r\n"
				 "User-Agent: Aqualung/%s\r\n"
				 "icy-metadata: 1\r\n"
				 "%s"
				 "Connection: close\r\n\r\n",
				 host, path, host, AQUALUNG_VERSION, extra_header);
		} else {
			snprintf(msg, msg_len,
				 "GET http://%s:%d%s HTTP/1.1\r\n"
				 "Host: %s:%d\r\n"
				 "User-Agent: Aqualung/%s\r\n"
				 "icy-metadata: 1\r\n"
				 "%s"
				 "Connection: close\r\n\r\n",
				 host, port, path, host, port, AQUALUNG_VERSION, extra_header);
		}
	}
}

http_session_t *
httpc_new(void) {

	http_session_t * session = (http_session_t *)malloc(sizeof(http_session_t));
	if (session == NULL)
		return NULL;
	memset(session, 0, sizeof(http_session_t));
	return session;
}

void
httpc_del(http_session_t * session) {

	free(session->URL);
	if (session->proxy != NULL)
		free(session->proxy);
	if (session->noproxy_domains != NULL)
		free(session->noproxy_domains);
	free_headers(&session->headers);
	free(session);
}

void
httpc_close(http_session_t * session) {

	if (session->is_active) {
		printf("closing HTTP connection\n");
		close(session->sock);
		session->is_active = 0;
	}
}

/* return 1 if host has to be accessed without the proxy */
int
noproxy_for_host(const char * noproxy_domains, const char * host) {

	char * s;
	char * nd;

	if (noproxy_domains == NULL)
		return 0;

	nd = strdup(noproxy_domains);
	s = strtok(nd, ",");
	while (s != NULL) {
		char * str = strip_whitespace(s);
		if (strstr(host, str) != NULL) {
			printf("%s matches %s, no proxy.\n", str, host);
			free(nd);
			return 1;
		}
		s = strtok(NULL, ",");
	}

	free(nd);
	return 0;
}


int
httpc_init(http_session_t * session, char * URL, int use_proxy, char * proxy, int proxy_port,
	   char * noproxy_domains, long long start_byte) {
	
	char * p;
	char host[1024];
	char port_str[8];
	int port;
	char msg_buf[1024];
	
	memset(session, 0, sizeof(http_session_t));
	
	if (!httpc_is_url(URL))
		return HTTPC_URL_ERROR;
	
	session->URL = strdup(URL);
	if (use_proxy) {
		session->use_proxy = use_proxy;
		session->proxy = strdup(proxy);
		session->proxy_port = proxy_port;
		session->noproxy_domains = strdup(noproxy_domains);
	}
    
	URL += strlen("http://");
	
	if ((p = strstr(URL, ":")) != NULL) {
		unsigned int l = p - URL;
		if (l > sizeof(host)) {
			return HTTPC_URL_ERROR;
		}
		strncpy(host, URL, l);
		host[l] = '\0';
		URL += l+1;
		if ((p = strstr(URL, "/")) != NULL) {
			unsigned int m = p - URL;
			if (m > sizeof(port_str)) {
				return HTTPC_URL_ERROR;
			}
			strncpy(port_str, URL, m);
			port_str[m] = '\0';
			URL += m;
		} else {
			strncpy(port_str, URL, sizeof(port_str));
			URL = "/";
		}
		if (sscanf(port_str, "%d", &port) != 1) {
			return HTTPC_URL_ERROR;
		}
	} else {
		if ((p = strstr(URL, "/")) != NULL) {
			unsigned int l = p - URL;
			if (l > sizeof(host)) {
				return HTTPC_URL_ERROR;
			}
			strncpy(host, URL, l);
			host[l] = '\0';
			URL += l;
			port = 80;      
		} else {
			strncpy(host, URL, sizeof(host));
			port = 80;
			URL = "/";
		}
	}
	
	make_http_request_text(host, port, URL, use_proxy, proxy,
			       start_byte, msg_buf, sizeof(msg_buf));
	printf("%s\n", msg_buf);
	
	if (!use_proxy || noproxy_for_host(noproxy_domains, host)) {
		session->sock = open_socket(host, port);
	} else {
		session->sock = open_socket(proxy, proxy_port);
	}

	if (session->sock < 0) {
		return HTTPC_CONNECTION_ERROR;
	}

	write_socket(session->sock, msg_buf, strlen(msg_buf));
	
	if (parse_http_headers(session) != 0) {
		close(session->sock);
		printf("http header error, server error or resource not found\n");
		return HTTPC_HEADER_ERROR;
	}

	if (strstr(session->headers.status, "HTTP/1.1 30") != NULL) {
		/* redirect */
		if (session->headers.location != NULL) {
			int ret;
			printf("redirecting to %s\n", session->headers.location);
			close(session->sock);
			ret = httpc_init(session, session->headers.location, use_proxy, proxy,
					 proxy_port, noproxy_domains, 0L);
			return ret;
		} else {
			close(session->sock);
			printf("redirect error\n");
			return HTTPC_REDIRECT_ERROR;
		}
	} else if ((session->headers.content_type != NULL) &&
		   (strcasecmp(session->headers.content_type, "audio/x-mpegurl") == 0)) {
		
		char buf[1024];
		int ret;
		read_sock_line(session->sock, buf, sizeof(buf));
		printf("following x-mpegurl to %s\n", buf);
		close(session->sock);
		ret = httpc_init(session, buf, use_proxy, proxy, proxy_port, noproxy_domains, 0L);
		return ret;
	}

	if (session->headers.content_length != 0) {
		session->type = HTTPC_SESSION_NORMAL;
	} else if ((session->headers.transfer_encoding != NULL) &&
		   (strcasecmp(session->headers.transfer_encoding, "chunked") == 0)) {		
		session->type = HTTPC_SESSION_CHUNKED;
	} else {
		session->type = HTTPC_SESSION_STREAM;
	}

	session->is_active = 1;
	session->byte_pos = start_byte;

	printf("HTTP connection successfully opened, type = %d\n", session->type);
	return 0;
}

int
httpc_read_normal(http_session_t * session, char * buf, int num) {

	int sock = session->sock;
	int tr = session->headers.content_length - session->byte_pos;
	int n_read;

	if (!session->is_active) {
		printf("[HTTPC] Reopening stream\n");
		httpc_reconnect(session);
	}
	
	printf("httpc_read_normal num = %d, pos = %lld, ", num, session->byte_pos);
	if (tr > num) {
		tr = num;
	}
	n_read = read_socket(sock, buf, tr);
	if (n_read < 0) {
		printf("ret -1\n");
		return -1;
	}
	session->byte_pos += n_read;
	printf("ret %d\n", n_read);
	return n_read;
}

int
httpc_read_chunked(http_session_t * session, char * buf, int num) {

	int sock = session->sock;
	int chunk_size = session->chunk_size;
	int chunk_pos = session->chunk_pos;
	int buf_pos = 0;
	char line[1024];

	printf("httpc_read_chunked\n");

	while (buf_pos < num && !session->end_of_data) {
		if (chunk_size - chunk_pos > 0) {
			int tw = chunk_size - chunk_pos;
			if (tw > num - buf_pos)
				tw = num - buf_pos;
			printf("buf_pos = %d  chunk_size = %d  chunk_pos = %d  tw = %d\n",
			       buf_pos, chunk_size, chunk_pos, tw);
			memcpy(buf + buf_pos, session->chunk_buf + chunk_pos, tw);
			chunk_pos += tw;
			buf_pos += tw;
		}

		if (chunk_pos == chunk_size) {
			/* read next chunk */
			chunk_pos = 0;
			if (chunk_size > 0)
				free(session->chunk_buf);
			read_sock_line(sock, line, sizeof(line));
			chunk_size = parse_chunk_size(line);
			if (chunk_size > 0) {
				int n_read;
				printf("chunk size = %d\n", chunk_size);
				session->chunk_buf = malloc(chunk_size);
				n_read = read_socket(sock, session->chunk_buf, chunk_size);
				if (n_read != chunk_size) {
					printf("httpc_read_chunked: premature end of chunk!\n");
				}
				read_sock_line(sock, line, sizeof(line));
			} else {
				printf("end of data\n");
				session->end_of_data = 1;
			}
		}

	}

	session->chunk_size = chunk_size;
	session->chunk_pos = chunk_pos;

	return buf_pos;
}

int
httpc_demux(http_session_t * session) {

	unsigned char meta_len;
	char * meta_buf;

	if (read_socket(session->sock, &meta_len, 1) != 1)
		return -1;

	meta_len *= 16;
	meta_buf = calloc(meta_len+1, 1);
	if (read_socket(session->sock, meta_buf, meta_len) != meta_len)
		return -1;

	meta_buf[(int)meta_len] = '\0';

	if (meta_len > 0) {
		printf("%s\n", meta_buf);
	}

	free(meta_buf);
	return 0;
}

int
httpc_read_stream_simple(http_session_t * session, char * buf, int num) {

	int n_read = read_socket(session->sock, buf, num);
	if (n_read < 0) {
		return 0;
	}
	return n_read;
}

int
httpc_read_stream(http_session_t * session, char * buf, int num) {

	int metaint = session->headers.icy_metaint;
	int n_read, n_read2;

	if (metaint == 0) {
		return httpc_read_stream_simple(session, buf, num);
	}

	if (metaint - session->metapos >= num) {
		n_read = read_socket(session->sock, buf, num);
		if (n_read < 0)
			return 0;
		session->metapos += n_read;
		return n_read;
	} else {
		n_read = metaint - session->metapos;
		n_read2 = num - n_read;

		n_read = read_socket(session->sock, buf, n_read);
		if (n_read < 0)
			return 0;

		httpc_demux(session);

		while (n_read2 > metaint) {
			int n = read_socket(session->sock, buf + n_read, metaint);
			if (n < 0)
				return 0;
			httpc_demux(session);
			n_read2 -= n;
			n_read += n;
		}

		if (n_read2 > 0) {
			n_read2 = read_socket(session->sock, buf + n_read, n_read2);
			if (n_read2 < 0)
				return 0;
			session->metapos = n_read2;
		} else {
			session->metapos = 0;
		}

		return n_read + n_read2;
	}
}

int
httpc_read(http_session_t * session, char * buf, int num) {

	switch (session->type) {
	case HTTPC_SESSION_NORMAL:
		return httpc_read_normal(session, buf, num);
	case HTTPC_SESSION_CHUNKED:
		return httpc_read_chunked(session, buf, num);
	case HTTPC_SESSION_STREAM:
		return httpc_read_stream(session, buf, num);
	default:
		printf("httpc_read: unknown session type = %d\n", session->type);
		return 0;
	}
}


int
httpc_reconnect(http_session_t * session) {

	char * URL;
	int use_proxy = 0;
	char * proxy = NULL;
	int proxy_port = 0;
	char * noproxy_domains = NULL;
	long long start_byte = 0L;
	int content_length = 0;
	int ret;
	
	URL = strdup(session->URL);
	if (session->use_proxy) {
		use_proxy = session->use_proxy;
		proxy = strdup(session->proxy);
		proxy_port = session->proxy_port;
		noproxy_domains = strdup(session->noproxy_domains);
	}
	
	if (session->type == HTTPC_SESSION_NORMAL) {
		start_byte = session->byte_pos;
		content_length = session->headers.content_length;
	} else {
		start_byte = 0;
	}

	ret = httpc_init(session, URL, use_proxy, proxy, proxy_port, noproxy_domains, start_byte);
	if (ret != HTTPC_OK) {
		fprintf(stderr, "http_reconnect: HTTP session reopen failed, ret = %d\n", ret);
	}

	if (session->type == HTTPC_SESSION_NORMAL) {
		session->headers.content_length = content_length;
	}
	
	free(URL);
	if (proxy != NULL)
		free(proxy);
	if (noproxy_domains != NULL)
		free(noproxy_domains);
	
	return ret;
}


int
httpc_seek(http_session_t * session, long long offset, int whence) {
	
	if (session->type != HTTPC_SESSION_NORMAL)
		return -1;
	
	printf("[HTTPC_SEEK] offset = %lld  whence = %d  byte_pos = %lld\n",
	       offset, whence, session->byte_pos);
	switch (whence) {
	case SEEK_SET:
		if (offset == session->byte_pos) {
			printf("noop SEEK_SET\n");
			return session->byte_pos;
		}
		break;
	case SEEK_CUR:
		if (offset == 0) {
			printf("noop SEEK_CUR\n");
			return session->byte_pos;
		}
		break;
	case SEEK_END:
		if (session->headers.content_length - offset == session->byte_pos) {
			printf("noop SEEK_END\n");
			return session->byte_pos;
		}
		break;
	}

	if (session->is_active) {
		printf("[HTTPC] Closing stream\n");
		httpc_close(session);
	}
	
	switch (whence) {
	case SEEK_SET:
		printf("[HTTPC] SEEK_SET offset = %lld\n", offset);
		if (offset > session->headers.content_length) {
			offset = session->headers.content_length;
		}
		session->byte_pos = offset;
		break;
	case SEEK_CUR:
		printf("[HTTPC] SEEK_CUR offset = %lld\n", offset);
		if (offset + session->byte_pos > session->headers.content_length) {
			session->byte_pos = session->headers.content_length;
		} else {
			session->byte_pos += offset;
		}
		break;
	case SEEK_END:
		printf("[HTTPC] SEEK_END offset = %lld\n", offset);
		if (offset > session->headers.content_length - session->byte_pos) {
			session->byte_pos = 0L;
		} else {
			session->byte_pos = session->headers.content_length - offset;
		}
		break;
	}
	
	return session->byte_pos;
}

long long
httpc_tell(http_session_t * session) {
	
	if (session->type != HTTPC_SESSION_NORMAL)
		return -1;
	
	printf("[HTTPC] TELL = %lld\n", session->byte_pos);
	return session->byte_pos;
}
