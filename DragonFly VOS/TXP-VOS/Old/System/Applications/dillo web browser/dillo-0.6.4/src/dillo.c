/*
 * Dillo web browser
 *
 * Copyright 1997 Raph Levien <raph@acm.org>
 * Copyright 1999, 2000, 2001 Jorge Arellano Cid <jcid@inf.utfsm.cl>
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
#include <gtk/gtk.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "dillo.h"
#include "misc.h"
#include "dw_gtk_statuslabel.h"
#include "nav.h"
#include "history.h"
#include "bookmark.h"
#include "dicache.h"
#include "dns.h"
#include "IO/Url.h"
#include "IO/mime.h"
#include "prefs.h"
#include "interface.h"
#include "dw.h"
#include "dw_widget.h"


/*
 * Forward declarations
 */
static void Dillo_check_home_dir(char *dir);



/*
 * ******************************** MAIN *************************************
 */
gint main(int argc, char *argv[])
{
   gchar *file;
   DilloUrl *start_url;
   BrowserWindow *bw;

   /* This lets threads in the file module end peacefully when aborted
    * todo: implement a cleaner mechanism (in file.c) */
   signal(SIGPIPE, SIG_IGN);

   g_print("Setting locale to %s\n", gtk_set_locale());
   gtk_init(&argc, &argv);
   gdk_rgb_init();

   /* check that ~/.dillo exists, create it if it doesn't */
   file = a_Misc_prepend_user_home(".dillo");
   Dillo_check_home_dir(file);
   g_free(file);

   a_Prefs_init();
   a_Dns_init();
   a_Http_init();
   a_Mime_init();
   a_Dicache_init();
   a_Interface_init();
   a_Dw_init();

   /* a_Nav_init() has been moved into this call because it needs to be
    * initialized with the new browser_window structure */
   bw = a_Interface_browser_window_new(prefs.width, prefs.height);

   a_Bookmarks_init();

   /* Send dillo startup screen */
   start_url = a_Url_new("splash", "about:", 0, 0);
   a_Nav_push(bw, start_url);
   a_Url_free(start_url);

   if (argc == 2) {
      if (access(argv[1], F_OK) == 0) {
         GString *UrlStr = g_string_sized_new(128);

         if (argv[1][0] == '/') {
            g_string_sprintf(UrlStr, "file:%s", argv[1]);
         } else {
            g_string_sprintf(UrlStr, "file:%s", g_get_current_dir());
            if (UrlStr->str[UrlStr->len - 1] != '/')
               g_string_append(UrlStr, "/");
            g_string_append(UrlStr, argv[1]);
         }
         start_url = a_Url_new(UrlStr->str, NULL, 0, 0);
         g_string_free(UrlStr, TRUE);
      } else {
         start_url = a_Url_new(argv[1], NULL, 0, 0);
      }
      a_Nav_push(bw, start_url);
      a_Url_free(start_url);
   }


   gtk_main();

   /*
    * Memory deallocating routines
    * (This can be left to the OS, but we'll do it, with a view to test
    *  and fix our memory management)
    */
   a_Cache_freeall();
   a_Dicache_freeall();
   a_Http_freeall();
   a_Dns_freeall();
   a_Prefs_freeall();
   a_Dw_freeall();
   a_History_free();

   printf("Dillo: normal exit!\n");
   return 0;
}

/*
 * Checks if '~/.dillo' directory exists.
 * If not, attempt to create it.
 */
static void Dillo_check_home_dir(char *dir)
{
   struct stat st;

   if ( stat(dir, &st) == -1 ) {
      if ( errno == ENOENT && mkdir(dir, 0700) < 0 ) {
         g_print("Dillo: error creating directory %s: %s\n",
                 dir, strerror(errno));
      } else
         g_print("Dillo: error reading %s: %s\n", dir, strerror(errno));
   }
}

