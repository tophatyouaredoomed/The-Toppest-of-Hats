/*
 * File: http.c
 *
 * Copyright (C) 2000, 2001 Jorge Arellano Cid <jcid@inf.utfsm.cl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * HTTP connect functions
 */


#include <unistd.h>
#include <errno.h>              /* for errno */
#include <string.h>             /* for strstr */
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>         /* for lots of socket stuff */
#include <netinet/in.h>         /* for ntohl and stuff */

#include "Url.h"
#include "IO.h"
#include "../klist.h"
#include "../dns.h"
#include "../cache.h"
#include "../web.h"
#include "../interface.h"
#include "../prefs.h"


/* Used to send a message to the bw's status bar */
#define BW_MSG(web, root, fmt...) \
   (a_Web_valid(web) && (!(root) || (web)->flags & WEB_RootUrl)) ? \
   a_Interface_msg((web)->bw, fmt) : (root)

#define DEBUG_LEVEL 5
#include "../debug.h"



/* 'Url' and 'web' are just references (no need to deallocate them here). */
typedef struct {
   gint SockFD;
   const DilloUrl *Url;    /* reference to original URL */
   guint port;             /* need a separate port in order to support PROXY */
   gboolean use_proxy;     /* indicates whether to use proxy or not */
   DilloWeb *web;          /* reference to client's web structure */
   guint32 ip_addr;        /* Holds the DNS answer */
   GIOChannel *GioCh;      /* GIOChannel to monitor the connecting process */
   gint Err;               /* Holds the errno of the connect() call */
   ChainLink *Info;        /* Used for CCC asynchronous operations */
} SocketData_t;


/*
 * Local data
 */
static Klist_t *ValidSocks = NULL; /* Active sockets list. It holds pointers to
                                    * SocketData_t structures. */

static DilloUrl *HTTP_Proxy = NULL;
static gchar **NoProxyVec = NULL;


/*              
 * Initialize proxy vars.
 */             
gint a_Http_init(void)
{
   gchar *env_proxy = getenv("http_proxy");

   if (env_proxy && strlen(env_proxy))
      HTTP_Proxy = a_Url_new(env_proxy, NULL, 0, 0);
   if (!HTTP_Proxy && prefs.http_proxy)
      HTTP_Proxy = prefs.http_proxy;
   NoProxyVec = prefs.no_proxy_vec;
   return 0;
}

/*
 * Forward declarations
 */
static void Http_send_query(ChainLink *Info, SocketData_t *S);
static void Http_expect_answer(SocketData_t *S);



/*
 * Create and init a new SocketData_t struct, insert into ValidSocks,
 * and return a primary key for it.
 */
static gint Http_sock_new(void)
{
   SocketData_t *S = g_new0(SocketData_t, 1);
   return a_Klist_insert(&ValidSocks, (gpointer)S);
}

/*
 * Free SocketData_t struct
 */
static void Http_socket_free(gint SKey)
{
   SocketData_t *S;

   if ((S = a_Klist_get_data(ValidSocks, SKey))) {
      a_Klist_remove(ValidSocks, SKey);
      if (S->GioCh)
        g_io_channel_unref(S->GioCh);
      g_free(S);
   }
}

/*
 * Make the http query string
 */
static char *Http_query(const DilloUrl *url, gboolean use_proxy)
{
   gchar *str, *ptr;
   GString *s_port    = g_string_new(""),
           *query     = g_string_new(""),
           *full_path = g_string_new("");

   /* Sending the default port in the query may cause a 302-answer.  --Jcid */
   if (URL_PORT(url) && URL_PORT(url) != DILLO_URL_HTTP_PORT)
      g_string_sprintfa(s_port, ":%d", URL_PORT(url));

   if (use_proxy) {
      g_string_sprintfa(full_path, "%s%s",
                        URL_STR(url),
                        (URL_PATH(url) || URL_QUERY(url)) ? "" : "/");
      if ((ptr = strrchr(full_path->str, '#')))
         g_string_truncate(full_path, ptr - full_path->str);
   } else {
      g_string_sprintfa(full_path, "%s%s%s%s",
                        URL_PATH(url)  ? URL_PATH(url) : "",
                        URL_QUERY(url) ? "?" : "",
                        URL_QUERY(url) ? URL_QUERY(url) : "",
                        (URL_PATH(url) || URL_QUERY(url)) ? "" : "/");
   }

   if ( URL_FLAGS(url) & URL_Post ){
      g_string_sprintfa(
         query,
         "POST %s HTTP/1.0\r\n"
         "Host: %s%s\r\n"
         "User-Agent: Dillo/%s\r\n"
         "Content-type: application/x-www-form-urlencoded\r\n"
         "Content-length: %ld\r\n"
         "\r\n"
         "%s",
         full_path->str, URL_HOST(url), s_port->str, VERSION,
         (glong)strlen(URL_DATA(url)), URL_DATA(url));

   } else {
      g_string_sprintfa(
         query,
         "GET %s HTTP/1.0\r\n"
         "%s"
         "Host: %s%s\r\n"
         "User-Agent: Dillo/%s\r\n"
         "\r\n",
         full_path->str,
         (URL_FLAGS(url) & URL_E2EReload) ?
            "Cache-Control: no-cache\r\nPragma: no-cache\r\n" : "",
         URL_HOST(url), s_port->str, VERSION);
   }

   str = query->str;
   g_string_free(query, FALSE);
   g_string_free(s_port, TRUE);
   g_string_free(full_path, TRUE);
   DEBUG_MSG(4, "Query:\n%s", str);
   return str;
}

/*
 * This function is called after the socket has been successfuly connected,
 * or upon an error condition on the connecting process.
 * Task: use the socket to send the HTTP-query and expect its answer
 */
static gboolean
 Http_use_socket(GIOChannel *src, GIOCondition cond, gpointer data)
{
   ChainLink *Info;
   SocketData_t *S;
   gint SKey = (gint) data;

   DEBUG_MSG(3, "Http_use_socket: %s [errno %d] [GIOcond %d]\n",
             g_strerror(errno), errno, cond);

   /* This check is required because glib may asynchronously
    * call this function with data that's no longer used  --Jcid   */
   if ( !(S = a_Klist_get_data(ValidSocks, SKey)) )
      return FALSE;

   Info = S->Info;
   if ( cond & G_IO_HUP ) {
      DEBUG_MSG(3, "--Connection broken\n");
      BW_MSG(S->web, 0, "ERROR: unable to connect to remote host");
      a_Chain_fcb(OpAbort, 1, Info, NULL, NULL);
      Http_socket_free(SKey);
   } else if ( S->Err ) {
      BW_MSG(S->web, 0, "ERROR: %s", g_strerror(S->Err));
      DEBUG_MSG(3, "Http_use_socket ERROR: %s\n", g_strerror(S->Err));
      a_Chain_fcb(OpAbort, 1, Info, NULL, NULL);
      g_io_channel_close(S->GioCh);
      Http_socket_free(SKey);
   } else if ( cond & G_IO_OUT ) {
      DEBUG_MSG(3, "--Connection established\n");
      g_io_channel_unref(S->GioCh);
      S->GioCh = NULL;
      Http_send_query(Info, S);
      Http_expect_answer(S);
   }
   return FALSE;
}

/*
 * This function gets called after the DNS succeeds solving a hostname.
 * Task: Finish socket setup and start connecting the socket.
 * Return value: 0 on success;  -1 on error.
 */
static int Http_connect_socket(ChainLink *Info)
{
   gint status;
   struct sockaddr_in name;
   SocketData_t *S = a_Klist_get_data(ValidSocks, (gint)Info->LocalKey);

   /* Some OSes require this...  */
   memset(&name, 0, sizeof(name));
   /* Set remaining parms. */
   name.sin_family = AF_INET;
   name.sin_port = S->port ? htons(S->port) : htons(DILLO_URL_HTTP_PORT);
   name.sin_addr.s_addr = htonl(S->ip_addr);

   S->GioCh = g_io_channel_unix_new(S->SockFD);
   g_io_add_watch(S->GioCh, G_IO_ERR | G_IO_HUP | G_IO_OUT,
                  Http_use_socket, Info->LocalKey);
   status = connect(S->SockFD, (struct sockaddr *)&name, sizeof(name));
   if ( status == -1 && errno != EINPROGRESS ) {
      S->Err = errno;
      return -1;
   }

   BW_MSG(S->web, 1, "Contacting host...");

   return 0; /* Success */
}


/*
 * Create and submit the HTTP query to the IO engine
 */
static void Http_send_query(ChainLink *Info, SocketData_t *S)
{
   IOData_t *io;
   gchar *query;
   void *link;

   /* Create the query */
   query = Http_query(S->Url, S->use_proxy);

   /* send query */
   BW_MSG(S->web, 1, "Sending query to %s...", URL_HOST(S->Url));
   io = a_IO_new(S->SockFD);
   io->Op = IOWrite;
   io->IOVec.iov_base = query;
   io->IOVec.iov_len  = strlen(query);
   io->Flags |= IOFlag_FreeIOVec;
   io->ExtData = NULL;
   link = a_Chain_link_new(a_Http_ccc, Info, CCC_FWD, a_IO_ccc);
   a_IO_ccc(OpStart, 1, link, io, NULL);
}

/*
 * Expect the HTTP query's answer
 */
static void Http_expect_answer(SocketData_t *S)
{
   IOData_t *io2;

   /* receive answer */
   io2 = a_IO_new(S->SockFD);
   io2->Op = IORead;
   io2->IOVec.iov_base = g_malloc(IOBufLen_Http);
   io2->IOVec.iov_len  = IOBufLen_Http;
   io2->Flags |= IOFlag_FreeIOVec;
   io2->ExtData = (void *) S->Url;
   a_IO_ccc(OpStart, 2, a_Chain_new(), io2, NULL);
}

/*
 * Test proxy settings and check the no_proxy domains list
 * Return value: whether to use proxy or not.
 */
static gint Http_must_use_proxy(const DilloUrl *url)
{
   gint i;

   if (HTTP_Proxy) {
      if (NoProxyVec)
         for (i = 0; NoProxyVec[i]; ++i)
            if (strstr(URL_AUTHORITY(url), NoProxyVec[i]))
               return 0;
      return 1;
   }
   return 0;
}

/*
 * Asynchronously create a new http connection for 'Url'
 * We'll set some socket parameters; the rest will be set later
 * when the IP is known.
 * ( Data = Requested Url; ExtraData = Web structure )
 * Return value: 0 on success, -1 otherwise
 */
