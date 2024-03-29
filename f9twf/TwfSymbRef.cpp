﻿// \file f9twf/TwfSymbRef.cpp
// \author fonwinz@gmail.com
#include "f9twf/TwfSymbRef.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace f9twf {

void TwfSymbRef_AddPriLmtFields(fon9::seed::Fields& flds, unsigned ifrom) {
   for (; ifrom < TwfSymbRef_Data::kPriLmtCount; ++ifrom) {
      flds.Add(fon9_MakeField(TwfSymbRef, Data_.PriLmts_[ifrom].Up_, fon9::RevPrintTo<std::string>("PriUpLmt", ifrom)));
      flds.Add(fon9_MakeField(TwfSymbRef, Data_.PriLmts_[ifrom].Dn_, fon9::RevPrintTo<std::string>("PriDnLmt", ifrom)));
   }
}
fon9::seed::Fields TwfSymbRef_MakeFields() {
   fon9::seed::Fields flds;
   flds.Add(fon9_MakeField(TwfSymbRef, Data_.PriRef_,         "PriRef"));
   flds.Add(fon9_MakeField(TwfSymbRef, Data_.PriLmts_[0].Up_, "PriUpLmt"));
   flds.Add(fon9_MakeField(TwfSymbRef, Data_.PriLmts_[0].Dn_, "PriDnLmt"));
   flds.Add(fon9_MakeField(TwfSymbRef, Data_.LvUpLmt_,        "LvUpLmt"));
   flds.Add(fon9_MakeField(TwfSymbRef, Data_.LvDnLmt_,        "LvDnLmt"));
   flds.Add(fon9_MakeField(TwfSymbRef, Data_.PriLmts_[1].Up_, "PriUpLmt1"));
   flds.Add(fon9_MakeField(TwfSymbRef, Data_.PriLmts_[1].Dn_, "PriDnLmt1"));
   flds.Add(fon9_MakeField(TwfSymbRef, Data_.PriLmts_[2].Up_, "PriUpLmt2"));
   flds.Add(fon9_MakeField(TwfSymbRef, Data_.PriLmts_[2].Dn_, "PriDnLmt2"));

   flds.Add(fon9_MakeField(TwfSymbRef, Data_.PPriClose_,      "PPriClose"));
   flds.Add(fon9_MakeField(TwfSymbRef, Data_.PQtyTotal_,      "PQtyTotal"));
   flds.Add(fon9_MakeField(TwfSymbRef, Data_.OpenInterest_,   "OpenInterest"));
   TwfSymbRef_AddPriLmtFields(flds, 3);
   return flds;
}
//--------------------------------------------------------------------------//
using TwfPri = fon9::fmkt::Pri;

f9twf_API bool TwfGetCombLmtPri(ExgCombSide combSide, const TwfSymbRef_Data& leg1, const TwfSymbRef_Data& leg2,
                                TwfLvLmts contractLvLmts, TwfPri* upLmt, TwfPri* dnLmt) {
   const TwfPri up1 = TwfGetLmtPri(leg1, contractLvLmts, UDIdx_Up);
   const TwfPri up2 = TwfGetLmtPri(leg2, contractLvLmts, UDIdx_Up);
   const TwfPri dn1 = TwfGetLmtPri(leg1, contractLvLmts, UDIdx_Dn);
   const TwfPri dn2 = TwfGetLmtPri(leg2, contractLvLmts, UDIdx_Dn);
   switch (combSide) {
   case ExgCombSide::SameSide:
      if (upLmt) *upLmt = CombPriLmt_SameSide(up1, up2);
      if (dnLmt) *dnLmt = CombPriLmt_SameSide(dn1, dn2);
      break;
   case ExgCombSide::SideIsLeg1:
      if (upLmt) *upLmt = CombPriLmt_SideIsLeg1(up1, dn2);
      if (dnLmt) *dnLmt = CombPriLmt_SideIsLeg1(dn1, up2);
      break;
   case ExgCombSide::SideIsLeg2:
      if (upLmt) *upLmt = CombPriLmt_SideIsLeg2(dn1, up2);
      if (dnLmt) *dnLmt = CombPriLmt_SideIsLeg2(up1, dn2);
      break;
   default:
   case ExgCombSide::None:
      return false;
   }
   return true;
}

} // namespaces
