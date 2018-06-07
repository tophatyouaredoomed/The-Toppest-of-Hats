#ifndef __DW_GTK_VIEWPORT_H__
#define __DW_GTK_VIEWPORT_H__

#include <gtk/gtklayout.h>
#include "dw_widget.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_DW_VIEWPORT            (a_Dw_gtk_viewport_get_type ())
#define GTK_DW_VIEWPORT(obj)            (GTK_CHECK_CAST (obj, \
                                           GTK_TYPE_DW_VIEWPORT,GtkDwViewport))
#define GTK_DW_VIEWPORT_CLASS(klass)    (GTK_CHECK_CLASS_CAST (klass, \
                                          GTK_TYPE_DW_VIEWPORT, \
                                          GtkDwViewportClass)
#define GTK_IS_DW_VIEWPORT(obj)         GTK_CHECK_TYPE (obj, \
                                           GTK_TYPE_DW_VIEWPORT)

typedef struct _GtkDwViewport       GtkDwViewport;
typedef struct _GtkDwViewportClass  GtkDwViewportClass;


struct _GtkDwViewport
{
   GtkLayout layout;

   GdkPixmap *back_pixmap;              /* backing pixmap for buffering */
   gint back_width, back_height;
   gint depth;

   DwWidget *child;
   DwWidget *last_entered;
   gint resize_idle_id;
   gboolean hscrollbar_used, vscrollbar_used, calc_size_blocked;

   /* updated by Dw_gtk_viewport_motion_notify */
   gdouble mouse_x, mouse_y;

   gchar *anchor;
   gint32 anchor_y;
   gint anchor_idle_id;
};


struct _GtkDwViewportClass
{
   GtkLayoutClass parent_class;
};


GtkType        a_Dw_gtk_viewport_get_type        (void);
GtkWidget*     a_Dw_gtk_viewport_new             (GtkAdjustment *hadjustment,
                                                  GtkAdjustment *vadjustment);
void           a_Dw_gtk_viewport_add_dw          (GtkDwViewport *viewport,
                                                  DwWidget *widget);

void           a_Dw_gtk_viewport_set_anchor      (GtkDwViewport *viewport,
                                                  const gchar *anchor);
void           a_Dw_gtk_viewport_set_scrolling_position (GtkDwViewport
                                                         *viewport,
                                                         gint pos);

void           Dw_gtk_viewport_remove_dw         (GtkDwViewport *viewport);
void           Dw_gtk_viewport_calc_size         (GtkDwViewport *viewport);

DwWidget*      Dw_gtk_viewport_widget_at_point (GtkDwViewport *viewport,
                                                gint32 x,
                                                gint32 y);

void           Dw_gtk_viewport_update_anchor    (GtkDwViewport *viewport);
void           Dw_gtk_viewport_scroll_to        (GtkDwViewport *viewport,
                                                 gint32 y);
void           Dw_gtk_viewport_remove_anchor    (GtkDwViewport *viewport);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DW_GTK_VIEWPORT_H__ */
