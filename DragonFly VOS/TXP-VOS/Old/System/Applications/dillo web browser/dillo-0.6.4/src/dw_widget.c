/*
 * File: dw_widget.c
 *
 * Copyright (C) 2001 Sebastian Geerken <S.Geerken@ping.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <gtk/gtk.h>
#include "dw_widget.h"
#include "dw_container.h"
#include "dw_gtk_viewport.h"

static void Dw_widget_init                (DwWidget *widget);
static void Dw_widget_class_init          (DwWidgetClass *klass);

static void Dw_widget_shutdown            (GtkObject *object);
static void Dw_widget_destroy             (GtkObject *object);

static void Dw_widget_real_size_request      (DwWidget *widget,
                                              DwRequisition *requisition);
static void Dw_widget_real_size_allocate     (DwWidget *widget,
                                              DwAllocation *allocation);
static void Dw_widget_real_get_extremes      (DwWidget *widget,
                                              DwExtremes *extremes);
static void Dw_widget_real_set_width         (DwWidget *widget,
                                              gint32 width);
static void Dw_widget_real_set_ascent        (DwWidget *widget,
                                              gint32 ascent);
static void Dw_widget_real_set_descent       (DwWidget *widget,
                                              gint32 descent);
static void Dw_widget_real_draw              (DwWidget *widget,
                                              DwRectangle *area,
                                              GdkEventExpose *event);
static void Dw_widget_real_realize           (DwWidget *widget);
static void Dw_widget_real_unrealize         (DwWidget *widget);

static gint Dw_widget_real_button_press      (DwWidget *widget,
                                              gint32 x,
                                              gint32 y,
                                              GdkEventButton *event);
static gint Dw_widget_real_button_release    (DwWidget *widget,
                                              gint32 x,
                                              gint32 y,
                                              GdkEventButton *event);
static gint Dw_widget_real_motion_notify     (DwWidget *widget,
                                              gint32 x,
                                              gint32 y,
                                              GdkEventMotion *event);
static gint Dw_widget_real_enter_notify      (DwWidget *widget,
                                              GdkEventMotion *event);
static gint Dw_widget_real_leave_notify      (DwWidget *widget,
                                              GdkEventMotion *event);

static void Dw_widget_update_cursor          (DwWidget *widget);


enum
{
   SIZE_REQUEST,
   FAST_SIZE_REQUEST,
   SIZE_ALLOCATE,
   SET_WIDTH,
   SET_ASCENT,
   SET_DESCENT,
   DRAW,
   REALIZE,
   UNREALIZE,
   BUTTON_PRESS_EVENT,
   BUTTON_RELEASE_EVENT,
   MOTION_NOTIFY_EVENT,
   ENTER_NOTIFY_EVENT,
   LEAVE_NOTIFY_EVENT,
   LAST_SIGNAL
};


static GtkObjectClass *parent_class;
static guint widget_signals[LAST_SIGNAL] = { 0 };

/*
 * Standard Gtk+ function
 */
GtkType a_Dw_widget_get_type (void)
{
   static GtkType type = 0;

   if (!type) {
      GtkTypeInfo info = {
         "DwWidget",
         sizeof (DwWidget),
         sizeof (DwWidgetClass),
         (GtkClassInitFunc) Dw_widget_class_init,
         (GtkObjectInitFunc) Dw_widget_init,
         (GtkArgSetFunc) NULL,
         (GtkArgGetFunc) NULL,
         (GtkClassInitFunc) NULL
      };

      type = gtk_type_unique (GTK_TYPE_OBJECT, &info);
   }

   return type;
}


/*
 * Standard Gtk+ function
 */
static void Dw_widget_init (DwWidget *widget)
{
   widget->flags = DW_NEEDS_RESIZE | DW_EXTREMES_CHANGED | DW_HAS_CONTENT;
   widget->parent = NULL;
   widget->viewport = NULL;

   widget->allocation.x = -1;
   widget->allocation.y = -1;
   widget->allocation.width = 1;
   widget->allocation.ascent = 1;
   widget->allocation.descent = 0;

   /* todo: comment */
   widget->anchors_table = NULL;
   widget->cursor = NULL;
   widget->style = NULL;
}


/*
 * Standard Gtk+ function
 */
