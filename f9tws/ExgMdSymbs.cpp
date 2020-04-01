// \file f9tws/ExgMdSymbs.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdSymbs.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9tws {

fon9::seed::LayoutSP ExgMdSymb::MakeLayout() {
   using namespace fon9::seed;
   using namespace fon9::fmkt;
   return LayoutSP{new LayoutN(
      fon9_MakeField(Symb, SymbId_, "Id"), TreeFlag::AddableRemovable | TreeFlag::Unordered,
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Base}, MakeFields(),           TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Ref},  SymbRef::MakeFields(),  TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_BS},   SymbBS::MakeFields(),   TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Deal}, SymbDeal::MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_High}, SymbHigh::MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Low},  SymbLow::MakeFields(),  TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Rt},   MdRtStream::MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}}
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
static inline fon9::fmkt::SymbData* GetExgMdSymbData(ExgMdSymb* pthis, int tabid) {
   return static_cast<size_t>(tabid) < fon9::numofele(kExgMdSymbOffset)
      ? reinterpret_cast<fon9::fmkt::SymbData*>(reinterpret_cast<char*>(pthis) + kExgMdSymbOffset[tabid])
      : nullptr;
}
fon9::fmkt::SymbData* ExgMdSymb::GetSymbData(int tabid) {
   return GetExgMdSymbData(this, tabid);
}
fon9::fmkt::SymbData* ExgMdSymb::FetchSymbData(int tabid) {
   return GetExgMdSymbData(this, tabid);
}
//--------------------------------------------------------------------------//
void ExgMdSymb::DailyClear(fon9::fmkt::SymbTree& tree, unsigned tdayYYYYMMDD) {
   this->TDayYYYYMMDD_ = tdayYYYYMMDD;
   this->SessionClear(tree, f9fmkt_TradingSessionId_Normal);
}
void ExgMdSymb::SessionClear(fon9::fmkt::SymbTree& tree, f9fmkt_TradingSessionId tsesId) {
   base::SessionClear(tsesId);
   this->Ref_.DailyClear();
   this->Deal_.DailyClear();
   this->BS_.DailyClear();
   this->High_.DailyClear();
   this->Low_.DailyClear();
   this->MdRtStream_.SessionClear(tree, *this);
}
void ExgMdSymb::BeforeRemove(fon9::fmkt::SymbTree& tree, unsigned tdayYYYYMMDD) {
   (void)tdayYYYYMMDD;
   this->MdRtStream_.BeforeRemove(tree, *this);
}
//--------------------------------------------------------------------------//
fon9::fmkt::SymbSP ExgMdSymbs::MakeSymb(const fon9::StrView& symbid) {
   return new ExgMdSymb(symbid, this->RtInnMgr_);
}

} // namespaces
