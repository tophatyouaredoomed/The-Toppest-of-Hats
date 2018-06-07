/*
 * File: dw_gtk_viewport.c
 *
 * Copyright (C) 2001  Sebastian Geerken <S.Geerken@ping.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <gtk/gtk.h>
#include "dw_gtk_viewport.h"
#include "dw_container.h"

#include "debug.h"

static GtkLayoutClass *parent_class = NULL;

/* object/class initialisation */
static void Dw_gtk_viewport_init          (GtkDwViewport *viewport);
static void Dw_gtk_viewport_class_init    (GtkDwViewportClass *klass);

/* GtkObject methods */
static void Dw_gtk_viewport_destroy       (GtkObject *object);

/* GtkWidget methods */
static void Dw_gtk_viewport_realize       (GtkWidget *widget);
static void Dw_gtk_viewport_unrealize     (GtkWidget *widget);
static void Dw_gtk_viewport_draw          (GtkWidget *widget,
                                           GdkRectangle *area);
static gint Dw_gtk_viewport_expose        (GtkWidget *widget,
                                           GdkEventExpose *event);
static gint Dw_gtk_viewport_button_press  (GtkWidget *widget,
                                           GdkEventButton *event);
static gint Dw_gtk_viewport_button_release(GtkWidget *widget,
                                           GdkEventButton *event);
static gint Dw_gtk_viewport_motion_notify (GtkWidget *widget,
                                           GdkEventMotion *event);
static gint Dw_gtk_viewport_enter_notify  (GtkWidget *widget,
                                           GdkEventCrossing *event);
static gint Dw_gtk_viewport_leave_notify  (GtkWidget *widget,
                                           GdkEventCrossing *event);
static void Dw_gtk_viewport_adj_changed   (GtkAdjustment *adj,
                                           GtkDwViewport *viewport);

/*
 * Standard Gtk+ function
 */
GtkType a_Dw_gtk_viewport_get_type (void)
{
   static GtkType type = 0;

   if (!type) {
      GtkTypeInfo info = {
         "GtkDwViewport",
         sizeof (GtkDwViewport),
         sizeof (GtkDwViewportClass),
         (GtkClassInitFunc) Dw_gtk_viewport_class_init,
         (GtkObjectInitFunc) Dw_gtk_viewport_init,
         (GtkArgSetFunc) NULL,
         (GtkArgGetFunc) NULL,
         (GtkClassInitFunc) NULL
      };

      type = gtk_type_unique (GTK_TYPE_LAYOUT, &info);
   }

   return type;
}


/*
 * Standard Gtk+ function
 */
GtkWidget* a_Dw_gtk_viewport_new (GtkAdjustment *hadjustment,
                                  GtkAdjustment *vadjustment)
{
   GtkWidget *widget;

   widget = gtk_widget_new (GTK_TYPE_DW_VIEWPORT, NULL);
   gtk_layout_set_hadjustment (GTK_LAYOUT (widget), hadjustment);
   gtk_layout_set_vadjustment (GTK_LAYOUT (widget), vadjustment);

   /* Following two statements expect that the adjustments are passed as
    * arguments (!= NULL), and don't change. This is the case in dillo,
    * however, for more general perposes, the signal function
    * "set_scroll_adjustments" had to be redefined.
    */
   gtk_signal_connect (GTK_OBJECT (hadjustment), "value_changed",
                       GTK_SIGNAL_FUNC (Dw_gtk_viewport_adj_changed),
                       (gpointer) widget);
   gtk_signal_connect (GTK_OBJECT (vadjustment), "value_changed",
                       GTK_SIGNAL_FUNC (Dw_gtk_viewport_adj_changed),
                       (gpointer) widget);

   return widget;
}

/*********************************
 *                               *
 *  object/class initialisation  *
 *                               *
 *********************************/

/*
 * Standard Gtk+ function
 */
