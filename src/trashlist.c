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
#include <stdlib.h>

#include "common.h"
#include "trashlist.h"


trashlist_t *
trashlist_new(void) {

	trashlist_t * root;

	if ((root = (trashlist_t *)malloc(sizeof(trashlist_t))) == NULL) {
		fprintf(stderr, "trashlist.c : trashlist_new(): malloc error\n");
		return NULL;
	}

	root->ptr = NULL;
	root->next = NULL;

	return root;
}


void
trashlist_add(trashlist_t * root, void * ptr) {

	trashlist_t * q_item = NULL;
	trashlist_t * q_prev;
	
	if ((q_item = (trashlist_t *)malloc(sizeof(trashlist_t))) == NULL) {
		fprintf(stderr, "trashlist.c : trashlist_add(): malloc error\n");
		return;
	}

	q_item->ptr = ptr;
	q_item->next = NULL;

	q_prev = root;
	while (q_prev->next != NULL) {
		q_prev = q_prev->next;
	}
	q_prev->next = q_item;
}


void
trashlist_free(trashlist_t * root) {

	trashlist_t * p;
	trashlist_t * q;
	
	if (root == NULL)
		return;
	
	p = root->next;
	while (p != NULL) {
		q = p->next;
		free(p->ptr);
		free(p);
		p = q;
	}

	free(root);
}
