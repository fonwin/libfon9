// \file fon9/fmkt/SymbTws.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTws.hpp"

namespace fon9 { namespace fmkt {

SymbTws::~SymbTws() {
}

void SymbTws::SessionClear(SymbTree& owner, f9fmkt_TradingSessionId tsesId) {
   base::SessionClear(owner, tsesId);
   SymbTwsBase::DailyClear();
}

seed::Fields SymbTws::MakeFields() {
   seed::Fields flds = base::MakeFields();
   SymbTwsBase::AddFields<SymbTws>(flds);
   return flds;
}

} } // namespaces
