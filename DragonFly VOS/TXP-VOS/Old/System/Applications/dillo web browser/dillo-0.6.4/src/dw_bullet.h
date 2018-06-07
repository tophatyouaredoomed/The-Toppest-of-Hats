#ifndef __DW_BULLET_H__
#define __DW_BULLET_H__

#include "dw_widget.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DW_TYPE_BULLET          (a_Dw_bullet_get_type ())
#define DW_BULLET(obj)          GTK_CHECK_CAST (obj,DW_TYPE_BULLET, DwBullet)
#define DW_BULLET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, DW_TYPE_BULLET, \
                                   DwBulletClass)
#define DW_IS_BULLET(obj)       GTK_CHECK_TYPE (obj, DW_TYPE_BULLET)


typedef struct _DwBullet      DwBullet;
typedef struct _DwBulletClass DwBulletClass;

typedef enum
{
   DW_BULLET_DISC=1,
   DW_BULLET_CIRCLE=2,
   DW_BULLET_SQUARE=3,
   DW_BULLET_1=4,
   DW_BULLET_a=5,
   DW_BULLET_A=6,
   DW_BULLET_i=7,
   DW_BULLET_I=8
} DwBulletType;

struct _DwBullet
{
   DwWidget widget;

   DwBulletType type;
};

struct _DwBulletClass
{
   DwWidgetClass parent_class;
};


GtkType         a_Dw_bullet_get_type    (void);
DwWidget*       a_Dw_bullet_new         (DwBulletType type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DW_BULLET_H__ */