static void Dw_widget_class_init (DwWidgetClass *klass)
{
   GtkObjectClass *object_class;

   parent_class = gtk_type_class (gtk_object_get_type ());

   object_class = GTK_OBJECT_CLASS (klass);

   widget_signals[SIZE_REQUEST] =
      gtk_signal_new ("size_request",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwWidgetClass, size_request),
                      gtk_marshal_NONE__POINTER,
                      GTK_TYPE_NONE,
                      1, GTK_TYPE_POINTER);
   widget_signals[SIZE_ALLOCATE] =
      gtk_signal_new ("size_allocate",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwWidgetClass, size_allocate),
                      gtk_marshal_NONE__POINTER,
                      GTK_TYPE_NONE,
                      1, GTK_TYPE_POINTER);
   widget_signals[SET_WIDTH] =
      gtk_signal_new ("set_width",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwWidgetClass, set_width),
                      gtk_marshal_NONE__POINTER,
                      GTK_TYPE_NONE,
                      1, GTK_TYPE_UINT);
   widget_signals[SET_ASCENT] =
      gtk_signal_new ("set_ascent",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwWidgetClass, set_ascent),
                      gtk_marshal_NONE__POINTER,
                      GTK_TYPE_NONE,
                      1, GTK_TYPE_UINT);
   widget_signals[SET_DESCENT] =
      gtk_signal_new ("set_descent",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwWidgetClass, set_descent),
                      gtk_marshal_NONE__POINTER,
                      GTK_TYPE_NONE,
                      1, GTK_TYPE_UINT);
   widget_signals[DRAW] =
      gtk_signal_new ("draw",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwWidgetClass, draw),
                      gtk_marshal_NONE__POINTER,
                      GTK_TYPE_NONE,
                      2, GTK_TYPE_POINTER, GTK_TYPE_GDK_EVENT);
   widget_signals[REALIZE] =
      gtk_signal_new ("realize",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwWidgetClass, realize),
                      gtk_marshal_NONE__NONE,
                      GTK_TYPE_NONE, 0);
   widget_signals[UNREALIZE] =
      gtk_signal_new ("unrealize",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwWidgetClass, unrealize),
                      gtk_marshal_NONE__NONE,
                      GTK_TYPE_NONE, 0);
  widget_signals[BUTTON_PRESS_EVENT] =
     gtk_signal_new ("button_press_event",
                     GTK_RUN_LAST,
                     object_class->type,
                     GTK_SIGNAL_OFFSET (DwWidgetClass, button_press_event),
                     gtk_marshal_BOOL__POINTER,
                     GTK_TYPE_BOOL,
                     3, GTK_TYPE_UINT, GTK_TYPE_UINT, GTK_TYPE_GDK_EVENT);
  widget_signals[BUTTON_RELEASE_EVENT] =
     gtk_signal_new ("button_release_event",
                     GTK_RUN_LAST,
                     object_class->type,
                     GTK_SIGNAL_OFFSET (DwWidgetClass, button_release_event),
                     gtk_marshal_BOOL__POINTER,
                     GTK_TYPE_BOOL,
                     3, GTK_TYPE_UINT, GTK_TYPE_UINT, GTK_TYPE_GDK_EVENT);
  widget_signals[MOTION_NOTIFY_EVENT] =
     gtk_signal_new ("motion_notify_event",
                     GTK_RUN_LAST,
                     object_class->type,
                     GTK_SIGNAL_OFFSET (DwWidgetClass, motion_notify_event),
                     gtk_marshal_BOOL__POINTER,
                     GTK_TYPE_BOOL,
                     3, GTK_TYPE_UINT, GTK_TYPE_UINT, GTK_TYPE_GDK_EVENT);
  widget_signals[ENTER_NOTIFY_EVENT] =
     gtk_signal_new ("enter_notify_event",
                     GTK_RUN_LAST,
                     object_class->type,
                     GTK_SIGNAL_OFFSET (DwWidgetClass, enter_notify_event),
                     gtk_marshal_BOOL__POINTER,
                     GTK_TYPE_BOOL,
                     3, GTK_TYPE_UINT, GTK_TYPE_UINT, GTK_TYPE_GDK_EVENT);
  widget_signals[LEAVE_NOTIFY_EVENT] =
     gtk_signal_new ("leave_notify_event",
                     GTK_RUN_LAST,
                     object_class->type,
                     GTK_SIGNAL_OFFSET (DwWidgetClass, leave_notify_event),
                     gtk_marshal_BOOL__POINTER,
                     GTK_TYPE_BOOL,
                     3, GTK_TYPE_UINT, GTK_TYPE_UINT, GTK_TYPE_GDK_EVENT);

   object_class->shutdown = Dw_widget_shutdown;
   object_class->destroy = Dw_widget_destroy;

   klass->size_request = Dw_widget_real_size_request;
   klass->get_extremes = Dw_widget_real_get_extremes;
   klass->size_allocate = Dw_widget_real_size_allocate;
   klass->mark_size_change = NULL;
   klass->mark_extremes_change = NULL;
   klass->set_width = Dw_widget_real_set_width;
   klass->set_ascent = Dw_widget_real_set_ascent;
   klass->set_descent = Dw_widget_real_set_descent;
   klass->draw = Dw_widget_real_draw;
   klass->realize = Dw_widget_real_realize;
   klass->unrealize = Dw_widget_real_unrealize;
   klass->button_press_event = Dw_widget_real_button_press;
   klass->button_release_event = Dw_widget_real_button_release;
   klass->motion_notify_event = Dw_widget_real_motion_notify;
   klass->enter_notify_event = Dw_widget_real_enter_notify;
   klass->leave_notify_event = Dw_widget_real_leave_notify;
}



