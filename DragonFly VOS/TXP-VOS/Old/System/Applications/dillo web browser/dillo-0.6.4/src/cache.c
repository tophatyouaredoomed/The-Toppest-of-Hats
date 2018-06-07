/*
 * File: cache.c
 *
 * Copyright 2000, 2001 Jorge Arellano Cid <jcid@inf.utfsm.cl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Dillo's cache module
 */

#include <ctype.h>              /* for tolower */
#include <sys/types.h>

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "config.h"
#include "list.h"
#include "IO/Url.h"
#include "IO/IO.h"
#include "web.h"
#include "dicache.h"
#include "interface.h"
#include "nav.h"

#define NULLKey 0

#define DEBUG_LEVEL 5
#include "debug.h"

/*
 *  Local data types
 */

typedef struct {
   const DilloUrl *Url;      /* Cached Url. Url is used as a primary Key */
   const char *Type;         /* MIME type string */
   GString *Header;          /* HTTP header */
   const DilloUrl *Location; /* New URI for redirects */
   void *Data;               /* Pointer to raw data */
   size_t ValidSize,         /* Actually size of valid range */
          TotalSize,         /* Goal size of the whole data (0 if unknown) */
          BuffSize;          /* Buffer Size for unknown length transfers */
   guint Flags;              /* Look Flag Defines in cache.h */
   IOData_t *io;             /* Pointer to IO data */
   ChainLink *CCCQuery;      /* CCC link for querying branch */
   ChainLink *CCCAnswer;     /* CCC link for answering branch */
} CacheData_t;


/*
 *  Local data
 */
/* A list for cached data */
static CacheData_t *CacheList = NULL;
static gint CacheListSize = 0, CacheListMax = 32;

/* A list for cache clients.
 * Although implemented as a list, we'll call it ClientQueue  --Jcid */
static GSList *ClientQueue = NULL;


/*
 *  Forward declarations
 */
static void Cache_process_queue(CacheData_t *entry);
static void Cache_delayed_process_queue(CacheData_t *entry);
static void Cache_stop_client(gint Key, gint force);


/* Client operations ------------------------------------------------------ */

/*
 * Make a unique primary-key for cache clients
 */
static gint Cache_client_make_key(void)
{
   static gint ClientKey = 0; /* Provide a primary key for each client */

   if ( ++ClientKey < 0 ) ClientKey = 1;
   return ClientKey;
}

/*
 * Add a client to ClientQueue.
 *  - Every client-camp is just a reference (except 'Web').
 *  - Return a unique number for identifying the client.
 */
static gint Cache_client_enqueue(const DilloUrl *Url, DilloWeb *Web,
                                 CA_Callback_t Callback, void *CbData)
{
   gint ClientKey;
   CacheClient_t *NewClient;

   NewClient = g_new(CacheClient_t, 1);
   ClientKey = Cache_client_make_key();
   NewClient->Key = ClientKey;
   NewClient->Url = Url;
   NewClient->Buf = NULL;
   NewClient->Callback = Callback;
   NewClient->CbData = CbData;
   NewClient->Web    = Web;

   ClientQueue = g_slist_append(ClientQueue, NewClient);

   return ClientKey;
}

/*
 * Compare function for searching a Client by its key
 */
static gint Cache_client_key_cmp(gconstpointer client, gconstpointer key)
{
   return ( GPOINTER_TO_INT(key) != ((CacheClient_t *)client)->Key );
}

/*
 * Compare function for searching a Client by its URL
 */
static gint Cache_client_url_cmp(gconstpointer client, gconstpointer url)
{
   return a_Url_cmp((DilloUrl *)url, ((CacheClient_t *)client)->Url);
}

/*
 * Compare function for searching a Client by hostname
 */
static gint Cache_client_host_cmp(gconstpointer client, gconstpointer hostname)
{
   return g_strcasecmp(URL_HOST(((CacheClient_t *)client)->Url),
                       (gchar *)hostname );
}

/*
 * Remove a client from the queue
 */
