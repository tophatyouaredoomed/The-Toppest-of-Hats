#ifndef __IO_URL_H__
#define __IO_URL_H__

#include <glib.h>
#include "../../config.h"
#include "../chain.h"
#include "../url.h"
#include "IO.h"


/*
 * URL opener codes
 */
#define URL_Http
#define URL_File
#define URL_Plugin
#define URL_None


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Returns a file descriptor to read data on; meta data is sent on FD_TypeWrite */
typedef gint (*UrlOpener_t) (const DilloUrl *url, void *data);

/*
 * Module functions
 */
ChainFunction_t a_Url_get_ccc_funct(const DilloUrl *Url);


/*
 * External functions
 */
extern void a_Http_freeall(void);
gint a_Http_init(void);

void a_Http_ccc (int Op, int Br, ChainLink *Info, void *Data, void *ExtraData);
void a_File_ccc (int Op, int Br, ChainLink *Info, void *Data, void *ExtraData);
void a_About_ccc(int Op, int Br, ChainLink *Info, void *Data, void *ExtraData);
void a_IO_ccc   (int Op, int Br, ChainLink *Info, void *Data, void *ExtraData);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __IO_URL_H__ */

