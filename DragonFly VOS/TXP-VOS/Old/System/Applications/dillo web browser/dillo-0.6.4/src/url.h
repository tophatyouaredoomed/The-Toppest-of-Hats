/*
 * File : url.h - Dillo
 *
 * Copyright (C) 2001 Jorge Arellano Cid   <jcid@users.sourceforge.net>
 *               2001 Livio Baldini Soares <livio@linux.ime.usp.br>
 *
 * Parse and normalize all URL's inside Dillo.
 */

#ifndef __URL_H__
#define __URL_H__

#include <string.h>       /* for strcmp */
#include <glib.h>


#define DILLO_URL_HTTP_PORT        80
#define DILLO_URL_HTTPS_PORT       443
#define DILLO_URL_FTP_PORT         21
#define DILLO_URL_MAILTO_PORT      25
#define DILLO_URL_NEWS_PORT        119
#define DILLO_URL_TELNET_PORT      23
#define DILLO_URL_GOPHER_PORT      70


/*
 * Values for DilloUrl->flags.
 * Specifies which which action to perform with an URL.
 */
#define URL_Get                 0x00000001
#define URL_Post                0x00000002
#define URL_ISindex             0x00000004
#define URL_Ismap               0x00000008
#define URL_RealmAccess         0x00000010

#define URL_E2EReload           0x00000020
#define URL_ReloadImages        0x00000040
#define URL_ReloadPage          0x00000080
#define URL_ReloadFromCache     0x00000100

#define URL_ReloadIncomplete    0x00000200

/* Access methods to fields inside DilloURL */
#define URL_SCHEME(u)     u->scheme
#define URL_AUTHORITY(u)  u->authority
#define URL_PATH(u)       u->path
#define URL_QUERY(u)      u->query
#define URL_FRAGMENT(u)   u->fragment
#define URL_HOST(u)       a_Url_hostname(u)
#define URL_PORT(u)       (URL_HOST(u) ? u->port : u->port)
#define URL_FLAGS(u)      u->flags
#define URL_DATA(u)       u->data
#define URL_ALT(u)        u->alt
#define URL_POS(u)        u->scrolling_position
#define URL_STR(u)        a_Url_str(u)

/* URL-camp compare methods */
#define URL_STRCAMP_EQ(s1,s2) \
   ((!(s1) && !(s2)) || ((s1) && (s2) && !strcmp(s1,s2)))
#define URL_STRCAMP_I_EQ(s1,s2) \
   ((!(s1) && !(s2)) || ((s1) && (s2) && !g_strcasecmp(s1,s2)))
#define URL_GSTRCAMP_EQ(s1,s2) \
   ((!(s1) && !(s2)) || ((s1) && (s2) && !strcmp((s1)->str,(s2)->str)))
#define URL_GSTRCAMP_I_EQ(s1,s2) \
   ((!(s1) && !(s2)) || ((s1) && (s2) && !g_strcasecmp((s1)->str,(s2)->str)))


typedef struct _DilloUrl DilloUrl;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct _DilloUrl {
   GString *url_string;
   const gchar *buffer;
   const gchar *scheme;            //
   const gchar *authority;         //
   const gchar *path;              // These are references only
   const gchar *query;             // (no need to free them)
   const gchar *fragment;          //
   const gchar *hostname;          //
   gint port;
   gint flags;
   const gchar *data;              /* POST */
   const gchar *alt;               /* "alt" text (used by image maps) */
   gint ismap_url_len;             /* Used by server side image maps */
   gint scrolling_position;        /* remember position of visited urls */
};


DilloUrl* a_Url_new(const gchar *url_str, const gchar *base_url,
                    gint flags, gint pos);
void a_Url_free(DilloUrl *u);
gchar *a_Url_str(const DilloUrl *url);
const gchar *a_Url_hostname(const DilloUrl *u);
DilloUrl* a_Url_dup(const DilloUrl *u);
gint a_Url_cmp(const DilloUrl* A, const DilloUrl* B);
void a_Url_set_flags(DilloUrl *u, gint flags);
void a_Url_set_data(DilloUrl *u, gchar *data);
void a_Url_set_alt(DilloUrl *u, const gchar *alt);
void a_Url_set_pos(DilloUrl *u, gint pos);
void a_Url_set_ismap_coords(DilloUrl *u, gchar *coord_str);
gchar *a_Url_parse_hex_path(const DilloUrl *u);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __URL_H__ */
