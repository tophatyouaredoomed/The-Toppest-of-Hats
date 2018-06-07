/*
 * File: file.c :)
 *
 * Copyright (C) 2000, 2001 Jorge Arellano Cid <jcid@inf.utfsm.cl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Directory scanning is no longer streamed, but it gets sorted instead!
 * Directory entries on top, files next.
 * Not forked anymore; pthread handled.
 * With new HTML layout.
 */

#include <pthread.h>
#include <ctype.h>           /* for tolower */
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>            /* for rint */

#include <errno.h>           /* for errno */
#include "Url.h"
#include "IO.h"
#include "../list.h"
#include "../web.h"
#include "../interface.h"

typedef struct _DilloDir {
   gint FD_Write, FD_Read;
   char *dirname;
   DIR *dir;
   gboolean scanned;   /* Flag: Have we scanned it? */
   char **dlist;       /* List of subdirectories (for sorting) */
   gint dlist_size;
   gint dlist_max;
   gint dlist_idx;
   char **flist;       /* List of files (for sorting) */
   gint flist_size;
   gint flist_max;
   gint flist_idx;

   pthread_t th1;      /* This transfer's thread id. */
} DilloDir;

typedef struct {
   gint FD_Write,   /* Where we write */
        FD_Read;    /* Where our clients read */
   gint FD;         /* Our local-file descriptor */
   char *Filename;
   const char *ContentType;
   glong FileSize;

   pthread_t th1;      /* This transfer's thread id. */
} DilloFile;



/*
 * Local data
 */


/*
 * Forward references
 */
static char *File_content_type(const char *filename);
static gint File_get_file(const gchar *FileName);
static gint File_get_dir(const gchar *DirName);
static char *File_dir2html(DilloDir *Ddir);
static void File_not_found_msg(DilloWeb *web, const char *filename, int fd);


/*
 * Allocate a DilloFile structure, and set working values in it.
 */
static DilloFile *File_dillofile_new(const char *filename)
{
   gint fds[2], fd;
   struct stat sb;
   DilloFile *Dfile;

   if ( (fd = open(filename, O_RDONLY)) < 0 || pipe(fds) )
      return NULL;

   Dfile = g_new(DilloFile, 1);
   Dfile->FD_Read  = fds[0];
   Dfile->FD_Write = fds[1];
   Dfile->FD = fd;
   Dfile->Filename = g_strdup(filename);
   Dfile->ContentType = File_content_type(filename);
   Dfile->FileSize = fstat(fd, &sb) ? -1 : (glong) sb.st_size;

   return Dfile;
}

/*
 * Deallocate a DilloFile structure.
 */
static void File_dillofile_free(DilloFile *Dfile)
{
   g_free(Dfile->Filename);
   g_free(Dfile);
}

/*
 * Allocate a DilloDir structure, and set safe values in it.
 */
static DilloDir *File_dillodir_new(char *dirname)
{
   DIR *dir;
   gint fds[2];
   DilloDir *Ddir;

   if ( !(dir = opendir(dirname)) || pipe(fds) )
      return NULL;

   Ddir = g_new(DilloDir, 1);
   Ddir->dir = dir;
   Ddir->scanned = FALSE;
   Ddir->dirname = g_strdup(dirname);
   Ddir->FD_Read  = fds[0];
   Ddir->FD_Write = fds[1];
   Ddir->dlist = NULL;
   Ddir->dlist_size = 0;
   Ddir->dlist_max = 256;
   Ddir->dlist_idx = 0;
   Ddir->flist = NULL;
   Ddir->flist_size = 0;
   Ddir->flist_max = 256;
   Ddir->flist_idx = 0;

   return Ddir;
}

/*
 * Deallocate a DilloDir structure.
 */
static void File_dillodir_free(DilloDir *Ddir)
{
   g_free(Ddir->dirname);
   g_free(Ddir);
}

/*
 * Read a local file, and send it through a pipe.
 * (This function runs on its own thread)
 */
