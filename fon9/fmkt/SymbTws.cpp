// \file fon9/fmkt/SymbTws.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTws.hpp"

namespace fon9 { namespace fmkt {

SymbTws::~SymbTws() {
}

seed::Fields SymbTws::MakeFields() {
   seed::Fields flds = base::MakeFields();
   SymbTwsBase::AddFields<SymbTws>(flds);
   return flds;
}

} } // namespaces
