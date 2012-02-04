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

#ifndef AQUALUNG_PORTS_H
#define AQUALUNG_PORTS_H


#define MAX_JACK_CLIENTS 128


void port_setup_dialog(void);
void ports_clicked_close(GtkWidget * widget, gpointer * data);


#endif /* AQUALUNG_PORTS_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

