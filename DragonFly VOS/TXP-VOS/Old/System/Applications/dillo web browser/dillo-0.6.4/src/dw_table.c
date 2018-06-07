/*
 * File: dw_table.c
 *
 * Copyright (C) 2001 Sebastian Geerken <sgeerken@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <string.h>

#include "dw_table.h"
#include "list.h"
#include "prefs.h"
#include "dw_gtk_viewport.h"

#define DEBUG_CALC_LEVEL   0
#define DEBUG_EXTR_LEVEL   0
#define DEBUG_WIDTH_LEVEL 10

//#define DEBUG_LEVEL 10
#include "debug.h"

/*
 * Some notes:
 *
 *    - table->num_children always has the value table->num_cols *
 *      table->num_rows, and shows the minimal size of
 *      table->children, *not* the number of children.
 *
 *    - table->children[i] may be NULL (unused cells), or a pointer to
 *      DwTableChild. A DwTableChild may be a span space (blocked by
 *      COLSPAN or ROWSPAN of another cell), or a cell starting at
 *      this position. In the last case, the macro CHILD_DEFINED
 *      returns TRUE.
 */

/*#define BORDERS*/

#define CHILD_DEFINED(table, n) ((n) < (table)->num_children && \
                                 (table)->children[(n)] != NULL && \
                                 (table)->children[(n)]->type != \
                                    DW_TABLE_SPAN_SPACE)


static void Dw_table_init          (DwTable *table);
static void Dw_table_class_init    (DwTableClass *klass);

static void Dw_table_destroy       (GtkObject *object);

static void Dw_table_size_request  (DwWidget *widget,
                                    DwRequisition *requisition);
static void Dw_table_get_extremes  (DwWidget *widget,
                                    DwExtremes *extremes);
static void Dw_table_size_allocate (DwWidget *widget,
                                    DwAllocation *allocation);
static void Dw_table_set_width     (DwWidget *widget,
                                    gint32 width);
static void Dw_table_set_ascent    (DwWidget *widget,
                                    gint32 ascent);
static void Dw_table_set_descent   (DwWidget *widget,
                                    gint32 descent);
static void Dw_table_draw          (DwWidget *widget,
                                    DwRectangle *area,
                                    GdkEventExpose *event);

static void Dw_table_add           (DwContainer *container,
                                    DwWidget *widget);
static void Dw_table_remove        (DwContainer *container,
                                    DwWidget *widget);
static void Dw_table_forall        (DwContainer *container,
                                    DwCallback callback,
                                    gpointer callback_data);
static gint Dw_table_findtext      (DwContainer *container,
                                    gpointer FP, gpointer KP,
                                    gchar *NewKey);

static void Dw_table_realloc_children (DwTable *table);

static void Dw_table_sub_create          (DwTable *table);
static void Dw_table_sub_free            (DwTableSub *sub);
static void Dw_table_sub_get_extremes    (DwTableSub *sub);
static void Dw_table_sub_calc_col_widths (DwTableSub *sub,
                                          gint32 width,
                                          gint32 total_width);


static DwContainerClass *parent_class;

enum {
   /* values of use_percentage */
   USE_PERCENTAGE_NO,    /* No percentage in subtable, and USE_PERCENTAGE_NO
                          * in all sub-subtables.
                          */
   USE_PERCENTAGE_SOME,  /* None of the other cases. This affects the minimal,
                          * but not the maximal width of the subtable.
                          */
   USE_PERCENTAGE_ALL    /* Percentage in subtable, or USE_PERCENTAGE_ALL in
                          * all sub-subtables. This affects the minimal and
                          * the maximal width of the subtable.
                          */
};

/*
 * Standard Gtk+ function.
 */
GtkType a_Dw_table_get_type (void)
{
   static GtkType type = 0;

   if (!type) {
      GtkTypeInfo info = {
         "DwTable",
         sizeof (DwTable),
         sizeof (DwTableClass),
         (GtkClassInitFunc) Dw_table_class_init,
         (GtkObjectInitFunc) Dw_table_init,
         (GtkArgSetFunc) NULL,
         (GtkArgGetFunc) NULL,
         (GtkClassInitFunc) NULL
      };

      type = gtk_type_unique (DW_TYPE_CONTAINER, &info);
   }

   return type;
}


/*
 * Standard Gtk+ function.
 */
DwWidget* a_Dw_table_new (void)
{
   GtkObject *object;

   object = gtk_object_new (DW_TYPE_TABLE, NULL);
   return DW_WIDGET (object);
}


/*
 * Standard Gtk+ function.
 */
static void Dw_table_init (DwTable *table)
{
   DW_WIDGET_SET_FLAGS (table, DW_USES_HINTS);

   /* random value */
   table->avail_width = 100;

   table->cur_row = -1;
   table->cur_col = 0;

   table->num_children_max = 1;  /* 16 */
   table->num_children = 0;
   table->children = NULL;

   table->num_col_width_max = 1; /* 8 */
   table->num_cols = 0;
   table->col_width = NULL;

   table->cum_height_max = 1;   /* row cumulative height array */
   table->num_rows = 0;         /* num_cum_height is (num_rows + 1) */
   table->cum_height = g_new(gint32, table->cum_height_max);

   table->row_style_max = 1;
   table->row_style = g_new(DwStyle*, table->row_style_max);

   table->sub = NULL;
}


/*
 * Standard Gtk+ function.
 */
static void Dw_table_class_init (DwTableClass *klass)
{
   GtkObjectClass *object_class;
   DwWidgetClass *widget_class;
   DwContainerClass *container_class;

   object_class = (GtkObjectClass*) klass;
   widget_class = (DwWidgetClass*) klass;
   container_class = (DwContainerClass*) klass;

   parent_class = gtk_type_class (DW_TYPE_CONTAINER);

   object_class->destroy = Dw_table_destroy;

   widget_class->size_request = Dw_table_size_request;
   widget_class->get_extremes = Dw_table_get_extremes;
   widget_class->size_allocate = Dw_table_size_allocate;
   widget_class->set_width = Dw_table_set_width;
   widget_class->set_ascent = Dw_table_set_ascent;
   widget_class->set_descent = Dw_table_set_descent;
   widget_class->draw = Dw_table_draw;

   container_class->add = Dw_table_add;
   container_class->remove = Dw_table_remove;
   container_class->forall = Dw_table_forall;
   container_class->findtext = Dw_table_findtext;
}


