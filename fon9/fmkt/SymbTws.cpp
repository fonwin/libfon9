// \file fon9/fmkt/SymbTws.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTws.hpp"

namespace fon9 { namespace fmkt {

SymbTws::~SymbTws() {
}
void SymbTws::OnSymbDailyClear(SymbTree& tree, const Symb& symb) {
   base::OnSymbDailyClear(tree, symb);
   this->SymbTwsBaseDailyClear();
}
void SymbTws::SessionClear(SymbTree& owner, f9fmkt_TradingSessionId tsesId) {
   (void)owner;
   this->TradingSessionId_ = tsesId;
   this->TradingSessionSt_ = f9fmkt_TradingSessionSt_Clear;
   this->SymbTwsBaseSessionClear();
}
seed::Fields SymbTws::MakeFields() {
   seed::Fields flds = base::MakeFields();
   SymbTwsBase::AddFields<SymbTws>(flds);
   return flds;
}

} } // namespaces
