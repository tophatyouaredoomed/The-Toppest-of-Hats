#ifndef __CHAIN_H__
#define __CHAIN_H__

/*
 * Concomitant control chain (CCC)
 * by Jorge Arellano Cid <jcid@inf.utfsm.cl>
 */

#include <glib.h>

/*
 * Supported CCC operations
 */
#define OpStart  1
#define OpSend   2
#define OpStop   3
#define OpEnd    4
#define OpAbort  5

/*
 * Linking direction
 */
#define CCC_FWD 1
#define CCC_BCK 2


typedef struct _ChainLink ChainLink;
typedef void (*ChainFunction_t)(int Op, int Branch, ChainLink *Info,
                                void *Data, void *ExtraData);

struct _ChainLink {
   ChainFunction_t Fcb;
   ChainLink *FcbInfo;
   ChainFunction_t Bcb;
   ChainLink *BcbInfo;
   void *LocalKey;
};

/*
 * Function prototypes
 */
ChainLink *a_Chain_new(void);
ChainLink *a_Chain_link_new(ChainFunction_t Funct, ChainLink *FuncData,
                            gint Direction, ChainFunction_t ModFunct);
void a_Chain_del_link(ChainLink *Info, gint Direction);
gint a_Chain_fcb(int Op, int Branch, ChainLink *Info,
                 void *Data, void *ExtraData);
gint a_Chain_bcb(int Op, int Branch, ChainLink *Info,
                 void *Data, void *ExtraData);


/*
 * CCC functions of subscribing modules
 */
void a_Cache_ccc(int Op, int Br, ChainLink *Info, void *Data, void *ExtraData);

#endif /* __CHAIN_H__ */
