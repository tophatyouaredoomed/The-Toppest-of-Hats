#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include "browser.h"

void a_Interface_init(void);
void a_Interface_stop(BrowserWindow *bw);
void a_Interface_clean(BrowserWindow *bw);
void a_Interface_quit_all(void);

void a_Interface_add_client(BrowserWindow *bw, gint Key, gint Root);
void a_Interface_remove_client(BrowserWindow *bw, gint ClientKey);
void a_Interface_add_url(BrowserWindow *bw, const DilloUrl *Url, gint Flags);
void a_Interface_close_client(BrowserWindow *bw, gint ClientKey);

void a_Interface_msg(BrowserWindow *bw, const char *format, ... );

void a_Interface_openfile_dialog(BrowserWindow *bw);
void a_Interface_open_dialog(GtkWidget *widget, BrowserWindow *bw);
void a_Interface_save_dialog(GtkWidget *widget, BrowserWindow *bw);
void a_Interface_save_link_dialog(GtkWidget *widget, BrowserWindow *bw);
void a_Interface_findtext_dialog(BrowserWindow *bw);
void a_Interface_quit_dialog(BrowserWindow *bw);

void a_Interface_set_page_title(BrowserWindow *bw, char *title);
void a_Interface_set_location_text(BrowserWindow *bw, char *text);
gchar *a_Interface_get_location_text(BrowserWindow *bw);
void a_Interface_reset_progress_bars(BrowserWindow *bw);
void a_Interface_entry_open_url(GtkWidget *widget, BrowserWindow *bw);
void a_Interface_set_cursor(BrowserWindow *bw, GdkCursorType CursorType);
BrowserWindow *a_Interface_browser_window_new(gint width, gint height);

void a_Interface_set_button_sens(BrowserWindow *bw);

void a_Interface_scroll_popup(GtkWidget *widget);

void a_Interface_question_dialog(
        BrowserWindow *bw, gchar *QuestionTxt,
        GtkSignalFunc OkCallback, void *OkCbData,
        GtkSignalFunc CancelCallback, void *CancelCbData);

void a_Interface_toggle_panel(BrowserWindow *bw);

#endif /* __INTERFACE_H__ */