/*
 * Standard Gtk+ function.
 */
static void Dw_table_destroy (GtkObject *object)
{
   DwTable *table = DW_TABLE (object);
   gint i;

   for (i = 0; i < table->num_children; i++)
      if (table->children[i]) {
         if (table->children[i]->type == DW_TABLE_CELL)
            gtk_object_unref(GTK_OBJECT(table->children[i]->data.cell.widget));
         g_free (table->children[i]);
      }

   g_free (table->children);
   table->num_children = 0;
   g_free (table->col_width);
   g_free (table->cum_height);

   for (i = 0; i < table->num_rows; i++)
      if (table->row_style[i])
         a_Dw_style_unref (table->row_style[i]);
   g_free (table->row_style);

   if (table->sub)
      Dw_table_sub_free (table->sub);

   (* GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}


/*
 * Standard Dw function.
 * Furthermore, table->width and table->cum_height are filled.
 */
static void Dw_table_size_request (DwWidget *widget,
                                   DwRequisition *requisition)
{
   DwExtremes extremes;
   gint32 avail_width;
   DwTable *table = DW_TABLE (widget);
   gint row, row2, col, col2, n;
   gint32 child_height, set_width;
   DwWidget *child;
   DwRequisition child_requisition;

   /* Calculate the widths of the columns, and store it in table->col_width. */
   if (table->num_cols > 0) {
      a_Dw_widget_get_extremes (DW_WIDGET (table), &extremes);

      if (DW_STYLE_IS_LENGTH (widget->style->width))
         avail_width = DW_STYLE_GET_LENGTH (widget->style->width,
                                            widget->style->font);
      else if (DW_STYLE_IS_PERCENTAGE (widget->style->width))
         /*
          * If the width is > 100%, we use 100%, this prevents ugly
          * results. (May be changed in future, when a more powerful
          * rendering is implemented, to handle fixed positions etc.,
          * as defined by CSS2.)
          */
         avail_width = table->avail_width *
            MIN (DW_STYLE_GET_PERCENTAGE (widget->style->width), 1.0);
      else
         avail_width = table->avail_width;

      if (avail_width < extremes.min_width)
         avail_width = extremes.min_width;
      if (widget->style->width == DW_STYLE_UNDEF_LENGTH &&
          avail_width > extremes.max_width)
         avail_width = extremes.max_width;

      DEBUG_MSG (DEBUG_WIDTH_LEVEL + 3,
                 "--> Calculating column widths for (%d x %d) table.\n",
                 table->num_cols, table->num_rows);
      DEBUG_MSG (DEBUG_WIDTH_LEVEL + 3,
                 "    [ %d --(%d, %d)--> %d ]\n",
                 table->avail_width, extremes.min_width, extremes.max_width,
                 avail_width);

      avail_width -= (2 * widget->style->border_spacing +
                      Dw_style_box_diff_width (widget->style));
      Dw_table_sub_calc_col_widths
         (table->sub, avail_width,
          avail_width - (table->num_cols - 1) * widget->style->border_spacing);

      DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2, "<-- END\n");
   }

   /* Calculate the total width. */
   requisition->width = Dw_style_box_diff_width (widget->style) +
      (table->num_cols + 1) * widget->style->border_spacing;
   for (col = 0; col < table->num_cols; col++)
      requisition->width += table->col_width[col];

   /* Calculate the height. */
   for (row = 0; row <= table->num_rows; row++)
      table->cum_height[row] = widget->style->border_spacing;

   for (row = 0; row < table->num_rows; row++) {
      for (col = 0; col < table->num_cols; col++) {
         n = col + row * table->num_cols;
         if (CHILD_DEFINED (table, n)) {
            child = table->children[n]->data.cell.widget;

            set_width = 0;
            for (col2 = col;
                 col2 < col + table->children[n]->data.cell.colspan;
                 col2++)
               set_width += table->col_width[col2];

            a_Dw_widget_set_width (child, set_width);
            a_Dw_widget_set_ascent (child, table->avail_ascent);
            a_Dw_widget_set_descent (child, table->avail_descent);

            a_Dw_widget_size_request (child, &child_requisition);
            child_height =
               child_requisition.ascent + child_requisition.descent;

            /*
             * Adjust the row offsets. This is *always* done from the
             * current position to the bottom, so this is perhaps
             * worth to be optimized (?).
             */
            for (row2 = row + table->children[n]->data.cell.rowspan;
                 row2 <= table->num_rows ;
                 row2++)
               if ( table->cum_height[row2] <
                    table->cum_height[row] + child_height
                    + widget->style->border_spacing ) {
                  table->cum_height[row2] =
                     table->cum_height[row] + child_height
                     + widget->style->border_spacing;
                  DEBUG_MSG (1, "Row %d starts at %d.\n",
                             row2,
                             table->cum_height[row +
                                           table->children[n]
                                           ->data.cell.rowspan]);
               }

            /* todo: column widths should be adjusted a bit. */
         }
      }
   }

   requisition->ascent = table->cum_height[table->num_rows] +
                         Dw_style_box_diff_height (widget->style);
   requisition->descent = 0;

   DEBUG_MSG (2, "Dw_table_size_request: %d x %d x %d\n",
              requisition->width, requisition->ascent, requisition->descent);
}


/*
 * Standard Dw function.
 */
static void Dw_table_get_extremes (DwWidget *widget,
                                   DwExtremes *extremes)
{
   DwTable *table = DW_TABLE (widget);
   gint32 diff;

   if (table->num_cols > 0) {
      if (table->sub == NULL)
         Dw_table_sub_create (table);

      DEBUG_MSG (DEBUG_EXTR_LEVEL + 3,
                 "--> Dw_table_get_extremes for (%d x %d) table.\n",
                 table->num_cols, table->num_rows);

      Dw_table_sub_get_extremes (table->sub);
      diff = 2 * widget->style->border_spacing +
         Dw_style_box_diff_width (widget->style);
      extremes->min_width = table->sub->total_extremes.min_width + diff;
      extremes->max_width = table->sub->total_extremes.max_width + diff;

      DEBUG_MSG (DEBUG_EXTR_LEVEL + 2, "<-- END\n");
   } else {
      extremes->min_width = widget->style->border_spacing
         + Dw_style_box_diff_width (widget->style);
      extremes->max_width = widget->style->border_spacing
         + Dw_style_box_diff_width (widget->style);
   }
}