static void Cache_client_dequeue(CacheClient_t *Client, gint Key)
{
   GSList *List;

   if (!Client &&
       (List = g_slist_find_custom(ClientQueue, GINT_TO_POINTER(Key),
                                   Cache_client_key_cmp)))
      Client = List->data;

   if ( Client ) {
      ClientQueue = g_slist_remove(ClientQueue, Client);
      a_Web_free(Client->Web);
      g_free(Client);
   }
}


/* Entry operations ------------------------------------------------------- */

/*
 * Set safe values for a new cache entry
 */
static void Cache_entry_init(CacheData_t *NewEntry, const DilloUrl *Url)
{
   NewEntry->Url = a_Url_dup(Url);
   NewEntry->Type = NULL;
   NewEntry->Header = g_string_new("");
   NewEntry->Location = NULL;
   NewEntry->Data = NULL;
   NewEntry->ValidSize = 0;
   NewEntry->TotalSize = 0;
   NewEntry->BuffSize = 4096;
   NewEntry->Flags = 0;
   NewEntry->io = NULL;
   NewEntry->CCCQuery = a_Chain_new();
   NewEntry->CCCAnswer = NULL;
}

/*
 * Allocate and set a new entry in the cache list
 */
static CacheData_t *Cache_entry_add(const DilloUrl *Url)
{
   /* Allocate memory */
   a_List_add(CacheList, CacheListSize, sizeof(*CacheList), CacheListMax);
   /* Set safe values */
   Cache_entry_init(&CacheList[CacheListSize], Url);
   return &CacheList[CacheListSize++];
}

/*
 * Get the data structure for a cached URL (using 'Url' as the search key)
 * If 'Url' isn't cached, return NULL
 * (We search backwards to speed up the process)
 */
static CacheData_t *Cache_entry_search(const DilloUrl *Url)
{
   gint i;

   for ( i = CacheListSize - 1; i >= 0; --i )
      if ( !a_Url_cmp(Url, CacheList[i].Url) )
         return (CacheList + i);

   return NULL;
}

/*
 * Get the data structure for a cached URL
 * If Url isn't cached, return NULL
 * ===> 'Url' must a be a pointer to 'entry->Url' <===
 */
static CacheData_t *Cache_entry_fast_search(const DilloUrl *Url)
{
   gint i;

   for ( i = CacheListSize - 1; i >= 0; --i )
      if ( Url == CacheList[i].Url )
         return (CacheList + i);
   return NULL;
}

/*
 *  Free the components of a CacheData_t struct.
 */
static void Cache_entry_free(CacheData_t *entry)
{
   a_Url_free((DilloUrl *)entry->Url);
   g_free((gchar *)entry->Type);
   g_string_free(entry->Header, TRUE);
   a_Url_free((DilloUrl *)entry->Location);
   g_free(entry->Data);
   /* CCCQuery and CCCAnswer are just references */
}

/*
 * Remove an entry from the CacheList (no CCC-function is called)
 */
static gint Cache_entry_remove_raw(CacheData_t *entry, DilloUrl *url)
{
   if (!entry && !(entry = Cache_entry_search(url)))
      return 0;

   /* There MUST NOT be any clients */
   g_return_val_if_fail(
      !g_slist_find_custom(ClientQueue, url, Cache_client_url_cmp), 0);

   /* remove from dicache */
   a_Dicache_invalidate_entry(url);

   /* remove from cache */
   Cache_entry_free(entry);
   a_List_remove(CacheList, entry - CacheList, CacheListSize);
   return 1;
}

/*
 * Remove an entry, using the CCC if necessary.
 * (entry SHOULD NOT have clients)
 */
