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
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <gdk/gdkx.h>

#include "common.h"
#include "transceiver.h"


int aqualung_socket_fd;
char aqualung_socket_filename[256];
int aqualung_session_id;


int
create_socket(const char * filename) {

	struct sockaddr_un name;
	int sock;
	size_t size;
	
	sock = socket(PF_LOCAL, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("create_socket(): socket");
		return -1;
	}
	
	name.sun_family = AF_LOCAL;
	strncpy(name.sun_path, filename, sizeof(name.sun_path));
	size = SUN_LEN(&name);
	if (bind(sock, (struct sockaddr *)&name, size) < 0) {
		perror("create_socket(): bind");
		return -1;
	}
	
	return sock;
}


char
receive_message(int fd, char * cmdarg) {
	
	fd_set set;
	struct timeval tv;
	struct sockaddr_un name;
	size_t size;
	int n_read;
	char buffer[MAXLEN];
	char rcmd;
	int i;
	
	FD_ZERO(&set);
	FD_SET(fd, &set);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	
	if (select(fd + 1, &set, NULL, NULL, &tv) <= 0)
		return 0;
	
	n_read = recvfrom(fd, buffer, MAXLEN-1, 0, (struct sockaddr *)&name, &size);
	buffer[n_read] = '\0';

	rcmd = buffer[0];
	switch (rcmd) {
	case RCMD_PING:
		break;
	case RCMD_BACK:
	case RCMD_PLAY:
	case RCMD_PAUSE:
	case RCMD_STOP:
	case RCMD_FWD:
	case RCMD_QUIT:
		return rcmd;
		break;
	case RCMD_LOAD:
	case RCMD_ENQUEUE:
		for (i = 1; i < MAXLEN && buffer[i] != '\0'; i++)
			buffer[i-1] = buffer[i];
		buffer[i-1] = '\0';
		strncpy(cmdarg, buffer, MAXLEN);
		return rcmd;
		break;

	default:
		fprintf(stderr, "Recv'd unrecognized remote command %d\n", rcmd);
		break;
	}

	return 0;
}


void
send_message(const char * filename, char * message, int len) {

	char tempsockname[MAXLEN];
        int sock;
        struct sockaddr_un name;
        size_t size;
        int nbytes;

	sprintf(tempsockname, "/tmp/aqualung_%s.tmp", g_get_user_name());
	sock = create_socket(tempsockname);
        name.sun_family = AF_LOCAL;
        strcpy(name.sun_path, filename);
        size = strlen(name.sun_path) + sizeof(name.sun_family);

        nbytes = sendto(sock, message, len+1, 0, (struct sockaddr *)&name, size);
        if (nbytes < 0) {
		perror("send_message(): sendto");
        }

        remove(tempsockname);
	close(sock);
}


void
send_message_to_session(int session_id, char * message, int len) {
	
	char name[MAXLEN];

	sprintf(name, "/tmp/aqualung_%s.%d", g_get_user_name(), session_id);
	send_message(name, message, len);
}


int
is_socket_alive(const char * filename) {

	char tempsockname[MAXLEN];
	int sock;
	struct sockaddr_un name;
	size_t size;
	int nbytes;
	char rcmd = RCMD_PING;

	sprintf(tempsockname, "/tmp/aqualung_%s.tmp", g_get_user_name());
	sock = create_socket(tempsockname);
	name.sun_family = AF_LOCAL;
	strcpy(name.sun_path, filename);
	size = strlen(name.sun_path) + sizeof(name.sun_family);

	nbytes = sendto(sock, &rcmd, 2, 0, (struct sockaddr *)&name, size);
	if (nbytes < 0)	{
		remove(tempsockname);
		return 0;
	}

	remove(tempsockname);
	close(sock);
	return 1;
}


void
setup_app_socket(void) {

	int sock = -1;
	int i = 0;
	char name[MAXLEN];

	while (sock == -1) {
		sprintf(name, "/tmp/aqualung_%s.%d", g_get_user_name(), i);
		if (!is_socket_alive(name)) {
			unlink(name);
			sock = create_socket(name);
		}
		++i;
	}
	
	aqualung_socket_fd = sock;
	aqualung_session_id = --i;
	strncpy(aqualung_socket_filename, name, 255);
}


void
close_app_socket(void) {

	remove(aqualung_socket_filename);
	close(aqualung_socket_fd);
}