/*
 * Standard Gtk+ function
 */
static void Dw_widget_shutdown (GtkObject *object)
{
   DwWidget *widget;

   widget = DW_WIDGET (object);

   a_Dw_widget_unrealize (widget);

   if (widget->parent)
      Dw_container_remove (DW_CONTAINER (widget->parent), widget);
   else
      Dw_gtk_viewport_remove_dw (GTK_DW_VIEWPORT (widget->viewport));

   parent_class->shutdown (object);
}

/*
 * Standard Gtk+ function
 */
static void Dw_widget_destroy (GtkObject *object)
{
   DwWidget *widget;

   widget = DW_WIDGET (object);
   if (widget->anchors_table) {
      g_hash_table_destroy(widget->anchors_table);
   }

   /* The widget the pointer is in? */
   if (widget->viewport != NULL &&
       widget == GTK_DW_VIEWPORT(widget->viewport)->last_entered)
      /* todo: perhaps call the leave_notify function? */
      GTK_DW_VIEWPORT(widget->viewport)->last_entered = NULL;

   if (widget->style)
      a_Dw_style_unref (widget->style);

   parent_class->destroy (object);
}

/*
 * Standard Dw function
 */
static void Dw_widget_real_size_request (DwWidget *widget,
                                         DwRequisition *requisition)
{
   g_warning ("DwWidget::size_request not implemented for `%s'",
              gtk_type_name (GTK_OBJECT_TYPE (widget)));

   /* return random size to prevent crashes*/
   requisition->width = 50;
   requisition->ascent = 50;
   requisition->descent = 50;
}


/*
 * Standard Dw function
 */
static void Dw_widget_real_get_extremes (DwWidget *widget,
                                         DwExtremes *extremes)
{
   /* Simply return the requisition width */
   DwRequisition requisition;

   a_Dw_widget_size_request (widget, &requisition);
   extremes->min_width = extremes->max_width = requisition.width;
}


/*
 * Standard Dw function
 */
static void Dw_widget_real_size_allocate  (DwWidget *widget,
                                           DwAllocation *allocation)
{
   /* ignored */
}


/*
 * Standard Dw function
 */
static void Dw_widget_real_set_width       (DwWidget *widget,
                                            gint32 width)
{
   /* ignored */
}


/*
 * Standard Dw function
 */
static void Dw_widget_real_set_ascent (DwWidget *widget,
                                       gint32 ascent)
{
   /* ignored */
}


/*
 * Standard Dw function
 */
static void Dw_widget_real_set_descent (DwWidget *widget,
                                        gint32 descent)
{
   /* ignored */
}


/*
 * Standard Dw function
 */
static void Dw_widget_real_draw    (DwWidget *widget,
                                    DwRectangle *area,
                                    GdkEventExpose *event)
{
   /* ignored */
}


/*
 * Standard Dw function
 */
static void Dw_widget_real_realize (DwWidget *widget)
{
   /* ignored */
}


/*
 * Standard Dw function
 */
static void Dw_widget_real_unrealize (DwWidget *widget)
{
   /* ignored */
}



/*
 * Standard Dw function
 */
static gint Dw_widget_real_button_press (DwWidget *widget,
                                         gint32 x,
                                         gint32 y,
                                         GdkEventButton *event)
{
   /* not handled */
   return FALSE;
}


/*
 * Standard Dw function
 */
static gint Dw_widget_real_button_release (DwWidget *widget,
                                           gint32 x,
                                           gint32 y,
                                           GdkEventButton *event)
{
   /* not handled */
   return FALSE;
}


/*
 * Standard Dw function
 */
static gint Dw_widget_real_motion_notify (DwWidget *widget,
                                          gint32 x,
                                          gint32 y,
                                          GdkEventMotion *event)
{
   /* not handled */
   return FALSE;
}


/*
 * Standard Dw function
 */
static gint Dw_widget_real_enter_notify (DwWidget *widget,
                                         GdkEventMotion *event)
{
   /* not handled */
   return FALSE;
}


