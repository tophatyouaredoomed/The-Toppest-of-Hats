/*
 * File: dns.c
 *
 * Copyright (C) 1999, 2000, 2001 Jorge Arellano Cid <jcid@inf.utfsm.cl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Non blocking pthread-handled Dns scheme
 */

#include <pthread.h>
#include <glib.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "dns.h"
#include "list.h"

#define DEBUG_LEVEL 5
#include "debug.h"


/*
 * Note: comment the following line for debugging or gprof profiling.
 */
#define G_DNS_THREADED

/*
 * Uncomment the following line for libc5 optimization
 */
/* #define LIBC5 */


/* Maximum dns resolving threads */
#ifdef G_DNS_THREADED
#  define G_DNS_MAX_SERVERS 4
#else
#  define G_DNS_MAX_SERVERS 1
#endif


typedef struct {
   gint channel;         /* Index of this channel [0 based] */
   gboolean in_use;      /* boolean to tell if server is doing a lookup */
   gboolean ip_ready;    /* boolean: is IP lookup done? */
   guint32 ip_addr;      /* IP address */
   char *hostname;       /* Adress to resolve */
   guint timeout_id;     /* gtk timeout function ID */
#ifdef G_DNS_THREADED
   pthread_t th1;        /* Thread id */
#endif
} DnsServer;

typedef struct {
   char *hostname;          /* host name for cache */
   guint32 ip_addr;         /* address of host */
} GDnsCache;

typedef struct {
   gint channel;    /* -2 if waiting, otherwise index to dns_server[] */
   char *hostname;  /* The one we're resolving */
   ChainLink *Info; /* CCC info */
} GDnsQueue;


/*
 * Forward declarations
 */
static gint Dns_timeout_client(gpointer data);

/*
 * Local Data
 */
static DnsServer dns_server[G_DNS_MAX_SERVERS];
static gint num_servers;
static GDnsCache *dns_cache;
static gint dns_cache_size, dns_cache_size_max;
static GDnsQueue *dns_queue;
static gint dns_queue_size, dns_queue_size_max;
/* A list for hostnames that haven't been solved */
static GSList *PendingHosts = NULL;


/* ----------------------------------------------------------------------
 *  Dns queue functions
 */
static void Dns_queue_add(ChainLink *Info, gint channel, const char *hostname)
{
   a_List_add(dns_queue,dns_queue_size,sizeof(*dns_queue),dns_queue_size_max);
   dns_queue[dns_queue_size].Info = Info;
   dns_queue[dns_queue_size].channel = channel;
   dns_queue[dns_queue_size].hostname = g_strdup(hostname);
   dns_queue_size++;
}

/*
 * Find hostname index in dns_queue
 * (if found, returns queue index; -1 if not)
 */
static gint Dns_queue_find(const char *hostname)
{
   gint i;

   for (i = 0; i < dns_queue_size; i++)
      if (!strcmp(hostname, dns_queue[i].hostname))
         return i;

   return -1;
}

/*
 * Given an index, remove an entry from the dns_queue
 */
static void Dns_queue_remove(int index)
{
   gint i;

   DEBUG_MSG(2, "Dns_queue_remove: deleting client [%d] [queue_size=%d]\n",
             index, dns_queue_size);

   if (index < dns_queue_size) {
      g_free(dns_queue[index].hostname);
      --dns_queue_size;         /* you'll find out why ;-) */
      for (i = index; i < dns_queue_size; i++)
         dns_queue[i] = dns_queue[i + 1];
   }
}

/*
 * Debug function
 *
void Dns_queue_print()
{
   gint i;

   g_print("Queue: [");
   for (i = 0; i < dns_queue_size; i++)
      g_print("%d:%s ", dns_queue[i].channel, dns_queue[i].hostname);
   g_print("]\n");
}
 */

/*
 *  Given a CCC Info, remove its client
 */
static void Dns_abort_by_ccc(ChainLink *Info)
{
   gint i;

   for (i = 0; i < dns_queue_size; i++)
      if (dns_queue[i].Info == Info) {
         Dns_queue_remove(i);
         break;
      }
}

/*
 *  Add an IP/hostname pair to Dns-cache
 */
static void Dns_cache_add(char *hostname, guint32 ip_addr)
{
   a_List_add(dns_cache,dns_cache_size,sizeof(*dns_cache),dns_cache_size_max);
   dns_cache[dns_cache_size].hostname = g_strdup(hostname);
   dns_cache[dns_cache_size].ip_addr = ip_addr;
   ++dns_cache_size;
   DEBUG_MSG(1, "Cache objects: %d\n", dns_cache_size);
}


/*
 *  Initializer function
 */
