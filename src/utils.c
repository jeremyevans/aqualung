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
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <glib.h>

#include "i18n.h"
#include "httpc.h"
#include "utils.h"


extern options_t options;


float convf(char * s) {

        float val, pow;
        int i, sign;

        for (i = 0; s[i] == ' ' || s[i] == '\n' || s[i] == '\t'; i++);
        sign = 1;
        if (s[i] == '+' || s[i] == '-')
                sign = (s[i++] == '+') ? 1 : -1;
        for (val = 0; s[i] >= '0' && s[i] <= '9'; i++)
                val = 10 * val + s[i] - '0';
        if ((s[i] == '.') || (s[i] == ','))
                i++;
        for (pow = 1; s[i] >= '0' && s[i] <= '9'; i++) {
                val = 10 * val + s[i] - '0';
                pow *= 10;
        }
        return(sign * val / pow);
}


int
is_all_wspace(char * str) {

	int i;

	if (str == NULL) {
		return 1;
	}

	for (i = 0; str[i]; i++) {
		if (str[i] != ' ' && str[i] != '\t') {
			return 0;
		}
	}

	return 1;
}


int
cut_trailing_whitespace(char * str) {

	int i = strlen(str) - 1;

	while (i >= 0) {
		if ((str[i] == ' ') || (str[i] == '\t')) {
			str[i] = '\0';
		} else {
			break;
		}
		--i;
	}
	return ((i >= 0) ? 1 : 0);
}


/* To print a string containing printf formatters, but
 * *not* followed by arguments -- I just want the plain
 * string, please... 
 */
void
escape_percents(char * in, char * out) {

	int i;
	int j = 0;
	for (i = 0; in[i] != '\0'; i++) {
		out[j] = in[i];
		++j;
		if (in[i] == '%') {
			out[j] = '%';
			++j;
		}
	}
	out[j] = '\0';
}

int
contains(int * cbuf, int num, char c) {

	int n;
	for (n = 0; n < num; ++n) {
		if (c == cbuf[n]) {
			return n;
		}
	}

	return -1;
}

/* returns
     0: success
    -1: error: expected '{' after '?'
    -2: error: expected '}' after '{'
    -3: error: unknown conversion type character after '%'
    -4: error: unknown conversion type character after '?'
 */
int
make_string_va(char * buf, char * format, ...) {

	va_list args;
	char ** strbuf = NULL;
	int * cbuf = NULL;
	int ch;
	int num = 0;
	int i, j, n, conj, disj;

	va_start(args, format);

	while ((ch = va_arg(args, int)) != 0) {

		char * p;

		++num;
		cbuf = (int *)realloc(cbuf, num * sizeof(int));
		cbuf[num-1] = ch;
		strbuf = (char **)realloc(strbuf, num * sizeof(char *));
		p = va_arg(args, char *);
		strbuf[num-1] = (p != NULL) ? strdup(p) : NULL;
	}

	va_end(args);

	cbuf = (int *)realloc(cbuf, (num + 3) * sizeof(int));
	strbuf = (char **)realloc(strbuf, (num + 3) * sizeof(char *));
	cbuf[num] = '%'; strbuf[num] = strdup("%"); ++num;
	cbuf[num] = '?'; strbuf[num] = strdup("?"); ++num;
	cbuf[num] = '}'; strbuf[num] = strdup("}"); ++num;

	i = 0;
	j = 0;
	while (format[i]) {

		switch (format[i]) {
		case '%':
			if ((n = contains(cbuf, num, format[i+1])) < 0) {
				return -3;
			} else {
				if (strbuf[n]) {
					buf[j] = '\0';
					strcat(buf, strbuf[n]);
					j = strlen(buf);
				}
				i += 2;
			}
			break;
		case '?':
			disj = 0;
			conj = 1;
			for (++i; format[i]; ++i) {
				if (format[i] == '|' || format[i] == '{') {
					disj = disj || conj;
					conj = 1;
				} else {
					if ((n = contains(cbuf, num, format[i])) < 0) {
						return -4;
					} else {
						conj = conj && (strbuf[n] != NULL);
					}
				}
				if (format[i] == '{') {
					break;
				}
			}
			if (!format[i]) {
				return -1;
			}
			++i;
			while (format[i] && format[i] != '}') {
				if (format[i] == '%') {
					if ((n = contains(cbuf, num, format[i+1])) < 0) {
						return -3;
					} else {
						if (disj && strbuf[n]) {
							buf[j] = '\0';
							strcat(buf, strbuf[n]);
							j = strlen(buf);
						}
						i += 2;
					}
				} else {
					if (disj) {
						buf[j++] = format[i];
					}
					++i;
				}
			}
			if (!format[i]) {
				return -2;
			}
			++i;
			break;
		default:
			buf[j++] = format[i++];
			break;
		}
	}

	buf[j] = '\0';

	for (n = 0; n < num; n++) {
		if (strbuf[n] != NULL) {
			free(strbuf[n]);
		}
	}

	free(cbuf);
	free(strbuf);

	return 0;
}

void
make_title_string(char * dest, char * templ,
		  char * artist, char * record, char * track) {

	make_string_va(dest, templ, 'a', artist, 'r', record, 't', track, 0);
}

void
make_string_strerror(int ret, char * buf) {

	switch (ret) {
	case -1:
		strncpy(buf, _("Unexpected end of string after '?'."), MAXLEN-1);
		break;
	case -2:
		strncpy(buf, _("Expected '}' after '{', but end of string found."), MAXLEN-1);
		break;
	case -3:
		strncpy(buf, _("Unknown conversion type character found after '%%%%'."), MAXLEN-1);
		break;
	case -4:
		strncpy(buf, _("Unknown conversion type character found after '?'."), MAXLEN-1);
		break;
	}
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

	if (str != NULL) {
		*s = strdup(str);
	} else {
		*s = NULL;
	}
}

