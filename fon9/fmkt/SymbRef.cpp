// \file fon9/fmkt/SymbRef.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbRef.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields SymbRef_MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbRef, Data_.PriRef_,   "PriRef"));
   flds.Add(fon9_MakeField(SymbRef, Data_.PriUpLmt_, "PriUpLmt"));
   flds.Add(fon9_MakeField(SymbRef, Data_.PriDnLmt_, "PriDnLmt"));
   return flds;
}

SymbRefTabDy::SymbDataSP SymbRefTabDy::FetchSymbData(Symb&) {
   return SymbDataSP{new SymbRef{}};
}

} } // namespaces