/*
 * Standard Dw function
 */
static gint Dw_widget_real_leave_notify (DwWidget *widget,
                                         GdkEventMotion *event)
{
   /* not handled */
   return FALSE;
}



/*
 * This function is a wrapper for DwWidget::size_request; it calls
 * this method only when needed.
 */
void a_Dw_widget_size_request (DwWidget *widget,
                               DwRequisition *requisition)
{
   if (DW_WIDGET_NEEDS_RESIZE (widget)) {
      /* todo: check requisition == &(widget->requisition) and do what? */
      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[SIZE_REQUEST],
                       requisition);
      widget->requisition = *requisition;
      DW_WIDGET_UNSET_FLAGS (widget, DW_NEEDS_RESIZE);
   } else
      *requisition = widget->requisition;
}

/*
 * Wrapper for DwWidget::get_extremes.
 */
void a_Dw_widget_get_extremes (DwWidget *widget,
                               DwExtremes *extremes)
{
   DwWidgetClass *klass;

   if (DW_WIDGET_EXTREMES_CHANGED (widget)) {
      klass =  DW_WIDGET_CLASS (GTK_OBJECT(widget)->klass);
      (* (klass->get_extremes)) (widget, extremes);
      widget->extremes = *extremes;
      DW_WIDGET_UNSET_FLAGS (widget, DW_EXTREMES_CHANGED);
   } else
      *extremes = widget->extremes;
}


/*
 * Wrapper for DwWidget::size_allocate, only called when needed.
 */
void a_Dw_widget_size_allocate  (DwWidget *widget,
                                 DwAllocation *allocation)
{
   if (DW_WIDGET_NEEDS_ALLOCATE (widget) ||
       allocation->x != widget->allocation.x ||
       allocation->y != widget->allocation.y ||
       allocation->width != widget->allocation.width ||
       allocation->ascent != widget->allocation.ascent ||
       allocation->descent != widget->allocation.descent) {

      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[SIZE_ALLOCATE],
                       allocation);
      widget->allocation = *allocation;
   }

   /*DW_WIDGET_UNSET_FLAGS (widget, DW_NEEDS_RESIZE);*/
}


void a_Dw_widget_set_width (DwWidget *widget,
                            gint32 width)
{
   gtk_signal_emit (GTK_OBJECT (widget), widget_signals[SET_WIDTH], width);
}


void a_Dw_widget_set_ascent (DwWidget *widget,
                             gint32 ascent)
{
   gtk_signal_emit (GTK_OBJECT (widget), widget_signals[SET_ASCENT], ascent);
}


void a_Dw_widget_set_descent (DwWidget *widget,
                              gint32 descent)
{
   gtk_signal_emit (GTK_OBJECT (widget), widget_signals[SET_DESCENT], descent);
}


void a_Dw_widget_draw (DwWidget *widget,
                       DwRectangle *area,
                       GdkEventExpose *event)
{
   gtk_signal_emit (GTK_OBJECT (widget), widget_signals[DRAW],
                    area, event);
}


void a_Dw_widget_realize (DwWidget *widget)
{
   if (!DW_WIDGET_REALIZED (widget)) {
      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[REALIZE]);
      DW_WIDGET_SET_FLAGS (widget, DW_REALIZED);

      if (DW_IS_CONTAINER (widget))
         a_Dw_container_forall (DW_CONTAINER (widget),
                                (DwCallback) a_Dw_widget_realize,
                                NULL);

      Dw_widget_update_cursor (widget);

      if (widget->parent == NULL && widget->style->background_color != NULL)
         gdk_window_set_background(GTK_LAYOUT(widget->viewport)->bin_window,
                                   &widget->style->background_color->color);
   }
}


void a_Dw_widget_unrealize (DwWidget *widget)
{
   if (DW_WIDGET_REALIZED (widget)) {
      a_Dw_widget_set_cursor (widget, NULL);
      gtk_signal_emit (GTK_OBJECT (widget), widget_signals[UNREALIZE]);
      DW_WIDGET_UNSET_FLAGS (widget, DW_REALIZED);

      if (DW_IS_CONTAINER (widget))
         a_Dw_container_forall (DW_CONTAINER (widget),
                                (DwCallback) a_Dw_widget_unrealize,
                                NULL);
   }
}


/*
 * Handles a mouse event.
 *
 * This function is called by Dw_gtk_viewport_mouse_event, the type of
 * the event is determined by event->type. x and y are world coordinates.
 * widget may be NULL (if the pointer is outside the top-level widget).
 *
 * When event is NULL, GDK_MOTION_NOTIFY is used as the type. This will
 * soon be the case when GDK_MOTION_NOTIFY events are simulated as a
 * result of viewport changes (bug #94)
 */
