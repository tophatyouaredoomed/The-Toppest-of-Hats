/*
 * File: interface.c
 *
 * Copyright (C) 1997 Raph Levien <raph@acm.org>
 * Copyright (C) 1999 Sammy Mannaert <nstalkie@tvd.be>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#include "list.h"
#include "dillo.h"
#include "history.h"
#include "nav.h"
#include "IO/Url.h"
#include "IO/IO.h"
#include "interface.h"
#include "commands.h"
#include "menu.h"
#include "config.h"
#include "bookmark.h"
#include "prefs.h"
#include "url.h"

#include "dw_widget.h"
#include "dw_gtk_scrolled_window.h"
#include "dw_gtk_viewport.h"
#include "dw_gtk_statuslabel.h"
#include "dw_container.h"
#include "progressbar.h"

#include "pixmaps.h"
#include <gdk/gdkkeysyms.h>

#define DEBUG_LEVEL 0
#include "debug.h"


/*
 * Local Data
 */
/* BrowserWindow holds all the widgets (and perhaps more)
 * for each new_browser.*/
BrowserWindow **browser_window;
gint num_bw, num_bw_max;


/*
 * Initialize global data
 */
void a_Interface_init(void)
{
   num_bw = 0;
   num_bw_max = 16;
   browser_window = NULL;
}

/*
 * Stop all active connections in the browser window (except downloads)
 */
void a_Interface_stop(BrowserWindow *bw)
{
   DEBUG_MSG(3, "a_Interface_stop: hi!\n");

   /* Remove root clients */
   while ( bw->NumRootClients ) {
      a_Cache_stop_client(bw->RootClients[0]);
      a_List_remove(bw->RootClients, 0, bw->NumRootClients);
   }
   /* Remove image clients */
   while ( bw->NumImageClients ) {
      a_Cache_stop_client(bw->ImageClients[0]);
      a_List_remove(bw->ImageClients, 0, bw->NumImageClients);
   }
}

/*
 * Empty RootClients, ImageClients and PageUrls lists and
 * reset progress bar data.
 */
void a_Interface_clean(BrowserWindow *bw)
{
   g_return_if_fail ( bw != NULL );

   while ( bw->NumRootClients )
      a_List_remove(bw->RootClients, 0, bw->NumRootClients);

   while ( bw->NumImageClients )
      a_List_remove(bw->ImageClients, 0, bw->NumImageClients);

   while ( bw->NumPageUrls ) {
      a_Url_free(bw->PageUrls[0].Url);
      a_List_remove(bw->PageUrls, 0, bw->NumPageUrls);
   }

   /* Zero image-progressbar data */
   bw->NumImages = 0;
   bw->NumImagesGot = 0;
}

/*=== Browser Window Interface Updating =====================================*/
/*
 * Remove the cache-client from the bw list
 * (client can be a image or a html page)
 */
void a_Interface_remove_client(BrowserWindow *bw, gint ClientKey)
{
   gint i;
   gboolean Found = FALSE;

   for ( i = 0; !Found && i < bw->NumRootClients; ++i)
      if ( bw->RootClients[i] == ClientKey ) {
         a_List_remove(bw->RootClients, i, bw->NumRootClients);
         Found = TRUE;
      }

   for ( i = 0; !Found && i < bw->NumImageClients; ++i)
      if ( bw->ImageClients[i] == ClientKey ) {
         a_List_remove(bw->ImageClients, i, bw->NumImageClients);
         bw->NumImagesGot++;
         Found = TRUE;
      }

   a_Interface_set_button_sens(bw);
}

/*
 * Remove the cache-client from the bw list
 * (client can be a image or a html page)
 */
void a_Interface_close_client(BrowserWindow *bw, gint ClientKey)
{
   gchar numstr[32];

   a_Interface_remove_client(bw, ClientKey);

   /* --Progress bars stuff-- */
   sprintf(numstr, "%s%d of %d", PBAR_ISTR(prefs.panel_size == 1),
           bw->NumImagesGot, bw->NumImages);
   a_Progressbar_update(bw->imgprogress, numstr,
                        (bw->NumImagesGot == bw->NumImages) ? 0 : 1 );
}

/*
 * Set the sensitivity on back/forw buttons and menu entries.
 */
static gint Interface_sens_idle_func(BrowserWindow *bw)
{
   gboolean back_sensitive, forw_sensitive, stop_sensitive;

   /* Stop button */
   stop_sensitive = (bw->NumRootClients > 0);
   gtk_widget_set_sensitive(bw->stop_button, stop_sensitive);

   /* Back and Forward buttons */
   back_sensitive = bw->nav_stack_ptr > 0;
   gtk_widget_set_sensitive(bw->back_button, back_sensitive);
   forw_sensitive = (bw->nav_stack_ptr < bw->nav_stack_size - 1 &&
                     !bw->nav_expecting);
   gtk_widget_set_sensitive(bw->forw_button, forw_sensitive);

   bw->sens_idle_tag = 0;
   return FALSE;
}

/*
 * Set the sensitivity on back/forw buttons and menu entries.
 */
void a_Interface_set_button_sens(BrowserWindow *bw)
{
   if (bw->sens_idle_tag != 0)
      return;
   bw->sens_idle_tag = gtk_idle_add(
                          (GtkFunction)Interface_sens_idle_func, bw);
}

/*
 * Add a reference to the cache-client in the browser window's list.
 * This helps us keep track of which are active in the window so that it's
 * possible to abort them.
 * (Root: Flag, whether a Root URL or not)
 */
void a_Interface_add_client(BrowserWindow *bw, gint Key, gint Root)
{
   gint nc;
   char numstr[32];

   g_return_if_fail ( bw != NULL );

   if ( Root ) {
      nc = bw->NumRootClients;
      a_List_add(bw->RootClients, nc, sizeof(*bw->RootClients),
                 bw->MaxRootClients);
      bw->RootClients[nc] = Key;
      bw->NumRootClients++;
      a_Interface_set_button_sens(bw);
   } else {
      nc = bw->NumImageClients;
      a_List_add(bw->ImageClients, nc, sizeof(*bw->ImageClients),
                 bw->MaxImageClients);
      bw->ImageClients[nc] = Key;
      bw->NumImageClients++;
      bw->NumImages++;
      a_Interface_set_button_sens(bw);

      /* --Progress bar stuff-- */
      sprintf(numstr, "%s%d of %d", PBAR_ISTR(prefs.panel_size == 1),
              bw->NumImagesGot, bw->NumImages);
      a_Progressbar_update(bw->imgprogress, numstr, 1);
   }
}

