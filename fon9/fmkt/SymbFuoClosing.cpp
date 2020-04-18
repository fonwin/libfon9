// \file fon9/fmkt/SymbFuoClosing.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbFuoClosing.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

fon9_API seed::Fields SymbFuoClosing_MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbFuoClosing, Data_.PriSettlement_, "PriSettlement"));
   return flds;
}

} } // namespaces