/*
 * Standard Dw function.
 */
static void Dw_table_size_allocate (DwWidget *widget,
                                    DwAllocation *allocation)
{
   DwTable *table = DW_TABLE (widget);
   gint row, col, col2, n;
   gint32 col_offset, cell_width, vdiff;
   DwWidget *child;
   DwAllocation child_allocation;

   /*
    * todo: This implementation depends on that size_request has been
    *       called before. Can this be assumed?
    */

   DEBUG_MSG (2, "Dw_table_size_allocate: %d x %d x %d\n",
              allocation->width, allocation->ascent, allocation->descent);

   vdiff = allocation->y + Dw_style_box_offset_y (widget->style);

   for (row = 0; row < table->num_rows; row++) {
      col_offset = allocation->x + widget->style->border_spacing
         + Dw_style_box_offset_x (widget->style);

      for (col = 0; col < table->num_cols; col++) {
         n = col + row * table->num_cols;
         if (CHILD_DEFINED (table, n)) {
            cell_width = (table->children[n]->data.cell.colspan - 1)
               * widget->style->border_spacing;
            for (col2 = col;
                 col2 < col + table->children[n]->data.cell.colspan;
                 col2++)
               cell_width += table->col_width[col2];

            child = table->children[n]->data.cell.widget;
            child_allocation.x = col_offset;
            child_allocation.y = vdiff + table->cum_height[row];
            child_allocation.width = cell_width;
            child_allocation.ascent =
               table->cum_height[row + table->children[n]->data.cell.rowspan] -
               table->cum_height[row] - widget->style->border_spacing;
            child_allocation.descent = 0;
            a_Dw_widget_size_allocate (child, &child_allocation);
         }

         col_offset += table->col_width[col] + widget->style->border_spacing;
      }
   }
}


/*
 * Standard Dw function.
 */
static void Dw_table_set_width (DwWidget *widget,
                                gint32 width)
{
   DwTable *table;

   table = DW_TABLE (widget);
   /* If limit_text_width is set to YES, a queue_resize may also be
      necessary. */
   if (table->avail_width != width || prefs.limit_text_width) {
      table->avail_width = width;
      Dw_widget_queue_resize (widget, 0, FALSE);
   }
}


/*
 * Standard Dw function.
 */
static void Dw_table_set_ascent (DwWidget *widget,
                                 gint32 ascent)
{
   DwTable *table;

   table = DW_TABLE (widget);
   if (table->avail_ascent != ascent) {
      table->avail_ascent = ascent;
      Dw_widget_queue_resize (widget, 0, FALSE);
   }
}


/*
 * Standard Dw function.
 */
static void Dw_table_set_descent (DwWidget *widget,
                                  gint32 descent)
{
   DwTable *table;

   table = DW_TABLE (widget);
   if (table->avail_descent != descent) {
      table->avail_descent = descent;
      Dw_widget_queue_resize (widget, 0, FALSE);
   }
}

/*
 * Standard Dw function.
 */
static void Dw_table_draw (DwWidget *widget,
                           DwRectangle *area,
                           GdkEventExpose *event)
{
   /* can be optimized, by iterating on the lines in area */
   DwTable *table = DW_TABLE (widget);
   DwRectangle child_area;
   DwWidget *child;
   gint i;
   gint32 offx, offy, width;

   Dw_widget_draw_widget_box (widget, area);

   offx = Dw_style_box_offset_x (widget->style);
   offy = Dw_style_box_offset_y (widget->style);
   width = widget->allocation.width - Dw_style_box_diff_width(widget->style);
   for (i = 0; i < table->num_rows; i++)
      if (table->row_style[i])
         Dw_widget_draw_box (widget, table->row_style[i], area,
                             offx, offy + table->cum_height[i],
                             width,
                             table->cum_height[i + 1] - table->cum_height[i]
                             - widget->style->border_spacing);

   for (i = 0; i < table->num_children; i++)
      if (CHILD_DEFINED (table, i)) {
         child = table->children[i]->data.cell.widget;
         if (Dw_widget_intersect (child, area, &child_area)) {
#ifdef BORDERS
            gdk_draw_rectangle
               (DW_WIDGET_WINDOW (child),
                widget->viewport->style->dark_gc[widget->viewport->state],
                FALSE,
                Dw_widget_x_world_to_viewport (child, child->allocation.x) - 1,
                Dw_widget_y_world_to_viewport (child, child->allocation.y) - 1,
                child->allocation.width + 2,
                child->allocation.ascent + child->allocation.descent + 2);
#endif
            a_Dw_widget_draw (child, &child_area, event);
         }
      }

#ifdef BORDERS
   gdk_draw_rectangle (DW_WIDGET_WINDOW (widget),
                       widget->style->color->gc, FALSE,
                       Dw_widget_x_world_to_viewport (widget,
                                                      widget->allocation.x),
                       Dw_widget_y_world_to_viewport (widget,
                                                      widget->allocation.y),
                       widget->allocation.width,
                       widget->allocation.ascent + widget->allocation.descent);
#endif
}


/*
 * Standard Dw function.
 */
static void Dw_table_add (DwContainer *container,
                          DwWidget *widget)
{
   a_Dw_table_add_cell (DW_TABLE (container), widget, 1, 1);
}


/*
 * Standard Dw function.
 */
static void Dw_table_remove (DwContainer *container,
                             DwWidget *widget)
{
   /* used? */
}


/*
 * Standard Dw function.
 */
static void Dw_table_forall (DwContainer *container,
                             DwCallback callback,
                             gpointer callback_data)
{
   gint i;
   DwTable *table = DW_TABLE (container);

   for (i = 0; i < table->num_children; i++)
      if (CHILD_DEFINED (table, i))
         callback (table->children[i]->data.cell.widget, callback_data);
}


/*
 * Standard DwContainer function.
 */
