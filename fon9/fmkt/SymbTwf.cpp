// \file fon9/fmkt/SymbTwf.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTwf.hpp"

namespace fon9 { namespace fmkt {

SymbTwf::~SymbTwf() {
}

seed::Fields SymbTwf::MakeFields() {
   seed::Fields flds = base::MakeFields();
   SymbTwfBase::AddFields<SymbTwf>(flds);
   return flds;
}

} } // namespaces
