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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i18n.h"
#include "httpc.h"


extern options_t options;


void
make_title_string(char * dest, char * templ,
		  char * artist, char * record, char * track) {

	int i;
	int j;
	int argc = 0;
	char * arg[3] = { "", "", "" };
	char temp[MAXLEN];

	temp[0] = templ[0];

	for (i = j = 1; i < strlen(templ) && j < MAXLEN - 1; i++, j++) {
		if (templ[i - 1] == '%') {
			if (argc < 3) {
				switch (templ[i]) {
				case 'a':
					arg[argc++] = artist;
					temp[j] = 's';
					break;
				case 'r':
					arg[argc++] = record;
					temp[j] = 's';
					break;
				case 't':
					arg[argc++] = track;
					temp[j] = 's';
					break;
				default:
					temp[j++] = '%';
					temp[j] = templ[i];
					break;
				}
			} else {
				temp[j++] = '%';
				temp[j] = templ[i];
			}
		} else {
			temp[j] = templ[i];
		}
	}

	temp[j] = '\0';
	
	snprintf(dest, MAXLEN - 1, temp, arg[0], arg[1], arg[2]);
}


void
make_title_string_no_album(char * dest, char * templ,
			   char * artist, char * track) {
	int i;
	int j;
	int argc = 0;
	char * arg[2] = { "", "" };
	char temp[MAXLEN];

	temp[0] = templ[0];

	for (i = j = 1; i < strlen(templ) && j < MAXLEN - 1; i++, j++) {
		if (templ[i - 1] == '%') {
			if (argc < 2) {
				switch (templ[i]) {
				case 'a':
					arg[argc++] = artist;
					temp[j] = 's';
					break;
				case 't':
					arg[argc++] = track;
					temp[j] = 's';
					break;
				default:
					temp[j++] = '%';
					temp[j] = templ[i];
					break;
				}
			} else {
				temp[j++] = '%';
				temp[j] = templ[i];
			}
		} else {
			temp[j] = templ[i];
		}
	}

	temp[j] = '\0';
	
	snprintf(dest, MAXLEN - 1, temp, arg[0], arg[1]);
}


/* returns (hh:mm:ss) or (mm:ss) format time string from sample position */
void
sample2time(unsigned long SR, unsigned long long sample, char * str, int sign) {

	int h;
	char m, s;

	if (!SR)
		SR = 1;

	h = (sample / SR) / 3600;
	m = (sample / SR) / 60 - h * 60;
	s = (sample / SR) - h * 3600 - m * 60;

	if (h > 9)
		sprintf(str, (sign)?("-%02d:%02d:%02d"):("%02d:%02d:%02d"), h, m, s);
	else if (h > 0)
		sprintf(str, (sign)?("-%1d:%02d:%02d"):("%1d:%02d:%02d"), h, m, s);
	else
		sprintf(str, (sign)?("-%02d:%02d"):("%02d:%02d"), m, s);
}


/* converts a length measured in seconds to the appropriate string */
void
time2time(float seconds, char * str) {

	int d, h;
	char m, s;

        d = seconds / 86400;
	h = seconds / 3600;
	m = seconds / 60 - h * 60;
	s = seconds - h * 3600 - m * 60;
        h = h - d * 24;

        if (d > 0) {
                if (d == 1 && h > 9) {
                        sprintf(str, "%d %s, %2d:%02d:%02d", d, _("day"), h, m, s);
                } else if (d == 1 && h < 9) {
                        sprintf(str, "%d %s, %1d:%02d:%02d", d, _("day"), h, m, s);
                } else if (d != 1 && h > 9) {
                        sprintf(str, "%d %s, %2d:%02d:%02d", d, _("days"), h, m, s);
                } else {
                        sprintf(str, "%d %s, %1d:%02d:%02d", d, _("days"), h, m, s);
                }
        } else if (h > 0) {
		if (h > 9) {
			sprintf(str, "%02d:%02d:%02d", h, m, s);
		} else {
			sprintf(str, "%1d:%02d:%02d", h, m, s);
		}
	} else {
		sprintf(str, "%02d:%02d", m, s);
	}
}


void
time2time_na(float seconds, char * str) {

	if (seconds == 0.0) {
		strcpy(str, "N/A");
	} else {
		time2time(seconds, str);
	}
}


/* pack 2 strings into one
 * output format: 4 hex digits -> length of 1st string (N)
 *                4 hex digits -> length of 2nd string (M)
 *                N characters of 1st string (w/o trailing zero)
 *                M characters of 2nd string (w/o trailing zero)
 *                trailing zero
 * sum: length(str1) + length(str2) + 4 + 4 + 1
 * result should point to an area with sufficient space
 */
void
pack_strings(char * str1, char * str2, char * result) {

	sprintf(result, "%04X%04X%s%s", strlen(str1), strlen(str2), str1, str2);
}


/* inverse of pack_strings()
 * str1 and str2 should point to areas of sufficient space
 */
void
unpack_strings(char * packed, char * str1, char * str2) {

	int len1, len2;

	if (strlen(packed) < 8) {
		str1[0] = '\0';
		str2[0] = '\0';
		return;
	}

	sscanf(packed, "%04X%04X", &len1, &len2);
	strncpy(str1, packed + 8, len1);
	strncpy(str2, packed + 8 + len1, len2);
	str1[len1] = '\0';
	str2[len2] = '\0';
}


/* out should be defined as char[MAXLEN] */
void
normalize_filename(const char * in, char * out) {

	if (httpc_is_url(in)) {
		strncpy(out, in, MAXLEN-1);
		return;
	}

	switch (in[0]) {
	case '/':
		strncpy(out, in, MAXLEN-1);
		break;
	case '~':
		snprintf(out, MAXLEN-1, "%s%s", options.home, in + 1);
		break;
	default:
		snprintf(out, MAXLEN-1, "%s/%s", options.cwd, in);
		break;
	}
}

void
free_strdup(char ** s, const char * str) {

	if (*s != NULL) {
		free(*s);
	}

	*s = strdup(str);
}

int
cdda_is_cdtrack(char * file) {

	return (strstr(file, "CDDA ") == file);
}


#ifndef HAVE_STRCASESTR
char
toupper(char c) {

	if (c >= 'a' && c <= 'z')
		return (c-32);
	return c;
}

char *
strcasestr(char *haystack, char *needle) {

	char *s1 = needle;
	char *s2 = haystack;
	char *sr = NULL;
	int inside = 0;

	while (*s2 != '\0') {
		if (toupper(*s1) == toupper(*s2)) {
			if (inside == 0) {
				sr = s2;
			}
			inside = 1;
			++s1;
			++s2;
			if (*s1 == '\0') {
				break;
			}
		} else {
			inside = 0;
			s1 = needle;
			++s2;
		}
	}
	if (inside == 1)
		return sr;
	return NULL;
}
#endif /* HAVE_STRCASESTR */
