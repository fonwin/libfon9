// \file fon9/fmkt/SymbDealData.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbDealData_hpp__
#define __fon9_fmkt_SymbDealData_hpp__
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/fmkt/FmdTypes.hpp"

namespace fon9 { namespace fmkt {

struct SymbDealData {
   /// 資訊時間(行情系統時間).
   DayTime  InfoTime_{DayTime::Null()};
   /// 交易所的成交時間(撮合系統時間).
   /// 交易所「資訊源」與「成交明細」可能存在時間差,
   /// 若交易所有提供「撮合系統的成交時間」, 則提供此欄位.
   /// 若沒有提供此欄位, 則此欄位為 DayTime::Null();
   DayTime  DealTime_{DayTime::Null()};
   /// 單筆成交價量.
   /// this->Deal_.Qty_ 不一定等於 this->TotalQty_ - prev->TotalQty_;
   /// 因為行情資料可能有漏.
   PriQty   Deal_{};
   /// 累計成交量.
   Qty      TotalQty_{};
   /// 累計買進成交筆數.
   Qty      DealBuyCnt_{};
   /// 累計賣出成交筆數.
   Qty      DealSellCnt_{};

   f9sv_DealFlag Flags_{};

   /// 由資訊來源提供, 若沒提供則為 0, 核心系統「不會主動」與「今日漲跌停」比對.
   f9sv_DealLmtFlag  LmtFlags_;

   char     Padding___[6];
   
   /// 市場行情序號.
   MarketDataSeq  MarketSeq_{ 0 };

   SymbDealData() {
      this->Clear();
   }
   void Clear() {
      ForceZeroNonTrivial(this);
      this->InfoTime_.AssignNull();
      this->DealTime_.AssignNull();
   }
   DayTime DealTime() const {
      return this->DealTime_.IsNull() ? this->InfoTime_ : this->DealTime_;
   }
};

} } // namespaces
#endif//__fon9_fmkt_SymbDealData_hpp__