static void Dw_gtk_viewport_init (GtkDwViewport *viewport)
{
   GTK_WIDGET_UNSET_FLAGS (viewport, GTK_NO_WINDOW);
   GTK_WIDGET_UNSET_FLAGS (viewport, GTK_CAN_FOCUS);

   /* Without this, gtk_layout_{draw|expose} will clear the window.
      Look at gtklayout.c */
   GTK_WIDGET_SET_FLAGS (viewport, GTK_APP_PAINTABLE);

   viewport->back_pixmap = NULL;
   viewport->child = NULL;
   viewport->last_entered = NULL;
   viewport->resize_idle_id = -1;
   viewport->back_pixmap = NULL;
   viewport->anchor = NULL;
   viewport->anchor_idle_id = -1;
}


/*
 * Standard Gtk+ function
 */
static void Dw_gtk_viewport_class_init (GtkDwViewportClass *klass)
{
   GtkObjectClass *object_class;
   GtkWidgetClass *widget_class;

   parent_class = gtk_type_class (gtk_layout_get_type ());

   object_class = (GtkObjectClass*) klass;
   widget_class = (GtkWidgetClass*) klass;

   object_class->destroy = Dw_gtk_viewport_destroy;

   widget_class->realize = Dw_gtk_viewport_realize;
   widget_class->unrealize = Dw_gtk_viewport_unrealize;
   widget_class->draw = Dw_gtk_viewport_draw;
   widget_class->expose_event = Dw_gtk_viewport_expose;
   widget_class->button_press_event = Dw_gtk_viewport_button_press;
   widget_class->button_release_event = Dw_gtk_viewport_button_release;
   widget_class->motion_notify_event = Dw_gtk_viewport_motion_notify;
   widget_class->enter_notify_event = Dw_gtk_viewport_enter_notify;
   widget_class->leave_notify_event = Dw_gtk_viewport_leave_notify;
}


/***********************
 *                     *
 *  GtkObject methods  *
 *                     *
 ***********************/

/*
 * Standard Gtk+ function
 */
