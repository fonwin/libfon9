// \file f9tws/ExgMdIndices.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdIndices.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9tws {

f9tws_API fon9::seed::Fields IndexDeal_MakeFields(bool isAddMarketSeq) {
   fon9::seed::Fields flds;
   if (isAddMarketSeq)
      flds.Add(fon9_MakeField(IndexDeal, Data_.MarketSeq_, "MktSeq"));
   flds.Add(fon9_MakeField(IndexDeal, Data_.Time_, "DealTime"));
   flds.Add(fon9_MakeField(IndexDeal, Data_.Pri_,  "DealPri"));
   return flds;
}
//--------------------------------------------------------------------------//
fon9::seed::LayoutSP ExgMdIndex::MakeLayout(bool isAddMarketSeq) {
   using namespace fon9;
   using namespace fon9::seed;
   using namespace fon9::fmkt;
   constexpr auto kTabFlag = TabFlag::NoSapling_NoSeedCommand_Writable;
   return LayoutSP{new LayoutN(
      fon9_MakeField(Symb, SymbId_, "Id"), TreeFlag::AddableRemovable | TreeFlag::Unordered,
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Base}, MakeFields(),                         kTabFlag}},
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Deal}, IndexDeal_MakeFields(isAddMarketSeq), kTabFlag}},
      f9fmkt_MAKE_TABS_OpenHighLow(),
      TabSP{new Tab{Named{fon9_kCSTR_TabName_Rt},   MdRtStream::MakeFields(),             kTabFlag}}
   )};
}
static const int32_t kExgMdIndexOffset[]{
   0, // Base
   fon9_OffsetOf(ExgMdIndex, Deal_),
   f9fmkt_MAKE_OFFSET_OpenHighLow(ExgMdIndex),
   fon9_OffsetOf(ExgMdIndex, MdRtStream_),
};
static inline fon9::fmkt::SymbData* GetExgMdIndexData(ExgMdIndex* pthis, int tabid) {
   return static_cast<size_t>(tabid) < fon9::numofele(kExgMdIndexOffset)
      ? reinterpret_cast<fon9::fmkt::SymbData*>(reinterpret_cast<char*>(pthis) + kExgMdIndexOffset[tabid])
      : nullptr;
}
fon9::fmkt::SymbData* ExgMdIndex::GetSymbData(int tabid) {
   return GetExgMdIndexData(this, tabid);
}
fon9::fmkt::SymbData* ExgMdIndex::FetchSymbData(int tabid) {
   return GetExgMdIndexData(this, tabid);
}
//--------------------------------------------------------------------------//
ExgMdIndex::~ExgMdIndex() {
}
void ExgMdIndex::SessionClear(fon9::fmkt::SymbTree& owner, f9fmkt_TradingSessionId tsesId) {
   this->TradingSessionId_ = tsesId;
   this->TradingSessionSt_ = f9fmkt_TradingSessionSt_Clear;
   this->MdRtStream_.OnSymbSessionClear(owner, *this);
}
void ExgMdIndex::OnBeforeRemove(fon9::fmkt::SymbTree& owner, unsigned tdayYYYYMMDD) {
   (void)tdayYYYYMMDD;
   this->MdRtStream_.BeforeRemove(owner, *this);
}
//--------------------------------------------------------------------------//
ExgMdIndices::ExgMdIndices(std::string pathFmt, bool isAddMarketSeq)
   : base(ExgMdIndex::MakeLayout(isAddMarketSeq), std::move(pathFmt),
          fon9::fmkt::MdSymbsCtrlFlag::AllowSubrSnapshotSymb
          | (isAddMarketSeq ? fon9::fmkt::MdSymbsCtrlFlag::HasMarketDataSeq : fon9::fmkt::MdSymbsCtrlFlag{})) {
}
fon9::fmkt::SymbSP ExgMdIndices::MakeSymb(const fon9::StrView& symbid) {
   return new ExgMdIndex(symbid, this->RtInnMgr_);
}

} // namespaces
