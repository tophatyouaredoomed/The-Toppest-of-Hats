/*
 * File: about.c
 *
 * Copyright (C) 1997 Raph Levien <raph@acm.org>
 * Copyright (C) 1999, 2001 Jorge Arellano Cid <jcid@inf.utfsm.cl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <pthread.h>

#include "Url.h"
#include "../nav.h"
#include "../web.h"

typedef struct _SplashInfo SplashInfo_t;

struct _SplashInfo {
   gint FD_Read;
   gint FD_Write;
};


/*
 * HTML text for startup screen
 */
static char *Splash=
"Content-type: text/html

<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">
<html>
<head>
<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">
</head>

<body  bgcolor=\"gray\" link=\"aqua\" vlink=\"black\">

<table WIDTH=\"100%\" BORDER=0 CELLSPACING=0 CELLPADDING=5>
<tr>
<td BGCOLOR=\"gray\">
<hr>
 <A HREF=\"http://dillo.sourceforge.net/dillo-help.html\">Help</A><br>
 <A HREF=\"http://dillo.sourceforge.net/\">Home Page</A><br>
 <A HREF=\"http://dillo.sourceforge.net/ChangeLog.html\">Full ChangeLog</A><br>
 <A HREF=\"http://www.google.com/\">Google</A><br>
<hr>
</td>

<td BGCOLOR=\"#9090F0\">
<h1><b>Dillo 0.6.4</b></h1>

<hr>
 <h1>Dillo project<br>
     <h3>Version 0.6.4<BR> <EM>(this is alpha code)</EM> </h3></h1>
<h4> License: </h4>
<p>
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
<p>
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

<hr>
<h4> Release overview: </h4>
<BLOCKQUOTE>
  This release is focused on bugfixes and cleanups, but it also features
some useful enhancements as: remembering the page-scrolling-position,
align tags and a new 'force_visited_color' option in dillorc.
</BLOCKQUOTE><BLOCKQUOTE>
 Remember that dillo project uses a release model where every new
browser shall be better than the former.
<EM>Keep up with the latest one!</EM>
</BLOCKQUOTE>
<hr>
<h4>NOTES:</h4>
<p>
<ul>
<li> There's a <STRONG>dillorc</STRONG> (readable config) file within the
tarball; It is well commented and has plenty of options to customize dillo,
so <STRONG>copy it</STRONG> to your <STRONG>~/.dillo/</STRONG> directory,
and modify to your taste.
<li> There's documentation for developers in the <CODE>/doc</CODE>
dir within the tarball;
you can find directions on everything else at the home page. </li>
<li> Dillo has context sensitive menus on the right-mouse-button
 (pages, links, Back and Forward buttons)</li>
<li> Dillo behaves very nice when browsing local files, images, and HTML.
It's also very good for Internet searching (try Google!).</li>
<li> This release is mainly intended <strong>for developers</strong>
and <em>advanced users</em></li>
</ul>
<hr>
<h4>Notes to Xfce users:</h4>
<P> Please bear in mind that dillo is alpha code; it is not ready for
end users yet. Anyway, local browsing (files and local HTTP) is quite stable
and chances are you'll not be disappointed.
<P> Tip: if you set BROWSER=dillo, Xfce's help will be a sweet tour!
<P> TABLES are a work in progress.
<P> FRAMES, Java, and Javascript are not supported.
<hr>
<hr>

</td>
</table>
</body>
</html>
";



/*
 * Send the splash screen through the IO using a pipe.
 */
static void About_send_splash(ChainLink *Info, DilloUrl *Url)
{
   void *link;
   gint SplashPipe[2];
   IOData_t *io1, *io2;
   SplashInfo_t *SpInfo = g_new(SplashInfo_t, 1);

   if (pipe(SplashPipe)){
      return;
   }
   SpInfo->FD_Read  = SplashPipe[0];
   SpInfo->FD_Write = SplashPipe[1];
   Info->LocalKey = SpInfo;

   /* send splash */
   io1 = a_IO_new(SpInfo->FD_Write);
   io1->Op = IOWrite;
   io1->IOVec.iov_base = Splash;
   io1->IOVec.iov_len  = strlen(Splash);
   io1->Flags |= IOFlag_ForceClose;
   io1->ExtData = NULL;
   link = a_Chain_link_new(a_About_ccc, Info, CCC_FWD, a_IO_ccc);
   a_IO_ccc(OpStart, 1, link, io1, NULL);

   /* receive answer */
   io2 = a_IO_new(SpInfo->FD_Read);
   io2->Op = IORead;
   io2->IOVec.iov_base = g_malloc(IOBufLen_About);
   io2->IOVec.iov_len  = IOBufLen_About;
   io2->Flags |= IOFlag_FreeIOVec;
   io2->ExtData = (void *) Url;
   a_IO_ccc(OpStart, 2, a_Chain_new(), io2, NULL);
}

/*
 * Push the right URL for each supported "about"
 * ( Data = Requested URL; ExtraData = Web structure )
 */
static gint About_get(ChainLink *Info, void *Data, void *ExtraData)
{
   char *loc;
   const char *tail;
   DilloUrl *Url = Data;
   DilloWeb *web = ExtraData;
   DilloUrl *LocUrl;

   tail = URL_PATH(Url) ? URL_PATH(Url) : "";

   if (!strcmp(tail, "splash")) {
      About_send_splash(Info, Url);
      return 1;
   }
   if (!strcmp(tail, "jwz"))
      loc = "http://www.jwz.org/";
   else if (!strcmp(tail, "raph"))
      loc = "http://www.levien.com/";
   else if (!strcmp(tail, "yosh"))
      loc = "http://yosh.gimp.org/";
   else if (!strcmp(tail, "snorfle"))
      loc = "http://www.snorfle.net/";
   else if (!strcmp(tail, "dillo"))
      loc = "http://dillo.sourceforge.net/";
   else if (!strcmp(tail, "help"))
      loc = "http://dillo.sourceforge.net/dillo-help.html";
   else
      loc = "http://www.google.com/";

   LocUrl = a_Url_new(loc, NULL, 0, 0);
   a_Nav_push(web->bw, LocUrl);
   a_Url_free(LocUrl);
   return 0;
}

/*
 * CCC function for the ABOUT module
 */
void
 a_About_ccc(int Op, int Branch, ChainLink *Info, void *Data, void *ExtraData)
{
   if ( Branch == 1 ) {
      /* Start about method */
      switch (Op) {
      case OpStart:
         if (About_get(Info, Data, ExtraData) == 0)
            a_Chain_fcb(OpAbort, 1, Info, NULL, ExtraData);
         break;
      }

   } else if ( Branch == 2 ) {
      /* IO send-data branch */
      switch (Op) {
      case OpStart:
         break;
      case OpEnd:
         a_Chain_del_link(Info, CCC_BCK);
         g_free(Info->LocalKey);
         a_Chain_fcb(OpEnd, 1, Info, NULL, ExtraData);
         break;
      case OpAbort:
         a_Chain_del_link(Info, CCC_BCK);
         g_free(Info->LocalKey);
         a_Chain_fcb(OpAbort, 1, Info, NULL, ExtraData);
         break;
      }

   } else if ( Branch == -1 ) {
      /* Backwards abort */
      switch (Op) {
      case OpAbort:
         g_free(Info->LocalKey);
         a_Chain_bcb(OpAbort, -1, Info, NULL, NULL);
         g_free(Info);
         break;
      }
   }
}

