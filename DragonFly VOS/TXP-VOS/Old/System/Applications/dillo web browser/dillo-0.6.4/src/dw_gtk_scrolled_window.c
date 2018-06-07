/*
 * File: dw_gtk_scrolled_window.c
 *
 * Copyright (C) 2001  Sebastian Geerken <S.Geerken@ping.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "dw_gtk_scrolled_window.h"
#include "dw_gtk_scrolled_frame.h"
#include "dw_gtk_viewport.h"
#include "dw_page.h"
#include <gtk/gtk.h>

#include "debug.h"

static GtkScrolledWindowClass *parent_class = NULL;

static void Dw_gtk_scrolled_window_init      (GtkDwScrolledWindow *scrolled);
static void Dw_gtk_scrolled_window_class_init(GtkDwScrolledWindowClass *klass);

static void Dw_gtk_scrolled_window_size_allocate (GtkWidget *widget,
                                                  GtkAllocation *allocation);

static void Dw_gtk_scrolled_window_changed1  (GtkDwScrolledWindow *scrolled);
static void Dw_gtk_scrolled_window_changed2  (GtkDwScrolledWindow *scrolled);


/*
 * Standard Gtk+ function
 */
GtkType a_Dw_gtk_scrolled_window_get_type (void)
{
   static GtkType type = 0;

   if (!type) {
      GtkTypeInfo info = {
         "GtkDwScrolledWindow",
         sizeof (GtkDwScrolledWindow),
         sizeof (GtkDwScrolledWindowClass),
         (GtkClassInitFunc) Dw_gtk_scrolled_window_class_init,
         (GtkObjectInitFunc) Dw_gtk_scrolled_window_init,
         (GtkArgSetFunc) NULL,
         (GtkArgGetFunc) NULL,
         (GtkClassInitFunc) NULL
      };

      type = gtk_type_unique (GTK_TYPE_SCROLLED_WINDOW, &info);
   }

   return type;
}


/*
 * Standard Gtk+ function
 */
GtkWidget* a_Dw_gtk_scrolled_window_new (void)
{
   return gtk_widget_new (GTK_TYPE_DW_SCROLLED_WINDOW, NULL);
}


/*
 * Standard Gtk+ function
 */
static void Dw_gtk_scrolled_window_init (GtkDwScrolledWindow *scrolled)
{
   GtkAdjustment *hadjustment, *vadjustment;
   GtkWidget *frame, *viewport;
   int i;
   char *signals[] = {
      "button_press_event",
      "button_release_event",
      "motion_notify_event",
      "key_press_event"         /* although the scrollbars are not focused */
   };

   hadjustment =
      GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 10.0, 0.0, 0.0));
   vadjustment =
      GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 10.0, 0.0, 0.0));

   gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (scrolled),
                                        hadjustment);
   gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (scrolled),
                                        vadjustment);

   frame = a_Dw_gtk_scrolled_frame_new (hadjustment, vadjustment);
   gtk_container_add (GTK_CONTAINER (scrolled), frame);
   gtk_widget_show (frame);

   viewport = a_Dw_gtk_viewport_new (hadjustment, vadjustment);
   gtk_container_add (GTK_CONTAINER (frame), viewport);
   gtk_widget_show (viewport);

   scrolled->vadjustment = vadjustment;
   scrolled->old_vadjustment_value = vadjustment->value;

   /*
    * For anchors, we need to recognize when the *user* changes the
    * viewport and distiguish them from changes caused by the program.
    * Instead of using the "change" signal and checking where it came
    * from, the following code connects all possible events by which
    * users could change the scrollbar adjustments. ...
    */
   for (i = 0; i < sizeof (signals) / sizeof (signals[0]); i++) {
      gtk_signal_connect_object
         (GTK_OBJECT (GTK_SCROLLED_WINDOW(scrolled)->vscrollbar),
          signals[i],  GTK_SIGNAL_FUNC (Dw_gtk_scrolled_window_changed1),
          GTK_OBJECT (scrolled));
      gtk_signal_connect_object_after
         (GTK_OBJECT (GTK_SCROLLED_WINDOW(scrolled)->vscrollbar),
          signals[i],  GTK_SIGNAL_FUNC (Dw_gtk_scrolled_window_changed2),
          GTK_OBJECT (scrolled));
   }

   /* ... The GtkDwScrolledFrame has a signal for this. */
   gtk_signal_connect_object (GTK_OBJECT (frame), "user_vchanged",
                              GTK_SIGNAL_FUNC (Dw_gtk_viewport_remove_anchor),
                              GTK_OBJECT (viewport));
#if 0
   /* This does not seem to work for GtkLayout's (see also dw_embed_gtk.c): */
   gtk_container_set_focus_hadjustment (GTK_CONTAINER (viewport),
                                        hadjustment);
   gtk_container_set_focus_vadjustment (GTK_CONTAINER (viewport),
                                        vadjustment);
