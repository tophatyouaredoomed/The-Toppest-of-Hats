/*
 * File: IO.c
 *
 * Copyright (C) 2000, 2001, 2002 Jorge Arellano Cid <jcid@inf.utfsm.cl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Dillo's signal driven IO engine
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <glib.h>
#include <gdk/gdk.h>
#include "../chain.h"
#include "../klist.h"
#include "IO.h"

//#define DEBUG_LEVEL 3
#include "../debug.h"


/*
 * Symbolic defines for shutdown() function
 * (Not defined in the same header file, for all distros --Jcid)
 */
#define IO_StopRd   0
#define IO_StopWr   1
#define IO_StopRdWr 2


/*
 * Local data
 */
static Klist_t *ValidIOs = NULL; /* Active IOs list. It holds pointers to
                                  * IOData_t structures. */

/*
 * Return a newly created, and initialized, 'io' struct
 */
IOData_t *a_IO_new(gint fd)
{
   IOData_t *io = g_new0(IOData_t, 1);
   io->GioCh = g_io_channel_unix_new(fd);
   io->FD = fd;
   io->Flags = 0;
   io->Key = 0;
   return io;
}

/*
 * Register an IO in ValidIOs
 */
static void IO_ins(IOData_t *io)
{
   io->Key = a_Klist_insert(&ValidIOs, (gpointer)io);
}

/*
 * Remove an IO from ValidIOs
 */
static void IO_del(IOData_t *io)
{
   a_Klist_remove(ValidIOs, io->Key);
}

/*
 * Return a io by its Key (NULL if not found)
 */
static IOData_t *IO_get(gint Key)
{
   return a_Klist_get_data(ValidIOs, Key);
}

/*
 * Free an 'io' struct
 */
static void IO_free(IOData_t *io)
{
  g_return_if_fail(IO_get(io->Key) == NULL);

  if (io->Flags & IOFlag_FreeIOVec)
     g_free(io->IOVec.iov_base);
  g_free(io);
}

/*
 * Close an open FD, and remove io controls.
 * (This function can be used for Close and Abort operations)
 */
static void IO_close_fd(IOData_t *io, gint CloseCode)
{
   /* With HTTP, if we close the writing part, the reading one also gets
    * closed! (other clients may set 'IOFlag_ForceClose') */
   if ((io->Flags & IOFlag_ForceClose) || (CloseCode != IO_StopWr))
      close(io->FD);

   /* Remove this IOData_t reference, from our ValidIOs list
    * We don't deallocate it here, just remove from the list.*/
   IO_del(io);
}

/*
 * Abort an open FD.
 *  This function is called to abort a FD connection due to an IO error
 *  or just because the connection is not required anymore.
 */
static gboolean IO_abort(IOData_t *io)
{
   /* Close and finish this FD's activity */
   IO_close_fd(io, IO_StopRdWr);

   return FALSE;
}

/*
 * Read data from a file descriptor into a specific buffer
 */
static gboolean IO_read(IOData_t *io)
{
   ssize_t St;
   gboolean ret, DataPending;

   DEBUG_MSG(3, "  IO_read2\n");

   do {
      ret = FALSE;
      DataPending = FALSE;

      St = readv(io->FD, &io->IOVec, 1);
      io->Status = St;
      DEBUG_MSG(3, "  IO_read2: %s [errno %d] [St %d]\n",
                g_strerror(errno), errno, St);

      if ( St < 0 ) {
         // Error
         io->Status = -errno;
         if (errno == EINTR)
            ret = TRUE;
         else if (errno == EAGAIN)
            ret = TRUE;

      } else if ( St == 0 ) {
         // All data read (EOF)
         IO_close_fd(io, IO_StopRd);
         a_IO_ccc(OpEnd, 2, io->Info, io, NULL);

      } else if ( St < io->IOVec.iov_len ){
         // We have all the new data
         a_IO_ccc(OpSend, 2, io->Info, io, NULL);
         ret = TRUE;

      } else { // BytesRead == io->IOVec.iov_len
         // We have new data, and maybe more...
         a_IO_ccc(OpSend, 2, io->Info, io, NULL);
         DataPending = TRUE;
      }
   } while (DataPending);

   return ret;
}

/*
 * Write data, from a specific buffer, into a file descriptor
 * (** Write operations MUST NOT free the buffer because the buffer
 *     start is modified.)
 * todo: Implement IOWrites, remove the constraint stated above.
 */