int
is_valid_year(int y) {

	return y >= YEAR_MIN && y <= YEAR_MAX;
}

int
cdda_is_cdtrack(char * file) {

	return (strstr(file, "CDDA ") == file);
}

int
is_dir(char * path) {

	struct stat st_file;

	if (stat(path, &st_file) == -1) {
		return 0;
	}

	return S_ISDIR(st_file.st_mode);
}

#ifndef HAVE_STRNDUP
char *
strndup(char * str, size_t len) {
    char * dup = (char *)malloc(len + 1);
    if (dup) {
        strncpy(dup, str, len);
        dup[len] =  '\0';
    }
    return dup;
}
#endif /* HAVE_STRNDUP */

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


map_t *
map_new(char * str) {

	map_t * map;

	if ((map = (map_t *)malloc(sizeof(map_t))) == NULL) {
		fprintf(stderr, "map_new(): malloc error\n");
		return NULL;
	}

	strncpy(map->str, str, MAXLEN-1);
	map->count = 1;
	map->next = NULL;

	return map;
}

void
map_put(map_t ** map, char * str) {

	map_t * pmap;
	map_t * _pmap;

	if (str == NULL || str[0] == '\0') {
		return;
	}

	if (*map == NULL) {
		*map = map_new(str);
	} else {

		for (_pmap = pmap = *map; pmap; _pmap = pmap, pmap = pmap->next) {

			char * key1 = g_utf8_casefold(str, -1);
			char * key2 = g_utf8_casefold(pmap->str, -1);

			if (!g_utf8_collate(key1, key2)) {
				pmap->count++;
				g_free(key1);
				g_free(key2);
				return;
			}

			g_free(key1);
			g_free(key2);
		}

		_pmap->next = map_new(str);
	}
}

char *
map_get_max(map_t * map) {

	map_t * pmap;
	int max = 0;
	char * str = NULL;

	for (pmap = map; pmap; pmap = pmap->next) {
		if (max <= pmap->count) {
			str = pmap->str;
			max = pmap->count;
		}
	}

	return str;
}

void
map_free(map_t * map) {

	map_t * pmap;

	for (pmap = map; pmap; map = pmap) {
		pmap = map->next;
		free(map);
	}
}

void
xml_save_str(xmlNodePtr node, char * varname, char * var) {

	xmlNewTextChild(node, NULL, (const xmlChar *)varname, (xmlChar *)var);
}

void
xml_save_int(xmlNodePtr node, char * varname, int var) {

	char str[32];

        snprintf(str, 31, "%d", var);
        xmlNewTextChild(node, NULL, (const xmlChar *)varname, (xmlChar *)str);
}

void
xml_save_uint(xmlNodePtr node, char * varname, unsigned var) {

	char str[32];

        snprintf(str, 31, "%u", var);
        xmlNewTextChild(node, NULL, (const xmlChar *)varname, (xmlChar *)str);
}

void
xml_save_float(xmlNodePtr node, char * varname, float var) {

	char str[32];

        snprintf(str, 31, "%.1f", var);
        xmlNewTextChild(node, NULL, (const xmlChar *)varname, (xmlChar *)str);
}

void
xml_save_int_array(xmlNodePtr node, char * varname, int * var, int idx) {

	char name[MAXLEN];

	snprintf(name, MAXLEN-1, "%s_%d", varname, idx);
	xml_save_int(node, name, var[idx]);
}

void
xml_load_str(xmlDocPtr doc, xmlNodePtr node, char * varname, char * var) {

	if (!xmlStrcmp(node->name, (const xmlChar *)varname)) {
		xmlChar * key = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		if (key != NULL) {
			strncpy(var, (char *)key, MAXLEN-1);
			xmlFree(key);
		}
	}
}

void
xml_load_str_dup(xmlDocPtr doc, xmlNodePtr node, char * varname, char ** var) {

	if (!xmlStrcmp(node->name, (const xmlChar *)varname)) {
		xmlChar * key = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		if (key != NULL) {
			*var = strdup((char *)key);
			xmlFree(key);
		}
	}
}

void
xml_load_int(xmlDocPtr doc, xmlNodePtr node, char * varname, int * var) {

	if ((!xmlStrcmp(node->name, (const xmlChar *)varname))) {
		xmlChar * key = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		if (key != NULL) {
			sscanf((char *)key, "%d", var);
			xmlFree(key);
		}
	}
}

void
xml_load_uint(xmlDocPtr doc, xmlNodePtr node, char * varname, unsigned * var) {

	if ((!xmlStrcmp(node->name, (const xmlChar *)varname))) {
		xmlChar * key = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		if (key != NULL) {
			sscanf((char *)key, "%u", var);
			xmlFree(key);
		}
	}
}

void
xml_load_float(xmlDocPtr doc, xmlNodePtr node, char * varname, float * var) {

	if ((!xmlStrcmp(node->name, (const xmlChar *)varname))) {
		xmlChar * key = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		if (key != NULL) {
			sscanf((char *)key, "%f", var);
			xmlFree(key);
		}
	}
}

void
xml_load_int_array(xmlDocPtr doc, xmlNodePtr node, char * varname, int * var, int idx) {

	char name[MAXLEN];

	snprintf(name, MAXLEN-1, "%s_%d", varname, idx);
	xml_load_int(doc, node, name, var + idx);
}



// vim: shiftwidth=8:tabstop=8:softtabstop=8:

