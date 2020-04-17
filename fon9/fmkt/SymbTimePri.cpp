// \file fon9/fmkt/SymbHL.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTimePri.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields SymbTimePri_MakeFields(std::string priName) {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbTimePri, Data_.Time_, "Time"));
   flds.Add(fon9_MakeField(SymbTimePri, Data_.Pri_,  std::move(priName)));
   return flds;
}

} } // namespaces
