/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi
              (C) 2006 Tomasz Maka

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

#ifndef _IFP_DEVICE_H
#define _IFP_DEVICE_H

#define PARENTDIR ("..")
#define DIRID ("<dir>")

enum {
        COLUMN_NAME = 0,
        COLUMN_TYPE_SIZE
};

enum {
        TYPE_NONE = 0,
        TYPE_DIR,
        TYPE_FILE
};

enum {
        UPLOAD_MODE = 0,
        DOWNLOAD_MODE
};

void aifp_transfer_files(gint mode);

#endif /* _IFP_DEVICE_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8:

