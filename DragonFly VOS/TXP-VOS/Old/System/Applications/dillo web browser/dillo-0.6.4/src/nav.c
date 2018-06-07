/*
 * File: nav.c
 *
 * Copyright (C) 1999 James McCollough <jamesm@gtwn.net>
 * Copyright (C) 2000 Jorge Arellano Cid <jcid@inf.utfsm.cl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/* Support for a navigation stack */

#include <stdio.h>
#include <gtk/gtk.h>
#include "list.h"
#include "nav.h"
#include "history.h"
#include "web.h"
#include "menu.h"
#include "interface.h"
#include "dw_widget.h"
#include "dw_gtk_scrolled_window.h"
#include "prefs.h"
#include "commands.h"

//#define DEBUG_LEVEL 3
#include "debug.h"

/*
 * Forward declarations
 */
static void Nav_reload(BrowserWindow *bw);

/*
 * Initialize the navigation structure with safe values
 */
void a_Nav_init(BrowserWindow *bw)
{
   bw->nav_stack_size = 0;
   bw->nav_stack_size_max = 16;
   bw->nav_stack = NULL;
   bw->nav_stack_ptr = -1;
   bw->nav_expecting = FALSE;
   bw->nav_expect_url = NULL;
}

/*
 * Create a DilloWeb structure for 'url' and ask the cache to send it back.
 *  - Also set a few things related to the browser window.
 * This function requests the page's root-URL; images and related stuff
 * are fetched directly by the HTML module.
 */
static void Nav_open_url(BrowserWindow *bw, const DilloUrl *url, gint offset)
{
   DilloUrl *old_url = NULL;
   gboolean MustLoad;
   gint ClientKey;
   DilloWeb *Web;
   gboolean ForceReload = (URL_FLAGS(url) & URL_E2EReload);

   /* Get the url of the current page */
   if ( bw->nav_stack_ptr != -1 )
      old_url = a_History_get_url(NAV_TOP(bw));

   /* Record current scrolling position
    * (the strcmp check is necessary because of redirections) */
   if (old_url && 
       !strcmp(URL_STR(old_url), a_Interface_get_location_text(bw))) {
      old_url->scrolling_position =
        a_Dw_gtk_scrolled_window_get_scrolling_position(
           GTK_DW_SCROLLED_WINDOW(bw->docwin));
   }

   /* Update navigation-stack-pointer (offset may be zero) */
   bw->nav_stack_ptr += offset;

   /* Page must be reloaded, if old and new url (without anchor) differ */
   MustLoad = ForceReload || !(old_url && !a_Url_cmp(old_url, url));
   if ( MustLoad ) {
      a_Interface_stop(bw);
      a_Interface_clean(bw);

      g_print("Nav_open_url: Url=>%s<\n", URL_STR(url));

      a_Menu_pagemarks_new(bw);

      Web = a_Web_new(url);
      Web->bw = bw;
      Web->flags |= WEB_RootUrl;
      if ((ClientKey = a_Cache_open_url(Web, NULL, NULL)) != 0) {
         a_Interface_add_client(bw, ClientKey, 1);
         a_Interface_add_url(bw, url, WEB_RootUrl);
      }
      a_Interface_set_cursor(bw, GDK_LEFT_PTR);
   }

   /* Jump to #anchor position */
   if (URL_FRAGMENT(url)) {
      /* todo: push on stack */
      a_Dw_gtk_scrolled_window_set_anchor(
         GTK_DW_SCROLLED_WINDOW(bw->docwin), URL_FRAGMENT(url));
   }
}

/*
 * Cancel the last expected url if present. The responsibility
 * for actually aborting the data stream remains with the caller.
 */
void a_Nav_cancel_expect(BrowserWindow *bw)
{
   if (bw->nav_expecting) {
      if (bw->nav_expect_url) {
         a_Url_free(bw->nav_expect_url);
         bw->nav_expect_url = NULL;
      }
      bw->nav_expecting = FALSE;
   }
}

/*
 * We have an answer! Set things accordingly.
 */
void a_Nav_expect_done(BrowserWindow *bw, DwWidget *dw)
{
   gint idx;
   DilloUrl *url;

   g_return_if_fail(bw != NULL);
   g_return_if_fail(bw->nav_expecting);

   ++bw->nav_stack_ptr;
   if ( bw->nav_stack_ptr == bw->nav_stack_size ) {
      a_List_add(bw->nav_stack, bw->nav_stack_size, sizeof(*bw->nav_stack),
                 bw->nav_stack_size_max);
      ++bw->nav_stack_size;
   } else {
      bw->nav_stack_size = bw->nav_stack_ptr + 1;
   }

   url = bw->nav_expect_url;
   /* unset E2EReload before adding this url to history */
   a_Url_set_flags(url, URL_FLAGS(url) & ~URL_E2EReload);

   idx = a_History_add_url(url);
   bw->nav_stack[bw->nav_stack_ptr] = idx;

   a_Url_free(url);
   bw->nav_expect_url = NULL;
   bw->nav_expecting = FALSE;

   a_Interface_set_button_sens(bw);
}

/*
 * Remove top-URL from the navigation stack.
 * (Used to remove URLs that force redirection)
 */
void a_Nav_remove_top_url(BrowserWindow *bw)
{
   g_return_if_fail (bw != NULL);

   /* Deallocate the URL a the top of the stack */
   if ( bw->nav_expecting == FALSE && bw->nav_stack_size > 0 ) {
      if ( bw->nav_stack_ptr == --bw->nav_stack_size )
         --bw->nav_stack_ptr;
   }
}

/*
 * Make 'url' the current browsed page (upon data arrival)
 * - Set bw to expect the URL data
 * - Ask the cache to feed back the requested URL (via Nav_open_url)
 */
void a_Nav_push(BrowserWindow *bw, const DilloUrl *url)
{
   g_return_if_fail (bw != NULL);

   if (bw->nav_expecting && a_Url_cmp(bw->nav_expect_url, url) == 0 &&
       URL_STRCAMP_EQ(URL_FRAGMENT(bw->nav_expect_url), URL_FRAGMENT(url))) {
      /* we're already expecting that url (most probably a double-click) */
      return;
   }
   a_Nav_cancel_expect(bw);
   bw->nav_expect_url = a_Url_dup(url);
   bw->nav_expecting = TRUE;
   Nav_open_url(bw, url, 0);
}

/*
 * Wraps a_Nav_push to match 'DwPage->link' function type
 */
void a_Nav_vpush(void *vbw, const DilloUrl *url)
{
   a_Nav_push(vbw, url);
}

/*
 * Send the browser back to previous page
 */
void a_Nav_back(BrowserWindow *bw)
{
   gint idx = bw->nav_stack_ptr;

   a_Nav_cancel_expect(bw);
   if ( --idx >= 0 ){
      a_Interface_msg(bw, "");
      Nav_open_url(bw, a_History_get_url(NAV_IDX(bw,idx)), -1);
   }
}

/*
 * Send the browser to next page in the history list
 */
void a_Nav_forw(BrowserWindow *bw)
{
   gint idx = bw->nav_stack_ptr;

   a_Nav_cancel_expect(bw);
   if (++idx < bw->nav_stack_size) {
      a_Interface_msg(bw, "");
      Nav_open_url(bw, a_History_get_url(NAV_IDX(bw,idx)), +1);
   }
}

/*
 * Redirect the browser to the HOME page!
 */
void a_Nav_home(BrowserWindow *bw)
{
   a_Nav_push(bw, prefs.home);
}

/*
 * Jump to an URL within the stack history
 * NewBw: {0 = same window, 1 = new window}
 */
void a_Nav_jump_callback(GtkWidget *widget, gpointer client_data, gint NewBw)
{
   int idx;
   BrowserWindow *bw = client_data;

   idx = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT (widget), "nav_idx"));
   if (idx >= 0 && idx < bw->nav_stack_size) {
      if (NewBw == 1) {
         a_Menu_popup_set_url(bw, a_History_get_url(NAV_IDX(bw,idx)));
         a_Commands_open_link_nw_callback(widget, bw);
      } else {
         Nav_open_url(bw, a_History_get_url(NAV_IDX(bw,idx)),
                      idx - bw->nav_stack_ptr);
      }
   }
}

/*
 * Callback for reload confirmation
 */
static void Nav_reload_confirmed(BrowserWindow *bw)
{
   DEBUG_MSG(3, "Nav_reload_confirmed\n");
   if ( bw->nav_stack_size &&
        bw->question_dialog_data == a_History_get_url(NAV_TOP(bw)) ) {
      // A genuine confirmation!
      DEBUG_MSG(3, "Nav_reload_confirmed test: OK\n");
      bw->question_dialog_data = NULL;
      Nav_reload(bw);
   }
}

/*
 * Callback for reload refusal
 */
static void Nav_reload_refused(BrowserWindow *bw)
{
   DEBUG_MSG(3, "Nav_reload_refused\n");
   bw->question_dialog_data = NULL;
}

/*
 * This one does a_Nav_reload's job!
 */
static void Nav_reload(BrowserWindow *bw)
{
   DilloUrl *url, *ReqURL;

   a_Nav_cancel_expect(bw);
   if ( bw->nav_stack_size ){
      url = a_History_get_url(NAV_TOP(bw));
      ReqURL = a_Url_dup(a_History_get_url(NAV_TOP(bw)));
      /* Let's make reload be end-to-end */
      a_Url_set_flags(ReqURL, URL_FLAGS(ReqURL) | URL_E2EReload);
      Nav_open_url(bw, ReqURL, 0);
      a_Url_free(ReqURL);
   }
}

/*
 * Implement the RELOAD button functionality.
 * (We haven't defined it yet ;)
 */
void a_Nav_reload(BrowserWindow *bw)
{
   DilloUrl *url;

   a_Nav_cancel_expect(bw);
   if ( bw->nav_stack_size ){
      url = a_History_get_url(NAV_TOP(bw));
      if (URL_FLAGS(url) & URL_Post) {
         /* Attempt to repost data, let's confirm... */
         bw->question_dialog_data = (gpointer)url;
         a_Interface_question_dialog(bw, "Repost form data?",
                                     Nav_reload_confirmed, bw,
                                     Nav_reload_refused, bw);

      } else {
         Nav_reload(bw);
      }
   }
}