gint Dw_widget_mouse_event (DwWidget *widget,
                            GtkWidget *viewwidget,
                            gint32 x,
                            gint32 y,
                            GdkEvent *event)
{
   /* todo: implement as signals */
   gint (*function)();
   DwWidgetClass *klass;
   GtkDwViewport *viewport = GTK_DW_VIEWPORT (viewwidget);
   GdkEventType event_type;
   DwWidget *ancestor, *w1, *w2;

   /* simulate crossing events */
   /* todo: resizing/moving widgets */
   if (widget != viewport->last_entered) {
      /* Determine the next common ancestor of the widgets. */
      if (viewport->last_entered == NULL || widget == NULL)
         ancestor = NULL;
      else {
         /* There is probably a faster algorithm. ;-) */
         ancestor = NULL;
         for (w1 = viewport->last_entered; ancestor != NULL && w1 != NULL;
              w1 = w1->parent)
            for (w2 = widget; ancestor != NULL && w2 != NULL; w2 = w2->parent)
               if (w1 == w2)
                  ancestor = w1;
      }

      /* leave_notify_event is called for all widgets down to the next
         common ancestor. */
      for (w1 = viewport->last_entered; w1 != ancestor; w1 = w1->parent) {
         klass = DW_WIDGET_CLASS (GTK_OBJECT(w1)->klass);
         klass->leave_notify_event (w1, (GdkEventMotion*) event);
      }

      viewport->last_entered = widget;

      /* todo: enter_notify_event is much simpler, but the mechanism used
         for leave_notify_event is (currently) not needed in this case. */
      if (widget) {
         klass = DW_WIDGET_CLASS (GTK_OBJECT (widget)->klass);
         (* (klass->enter_notify_event)) (widget, (GdkEventMotion*) event);
         Dw_widget_update_cursor (widget);
      } else
         gdk_window_set_cursor (GTK_LAYOUT(viewport)->bin_window, NULL);
   }

   /* other events */
   event_type = event ? event->type : GDK_MOTION_NOTIFY;

   while (widget) {
      klass = DW_WIDGET_CLASS (GTK_OBJECT (widget)->klass);

      switch (event_type) {
      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
         function = klass->button_press_event;
         break;

      case GDK_BUTTON_RELEASE:
         function = klass->button_release_event;
         break;

      case GDK_MOTION_NOTIFY:
         function = klass->motion_notify_event;
         break;

      default:
         function = NULL;
         break;
      }

      if (function != NULL && (*function) (widget,
                                           x - widget->allocation.x,
                                           y - widget->allocation.y,
                                           event))
         return TRUE;

      widget = widget->parent;
   }

   return FALSE;
}


/*
 *  Change the style of a widget. The old style is automatically
 *  unreferred, the new is referred. If this call causes the widget to
 *  change its size, Dw_widget_queue_resize is called. (todo: the
 *  latter is not implemented yet)
 */
void a_Dw_widget_set_style (DwWidget *widget,
                            DwStyle *style)
{
   if (widget->style)
      a_Dw_style_unref (widget->style);

   a_Dw_style_ref (style);
   widget->style = style;

   if (widget->parent == NULL &&
       DW_WIDGET_REALIZED (widget) &&
       widget->style->background_color != NULL)
      gdk_window_set_background(GTK_LAYOUT(widget->viewport)->bin_window,
                                &widget->style->background_color->color);
}


/*
 * Set the cursor of the viewport.
 * Called from several other functions.
 */
static void Dw_widget_update_cursor (DwWidget *widget)
{
   GtkDwViewport *viewport = GTK_DW_VIEWPORT (widget->viewport);
   DwWidget *cursor_widget;

   if (GTK_WIDGET_REALIZED (viewport)) {
      /* Search cursor to use, going up from last_entered (not from widget!).
       */
      cursor_widget = viewport->last_entered;
      while (cursor_widget && cursor_widget->cursor == NULL)
         cursor_widget = cursor_widget->parent;

      if (cursor_widget)
         gdk_window_set_cursor (GTK_LAYOUT(viewport)->bin_window,
                                cursor_widget->cursor);
      else
         gdk_window_set_cursor (GTK_LAYOUT(viewport)->bin_window,
                                NULL);
   }
}

/*
 * Set the cursor for a DwWidget. cursor has to be stored elsewhere, it
 * is not copied (and not destroyed). If cursor is NULL, the cursor of
 * the parent widget is used.
 */
void a_Dw_widget_set_cursor (DwWidget *widget,
                             GdkCursor *cursor)
{
   widget->cursor = cursor;
   if (DW_WIDGET_REALIZED (widget))
      Dw_widget_update_cursor (widget);
}


