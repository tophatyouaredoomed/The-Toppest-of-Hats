#ifndef __HTML_H__
#define __HTML_H__

#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>

#include "browser.h"         /* for BrowserWindow */
#include "dw_widget.h"       /* for DwWidget */
#include "dw_image.h"        /* for DwImageMapList */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* First, the html linkblock. For now, this mostly has forms, although
   pointers to actual links will go here soon, if for no other reason
   than to implement history-sensitive link colors. Also, it seems
   likely that imagemaps will go here. */

typedef struct _DilloHtmlLB      DilloHtmlLB;

typedef struct _DilloHtml        DilloHtml;
typedef struct _DilloHtmlClass   DilloHtmlClass;
typedef struct _DilloHtmlState   DilloHtmlState;
typedef struct _DilloHtmlForm    DilloHtmlForm;
typedef struct _DilloHtmlOption  DilloHtmlOption;
typedef struct _DilloHtmlSelect  DilloHtmlSelect;
typedef struct _DilloHtmlInput   DilloHtmlInput;


struct _DilloHtmlLB {
  BrowserWindow *bw;
  DilloUrl *base_url;

  DilloHtmlForm *forms;
  gint num_forms;
  gint num_forms_max;

  DilloUrl **links;
  gint num_links;
  gint num_links_max;

  DwImageMapList maps;
  gboolean map_open;

  gint32 link_color;
  gint32 visited_color;
};


typedef enum {
  DILLO_HTML_PARSE_MODE_INIT,
  DILLO_HTML_PARSE_MODE_STASH,
  DILLO_HTML_PARSE_MODE_STASH_AND_BODY,
  DILLO_HTML_PARSE_MODE_SCRIPT,
  DILLO_HTML_PARSE_MODE_BODY,
  DILLO_HTML_PARSE_MODE_VERBATIM,
  DILLO_HTML_PARSE_MODE_PRE
} DilloHtmlParseMode;

typedef enum {
  SEEK_ATTR_START,
  MATCH_ATTR_NAME,
  SEEK_TOKEN_START,
  SEEK_VALUE_START,
  SKIP_VALUE,
  GET_VALUE,
  FINISHED
} DilloHtmlTagParsingState;

typedef enum {
  HTML_LeftTrim      = 1 << 0,
  HTML_RightTrim     = 1 << 1,
  HTML_ParseEntities = 1 << 2,
} DilloHtmlTagParsingFlags;

typedef enum {
  DILLO_HTML_TABLE_MODE_NONE,  /* no table at all */
  DILLO_HTML_TABLE_MODE_TOP,   /* outside of <tr> */
  DILLO_HTML_TABLE_MODE_TR,    /* inside of <tr>, outside of <td> */
  DILLO_HTML_TABLE_MODE_TD     /* inside of <td> */
} DilloHtmlTableMode;


typedef enum {
  DILLO_HTML_IN_HTML,
  DILLO_HTML_IN_HEAD,
  DILLO_HTML_IN_BODY
} DilloHtmlProcessingState;


struct _DilloHtmlState {
  char *tag;
  DwStyle *style, *table_cell_style;
  DilloHtmlParseMode parse_mode;
  DilloHtmlTableMode table_mode;
  gint list_level;
  gint list_number;
  DwWidget *page, *table;

   /* This is used to align list items (especially in enumerated lists) */
   DwWidget *ref_list_item;

   /* This makes image processing faster than a function
      a_Dw_widget_get_background_color. */
   gint32 current_bg_color;

   /* This is used for list items etc; if it is set to TRUE, breaks
      have to be "handed over" (see Html_add_indented and
      Html_eventually_pop_dw). */
   gboolean hand_over_break;
};

typedef enum {
  DILLO_HTML_METHOD_UNKNOWN,
  DILLO_HTML_METHOD_GET,
  DILLO_HTML_METHOD_POST
} DilloHtmlMethod;

typedef enum {
  DILLO_HTML_ENC_URLENCODING
} DilloHtmlEnc;

struct _DilloHtmlForm {
  DilloHtmlMethod method;
  DilloUrl *action;
  DilloHtmlEnc enc;

  DilloHtmlInput *inputs;
  gint num_inputs;
  gint num_inputs_max;
  gboolean HasSubmitButton;  /* Submit on enterpress if it doesn't have */
};

struct _DilloHtmlOption {
  GtkWidget *menuitem;
  char *value;
  gboolean init_val;
};

struct _DilloHtmlSelect {
  GtkWidget *menu;
  gint size;

  DilloHtmlOption *options;
  gint num_options;
  gint num_options_max;
};

typedef enum {
  DILLO_HTML_INPUT_TEXT,
  DILLO_HTML_INPUT_PASSWORD,
  DILLO_HTML_INPUT_CHECKBOX,
  DILLO_HTML_INPUT_RADIO,
  DILLO_HTML_INPUT_IMAGE,
  DILLO_HTML_INPUT_FILE,
  DILLO_HTML_INPUT_BUTTON,
  DILLO_HTML_INPUT_HIDDEN,
  DILLO_HTML_INPUT_SUBMIT,
  DILLO_HTML_INPUT_RESET,
  DILLO_HTML_INPUT_SELECT,
  DILLO_HTML_INPUT_SEL_LIST,
  DILLO_HTML_INPUT_TEXTAREA,
  DILLO_HTML_INPUT_INDEX
} DilloHtmlInputType;

struct _DilloHtmlInput {
  DilloHtmlInputType type;
  GtkWidget *widget;
  char *name;
  char *init_str;    /* note: some overloading - for buttons, init_str
                        is simply the value of the button; for text
                        entries, it is the initial value */
  DilloHtmlSelect *select;
  gboolean init_val; /* only meaningful for buttons */
};

struct _DilloHtml {
  DwWidget *dw;          /* this is duplicated in the stack (page) */

  DilloHtmlLB *linkblock;
  size_t Start_Ofs;

  DilloHtmlState *stack;
  gint stack_top;        /* Index to the top of the stack [0 based] */
  gint stack_max;

  DilloHtmlProcessingState InTag; /* where in the document we are */

  GString *Stash;
  gboolean StashSpace;

  gboolean PrevWasCR;     /* Flag to help parsing of "\r\n" in PRE tags */
  gboolean InForm;
  gboolean InSelect;
  gboolean InVisitedLink; /* Used to 'force_visited_colors' */

  GString *attr_data;

  BrowserWindow *bw;
};

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __HTML_H__ */
