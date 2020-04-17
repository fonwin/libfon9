// \file fon9/fmkt/SymbDynBand.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbDynBand.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields SymbDynBand_MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbDynBand, Data_.DynBandSt_, "St"));
   flds.Add(fon9_MakeField(SymbDynBand, Data_.StHHMMSS_,  "Time"));
   flds.Add(fon9_MakeField(SymbDynBand, Data_.Reason_,    "Reason"));
   flds.Add(fon9_MakeField(SymbDynBand, Data_.SideType_,  "SideType"));
   flds.Add(fon9_MakeField(SymbDynBand, Data_.RangeL_,    "RangeL"));
   flds.Add(fon9_MakeField(SymbDynBand, Data_.RangeS_,    "RangeS"));
   return flds;
}

} } // namespaces
