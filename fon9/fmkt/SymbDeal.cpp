// \file fon9/fmkt/SymbDeal.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbDeal.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields SymbDeal::MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbDeal, Data_.Time_,      "Time"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.Deal_.Pri_, "DealPri"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.Deal_.Qty_, "DealQty"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.TotalQty_,  "TotalQty"));
   return flds;
}

SymbDealTabDy::SymbDataSP SymbDealTabDy::FetchSymbData(Symb&) {
   return SymbDataSP{new SymbDeal{}};
}

} } // namespaces