static gboolean IO_write(IOData_t *io)
{
   ssize_t St;
   gboolean ret = FALSE;

   DEBUG_MSG(3, "  IO_write\n");

      St = writev(io->FD, &io->IOVec, 1);
      io->Status = St;
      DEBUG_MSG(3, "  IO_write: %s [errno %d] [St %d]\n",
                g_strerror(errno), errno, St);

      if ( St < 0 ) {
         // Error
         io->Status = -errno;
         if (errno == EINTR)
            ret = TRUE;
         else if (errno == EAGAIN)
            ret = TRUE; // todo: ???

      } else if ( St < io->IOVec.iov_len ){
         // Not all data written
         io->IOVec.iov_len  -= St;
         io->IOVec.iov_base = (gchar *)io->IOVec.iov_base + St;
         ret = TRUE;

      } else {
         // All data in buffer written
         if ( io->Op == IOWrite ) {
            /* Single write */
            IO_close_fd(io, IO_StopWr);
            a_IO_ccc(OpEnd, 1, io->Info, io, NULL);
         } else if ( io->Op == IOWrites ) {
            /* todo: Writing in small chunks (not implemented) */
         }
      }

   return ret;
}

/*
 * Handle background IO for a given FD (reads | writes)
 * (This function gets called by glib when there's activity in the FD)
 */
static gboolean IO_callback(GIOChannel *src, GIOCondition cond, gpointer data)
{
   gboolean ret = FALSE;
   IOData_t *io = IO_get(GPOINTER_TO_INT(data));

   DEBUG_MSG(3, " IO_callback: [GIOcond %d]\n", cond);

   /* Sometimes glib delivers events on already aborted FDs  --Jcid */
   if ( io == NULL ) {
      DEBUG_MSG(3, " IO_callback: call on already closed io!\n");
      return FALSE;
   }

   if ( cond & (G_IO_IN | G_IO_HUP) ){             /* Read */
      ret = IO_read(io);
   } else if ( cond & G_IO_OUT ){     /* Write */
      while ( IO_write(io) );
   }

   if ( cond & G_IO_ERR ){     /* Error */
      io->Status = -EIO;
      ret = IO_abort(io);
   } else if ( cond & (G_IO_PRI | G_IO_NVAL) ){
      /* Ignore these exceptional conditions */
      ret = FALSE;
   }

   return ret;
}

/*
 * Receive an IO request (IORead | IOWrite | IOWrites),
 * Set the GIOChannel and let it flow!
 */
static void IO_submit(IOData_t *r_io)
{
   /* Insert this IO in ValidIOs */
   IO_ins(r_io);

   /* Set FD to background and to generate signals */
   fcntl(r_io->FD, F_SETFL, O_NONBLOCK | fcntl(r_io->FD, F_GETFL) );

   if ( r_io->Op == IORead ) {
      g_io_add_watch(r_io->GioCh, G_IO_IN | G_IO_ERR | G_IO_HUP,
                     IO_callback, GINT_TO_POINTER (r_io->Key));
      g_io_channel_unref(r_io->GioCh);

   } else if ( r_io->Op == IOWrite || r_io->Op == IOWrites ) {
      g_io_add_watch(r_io->GioCh, G_IO_OUT | G_IO_ERR, 
                     IO_callback, GINT_TO_POINTER (r_io->Key));
      g_io_channel_unref(r_io->GioCh);
   }
}

/*
 * CCC function for the IO module
 * ( Data = IOData_t* ; ExtraData = NULL )
 */
void a_IO_ccc(int Op, int Branch, ChainLink *Info, void *Data, void *ExtraData)
{
   IOData_t *io = Data;

   if ( Branch == 1 ) {
      /* Send query */
      switch (Op) {
      case OpStart:
         io->Info = Info;
         Info->LocalKey = io;
         IO_submit(io);
         break;
      case OpEnd:
         a_Chain_fcb(OpEnd, 2, Info, io, NULL);
         IO_free(io);
         break;
      case OpAbort:
         a_Chain_fcb(OpAbort, 2, Info, NULL, NULL);
         IO_free(io);
         break;
      }

   } else if ( Branch == 2 ) {
      /* Receive answer */
      switch (Op) {
      case OpStart:
         io->Info = Info;
         Info->LocalKey = io;
         a_Chain_link_new(a_IO_ccc, Info, CCC_BCK, a_Cache_ccc);
         a_Chain_fcb(OpStart, 2, Info, io, io->ExtData);
         IO_submit(io);
         break;
      case OpSend:
         a_Chain_fcb(OpSend, 2, Info, io, NULL);
         break;
      case OpEnd:
         a_Chain_fcb(OpEnd, 2, Info, io, NULL);
         IO_free(io);
         break;
      case OpAbort:
         a_Chain_fcb(OpAbort, 2, Info, io, NULL);
         IO_free(io);
         break;
      }

   } else if ( Branch == -1 ) {
      /* Backwards call */
      switch (Op) {
      case OpAbort:
         DEBUG_MSG(3, "IO   : OpAbort [-1]\n");
         io = Info->LocalKey;
         IO_abort(io);
         IO_free(io);
         g_free(Info);
         break;
      }
   }
}

