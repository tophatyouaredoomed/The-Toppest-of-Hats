/*
 * File: dw_image.c
 *
 * Copyright (C) 2001 Sebastian Geerken  <S.Geerken@ping.de>,
 *                    Jorge Arellano Cid <jcid@inf.utfsm.cl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "dw_image.h"
#include "dw_gtk_viewport.h"
#include "prefs.h"
#include "dw_marshal.h"
#include "list.h"
#include "dicache.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

static void Dw_image_init               (DwImage *image);
static void Dw_image_class_init         (DwImageClass *klass);

static void Dw_image_destroy            (GtkObject *object);

static void Dw_image_size_request       (DwWidget *widget,
                                         DwRequisition *requisition);
static void Dw_image_size_allocate      (DwWidget *widget,
                                         DwAllocation *allocation);
static void Dw_image_draw               (DwWidget *widget,
                                         DwRectangle *area,
                                         GdkEventExpose *event);
static gint Dw_image_button_press       (DwWidget *widget,
                                         gint32 x,
                                         gint32 y,
                                         GdkEventButton *event);
static gint Dw_image_button_release     (DwWidget *widget,
                                         gint32 x,
                                         gint32 y,
                                         GdkEventButton *event);
static gint Dw_image_motion_notify      (DwWidget *widget,
                                         gint32 x,
                                         gint32 y,
                                         GdkEventMotion *event);
static gint Dw_image_enter_notify       (DwWidget *widget,
                                         GdkEventMotion *event);
static gint Dw_image_leave_notify       (DwWidget *widget,
                                         GdkEventMotion *event);

static void Dw_image_find_link          (DwImage *image,
                                         gint x, gint y,
                                         gint *link,
                                         gint *link_x, gint *link_y);
static void Dw_image_scale_row          (DwImage *image, gint y_dest);
static void Dw_image_scale              (DwImage *image);

static gint Dw_image_map_list_find_link (DwImageMapList *list,
                                         DilloUrl *url,
                                         gint x,
                                         gint y);


#define Dw_image_scaled_y(image, y_src) \
   ( (y_src) * ( ((DwWidget*)(image))->allocation.ascent +  \
                 ((DwWidget*)(image))->allocation.descent - \
                 Dw_style_box_diff_height ( ((DwWidget*)(image))->style ) ) \
             / ((DwImage*)(image))->height )


enum
{
   LINK_ENTERED,
   LINK_PRESSED,
   LINK_RELEASED,
   LINK_CLICKED,
   LAST_SIGNAL
};

static guint image_signals[LAST_SIGNAL] = { 0 };
static DwWidgetClass *parent_class;


/*
 * Standard Gtk+ function.
 */
GtkType a_Dw_image_get_type (void)
{
   static GtkType type = 0;

   if (!type) {
      GtkTypeInfo info = {
         "DwImage",
         sizeof (DwImage),
         sizeof (DwImageClass),
         (GtkClassInitFunc) Dw_image_class_init,
         (GtkObjectInitFunc) Dw_image_init,
         (GtkArgSetFunc) NULL,
         (GtkArgGetFunc) NULL,
         (GtkClassInitFunc) NULL
      };

      type = gtk_type_unique (DW_TYPE_WIDGET, &info);
   }

   return type;
}


/*
 * Standard Gtk+ function.
 */
DwWidget* a_Dw_image_new (DwImageType type, const gchar *alt)
{
   GtkObject *object;

   object = gtk_object_new (DW_TYPE_IMAGE, NULL);
   if (alt && alt[0] && prefs.show_alt)
      DW_IMAGE(object)->alt_tooltip = a_Dw_tooltip_new (alt);
   return DW_WIDGET (object);
}


/*
 * Standard Gtk+ function.
 */
static void Dw_image_init (DwImage *image)
{
   image->url = NULL;
   image->width = 0;
   image->height = 0;
   image->buffer = NULL;
   image->scaled_buffer = NULL;
   image->alt_tooltip = NULL;
   image->usemap_url = NULL;
}


/*
 * Standard Gtk+ function.
 */
static void Dw_image_class_init (DwImageClass *klass)
{
   GtkObjectClass *object_class;
   DwWidgetClass *widget_class;

   parent_class = gtk_type_class (DW_TYPE_WIDGET);

   object_class = (GtkObjectClass*)klass;
   object_class->destroy = Dw_image_destroy;

   image_signals[LINK_ENTERED] =
      gtk_signal_new ("link_entered",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwImageClass, link_entered),
                      Dw_marshal_link_enter,
                      GTK_TYPE_NONE,
                      3, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT);
   image_signals[LINK_PRESSED] =
      gtk_signal_new ("link_pressed",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwImageClass, link_pressed),
                      Dw_marshal_link_button,
                      GTK_TYPE_NONE,
                      4, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT,
                      GTK_TYPE_GDK_EVENT);
   image_signals[LINK_RELEASED] =
      gtk_signal_new ("link_released",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwImageClass, link_released),
                      Dw_marshal_link_button,
                      GTK_TYPE_NONE,
                      4, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT,
                      GTK_TYPE_GDK_EVENT);
   image_signals[LINK_CLICKED] =
      gtk_signal_new ("link_clicked",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwImageClass, link_clicked),
                      Dw_marshal_link_button,
                      GTK_TYPE_NONE,
                      4, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT,
                      GTK_TYPE_GDK_EVENT);
   gtk_object_class_add_signals (object_class, image_signals, LAST_SIGNAL);

   widget_class = (DwWidgetClass*)klass;
   widget_class->size_request = Dw_image_size_request;
   widget_class->size_allocate = Dw_image_size_allocate;
   widget_class->draw = Dw_image_draw;
   widget_class->button_press_event = Dw_image_button_press;
   widget_class->button_release_event = Dw_image_button_release;
   widget_class->motion_notify_event = Dw_image_motion_notify;
   widget_class->enter_notify_event = Dw_image_enter_notify;
   widget_class->leave_notify_event = Dw_image_leave_notify;
}


/*
 * Standard Gtk+ function.
 */