void a_Dns_init(void)
{
   gint i;

   DEBUG_MSG(5, "dillo_dns_init: Here we go!\n");
   dns_queue_size = 0;
   dns_queue_size_max = 16;
   dns_queue = g_new(GDnsQueue, dns_queue_size_max);

   dns_cache_size = 0;
   dns_cache_size_max = 16;
   dns_cache = g_new(GDnsCache, dns_cache_size_max);

   num_servers = G_DNS_MAX_SERVERS;

   /* Initialize servers data */
   for (i = 0; i < num_servers; ++i) {
      dns_server[i].channel = i;
      dns_server[i].in_use = FALSE;
      dns_server[i].ip_ready = FALSE;
      dns_server[i].ip_addr = 0;
      dns_server[i].hostname = NULL;
      dns_server[i].timeout_id = -1;
#ifdef G_DNS_THREADED
      dns_server[i].th1 = -1;
#endif
   }
}

/*
 *  Server function (runs on its own thread)
 */
static void *Dns_server(void *data)
{
   struct hostent *host;
   guint32 ip_addr;
   gint channel = GPOINTER_TO_INT(data);
#ifdef LIBC5
   gint h_err;
   char buff[1024];
   struct hostent sh;
#endif

#ifdef G_DNS_THREADED
   /* Set this thread to detached state */
   pthread_detach(dns_server[channel].th1);
#endif

   DEBUG_MSG(3, "Dns_server: starting...\n ch: %d host: %s\n",
             channel, dns_server[channel].hostname);

#ifdef LIBC5
   host = gethostbyname_r(dns_server[channel].hostname, &sh, buff,
                          sizeof(buff), &h_err);
#else
   host = gethostbyname(dns_server[channel].hostname);
#endif

   if (host == NULL) {
      ip_addr = 0;
   } else {
      memcpy(&ip_addr, host->h_addr_list[0], sizeof(ip_addr));
      ip_addr = ntohl(ip_addr);
   }

   /* write hostname to client */
   DEBUG_MSG(5, "Dns_server [%d]: %s is %x\n", channel,
             dns_server[channel].hostname, ip_addr);
   dns_server[channel].ip_addr = ip_addr;
   dns_server[channel].ip_ready = TRUE;

   return NULL;                 /* (avoids a compiler warning) */
}


#ifdef G_DNS_THREADED
#else
/*
 *  Blocking server-function (it doesn't use threads)
 */
static void Dns_blocking_server(void)
{
   struct hostent *host;
   guint32 ip_addr;
   gint index;

   index = 0;
   DEBUG_MSG(3, "Dns_blocking_server: starting...\n");
   DEBUG_MSG(3, "Dns_blocking_server: dns_server[%d].hostname = %s\n",
             index, dns_server[index].hostname);

   host = gethostbyname(dns_server[index].hostname);
   if (host == NULL) {
      DEBUG_MSG(3, "--> Dns_blocking_server: gethostbyname NULL return\n");
      ip_addr = 0;
   } else {
      DEBUG_MSG(3, "--> Dns_blocking_server - good return\n");
      memcpy(&ip_addr, host->h_addr_list[0], sizeof(ip_addr));
      ip_addr = ntohl(ip_addr);
   }

   /* write IP to server data channel */
   DEBUG_MSG(3, "Dns_blocking_server: IP of %s is %x\n",
             dns_server[index].hostname, ip_addr);
   dns_server[index].ip_addr = ip_addr;
   dns_server[index].ip_ready = TRUE;

   DEBUG_MSG(3, "Dns_blocking_server: leaving...\n");
}
#endif

/*
 *  Request function (spawn a server and let it handle the request)
 */
static void Dns_server_req(gint channel, const char *hostname)
{
   dns_server[channel].in_use = TRUE;
   dns_server[channel].ip_ready = FALSE;

   g_free(dns_server[channel].hostname);
   dns_server[channel].hostname = g_strdup(hostname);

   /* Let's set a timeout client to poll the server channel (5 times/sec) */
   dns_server[channel].timeout_id =
      gtk_timeout_add(200, (GtkFunction)Dns_timeout_client,
                      GINT_TO_POINTER(dns_server[channel].channel));

#ifdef G_DNS_THREADED
   /* Spawn thread */
   pthread_create(&dns_server[channel].th1, NULL, Dns_server,
                  GINT_TO_POINTER(dns_server[channel].channel));
#else
   Dns_blocking_server();
#endif
}

/*
 * Return the IP for the given hostname.
 * Side effect: a thread can be spawned later.
 */
