/*
 * File: findtext.c
 *
 * Copyright 2001 Jorge Arellano Cid <jcid@inf.utfsm.cl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Support functions for finding text in a page.
 */

#include <string.h>   /* for strpbrk */
#include "findtext.h"


/*
 * Parse string into a new KeyData structure.
 */
KeyData *a_Findtext_key_new(gchar *string)
{
   gchar start, end, *p, *s, *t;
   WordData *wd;
   KeyData *Key;

   g_return_val_if_fail(string != NULL && *string, NULL);

   Key = g_new(KeyData, 1);
   Key->KeyStr  = g_strdup(string);
   Key->WordList = NULL;
   Key->Buf = NULL;
   Key->State = M_FirstWord;
   Key->Matches = 0;
   Key->y_pos = -1;

   p = string;
   while ( *p ) {
      for (start = '*'; isspace(*p); ++p, start = ' ');
      for (s = p; *p && !isspace(*p); ++p);
      end = isspace(*p) ? ' ' : '*';
      if (p - s) {
         /* Add a new word */
         wd = g_new(WordData, 1);
         t = g_strndup(s, p - s);
         wd->Word = g_string_new(t);
         g_free(t);
         Key->WordList = g_slist_append(Key->WordList, wd);
         /* Set word flags */
         if (start == ' ')
            wd->Flag = (end == ' ') ? FT_FullWord : FT_LeftSubStr;
         else
            wd->Flag = (end == ' ') ? FT_RightSubStr : FT_FreeSubStr;
      }
   }

   return Key;
}

/*
 * Free KeyData structure
 */
void a_Findtext_key_free(KeyData *Key)
{
   GSList *node;

   g_free(Key->KeyStr);
   for (node = Key->WordList; node; node = g_slist_next(node)) {
      g_string_free(((WordData *)node->data)->Word, TRUE);
      g_free(node->data);
   }
   g_slist_free(Key->WordList);
   g_slist_free(Key->Buf);
   g_free(Key);
}

/*
 * Search text string for first token
 * Return: -1 if not found, else token start index
 */
static gint Findtext_seek_token(gchar *txt, WordData *wd)
{
   gchar *p;
   gint w_start, w_end, found;
   static gchar *StopChar = NULL;

   if (!StopChar)
      StopChar = g_strdup("xX");
   StopChar[0] = tolower(wd->Word->str[0]);
   StopChar[1] = toupper(wd->Word->str[0]);
   for (p = txt; (p = strpbrk(p, StopChar)); ++p) {

      if (!g_strncasecmp(p, wd->Word->str, wd->Word->len)) {
         w_start = (p == txt || (p > txt && isspace(p[-1])));
         w_end = (!p[wd->Word->len] || isspace(p[wd->Word->len]));

         found = (wd->Flag == FT_FreeSubStr) ||
                 (wd->Flag == FT_FullWord && w_start && w_end) ||
                 (wd->Flag == FT_LeftSubStr && w_start) ||
                 (wd->Flag == FT_RightSubStr && w_end);
         if (found)
            break;
      }
   }
   return (p ? p - txt : -1);
}

/*
 * Search next token (TokenNum) in Buffer
 * Return: -1 if end of buffer, -2 if fail, else token offset
 */
static gint Findtext_seek_next_token(KeyData *Key, gint TokenNum)
{
   gchar *p, *txt;
   WordData *wd;
   gint w_start, w_end, found,
        BufNode, /* [0 based] */
        Token;   /* [0 based] */
   static gchar *StopSet = " \n\r\t";

   g_return_val_if_fail(TokenNum > 0, -2);

   BufNode = 0;
   txt = p = g_slist_nth_data(Key->Buf, BufNode);
   for (Token = -1; Token < TokenNum;  ) {
      if (p) {
         /* find start of token */
         for (  ; isspace(*p); ++p);
         if (*p && ++Token == TokenNum)
            break;
         else
            p = strpbrk(p, StopSet);
      }
      if (!p && !(txt = p = g_slist_nth_data(Key->Buf, ++BufNode)))
         return -1;
   }

   found = 0;
   wd = g_slist_nth_data(Key->WordList, TokenNum);
   if (!g_strncasecmp(p, wd->Word->str, wd->Word->len)) {
      w_start = (p == txt || (p > txt && isspace(p[-1])));
      w_end = (!p[wd->Word->len] || isspace(p[wd->Word->len]));

      found = (wd->Flag == FT_FreeSubStr) ||
              (wd->Flag == FT_FullWord && w_start && w_end) ||
              (wd->Flag == FT_LeftSubStr && w_start) ||
              (wd->Flag == FT_RightSubStr && w_end);
   }
   return (found ? p - txt : -2);
}

/*
 * Compare a text-string with a KeyData structure
 * Return: 1 if first token is found, 0 otherwise
 */
gint a_Findtext_compare(gchar *text, KeyData *Key)
{
   gint st = 0;

   while (1) {
      switch (Key->State) {
      case M_FirstWord:
         Key->Matches = 0;
         Key->y_pos = -1;
         if (Key->Buf) {
            st = Findtext_seek_token(Key->Buf->data,
                                     g_slist_nth_data(Key->WordList, 0));
            if (st == -1) {
               Key->Buf = g_slist_remove(Key->Buf, Key->Buf->data);
               Key->State = M_FirstWord;
            } else {
               Key->Buf->data = (gchar *)Key->Buf->data + st;
               Key->Matches = 1;
               Key->State = M_NextWord;
            }
         } else {
            Key->State = M_NewText;
         }
         break;

      case M_NextWord:
         if (Key->Matches < g_slist_length(Key->WordList)) {
            st = Findtext_seek_next_token(Key, Key->Matches);
            if (st == -2) {
               Key->Buf->data = (gchar*)Key->Buf->data +
                                ((WordData*)Key->WordList->data)->Word->len;
               Key->State = M_FirstWord;
            } else if (st == -1) {
               Key->State = M_Read;
            } else {
               Key->Matches++;
               Key->State = M_NextWord;
            }
         } else {
            Key->Buf->data = (gchar*)Key->Buf->data +
                             ((WordData*)Key->WordList->data)->Word->len;
            Key->State = M_Found;
         }
         break;

      case M_Read:
         Key->State = M_NewText;
         return Key->Matches;

      case M_NewText:
         if (text) {
            Key->Buf = g_slist_append(Key->Buf, text);
            text = NULL;
            Key->State = (Key->Matches) ? M_NextWord : M_FirstWord;
         } else {
            Key->State = M_Read;
         }
         break;

      case M_Found:
         Key->State = M_FirstWord;
         return Key->Matches;
      }
   }
   return 0; /* compiler happiness */
}

