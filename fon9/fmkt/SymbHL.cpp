// \file fon9/fmkt/SymbHL.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbHL.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields SymbHigh::MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbHigh, Data_.Time_,    "Time"));
   flds.Add(fon9_MakeField(SymbHigh, Data_.PriHigh_, "PriHigh"));
   return flds;
}

seed::Fields SymbLow::MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbLow, Data_.Time_,   "Time"));
   flds.Add(fon9_MakeField(SymbLow, Data_.PriLow_, "PriLow"));
   return flds;
}

} } // namespaces
