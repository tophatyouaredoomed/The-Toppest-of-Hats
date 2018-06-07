#ifndef __DW_STYLE_H__
#define __DW_STYLE_H__

#include <gdk/gdktypes.h>

#define TEXT_SUB 0
#define TEXT_SUP 1

#define DW_STYLE_ALIGN_LEFT   1
#define DW_STYLE_ALIGN_RIGHT  2

typedef enum {
   DW_STYLE_BORDER_NONE,
   DW_STYLE_BORDER_HIDDEN,
   DW_STYLE_BORDER_DOTTED,
   DW_STYLE_BORDER_DASHED,
   DW_STYLE_BORDER_SOLID,
   DW_STYLE_BORDER_DOUBLE,
   DW_STYLE_BORDER_GROOVE,
   DW_STYLE_BORDER_RIDGE,
   DW_STYLE_BORDER_INSET,
   DW_STYLE_BORDER_OUTSET
} DwStyleBorderStyle;

typedef enum {
   DW_STYLE_TEXT_ALIGN_LEFT,
   DW_STYLE_TEXT_ALIGN_RIGHT,
   DW_STYLE_TEXT_ALIGN_CENTER,
   DW_STYLE_TEXT_ALIGN_JUSTIFY,
   DW_STYLE_TEXT_ALIGN_STRING,
} DwStyleTextAlignType;

typedef struct _DwStyle            DwStyle;
typedef struct _DwStyleFont        DwStyleFont;
typedef struct _DwStyleColor       DwStyleColor;
typedef struct _DwStyleShadedColor DwStyleShadedColor;
typedef struct _DwStyleBox         DwStyleBox;

typedef gint DwStyleLength;

struct _DwStyleBox
{
   /* in future also percentages */
   gint32 top, right, bottom, left;
};

struct _DwStyle
{
   gint ref_count;

   DwStyleFont *font;
   gint link; /* will perhaps move */
   gint uline;
   gint strike;
   gint SubSup;
   gboolean nowrap;
   /*gint underline;*/
   DwStyleColor *color, *background_color;

   DwStyleTextAlignType text_align;
   gchar text_align_char; /* In future, strings will be supported. */

   gint32 border_spacing;
   DwStyleLength width, height;

   DwStyleBox margin, border_width, padding;
   struct { DwStyleShadedColor *top, *right, *bottom, *left; } border_color;
   struct { DwStyleBorderStyle top, right, bottom, left; } border_style;
};

struct _DwStyleFont
{
   gint ref_count;

   char *name;
   gchar size;
   gchar bold;
   gchar italic;

#ifdef USE_TYPE1
   gint t1fontid;
#else
   GdkFont *font;
#endif
   gint space_width;
   gint x_height;
};


struct _DwStyleColor
{
   gint ref_count;
   gint color_val;
   GdkColor color;
   GdkGC *gc;
};


struct _DwStyleShadedColor
{
   gint ref_count;
   gint color_val;
   GdkColor color, color_dark, color_light;
   GdkGC *gc, *gc_dark, *gc_light;
};


void a_Dw_style_init    (void);
void a_Dw_style_freeall (void);

void                a_Dw_style_init_values        (DwStyle *style_attrs,
                                                   GdkWindow *window);

DwStyle*            a_Dw_style_new                (DwStyle *style_attrs,
                                                   GdkWindow *window);
DwStyleFont*        a_Dw_style_font_new           (DwStyleFont *font_attrs);
DwStyleFont*        a_Dw_style_font_new_from_list (DwStyleFont *font_attrs);
DwStyleColor*       a_Dw_style_color_new          (gint color_val,
                                                   GdkWindow *window);
DwStyleShadedColor* a_Dw_style_shaded_color_new   (gint color_val,
                                                   GdkWindow *window);

#define a_Dw_style_ref(style)   ((style)->ref_count++)
#define a_Dw_style_unref(style) if (--((style)->ref_count) == 0) \
                                   Dw_style_remove (style)

/* Lengths */
#define DW_STYLE_CREATE_LENGTH(n)       (((n) << 2) | 1)
#define DW_STYLE_CREATE_EX_LENGTH(n)    ((DW_STYLE_FLOAT_TO_REAL (n) << 3) | 3)
#define DW_STYLE_CREATE_EM_LENGTH(n)    ((DW_STYLE_FLOAT_TO_REAL (n) << 3) | 7)
#define DW_STYLE_CREATE_PERCENTAGE(n)   ((DW_STYLE_FLOAT_TO_REAL (n) << 3) | 2)
#define DW_STYLE_CREATE_RELATIVE(n)     ((DW_STYLE_FLOAT_TO_REAL (n) << 3) | 6)
#define DW_STYLE_UNDEF_LENGTH           0

#define DW_STYLE_IS_LENGTH(l)           ((l) & 1)
#define DW_STYLE_IS_PERCENTAGE(l)       (((l) & 7) == 2)
#define DW_STYLE_IS_RELATIVE(l)         (((l) & 7) == 6)

#define DW_STYLE_GET_LENGTH(l, f)       ((l) & 2 ? \
                                         /* relative to font */ \
                                         (DW_STYLE_REAL_TO_FLOAT ((l) >> 3) * \
                                          ((l) & 4 ? \
                                           (f)->size : (f)->x_height) ) : \
                                         /* pixels */ \
                                         (l) >> 2 )
#define DW_STYLE_GET_PERCENTAGE(l)      DW_STYLE_REAL_TO_FLOAT ((l) >> 3)
#define DW_STYLE_GET_RELATIVE(l)        DW_STYLE_REAL_TO_FLOAT ((l) >> 3)

/* The next two macros are private. */
#define DW_STYLE_REAL_TO_FLOAT(v)       ((gfloat)(v) / 0x10000)
#define DW_STYLE_FLOAT_TO_REAL(v)       ((gint)((v) * 0x10000))


/* Don't use this function directly! */
void Dw_style_remove (DwStyle *style);

#define a_Dw_style_box_set_val(box, val) \
   ((box)->top = (box)->right = (box)->bottom = (box)->left = (val))
#define a_Dw_style_box_set_border_color(style, val) \
   ((style)->border_color.top = (style)->border_color.right = \
    (style)->border_color.bottom = (style)->border_color.left = (val))
#define a_Dw_style_box_set_border_style(style, val) \
   ((style)->border_style.top = (style)->border_style.right = \
    (style)->border_style.bottom = (style)->border_style.left = (val))

/* For use of widgets */
#define DW_INFINITY 1000000 /* random */

void Dw_style_draw_border     (GdkWindow *window,
                               GdkRectangle *area,
                               gint32 vx,
                               gint32 vy,
                               gint32 x,
                               gint32 y,
                               gint32 width,
                               gint32 height,
                               DwStyle *style);
void Dw_style_draw_background (GdkWindow *window,
                               GdkRectangle *area,
                               gint32 vx,
                               gint32 vy,
                               gint32 x,
                               gint32 y,
                               gint32 width,
                               gint32 height,
                               DwStyle *style);

#define Dw_style_box_offset_x(style)    ((style)->margin.left + \
                                         (style)->border_width.left + \
                                         (style)->padding.left)
#define Dw_style_box_rest_width(style)  ((style)->margin.right + \
                                         (style)->border_width.right + \
                                         (style)->padding.right)
#define Dw_style_box_diff_width(style)  (Dw_style_box_offset_x(style) + \
                                         Dw_style_box_rest_width(style))

#define Dw_style_box_offset_y(style)    ((style)->margin.top + \
                                         (style)->border_width.top + \
                                         (style)->padding.top)
#define Dw_style_box_rest_height(style) ((style)->margin.bottom + \
                                         (style)->border_width.bottom + \
                                         (style)->padding.bottom)
#define Dw_style_box_diff_height(style) (Dw_style_box_offset_y(style) + \
                                         Dw_style_box_rest_height(style))

#endif /* __DW_STYLE_H__ */
