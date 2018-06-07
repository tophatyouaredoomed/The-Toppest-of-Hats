/*
 * Dillo
 *
 * Some code copied from:
 * The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "history.h"
#include "nav.h"
#include "commands.h"
#include "dw_gtk_scrolled_window.h"
#include "dw_page.h"         /* for a_Dw_page_add_anchor */
#include "bookmark.h"
#include "interface.h"

/*
 * Forward declarations
 */
static void Menu_pagemarks_goto_pagemark(GtkWidget *widget,
                                         gpointer client_data);

/*
 * Make a new menu, insert it into the menu bar, and return it.
 */
static GtkWidget* Menu_new(GtkWidget *menubar, const char *name,
                           gboolean right_justify, BrowserWindow *bw)
{
   GtkWidget *menu;
   GtkWidget *menuitem;
   guint tmp_key;

   menu = gtk_menu_new();
   menuitem = gtk_menu_item_new_with_label((char *) name);
   tmp_key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(menuitem)->child), name);
   gtk_widget_add_accelerator(menuitem, "activate_item", bw->accel_group,
                              tmp_key, GDK_MOD1_MASK, 0);

   if ( right_justify )
      gtk_menu_item_right_justify(GTK_MENU_ITEM(menuitem));
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
   gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);
   gtk_widget_show(menuitem);
   gtk_menu_set_accel_group(GTK_MENU(menu), bw->accel_group);
   return menu;
}

/*
 * Add an item to a menu, including the name, an accelerator (not yet
 * implemented), and a callback function for activation.
 */
static GtkWidget *
 Menu_add (GtkWidget *menu, const char *name, const char *accel,
           BrowserWindow *bw,
           void (*callback) (GtkWidget *widget, void *data), void *data)
{
   GtkWidget *menuitem;
   GtkAccelGroup *menu_accels;
   GdkModifierType accel_mods;
   guint accel_key;
   guint tmp_key;

   menuitem = gtk_menu_item_new_with_label((char *) name);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menu_accels = gtk_menu_ensure_uline_accel_group(GTK_MENU(menu));
   tmp_key = gtk_label_parse_uline(GTK_LABEL(GTK_BIN(menuitem)->child), name);
   gtk_widget_add_accelerator(menuitem, "activate_item",
                              menu_accels, tmp_key, 0, 0);
   gtk_widget_show(menuitem);

   if (accel != NULL) {
      gtk_accelerator_parse(accel, &accel_key, &accel_mods);
      gtk_widget_add_accelerator(menuitem, "activate", bw->accel_group,
         accel_key, (guint)accel_mods, GTK_ACCEL_VISIBLE);
   }
   if (callback != NULL)
      gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                         (GtkSignalFunc) callback, data);
   return menuitem;
}

/*
 * Add a separator into the menu.
 */
static void Menu_sep(GtkWidget *menu)
{
   GtkWidget *widget;

   widget = gtk_menu_item_new();
   gtk_menu_append(GTK_MENU(menu), widget);
   gtk_widget_set_sensitive(widget, FALSE);
   gtk_widget_show(widget);
}

/*
 * Make up a new menubar for a main browser window. The accelerator table
 * is stored in bw->accel_group.
 * Currently does not deal with dynamic menu items (bookmarks and history).
 * CP: It *seems* to handle dynamic menu items...
 */
GtkWidget *a_Menu_mainbar_new(BrowserWindow *bw, gint tiny)
{
   GtkWidget *menubar;
   GtkWidget *file_menu;
   GtkWidget *bookmarks_menu;
   // GtkWidget *help_menu;

   bw->menubar = menubar = gtk_menu_bar_new();

   /* FILE MENU */
   file_menu = Menu_new(menubar, tiny ? "_F" : "_File", FALSE, bw);
   Menu_add(file_menu, "_New Browser", "<ctrl>N", bw,
            a_Commands_new_callback, bw);
   Menu_add(file_menu, "_Open File...", "<ctrl>O", bw,
            a_Commands_openfile_callback, bw);
   Menu_add(file_menu, "Open _URL...", "<ctrl>L", bw,
            a_Commands_openurl_callback, bw);
   /*
   Menu_add(file_menu, "_Preferences", "<ctrl>E", bw,
            a_Commands_prefs_callback, bw);
   */
   Menu_add(file_menu, "Close Window", "<ctrl>Q", bw,
            a_Commands_close_callback, bw);
   Menu_sep(file_menu);
   Menu_add(file_menu, "Exit Dillo", "<alt>Q", bw,
            a_Commands_exit_callback, bw);

   /* BOOKMARKS MENU */
   bookmarks_menu = Menu_new(menubar, tiny ? "_B" : "_Bookmarks", FALSE, bw);
   bw->bookmarks_menu = bookmarks_menu;
   Menu_add(bookmarks_menu, "_View Bookmarks", NULL, bw,
            a_Commands_viewbm_callback, bw);
   Menu_sep(bookmarks_menu);
   a_Bookmarks_fill_new_menu(bw);

   /* HELP MENU
   help_menu = Menu_new(menubar, "_Help", TRUE, bw);
   Menu_add(help_menu, "Dillo _Home", NULL, bw,
            a_Commands_helphome_callback, bw);
   Menu_add(help_menu, "Dillo _Manual", NULL, bw,
            a_Commands_manual_callback, bw);
   */
   return menubar;
}

/*
 * Make a new popup menu and return it.
 */
GtkWidget *a_Menu_popup_op_new(BrowserWindow *bw)
{
   GtkWidget *menu;

   menu = gtk_menu_new();
   Menu_sep(menu);
   Menu_add(menu, "     PAGE OPTIONS", NULL, bw, NULL, NULL);
   Menu_sep(menu);
   Menu_add(menu, "View page Source", NULL, bw,
            a_Commands_viewsource_callback, bw);

   Menu_add(menu, "Bookmark this page", NULL, bw,
            a_Commands_addbm_callback, bw);
   Menu_sep(menu);
   Menu_add(menu, "_Find Text", "<ctrl>F", bw,
            a_Commands_findtext_callback, bw);
   bw->pagemarks_menuitem = Menu_add(menu, "Jump to...", NULL, bw, NULL, bw);

   Menu_sep(menu);
   Menu_add(menu, "Save page As...", NULL, bw,
            a_Commands_save_callback, bw);

   return menu;
}

/*
 * Set the popup's DilloUrl
 */
void a_Menu_popup_set_url(BrowserWindow *bw, DilloUrl *url)
{
   if (bw->menu_popup.url)
      a_Url_free(bw->menu_popup.url);
   bw->menu_popup.url = a_Url_dup(url);
}

/*
 * Get the popup's DilloUrl
 */
DilloUrl *a_Menu_popup_get_url(BrowserWindow *bw)
{
   return bw->menu_popup.url;
}

/*
 * Put the url in the status bar when moving cursor over menuitem.
 */
void a_Menu_popup_history_select_callback(GtkWidget *widget, gpointer data)
{
   gint idx;
   BrowserWindow *bw = data;

   idx = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT (widget), "nav_idx"));
   if (idx >= 0 && idx < bw->nav_stack_size) {
      a_Interface_msg(bw, "%s", URL_STR(a_History_get_url(NAV_IDX(bw,idx))));
      bw->status_is_link = 1;
   }
}

/*
 * Remove the url in the status bar when removing cursor from menuitem.
 */
void a_Menu_popup_history_deselect_callback(GtkWidget *widget, gpointer data)
{
   BrowserWindow *bw = data;

   a_Interface_msg(bw, "");
}

/*
 * Make a new popup menu for history browsing
 * direction: {backward = -1, forward = 1}
 */
GtkWidget *a_Menu_popup_history_new(BrowserWindow *bw, gint direction)
{
   int i;
   GtkWidget *menu, *menu_item;
   GString *text = g_string_sized_new(128);

   menu = gtk_menu_new();
   Menu_sep(menu);
   if (direction>0) {
      Menu_add(menu, "     FOLLOWING PAGES     ", NULL, bw, NULL, NULL);
      i = bw->nav_stack_ptr + 1;
   } else {
      Menu_add(menu, "     PREVIOUS PAGES     ", NULL, bw, NULL, NULL);
      i = bw->nav_stack_ptr - 1;
   }
   Menu_sep(menu);

   for ( ; i >= 0 && i < bw->nav_stack_size ; i+=direction ) {
      if (a_History_get_title(NAV_IDX(bw,i))) {
         g_string_assign(text, a_History_get_title(NAV_IDX(bw,i)));
      } else {
         g_string_assign(text, URL_STR(a_History_get_url(NAV_IDX(bw,i))));
      }
      if ( text->len > 64 ) {
         g_string_truncate(text, 64);
         g_string_append(text, "...");
      }
      menu_item = Menu_add(menu, text->str, NULL, bw, NULL, NULL);
      /* attach the nav_stack index to the menu item */
      gtk_object_set_data(GTK_OBJECT (menu_item), "nav_idx",
                          GINT_TO_POINTER(i));

      gtk_signal_connect (
         GTK_OBJECT (menu_item), "select",
         GTK_SIGNAL_FUNC (a_Interface_scroll_popup), NULL);
      gtk_signal_connect (
         GTK_OBJECT (menu_item), "select",
         GTK_SIGNAL_FUNC (a_Menu_popup_history_select_callback), bw);
      gtk_signal_connect (
         GTK_OBJECT (menu_item), "deselect",
         GTK_SIGNAL_FUNC (a_Menu_popup_history_deselect_callback), bw);
      gtk_signal_connect (
         GTK_OBJECT (menu_item), "button-press-event",
         GTK_SIGNAL_FUNC (a_Commands_historypress_callback), bw);
   }

   g_string_free(text, TRUE);
   return menu;
}