static gint Dw_table_findtext (DwContainer *container,
                               gpointer FP, gpointer KP,
                               gchar *NewKey)
{
   DwWidget *widget;
   DwTable *table = DW_TABLE (container);
   FindData *F = *(FindData**)FP;

   g_return_val_if_fail ((DW_WIDGET(table))->viewport != NULL && KP, 0);

   if ( !F )
      *(FindData **)FP = F = g_new0(FindData, 1);

   if ( !F->Key || strcmp(F->Key->KeyStr, NewKey) ||
        F->widget != DW_WIDGET (table) )
      F->State = F_NewKey;

   /* Let the FSM find the search string */
   while ( 1 ) {
      switch (F->State) {
      case F_NewKey:
         F->Key = KP;
         F->widget = DW_WIDGET (table);
         F->WordNum = 0;
         F->State = F_Seek;
         break;
      case F_Seek:
         for (   ; F->WordNum < table->num_children; F->WordNum++) {
           if (CHILD_DEFINED (table, F->WordNum)){
              widget = table->children[F->WordNum]->data.cell.widget;
              if (DW_IS_CONTAINER (widget) &&
                  a_Dw_container_findtext(DW_CONTAINER (widget),
                                          (gpointer)&F->next,
                                          (gpointer)F->Key,
                                          NewKey)){
                  F->State = F_Seek;
                  return 1;
              }
           }
         }
         F->State = F_End;
         break;
      case F_GetPos:
      case F_Found:
      case F_End:
         /* free memory */
         g_free(F);
         *(FindData **)FP = NULL;
         return 0;
      }
   }

   /* compiler happiness */
   return 0;
}


/*
 * Make sure that there is enough space for table->children,
 * table->cum_height and table->row_style, and fill the rest of
 * table->children with NULL.
 */
static void Dw_table_realloc_children (DwTable *table)
{
   gint old_num_children, i;

   table->num_rows++;
   a_List_resize (table->cum_height, table->num_rows, sizeof(gint32),
                  table->cum_height_max);
   table->num_rows--;
   a_List_resize (table->row_style, table->num_rows, sizeof(gint32),
                  table->row_style_max);

   old_num_children = table->num_children;
   table->num_children = table->num_cols * table->num_rows;
   a_List_resize (table->children, table->num_children, sizeof(DwTableChild*),
                  table->num_children_max);

   for (i = old_num_children; i < table->num_children; i++)
      table->children[i] = NULL;
}


/*
 * Add widget as a cell to table. colspan and rowspan have the same
 * meaning as the attributes of <td> and <th>.
 */
void a_Dw_table_add_cell (DwTable *table,
                          DwWidget *widget,
                          gint colspan,
                          gint rowspan)
{
   gint col, row, old_num_cols, old_num_rows;
   DwTableChild *child;

   if (table->num_rows == 0)
      /* to prevent a crash */
      a_Dw_table_add_row (table, NULL);

   /* todo: {col|row}span = 0 is not supported yet: */
   if (colspan < 1)
      colspan = 1;
   if (rowspan < 1)
      rowspan = 1;

   while (table->cur_col < table->num_cols &&
          (child = table->children[table->cur_row * table->num_cols
                                   + table->cur_col]) != NULL &&
          child->type == DW_TABLE_SPAN_SPACE)
      table->cur_col++;

   if (table->cur_row + rowspan > table->num_rows) {
      old_num_rows = table->num_rows;
      table->num_rows = table->cur_row + rowspan;
      Dw_table_realloc_children (table);
      for (row = old_num_rows; row < table->num_rows; row++)
         table->row_style[row] = NULL;
   }

   if (table->cur_col + colspan > table->num_cols) {
      old_num_cols = table->num_cols;
      table->num_cols = table->cur_col + colspan;
      a_List_resize (table->col_width, table->num_cols, sizeof(gint32),
                     table->num_col_width_max);

      table->num_children = table->num_cols * table->num_rows;
      a_List_resize (table->children, table->num_children,
                     sizeof(DwTableChild*), table->num_children_max);

      for (row = table->num_rows - 1; row >= 0; row--) {
         for (col = old_num_cols - 1; col >= 0; col--) {
            table->children[row * table->num_cols + col] =
               table->children[row * old_num_cols + col];

         }

         for (col = old_num_cols; col < table->num_cols; col++)
            table->children[row * table->num_cols + col] = NULL;
      }
   }

   for (col = 0; col < colspan; col++)
      for (row = 0; row < rowspan; row++)
         if (!(col == 0 && row == 0)) {
            child = g_new (DwTableChild, 1);
            child->type = DW_TABLE_SPAN_SPACE;
            child->data.span_space.start_col = table->cur_col;
            child->data.span_space.start_row = table->cur_row;
            table->children[(table->cur_row + row) * table->num_cols
                           + (table->cur_col + col)] = child;
         }

   child = g_new (DwTableChild, 1);
   child->type = DW_TABLE_CELL;
   child->data.cell.widget = widget;
   child->data.cell.colspan = colspan;
   child->data.cell.rowspan = rowspan;
   table->children[table->cur_row * table->num_cols + table->cur_col] = child;
   table->cur_col += colspan;

   Dw_widget_set_parent (widget, DW_WIDGET (table));
   Dw_widget_queue_resize (widget, 0, TRUE);

   /* The cell structure has changed, so the subtables have to be rebuilt. */
   if (table->sub) {
      Dw_table_sub_free (table->sub);
      table->sub = NULL;
   }
}


/*
 * Add a new row to the table, and start adding cells from the
 * left-most column.
 */
void a_Dw_table_add_row (DwTable *table,
                         DwStyle *style)
{
   gint row, old_num_rows;

   table->cur_row++;

   if (table->cur_row >= table->num_rows) {
      old_num_rows = table->num_rows;
      table->num_rows = table->cur_row + 1;
      Dw_table_realloc_children (table);
      for (row = old_num_rows; row < table->num_rows; row++)
         table->row_style[row] = NULL;
   }

   if (table->row_style[table->cur_row])
      a_Dw_style_unref (table->row_style[table->cur_row]);
   table->row_style[table->cur_row] = style;
   if (style)
      a_Dw_style_ref (style);

   table->cur_col = 0;
}


/* ----------------------------------------------------------------------
 *
 *    General Functions for Subtables
 *
 * ---------------------------------------------------------------------- */

