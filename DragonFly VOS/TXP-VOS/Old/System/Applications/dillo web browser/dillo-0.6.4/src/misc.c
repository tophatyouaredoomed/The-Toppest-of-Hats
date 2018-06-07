/*
 * File: misc.c
 *
 * Copyright (C) 2000 Jorge Arellano Cid <jcid@inf.utfsm.cl>,
 *                    Jörgen Viksell <vsksga@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

/*
 * Prepend the users home-dir to 'file' string i.e,
 * pass in .dillo/bookmarks.html and it will return
 * /home/imain/.dillo/bookmarks.html
 *
 * Remember to g_free() returned value!
 */
char *a_Misc_prepend_user_home(const char *file)
{
   return ( g_strconcat(g_get_home_dir(), "/", file, NULL) );
}

/*
 * Case insensitive strstr
 */
char *a_Misc_stristr(char *src, char *str)
{
   int i, j;

   for (i = 0, j = 0; src[i] && str[j]; ++i)
      j = (tolower(src[i]) == tolower(str[j])) ? j + 1 : 0;

   if (!str[j])                 /* Got all */
      return (src + i - j);
   return NULL;
}

#define TAB_SIZE 8

/*
 * Takes a string and converts any tabs to spaces.
 */
char *a_Misc_expand_tabs(const char *str)
{
   GString *New = g_string_new("");
   int len, i, j, pos, old_pos;
   char *val;

   if ( (len = strlen(str)) ) {
      for(pos = 0, i = 0; i < len; i++) {
         if (str[i] == '\t') {
            /* Fill with whitespaces until the next tab. */
            old_pos = pos;
            pos += TAB_SIZE - (pos % TAB_SIZE);
            for(j = old_pos; j < pos; j++)
               g_string_append_c(New, ' ');
         } else {
            g_string_append_c(New, str[i]);
            pos++;
         }
      }
   }
   val = New->str;
   g_string_free(New, FALSE);
   return val;
}
