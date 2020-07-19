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
#include <string.h>
#include <libxml/globals.h>
#include <libxml/tree.h>

#include "common.h"
#include "utils_xml.h"


void
xml_save_str(xmlNodePtr node, char * varname, char * var) {

	xmlNewTextChild(node, NULL, (const xmlChar *)varname, (xmlChar *)var);
}

void
xml_save_int(xmlNodePtr node, char * varname, int var) {

	char str[32];

        arr_snprintf(str, "%d", var);
        xmlNewTextChild(node, NULL, (const xmlChar *)varname, (xmlChar *)str);
}

void
xml_save_uint(xmlNodePtr node, char * varname, unsigned var) {

	char str[32];

        arr_snprintf(str, "%u", var);
        xmlNewTextChild(node, NULL, (const xmlChar *)varname, (xmlChar *)str);
}

void
xml_save_float(xmlNodePtr node, char * varname, float var) {

	char str[32];

        arr_snprintf(str, "%.1f", var);
        xmlNewTextChild(node, NULL, (const xmlChar *)varname, (xmlChar *)str);
}

void
xml_save_int_array(xmlNodePtr node, char * varname, int * var, int idx) {

	char name[MAXLEN];

	arr_snprintf(name, "%s_%d", varname, idx);
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

	arr_snprintf(name, "%s_%d", varname, idx);
	xml_load_int(doc, node, name, var + idx);
}


// vim: shiftwidth=8:tabstop=8:softtabstop=8:

