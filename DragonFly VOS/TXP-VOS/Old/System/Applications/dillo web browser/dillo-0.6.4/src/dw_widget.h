#ifndef __DW_WIDGET_H__
#define __DW_WIDGET_H__

#include <gtk/gtkobject.h>
#include <gtk/gtkwidget.h>
#include <gdk/gdktypes.h>

#include "dw_style.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DW_TYPE_WIDGET          (a_Dw_widget_get_type ())
#define DW_WIDGET(obj)          GTK_CHECK_CAST (obj, DW_TYPE_WIDGET, DwWidget)
#define DW_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, DW_TYPE_WIDGET, \
                                   DwWidgetClass)
#define DW_IS_WIDGET(obj)       GTK_CHECK_TYPE (obj, DW_TYPE_WIDGET)


#define DW_WIDGET_SET_FLAGS(wid, flag)    (DW_WIDGET(wid)->flags |= (flag))
#define DW_WIDGET_UNSET_FLAGS(wid, flag)  (DW_WIDGET(wid)->flags &= ~(flag))

#define DW_WIDGET_NEEDS_RESIZE(wid)      (DW_WIDGET(wid)->flags & \
                                          DW_NEEDS_RESIZE)
#define DW_WIDGET_NEEDS_ALLOCATE(wid)    (DW_WIDGET(wid)->flags & \
                                          DW_NEEDS_ALLOCATE)
#define DW_WIDGET_EXTREMES_CHANGED(wid)  (DW_WIDGET(wid)->flags & \
                                          DW_EXTREMES_CHANGED)
#define DW_WIDGET_REALIZED(wid)          (DW_WIDGET(wid)->flags & DW_REALIZED)

#define DW_WIDGET_USES_HINTS(wid)        (DW_WIDGET(wid)->flags & \
                                          DW_USES_HINTS)
#define DW_WIDGET_HAS_CONTENT(wid)       (DW_WIDGET(wid)->flags & \
                                          DW_HAS_CONTENT)

#define DW_NEEDS_RESIZE      (1 << 0)
#define DW_NEEDS_ALLOCATE    (1 << 1)
#define DW_EXTREMES_CHANGED  (1 << 2)
#define DW_REALIZED          (1 << 3)

#define DW_USES_HINTS        (1 << 4)
#define DW_HAS_CONTENT       (1 << 5)

#define DW_WIDGET_WINDOW(widget) \
   (((GtkDwViewport*)(widget)->viewport)->back_pixmap)

typedef struct _DwRectangle             DwRectangle;
typedef struct _DwAllocation            DwAllocation;
typedef struct _DwRequisition           DwRequisition;
typedef struct _DwExtremes              DwExtremes;

typedef struct _DwWidget                DwWidget;
typedef struct _DwWidgetClass           DwWidgetClass;

#define DW_PAINT_DEFAULT_BGND   0xd6d6c0


struct _DwRectangle
{
   gint32 x;
   gint32 y;
   gint32 width;
   gint32 height;
};


struct _DwAllocation
{
   gint32 x;
   gint32 y;
   gint32 width;
   gint32 ascent;
   gint32 descent;
};


struct _DwRequisition
{
   gint32 width;
   gint32 ascent;
   gint32 descent;
};


struct _DwExtremes
{
   gint32 min_width;
   gint32 max_width;
};

struct _DwWidget
{
   GtkObject object;

   /* the parent widget, NULL for top-level widgets */
   DwWidget *parent;

   /* todo: comment */
   gint parent_ref;

   /* the viewport in which the widget is shown */
   GtkWidget *viewport;

   /* see DW_... at the beginning */
   gint flags;

   /* the current allocation: size and position, always relative to the
      scrolled area! */
   DwAllocation allocation;

   /* a_Dw_widget_size_request stores the result of the last call of
      Dw_xxx_size_request here, don't read this directly, but call
      a_Dw_widget_size_request. */
   DwRequisition requisition;

   /* analogue to requisition */
   DwExtremes extremes;

   /* Anchors of the widget.
    * Key: gchar*, has to be stored elsewhere
    * Value: int (pixel offset [1 based]) */
   GHashTable *anchors_table;

   GdkCursor *cursor; /* todo: move this to style */
   DwStyle *style;
};


struct _DwWidgetClass
{
   GtkObjectClass parent_class;

