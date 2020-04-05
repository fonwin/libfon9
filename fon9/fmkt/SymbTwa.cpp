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
   base::SessionClear(owner, tsesId);
   SymbTwaBase::DailyClear();
}

seed::Fields SymbTwa::MakeFields() {
   seed::Fields flds = base::MakeFields();
   SymbTwaBase::AddFields<SymbTwa>(flds);
   return flds;
}

} } // namespaces