static gboolean Dw_table_sub_spans_width (DwTableSub *sub,
                                         gint row)
{
   gint n, start_col, start_row, start_n, colspan;

   n = sub->start_col + sub->table->num_cols * row;

   if (sub->table->children[n]) {
      if (sub->table->children[n]->type == DW_TABLE_CELL) {
         start_col = sub->start_col;
         start_row = row;
      } else {
         start_col = sub->table->children[n]->data.span_space.start_col;
         start_row = sub->table->children[n]->data.span_space.start_row;
      }

      start_n = start_col + sub->table->num_cols * start_row;
      colspan = sub->table->children[start_n]->data.cell.colspan;

      return (start_col <= sub->start_col &&
              start_col + colspan >= sub->end_col);
   } else
      return FALSE;
}

/*
 * Fill the DwTableSub and create the sub DwTableSub's. Only for use
 * by Dw_table_get_sub.
 */
static void Dw_table_sub_calc_subs (DwTableSub *sub)
{
   gint col, row, n, start_n, colspan, most_right, start_col, start_row;

   if (sub->start_col + 1 == sub->end_col) {
      DEBUG_MSG (DEBUG_CALC_LEVEL + 2, "   single column at %d\n",
                 sub->start_col);
      sub->num_subs = 0;
      sub->subs = NULL;

      for (row = 0; row < sub->table->num_rows; row++)
         if (!a_Bitvec_get_bit (sub->removed_rows, row)) {
            a_Bitvec_set_bit (sub->spanning_rows, row);
            DEBUG_MSG (DEBUG_CALC_LEVEL + 1,
                       "      row %d spans width\n", row);

         }
   } else {
      DEBUG_MSG (DEBUG_CALC_LEVEL + 2,
                 "   --> complex subtable from %d to %d\n",
                 sub->start_col, sub->end_col);

      for (row = 0; row < sub->table->num_rows; row++)
         if (!a_Bitvec_get_bit (sub->removed_rows, row) &&
             Dw_table_sub_spans_width (sub, row)) {
            a_Bitvec_set_bit (sub->spanning_rows, row);
            DEBUG_MSG (DEBUG_CALC_LEVEL + 1,
                       "      row %d spans width\n", row);

         }

      sub->num_subs = 0;
      sub->subs = g_new (DwTableSub, sub->end_col - sub->start_col);

      col = sub->start_col;
      while (col < sub->end_col) {
         DEBUG_MSG (DEBUG_CALC_LEVEL + 1, "      column %d\n", col);

         sub->subs[sub->num_subs].table = sub->table;
         sub->subs[sub->num_subs].start_col = col;
         sub->subs[sub->num_subs].removed_rows =
            a_Bitvec_new (sub->table->num_rows);
         sub->subs[sub->num_subs].spanning_rows =
            a_Bitvec_new (sub->table->num_rows);

         /* Search for the greatest colspan value. */
         most_right = col + 1;
         for (row = 0; row < sub->table->num_rows; row++)
            if (a_Bitvec_get_bit (sub->removed_rows, row) ||
                a_Bitvec_get_bit (sub->spanning_rows, row))
               a_Bitvec_set_bit (sub->subs[sub->num_subs].removed_rows, row);
            else {
               n = col + sub->table->num_cols * row;

               if (sub->table->children[n]) {
                  if (sub->table->children[n]->type == DW_TABLE_CELL) {
                     start_col = col;
                     start_row = row;
                     start_n = n;
                  } else {
                     start_col =
                        sub->table->children[n]->data.span_space.start_col;
                     start_row =
                        sub->table->children[n]->data.span_space.start_row;
                     start_n = start_col + sub->table->num_cols * start_row;
                  }

                  if (row == start_row) {
                     colspan =
                        sub->table->children[start_n]->data.cell.colspan -
                        (col - start_col);

                     if (start_col + colspan > most_right &&
                         start_col + colspan <= sub->end_col)
                        most_right = start_col + colspan;
                  }
               }
            }

         sub->subs[sub->num_subs].end_col = most_right;
         Dw_table_sub_calc_subs (&sub->subs[sub->num_subs]);

         sub->num_subs++;
         col = most_right;
      }

      DEBUG_MSG (DEBUG_CALC_LEVEL + 1, "   <-- END\n");
   }
}

/*
 * Create a DwTableSub for a table.
 */
static void Dw_table_sub_create (DwTable *table)
{
   if (table->sub)
      Dw_table_sub_free (table->sub);

   DEBUG_MSG (DEBUG_CALC_LEVEL + 3,
              "--> Dw_table_sub_create widths for (%d x %d) table.\n",
              table->num_cols, table->num_rows);

   table->sub = g_new (DwTableSub, 1);
   table->sub->table = table;
   table->sub->start_col = 0;
   table->sub->end_col = table->num_cols;
   table->sub->removed_rows = a_Bitvec_new (table->num_rows);
   table->sub->spanning_rows = a_Bitvec_new (table->num_rows);

   Dw_table_sub_calc_subs (table->sub);

   DEBUG_MSG (DEBUG_CALC_LEVEL + 2, "<-- END\n");
}

/*
 * Clean up a DwTableSub. Only for use by Dw_table_free_sub.
 */
static void Dw_table_sub_clean_up (DwTableSub *sub)
{
   gint i;

   a_Bitvec_free (sub->removed_rows);
   a_Bitvec_free (sub->spanning_rows);
   if (sub->subs) {
      for (i = 0; i < sub->num_subs; i++)
         Dw_table_sub_clean_up (&sub->subs[i]);
      g_free (sub->subs);
   }
}

/*
 * Free a DwTableSub.
 */
static void Dw_table_sub_free (DwTableSub *sub)
{
   Dw_table_sub_clean_up (sub);
   g_free (sub);
}


/* ----------------------------------------------------------------------
 *
 *    Specific Functions for Subtables
 *
 *    NOTE: The spaces left of the left most column and right of the
 *    right most column do not belong to a subtable.
 *
 * ---------------------------------------------------------------------- */

/*
 * Calculate the extremes of a subtable.
 */
