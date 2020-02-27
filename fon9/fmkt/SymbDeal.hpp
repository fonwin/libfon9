﻿// \file fon9/fmkt/SymbDeal.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbDeal_hpp__
#define __fon9_fmkt_SymbDeal_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 { namespace fmkt {

enum class DealFlag : uint8_t {
   /// 試撮價格訊息.
   /// 何時可能會有試撮? 開盤前, 收盤前, 瞬間漲跌幅過大暫停交易期間(保險絲熔斷機制)...
   Calculated = 0x01,

   /// 成交時間有異動.
   DealTimeChanged = 0x02,
   /// DealBuyCnt 有異動.
   DealBuyCntChanged = 0x04,
   /// DealSellCnt 有異動.
   DealSellCntChanged = 0x08,

   /// TotalQty 不連續, 藉此判斷是否有遺漏成交明細.
   TotalQtyLost = 0x10,
};
fon9_ENABLE_ENUM_BITWISE_OP(DealFlag);

/// \ingroup fmkt
/// 商品資料的擴充: 行情的一筆成交資料.
class fon9_API SymbDeal : public SymbData {
   fon9_NON_COPY_NON_MOVE(SymbDeal);
public:
   struct Data {
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
      DealFlag Flags_{};
      char     Padding___[7];
   };
   Data  Data_;

   SymbDeal(const Data& rhs) : Data_{rhs} {
   }
   SymbDeal() = default;

   void DailyClear() {
      memset(&this->Data_, 0, sizeof(this->Data_));
      this->Data_.InfoTime_.AssignNull();
      this->Data_.DealTime_.AssignNull();
   }
   DayTime DealTime() const {
      return this->Data_.DealTime_.IsNull() ? this->Data_.InfoTime_ : this->Data_.DealTime_;
   }

   static seed::Fields MakeFields();
};

class fon9_API SymbDealTabDy : public SymbDataTab {
   fon9_NON_COPY_NON_MOVE(SymbDealTabDy);
   using base = SymbDataTab;
public:
   SymbDealTabDy(Named&& named)
      : base{std::move(named), SymbDeal::MakeFields(), seed::TabFlag::NoSapling_NoSeedCommand_Writable} {
   }

   SymbDataSP FetchSymbData(Symb&) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbDeal_hpp__
