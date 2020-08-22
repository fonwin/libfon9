// \file fon9/fmkt/SymbBSData.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbBSData_hpp__
#define __fon9_fmkt_SymbBSData_hpp__
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/fmkt/FmdTypes.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 { namespace fmkt {

struct SymbBSData {
   enum {
      /// 買賣價量列表數量.
      kBSCount = 5,
   };

   /// 報價時間.
   DayTime        InfoTime_{DayTime::Null()};
   /// 賣出價量列表, [0]=最佳賣出價量.
   PriQty         Sells_[kBSCount];
   /// 買進價量列表, [0]=最佳買進價量.
   PriQty         Buys_[kBSCount];

   f9sv_BSFlag    Flags_{};

   /// 由資訊來源提供, 若沒提供則為 0, 核心系統不會主動與 今日漲跌停 比對.
   f9sv_BSLmtFlag LmtFlags_{};

   char           Padding___[2];

   /// 市場行情序號.
   MarketDataSeq  MarketSeq_{0};
};

struct SymbTwfBSData : public SymbBSData {
   /// 衍生賣出.
   PriQty   DerivedSell_{};
   /// 衍生買進.
   PriQty   DerivedBuy_{};

   void Clear(DayTime tm = DayTime::Null()) {
      ForceZeroNonTrivial(this);
      this->InfoTime_ = tm;
   }
};

struct SymbTwsBSData : public SymbBSData {
   void Clear(DayTime tm = DayTime::Null()) {
      ForceZeroNonTrivial(this);
      this->InfoTime_ = tm;
   }
};

} } // namespaces
#endif//__fon9_fmkt_SymbBSData_hpp__
