/* This module contains the dw_page widget, which is the "back end" to
   Web text widgets including html. */

#ifndef __DW_PAGE_H__
#define __DW_PAGE_H__

#undef USE_TYPE1

#include <gdk/gdk.h>
#include "dw_container.h"
#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DW_TYPE_PAGE            (a_Dw_page_get_type ())
#define DW_PAGE(obj)            GTK_CHECK_CAST ((obj), DW_TYPE_PAGE, DwPage)
#define DW_PAGE_CLASS(klass)    GTK_CHECK_CLASS_CAST ((klass), DW_TYPE_PAGE, \
                                   DwPageClass)
#define DW_IS_PAGE(obj)         GTK_CHECK_TYPE ((obj), DW_TYPE_PAGE)
#define DW_IS_PAGE_CLASS(klass) GTK_CHECK_CLASS_TYPE ((klass), DW_TYPE_PAGE)


typedef struct _DwPage        DwPage;
typedef struct _DwPageClass   DwPageClass;

/* Internal data structures (maybe shouldn't be in public .h file? */
typedef struct _DwPageLine   DwPageLine;
typedef struct _DwPageWord   DwPageWord;


struct _DwPageLine
{
   gint first_word;    /* first-word's position in DwPageWord [0 based] */
   gint last_word;     /* last-word's position in DwPageWord [1 based] */

   gint32 top, ascent, descent, break_space, left_offset;

   /* The following members contain accumulated values, from the top
    * down to the line before. */
   gint32 max_line_width; /* maximum of all line widths */
   gint32 max_word_min;   /* maximum of all word minima */
   gint32 max_par_max;    /* maximum of all paragraph maxima */
   gint32 par_min;        /* the minimal total width down from the last
                             paragraph start */
   gint32 par_max;        /* the maximal total width down from the last
                             paragraph start */
};

#define DW_PAGE_CONTENT_TEXT   0
#define DW_PAGE_CONTENT_WIDGET 1
#define DW_PAGE_CONTENT_ANCHOR 2
#define DW_PAGE_CONTENT_BREAK  3

struct _DwPageWord {
   /* todo: perhaps add a x_left? */
   DwRequisition size;
   /* Space after the word, only if it's not a break: */
   gint32 orig_space; /* from font, set by a_Dw_page_add_space */
   gint32 eff_space;  /* effective space, set by Dw_page_word_wrap,
                         used for drawing etc. */

   /* This is a variant record (i.e. it could point to a widget
    * instead of just being text). */
   gint content_type;
   union {
      char *text;
      DwWidget *widget;
      char *anchor;
      gint break_space;
   } content;

   DwStyle *style;
   DwStyle *space_style; /* initially the same as of the word, later
                            set by a_Dw_page_add_space */
};


struct _DwPage
{
   DwContainer container;

   /* These fields provide some ad-hoc-functionality, used by sub-classes. */
   gboolean list_item;    /* If TRUE, the first word of the page is treated
                             specially (search in source). */
   gint32 inner_padding;  /* This is an additional padding on the left side
                             (used by DwListItem). */
   gint32 line1_offset;   /* This is an additional offset of the first line.
                             May be negative (shift to left) or positive
                             (shift to right). */

   gboolean must_queue_resize;

   /* These values are set by set_... */
   gint32 avail_width, avail_ascent, avail_descent;

   gint32 last_line_width;
   gint wrap_ref;

   DwPageLine *lines;
   gint num_lines;
   gint num_lines_max; /* number allocated */

   DwPageWord *words;
   gint num_words;
   gint num_words_max; /* number allocated */

   /* The link under a button press */
   gint link_pressed;

   /* The link under the button */
   gint hover_link;
};


struct _DwPageClass
{
   DwContainerClass parent_class;

   void (*link_entered)  (DwPage *page,
                          gint link, gint x, gint y);
   void (*link_pressed)  (DwPage *page,
                          gint link, gint x, gint y,
                          GdkEventButton *event);
   void (*link_released) (DwPage *page,
                          gint link, gint x, gint y,
                          GdkEventButton *event);
   void (*link_clicked)  (DwPage *page,
                          gint link, gint x, gint y,
                          GdkEventButton *event);
};


DwWidget*  a_Dw_page_new           (void);
GtkType    a_Dw_page_get_type      (void);

void a_Dw_page_flush (DwPage *page);
void a_Dw_page_add_text (DwPage *page, char *text, DwStyle *style);
void a_Dw_page_add_widget (DwPage *page,
                           DwWidget *widget,
                           DwStyle *style);
void a_Dw_page_add_anchor (DwPage *page, const char *name, DwStyle *style);
void a_Dw_page_add_space(DwPage *page, DwStyle *style);
void a_Dw_page_add_break (DwPage *page, gint space, DwStyle *style);
void a_Dw_page_hand_over_break (DwPage *page, DwStyle *style);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __DW_PAGE_H__ */
