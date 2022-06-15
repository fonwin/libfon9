// \file fon9/fmkt/SymbTwfBase.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbTwfBase_hpp__
#define __fon9_fmkt_SymbTwfBase_hpp__
#include "fon9/seed/Tab.hpp"
#include "fon9/Decimal.hpp"

namespace fon9 { namespace fmkt {

using SymbFlowGroup_t = uint8_t;
using SymbSeqNo_t = uint16_t;
using TwfStrikePrice = Decimal<uint32_t, 4>;

/// \ingroup fmkt
/// 台灣期交所預設的商品基本資料.
struct fon9_API SymbTwfBase {
   /// 下市日期(最後可交易日).
   uint32_t             EndYYYYMMDD_{0};
   /// 交易所的價格欄位, 若沒有定義小數位, 則使用 PriceOrigDiv_ 來處理;
   /// - 例如: 期交所Tmp格式的下單、回報, 都需要用 PriceOrigDiv_ 來決定小數位:
   ///   - 若 TaiFexPri = Decimal<int64_t,9>;
   ///   - 期交所 P08 的 decimal_locator = 2;
   ///   - 則 PriceOrigDiv_ = 10^7;
   uint32_t             PriceOrigDiv_{0};
   /// 選擇權商品代碼, 履約價格部份之小數調整.
   /// - 例如: "TEO03200L9" 電子 320.0 買權;
   ///   此時商品Id的履約價:小數1位; 則 StrikePriceDiv_ = 10;
   uint32_t             StrikePriceDiv_{0};
   /// 期交所定義的商品序號.
   SymbSeqNo_t          ExgSymbSeq_{0};
   /// 台灣期交所的流程群組代碼.
   SymbFlowGroup_t      FlowGroup_{0};
   /// 買賣權別 'C':call, 'P':put; '\0':期貨.
   char                 CallPut_;
   /// 交割年月.
   uint32_t             SettleYYYYMM_;
   /// 履約價.
   TwfStrikePrice       StrikePrice_;
   /// 市價單最大委託量, 市價下單數量必須 <= MaxQtyMarket_.
   /// 來自 P08.market_order_ceiling_;
   uint16_t             MaxQtyMarket_{0};
   /// 限價單最大委託量, 限價下單數量必須 <= MaxQtyLimit_.
   /// 期交所規範: https://www.taifex.com.tw/cht/4/oamOrderlimit
   /// 匯入 P08 時, 根據 [P08.prod_kind_ 及 期交所規範] 填入此值.
   uint16_t             MaxQtyLimit_{0};
   /// P08 or I010 提供的價格小數位數.
   uint8_t              PriceDecimalLocator_{0};
   char                 Padding____[3];

   void SymbTwfBaseClear() {
      this->FlowGroup_ = 0;
      this->ExgSymbSeq_ = 0;
   }
   void SymbTwfBaseDailyClear() {
      this->SymbTwfBaseClear();
   }
   void SymbTwfBaseSessionClear() {
      this->SymbTwfBaseClear();
   }

   template <class Derived>
   static void AddFields(seed::Fields& flds) {
      AddFields(fon9_OffsetOfBase(Derived, SymbTwfBase), flds);
   }
   static void AddFields(int ofsadj, seed::Fields& flds);
};

} } // namespaces
#endif//__fon9_fmkt_SymbTwfBase_hpp__
