// \file fon9/fmkt/Symb.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/Symb.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

SymbData::~SymbData() {
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
   flds.Add(fon9_MakeField(Symb, TradingMarket_, "Market"));
   flds.Add(fon9_MakeField2(Symb, ShUnit));
   flds.Add(fon9_MakeField2(Symb, FlowGroup));
   return flds;
}

} } // namespaces
