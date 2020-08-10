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

LayoutSP ExgMdSymb::MakeLayout(bool isAddMarketSeq) {
   constexpr auto kTabFlag = TabFlag::NoSapling_NoSeedCommand_Writable;
   return LayoutSP{new LayoutN(
      fon9_MakeField(Symb, SymbId_, "Id"), TreeFlag::AddableRemovable | TreeFlag::Unordered,
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Base},    MakeFields(),                kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Ref},     SymbRef_MakeFields(),        kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_BS},      SymbTwsBS_MakeFields(isAddMarketSeq), kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Deal},    SymbTwsDeal_MakeFields(),    kTabFlag}},
      f9fmkt_MAKE_TABS_OpenHighLow(),
      TabSP{new Tab{Named{fon9_kCSTR_TabName_BreakSt}, SymbBreakSt_MakeFieldsTws(), kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Rt},      MdRtStream::MakeFields(),    kTabFlag}}
   )};
}
static const int32_t kExgMdSymbOffset[]{
   0, // Base
   fon9_OffsetOf(ExgMdSymb, Ref_),
   fon9_OffsetOf(ExgMdSymb, BS_),
   fon9_OffsetOf(ExgMdSymb, Deal_),
   f9fmkt_MAKE_OFFSET_OpenHighLow(ExgMdSymb),
   fon9_OffsetOf(ExgMdSymb, BreakSt_),
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
ExgMdSymb::~ExgMdSymb() {
}
void ExgMdSymb::SessionClear(fon9::fmkt::SymbTree& owner, f9fmkt_TradingSessionId tsesId) {
   this->TradingSessionId_ = tsesId;
   this->TradingSessionSt_ = f9fmkt_TradingSessionSt_Clear;
   this->MdRtStream_.OnSymbSessionClear(owner, *this);
}
void ExgMdSymb::OnBeforeRemove(SymbTree& owner, unsigned tdayYYYYMMDD) {
   (void)tdayYYYYMMDD;
   this->MdRtStream_.BeforeRemove(owner, *this);
}
//--------------------------------------------------------------------------//
ExgMdSymbs::ExgMdSymbs(std::string rtiPathFmt, bool isAddMarketSeq)
   : base(ExgMdSymb::MakeLayout(isAddMarketSeq), std::move(rtiPathFmt),
          fon9::fmkt::MdSymbsCtrlFlag::AllowSubrSnapshotSymb
          | (isAddMarketSeq ? fon9::fmkt::MdSymbsCtrlFlag::HasMarketDataSeq : fon9::fmkt::MdSymbsCtrlFlag{}))
   , TabBreakSt_{LayoutSP_->GetTab(fon9_kCSTR_TabName_BreakSt)} {
}
SymbSP ExgMdSymbs::MakeSymb(const StrView& symbid) {
   return new ExgMdSymb(symbid, this->RtInnMgr_);
}

} // namespaces
