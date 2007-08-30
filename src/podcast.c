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

#ifdef HAVE_PODCAST

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#ifdef _WIN32
#include <glib.h>
#else
#include <pthread.h>
#endif /* _WIN32*/

#include "common.h"
#include "i18n.h"
#include "utils.h"
#include "decoder/file_decoder.h"
#include "options.h"
#include "httpc.h"
#include "store_podcast.h"
#include "podcast.h"

#define BUFSIZE 10240

extern options_t options;


podcast_t *
podcast_new(void) {

	podcast_t * podcast;

	if ((podcast = (podcast_t *)calloc(1, sizeof(podcast_t))) == NULL) {
		fprintf(stderr, "podcast_new: calloc error\n");
		return NULL;
	}

	podcast->state = PODCAST_STATE_IDLE;
	podcast->items = NULL;

	return podcast;
}

void
podcast_free(podcast_t * podcast) {

	if (podcast->dir) {
		free(podcast->dir);
	}
	if (podcast->title) {
		free(podcast->title);
	}
	if (podcast->author) {
		free(podcast->author);
	}
	if (podcast->desc) {
		free(podcast->desc);
	}
	if (podcast->url) {
		free(podcast->url);
	}

	g_slist_free(podcast->items);

	free(podcast);
}

podcast_item_t *
podcast_item_new(void) {

	podcast_item_t * item;

	if ((item = (podcast_item_t *)calloc(1, sizeof(podcast_item_t))) == NULL) {
		fprintf(stderr, "podcast_item_new: calloc error\n");
		return NULL;
	}

	item->new = 1;
	item->size = 0;
	item->date = 0;
	item->duration = 0.0f;

	return item;
}

void
podcast_item_free(podcast_item_t * item) {

	if (item->file) {
		free(item->file);
	}
	if (item->title) {
		free(item->title);
	}
	if (item->desc) {
		free(item->desc);
	}
	if (item->url) {
		free(item->url);
	}
	free(item);
}

gint
podcast_item_compare_date(gconstpointer list1, gconstpointer list2) {

	unsigned date1 = ((podcast_item_t *)list1)->date;
	unsigned date2 = ((podcast_item_t *)list2)->date;

	if (date1 < date2) {
		return 1;
	} else if (date1 > date2) {
		return -1;
	}

	return 0;
}

gint
podcast_item_compare_url(gconstpointer list, gconstpointer url) {

	return strcmp(((podcast_item_t *)list)->url, (char * )url);
}


char *
podcast_file_from_url(char * url) {

	char * str;
	char * valid = "abcdefghijklmnopqrstuvwxyz0123456789";
	char * lastdot;
	char * file;

	str = g_ascii_strdown(url, -1);
	lastdot = strrchr(str, '.');
	g_strcanon(str, valid, '_');

	if (lastdot != NULL) {
		*lastdot = '.';
	}

	if (strstr(str, "http___") != NULL) {
		file = strdup(str + 7);
	} else {
		file = strdup(str);
	}

	g_free(str);

	return file;
}

unsigned
parse_rfc822(char * str) {

	char * months[] = { "Ja", "F", "Mar", "Ap", "May", "Jun",
			    "Jul", "Au", "S", "O", "N", "D" };

	char * tz[] = { "UT", "GMT", "EST", "EDT", "CST", "CDT", "MST", "MDT", "PST", "PDT",
			"A", "B", "C", "D", "E", "F", "G", "H", "I", "K", "L", "M",
			"N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z" };
	int tzval[] = { 0, 0, -5, -4, -6, -5, -7, -6, -8, -7,
			-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12,
			1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0 };
	int i;

	int y = 0;  /* year */
	int m = 0;  /* month */
	int d = 0;  /* day */
	int H = 0;  /* hour */
	int M = 0;  /* min */
	int S = 0;  /* sec */
	char b[16]; /* month name */
	char z[16]; /* timezone */


	if (sscanf(str, "%*[^,], %d %15s %d %d:%d:%d %15s", &d, b, &y, &H, &M, &S, z) == 7) {

		GDate * epoch;
		GDate * date;

		for (i = 0; i < 12; i++) {
			if (strstr(b, months[i]) != NULL) {
				m = i + 1;
				break;
			}
		}

		if (y < 100) {
			y += 2000;
		} else if (y < 1000) {
			y += 1900;
		}

		if (!g_date_valid_dmy(d, m, y)) {
			return 0;
		}

		for (i = 0; i < sizeof(tzval) / sizeof(int); i++) {
			if (!strcmp(tz[i], z)) {
				H += tzval[i];
				break;
			}
		}

		if (sscanf(z, "%d", &i) == 1) {
			H -= i / 100;
			M -= i % 100;
		}

		epoch = g_date_new_dmy(1, 1, 1970);
		date = g_date_new_dmy(d, m, y);

		return g_date_days_between(epoch, date) * 86400 + H * 3600 + M * 60 + S;
	}

	return 0;
}

