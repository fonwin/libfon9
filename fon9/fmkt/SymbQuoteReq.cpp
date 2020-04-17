// \file fon9/fmkt/SymbQuoteReq.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbQuoteReq.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields SymbQuoteReq_MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbQuoteReq, Data_.DisclosureTime_,  "Time"));
   flds.Add(fon9_MakeField(SymbQuoteReq, Data_.DurationSeconds_, "DurSecs"));
   return flds;
}

} } // namespaces
