// \file f9extests/SymbIn.hpp
// \author fonwinz@gmail.com
#ifndef __f9extests_SymbIn_hpp__
#define __f9extests_SymbIn_hpp__
#include "f9extests/Config.h"
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/fmkt/SymbRef.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/fmkt/SymbDeal.hpp"
#include "fon9/fmkt/SymbTwa.hpp"

namespace f9extests {

/// \ingroup fmkt
/// 所有商品資料直接集中在此:
/// - 適用於: 追求速度(而非彈性)的系統.
/// - 這裡僅是範例.
class f9extests_API SymbIn : public fon9::fmkt::SymbTwa {
   fon9_NON_COPY_NON_MOVE(SymbIn);
   using base = fon9::fmkt::SymbTwa;
public:
   fon9::fmkt::SymbRef     Ref_;
   fon9::fmkt::SymbTwaBS   BS_;
   fon9::fmkt::SymbDeal    Deal_;

   using base::base;

   fon9::fmkt::SymbData* GetSymbData(int tabid) override;
   fon9::fmkt::SymbData* FetchSymbData(int tabid) override;

   static fon9::seed::LayoutSP MakeLayout(fon9::seed::TreeFlag flags);
};

class f9extests_API SymbInTree : public fon9::fmkt::SymbTree {
   fon9_NON_COPY_NON_MOVE(SymbInTree);
   using base = fon9::fmkt::SymbTree;

public:
   using base::base;

   fon9::fmkt::SymbSP MakeSymb(const fon9::StrView& symbid) override;
};
using SymbInTreeSP = fon9::intrusive_ptr<SymbInTree>;

} // namespaces
#endif//__f9extests_SymbIn_hpp__