static void Dw_table_sub_get_extremes (DwTableSub *sub)
{
   gint i, row, n, start_col, start_row, start_n, num_cols, colspan;
   DwWidget *child, *widget;
   DwExtremes child_extremes, sum_extremes;
   gint32 col_width;
   gint32 border_spacing = DW_WIDGET(sub->table)->style->border_spacing;
   gfloat percentage;
   gfloat sum_percentage;
   gint use_percentage_map[3][3] = {
      { USE_PERCENTAGE_NO, USE_PERCENTAGE_SOME, -1 },
      { USE_PERCENTAGE_SOME, USE_PERCENTAGE_SOME, USE_PERCENTAGE_SOME },
      { USE_PERCENTAGE_SOME, USE_PERCENTAGE_SOME, USE_PERCENTAGE_ALL } };

   num_cols = sub->end_col - sub->start_col;
   sub->use_percentage = USE_PERCENTAGE_NO;
   sub->fixed_width = FALSE;
   sub->percentage = 0;

   DEBUG_MSG (DEBUG_EXTR_LEVEL + 2, "   subtable from %d to %d\n",
              sub->start_col, sub->end_col);

   /* 1. cells spanning the whole width */
   sub->span_extremes.min_width = 0;
   sub->span_extremes.max_width = 0;

   for (row = 0; row < sub->table->num_rows; row++) {
      if (!a_Bitvec_get_bit (sub->removed_rows, row) &&
          a_Bitvec_get_bit (sub->spanning_rows, row)) {
         n = sub->start_col + row * sub->table->num_cols;
         if (sub->table->children[n]) {
            if (sub->table->children[n]->type == DW_TABLE_CELL) {
               start_row = row;
               start_n = n;
            } else {
               start_col = sub->table->children[n]->data.span_space.start_col;
               start_row = sub->table->children[n]->data.span_space.start_row;
               start_n = start_col + sub->table->num_cols * start_row;
            }

            if (row == start_row) {
               child = sub->table->children[start_n]->data.cell.widget;
               a_Dw_widget_get_extremes (child, &child_extremes);
               colspan = sub->table->children[start_n]->data.cell.colspan;

               /* Adjust width argument of the cell */
               if (DW_STYLE_IS_LENGTH (child->style->width)) {
                  col_width = DW_STYLE_GET_LENGTH (child->style->width,
                                                   child->style->font);
                  if (col_width < child_extremes.min_width)
                     col_width = child_extremes.min_width;
                  child_extremes.min_width = col_width;
                  child_extremes.max_width = col_width;
                  sub->fixed_width = TRUE;
                  DEBUG_MSG (DEBUG_EXTR_LEVEL,
                             "         following adjusted by %d pixels:\n",
                             col_width);
               } else if (DW_STYLE_IS_PERCENTAGE (child->style->width)) {
                  percentage = DW_STYLE_GET_PERCENTAGE (child->style->width);
                  sub->percentage = MAX (sub->percentage, percentage);
                  sub->use_percentage = USE_PERCENTAGE_ALL;
                  DEBUG_MSG (DEBUG_EXTR_LEVEL,
                             "         following adjusted by %g %%\n",
                             100 * sub->percentage);
               }

               if (num_cols != colspan) {
                  child_extremes.min_width =
                     child_extremes.min_width * num_cols / colspan;
                  child_extremes.max_width =
                     child_extremes.max_width * num_cols / colspan;
               }

               DEBUG_MSG (DEBUG_EXTR_LEVEL,
                          "         row %d: cell extremes: %d / %d "
                          "[was multiplied by %d / %d]\n",
                          row, child_extremes.min_width,
                          child_extremes.max_width,
                          num_cols, colspan);

               if (child_extremes.min_width > sub->span_extremes.min_width)
                  sub->span_extremes.min_width = child_extremes.min_width;
               if (child_extremes.max_width > sub->span_extremes.max_width)
                  sub->span_extremes.max_width = child_extremes.max_width;
            } else {
               DEBUG_MSG (DEBUG_EXTR_LEVEL,
                          "         row %d: omitted\n", row);
            }
         }
      }
   }

   DEBUG_MSG (DEBUG_EXTR_LEVEL + 1, "      spanning subs: %d / %d\n",
              sub->span_extremes.min_width, sub->span_extremes.max_width);

   if (sub->table->sub == sub) {
      /* Adjust width argument of the table (fixed, percentages
         later in the code). */
      widget = DW_WIDGET(sub->table);
      if (DW_STYLE_IS_LENGTH (widget->style->width)) {
         col_width = DW_STYLE_GET_LENGTH (widget->style->width,
                                          widget->style->font);
         if (col_width < sub->span_extremes.min_width)
            col_width = sub->span_extremes.min_width;
         sub->span_extremes.min_width = col_width;
         sub->span_extremes.max_width = col_width;
      }
   }

   /* 2. subtables */
   sum_extremes.min_width = (sub->num_subs - 1) * border_spacing;
   sum_extremes.max_width = (sub->num_subs - 1) * border_spacing;
   sum_percentage = 0;

   if (num_cols > 1)
      for (i = 0; i < sub->num_subs; i++) {
         Dw_table_sub_get_extremes (&sub->subs[i]);
         sum_extremes.min_width += sub->subs[i].total_extremes.min_width;
         sum_extremes.max_width += sub->subs[i].total_extremes.max_width;
         sum_percentage += sub->subs[i].percentage;
         if ((sub->use_percentage =
                use_percentage_map
                  [sub->use_percentage][sub->subs[i].use_percentage]) == -1)
            sub->use_percentage =
               (i == 0) ? USE_PERCENTAGE_ALL : USE_PERCENTAGE_SOME;
      }

   DEBUG_MSG (DEBUG_EXTR_LEVEL + 1, "      sum of subsubs: %d / %d\n",
              sum_extremes.min_width, sum_extremes.max_width);

   if (sub->fixed_width) {
      sum_extremes.max_width = sum_extremes.min_width;
      sub->span_extremes.max_width = sub->span_extremes.min_width;
   }

   sub->total_extremes.min_width = MAX (sum_extremes.min_width,
                                        sub->span_extremes.min_width);
   sub->total_extremes.max_width = MAX (sum_extremes.max_width,
                                        sub->span_extremes.max_width);
   sub->percentage = MAX (sub->percentage, sum_percentage);

   DEBUG_MSG (DEBUG_EXTR_LEVEL + 1, "      final: %d / %d\n",
              sub->total_extremes.min_width, sub->total_extremes.max_width);
}

/*
 * Corrent minima or maxima, used by Dw_table_sub_calc_col_widths.
 */
