/*
 * File: html.c
 *
 * Copyright (C) 1997 Raph Levien <raph@acm.org>
 * Copyright (C) 1999 James McCollough <jamesm@gtwn.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Dillo HTML parsing routines
 */

#define USE_TABLES

#include <ctype.h>      /* for isspace and tolower */
#include <string.h>     /* for memcpy and memmove */
#include <stdlib.h>
#include <stdio.h>      /* for sprintf */
#include <math.h>      /* for rint */

#include <gtk/gtk.h>

#include "list.h"
#include "colors.h"
#include "dillo.h"
#include "history.h"
#include "nav.h"
#include "menu.h"
#include "commands.h"
#include "dw.h"         /* for Dw_cursor_hand */

#include "dw_widget.h"
#include "dw_page.h"
#include "dw_bullet.h"
#include "dw_hruler.h"
#include "dw_embed_gtk.h"
#include "dw_table.h"
#include "dw_list_item.h"
#include "IO/IO.h"
#include "IO/Url.h"
#include "interface.h"
#include "progressbar.h"
#include "prefs.h"
#include "misc.h"

//#define DEBUG_LEVEL 3
#include "debug.h"

typedef void (*TagFunct) (DilloHtml *Html, char *Tag, gint Tagsize);


/*
 * Forward declarations
 */
static const char *Html_get_attr(DilloHtml *html,
                                 const char *tag,
                                 gint tagsize,
                                 const char *attrname);
static const char *Html_get_attr2(DilloHtml *html,
                                  const char *tag,
                                  gint tagsize,
                                  const char *attrname,
                                  DilloHtmlTagParsingFlags flags);
static void Html_add_widget(DilloHtml *html, DwWidget *widget,
                            char *width_str, char *height_str,
                            DwStyle *style_attrs);
static void Html_write(DilloHtml *html, char *Buf, gint BufSize, gint Eof);
static void Html_close(DilloHtml *html, gint ClientKey);
static void Html_callback(int Op, CacheClient_t *Client);
static DilloHtml *Html_new(BrowserWindow *bw, const DilloUrl *url);

/*
 * Local Data
 */
static const char
   *roman_I0[] ={"I","II","III","IV","V","VI","VII","VIII","IX"},
   *roman_I1[] ={"X","XX","XXX","XL","L","LX","LXX","LXXX","XC"},
   *roman_I2[] ={"C","CC","CCC","CD","D","DC","DCC","DCCC","CM"},
   *roman_I3[] ={"M","MM","MMM","MMMM"};

/* The following array of font sizes has to be _strictly_ crescent */
static const gint FontSizes[] = {8, 10, 12, 14, 18, 24};
static const gint FontSizesNum = 6;
static const gint FontSizesBase = 2;


/*
 * Set callback function and callback data for "html/text" MIME type.
 */
DwWidget *a_Html_text(const char *Type, void *P, CA_Callback_t *Call,
                      void **Data)
{
   DilloWeb *web = P;
   DilloHtml *html = Html_new(web->bw, web->url);

   *Data = (void *) html;
   *Call = (CA_Callback_t) Html_callback;

   return html->dw;
}


/*
 * We'll make the linkblock first to get it out of the way.
 */
static DilloHtmlLB *Html_lb_new(BrowserWindow *bw, const DilloUrl *url)
{
   DilloHtmlLB *html_lb = g_new(DilloHtmlLB, 1);

   html_lb->bw = bw;
   html_lb->base_url = a_Url_dup(url);
   html_lb->num_forms_max = 1;
   html_lb->num_forms = 0;
   html_lb->forms = NULL;

   html_lb->num_links_max = 1;
   html_lb->num_links = 0;
   html_lb->links = NULL;
   a_Dw_image_map_list_init(&html_lb->maps);
   html_lb->map_open = FALSE;

   html_lb->link_color = prefs.link_color;
   html_lb->visited_color = prefs.visited_color;

   return html_lb;
}

/*
 * Free the memory used by the linkblock
 */
static void Html_lb_free(void *lb)
{
   gint i, j, k;
   DilloHtmlForm *form;
   DilloHtmlLB *html_lb = lb;

   DEBUG_MSG(3, "Html_lb_free\n");

   a_Url_free(html_lb->base_url);

   for (i = 0; i < html_lb->num_forms; i++) {
      form = &html_lb->forms[i];
      a_Url_free(form->action);
      for (j = 0; j < form->num_inputs; j++) {
         g_free(form->inputs[j].name);
         g_free(form->inputs[j].init_str);

         if (form->inputs[j].type == DILLO_HTML_INPUT_SELECT ||
            form->inputs[j].type == DILLO_HTML_INPUT_SEL_LIST) {
            for(k = 0; k < form->inputs[j].select->num_options; k++) {
               g_free(form->inputs[j].select->options[k].value);
            }
            g_free(form->inputs[j].select->options);
            g_free(form->inputs[j].select);
         }
      }
      g_free(form->inputs);
   }
   g_free(html_lb->forms);

   for (i = 0; i < html_lb->num_links; i++)
      if (html_lb->links[i])
         a_Url_free(html_lb->links[i]);
   g_free(html_lb->links);

   a_Dw_image_map_list_free(&html_lb->maps);

   g_free(html_lb);
}


/*
 * Set the URL data for image maps.
 */
static void Html_set_link_coordinates(DilloHtmlLB *lb,
                                      gint link, gint x, gint y)
{
   gchar data[32];

   if (x != -1) {
      sprintf(data, "?%d,%d", x, y);
      a_Url_set_ismap_coords(lb->links[link], data);
   }
}

/*
 * Handle the status function generated by the dw scroller,
 * and show the url in the browser status-bar.
 */
static void Html_handle_status(DwWidget *widget, gint link, gint x, gint y,
                               DilloHtmlLB *lb)
{
   DilloUrl *url;

   url = (link == -1) ? NULL : lb->links[link];
   if (url) {
      Html_set_link_coordinates(lb, link, x, y);
      a_Interface_msg(lb->bw, "%s",
                         URL_ALT(url) ? URL_ALT(url) : URL_STR(url));
      a_Dw_widget_set_cursor (widget, Dw_cursor_hand);
      lb->bw->status_is_link = 1;

   } else {
      if (lb->bw->status_is_link)
         a_Interface_msg(lb->bw, "");
      a_Dw_widget_set_cursor (widget, NULL);
   }
}

/*
 * Popup the link menu ("link_pressed" callback of the page)
 */
static void Html_link_menu(DwWidget *widget, gint link, gint x, gint y,
                           GdkEventButton *event, DilloHtmlLB *lb)
{
   if (event->button == 3) {
      Html_set_link_coordinates(lb, link, x, y);
      a_Menu_popup_set_url(lb->bw, lb->links[link]);
      gtk_menu_popup(GTK_MENU(lb->bw->menu_popup.over_link), NULL, NULL,
                     NULL, NULL, event->button, event->time);
   }
}


/*
 * Activate a link ("link_clicked" callback of the page)
 */
static void Html_link_clicked(DwWidget *widget, gint link, gint x, gint y,
                              GdkEventButton *event, DilloHtmlLB *lb)
{
   DwPage *page;
   DwStyle *old_style, style_attrs;
   gint i, j, changed;

   Html_set_link_coordinates(lb, link, x, y);
   if (event->button == 1)
      a_Nav_push(lb->bw, lb->links[link]);
   else if (event->button == 2) {
      a_Menu_popup_set_url(lb->bw, lb->links[link]);
      a_Commands_open_link_nw_callback(NULL, lb->bw);
   }

   /*
    * This is only a workaround to visualize links opened in a new
    * window. It will definitely change.
    */
   if (DW_IS_PAGE (widget)) {
      page = DW_PAGE (widget);

      for (i = 0; i < page->num_lines; i++) {
         changed = FALSE;

         for (j = page->lines[i].first_word; j < page->lines[i].last_word; j++)
            if (page->words[j].style->link == link) {
               old_style = page->words[j].style;

               style_attrs = *old_style;
               style_attrs.color =
                  a_Dw_style_color_new (lb->visited_color,
                                        widget->viewport->window);
               page->words[j].style =
                  a_Dw_style_new (&style_attrs, widget->viewport->window);
               /* unref'ing it before may crash dillo! */
               a_Dw_style_unref (old_style);
               changed = TRUE;
            }

         if (changed)
            Dw_widget_queue_draw_area (widget, 0, page->lines[i].top,
                                       widget->allocation.width,
                                       page->lines[i].ascent
                                       + page->lines[i].descent);
      }
   }
   /* end workaround */
}

/*
 * Popup the page menu ("button_press_event" callback of the viewport)
 */
static int Html_page_menu(GtkWidget *viewport, GdkEventButton *event,
                          BrowserWindow *bw)
{
   if (event->button == 3) {
      a_Menu_popup_set_url(bw, a_History_get_url(NAV_TOP(bw)));
      gtk_menu_popup(GTK_MENU(bw->menu_popup.over_page), NULL, NULL,
                     NULL, NULL, event->button, event->time);
      return TRUE;
   } else
      return FALSE;
}

/*
 * Connect all signals of a page or an image.
 */
static void Html_connect_signals(DilloHtml *html, GtkObject *widget)
{
   gtk_signal_connect (widget, "link_entered",
                       GTK_SIGNAL_FUNC(Html_handle_status),
                       (gpointer)html->linkblock);
   gtk_signal_connect (widget, "link_pressed", GTK_SIGNAL_FUNC(Html_link_menu),
                       (gpointer)html->linkblock);
   gtk_signal_connect (widget, "link_clicked",
                       GTK_SIGNAL_FUNC(Html_link_clicked),
                       (gpointer)html->linkblock);
}


/*
 * Create a new link in the linkblock, set it as the url's parent
 * and return the index.
 */
static gint Html_set_new_link(DilloHtml *html, DilloUrl **url)
{
   gint nl;

   nl = html->linkblock->num_links;
   a_List_add(html->linkblock->links, nl, sizeof(*html->linkblock->links),
              html->linkblock->num_links_max);
   html->linkblock->links[nl] = (*url) ? *url : NULL;
   return html->linkblock->num_links++;
}


/*
 * Allocate and insert form information into the Html linkblock
 */
static gint Html_form_new(DilloHtmlLB *html_lb,
                          DilloHtmlMethod method,
                          const DilloUrl *action,
                          DilloHtmlEnc enc)
{
   gint nf;

   a_List_add(html_lb->forms, html_lb->num_forms, sizeof(*html_lb->forms),
              html_lb->num_forms_max);

   nf = html_lb->num_forms;
   html_lb->forms[nf].method = method;
   html_lb->forms[nf].action = a_Url_dup(action);
   html_lb->forms[nf].enc = enc;
   html_lb->forms[nf].num_inputs = 0;
   html_lb->forms[nf].num_inputs_max = 4;
   html_lb->forms[nf].inputs = NULL;
   html_lb->forms[nf].HasSubmitButton = FALSE;
   html_lb->num_forms++;

   // g_print("Html_form_new: action=%s nform=%d\n", action, nf);
   return nf;
}


/*
 * Change one toplevel attribute. var should be an identifier. val is
 * only evaluated once, so you can safely use a function call for it.
 */
#define HTML_SET_TOP_ATTR(html, var, val) \
   do { \
      DwStyle style_attrs, *old_style; \
       \
      old_style = (html)->stack[(html)->stack_top].style; \
      style_attrs = *old_style; \
      style_attrs.var = (val); \
      (html)->stack[(html)->stack_top].style = \
         a_Dw_style_new (&style_attrs, (html)->bw->main_window->window); \
      a_Dw_style_unref (old_style); \
   } while (FALSE)

/*
 * Set the font at the top of the stack. BImask specifies which
 * attributes in BI should be changed.
 */
static void Html_set_top_font(DilloHtml *html, gchar *name, gint size,
                              gint BI, gint BImask)
{
   DwStyleFont font_attrs;

   font_attrs = *html->stack[(html)->stack_top].style->font;
   if ( name )
      font_attrs.name = name;
   if ( size )
      font_attrs.size = size;
   if ( BImask & 1 )
      font_attrs.bold   = (BI & 1) ? TRUE : FALSE;
   if ( BImask & 2 )
      font_attrs.italic = (BI & 2) ? TRUE : FALSE;

   HTML_SET_TOP_ATTR (html, font, a_Dw_style_font_new (&font_attrs));
}

/*
 * Evaluates the ALIGN attribute (reft|center|right|justify) and
 * sets the style at the top of the stack.
 */
static void Html_tag_set_align_attr(DilloHtml *html, char *tag, gint tagsize)
{
   const char *align;
   
   if ((align = Html_get_attr(html, tag, tagsize, "align"))) {
      if (strcasecmp (align, "left") == 0)
         HTML_SET_TOP_ATTR (html, text_align, DW_STYLE_TEXT_ALIGN_LEFT);
      else if (strcasecmp (align, "right") == 0)
         HTML_SET_TOP_ATTR (html, text_align, DW_STYLE_TEXT_ALIGN_RIGHT);
      else if (strcasecmp (align, "center") == 0)
         HTML_SET_TOP_ATTR (html, text_align, DW_STYLE_TEXT_ALIGN_CENTER);
      else if (strcasecmp (align, "justify") == 0)
         HTML_SET_TOP_ATTR (html, text_align, DW_STYLE_TEXT_ALIGN_JUSTIFY);
   }
}

/*
 * Add a new DwPage into the current DwPage, for indentation.
 * left and right are the horizontal indentation amounts, space is the
 * vertical space around the block.
 */
static void Html_add_indented_widget(DilloHtml *html, DwWidget *page,
                                     int left, int right, int space)
{
   DwStyle style_attrs, *style;

   style_attrs = *html->stack[html->stack_top].style;

   a_Dw_style_box_set_val(&style_attrs.margin, 0);
   a_Dw_style_box_set_val(&style_attrs.border_width, 0);
   a_Dw_style_box_set_val(&style_attrs.padding, 0);

   /* Activate this for debugging */
#if 0
   a_Dw_style_box_set_val(&style_attrs.border_width, 1);
   a_Dw_style_box_set_border_color
      (&style_attrs,
       a_Dw_style_shaded_color_new(style_attrs.color->color_val,
                                   html->bw->main_window->window));
   a_Dw_style_box_set_border_style(&style_attrs, DW_STYLE_BORDER_DASHED);
#endif

   style_attrs.margin.left = left;
   style_attrs.margin.right = right;
   style = a_Dw_style_new (&style_attrs, html->bw->main_window->window);

   a_Dw_page_add_break (DW_PAGE (html->dw), space, style);
   a_Dw_page_add_widget (DW_PAGE (html->dw), page, style);
   a_Dw_page_add_break (DW_PAGE (html->dw), space, style);
   html->stack[html->stack_top].page = html->dw = page;
   html->stack[html->stack_top].hand_over_break = TRUE;
   a_Dw_style_unref (style);

   /* Handle it when the user clicks on a link */
   Html_connect_signals(html, GTK_OBJECT(page));
}

/*
 * Create and add a new indented DwPage to the current DwPage
 */
static void Html_add_indented(DilloHtml *html, int left, int right, int space)
{
   DwWidget *page = a_Dw_page_new ();
   Html_add_indented_widget (html, page, left, right, space);
}

/*
 * Given a font_size, this will return the correct 'level'.
 * (or the closest, if the exact level isn't found).
 */
static gint Html_fontsize_to_level(gint fontsize)
{
   gint i, level;
   gdouble normalized_size = fontsize / prefs.font_factor,
           approximation   = FontSizes[FontSizesNum-1] + 1;

   for (i = level = 0; i < FontSizesNum; i++)
      if (approximation >= fabs(normalized_size - FontSizes[i])) {
         approximation = fabs(normalized_size - FontSizes[i]);
         level = i;
      } else {
         break;
      }

   return level;
}

/*
 * Given a level of a font, this will return the correct 'size'.
 */
static gint Html_level_to_fontsize(gint level)
{
   level = MAX(0, level);
   level = MIN(FontSizesNum - 1, level);

   return rint(FontSizes[level]*prefs.font_factor);
}

/*
 * Miscelaneous initializations for a DwPage
 */
