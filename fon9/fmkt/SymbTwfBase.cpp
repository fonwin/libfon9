// \file fon9/fmkt/SymbTwfBase.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTwfBase.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

void SymbTwfBase::AddFields(int ofsadj, seed::Fields& flds) {
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, SymbTwfBase, FlowGroup));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, SymbTwfBase, PriceOrigDiv));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, SymbTwfBase, StrikePriceDiv));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, SymbTwfBase, ExgSymbSeq));
   flds.Add(fon9_MakeField_OfsAdj (ofsadj, SymbTwfBase, EndYYYYMMDD_, "EndDate"));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, SymbTwfBase, SettleYYYYMM));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, SymbTwfBase, StrikePrice));
   flds.Add(fon9_MakeField2_OfsAdj(ofsadj, SymbTwfBase, CallPut));
}

} } // namespaces
