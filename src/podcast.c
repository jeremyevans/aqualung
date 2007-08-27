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
		return -1;
	} else if (date1 > date2) {
		return 1;
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

	char * months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
			    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

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
	char a[16]; /* weekday (unused) */
	char b[16]; /* month name */
	char z[16]; /* timezone */


	if (sscanf(str, "%s %d %s %d %d:%d:%d %s", a, &d, b, &y, &H, &M, &S, z) == 8) {

		GDate * epoch;
		GDate * date;

		for (i = 0; i < 12; i++) {
			if (!strcmp(months[i], b)) {
				m = i + 1;
				break;
			}
		}

		if (y < 90) {
			y += 2000;
		} else if (y < 100) {
			y += 1900;
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

int
podcast_generic_download(char * url, char * path) {

	http_session_t * session;
	char buf[BUFSIZE];
	FILE * out;
	int n_read;
	int ret;

	if ((out = fopen(path, "wb")) == NULL) {
		fprintf(stderr, "podcast_generic_download: unable to open file %s\n", path);
		return -1;
	}

	if ((session = httpc_new()) == NULL) {
		return -1;
	}

	if ((ret = httpc_init(session, NULL, url,
			      options.inet_use_proxy,
			      options.inet_proxy,
			      options.inet_proxy_port,
			      options.inet_noproxy_domains, 0L)) != HTTPC_OK) {

		fprintf(stderr, "podcast_generic_download: httpc_init failed, ret = %d\n", ret);
		httpc_del(session);
		return -1;
	}

	while ((n_read = httpc_read(session, buf, BUFSIZE)) > 0) {
		fwrite(buf, sizeof(char), n_read, out);
		printf(".");
		fflush(stdout);
	}

	httpc_close(session);
	httpc_del(session);

	fclose(out);

	printf(" done.\n");

	return 0;
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
		} else if (!xmlStrcmp(node->name, (const xmlChar *)"enclosure")) {
			xmlChar * tmp;
			pitem->url = (char *)xmlGetProp(node, (const xmlChar *)"url");
			if ((tmp = xmlGetProp(node, (const xmlChar *)"length")) != NULL) {
				sscanf((char *)tmp, "%u", &pitem->size);
				xmlFree(tmp);
			}
		} else if (!xmlStrcmp(node->name, (const xmlChar *)"pubDate")) {
			xmlChar * tmp = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
			pitem->date = parse_rfc822((char *)tmp);
			xmlFree(tmp);
		}
	}

	if (pitem->url == NULL || pitem->size == 0) {
		podcast_item_free(pitem);
		return;
	}

	if (pitem->date == 0) {
		GTimeVal tval;
		g_get_current_time(&tval);
		pitem->date = tval.tv_sec;
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
                        podcast->title = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(node->name, (const xmlChar *)"author")) {
                        podcast->author = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		} else if (!xmlStrcmp(node->name, (const xmlChar *)"description")) {
                        podcast->desc = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		}
	}

	for (node = channel->xmlChildrenNode; node != NULL; node = node->next) {
		if (!xmlStrcmp(node->name, (const xmlChar *)"item")) {
			parse_rss_item(podcast, list, doc, node);
		}
	}
}

void
podcast_parse(podcast_t * podcast, GSList ** list) {

	xmlDocPtr doc;
	xmlNodePtr node;
	char filename[MAXLEN];
	char * file;

	file = podcast_file_from_url(podcast->url);
	snprintf(filename, MAXLEN-1, "%s/.%s", podcast->dir, file);
	free(file);

	if (podcast_generic_download(podcast->url, filename) < 0) {
		return;
	}

	doc = xmlParseFile(filename);
	if (doc == NULL) {
		return;
	}

	node = xmlDocGetRootElement(doc);
	if (node == NULL) {
		xmlFreeDoc(doc);
		return;
	}

	if (!xmlStrcmp(node->name, (const xmlChar *)"rss")) {
		printf("RSS detected\n");
		parse_rss(podcast, list, doc, node);
	} else {
		printf("unknown format: %s\n", node->name);
	}

	xmlFreeDoc(doc);
}


GSList *
podcast_remove_first_item(podcast_t * podcast, GSList * list) {

	podcast_item_t * item = (podcast_item_t *)list->data;

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

	return g_slist_remove_link(list, list);
}

void
podcast_item_download(podcast_t * podcast, podcast_item_t * item) {

	char * file;
	char path[MAXLEN];
	float duration;
	

	printf("podcast_item_download\n");

	file = podcast_file_from_url(item->url);
	snprintf(path, MAXLEN-1, "%s/%s", podcast->dir, file);
	free(file);
	item->file = strdup(path);

	printf("downloading '%s' => '%s'\n", item->url, item->file);

	if (podcast_generic_download(item->url, item->file) < 0) {
		podcast_item_free(item);
		return;
	}

	duration = get_file_duration(item->file);
	item->duration = duration < 0.0f ? 0.0f : duration;

	podcast->items = g_slist_prepend(podcast->items, item);
	store_podcast_add_item(podcast, item);
}

void *
podcast_update_thread(void * arg) {

	podcast_t * podcast = (podcast_t *)arg;
	unsigned size = 0;
	int count = 0;

	GTimeVal tval;
	GSList * list;
	GSList * node;

	AQUALUNG_THREAD_DETACH();


	list = g_slist_copy(podcast->items);
	podcast_parse(podcast, &list);
	list = g_slist_sort(list, podcast_item_compare_date);

	g_get_current_time(&tval);
	podcast->last_checked = tval.tv_sec;

	for (node = list; node; node = node->next) {
		podcast_item_t * item = (podcast_item_t *)node->data;
		size += item->size;
		++count;
	}

	while ((list != NULL) &&
	       ((podcast->flags & PODCAST_DATE_LIMIT && tval.tv_sec - ((podcast_item_t *)list->data)->date > podcast->date_limit) ||
		(podcast->flags & PODCAST_SIZE_LIMIT && size > podcast->size_limit) ||
		(podcast->flags & PODCAST_COUNT_LIMIT && count > podcast->count_limit))) {

		size -= ((podcast_item_t *)list->data)->size;
		--count;
		list = podcast_remove_first_item(podcast, list);
	}

	list = g_slist_reverse(list);

	for (node = list; node; node = node->next) {
		podcast_item_t * item = (podcast_item_t *)node->data;

		if (item->file == NULL) {
			podcast_item_download(podcast, item);
		}
	}

	g_slist_free(list);

	store_podcast_update_podcast(podcast);

	return NULL;
}

void
podcast_update(podcast_t * podcast) {

	if (podcast->updating == 0) {

		AQUALUNG_THREAD_DECLARE(thread_id);

		podcast->updating = 1;
		AQUALUNG_THREAD_CREATE(thread_id, NULL, podcast_update_thread, podcast);
	}
}

#endif /* HAVE_PODCAST */