/*
 * ...
 */
DwWidget *a_Dw_widget_get_toplevel (DwWidget *widget)
{
   while (widget->parent)
      widget = widget->parent;

   return widget;
}

/*
 * Scroll viewport to pos (vertical widget coordinate).
 */
void a_Dw_widget_scroll_to (DwWidget *widget,
                            int pos)
{
   Dw_gtk_viewport_scroll_to (GTK_DW_VIEWPORT (widget->viewport),
                              pos + widget->allocation.y);
}


/*
 * ...
 * The function has been copied from gdktrectangle.c
 */
gint Dw_rectangle_intersect (DwRectangle *src1,
                             DwRectangle *src2,
                             DwRectangle *dest)
{
   DwRectangle *temp;
   gint src1_x2, src1_y2;
   gint src2_x2, src2_y2;
   gint return_val;

   g_return_val_if_fail (src1 != NULL, FALSE);
   g_return_val_if_fail (src2 != NULL, FALSE);
   g_return_val_if_fail (dest != NULL, FALSE);

   return_val = FALSE;

   if (src2->x < src1->x) {
      temp = src1;
      src1 = src2;
      src2 = temp;
   }
   dest->x = src2->x;

   src1_x2 = src1->x + src1->width;
   src2_x2 = src2->x + src2->width;

   if (src2->x < src1_x2) {
      if (src1_x2 < src2_x2)
         dest->width = src1_x2 - dest->x;
      else
         dest->width = src2_x2 - dest->x;

      if (src2->y < src1->y) {
         temp = src1;
         src1 = src2;
         src2 = temp;
      }
      dest->y = src2->y;

      src1_y2 = src1->y + src1->height;
      src2_y2 = src2->y + src2->height;

      if (src2->y < src1_y2) {
         return_val = TRUE;

         if (src1_y2 < src2_y2)
            dest->height = src1_y2 - dest->y;
         else
            dest->height = src2_y2 - dest->y;

         if (dest->height == 0)
            return_val = FALSE;
         if (dest->width == 0)
            return_val = FALSE;
      }
   }

   return return_val;
}


/*
 * Calculates the intersection of widget->allocation and area, returned in
 * intersection (in widget coordinates!). Typically used by containers when
 * drawing their children. Returns whether intersection is not empty.
 */
gint Dw_widget_intersect (DwWidget *widget,
                          DwRectangle *area,
                          DwRectangle *intersection)
{
#if 1
   DwRectangle parent_area, child_area;

   parent_area = *area;
   parent_area.x += widget->parent->allocation.x;
   parent_area.y += widget->parent->allocation.y;

   child_area.x = widget->allocation.x;
   child_area.y = widget->allocation.y;
   child_area.width = widget->allocation.width;
   child_area.height = widget->allocation.ascent + widget->allocation.descent;

   if (Dw_rectangle_intersect (&parent_area, &child_area, intersection)) {
      intersection->x -= widget->allocation.x;
      intersection->y -= widget->allocation.y;
      return TRUE;
   } else
      return FALSE;
#else
   intersection->x = 0;
   intersection->y = 0;
   intersection->width = widget->allocation.width;
   intersection->height = (widget->allocation.ascent +
                           widget->allocation.descent);

   return TRUE;
#endif
}


void Dw_widget_set_parent (DwWidget *widget,
                           DwWidget *parent)
{
   gtk_object_ref(GTK_OBJECT (widget));
   gtk_object_sink(GTK_OBJECT (widget));
   widget->parent = parent;
   widget->viewport = parent->viewport;
   /*widget->window = parent->window;*/

   if (DW_WIDGET_REALIZED (parent))
      a_Dw_widget_realize (widget);
}


/*
 * Converting between coordinates.
 */

gint32 Dw_widget_x_viewport_to_world (DwWidget *widget,
                                      gint16 viewport_x)
{
   GtkAdjustment *adjustment;

   g_return_val_if_fail (widget && widget->viewport, 0);
   adjustment = gtk_layout_get_hadjustment (GTK_LAYOUT (widget->viewport));
   g_return_val_if_fail (adjustment != NULL, 0);

   return viewport_x + (gint32)adjustment->value;
}


gint32  Dw_widget_y_viewport_to_world (DwWidget *widget,
                                       gint16 viewport_y)
{
   GtkAdjustment *adjustment;

   g_return_val_if_fail (widget && widget->viewport, 0);
   adjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (widget->viewport));
   g_return_val_if_fail (adjustment != NULL, 0);

   return viewport_y + (gint32)adjustment->value;
}