   void (*size_request)         (DwWidget *widget,
                                 DwRequisition *requisition);
   void (*get_extremes)         (DwWidget *widget,
                                 DwExtremes *extremes);
   void (*size_allocate)        (DwWidget *widget,
                                 DwAllocation *allocation);
   void (*mark_size_change)     (DwWidget *widget,
                                 gint ref);
   void (*mark_extremes_change) (DwWidget *widget,
                                 gint ref);
   void (*set_width)            (DwWidget *widget,
                                 gint32 width);
   void (*set_ascent)           (DwWidget *widget,
                                 gint32 ascent);
   void (*set_descent)          (DwWidget *widget,
                                 gint32 descent);
   void (*draw)                 (DwWidget *widget,
                                 DwRectangle *area,
                                 GdkEventExpose *event);

   void (*realize)              (DwWidget *widget);
   void (*unrealize)            (DwWidget *widget);

   gint (*button_press_event)   (DwWidget *widget,
                                 gint32 x,
                                 gint32 y,
                                 GdkEventButton *event);
   gint (*button_release_event) (DwWidget *widget,
                                 gint32 x,
                                 gint32 y,
                                 GdkEventButton *event);
   gint (*motion_notify_event)  (DwWidget *widget,
                                 gint32 x,
                                 gint32 y,
                                 GdkEventMotion *event);
   gint (*enter_notify_event)   (DwWidget *widget,
                                 GdkEventMotion *event);
   gint (*leave_notify_event)   (DwWidget *widget,
                                 GdkEventMotion *event);
};


GtkType a_Dw_widget_get_type        (void);

void    a_Dw_widget_size_request    (DwWidget *widget,
                                     DwRequisition *requisition);
void    a_Dw_widget_get_extremes    (DwWidget *widget,
                                     DwExtremes *extremes);
void    a_Dw_widget_size_allocate   (DwWidget *widget,
                                     DwAllocation *allocation);
void    a_Dw_widget_set_width       (DwWidget *widget,
                                     gint32 width);
void    a_Dw_widget_set_ascent      (DwWidget *widget,
                                     gint32 ascent);
void    a_Dw_widget_set_descent     (DwWidget *widget,
                                     gint32 descent);
void    a_Dw_widget_draw            (DwWidget *widget,
                                     DwRectangle *area,
                                     GdkEventExpose *event);
void    a_Dw_widget_realize         (DwWidget *widget);
void    a_Dw_widget_unrealize       (DwWidget *widget);

void    a_Dw_widget_set_style       (DwWidget *widget,
                                     DwStyle *style);
void    a_Dw_widget_set_cursor      (DwWidget *widget,
                                     GdkCursor *cursor);

DwWidget *a_Dw_widget_get_toplevel  (DwWidget *widget);

void    a_Dw_widget_scroll_to       (DwWidget *widget,
                                     int pos);

/* Only for Dw module */
gint    Dw_rectangle_intersect      (DwRectangle *src1,
                                     DwRectangle *src2,
                                     DwRectangle *dest);
gint    Dw_widget_intersect         (DwWidget *widget,
                                     DwRectangle *area,
                                     DwRectangle *intersection);
void    Dw_widget_set_parent        (DwWidget *widget,
                                     DwWidget *parent);

gint32  Dw_widget_x_viewport_to_world (DwWidget *widget,
                                       gint16 viewport_x);
gint32  Dw_widget_y_viewport_to_world (DwWidget *widget,
                                       gint16 viewport_y);
gint16  Dw_widget_x_world_to_viewport (DwWidget *widget,
                                       gint32 world_x);
gint16  Dw_widget_y_world_to_viewport (DwWidget *widget,
                                       gint32 world_y);

gint    Dw_widget_mouse_event       (DwWidget *widget,
                                     GtkWidget *viewwidget,
                                     gint32 x,
                                     gint32 y,
                                     GdkEvent *event);
void    Dw_widget_queue_draw        (DwWidget *widget);
void    Dw_widget_queue_draw_area   (DwWidget *widget,
                                     gint32 x,
                                     gint32 y,
                                     gint32 width,
                                     gint32 height);
void    Dw_widget_queue_clear       (DwWidget *widget);
void    Dw_widget_queue_clear_area  (DwWidget *widget,
                                     gint32 x,
                                     gint32 y,
                                     gint32 width,
                                     gint32 height);
void    Dw_widget_queue_resize      (DwWidget *widget,
                                     gint ref,
                                     gboolean extremes_changed);

void    Dw_widget_set_anchor        (DwWidget *widget,
                                     gchar *name,
                                     int pos);

/* Wrappers for Dw_style_draw_box */
void    Dw_widget_draw_box          (DwWidget *widget,
                                     DwStyle *style,
                                     DwRectangle *area,
                                     gint32 x,
                                     gint32 y,
                                     gint32 width,
                                     gint32 height);
void    Dw_widget_draw_widget_box   (DwWidget *widget,
                                     DwRectangle *area);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DW_WIDGET_H__ */
