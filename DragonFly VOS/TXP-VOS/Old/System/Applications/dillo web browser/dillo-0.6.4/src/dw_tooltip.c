/*
 * A few notes:
 *
 *    - Currently, a window is created every time before it is shown, and
 *      destroyed, before it is hidden. This saves (probably?) some
 *      memory, but can simply be changed. An alternative is having a
 *      global window for all tooltips.
 *
 *    - Tooltips are positioned near the pointer, as opposed to Gtk+
 *      tooltips, which are positioned near the widget.
 *
 * Sebastian
 */

#include <gtk/gtk.h>
#include "dw_tooltip.h"

static gint Dw_tooltip_draw (DwTooltip *tooltip);

/*
 * Create a new tooltip.
 */
DwTooltip* a_Dw_tooltip_new (const gchar *text)
{
   DwTooltip *tooltip;

   tooltip = g_new (DwTooltip, 1);
   tooltip->window = NULL;
   tooltip->timeout_id = -1;
   tooltip->text = g_strdup (text);
   return tooltip;
}


/*
 * Destroy the tooltip.
 */
void a_Dw_tooltip_destroy (DwTooltip *tooltip)
{
   a_Dw_tooltip_on_leave (tooltip);
   g_free (tooltip->text);
   g_free (tooltip);
}


/*
 * Call this function if the pointer has entered the widget/word.
 */
void a_Dw_tooltip_on_enter (DwTooltip *tooltip)
{
   a_Dw_tooltip_on_leave (tooltip);
   tooltip->timeout_id = gtk_timeout_add(500, (GtkFunction)Dw_tooltip_draw,
                                         tooltip);
}


/*
 * Call this function if the pointer has left the widget/word.
 */
void a_Dw_tooltip_on_leave (DwTooltip *tooltip)
{
   if (tooltip->timeout_id != -1) {
      gtk_timeout_remove(tooltip->timeout_id);
      tooltip->timeout_id = -1;
   }

   if (tooltip->window != NULL) {
      gtk_widget_destroy(tooltip->window);
      tooltip->window = NULL;
   }
}


/*
 * Call this function if the pointer has moved within the widget/word.
 */
void a_Dw_tooltip_on_motion (DwTooltip *tooltip)
{
   a_Dw_tooltip_on_enter (tooltip);
}

/*
 *  Draw the tooltip. Called as a timeout function.
 */
static gint Dw_tooltip_draw (DwTooltip *tooltip)
{
   GtkStyle *style;
   gint x, y, width, ascent, descent;

   gdk_window_get_pointer (NULL, &x, &y, NULL);

   tooltip->window = gtk_window_new(GTK_WINDOW_POPUP);
   gtk_widget_set_app_paintable (tooltip->window, TRUE);
   gtk_widget_set_name (tooltip->window, "gtk-tooltips");
   gtk_widget_ensure_style (tooltip->window);
   style = tooltip->window->style;
   width = gdk_string_width (style->font, tooltip->text);
   ascent = style->font->ascent;
   descent = style->font->descent;
   gtk_widget_set_usize (tooltip->window, width + 8, ascent + descent + 8);

   gtk_widget_popup(tooltip->window, x + 10, y + 10);
   style = tooltip->window->style;
   gtk_paint_flat_box(style, tooltip->window->window,
                      GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                      NULL, GTK_WIDGET(tooltip->window), "tooltip",
                      0, 0, -1, -1);
   gtk_paint_string(style, tooltip->window->window,
                    GTK_STATE_NORMAL,
                    NULL, GTK_WIDGET(tooltip->window), "tooltip",
                    4, ascent + 4,
                    tooltip->text);

   tooltip->timeout_id = -1;
   return FALSE;
}



