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

#ifdef HAVE_LOOP

#include <gtk/gtk.h>

#include "loop_bar.h"


#define AQUALUNG_LOOP_BAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), AQUALUNG_TYPE_LOOP_BAR, AqualungLoopBarPrivate))

G_DEFINE_TYPE (AqualungLoopBar, aqualung_loop_bar, GTK_TYPE_DRAWING_AREA);


typedef struct _AqualungLoopBarPrivate AqualungLoopBarPrivate;

struct _AqualungLoopBarPrivate {

	float start;
	float end;

	gboolean start_dragged;
	gboolean end_dragged;

	gboolean start_hover;
	gboolean end_hover;

	gint prev_x;
	gint width;
	gint margin;
};

enum {
	RANGE_CHANGED,
	LAST_SIGNAL
};

static guint aqualung_loop_bar_signals[LAST_SIGNAL] = { 0 };


void
_loop_bar_marshal_VOID__FLOAT_FLOAT(GClosure * closure,
				    GValue * return_value,
				    guint n_param_values,
				    const GValue * param_values,
				    gpointer invocation_hint,
				    gpointer marshal_data)
{
	typedef void (* GMarshalFunc_VOID__FLOAT_FLOAT) (gpointer data1,
							 gfloat arg_1,
							 gfloat arg_2,
							 gpointer data2);
	GMarshalFunc_VOID__FLOAT_FLOAT callback;
	GCClosure * cc = (GCClosure *)closure;
	gpointer data1, data2;

	g_return_if_fail(n_param_values == 3);

	if (G_CCLOSURE_SWAP_DATA(closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	} else {
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}

	callback = (GMarshalFunc_VOID__FLOAT_FLOAT) (marshal_data ? marshal_data : cc->callback);

	callback(data1,
		 g_value_get_float(param_values + 1),
		 g_value_get_float(param_values + 2),
		 data2);
}


void
aqualung_loop_bar_draw_marker(GtkWidget * widget, cairo_t * cr, int x, int height, gboolean hover) {

	AqualungLoopBarPrivate * priv = AQUALUNG_LOOP_BAR_GET_PRIVATE(widget);

	GdkColor fg = widget->style->fg[hover ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL];

	double fg_r = fg.red / 65535.0;
	double fg_g = fg.green / 65535.0;
	double fg_b = fg.blue / 65535.0;

	cairo_new_path(cr);
	cairo_move_to(cr, x - priv->width, 0);

	cairo_rel_line_to(cr, 2 * priv->width, 0);
	cairo_rel_line_to(cr, 0, height / 2);
	cairo_rel_line_to(cr, -priv->width, height / 2);
	cairo_rel_line_to(cr, -priv->width, -height / 2);
	cairo_rel_line_to(cr, 0, -height / 2);
	cairo_close_path(cr);

	cairo_set_source_rgb(cr, fg_r, fg_g, fg_b);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, fg_r / 2, fg_g / 2, fg_b / 2);
	cairo_stroke(cr);
}


gboolean
aqualung_loop_bar_expose(GtkWidget * widget, GdkEventExpose * event) {

        cairo_t * cr;
	int x_start;
	int x_end;
	int height;
	
	GdkColor bg = widget->style->bg[GTK_STATE_SELECTED];
	GdkColor bg2 = widget->style->bg[GTK_STATE_ACTIVE];

	double bg_r = bg.red / 65535.0;
	double bg_g = bg.green / 65535.0;
	double bg_b = bg.blue / 65535.0;

	double bg2_r = bg2.red / 65535.0;
	double bg2_g = bg2.green / 65535.0;
	double bg2_b = bg2.blue / 65535.0;

	AqualungLoopBarPrivate * priv = AQUALUNG_LOOP_BAR_GET_PRIVATE(widget);

	x_start = (widget->allocation.width - 2 * priv->margin) * priv->start + priv->margin;
	x_end = (widget->allocation.width - 2 * priv->margin) * priv->end + priv->margin;
	height = widget->allocation.height;

        cr = gdk_cairo_create(widget->window);

        cairo_rectangle(cr,
                        event->area.x, event->area.y,
                        event->area.width, event->area.height);
        cairo_clip(cr);

	cairo_set_line_width(cr, 1.0);

	cairo_new_path(cr);
        cairo_rectangle(cr, x_start, 0, x_end - x_start, height);
	cairo_set_source_rgb(cr, bg_r, bg_g, bg_b);
	cairo_fill_preserve(cr);

	aqualung_loop_bar_draw_marker(widget, cr, x_end, height, priv->end_hover);
	aqualung_loop_bar_draw_marker(widget, cr, x_start, height, priv->start_hover);

	cairo_new_path(cr);
	cairo_move_to(cr, 0, height);
	cairo_rel_line_to(cr, 0, -height);
	cairo_rel_line_to(cr, widget->allocation.width, 0);
	cairo_set_source_rgb(cr, bg2_r, bg2_g, bg2_b);
	cairo_stroke(cr);

	cairo_new_path(cr);
	cairo_move_to(cr, 0, height);
	cairo_rel_line_to(cr, widget->allocation.width, 0);
	cairo_rel_line_to(cr, 0, -height);
	cairo_set_source_rgb(cr,
			     bg2_r * 2,
			     bg2_g * 2,
			     bg2_b * 2);
	cairo_stroke(cr);

        cairo_destroy(cr);

	return FALSE;
}


