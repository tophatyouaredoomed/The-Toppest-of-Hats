#ifndef __DW_TOOLTIP_H__
#define __DW_TOOLTIP_H__

#include <gtk/gtkwidget.h>

typedef struct _DwTooltip DwTooltip;

struct _DwTooltip
{
   GtkWidget *window;
   gchar *text;
   guint timeout_id;
};


DwTooltip* a_Dw_tooltip_new       (const gchar *text);
void       a_Dw_tooltip_destroy   (DwTooltip *tooltip);

void       a_Dw_tooltip_on_enter  (DwTooltip *tooltip);
void       a_Dw_tooltip_on_leave  (DwTooltip *tooltip);
void       a_Dw_tooltip_on_motion (DwTooltip *tooltip);


#endif /* __DW_TOOLTIP_H__ */
