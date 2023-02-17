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

f9tws_API Fields TwsSymbRef_MakeFields() {
   Fields flds = SymbRef_MakeFields();
   flds.Add(fon9_MakeField(TwsSymbRef, Data_.PPriClose_, "PPriClose"));
   flds.Add(fon9_MakeField(TwsSymbRef, Data_.PQtyTotal_, "PQtyTotal"));
   return flds;
}

LayoutSP ExgMdSymb::MakeLayout(bool isAddMarketSeq) {
   constexpr auto kTabFlag = TabFlag::NoSapling_NoSeedCommand_Writable;
   auto symbBaseFields = MakeFields();
   symbBaseFields.Add(fon9_MakeField2(ExgMdSymb, NameUTF8));
   return LayoutSP{new LayoutN(
      fon9_MakeField(Symb, SymbId_, "Id"), TreeFlag::AddableRemovable | TreeFlag::Unordered,
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Base},    std::move(symbBaseFields),   kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Ref},     TwsSymbRef_MakeFields(),     kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Deal},    SymbTwsDeal_MakeFields(isAddMarketSeq), kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_BS},      SymbTwsBS_MakeFields(isAddMarketSeq), kTabFlag}},
      f9fmkt_MAKE_TABS_OpenHighLow(),
      TabSP{new Tab{Named{fon9_kCSTR_TabName_BreakSt}, SymbBreakSt_MakeFieldsTws(), kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Rt},      MdRtStream::MakeFields(),    kTabFlag}}
   )};
}
static const int32_t kExgMdSymbOffset[]{
   0, // Base
   fon9_OffsetOf(ExgMdSymb, Ref_),
   fon9_OffsetOf(ExgMdSymb, Deal_),
   fon9_OffsetOf(ExgMdSymb, BS_),
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
void ExgMdSymb::DailyClear(SymbTree& owner, unsigned tdayYYYYMMDD) {
   const auto  stkName = this->NameUTF8_;
   // 保留部分資料? base::DailyClear() 之後還原. e.g. 昨收=LastDeal, 昨量=LastTotalQty; ...
   // 昨收、昨量使用匯入方式. 所以這裡不用額外處理.
   base::DailyClear(owner, tdayYYYYMMDD);
   this->NameUTF8_ = stkName;
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
   , TabBreakSt_{LayoutSP_->GetTab(fon9_kCSTR_TabName_BreakSt)}
   , FldBaseNameUTF8_{LayoutSP_->GetTab(fon9_kCSTR_TabName_Base)->Fields_.Get("NameUTF8")} {
}
SymbSP ExgMdSymbs::MakeSymb(const StrView& symbid) {
   return new ExgMdSymb(symbid, this->RtInnMgr_);
}

} // namespaces