static void Html_set_dwpage(DilloHtml *html)
{
   char *PageFonts[] = { "helvetica" , "lucida", "courier", "utopia" };

   DwWidget *widget;
   DwPage *page;
   DwStyle style_attrs;
   DwStyleFont font;

   g_return_if_fail (html->dw == NULL);

   widget = a_Dw_page_new ();
   page = DW_PAGE (widget);
   html->dw = html->stack[0].page = widget;

   /* Create a dummy font, attribute, and tag for the bottom of the stack. */
   font.name = PageFonts[0]; /* Helvetica */
   font.size = Html_level_to_fontsize(FontSizesBase);
   font.bold = FALSE;
   font.italic = FALSE;

   a_Dw_style_init_values (&style_attrs, html->bw->main_window->window);
   style_attrs.font = a_Dw_style_font_new (&font);
   style_attrs.color = a_Dw_style_color_new (prefs.text_color,
                                             html->bw->main_window->window);
   html->stack[0].style = a_Dw_style_new (&style_attrs,
                                          html->bw->main_window->window);

   html->stack[0].table_cell_style = NULL;

   /* Handle it when the user clicks on a link */
   Html_connect_signals(html, GTK_OBJECT(widget));

   gtk_signal_connect_while_alive (
      GTK_OBJECT(GTK_BIN(html->bw->docwin)->child), "button_press_event",
      GTK_SIGNAL_FUNC(Html_page_menu), (gpointer)html->bw, GTK_OBJECT (page));

   /* Destroy the linkblock when the DwPage is destroyed */
   gtk_signal_connect_object(GTK_OBJECT(page), "destroy",
                             GTK_SIGNAL_FUNC(Html_lb_free),
                             (gpointer)html->linkblock);
}

/*
 * Create and initialize a new DilloHtml structure
 */
static DilloHtml *Html_new(BrowserWindow *bw, const DilloUrl *url)
{
   DilloHtml *html;

   html = g_new(DilloHtml, 1);

   html->Start_Ofs = 0;
   html->dw = NULL;
   html->bw = bw;
   html->linkblock = Html_lb_new(bw, url);

   html->stack_max = 16;
   html->stack_top = 0;
   html->stack = g_new(DilloHtmlState, html->stack_max);
   html->stack[0].tag = g_strdup("dillo");
   html->stack[0].parse_mode = DILLO_HTML_PARSE_MODE_INIT;
   html->stack[0].table_mode = DILLO_HTML_TABLE_MODE_NONE;
   html->stack[0].list_level = 0;
   html->stack[0].list_number = 0;
   html->stack[0].page = NULL;
   html->stack[0].table = NULL;
   html->stack[0].ref_list_item = NULL;
   html->stack[0].current_bg_color = prefs.bg_color;
   html->stack[0].hand_over_break = FALSE;

   html->Stash = g_string_new("");
   html->StashSpace = FALSE;

   html->PrevWasCR = FALSE;
   html->InForm = FALSE;
   html->InSelect = FALSE;
   html->InVisitedLink = FALSE;

   html->InTag = DILLO_HTML_IN_HTML;

   html->attr_data = g_string_sized_new(1024);

   Html_set_dwpage(html);

   return html;
}

/*
 * Initialize the stash buffer
 */
static void Html_stash_init(DilloHtml *html)
{
   html->stack[html->stack_top].parse_mode = DILLO_HTML_PARSE_MODE_STASH;
   html->StashSpace = FALSE;
   g_string_truncate(html->Stash, 0);
}

/* Entities list from the HTML 4.01 DTD */
typedef struct {
   char *entity;
   int isocode;
} Ent_t;

#define NumEnt 252
static const Ent_t Entities[NumEnt] = {
   {"AElig",0306}, {"Aacute",0301}, {"Acirc",0302},  {"Agrave",0300},
   {"Alpha",01621},{"Aring",0305},  {"Atilde",0303}, {"Auml",0304},
   {"Beta",01622}, {"Ccedil",0307}, {"Chi",01647},   {"Dagger",020041},
   {"Delta",01624},{"ETH",0320},    {"Eacute",0311}, {"Ecirc",0312},
   {"Egrave",0310},{"Epsilon",01625},{"Eta",01627},  {"Euml",0313},
   {"Gamma",01623},{"Iacute",0315}, {"Icirc",0316},  {"Igrave",0314},
   {"Iota",01631}, {"Iuml",0317},   {"Kappa",01632}, {"Lambda",01633},
   {"Mu",01634},   {"Ntilde",0321}, {"Nu",01635},    {"OElig",0522},
   {"Oacute",0323},{"Ocirc",0324},  {"Ograve",0322}, {"Omega",01651},
   {"Omicron",01637},{"Oslash",0330},{"Otilde",0325},{"Ouml",0326},
   {"Phi",01646},  {"Pi",01640},    {"Prime",020063},{"Psi",01650},
   {"Rho",01641},  {"Scaron",0540}, {"Sigma",01643}, {"THORN",0336},
   {"Tau",01644},  {"Theta",01630}, {"Uacute",0332}, {"Ucirc",0333},
   {"Ugrave",0331},{"Upsilon",01645},{"Uuml",0334},  {"Xi",01636},
   {"Yacute",0335},{"Yuml",0570},   {"Zeta",01626},  {"aacute",0341},
   {"acirc",0342}, {"acute",0264},  {"aelig",0346},  {"agrave",0340},
   {"alefsym",020465},{"alpha",01661},{"amp",38},    {"and",021047},
   {"ang",021040}, {"aring",0345},  {"asymp",021110},{"atilde",0343},
   {"auml",0344},  {"bdquo",020036},{"beta",01662},  {"brvbar",0246},
   {"bull",020042},{"cap",021051},  {"ccedil",0347}, {"cedil",0270},
   {"cent",0242},  {"chi",01707},   {"circ",01306},  {"clubs",023143},
   {"cong",021105},{"copy",0251},   {"crarr",020665},{"cup",021052},
   {"curren",0244},{"dArr",020723}, {"dagger",020040},{"darr",020623},
   {"deg",0260},   {"delta",01664}, {"diams",023146},{"divide",0367},
   {"eacute",0351},{"ecirc",0352},  {"egrave",0350}, {"empty",021005},
   {"emsp",020003},{"ensp",020002}, {"epsilon",01665},{"equiv",021141},
   {"eta",01667},  {"eth",0360},    {"euml",0353},   {"euro",020254},
   {"exist",021003},{"fnof",0622},  {"forall",021000},{"frac12",0275},
   {"frac14",0274},{"frac34",0276}, {"frasl",020104},{"gamma",01663},
   {"ge",021145},  {"gt",62},       {"hArr",020724}, {"harr",020624},
   {"hearts",023145},{"hellip",020046},{"iacute",0355},{"icirc",0356},
   {"iexcl",0241}, {"igrave",0354}, {"image",020421},{"infin",021036},
   {"int",021053}, {"iota",01671},  {"iquest",0277}, {"isin",021010},
   {"iuml",0357},  {"kappa",01672}, {"lArr",020720}, {"lambda",01673},
   {"lang",021451},{"laquo",0253},  {"larr",020620}, {"lceil",021410},
   {"ldquo",020034},{"le",021144},  {"lfloor",021412},{"lowast",021027},
   {"loz",022712}, {"lrm",020016},  {"lsaquo",020071},{"lsquo",020030},
   {"lt",60},      {"macr",0257},   {"mdash",020024},{"micro",0265},
   {"middot",0267},{"minus",021022},{"mu",01674},    {"nabla",021007},
   {"nbsp",0240},  {"ndash",020023},{"ne",021140},   {"ni",021013},
   {"not",0254},   {"notin",021011},{"nsub",021204}, {"ntilde",0361},
   {"nu",01675},   {"oacute",0363}, {"ocirc",0364},  {"oelig",0523},
   {"ograve",0362},{"oline",020076},{"omega",01711}, {"omicron",01677},
   {"oplus",021225},{"or",021050},  {"ordf",0252},   {"ordm",0272},
   {"oslash",0370},{"otilde",0365}, {"otimes",021227},{"ouml",0366},
   {"para",0266},  {"part",021002}, {"permil",020060},{"perp",021245},
   {"phi",01706},  {"pi",01700},    {"piv",01726},   {"plusmn",0261},
   {"pound",0243}, {"prime",020062},{"prod",021017}, {"prop",021035},
   {"psi",01710},  {"quot",34},     {"rArr",020722}, {"radic",021032},
   {"rang",021452},{"raquo",0273},  {"rarr",020622}, {"rceil",021411},
   {"rdquo",020035},{"real",020434},{"reg",0256},    {"rfloor",021413},
   {"rho",01701},  {"rlm",020017},  {"rsaquo",020072},{"rsquo",020031},
   {"sbquo",020032},{"scaron",0541},{"sdot",021305}, {"sect",0247},
   {"shy",0255},   {"sigma",01703}, {"sigmaf",01702},{"sim",021074},
   {"spades",023140},{"sub",021202},{"sube",021206}, {"sum",021021},
   {"sup",021203}, {"sup1",0271},   {"sup2",0262},   {"sup3",0263},
   {"supe",021207},{"szlig",0337},  {"tau",01704},   {"there4",021064},
   {"theta",01670},{"thetasym",01721},{"thinsp",020011},{"thorn",0376},
   {"tilde",01334},{"times",0327},  {"trade",020442},{"uArr",020721},
   {"uacute",0372},{"uarr",020621}, {"ucirc",0373},  {"ugrave",0371},
   {"uml",0250},   {"upsih",01722}, {"upsilon",01705},{"uuml",0374},
   {"weierp",020430},{"xi",01676},  {"yacute",0375}, {"yen",0245},
   {"yuml",0377},  {"zeta",01666},  {"zwj",020015},  {"zwnj",020014}
};


/*
 * Comparison function for binary search
 */
static int Html_entity_comp(const void *a, const void *b)
{
   return strcmp(((Ent_t *)a)->entity, ((Ent_t *)b)->entity);
}

/*
 * Binary search of 'key' in entity list
 */
static int Html_entity_search(char *key)
{
   Ent_t *res, EntKey;

   EntKey.entity = key;
   res = bsearch(&EntKey, Entities, NumEnt, sizeof(Ent_t), Html_entity_comp);
   if ( res )
     return (res - Entities);
   return -1;
}

/*
 * Given an entity, return the ISO-Latin1 character code.
 * (-1 if not a valid entity)
 */
static gint Html_parse_entity(const gchar *token, gint toksize)
{
   gint base, isocode, i;
   gchar *eoe, *name;

   g_return_val_if_fail (token[0] == '&', -1);

   eoe = (toksize) ? memchr(token, ';', toksize) : strchr(token, ';');
   if (eoe) {
      if (token[1] == '#') {
         /* Numeric token */
         base = (token[2] == 'x' || token[2] == 'X') ? 16 : 10;
         isocode = strtol(token + 2 + (base==16), NULL, base);
         return (isocode > 0 && isocode <= 255) ? isocode : -1;
      } else {
         /* Search for named entity */
         name = g_strndup(token + 1, eoe - token - 1);
         i = Html_entity_search(name);
         g_free(name);
         return (i != -1) ? Entities[i].isocode : -1;
      }
   }
   return -1;
}

/*
 * Convert all the entities in a token to plain ISO character codes. Takes
 * a token and its length, and returns a newly allocated string.
 */
static char *Html_parse_entities(gchar *token, gint toksize)
{
   gchar *new_str;
   gint i, j, isocode;

   if ( memchr(token, '&', toksize) == NULL )
      return g_strndup(token, toksize);

   new_str = g_new(char, toksize + 1);
   for (i = j = 0; i < toksize; i++) {
      if (token[i] == '&' &&
          (isocode = Html_parse_entity(token + i, toksize - i)) != -1) {
         new_str[j++] = isocode;
         while(token[++i] != ';');
      } else {
         new_str[j++] = token[i];
      }
   }
   new_str[j] = '\0';
   return new_str;
}

/*
 * Parse spaces
 *
 * todo: Parse TABS when in MODE_PRE. Note that it's not a matter of expanding
 *       TAB chars in the 'space' string, but to pad them according to the
 *       number of characters in the current line!
 */
static void Html_process_space(DilloHtml *html, char *space, gint spacesize)
{
   gint i;
   DilloHtmlParseMode parse_mode = html->stack[html->stack_top].parse_mode;

   if ( parse_mode == DILLO_HTML_PARSE_MODE_STASH ) {
      html->StashSpace = (html->Stash->len > 0);

   } else if ( parse_mode == DILLO_HTML_PARSE_MODE_SCRIPT ||
               parse_mode == DILLO_HTML_PARSE_MODE_VERBATIM ) {
      char *Pword = g_strndup(space, spacesize);
      g_string_append(html->Stash, Pword);
      g_free(Pword);

   } else if ( parse_mode == DILLO_HTML_PARSE_MODE_PRE ) {
      /* re-scan the string for characters that cause line breaks */
      for (i = 0; i < spacesize; i++) {
         /* Support for "\r", "\n" and "\r\n" line breaks */
         if (space[i] == '\r' || (space[i] == '\n' && !html->PrevWasCR))
            a_Dw_page_add_break(DW_PAGE (html->dw), 0,
                                html->stack[(html)->stack_top].style);

         a_Dw_page_add_text(DW_PAGE (html->dw),
                            g_strndup(space + i, 1),
                            html->stack[html->stack_top].style);

         html->PrevWasCR = (space[i] == '\r');
      }

   } else {
      a_Dw_page_add_space(DW_PAGE (html->dw),
                          html->stack[html->stack_top].style);
      if ( parse_mode == DILLO_HTML_PARSE_MODE_STASH_AND_BODY )
         html->StashSpace = (html->Stash->len > 0);
   }
}

/*
 * Handles putting the word into its proper place
 *  > STASH, SCRIPT or VERBATIM --> html->Stash
 *  > otherwise it goes through a_Dw_page_add_text()
 *
 * Entities are parsed (or not) according to parse_mode.
 */
static void Html_process_word(DilloHtml *html, char *word, gint size)
{
   gchar *Pword;
   DilloHtmlParseMode parse_mode = html->stack[html->stack_top].parse_mode;

   if ( parse_mode == DILLO_HTML_PARSE_MODE_STASH ||
        parse_mode == DILLO_HTML_PARSE_MODE_STASH_AND_BODY ) {
      if ( html->StashSpace ) {
         g_string_append_c(html->Stash, ' ');
         html->StashSpace = FALSE;
      }
      Pword = Html_parse_entities(word, size);
      g_string_append(html->Stash, Pword);
      g_free(Pword);

   } else if ( parse_mode == DILLO_HTML_PARSE_MODE_SCRIPT ) {
      if ( html->StashSpace ) {
         g_string_append_c(html->Stash, ' ');
         html->StashSpace = FALSE;
      }

      /* word goes in untouched, it is the language's responsibilty */
      Pword = g_strndup(word, size);
      g_string_append(html->Stash, Pword);
      g_free(Pword);
   }

   if ( parse_mode == DILLO_HTML_PARSE_MODE_STASH  ||
        parse_mode == DILLO_HTML_PARSE_MODE_SCRIPT ) return;

   if ( parse_mode == DILLO_HTML_PARSE_MODE_VERBATIM ){
      /* Here we add the word Verbatim, not parsed */
      Pword = g_strndup(word, size);
      g_string_append(html->Stash, Pword);
      g_free(Pword);

   } else if ( parse_mode == DILLO_HTML_PARSE_MODE_PRE ) {
      gint start, i;

      /* al this overhead is to catch white-space entities */
      Pword = Html_parse_entities(word, size);
      for (start = i = 0; Pword[i]; start = i)
         if (isspace(Pword[i])) {
            while (Pword[++i] && isspace(Pword[i]));
            Html_process_space(html, Pword + start, i - start);
         } else {
            while (Pword[++i] && !isspace(Pword[i]));
            a_Dw_page_add_text(DW_PAGE (html->dw),
                               g_strndup(Pword + start, i - start),
                               html->stack[html->stack_top].style);
         }
      g_free(Pword);

   } else {
      a_Dw_page_add_text(DW_PAGE (html->dw),
                         Html_parse_entities(word, size),
                         html->stack[html->stack_top].style);
   }
}

/*
 * Does the tag in tagstr (e.g. "p") match the tag in the tag, tagsize
 * structure, with the initial < skipped over (e.g. "P align=center>")
 */
