/*
 * File: dw_page.c
 *
 * Copyright (C) 1997 Raph Levien <raph@acm.org>
 * Copyright (C) 1999 Luca Rota <drake@freemail.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * This module contains the Dw_page widget, which is the "back end" to
 * Web text widgets including html.
 */

#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>

#include "list.h"
#include "dw_page.h"
#include "dw_marshal.h"
#include "dw_gtk_viewport.h"

#include "prefs.h"

//#define DEBUG_LEVEL 2
#include "debug.h"

static void Dw_page_init          (DwPage *page);
static void Dw_page_class_init    (DwPageClass *klass);

static void Dw_page_destroy       (GtkObject *object);

static void Dw_page_size_request      (DwWidget *widget,
                                      DwRequisition *requisition);
static void Dw_page_get_extremes      (DwWidget *widget,
                                       DwExtremes *extremes);
static void Dw_page_size_allocate     (DwWidget *widget,
                                       DwAllocation *allocation);
static void Dw_page_mark_change       (DwWidget *widget,
                                       gint ref);
static void Dw_page_set_width         (DwWidget *widget,
                                       gint32 width);
static void Dw_page_set_ascent        (DwWidget *widget,
                                       gint32 ascent);
static void Dw_page_set_descent       (DwWidget *widget,
                                       gint32 descent);
static void Dw_page_draw              (DwWidget *page,
                                       DwRectangle *area,
                                       GdkEventExpose *event);
static gint Dw_page_button_press      (DwWidget *widget,
                                       gint32 x,
                                       gint32 y,
                                       GdkEventButton *event);
static gint Dw_page_button_release    (DwWidget *widget,
                                       gint32 x,
                                       gint32 y,
                                       GdkEventButton *event);
static gint Dw_page_motion_notify     (DwWidget *widget,
                                       gint32 x,
                                       gint32 y,
                                       GdkEventMotion *event);
static gint Dw_page_leave_notify      (DwWidget *widget,
                                       GdkEventMotion *event);


static void Dw_page_add               (DwContainer *container,
                                       DwWidget *widget);
static void Dw_page_remove            (DwContainer *container,
                                       DwWidget *widget);
static void Dw_page_forall            (DwContainer *container,
                                       DwCallback callback,
                                       gpointer callback_data);
static gint Dw_page_findtext          (DwContainer *container,
                                       gpointer FP, gpointer KP,
                                       gchar *NewKey);

static void Dw_page_rewrap            (DwPage *page);

/*
 * Returns the x offset (the indentation plus any offset needed for
 * centering or right justification) for the line. The offset returned
 * is relative to the page *content* (i.e. without border etc.).
 */
#define Dw_page_line_x_offset(page, line) \
   ( (page)->inner_padding + (line)->left_offset + \
     ((line) == (page)->lines ? (page)->line1_offset : 0) )

/*
 * Like Dw_style_box_offset_x, but relative to the allocation (i.e.
 * including border etc.).
 */
#define Dw_page_line_total_x_offset(page, line) \
   ( Dw_page_line_x_offset (page, line) + \
     Dw_style_box_offset_x (((DwWidget*)(page))->style) )


enum
{
   LINK_ENTERED,
   LINK_PRESSED,
   LINK_RELEASED,
   LINK_CLICKED,
   LAST_SIGNAL
};

static DwContainerClass *parent_class;
static guint page_signals[LAST_SIGNAL] = { 0 };


/*
 * Return the type of DwPage
 */
GtkType a_Dw_page_get_type (void)
{
   static GtkType type = 0;

   if (!type) {
      GtkTypeInfo info = {
         "DwPage",
         sizeof (DwPage),
         sizeof (DwPageClass),
         (GtkClassInitFunc) Dw_page_class_init,
         (GtkObjectInitFunc) Dw_page_init,
         (GtkArgSetFunc) NULL,
         (GtkArgGetFunc) NULL,
         (GtkClassInitFunc) NULL
      };

      type = gtk_type_unique (DW_TYPE_CONTAINER, &info);
   }

   return type;
}


/*
 * Create a new DwPage
 */
DwWidget* a_Dw_page_new (void)
{
   GtkObject *object;

   object = gtk_object_new (DW_TYPE_PAGE, NULL);
   return DW_WIDGET (object);
}


/*
 * Initialize a DwPage
 */
static void Dw_page_init (DwPage *page)
{
   DW_WIDGET_SET_FLAGS (page, DW_USES_HINTS);

   page->list_item = FALSE;
   page->inner_padding = 0;
   page->line1_offset = 0;

   /*
    * The initial sizes of page->lines and page->words should not be
    * too high, since this will waste much memory with tables
    * containing many small cells. The few more calls to realloc
    * should not decrease the speed considerably.
    * (Current setting is for minimal memory usage. An interesting fact
    * is that high values decrease speed due to memory handling overhead!)
    * todo: Some tests would be useful.
    */
   page->num_lines_max = 1; /* 2 */
   page->num_lines = 0;
   page->lines = NULL;      /* g_new(DwPageLine, page->num_lines_max); */

   page->num_words_max = 1; /* 8 */
   page->num_words = 0;
   page->words = NULL;      /* g_new(DwPageWord, page->num_words_max); */

   page->last_line_width = 0;
   page->wrap_ref = -1;

   page->hover_link = -1;

   /* random values */
   page->avail_width = 100;
   page->avail_ascent = 100;
   page->avail_descent = 0;
}

/*
 * Initialize the DwPage's class
 */
