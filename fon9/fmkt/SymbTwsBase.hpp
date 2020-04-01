// \file fon9/fmkt/SymbTwsBase.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbTwsBase_hpp__
#define __fon9_fmkt_SymbTwsBase_hpp__
#include "fon9/seed/Tab.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 台灣證券預設的商品基本資料.
/// - ShUnit
struct SymbTwsBase {
   /// 台灣證券「一張」的單位數(股數)
   uint32_t ShUnit_{0};
   char     Padding___[4];

   void DailyClear() {
      this->ShUnit_ = 0;
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