/*
 * Add an URL to the browser window's list.
 * This helps us keep track of page requested URLs so that it's
 * possible to stop, abort and reload them.)
 *   Flags: Chosen from {BW_Root, BW_Image, BW_Download}
 */
void a_Interface_add_url(BrowserWindow *bw, const DilloUrl *Url, gint Flags)
{
   gint nu, i;
   gboolean found = FALSE;

   g_return_if_fail ( bw != NULL && Url != NULL );

   nu = bw->NumPageUrls;
   for ( i = 0; i < nu; i++ ) {
      if ( !a_Url_cmp(Url, bw->PageUrls[i].Url) ) {
         found = TRUE;
         break;
      }
   }
   if ( !found ) {
      a_List_add(bw->PageUrls, nu, sizeof(*bw->PageUrls), bw->MaxPageUrls);
      bw->PageUrls[nu].Url = a_Url_dup(Url);
      bw->PageUrls[nu].Flags = Flags;
      bw->NumPageUrls++;
   }

   /* test:
   g_print("Urls:\n");
   for (i = 0; i < bw->NumPageUrls; i++)
      g_print("%s\n", bw->PageUrls[i].Url);
   g_print("---\n");
   */
}

/*
 * Remove a single browser window. This includes all its open childs,
 * freeing all resources associated with them, and exiting gtk
 * if no browser windows are left.
 */
static gboolean Interface_quit(GtkWidget *widget, BrowserWindow *bw)
{
   gint i;

   /* stop/abort open connections. */
   a_Interface_stop(bw);

   g_slist_free(bw->PanelHandles);

   if (bw->open_dialog_window != NULL)
      gtk_widget_destroy(bw->open_dialog_window);
   if (bw->openfile_dialog_window != NULL)
      gtk_widget_destroy(bw->openfile_dialog_window);
   if (bw->quit_dialog_window != NULL)
      gtk_widget_destroy(bw->quit_dialog_window);
   if (bw->findtext_dialog_window != NULL)
      gtk_widget_destroy(bw->findtext_dialog_window);
   if (bw->question_dialog_window != NULL)
      gtk_widget_destroy(bw->question_dialog_window);

   if (bw->menu_popup.over_back)
      gtk_widget_destroy(bw->menu_popup.over_back);
   if (bw->menu_popup.over_forw)
      gtk_widget_destroy(bw->menu_popup.over_forw);
   if (bw->menu_popup.url)
      a_Url_free(bw->menu_popup.url);

   if (bw->sens_idle_tag)
      gtk_idle_remove(bw->sens_idle_tag);

   for (i = 0; i < num_bw; i++)
      if (browser_window[i] == bw) {
         browser_window[i] = browser_window[--num_bw];
         break;
      }

   g_free(bw->nav_stack);
   g_free(bw->RootClients);
   g_free(bw->ImageClients);

   for (i = 0; i < bw->NumPageUrls; i++)
      a_Url_free(bw->PageUrls[i].Url);
   g_free(bw->PageUrls);
   g_free(bw);

   if (num_bw == 0)
      gtk_main_quit();

   return FALSE;
}


/*=== Browser Window Interface Construction =================================*/
/*
 * Clear a text entry
 */
static void Interface_entry_clear(GtkEntry *entry)
{
   gtk_entry_set_text(GTK_ENTRY (entry), "");
   gtk_widget_grab_focus(GTK_WIDGET(entry));
}

/*
 * Create a pixmap and return it.
 */
static GtkWidget *Interface_pixmap_new(GtkWidget *parent, gchar **data)
{
   GtkWidget *pixmapwid;
   GdkPixmap *pixmap;
   GdkBitmap *mask;
   GtkStyle *style;

   style = gtk_widget_get_style(parent);

   pixmap = gdk_pixmap_create_from_xpm_d(parent->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL], data);

   pixmapwid = gtk_pixmap_new(pixmap, mask);

   return (pixmapwid);
}

/*
 * Set the bw's cursor type
 */
void a_Interface_set_cursor(BrowserWindow *bw, GdkCursorType CursorType)
{
   GdkCursor *cursor;

   if ( bw->CursorType != CursorType ) {
      cursor = gdk_cursor_new(CursorType);
      gdk_window_set_cursor(bw->docwin->window, cursor);
      gdk_cursor_destroy(cursor);
      bw->CursorType = CursorType;
   }
}

/*
 * Extract accelerator key from 'key_str' and
 * connect button's "clicked" event with {Alt | MOD2} + key
 */
static void Interface_set_button_accel(GtkButton *button,
                                       const char *key_str,
                                       GtkAccelGroup *accel_group)
{
   gint accel_key = tolower(key_str[1]);
   gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel_group,
                              accel_key, GDK_MOD1_MASK, GTK_ACCEL_LOCKED);
   gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel_group,
                              accel_key, GDK_MOD2_MASK, GTK_ACCEL_LOCKED);
}

/*
 * Create the "NEW" button and its location-entry.
 */
static GtkWidget *Interface_locbar_new(BrowserWindow *bw)
{
   GtkWidget *hbox, *toolbar;

   hbox = gtk_hbox_new(FALSE, 0);

   /* location entry */
   bw->location = gtk_entry_new();
   gtk_signal_connect(GTK_OBJECT(bw->location), "activate",
                      (GtkSignalFunc) a_Interface_entry_open_url, bw);
   toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
   gtk_toolbar_set_button_relief(GTK_TOOLBAR(toolbar), GTK_RELIEF_NONE);
   GTK_WIDGET_UNSET_FLAGS (toolbar, GTK_CAN_FOCUS);

   bw->clear_url_button = gtk_toolbar_append_item(
                             GTK_TOOLBAR(toolbar),
                             NULL, "Clear the url-box!", "Toolbar/New",
                             Interface_pixmap_new(bw->main_window, s_new_xpm),
                             NULL, NULL);
   gtk_signal_connect_object(GTK_OBJECT(bw->clear_url_button), "clicked",
                             GTK_SIGNAL_FUNC (Interface_entry_clear),
                             GTK_OBJECT(bw->location));

   gtk_box_pack_start(GTK_BOX(hbox), toolbar, FALSE, FALSE, 0);
   gtk_widget_show(toolbar);
   gtk_box_pack_start(GTK_BOX(hbox), bw->location, TRUE, TRUE, 0);
   gtk_widget_show(bw->location);
   gtk_widget_show(hbox);
   return (hbox);
}

/*
 * Create a new toolbar (Back, Forward, Home, Reload, Save and Stop buttons)
 */
static GtkWidget *Interface_toolbar_new(BrowserWindow *bw, gint label)
{
   GtkWidget *toolbar;
   gboolean s = prefs.small_icons;

   toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
   gtk_toolbar_set_button_relief(GTK_TOOLBAR(toolbar), GTK_RELIEF_NONE);

   /* back button */
   bw->back_button = gtk_toolbar_append_item(
                        GTK_TOOLBAR(toolbar),
                        label ? "Back" : NULL,
                        "Go to previous page", "Toolbar/Back",
                        Interface_pixmap_new(bw->main_window,
                                             s ? s_left_xpm : left_xpm),
                        (GtkSignalFunc) a_Commands_back_callback, bw);
   gtk_widget_set_sensitive(bw->back_button, FALSE);
   Interface_set_button_accel(GTK_BUTTON(bw->back_button), "_,",
                              bw->accel_group);
   gtk_signal_connect(GTK_OBJECT(bw->back_button), "button-press-event",
                      GTK_SIGNAL_FUNC(a_Commands_navpress_callback), bw);

   /* forward button */
   bw->forw_button = gtk_toolbar_append_item(
                        GTK_TOOLBAR(toolbar),
                        label ? "Forward" : NULL,
                        "Go to next page", "Toolbar/Forward",
                        Interface_pixmap_new(bw->main_window,
                                             s ? s_right_xpm : right_xpm),
                        (GtkSignalFunc) a_Commands_forw_callback, bw);
   gtk_widget_set_sensitive(bw->forw_button, FALSE);
   Interface_set_button_accel(GTK_BUTTON(bw->forw_button), "_.",
                              bw->accel_group);
   gtk_signal_connect(GTK_OBJECT(bw->forw_button), "button-press-event",
                      GTK_SIGNAL_FUNC(a_Commands_navpress_callback), bw);

   /* home button */
   bw->home_button = gtk_toolbar_append_item(
                        GTK_TOOLBAR(toolbar),
                        label ? "Home" : NULL,
                        "Go to the Home page", "Toolbar/Home",
                        Interface_pixmap_new(bw->main_window,
                                             s ? s_home_xpm : home_xpm),
                        (GtkSignalFunc) a_Commands_home_callback, bw);
   gtk_signal_connect(GTK_OBJECT(bw->home_button), "button-press-event",
                      GTK_SIGNAL_FUNC(a_Commands_navpress_callback), bw);

   /* reload button */
   bw->reload_button = gtk_toolbar_append_item(
                          GTK_TOOLBAR(toolbar),
                          label ? "Reload" : NULL,
                          "Reload this page", "Toolbar/Reload",
                          Interface_pixmap_new(bw->main_window,
                                               s ? s_reload_xpm : reload_xpm),
                          (GtkSignalFunc) a_Commands_reload_callback, bw);

   /* save button */
   bw->save_button = gtk_toolbar_append_item(
                        GTK_TOOLBAR(toolbar),
                        label ? "Save" : NULL,
                        "Save this page", "Toolbar/Save",
                        Interface_pixmap_new(bw->main_window,
                                             s ? s_save_xpm : save_xpm),
                        (GtkSignalFunc) a_Commands_save_callback, bw);
   /* stop button */
   bw->stop_button = gtk_toolbar_append_item(
                        GTK_TOOLBAR(toolbar),
                        label ? "Stop" : NULL,
                        "Stop the current transfer", "Toolbar/Stop",
                        Interface_pixmap_new(bw->main_window,
                                             s ? s_stop_xpm : stop_xpm),
                        (GtkSignalFunc) a_Commands_stop_callback, bw);
   gtk_widget_set_sensitive(bw->stop_button, FALSE);

   gtk_widget_show(toolbar);
   return toolbar;
}

/*
 * Create the progressbar's box
 */
static GtkWidget *Interface_progressbox_new(BrowserWindow *bw, gint vertical)
{
   GtkWidget *progbox;

   progbox = vertical ? gtk_vbox_new(FALSE, 0) : gtk_hbox_new(FALSE, 0);
   bw->progress_box = progbox;
   bw->imgprogress = a_Progressbar_new();
   bw->progress = a_Progressbar_new();
   gtk_box_pack_start(GTK_BOX(progbox), bw->imgprogress, TRUE, TRUE, 0);
   gtk_widget_show(bw->imgprogress);
   gtk_box_pack_start(GTK_BOX(progbox), bw->progress, TRUE, TRUE, 0);
   gtk_widget_show(bw->progress);
   return (progbox);
}

/*
 * Hide/Unhide this bw's control panels.
 */
void a_Interface_toggle_panel(BrowserWindow *bw)
{
   static gint hide = 1;

   if (hide) {
      g_slist_foreach(bw->PanelHandles, (GFunc)gtk_widget_hide, NULL);
      gtk_widget_hide(bw->status);
   } else {
      g_slist_foreach(bw->PanelHandles, (GFunc)gtk_widget_show, NULL);
      gtk_widget_show(bw->status);
   }
   hide = !(hide);
}

/*
 * Customize the appearance of the bw.
 */
static void Interface_browser_window_customize(BrowserWindow *bw)
{
   if ( !prefs.show_back )
      gtk_widget_hide(bw->back_button);
   if ( !prefs.show_forw )
      gtk_widget_hide(bw->forw_button);
   if ( !prefs.show_home )
      gtk_widget_hide(bw->home_button);
   if ( !prefs.show_reload )
      gtk_widget_hide(bw->reload_button);
   if ( !prefs.show_save )
      gtk_widget_hide(bw->save_button);
   if ( !prefs.show_stop )
      gtk_widget_hide(bw->stop_button);
   if ( !prefs.show_menubar )
      gtk_widget_hide(bw->menubar);
   if ( !prefs.show_clear_url)
      gtk_widget_hide(bw->clear_url_button);
   if ( !prefs.show_url )
      gtk_widget_hide(bw->location);
   if ( !prefs.show_progress_box )
      gtk_widget_hide(bw->progress_box);
}

/*
 * Handler for mouse-clicks that don't belong to the viewport.
 */
static int Interface_click_callback(BrowserWindow *bw, GdkEventButton *event)
{
   if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
      a_Interface_toggle_panel(bw);
// else  g_print("Click!\n");

   return FALSE;
}

/*
 * Create a new browser window and return it.
 * (the new window is stored in browser_window[])
 */
BrowserWindow *a_Interface_browser_window_new(gint width, gint height)
{
   GtkWidget *box1, *box2, *hbox,
             *progbox, *toolbar, *handlebox, *menubar, *locbox;
   BrowserWindow *bw;
   char buf[64];

   /* We use g_new0() to zero the memory */
   bw = g_new0(BrowserWindow, 1);

   a_List_add(browser_window, num_bw, sizeof(*browser_window), num_bw_max);
   browser_window[num_bw++] = bw;

   /* initialize nav_stack struct in browser_window struct */
   a_Nav_init(bw);

   bw->main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

   gtk_window_set_policy(GTK_WINDOW(bw->main_window), TRUE, TRUE, FALSE);
   gtk_signal_connect(GTK_OBJECT(bw->main_window), "delete_event",
                      GTK_SIGNAL_FUNC(gtk_object_destroy), bw);
   gtk_signal_connect(GTK_OBJECT(bw->main_window), "destroy",
                      GTK_SIGNAL_FUNC(Interface_quit), bw);
   gtk_container_border_width(GTK_CONTAINER(bw->main_window), 0);

   gtk_window_set_wmclass(GTK_WINDOW(bw->main_window), "dillo", "Dillo");

   /* -RL :: I must realize the window to see it correctly */
   gtk_widget_realize(bw->main_window);

   /* Create and attach an accel group to the main window */
   bw->accel_group = gtk_accel_group_new();
   gtk_window_add_accel_group(GTK_WINDOW(bw->main_window), bw->accel_group);

   /* set window title */
   g_snprintf(buf, 64, "Version %s", VERSION);
   a_Interface_set_page_title(bw, buf);

   box1 = gtk_vbox_new(FALSE, 0);

   /* setup the control panel */
   if (prefs.panel_size == 1) {
      handlebox = gtk_handle_box_new();
      bw->PanelHandles = g_slist_append(bw->PanelHandles, handlebox);
      hbox = gtk_hbox_new(FALSE, 0);
      /* Control Buttons */
      toolbar = Interface_toolbar_new(bw, 0);
      /* Menus */
      menubar = a_Menu_mainbar_new(bw, 1);
      /* Location entry */
      locbox = Interface_locbar_new(bw);
      /* progress bars */
      progbox = Interface_progressbox_new(bw, 0);

      gtk_box_pack_start(GTK_BOX(hbox), toolbar, FALSE, FALSE, 0);
      gtk_widget_show(toolbar);
      gtk_box_pack_start(GTK_BOX(hbox), menubar, FALSE, FALSE, 0);
      gtk_widget_show(menubar);
      gtk_box_pack_start(GTK_BOX(hbox), locbox, TRUE, TRUE, 0);
      gtk_widget_show(locbox);
      gtk_box_pack_start(GTK_BOX(hbox), progbox, FALSE, FALSE, 0);
      gtk_widget_show(progbox);
      gtk_container_add(GTK_CONTAINER(handlebox), hbox);
      gtk_widget_show(hbox);
      gtk_box_pack_start(GTK_BOX(box1), handlebox, FALSE, FALSE, 0);
      gtk_widget_show(handlebox);

   } else if (prefs.panel_size == 2) {
      handlebox = gtk_handle_box_new();
      bw->PanelHandles = g_slist_append(bw->PanelHandles, handlebox);
      hbox = gtk_hbox_new(FALSE, 0);
      menubar = a_Menu_mainbar_new(bw, 0);
      locbox = Interface_locbar_new(bw);
      gtk_box_pack_start(GTK_BOX(hbox), menubar, FALSE, FALSE, 0);
      gtk_widget_show(menubar);
      gtk_box_pack_start(GTK_BOX(hbox), locbox, TRUE, TRUE, 0);
      gtk_widget_show(locbox);
      gtk_container_add(GTK_CONTAINER(handlebox), hbox);
      gtk_widget_show(hbox);
      gtk_box_pack_start(GTK_BOX(box1), handlebox, FALSE, FALSE, 0);
      gtk_widget_show(handlebox);

      handlebox = gtk_handle_box_new();
      bw->PanelHandles = g_slist_append(bw->PanelHandles, handlebox);
      gtk_container_border_width(GTK_CONTAINER(handlebox), 4);
      hbox = gtk_hbox_new(FALSE, 0);
      toolbar = Interface_toolbar_new(bw, 1);
      progbox = Interface_progressbox_new(bw, 1);
      gtk_box_pack_start(GTK_BOX(hbox), toolbar, TRUE, TRUE, 0);
      gtk_widget_show(toolbar);
      gtk_box_pack_start(GTK_BOX(hbox), progbox, FALSE, FALSE, 0);
      gtk_widget_show(progbox);
      gtk_container_add(GTK_CONTAINER(handlebox), hbox);
      gtk_widget_show(hbox);
      gtk_box_pack_start(GTK_BOX(box1), handlebox, FALSE, FALSE, 0);
      gtk_widget_show(handlebox);

   } else {
      handlebox = gtk_handle_box_new();
      bw->PanelHandles = g_slist_append(bw->PanelHandles, handlebox);
      menubar = a_Menu_mainbar_new(bw, 0);
      gtk_container_add(GTK_CONTAINER(handlebox), menubar);
      gtk_widget_show(menubar);
      gtk_box_pack_start(GTK_BOX(box1), handlebox, FALSE, FALSE, 0);
      gtk_widget_show(handlebox);

      handlebox = gtk_handle_box_new();
      bw->PanelHandles = g_slist_append(bw->PanelHandles, handlebox);
      gtk_container_border_width(GTK_CONTAINER(handlebox), 4);
      hbox = gtk_hbox_new(FALSE, 0);
      toolbar = Interface_toolbar_new(bw, 1);
      progbox = Interface_progressbox_new(bw, 1);
      gtk_box_pack_start(GTK_BOX(hbox), toolbar, TRUE, TRUE, 0);
      gtk_widget_show(toolbar);
      gtk_box_pack_start(GTK_BOX(hbox), progbox, FALSE, FALSE, 0);
      gtk_widget_show(progbox);
      gtk_container_add(GTK_CONTAINER(handlebox), hbox);
      gtk_widget_show(hbox);
      gtk_box_pack_start(GTK_BOX(box1), handlebox, FALSE, FALSE, 0);
      gtk_widget_show(handlebox);

      handlebox = gtk_handle_box_new();
      bw->PanelHandles = g_slist_append(bw->PanelHandles, handlebox);
      locbox = Interface_locbar_new(bw);
      gtk_container_add(GTK_CONTAINER(handlebox), locbox);
      gtk_widget_show(locbox);
      gtk_box_pack_start(GTK_BOX(box1), handlebox, FALSE, FALSE, 0);
      gtk_widget_show(handlebox);
   }

   /* Add box1 */
   gtk_container_add(GTK_CONTAINER(bw->main_window), box1);

   /* Now the main document window */
   bw->docwin = a_Dw_gtk_scrolled_window_new();
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bw->docwin),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(box1), bw->docwin, TRUE, TRUE, 0);
   gtk_widget_show(bw->docwin);

   gtk_signal_connect_object_after(GTK_OBJECT(GTK_BIN(bw->docwin)->child),
                                   "button_press_event",
                                   GTK_SIGNAL_FUNC(Interface_click_callback),
                                   (gpointer)bw);

   gtk_widget_set_usize(bw->main_window, width, height);

   /* status widget */
   bw->status = a_Dw_gtk_statuslabel_new("");
   gtk_misc_set_alignment(GTK_MISC(bw->status), 0.0, 0.5);
   box2 = gtk_hbox_new(FALSE, 0);

   gtk_box_pack_start(GTK_BOX(box2), bw->status, TRUE, TRUE, 2);
   gtk_widget_show(bw->status);

   gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 2);
   gtk_widget_show(box2);

   gtk_widget_show(bw->main_window);
   gtk_widget_show(box1);

   /* initialize the rest of the bw's data. */
   bw->pagemarks_menuitem = NULL;
   bw->pagemarks_menu = NULL;
   bw->pagemarks_last = NULL;
   bw->menu_popup.over_page = a_Menu_popup_op_new(bw);
   bw->menu_popup.over_link = a_Menu_popup_ol_new(bw);
   bw->menu_popup.over_back = NULL;
   bw->menu_popup.over_forw = NULL;
   bw->menu_popup.url = NULL;
   bw->sens_idle_tag = 0;

   bw->CursorType = -1;

   bw->RootClients = NULL;
   bw->NumRootClients = 0;
   bw->MaxRootClients = 8;

   bw->ImageClients = NULL;
   bw->NumImageClients = 0;
   bw->MaxImageClients = 8;
   bw->NumImages = 0;
   bw->NumImagesGot = 0;

   bw->PageUrls = NULL;
   bw->NumPageUrls = 0;
   bw->MaxPageUrls = 8;

   bw->open_dialog_window = NULL;
   bw->open_dialog_entry = NULL;
   bw->openfile_dialog_window = NULL;
   bw->quit_dialog_window = NULL;
   bw->save_dialog_window = NULL;
   bw->save_link_dialog_window = NULL;
   bw->findtext_dialog_window = NULL;
   bw->findtext_dialog_entry = NULL;
   bw->findtext_data = NULL;
   bw->question_dialog_window = NULL;
   bw->question_dialog_data = NULL;
   bw->viewsource_window = NULL;

   /* now that the bw is made, let's customize it.. */
   Interface_browser_window_customize(bw);

   return bw;
}

/*
 * Set the title of the browser window to start with "Dillo: "
 * prepended to it.
 */
void a_Interface_set_page_title(BrowserWindow *bw, char *title)
{
   GString *buf;

   g_return_if_fail (bw != NULL && title != NULL);

   buf = g_string_new("");
   g_string_sprintfa(buf, "Dillo: %s", title);
   gtk_window_set_title(GTK_WINDOW(bw->main_window), buf->str);
   g_string_free(buf, TRUE);
}

/*
 * Set location entry's text
 */
void a_Interface_set_location_text(BrowserWindow *bw, char *text)
{
   gtk_entry_set_text(GTK_ENTRY(bw->location), text);
}

/*
 * Get location entry's text
 */
gchar *a_Interface_get_location_text(BrowserWindow *bw)
{
   return gtk_entry_get_text(GTK_ENTRY(bw->location));
}

/*
 * Reset images and text progress bars
 */
void a_Interface_reset_progress_bars(BrowserWindow *bw)
{
   a_Progressbar_update(bw->progress, "", 0);
   a_Progressbar_update(bw->imgprogress, "", 0);
}

/*
 * Set the status string on the bottom of the dillo window.
 */
void a_Interface_msg(BrowserWindow *bw, const char *format, ... )
{
static char msg[1024];
va_list argp;

   if ( bw ) {
      va_start(argp, format);
      vsnprintf(msg, 1024, format, argp);
      va_end(argp);
      gtk_label_set(GTK_LABEL(bw->status), msg);
      bw->status_is_link = 0;
   }
}

/*
 * Called from `destroy' callback in Interface_make_*_dialog
 */
static void Interface_destroy_window(GtkWidget *widget, GtkWidget **window)
{
   gtk_widget_destroy(*window);
   *window = NULL;
}


/*
 * Close and free every single browser_window (called at exit time)
 */
void a_Interface_quit_all(void)
{
   BrowserWindow **bws;
   gint i, n_bw;

   n_bw = num_bw;
   bws = g_malloc(sizeof(BrowserWindow *) * n_bw);

   /* we copy into a new list because destroying the main window can
    * modify the browser_window array. */
   for (i = 0; i < n_bw; i++)
      bws[i] = browser_window[i];

   for (i = 0; i < n_bw; i++)
      gtk_widget_destroy(bws[i]->main_window);

   g_free(bws);
}

/*
 * Make a dialog for choosing files (by calling
 * gtk_file_selection_*() calls)
 * This can be used for saving, opening, or whatever,
 * just set the correct callbacks
 */
static void
 Interface_make_choose_file_dialog(GtkWidget **DialogWindow,
                                   char *WmName, char *WmClass, char *WTitle,
                                   GtkSignalFunc B1CallBack, void *B1CbData)
{
   *DialogWindow = gtk_file_selection_new(WTitle);
   gtk_window_set_modal(GTK_WINDOW(*DialogWindow), FALSE);
   gtk_window_set_wmclass(GTK_WINDOW(*DialogWindow), WmName, WmClass);

   gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(*DialogWindow));
   gtk_signal_connect(
      GTK_OBJECT(*DialogWindow),
      "destroy", (GtkSignalFunc) Interface_destroy_window, DialogWindow);
   gtk_signal_connect(
      GTK_OBJECT(GTK_FILE_SELECTION(*DialogWindow)->ok_button),
      "clicked", (GtkSignalFunc) B1CallBack, B1CbData);
   gtk_signal_connect(
      GTK_OBJECT(GTK_FILE_SELECTION (*DialogWindow)->cancel_button),
      "clicked", (GtkSignalFunc) Interface_destroy_window, DialogWindow);
}

/*
 * Get the file URL from the widget and push it to the browser window.
 */
static void
 Interface_openfile_ok_callback(GtkWidget *widget, BrowserWindow *bw)
{
   char *fn;
   DilloUrl *url;
   GString *UrlStr = g_string_sized_new(1024);

   fn = gtk_file_selection_get_filename(
           GTK_FILE_SELECTION(bw->openfile_dialog_window));

   g_string_sprintf(UrlStr, "file:%s", fn);
   url = a_Url_new(UrlStr->str, NULL, 0, 0);
   a_Nav_push(bw, url);
   g_string_free(UrlStr, TRUE);
   a_Url_free(url);

   gtk_widget_destroy(bw->openfile_dialog_window);
}

/*
 * Open an URL specified in the location entry, or in the open URL dialog.
 * The URL is not sent "as is". It's fully parsed by a_Url_new().
 */
void a_Interface_entry_open_url(GtkWidget *widget, BrowserWindow *bw)
{
   gchar *text;
   DilloUrl *url;
   GtkEntry *entry;

   /* widget = { bw->location | bw->open_dialog_entry } */
   entry = GTK_ENTRY(widget);
   text = gtk_entry_get_text(entry);

   DEBUG_MSG(1, "entry_open_url %s\n", text);

   if ( text && *text ) {
      url = a_Url_new(text, NULL, 0, 0);
      if ( url ) {
         a_Nav_push(bw, url);
         a_Url_free(url);
      }
   }

   if (bw->open_dialog_window != NULL)
      gtk_widget_hide(bw->open_dialog_window);
}

/*
 * Create and show the "Open File" dialog
 */
void a_Interface_openfile_dialog(BrowserWindow *bw)
{
   if (!bw->openfile_dialog_window) {
      Interface_make_choose_file_dialog(
         &(bw->openfile_dialog_window),
         "openfile_dialog", "Dillo", "Dillo: Open File",
         (GtkSignalFunc) Interface_openfile_ok_callback, (void *)bw);
   }

   if (!GTK_WIDGET_VISIBLE(bw->openfile_dialog_window))
      gtk_widget_show(bw->openfile_dialog_window);
   else
      gdk_window_raise(bw->openfile_dialog_window->window);
}

/*
 * Make a dialog interface with three buttons and a text entry
 */
static void
 Interface_make_dialog(GtkWidget **DialogWindow, char *WmName, char *WmClass,
   char *WTitle, GtkWidget **DialogEntry, char *EntryStr,
   char *B1Label, GtkSignalFunc B1CallBack, void *B1CbData)
{
   GtkWidget *button, *box1, *box2, *entry;

   *DialogWindow = gtk_window_new(GTK_WINDOW_DIALOG);
   gtk_window_set_wmclass(GTK_WINDOW(*DialogWindow), WmName, WmClass);
   gtk_window_set_position(GTK_WINDOW(*DialogWindow), GTK_WIN_POS_CENTER);
   gtk_window_set_title(GTK_WINDOW(*DialogWindow), WTitle);
   gtk_signal_connect(GTK_OBJECT(*DialogWindow), "destroy",
                      (GtkSignalFunc) Interface_destroy_window, DialogWindow);

   gtk_container_border_width(GTK_CONTAINER(*DialogWindow), 5);

   box1 = gtk_vbox_new(FALSE, 5);
   gtk_container_add(GTK_CONTAINER(*DialogWindow), box1);
   gtk_widget_show(box1);

   entry = gtk_entry_new();
   GTK_WIDGET_SET_FLAGS(entry, GTK_HAS_FOCUS);
   gtk_widget_set_usize(entry, 250, 0);
   gtk_entry_set_text(GTK_ENTRY(entry), EntryStr);
   gtk_box_pack_start(GTK_BOX(box1), entry, FALSE, FALSE, 0);
   *DialogEntry = GTK_WIDGET(entry);
   gtk_widget_show(entry);

   gtk_signal_connect(GTK_OBJECT(entry), "activate", B1CallBack, B1CbData);

   box2 = gtk_hbox_new(TRUE, 5);
   gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 0);
   gtk_widget_show(box2);

   button = gtk_button_new_with_label(B1Label);
   gtk_signal_connect(GTK_OBJECT(button), "clicked", B1CallBack, B1CbData);
   GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
   gtk_box_pack_start(GTK_BOX(box2), button, FALSE, TRUE, 0);
   gtk_widget_grab_default(button);
   gtk_widget_show(button);
   gtk_signal_connect_object(GTK_OBJECT(entry), "focus_in_event",
                             (GtkSignalFunc) gtk_widget_grab_default,
                             GTK_OBJECT(button));

   button = gtk_button_new_with_label("Clear");
   gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                             (GtkSignalFunc) Interface_entry_clear,
                             GTK_OBJECT(entry));
   GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
   gtk_box_pack_start(GTK_BOX(box2), button, FALSE, TRUE, 0);
   gtk_widget_show(button);

   button = gtk_button_new_with_label("Cancel");
   gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                             (GtkSignalFunc) gtk_widget_destroy,
                             GTK_OBJECT(*DialogWindow));
   GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
   gtk_box_pack_start(GTK_BOX(box2), button, FALSE, TRUE, 0);
   gtk_widget_show(button);

   gtk_widget_grab_focus(entry);
}

/*
 * Make a question-dialog with a question, OK and Cancel.
 */
static void Interface_make_question_dialog(
        GtkWidget **DialogWindow, char *WmName, char *WmClass,
        char *WTitle, char *Question,
        GtkSignalFunc OkCallback, void *OkCbData,
        GtkSignalFunc CancelCallback, void *CancelCbData)
{
   GtkWidget *frame, *label, *button, *box1, *box2;

   *DialogWindow = gtk_window_new(GTK_WINDOW_DIALOG);
   gtk_window_set_wmclass(GTK_WINDOW(*DialogWindow), WmName, WmClass);
   gtk_window_set_title(GTK_WINDOW(*DialogWindow), WTitle);
   gtk_container_border_width(GTK_CONTAINER(*DialogWindow), 10);
   gtk_signal_connect(GTK_OBJECT(*DialogWindow), "destroy",
                      (GtkSignalFunc) Interface_destroy_window, DialogWindow);

   box1 = gtk_vbox_new(FALSE, 5);
   frame = gtk_frame_new(NULL);
   gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
   label = gtk_label_new(Question);
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_FILL);
   gtk_misc_set_padding(GTK_MISC(label), 20, 20);
   gtk_container_add(GTK_CONTAINER(frame), label);
   gtk_widget_show(label);
   gtk_widget_show(frame);
   gtk_box_pack_start(GTK_BOX(box1), frame, TRUE, TRUE, 0);

   box2 = gtk_hbox_new(TRUE, 5);
   button = gtk_button_new_with_label("OK");
   gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                             OkCallback, OkCbData);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
                      (GtkSignalFunc) Interface_destroy_window, DialogWindow);
   gtk_widget_show(button);
   gtk_box_pack_start(GTK_BOX(box2), button, FALSE, TRUE, 0);
   button = gtk_button_new_with_label("Cancel");
   gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                             CancelCallback, CancelCbData);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
                      (GtkSignalFunc) Interface_destroy_window, DialogWindow);
   gtk_widget_show(button);
   gtk_box_pack_start(GTK_BOX(box2), button, FALSE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 0);
   gtk_container_add(GTK_CONTAINER(*DialogWindow), box1);

   gtk_widget_show(box2);
   gtk_widget_show(box1);
   gtk_widget_grab_focus(button);
   gtk_widget_show(*DialogWindow);
}

/*
 * Create and show an [OK|Cancel] question dialog
 */
void a_Interface_question_dialog(
        BrowserWindow *bw, gchar *QuestionTxt,
        GtkSignalFunc OkCallback, void *OkCbData,
        GtkSignalFunc CancelCallback, void *CancelCbData)
{
   if (!bw->question_dialog_window) {
      Interface_make_question_dialog(&(bw->question_dialog_window),
         "question_dialog", "Dillo", "Dillo: Question", QuestionTxt,
         OkCallback, OkCbData, CancelCallback, CancelCbData);
   } else {
      gtk_widget_destroy(bw->question_dialog_window);
   }
}

/*
 * Create and show the open URL dialog
 */
void a_Interface_open_dialog(GtkWidget *widget, BrowserWindow *bw)
{
   if (!bw->open_dialog_window) {
      Interface_make_dialog(&(bw->open_dialog_window),
         "open_dialog", "Dillo", "Dillo: Open URL",
         &(bw->open_dialog_entry), "",
         "OK", (GtkSignalFunc) a_Interface_entry_open_url, (void *)bw);
      if (prefs.transient_dialogs)
         gtk_window_set_transient_for(GTK_WINDOW(bw->open_dialog_window),
                                      GTK_WINDOW(bw->main_window));
   }

   if (!GTK_WIDGET_VISIBLE(bw->open_dialog_window))
      gtk_widget_show(bw->open_dialog_window);
   else
      gdk_window_raise(bw->open_dialog_window->window);
}

/*
 * Receive data from the cache and save it to a local file
 */
static void Interface_save_callback(int Op, CacheClient_t *Client)
{
   DilloWeb *Web = Client->Web;
   gint Bytes;

   if ( Op ){
      struct stat st;

      fflush(Web->stream);
      fstat(fileno(Web->stream), &st);
      fclose(Web->stream);
      a_Interface_msg(Web->bw, "File saved (%d Bytes)", st.st_size);
   } else {
      if ( (Bytes = Client->BufSize - Web->SavedBytes) > 0 ) {
         Bytes = fwrite(Client->Buf + Web->SavedBytes, 1, Bytes, Web->stream);
         Web->SavedBytes += Bytes;
      }
   }
}

/*
 * Save current page to a local file
 */
