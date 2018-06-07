/*
 * File: dw_bullet.c
 *
 * Copyright (C) 1997 Raph Levien <raph@acm.org>
 * Copyright (C) 1999 Luca Rota <drake@freemail.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "dw_bullet.h"
#include "dw_gtk_viewport.h"

static void Dw_bullet_init              (DwBullet *bullet);
static void Dw_bullet_class_init        (DwBulletClass *klass);
static void Dw_bullet_size_request      (DwWidget *widget,
                                         DwRequisition *requisition);
static void Dw_bullet_draw              (DwWidget *widget,
                                         DwRectangle *area,
                                         GdkEventExpose *event);


GtkType a_Dw_bullet_get_type (void)
{
   static GtkType type = 0;

   if (!type) {
      GtkTypeInfo info = {
         "DwBullet",
         sizeof (DwBullet),
         sizeof (DwBulletClass),
         (GtkClassInitFunc) Dw_bullet_class_init,
         (GtkObjectInitFunc) Dw_bullet_init,
         (GtkArgSetFunc) NULL,
         (GtkArgGetFunc) NULL,
         (GtkClassInitFunc) NULL
      };

      type = gtk_type_unique (DW_TYPE_WIDGET, &info);
   }

   return type;
}


DwWidget* a_Dw_bullet_new (DwBulletType type)
{
   GtkObject *object;

   object = gtk_object_new (DW_TYPE_BULLET, NULL);
   DW_BULLET(object)->type = type;

   return DW_WIDGET (object);
}


static void Dw_bullet_init (DwBullet *bullet)
{
   bullet->type = DW_BULLET_DISC;
}


static void Dw_bullet_class_init (DwBulletClass *klass)
{
   DwWidgetClass *widget_class;

   widget_class = (DwWidgetClass*)klass;
   widget_class->size_request = Dw_bullet_size_request;
   widget_class->draw = Dw_bullet_draw;
}


static void Dw_bullet_size_request (DwWidget *widget,
                                    DwRequisition *requisition)
{
   requisition->width = 8;
   requisition->ascent = 8;
   requisition->descent = 0;
}


static void Dw_bullet_draw (DwWidget *widget,
                            DwRectangle *area,
                            GdkEventExpose *event)
{
   gint x, y;
   GdkGC *gc;
   GdkWindow *window;

   x = Dw_widget_x_world_to_viewport (widget, widget->allocation.x);
   y = Dw_widget_y_world_to_viewport (widget, widget->allocation.y);
   gc = widget->style->color->gc;
   window = DW_WIDGET_WINDOW (widget);

   switch (DW_BULLET(widget)->type) {
   case DW_BULLET_DISC:
      gdk_draw_arc(window, gc, TRUE, x + 2, y + 1, 4, 4, 0, 360 * 64);
      gdk_draw_arc(window, gc, FALSE, x + 2, y + 1, 4, 4, 0, 360 * 64);
      break;
   case DW_BULLET_CIRCLE:
      gdk_draw_arc(window, gc, FALSE, x + 1, y, 6, 6, 0, 360 * 64);
      break;
   case DW_BULLET_SQUARE:
      gdk_draw_rectangle(window, gc, FALSE, x + 1, y, 6, 6);
      break;
   default: /* Should/could the numeric bullets be treated here ? */
      break;
   }
}
