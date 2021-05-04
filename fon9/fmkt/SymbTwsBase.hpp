// \file fon9/fmkt/SymbTwsBase.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbTwsBase_hpp__
#define __fon9_fmkt_SymbTwsBase_hpp__
#include "fon9/seed/Tab.hpp"

namespace fon9 { namespace fmkt {

enum class TwsBaseFlag : uint8_t {
   MatchingMethodMask = 0x03,
   /// 逐筆撮合.
   ContinuousMarket = 0x01,
   /// 集合競價.
   AggregateAuction = 0x02,
};
fon9_ENABLE_ENUM_BITWISE_OP(TwsBaseFlag);
/// 傳回:
/// TwsBaseFlag::ContinuousMarket = 逐筆撮合;
/// TwsBaseFlag::AggregateAuction = 集合競價;
/// TwsBaseFlag{} 尚未提供此資訊;
inline TwsBaseFlag GetMatchingMethod(TwsBaseFlag f) {
   return f & TwsBaseFlag::MatchingMethodMask;
}
enum class StkCTGCD : char {
   /// 一般板.
   Normal = '\0',
   /// 上市創新板.
   Innovation = '3',
   /// 上櫃戰略板.
   Pioneer = '3'
};

/// \ingroup fmkt
/// 台灣證券預設的商品基本資料.
/// - ShUnit
struct fon9_API SymbTwsBase {
   /// 台灣證券「一張」的單位數(股數)
   uint32_t    ShUnit_{0};
   TwsBaseFlag TwsFlags_{};
   StkCTGCD    StkCTGCD_{};
   char        Padding___[2];

   void SymbTwsBaseDailyClear() {
   }
   void SymbTwsBaseSessionClear() {
   }

   template <class Derived>
   static void AddFields(seed::Fields& flds) {
      AddFields(fon9_OffsetOfBase(Derived, SymbTwsBase), flds);
   }
   static void AddFields(int ofsadj, seed::Fields& flds);
};

/// 取得台灣證券的整股交易單位, 若沒設定, 則 1張 = 1000股.
inline uint32_t GetSymbTwsShUnit(const SymbTwsBase* symb) {
   if (symb && 0 < symb->ShUnit_)
      return symb->ShUnit_;
   return 1000;
}

} } // namespaces
#endif//__fon9_fmkt_SymbTwsBase_hpp__