unsigned
parse_ymd(char * str) {

	int a = 0;
	int b = 0;
	int c = 0;

	if (sscanf(str, "%d-%d-%d", &a, &b, &c) == 3) {

		GDate * epoch;
		GDate * date;
		int y = 0, m = 0, d = 0;

		if (a > 1900) {
			y = a;
			m = b;
			d = c;
		} else if (c > 1900) {
			y = c;
			m = a;
			d = b;
		} else {
			return 0;
		}

		if (!g_date_valid_dmy(d, m, y)) {
			return 0;
		}

		epoch = g_date_new_dmy(1, 1, 1970);
		date = g_date_new_dmy(d, m, y);

		return g_date_days_between(epoch, date) * 86400;
	}

	return 0;
}

unsigned
parse_date(char * str) {

	GTimeVal tval;
	unsigned val;

	if ((val = parse_rfc822(str)) > 0) {
		return val;
	}

	if ((val = parse_ymd(str)) > 0) {
		return val;
	}

	g_get_current_time(&tval);
	return tval.tv_sec;
}

int
podcast_generic_download(podcast_t * podcast, char * url, char * path) {

	http_session_t * session;
	char buf[BUFSIZE];
	FILE * out;
	long long pos = 0;
	int n_read;
	int ret;
	int credit = 5;
	int penalty = 0;

	printf("downloading '%s' => '%s'\n", url, path);

	if ((out = fopen(path, "wb")) == NULL) {
		fprintf(stderr, "podcast_generic_download: unable to open file %s\n", path);
		return -1;
	}

	printf("output file opened successfully.\n");

	while (credit > 0) {

		if (podcast->state == PODCAST_STATE_ABORTED) {
			break;
		}

		printf("download credit = %d\n", credit);

		if ((session = httpc_new()) == NULL) {
			fclose(out);
			unlink(path);
			return -1;
		}

		printf("start position = %lld\n", pos);

		if ((ret = httpc_init(session, NULL, url,
				      options.inet_use_proxy,
				      options.inet_proxy,
				      options.inet_proxy_port,
				      options.inet_noproxy_domains, 0L)) != HTTPC_OK) {

			fprintf(stderr, "podcast_generic_download: httpc_init failed, ret = %d\n", ret);
			httpc_del(session);
			--credit;
			continue;
		}

		printf("session type = %d\n", session->type);

		if (httpc_seek(session, pos, SEEK_SET) < -1) {
			fprintf(stderr, "httpc_seek failed\n");
			--credit;
			continue;
		}

		penalty = 1;
		while ((n_read = httpc_read(session, buf, BUFSIZE)) > 0) {

			if (podcast->state == PODCAST_STATE_ABORTED) {
				break;
			}

			pos += n_read;
			penalty = 0;
			fwrite(buf, sizeof(char), n_read, out);
			printf(".");
			fflush(stdout);
		}

		httpc_close(session);
		httpc_del(session);

		if (podcast->state == PODCAST_STATE_ABORTED) {
			break;
		}

		if (n_read < 0) {
			printf("\ndownload error, penalty = %d\n", penalty);
			credit -= penalty;
			continue;
		}

		break;
	}

	if (podcast->state == PODCAST_STATE_ABORTED || credit == 0) {

		if (podcast->state == PODCAST_STATE_ABORTED) {
			printf("\naborted by user, unlink downloaded file\n");
		} else {
			printf("\nno more credit, download failed\n");
		}

		fclose(out);
		unlink(path);
		return -1;
	}

	printf("\ndownload finished successfully, pos = %lld\n", pos);
	fclose(out);
	return 0;
}

