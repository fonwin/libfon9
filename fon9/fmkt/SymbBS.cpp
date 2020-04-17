// \file fon9/fmkt/SymbBS.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields SymbBS_MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbBS, Data_.InfoTime_, "InfoTime"));
   char bsPriName[] = "-nP";
   char bsQtyName[] = "-nQ";
   int  idx;
   bsPriName[0] = bsQtyName[0] = 'S';
   for (idx = SymbBSData::kBSCount - 1; idx >= 0; --idx) {
      bsPriName[1] = bsQtyName[1] = static_cast<char>(idx + '1');
      flds.Add(fon9_MakeField(SymbBS, Data_.Sells_[idx].Pri_, bsPriName));
      flds.Add(fon9_MakeField(SymbBS, Data_.Sells_[idx].Qty_, bsQtyName));
   }
   bsPriName[0] = bsQtyName[0] = 'B';
   for (idx = 0; idx < SymbBSData::kBSCount; ++idx) {
      bsPriName[1] = bsQtyName[1] = static_cast<char>(idx + '1');
      flds.Add(fon9_MakeField(SymbBS, Data_.Buys_[idx].Pri_, bsPriName));
      flds.Add(fon9_MakeField(SymbBS, Data_.Buys_[idx].Qty_, bsQtyName));
   }
   flds.Add(fon9_MakeField(SymbBS, Data_.DerivedSell_.Pri_, "DS1P"));
   flds.Add(fon9_MakeField(SymbBS, Data_.DerivedSell_.Qty_, "DS1Q"));
   flds.Add(fon9_MakeField(SymbBS, Data_.DerivedBuy_.Pri_,  "DB1P"));
   flds.Add(fon9_MakeField(SymbBS, Data_.DerivedBuy_.Qty_,  "DB1Q"));

   flds.Add(seed::FieldSP{new seed::FieldIntHx<underlying_type_t<BSFlag>>(
      Named("Flags"), fon9_OffsetOfRawPointer(SymbBS, Data_.Flags_))});
   return flds;
}

SymbBSTabDy::SymbDataSP SymbBSTabDy::FetchSymbData(Symb&) {
   return SymbDataSP{new SymbBS{}};
}

} } // namespaces
