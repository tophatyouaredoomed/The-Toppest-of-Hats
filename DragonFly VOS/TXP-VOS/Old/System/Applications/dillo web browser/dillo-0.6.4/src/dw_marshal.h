#ifndef __DW_MARSHAL_H__
#define __DW_MARSHAL_H__

#include <gtk/gtktypeutils.h>
#include <gtk/gtkobject.h>

void Dw_marshal_NONE__INT_INT_INT (GtkObject * object,
                                   GtkSignalFunc func,
                                   gpointer func_data, GtkArg * args);
void Dw_marshal_NONE__INT_INT_INT_POINTER (GtkObject * object,
                                           GtkSignalFunc func,
                                           gpointer func_data, GtkArg * args);

/*
 * Marshal fuctions for standard link signals.
 */
#define Dw_marshal_link_enter  Dw_marshal_NONE__INT_INT_INT
#define Dw_marshal_link_button Dw_marshal_NONE__INT_INT_INT_POINTER


#endif /* __DW_MARSHAL_H__ */
