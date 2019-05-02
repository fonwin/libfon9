// \file tws/TwsIvSc.cpp
// \author fonwinz@gmail.com
#include "tws/TwsIvSc.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9omstw {

void TwsIvSc::MakeFields(fon9::seed::Fields& scflds) {
   scflds.Add(fon9_MakeField(fon9::Named{"TotalBuy"},  TwsIvSc, TotalBuy_));
   scflds.Add(fon9_MakeField(fon9::Named{"TotalSell"}, TwsIvSc, TotalSell_));
}

} // namespaces
