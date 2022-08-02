// \file fon9/fmkt/Symb.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/Symb.hpp"
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

SymbData::~SymbData() {
}
//--------------------------------------------------------------------------//
Symb::~Symb() {
}

static void OnSymbEv(SymbTree& tree, Symb& symb, void (SymbData::*fnEv)(SymbTree&, const Symb&)) {
   const int tabCount = static_cast<int>(tree.LayoutSP_->GetTabCount());
   for (int tabid = 0; tabid < tabCount; ++tabid) {
      if (auto* dat = symb.GetSymbData(tabid))
         (dat->*fnEv)(tree, symb);
   }
}
void Symb::OnSymbDailyClear(SymbTree& tree, const Symb& symb) {
   (void)tree; (void)symb; assert(this == &symb);
}
void Symb::OnSymbSessionClear(SymbTree& tree, const Symb& symb) {
   (void)tree; (void)symb; assert(this == &symb);
}
void Symb::DailyClear(SymbTree& owner, unsigned tdayYYYYMMDD) {
   this->TDayYYYYMMDD_ = tdayYYYYMMDD;
   this->TradingSessionId_ = f9fmkt_TradingSessionId_Normal;
   this->TradingSessionSt_ = f9fmkt_TradingSessionSt_Clear;
   OnSymbEv(owner, *this, &SymbData::OnSymbDailyClear);
}
void Symb::SessionClear(SymbTree& owner, f9fmkt_TradingSessionId tsesId) {
   this->TradingSessionId_ = tsesId;
   this->TradingSessionSt_ = f9fmkt_TradingSessionSt_Clear;
   OnSymbEv(owner, *this, &SymbData::OnSymbSessionClear);
}
bool Symb::IsExpired(unsigned tdayYYYYMMDD) const {
   (void)tdayYYYYMMDD;
   return false;
}
void Symb::OnBeforeRemove(SymbTree& owner, unsigned tdayYYYYMMDD) {
   (void)owner; (void)tdayYYYYMMDD;
}
void Symb::OnBeforeInsertToTree(SymbTree& owner, const void* symbsLocker) {
   (void)owner; (void)symbsLocker;
}
//--------------------------------------------------------------------------//
SymbData* Symb::GetSymbData(int tabid) {
   return tabid <= 0 ? this : nullptr;
}
SymbData* Symb::FetchSymbData(int tabid) {
   return tabid <= 0 ? this : nullptr;
}
seed::Fields Symb::MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(Symb, TDayYYYYMMDD_,     "TDay"));
   flds.Add(fon9_MakeField(Symb, TradingMarket_,    "Market"));
   flds.Add(fon9_MakeField(Symb, TradingSessionId_, "Session"));
   flds.Add(seed::FieldSP{new seed::FieldIntHx<underlying_type_t<f9fmkt_TradingSessionSt>>(
      Named("SessionSt"), fon9_OffsetOfRawPointer(Symb, TradingSessionSt_))});
   return flds;
}

} } // namespaces
