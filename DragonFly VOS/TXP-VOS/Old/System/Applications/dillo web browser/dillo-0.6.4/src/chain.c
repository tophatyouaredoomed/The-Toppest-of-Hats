/*
 * File: chain.c
 * Concomitant control chain (CCC)
 * by Jorge Arellano Cid <jcid@inf.utfsm.cl>
 */

#include "chain.h"


/*
 * Create and initialize a new chain-link
 */
ChainLink *a_Chain_new(void)
{
   return g_new0(ChainLink, 1);
}

/*
 * Create and fill a new link with valid data.
 * 'Direction' tells whether to make a forward or backward fill.
 */
ChainLink *a_Chain_link_new(ChainFunction_t Func, ChainLink *FuncInfo,
                            gint Direction, ChainFunction_t ModFunc)
{
   ChainLink *NewLink = a_Chain_new();
   ChainLink *OldLink = FuncInfo;

   if ( Direction == CCC_FWD ) {
      NewLink->Fcb = Func;
      NewLink->FcbInfo = FuncInfo;
      OldLink->Bcb = ModFunc;
      OldLink->BcbInfo = NewLink;

   } else { /* CCC_BCK */
      NewLink->Bcb = Func;
      NewLink->BcbInfo = FuncInfo;
      OldLink->Fcb = ModFunc;
      OldLink->FcbInfo = NewLink;
   }

   return NewLink;
}

/*
 * Destroy a link and update the referer.
 * 'Direction' tells whether to delete the forward or backward link.
 */
void a_Chain_del_link(ChainLink *Info, gint Direction)
{
   ChainLink *DeadLink;

   if ( Direction == CCC_FWD ) {
      DeadLink = Info->FcbInfo;
      Info->Fcb = NULL;
      Info->FcbInfo = NULL;
   } else {       /* CCC_BCK */
      DeadLink = Info->BcbInfo;
      Info->Bcb = NULL;
      Info->BcbInfo = NULL;
   }
   g_free(DeadLink);
}

/*
 * Issue the forward callback of the 'Info' link
 */
gint a_Chain_fcb(int Op, int Branch, ChainLink *Info,
                 void *Data, void *ExtraData)
{
   if ( Info->Fcb ) {
      Info->Fcb(Op, Branch, Info->FcbInfo, Data, ExtraData);
      return 1;
   }
   return 0;
}

/*
 * Issue the backward callback of the 'Info' link
 */
gint a_Chain_bcb(int Op, int Branch, ChainLink *Info,
                 void *Data, void *ExtraData)
{
   if ( Info->Bcb ) {
      Info->Bcb(Op, Branch, Info->BcbInfo, Data, ExtraData);
      return 1;
   }
   return 0;
}

