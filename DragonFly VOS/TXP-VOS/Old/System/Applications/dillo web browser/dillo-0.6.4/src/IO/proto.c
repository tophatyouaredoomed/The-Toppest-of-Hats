/*
 * File: proto.c
 *
 * Copyright (C) 1997 Raph Levien <raph@acm.org>
 * Copyright (C) 1999 Jorge Arellano Cid <jcid@inf.utfsm.cl>,
 *                    Luca Rota <drake@freemail.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>              /* for errno */
#include <signal.h>

#include <sys/types.h>
#include <sys/wait.h>
#include "../web.h"
#include "IO.h"

/* This module handles the Dillo plug-in api. */

/* todo: get abort to work. */

typedef struct _DilloProto DilloProto;

struct _DilloProto {
   const char *prefix;
   const char *handler;
};

static DilloProto plugins[] = {
   {"dpi:", "/usr/local/bin/dpi_test"},
   {"ftp:", "/usr/local/bin/dillo_ftp"}
};

/*
 * Compares the url's prefix with the plugins' prefix
 */
static gint Proto_prefix(const DilloUrl *url, const char *prefix)
{
   return URL_SCHEME(url) ?
          !strncmp(URL_SCHEME(url), prefix, strlen(prefix)) : 1;
}

/*
 * Find the plugin number to handle a given URL, or -1 if there is no
 * plugin for it.
 */
static gint Proto_find(const DilloUrl *url)
{
   guint i;

   for (i = 0; i < sizeof(plugins) / sizeof(DilloProto); i++)
      if (Proto_prefix(url, plugins[i].prefix) &&
          access(plugins[i].handler, X_OK) == 0 )
         return i;
   return -1;
}

#define MAX_ENV 32

/*
 * Build a simple environment with PATH and REQUEST_URI for the execle() call
 */
static void Proto_build_env(char **env, const DilloUrl *url)
{
   gchar *PATH = getenv("PATH");

   env[0] = g_strconcat("PATH=", PATH, NULL);
   env[1] = g_strconcat("REQUEST_URI=", URL_STR(url), NULL);
   env[2] = NULL;
}

/*
 * Search the right handler for the given url, and then execute the plugin
 */
gint a_Proto_get_url(const DilloUrl *Url, void *Data)
{
   char *env[MAX_ENV];
   pid_t child_pid;
   gint filedes[2];
   IOData_t *io;
   gint proto_num = Proto_find(Url);
   // gboolean Error = FALSE;

   if (proto_num < 0)
      return -1;

   Proto_build_env(env, Url);

   if (pipe(filedes) != 0)
      return -1;

   if ((child_pid = fork()) < 0)
      return -1;

   if (!child_pid) {
      /* this is the child */

      signal(SIGCHLD, _exit);
      signal(SIGPIPE, _exit);

      /* redirect stdout to the pipe we just created */
      close(filedes[0]);        /* close read end */
      if (filedes[1] != STDOUT_FILENO) {
         if (dup2(filedes[1], STDOUT_FILENO) != STDOUT_FILENO) {
            g_warning("ERROR in dup2 call for stdout\n");
            _exit(-1);
         }
      }
      execle(plugins[proto_num].handler, plugins[proto_num].handler,
             (char *) NULL, env);
   }

   /* This is the parent */
   /* Receive answer through the IO engine */
   io = g_new0(IOData_t, 1);
   io->Op = IORead;
   io->IOVec.iov_base = g_malloc(IOBufLen_Proto);
   io->IOVec.iov_len  = IOBufLen_Proto;
   io->ExtData = (void *) Url;
   io->FD = filedes[0];
   // a_IO_ccc(OpStart, ...)

   close(filedes[1]);           /* close write end */
   g_free(env[0]);
   g_free(env[1]);
   return filedes[0];
}
