
/* cruelty :) */

/* Copyright (C) 1997 Ian Main
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

#include <gtk/gtk.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "interface.h"
#include "dillo.h"
#include "nav.h"
#include "misc.h"
#include "history.h"
#include "menu.h"

#define LOAD_BOOKMARKS 1
#define SAVE_BOOKMARK 2
#define CLOSE_BOOKMARKS 3

/* this #define will cut page title if > 39 chars */
#define TITLE39

/* double quote */
#define D_QUOTE 0x22

/* Data types */

typedef struct _Bookmark Bookmark;
typedef struct _CallbackInfo CallbackInfo;

struct _Bookmark {
   char *title;
   DilloUrl *url;
   GtkWidget *menuitem;
};

struct _CallbackInfo {
   BrowserWindow *bw;
   guint index;
};

/*
 * Forward declarations
 */
static void
   Bookmarks_load_to_menu(FILE *fp),
   Bookmarks_file_op(gint operation, const char *title, const DilloUrl *url),
   Bookmarks_save_to_file(FILE *fp, const char *title, const DilloUrl *url);
static char *
   Bookmarks_search_line(char *line, char *start_text, char *end_text);

/*
 * Local data
 */
static Bookmark *bookmarks = NULL;
static gint num_bookmarks = 0;
static gint num_bookmarks_max = 16;


/*
 * Allocate memory and load the bookmarks list
 */
void a_Bookmarks_init(void)
{
   gchar *file;

   /* Here we load and set the bookmarks */
   file = a_Misc_prepend_user_home(".dillo/bookmarks.html");
   Bookmarks_file_op(LOAD_BOOKMARKS, file, NULL);
   g_free(file);
}

/*
 * ?
 */
static void Bookmarks_goto_bookmark(GtkWidget *widget, CallbackInfo *CbInfo)
{
   if (CbInfo->index >= num_bookmarks) {
      g_warning("bookmark not found!\n");
      return;
   }
   a_Nav_push(CbInfo->bw, bookmarks[CbInfo->index].url);
}

/*
 * Add a bookmark to the bookmarks menu of a particular browser window
 */
static void Bookmarks_add_to_menu(BrowserWindow *bw, GtkWidget *menuitem,
                                  guint index)
{
   CallbackInfo *CbInfo;

   gtk_menu_append(GTK_MENU(bw->bookmarks_menu), menuitem);

   CbInfo = g_new(CallbackInfo, 1);
   CbInfo->bw = bw;
   CbInfo->index = index;

   /* accelerator goes here */
   gtk_signal_connect(GTK_OBJECT (menuitem), "activate",
                      (GtkSignalFunc)Bookmarks_goto_bookmark, CbInfo);
}

/*
 * ?
 */
static GtkWidget *Bookmarks_insert(const char *title, const DilloUrl *url)
{
   GtkWidget *menuitem;

   menuitem = gtk_menu_item_new_with_label(title ? title : URL_STR(url));
   gtk_widget_show(menuitem);

   a_List_add(bookmarks, num_bookmarks, sizeof(*bookmarks), num_bookmarks_max);
   bookmarks[num_bookmarks].title = g_strdup(title ? title : URL_STR(url));
   bookmarks[num_bookmarks].url = a_Url_dup(url);
   bookmarks[num_bookmarks].menuitem = menuitem;
   num_bookmarks++;
   return menuitem;
}

/*
 * Add the new bookmark to bookmarks menu of _all_ browser windows and then
 * write the new bookmark to file
 */
void a_Bookmarks_add(GtkWidget *widget, gpointer client_data)
{
   BrowserWindow *bw = (BrowserWindow *)client_data;
   gint i;
#ifdef TITLE39
   gboolean allocated = FALSE;
#endif
   gchar *title;
   DilloUrl *url;
   GtkWidget *menuitem;

   url = a_Menu_popup_get_url(bw);
   g_return_if_fail(url != NULL);

   /* if the page has no title, we'll use the url string */
   title = (gchar *) a_History_get_title_by_url(url, 1);

#ifdef TITLE39
   if (strlen (title) > 39) {
      char buf1[20];
      char buf2[20];

      memcpy (buf1, title, 18);
      buf1[18] = '\0';
      strcpy (buf2, title + strlen (title) - 18);
      buf2[18] = '\0';
      title = g_strconcat (buf1, "...", buf2, NULL);
      allocated = TRUE;
   }
#endif

   /* add bookmark to bookmarks menu of _all_ browser windows. */
   menuitem = Bookmarks_insert(title, url);
   Bookmarks_add_to_menu(browser_window[0], menuitem, num_bookmarks-1);
   for (i = 1; i < num_bw; i++) {
      menuitem= gtk_menu_item_new_with_label(bookmarks[num_bookmarks-1].title);
      gtk_widget_show(menuitem);
      Bookmarks_add_to_menu(browser_window[i], menuitem, num_bookmarks-1);
   }

   /* write bookmark to file */
   Bookmarks_file_op(SAVE_BOOKMARK, title, url);

#ifdef TITLE39
   if (allocated)
      g_free (title);
#endif
}

