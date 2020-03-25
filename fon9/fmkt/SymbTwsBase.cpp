// \file fon9/fmkt/SymbTwsBase.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTwsBase.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

void SymbTwsBase::AddFields(int ofsadj, seed::Fields& flds) {
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, SymbTwsBase, ShUnit));
}

} } // namespaces
