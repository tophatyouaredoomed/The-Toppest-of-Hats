/*
 * Only a free and freeall function. Currently there is only one
 * submodule with init/freeall functions.
 */

#include "dw.h"
#include "dw_style.h"
#include <gdk/gdk.h>

GdkCursor *Dw_cursor_hand;

void a_Dw_init (void)
{
   a_Dw_style_init ();

   Dw_cursor_hand = gdk_cursor_new(GDK_HAND2);
}


void a_Dw_freeall (void)
{
   a_Dw_style_freeall ();

   gdk_cursor_destroy (Dw_cursor_hand);
}