static void Dw_page_class_init (DwPageClass *klass)
{
   GtkObjectClass *object_class;
   DwWidgetClass *widget_class;
   DwContainerClass *container_class;

   object_class = (GtkObjectClass*) klass;
   widget_class = (DwWidgetClass*) klass;
   container_class = (DwContainerClass*) klass;
   parent_class = gtk_type_class (DW_TYPE_CONTAINER);

   /* todo: the link_xxx signals should return boolean values, which are
    * then returned by the event signal function of DwPage. But I don't
    * know that much about Gtk+ signals :-(
    * --SG
    */
   page_signals[LINK_ENTERED] =
      gtk_signal_new ("link_entered",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwPageClass, link_entered),
                      Dw_marshal_link_enter,
                      GTK_TYPE_NONE,
                      3, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT);
   page_signals[LINK_PRESSED] =
      gtk_signal_new ("link_pressed",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwPageClass, link_pressed),
                      Dw_marshal_link_button,
                      GTK_TYPE_NONE,
                      4, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT,
                      GTK_TYPE_GDK_EVENT);
   page_signals[LINK_RELEASED] =
      gtk_signal_new ("link_released",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwPageClass, link_released),
                      Dw_marshal_link_button,
                      GTK_TYPE_NONE,
                      4, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT,
                      GTK_TYPE_GDK_EVENT);
   page_signals[LINK_CLICKED] =
      gtk_signal_new ("link_clicked",
                      GTK_RUN_FIRST,
                      object_class->type,
                      GTK_SIGNAL_OFFSET (DwPageClass, link_clicked),
                      Dw_marshal_link_button,
                      GTK_TYPE_NONE,
                      4, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT,
                      GTK_TYPE_GDK_EVENT);
   gtk_object_class_add_signals (object_class, page_signals, LAST_SIGNAL);

   object_class->destroy = Dw_page_destroy;

   widget_class->size_request = Dw_page_size_request;
   widget_class->get_extremes = Dw_page_get_extremes;
   widget_class->size_allocate = Dw_page_size_allocate;
   widget_class->mark_size_change = Dw_page_mark_change;
   widget_class->mark_extremes_change = Dw_page_mark_change;
   widget_class->set_width = Dw_page_set_width;
   widget_class->set_ascent = Dw_page_set_ascent;
   widget_class->set_descent = Dw_page_set_descent;
   widget_class->draw = Dw_page_draw;
   widget_class->button_press_event = Dw_page_button_press;
   widget_class->button_release_event = Dw_page_button_release;
   widget_class->motion_notify_event = Dw_page_motion_notify;
   widget_class->leave_notify_event = Dw_page_leave_notify;

   container_class->add = Dw_page_add;
   container_class->remove = Dw_page_remove;
   container_class->forall = Dw_page_forall;
   container_class->findtext = Dw_page_findtext;

   klass->link_entered = NULL;
   klass->link_pressed = NULL;
   klass->link_released = NULL;
   klass->link_clicked = NULL;
}


/*
 * Destroy the page (standard Gtk+ function)
 */
