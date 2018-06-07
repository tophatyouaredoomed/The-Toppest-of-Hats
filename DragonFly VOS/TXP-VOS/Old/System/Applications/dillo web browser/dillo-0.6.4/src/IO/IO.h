#ifndef __IO_H__
#define __IO_H__

#include<unistd.h>
#include<sys/uio.h>

#include "../chain.h"

/*
 * IO Operations
 */
#define IORead   0
#define IOWrite  1
#define IOWrites 2
#define IOClose  3
#define IOAbort  4

/*
 * IO Flags
 */
#define IOFlag_FreeIOVec   (1 << 1)
#define IOFlag_ForceClose  (1 << 2)

/*
 * IO constants
 */
#define IOBufLen_Http    4096
#define IOBufLen_File    4096
#define IOBufLen_Proto   4096
#define IOBufLen_About   4096


typedef struct {
   gint Key;              /* Primary Key (for klist) */
   gint Op;               /* IORead | IOWrite | IOWrites */
   gint FD;               /* Current File Descriptor */
   glong Status;          /* Number of bytes read, or -errno code */
   struct iovec IOVec;    /* Buffer place and length */
   gint Flags;            /* Flag array (look definitions above) */
   void *ExtData;         /* External data reference (not used by IO.c) */
   void *Info;            /* CCC Info structure for this IO */
   GIOChannel *GioCh;     /* IO channel */
   gint GdkTag;           /* gdk_input tag (used to remove) */
} IOData_t;


/*
 * Exported functions
 */
IOData_t* a_IO_new(gint fd);
void a_IO_ccc(int Op, int Br, ChainLink *Info, void *Data, void *ExtraData);

#endif /* __IO_H__ */

