// \file fon9/fmkt/SymbFuoClosing.hpp
//
// 期權商品收盤後相關資訊, 例如: 結算價;
//
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbFuoClosing_hpp__
#define __fon9_fmkt_SymbFuoClosing_hpp__
#include "fon9/fmkt/Symb.hpp"
#include "fon9/fmkt/FmktTypes.hpp"

namespace fon9 { namespace fmkt {

struct SymbFuoClosing_Data {
   /// 結算價
   Pri   PriSettlement_{Pri::Null()};

   void Clear() {
      this->PriSettlement_.AssignNull();
   }
   bool operator==(const SymbFuoClosing_Data& rhs) const {
      return this->PriSettlement_ == rhs.PriSettlement_;
   }
   bool operator!=(const SymbFuoClosing_Data& rhs) const {
      return !this->operator==(rhs);
   }
};

/// \ingroup fmkt
/// 商品資料的擴充: 期權商品收盤後相關資訊.
fon9_API_TEMPLATE_CLASS(SymbFuoClosing, SimpleSymbData, SymbFuoClosing_Data);

fon9_API seed::Fields SymbFuoClosing_MakeFields();

} } // namespaces
#endif//__fon9_fmkt_SymbFuoClosing_hpp__