static void Dw_page_destroy (GtkObject *object)
{
   DwPage *page = DW_PAGE (object);
   DwPageWord *word;
   gint i;

   DEBUG_MSG(10, "Dw_page_destroy\n");

   for (i = 0; i < page->num_words; i++) {
      word = &page->words[i];
      if (word->content_type == DW_PAGE_CONTENT_WIDGET)
         gtk_object_unref(GTK_OBJECT(word->content.widget));
      else if (word->content_type == DW_PAGE_CONTENT_TEXT)
         g_free(word->content.text);
      else if (word->content_type == DW_PAGE_CONTENT_ANCHOR)
         g_free(word->content.anchor);

      a_Dw_style_unref (word->style);
      a_Dw_style_unref (word->space_style);
   }

   g_free (page->lines);
   g_free (page->words);

   /* Make sure we don't own widgets anymore. Necessary before call of
      parent_class->destroy. */
   page->num_words = 0;
   page->num_lines = 0;

   (* GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}


/*
 * Standard Dw function
 *
 * The ascent of a page is the ascent of the first line, plus
 * padding/border/margin. This can be used to align the first lines
 * of several pages in a horizontal line.
 */
static void Dw_page_size_request (DwWidget *widget,
                                  DwRequisition *requisition)
{
   DwPage *page = DW_PAGE (widget);
   DwPageLine *last_line;
   gint height;

   Dw_page_rewrap(page);

   if (page->num_lines > 0) {
      last_line = &page->lines[page->num_lines - 1];
      requisition->width =
         MAX (last_line->max_line_width, page->last_line_width);
      /* Note: the break_space of the last line is ignored, so breaks
         at the end of a page are not visible. */
      height =
         last_line->top + last_line->ascent + last_line->descent +
         Dw_style_box_rest_height (widget->style);
      requisition->ascent = page->lines[0].top + page->lines[0].ascent;
      requisition->descent = height - requisition->ascent;
   } else {
      requisition->width = page->last_line_width;
      requisition->ascent = Dw_style_box_offset_y (widget->style);
      requisition->descent = Dw_style_box_rest_height (widget->style);
   }

   requisition->width +=
      page->inner_padding + Dw_style_box_diff_width (widget->style);
   if (requisition->width < page->avail_width)
      requisition->width = page->avail_width;
}


/*
 * Get the extremes of a word within a page.
 */
static void Dw_page_get_word_extremes (DwPageWord *word,
                                       DwExtremes *extremes)
{
   if (word->content_type == DW_PAGE_CONTENT_WIDGET) {
      if (DW_WIDGET_USES_HINTS (word->content.widget))
         a_Dw_widget_get_extremes (word->content.widget, extremes);
      else {
         if (DW_STYLE_IS_PERCENTAGE (word->content.widget->style->width)) {
            extremes->min_width = 0;
            if (DW_WIDGET_HAS_CONTENT (word->content.widget))
               extremes->max_width = DW_INFINITY;
            else
               extremes->max_width = 0;
         } else if (DW_STYLE_IS_LENGTH (word->content.widget->style->width)) {
            extremes->min_width = extremes->max_width =
               DW_STYLE_GET_LENGTH(word->content.widget->style->width,
                                   word->style->font);
         } else
            a_Dw_widget_get_extremes (word->content.widget, extremes);
      }
   } else {
      extremes->min_width = word->size.width;
      extremes->max_width = word->size.width;
   }
}

/*
 * Standard Dw function
 */
static void Dw_page_get_extremes (DwWidget *widget,
                                  DwExtremes *extremes)
{
   DwPage *page = DW_PAGE (widget);
   DwExtremes word_extremes;
   DwPageLine *line;
   DwPageWord *word;
   gint word_index, line_index;
   gint32 par_min, par_max;
   gboolean nowrap;

   DEBUG_MSG (4, "Dw_page_get_extremes\n");

   if (page->num_lines == 0) {
      /* empty page */
      extremes->min_width = 0;
      extremes->max_width = 0;
   } else if (page->wrap_ref == -1) {
      /* no rewrap necessary -> values in lines are up to date */
      line = &page->lines[page->num_lines - 1];
      if (page->words[line->first_word].style->nowrap)
         extremes->min_width = MAX (line->max_word_min, line->par_min);
      else
         extremes->min_width = line->max_word_min;
      extremes->max_width = MAX (line->max_par_max, line->par_max);
   } else {
      /* Calculate the extremes, based on the values in the line from
         where a rewrap is necessary. */
      if (page->wrap_ref == 0) {
         extremes->min_width = 0;
         extremes->max_width = 0;
         par_min = 0;
         par_max = 0;
      } else {
         line = &page->lines[page->wrap_ref - 1];
         extremes->min_width = line->max_word_min;
         extremes->max_width = line->max_par_max;
         par_min = line->par_min;
         par_max = line->par_max;
      }

      for (line_index = page->wrap_ref; line_index < page->num_lines;
           line_index++) {
         line = &page->lines[line_index];
         nowrap = page->words[line->first_word].style->nowrap;

         for (word_index = line->first_word; word_index < line->last_word;
              word_index++) {
            word = &page->words[word_index];
            Dw_page_get_word_extremes (word, &word_extremes);

            /* For the first word, we simply add the line1_offset. */
            if (word_index == 0) {
               word_extremes.min_width += page->line1_offset;
               word_extremes.max_width += page->line1_offset;
            }
           
            if (nowrap)
               par_min += word_extremes.min_width + word->orig_space;
            else
               if (extremes->min_width < word_extremes.min_width)
                  extremes->min_width = word_extremes.min_width;

            par_max += word_extremes.max_width + word->orig_space;

            DEBUG_MSG (1, "      word \"%s\": max_width = %d\n",
                       word->content_type == DW_PAGE_CONTENT_TEXT ?
                       word->content.text : "<no text>",
                       word_extremes.max_width);
         }

         if ( ( line->last_word > line->first_word &&
                page->words[line->last_word - 1].content_type
                == DW_PAGE_CONTENT_BREAK ) ||
              line_index == page->num_lines - 1 ) {
            word = &page->words[line->last_word - 1];
            par_max -= word->orig_space;

            DEBUG_MSG (2, "   par_max = %d, after word %d (\"%s\")\n",
                       par_max, line->last_word - 1,
                       word->content_type == DW_PAGE_CONTENT_TEXT ?
                       word->content.text : "<no text>");

            if (extremes->max_width < par_max)
               extremes->max_width = par_max;

            if (nowrap) {
               par_min -= word->orig_space;
               if (extremes->min_width < par_min)
                  extremes->min_width = par_min;
            }

            par_min = 0;
            par_max = 0;
         }
      }

      DEBUG_MSG (3, "   Result: %d, %d\n",
                 extremes->min_width, extremes->max_width);
   }

   extremes->min_width +=
      page->inner_padding + Dw_style_box_diff_width (widget->style);
   extremes->max_width +=
      page->inner_padding + Dw_style_box_diff_width (widget->style);
}


/*
 * Standard Dw function
 */
static void Dw_page_size_allocate (DwWidget *widget,
                                   DwAllocation *allocation)
{
   DwPage *page;
   int line_index, word_index;
   DwPageLine *line;
   DwPageWord *word;
   int x_cursor;
   DwAllocation child_allocation;

   page = DW_PAGE (widget);

   for (line_index = 0; line_index < page->num_lines; line_index++) {
      line = &(page->lines[line_index]);
      x_cursor = Dw_page_line_total_x_offset(page, line);

      for (word_index = line->first_word; word_index < line->last_word;
           word_index++) {
         word = &(page->words[word_index]);

         if (word->content_type == DW_PAGE_CONTENT_WIDGET) {

            /* todo: justification within the line is done here! */
            child_allocation.x = x_cursor + allocation->x;
            /* align=top:
               child_allocation.y = line->top + allocation->y;
            */
            /* align=bottom (base line) */
            child_allocation.y = line->top + allocation->y
               + (line->ascent - word->size.ascent);
            child_allocation.width = word->size.width;
            child_allocation.ascent = word->size.ascent;
            child_allocation.descent = word->size.descent;
            a_Dw_widget_size_allocate (word->content.widget,
                                       &child_allocation);
         }

         x_cursor += (word->size.width + word->eff_space);
      }
   }
}


/*
 * Implementation for both mark_size_change and mark_extremes_change.
 */
static void Dw_page_mark_change (DwWidget *widget,
                                 gint ref)
{
   DwPage *page;

   if (ref != -1) {
      page = DW_PAGE (widget);
      if (page->wrap_ref == -1)
         page->wrap_ref = ref;
      else
         page->wrap_ref = MIN(page->wrap_ref, ref);
   }
}


/*
 * Standard Dw function
 */
static void Dw_page_set_width (DwWidget *widget,
                               gint32 width)
{
   DwPage *page;

   page = DW_PAGE (widget);

   /* If limit_text_width is set to YES, a queue_resize may also be
      necessary. */
   if (page->avail_width != width || prefs.limit_text_width) {
      page->avail_width = width;
      Dw_widget_queue_resize (widget, 0, FALSE);
      page->must_queue_resize = FALSE;
   }
}


/*
 * Standard Dw function
 */
static void Dw_page_set_ascent (DwWidget *widget,
                                gint32 ascent)
{
   DwPage *page;

   page = DW_PAGE (widget);

   if (page->avail_ascent != ascent) {
      page->avail_ascent = ascent;
      Dw_widget_queue_resize (widget, 0, FALSE);
      page->must_queue_resize = FALSE;
   }
}


/*
 * Standard Dw function
 */
static void Dw_page_set_descent (DwWidget *widget,
                                 gint32 descent)
{
   DwPage *page;

   page = DW_PAGE (widget);

   if (page->avail_descent != descent) {
      page->avail_descent = descent;
      Dw_widget_queue_resize (widget, 0, FALSE);
      page->must_queue_resize = FALSE;
   }
}


/*
 * Standard Dw function
 */
static void Dw_page_add (DwContainer *container,
                         DwWidget *widget)
{
   /* todo */
}


/*
 * Standard Dw function
 */
static void Dw_page_remove (DwContainer *container,
                            DwWidget *widget)
{
   /* todo */
}


/*
 * Standard Dw function
 */
static void Dw_page_forall (DwContainer *container,
                            DwCallback callback,
                            gpointer callback_data)
{
   DwPage *page;
   int word_index;
   DwPageWord *word;

   page = DW_PAGE (container);

   for(word_index = 0; word_index < page->num_words; word_index++) {
      word = &page->words[word_index];

      if (word->content_type == DW_PAGE_CONTENT_WIDGET)
         (*callback) (word->content.widget, callback_data);
   }
}

/*
 * This function is called in two cases: (i) when a word is added (by
 * Dw_page_add_word), and (ii) when a page has to be (partially)
 * rewrapped. It does word wrap, and adds new lines, if necesary.
 */
static void Dw_page_word_wrap (DwPage *page, gint word_ind)
{
   DwPageLine *last_line, *plast_line;
   DwPageWord *word, *last_word;
   gint32 avail_width, last_space, left_offset;
   gboolean new_line = FALSE, new_par = FALSE;
   DwWidget *widget = DW_WIDGET (page);
   DwExtremes word_extremes;

   avail_width =
      page->avail_width - Dw_style_box_diff_width(widget->style)
      - page->inner_padding;
   if (prefs.limit_text_width &&
       avail_width > widget->viewport->allocation.width - 10)
      avail_width = widget->viewport->allocation.width - 10;

   word = &page->words[word_ind];

   if (page->num_lines == 0) {
      new_line = TRUE;
      new_par = TRUE;
      last_line = NULL;
   } else {
      last_line = &page->lines[page->num_lines - 1];

      if (page->num_words > 0) {
         last_word = &page->words[word_ind - 1];
         if (last_word->content_type == DW_PAGE_CONTENT_BREAK) {
            /* last word is a break */
            new_line = TRUE;
            new_par = TRUE;
         } else if (word->style->nowrap) {
            new_line = FALSE;
            new_par = FALSE;
         } else {
            if (last_line->first_word != word_ind) {
               /* does new word fit into the last line? */
               new_line = (page->last_line_width + last_word->orig_space
                           + word->size.width > avail_width);
            }
         }
      }
   }

   /* Has sometimes the wrong value. */
   word->eff_space = word->orig_space;

   if (last_line != NULL && new_line && !new_par &&
       word->style->text_align == DW_STYLE_TEXT_ALIGN_JUSTIFY) {
      /* Justified lines. To avoid rounding errors, the calculation is
         based on accumulated values (*_cum). */
      gint i;
      gint32 orig_space_sum, orig_space_cum;
      gint32 eff_space_diff_cum, last_eff_space_diff_cum;
      gint32 diff;

      diff = avail_width - page->last_line_width;
      if (diff > 0) {
         orig_space_sum = 0;
         for (i = last_line->first_word; i < last_line->last_word - 1; i++)
            orig_space_sum += page->words[i].orig_space;
        
         orig_space_cum = 0;
         last_eff_space_diff_cum = 0;
         for (i = last_line->first_word; i < last_line->last_word - 1; i++) {
            orig_space_cum += page->words[i].orig_space;
            
            if (orig_space_cum == 0)
               eff_space_diff_cum = last_eff_space_diff_cum;
            else
               eff_space_diff_cum = diff * orig_space_cum / orig_space_sum;

            page->words[i].eff_space = page->words[i].orig_space +
               (eff_space_diff_cum - last_eff_space_diff_cum);

            last_eff_space_diff_cum = eff_space_diff_cum;
         }
      }
   }         

   if (new_line) {
      /* Add a new line. */
      page->num_lines++;
      a_List_add(page->lines, page->num_lines, sizeof(DwPageLine),
                 page->num_lines_max);

      last_line = &page->lines[page->num_lines - 1];

      if (page->num_lines == 1)
         plast_line = NULL;
      else
         plast_line = &page->lines[page->num_lines - 2];

      if (plast_line) {
         /* second or more lines: copy values of last line */
         last_line->top =
            plast_line->top + plast_line->ascent +
            plast_line->descent + plast_line->break_space;
         last_line->max_line_width = plast_line->max_line_width;
         last_line->max_word_min = plast_line->max_word_min;
         last_line->max_par_max = plast_line->max_par_max;
         last_line->par_min = plast_line->par_min;
         last_line->par_max = plast_line->par_max;
      } else {
         /* first line: initialize values */
         last_line->top = Dw_style_box_offset_y (DW_WIDGET(page)->style);
         last_line->max_line_width = page->line1_offset;
         last_line->max_word_min = 0;
         last_line->max_par_max = 0;
         last_line->par_min =  page->line1_offset;
         last_line->par_max =  page->line1_offset;
      }

      last_line->first_word = last_line->last_word = word_ind;
      last_line->ascent = 0;
      last_line->descent = 0;
      last_line->break_space = 0;
      last_line->left_offset = 0;

      /* update values in line */
      last_line->max_line_width =
         MAX (last_line->max_line_width, page->last_line_width);

      if (page->num_lines > 1)
         page->last_line_width = 0;
      else
         page->last_line_width = page->line1_offset;

      if (new_par) {
         last_line->max_par_max =
            MAX (last_line->max_par_max, last_line->par_max);
         if (page->num_lines > 1) {
            last_line->par_min = 0;
            last_line->par_max = 0;
         } else {
            last_line->par_min = page->line1_offset;
            last_line->par_max = page->line1_offset;
         }
      }
   }

   last_line->last_word = word_ind + 1;
   last_line->ascent = MAX (last_line->ascent, word->size.ascent);
   last_line->descent = MAX (last_line->descent, word->size.descent);

   Dw_page_get_word_extremes (word, &word_extremes);
   last_space = (word_ind > 0) ? page->words[word_ind - 1].orig_space : 0;

   if (word->content_type == DW_PAGE_CONTENT_BREAK)
      last_line->break_space = MAX (word->content.break_space,
                                    last_line->break_space);

   page->last_line_width += word->size.width;
   if (!new_line)
      page->last_line_width += last_space;

   last_line->par_max += word_extremes.max_width;
   if (!new_par)
      last_line->par_max += last_space;

   if (word->style->nowrap)
      last_line->par_min += word_extremes.min_width;
   else
      last_line->max_word_min =
         MAX (last_line->max_word_min, word_extremes.min_width);

   /* Finally, justify the line. Breaks are ignored, since the HTML
    * parser sometimes assignes the wrong style to them. (todo: ) */
   if (word->content_type != DW_PAGE_CONTENT_BREAK) {
      switch (word->style->text_align) {
      case DW_STYLE_TEXT_ALIGN_LEFT:
      case DW_STYLE_TEXT_ALIGN_JUSTIFY:  /* see some lines above */
      case DW_STYLE_TEXT_ALIGN_STRING:   /* handled elsewhere (in future) */
         left_offset = 0;
         break;
         
      case DW_STYLE_TEXT_ALIGN_RIGHT:
         left_offset = avail_width - page->last_line_width;
         break;
         
      case DW_STYLE_TEXT_ALIGN_CENTER:
         left_offset = (avail_width - page->last_line_width) / 2;
         break;
         
      default:
         /* compiler happiness */
         left_offset = 0;
      }

      /* For large lines (images etc), which do not fit into the viewport: */
      if (left_offset < 0)
         left_offset = 0;

      if (page->list_item && last_line == page->lines) {
         /* List item markers are always on the left. */
         last_line->left_offset = 0;
         page->words[0].eff_space = page->words[0].orig_space + left_offset;
      } else
         last_line->left_offset = left_offset;
   }

   page->must_queue_resize = TRUE;
}


/*
 * Calculate the size of a widget within the page.
 * (Subject of change in the near future!)
 */
static void Dw_page_calc_widget_size (DwPage *page,
                                      DwWidget *widget,
                                      DwRequisition *size)
{
   DwRequisition requisition;
   gint32 avail_width, avail_ascent, avail_descent;

   avail_width =
      page->avail_width - Dw_style_box_diff_width(DW_WIDGET(page)->style);
   avail_ascent =
      page->avail_ascent - Dw_style_box_diff_height(DW_WIDGET(page)->style);
   avail_descent = page->avail_descent;

   if (DW_WIDGET_USES_HINTS (widget)) {
      a_Dw_widget_set_width (widget, avail_width);
      a_Dw_widget_set_ascent (widget, avail_ascent);
      a_Dw_widget_set_descent (widget, avail_descent);
      a_Dw_widget_size_request (widget, size);
   } else {
      if (widget->style->width == DW_STYLE_UNDEF_LENGTH ||
          widget->style->height == DW_STYLE_UNDEF_LENGTH)
         a_Dw_widget_size_request (widget, &requisition);

      if (widget->style->width == DW_STYLE_UNDEF_LENGTH)
         size->width = requisition.width;
      else if (DW_STYLE_IS_LENGTH (widget->style->width))
         size->width =
            DW_STYLE_GET_LENGTH (widget->style->width, widget->style->font)
            + Dw_style_box_diff_width (widget->style);
      else
         size->width =
            DW_STYLE_GET_PERCENTAGE (widget->style->width) *  avail_width;

      if (widget->style->height == DW_STYLE_UNDEF_LENGTH) {
         size->ascent = requisition.ascent;
         size->descent = requisition.descent;
      } else if (DW_STYLE_IS_LENGTH (widget->style->width)) {
         size->ascent =
            DW_STYLE_GET_LENGTH (widget->style->height, widget->style->font)
            + Dw_style_box_diff_height (widget->style);
         size->descent = 0;
      } else {
         size->ascent =
            DW_STYLE_GET_PERCENTAGE (widget->style->height) * avail_ascent;
         size->descent =
            DW_STYLE_GET_PERCENTAGE (widget->style->height) * avail_descent;
      }
   }
}


/*
 * Rewrap the page from the line from which this is necessary.
 * There are basically two times we'll want to do this:
 * either when the viewport is resized, or when the size changes on one
 * of the child widgets.
 */
static void Dw_page_rewrap(DwPage *page)
{
   DwWidget *widget;
   gint word_index;
   DwPageWord *word;

   if (page->wrap_ref == -1)
      return;

   widget = DW_WIDGET (page);

   page->num_lines = page->wrap_ref;
   page->last_line_width = 0;

   if (page->num_lines > 0)
      word_index = page->lines[page->num_lines - 1].first_word;
   else
      word_index = 0;

   for (; word_index < page->num_words; word_index++) {
      word = &page->words[word_index];

      if (word->content_type == DW_PAGE_CONTENT_WIDGET)
         Dw_page_calc_widget_size (page, word->content.widget, &word->size);
      Dw_page_word_wrap(page, word_index);
      if (word->content_type == DW_PAGE_CONTENT_WIDGET)
         word->content.widget->parent_ref = page->num_lines - 1;

      if ( word->content_type == DW_PAGE_CONTENT_ANCHOR ) {
         Dw_widget_set_anchor(widget, word->content.anchor,
                              page->lines[page->num_lines - 1].top);
      }
   }
}

/*
 * Paint a line
 * - x and y are toplevel dw coordinates (Question: what Dw? Changed. Test!)
 * - area is used always (ev. set it to event->area)
 * - event is only used when is_expose
 */
static void Dw_page_draw_line (DwPage *page,
                               DwPageLine *line,
                               DwRectangle *area,
                               GdkEventExpose *event)
{
   DwWidget *widget;
   DwPageWord *word;
   gint word_index;
   gint x_cursor, y_cursor;
   gint diff;
   DwWidget *child;
   DwRectangle child_area;
   GdkWindow *window;

   /* Here's an idea on how to optimize this routine to minimize the number
    * of calls to gdk_draw_string:
    *
    * Copy the text from the words into a buffer, adding a new word
    * only if: the attributes match, and the spacing is either zero or
    * equal to the width of ' '. In the latter case, copy a " " into
    * the buffer. Then draw the buffer. */

   widget = DW_WIDGET (page);
   window = DW_WIDGET_WINDOW (widget);
   x_cursor =
      Dw_widget_x_world_to_viewport (widget,
                                     widget->allocation.x +
                                     Dw_page_line_total_x_offset(page, line));
   y_cursor =
      Dw_widget_y_world_to_viewport (widget,
                                     widget->allocation.y
                                     + line->top + line->ascent);

   for (word_index = line->first_word; word_index < line->last_word;
        word_index++) {
      word = &page->words[word_index];
      diff = 0;
      switch (word->content_type) {
      case DW_PAGE_CONTENT_TEXT:
         /* Adjust the text baseline if the word is <SUP>-ed or <SUB>-ed */
         if (word->style->SubSup == TEXT_SUB)
            diff = word->size.ascent / 2;
         else if (word->style->SubSup == TEXT_SUP)
            diff -= word->size.ascent / 3;

         gdk_draw_string(window,
                         word->style->font->font,
                         word->style->color->gc,
                         x_cursor,
                         y_cursor + diff,
                         word->content.text);

         /* underline */
         if (word->style->uline >= 0)
            gdk_draw_line(window,
                          word->style->color->gc,
                          x_cursor,
                          y_cursor + 1 + diff,
                          x_cursor + word->size.width - 1,
                          y_cursor + 1 + diff);
         if (word_index + 1 < line->last_word &&
             word->space_style->uline >= 0)
            gdk_draw_line(window,
                          word->style->color->gc,
                          x_cursor + word->size.width,
                          y_cursor + 1 + diff,
                          x_cursor + word->size.width + word->eff_space - 1,
                          y_cursor + 1 + diff);
         
         /* strike-through */
         if (word->style->strike >= 0)
            gdk_draw_line(window,
                          word->style->color->gc,
                          x_cursor,
                          y_cursor - word->size.ascent / 2 + diff,
                          x_cursor + word->size.width - 1,
                          y_cursor - word->size.ascent / 2 + diff);
         if (word_index + 1 < line->last_word &&
             word->space_style->strike >= 0)
            gdk_draw_line(window,
                          word->style->color->gc,
                          x_cursor + word->size.width,
                          y_cursor - word->size.ascent / 2 + diff,
                          x_cursor + word->size.width + word->eff_space - 1,
                          y_cursor - word->size.ascent / 2 + diff);

         break;

      case DW_PAGE_CONTENT_WIDGET:
         child = word->content.widget;
         if (Dw_widget_intersect (child, area, &child_area))
            a_Dw_widget_draw (child, &child_area, event);

         break;

      case DW_PAGE_CONTENT_ANCHOR: case DW_PAGE_CONTENT_BREAK:
         /* nothing - an anchor/break isn't seen */
         /* BUG: sometimes anchors have x_space;
          * we subtract that just in case --EG */
         x_cursor -= word->size.width + word->eff_space;
#if 0
         /* Useful for testing: draw breaks. */
         if (word->content_type == DW_PAGE_CONTENT_BREAK)
            gdk_draw_rectangle (window, word->style->color->gc, TRUE,
                                Dw_widget_x_world_to_viewport(widget,
                                   widget->allocation.x +
                                      Dw_page_line_total_x_offset(page, line)),
                                y_cursor + line->descent,
                                widget->allocation.width -
                                   Dw_style_box_diff_width(widget->style),
                                word->content.break_space);
#endif
         break;

      default:
         //g_print ("                          BUG!!! at (%d, %d).\n",
         //         x_cursor, y_cursor + diff);

      }
      x_cursor += word->size.width + word->eff_space;
   }
}

/*
 * Find the first line index that includes y, relative to top of widget.
 */
static gint Dw_page_find_line_index(DwPage *page, gint y)
{
   gint max_index = page->num_lines - 1;
   gint step, index, low = 0;

   step = (page->num_lines+1) >> 1;
   while ( step > 1 ) {
      index = low + step;
      if (index <= max_index && page->lines[index].top < y)
         low = index;
      step = (step + 1) >> 1;
   }

   if (low < max_index && page->lines[low+1].top < y)
      low++;

   /*
    * This new routine returns the line number between (top) and
    * (top + size.ascent + size.descent + break_space): the space
    * _below_ the line is considered part of the line.  Old routine
    * returned line number between (top - previous_line->break_space)
    * and (top + size.ascent + size.descent): the space _above_ the
    * line was considered part of the line.  This is important for
    * Dw_page_find_link() --EG
    */
   return low;
}

/*
 * Draw the actual lines, starting at (x, y) in toplevel Dw coords.
 * (former Dw_page_expose_lines)
 */
static void Dw_page_draw (DwWidget *widget,
                          DwRectangle *area,
                          GdkEventExpose *event)
{
   DwPage *page;
   gint line_index;
   DwPageLine *line;

   Dw_widget_draw_widget_box (widget, area);

   page = DW_PAGE (widget);
   line_index = Dw_page_find_line_index(page, area->y);

   for (; line_index < page->num_lines; line_index++) {
      line = &(page->lines[line_index]);
      if (line->top >= area->y + area->height)
         break;

      Dw_page_draw_line (page, line, area, event);
   }
}


/*
 * Find a link given a coordinate location relative to the window
 */
static gint Dw_page_find_link(DwPage *page, gint x, gint y)
{
   gint line_index, word_index;
   gint x_cursor, last_x_cursor;
   DwPageLine *line;
   DwPageWord *word;

   if ( (line_index = Dw_page_find_line_index(page, y)) >= page->num_lines )
      return -1;
   line = &page->lines[line_index];
   if (line->top + line->ascent + line->descent <= y)
      return -1;

   x_cursor = Dw_page_line_total_x_offset(page, line);
   for (word_index = line->first_word; word_index < line->last_word;
        word_index++) {
      word = &page->words[word_index];
      last_x_cursor = x_cursor;
      x_cursor += word->size.width + word->eff_space;
      if (last_x_cursor <= x && x_cursor > x)
         return word->style->link;

   }
   return -1;
}


/*
 * Standard Dw function.
 */
static gint Dw_page_button_press  (DwWidget *widget,
                                   gint32 x,
                                   gint32 y,
                                   GdkEventButton *event)
{
   DwPage *page = DW_PAGE (widget);

#ifdef VERBOSE
   g_print("Dw_page_button_press: button (%d, %d) +%d\n",
           x, y, button->button);
#endif

   page->link_pressed = Dw_page_find_link(page, x, y);
   if (page->link_pressed >= 0) {
      gtk_signal_emit (GTK_OBJECT (widget), page_signals[LINK_PRESSED],
                       page->link_pressed, -1, -1, event);
      return TRUE;
   } else
      return FALSE;
}


/*
 * Standard Dw function.
 */
static gint Dw_page_button_release(DwWidget *widget,
                                   gint32 x,
                                   gint32 y,
                                   GdkEventButton *event)
{
   DwPage *page = DW_PAGE (widget);
   gint link_pressed, link_released;

#ifdef VERBOSE
   g_print("Dw_page_button_release: button (%d, %d) +%d\n",
           x, y, event->button);
#endif

   link_pressed = page->link_pressed;
   link_released = Dw_page_find_link(page, x, y);
   page->link_pressed = -1;

   if (link_released >= 0) {
      gtk_signal_emit (GTK_OBJECT (widget), page_signals[LINK_RELEASED],
                       link_released, -1, -1, event);

      if (link_pressed == link_released) {
         gtk_signal_emit (GTK_OBJECT (widget), page_signals[LINK_CLICKED],
                          link_released, -1, -1, event);
      }

      return TRUE;
   } else
      return FALSE;
}


/*
 * Standard Dw function.
 */
static gint Dw_page_motion_notify (DwWidget *widget,
                                   gint32 x,
                                   gint32 y,
                                   GdkEventMotion *event)
{
   DwPage *page = DW_PAGE (widget);
   gint link, link_old;

   link_old = page->hover_link;
   link = Dw_page_find_link(page, x, y);
   page->hover_link = link;

   if (link != link_old) {
      gtk_signal_emit (GTK_OBJECT (widget), page_signals[LINK_ENTERED],
                       link, -1, -1);
      return TRUE;
   } else
      return (link != -1);
}


/*
 * Standard Dw function.
 */
static gint Dw_page_leave_notify (DwWidget *widget,
                                  GdkEventMotion *event)
{
   DwPage *page = DW_PAGE (widget);

   if (page->hover_link != -1) {
      page->hover_link = -1;
      gtk_signal_emit (GTK_OBJECT (widget), page_signals[LINK_ENTERED],
                       -1, -1, -1);
   }

   return FALSE;
}


/*
 * Add a new word (text, widget etc.) to a page.
 */
static DwPageWord *Dw_page_add_word(DwPage *page,
                                    gint width,
                                    gint ascent,
                                    gint descent,
                                    DwStyle *style)
{
   DwPageWord *word;

   page->num_words++;
   a_List_add(page->words, page->num_words, sizeof(*page->words),
              page->num_words_max);

   word = &page->words[page->num_words - 1];
   word->size.width = width;
   word->orig_space = 0;
   word->eff_space = 0;
   word->size.ascent = ascent;
   word->size.descent = descent;

   word->style = style;
   word->space_style = style;
   a_Dw_style_ref (style);
   a_Dw_style_ref (style);

   return word;
}


/*
 * Add a word to the page structure. Stashes the argument pointer in
 * the page data structure so that it will be deallocated on destroy.
 */
void a_Dw_page_add_text(DwPage *page, char *text, DwStyle *style)
{
   DwPageWord *word;
   gint width, ascent, descent;

   width = gdk_string_width(style->font->font, text);
   ascent = style->font->font->ascent;
   descent = style->font->font->descent;

   /* In case of a sub or super script we increase the word's height and
    * potentially the line's height.
    */
   if (style->SubSup == TEXT_SUB)
      descent += (ascent / 2);
   else if (style->SubSup == TEXT_SUP)
      ascent += (ascent / 3);

   word = Dw_page_add_word(page, width, ascent, descent, style);
   word->content_type = DW_PAGE_CONTENT_TEXT;
   word->content.text = text;
   Dw_page_word_wrap (page, page->num_words - 1);
}

/*
 * Add a widget (word type) to the page.
 */
void a_Dw_page_add_widget (DwPage *page,
                           DwWidget *widget,
                           DwStyle *style)
{
   DwPageWord *word;
   DwRequisition size;

   a_Dw_widget_set_style (widget, style);

   Dw_page_calc_widget_size (page, widget, &size);
   word = Dw_page_add_word(page, size.width, size.ascent, size.descent, style);

   word->content_type = DW_PAGE_CONTENT_WIDGET;
   word->content.widget = widget;

   Dw_widget_set_parent (widget, DW_WIDGET (page));
   Dw_page_word_wrap (page, page->num_words - 1);
   word->content.widget->parent_ref = page->num_lines - 1;
}


/*
 * Add an anchor to the page. name is copied, so no strdup is neccessary for
 * the caller.
 */
void a_Dw_page_add_anchor(DwPage *page, const char *name, DwStyle *style)
{
   DwPageWord *word;

   word = Dw_page_add_word(page, 0, 0, 0, style);
   word->content_type = DW_PAGE_CONTENT_ANCHOR;
   word->content.anchor = g_strdup(name);
   Dw_page_word_wrap (page, page->num_words - 1);

   Dw_widget_set_anchor(DW_WIDGET(page), word->content.anchor,
                        page->lines[page->num_lines - 1].top);
}


/*
 * ?
 */
void a_Dw_page_add_space(DwPage *page, DwStyle *style)
{
   gint nl, nw;
   gint space;

   nl = page->num_lines - 1;
   if (nl >= 0) {
      nw = page->num_words - 1;
      if (nw >= 0) {
         space = style->font->space_width;
         page->words[nw].orig_space = space;
         page->words[nw].eff_space = space;

         a_Dw_style_unref (page->words[nw].space_style);
         page->words[nw].space_style = style;
         a_Dw_style_ref (style);
      }
   }
}


/*
 * Cause a break
 */
void a_Dw_page_add_break(DwPage *page, gint space, DwStyle *style)
{
   DwPageWord *word, *word2;
   DwWidget *widget;
   DwPage *page2;
   gboolean isfirst;
   gint lineno;

   /* A break may not be the first word of a page, or directly after
      the bullet/number (which is the first word) in a list item. (See
      also comment in Dw_page_size_request.) */
   if (page->num_words == 0 ||
       (page->list_item && page->num_words == 1)) {
      /* This is a bit hackish: If a break is added as the
         first/second word of a page, and the parent widget is also a
         DwPage, and there is a break before -- this is the case when
         a widget is used as a text box (lists, blockquotes, list
         items etc) -- then we simply adjust the break before, in a
         way that the space is in any case visible. */

      /* Find the widget where to adjust the break_space. */
      for (widget = DW_WIDGET(page);
           widget->parent && DW_IS_PAGE (widget->parent);
           widget = widget->parent) {
         page2 = DW_PAGE(widget->parent);
         if (page2->list_item)
            isfirst = (page2->words[1].content_type == DW_PAGE_CONTENT_WIDGET
                       && page2->words[1].content.widget == widget);
         else
            isfirst = (page2->words[0].content_type == DW_PAGE_CONTENT_WIDGET
                        && page2->words[0].content.widget == widget);
         if (!isfirst) {
            /* The page we searched for has been found. */
            lineno = widget->parent_ref;
            if (lineno > 0 &&
                (word2 = &page2->words[page2->lines[lineno - 1].first_word]) &&
                word2->content_type == DW_PAGE_CONTENT_BREAK) {
               if(word2->content.break_space < space) {
                  word2->content.break_space = space;
                  Dw_widget_queue_resize (DW_WIDGET(page2), lineno, FALSE);
                  page2->must_queue_resize = FALSE;
               }
            }
            return;
         }
         /* Otherwise continue to examine parents. */
      }
      /* Return in any case. */
      return;
   }

   /* Another break before? */
   if ((word = &page->words[page->num_words - 1]) &&
       word->content_type == DW_PAGE_CONTENT_BREAK) {
      word->content.break_space = MAX (word->content.break_space, space);
      return;
   }

   word = Dw_page_add_word(page, 0, 0, 0, style);
   word->content_type = DW_PAGE_CONTENT_BREAK;
   word->content.break_space = space;
   Dw_page_word_wrap (page, page->num_words - 1);
}

/*
 * This function "hands" the last break of a page "over" to a parent
 * page. This is used for "collapsing spaces".
 */
void a_Dw_page_hand_over_break (DwPage *page, DwStyle *style)
{
   DwPageLine *last_line;
   DwWidget *parent;

   if (page->num_lines == 0)
      return;

   last_line = &page->lines[page->num_lines - 1];
   if (last_line->break_space != 0 &&
       (parent = DW_WIDGET(page)->parent) && DW_IS_PAGE (parent))
      a_Dw_page_add_break (DW_PAGE (parent), last_line->break_space, style);
}

/*
 * todo: comment
 */
void a_Dw_page_flush (DwPage *page)
{
   if (page->must_queue_resize) {
      Dw_widget_queue_resize (DW_WIDGET(page), -1, TRUE);
      page->must_queue_resize = FALSE;
   }
}


/*
 * Find the text in the page.
 * (Standar DwContainer function)    -- todo: move near forall
 */
static gint Dw_page_findtext(DwContainer *container,
                             gpointer FP, gpointer KP,
                             gchar *NewKey)
{
   gint i;
   DwPageWord *word;
   FindData *F, *Fi, *Fj;
   DwPage *page = DW_PAGE(container);

   g_return_val_if_fail ((DW_WIDGET(page))->viewport != NULL, 0);
   if (!NewKey || !*NewKey)
      return 0;

   if ( !(F = *(FindData**)FP) )
      *(FindData **)FP = F = g_new0(FindData, 1);

   if ( !F->Key || strcmp(F->Key->KeyStr, NewKey) ||
        F->widget != DW_WIDGET (page) )
      F->State = F_NewKey;

   /* Let the FSM find the search string */
   while ( 1 ) {
      switch (F->State) {
      case F_NewKey:
         /* free FindData stack */
         for (Fi = F->next; Fi; Fi = Fj){
            Fj = Fi ? Fi->next : NULL;
            g_free(Fi);
         }
         /* initialize F */
         if (!KP && F->Key)
            a_Findtext_key_free(F->Key);
         F->Key = (KP) ? KP : a_Findtext_key_new(NewKey);
         F->widget = DW_WIDGET (page);
         F->WordNum = 0;
         F->next = NULL;
         F->State = F_Seek;
         break;
      case F_Seek:
         for (   ; F->WordNum < page->num_words; F->WordNum++) {
            word = &page->words[F->WordNum];
            if (word->content_type == DW_PAGE_CONTENT_TEXT &&
                a_Findtext_compare(word->content.text, F->Key)) {
               F->State = F_GetPos;
               break;
            } else if (word->content_type == DW_PAGE_CONTENT_WIDGET &&
                       DW_IS_CONTAINER (word->content.widget)) {
               if ( a_Dw_container_findtext(
                       DW_CONTAINER(word->content.widget),
                       (gpointer)&F->next, (gpointer)F->Key, NewKey) ) {
                  F->State = F_Seek;
                  return 1;
               }
            }
         }
         if (F->WordNum == page->num_words)
            F->State = F_End;
         break;
      case F_GetPos:
         if (F->Key->y_pos < 0) {
            for (i = 0; page->lines[i].last_word <= F->WordNum; ++i);
            F->Key->y_pos = page->lines[i].top;
         }
         F->WordNum++;
         F->State = (g_slist_length(F->Key->WordList) == F->Key->Matches) ?
                    F_Found : F_Seek;
         break;
      case F_Found:
         a_Dw_widget_scroll_to(DW_WIDGET (page), F->Key->y_pos);
         //g_print(">>>[Wn %d]\n", F->WordNum);
         F->State = F_Seek;
         return 1;
      case F_End:
         /* free memory */
         if (!KP && F->Key)
            a_Findtext_key_free(F->Key);
         g_free(F);
         *(FindData **)FP = NULL;
         return 0;
      }
   }

   /* compiler happiness */
   return 0;
}