static void Interface_file_save_url(GtkWidget *widget, BrowserWindow *bw)
{
   const char *url_str, *name;
   GtkFileSelection *choosefile;
   GtkEntry *entry_url;
   DilloUrl *url;
   FILE *out;

   choosefile = GTK_FILE_SELECTION(bw->save_dialog_window);
   entry_url = GTK_ENTRY(bw->location);
   name = gtk_file_selection_get_filename(choosefile);
   url_str = gtk_entry_get_text(entry_url);
   url = a_Url_new(url_str, NULL, 0, 0);

   if ( strlen(name) && (out = fopen(name, "w")) != NULL ) {
      DilloWeb *Web = a_Web_new(url);
      Web->bw = bw;
      Web->stream = out;
      Web->flags |= WEB_Download;
      /* todo: keep track of this client */
      a_Cache_open_url(Web, Interface_save_callback, Web);
   }
   a_Url_free(url);

   gtk_widget_destroy(bw->save_dialog_window);
}

/*
 * Save the link-URL to a local file
 */
static void Interface_file_save_link(GtkWidget *widget, BrowserWindow *bw)
{
   const gchar *name;
   const DilloUrl *url;
   FILE *out;

   name = gtk_file_selection_get_filename(
             GTK_FILE_SELECTION(bw->save_link_dialog_window));
   url  = a_Menu_popup_get_url(bw);

   if ( strlen(name) && (out = fopen(name, "w")) != NULL ) {
      DilloWeb *Web = a_Web_new(url);
      Web->bw = bw;
      Web->stream = out;
      Web->flags |= WEB_Download;
      /* todo: keep track of this client */
      a_Cache_open_url(Web, Interface_save_callback, Web);
   }

   gtk_widget_destroy(bw->save_link_dialog_window);
}

/*
 * Scan Url and return a local-filename suggestion for saving
 */
static char *Interface_make_save_name(const DilloUrl *url)
{
   gchar *FileName;

   if (URL_PATH(url) && (FileName = strrchr(URL_PATH(url), '/')))
      return g_strndup(++FileName, MIN(strlen(FileName), 64));
   return g_strdup("");
}

/*
 * Show the dialog interface for saving an URL
 */
void a_Interface_save_dialog(GtkWidget *widget, BrowserWindow *bw)
{
   gchar *SuggestedName;   /* Suggested save name */
   DilloUrl* url;

   if (!bw->save_dialog_window) {
      Interface_make_choose_file_dialog(
         &bw->save_dialog_window,
         "save_dialog", "Dillo", "Dillo: Save URL as File...",
         (GtkSignalFunc) Interface_file_save_url, (void *)bw );
   }
   url = a_Url_new(a_Interface_get_location_text(bw), NULL, 0, 0);
   SuggestedName = Interface_make_save_name(url);
   gtk_file_selection_set_filename(
      GTK_FILE_SELECTION(bw->save_dialog_window), SuggestedName);
   g_free(SuggestedName);
   a_Url_free(url);

   if (!GTK_WIDGET_VISIBLE(bw->save_dialog_window))
      gtk_widget_show(bw->save_dialog_window);
   else
      gdk_window_raise(bw->save_dialog_window->window);
}

/*
 * Show the dialog interface for saving a link
 */
void a_Interface_save_link_dialog(GtkWidget *widget, BrowserWindow *bw)
{
   char *SuggestedName;   /* Suggested save name */

   if (!bw->save_link_dialog_window) {
      Interface_make_choose_file_dialog(
         &bw->save_link_dialog_window,
         "save_link_dialog", "Dillo",
         "Dillo: Save link as File...",
         (GtkSignalFunc) Interface_file_save_link,
         (void *)bw);
   }
   SuggestedName = Interface_make_save_name(a_Menu_popup_get_url(bw));
   gtk_file_selection_set_filename(
      GTK_FILE_SELECTION(bw->save_link_dialog_window), SuggestedName);
   g_free(SuggestedName);

   if (!GTK_WIDGET_VISIBLE(bw->save_link_dialog_window))
      gtk_widget_show(bw->save_link_dialog_window);
   else
      gdk_window_raise(bw->save_link_dialog_window->window);
}

/*
 * Scroll to an occurence of a string in the open page
 */
static void Interface_entry_search(GtkWidget *widget, BrowserWindow* bw)
{
   DwWidget *Dw;
   char *string;

   Dw = a_Dw_gtk_scrolled_window_get_dw(GTK_DW_SCROLLED_WINDOW(bw->docwin));
   string = gtk_editable_get_chars(GTK_EDITABLE(bw->findtext_dialog_entry),
                                   0, -1);
   a_Dw_container_findtext(DW_CONTAINER (Dw),
                           &bw->findtext_data, NULL, string);
   g_free(string);
}

/*
 * Show the dialog interface for finding text in a page
 */
void a_Interface_findtext_dialog(BrowserWindow *bw)
{
   if (!bw->findtext_dialog_window) {
      Interface_make_dialog(&(bw->findtext_dialog_window),
         "findtext_dialog", "Dillo", "Dillo: Find text in page",
         &(bw->findtext_dialog_entry), "",
         "Find", (GtkSignalFunc) Interface_entry_search, (void *)bw);
      if (prefs.transient_dialogs)
         gtk_window_set_transient_for(GTK_WINDOW(bw->findtext_dialog_window),
                                      GTK_WINDOW(bw->main_window));
   }

   if (!GTK_WIDGET_VISIBLE(bw->findtext_dialog_window))
      gtk_widget_show(bw->findtext_dialog_window);
   else
      gdk_window_raise(bw->findtext_dialog_window->window);
}

/*
 * This signal callback adjusts the position of a menu.
 * It's useful for very long menus.
 */
void a_Interface_scroll_popup(GtkWidget *widget)
{
   /*
    * todo:
    *   1) Scrolling menues should rather be the task of Gtk+. This is
    *      a hack, and I don't know if it does not break anything.
    *   2) It could be improved, e.g. a timeout could be added for
    *      better mouse navigation.
    */
   int y, h, mx, my, sh;

   y = widget->allocation.y;
   h = widget->allocation.height;
   gdk_window_get_geometry (widget->parent->parent->window,
                            &mx, &my, NULL, NULL, NULL);
   sh = gdk_screen_height ();

   if (y + my < 0)
      gdk_window_move (widget->parent->parent->window, mx, - y + 1);
   else if (y + my > sh - h)
      gdk_window_move (widget->parent->parent->window, mx, sh - h - y - 1);
}