void
string_remove_html(char * str) {

	int i, j;

	if (str == NULL) {
		return;
	}

	for (i = j = 0; str[i]; i++) {
		if (str[i] == '<') {
			while (str[i] && str[i] != '>') {
				++i;
			}
			if (str[i] == '\0') {
				break;
			}
		} else {
			str[j++] = str[i];
		}
	}

	str[j] = '\0';
}

void
parse_rss_item(podcast_t * podcast, GSList ** list, xmlDocPtr doc, xmlNodePtr item) {

	podcast_item_t * pitem;
	xmlNodePtr node;

	if ((pitem = podcast_item_new()) == NULL) {
		return;
	}

	for (node = item->xmlChildrenNode; node != NULL; node = node->next) {
		if (!xmlStrcmp(node->name, (const xmlChar *)"title")) {
                        pitem->title = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(node->name, (const xmlChar *)"description")) {
                        pitem->desc = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(node->name, (const xmlChar *)"summary")) {
			if (pitem->desc == NULL) {
				pitem->desc = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
			}
		} else if (!xmlStrcmp(node->name, (const xmlChar *)"enclosure")) {
			xmlChar * tmp;
			pitem->url = (char *)xmlGetProp(node, (const xmlChar *)"url");
			if ((tmp = xmlGetProp(node, (const xmlChar *)"length")) != NULL) {
				sscanf((char *)tmp, "%u", &pitem->size);
				xmlFree(tmp);
			}
		} else if (!xmlStrcmp(node->name, (const xmlChar *)"pubDate")) {
			xmlChar * tmp = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
			pitem->date = parse_date((char *)tmp);
			xmlFree(tmp);
		}
	}

	if (pitem->url == NULL) {
		podcast_item_free(pitem);
		return;
	}

	string_remove_html(pitem->desc);

	if (pitem->title == NULL) {
		pitem->title = strdup(_("Untitled"));
	}

	if (g_slist_find_custom(*list, pitem->url, podcast_item_compare_url) == NULL) {
		*list = g_slist_prepend(*list, pitem);
	} else {
		podcast_item_free(pitem);
	}
}

void
parse_rss(podcast_t * podcast, GSList ** list, xmlDocPtr doc, xmlNodePtr rss) {

	xmlNodePtr channel;
	xmlNodePtr node;


	for (channel = rss->xmlChildrenNode; channel != NULL; channel = channel->next) {
		if (!xmlStrcmp(channel->name, (const xmlChar *)"channel")) {
			break;
		}
	}

	if (channel == NULL) {
		fprintf(stderr, "parse_rss: no channel found\n");
		return;
	}

	for (node = channel->xmlChildrenNode; node != NULL; node = node->next) {

		if (!xmlStrcmp(node->name, (const xmlChar *)"title")) {
			if (podcast->title) {
				free(podcast->title);
			}
                        podcast->title = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(node->name, (const xmlChar *)"author")) {
			if (podcast->author) {
				free(podcast->author);
			}
                        podcast->author = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(node->name, (const xmlChar *)"description")) {
			if (podcast->desc) {
				free(podcast->desc);
			}
                        podcast->desc = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(node->name, (const xmlChar *)"summary")) {
			if (podcast->desc == NULL) {
				podcast->desc = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
			}
		}
	}

	if (podcast->title == NULL) {
		podcast->title = strdup(_("Untitled"));
	}

	string_remove_html(podcast->desc);

	for (node = channel->xmlChildrenNode; node != NULL; node = node->next) {
		if (!xmlStrcmp(node->name, (const xmlChar *)"item")) {
			parse_rss_item(podcast, list, doc, node);
		}
	}
}

int
podcast_parse(podcast_t * podcast, GSList ** list) {

	xmlDocPtr doc;
	xmlNodePtr node;
	char filename[MAXLEN];
	char * file;

	file = podcast_file_from_url(podcast->url);
	snprintf(filename, MAXLEN-1, "%s/.%s", podcast->dir, file);
	free(file);

	if (podcast_generic_download(podcast, podcast->url, filename) < 0) {
		return -1;
	}

	doc = xmlParseFile(filename);
	if (doc == NULL) {
		unlink(filename);
		return -1;
	}

	node = xmlDocGetRootElement(doc);
	if (node == NULL) {
		xmlFreeDoc(doc);
		unlink(filename);
		return -1;
	}

	if (!xmlStrcmp(node->name, (const xmlChar *)"rss")) {
		printf("RSS detected\n");
		parse_rss(podcast, list, doc, node);
	} else {
		printf("unknown format: %s\n", node->name);
	}

	xmlFreeDoc(doc);
	unlink(filename);

	return 0;
}