static void Dns_lookup(ChainLink *Info, const char *hostname)
{
   gint i, channel;

   g_return_if_fail (hostname != NULL);

   /* check for cache hit. */
   for (i = 0; i < dns_cache_size; i++)
      if (!strcmp(hostname, dns_cache[i].hostname))
         break;

   /* if it hits already resolved, call the Callback inmediately. */
   if (i < dns_cache_size) {
      a_Dns_ccc(OpSend, 1, Info, &dns_cache[i].ip_addr, NULL);
      a_Dns_ccc(OpEnd, 1, Info, NULL, NULL);
      return;
   }
   if ((i = Dns_queue_find(hostname)) != -1) {
      /* hit in queue, but answer hasn't come back yet. */
      Dns_queue_add(Info, dns_queue[i].channel, hostname);
   } else {
      /* Never requested before -- we must solve it! */

      /* Find a channel we can send the request to */
      for (channel = 0; channel < num_servers; channel++)
         if (!dns_server[channel].in_use)
            break;
      if (channel < num_servers) {
         /* Found a free channel! */
         Dns_queue_add(Info, channel, hostname);
         Dns_server_req(channel, hostname);
      } else {
         /* We'll have to wait for a thread to finish... */
         Dns_queue_add(Info, -2, hostname);
      }
   }
}

/*
 * Give answer to all queued callbacks on this channel
 */
static void Dns_serve_channel(gint channel)
{
   gint i;
   ChainLink *Info;
   DnsServer *srv = &dns_server[channel];

   for (i = 0; i < dns_queue_size; i++) {
      if (dns_queue[i].channel == channel) {
         Info = dns_queue[i].Info;
         if ( srv->ip_addr == 0 ) {
            a_Dns_ccc(OpAbort, 1, Info, NULL, NULL);
         } else {
            a_Dns_ccc(OpSend, 1, Info, &srv->ip_addr, NULL);
            a_Dns_ccc(OpEnd, 1, Info, NULL, NULL);
         }
         Dns_queue_remove(i);
         --i;
      }
   }
   /* set current channel free */
   srv->in_use = FALSE;
}

/*
 * Assign free channels to waiting clients (-2)
 */
static void Dns_assign_channels(void)
{
   gint ch, i, j;

   for (ch = 0; ch < num_servers; ++ch) {
      if (dns_server[ch].in_use == FALSE) {
         /* Find the next query in the queue (we're a FIFO) */
         for (i = 0; i < dns_queue_size; i++)
            if (dns_queue[i].channel == -2)
               break;

         if (i < dns_queue_size) {
            /* assign this channel to every queued request
             * with the same hostname*/
            for (j = i; j < dns_queue_size; j++)
               if (dns_queue[j].channel == -2 &&
                   !strcmp(dns_queue[j].hostname, dns_queue[i].hostname))
                  dns_queue[j].channel = ch;
            Dns_server_req(ch, dns_queue[i].hostname);
         } else
            return;
      }
   }
}

/*
 * This is a Gtk+ timeout function that
 * reads the DNS results and resumes the stopped jobs.
 */
static gint Dns_timeout_client(gpointer data)
{
   gint channel = GPOINTER_TO_INT(data);
   DnsServer *srv = &dns_server[channel];

   if ( srv->ip_ready ){
      if (srv->ip_addr != 0) {
         /* DNS succeeded, let's cache it */
         Dns_cache_add(srv->hostname, srv->ip_addr);
      }
      Dns_serve_channel(channel);
      Dns_assign_channels();
      return FALSE;  /* Done! */
   }

   /* IP not already solved, keep on trying... */
   return TRUE;
}

/*
 * CCC function for the DNS module
 * ( Data = hostname | IP; ExtraData = {Not used here} )
 */
void
 a_Dns_ccc(int Op, int Branch, ChainLink *Info, void *Data, void *ExtraData)
{
   gchar *Hostname;

   if ( Branch == 1 ){
      switch (Op) {
      case OpStart:
         Hostname = g_strdup((gchar *)Data);
         PendingHosts = g_slist_append(PendingHosts, Hostname);
         Info->LocalKey = Hostname;
         Dns_lookup(Info, Hostname);
         break;
      case OpSend:
         a_Chain_fcb(OpSend, 1, Info, Data, NULL);
         break;
      case OpEnd:
         Hostname = Info->LocalKey;
         a_Chain_fcb(OpEnd, 1, Info, NULL, NULL);
         PendingHosts = g_slist_remove(PendingHosts, Hostname);
         g_free(Hostname);
         break;
      case OpAbort:
         Hostname = Info->LocalKey;
         a_Chain_fcb(OpAbort, 1, Info, NULL, NULL);
         PendingHosts = g_slist_remove(PendingHosts, Hostname);
         g_free(Hostname);
         break;
      }

   } else if ( Branch == -1 ) {
      switch (Op) {
      case OpAbort:
         Dns_abort_by_ccc(Info);
         PendingHosts = g_slist_remove(PendingHosts, Info->LocalKey);
         g_free(Info->LocalKey);
         g_free(Info);
         break;
      }
   }
}

/*
 *  Dns memory-deallocation
 *  (Call this one at exit time)
 *  The Dns_queue is deallocated at execution time (no need to do that here)
 *  'dns_cache' is the only one that grows dinamically
 */
void a_Dns_freeall(void)
{
   gint i;

   for ( i = 0; i < dns_cache_size; ++i ){
      g_free(dns_cache[i].hostname);
   }
   g_free(dns_cache);
}