void
aqualung_loop_bar_redraw_canvas(GtkWidget * widget) {

        GdkRegion *region;
        
        if (!widget->window) {
		return;
	}

        region = gdk_drawable_get_clip_region(widget->window);
        gdk_window_invalidate_region(widget->window, region, TRUE);
        gdk_window_process_updates(widget->window, TRUE);
        gdk_region_destroy(region);
}


void
aqualung_loop_bar_update(GtkWidget * widget, int x) {

	AqualungLoopBarPrivate * priv = AQUALUNG_LOOP_BAR_GET_PRIVATE(widget);
	gboolean redraw = FALSE;

	if (priv->start_dragged ||
	    (!priv->end_dragged &&
	     x >= (widget->allocation.width - 2 * priv->margin) * priv->start + priv->margin - priv->width &&
	     x <= (widget->allocation.width - 2 * priv->margin) * priv->start + priv->margin + priv->width)) {
		if (priv->start_hover == FALSE) {
			priv->start_hover = TRUE;
			priv->end_hover = FALSE;
			redraw = TRUE;
		}
	} else {
		if (priv->start_hover == TRUE) {
			priv->start_hover = FALSE;
			redraw = TRUE;
		}
	}

	if (priv->end_dragged ||
	    (!priv->start_dragged &&
	     x >= (widget->allocation.width - 2 * priv->margin) * priv->end + priv->margin - priv->width &&
	     x <= (widget->allocation.width - 2 * priv->margin) * priv->end + priv->margin + priv->width)) {
		if (priv->end_hover == FALSE && priv->start_hover == FALSE) {
			priv->end_hover = TRUE;
			redraw = TRUE;
		}
	} else {
		if (priv->end_hover == TRUE) {
			priv->end_hover = FALSE;
			redraw = TRUE;
		}
	}

	if (priv->start_dragged) {
		priv->start += (float)(x - priv->prev_x) / (widget->allocation.width - 2 * priv->margin);
		priv->start = priv->start < 0 ? 0 : priv->start;
		priv->start = priv->start > priv->end ? priv->end : priv->start;

		priv->prev_x = x;

		redraw = TRUE;

	} else if (priv->end_dragged) {

		priv->end += (float)(x - priv->prev_x) / (widget->allocation.width - 2 * priv->margin);
		priv->end = priv->end < priv->start ? priv->start : priv->end;
		priv->end = priv->end > 1 ? 1 : priv->end;

		priv->prev_x = x;

		redraw = TRUE;
	}

	if (redraw) {
		aqualung_loop_bar_redraw_canvas(widget);
	}
}


gboolean
aqualung_loop_bar_motion_notify(GtkWidget * widget, GdkEventMotion * event) {

	aqualung_loop_bar_update(widget, event->x);

	return FALSE;
}


gboolean
aqualung_loop_bar_button_press(GtkWidget * widget, GdkEventButton * event) {

	AqualungLoopBarPrivate * priv = AQUALUNG_LOOP_BAR_GET_PRIVATE(widget);

        if (event->state & GDK_SHIFT_MASK) {  /* SHIFT */
		priv->start = 0.0f;
		priv->end = 1.0f;
		aqualung_loop_bar_redraw_canvas(widget);
		return TRUE;
	}

	if (event->button == 1) {

		priv->prev_x = event->x;

		if (event->x >= (widget->allocation.width - 2 * priv->margin) * priv->start + priv->margin - priv->width &&
		    event->x <= (widget->allocation.width - 2 * priv->margin) * priv->start + priv->margin + priv->width) {

			priv->start_dragged = TRUE;

		} else if (event->x >= (widget->allocation.width - 2 * priv->margin) * priv->end + priv->margin - priv->width &&
			   event->x <= (widget->allocation.width - 2 * priv->margin) * priv->end + priv->margin + priv->width) {

			priv->end_dragged = TRUE;
		}
	}

	return FALSE;
}


gboolean
aqualung_loop_bar_button_release(GtkWidget * widget, GdkEventButton * event) {

	AqualungLoopBarPrivate * priv = AQUALUNG_LOOP_BAR_GET_PRIVATE(widget);

	priv->start_dragged = FALSE;
	priv->end_dragged = FALSE;

	aqualung_loop_bar_update(widget, event->x);

	g_signal_emit(widget,
		      aqualung_loop_bar_signals[RANGE_CHANGED],
		      0,
		      priv->start, priv->end);

	return FALSE;
}


