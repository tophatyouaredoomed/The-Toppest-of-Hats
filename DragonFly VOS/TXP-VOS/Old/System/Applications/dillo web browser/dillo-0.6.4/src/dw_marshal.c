#include "dw_marshal.h"


typedef void (*GtkSignal_NONE__INT_INT_INT) (GtkObject * object,
                                             gint arg1, gint arg2,
                                             gint arg3, gpointer user_data);

void Dw_marshal_NONE__INT_INT_INT (GtkObject * object,
                                   GtkSignalFunc func,
                                   gpointer func_data, GtkArg * args)
{
   GtkSignal_NONE__INT_INT_INT rfunc;
   rfunc = (GtkSignal_NONE__INT_INT_INT) func;
   rfunc (object,
          GTK_VALUE_INT (args[0]), GTK_VALUE_INT (args[1]),
          GTK_VALUE_INT (args[2]), func_data);
}


typedef void (*GtkSignal_NONE__INT_INT_INT_POINTER) (GtkObject * object,
                                                     gint arg1, gint arg2,
                                                     gint arg3,
                                                     gpointer arg4,
                                                     gpointer user_data);

void Dw_marshal_NONE__INT_INT_INT_POINTER (GtkObject * object,
                                           GtkSignalFunc func,
                                           gpointer func_data, GtkArg * args)
{
   GtkSignal_NONE__INT_INT_INT_POINTER rfunc;
   rfunc = (GtkSignal_NONE__INT_INT_INT_POINTER) func;
   rfunc (object,
          GTK_VALUE_INT (args[0]), GTK_VALUE_INT (args[1]),
          GTK_VALUE_INT (args[2]), GTK_VALUE_POINTER (args[3]), func_data);
}

