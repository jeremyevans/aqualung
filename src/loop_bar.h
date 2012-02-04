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

#ifndef AQUALUNG_LOOP_BAR_H
#define AQUALUNG_LOOP_BAR_H

#include <config.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AQUALUNG_TYPE_LOOP_BAR             (aqualung_loop_bar_get_type ())
#define AQUALUNG_LOOP_BAR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), AQUALUNG_TYPE_LOOP_BAR, AqualungLoopBar))
#define AQUALUNG_LOOP_BAR_CLASS(obj)       (G_TYPE_CHECK_CLASS_CAST ((obj), AQUALUNG_LOOP_BAR,  AqualungLoopBarClass))
#define AQUALUNG_IS_LOOP_BAR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AQUALUNG_TYPE_LOOP_BAR))
#define AQUALUNG_IS_LOOP_BAR_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE ((obj), AQUALUNG_TYPE_LOOP_BAR))
#define AQUALUNG_LOOP_BAR_GET_CLASS        (G_TYPE_INSTANCE_GET_CLASS ((obj), AQUALUNG_TYPE_LOOP_BAR, AqualungLoopBarClass))

typedef struct _AqualungLoopBar AqualungLoopBar;
typedef struct _AqualungLoopBarClass AqualungLoopBarClass;

struct _AqualungLoopBar {

        GtkDrawingArea parent;
};

struct _AqualungLoopBarClass {

        GtkDrawingAreaClass parent_class;

	void (* range_changed) (AqualungLoopBar * bar, float start, float end);
};

GType aqualung_loop_bar_get_type(void) G_GNUC_CONST;

GtkWidget * aqualung_loop_bar_new(float start, float end);

void aqualung_loop_bar_adjust_start(AqualungLoopBar * bar, float start);
void aqualung_loop_bar_adjust_end(AqualungLoopBar * bar, float end);

G_END_DECLS


#endif /* AQUALUNG_LOOP_BAR_H */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