#define EXTR_VALUE(e) (max ? (e).max_width : (e).min_width)

static void Dw_table_sub_adjust_col_widths (DwTableSub *sub,
                                            gboolean max,
                                            gint32 width,
                                            DwExtremes *sub_extremes,
                                            gint num_nf_subs,
                                            gint num_nf_cols,
                                            gint32 sum_nf_sub_widths)
{
   gint i, cols_per_sub, rest_n, rest_w;
   gint32 sum_sub_widths, sum_orig_sub_widths, sub_extr_width, delta;

   sum_sub_widths = 0;
   sum_orig_sub_widths = 0;

   for (i = 0; i < sub->num_subs; i++) {
      sum_orig_sub_widths += EXTR_VALUE (sub->subs[i].total_extremes);
      sum_sub_widths += EXTR_VALUE (sub_extremes[i]);
   }

   DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2,
              "       Comparing %s: total: %d, sum: %d\n",
              (max ? "max" : "min"),  width, sum_sub_widths);

   if (num_nf_subs == -1) {
     /* If all subs are fixed, unfix them all. */
     sum_nf_sub_widths = sum_sub_widths;
     num_nf_cols = sub->end_col - sub->start_col;
   }

   if (sum_sub_widths < width) {
      if (sum_nf_sub_widths == 0) {
         /* All non-fixed columns zero: Apportion the rest to the
          * non-fixed columns, according to the columns per subtable.
          */
         rest_w = width - sum_sub_widths;
         rest_n = num_nf_cols;

         for (i = 0; i < sub->num_subs; i++) {
            if (num_nf_subs == -1 ||
                !(sub->subs[i].fixed_width ||
                  sub->subs[i].use_percentage == USE_PERCENTAGE_ALL)) {
              cols_per_sub = (sub->subs[i].end_col - sub->subs[i].start_col);
              sub_extr_width = rest_w * cols_per_sub / rest_n;
              rest_w -= sub_extr_width;
              rest_n -= cols_per_sub;
              EXTR_VALUE (sub_extremes[i]) = sub_extr_width;
            }
         }
      } else {
         /* Apportion the rest, according to current values. */
         rest_w = width - sum_sub_widths;
         rest_n = sum_nf_sub_widths;

         for (i = 0; i < sub->num_subs; i++)
            if (num_nf_subs == -1 ||
                !(sub->subs[i].fixed_width ||
                  sub->subs[i].use_percentage == USE_PERCENTAGE_ALL)) {
               DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2,
                          "          increasing sub %d: %d -> ",
                          i, EXTR_VALUE (sub_extremes[i]));
               if (EXTR_VALUE (sub_extremes[i]) > 0) {
                  delta = rest_w * EXTR_VALUE (sub_extremes[i]) / rest_n;
                  rest_w -= delta;
                  rest_n -= EXTR_VALUE (sub_extremes[i]);
                  EXTR_VALUE (sub_extremes[i]) += delta;
               }
               DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2, "%d\n",
                          EXTR_VALUE (sub_extremes[i]));
            }
      }
   }
}


/*
 * Calculate the column widths of a subtable, fill table->width.
 */
