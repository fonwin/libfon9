// \file fon9/fmkt/SymbTwfBase.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbTwfBase_hpp__
#define __fon9_fmkt_SymbTwfBase_hpp__
#include "fon9/seed/Tab.hpp"

namespace fon9 { namespace fmkt {

using SymbFlowGroup_t = uint8_t;
using SymbSeqNo_t = uint16_t;

/// \ingroup fmkt
/// 台灣期交所預設的商品基本資料.
struct SymbTwfBase {
   /// 下市日期.
   uint32_t EndYYYYMMDD_{0};

   /// 交易所的價格欄位, 若沒有定義小數位, 則使用 PriceOrigDiv_ 來處理;
   /// - 例如: 期交所Tmp格式的下單、回報, 都需要用 PriceOrigDiv_ 來決定小數位:
   ///   - 若 TaiFexPri = Decimal<int64_t,9>;
   ///   - 期交所 P08 的 decimal_locator = 2;
   ///   - 則 PriceOrigDiv_ = 10^7;
   uint32_t PriceOrigDiv_{0};
   /// 選擇權商品代碼, 履約價格部份之小數調整.
   /// - 例如: "TEO03200L9" 電子 320.0 買權;
   ///   此時商品Id的履約價:小數1位; 則 StrikePriceDiv_ = 10;
   uint32_t StrikePriceDiv_{0};

   /// 期交所定義的商品序號.
   SymbSeqNo_t ExgSymbSeq_{0};

   /// 台灣期交所的流程群組代碼.
   SymbFlowGroup_t   FlowGroup_{0};

   char  Padding__[1];

   template <class Derived>
   static void AddFields(seed::Fields& flds) {
      AddFields(fon9_OffsetOfBase(Derived, SymbTwfBase), flds);
   }
   static void AddFields(int ofsadj, seed::Fields& flds);
};

} } // namespaces
#endif//__fon9_fmkt_SymbTwfBase_hpp__
