// \file f9twf/ExgTmpTypes.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgTmpTypes.hpp"

namespace f9twf {

f9twf_API TmpCheckSum TmpCalcCheckSum(const TmpHeader& pktmp, size_t pksz) {
   assert(pksz == TmpGetValueU(pktmp.MsgLength_) + sizeof(pktmp.MsgLength_) + sizeof(TmpCheckSum));
   const byte* pbeg = reinterpret_cast<const byte*>(&pktmp);
   const byte* pend = pbeg + pksz - sizeof(TmpCheckSum);
   TmpCheckSum chksum = 0;
   while (pbeg < pend)
      chksum = static_cast<TmpCheckSum>(chksum + *pbeg++);
   return chksum;
}

//--------------------------------------------------------------------------//

bool ExgCombSymbId::Parse(const fon9::StrView symbid) {
   memset(this, 0, sizeof(*this));
   const size_t      idsz = symbid.size();
   const char* const idstr = symbid.begin();

   #define widthof(cstr)   (sizeof(cstr)-1)
   // 選擇權單式Id最短長度為10, 選擇權複式Id最短(TimeSpread)長度為13;
   // 所以先用 13 來過濾: 單式 or 期貨複式.
   if (idsz < 13) {
      static const size_t  kWidth_FutWeeklyTimeSpread = widthof("PPPCC/XXXDD");//len=11;
      switch (idsz) {
      case widthof("PPPCC/DD"): // 期貨價差, len=8;
      case kWidth_FutWeeklyTimeSpread:
         if (idstr[5] != '/')
            break;
         this->LegId1_.CopyFrom(idstr, 5);
         if (idsz == kWidth_FutWeeklyTimeSpread)
            this->LegId2_.CopyFrom(idstr + 6, 5);
         else {
            this->LegId2_.ForceLen(5);
            memcpy(this->LegId2_.begin(), idstr, 3);
            memcpy(this->LegId2_.begin() + 3, idstr + 6, 2);
         }
         this->CombOp_ = TmpCombOp::TimeSpread;
         this->CombSide_ = ExgCombSide::SideIsLeg2;
         return true;
      }
   __SET_SINGLE_RETURN:;
      this->LegId1_.CopyFrom(idstr, idsz);
      return true;
   }
   char chCombOp;
   // "PPPPPPSFxy/xy" 股期價差(LongId)
   // "PPPAAAAACC/DD" 選擇權價差(ShortId)
   // "PPPAAAAACC/pppDD"
   // "PPPAAAAACC:BBBBBDD"
   // [012345678901234567]
   switch (chCombOp = idstr[10]) {
   case '/':
      this->CombOp_ = TmpCombOp::TimeSpread;
      goto __OP_AT_10;
   case '-':
      this->CombOp_ = TmpCombOp::ConversionReversals;
      goto __OP_AT_10;
   case ':':
      switch (idsz) {
      case widthof("PPPAAAAACC:DD"):
         this->CombOp_ = TmpCombOp::Straddle;
         break;
      case widthof("PPPAAAAACC:BBBBBDD"):
         this->CombOp_ = TmpCombOp::Strangle;
         break;
      default:
         return false;
      }
      goto __OP_AT_10;
      // -----------------------------
   __OP_AT_10:;
      this->LegId2_.ForceLen(10);
      switch (idsz) {
      case widthof("PPPAAAAACC/DD"):
         //        "PPPPPPSFxy/xy"
         //        [0123456789012]
         memcpy(this->LegId2_.begin(), idstr, 8);
         memcpy(this->LegId2_.begin() + 8, idstr + 11, 2);
         break;
      case widthof("PPPAAAAACC/pppDD") /* Weekly time spread */:
         //        [0123456789012345]
         memcpy(this->LegId2_.begin(), idstr + 11, 3);
         memcpy(this->LegId2_.begin() + 3, idstr + 3, 5);
         memcpy(this->LegId2_.begin() + 8, idstr + 14, 2);
         break;
      case widthof("PPPAAAAACC:BBBBBDD"):
         //        [012345678901234567]
         memcpy(this->LegId2_.begin(), idstr, 3);
         memcpy(this->LegId2_.begin() + 3, idstr + 11, 7);
         break;
      default:
         return false;
      }
      this->LegId1_.CopyFrom(idstr, 10);

   __SET_COMB_SIDE_RETURN:;
      if (chCombOp == ':')
         this->CombSide_ = ExgCombSide::SameSide;
      else { // if(chCombOp == "/" || chCombOp == "-") {
         this->CombSide_ = ExgCombSide::SideIsLeg2;
      }
      return true;
      // -----------------------------
   }
   switch (idsz) {
   case widthof("PPPPPPSOpppppCC"): // Opt,LongId,len=15;
      goto __SET_SINGLE_RETURN;
   case widthof("PPPAAAAA/BBBBBCC"):
      //        [0123456789.12345], len=16;
      if (idstr[8] != '/')
         return false;
      this->LegId1_.ForceLen(10);
      memcpy(this->LegId1_.begin(), idstr, 8);
      memcpy(this->LegId1_.begin() + 8, idstr + 14, 2);
      this->LegId2_.ForceLen(10);
      memcpy(this->LegId2_.begin(), idstr, 3);
      memcpy(this->LegId2_.begin() + 3, idstr + 9, 7);
      this->CombOp_ = TmpCombOp::PriceSpread;
      this->CombSide_ = ExgCombSide::SideIsLeg2;
      return true;
   case widthof("PPPPPPSOppppp/pppppxy"):
      //        [0123456789.1234567890], len=21;
      if (idstr[13] != '/')
         return false;
      this->LegId1_.ForceLen(15);
      memcpy(this->LegId1_.begin(), idstr, 13);
      memcpy(this->LegId1_.begin() + 13, idstr + 19, 2);
      this->LegId2_.ForceLen(15);
      memcpy(this->LegId2_.begin(), idstr, 8);
      memcpy(this->LegId2_.begin() + 8, idstr + 14, 7);
      this->CombOp_ = TmpCombOp::PriceSpread;
      this->CombSide_ = ExgCombSide::SideIsLeg2;
      return true;
   case widthof("PPPPPPSTAAAAACC:BBBBBDD"):
      //        [0123456789.123456789012], len=23;
      if (idstr[15] != ':')
         return false;
      this->LegId1_.ForceLen(15);
      memcpy(this->LegId1_.begin(), idstr, 15);
      this->LegId2_.ForceLen(15);
      memcpy(this->LegId2_.begin(), idstr, 8);
      memcpy(this->LegId2_.begin() + 8, idstr + 16, 7);
      this->CombOp_ = TmpCombOp::Strangle;
      this->CombSide_ = ExgCombSide::SameSide;
      return true;
   case widthof("PPPPPPSOpppppxy-xy"):
      //        [0123456789.1234567], len=18;
      switch (chCombOp = idstr[15]) {
      case '/':   this->CombOp_ = TmpCombOp::TimeSpread;          break;
      case ':':   this->CombOp_ = TmpCombOp::Straddle;            break;
      case '-':   this->CombOp_ = TmpCombOp::ConversionReversals; break;
      default:    return false;
      }
      this->LegId1_.CopyFrom(idstr, 15);
      this->LegId2_.ForceLen(15);
      memcpy(this->LegId2_.begin(), idstr, 13);
      memcpy(this->LegId2_.begin() + 13, idstr + 16, 2);
      goto __SET_COMB_SIDE_RETURN;
   }
   return false;
}

//--------------------------------------------------------------------------//
f9twf_API bool FutMakeCombSymbolId(SymbolId& symb, fon9::StrView leg1, fon9::StrView leg2, TmpCombOp op) {
   symb.clear();
   if (op != TmpCombOp::TimeSpread)
      return false;
   size_t sz1 = leg1.size();
   if (sz1 != leg2.size())
      return false;
   char* pstr = symb.begin();
   switch (sz1) {
   case 5: // ShortId: PPPCC(5) or PPPCC/DD(8) or PPPCC/PPWDD(11)
      memcpy(pstr, leg1.begin(), 5);
      pstr[5] = '/';
      if (memcmp(leg1.begin(), leg2.begin(), 3) == 0) {
         memcpy(pstr + 6, leg2.begin() + 3, 2);
         symb.ForceLen(8);
      }
      else {
         memcpy(pstr + 6, leg2.begin(), 5);
         symb.ForceLen(11);
      }
      return true;
   case 10: // LongId: PPPPPPSTCC(10) or PPPPPPSTCC/DD(13)
      if (memcmp(leg1.begin(), leg2.begin(), 8) != 0)
         return false;
      memcpy(pstr, leg1.begin(), 10);
      pstr[10] = '/';
      memcpy(pstr + 11, leg2.begin() + 8, 2);
      symb.ForceLen(13);
      return true;
   }
   return false;
}
f9twf_API bool OptMakeCombSymbolId(SymbolId& symb, fon9::StrView leg1, fon9::StrView leg2, TmpCombOp op) {
   symb.clear();
   size_t sz1 = leg1.size();
   if (sz1 != leg2.size() || (sz1 != 10 && sz1 != 15))
      return false;
   char* pstr = symb.begin();
   switch (op) {
   case TmpCombOp::Single:
      return false;
   case TmpCombOp::PriceSpread:
      if (sz1 == 10) {
         if (memcmp(leg1.begin() + 8, leg2.begin() + 8, 2) != 0
             || memcmp(leg1.begin(), leg2.begin(), 3) != 0)
            return false;
         symb.ForceLen(widthof("PPPAAAAA/BBBBBCC"));
         memcpy(pstr, leg1.begin(), 8);
         pstr[8] = '/';
         memcpy(pstr + 9, leg2.begin() + 3, 7);
         return true;
      }
      if (memcmp(leg1.begin() + 13, leg2.begin() + 13, 2) != 0
          || memcmp(leg1.begin(), leg2.begin(), 8) != 0)
         return false;
      symb.ForceLen(widthof("PPPPPPSOppppp/pppppxy"));
      memcpy(pstr, leg1.begin(), 13);
      pstr[13] = '/';
      memcpy(pstr + 14, leg2.begin() + 8, 7);
      return true;
   case TmpCombOp::TimeSpread:
      if (sz1 == 10) {
         if (memcmp(leg1.begin() + 3, leg2.begin() + 3, 5) != 0)
            return false;
         if (memcmp(leg1.begin(), leg2.begin(), 3) == 0) {
            symb.ForceLen(widthof("PPPAAAAACC/DD"));
            memcpy(pstr + 11, leg2.begin() + 8, 2);
         }
         else {
            symb.ForceLen(widthof("PPPAAAAACC/pppDD"));
            memcpy(pstr + 11, leg2.begin(), 3);
            memcpy(pstr + 14, leg2.begin() + 8, 2);
         }
         memcpy(pstr, leg1.begin(), 10);
         pstr[10] = '/';
         return true;
      }
      pstr[15] = '/';
   __LONG_COMB_TIME:;
      if (memcmp(leg1.begin(), leg2.begin(), 13) != 0)
         return false;
      symb.ForceLen(widthof("PPPPPPSOpppppxy/xy"));
      memcpy(pstr, leg1.begin(), 15);
      memcpy(pstr + 16, leg2.begin() + 13, 2);
      return true;
   case TmpCombOp::Straddle: // "PPPAAAAACC:DD", "PPPPPPSOpppppxy:xy"
      if (sz1 == 10) {
         pstr[10] = ':';
   __SHORT_COMB_TIME:;
         if (memcmp(leg1.begin(), leg2.begin(), 8) != 0)
            return false;
         symb.ForceLen(widthof("PPPAAAAACC:DD"));
         memcpy(pstr, leg1.begin(), 10);
         memcpy(pstr + 11, leg2.begin() + 8, 2);
         return true;
      }
      pstr[15] = ':';
      goto __LONG_COMB_TIME;
   case TmpCombOp::Strangle: // "PPPAAAAACC:BBBBBDD", "PPPPPPSTAAAAACC:BBBBBDD"
      if (sz1 == 10) {
         if (memcmp(leg1.begin(), leg2.begin(), 3) != 0)
            return false;
         symb.ForceLen(widthof("PPPAAAAACC:BBBBBDD"));
         memcpy(pstr, leg1.begin(), 10);
         pstr[10] = ':';
         memcpy(pstr + 11, leg2.begin() + 3, 7);
         return true;
      }
      if (memcmp(leg1.begin(), leg2.begin(), 8) != 0)
         return false;
      symb.ForceLen(widthof("PPPPPPSTAAAAACC:BBBBBDD"));
      memcpy(pstr, leg1.begin(), 15);
      pstr[15] = ':';
      memcpy(pstr + 16, leg2.begin() + 8, 7);
      return true;
   case TmpCombOp::ConversionReversals: // "PPPAAAAACC/DD", "PPPPPPSOpppppxy-xy"
      if (sz1 == 10) {
         pstr[10] = '-';
         goto __SHORT_COMB_TIME;
      }
      pstr[15] = '-';
      goto __LONG_COMB_TIME;
   }
   return false;
}

} // namespaces