#endif
}


/*
 * Standard Gtk+ function
 */
static void Dw_gtk_scrolled_window_class_init (GtkDwScrolledWindowClass *klass)
{
   parent_class = gtk_type_class (gtk_scrolled_window_get_type ());
   GTK_WIDGET_CLASS(klass)->size_allocate =
      Dw_gtk_scrolled_window_size_allocate;
}


/*
 * Standard Gtk+ function
 */
static void Dw_gtk_scrolled_window_size_allocate (GtkWidget *widget,
                                                  GtkAllocation *allocation)
{
   GtkDwViewport *viewport;
   GtkAllocation old_allocation = widget->allocation;


   GTK_WIDGET_CLASS(parent_class)->size_allocate (widget, allocation);
   widget->allocation = *allocation;

   DEBUG_MSG (2, "Dw_gtk_scrolled_window_size_allocate: %d x %d\n",
              allocation->width, allocation->height);
   if (old_allocation.width != allocation->width ||
       old_allocation.height != allocation->height) {
      viewport =  GTK_DW_VIEWPORT (GTK_BIN(GTK_BIN(widget)->child)->child);

      /* It may be that scrollbars are not needed anymore. See
         Dw_gtk_viewport_calc_size for more details. */
      if (allocation->width > old_allocation.width)
         viewport->hscrollbar_used = FALSE;
      if (allocation->height > old_allocation.height)
         viewport->vscrollbar_used = FALSE;

      Dw_gtk_viewport_calc_size (viewport);
   }
}


/*
 * Sets the top-level DwWidget. The old top-level DwWidget is destroyed.
 */
void a_Dw_gtk_scrolled_window_set_dw (GtkDwScrolledWindow *scrolled,
                                      DwWidget *widget)
{
   GtkWidget *viewport;
   DwWidget *old_child;

   viewport = GTK_BIN(GTK_BIN(scrolled)->child)->child;

   if ((old_child = GTK_DW_VIEWPORT (viewport)->child))
      gtk_object_destroy (GTK_OBJECT (old_child));

   a_Dw_gtk_viewport_add_dw (GTK_DW_VIEWPORT (viewport), widget);
}

/*
 * Gets the top-level DwWidget.
 */
DwWidget* a_Dw_gtk_scrolled_window_get_dw (GtkDwScrolledWindow *scrolled)
{
   GtkWidget *viewport;

   viewport = GTK_BIN(GTK_BIN(scrolled)->child)->child;
   return GTK_DW_VIEWPORT(viewport)->child;
}

/*
 * See a_Dw_gtk_viewport_set_anchor.
 */
void a_Dw_gtk_scrolled_window_set_anchor (GtkDwScrolledWindow *scrolled,
                                          const gchar *anchor)
{
   GtkWidget *viewport;

   viewport = GTK_BIN(GTK_BIN(scrolled)->child)->child;
   a_Dw_gtk_viewport_set_anchor (GTK_DW_VIEWPORT (viewport), anchor);
}

/*
 * The scrolling position is remembered setting this value in the history-URL
 */
gint a_Dw_gtk_scrolled_window_get_scrolling_position (GtkDwScrolledWindow 
                                                     *scrolled)
{
   GtkLayout *viewport = GTK_LAYOUT(GTK_BIN(GTK_BIN(scrolled)->child)->child);

   return ((int) viewport->vadjustment->value);
}

/*
 * See a_Dw_gtk_viewport_set_scrolling_position.
 */
void a_Dw_gtk_scrolled_window_set_scrolling_position (GtkDwScrolledWindow
                                                      *scrolled,
                                                      gint pos)
{
   GtkWidget *viewport;

   viewport = GTK_BIN(GTK_BIN(scrolled)->child)->child;
   a_Dw_gtk_viewport_set_scrolling_position (GTK_DW_VIEWPORT (viewport), pos);
}


/*
 * See also Dw_gtk_scrolled_window_init.
 * Called before possible change, save the old value.
 */
static void Dw_gtk_scrolled_window_changed1 (GtkDwScrolledWindow *scrolled)
{
   scrolled->old_vadjustment_value = scrolled->vadjustment->value;
}


/*
 * See also Dw_gtk_scrolled_window_init.
 * Called after possible change, compare old and new values.
 */
static void Dw_gtk_scrolled_window_changed2 (GtkDwScrolledWindow *scrolled)
{
   GtkWidget *viewport;

   if (scrolled->old_vadjustment_value != scrolled->vadjustment->value) {
      viewport = GTK_BIN(GTK_BIN(scrolled)->child)->child;
      Dw_gtk_viewport_remove_anchor (GTK_DW_VIEWPORT (viewport));
   }
}
