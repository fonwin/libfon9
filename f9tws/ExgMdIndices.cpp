// \file f9tws/ExgMdIndices.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdIndices.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9tws {

fon9::seed::Fields IndexDeal::MakeFields() {
   fon9::seed::Fields flds;
   flds.Add(fon9_MakeField(IndexDeal, Data_.DealTime_, "DealTime"));
   flds.Add(fon9_MakeField(IndexDeal, Data_.DealPri_,  "DealPri"));
   return flds;
}
//--------------------------------------------------------------------------//
fon9::seed::LayoutSP ExgMdIndex::MakeLayout() {
   using namespace fon9::seed;
   using namespace fon9::fmkt;
   return LayoutSP{new LayoutN(
      fon9_MakeField(Symb, SymbId_, "Id"), TreeFlag::AddableRemovable | TreeFlag::Unordered,
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Base}, MakeFields(),           TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Deal}, SymbDeal::MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_High}, SymbHigh::MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Low},  SymbLow::MakeFields(),  TabFlag::NoSapling_NoSeedCommand_Writable}},
      TabSP{new Tab{fon9::Named{fon9_kCSTR_TabName_Rt},   MdRtStream::MakeFields(), TabFlag::NoSapling_NoSeedCommand_Writable}}
   )};
}
static const int32_t kExgMdIndexOffset[]{
   0, // Base
   fon9_OffsetOf(ExgMdIndex, Deal_),
   fon9_OffsetOf(ExgMdIndex, High_),
   fon9_OffsetOf(ExgMdIndex, Low_),
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
void ExgMdIndex::DailyClear(fon9::fmkt::SymbTree& tree, unsigned tdayYYYYMMDD) {
   this->TDayYYYYMMDD_ = tdayYYYYMMDD;
   this->SessionClear(tree, f9fmkt_TradingSessionId_Normal);
}
void ExgMdIndex::SessionClear(fon9::fmkt::SymbTree& tree, f9fmkt_TradingSessionId tsesId) {
   base::SessionClear(tsesId);
   this->Deal_.DailyClear();
   this->High_.DailyClear();
   this->Low_.DailyClear();
   this->MdRtStream_.SessionClear(tree, *this);
}
void ExgMdIndex::BeforeRemove(fon9::fmkt::SymbTree& tree, unsigned tdayYYYYMMDD) {
   (void)tdayYYYYMMDD;
   this->MdRtStream_.BeforeRemove(tree, *this);
}
//--------------------------------------------------------------------------//
fon9::fmkt::SymbSP ExgMdIndices::MakeSymb(const fon9::StrView& symbid) {
   return new ExgMdIndex(symbid, this->RtInnMgr_);
}

} // namespaces