static void Cache_entry_remove(CacheData_t *entry, DilloUrl *url)
{
   ChainLink *InfoQuery, *InfoAnswer;

   if (!entry && !(entry = Cache_entry_search(url)))
      return;

   InfoQuery  = entry->CCCQuery;
   InfoAnswer = entry->CCCAnswer;

   if (InfoQuery) {
      DEBUG_MSG(2, "## Aborting CCCQuery\n");
      a_Cache_ccc(OpAbort, -1, InfoQuery, NULL, NULL);
   } else if (InfoAnswer) {
      DEBUG_MSG(2, "## Aborting CCCAnswer\n");
      a_Cache_ccc(OpAbort, -1, InfoAnswer, NULL, NULL);
   } else {
      DEBUG_MSG(2, "## Aborting raw2\n");
      Cache_entry_remove_raw(entry, url);
   }
}


/* Misc. operations ------------------------------------------------------- */

/*
 * Given an entry (url), remove all its clients (by url or host).
 */
static void Cache_stop_clients(DilloUrl *url, gint ByHost)
{
   gint i;
   CacheClient_t *Client;

   for (i = 0; (Client = g_slist_nth_data(ClientQueue, i)); ++i) {
      if ( (ByHost && Cache_client_host_cmp(Client, URL_HOST(url)) == 0) ||
           (!ByHost && Cache_client_url_cmp(Client, url) == 0) ) {
         Cache_stop_client(Client->Key, 0);
         --i;
      }
   }
}

/*
 * Prepare a reload by stopping clients and removing the entry
 *  Return value: 1 if on success, 0 otherwise
 */
static gint Cache_prepare_reload(DilloUrl *url)
{
   CacheClient_t *Client;
   DilloWeb *ClientWeb;
   gint i;

   for (i = 0; (Client = g_slist_nth_data(ClientQueue, i)); ++i){
      if (Cache_client_url_cmp(Client, url) == 0 &&
          (ClientWeb = Client->Web) && !(ClientWeb->flags & WEB_Download))
         Cache_stop_client(Client->Key, 0);
   }

   if (!g_slist_find_custom(ClientQueue, url, Cache_client_url_cmp)) {
      /* There're no clients for this entry */
      DEBUG_MSG(2, "## No more clients for this entry\n");
      Cache_entry_remove(NULL, url);
      return 1;
   } else {
      g_print("Cache_prepare_reload: ERROR, entry still has clients\n");
   }

   return 0;
}


/*
 * Try finding the url in the cache. If it hits, send the cache contents
 * from there. If it misses, set up a new connection.
 * Return value: A primary key for identifying the client.
 */
static gint Cache_open_url(DilloWeb *Web, CA_Callback_t Call, void *CbData)
{
   void *link;
   gint ClientKey;
   ChainFunction_t cccFunct;
   DilloUrl *Url = Web->url;
   CacheData_t *entry = Cache_entry_search(Url);

   if ( !entry ) {
      /* URL not cached: create an entry, send our client to the queue,
       * and open a new connection */
      entry = Cache_entry_add(Url);
      ClientKey = Cache_client_enqueue(entry->Url, Web, Call, CbData);
      a_Cache_ccc(OpStart, 1, entry->CCCQuery, NULL, (void *)entry->Url);
      cccFunct = a_Url_get_ccc_funct(entry->Url);
      if ( cccFunct ) {
         link = a_Chain_link_new(a_Cache_ccc, entry->CCCQuery,
                                 CCC_FWD, cccFunct);
         cccFunct(OpStart, 1, link, (void *)entry->Url, Web);
      } else {
         a_Interface_msg(Web->bw, "ERROR: unsupported protocol");
         a_Cache_ccc(OpAbort, 1, entry->CCCQuery, NULL, NULL);
      }
   } else {
      /* Feed our client with cached data */
      ClientKey = Cache_client_enqueue(entry->Url, Web, Call, CbData);
      Cache_delayed_process_queue(entry);
   }
   return ClientKey;
}

/*
 * Try finding the url in the cache. If it hits, send the cache contents
 * from there. If it misses, set up a new connection.
 *
 * - 'Web' is an auxiliar data structure with misc. parameters.
 * - 'Call' is the callback that receives the data
 * - 'CbData' is custom data passed to 'Call'
 *   Note: 'Call' and/or 'CbData' can be NULL, in that case they get set
 *   later by a_Web_dispatch_by_type, based on content/type and 'Web' data.
 *
 * Return value: A primary key for identifying the client.
 */
gint a_Cache_open_url(void *web, CA_Callback_t Call, void *CbData)
{
   gint ClientKey;
   DICacheEntry *DicEntry;
   CacheData_t *entry;
   DilloWeb *Web = web;
   DilloUrl *Url = Web->url;

   if (URL_FLAGS(Url) & URL_E2EReload) {
      /* Reload operation */
      Cache_prepare_reload(Url);
   }

   if ( Call ) {
      /* This is a verbatim request */
      ClientKey = Cache_open_url(Web, Call, CbData);

   } else if ( (DicEntry = a_Dicache_get_entry(Url)) &&
               (entry = Cache_entry_search(Url)) ) {
      /* We have it in the Dicache! */
      ClientKey = Cache_client_enqueue(entry->Url, Web, Call, CbData);
      Cache_delayed_process_queue(entry);

   } else {
      /* It can be anything; let's request it and decide what to do
         when we get more info... */
      ClientKey = Cache_open_url(Web, Call, CbData);
   }

   if (g_slist_find_custom(ClientQueue, GINT_TO_POINTER(ClientKey),
                           Cache_client_key_cmp))
      return ClientKey;
   else
      return 0; /* Aborted */
}

/*
 * Return the pointer to URL 'Data' in the cache.
 * (We also return its size in 'Size')
 */
char *a_Cache_url_read(const DilloUrl *Url, gint *Size)
{
   CacheData_t *entry = Cache_entry_search(Url);

   if ( !entry ) {
      *Size = 0;
      return NULL;
   }
   if ( entry->Flags & CA_Redirect && entry->Location )
      return a_Cache_url_read(entry->Location, Size);

   *Size = entry->ValidSize;
   return (char *) entry->Data;
}

/*
 * Extract a single field from the header, allocating and storing the value
 * in 'field'. ('fieldname' must not include the trailing ':')
 * Return a new string with the field-content if found (NULL on error)
 * (This function expects a '\r' stripped header)
 */
static char *Cache_parse_field(const char *header, const char *fieldname)
{
   char *field;
   gint i, j;

   for ( i = 0; header[i]; i++ ) {
      /* Search fieldname */
      for (j = 0; fieldname[j]; j++)
        if ( tolower(fieldname[j]) != tolower(header[i + j]))
           break;
      if ( fieldname[j] ) {
         /* skip to next line */
         for ( i += j; header[i] != '\n'; i++);
         continue;
      }

      i += j;
      while (header[i] == ' ') i++;
      if (header[i] == ':' ) {
        /* Field found! */
        while (header[++i] == ' ');
        for (j = 0; header[i + j] != '\n'; j++);
        field = g_strndup(header + i, j);
        return field;
      }
   }
   return NULL;
}

/*
 * Scan, allocate, and set things according to header info.
 * (This function needs the whole header to work)
 */
