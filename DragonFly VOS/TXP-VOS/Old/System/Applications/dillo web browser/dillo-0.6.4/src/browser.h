#ifndef __DILLO_BROWSER_H__
#define __DILLO_BROWSER_H__

#include <sys/types.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "url.h"     /* for DilloUrl */


typedef struct _BrowserWindow BrowserWindow;
typedef struct _DilloMenuPopup DilloMenuPopup;

typedef struct {
    DilloUrl *Url;   /* URL-key for this cache connection */
    gint Flags;      /* {WEB_RootUrl, WEB_Image, WEB_Download} */
} BwUrls;

/* The popup menus so that we can call them. */
struct _DilloMenuPopup
{
   GtkWidget *over_page;
   GtkWidget *over_link;
   GtkWidget *over_back;
   GtkWidget *over_forw;
   DilloUrl *url;
};

/* browser_window contains all widgets to create a single window */
struct _BrowserWindow
{
   /* Control-Panel handleboxes --used for hiding */
   GSList *PanelHandles;

   /* widgets for the main window */
   GtkWidget *main_window;
   GtkWidget *back_button;
   GtkWidget *forw_button;
   GtkWidget *home_button;
   GtkWidget *reload_button;
   GtkWidget *save_button;
   GtkWidget *stop_button;
   GtkWidget *menubar;
   GtkWidget *clear_url_button;
   GtkWidget *location;
   GtkWidget *progress_box;
   GtkWidget *status;
   gint status_is_link;
   GtkWidget *imgprogress;
   GtkWidget *progress;

   /* the keyboard accelerator table */
   GtkAccelGroup *accel_group;

   /* Popup menu for this BrowserWindow */
   DilloMenuPopup menu_popup;

   /* The bookmarks menu so that we can add things to it. */
   GtkWidget *bookmarks_menu;

   /* The "Headings" and "Anchors" menus */
   GtkWidget *pagemarks_menuitem;
   GtkWidget *pagemarks_menu;
   GtkWidget *pagemarks_last;

   /* This is the main document widget. (HTML rendering or whatever) */
   GtkWidget *docwin;

   /* Current cursor type */
   GdkCursorType CursorType;

   /* A list of active cache clients in the window (The primary Key) */
   gint *RootClients;
   gint NumRootClients;
   gint MaxRootClients;

   /* Image Keys for all active connections in the window */
   gint *ImageClients;
   gint NumImageClients;
   gint MaxImageClients;
   /* Number of different images in the page */
   gint NumImages;
   /* Number of different images already loaded */
   gint NumImagesGot;

   /* List of all Urls requested by this page (and its types) */
   BwUrls *PageUrls;
   gint NumPageUrls;
   gint MaxPageUrls;

   /* widgets for dialog boxes off main window */
   GtkWidget *open_dialog_window;
   GtkWidget *open_dialog_entry;
   GtkWidget *openfile_dialog_window;
   GtkWidget *quit_dialog_window;
   GtkWidget *save_dialog_window;
   GtkWidget *save_link_dialog_window;
   GtkWidget *findtext_dialog_window;
   GtkWidget *findtext_dialog_entry;
   gpointer  findtext_data;
   GtkWidget *question_dialog_window;
   gpointer  question_dialog_data;
   GtkWidget *viewsource_window;

   /* Dillo navigation stack (holds indexes to history list) */
   gint *nav_stack;
   gint nav_stack_size;       /* [1 based] */
   gint nav_stack_size_max;
   /* 'nav_stack_ptr' refers to what's being displayed */
   gint nav_stack_ptr;        /* [0 based] */
   /* When the user clicks a link, the URL isn't pushed directly to history;
    * nav_expect_url holds it until the first answer-bytes are got. Only then
    * it is sent to history and referenced in 'nav_stack[++nav_stack_ptr]' */
   DilloUrl *nav_expect_url;
   /* 'nav_expecting' is true if the last URL is being loaded for
    * the first time and has not gotten the dw yet. */
   gboolean nav_expecting;

   /* The tag for the idle function that sets button sensitivity. */
   gint sens_idle_tag;
};


extern BrowserWindow **browser_window;
extern gint num_bw;

#endif /* __DILLO_BROWSER_H__ */

