// \file f9tws/ExgMdSymbs.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdSymbs.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/fmkt/SymbTabNames.h"
#include "fon9/Log.hpp"

namespace f9tws {
using namespace fon9;
using namespace fon9::seed;
using namespace fon9::fmkt;

LayoutSP ExgMdSymb::MakeLayout() {
   return LayoutSP{new LayoutN(
      fon9_MakeField(Symb, SymbId_, "Id"), TreeFlag::AddableRemovable | TreeFlag::Unordered,
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Base}, MakeFields(),             TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Ref},  SymbRef::MakeFields(),    TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_BS},   SymbBS::MakeFields(),     TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Deal}, SymbDeal::MakeFields(),   TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_High}, SymbHigh::MakeFields(),   TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Low},  SymbLow::MakeFields(),    TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Rt},   MdRtStream::MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}}
   )};
}
static const int32_t kExgMdSymbOffset[]{
   0, // Base
   fon9_OffsetOf(ExgMdSymb, Ref_),
   fon9_OffsetOf(ExgMdSymb, BS_),
   fon9_OffsetOf(ExgMdSymb, Deal_),
   fon9_OffsetOf(ExgMdSymb, High_),
   fon9_OffsetOf(ExgMdSymb, Low_),
   fon9_OffsetOf(ExgMdSymb, MdRtStream_),
};
static inline SymbData* GetExgMdSymbData(ExgMdSymb* pthis, int tabid) {
   return static_cast<size_t>(tabid) < numofele(kExgMdSymbOffset)
      ? reinterpret_cast<SymbData*>(reinterpret_cast<char*>(pthis) + kExgMdSymbOffset[tabid])
      : nullptr;
}
SymbData* ExgMdSymb::GetSymbData(int tabid) {
   return GetExgMdSymbData(this, tabid);
}
SymbData* ExgMdSymb::FetchSymbData(int tabid) {
   return GetExgMdSymbData(this, tabid);
}
//--------------------------------------------------------------------------//
void ExgMdSymb::SessionClear(SymbTree& owner, f9fmkt_TradingSessionId tsesId) {
   base::SessionClear(owner, tsesId);
   this->Ref_.DailyClear();
   this->Deal_.DailyClear();
   this->BS_.DailyClear();
   this->High_.DailyClear();
   this->Low_.DailyClear();
   this->MdRtStream_.SessionClear(owner, *this);
}
void ExgMdSymb::OnBeforeRemove(SymbTree& owner, unsigned tdayYYYYMMDD) {
   (void)tdayYYYYMMDD;
   this->MdRtStream_.BeforeRemove(owner, *this);
}
//--------------------------------------------------------------------------//
ExgMdSymbs::ExgMdSymbs(std::string rtiPathFmt)
   : base(ExgMdSymb::MakeLayout(), std::move(rtiPathFmt), EnAllowSubrSnapshotSymb) {
}
SymbSP ExgMdSymbs::MakeSymb(const StrView& symbid) {
   return new ExgMdSymb(symbid, this->RtInnMgr_);
}

} // namespaces