static void Cache_parse_header(CacheData_t *entry, IOData_t *io, gint HdrLen)
{
   gchar *header = entry->Header->str;
   gchar *Length, *Type, *location_str;

   /* Get Content-Type */
   Type = Cache_parse_field(header, "Content-Type");
// entry->Type = Type ? Type : g_strdup("text/html"); Hack for ebay  --Jcid
   entry->Type = Type ? Type : g_strdup("application/octet-stream");

   if ( header[9] == '3' && header[10] == '0' ) {
      /* 30x: URL redirection */
      entry->Flags |= CA_Redirect;
      if ( header[11] == '1' )
         /* 301 Moved Permanently */
         entry->Flags |= CA_ForceRedirect;
      location_str = Cache_parse_field(header, "Location");
      entry->Location = a_Url_new(location_str, URL_STR(entry->Url), 0, 0);
      g_free(location_str);

   } else if ( strncmp(header + 9, "404", 3) == 0 ) {
      entry->Flags |= CA_NotFound;
   }

   entry->ValidSize = io->Status - HdrLen;
   if ( (Length = Cache_parse_field(header, "Content-Length")) != NULL ) {
      entry->Flags |= CA_GotLength;
      entry->TotalSize = strtol(Length, NULL, 10);
      g_free(Length);
      if (entry->TotalSize < entry->ValidSize)
         entry->TotalSize = 0;
   }

   if ( entry->TotalSize > 0 && entry->TotalSize >= entry->ValidSize ) {
      entry->Data = g_malloc(entry->TotalSize);
      memcpy(entry->Data, (char*)io->IOVec.iov_base+HdrLen, io->Status-HdrLen);
      /* Free preallocated buffer */
      if (io->Flags & IOFlag_FreeIOVec)
         g_free(io->IOVec.iov_base);
      /* Prepare next read */
      io->IOVec.iov_base = (char *)entry->Data + entry->ValidSize;
      io->IOVec.iov_len  = entry->TotalSize - entry->ValidSize;
      io->Flags &= ~IOFlag_FreeIOVec;
   } else {
      /* We don't know the size of the transfer; A lazy server? ;) */
      entry->Data = g_malloc(entry->ValidSize + entry->BuffSize);
      memcpy(entry->Data, (char *)io->IOVec.iov_base+HdrLen, entry->ValidSize);
      /* Prepare next read */
      if (io->Flags & IOFlag_FreeIOVec)
         g_free(io->IOVec.iov_base);
      io->IOVec.iov_base = (char *)entry->Data + entry->ValidSize;
      io->IOVec.iov_len  = entry->BuffSize;
      io->Flags &= ~IOFlag_FreeIOVec;
   }
}

/*
 * Consume bytes until the whole header is got (up to a "\r\n\r\n" sequence)
 * (Also strip '\r' chars from header)
 */
static gint Cache_get_header(IOData_t *io, CacheData_t *entry)
{
   gint N, i;
   GString *hdr = entry->Header;
   guchar *data = io->IOVec.iov_base;

   /* Header finishes when N = 2 */
   N = (hdr->len && hdr->str[hdr->len - 1] == '\n');
   for ( i = 0; i < io->Status && N < 2; ++i ) {
      if ( data[i] == '\r' || !data[i] )
         continue;
      N = (data[i] == '\n') ? N + 1 : 0;
      g_string_append_c(hdr, data[i]);
   }

   if ( N == 2 ){
      /* Got whole header */
      DEBUG_MSG(2, "Header [io_len=%d]\n%s", i, hdr->str);
      entry->Flags |= CA_GotHeader;
      /* Return number of original-header bytes in this io [1 based] */
      return i;
   }
   return 0;
}

/*
 * Receive new data, update the reception buffer (for next read), update the
 * cache, and service the client queue.
 *
 * This function gets called whenever the IO has new data.
 *  'Op' is the operation to perform
 *  'VPtr' is a (void) pointer to the IO control structure
 */