static gint Http_get(ChainLink *Info, void *Data, void *ExtraData)
{
   void *link;
   const DilloUrl *Url = Data;
   SocketData_t *S = a_Klist_get_data(ValidSocks, (gint)Info->LocalKey);
   gchar hostname[256], *Host = hostname;

   /* Reference Info data */
   S->Info = Info;
   /* Reference Web data */
   S->web = ExtraData;

   /* Proxy support */
   if (Http_must_use_proxy(Url)) {
      strncpy(hostname, URL_HOST(HTTP_Proxy), sizeof(hostname));
      S->port = URL_PORT(HTTP_Proxy);
      S->use_proxy = TRUE;
   } else {
      Host = (gchar *)URL_HOST(Url);
      S->port = URL_PORT(Url);
      S->use_proxy = FALSE;
   }

   /* Set more socket parameters */
   S->Url = Url;
   if ( (S->SockFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ) {
      S->Err = errno;
      DEBUG_MSG(5, "Http_get ERROR: %s\n", g_strerror(errno));
      return -1;
   }
   /* set NONBLOCKING */
   fcntl(S->SockFD, F_SETFL, O_NONBLOCK | fcntl(S->SockFD, F_GETFL));

   /* Let the user know what we'll do */
   BW_MSG(S->web, 1, "DNS solving %s", URL_HOST(S->Url));

   /* Let the DNS engine solve the hostname, and when done,
    * we'll try to connect the socket */
   link = a_Chain_link_new(a_Http_ccc, Info, CCC_FWD, a_Dns_ccc);
   a_Dns_ccc(OpStart, 1, link, Host, NULL);
   return 0;
}

/*
 * CCC function for the HTTP module
 */
void
 a_Http_ccc(int Op, int Branch, ChainLink *Info, void *Data, void *ExtraData)
{
   gint SKey = (gint)Info->LocalKey;
   SocketData_t *S = a_Klist_get_data(ValidSocks, SKey);

   if ( Branch == 1 ) {
      /* DNS query branch */
      switch (Op) {
      case OpStart:
         Info->LocalKey = (void *) SKey = Http_sock_new();
         if (Http_get(Info, Data, ExtraData) < 0) {
            DEBUG_MSG(2, " HTTP: new abort handler! #2\n");
            S = a_Klist_get_data(ValidSocks, SKey);
            BW_MSG(S->web, 1, "ERROR: %s", g_strerror(S->Err));
            Http_socket_free(SKey);
            a_Chain_fcb(OpAbort, 1, Info, NULL, NULL);
         }
         break;
      case OpSend:
         /* Successful DNS answer; save the IP */
         if (S)
            S->ip_addr = *(int *)Data;
         break;
      case OpEnd:
         if (S) {
            /* Unlink DNS_Info */
            a_Chain_del_link(Info, CCC_BCK);
            /* start connecting the socket */
            if (Http_connect_socket(Info) < 0) {
               DEBUG_MSG(2, " HTTP: new abort handler! #1\n");
               BW_MSG(S->web, 1, "ERROR: %s", g_strerror(S->Err));
               Http_socket_free(SKey);
               a_Chain_fcb(OpAbort, 1, Info, NULL, NULL);
            }
         }
         break;
      case OpAbort:
         /* DNS wasn't able to resolve the hostname */
         if (S) {
            /* Unlink DNS_Info */
            a_Chain_del_link(Info, CCC_BCK);
            BW_MSG(S->web, 0, "ERROR: Dns can't solve %s",
                   (S->use_proxy) ? URL_HOST(HTTP_Proxy) : URL_HOST(S->Url));
            while (close(S->SockFD) == EINTR);
            Http_socket_free(SKey);
            /* send abort message to higher-level functions */
            a_Chain_fcb(OpAbort, 1, Info, NULL, NULL);
         }
         break;
      }

   } else if ( Branch == 2 ) {
      /* IO send-query branch */
      switch (Op) {
      case OpStart:
         /* LocalKey was set by branch 1 */
         break;
      case OpEnd:
         /* finished sending the HTTP query */
         if (S) {
            BW_MSG(S->web, 1, "Query sent, waiting for reply...");
            a_Chain_del_link(Info, CCC_BCK);
            a_Chain_fcb(OpEnd, 1, Info, NULL, NULL);
            Http_socket_free(SKey);
         }
         break;
      case OpAbort:
         /* something bad happened... */
         /* unlink IO_Info */
         if (S) {
            a_Chain_del_link(Info, CCC_BCK);
            a_Chain_fcb(OpAbort, 1, Info, NULL, NULL);
            Http_socket_free(SKey);
         }
         break;
      }

   } else if ( Branch == -1 ) {
      /* Backwards abort */
      switch (Op) {
      case OpAbort:
         /* something bad happened... */
         DEBUG_MSG(2, "Http: OpAbort [-1]\n");
         Http_socket_free(SKey);
         a_Chain_bcb(OpAbort, -1, Info, NULL, NULL);
         g_free(Info);
         break;
      }
   }
}



/*
 * Deallocate memory used by http module
 * (Call this one at exit time)
 */
void a_Http_freeall(void)
{
   a_Klist_free(&ValidSocks);
}

