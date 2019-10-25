// \file f9twf/f9twf_SymbId_UT.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgTmpTypes.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/RevPrint.hpp"

using namespace f9twf;
using SingleId = ExgCombSymbId::Id;

void TestCodeMY(unsigned yyyymm, const char (&strMY)[3], bool isOptPut) {
   CodeMY codeMY;
   if (!(isOptPut ? codeMY.FromYYYYMM_OptPut(yyyymm) : codeMY.FromYYYYMM(yyyymm)))
      std::cout << "|err=FromYYYYMM:" << yyyymm;
   else if (memcmp(codeMY.MY_, strMY, 2) != 0)
      std::cout << "|FromYYYYMM=" << yyyymm
      << "|err.res=" << codeMY.MY_[0] << codeMY.MY_[1] << "|expected=" << strMY;
   else if (codeMY.ToYMM() != (fon9::signed_cast(yyyymm % 1000) * (isOptPut ? -1 : 1))) {
      std::cout << "|ToYMM=" << strMY
         << "|err=" << codeMY.ToYMM() << "|YYYYMM=" << yyyymm;
   }
   else
      return;
   std::cout << "\r[ERROR]" << std::endl;
   abort();
}
//--------------------------------------------------------------------------//
void TestCombIdParse(fon9::StrView symbid,
                     SingleId      leg1,
                     SingleId      leg2,
                     ExgCombSide   combSide,
                     TmpCombOp     combOp,
                     TmpSymbolType symbType) {
   std::cout << fon9::RevPrintTo<std::string>(
      "[TEST ] symbid=", symbid, "|leg1=", leg1, "|leg2=", leg2, "|side=", combSide, "|op=", combOp);
   ExgCombSymbId combId;
   if (!combId.Parse(symbid)) {
   __ERROR_ABORT:;
      std::cout << "\r[ERROR]" << std::endl;
      abort();
   }
   if (combSide != combId.CombSide_
       || combOp != combId.CombOp_
       || leg1 != combId.LegId1_
       || leg2 != combId.LegId2_) {
      std::cout << fon9::RevPrintTo<std::string>(
         "|err.leg1=", combId.LegId1_, "|leg2=", combId.LegId2_,
         "|side=", combId.CombSide_, "|op=", combId.CombOp_);
      goto __ERROR_ABORT;
   }
   const bool isFut = (leg1.length() < 10 || (leg1.length() == 10 && *(leg1.begin() + 7) == 'F'));
   SymbolId   symb;
   if (combOp == TmpCombOp::Single)
      symb.CopyFrom(leg1.begin(), leg1.length());
   else {
      if (isFut) {
         if (!FutMakeCombSymbolId(symb, ToStrView(leg1), ToStrView(leg2), combOp)) {
            std::cout << "|err.FutMakeCombSymbolId()";
            goto __ERROR_ABORT;
         }
      }
      else {
         if (!OptMakeCombSymbolId(symb, ToStrView(leg1), ToStrView(leg2), combOp)) {
            std::cout << "|err.OptMakeCombSymbolId()";
            goto __ERROR_ABORT;
         }
      }
   }
   if (symbid != ToStrView(symb)) {
      std::cout << "|err.MakeCombSymbolId()=" << std::string{symb.begin(), symb.length()};
      goto __ERROR_ABORT;
   }
   TmpSymbolType rSymbType = (isFut
                              ? FutCheckSymbolType(symb, combOp != TmpCombOp::Single)
                              : OptCheckSymbolType(symb, combOp != TmpCombOp::Single));
   if (rSymbType != symbType) {
      std::cout
         << "|SymbolType=" << static_cast<int>(symbType)
         << "|err.SymbolType=" << static_cast<int>(rSymbType);
      goto __ERROR_ABORT;
   }
   std::cout << "\r[OK   ]" << std::endl;
}
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   (void)argc; (void)argv;
   fon9::AutoPrintTestInfo utinfo{"f9twf_SymbId_UT"};

   TestCombIdParse("MXFA6",         "MXFA6",      "",           ExgCombSide::None,       TmpCombOp::Single,     TmpSymbolType::ShortText);
   TestCombIdParse("MXFA6/C6",      "MXFA6",      "MXFC6",      ExgCombSide::SideIsLeg2, TmpCombOp::TimeSpread, TmpSymbolType::ShortText);
   TestCombIdParse("MXFC6/MX4C6",   "MXFC6",      "MX4C6",      ExgCombSide::SideIsLeg2, TmpCombOp::TimeSpread, TmpSymbolType::ShortText);
   TestCombIdParse("3008  SFC6",    "3008  SFC6", "",           ExgCombSide::None,       TmpCombOp::Single,     TmpSymbolType::LongText);
   TestCombIdParse("1303  SFF3/I3", "1303  SFF3", "1303  SFI3", ExgCombSide::SideIsLeg2, TmpCombOp::TimeSpread, TmpSymbolType::LongText);

   TestCombIdParse("TXO06900I9",      "TXO06900I9",      "", ExgCombSide::None, TmpCombOp::Single, TmpSymbolType::ShortText);
   TestCombIdParse("1303  SO05200C3", "1303  SO05200C3", "", ExgCombSide::None, TmpCombOp::Single, TmpSymbolType::LongText);

   TestCombIdParse("TXO05200/05100C3",   "TXO05200C3", "TXO05100C3", ExgCombSide::SideIsLeg2, TmpCombOp::PriceSpread,         TmpSymbolType::ShortText);
   TestCombIdParse("TXO05200F3/I3",      "TXO05200F3", "TXO05200I3", ExgCombSide::SideIsLeg2, TmpCombOp::TimeSpread,          TmpSymbolType::ShortText);
   TestCombIdParse("TXO05200D3:P3",      "TXO05200D3", "TXO05200P3", ExgCombSide::SameSide,   TmpCombOp::Straddle,            TmpSymbolType::ShortText);
   TestCombIdParse("TXO05200D3:05100P3", "TXO05200D3", "TXO05100P3", ExgCombSide::SameSide,   TmpCombOp::Strangle,            TmpSymbolType::ShortText);
   TestCombIdParse("TXO05200G3-S3",      "TXO05200G3", "TXO05200S3", ExgCombSide::SideIsLeg2, TmpCombOp::ConversionReversals, TmpSymbolType::ShortText);
   TestCombIdParse("TX106300O6/TXOO6",   "TX106300O6", "TXO06300O6", ExgCombSide::SideIsLeg2, TmpCombOp::TimeSpread,          TmpSymbolType::ShortText);

   TestCombIdParse("1303  SO05200/05100C3",   "1303  SO05200C3", "1303  SO05100C3", ExgCombSide::SideIsLeg2, TmpCombOp::PriceSpread,         TmpSymbolType::LongText);
   TestCombIdParse("1303  SO05200F3/I3",      "1303  SO05200F3", "1303  SO05200I3", ExgCombSide::SideIsLeg2, TmpCombOp::TimeSpread,          TmpSymbolType::LongText);
   TestCombIdParse("1303  SO05200D3:P3",      "1303  SO05200D3", "1303  SO05200P3", ExgCombSide::SameSide,   TmpCombOp::Straddle,            TmpSymbolType::LongText);
   TestCombIdParse("1303  SO05200D3:05100P3", "1303  SO05200D3", "1303  SO05100P3", ExgCombSide::SameSide,   TmpCombOp::Strangle,            TmpSymbolType::LongText);
   TestCombIdParse("1303  SO05200G3-S3",      "1303  SO05200G3", "1303  SO05200S3", ExgCombSide::SideIsLeg2, TmpCombOp::ConversionReversals, TmpSymbolType::LongText);

   std::cout << "[TEST ] CodeMY";
   TestCodeMY(201901, "A9", false);
   TestCodeMY(201901, "M9", true);
   TestCodeMY(201912, "L9", false);
   TestCodeMY(201912, "X9", true);
   TestCodeMY(202001, "A0", false);
   TestCodeMY(202001, "M0", true);
   TestCodeMY(202012, "L0", false);
   TestCodeMY(202012, "X0", true);
   std::cout << "\r[OK   ]" << std::endl;
}