static void Dw_table_sub_calc_col_widths (DwTableSub *sub,
                                          gint32 width,
                                          gint32 total_width)
{
   enum { AT_MIN, AT_MAX, AT_NORM }; /* for sub_status */
   gint *sub_status;
   gint32 avail_width, sub_width, width_norm_cols;
   gint32 col_width, sum_sub_min_widths, sum_orig_sub_min_widths;
   gint i, num_cols, cols_per_sub, num_norm_cols, num_nf_subs, num_nf_cols;
   gint rest_w, rest_n, delta;
   gboolean success;
   gint32 border_spacing, diff;
   DwExtremes extremes, *sub_extremes, sum_nf_sub_extremes;

   num_cols = sub->end_col - sub->start_col;

   if (num_cols == 1) {
      /* single column */
      DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2, "   single column at %d: width = %d\n",
                 sub->start_col, width);
      sub->table->col_width[sub->start_col] = width;
   } else {
      /* complex subtable
       *
       * The comments "STEP <n>" refer to the documentation in
       * ../doc/DwTable.txt, "Calculating column widths". Read this
       * parallel.
       */
      DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2,
                 "   --> complex subtable from %d to %d, width = %d "
                 "(total = %d)\n",
                 sub->start_col, sub->end_col, width, total_width);

      border_spacing = DW_WIDGET(sub->table)->style->border_spacing;
      diff =  (sub->num_subs - 1) * border_spacing;
      avail_width = width - diff;

      extremes = sub->total_extremes;
      if (width > extremes.max_width)
         extremes.max_width = width;

      sub_extremes = g_new (DwExtremes, sub->num_subs);
      sum_sub_min_widths = 0;

      /* ---- STEP 1: Calculate relative widths. ---- */
      for (i = 0; i < sub->num_subs; i++) {
         sub_extremes[i] = sub->subs[i].total_extremes;

         if (sub->subs[i].use_percentage != USE_PERCENTAGE_NO) {
            DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2,
                       "          sub %d [%s] has width of %g%% "
                       "-> adjusting: %d/%d -> ",
                       i, sub->subs[i].fixed_width ? "fixed" : "variable",
                       100 * sub->subs[i].percentage,
                       sub_extremes[i].min_width, sub_extremes[i].max_width);
            col_width = sub->subs[i].percentage * total_width;
            if (col_width < sub_extremes[i].min_width)
               col_width = sub_extremes[i].min_width;
            sub_extremes[i].min_width = col_width;
            if (sub->subs[i].use_percentage == USE_PERCENTAGE_ALL)
               sub_extremes[i].max_width = col_width;

            DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2, "%d/%d\n",
                       sub_extremes[i].min_width, sub_extremes[i].max_width);
         } else {
            DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2,
                       "          sub %d [%s]: %d/%d\n", i,
                       sub->subs[i].fixed_width ? "fixed" : "variable",
                       sub_extremes[i].min_width, sub_extremes[i].max_width);
         }

         sum_sub_min_widths += sub_extremes[i].min_width;
      }

      /* ---- STEP 2: Eventually, decrease them again.  ---- */
      if (sum_sub_min_widths > width) {
         sum_orig_sub_min_widths = 0;
         for (i = 0; i < sub->num_subs; i++)
            sum_orig_sub_min_widths += sub->subs[i].total_extremes.min_width;

         rest_w = sum_sub_min_widths - (width -diff);
         rest_n = sum_sub_min_widths - sum_orig_sub_min_widths;
         for (i = 0; i < sub->num_subs; i++) {
            DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2,
                       "          decreasing sub %d: %d -> ",
                       i, sub_extremes[i].min_width);
            if (sub_extremes[i].min_width !=
                sub->subs[i].total_extremes.min_width) {
               delta =
                  rest_w * (sub_extremes[i].min_width
                            - sub->subs[i].total_extremes.min_width) / rest_n;
               rest_w -= delta;
               rest_n -= (sub_extremes[i].min_width
                          - sub->subs[i].total_extremes.min_width);
            } else
               delta = 0;

            sub_width = sub_extremes[i].min_width - delta;
            Dw_table_sub_calc_col_widths (&sub->subs[i], sub_width,
                                          total_width);
            DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2, "%d\n", sub_width);
         }
      } else {
         /* ---- STEP 3: Eventually, increase the sub-subtable extremes ---- */
         num_nf_subs = 0;
         num_nf_cols = 0;
         sum_nf_sub_extremes.min_width = 0;
         sum_nf_sub_extremes.max_width = 0;

         for (i = 0; i < sub->num_subs; i++)
            if (!(sub->subs[i].fixed_width ||
                  sub->subs[i].use_percentage == USE_PERCENTAGE_ALL)) {
               num_nf_cols += (sub->subs[i].end_col - sub->subs[i].start_col);
               num_nf_subs++;
               sum_nf_sub_extremes.min_width += sub_extremes[i].min_width;
               sum_nf_sub_extremes.max_width += sub_extremes[i].max_width;
            }

         /* If all subtables are fixed, unfix them all. */
         if (num_nf_subs == 0) {
            num_nf_subs = -1;
            num_nf_cols = num_cols;
         }

         Dw_table_sub_adjust_col_widths
            (sub, FALSE, extremes.min_width, sub_extremes, num_nf_subs,
             num_nf_cols, sum_nf_sub_extremes.min_width);
         Dw_table_sub_adjust_col_widths
            (sub, TRUE, extremes.max_width, sub_extremes, num_nf_subs,
             num_nf_cols, sum_nf_sub_extremes.max_width);

         /* ---- STEP 4: Finally, calculate the widths. ---- */
         sub_status = g_new (gint, sub->num_subs);

         /* First, assume that all columns have the same width. */
         for (i = 0; i < sub->num_subs; i++)
            sub_status[i] = AT_NORM;

         do {
            DEBUG_MSG (DEBUG_WIDTH_LEVEL + 3, "       columns: ");

            /* Calculate the normal width, and the number of columns at this
               width. */
            width_norm_cols = avail_width;
            num_norm_cols = num_cols;
            for (i = 0; i < sub->num_subs; i++) {
               if (sub_status[i] != AT_NORM) {
                  num_norm_cols -=
                     (sub->subs[i].end_col - sub->subs[i].start_col);
                  if (sub_status[i] == AT_MIN)
                     width_norm_cols -= sub_extremes[i].min_width;
                  else
                     width_norm_cols -= sub_extremes[i].max_width;
               }

               DEBUG_MSG (DEBUG_WIDTH_LEVEL + 3, "%c", "ian"[sub_status[i]]);
            }

            DEBUG_MSG (DEBUG_WIDTH_LEVEL + 3, "\n");
            DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2, "       norm: %d, width = %d\n",
                       num_norm_cols, width_norm_cols);

            /*
             * Iteratively, test for minimum/maximum, and correct the
             * status. As soon as one test fails, the status is
             * changed, and the iteration starts again from the
             * beginning.
             */
            success = TRUE;
            for (i = 0; success && i < sub->num_subs; i++) {
               cols_per_sub = (sub->subs[i].end_col - sub->subs[i].start_col);

               switch (sub_status[i]) {
               case AT_NORM:
                  /* Columns at normal width must between min and max. */
                  if (width_norm_cols * cols_per_sub
                      < num_norm_cols * sub_extremes[i].min_width) {
                     DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2,
                                "       sub %d at min\n", i);
                     sub_status[i] = AT_MIN;
                     success = FALSE;
                  } else if (width_norm_cols * cols_per_sub
                             > num_norm_cols * sub_extremes[i].max_width) {
                     DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2,
                                "       sub %d at max\n", i);
                     sub_status[i] = AT_MAX;
                     success = FALSE;
                  }
                  break;

               case AT_MIN:
                  /* If the column is at min, the the normal width (which has
                     been changed), must tested against the min. */
                  if (width_norm_cols * cols_per_sub
                      > num_norm_cols * sub_extremes[i].min_width) {
                     DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2,
                                "       sub %d at norm\n", i);
                     sub_status[i] = AT_NORM;
                     success = FALSE;
                  }
                  break;

               case AT_MAX:
                  /* If the column is at max, the the normal width (which has
                     been changed), must tested against the max. */
                  if (width_norm_cols * cols_per_sub
                      < num_norm_cols * sub_extremes[i].max_width) {
                     DEBUG_MSG (DEBUG_WIDTH_LEVEL + 2,
                                "       sub %d at norm\n", i);
                     sub_status[i] = AT_NORM;
                     success = FALSE;
                  }
                  break;
               }
            }
         } while (!success);

         /* ---- STEP 5: Apply calculate the widths. ---- */
         rest_n= num_norm_cols;
         rest_w = width_norm_cols;
         for (i = 0; i < sub->num_subs; i++) {
            if (sub_status[i] == AT_MIN)
               sub_width = sub_extremes[i].min_width;
            else if (sub_status[i] == AT_MAX)
               sub_width = sub_extremes[i].max_width;
            else {
               cols_per_sub = (sub->subs[i].end_col - sub->subs[i].start_col);
               sub_width = rest_w * cols_per_sub / rest_n;
               rest_w -= sub_width;
               rest_n-= cols_per_sub;
            }

            Dw_table_sub_calc_col_widths (&sub->subs[i], sub_width,
                                          total_width);
         }

         g_free (sub_status);
      }

      g_free (sub_extremes);
   }
}