GSList *
podcast_list_remove_item(podcast_t * podcast, GSList * list, GSList * litem) {

	podcast_item_t * item = (podcast_item_t *)litem->data;

	if (item->file) {
		if (unlink(item->file) < 0) {
			fprintf(stderr, "unlink: unable to unlink %s\n", item->file);
			perror("unlink");
		}

		podcast->items = g_slist_remove(podcast->items, item);
		store_podcast_remove_item(podcast, item);
	} else {
		podcast_item_free(item);
	}

	return g_slist_delete_link(list, litem);
}

void
podcast_item_download(podcast_t * podcast, GSList ** list, GSList * node) {

	podcast_item_t * item = (podcast_item_t *)node->data;
	char * file;
	char path[MAXLEN];
	float duration;
	struct stat statbuf;	

	printf("podcast_item_download\n");

	file = podcast_file_from_url(item->url);
	snprintf(path, MAXLEN-1, "%s/%s", podcast->dir, file);
	free(file);

	if (podcast_generic_download(podcast, item->url, path) < 0) {
		printf("aborted by user or premanent download error, could not finish download\n");
		printf("moving on to next file\n");
		*list = podcast_list_remove_item(podcast, *list, node);
		return;
	}

	item->file = strdup(path);
	duration = get_file_duration(item->file);
	item->duration = duration < 0.0f ? 0.0f : duration;

	if (stat(item->file, &statbuf) == 0) {
		item->size = statbuf.st_size;
	}

	podcast->items = g_slist_prepend(podcast->items, item);
	store_podcast_add_item(podcast, item);
}

void
podcast_apply_limits(podcast_t * podcast, GSList ** list) {

	GSList * node;
	unsigned size = 0;
	int count = 0;

	printf("podcast_apply_limits\n");
	for (node = *list; node; node = node->next) {
		podcast_item_t * item = (podcast_item_t *)node->data;
		printf("item url = %s\n", item->url);
		size += item->size;
		++count;
	}

	node = g_slist_last(*list);
	while (*list != NULL &&
	       ((podcast->flags & PODCAST_DATE_LIMIT &&
		 podcast->last_checked - ((podcast_item_t *)node->data)->date > podcast->date_limit)
		||
		(podcast->flags & PODCAST_SIZE_LIMIT &&
		 size > podcast->size_limit)
		||
		(podcast->flags & PODCAST_COUNT_LIMIT &&
		 count > podcast->count_limit))) {

		size -= ((podcast_item_t *)node->data)->size;
		--count;
		*list = podcast_list_remove_item(podcast, *list, node);
		node = g_slist_last(*list);
	}
}

int
podcast_download_next(podcast_t * podcast, GSList ** list) {

	GSList * node;

	printf("podcast_download_next\n");
	for (node = *list; node; node = node->next) {
		podcast_item_t * item = (podcast_item_t *)node->data;

		if (podcast->state == PODCAST_STATE_ABORTED) {
			if (item->file == NULL) {
				podcast_item_free(item);
			}
			continue;
		}

		if (item->file == NULL) {
			podcast_item_download(podcast, list, node);
			return 1;
		}
	}

	return 0;
}

void *
podcast_update_thread(void * arg) {

	podcast_t * podcast = (podcast_t *)arg;

	GTimeVal tval;
	GSList * list;

	AQUALUNG_THREAD_DETACH();


	list = g_slist_copy(podcast->items);

	if (podcast_parse(podcast, &list) < 0) {
		goto finish;
	}

	list = g_slist_sort(list, podcast_item_compare_date);

	g_get_current_time(&tval);
	podcast->last_checked = tval.tv_sec;

	do {
		podcast_apply_limits(podcast, &list);
	} while (podcast_download_next(podcast, &list));

 finish:
	g_slist_free(list);

	store_podcast_update_podcast(podcast);

	return NULL;
}

void
podcast_update(podcast_t * podcast) {

	if (podcast->state == PODCAST_STATE_IDLE) {

		AQUALUNG_THREAD_DECLARE(thread_id);

		podcast->state = PODCAST_STATE_UPDATE;
		AQUALUNG_THREAD_CREATE(thread_id, NULL, podcast_update_thread, podcast);
	}
}

#endif /* HAVE_PODCAST */
