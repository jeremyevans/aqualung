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

#ifndef AQUALUNG_UTILS_XML_H
#define AQUALUNG_UTILS_XML_H

#include <libxml/tree.h>


void xml_save_str(xmlNodePtr node, char * varname, char * var);
void xml_save_int(xmlNodePtr node, char * varname, int var);
void xml_save_uint(xmlNodePtr node, char * varname, unsigned var);
void xml_save_float(xmlNodePtr node, char * varname, float var);
void xml_save_int_array(xmlNodePtr node, char * varname, int * var, int idx);

void xml_load_str(xmlDocPtr doc, xmlNodePtr node, char * varname, char * var, size_t var_size);
void xml_load_str_dup(xmlDocPtr doc, xmlNodePtr node, char * varname, char ** var);
void xml_load_int(xmlDocPtr doc, xmlNodePtr node, char * varname, int * var);
void xml_load_uint(xmlDocPtr doc, xmlNodePtr node, char * varname, unsigned * var);
void xml_load_float(xmlDocPtr doc, xmlNodePtr node, char * varname, float * var);
void xml_load_int_array(xmlDocPtr doc, xmlNodePtr node, char * varname, int * var, int idx);


#endif /* AQUALUNG_UTILS_XML_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8:

