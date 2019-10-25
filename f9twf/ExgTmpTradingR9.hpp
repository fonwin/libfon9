// \file f9twf/ExgTmpTradingR9.hpp
//
// TMP封包: 報價要求R09/R39; 報價回報使用: (R02/R32) * 2(Bid/Offer).
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgTmpTradingR9_hpp__
#define __f9twf_ExgTmpTradingR9_hpp__
#include "f9twf/ExgTmpTradingR1.hpp"

namespace f9twf {

struct TmpR9Front : public TmpHeader, public TmpR1BfSym {
   struct BackType {
      TmpPrice       BidPx_;
      TmpPrice       OfferPx_;
      TmpQty         BidSize_;
      TmpQty         OfferSize_;
      TmpIvacNo      IvacNo_;
      TmpIvacFlag    IvacFlag_;
      TmpTimeInForce TimeInForce_;
      TmpPosEff      PosEff_;
      TmpSource      Source_;
      TmpCheckSum    CheckSum_;
   };
};
using TmpR9Back = TmpR9Front::BackType;
using TmpR09 = TmpTriDef<TmpR9Front, TmpSymIdS>;
using TmpR39 = TmpTriDef<TmpR9Front, TmpSymIdL>;

static_assert(sizeof(TmpR09) ==  84, "struct TmpR09; must pack?");
static_assert(sizeof(TmpR39) == 104, "struct TmpR39; must pack?");

} // namespaces
#endif//__f9twf_ExgTmpTradingR9_hpp__
