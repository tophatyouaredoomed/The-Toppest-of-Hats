#ifndef __BOOKMARK_H__
#define __BOOKMARK_H__

#include "browser.h"


void a_Bookmarks_init();
void a_Bookmarks_add(GtkWidget *widget, gpointer client_data);
void a_Bookmarks_fill_new_menu(BrowserWindow *bw);

#endif /* __BOOKMARK_H__ */