static gboolean Html_match_tag(const char *tagstr, char *tag, gint tagsize)
{
   gint i;

   for (i = 0; i < tagsize && tagstr[i] != '\0'; i++) {
      if (tolower(tagstr[i]) != tolower(tag[i]))
         return FALSE;
   }
   /* The test for '/' is for xml compatibility: "empty/>" will be matched. */
   if (i < tagsize && (isspace(tag[i]) || tag[i] == '>' || tag[i] == '/'))
      return TRUE;
   return FALSE;
}

/*
 * Push the tag (copying attributes from the top of the stack)
 */
static void Html_push_tag(DilloHtml *html, char *tag, gint tagsize)
{
   char *tagstr;
   gint tag_end;
   gint n_items;

   /* Copy the tag itself (no parameters) into tagstr. */
   for (tag_end = 1; tag_end < tagsize && isalnum(tag[tag_end]); tag_end++);
   tagstr = g_strndup(tag + 1, tag_end - 1);

   n_items = html->stack_top + 1;
   a_List_add(html->stack, n_items, sizeof(*html->stack), html->stack_max);
   /* We'll copy the former stack item and just change the tag
    * instead of copying all fields except for tag.  --Jcid */
   html->stack[n_items] = html->stack[n_items - 1];
   html->stack[n_items].tag = tagstr;
   html->stack_top = n_items;
   /* proper memory management, may be unref'd later */
   a_Dw_style_ref (html->stack[html->stack_top].style);
   if (html->stack[html->stack_top].table_cell_style)
      a_Dw_style_ref (html->stack[html->stack_top].table_cell_style);
   html->dw = html->stack[html->stack_top].page;
}

/*
 * This function is called by Html_cleanup_tag and Html_pop_tag, to
 * handle nested DwPage widgets.
 */
static void Html_eventually_pop_dw(DilloHtml *html)
{
   if(html->dw != html->stack[html->stack_top].page) {
      if(html->stack[html->stack_top].hand_over_break)
         a_Dw_page_hand_over_break(DW_PAGE(html->dw),
                                   html->stack[(html)->stack_top].style);
      a_Dw_page_flush(DW_PAGE(html->dw));
      html->dw = html->stack[html->stack_top].page;
   }
}

/*
 * Remove the stack's topmost tag (only if it matches)
 * If it matches, TRUE is returned.
 */
static gboolean Html_cleanup_tag(DilloHtml *html, char *tag){
   if ( html->stack_top &&
        Html_match_tag(html->stack[html->stack_top].tag, tag, strlen(tag)) ) {
      a_Dw_style_unref (html->stack[html->stack_top].style);
      if (html->stack[html->stack_top].table_cell_style)
         a_Dw_style_unref (html->stack[html->stack_top].table_cell_style);
      g_free(html->stack[html->stack_top--].tag);
      Html_eventually_pop_dw(html);
      return TRUE;
   } else
      return FALSE;
}

/*
 * Do a paragraph break and push the tag. This pops unclosed <p> tags
 * off the stack, if there are any.
 */
static void Html_par_push_tag(DilloHtml *html, char *tag, gint tagsize)
{
   Html_cleanup_tag(html, "p>");
   Html_push_tag(html, tag, tagsize);
   a_Dw_page_add_break(DW_PAGE (html->dw), 9,
                       html->stack[(html)->stack_top].style);
}

/*
 * Pop the tag. At the moment, this assumes tags nest!
 */
static void Html_pop_tag(DilloHtml *html, char *tag, gint tagsize)
{
   gint tag_index;

   /* todo: handle non-nesting case more gracefully (search stack
    * downwards for matching tag). */

   for (tag_index = html->stack_top; tag_index > 0; tag_index--) {
      if (Html_match_tag(html->stack[tag_index].tag, tag + 2, tagsize - 2)) {
         while (html->stack_top >= tag_index) {
            a_Dw_style_unref (html->stack[html->stack_top].style);
            if (html->stack[html->stack_top].table_cell_style)
               a_Dw_style_unref(html->stack[html->stack_top].table_cell_style);
            g_free(html->stack[html->stack_top--].tag);
         }
         Html_eventually_pop_dw(html);
         return;
      }
   }
   /* Not found, just ignore. */
}

/*
 * Some parsing routines.
 */

/*
 * Used by Html_parse_length and Html_parse_multi_length_list.
 */
static DwStyleLength Html_parse_length_or_multi_length (const gchar *attr,
                                                        gchar **endptr)
{
   DwStyleLength l;
   double v;
   gchar *end;

   v = strtod (attr, &end);
   switch (*end) {
   case '%':
      end++;
      l = DW_STYLE_CREATE_PERCENTAGE (v / 100);
      break;

   case '*':
      end++;
      l = DW_STYLE_CREATE_RELATIVE (v);
      break;

   default:
      l = DW_STYLE_CREATE_LENGTH ((gint)v);
      break;
   }

   if (endptr)
      *endptr = end;
   return l;
}


/*
 * Returns a length or a percentage, or DW_STYLE_UNDEF_LENGTH in case
 * of an error, or if attr is NULL.
 */
static DwStyleLength Html_parse_length (const gchar *attr)
{
   DwStyleLength l;
   gchar *end;

   l = Html_parse_length_or_multi_length (attr, &end);
   if (DW_STYLE_IS_RELATIVE (l))
      /* not allowed as &Length; */
      return DW_STYLE_UNDEF_LENGTH;
   else {
      /* allow only whitespaces */
      if (*end && !isspace (*end)) {
         DEBUG_HTML_MSG("Garbage after length: %s\n", attr);
         return DW_STYLE_UNDEF_LENGTH;
      }
   }

   return l;
}

/*
 * Returns a vector of lenghts/percentages. The caller has to free the
 * result when it is not longer used.
 */
G_GNUC_UNUSED static DwStyleLength*
Html_parse_multi_length_list (const gchar *attr)
{
   DwStyleLength *l;
   gint n, max_n;
   gchar *end;

   n = 0;
   max_n = 8;
   l = g_new (DwStyleLength, max_n);

   while (TRUE) {
      l[n] = Html_parse_length_or_multi_length (attr, &end);
      n++;
      a_List_add (l, n, sizeof(DwStyleLength) , max_n);

      while (isspace (*end))
         end++;
      if (*end == ',')
         attr = end + 1;
      else
         /* error or end */
         break;
   }

   l[n] = DW_STYLE_UNDEF_LENGTH;
   return l;
}


/*
 * Handle open HEAD element
 * sets DilloHtml->InTag to IN_HTML
 */
static void Html_tag_open_head(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   html->InTag = DILLO_HTML_IN_HEAD;
}

/*
 * Handle close HEAD element
 * sets DilloHtml->InTag to IN_HTML
 */
static void Html_tag_close_head(DilloHtml *html, char *tag, gint tagsize)
{
   Html_pop_tag(html, tag, tagsize);
   html->InTag = DILLO_HTML_IN_HTML;
}

/*
 * Handle open TITLE
 * calls stash init, where the title string will be stored
 */
static void Html_tag_open_title(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_stash_init(html);
}

/*
 * Handle close TITLE
 * set page-title in the browser window and in the history.
 */
static void Html_tag_close_title(DilloHtml *html, char *tag, gint tagsize)
{
   if (html->InTag == DILLO_HTML_IN_HEAD) {
      /* title is only valid inside HEAD */
      a_Interface_set_page_title(html->linkblock->bw, html->Stash->str);
      a_History_set_title(NAV_TOP(html->linkblock->bw), html->Stash->str);
   }
   Html_pop_tag(html, tag, tagsize);
}

/*
 * Handle open SCRIPT
 * initializes stash, where the embedded code will be stored
 * MODE_SCRIPT is used because MODE_STASH catches entities
 */
static void Html_tag_open_script(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_stash_init(html);
   html->stack[html->stack_top].parse_mode = DILLO_HTML_PARSE_MODE_SCRIPT;
}

/*
 * Handle close SCRIPT
 */
static void Html_tag_close_script(DilloHtml *html, char *tag, gint tagsize)
{
#ifdef VERBOSE
   g_print("Html_tag_close_script: script text -> '%s'\n", html->Stash->str);
#endif

   /* eventually the stash will be sent to an interpreter for parsing */
   Html_pop_tag(html, tag, tagsize);
}

/*
 * Handle open STYLE
 * store the contents to the stash where (in the future) the style
 * sheet interpreter can get it.
 */
static void Html_tag_open_style(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_stash_init(html);
   html->stack[html->stack_top].parse_mode = DILLO_HTML_PARSE_MODE_SCRIPT;
}

/*
 * Handle close STYLE
 */
static void Html_tag_close_style(DilloHtml *html, char *tag, gint tagsize)
{
   /* eventually the stash will be sent to an interpreter for parsing */
   Html_pop_tag(html, tag, tagsize);
}

/*
 * <BODY>
 */
static void Html_tag_open_body(DilloHtml *html, char *tag, gint tagsize)
{
   const char *attrbuf;
   DwPage *page;
   DwStyle style_attrs, *style;
   gint32 color;

   page = DW_PAGE (html->dw);

   if (!prefs.force_my_colors) {
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "bgcolor"))) {
         color = a_Color_parse (attrbuf, prefs.bg_color);
         if ( (color == 0xffffff && !prefs.allow_white_bg) ||
              prefs.force_my_colors )
            color = prefs.bg_color;

         style_attrs = *html->dw->style;
         style_attrs.background_color =
            a_Dw_style_color_new (color, html->bw->main_window->window);
         style = a_Dw_style_new (&style_attrs, html->bw->main_window->window);
         a_Dw_widget_set_style (html->dw, style);
         a_Dw_style_unref (style);
         html->stack[html->stack_top].current_bg_color = color;
      }

      if ((attrbuf = Html_get_attr(html, tag, tagsize, "text"))) {
         color = a_Color_parse (attrbuf, prefs.text_color);
         HTML_SET_TOP_ATTR
            (html, color,
             a_Dw_style_color_new (color, html->bw->main_window->window));
      }

      if ((attrbuf = Html_get_attr(html, tag, tagsize, "link")))
         html->linkblock->link_color = a_Color_parse(attrbuf,
                                                     prefs.link_color);

      if ((attrbuf = Html_get_attr(html, tag, tagsize, "vlink")))
         html->linkblock->visited_color =
            a_Color_parse (attrbuf, prefs.visited_color);

      if (prefs.force_visited_color &&
          html->linkblock->link_color == html->linkblock->visited_color) {
         // Get a color that has a "safe distance" from text, link and bg
         html->linkblock->visited_color =
            a_Color_vc(html->stack[html->stack_top].style->color->color_val,
                       html->linkblock->link_color,
                       html->stack[html->stack_top].current_bg_color);
      }
   }

   /* If we are still in HEAD, cleanup after HEAD tag;
    * assume document author forgot to close HEAD */
   if (html->InTag == DILLO_HTML_IN_HEAD)
      Html_cleanup_tag(html, "head>");

   Html_push_tag (html, tag, tagsize);
   html->stack[html->stack_top].parse_mode = DILLO_HTML_PARSE_MODE_BODY;
   html->InTag = DILLO_HTML_IN_BODY;
}

/*
 * <P>
 */
static void Html_tag_open_p(DilloHtml *html, char *tag, gint tagsize)
{
   Html_par_push_tag(html, tag, tagsize);
   Html_tag_set_align_attr (html, tag, tagsize);
}

/*
 * <TABLE>
 */
static void Html_tag_open_table(DilloHtml *html, char *tag, gint tagsize)
{
#ifdef USE_TABLES
   DwWidget *table;
   DwStyle style_attrs, *tstyle, *old_style;
   const char *attrbuf;
   gint32 border = 0, cellspacing = 1, cellpadding = 2, bgcolor;
#endif

   Html_par_push_tag(html, tag, tagsize);

#ifdef USE_TABLES
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "border")))
      border = isdigit(attrbuf[0]) ? strtol (attrbuf, NULL, 10) : 1;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "cellspacing")))
      cellspacing = strtol (attrbuf, NULL, 10);
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "cellpadding")))
      cellpadding = strtol (attrbuf, NULL, 10);

   /* The style for the table */
   style_attrs = *html->stack[html->stack_top].style;

   a_Dw_style_box_set_val (&style_attrs.border_width, border);
   a_Dw_style_box_set_border_color
      (&style_attrs,
       a_Dw_style_shaded_color_new (
          html->stack[html->stack_top].current_bg_color,
          html->bw->main_window->window));
   a_Dw_style_box_set_border_style (&style_attrs, DW_STYLE_BORDER_OUTSET);
   style_attrs.border_spacing = cellspacing;

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "width")))
      style_attrs.width = Html_parse_length (attrbuf);

   if (!prefs.force_my_colors &&
       (attrbuf = Html_get_attr(html, tag, tagsize, "bgcolor"))) {
      bgcolor = a_Color_parse (attrbuf, -1);
      if (bgcolor != -1) {
         if (bgcolor == 0xffffff && !prefs.allow_white_bg)
            bgcolor = prefs.bg_color;
         html->stack[html->stack_top].current_bg_color = bgcolor;
         style_attrs.background_color =
            a_Dw_style_color_new (bgcolor, html->bw->main_window->window);
      }
   }

   tstyle = a_Dw_style_new (&style_attrs, html->bw->main_window->window);

   /* The style for the cells */
   style_attrs = *html->stack[html->stack_top].style;
   a_Dw_style_box_set_val (&style_attrs.border_width, border ? 1 : 0);
   a_Dw_style_box_set_val (&style_attrs.padding, cellpadding);
   a_Dw_style_box_set_border_color (&style_attrs, tstyle->border_color.top);
   a_Dw_style_box_set_border_style (&style_attrs, DW_STYLE_BORDER_INSET);

   old_style = html->stack[html->stack_top].table_cell_style;
   html->stack[html->stack_top].table_cell_style =
      a_Dw_style_new (&style_attrs, html->bw->main_window->window);
   if (old_style)
      a_Dw_style_unref (old_style);

   table = a_Dw_table_new ();
   a_Dw_page_add_widget (DW_PAGE (html->dw), table, tstyle);
   a_Dw_style_unref (tstyle);

   html->stack[html->stack_top].table_mode = DILLO_HTML_TABLE_MODE_TOP;
   html->stack[html->stack_top].table = table;
#endif
}


/*
 * used by <TD> and <TH>
 */
static void Html_tag_open_table_cell(DilloHtml *html, char *tag, gint tagsize,
                                     DwStyleTextAlignType text_align)
{
#ifdef USE_TABLES
   DwWidget *col_page;
   gint colspan = 1, rowspan = 1;
   const char *attrbuf;
   DwStyle style_attrs, *style, *old_style;
   gint32 bgcolor;
   gboolean new_style;

   switch (html->stack[html->stack_top].table_mode) {
   case DILLO_HTML_TABLE_MODE_NONE:
      DEBUG_HTML_MSG("<td> or <th> outside <table>\n");
      return;

   case DILLO_HTML_TABLE_MODE_TOP:
      DEBUG_HTML_MSG("<td> or <th> outside <tr>\n");
      /* a_Dw_table_add_cell takes care that dillo does not crash. */
      /* continues */
   case DILLO_HTML_TABLE_MODE_TR:
   case DILLO_HTML_TABLE_MODE_TD:
      /* todo: check errors? */
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "colspan")))
         colspan = strtol (attrbuf, NULL, 10);
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "rowspan")))
         rowspan = strtol (attrbuf, NULL, 10);

      /* text style */
      old_style = html->stack[html->stack_top].style;
      style_attrs = *old_style;
      style_attrs.text_align = text_align;
      if (Html_get_attr(html, tag, tagsize, "nowrap"))
         style_attrs.nowrap = TRUE;
      else
         style_attrs.nowrap = FALSE;

      html->stack[html->stack_top].style =
         a_Dw_style_new (&style_attrs, html->bw->main_window->window);
      a_Dw_style_unref (old_style);
      Html_tag_set_align_attr (html, tag, tagsize);

      /* cell style */
      style_attrs = *html->stack[html->stack_top].table_cell_style;
      new_style = FALSE;

      if ((attrbuf = Html_get_attr(html, tag, tagsize, "width"))) {
         style_attrs.width = Html_parse_length (attrbuf);
         new_style = TRUE;
      }

      if (!prefs.force_my_colors &&
          (attrbuf = Html_get_attr(html, tag, tagsize, "bgcolor"))) {
         bgcolor = a_Color_parse (attrbuf, -1);
         if (bgcolor != -1) {
            if (bgcolor == 0xffffff && !prefs.allow_white_bg)
               bgcolor = prefs.bg_color;

            new_style = TRUE;
            style_attrs.background_color =
               a_Dw_style_color_new (bgcolor, html->bw->main_window->window);
            html->stack[html->stack_top].current_bg_color = bgcolor;
         }
      }

      col_page = a_Dw_page_new ();
      if (new_style) {
         style = a_Dw_style_new (&style_attrs, html->bw->main_window->window);
         a_Dw_widget_set_style (col_page, style);
         a_Dw_style_unref (style);
      } else
         a_Dw_widget_set_style (col_page,
                                html->stack[html->stack_top].table_cell_style);

      a_Dw_table_add_cell (DW_TABLE (html->stack[html->stack_top].table),
                           col_page, colspan, rowspan);
      html->stack[html->stack_top].page = html->dw = col_page;

      /* Handle it when the user clicks on a link */
      Html_connect_signals(html, GTK_OBJECT(col_page));
      break;

   default:
      /* compiler happiness */
      break;
   }

   html->stack[html->stack_top].table_mode = DILLO_HTML_TABLE_MODE_TD;