static void *File_transfer_file(void *data)
{
   char buf[8192];
   DilloFile *Dfile = data;
   ssize_t nbytes;

   /* Set this thread to detached state */
   pthread_detach(Dfile->th1);

   /* Send content type info */
   sprintf(buf, "Content-Type: %s\n", Dfile->ContentType);
   write(Dfile->FD_Write, buf, strlen(buf));

   /* Send File Size info */
   if (Dfile->FileSize != -1) {
      sprintf(buf, "Content-length: %ld\n", Dfile->FileSize);
      write(Dfile->FD_Write, buf, strlen(buf));
   }
   /* Send end-of-header */
   strcpy(buf, "\n");
   write(Dfile->FD_Write, buf, strlen(buf));


   /* Append raw file contents */
   while ( (nbytes = read(Dfile->FD, buf, 8192)) != 0 ) {
      write(Dfile->FD_Write, buf, nbytes);
   }

   close(Dfile->FD);
   close(Dfile->FD_Write);
   File_dillofile_free(Dfile);
   return NULL;
}

/*
 * Read a local directory, translate it to html, and send it through a pipe.
 * (This function runs on its own thread)
 */
static void *File_transfer_dir(void *data)
{
   char buf[8192],
        *s;
   DilloDir *Ddir = data;

   /* Set this thread to detached state */
   pthread_detach(Ddir->th1);

   /* Send MIME content/type info */
   sprintf(buf, "Content-Type: %s\n\n", File_content_type("dir.html"));
   write(Ddir->FD_Write, buf, strlen(buf));

   /* Send page title */
   sprintf(buf, "<HTML><HEAD><TITLE>%s%s</TITLE></HEAD>\n",
                "file:", Ddir->dirname);
   write(Ddir->FD_Write, buf, strlen(buf));
   sprintf(buf, "<BODY><H1>%s %s</H1>\n<pre>\n", "Directory listing of",
                Ddir->dirname);
   write(Ddir->FD_Write, buf, strlen(buf));

   /* Append formatted directory contents */
   while ( (s = File_dir2html(Ddir)) ){
      write(Ddir->FD_Write, s, strlen(s));
   }

   /* Close open HTML tags */
   sprintf(buf, "\n</pre></BODY></HTML>\n");
   write(Ddir->FD_Write, buf, strlen(buf));

   close(Ddir->FD_Write);
   closedir(Ddir->dir);
   Ddir->dir = NULL;
   File_dillodir_free(Ddir);
   return NULL;
}

/*
 * Return 1 if the extension matches that of the filename.
 */
static gint File_ext(const char *filename, const char *ext)
{
   char *e;

   if ( !(e = strrchr(filename, '.')) )
      return 0;
   return (strcasecmp(ext, ++e) == 0);
}

/*
 * Based on the extension, return the content_type for the file.
 */
static char *File_content_type(const char *filename)
{
   if (File_ext(filename, "gif")) {
      return "image/gif";
   } else if (File_ext(filename, "jpg") || File_ext(filename, "jpeg")) {
      return "image/jpeg";
   } else if (File_ext(filename, "png")) {
      return "image/png";
   } else if (File_ext(filename, "html") || File_ext(filename, "htm") ||
              File_ext(filename, "shtml")) {
      return "text/html";
   }
   return "text/plain";
}

/*
 * Create a new file connection for 'Url', and asynchronously
 * feed the bytes that come back to the cache.
 * ( Data = Requested Url; ExtraData = Web structure )
 */
static void File_get(ChainLink *Info, void *Data, void *ExtraData)
{
   const gchar *path;
   gchar *filename;
   gint fd;
   struct stat sb;
   IOData_t *io;
   const DilloUrl *Url = Data;
   DilloWeb *web = ExtraData;

   path = URL_PATH(Url);
   if (!path || !strcmp(path, ".") || !strcmp(path, "./"))
      /* if there's no path in the URL, show current directory */
      filename = g_get_current_dir();
   else if (!strcmp(path, "~") || !strcmp(path, "~/"))
      filename = g_strdup(g_get_home_dir());
   else
      filename = a_Url_parse_hex_path(Url);

   if ( stat(filename, &sb) != 0 ) {
      /* stat failed, prepare a file-not-found error. */
      fd = -2;
   } else if (S_ISDIR(sb.st_mode)) {
      /* set up for reading directory */
      fd = File_get_dir(filename);
   } else {
      /* set up for reading a file */
      fd = File_get_file(filename);
   }

   if ( fd < 0 ) {
      File_not_found_msg(web, filename, fd);
      a_Chain_fcb(OpAbort, 1, Info, NULL, NULL);

   } else {
      /* End the requesting CCC */
      a_Chain_fcb(OpEnd, 1, Info, NULL, ExtraData);

      /* receive answer */
      io = a_IO_new(fd);
      io->Op = IORead;
      io->IOVec.iov_base = g_malloc(IOBufLen_File);
      io->IOVec.iov_len  = IOBufLen_File;
      io->Flags |= IOFlag_FreeIOVec;
      io->ExtData = (void *) Url;
      a_IO_ccc(OpStart, 2, a_Chain_new(), io, NULL);
   }

   g_free(filename);
   return;
}

