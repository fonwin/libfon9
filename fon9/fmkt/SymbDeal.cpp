// \file fon9/fmkt/SymbDeal.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbDeal.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

fon9_API seed::Fields SymbDeal_MakeFields(bool isAddLmtFlags, bool isAddMarketSeq) {
   seed::Fields flds;
   flds.Add(fon9_MakeField(SymbDeal, Data_.InfoTime_,    "InfoTime"));
   if (isAddMarketSeq)
      flds.Add(fon9_MakeField(SymbDeal, Data_.MarketSeq_, "MktSeq"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.DealTime_,    "DealTime"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.Deal_.Pri_,   "DealPri"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.Deal_.Qty_,   "DealQty"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.TotalQty_,    "TotalQty"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.DealBuyCnt_,  "DealBuyCnt"));
   flds.Add(fon9_MakeField(SymbDeal, Data_.DealSellCnt_, "DealSellCnt"));
   flds.Add(seed::FieldSP{new seed::FieldIntHx<underlying_type_t<f9sv_DealFlag>>(
      Named("Flags"), fon9_OffsetOfRawPointer(SymbDeal, Data_.Flags_))});
   if(isAddLmtFlags)
      flds.Add(seed::FieldSP{new seed::FieldIntHx<underlying_type_t<f9sv_DealLmtFlag>>(
         Named("LmtFlags"), fon9_OffsetOfRawPointer(SymbDeal, Data_.LmtFlags_))});
   return flds;
}

SymbDealTabDy::SymbDataSP SymbDealTabDy::FetchSymbData(Symb&) {
   return SymbDataSP{new SymbDeal{}};
}

} } // namespaces
