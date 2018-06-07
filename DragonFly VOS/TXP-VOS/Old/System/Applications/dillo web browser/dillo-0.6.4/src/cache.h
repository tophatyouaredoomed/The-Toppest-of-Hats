#ifndef __CACHE_H__
#define __CACHE_H__

#include <glib.h>
#include "chain.h"
#include "url.h"

/*
 * Cache Op codes
 */
#define CA_Send    (0)  /* Normal update */
#define CA_Close   (1)  /* Successful operation close */
#define CA_Abort   (2)  /* Operation abort */

/*
 * Flag Defines
 */
#define CA_GotHeader      (1)  /* True if header is completely got */
#define CA_GotLength      (2)  /* True if we have all Data in cache */
#define CA_GotData        (4)  /* True if we have all Data in cache */
#define CA_FreeData       (8)  /* Free the cache Data on close */
#define CA_Redirect      (16)  /* Data actually points to a redirect */
#define CA_ForceRedirect (32)  /* Unconditional redirect */
#define CA_NotFound      (64)  /* True if remote server didn't found the URL */
#define CA_Stopped      (128)  /* True if the entry has been stopped */
#define CA_MsgErased    (256)  /* Used to erase the bw's status bar */

/*
 * Callback type for cache clients
 */
typedef struct _CacheClient CacheClient_t;
typedef void (*CA_Callback_t)(int Op, CacheClient_t *Client);

/*
 * Data structure for cache clients.
 */
struct _CacheClient {
   gint Key;                /* Primary Key for this client */
   const DilloUrl *Url;     /* Pointer to a cache entry Url */
   guchar *Buf;             /* Pointer to cache-data */
   guint BufSize;           /* Valid size of cache-data */
   CA_Callback_t Callback;  /* Client function */
   void *CbData;            /* Client function data */
   void *Web;               /* Pointer to the Web structure of our client */
};

/*
 * Function prototypes
 */
gint a_Cache_open_url(void *Web, CA_Callback_t Call, void *CbData);

char *a_Cache_url_read(const DilloUrl *url, gint *size);
void a_Cache_freeall(void);
void a_Cache_null_client(int Op, CacheClient_t *Client);
void a_Cache_stop_client(gint Key);


#endif /* __CACHE_H__ */

