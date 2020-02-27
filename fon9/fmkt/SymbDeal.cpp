// \file fon9/fmkt/SymbDeal.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbDeal.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields SymbDeal::MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbDeal, Data_.InfoTime_,    "InfoTime"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.DealTime_,    "DealTime"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.Deal_.Pri_,   "DealPri"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.Deal_.Qty_,   "DealQty"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.TotalQty_,    "TotalQty"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.DealBuyCnt_,  "DealBuyCnt"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.DealSellCnt_, "DealSellCnt"));
   flds.Add(seed::FieldSP{new seed::FieldIntHx<underlying_type_t<DealFlag>>(
      Named("Flags"), fon9_OffsetOfRawPointer(SymbDeal, Data_.Flags_))});
   return flds;
}

SymbDealTabDy::SymbDataSP SymbDealTabDy::FetchSymbData(Symb&) {
   return SymbDataSP{new SymbDeal{}};
}

} } // namespaces
