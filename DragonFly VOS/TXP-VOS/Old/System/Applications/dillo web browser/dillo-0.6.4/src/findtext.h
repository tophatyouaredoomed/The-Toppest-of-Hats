#ifndef __FINDTEXT_H__
#define __FINDTEXT_H__

#include <ctype.h>
#include <glib.h>
#include "dw_widget.h"

/*
 * return values
 */
#define FT_None      0
#define FT_Partial   1
#define FT_Success   2


typedef enum {
  F_NewKey,
  F_Seek,
  F_GetPos,
  F_Found,
  F_End
} FindState;

typedef enum {
  M_FirstWord,
  M_NextWord,
  M_Read,
  M_NewText,
  M_Found
} ParserState;

typedef enum {
  FT_FullWord,
  FT_FreeSubStr,
  FT_LeftSubStr,
  FT_RightSubStr
} WordFlag;


typedef struct _WordData WordData;
typedef struct _KeyData  KeyData;
typedef struct _FindData FindData;

struct _WordData {
   GString *Word;
   WordFlag Flag;
};

struct _KeyData {
   gchar *KeyStr;
   GSList *WordList;
   GSList *Buf;
   ParserState State;
   gint Matches;
   gint y_pos;
};

struct _FindData {
   DwWidget *widget;
   KeyData *Key;
   gint WordNum;
   FindState State;
   FindData *next;
};


/*
 * Function prototypes
 */
KeyData *a_Findtext_key_new(gchar *string);
void a_Findtext_key_free(KeyData *Key);
gint a_Findtext_compare(gchar *text, KeyData *Key);


#endif /* __FINDTEXT_H__ */