#endif
}


/*
 * <TD>
 */
static void Html_tag_open_td(DilloHtml *html, char *tag, gint tagsize)
{
   Html_cleanup_tag(html, "p>");
   Html_cleanup_tag(html, "td>");
   Html_cleanup_tag(html, "th>");

   Html_push_tag(html, tag, tagsize);
   Html_tag_open_table_cell (html, tag, tagsize, DW_STYLE_TEXT_ALIGN_LEFT);
}


/*
 * <TH>
 */
static void Html_tag_open_th(DilloHtml *html, char *tag, gint tagsize)
{
   Html_cleanup_tag(html, "p>");
   Html_cleanup_tag(html, "td>");
   Html_cleanup_tag(html, "th>");

   Html_push_tag(html, tag, tagsize);
   Html_set_top_font(html, NULL, 0, 1, 1);
   Html_tag_open_table_cell (html, tag, tagsize, DW_STYLE_TEXT_ALIGN_CENTER);
}


/*
 * <TR>
 */
static void Html_tag_open_tr(DilloHtml *html, char *tag, gint tagsize)
{
   const char *attrbuf;
   DwStyle style_attrs, *style;
   gint32 bgcolor;

   Html_cleanup_tag(html, "p>");
   Html_cleanup_tag(html, "td>");
   Html_cleanup_tag(html, "th>");
   Html_cleanup_tag(html, "tr>");

   Html_push_tag(html, tag, tagsize);

#ifdef USE_TABLES
   switch (html->stack[html->stack_top].table_mode) {
   case DILLO_HTML_TABLE_MODE_NONE:
      //g_print ("Invalid HTML syntax: <tr> outside <table>\n");
      return;

   case DILLO_HTML_TABLE_MODE_TOP:
   case DILLO_HTML_TABLE_MODE_TR:
   case DILLO_HTML_TABLE_MODE_TD:
      style = NULL;

      if (!prefs.force_my_colors &&
          (attrbuf = Html_get_attr(html, tag, tagsize, "bgcolor"))) {
         bgcolor = a_Color_parse (attrbuf, -1);
         if (bgcolor != -1) {
            if (bgcolor == 0xffffff && !prefs.allow_white_bg)
               bgcolor = prefs.bg_color;

            style_attrs = *html->stack[html->stack_top].style;
            style_attrs.background_color =
               a_Dw_style_color_new (bgcolor, html->bw->main_window->window);
            style =
               a_Dw_style_new (&style_attrs, html->bw->main_window->window);
            html->stack[html->stack_top].current_bg_color = bgcolor;
         }
      }

      a_Dw_table_add_row (DW_TABLE (html->stack[html->stack_top].table),
                          style);
      if (style)
         a_Dw_style_unref (style);
      break;

   default:
      break;
   }

   html->stack[html->stack_top].table_mode = DILLO_HTML_TABLE_MODE_TR;
#else
   a_Dw_page_add_break(DW_PAGE (html->dw), 0,
                       html->stack[(html)->stack_top].style);
#endif
}

/*
 * <FRAME>
 * todo: This is just a temporary fix while real frame support
 *       isn't finished. Imitates lynx/w3m's frames.
 */
static void Html_tag_open_frame (DilloHtml *html, gchar *tag, gint tagsize)
{
   const char *attrbuf;
   gchar *src;
   DilloUrl *url;
   DwPage *page;
   DwStyle style_attrs, *link_style;
   DwWidget *bullet;
   gint dummy;

   page = DW_PAGE(html->dw);

   if ( !(attrbuf = Html_get_attr(html, tag, tagsize, "src")) )
      return;

   if ( !(url = a_Url_new(attrbuf, URL_STR(html->linkblock->base_url), 0, 0)) )
      return;

   src = g_strdup(attrbuf);

   style_attrs = *(html->stack[html->stack_top].style);

   if (a_Cache_url_read(url, &dummy))  /* visited frame   */
      style_attrs.color = a_Dw_style_color_new
         (html->linkblock->visited_color, html->bw->main_window->window);
   else                                /* unvisited frame */
      style_attrs.color = a_Dw_style_color_new
         (html->linkblock->link_color,  html->bw->main_window->window);

   style_attrs.uline = TRUE;
   style_attrs.link = Html_set_new_link(html, &url);
   link_style = a_Dw_style_new (&style_attrs,
                                html->bw->main_window->window);

   a_Dw_page_add_break(page, 5, html->stack[(html)->stack_top].style);

   bullet = a_Dw_bullet_new(DW_BULLET_DISC);
   a_Dw_page_add_widget(page, bullet, html->stack[html->stack_top].style);
   a_Dw_page_add_space(page, html->stack[html->stack_top].style);

   /* If 'name' tag is present use it, if not use 'src' value */
   if ( !(attrbuf = Html_get_attr(html, tag, tagsize, "name")) ) {
      a_Dw_page_add_text(page, g_strdup(src), link_style);
   } else {
      a_Dw_page_add_text(page, g_strdup(attrbuf), link_style);
   }
   a_Dw_page_add_break(page, 5, html->stack[(html)->stack_top].style);

   a_Dw_style_unref(link_style);
   g_free(src);
}

/*
 * <FRAMESET>
 * todo: This is just a temporary fix while real frame support
 *       isn't finished. Imitates lynx/w3m's frames.
 */
static void Html_tag_open_frameset (DilloHtml *html, gchar *tag, gint tagsize)
{
   a_Dw_page_add_text(DW_PAGE(html->dw), g_strdup("--FRAME--"),
                      html->stack[html->stack_top].style);
   Html_add_indented(html, 40, 0, 5);
}

/*
 * <H1> | <H2> | <H3> | <H4> | <H5> | <H6>
 */
static void Html_tag_open_h(DilloHtml *html, char *tag, gint tagsize)
{
   Html_par_push_tag(html, tag, tagsize);

   /* todo: combining these two would be slightly faster */
   Html_set_top_font(html, "helvetica",
                     Html_level_to_fontsize(FontSizesNum - (tag[2] - '0')),
                     1, 3);
   Html_tag_set_align_attr (html, tag, tagsize);

   /* First finalize unclosed H tags (we test if already named anyway) */
   a_Menu_pagemarks_set_text(html->bw, html->Stash->str);
   a_Menu_pagemarks_add(html->bw, DW_PAGE (html->dw),
                        html->stack[html->stack_top].style, (tag[2] - '0'));
   Html_stash_init(html);
   html->stack[html->stack_top].parse_mode =
      DILLO_HTML_PARSE_MODE_STASH_AND_BODY;
}

static void Html_tag_close_h(DilloHtml *html, char *tag, gint tagsize)
{
   a_Menu_pagemarks_set_text(html->bw, html->Stash->str);
   html->stack[html->stack_top].parse_mode = DILLO_HTML_PARSE_MODE_BODY;
   Html_pop_tag(html, tag, tagsize);
   a_Dw_page_add_break(DW_PAGE (html->dw), 9,
                       html->stack[(html)->stack_top].style);
}

/*
 * <BIG> | <SMALL>
 */
static void Html_tag_open_big_small(DilloHtml *html, char *tag, gint tagsize)
{
   gint level;

   Html_push_tag(html, tag, tagsize);

   level =
      Html_fontsize_to_level(html->stack[html->stack_top].style->font->size) +
      ((g_strncasecmp(tag+1, "big", 3)) ? -1 : 1);
   Html_set_top_font(html, NULL, Html_level_to_fontsize(level), 0, 0);
}

/*
 * <BR>
 */
static void Html_tag_open_br(DilloHtml *html, char *tag, gint tagsize)
{
   a_Dw_page_add_break(DW_PAGE (html->dw), 0,
                       html->stack[(html)->stack_top].style);
}

static void Html_tag_open_font(DilloHtml *html, char *tag, gint tagsize)
{
#if 1
   DwStyle style_attrs, *old_style;
   /*DwStyleFont font;*/
   const char *attrbuf;
   gint32 color;

   Html_push_tag(html, tag, tagsize);

   if (!prefs.force_my_colors) {
      old_style = html->stack[html->stack_top].style;
      style_attrs = *old_style;

      if ((attrbuf = Html_get_attr(html, tag, tagsize, "color"))) {
         if (!(prefs.force_visited_color && html->InVisitedLink)) {
            /* use current text color as default */
            color = a_Color_parse(attrbuf, style_attrs.color->color_val);
            style_attrs.color = a_Dw_style_color_new
               (color, html->bw->main_window->window);
         }
      }

#if 0
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "face"))) {
         font = *( style_attrs.font );
         font.name = attrbuf;
         style_attrs.font = a_Dw_style_font_new_from_list (&font);
      }
#endif

      html->stack[html->stack_top].style =
         a_Dw_style_new (&style_attrs, html->bw->main_window->window);
      a_Dw_style_unref (old_style);
   }

#else
   Html_push_tag(html, tag, tagsize);
#endif
}


/*
 * <B>
 */
static void Html_tag_open_b(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_set_top_font(html, NULL, 0, 1, 1);
}

/*
 * <STRONG>
 */
static void Html_tag_open_strong(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_set_top_font(html, NULL, 0, 1, 1);
}

/*
 * <I>
 */
static void Html_tag_open_i(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_set_top_font(html, NULL, 0, 2, 2);
}

/*
 * <EM>
 */
static void Html_tag_open_em(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_set_top_font(html, NULL, 0, 2, 2);
}

/*
 * <CITE>
 */
static void Html_tag_open_cite(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_set_top_font(html, NULL, 0, 2, 2);
}

/*
 * <CENTER>
 */
static void Html_tag_open_center(DilloHtml *html, char *tag, gint tagsize)
{
   a_Dw_page_add_break(DW_PAGE (html->dw), 0,
                       html->stack[(html)->stack_top].style);
   Html_push_tag(html, tag, tagsize);
   HTML_SET_TOP_ATTR(html, text_align, DW_STYLE_TEXT_ALIGN_CENTER);
}

/*
 * <TT>
 */
static void Html_tag_open_tt(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_set_top_font(html, "courier", 0, 0, 0);
}

/*
 * Create a new Image struct and request the image-url to the cache
 * (If it either hits or misses, is not relevant here; that's up to the
 *  cache functions)
 */
static void Html_tag_open_img(DilloHtml *html, char *tag, gint tagsize)
{
   DilloImage *Image;
   DilloWeb *Web;
   DilloUrl *url, *usemap_url;
   DwPage *page;
   DwStyle style_attrs;
   char *width_ptr, *height_ptr, *alt_ptr;
   const char *attrbuf;
   gint ClientKey, border;

   if ( !(attrbuf = Html_get_attr(html, tag, tagsize, "src")) )
      return;

   if ( !(url = a_Url_new(attrbuf, URL_STR(html->linkblock->base_url), 0, 0)) )
      return;

   page = DW_PAGE (html->dw);

   width_ptr = NULL;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "width")))
      width_ptr = g_strdup(attrbuf);

   height_ptr = NULL;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "height")))
      height_ptr = g_strdup(attrbuf);

   alt_ptr = NULL;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "alt")))
      alt_ptr = g_strdup(attrbuf);

   usemap_url = NULL;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "usemap")))
      /* todo: usemap URLs outside of the document are not used. */
      usemap_url = a_Url_new(attrbuf, URL_STR(html->linkblock->base_url), 0,0);

   if ((width_ptr && !height_ptr) || (height_ptr && !width_ptr))
      DEBUG_HTML_MSG("Image tag only sepecifies <%s>\n",
                     (width_ptr) ? "width" : "height");

   style_attrs = *html->stack[html->stack_top].style;

   if (html->stack[html->stack_top].style->link != -1 || usemap_url != NULL) {
      /* Images within links */
      border = 1;
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "border")))
         border = strtol (attrbuf, NULL, 10);

      if (html->stack[html->stack_top].style->link != -1)
         /* In this case we can use the text color */
         a_Dw_style_box_set_border_color
            (&style_attrs,
             a_Dw_style_shaded_color_new (style_attrs.color->color_val,
                                          html->bw->main_window->window));
      else
         a_Dw_style_box_set_border_color
            (&style_attrs,
             a_Dw_style_shaded_color_new (html->linkblock->link_color,
                                          html->bw->main_window->window));

      a_Dw_style_box_set_border_style (&style_attrs, DW_STYLE_BORDER_SOLID);
      a_Dw_style_box_set_val (&style_attrs.border_width, border);
   }

   /* Add a new image widget to this page */
   Image = a_Image_new(0, 0, alt_ptr,
                       html->stack[html->stack_top].current_bg_color);
   Html_add_widget(html, DW_WIDGET(Image->dw), width_ptr, height_ptr,
                   &style_attrs);
   Html_connect_signals(html, GTK_OBJECT(Image->dw));

   /* Image maps */
   if (Html_get_attr(html, tag, tagsize, "ismap")) {
      /* BUG: if several ISMAP images follow each other without
       * being separated with a word, only the first one is ISMAPed
       */
      a_Dw_image_set_ismap (Image->dw);
      //g_print("  Html_tag_open_img: server-side map (ISMAP)\n");
   }
   if (usemap_url) {
      a_Dw_image_set_usemap (Image->dw, &html->linkblock->maps, usemap_url);
      a_Url_free (usemap_url);
   }

   /* Fill a Web structure for the cache query */
   Web = a_Web_new(url);
   Web->bw = html->bw;
   Web->Image = Image;
   Web->flags |= WEB_Image;
   /* Request image data from the cache */
   if ((ClientKey = a_Cache_open_url(Web, NULL, NULL)) != 0) {
      a_Interface_add_client(html->bw, ClientKey, 0);
      a_Interface_add_url(html->bw, url, WEB_Image);
   }
   a_Url_free(url);
   g_free(width_ptr);
   g_free(height_ptr);
   g_free(alt_ptr);
}

/*
 * <map>
 */
static void Html_tag_open_map(DilloHtml *html, char *tag, gint tagsize)
{
   char *hash_name;
   const char *attrbuf;
   DilloUrl *url;

   Html_push_tag(html, tag, tagsize);

   if (html->linkblock->map_open)
      g_print ("Invalid HTML syntax: nested <map>\n");
   else {
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "name"))) {
         hash_name = g_strdup_printf("#%s", attrbuf);
         url = a_Url_new (hash_name, URL_STR(html->linkblock->base_url), 0, 0);
         a_Dw_image_map_list_add_map (&html->linkblock->maps, url);
         a_Url_free (url);
         g_free(hash_name);
      }
      html->linkblock->map_open = TRUE;
   }
}

/*
 * ?
 */
static void Html_tag_close_map(DilloHtml *html, char *tag, gint tagsize)
{
   if (!html->linkblock->map_open)
      g_print ("Invalid HTML syntax: </map> without <map>\n");
   html->linkblock->map_open = FALSE;
   Html_pop_tag(html, tag, tagsize);
}

/*
 * Read coords in a string and fill a GdkPoint array
 */