/*
 * Never called (the file remains open all the time)  --Jcid
 */
G_GNUC_UNUSED static void Bookmarks_close(void)
{
   Bookmarks_file_op(CLOSE_BOOKMARKS, NULL, NULL);
}

/*
 * Performs operations on the bookmark file..
 * for first call, title is the filename
 */
static void
 Bookmarks_file_op(gint operation, const char *title, const DilloUrl *url)
{
   static FILE *fp;
   static gint initialized = 0;

   if (!initialized) {
      if (operation == LOAD_BOOKMARKS) {
         if ((fp = fopen(title, "a+")) == NULL)
            g_print("dillo: opening bookmark file %s: %s\n",
                    title, strerror(errno));
         else
            initialized = 1;
      } else
         g_print("Error: invalid call to Bookmarks_file_op.\n");
   }

   g_return_if_fail( initialized );

   switch (operation) {
   case LOAD_BOOKMARKS:
      Bookmarks_load_to_menu(fp);
      break;

   case SAVE_BOOKMARK:
      Bookmarks_save_to_file(fp, title, url);
      break;

   case CLOSE_BOOKMARKS:
      fclose(fp);
      break;

   default:
      break;
   }
}

/*
 * Save bookmarks to ~/.dillo/bookmarks.html
 */
static void
 Bookmarks_save_to_file(FILE *fp, const char *title, const DilloUrl *url)
{
   /* if there is no title, use url as title */
   /* CP: Adding <li>...</li> in prep for future structuring of file */
   fseek(fp, 0L, SEEK_END);
   fprintf(fp, "<li><A HREF=%c%s%c>%s</A></li>\n", D_QUOTE, URL_STR(url),
           D_QUOTE, title ? title : URL_STR(url));
   fflush(fp);
}

/*
 * Load bookmarks
 */
static void Bookmarks_load_to_menu(FILE *fp)
{
   gchar *title, *url_str;
   DilloUrl *url;
   char buf[4096];
   gint i = 0;
   GtkWidget *menuitem;

   rewind(fp);

   g_print("Loading bookmarks...\n");
   while (1) {
      /* Read a whole line from the file */
      if ((fgets(buf, 4096, fp)) == NULL)
         break;

      /* get url from line */
      if ( !(url_str = Bookmarks_search_line(buf, "=\"", "\">")) )
         continue;

      /* get title from line */
      if ( !(title = Bookmarks_search_line(buf, "\">", "</")) ){
         g_free(url_str);
         continue;
      }

      url = a_Url_new(url_str, NULL, 0, 0);
      menuitem = Bookmarks_insert(title, url);
      /* for (i = 0; i < num_bw; i++)
       *    Bookmarks_add_to_menu(browser_window[i], menuitem); */
      Bookmarks_add_to_menu(browser_window[0], menuitem, i);
      g_free(url_str);
      g_free(title);
      a_Url_free(url);
      i++;
   }
}

/*
 * Copy bookmarks when new browser windows are opened.
 * Called by 'a_Menu_mainbar_new()'
 */
void a_Bookmarks_fill_new_menu(BrowserWindow *bw)
{
   gint i;
   GtkWidget *menuitem;

   for (i = 0; i < num_bookmarks; i++) {
     menuitem = gtk_menu_item_new_with_label(bookmarks[i].title);
     gtk_widget_show(menuitem);
     Bookmarks_add_to_menu(bw, menuitem, i);
   }
}

/*
 * Return the text between start_text and end_text in line.
 * ** this function allocates memory for returned-text which must be freed! **
 * I really hope there isn't a gtk or one of your html functions
 * that does this :) I'm sure there must be an html one already..
 *
 * On error, returns NULL.
 */
static char *
 Bookmarks_search_line(char *line, char *start_text, char *end_text) {
   gint segment_length;
   char *start_index, *end_index;

   /* if string is not found, return NULL */
   if ((start_index = strstr(line, start_text)) == NULL)
      return (NULL);
   if ((end_index = strstr(line, end_text)) == NULL)
      return (NULL);

   /* adjustment cause strstr returns the start of the text */
   start_index += strlen(start_text);

   /* find length of text segment */
   segment_length = end_index - start_index;
   return g_strndup(start_index, segment_length);
}
