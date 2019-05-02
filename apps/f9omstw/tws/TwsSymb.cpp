// \file tws/TwsSymb.cpp
// \author fonwinz@gmail.com
#include "tws/TwsSymb.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

static const int32_t kTwsSymbOffset[]{
   0,
   // fon9_OffsetOf(TwsSymb, Ref_),
};
static inline fon9::fmkt::SymbData* GetTwsSymbData(TwsSymb* pthis, int tabid) {
   return static_cast<size_t>(tabid) < fon9::numofele(kTwsSymbOffset)
      ? reinterpret_cast<fon9::fmkt::SymbData*>(reinterpret_cast<char*>(pthis) + kTwsSymbOffset[tabid])
      : nullptr;
}
fon9::fmkt::SymbData* TwsSymb::GetSymbData(int tabid) {
   return GetTwsSymbData(this, tabid);
}
fon9::fmkt::SymbData* TwsSymb::FetchSymbData(int tabid) {
   return GetTwsSymbData(this, tabid);
}
fon9::seed::LayoutSP TwsSymb::MakeLayout(fon9::seed::TreeFlag treeflags) {
   using namespace fon9::seed;
   return new Layout1(fon9_MakeField(fon9::Named{"SymbId"}, TwsSymb, SymbId_), treeflags,
                      new Tab{fon9::Named{"Base"}, TwsSymb::MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}
   );
}

} // namespaces