gboolean
aqualung_loop_bar_enter_notify(GtkWidget * widget, GdkEventCrossing * event) {

	aqualung_loop_bar_update(widget, event->x);

	return FALSE;
}


gboolean
aqualung_loop_bar_leave_notify(GtkWidget * widget, GdkEventCrossing * event) {

	AqualungLoopBarPrivate * priv;

	if (event->y >= 0 && event->y < widget->allocation.height &&
	    event->x >= 0 && event->x < widget->allocation.width) {
		/* before each button press event we receive a leave
		 * notify event, which should be neglected. */
		return FALSE;
	}

	priv = AQUALUNG_LOOP_BAR_GET_PRIVATE(widget);

	priv->start_hover = priv->start_dragged;
	priv->end_hover = priv->end_dragged;

	aqualung_loop_bar_redraw_canvas(widget);

	return FALSE;
}


void
aqualung_loop_bar_class_init(AqualungLoopBarClass * class) {

	GObjectClass * obj_class;
        GtkWidgetClass * widget_class;

	obj_class = G_OBJECT_CLASS(class);
        widget_class = GTK_WIDGET_CLASS(class);

        aqualung_loop_bar_signals[RANGE_CHANGED] = g_signal_new (
                        "range-changed",
                        G_OBJECT_CLASS_TYPE(obj_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET(AqualungLoopBarClass, range_changed),
                        NULL, NULL,
                        _loop_bar_marshal_VOID__FLOAT_FLOAT,
                        G_TYPE_NONE, 2,
                        G_TYPE_FLOAT,
                        G_TYPE_FLOAT);

        widget_class->expose_event = aqualung_loop_bar_expose;
	widget_class->button_press_event = aqualung_loop_bar_button_press;
	widget_class->button_release_event = aqualung_loop_bar_button_release;
	widget_class->motion_notify_event = aqualung_loop_bar_motion_notify;
	widget_class->enter_notify_event = aqualung_loop_bar_enter_notify;
	widget_class->leave_notify_event = aqualung_loop_bar_leave_notify;

	g_type_class_add_private(obj_class, sizeof(AqualungLoopBarPrivate));
}


void
aqualung_loop_bar_init(AqualungLoopBar * bar) {

	AqualungLoopBarPrivate * priv = AQUALUNG_LOOP_BAR_GET_PRIVATE(bar);

	priv->start = 0.0f;
	priv->end = 1.0f;
	priv->start_dragged = FALSE;
	priv->end_dragged = FALSE;
	priv->start_hover = FALSE;
	priv->end_hover = FALSE;
	priv->width = 4;
	priv->margin = 16;

	gtk_widget_add_events(GTK_WIDGET(bar),
			      GDK_BUTTON_PRESS_MASK |
			      GDK_BUTTON_RELEASE_MASK |
			      GDK_POINTER_MOTION_MASK |
			      GDK_ENTER_NOTIFY_MASK |
			      GDK_LEAVE_NOTIFY_MASK);
}

GtkWidget *
aqualung_loop_bar_new(float start, float end) {

	GtkWidget * widget;
	AqualungLoopBarPrivate * priv;

	widget = g_object_new(AQUALUNG_TYPE_LOOP_BAR, NULL);
	priv = AQUALUNG_LOOP_BAR_GET_PRIVATE(AQUALUNG_LOOP_BAR(widget));

	priv->start = start;
	priv->end = end;

        return widget;
}

void
aqualung_loop_bar_adjust_start(AqualungLoopBar * bar, float start) {

	AqualungLoopBarPrivate * priv = AQUALUNG_LOOP_BAR_GET_PRIVATE(bar);

	if (start < priv->end) {
		priv->start = start;
		aqualung_loop_bar_redraw_canvas(GTK_WIDGET(bar));
		g_signal_emit(GTK_WIDGET(bar),
			      aqualung_loop_bar_signals[RANGE_CHANGED],
			      0,
			      priv->start, priv->end);
	}
}

void
aqualung_loop_bar_adjust_end(AqualungLoopBar * bar, float end) {

	AqualungLoopBarPrivate * priv = AQUALUNG_LOOP_BAR_GET_PRIVATE(bar);

	if (end > priv->start) {
		priv->end = end;
		aqualung_loop_bar_redraw_canvas(GTK_WIDGET(bar));
		g_signal_emit(GTK_WIDGET(bar),
			      aqualung_loop_bar_signals[RANGE_CHANGED],
			      0,
			      priv->start, priv->end);
	}
}

#endif /* HAVE_LOOP */

// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

