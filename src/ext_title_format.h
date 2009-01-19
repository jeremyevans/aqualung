/*                                                     -*- linux-c -*-
    Copyright (C) 2008 Jeremy Evans

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

#ifndef _EXT_TITLE_FORMAT_H
#define _EXT_TITLE_FORMAT_H
#include "decoder/file_decoder.h"

void setup_extended_title_formatting(void);
char * extended_title_format(file_decoder_t * fdec);

#endif /* _EXT_TITLE_FORMAT_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8:

