// \file f9twf/ExgTmpTradingR7.hpp
//
// TMP封包: 詢價要求R07/R37; 詢價回報R08/R38.
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgTmpTradingR7_hpp__
#define __f9twf_ExgTmpTradingR7_hpp__
#include "f9twf/ExgTmpTypes.hpp"

namespace f9twf {

struct TmpR7BfSym {
   OrdNo          OrderNo_;
   TmpOrdId       OrdId_;
   TmpFcmId       IvacFcmId_;
   TmpSymbolType  SymbolType_;
};
struct TmpR7Front : public TmpHeader, public TmpR7BfSym {
   struct BackType {
      TmpSource   Source_;
      TmpCheckSum CheckSum_;
   };
};
using TmpR7Back = TmpR7Front::BackType;
using TmpR07 = TmpTriDef<TmpR7Front, TmpSymIdS>;
using TmpR37 = TmpTriDef<TmpR7Front, TmpSymIdL>;

static_assert(sizeof(TmpR07) == 54, "struct TmpR07; must pack?");
static_assert(sizeof(TmpR37) == 74, "struct TmpR37; must pack?");

//--------------------------------------------------------------------------//

struct TmpR8Front : public TmpHeaderSt, public TmpR7BfSym {
   struct BackType {
      TmpCheckSum    CheckSum_;
   };
};
using TmpR8Back = TmpR8Front::BackType;
using TmpR08 = TmpTriDef<TmpR8Front, TmpSymIdS>;
using TmpR38 = TmpTriDef<TmpR8Front, TmpSymIdL>;

static_assert(sizeof(TmpR08) == 51, "struct TmpR08; must pack?");
static_assert(sizeof(TmpR38) == 71, "struct TmpR38; must pack?");

} // namespaces
#endif//__f9twf_ExgTmpTradingR7_hpp__
