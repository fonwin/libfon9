// \file fon9/fmkt/SymbTwa.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTwa.hpp"

namespace fon9 { namespace fmkt {

SymbTwa::~SymbTwa() {
}
bool SymbTwa::IsExpired(unsigned tdayYYYYMMDD) const {
   return IsSymbExpired(this->EndYYYYMMDD_, tdayYYYYMMDD);
}
void SymbTwa::SessionClear(SymbTree& owner, f9fmkt_TradingSessionId tsesId) {
   if (this->TradingMarket_ == f9fmkt_TradingMarket_TwFUT
       || this->TradingMarket_ == f9fmkt_TradingMarket_TwOPT)
      base::SessionClear(owner, tsesId);
   else {
      this->TradingSessionId_ = tsesId;
      this->TradingSessionSt_ = f9fmkt_TradingSessionSt_Clear;
      this->SymbTwaBaseSessionClear();
   }
}
void SymbTwa::OnSymbDailyClear(SymbTree& tree, const Symb& symb) {
   base::OnSymbDailyClear(tree, symb);
   this->SymbTwaBaseDailyClear();
}
void SymbTwa::OnSymbSessionClear(SymbTree& tree, const Symb& symb) {
   base::OnSymbSessionClear(tree, symb);
   this->SymbTwaBaseSessionClear();
}
seed::Fields SymbTwa::MakeFields() {
   seed::Fields flds = base::MakeFields();
   SymbTwaBase::AddFields<SymbTwa>(flds);
   return flds;
}

} } // namespaces