static void Cache_process_io(int Op, void *VPtr)
{
   gint Status, len;
   IOData_t *io = VPtr;
   const DilloUrl *Url = io->ExtData;
   CacheData_t *entry = Cache_entry_fast_search(Url);

   /* Assert a valid entry (not aborted) */
   if ( !entry )
      return;

   /* Keep track of this entry's io */
   entry->io = io;

   if ( Op == IOClose ) {
      if (entry->Flags & CA_GotLength && entry->TotalSize != entry->ValidSize){
         DEBUG_HTTP_MSG("Content-Length does NOT match message body,\n"
                        " at: %s\n", URL_STR(entry->Url));
      }
      entry->Flags |= CA_GotData;
      entry->Flags &= ~CA_Stopped;          /* it may catch up! */
      entry->TotalSize = entry->ValidSize;
      entry->io = NULL;
      entry->CCCAnswer = NULL;
      Cache_process_queue(entry);
      return;
   } else if ( Op == IOAbort ) {
      /* todo: implement Abort
       * (eliminate cache entry and anything related) */
      DEBUG_MSG(5, "Cache_process_io Op = IOAbort; not implemented yet\n");
      entry->io = NULL;
      entry->CCCAnswer = NULL;
      return;
   }

   if ( !(entry->Flags & CA_GotHeader) ) {
      /* Haven't got the whole header yet */
      len = Cache_get_header(io, entry);
      if ( entry->Flags & CA_GotHeader ) {
         /* Let's scan, allocate, and set things according to header info */
         Cache_parse_header(entry, io, len);
         /* Now that we have it parsed, let's update our clients */
         Cache_process_queue(entry);
      }
      return;
   }

   Status = io->Status;
   entry->ValidSize += Status;
   if ( Status < (gint)io->IOVec.iov_len ) {
      /* An incomplete buffer; update buffer & size */
      io->IOVec.iov_len  -= Status;
      io->IOVec.iov_base = (char *)io->IOVec.iov_base + Status;
   } else if ( Status == (gint)io->IOVec.iov_len ) {
      /* A full buffer! */
      if ( !entry->TotalSize ) {
         /* We are receiving in small chunks... */
         entry->Data = g_realloc(entry->Data,entry->ValidSize+entry->BuffSize);
         io->IOVec.iov_base = (char *)entry->Data + entry->ValidSize;
         io->IOVec.iov_len  = entry->BuffSize;
      } else {
         /* We have a preallocated buffer! */
         io->IOVec.iov_len  -= Status;
         io->IOVec.iov_base = (char *)io->IOVec.iov_base + Status;
      }
   }
   Cache_process_queue(entry);
}

/*
 * Process redirections (HTTP 30x answers)
 * (This is a work in progress --not finished yet)
 */
static gint Cache_redirect(CacheData_t *entry, gint Flags, BrowserWindow *bw)
{
   DilloUrl *NewUrl;

   if ( ((entry->Flags & CA_Redirect) && entry->Location) &&
        ((entry->Flags & CA_ForceRedirect) || !entry->ValidSize ||
         entry->ValidSize < 1024 ) ) {

      DEBUG_MSG(3, ">>>Redirect from: %s\n to %s\n",
              URL_STR(entry->Url), URL_STR(entry->Location));
      DEBUG_MSG(3,"%s", entry->Header->str);

      if ( Flags & WEB_RootUrl ) {
         /* Redirection of the main page */
         a_Nav_remove_top_url(bw);
         NewUrl = a_Url_new(URL_STR(entry->Location),URL_STR(entry->Url),0,0);
         a_Nav_push(bw, NewUrl);
         a_Url_free(NewUrl);
      } else {
         /* Sub entity redirection (most probably an image) */
         if ( !entry->ValidSize ) {
            DEBUG_MSG(3,">>>Image redirection without entity-content<<<\n");
         } else {
            DEBUG_MSG(3, ">>>Image redirection with entity-content<<<\n");
         }
      }
   }
   return 0;
}

/*
 * Do nothing, but let the cache fill the entry.
 * (Currently used to ignore image redirects  --Jcid)
 */
void a_Cache_null_client(int Op, CacheClient_t *Client)
{
 return;
}

/*
 * Update cache clients for a single cache-entry
 * Tasks:
 *   - Set the client function (if not already set)
 *   - Look if new data is available and pass it to client functions
 *   - Remove clients when done
 *   - Call redirect handler
 *
 * todo: Implement CA_Abort Op in client callback
 */
