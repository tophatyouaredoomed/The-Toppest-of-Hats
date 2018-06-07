/*
 * File: commands.c
 *
 * Copyright (C) 1997 Raph Levien <raph@acm.org>
 * Copyright (C) 1999 Sammy Mannaert <nstalkie@tvd.be>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <gtk/gtk.h>
#include <stdio.h>              /* for sprintf */
#include <sys/time.h>           /* for gettimeofday (testing gorp only) */
#include <unistd.h>
#include <string.h>             /* for strcat() */

#include "bookmark.h"
#include "interface.h"
#include "dillo.h"
#include "history.h"
#include "nav.h"
#include "misc.h"
#include "commands.h"
#include "prefs.h"
#include "menu.h"

/*
 * Local data
 */
static char *Selection = NULL;


/* DILLO MENU */

/*
 * Create a new browser window
 */
void a_Commands_new_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *)client_data;

   a_Interface_browser_window_new(bw->main_window->allocation.width,
                                  bw->main_window->allocation.height);
}

/*
 * Create and show the "Open file" dialog
 */
void a_Commands_openfile_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *) client_data;

   a_Interface_openfile_dialog(bw);
}

/*
 * Create and show the "Open Url" dialog window
 */
void a_Commands_openurl_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *) client_data;
   a_Interface_open_dialog(widget, bw);
}

/*
 * ?
 */
void a_Commands_prefs_callback(GtkWidget *widget, gpointer client_data)
{
}

/*
 * Close browser window, and exit dillo if it's the last one.
 */
void a_Commands_close_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *)client_data;
   gtk_widget_destroy(bw->main_window);
}

/*
 * Free memory and quit dillo
 */
void a_Commands_exit_callback(GtkWidget *widget, gpointer client_data)
{
   a_Interface_quit_all();
}


/* DOCUMENT MENU */

/*
 * Show current page's source code.
 */
void a_Commands_viewsource_callback (GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *)client_data;
   char *buf;
   gint size;
   GtkWidget *window, *box1, *button, *scrolled_window, *text;

   if (bw->viewsource_window)
      gtk_widget_destroy (bw->viewsource_window);

   /* -RL :: This code is adapted from testgtk. */
   if ( !bw->viewsource_window ) {
      window = bw->viewsource_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_wmclass(GTK_WINDOW(window), "view_source", "Dillo");
      gtk_widget_set_name (window, "text window");
      gtk_widget_set_usize (window, 500, 500);
      gtk_window_set_policy (GTK_WINDOW(window), TRUE, TRUE, FALSE);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
                          GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                          &bw->viewsource_window);

      gtk_window_set_title (GTK_WINDOW (window), "View Source");
      gtk_container_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);

      scrolled_window = gtk_scrolled_window_new (NULL, NULL);
      gtk_box_pack_start (GTK_BOX (box1), scrolled_window, TRUE, TRUE, 0);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_ALWAYS);
      gtk_widget_show (scrolled_window);

      text = gtk_text_new (NULL, NULL);
      gtk_text_set_editable (GTK_TEXT (text), FALSE);
      gtk_container_add (GTK_CONTAINER (scrolled_window), text);
      gtk_widget_show (text);

      gtk_text_freeze (GTK_TEXT (text));
      buf = a_Cache_url_read(a_History_get_url(NAV_TOP(bw)), &size);
      gtk_text_insert (GTK_TEXT (text), NULL, NULL, NULL, buf, size);
      gtk_text_thaw (GTK_TEXT (text));

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box1), button, FALSE, FALSE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
   }

   if (!GTK_WIDGET_VISIBLE (bw->viewsource_window))
      gtk_widget_show (bw->viewsource_window);
}

/*
 * ?
 */
void a_Commands_selectall_callback(GtkWidget *widget, gpointer client_data)
{
}

/*
 * Create and show the "Find Text" dialog window
 */
void a_Commands_findtext_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *) client_data;

   a_Interface_findtext_dialog( bw );
}

/*
 * Print the page!
 * ('cat page.html | html2ps | lpr -Pcool'   Why bother?  I think providing
 * such an option in a configurable way should cut it  --Jcid)
 */
void a_Commands_print_callback(GtkWidget *widget, gpointer client_data)
{
}


/* BROWSE MENU */

/*
 * Abort all active connections for this page
 * (Downloads MUST keep flowing)
 */
void a_Commands_stop_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = client_data;
   a_Nav_cancel_expect(bw);
   a_Interface_stop(bw);
   a_Interface_set_button_sens(bw);
   a_Interface_msg(bw, "Stopped");
}

/*
 *  Back to previous page
 */
void a_Commands_back_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *) client_data;

   a_Nav_back(bw);
}

/*
 * Second handler for button-press-event in the toolbar
 */
