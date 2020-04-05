// \file fon9/fmkt/SymbTwf.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTwf.hpp"

namespace fon9 { namespace fmkt {

SymbTwf::~SymbTwf() {
}

bool SymbTwf::IsExpired(unsigned tdayYYYYMMDD) const {
   return IsSymbExpired(this->EndYYYYMMDD_, tdayYYYYMMDD);
}

void SymbTwf::SessionClear(SymbTree& owner, f9fmkt_TradingSessionId tsesId) {
   base::SessionClear(owner, tsesId);
   SymbTwfBase::DailyClear();
}

seed::Fields SymbTwf::MakeFields() {
   seed::Fields flds = base::MakeFields();
   SymbTwfBase::AddFields<SymbTwf>(flds);
   return flds;
}

} } // namespaces