static int Html_read_coords(const char *str, GdkPoint *array)
{
   gint i, toggle, pending, coord;
   const char *tail = str;
   char *newtail = NULL;

   i = 0;
   toggle = 0;
   pending = 1;
   while( pending ) {
      coord = strtol(tail, &newtail, 10);
      if (toggle) {
        array[i].y = coord;
        array[++i].x = 0;
        toggle = 0;
      } else {
        array[i].x = coord;
        array[i].y = -1;
        toggle = 1;
      }
      if (!*newtail || (coord == 0 && newtail == tail)) {
         pending = 0;
      } else {
         if (*newtail != ',') {
            DEBUG_HTML_MSG("usemap coords MUST be separated with ','\n");
         }
         tail = newtail + 1;
      }
   }

   return i;
}

/*
 * <AREA>
 */
static void Html_tag_open_area(DilloHtml *html, char *tag, gint tagsize)
{
   GdkPoint point[1024];
   DilloUrl* url;
   const char *attrbuf;
   gint type = DW_IMAGE_MAP_SHAPE_RECT;
   gint nbpoints, link = -1;

   if ( (attrbuf = Html_get_attr(html, tag, tagsize, "shape")) ) {
      if ( g_strcasecmp(attrbuf, "rect") == 0 )
         type = DW_IMAGE_MAP_SHAPE_RECT;
      else if ( g_strcasecmp(attrbuf, "circle") == 0 )
         type = DW_IMAGE_MAP_SHAPE_CIRCLE;
      else if ( g_strncasecmp(attrbuf, "poly", 4) == 0 )
         type = DW_IMAGE_MAP_SHAPE_POLY;
      else
         type = DW_IMAGE_MAP_SHAPE_RECT;
   }
   /* todo: add support for coords in % */
   if ( (attrbuf = Html_get_attr(html, tag, tagsize, "coords")) ) {
      /* Is this a valid poly ?
       * rect = x0,y0,x1,y1               => 2
       * circle = x,y,r                   => 2
       * poly = x0,y0,x1,y1,x2,y2 minimum => 3 */
      nbpoints = Html_read_coords(attrbuf, point);
   } else
      return;

   if ( Html_get_attr(html, tag, tagsize, "nohref") ) {
      link = -1;
      //g_print("nohref");
   }

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "href"))) {
      url = a_Url_new(attrbuf, URL_STR(html->linkblock->base_url), 0, 0);
      g_return_if_fail ( url != NULL );
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "alt")))
         a_Url_set_alt(url, attrbuf);

      link = Html_set_new_link(html, &url);
   }

   a_Dw_image_map_list_add_shape(&html->linkblock->maps, type, link,
                                 point, nbpoints);
}

/*
 * <A>
 */
static void Html_tag_open_a(DilloHtml *html, char *tag, gint tagsize)
{
   DwPage *page;
   DwStyle style_attrs, *old_style;
   DilloUrl *url;
   const char *attrbuf;
   gint dummy;

   Html_push_tag(html, tag, tagsize);

   page = DW_PAGE (html->dw);

   /* todo: add support for MAP with A HREF */
   Html_tag_open_area(html, tag, tagsize);

   if ( (attrbuf = Html_get_attr(html, tag, tagsize, "href"))) {
      url = a_Url_new(attrbuf, URL_STR(html->linkblock->base_url), 0, 0);
      g_return_if_fail ( url != NULL );

      old_style = html->stack[html->stack_top].style;
      style_attrs = *old_style;

      if (a_Cache_url_read(url, &dummy)) {
         html->InVisitedLink = TRUE;
         style_attrs.color = a_Dw_style_color_new
            (html->linkblock->visited_color, html->bw->main_window->window);
      } else {
         style_attrs.color = a_Dw_style_color_new
            (html->linkblock->link_color,  html->bw->main_window->window);
      }
      style_attrs.uline = TRUE;

      style_attrs.link = Html_set_new_link(html, &url);

      html->stack[html->stack_top].style =
         a_Dw_style_new (&style_attrs, html->bw->main_window->window);
      a_Dw_style_unref (old_style);
   }

   if ( (attrbuf = Html_get_attr(html, tag, tagsize, "name"))) {
      a_Dw_page_add_anchor(page, attrbuf, html->stack[html->stack_top].style);
      // g_print("Registering ANCHOR: %s\n", attrbuf);
   }
}

/*
 * <A> close function
 */
static void Html_tag_close_a(DilloHtml *html, char *tag, gint tagsize)
{
   html->InVisitedLink = FALSE;
   Html_pop_tag(html, tag, tagsize);
}

/*
 * Insert underlined text in the page.
 */
static void Html_tag_open_u(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   HTML_SET_TOP_ATTR (html, uline, 1);
}

/*
 * Insert strike-through text. Used by <S>, <STRIKE> and <DEL>.
 */
static void Html_tag_open_strike(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   HTML_SET_TOP_ATTR (html, strike, 1);
}

/*
 * <BLOCKQUOTE>
 */
static void Html_tag_open_blockquote(DilloHtml *html, char *tag, gint tagsize)
{
   Html_par_push_tag(html, tag, tagsize);
   Html_add_indented(html, 40, 40, 9);
}

/*
 * Handle the <UL> tag.
 */
static void Html_tag_open_ul(DilloHtml *html, char *tag, gint tagsize)
{
   const char *attrbuf;

   Html_par_push_tag(html, tag, tagsize);
   Html_add_indented(html, 40, 0, 9);

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "type"))) {
      if (g_strncasecmp(attrbuf, "disc", 4) == 0)
         html->stack[html->stack_top].list_level = DW_BULLET_DISC;
      else if (g_strncasecmp(attrbuf, "circle", 6) == 0)
         html->stack[html->stack_top].list_level = DW_BULLET_CIRCLE;
      else if (g_strncasecmp(attrbuf, "square", 6) == 0)
         html->stack[html->stack_top].list_level = DW_BULLET_SQUARE;

   } else if (++(html->stack[html->stack_top].list_level) > DW_BULLET_SQUARE)
      html->stack[html->stack_top].list_level = DW_BULLET_DISC;
   /* --EG :: I changed the behavior here : types are cycling instead of
    * being forced to square. It's easier for mixed lists level counting. */

   html->stack[html->stack_top].list_number = 0;
   html->stack[html->stack_top].ref_list_item = NULL;
}

/*
 * Handle the <OL> tag.
 */
static void Html_tag_open_ol(DilloHtml *html, char *tag, gint tagsize)
{
   const char *attrbuf;

   Html_par_push_tag (html, tag, tagsize);
   Html_add_indented(html, 40, 0, 9);

   html->stack[html->stack_top].list_level = DW_BULLET_1;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "type"))) {
      if (*attrbuf == '1')
         html->stack[html->stack_top].list_level = DW_BULLET_1;
      else if (*attrbuf == 'a')
         html->stack[html->stack_top].list_level = DW_BULLET_a;
      else if (*attrbuf == 'A')
         html->stack[html->stack_top].list_level = DW_BULLET_A;
      else if (*attrbuf == 'i')
         html->stack[html->stack_top].list_level = DW_BULLET_i;
      else if (*attrbuf == 'I')
         html->stack[html->stack_top].list_level = DW_BULLET_I;
   }

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "start")))
      html->stack[html->stack_top].list_number = strtol(attrbuf, NULL, 10);
   else
      html->stack[html->stack_top].list_number = 1;
   html->stack[html->stack_top].ref_list_item = NULL;
}

/*
 * Handle the <LI> tag.
 */
static void Html_tag_open_li(DilloHtml *html, char *tag, gint tagsize)
{
   DwWidget *bullet, *list_item, **ref_list_item;
   int i3,i2, i1, i0;
   char str[64];
   const char *attrbuf;
   gboolean low = FALSE;
   gint *list_number;
   gboolean push_par_break;

   /* We push a 9 pixel break on the stack only if there is an open <p>.
      todo: necessary? */
   push_par_break = Html_cleanup_tag(html, "p>");
   Html_cleanup_tag(html, "li>");

   /* This is necessary, because the tag is pushed next. */
   list_number = &html->stack[html->stack_top].list_number;
   ref_list_item = &html->stack[html->stack_top].ref_list_item;

   Html_push_tag(html, tag, tagsize);
   a_Dw_page_add_break(DW_PAGE (html->dw), push_par_break ? 9 : 0,
                       html->stack[(html)->stack_top].style);

   list_item = a_Dw_list_item_new((DwListItem*)*ref_list_item);
   Html_add_indented_widget(html, list_item, 0, 0, 0 /* or 1 */);
   *ref_list_item = list_item;

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "value")))
      *list_number = strtol(attrbuf, NULL, 10);

   if ( *list_number ){
      /* ORDERED LIST */
      switch(html->stack[html->stack_top].list_level){
         case DW_BULLET_a:
            low = TRUE;
         case DW_BULLET_A:
            i0 = *list_number - 1;
            i1 = i0/26; i2 = i1/26;
            i0 %= 26;   i1 %= 26;
            if (i2 > 26) /* more than 26*26*26=17576 elements ? */
               sprintf(str, "****.");
            else if (i2)
               sprintf(str, "%c%c%c.", 'A'+i2-1,'A'+i1-1,'A'+i0);
            else if (i1)
               sprintf(str, "%c%c.", 'A'+i1-1,'A'+i0);
            else
               sprintf(str, "%c.", 'A'+i0);
            if ( low )
               g_strdown(str);
            break;
         case DW_BULLET_i:
            low = TRUE;
         case DW_BULLET_I:
            i0 = *list_number - 1;
            i1 = i0/10; i2 = i1/10; i3 = i2/10;
            i0 %= 10;   i1 %= 10;   i2 %= 10;
            if (i3 > 4) /* more than 4999 elements ? */
               sprintf(str, "****.");
            else if (i3)
               sprintf(str, "%s%s%s%s.", roman_I3[i3-1], roman_I2[i2-1],
                       roman_I1[i1-1], roman_I0[i0]);
            else if (i2)
               sprintf(str, "%s%s%s.", roman_I2[i2-1],
                       roman_I1[i1-1], roman_I0[i0]);
            else if (i1)
               sprintf(str, "%s%s.", roman_I1[i1-1], roman_I0[i0]);
            else
               sprintf(str, "%s.", roman_I0[i0]);
            if ( low )
               g_strdown(str);
            break;
         case DW_BULLET_1:
         default:
            sprintf(str, "%d.", *list_number);
            break;
      }
      (*list_number)++;
      a_Dw_list_item_init_with_text(DW_LIST_ITEM (html->dw), g_strdup(str),
                                    html->stack[html->stack_top].style);
   } else {
      /* UNORDERED LIST */
      bullet = a_Dw_bullet_new(html->stack[html->stack_top].list_level);
      a_Dw_list_item_init_with_widget(DW_LIST_ITEM(html->dw), bullet,
                                      html->stack[html->stack_top].style);
   }
}

/*
 * <HR>
 */
static void Html_tag_open_hr(DilloHtml *html, char *tag, gint tagsize)
{
   DwWidget *hruler;
   DwStyle style_attrs;
   char *width_ptr;
   const char *attrbuf;
   gint32 size = 0;

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "width")))
      width_ptr = g_strdup(attrbuf);
   else
      width_ptr = g_strdup("100%");

   style_attrs = *html->stack[html->stack_top].style;

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "size")))
      size = strtol(attrbuf, NULL, 10);
  
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "align"))) {
      if (strcasecmp (attrbuf, "left") == 0)
         style_attrs.text_align = DW_STYLE_TEXT_ALIGN_LEFT;
      else if (strcasecmp (attrbuf, "right") == 0)
         style_attrs.text_align = DW_STYLE_TEXT_ALIGN_RIGHT;
      else if (strcasecmp (attrbuf, "center") == 0)
         style_attrs.text_align = DW_STYLE_TEXT_ALIGN_CENTER;
   }

   /* todo: evaluate attribute */
   if (Html_get_attr(html, tag, tagsize, "noshade")) {
      a_Dw_style_box_set_border_style (&style_attrs, DW_STYLE_BORDER_SOLID);
      a_Dw_style_box_set_border_color
         (&style_attrs,
          a_Dw_style_shaded_color_new (style_attrs.color->color_val,
                                       html->bw->main_window->window));
      if (size < 1)
         size = 1;
   } else {
      a_Dw_style_box_set_border_style (&style_attrs, DW_STYLE_BORDER_INSET);
      a_Dw_style_box_set_border_color
         (&style_attrs,
          a_Dw_style_shaded_color_new
             (html->stack[html->stack_top].current_bg_color,
              html->bw->main_window->window));
      if (size < 2)
         size = 2;
   }

   style_attrs.border_width.top =
      style_attrs.border_width.left = (size + 1) / 2;
   style_attrs.border_width.bottom =
      style_attrs.border_width.right = size / 2;

   a_Dw_page_add_break (DW_PAGE (html->dw), 5,
                        html->stack[(html)->stack_top].style);
   hruler = a_Dw_hruler_new ();
   Html_add_widget(html, hruler, width_ptr, NULL, &style_attrs);
   a_Dw_page_add_break (DW_PAGE (html->dw), 5,
                        html->stack[(html)->stack_top].style);
   g_free(width_ptr);
}

/*
 * <DL>
 */
static void Html_tag_open_dl(DilloHtml *html, char *tag, gint tagsize)
{
   /* may want to actually do some stuff here. */
   Html_par_push_tag(html, tag, tagsize);
}

/*
 * <DT>
 */
static void Html_tag_open_dt(DilloHtml *html, char *tag, gint tagsize)
{
   Html_cleanup_tag(html, "p>");
   Html_cleanup_tag(html, "dd>");
   Html_cleanup_tag(html, "dt>");
   Html_par_push_tag(html, tag, tagsize);
   Html_set_top_font(html, NULL, 0, 1, 1);
}

/*
 * <DD>
 */
static void Html_tag_open_dd(DilloHtml *html, char *tag, gint tagsize)
{
   Html_cleanup_tag(html, "p>");
   Html_cleanup_tag(html, "dd>");
   Html_cleanup_tag(html, "dt>");

   Html_par_push_tag(html, tag, tagsize);
   Html_add_indented(html, 40, 40, 9);
}

/*
 * <PRE>
 */
static void Html_tag_open_pre(DilloHtml *html, char *tag, gint tagsize)
{
   Html_par_push_tag(html, tag, tagsize);
   Html_set_top_font(html, "courier", 0, 0, 0);

   /* Is the placement of this statement right? */
   html->stack[html->stack_top].parse_mode = DILLO_HTML_PARSE_MODE_PRE;
   HTML_SET_TOP_ATTR (html, nowrap, TRUE);
}

/*
 * Handle <FORM> tag
 */
static void Html_tag_open_form(DilloHtml *html, char *tag, gint tagsize)
{
   DilloUrl *action;
   DilloHtmlMethod method;
   DilloHtmlEnc enc;
   const char *attrbuf;

   Html_par_push_tag(html, tag, tagsize);

   html->InForm = TRUE;

   method = DILLO_HTML_METHOD_GET;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "method"))) {
      if (!g_strcasecmp(attrbuf, "post"))
         method = DILLO_HTML_METHOD_POST;
      /* todo: maybe deal with unknown methods? */
   }
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "action")))
      action = a_Url_new(attrbuf, URL_STR(html->linkblock->base_url), 0, 0);
   else
      action = a_Url_dup(html->linkblock->base_url);
   enc = DILLO_HTML_ENC_URLENCODING;
   if ( (attrbuf = Html_get_attr(html, tag, tagsize, "encoding")) ) {
      /* todo: maybe deal with unknown encodings? */
   }
   Html_form_new(html->linkblock, method, action, enc);
   a_Url_free(action);
}

static void Html_tag_close_form(DilloHtml *html, char *tag, gint tagsize)
{
   DilloHtmlForm *form;
   gint i;

   if (html->InForm) {
      /* Make buttons sensitive again */
      form = &(html->linkblock->forms[html->linkblock->num_forms - 1]);
      for(i = 0; i < form->num_inputs; i++) {
         if (form->inputs[i].type == DILLO_HTML_INPUT_SUBMIT ||
            form->inputs[i].type == DILLO_HTML_INPUT_RESET) {
            gtk_widget_set_sensitive(form->inputs[i].widget, TRUE);
         }
      }
   }
   html->InForm = FALSE;
   html->InSelect = FALSE;
   Html_pop_tag(html, tag, tagsize);
}

/*
 * Handle <META>
 * We do not support http-equiv=refresh because it's non standar and
 * can be easily abused!
 */
static void Html_tag_open_meta(DilloHtml *html, char *tag, gint tagsize)
{
   /* only valid inside HEAD */
   if (html->InTag == DILLO_HTML_IN_HEAD)
      return;
}

/*
 * Set the history of the menu to be consistent with the active menuitem.
 */
static void Html_select_set_history(DilloHtmlInput *input)
{
   gint i;

   for (i = 0; i < input->select->num_options; i++) {
      if (GTK_CHECK_MENU_ITEM(input->select->options[i].menuitem)->active) {
         gtk_option_menu_set_history(GTK_OPTION_MENU(input->widget), i);
         break;
      }
   }
}

/*
 * Reset the input widget to the initial value.
 */
static void Html_reset_input(DilloHtmlInput *input)
{
   gint i;

   switch (input->type) {
   case DILLO_HTML_INPUT_TEXT:
   case DILLO_HTML_INPUT_PASSWORD:
      gtk_entry_set_text(GTK_ENTRY(input->widget), input->init_str);
      break;
   case DILLO_HTML_INPUT_CHECKBOX:
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(input->widget),
                                  input->init_val);
      break;
   case DILLO_HTML_INPUT_RADIO:
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(input->widget),
                                  input->init_val);
      break;
   case DILLO_HTML_INPUT_SELECT:
      if (input->select != NULL) {
         /* this is in reverse order so that, in case more than one was
          * selected, we get the last one, which is consistent with handling
          * of multiple selected options in the layout code. */
         for (i = input->select->num_options - 1; i >= 0; i--) {
            if (input->select->options[i].init_val) {
               gtk_menu_item_activate(GTK_MENU_ITEM
                                      (input->select->options[i].menuitem));
               Html_select_set_history(input);
               break;
            }
         }
      }
      break;
   case DILLO_HTML_INPUT_SEL_LIST:
      if (!input->select)
         break;
      for (i = 0; i < input->select->num_options; i++) {
         if (input->select->options[i].init_val) {
            if (input->select->options[i].menuitem->state == GTK_STATE_NORMAL)
               gtk_list_select_child(GTK_LIST(input->select->menu),
                                     input->select->options[i].menuitem);
         } else {
            if (input->select->options[i].menuitem->state==GTK_STATE_SELECTED)
               gtk_list_unselect_child(GTK_LIST(input->select->menu),
                                       input->select->options[i].menuitem);
         }
      }
      break;
   case DILLO_HTML_INPUT_TEXTAREA:
      if (input->init_str != NULL) {
         int pos = 0;
         gtk_editable_delete_text(GTK_EDITABLE(input->widget), 0, -1);
         gtk_editable_insert_text(GTK_EDITABLE(input->widget), input->init_str,
                                  strlen(input->init_str), &pos);
      }
      break;
   default:
      break;
   }
}


/*
 * Add a new input to the form data structure, setting the initial
 * values.
 */
static void Html_add_input(DilloHtmlForm *form,
                           DilloHtmlInputType type,
                           GtkWidget *widget,
                           const char *name,
                           const char *init_str,
                           DilloHtmlSelect *select,
                           gboolean init_val)
{
   DilloHtmlInput *input;

// g_print("name=[%s] init_str=[%s] init_val=[%d]\n",
//          name, init_str, init_val);
   a_List_add(form->inputs, form->num_inputs, sizeof(*form->inputs),
              form->num_inputs_max);
   input = &(form->inputs[form->num_inputs]);
   input->type = type;
   input->widget = widget;
   input->name = (name) ? g_strdup(name) : NULL;
   input->init_str = (init_str) ? g_strdup(init_str) : NULL;
   input->select = select;
   input->init_val = init_val;
   Html_reset_input(input);
   form->num_inputs++;
}


/*
 * Given a GtkWidget, find the form that contains it.
 * Return value: form_index if successful, -1 otherwise.
 */
static int Html_find_form(GtkWidget *reset, DilloHtmlLB *html_lb)
{
   gint form_index;
   gint input_index;
   DilloHtmlForm *form;

   for (form_index = 0; form_index < html_lb->num_forms; form_index++) {
      form = &(html_lb->forms[form_index]);
      for (input_index = 0; input_index < form->num_inputs; input_index++) {
         if (form->inputs[input_index].widget == reset) {
            return form_index;
         }
      }
   }
   return -1;
}

/*
 * Reset all inputs in the form containing reset to their initial values.
 * In general, reset is the reset button for the form.
 */
static void Html_reset_form(GtkWidget *reset, DilloHtmlLB *html_lb)
{
   gint i, j;
   DilloHtmlForm *form;

   if ( (i = Html_find_form(reset, html_lb)) != -1 ){
      form = &html_lb->forms[i];
      for ( j = 0; j < form->num_inputs; j++)
         Html_reset_input(&(form->inputs[j]));
   }
}

/*
 * Urlencode 'val' and append it to 'str'
 * -RL :: According to the RFC 1738, only alphanumerics, the special
 *        characters "$-_.+!*'(),", and reserved characters ";/?:@=&" used
 *        for their *reserved purposes* may be used unencoded within a URL.
 * We'll escape everything but alphanumeric and "-_.*" (as lynx).  --Jcid
 */
static void Html_urlencode_append(GString *str, const char *val)
{
   gint i;
   static const char *verbatim = "-_.*";
   static const char *hex = "0123456789ABCDEF";

   if ( val == NULL )
      return;
   for (i = 0; val[i] != '\0'; i++) {
      if (val[i] == ' ') {
         g_string_append_c(str, '+');
      } else if (isalnum (val[i]) || strchr(verbatim, val[i])) {
         g_string_append_c(str, val[i]);
      } else if (val[i] == '\n') {
         g_string_append(str, "%0D%0A");
      } else {
         g_string_append_c(str, '%');
         g_string_append_c(str, hex[(val[i] >> 4) & 15]);
         g_string_append_c(str, hex[val[i] & 15]);
      }
   }
}

/*
 * Append a name-value pair to an existing url.
 * (name and value are urlencoded before appending them)
 */
static void
 Html_append_input(GString *url, const char *name, const char *value)
{
   if (name != NULL) {
      Html_urlencode_append(url, name);
      g_string_append_c(url, '=');
      Html_urlencode_append(url, value);
      g_string_append_c(url, '&');
   }
}

/*
 * Submit the form containing the submit input by making a new query URL
 * and sending it with a_Nav_push.
 * (Called by GTK+)
 */
static void Html_submit_form(GtkWidget *submit, DilloHtmlLB *html_lb)
{
   gint i, input_index;
   DilloHtmlForm *form;
   DilloHtmlInput *input;
   DilloUrl *new_url;
   gchar *url_str, *action_str, *p;

   /* Search the form that generated the submition event */
   if ( (i = Html_find_form(submit, html_lb)) == -1 )
      return;

   form = &html_lb->forms[i];
   if ((form->method == DILLO_HTML_METHOD_GET) ||
       (form->method == DILLO_HTML_METHOD_POST)) {
      GString *DataStr = g_string_sized_new(4096);

      DEBUG_MSG(3,"Html_submit_form form->action=%s\n",URL_STR(form->action));

      for (input_index = 0; input_index < form->num_inputs; input_index++) {
         input = &(form->inputs[input_index]);
         switch (input->type) {
           case DILLO_HTML_INPUT_TEXT:
           case DILLO_HTML_INPUT_PASSWORD:
              Html_append_input(DataStr, input->name,
                                gtk_entry_get_text(GTK_ENTRY(input->widget)));
              break;
           case DILLO_HTML_INPUT_CHECKBOX:
           case DILLO_HTML_INPUT_RADIO:
              if (GTK_TOGGLE_BUTTON(input->widget)->active &&
                  input->name != NULL && input->init_str != NULL) {
                 Html_append_input(DataStr, input->name, input->init_str);
              }
              break;
           case DILLO_HTML_INPUT_HIDDEN:
              Html_append_input(DataStr, input->name, input->init_str);
              break;
           case DILLO_HTML_INPUT_SELECT:
              for (i = 0; i < input->select->num_options; i++) {
                 if (GTK_CHECK_MENU_ITEM(input->select->options[i].menuitem)->
                     active) {
                    Html_append_input(DataStr, input->name,
                                      input->select->options[i].value);
                    break;
                 }
              }
              break;
           case DILLO_HTML_INPUT_SEL_LIST:
              for (i = 0; i < input->select->num_options; i++) {
                 if (input->select->options[i].menuitem->state ==
                     GTK_STATE_SELECTED) {
                    Html_append_input(DataStr, input->name,
                                      input->select->options[i].value);
                 }
              }
              break;
           case DILLO_HTML_INPUT_TEXTAREA:
              Html_append_input(DataStr, input->name,
                 gtk_editable_get_chars(GTK_EDITABLE (input->widget),0,-1));
              break;
           case DILLO_HTML_INPUT_INDEX:
              Html_urlencode_append(DataStr,
                 gtk_entry_get_text(GTK_ENTRY(input->widget)));
              break;
           case DILLO_HTML_INPUT_SUBMIT:
              /* Only the button that triggered the submit. */
              if (input->widget == submit)
                 Html_append_input(DataStr, input->name, input->init_str);
              break;
           default:
              break;
         } /* switch */
      } /* for (inputs) */

      if ( DataStr->str[DataStr->len - 1] == '&' )
         g_string_truncate(DataStr, DataStr->len - 1);

      /* remove <fragment> and <query> sections if present */
      action_str = g_strdup(URL_STR(form->action));
      if ((p = strchr(action_str, '#')) && (*p = 0));
      if ((p = strchr(action_str, '?')) && (*p = 0));

      if (form->method == DILLO_HTML_METHOD_POST) {
        new_url = a_Url_new(action_str, NULL, 0, 0);
        a_Url_set_data(new_url, DataStr->str);
        a_Url_set_flags(new_url, URL_FLAGS(new_url) | URL_Post);
      } else {
        url_str = g_strconcat(action_str, "?", DataStr->str, NULL);
        new_url = a_Url_new(url_str, NULL, 0, 0);
        a_Url_set_flags(new_url, URL_FLAGS(new_url) | URL_Get);
        g_free(url_str);
      }

      a_Nav_push(html_lb->bw, new_url);
      g_free(action_str);
      g_string_free(DataStr, TRUE);
      a_Url_free(new_url);
   } else {
      g_print("Html_submit_form: Method unknown\n");
   }
}


/*
 * Submit form if it has no submit button.
 * (Called by GTK+ when the user presses enter in a text entry within a form)
 */
static void Html_enter_submit_form(GtkWidget *submit, DilloHtmlLB *html_lb)
{
   gint i;

   /* Search the form that generated the submition event */
   if ( (i = Html_find_form(submit, html_lb)) == -1 )
      return;

   if ( html_lb->forms[i].HasSubmitButton == FALSE )
      Html_submit_form(submit, html_lb);
}

/*
 * Add a new input to current form
 */
static void Html_tag_open_input(DilloHtml *html, char *tag, gint tagsize)
{
   DilloHtmlForm *form;
   DilloHtmlInputType inp_type;
   DilloHtmlLB *html_lb;
   DwWidget *embed_gtk;
   GtkWidget *widget = NULL;
   GSList *group;
   char *value, *name, *type, *init_str;
   const char *attrbuf;
   gboolean init_val = FALSE;
   gint input_index;

   if (!html->InForm) {
      DEBUG_HTML_MSG("input camp outside <form>\n");
      return;
   }

   html_lb = html->linkblock;
   form = &(html_lb->forms[html_lb->num_forms - 1]);

   /* Get 'value', 'name' and 'type' */
   value = (attrbuf = Html_get_attr(html, tag, tagsize, "value")) ?
            g_strdup(attrbuf) : NULL;

   name = (attrbuf = Html_get_attr(html, tag, tagsize, "name")) ?
           g_strdup(attrbuf) : NULL;

   type = (attrbuf = Html_get_attr(html, tag, tagsize, "type")) ?
           g_strdup(attrbuf) : g_strdup("");

   init_str = NULL;
   if (!g_strcasecmp(type, "password")) {
      inp_type = DILLO_HTML_INPUT_PASSWORD;
      widget = gtk_entry_new();
      gtk_entry_set_visibility(GTK_ENTRY(widget), FALSE);
      /* Don't parse for entities */
      if (value)
         init_str = g_strdup(Html_get_attr2(html, tag, tagsize, "value", 0));
   } else if (!g_strcasecmp(type, "checkbox")) {
      inp_type = DILLO_HTML_INPUT_CHECKBOX;
      widget = gtk_check_button_new();
      init_val = (Html_get_attr(html, tag, tagsize, "checked") != NULL);
      init_str = (value) ? value : g_strdup("on");
   } else if (!g_strcasecmp(type, "radio")) {
      inp_type = DILLO_HTML_INPUT_RADIO;
      group = NULL;
      for (input_index = 0; input_index < form->num_inputs; input_index++) {
         if (form->inputs[input_index].type == DILLO_HTML_INPUT_RADIO &&
             (form->inputs[input_index].name &&
              !g_strcasecmp(form->inputs[input_index].name, name)) ){
            group = gtk_radio_button_group(GTK_RADIO_BUTTON
                                           (form->inputs[input_index].widget));
            form->inputs[input_index].init_val = TRUE;
            break;
         }
      }
      widget = gtk_radio_button_new(group);

      init_val = (Html_get_attr(html, tag, tagsize, "checked") != NULL);
      init_str = (value) ? value : NULL;
   } else if (!g_strcasecmp(type, "hidden")) {
      inp_type = DILLO_HTML_INPUT_HIDDEN;
      /* Don't parse for entities */
      if (value)
         init_str = g_strdup(Html_get_attr2(html, tag, tagsize, "value", 0));
   } else if (!g_strcasecmp(type, "submit")) {
      inp_type = DILLO_HTML_INPUT_SUBMIT;
      form->HasSubmitButton = TRUE;
      init_str = (value) ? value : g_strdup("submit");
      widget = gtk_button_new_with_label(init_str);
      gtk_widget_set_sensitive(widget, FALSE); /* Until end of FORM! */
      gtk_signal_connect(GTK_OBJECT(widget), "clicked",
                         GTK_SIGNAL_FUNC(Html_submit_form), html_lb);
   } else if (!g_strcasecmp(type, "reset")) {
      inp_type = DILLO_HTML_INPUT_RESET;
      init_str = (value) ? value : g_strdup("Reset");
      widget = gtk_button_new_with_label(init_str);
      gtk_widget_set_sensitive(widget, FALSE); /* Until end of FORM! */
      gtk_signal_connect(GTK_OBJECT(widget), "clicked",
                         GTK_SIGNAL_FUNC(Html_reset_form), html_lb);
   } else if (!g_strcasecmp(type, "image")) {
      /* TODO: implement this as an image. It'd better be clickable too! =) */
      inp_type = DILLO_HTML_INPUT_IMAGE;
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "alt")))
         widget = gtk_button_new_with_label(attrbuf);
      else
         widget = gtk_button_new_with_label("Submit");
      gtk_signal_connect(GTK_OBJECT(widget), "clicked",
                         GTK_SIGNAL_FUNC(Html_submit_form), html_lb);
      init_str = (value) ? value : NULL;
   } else if (!g_strcasecmp(type, "file")) {
      /* TODO: */
      inp_type = DILLO_HTML_INPUT_FILE;
      init_str = (value) ? value : NULL;
      g_print("An input of the type \"file\" wasn't rendered!\n");
   } else if (!g_strcasecmp(type, "button")) {
      inp_type = DILLO_HTML_INPUT_BUTTON;
      if (value) {
         init_str = value;
         widget = gtk_button_new_with_label(init_str);
      }
   } else {
      /* Text input, which also is the default */
      inp_type = DILLO_HTML_INPUT_TEXT;
      widget = gtk_entry_new();

      init_str = (value) ? value : NULL;
      gtk_signal_connect(GTK_OBJECT(widget), "activate",
                         GTK_SIGNAL_FUNC(Html_enter_submit_form),
                         html_lb);
   }

   Html_add_input(form, inp_type, widget, name,
                  (init_str) ? init_str : "", NULL, init_val);

   if (widget != NULL) {
      if (inp_type == DILLO_HTML_INPUT_TEXT ||
         inp_type == DILLO_HTML_INPUT_PASSWORD) {
         /*
          * The following is necessary, because gtk_entry_button_press
          * returns FALSE, so the event would be delivered to the
          * GtkDwScrolledFrame, which then would be focused, instead of
          * the entry.
          */
         gtk_signal_connect_after(GTK_OBJECT(widget), "button_press_event",
                                  GTK_SIGNAL_FUNC(gtk_true), NULL);

         /* Readonly or not? */
         gtk_entry_set_editable(
            GTK_ENTRY(widget),
            !(Html_get_attr(html, tag, tagsize, "readonly")));

         /* Set width of the entry */
         if ((attrbuf = Html_get_attr(html, tag, tagsize, "size")))
            gtk_widget_set_usize(widget, strtol(attrbuf, NULL, 10) *
                                 gdk_char_width(widget->style->font, '0'), 0);

         /* Maximum length of the text in the entry */
         if ((attrbuf = Html_get_attr(html, tag, tagsize, "maxlength")))
            gtk_entry_set_max_length(GTK_ENTRY(widget),
                                     strtol(attrbuf, NULL, 10));
      }
      gtk_widget_show(widget);

      embed_gtk = a_Dw_embed_gtk_new();
      a_Dw_embed_gtk_add_gtk (DW_EMBED_GTK (embed_gtk), widget);
      a_Dw_page_add_widget(DW_PAGE (html->dw), embed_gtk,
                           html->stack[html->stack_top].style);
   }

   g_free(type);
   g_free(name);
   if (init_str != value)
      g_free(init_str);
   g_free(value);
}

/*
 * The ISINDEX tag is just a deprecated form of <INPUT type=text> with
 * implied FORM, afaics.
 */
static void Html_tag_open_isindex(DilloHtml *html, char *tag, gint tagsize)
{
   DilloHtmlForm *form;
   DilloHtmlLB *html_lb;
   DilloUrl *action;
   GtkWidget *widget;
   DwWidget *embed_gtk;
   const char *attrbuf;

   html_lb = html->linkblock;

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "action")))
      action = a_Url_new(attrbuf, URL_STR(html->linkblock->base_url), 0, 0);
   else
      action = a_Url_dup(html->linkblock->base_url);

   Html_form_new(html->linkblock, DILLO_HTML_METHOD_GET, action,
                 DILLO_HTML_ENC_URLENCODING);

   form = &(html_lb->forms[html_lb->num_forms - 1]);

   Html_cleanup_tag(html, "p>");
   a_Dw_page_add_break(DW_PAGE (html->dw), 9,
                       html->stack[(html)->stack_top].style);

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "prompt")))
      a_Dw_page_add_text(DW_PAGE (html->dw), g_strdup(attrbuf),
                         html->stack[html->stack_top].style);

   widget = gtk_entry_new();
   Html_add_input(form, DILLO_HTML_INPUT_INDEX,
                  widget, NULL, NULL, NULL, FALSE);
   gtk_signal_connect(GTK_OBJECT(widget), "activate",
                      GTK_SIGNAL_FUNC(Html_enter_submit_form),
                      html_lb);
   gtk_widget_show(widget);
   /* compare <input type=text> */
   gtk_signal_connect_after(GTK_OBJECT(widget), "button_press_event",
                            GTK_SIGNAL_FUNC(gtk_true),
                            NULL);

   embed_gtk = a_Dw_embed_gtk_new();
   a_Dw_embed_gtk_add_gtk(DW_EMBED_GTK(embed_gtk), widget);
   a_Dw_page_add_widget(DW_PAGE (html->dw), embed_gtk,
                        html->stack[html->stack_top].style);

   g_free(action);
}

/*
 * Close  textarea
 * (TEXTAREA is parsed in VERBATIM mode, and entities are handled here)
 */
static void Html_tag_close_textarea(DilloHtml *html, char *tag, gint tagsize)
{
   DilloHtmlLB *html_lb = html->linkblock;
   char *str;
   DilloHtmlForm *form;

   if (!html->InForm)
      return;

   /* Remove a newline that follows the opening tag */
   if (html->Stash->str[0] == '\n')
      html->Stash = g_string_erase(html->Stash, 0, 1);

   str = Html_parse_entities(html->Stash->str, html->Stash->len);

   form = &(html_lb->forms[html_lb->num_forms - 1]);

   form->inputs[form->num_inputs - 1].init_str = str;
   gtk_text_insert(GTK_TEXT(form->inputs[form->num_inputs - 1].widget),
                   NULL, NULL, NULL, str, -1);
   Html_pop_tag(html, tag, tagsize);
}

/*
 * The textarea tag
 * (todo: It doesn't support wrapping).
 */
static void Html_tag_open_textarea(DilloHtml *html, char *tag, gint tagsize)
{
   DilloHtmlLB *html_lb;
   DilloHtmlForm *form;
   GtkWidget *widget;
   GtkWidget *scroll;
   DwWidget *embed_gtk;
   char *name;
   const char *attrbuf;
   int cols, rows;

   html_lb = html->linkblock;

   if (!html->InForm)
      return;

   form = &(html_lb->forms[html_lb->num_forms - 1]);

   Html_push_tag(html, tag, tagsize);
   Html_stash_init(html);
   html->stack[html->stack_top].parse_mode = DILLO_HTML_PARSE_MODE_VERBATIM;

   cols = 20;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "cols")))
      cols = strtol(attrbuf, NULL, 10);
   rows = 10;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "rows")))
      rows = strtol(attrbuf, NULL, 10);
   name = NULL;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "name")))
      name = g_strdup(attrbuf);

   widget = gtk_text_new(NULL, NULL);
   /* compare <input type=text> */
   gtk_signal_connect_after(GTK_OBJECT(widget), "button_press_event",
                            GTK_SIGNAL_FUNC(gtk_true),
                            NULL);

   /* Calculate the width and height based on the cols and rows
    * todo: Get it right... Get the metrics from the font that will be used.
    */
   gtk_widget_set_usize(widget, 6 * cols, 16 * rows);

   /* If the attribute readonly isn't specified we make the textarea
    * editable. If readonly is set we don't have to do anything.
    */
   if (!Html_get_attr(html, tag, tagsize, "readonly"))
      gtk_text_set_editable(GTK_TEXT(widget), TRUE);

   scroll = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
   gtk_container_add(GTK_CONTAINER(scroll), widget);
   gtk_widget_show(widget);
   gtk_widget_show(scroll);

   Html_add_input(form, DILLO_HTML_INPUT_TEXTAREA,
                  widget, name, NULL, NULL, FALSE);
   g_free(name);

   embed_gtk = a_Dw_embed_gtk_new ();
   a_Dw_embed_gtk_add_gtk (DW_EMBED_GTK (embed_gtk), scroll);
   a_Dw_page_add_widget(DW_PAGE (html->dw), embed_gtk,
                        html->stack[html->stack_top].style);
}

/*
 * <SELECT>
 */
/* The select tag is quite tricky, because of gorpy html syntax. */
static void Html_tag_open_select(DilloHtml *html, char *tag, gint tagsize)
{
   DilloHtmlForm *form;
   DilloHtmlSelect *Select;
   DilloHtmlLB *html_lb;
   GtkWidget *widget, *menu;
   char *name;
   const char *attrbuf;
   gint size, type;

   Html_push_tag(html, tag, tagsize);

   if (!html->InForm)
      return;

   html->InSelect = TRUE;

   html_lb = html->linkblock;

   form = &(html_lb->forms[html_lb->num_forms - 1]);

   name = NULL;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "name")))
      name = g_strdup(attrbuf);
   size = 1;
   if ((attrbuf = Html_get_attr(html, tag, tagsize, "size")))
      size = strtol(attrbuf, NULL, 10);
   if (size < 1)
      size = 1;

   if (size == 1) {
      menu = gtk_menu_new();
      widget = gtk_option_menu_new();
      type = DILLO_HTML_INPUT_SELECT;
   } else {
      menu = gtk_list_new();
      widget = menu;
      if (Html_get_attr(html, tag, tagsize, "multiple"))
         gtk_list_set_selection_mode(GTK_LIST(menu), GTK_SELECTION_MULTIPLE);
      type = DILLO_HTML_INPUT_SEL_LIST;
   }

   Select = g_new(DilloHtmlSelect, 1);
   Select->menu = menu;
   Select->size = size;
   Select->num_options = 0;
   Select->num_options_max = 8;
   Select->options = g_new(DilloHtmlOption, Select->num_options_max);

   Html_add_input(form, type, widget, name, NULL, Select, FALSE);
   Html_stash_init(html);
   g_free(name);
}

/*
 * ?
 */
static void Html_option_finish(DilloHtml *html)
{
   DilloHtmlForm *form;
   DilloHtmlInput *input;
   GtkWidget *menuitem;
   GSList *group;
   DilloHtmlSelect *select;

   if (!html->InForm)
      return;

   form = &(html->linkblock->forms[html->linkblock->num_forms - 1]);
   input = &(form->inputs[form->num_inputs - 1]);
   if ( input->select->num_options <= 0)
      return;

   select = input->select;
   if (input->type == DILLO_HTML_INPUT_SELECT ) {
      if ( select->num_options == 1)
         group = NULL;
      else
         group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM
                                   (select->options[0].menuitem));
      menuitem = gtk_radio_menu_item_new_with_label(group, html->Stash->str);
      select->options[select->num_options - 1].menuitem = menuitem;
      if ( select->options[select->num_options - 1].value == NULL )
         select->options[select->num_options - 1].value =
             g_strdup(html->Stash->str);
      gtk_menu_append(GTK_MENU(select->menu), menuitem);
      if ( select->options[select->num_options - 1].init_val )
         gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
      gtk_widget_show(menuitem);
      gtk_signal_connect (GTK_OBJECT (menuitem), "select",
                          GTK_SIGNAL_FUNC (a_Interface_scroll_popup),
                          NULL);
   } else if ( input->type == DILLO_HTML_INPUT_SEL_LIST ) {
      menuitem = gtk_list_item_new_with_label(html->Stash->str);
      select->options[select->num_options - 1].menuitem = menuitem;
      if (select->options[select->num_options - 1].value == NULL)
         select->options[select->num_options - 1].value =
             g_strdup(html->Stash->str);
      gtk_container_add(GTK_CONTAINER(select->menu), menuitem);
      if ( select->options[select->num_options - 1].init_val )
         gtk_list_select_child(GTK_LIST(select->menu), menuitem);
      gtk_widget_show(menuitem);
   }
}

/*
 * ?
 */
static void Html_tag_open_option(DilloHtml *html, char *tag, gint tagsize)
{
   DilloHtmlForm *form;
   DilloHtmlInput *input;
   DilloHtmlLB *html_lb;
   const char *attrbuf;
   gint no;

   if (!html->InSelect)
      return;

   html_lb = html->linkblock;

   form = &(html_lb->forms[html_lb->num_forms - 1]);
   input = &(form->inputs[form->num_inputs - 1]);
   if (input->type == DILLO_HTML_INPUT_SELECT ||
       input->type == DILLO_HTML_INPUT_SEL_LIST) {
      Html_option_finish(html);
      no = input->select->num_options;
      a_List_add(input->select->options, no, sizeof(DilloHtmlOption),
                 input->select->num_options_max);
      input->select->options[no].menuitem = NULL;
      if ((attrbuf = Html_get_attr(html, tag, tagsize, "value")))
         input->select->options[no].value = g_strdup(attrbuf);
      else
         input->select->options[no].value = NULL;
      input->select->options[no].init_val =
         (Html_get_attr(html, tag, tagsize, "selected") != NULL);
      input->select->num_options++;
   }
   Html_stash_init(html);
}

/*
 * ?
 */
static void Html_tag_close_select(DilloHtml *html, char *tag, gint tagsize)
{
   DilloHtmlForm *form;
   DilloHtmlInput *input;
   GtkWidget *scrolledwindow;
   DilloHtmlLB *html_lb;
   DwWidget *embed_gtk;

   if (!html->InSelect)
      return;

   html->InSelect = FALSE;

   html_lb = html->linkblock;

   form = &(html_lb->forms[html_lb->num_forms - 1]);
   input = &(form->inputs[form->num_inputs - 1]);
   if (input->type == DILLO_HTML_INPUT_SELECT) {
      Html_option_finish(html);

      gtk_option_menu_set_menu(GTK_OPTION_MENU(input->widget),
                               input->select->menu);
      Html_select_set_history(input);
#if 0
      gtk_option_menu_set_history(GTK_OPTION_MENU(input->widget), 1);
#endif

      gtk_widget_show(input->widget);

      embed_gtk = a_Dw_embed_gtk_new ();
      a_Dw_embed_gtk_add_gtk (DW_EMBED_GTK (embed_gtk), input->widget);
      a_Dw_page_add_widget(DW_PAGE (html->dw), embed_gtk,
                           html->stack[html->stack_top].style);
   } else if (input->type == DILLO_HTML_INPUT_SEL_LIST) {
      Html_option_finish(html);

      if (input->select->size < input->select->num_options) {
         scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
         gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
         gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW
                                               (scrolledwindow),
                                               input->widget);

         gtk_container_set_focus_vadjustment
            (GTK_CONTAINER (input->widget),
             gtk_scrolled_window_get_vadjustment
             (GTK_SCROLLED_WINDOW(scrolledwindow)));

         /* Calculate the height of the scrolled window */
         gtk_widget_size_request(input->widget, &input->widget->requisition);
         gtk_widget_set_usize(scrolledwindow, -1, input->select->size *
                              (input->select->options->menuitem->
                               requisition.height));
         gtk_widget_show(input->widget);
         input->widget = scrolledwindow;
      }
      gtk_widget_show(input->widget);

      /* note: In this next call, scrolledwindows get a g_warning from
       * gdkwindow.c:422. I'm not really going to sweat it now - the
       * embedded widget stuff is going to get massively redone anyway. */
      embed_gtk = a_Dw_embed_gtk_new ();
      a_Dw_embed_gtk_add_gtk (DW_EMBED_GTK (embed_gtk), input->widget);
      a_Dw_page_add_widget(DW_PAGE (html->dw), embed_gtk,
                           html->stack[html->stack_top].style);
   }
   Html_pop_tag(html, tag, tagsize);
}

/*
 * Set the Document Base URI
 */
static void Html_tag_open_base(DilloHtml *html, char *tag, gint tagsize)
{
   const char *attrbuf;

   if ((attrbuf = Html_get_attr(html, tag, tagsize, "href"))) {
      a_Url_free(html->linkblock->base_url);
      html->linkblock->base_url = a_Url_new(attrbuf, NULL, 0, 0);
   }
}

/*
 * <CODE>
 */
static void Html_tag_open_code(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_set_top_font(html, "courier", 0, 0, 0);
}

/*
 * <DFN>
 */
static void Html_tag_open_dfn(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_set_top_font(html, NULL, 0, 2, 3);
}

/*
 * <KBD>
 */
static void Html_tag_open_kbd(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_set_top_font(html, "courier", 0, 0, 0);
}

static void Html_tag_open_samp(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_set_top_font(html, "courier", 0, 0, 0);
}

/*
 * <VAR>
 */
static void Html_tag_open_var(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   Html_set_top_font(html, NULL, 0, 2, 2);
}

static void Html_tag_open_sub(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   HTML_SET_TOP_ATTR (html, SubSup, TEXT_SUB);
}

static void Html_tag_open_sup(DilloHtml *html, char *tag, gint tagsize)
{
   Html_push_tag(html, tag, tagsize);
   HTML_SET_TOP_ATTR (html, SubSup, TEXT_SUP);
}

/*
 * <DIV> (todo: make a complete implementation)
 */
static void Html_tag_open_div(DilloHtml *html, char *tag, gint tagsize)
{
   a_Dw_page_add_break(DW_PAGE (html->dw), 0,
                       html->stack[(html)->stack_top].style);
   Html_push_tag(html, tag, tagsize);
   Html_tag_set_align_attr (html, tag, tagsize);
}

/*
 * </DIV>, also used for </CENTER>
 */
static void Html_tag_close_div(DilloHtml *html, char *tag, gint tagsize)
{
   Html_pop_tag(html, tag, tagsize);
   a_Dw_page_add_break(DW_PAGE (html->dw), 0,
                       html->stack[(html)->stack_top].style);
}

/*
 * Default close for most tags - just pop the stack.
 */
static void Html_tag_close_default(DilloHtml *html, char *tag, gint tagsize)
{
   Html_pop_tag(html, tag, tagsize);
}

/*
 * Default close for paragraph tags - pop the stack and break.
 */
static void Html_tag_close_par(DilloHtml *html, char *tag, gint tagsize)
{
   Html_pop_tag(html, tag, tagsize);
   a_Dw_page_add_break(DW_PAGE (html->dw), 9,
                       html->stack[(html)->stack_top].style);
}

/*
 * Default close for tags with no close (e.g. <br>) - do nothing
 */
static void Html_tag_close_nop(DilloHtml *html, char *tag, gint tagsize)
{
}


/*
 * Function index for the open and close functions for each tag
 * (Alphabetically sorted for a binary search)
 */
typedef struct {
   gchar *name;
   TagFunct open, close;
} TagInfo;

static const TagInfo Tags[] = {
  {"a", Html_tag_open_a, Html_tag_close_a},
  /* abbr */
  /* acronym */
  {"address", Html_tag_open_i, Html_tag_close_par},
  {"area", Html_tag_open_area, Html_tag_close_nop},
  {"b", Html_tag_open_b, Html_tag_close_default},
  {"base", Html_tag_open_base, Html_tag_close_nop},
  {"big", Html_tag_open_big_small, Html_tag_close_default},
  {"blockquote", Html_tag_open_blockquote, Html_tag_close_par},
  {"body", Html_tag_open_body, Html_tag_close_default},
  {"br", Html_tag_open_br, Html_tag_close_nop},
  /* button */
  /* caption */
  {"center", Html_tag_open_center, Html_tag_close_div},
  {"cite", Html_tag_open_cite, Html_tag_close_default},
  {"code", Html_tag_open_code, Html_tag_close_default},
  /* col */
  /* colgroup */
  {"dd", Html_tag_open_dd, Html_tag_close_par},
  {"del", Html_tag_open_strike, Html_tag_close_default},
  {"dfn", Html_tag_open_dfn, Html_tag_close_default},
  {"div", Html_tag_open_div, Html_tag_close_div},         // todo: complete!
  {"dl", Html_tag_open_dl, Html_tag_close_par},
  {"dt", Html_tag_open_dt, Html_tag_close_par},
  {"em", Html_tag_open_em, Html_tag_close_default},
  /* fieldset */
  {"font", Html_tag_open_font, Html_tag_close_default},
  {"form", Html_tag_open_form, Html_tag_close_form},
  {"frame", Html_tag_open_frame, Html_tag_close_nop},
  {"frameset", Html_tag_open_frameset, Html_tag_close_default},
  {"h1", Html_tag_open_h, Html_tag_close_h},
  {"h2", Html_tag_open_h, Html_tag_close_h},
  {"h3", Html_tag_open_h, Html_tag_close_h},
  {"h4", Html_tag_open_h, Html_tag_close_h},
  {"h5", Html_tag_open_h, Html_tag_close_h},
  {"h6", Html_tag_open_h, Html_tag_close_h},
  {"head", Html_tag_open_head, Html_tag_close_head},
  {"hr", Html_tag_open_hr, Html_tag_close_nop},
  /* html */
  {"i", Html_tag_open_i, Html_tag_close_default},
  {"img", Html_tag_open_img, Html_tag_close_nop},
  {"input", Html_tag_open_input, Html_tag_close_nop},
  /* ins */
  {"isindex", Html_tag_open_isindex, Html_tag_close_nop},
  {"kbd", Html_tag_open_kbd, Html_tag_close_default},
  /* label */
  /* legend */
  {"li", Html_tag_open_li, Html_tag_close_default},
  /* link */
  {"map", Html_tag_open_map, Html_tag_close_map},
  {"meta", Html_tag_open_meta, Html_tag_close_nop},
  /* noframes */
  /* noscript */
  /* object */
  {"ol", Html_tag_open_ol, Html_tag_close_par},
  /* optgroup */
  {"option", Html_tag_open_option, Html_tag_close_nop},
  {"p", Html_tag_open_p, Html_tag_close_par},
  /* param */
  {"pre", Html_tag_open_pre, Html_tag_close_par},
  /* q */
  {"s", Html_tag_open_strike, Html_tag_close_default},
  {"samp", Html_tag_open_samp, Html_tag_close_default},
  {"script", Html_tag_open_script, Html_tag_close_script},
  {"select", Html_tag_open_select, Html_tag_close_select},
  {"small", Html_tag_open_big_small, Html_tag_close_default},
  /* span */
  {"strike", Html_tag_open_strike, Html_tag_close_default},
  {"strong", Html_tag_open_strong, Html_tag_close_default},
  {"style", Html_tag_open_style, Html_tag_close_style},
  {"sub", Html_tag_open_sub, Html_tag_close_default},
  {"sup", Html_tag_open_sup, Html_tag_close_default},
  {"table", Html_tag_open_table, Html_tag_close_par},
  /* tbody */
  {"td", Html_tag_open_td, Html_tag_close_default},
  {"textarea", Html_tag_open_textarea, Html_tag_close_textarea},
  /* tfoot */
  {"th", Html_tag_open_th, Html_tag_close_default},
  /* thead */
  {"title", Html_tag_open_title, Html_tag_close_title},
  {"tr", Html_tag_open_tr, Html_tag_close_nop},
  {"tt", Html_tag_open_tt, Html_tag_close_default},
  {"u", Html_tag_open_u, Html_tag_close_default},
  {"ul", Html_tag_open_ul, Html_tag_close_par},
  {"var", Html_tag_open_var, Html_tag_close_default}
};
#define NTAGS (sizeof(Tags)/sizeof(Tags[0]))


/*
 * Compares tag from buffer ('/' or '>' or space-ended string) [p1]
 * with tag from taglist (lowercase, zero ended string) [p2]
 * Return value: as strcmp()
 */
static gint Html_tag_compare(char *p1, char *p2)
{
   while ( *p2 ) {
      if ( tolower(*p1) != *p2 )
         return(tolower(*p1) - *p2);
      ++p1;
      ++p2;
   }
   return !strchr(" >/\n\r\t", *p1);
}

/*
 * Get 'tag' index
 * return -1 if tag is not handled yet
 */
static gint Html_tag_index(char *tag)
{
   gint low, high, mid, cond;

   /* Binary search */
   low = 0;
   high = NTAGS - 1;          /* Last tag index */
   while (low <= high) {
      mid = (low + high) / 2;
      if ((cond = Html_tag_compare(tag, Tags[mid].name)) < 0 )
         high = mid - 1;
      else if (cond > 0)
         low = mid + 1;
      else
         return mid;
   }
   return -1;
}

/*
 * Process a tag, given as 'tag' and 'tagsize'.
 * ('tag' must include the enclosing angle brackets)
 * This function calls the right open or close function for the tag.
 */
static void Html_process_tag(DilloHtml *html, char *tag, gint tagsize)
{
   gint i;
   char *start;

   /* discard the '<' */
   start = tag + 1;
   /* discard junk */
   while ( isspace(*start) )
      ++start;

   i = Html_tag_index(start + (*start == '/'));
   if (i != -1) {
      if (*start != '/') {
         /* Open function */
         Tags[i].open (html, tag, tagsize);
      } else {
         /* Close function */
         Tags[i].close (html, tag, tagsize);
      }
   } else {
      /* tag not working - just ignore it */
   }
}

/*
 * Get attribute value for 'attrname' and return it.
 *  Tags start with '<' and end with a '>' (Ex: "<P align=center>")
 *  tagsize = strlen(tag) from '<' to '>', inclusive.
 *
 * Returns one of the following:
 *    * The value of the attribute.
 *    * An empty string if the attribute exists but has no value.
 *    * NULL if the attribute doesn't exist.
 */
static const char *Html_get_attr2(DilloHtml *html,
                                  const char *tag,
                                  gint tagsize,
                                  const char *attrname,
                                  DilloHtmlTagParsingFlags flags)
{
   gint i, isocode, Found = 0, delimiter = 0, attr_pos = 0;
   GString *Buf = html->attr_data;
   DilloHtmlTagParsingState state = SEEK_ATTR_START;

   g_return_val_if_fail(*attrname, NULL);

   g_string_truncate(Buf, 0);

   for (i = 1; i < tagsize; ++i) {
      switch (state) {
      case SEEK_ATTR_START:
         if (isspace(tag[i]))
            state = SEEK_TOKEN_START;
         else if (tag[i] == '=')
            state = SEEK_VALUE_START;
         break;

      case MATCH_ATTR_NAME:
         if ((Found = (!(attrname[attr_pos]) &&
                       (tag[i] == '=' || isspace(tag[i]) || tag[i] == '>')))) {
            state = SEEK_TOKEN_START;
            --i;
         } else if (tolower(tag[i]) != tolower(attrname[attr_pos++]))
            state = SEEK_ATTR_START;
         break;

      case SEEK_TOKEN_START:
         if (tag[i] == '=') {
            state = SEEK_VALUE_START;
         } else if (!isspace(tag[i])) {
            attr_pos = 0;
            state = (Found) ? FINISHED : MATCH_ATTR_NAME;
            --i;
         }
         break;
      case SEEK_VALUE_START:
         if (!isspace(tag[i])) {
            delimiter = (tag[i] == '"' || tag[i] == '\'') ? tag[i] : ' ';
            i -= (delimiter == ' ');
            state = (Found) ? GET_VALUE : SKIP_VALUE;
         }
         break;

      case SKIP_VALUE:
         if ((delimiter == ' ' && isspace(tag[i])) || tag[i] == delimiter)
            state = SEEK_TOKEN_START;
         break;
      case GET_VALUE:
         if ((delimiter == ' ' && (isspace(tag[i]) || tag[i] == '>')) ||
             tag[i] == delimiter) {
            state = FINISHED;
         } else if (tag[i] == '&' && (flags & HTML_ParseEntities)) {
            if ((isocode = Html_parse_entity(tag+i, tagsize-i)) != -1) {
               g_string_append_c(Buf, (gchar) isocode);
               while(tag[++i] != ';');
            } else {
               g_string_append_c(Buf, tag[i]);
            }
         } else if (tag[i] == '\r' || tag[i] == '\t') {
            g_string_append_c(Buf, ' ');
         } else if (tag[i] == '\n') {
            /* ignore */
         } else {
            g_string_append_c(Buf, tag[i]);
         }
         break;

      case FINISHED:
         i = tagsize;
         break;
      }
   }

   return (Found) ? Buf->str : NULL;
}

/*
 * Call Html_get_attr2 telling it to parse entities and strip the result
 */
static const char *Html_get_attr(DilloHtml *html,
                                 const char *tag,
                                 gint tagsize,
                                 const char *attrname)
{
   return Html_get_attr2(html, tag, tagsize, attrname,
                         HTML_LeftTrim | HTML_RightTrim | HTML_ParseEntities);
}

/*
 * Add a widget to the page.
 */
static void Html_add_widget(DilloHtml *html,
                            DwWidget *widget,
                            char *width_str,
                            char *height_str,
                            DwStyle *style_attrs)
{
   DwStyle new_style_attrs, *style;

   new_style_attrs = *style_attrs;
   new_style_attrs.width = width_str ?
      Html_parse_length (width_str) : DW_STYLE_UNDEF_LENGTH;
   new_style_attrs.height = height_str ?
      Html_parse_length (height_str) : DW_STYLE_UNDEF_LENGTH;
   style = a_Dw_style_new (&new_style_attrs, (html)->bw->main_window->window);
   a_Dw_page_add_widget(DW_PAGE (html->dw), widget, style);
   a_Dw_style_unref (style);
}


/*
 * Dispatch the apropriate function for 'Op'
 * This function is a Cache client and gets called whenever new data arrives
 *  Op      : operation to perform.
 *  CbData  : a pointer to a DilloHtml structure
 *  Buf     : a pointer to new data
 *  BufSize : new data size (in bytes)
 */
static void Html_callback(int Op, CacheClient_t *Client)
{
   if ( Op ) {
      Html_write(Client->CbData, Client->Buf, Client->BufSize, 1);
      Html_close(Client->CbData, Client->Key);
   } else
      Html_write(Client->CbData, Client->Buf, Client->BufSize, 0);
}

/*
 * Here's where we parse the html and put it into the page structure.
 * (This function is called by Html_callback whenever there's new data)
 */
static void Html_write(DilloHtml *html, char *Buf, gint BufSize, gint Eof)
{
   char ch = 0, *p;
   DwPage *page;
   char completestr[32];
   gint token_start, buf_index;
   char *buf = Buf + html->Start_Ofs;
   gint bufsize = BufSize - html->Start_Ofs;

   g_return_if_fail ( (page = DW_PAGE (html->dw)) != NULL );

   buf = g_strndup(buf, bufsize);
   /*a_Dw_page_update_begin(page);*/

   /* Now, 'buf' and 'bufsize' define a buffer aligned to start at a token
    * boundary. Iterate through tokens until end of buffer is reached. */
   buf_index = 0;
   token_start = buf_index;
   while (buf_index < bufsize) {
      /* invariant: buf_index == bufsize || token_start == buf_index */

      if ( isspace(buf[buf_index]) ) {
         /* whitespace: group all available whitespace */
         while (++buf_index < bufsize && isspace(buf[buf_index]));
         Html_process_space(html, buf + token_start, buf_index - token_start);
         token_start = buf_index;

      } else if (buf[buf_index] == '<' && (ch = buf[buf_index + 1]) &&
                 (isalpha(ch) || strchr("/!?", ch)) ) {
         /* Tag */
         if (buf_index + 3 < bufsize && !strncmp(buf + buf_index, "<!--", 4)) {
            /* Comment: search for close of comment, skipping over
             * everything except a matching "-->" tag. */
            while ( (p = memchr(buf + buf_index, '>', bufsize - buf_index)) ){
               buf_index = p - buf + 1;
               if ( p[-1] == '-' && p[-2] == '-' ) break;
            }
            if ( p ) {
               /* Got the whole comment. If you ever want to process it,
                * put code here. You'll need to do this for J*Script and
                * style sheets. */
               token_start = buf_index;
            } else
               buf_index = bufsize;
         } else {
            /* Tag: search end of tag (skipping over quoted strings) */
            while ( buf_index < bufsize ) {
               buf_index++;
               buf_index += strcspn(buf + buf_index, ">\"'");
               if ( (ch = buf[buf_index]) == '>' ) {
                  break;
               } else if ( ch == '"' || ch == '\'' ) {
                  /* Skip over quoted string */
                  buf_index++;
                  buf_index += strcspn(buf + buf_index,
                                       (ch == '"') ? "\">" : "'>");
                  if ( buf[buf_index] == '>' )
                     /* Unterminated string value */
                     break;
               }
            }
            if (buf_index < bufsize) {
               buf_index++;
               Html_process_tag(html, buf + token_start,
                                buf_index - token_start);
               token_start = buf_index;
            }
         }
      } else {
         /* A Word: search for whitespace or tag open */
         while (++buf_index < bufsize) {
            buf_index += strcspn(buf + buf_index, " <\n\r\t\f\v");
            if ( buf[buf_index] == '<' && (ch = buf[buf_index + 1]) &&
                 !isalpha(ch) && !strchr("/!?", ch))
               continue;
            break;
         }
         if (buf_index < bufsize || Eof) {
            /* successfully found end of token */
            Html_process_word(html, buf + token_start,
                              buf_index - token_start);
            token_start = buf_index;
         }
      }
   }/*while*/

   html->Start_Ofs += token_start;

   if ( html->bw ) {
      sprintf(completestr,"%s%.1f Kb", PBAR_PSTR(prefs.panel_size == 1),
              (float)html->Start_Ofs/1024);
      a_Progressbar_update(html->bw->progress, completestr, 1);
   }

   a_Dw_page_flush(page);
   g_free(buf);
}

/*
 * Finish parsing a HTML page
 * (Free html struct, close the client and update progressbar).
 */
static void Html_close(DilloHtml *html, gint ClientKey)
{
   gint i;

   for (i = 0; i <= html->stack_top; i++) {
      g_free(html->stack[i].tag);
      a_Dw_style_unref (html->stack[i].style);
      if (html->stack[i].table_cell_style)
         a_Dw_style_unref (html->stack[i].table_cell_style);
   }
   g_free(html->stack);

   g_string_free(html->Stash, TRUE);
   g_string_free(html->attr_data, TRUE);

   /* Remove this client from our active list */
   a_Interface_close_client(html->bw, ClientKey);

   /* Set progress bar insensitive */
   a_Progressbar_update(html->bw->progress, NULL, 0);

   g_free(html);
}


