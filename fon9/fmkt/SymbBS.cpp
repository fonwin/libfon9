// \file fon9/fmkt/SymbBS.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

static void SymbBS_MakeFields(int ofsadj, seed::Fields& flds, bool isAddMarketSeq) {
   flds.Add(fon9_MakeField_OfsAdj(ofsadj, SymbBSData, InfoTime_, "InfoTime"));
   if (isAddMarketSeq)
      flds.Add(fon9_MakeField_OfsAdj(ofsadj, SymbBSData, MarketSeq_, "MktSeq"));
   char bsPriName[] = "-nP";
   char bsQtyName[] = "-nQ";
   int  idx;
   bsPriName[0] = bsQtyName[0] = 'S';
   for (idx = SymbBSData::kBSCount - 1; idx >= 0; --idx) {
      bsPriName[1] = bsQtyName[1] = static_cast<char>(idx + '1');
      flds.Add(fon9_MakeField_OfsAdj(ofsadj, SymbBSData, Sells_[idx].Pri_, bsPriName));
      flds.Add(fon9_MakeField_OfsAdj(ofsadj, SymbBSData, Sells_[idx].Qty_, bsQtyName));
   }
   bsPriName[0] = bsQtyName[0] = 'B';
   for (idx = 0; idx < SymbBSData::kBSCount; ++idx) {
      bsPriName[1] = bsQtyName[1] = static_cast<char>(idx + '1');
      flds.Add(fon9_MakeField_OfsAdj(ofsadj, SymbBSData, Buys_[idx].Pri_, bsPriName));
      flds.Add(fon9_MakeField_OfsAdj(ofsadj, SymbBSData, Buys_[idx].Qty_, bsQtyName));
   }
   flds.Add(seed::FieldSP{new seed::FieldIntHx<underlying_type_t<f9sv_BSFlag>>(
      Named("Flags"), ofsadj + fon9_OffsetOfRawPointer(SymbBSData, Flags_))});
}
static void AppendLmtFlags(int ofsadj, seed::Fields& flds) {
   flds.Add(seed::FieldSP{new seed::FieldIntHx<underlying_type_t<f9sv_BSLmtFlag>>(
      Named("LmtFlags"), ofsadj + fon9_OffsetOfRawPointer(SymbBSData, LmtFlags_))});
}
fon9_API seed::Fields SymbTwsBS_MakeFields(bool isAddMarketSeq) {
   seed::Fields flds;
   SymbBS_MakeFields(fon9_OffsetOf(SymbTwsBS, Data_), flds, isAddMarketSeq);
   AppendLmtFlags(fon9_OffsetOf(SymbTwsBS, Data_), flds);
   return flds;
}
fon9_API seed::Fields SymbTwfBS_MakeFields(bool isAddMarketSeq, bool isAddChannelSeq) {
   seed::Fields flds;
   SymbBS_MakeFields(fon9_OffsetOf(SymbTwfBS, Data_), flds, isAddMarketSeq);
   flds.Add(fon9_MakeField(SymbTwfBS, Data_.DerivedSell_.Pri_, "DS1P"));
   flds.Add(fon9_MakeField(SymbTwfBS, Data_.DerivedSell_.Qty_, "DS1Q"));
   flds.Add(fon9_MakeField(SymbTwfBS, Data_.DerivedBuy_.Pri_,  "DB1P"));
   flds.Add(fon9_MakeField(SymbTwfBS, Data_.DerivedBuy_.Qty_,  "DB1Q"));
   if (isAddChannelSeq)
      flds.Add(fon9_MakeField(SymbTwfBS, Data_.ChannelSeq_, "ChannelSeq"));
   return flds;
}
fon9_API seed::Fields SymbTwaBS_MakeFields(bool isAddMarketSeq) {
   seed::Fields flds = SymbTwfBS_MakeFields(isAddMarketSeq, false);
   AppendLmtFlags(fon9_OffsetOf(SymbTwfBS, Data_), flds);
   return flds;
}

SymbBSTabDy::SymbDataSP SymbBSTabDy::FetchSymbData(Symb&) {
   return SymbDataSP{new SymbTwaBS{}};
}

} } // namespaces
