#ifndef __DW_GTK_SCROLLED_WINDOW_H__
#define __DW_GTK_SCROLLED_WINDOW_H__

#include <gtk/gtkscrolledwindow.h>
#include "dw_widget.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_DW_SCROLLED_WINDOW     (a_Dw_gtk_scrolled_window_get_type ())
#define GTK_DW_SCROLLED_WINDOW(obj)         (GTK_CHECK_CAST (obj, \
                                               GTK_TYPE_DW_SCROLLED_WINDOW, \
                                               GtkDwScrolledWindow))
#define GTK_DW_SCROLLED_WINDOW_CLASS(klass) (GTK_CHECK_CLASS_CAST (klass, \
                                                GTK_TYPE_DW_SCROLLED_WINDOW, \
                                                GtkDwScrolledWindowClass))
#define GTK_IS_DW_SCROLLED_WINDOW(obj)      GTK_CHECK_TYPE (obj, \
                                               GTK_TYPE_DW_SCROLLED_WINDOW)


typedef struct _GtkDwScrolledWindow      GtkDwScrolledWindow;
typedef struct _GtkDwScrolledWindowClass GtkDwScrolledWindowClass;


struct _GtkDwScrolledWindow
{
   GtkScrolledWindow scrolled_window;

   GtkAdjustment *vadjustment;
   gfloat old_vadjustment_value;
};


struct _GtkDwScrolledWindowClass
{
   GtkScrolledWindowClass parent_class;
};


GtkType    a_Dw_gtk_scrolled_window_get_type (void);
GtkWidget* a_Dw_gtk_scrolled_window_new (void);
void       a_Dw_gtk_scrolled_window_set_dw (
              GtkDwScrolledWindow *scrolled, DwWidget *widget);
DwWidget*  a_Dw_gtk_scrolled_window_get_dw (GtkDwScrolledWindow *scrolled);

void       a_Dw_gtk_scrolled_window_set_anchor (
              GtkDwScrolledWindow *scrolled, const gchar *anchor);
gint       a_Dw_gtk_scrolled_window_get_scrolling_position (
              GtkDwScrolledWindow *scrolled);
void       a_Dw_gtk_scrolled_window_set_scrolling_position (
              GtkDwScrolledWindow *scrolled, gint pos);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DW_GTK_SCROLLED_WINDOW_H__ */