/*
 * Make a new popup menu for when the mouse is over a link.
 */
GtkWidget *a_Menu_popup_ol_new(BrowserWindow *bw)
{
   GtkWidget *menu;
   GtkWidget *copy;

   menu = gtk_menu_new();
   Menu_sep(menu);
   Menu_add(menu, "       LINK OPTIONS", NULL, bw, NULL, NULL);
   Menu_sep(menu);
   Menu_add(menu, "Open Link in New Window", NULL, bw,
            a_Commands_open_link_nw_callback, bw);
   Menu_add(menu, "Add Bookmark for Link", NULL, bw,
            a_Commands_addbm_callback, bw);

   copy = Menu_add(menu, "Copy Link location", NULL, bw,
            a_Commands_set_selection_callback, bw);
   gtk_signal_connect(GTK_OBJECT(copy), "selection_clear_event",
      GTK_SIGNAL_FUNC(a_Commands_clear_selection_callback), NULL);
   gtk_selection_add_target(copy, GDK_SELECTION_PRIMARY,
      GDK_SELECTION_TYPE_STRING, 1);
   gtk_signal_connect(GTK_OBJECT(copy), "selection_get",
      GTK_SIGNAL_FUNC(a_Commands_give_selection_callback), NULL);

   Menu_sep(menu);
   Menu_add(menu, "Save Link As...", NULL, bw,
            a_Commands_save_link_callback, bw);
   return menu;
}

/*
 * Functions to manipulate Pagemarks menu.
 * Should this be in a separate "pagemark.c" ?
 */

/*
 * make the page scroll to the pagemark
 */
static void Menu_pagemarks_goto_pagemark(GtkWidget *widget,
                                         gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *) client_data;
   char anchor[32];

   sprintf (anchor, "#%ld", (long int)widget);
   a_Dw_gtk_scrolled_window_set_anchor(GTK_DW_SCROLLED_WINDOW(bw->docwin),
                                       anchor);
}

/*
 * Deallocate the memory used by a pagemarks menu and create a new one
 */
void a_Menu_pagemarks_new(BrowserWindow *bw)
{
   gtk_widget_set_sensitive(bw->pagemarks_menuitem, FALSE);

   if (bw->pagemarks_menu)
      gtk_widget_destroy(bw->pagemarks_menu);
   bw->pagemarks_menu = gtk_menu_new();
   bw->pagemarks_last = NULL;

   gtk_menu_item_set_submenu(GTK_MENU_ITEM(bw->pagemarks_menuitem),
                             bw->pagemarks_menu);

   /* todo: add a scroller so one can access  all menu entries,
    * even if there's a lot of pagemarks. */
   /* --EG: I haven't found a way to pass a scroller
    * as widget for the submenu */
}

/*
 * Add a new pagemark (opening H tag).
 * The text can be set only at the closing H tag.
 * level is the level of the heading (1-6).
 */
void a_Menu_pagemarks_add(BrowserWindow *bw, void *page, void *style,
                          gint level)
{
   char anchor[32], spaces[32], name[32];

   gtk_widget_set_sensitive(bw->pagemarks_menuitem, TRUE);
   sprintf(spaces, "%*s", 3 * (level - 1), "");
   bw->pagemarks_last = Menu_add(bw->pagemarks_menu, spaces, NULL, bw,
                                 Menu_pagemarks_goto_pagemark, bw);
   sprintf(anchor, "#%ld", (glong)(bw->pagemarks_last));
   a_Dw_page_add_anchor(page, anchor, style);
   gtk_signal_connect (GTK_OBJECT (bw->pagemarks_last), "select",
                       GTK_SIGNAL_FUNC (a_Interface_scroll_popup), NULL);
   sprintf(name, "dilloHeading%d", level);
   gtk_widget_set_name(bw->pagemarks_last, name);
}

/*
 * Set the text for the last created pagemark.
 */
void a_Menu_pagemarks_set_text(BrowserWindow *bw, const char *str)
{
   GString *text;
   GtkWidget *child;

   /* Avoid problems with lonely closing tags and nested headings */
   if ( bw->pagemarks_last &&
       (child = GTK_BIN (bw->pagemarks_last)->child) &&
       GTK_IS_LABEL (child) ) {
      text = g_string_new(GTK_LABEL(child)->label);
      g_string_append(text, str);
      if ( text->len > 64 ) {
         g_string_truncate(text, 64);
         g_string_append(text, "...");
      }
      gtk_label_set_text(GTK_LABEL (child), text->str);
      g_string_free(text, 1);
      bw->pagemarks_last = NULL;
   }
}
