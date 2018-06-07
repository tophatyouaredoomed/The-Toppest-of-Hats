/*
 * File: progressbar.c
 *
 * Copyright (C) 1999 James McCollough <jamesm@gtwn.net>,
 *                    Jorge Arellano Cid <jcid@inf.utfsm.cl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <string.h>
#include <stdio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "progressbar.h"


/*
 * The progressbar is basically a GtkBox with a text label and gtkprogressbar
 * packed in.  You specify the label string, color, font, and fontsize
 */
GtkWidget* a_Progressbar_new(void)
{
   return gtk_statusbar_new();
}

/*
 * This just sets the color.  basically for future preferences stuff
 */
/*
gint a_Progressbar_set_color(GtkWidget *pbar, GdkColor color)
{
   GtkStyle *style;
   GList *child_list;

   style = gtk_style_copy(gtk_widget_get_style(pbar));
   style->bg[GTK_STATE_PRELIGHT] = color;
   child_list = gtk_container_children(GTK_CONTAINER(pbar));
   while (child_list != NULL) {
      if (GTK_IS_STATUS(child_list->data))
         gtk_widget_set_style(GTK_WIDGET(child_list->data), style);
      child_list = child_list->next;
   }
   return(0);
}
*/

/*
 * Again, for future preferences stuff
 */
gint a_Progressbar_set_font(GtkWidget *pbar, const char *font)
{
   GtkStyle *style;

   style = gtk_style_copy(gtk_widget_get_style(pbar));
   style->font = gdk_font_load(font);
   if ( !style->font )
      return -1;
   gtk_container_foreach(GTK_CONTAINER(pbar),
                         (GtkCallback) gtk_widget_set_style, style);
   return(0);
}

/*
 * Update the specified progress bar.
 *  updatestr : String to display within the bar (NULL is ignored)
 *  sens      : sensitivity (0 = set insensitive, 1 = set sensitive)
 */
void a_Progressbar_update(GtkWidget *pbar, const char *updatestr, gint sens)
{
   gint context_id;

   gtk_widget_set_sensitive(pbar, (sens == 0) ? FALSE : TRUE);

   if ( updatestr != NULL ) {
      context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(pbar), "text");
      gtk_statusbar_pop(GTK_STATUSBAR(pbar), context_id);
      gtk_statusbar_push(GTK_STATUSBAR(pbar), context_id, updatestr);
   }
}