static void Dw_image_destroy (GtkObject *object)
{
   DwImage *image = DW_IMAGE (object);

   if (image->alt_tooltip)
      a_Dw_tooltip_destroy (image->alt_tooltip);
   if (image->scaled_buffer)
      g_free (image->scaled_buffer);
   if (image->usemap_url)
      a_Url_free(image->usemap_url);
   if (image->url)
      a_Dicache_unref(image->url, image->version);

   (* GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}


/*
 * Standard Dw function.
 */
static void Dw_image_size_request (DwWidget *widget,
                                   DwRequisition *requisition)
{
   DwImage *image;

   image = DW_IMAGE (widget);
   requisition->width = image->width + Dw_style_box_diff_width (widget->style);
   requisition->ascent =
      image->height + Dw_style_box_diff_height (widget->style);
   requisition->descent = 0;
}


/*
 * Standard Dw function.
 */
static void Dw_image_size_allocate (DwWidget *widget,
                                    DwAllocation *allocation)
{
   DwImage *image;

   /* if image is moved only */
   if (allocation->width == widget->allocation.width &&
       allocation->ascent + allocation->descent
       == widget->allocation.ascent + widget->allocation.descent)
      return;

   /* this is also done in a_Dw_widget_size_allocate, but
      Dw_image_scale needs this. */
   widget->allocation = *allocation;

   image = DW_IMAGE (widget);
   if (image->buffer != NULL && image->width > 0 && image->height > 0)
      Dw_image_scale (image);
}


/*
 * Standard Dw function.
 */
static void Dw_image_draw (DwWidget *widget,
                           DwRectangle *area,
                           GdkEventExpose *event)
{
   gint32 vx, vy;
   DwRectangle content, intersection;
   GdkGC *gc;
   DwImage *image = DW_IMAGE (widget);
   guchar *buffer, *bstart;

   //g_print(">Area x=%d y=%d w=%d h=%d\n", area->x, area->y,
   //        area->width, area->height);

   Dw_widget_draw_widget_box (widget, area);

   if (image->buffer) {
      content.x = Dw_style_box_offset_x (widget->style);
      content.y = Dw_style_box_offset_y (widget->style);
      content.width =
         widget->allocation.width - Dw_style_box_diff_width (widget->style);
      content.height = widget->allocation.ascent + widget->allocation.descent -
         Dw_style_box_diff_height (widget->style);

      if (Dw_rectangle_intersect (area, &content, &intersection)) {
         vx = Dw_widget_x_world_to_viewport (widget, widget->allocation.x);
         vy = Dw_widget_y_world_to_viewport (widget, widget->allocation.y);
         gc = widget->style->color->gc;

         if (image->scaled_buffer)
            buffer = image->scaled_buffer;
         else
            buffer = image->buffer;

         bstart = buffer +
            3 * ( intersection.x - Dw_style_box_offset_x (widget->style)+
                  content.width * (intersection.y -
                                   Dw_style_box_offset_y (widget->style)) );

         gdk_draw_rgb_image(
            DW_WIDGET_WINDOW (widget), gc,
            vx + intersection.x, vy + intersection.y,
            intersection.width, intersection.height, GDK_RGB_DITHER_MAX,
            bstart, content.width * 3);
      }
   }
}


/*
 * Standard Dw function.
 */
static gint Dw_image_enter_notify (DwWidget *widget,
                                   GdkEventMotion *event)
{
   DwImage *image = DW_IMAGE (widget);

   if (image->alt_tooltip)
      a_Dw_tooltip_on_enter (image->alt_tooltip);
   return FALSE;
}


/*
 * Standard Dw function.
 */
static gint Dw_image_leave_notify (DwWidget *widget,
                                   GdkEventMotion *event)
{
   DwImage *image = DW_IMAGE (widget);

   if (image->alt_tooltip)
      a_Dw_tooltip_on_leave (image->alt_tooltip);

   if (image->hover_link != -1) {
      image->hover_link = -1;
      gtk_signal_emit (GTK_OBJECT (widget), image_signals[LINK_ENTERED],
                       -1, -1, -1);
   }

   return FALSE;
}


/*
 * Standard Dw function.
 */
static gint Dw_image_button_press (DwWidget *widget,
                                   gint32 x,
                                   gint32 y,
                                   GdkEventButton *event)
{
   DwImage *image = DW_IMAGE (widget);
   gint link_x, link_y;

   Dw_image_find_link (image, x, y, &image->pressed_link, &link_x, &link_y);

   if (image->pressed_link >= 0) {
      gtk_signal_emit (GTK_OBJECT (widget), image_signals[LINK_PRESSED],
                       image->pressed_link, link_x, link_y, event);
      return TRUE;
   } else
      return FALSE;
}


/*
 * Standard Dw function.
 */
static gint Dw_image_button_release (DwWidget *widget,
                                     gint32 x,
                                     gint32 y,
                                     GdkEventButton *event)
{
   DwImage *image = DW_IMAGE (widget);
   gint link_pressed, link_released, link_x, link_y;

   link_pressed = image->pressed_link;
   Dw_image_find_link (image, x, y, &link_released, &link_x, &link_y);
   image->pressed_link = -1;

   if (link_released >= 0) {
      gtk_signal_emit (GTK_OBJECT (widget), image_signals[LINK_RELEASED],
                       link_released, link_x, link_y, event);
      if (link_pressed == link_released)
         gtk_signal_emit (GTK_OBJECT (widget), image_signals[LINK_CLICKED],
                          link_released, link_x, link_y, event);
      return TRUE;
   } else
      return FALSE;
}


/*
 * Standard Dw function.
 */
static gint Dw_image_motion_notify (DwWidget *widget,
                                    gint32 x, gint32 y,
                                    GdkEventMotion *event)
{
   DwImage *image = DW_IMAGE (widget);
   gint link, link_old, link_x, link_y;

   if (image->alt_tooltip)
      a_Dw_tooltip_on_motion (image->alt_tooltip);

   link_old = image->hover_link;
   Dw_image_find_link (image, x, y, &link, &link_x, &link_y);
   image->hover_link = link;

   /* In case of a ISMAP image, the signal has to be emitted always */
   if (link_x != -1 || link != link_old) {
      gtk_signal_emit (GTK_OBJECT (widget), image_signals[LINK_ENTERED],
                       link, link_x, link_y);
      return TRUE;
   } else
      return (link != -1);
}


/*
 * Find a link given a coordinate location relative to the window.
 */
static void Dw_image_find_link (DwImage *image,
                                gint x, gint y,
                                gint *link,
                                gint *link_x, gint *link_y)
{
   DwStyle *style;
   DwWidget *widget = DW_WIDGET (image);

   *link_x = -1;
   *link_y = -1;

   if (image->map_list && image->usemap_url) {
      *link = Dw_image_map_list_find_link (image->map_list,
                                           image->usemap_url, x, y);
      if (*link != -1)
         return;
   }

   if (image->ismap &&
       (style = DW_WIDGET(image)->style) && style->link != -1) {
      *link = style->link;
      *link_x = x - Dw_style_box_offset_x (widget->style);
      *link_y = y - Dw_style_box_offset_y (widget->style);
      return;
   }

   *link = -1;
}


/*
 * Set or resize a image.
 */
void a_Dw_image_size(DwImage *image, gint width, gint height)
{
   gint Resize = (image->width != width || image->height != height);

   image->width = width;
   image->height = height;
   if ( Resize )
      Dw_widget_queue_resize(DW_WIDGET (image), 0, TRUE);

   if (image->buffer)
      /* if a_Dw_image_set_buffer has been called before */
      Dw_image_scale (image);
}

/*
 * Called after the RGB line buffer has been copied into the full
 * image buffer. Uses for drawing and scaling.
 */
void a_Dw_image_draw_row(DwImage *image,
                         gint Width, gint Height, gint x, gint y)
{
   DwWidget *widget = DW_WIDGET (image);
   gint dy1, dy2;

   // g_print("a_Dw_image_draw_row: x=%d y=%d\n", x, y);
   g_return_if_fail (image->buffer != NULL);

   if (image->scaled_buffer) {
      Dw_image_scale_row (image, y);

      dy1 = Dw_image_scaled_y (image, y);
      dy2 = Dw_image_scaled_y (image, y + 1);
      Dw_widget_queue_draw_area(widget,
                                x, dy1 + Dw_style_box_offset_y (widget->style),
                                widget->allocation.width, dy2 - dy1);
   } else
      Dw_widget_queue_draw_area (widget,
                                 x, y + Dw_style_box_offset_y (widget->style),
                                 widget->allocation.width, 1);
}

/*
 * Set the widget buffer to reference the dicache entry buffer
 */
void a_Dw_image_set_buffer(DwImage *image, guchar *ImageBuffer,
                           DilloUrl *url, gint version)
{
   image->buffer = ImageBuffer;
   image->url = url;
   image->version = version;

   if (image->width > 0 && image->height > 0)
      /* if a_Dw_image_set_size has been called before */
      Dw_image_scale (image);
}

/*
 * Sets image as server side image map.
 */
void a_Dw_image_set_ismap (DwImage *image)
{
   image->ismap = TRUE;
}

/*
 * Sets image as client side image map.
 */
void a_Dw_image_set_usemap (DwImage *image,  DwImageMapList *map_list,
                            DilloUrl *usemap_url)
{
   image->map_list = map_list;

   if (image->usemap_url)
      a_Url_free(image->usemap_url);
   image->usemap_url = usemap_url ? a_Url_dup (usemap_url) : NULL;
}


/*
 * Scale the whole image: Compare buffer size with allocation, and, if
 * necessary, allocate a second buffer and scale all rows.
 */
static void Dw_image_scale (DwImage *image)
{
   int w, h, y;
   DwWidget *widget;

   if (image->scaled_buffer) {
      g_free (image->scaled_buffer);
      image->scaled_buffer = NULL;
   }

   widget = DW_WIDGET (image);
   w = widget->allocation.width - Dw_style_box_diff_width (widget->style);
   h = widget->allocation.ascent + widget->allocation.descent -
      Dw_style_box_diff_height (widget->style);

   /* Zero or negative sizes? Ignore. */
   if (w <= 0 || h <= 0)
      return;

   if (image->width != w || image->height != h) {
      /* scaled image */
      image->scaled_buffer = g_malloc (3 * w * h);

      for (y = 0; y < image->height; y++)
         Dw_image_scale_row (image, y);
   }
}


/*
 * Scale one row. y_src is the row in the dicache buffer.
 */
static void Dw_image_scale_row (DwImage *image, gint y_src)
{
   DwWidget *widget;
   guchar *src, *dest, *dest1;
   gint w_src, w_dest, x_src, x_dest, y_dest1, y_dest2, y_dest, delta;

   widget = DW_WIDGET (image);
   w_src = image->width;
   w_dest = widget->allocation.width - Dw_style_box_diff_width (widget->style);
   y_dest1 = Dw_image_scaled_y (image, y_src);
   y_dest2 = Dw_image_scaled_y (image, y_src + 1);

   src = image->buffer + 3 * y_src * w_src;

   if (y_dest1 != y_dest2) {
      dest1 = image->scaled_buffer + 3 * y_dest1 * w_dest;

      if (w_src == w_dest)
         memcpy (dest1, src, 3 * w_src);
      else if (w_dest > w_src) {
         delta = w_src / 2;
         x_src = 0;
         x_dest = 0;

         while (x_dest < w_dest) {
            memcpy (dest1 + 3 * x_dest, src + 3 * x_src, 3);
            x_dest++;
            delta += w_src;
            while (delta > w_dest) {
               delta -= w_dest;
               x_src++;
            }
         }
      } else {
         delta = w_dest / 2;
         x_src = 0;
         x_dest = 0;

         while (x_src < w_src) {
            memcpy (dest1 + 3 * x_dest, src + 3 * x_src, 3);
            x_src++;
            delta += w_dest;
            while (delta > w_src) {
               delta -= w_src;
               x_dest++;
            }
         }
      }

      /* The other lines are simply copied. */
      for (y_dest = y_dest1 + 1; y_dest < y_dest2; y_dest++) {
         dest = image->scaled_buffer + 3 * y_dest * w_dest;
         memcpy (dest, dest1, 3 * w_dest);
      }
   }
}


/*
 * Image Maps
 */

/*
 * Initialize a DwImageMapList. The memory has to be allocated before.
 */
void a_Dw_image_map_list_init (DwImageMapList *list)
{
   list->num_maps = 0;
   list->num_maps_max = 8;
   list->maps = g_new(DwImageMap, list->num_maps_max);

   list->num_shapes = 0;
   list->num_shapes_max = 8;
   list->shapes = g_new(DwImageMapShape, list->num_shapes_max);
}


/*
 * Free the content of a DwImageMapList. The memory for the list is
 * not freed.
 */
void a_Dw_image_map_list_free (DwImageMapList *list)
{
   gint i;

   for (i = 0; i < list->num_maps; i++)
      a_Url_free (list->maps[i].url);
   g_free (list->maps);

   for (i = 0; i < list->num_shapes; i++)
      if (list->shapes[i].type == DW_IMAGE_MAP_SHAPE_POLY)
         gdk_region_destroy (list->shapes[i].data.poly);
   g_free (list->shapes);
}


/*
 * Add a new map to the list. Next added shapes will belong to this
 * map.
 */
void a_Dw_image_map_list_add_map (DwImageMapList *list,
                                  DilloUrl *url)
{
   list->num_maps++;
   a_List_add(list->maps, list->num_maps, sizeof(DwImageMap),
              list->num_maps_max);
   list->maps[list->num_maps - 1].url = a_Url_dup (url);
   list->maps[list->num_maps - 1].start_shape = list->num_shapes;
}


void a_Dw_image_map_list_add_shape (DwImageMapList *list,
                                    gint type,
                                    gint link,
                                    GdkPoint *points,
                                    gint num_points)
{
   gboolean correct_args;
   DwImageMapShape *shape;

   switch (type) {
   case DW_IMAGE_MAP_SHAPE_RECT:
      correct_args = (num_points >= 2);
      break;

   case DW_IMAGE_MAP_SHAPE_CIRCLE:
      correct_args = (num_points >= 1 && points[1].x != 0);
      break;

   case DW_IMAGE_MAP_SHAPE_POLY:
      correct_args = (num_points >= 3);
      break;

   default:
      correct_args = FALSE;
   }

   if (correct_args) {
      list->num_shapes++;
      a_List_add(list->shapes, list->num_shapes, sizeof(DwImageMapShape),
              list->num_shapes_max);
      shape = &list->shapes[list->num_shapes - 1];
      shape->type = type;
      shape->link = link;

      switch (type) {
      case DW_IMAGE_MAP_SHAPE_RECT:
         shape->data.rect.left = points[0].x;
         shape->data.rect.top = points[0].y;
         shape->data.rect.right = points[1].x;
         shape->data.rect.bottom = points[1].y;
         break;

      case DW_IMAGE_MAP_SHAPE_CIRCLE:
         shape->data.circle.x = points[0].x;
         shape->data.circle.y = points[0].y;
         shape->data.circle.r2 = points[1].x * points[1].x;
         break;

      case DW_IMAGE_MAP_SHAPE_POLY:
         shape->data.poly = gdk_region_polygon (points, num_points,
                                                GDK_WINDING_RULE);
         break;
      }
   }
}


/*
 * Find a link in an image map.
 */
static gint Dw_image_map_list_find_link (DwImageMapList *list,
                                         DilloUrl *url,
                                         gint x,
                                         gint y)
{
   gint i, j, start, end, dx, dy;
   DwImageMapShape *shape;

   for (i = 0; i < list->num_maps; i++) {
      if (a_Url_cmp (list->maps[i].url, url) == 0 &&
          URL_STRCAMP_EQ(URL_FRAGMENT(list->maps[i].url), URL_FRAGMENT(url))) {
         /* map found */
         start = list->maps[i].start_shape;
         if (i == list->num_maps - 1)
            end = list->num_shapes;
         else
            end = list->maps[i + 1].start_shape;

         for (j = start; j < end; j++) {
            shape = &list->shapes[j];
            switch (shape->type) {
            case DW_IMAGE_MAP_SHAPE_CIRCLE:
               dx = shape->data.circle.x - x;
               dy = shape->data.circle.y - y;
               if (shape->data.circle.r2 >= (dx*dx + dy*dy))
                  return shape->link;
               break;

            case DW_IMAGE_MAP_SHAPE_RECT:
               if (x > shape->data.rect.left &&
                   x < shape->data.rect.right &&
                   y > shape->data.rect.top &&
                   y < shape->data.rect.bottom)
                  return shape->link;
               break;

            case DW_IMAGE_MAP_SHAPE_POLY:
               if (gdk_region_point_in (shape->data.poly, x, y))
                  return shape->link;
               break;
            }
         }

         /* no shape found */
         return -1;
      }
   }

   /* no map found */
   return -1;
}
