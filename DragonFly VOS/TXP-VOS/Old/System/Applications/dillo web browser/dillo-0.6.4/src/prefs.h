#ifndef __PREFS_H__
#define __PREFS_H__

#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define D_GEOMETRY_DEFAULT_WIDTH   640
#define D_GEOMETRY_DEFAULT_HEIGHT  550

#define DW_COLOR_DEFAULT_GREY   0xd6d6d6
#define DW_COLOR_DEFAULT_BLACK  0x000000
#define DW_COLOR_DEFAULT_BLUE   0x0000ff
#define DW_COLOR_DEFAULT_PURPLE 0x800080
#define DW_COLOR_DEFAULT_BGND   0xd6d6c0

/* define enumeration values to be returned */
enum {
   PARSE_OK = 0,
   FILE_NOT_FOUND
};

/* define enumeration values to be returned for specific symbols */
typedef enum {
   DRC_TOKEN_FIRST = G_TOKEN_LAST,
   DRC_TOKEN_GEOMETRY,
   DRC_TOKEN_PROXY,
   DRC_TOKEN_NOPROXY,
   DRC_TOKEN_LINK_COLOR,
   DRC_TOKEN_VISITED_COLOR,
   DRC_TOKEN_BG_COLOR,
   DRC_TOKEN_ALLOW_WHITE_BG,
   DRC_TOKEN_FORCE_MY_COLORS,
   DRC_TOKEN_FORCE_VISITED_COLOR,
   DRC_TOKEN_TEXT_COLOR,
   DRC_TOKEN_USE_OBLIQUE,
   DRC_TOKEN_HOME,
   DRC_TOKEN_PANEL_SIZE,
   DRC_TOKEN_SMALL_ICONS,
   DRC_TOKEN_FONT_FACTOR,
   DRC_TOKEN_SHOW_ALT,
   DRC_TOKEN_LIMIT_TEXT_WIDTH,
   DRC_TOKEN_USE_DICACHE,
   DRC_TOKEN_SHOW_BACK,
   DRC_TOKEN_SHOW_FORW,
   DRC_TOKEN_SHOW_HOME,
   DRC_TOKEN_SHOW_RELOAD,
   DRC_TOKEN_SHOW_SAVE,
   DRC_TOKEN_SHOW_STOP,
   DRC_TOKEN_SHOW_MENUBAR,
   DRC_TOKEN_SHOW_CLEAR_URL,
   DRC_TOKEN_SHOW_URL,
   DRC_TOKEN_SHOW_PROGRESS_BOX,
   DRC_TOKEN_TRANSIENT_DIALOGS,

   DRC_TOKEN_LAST
} Dillo_Rc_TokenType;

typedef struct _DilloPrefs DilloPrefs;

struct _DilloPrefs {
   gint width;
   gint height;
   DilloUrl *http_proxy;
   char *no_proxy;
   char **no_proxy_vec;
   DilloUrl *home;
   guint32 link_color;
   guint32 visited_color;
   guint32 bg_color;
   guint32 text_color;
   gboolean allow_white_bg;
   gboolean use_oblique;
   gboolean force_my_colors;
   gboolean force_visited_color;
   gboolean show_alt;
   gint panel_size;
   gboolean small_icons;
   gboolean limit_text_width;
   gdouble font_factor;
   gboolean use_dicache;
   gboolean show_back;
   gboolean show_forw;
   gboolean show_home;
   gboolean show_reload;
   gboolean show_save;
   gboolean show_stop;
   gboolean show_menubar;
   gboolean show_clear_url;
   gboolean show_url;
   gboolean show_progress_box;
   gboolean transient_dialogs;
};

/* Global Data */
DilloPrefs prefs;

void a_Prefs_init(void);
void a_Prefs_freeall(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PREFS_H__ */
