// \file fon9/fmkt/SymbTwa.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTwa.hpp"

namespace fon9 { namespace fmkt {

SymbTwa::~SymbTwa() {
}

seed::Fields SymbTwa::MakeFields() {
   seed::Fields flds = base::MakeFields();
   SymbTwaBase::AddFields<SymbTwa>(flds);
   return flds;
}

} } // namespaces
