// \file fon9/fmkt/SymbBreakSt.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbBreakSt.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields SymbBreakSt_MakeFieldsTws() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbBreakSt, Data_.BreakHHMMSS_,   "BreakTime"));
   flds.Add(fon9_MakeField(SymbBreakSt, Data_.RestartHHMMSS_, "RestartTime"));
   return flds;
}

seed::Fields SymbBreakSt_MakeFieldsTwf() {
   seed::Fields flds = SymbBreakSt_MakeFieldsTws();
   flds.Add(fon9_MakeField(SymbBreakSt, Data_.ReopenHHMMSS_, "ReopenTime"));
   flds.Add(fon9_MakeField(SymbBreakSt, Data_.Reason_,       "Reason"));
   return flds;
}

} } // namespaces