/*
 * CCC function for the FILE module
 */
void
 a_File_ccc(int Op, int Branch, ChainLink *Info, void *Data, void *ExtraData)
{
   if ( Op == OpStart ) {
      File_get(Info, Data, ExtraData);
   }
}

/*
 * Create a pipe connection for URL 'url', which is a directory,
 * and feed an ordered html listing through it.
 * (The feeding function runs on its own thread)
 */
static gint File_get_dir(const gchar *DirName)
{
   GString *g_dirname;
   DilloDir *Ddir;

   /* Let's make sure this directory url has a trailing slash */
   g_dirname = g_string_new(DirName);
   if ( g_dirname->str[g_dirname->len - 1] != '/' )
      g_string_append(g_dirname, "/");

   /* Let's get a structure ready for transfer */
   Ddir = File_dillodir_new(g_dirname->str);
   g_string_free(g_dirname, TRUE);
   if ( Ddir ) {
      gint fd = Ddir->FD_Read;
      pthread_create(&Ddir->th1, NULL, File_transfer_dir, Ddir);
      return fd;
   } else
      return -1;
}

/*
 * Create a pipe connection for URL 'url', which is a file,
 * send the MIME content/type through it and then send the file itself.
 * (The feeding function runs on its own thread)
 */
static gint File_get_file(const gchar *FileName)
{
   DilloFile *Dfile;

   /* Create a control structure for this transfer */
   Dfile = File_dillofile_new(FileName);
   if ( Dfile ) {
      gint fd = Dfile->FD_Read;
      pthread_create(&Dfile->th1, NULL, File_transfer_file, Dfile);
      return fd;
   } else
      return -1;
}

/*
 * Return a HTML-line from file info.
 */
static char *File_info2html(struct stat *SPtr, const char *name,
                            const char *dirname, const char *date)
{
   static char *dots = ".. .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. ..";
   static char buf[1200];
   gint size, ndots;
   char *sizeunits;

#define MAXNAMESIZE 30
   char namebuf[MAXNAMESIZE + 1];
   const char *ref;
   char anchor[1024];
   char *cont;
   char *longcont;

   if (!name)
      return NULL;

   if (strcmp(name, "..") == 0) {
      if ( strcmp(dirname, "/") != 0 ){        /* Not the root dir */
         char *parent = g_strdup(dirname);
         char *p;
         /* cut trailing '/' */
         parent[strlen(parent) - 1] = '\0';
         /* make 'parent' have the parent dir path */
         if ( (p = strrchr(parent, '/')) )
            *(p + 1) = '\0';
         else { /* in case name == ".." */
            g_free(parent);
            parent = g_get_current_dir();
            if ( (p = strrchr(parent, '/')) )
              *p = '\0'; /* already cut trailing '/' */
            if ( (p = strrchr(parent, '/')) )
              *(p + 1) = '\0';
         }
         sprintf(buf, "<a href=\"file:%s\">%s</a>\n<p>\n",
                 parent, "Parent directory");
         g_free(parent);
      } else
         strcpy(buf, "<br>\n");
      return buf;
   }

   if (SPtr->st_size <= 9999) {
      size = SPtr->st_size;
      sizeunits = "bytes";
   } else if (SPtr->st_size / 1024 <= 9999) {
      size = rint(SPtr->st_size / 1024.0);
      sizeunits = "Kb";
   } else {
      size = rint(SPtr->st_size / 1048576.0);
      sizeunits = "Mb";
   }
   /* we could note if it's a symlink... */
   if S_ISDIR (SPtr->st_mode) {
      cont = "application/directory";
      longcont = "Directory";
   } else if (SPtr->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
      cont = "application/executable";
      longcont = "Executable";
   } else {
      cont = File_content_type(name);
      longcont = cont;
      if (!cont) {
         cont = "unknown";
         longcont = "";
      }
   }

#if 0
   /* Setup ref before shortening name so we have the full filename */
   if (fileinfo->symlink)
      ref = fileinfo->symlink;
   else
#endif
      ref = name;

   if ( strlen(name) > MAXNAMESIZE) {
      memcpy(namebuf, name, MAXNAMESIZE - 3);
      strcpy(namebuf + (MAXNAMESIZE - 3), "...");
      name = namebuf;
   }
   ndots = MAXNAMESIZE - strlen(name);
   ndots = MAX(ndots, 0);

   if (ref[0] == '/')
      sprintf(anchor, "file:%s", ref);
   else
      sprintf(anchor, "file:%s%s", dirname, ref);
   sprintf(buf,
           /* "<img src=\"internal:icon/%s\">"   Not yet...  --Jcid */
           "%s<a href=\"%s\">%s</a> %s%10s %5d %-5s %s\n",
           S_ISDIR (SPtr->st_mode) ? ">" : " ", anchor, name,
/*
           "<a href=\"%s\">%s</a>%s %s%10s %5d %-5s %s\n",
           anchor, name, S_ISDIR (SPtr->st_mode) ? "/" : " ",
*/
           dots+50-ndots, longcont, size, sizeunits, date);

   return buf;
}

/*
 * Given a timestamp, create a date string and place it in 'datestr'
 */
static void File_get_datestr(time_t *mtime, char *datestr)
{
   time_t currenttime;
   char *ds;

   time(&currenttime);
   ds = ctime(mtime);
   if (currenttime - *mtime > 15811200) {
      /* over about 6 months old */
      sprintf(datestr, "%6.6s  %4.4s", ds + 4, ds + 20);
   } else {
      /* less than about 6 months old */
      sprintf(datestr, "%6.6s %5.5s", ds + 4, ds + 11);
   }
}

/*
 * Compare two strings
 * This function is used for sorting directories
 */
static gint File_comp(const void *a, const void *b)
{
   return strcmp(*(char **)a, *(char **)b);
}

/*
 * Read directory entries to a list, sort them alphabetically, and return
 * formatted HTML, line by line, one per entry.
 * When there're no more entries, return NULL
 */
static char *File_dir2html(DilloDir *Ddir)
{
   struct dirent *de;
   struct stat sb;
   char *name, *s;
   gint *index;

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif
   char fname[MAXPATHLEN + 1];
   char datebuf[64];

   if ( !Ddir->scanned ) {
      /* Lets scan every name and sort them */
      while ((de = readdir(Ddir->dir)) != 0) {
         if (strcmp(de->d_name, ".") == 0)
            continue;              /* skip "." */
         sprintf(fname, "%s/%s", Ddir->dirname, de->d_name);

         if ( stat(fname, &sb) == -1)
            continue;              /* ignore files we can't stat */

         if ( S_ISDIR(sb.st_mode) ) {
            a_List_add(Ddir->dlist, Ddir->dlist_size, sizeof(char *),
                       Ddir->dlist_max);
            Ddir->dlist[Ddir->dlist_size] = g_strdup(de->d_name);
            ++Ddir->dlist_size;
         } else {
            a_List_add(Ddir->flist, Ddir->flist_size, sizeof(char *),
                       Ddir->flist_max);
            Ddir->flist[Ddir->flist_size] = g_strdup(de->d_name);
            ++Ddir->flist_size;
         }
      }
      /* sort the enries */
      qsort(Ddir->dlist, Ddir->dlist_size, sizeof(char *), File_comp);
      qsort(Ddir->flist, Ddir->flist_size, sizeof(char *), File_comp);

      /* Directory scanning done */
      Ddir->scanned = TRUE;
   }

   if ( Ddir->dlist_idx < Ddir->dlist_size ) {
      /* We still have directory entries */
      name = Ddir->dlist[Ddir->dlist_idx];
      index = &Ddir->dlist_idx;
   } else if ( Ddir->flist_idx < Ddir->flist_size ) {
      /* We still have file entries */
      name = Ddir->flist[Ddir->flist_idx];
      index = &Ddir->flist_idx;
   } else {
      g_free(Ddir->flist);
      g_free(Ddir->dlist);
      return NULL;
   }

   sprintf(fname, "%s/%s", Ddir->dirname, name);
   stat(fname, &sb);
   File_get_datestr(&sb.st_mtime, datebuf);
   s = File_info2html(&sb, name, Ddir->dirname, datebuf);
   g_free(name);
   ++(*index);
   return s;
}

/*
 * Give a file-not-found error (an HTML page).
 * Return Value: -1
 */
static void File_not_found_msg(DilloWeb *web, const char *filename, int fd)
{
   if ( web->flags & WEB_RootUrl )
      a_Interface_msg(web->bw, "ERROR: Can't find %s %s",
                         (fd == -2) ? "" : "file", filename);
   else
      g_print("Warning: Can't find <%s>\n", filename);
}