static void Cache_process_queue(CacheData_t *entry)
{
   gint i;
   CacheClient_t *Client;
   DilloWeb *ClientWeb;
   static gboolean Busy = FALSE;
   gboolean EntryHasClients = FALSE;
   const DilloUrl *Url = entry->Url;

   if ( Busy )
      DEBUG_MSG(5, "FATAL!:*** >>>> Cache_process_queue Caught busy!!!\n");
   Busy = TRUE;
   if ( !(entry->Flags & CA_GotHeader) ) {
      Busy = FALSE;
      return;
   }

   for ( i = 0; (Client = g_slist_nth_data(ClientQueue, i)); ++i ) {
      if ( Client->Url == Url ) {
         EntryHasClients = TRUE;
         ClientWeb = Client->Web; /* It was a (void*) */

         /* For root URLs, clear the "expecting for reply..." message */
         if (ClientWeb->flags & WEB_RootUrl && !(entry->Flags & CA_MsgErased)){
            a_Interface_msg(ClientWeb->bw, "");
            entry->Flags |= CA_MsgErased;
         }
         /* For non root URLs, ignore redirections and 404 answers */
         if ( !(ClientWeb->flags & WEB_RootUrl) &&
              (entry->Flags & CA_Redirect || entry->Flags & CA_NotFound) )
            Client->Callback = a_Cache_null_client;

         /* Set client function */
         if ( !Client->Callback )
            a_Web_dispatch_by_type(entry->Type, ClientWeb,
                                   &Client->Callback, &Client->CbData);
         /* Send data to our client */
         if ( (Client->BufSize = entry->ValidSize) > 0) {
            Client->Buf = (guchar *)entry->Data;
            (Client->Callback)(CA_Send, Client);
         }

         /* Remove client when done */
         if ( (entry->Flags & CA_GotData) ) {
            /* Copy Client data to local vars */
            void *bw = ClientWeb->bw;
            gint flags = ClientWeb->flags;
            /* We finished sending data, let the client know */
            if (!Client->Callback)
               DEBUG_MSG(3, "Client Callback is NULL");
            else
               (Client->Callback)(CA_Close, Client);
            Cache_client_dequeue(Client, NULLKey);
            --i; /* Keep the index value in the next iteration */
            if ( entry->Flags & CA_Redirect )
               Cache_redirect(entry, flags, bw);
         }
      }
   } /* for */

   Busy = FALSE;
   DEBUG_MSG(1, "QueueSize ====> %d\n", g_slist_length(ClientQueue));
}

/*
 * Callback function for Cache_delayed_process_queue.
 */
static gint Cache_delayed_process_queue_callback(gpointer data)
{
   Cache_process_queue( (CacheData_t *)data );
   return FALSE;
}

/*
 * Call Cache_process_queue from the gtk_main cycle
 */
static void Cache_delayed_process_queue(CacheData_t *entry)
{
   gtk_idle_add((GtkFunction)Cache_delayed_process_queue_callback, entry);
}


/*
 * Remove a cache client
 * todo: beware of downloads
 */
static void Cache_remove_client_raw(CacheClient_t *Client, gint Key)
{
   Cache_client_dequeue(Client, Key);
}

/*
 * Remove every Interface-client of a single Url.
 * todo: beware of downloads
 * (this is transitory code)
 */
static void Cache_remove_interface_clients(const DilloUrl *Url)
{
   gint i;
   DilloWeb *Web;
   CacheClient_t *Client;

   for ( i = 0; (Client = g_slist_nth_data(ClientQueue, i)); ++i ) {
      if ( Client->Url == Url ) {
         Web = Client->Web;
         a_Interface_remove_client(Web->bw, Client->Key);
      }
   }
}

/*
 * Remove a client from the client queue
 * todo: notify the dicache and upper layers
 */
static void Cache_stop_client(gint Key, gint force)
{
   CacheClient_t *Client;
   CacheData_t *entry;
   GSList *List;
   DilloUrl *url;

   if (!(List = g_slist_find_custom(ClientQueue, GINT_TO_POINTER(Key),
                                    Cache_client_key_cmp))){
      DEBUG_MSG(5, "WARNING: Cache_stop_client, inexistent client\n");
      return;
   }

   Client = List->data;
   url = (DilloUrl *)Client->Url;
   Cache_remove_client_raw(Client, NULLKey);

   if (force &&
       !g_slist_find_custom(ClientQueue, url, Cache_client_url_cmp)) {
      /* it was the last client of this entry */
      if ((entry = Cache_entry_fast_search(url))) {
         if (entry->CCCQuery) {
            a_Cache_ccc(OpAbort, -1, entry->CCCQuery, NULL, NULL);
         } else if (entry->CCCAnswer) {
            a_Cache_ccc(OpStop, -1, entry->CCCAnswer, NULL, Client);
         }
      }
   }
}