void a_Commands_navpress_callback(GtkWidget *widget,
                                  GdkEventButton *event,
                                  gpointer client_data)
{
   BrowserWindow *bw = client_data;

   switch (event->button) {
   case 1:
      /* Handled by the default toolbar button handler */
      break;
   case 2:
      /* Not used yet */
      break;
   case 3:
      if (widget == bw->back_button) {
         if (bw->menu_popup.over_back)
            gtk_widget_destroy(bw->menu_popup.over_back);
         bw->menu_popup.over_back = a_Menu_popup_history_new(bw, -1);
         gtk_menu_popup(GTK_MENU (bw->menu_popup.over_back), NULL, NULL,
                        NULL, NULL, event->button, event->time);

      } else if (widget == bw->forw_button) {
         if (bw->menu_popup.over_forw)
            gtk_widget_destroy(bw->menu_popup.over_forw);
         bw->menu_popup.over_forw = a_Menu_popup_history_new(bw, +1);
         gtk_menu_popup(GTK_MENU (bw->menu_popup.over_forw), NULL, NULL,
                        NULL, NULL, event->button, event->time);
      }
      break;
   }
}

/*
 * Second handler for button-press-event in history menus.
 */
void a_Commands_historypress_callback(GtkWidget *widget,
                                      GdkEventButton *event,
                                      gpointer client_data)
{
   BrowserWindow *bw = client_data;

   switch (event->button) {
   case 1:
      /* Open link in the same bw */
      a_Nav_jump_callback(widget, bw, 0);
      break;
   case 2:
      /* Open link in a new bw */
      a_Nav_jump_callback(widget, bw, 1);
      break;
   case 3:
      /* Not used */
      break;
   }
}

/*
 * Go to the next page in the history buffer
 */
void a_Commands_forw_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *) client_data;

   a_Nav_forw(bw);
}

/*
 * Start the reload process
 */
void a_Commands_reload_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *) client_data;

   a_Nav_reload(bw);
}

/*
 * Go home!
 */
void a_Commands_home_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *) client_data;

   a_Nav_home(bw);
}

/*
 * Bring up the save page dialog
 */
void a_Commands_save_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *) client_data;

   a_Interface_save_dialog(widget, bw);
}

/*
 * Bring up the save link dialog
 */
void a_Commands_save_link_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *) client_data;

   a_Interface_save_link_dialog(widget, bw);
}


/* BOOKMARKS MENU */

/*
 * Add a bookmark to the current bookmark widget.
 */
void a_Commands_addbm_callback(GtkWidget *widget, gpointer client_data)
{
   a_Bookmarks_add(widget, client_data);
}

/*
 * Show the bookmarks-file as rendered html
 */
void a_Commands_viewbm_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *) client_data;
   char *bmfile;
   DilloUrl *url;

   /* todo: add a TITLE tag in bookmarks.html */

   bmfile = a_Misc_prepend_user_home (".dillo/bookmarks.html");
   url = a_Url_new(bmfile, "file:/", 0, 0);
   a_Nav_push(bw, url);
   g_free(bmfile);
   a_Url_free(url);
}


/* HELP MENU */

/*
 * This one was intended as a link to help-info on the web site, but
 * currently points to the home page  --Jcid
 */
void a_Commands_helphome_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *) client_data;
   DilloUrl *url = a_Url_new(DILLO_HOME, NULL, 0, 0);

   a_Nav_push(bw, url);
   a_Url_free(url);
}

/*
 * ?
 */
void a_Commands_manual_callback(GtkWidget *widget, gpointer client_data)
{
   /* CP: Uncomment this when the feature is implemented. :)
    * BrowserWindow *bw = (BrowserWindow *)client_data;
    * a_Nav_push (bw, "file:/usr/local/man/man1/dillo.man | groff -man");
    */
}


/* RIGHT BUTTON POP-UP MENU */

/*
 * Open link in another browser-window
 */
void a_Commands_open_link_nw_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *)client_data;
   gint width, height;
   BrowserWindow *newbw;

   gdk_window_get_size (bw->main_window->window, &width, &height);
   newbw = a_Interface_browser_window_new(width, height);
   a_Nav_push(newbw, a_Menu_popup_get_url(bw));
}


/* SELECTION */

/*
 * Callback for Copy (when user selects a string).
 */
void a_Commands_set_selection_callback(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *)client_data;

   if (gtk_selection_owner_set(widget,GDK_SELECTION_PRIMARY,GDK_CURRENT_TIME)){
      /* --EG: Why would it fail ? */
      Selection = g_strdup(URL_STR(a_Menu_popup_get_url(bw)));
   }
}

/*
 * Callback for Paste (when another application wants the selection)
 */
void a_Commands_give_selection_callback(GtkWidget *widget,
        GtkSelectionData *data, guint info, guint time)
{
   gtk_selection_data_set(data, GDK_SELECTION_TYPE_STRING,
                          8, Selection, strlen(Selection));
}

/*
 * Clear selection
 */
gint a_Commands_clear_selection_callback(GtkWidget *widget,
                                         GdkEventSelection *event)
{
   g_free(Selection);
   Selection = NULL;

   /* here we should un-highlight the selected text */
   return TRUE;
}