static void Dw_gtk_viewport_destroy (GtkObject *object)
{
   GtkDwViewport *viewport;

   viewport = GTK_DW_VIEWPORT (object);

   if (viewport->back_pixmap)
      gdk_pixmap_unref (viewport->back_pixmap);
   if (viewport->child)
      gtk_object_destroy (GTK_OBJECT (viewport->child));
   if (viewport->resize_idle_id)
      gtk_idle_remove (viewport->resize_idle_id);
   if (viewport->anchor)
      g_free (viewport->anchor);
   if (viewport->anchor_idle_id != -1)
      gtk_idle_remove (viewport->anchor_idle_id);

   (* GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}


/***********************
 *                     *
 *  GtkWidget methods  *
 *                     *
 ***********************/

/*
 * Standard Gtk+ function
 */
static void Dw_gtk_viewport_realize (GtkWidget *widget)
{
   GtkDwViewport *viewport;

   (* (GTK_WIDGET_CLASS(parent_class)->realize)) (widget);

   gdk_window_set_events (widget->window,
                          gdk_window_get_events (widget->window)
                          | GDK_BUTTON_PRESS_MASK
                          | GDK_BUTTON_RELEASE_MASK
                          | GDK_POINTER_MOTION_MASK
                          | GDK_ENTER_NOTIFY_MASK
                          | GDK_LEAVE_NOTIFY_MASK);

   viewport = GTK_DW_VIEWPORT (widget);
   if (viewport->child)
      a_Dw_widget_realize (viewport->child);
   gdk_window_get_geometry (widget->window, NULL, NULL, NULL, NULL,
                            &viewport->depth);
}


/*
 * Standard Gtk+ function
 */
static void Dw_gtk_viewport_unrealize (GtkWidget *widget)
{
   GtkDwViewport *viewport;

   (* (GTK_WIDGET_CLASS(parent_class)->unrealize)) (widget);

   viewport = GTK_DW_VIEWPORT (widget);
   if (viewport->child)
      a_Dw_widget_unrealize (viewport->child);
}


/*
 * (Nearly) standard Gtk+ function
 */
static void Dw_gtk_viewport_paint (GtkWidget *widget,
                                   GdkRectangle *area,
                                   GdkEventExpose *event)
{
   GtkLayout *layout;
   DwRectangle parent_area, child_area, intersection;
   GtkDwViewport *viewport;
   gboolean new_back_pixmap;

   if (GTK_WIDGET_DRAWABLE (widget)) {
      layout = GTK_LAYOUT (widget);
      viewport = GTK_DW_VIEWPORT (widget);

      DEBUG_MSG (2, "Drawing (%d, %d), %d x %d\n",
                 area->x, area->y, area->width, area->height);

      /* Make sure the backing pixmap is large enough. */
      if (viewport->child) {
         if (viewport->back_pixmap)
            new_back_pixmap =
               (widget->allocation.width > viewport->back_width ||
                widget->allocation.height > viewport->back_height);
         else
            new_back_pixmap = TRUE;

         if (new_back_pixmap) {
            if (viewport->back_pixmap)
               gdk_pixmap_ref (viewport->back_pixmap);
            viewport->back_pixmap = gdk_pixmap_new (widget->window,
                                                    widget->allocation.width,
                                                    widget->allocation.height,
                                                    viewport->depth);
            viewport->back_width = widget->allocation.width;
            viewport->back_height = widget->allocation.height;
            DEBUG_MSG (1, "   Creating new pixmap, size = %d x %d\n",
                       widget->allocation.width, widget->allocation.height);
         }

         /* Draw top-level Dw widget. */
         parent_area.x =
            Dw_widget_x_viewport_to_world (viewport->child, area->x);
         parent_area.y =
            Dw_widget_y_viewport_to_world (viewport->child, area->y);
         parent_area.width = area->width;
         parent_area.height = area->height;

         child_area.x = viewport->child->allocation.x;
         child_area.y = viewport->child->allocation.y;
         child_area.width = viewport->child->allocation.width;
         child_area.height = (viewport->child->allocation.ascent +
                              viewport->child->allocation.descent);

         if (Dw_rectangle_intersect (&parent_area, &child_area,
                                     &intersection)) {
            intersection.x -= viewport->child->allocation.x;
            intersection.y -= viewport->child->allocation.y;

            /* "Clear" backing pixmap. */
            gdk_draw_rectangle (viewport->back_pixmap,
                                viewport->child->style->background_color->gc,
                                TRUE, area->x, area->y,
                                area->width, area->height);
            /* Widgets draw in backing pixmap. */
            a_Dw_widget_draw (viewport->child, &intersection, event);
            /* Copy backing pixmap into window. */
            gdk_draw_pixmap (layout->bin_window, widget->style->black_gc,
                             viewport->back_pixmap, area->x, area->y,
                             area->x, area->y, area->width, area->height);
         }
      } else
         gdk_window_clear_area (layout->bin_window,
                                area->x, area->y, area->width, area->height);
   }
}


/*
 * Standard Gtk+ function
 */
static void Dw_gtk_viewport_draw (GtkWidget *widget,
                                  GdkRectangle *area)
{
   Dw_gtk_viewport_paint (widget, area, NULL);
   (* (GTK_WIDGET_CLASS(parent_class)->draw)) (widget, area);
}


/*
 * Standard Gtk+ function
 */
static gint Dw_gtk_viewport_expose (GtkWidget *widget,
                                    GdkEventExpose *event)
{
   Dw_gtk_viewport_paint (widget, &(event->area), event);
   return (* (GTK_WIDGET_CLASS(parent_class)->expose_event)) (widget, event);
}


/*
 * Handle the mouse event and deliver it to the Dw widget.
 * Most is done in a_Dw_widget_mouse_event.
 */
static gint Dw_gtk_viewport_mouse_event (GtkWidget *widget,
                                         gint32 x,
                                         gint32 y,
                                         GdkEvent *event)
{
   GtkDwViewport *viewport;
   DwWidget *dw_widget;
   gint32 world_x, world_y;

   if (event == NULL || event->any.window == widget->window) {
      viewport = GTK_DW_VIEWPORT (widget);
      if (viewport->child) {
         world_x = x + gtk_layout_get_hadjustment(GTK_LAYOUT(viewport))->value;
         world_y = y + gtk_layout_get_vadjustment(GTK_LAYOUT(viewport))->value;
         dw_widget =
            Dw_gtk_viewport_widget_at_point (viewport, world_x, world_y);

         return Dw_widget_mouse_event (dw_widget, widget,
                                       world_x, world_y, event);
      }
   }

   return FALSE;
}


/*
 * Standard Gtk+ function
 */
static gint Dw_gtk_viewport_button_press (GtkWidget *widget,
                                          GdkEventButton *event)
{
   return Dw_gtk_viewport_mouse_event (widget, event->x, event->y,
                                       (GdkEvent*) event);
}


/*
 * Standard Gtk+ function
 */
static gint Dw_gtk_viewport_button_release (GtkWidget *widget,
                                            GdkEventButton *event)
{
   return Dw_gtk_viewport_mouse_event (widget, event->x, event->y,
                                       (GdkEvent*) event);
}


/*
 * Standard Gtk+ function
 */
static gint Dw_gtk_viewport_motion_notify (GtkWidget *widget,
                                           GdkEventMotion *event)
{
   GtkDwViewport *viewport = GTK_DW_VIEWPORT (widget);

   viewport->mouse_x = event->x;
   viewport->mouse_y = event->y;
   return Dw_gtk_viewport_mouse_event (widget, event->x, event->y,
                                       (GdkEvent*) event);
}


/*
 * Standard Gtk+ function
 */
static gint Dw_gtk_viewport_enter_notify (GtkWidget *widget,
                                          GdkEventCrossing *event)
{
   return Dw_gtk_viewport_mouse_event (widget, event->x, event->y, NULL);
}


/*
 * Standard Gtk+ function
 */
static gint Dw_gtk_viewport_leave_notify (GtkWidget *widget,
                                          GdkEventCrossing *event)
{
   /* There will anyway be no Dw widget, thus this simple call */
   return Dw_widget_mouse_event (NULL, widget, 0, 0, NULL);
}


/*
 * This function is called when the viewport changes, and causes
 * motion_notify events to be simulated.
 */
static void Dw_gtk_viewport_adj_changed (GtkAdjustment *adj,
                                         GtkDwViewport *viewport)
{
   Dw_gtk_viewport_mouse_event (GTK_WIDGET (viewport),
                                viewport->mouse_x, viewport->mouse_y, NULL);
}

/**********************
 *                    *
 *  public functions  *
 *                    *
 **********************/

/*
 * Set the top-level Dw widget.
 * If there is already one, you must destroy it before, otherwise the
 * function will fail.
 */
void a_Dw_gtk_viewport_add_dw (GtkDwViewport *viewport,
                               DwWidget *widget)
{
   g_return_if_fail (viewport->child == NULL);

   viewport->child = widget;

   widget->parent = NULL;
   widget->viewport = GTK_WIDGET (viewport);
   /*widget->window = viewport->back_pixmap;*/

   if (GTK_WIDGET_REALIZED (viewport))
      a_Dw_widget_realize (widget);

   Dw_gtk_viewport_calc_size (viewport);
   Dw_gtk_viewport_remove_anchor (viewport);
}

/**************************************************
 *                                                *
 *  Functions used by GtkDwViewport and DwWidget  *
 *                                                *
 **************************************************/

/*
 * This function only *recognizes* that the top-level Dw widget is to
 * be removed. It is called by Dw_widget_shutdown.
 * Don't use this function directly!
 */
void Dw_gtk_viewport_remove_dw (GtkDwViewport *viewport)
{
   viewport->child = NULL;
   Dw_gtk_viewport_remove_anchor (viewport);
   Dw_gtk_viewport_calc_size (viewport);
}


/*
 * Used by Dw_gtk_viewport_calc_size.
 */
static void Dw_gtk_viewport_calc_child_size (GtkDwViewport *viewport,
                                             gint32 child_width,
                                             gint32 child_height,
                                             DwRequisition *child_requisition)
{
   if (child_width < 0) child_width = 0;
   if (child_height < 0) child_height = 0;

   DEBUG_MSG (2, "   width = %d, height = %d ...\n",
              child_width, child_height);

   a_Dw_widget_set_width (viewport->child, child_width);
   a_Dw_widget_set_ascent (viewport->child, child_height);
   a_Dw_widget_set_descent (viewport->child, 0);

   a_Dw_widget_size_request (viewport->child, child_requisition);
}


/*
 * Calculate the size of the scrolled area and allocate the top-level
 * widget. This function is called when the top-level Dw widget has
 * changed its size etc.
 */
void Dw_gtk_viewport_calc_size (GtkDwViewport *viewport)
{
   GtkWidget *widget;
   GtkScrolledWindow *scrolled;

   DwRequisition child_requisition;
   DwAllocation child_allocation;
   gint border_width,  space;

   GtkRequisition bar_requisition;
   gint max_width, max_height, bar_width_diff, bar_height_diff, child_height;

   if (viewport->calc_size_blocked)
      return;

   viewport->calc_size_blocked = TRUE;

   if (viewport->child) {
      /*
       * Determine the size hints for the Dw widget. This is a bit
       * tricky, because you must know if scrollbars are visible or
       * not, which depends on the size of the Dw widget, which then
       * depends on the hints. The idea is to test several
       * configurations, there are four of them, from combining the
       * cases horizontal/vertical scrollbar visible/invisible.
       *
       * For optimization, the horizontal scrollbar is currently not
       * regarded, the height hint is always the same, as if the
       * scrollbar was allways visible. In future, this may be
       * implemented correctly, by using the minimal width to optimize
       * most cases. (Minimal widths will also be used by tables.)
       *
       * Furthermore, the last result (vertical scrollbar visible or
       * not) is stored in the viewport, and tested first. This will
       * make a second test only necessary when the visibility
       * switches, which normally happens only once when filling the
       * page with text. (Actually, this assumes that the page size is
       * always *growing*, but this is nevertheless true in dillo.)
       */

      widget = GTK_WIDGET (viewport);
      scrolled = GTK_SCROLLED_WINDOW (widget->parent->parent);
      space = GTK_SCROLLED_WINDOW_CLASS(GTK_OBJECT(scrolled)->klass)
         ->scrollbar_spacing;
      border_width = GTK_CONTAINER(viewport)->border_width;

      gtk_widget_size_request (scrolled->vscrollbar, &bar_requisition);
      bar_width_diff = bar_requisition.width + space;
      max_width = widget->allocation.width - 2 * border_width;
      if (scrolled->vscrollbar_visible)
         max_width += bar_width_diff;

      gtk_widget_size_request (scrolled->hscrollbar, &bar_requisition);
      bar_height_diff = bar_requisition.height + space;
      max_height = widget->allocation.height - 2 * border_width;
      if (scrolled->hscrollbar_visible)
         max_height += bar_height_diff;

      DEBUG_MSG (2, "------------------------------------------------->\n");
      DEBUG_MSG (2, "Dw_gtk_viewport_calc_size: %d x %d (%c/%c) -> %d x %d\n",
                 widget->allocation.width, widget->allocation.height,
                 scrolled->vscrollbar_visible ? 't' : 'f',
                 scrolled->hscrollbar_visible ? 't' : 'f',
                 max_width, max_height);

      if (scrolled->vscrollbar_policy == GTK_POLICY_NEVER)
         child_height = max_height;
      else
         child_height = max_height - bar_height_diff;

      switch (scrolled->vscrollbar_policy) {
      case GTK_POLICY_ALWAYS:
         Dw_gtk_viewport_calc_child_size (viewport, max_width - bar_width_diff,
                                          child_height,
                                          &child_requisition);
         break;

      case GTK_POLICY_AUTOMATIC:
         if (viewport->vscrollbar_used) {
            DEBUG_MSG (2, "Testing with vertical scrollbar ...\n");
            Dw_gtk_viewport_calc_child_size (viewport,
                                             max_width - bar_width_diff,
                                             child_height,
                                             &child_requisition);

            if (child_requisition.ascent
                + child_requisition.descent <= child_height) {
               DEBUG_MSG (2, "   failed!\n");
               Dw_gtk_viewport_calc_child_size (viewport, max_width,
                                                child_height,
                                                &child_requisition);
               viewport->vscrollbar_used = TRUE;
            }

         } else {
            DEBUG_MSG (2, "Testing without vertical scrollbar ...\n");
            Dw_gtk_viewport_calc_child_size (viewport, max_width,
                                             child_height,
                                             &child_requisition);

            /* todo: see above */
            if (child_requisition.ascent
                + child_requisition.descent > child_height) {
               DEBUG_MSG (2, "   failed!\n");
               Dw_gtk_viewport_calc_child_size (viewport,
                                                max_width - bar_width_diff,
                                                child_height,
                                                &child_requisition);
               viewport->vscrollbar_used = TRUE;
            }
         }
         break;

      case GTK_POLICY_NEVER:
         Dw_gtk_viewport_calc_child_size (viewport, max_width,
                                          child_height,
                                          &child_requisition);
      }

      child_allocation.x = border_width;
      child_allocation.y = border_width;
      child_allocation.width = child_requisition.width;
      child_allocation.ascent = child_requisition.ascent;
      child_allocation.descent = child_requisition.descent;
      a_Dw_widget_size_allocate (viewport->child, &child_allocation);

      gtk_layout_set_size (GTK_LAYOUT (viewport),
                           child_requisition.width + 2 * border_width,
                           child_requisition.ascent + child_requisition.descent
                           + 2 * border_width);

      DEBUG_MSG (1, "Setting size to %d x %d\n",
                 child_requisition.width + 2 * border_width,
                 child_requisition.ascent + child_requisition.descent
                 + 2 * border_width);

      DEBUG_MSG (2, "<-------------------------------------------------\n");
   } else {
      gtk_layout_set_size (GTK_LAYOUT (viewport), 1, 1);
      viewport->hscrollbar_used = FALSE;
      viewport->vscrollbar_used = FALSE;
   }

   Dw_gtk_viewport_update_anchor (viewport);
   gtk_widget_queue_draw (GTK_WIDGET (viewport));

   viewport->calc_size_blocked = FALSE;
}


/* used by Dw_gtk_viewport_widget_at_point */
typedef struct
{
   gint32 x;
   gint32 y;
   DwWidget *widget;
} WidgetAtPointData;

/* used by Dw_gtk_viewport_widget_at_point */
static void Dw_gtk_viewport_widget_at_point_callback (DwWidget *widget,
                                                      gpointer data)
{
   WidgetAtPointData *callback_data;

   callback_data = (WidgetAtPointData*) data;

   if (callback_data->x >= widget->allocation.x &&
       callback_data->y >= widget->allocation.y &&
       callback_data->x < widget->allocation.x + widget->allocation.width &&
       callback_data->y < widget->allocation.y + (widget->allocation.ascent +
                                                  widget->allocation.descent))
      {
         if (DW_IS_CONTAINER (widget))
            a_Dw_container_forall (DW_CONTAINER (widget),
                                   Dw_gtk_viewport_widget_at_point_callback,
                                   data);

         if (callback_data->widget == NULL)
            callback_data->widget = widget;
      }
}

/*
 * Return the widget at point (x, y) (world coordinates).
 */
DwWidget* Dw_gtk_viewport_widget_at_point (GtkDwViewport *viewport,
                                           gint32 x,
                                           gint32 y)
{
   WidgetAtPointData callback_data;

   callback_data.x = x;
   callback_data.y = y;
   callback_data.widget = NULL;

   if (viewport->child)
      Dw_gtk_viewport_widget_at_point_callback (viewport->child,
                                                &callback_data);

   return callback_data.widget;
}


/*************
 *           *
 *  Anchors  *
 *           *
 *************/

/*
 * See Dw_gtk_viewport_scroll_to.
 */
static gint Dw_gtk_viewport_update_anchor_idle (gpointer data)
{
   GtkDwViewport *viewport;
   GtkAdjustment *adj;

   viewport = GTK_DW_VIEWPORT (data);
   adj = GTK_LAYOUT(viewport)->vadjustment;

   if (viewport->anchor_y > adj->upper - adj->page_size)
      gtk_adjustment_set_value (adj, adj->upper - adj->page_size);
   else
      gtk_adjustment_set_value (adj, viewport->anchor_y);

   viewport->anchor_idle_id = -1;
   return FALSE;
}


/*
 * Called by Dw_gtk_viewport_update_anchor.
 */
static void Dw_gtk_viewport_update_anchor_rec (DwWidget *widget)
{
   gpointer p;
   GtkDwViewport *viewport;

   viewport = GTK_DW_VIEWPORT (widget->viewport);

   if (widget->anchors_table &&
       (p = g_hash_table_lookup (widget->anchors_table, viewport->anchor)))
      Dw_gtk_viewport_scroll_to (viewport,
				 GPOINTER_TO_INT(p) + widget->allocation.y);
   else {
      if (DW_IS_CONTAINER (widget))
         a_Dw_container_forall (DW_CONTAINER (widget),
                                (DwCallback)Dw_gtk_viewport_update_anchor_rec,
                                NULL);
   }
}


/*
 * Called when possibly the scroll position has to be changed because
 * of anchors.
 */
void Dw_gtk_viewport_update_anchor (GtkDwViewport *viewport)
{
   if (viewport->anchor && viewport->child)
      Dw_gtk_viewport_update_anchor_rec (viewport->child);
}


/*
 * Sets the anchor to scroll to.
 */
void a_Dw_gtk_viewport_set_anchor (GtkDwViewport *viewport,
                                   const gchar *anchor)
{
   Dw_gtk_viewport_remove_anchor (viewport);

   if (anchor) {
      viewport->anchor = g_strdup(anchor);
      Dw_gtk_viewport_update_anchor (viewport);
   } else {
      viewport->anchor = NULL;
      gtk_adjustment_set_value (GTK_LAYOUT(viewport)->vadjustment, 0);
   }
}


/*
 * Sets the position to scroll to. The current anchor will be removed.
 */
void a_Dw_gtk_viewport_set_scrolling_position (GtkDwViewport *viewport,
                                               gint pos)
{
   Dw_gtk_viewport_remove_anchor (viewport);
   Dw_gtk_viewport_scroll_to (viewport, pos);
}

/*
 * Scrolls the viewport to position y.
 * This is done in an idle function.
 */
void Dw_gtk_viewport_scroll_to (GtkDwViewport *viewport,
                                gint32 y)
{
   viewport->anchor_y = y;
   if (viewport->anchor_idle_id == -1)
      viewport->anchor_idle_id = gtk_idle_add
         (Dw_gtk_viewport_update_anchor_idle, (gpointer)viewport);
}


/*
 * Remove anchor and idle function.
 */
void Dw_gtk_viewport_remove_anchor (GtkDwViewport *viewport)
{
   if (viewport->anchor) {
      g_free (viewport->anchor);
      viewport->anchor = NULL;
   }

   if (viewport->anchor_idle_id != -1) {
      gtk_idle_remove (viewport->anchor_idle_id);
      viewport->anchor_idle_id = -1;
   }
}
