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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <glib.h>

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
	
	size = sizeof(name);
	n_read = recvfrom(fd, buffer, MAXLEN-1, 0, (struct sockaddr *)&name, (socklen_t *)&size);
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

	case RCMD_VOLADJ:
	case RCMD_ADD_FILE:
	case RCMD_CUSTOM:
		strncpy(cmdarg, buffer + 1, MAXLEN-2);
		return rcmd;

	case RCMD_ADD_COMMIT:
		for (i = 1; i <= 3; i++) {
			cmdarg[i-1] = buffer[i];
		}
		cmdarg[3] = '\0';
		strncpy(cmdarg + 3, buffer + 4, MAXLEN-5);
		return rcmd;

	default:
		fprintf(stderr, "Recv'd unrecognized remote command %d\n", rcmd);
		break;
	}

	return 0;
}


int
send_message(const char * filename, char * message, int len) {

	char tempsockname[MAXLEN];
        int sock;
        struct sockaddr_un name;
        int nbytes;
	struct timespec req_time;
	struct timespec rem_time;

	req_time.tv_sec = 0;
	req_time.tv_nsec = 100000000;

	sprintf(tempsockname, "/tmp/aqualung_%s.tmp", g_get_user_name());
	sock = create_socket(tempsockname);
        name.sun_family = AF_LOCAL;
        strcpy(name.sun_path, filename);

	do {
		nbytes = sendto(sock, message, len+1, 0, (struct sockaddr *)&name, sizeof(name));
		if (nbytes == -1 && errno == ENOBUFS) {
			nanosleep(&req_time, &rem_time);
		}
	} while (nbytes == -1 && errno == ENOBUFS);

        remove(tempsockname);
	close(sock);
	return nbytes;
}

int
send_message_to_session_report_error(int session_id, char * message, int len, int report_error) {

	char name[MAXLEN];
	int nbytes = 0;

	sprintf(name, "/tmp/aqualung_%s.%d", g_get_user_name(), session_id);
	if((nbytes = send_message(name, message, len)) < 0 && report_error) {
		perror("send_message(): sendto");
	}
	return nbytes;
}

void
send_message_to_session(int session_id, char * message, int len) {

	send_message_to_session_report_error(session_id, message, len, 1);
}

void
setup_app_socket(void) {

	int sock = -1;
	int i;
	char name[MAXLEN];
	char rcmd = RCMD_PING;

	for (i=0; sock == -1; i++) {
		if (send_message_to_session_report_error(i, &rcmd, 1, 0) < 0) {
			sprintf(name, "/tmp/aqualung_%s.%d", g_get_user_name(), i);
			unlink(name);
			sock = create_socket(name);
		}
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

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