gint16  Dw_widget_x_world_to_viewport (DwWidget *widget,
                                       gint32 world_x)
{
   GtkAdjustment *adjustment;

   g_return_val_if_fail (widget && widget->viewport, 0);
   adjustment = gtk_layout_get_hadjustment (GTK_LAYOUT (widget->viewport));
   g_return_val_if_fail (adjustment != NULL, 0);

   return world_x - (gint32)adjustment->value;
}


gint16  Dw_widget_y_world_to_viewport (DwWidget *widget,
                                       gint32 world_y)
{
   GtkAdjustment *adjustment;

   g_return_val_if_fail (widget && widget->viewport, 0);
   adjustment = gtk_layout_get_vadjustment (GTK_LAYOUT (widget->viewport));
   g_return_val_if_fail (adjustment != NULL, 0);

   return world_y - (gint32)adjustment->value;
}


/*
 * Calculate the intersection of (x, y, width, height) (widget
 * coordinates) and the current viewport area. gdk_intersection has
 * (of course) viewport coordinates, the return value is TRUE iff the
 * intersection is not empty.
 */
static gboolean Dw_widget_intersect_viewport (DwWidget *widget,
                                              gint32 x,
                                              gint32 y,
                                              gint32 width,
                                              gint32 height,
                                              GdkRectangle *gdk_intersection)
{
   GtkLayout *layout;
   DwRectangle widget_area, viewport_area, intersection;

   g_return_val_if_fail (widget && widget->viewport, FALSE);

   layout = GTK_LAYOUT (widget->viewport);

   widget_area.x = widget->allocation.x + x;
   widget_area.y = widget->allocation.y + y;
   widget_area.width = width;
   widget_area.height = height;

   viewport_area.x = layout->xoffset;
   viewport_area.y = layout->yoffset;
   viewport_area.width = widget->viewport->allocation.width;
   viewport_area.height = widget->viewport->allocation.height;

   if (Dw_rectangle_intersect (&widget_area, &viewport_area, &intersection)) {
      gdk_intersection->x = intersection.x - layout->xoffset;
      gdk_intersection->y = intersection.y - layout->yoffset;
      gdk_intersection->width = intersection.width;
      gdk_intersection->height = intersection.height;
      return TRUE;
   } else
      return FALSE;
}


/*
 * ...
 */
void Dw_widget_queue_draw (DwWidget *widget)
{
   Dw_widget_queue_draw_area (
      widget,
      0, 0,
      widget->allocation.width,
      widget->allocation.ascent + widget->allocation.descent);
}


/*
 * ...
 */
void Dw_widget_queue_draw_area (DwWidget *widget,
                                gint32 x,
                                gint32 y,
                                gint32 width,
                                gint32 height)
{
   GdkRectangle gdk_area;

   if (Dw_widget_intersect_viewport (widget, x, y, width, height, &gdk_area))
      gtk_widget_queue_draw_area (widget->viewport, gdk_area.x, gdk_area.y,
                                  gdk_area.width, gdk_area.height);
}


/*
 * ...
 */
void Dw_widget_queue_clear (DwWidget *widget)
{
   Dw_widget_queue_clear_area (
      widget,
      0, 0,
      widget->allocation.width,
      widget->allocation.ascent + widget->allocation.descent);
}


/*
 * ...
 */
void Dw_widget_queue_clear_area (DwWidget *widget,
                                 gint32 x,
                                 gint32 y,
                                 gint32 width,
                                 gint32 height)
{
   GdkRectangle gdk_area;

   if (Dw_widget_intersect_viewport (widget, x, y, width, height, &gdk_area))
      gtk_widget_queue_clear_area (widget->viewport, gdk_area.x, gdk_area.y,
                                   gdk_area.width, gdk_area.height);
}


/*
 * Resizing of Widgets.
 * The interface was adopted by Gtk+, but the implementation is far simpler,
 * since Gtk+ handles a lot of cases which are irrelevant to Dw.
 */

/*
 * Used by Dw_widget_queue_resize.
 */
static int Dw_widget_queue_resize_idle (gpointer data)
{
   GtkDwViewport *viewport = GTK_DW_VIEWPORT (data);

   Dw_gtk_viewport_calc_size (viewport);
   viewport->resize_idle_id = -1;
   return FALSE;
}


/*
 * This function should be called, if the widget changed its size.
 */
