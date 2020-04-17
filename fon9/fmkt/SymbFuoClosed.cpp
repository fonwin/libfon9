// \file fon9/fmkt/SymbFuoClosed.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbFuoClosed.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

fon9_API seed::Fields SymbFuoClosed_MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbFuoClosed, Data_.PriSettlement_, "PriSettlement"));
   return flds;
}

} } // namespaces