/*
 * Remove a client from the client queue
 * (It may keep feeding the cache, but nothing else)
 */
void a_Cache_stop_client(gint Key)
{
   Cache_stop_client(Key, 0);
}


/*
 * CCC function for the CACHE module
 */
void
 a_Cache_ccc(int Op, int Branch, ChainLink *Info, void *Data, void *ExtraData)
{
   CacheData_t *entry;

   if ( Branch == 1 ) {
      /* Querying branch */
      switch (Op) {
      case OpStart:
         DEBUG_MSG(3, "Cache CCC (OpStart) [0]");
         Info->LocalKey = ExtraData;
         break;
      case OpEnd:
         /* unlink HTTP_Info */
         a_Chain_del_link(Info, CCC_BCK);
         /* 'entry->CCCQuery' and 'Info' point to the same place! */
         if ((entry = Cache_entry_fast_search(Info->LocalKey)) != NULL)
            entry->CCCQuery = NULL;
         g_free(Info);
         DEBUG_MSG(3, "Cache CCC (OpEnd) [0]\n");
         break;
      case OpAbort:
         /* Unlink HTTP_Info */
         a_Chain_del_link(Info, CCC_BCK);

         /* remove interface client-references of this entry */
         Cache_remove_interface_clients(Info->LocalKey);
         /* remove clients of this entry */
         Cache_stop_clients(Info->LocalKey, 0);
         /* remove the entry */
         Cache_entry_remove_raw(NULL, Info->LocalKey);

         g_free(Info);
         DEBUG_MSG(3, "Cache CCC (OpAbort) [0]\n");
         break;
      }

   } else if (Branch == 2) {
      /* Answering branch */
      switch (Op) {
      case OpStart:
         Info->LocalKey = ExtraData;
         if ((entry = Cache_entry_fast_search(Info->LocalKey))) {
            entry->CCCAnswer = Info;
         } else {
            /* The cache-entry was removed */
            a_Chain_bcb(OpAbort, -1, Info, NULL, NULL);
            g_free(Info);
         }
         break;
      case OpSend:
         /* Send data */
         if ((entry = Cache_entry_fast_search(Info->LocalKey))) {
            Cache_process_io(IORead, Data);
         } else {
            a_Chain_bcb(OpAbort, -1, Info, NULL, NULL);
            g_free(Info);
         }
         break;
      case OpEnd:
         /* Unlink HTTP_Info */
         a_Chain_del_link(Info, CCC_BCK);
         g_free(Info);
         Cache_process_io(IOClose, Data);
         DEBUG_MSG(3, "Cache CCC (OpEnd) [1]\n");
         break;
      case OpAbort:
         a_Chain_del_link(Info, CCC_BCK);
         g_free(Info);
         Cache_process_io(IOAbort, Data);
         DEBUG_MSG(3, "Cache CCC (OpAbort) [1]\n");
         break;
      }

   } else if (Branch == -1) {
      /* Backwards operations */
      switch (Op) {
      case OpStop:
         /* We'll let it fill the entry by now... */
         // a_Chain_bcb(OpStop, -1, Info, NULL, NULL);
         // g_free(Info);
         break;
      case OpAbort:
         DEBUG_MSG(2, "Cache: OpAbort [-1]\n");
         Cache_entry_remove_raw(NULL, Info->LocalKey);
         a_Chain_bcb(OpAbort, -1, Info, NULL, NULL);
         g_free(Info);
         break;
      }
   }
}



/*
 * Memory deallocator (only called at exit time)
 */
void a_Cache_freeall(void)
{
   CacheData_t *entry;
   CacheClient_t *Client;

   /* free the client queue */
   while ( (Client = g_slist_nth_data(ClientQueue, 0)) )
      Cache_client_dequeue(Client, NULLKey);

   /* free the main cache */
   while (CacheListSize > 0) {
      entry = &CacheList[0];
      Cache_entry_free(entry);
      a_List_remove(CacheList, 0, CacheListSize);
   }
   g_free(CacheList);
}