void Dw_widget_queue_resize (DwWidget *widget,
                             gint ref,
                             gboolean extremes_changed)
{
   DwWidget *widget2, *child;
   GtkDwViewport *viewport;
   DwWidgetClass *klass;

   klass =  (DwWidgetClass*)(((GtkObject*)widget)->klass);
   DW_WIDGET_SET_FLAGS (widget, DW_NEEDS_RESIZE);
   if (klass->mark_size_change)
      klass->mark_size_change (widget, ref);

   if (extremes_changed) {
      DW_WIDGET_SET_FLAGS (widget, DW_EXTREMES_CHANGED);
      if (klass->mark_extremes_change)
         klass->mark_extremes_change (widget, ref);
   }

   for (widget2 = widget->parent, child = widget;
        widget2;
        child = widget2, widget2 = widget2->parent) {
      klass =  (DwWidgetClass*)(((GtkObject*)widget2)->klass);
      DW_WIDGET_SET_FLAGS (widget2, DW_NEEDS_RESIZE);
      if (klass->mark_size_change)
         klass->mark_size_change (widget2, child->parent_ref);
      DW_WIDGET_SET_FLAGS (widget2, DW_NEEDS_ALLOCATE);

      if (extremes_changed) {
         DW_WIDGET_SET_FLAGS (widget2, DW_EXTREMES_CHANGED);
         if (klass->mark_extremes_change)
            klass->mark_extremes_change (widget2, child->parent_ref);
      }
   }

   if (widget->viewport) {
      viewport = GTK_DW_VIEWPORT (widget->viewport);
      if (viewport->resize_idle_id == -1)
         viewport->resize_idle_id =
            gtk_idle_add (Dw_widget_queue_resize_idle, viewport);
   }
}


/*
 * Add or change an anchor.
 * The widget is responsible for storing a copy of name.
 */
void Dw_widget_set_anchor (DwWidget *widget, gchar *name, int pos)
{
   if (widget->anchors_table == NULL)
      widget->anchors_table = g_hash_table_new(g_str_hash, g_str_equal);

   g_hash_table_insert(widget->anchors_table, name, GINT_TO_POINTER(pos));
   Dw_gtk_viewport_update_anchor(GTK_DW_VIEWPORT (widget->viewport));
}



/*
 * Draw borders and background of a widget part, which allocation is
 * given by (x, y, width, height) (widget coordinates).
 */
void Dw_widget_draw_box (DwWidget *widget,
                         DwStyle *style,
                         DwRectangle *area,
                         gint32 x,
                         gint32 y,
                         gint32 width,
                         gint32 height)
{
   GdkRectangle gdk_area;
   gint32 vx, vy;

   if (Dw_widget_intersect_viewport (widget, area->x, area->y,
                                     area->width, area->height, &gdk_area)) {
      vx = Dw_widget_x_viewport_to_world (widget, 0);
      vy = Dw_widget_y_viewport_to_world (widget, 0);

      Dw_style_draw_border (DW_WIDGET_WINDOW (widget), &gdk_area,
                            vx, vy,
                            widget->allocation.x + x,
                            widget->allocation.y + y,
                            width, height,
                            style);

      if (style->background_color)
         Dw_style_draw_background (DW_WIDGET_WINDOW (widget), &gdk_area,
                                   vx, vy,
                                   widget->allocation.x + x,
                                   widget->allocation.y + y,
                                   width, height,
                                   style);
   }
}


/*
 * Draw borders and background of a widget.
 */
void Dw_widget_draw_widget_box (DwWidget *widget,
                                DwRectangle *area)
{
   GdkRectangle gdk_area;
   gint32 vx, vy;

   if (Dw_widget_intersect_viewport (widget, area->x, area->y,
                                     area->width, area->height, &gdk_area)) {
      vx = Dw_widget_x_viewport_to_world (widget, 0);
      vy = Dw_widget_y_viewport_to_world (widget, 0);

      Dw_style_draw_border (DW_WIDGET_WINDOW (widget), &gdk_area,
                            vx, vy,
                            widget->allocation.x,
                            widget->allocation.y,
                            widget->allocation.width,
                            widget->allocation.ascent
                            + widget->allocation.descent,
                            widget->style);

      /* - Toplevel widget background colors are set as viewport
       *   background color. This is not crucial for the rendering, but
       *   looks a bit nicer when scrolling. Furthermore, the viewport
       *   does anything else in this case.
       *
       * - Since widgets are always drawn from top to bottom, it is
       *   *not* necessary to draw the background if
       *   widget->style->background_color is NULL (shining through).
       */
      if (widget->parent && widget->style->background_color)
         Dw_style_draw_background (DW_WIDGET_WINDOW (widget), &gdk_area,
                                   vx, vy,
                                   widget->allocation.x,
                                   widget->allocation.y,
                                   widget->allocation.width,
                                   widget->allocation.ascent
                                   + widget->allocation.descent,
                                   widget->style);
   }
}
