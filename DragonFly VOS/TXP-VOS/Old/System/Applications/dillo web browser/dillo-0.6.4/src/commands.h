#ifndef __COMMANDS_H__
#define __COMMANDS_H__

void a_Commands_new_callback(GtkWidget * widget, gpointer client_data);
void a_Commands_openfile_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_openurl_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_prefs_callback(GtkWidget * widget, gpointer client_data);
void a_Commands_close_callback(GtkWidget * widget, gpointer client_data);
void a_Commands_exit_callback (GtkWidget *widget, gpointer client_data);

void a_Commands_viewsource_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_selectall_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_findtext_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_print_callback (GtkWidget *widget, gpointer client_data);

void a_Commands_navpress_callback (
        GtkWidget *widget, GdkEventButton *event, gpointer client_data);
void a_Commands_historypress_callback(
        GtkWidget *widget, GdkEventButton *event, gpointer client_data);

void a_Commands_back_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_forw_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_reload_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_stop_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_home_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_save_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_save_link_callback (GtkWidget *widget, gpointer client_data);

void a_Commands_addbm_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_viewbm_callback (GtkWidget *widget, gpointer client_data);

void a_Commands_helphome_callback (GtkWidget *widget, gpointer client_data);
void a_Commands_manual_callback (GtkWidget *widget, gpointer client_data);

void a_Commands_open_link_nw_callback(GtkWidget *widget, gpointer client_data);

void a_Commands_set_selection_callback(GtkWidget *widget, gpointer client_data);
void a_Commands_give_selection_callback(GtkWidget *widget,
      GtkSelectionData *data, guint info, guint time);
gint a_Commands_clear_selection_callback(GtkWidget *, GdkEventSelection *);

#endif /* __COMMANDS_H__ */
