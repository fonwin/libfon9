// \file fon9/fmkt/SymbTwaBase.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbTwaBase_hpp__
#define __fon9_fmkt_SymbTwaBase_hpp__
#include "fon9/fmkt/SymbTwsBase.hpp"
#include "fon9/fmkt/SymbTwfBase.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 台灣 「證券」+「期交所」預設的商品基本資料.
struct SymbTwaBase : public SymbTwsBase, public SymbTwfBase {
   template <class Derived>
   static void AddFields(seed::Fields& flds) {
      SymbTwsBase::AddFields<Derived>(flds);
      SymbTwfBase::AddFields<Derived>(flds);
   }
};

} } // namespaces
#endif//__fon9_fmkt_SymbTwaBase_hpp__
