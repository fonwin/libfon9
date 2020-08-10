// \file f9twf/ExgMdFmtBasicInfo.hpp
//
// 台灣期交所行情格式: 商品基本資料訊息(I010)
// 台灣期交所行情格式: 商品漲跌幅資訊(I012)
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtBasicInfo_hpp__
#define __f9twf_ExgMdFmtBasicInfo_hpp__
#include "f9twf/ExgMdFmt.hpp"

namespace f9twf {
fon9_PACK(1);

struct ExgMdBasicInfo_V7 {
   ExgMdProdId10     ProdId_;
   ExgMdPrice        RiseLimitPrice1_;
   ExgMdPrice        ReferencePrice_;
   ExgMdPrice        FallLimitPrice1_;
   ExgMdPrice        RiseLimitPrice2_;
   ExgMdPrice        FallLimitPrice2_;
   ExgMdPrice        RiseLimitPrice3_;
   ExgMdPrice        FallLimitPrice3_;
};
struct ExgMdBasicInfo_C7 {
   char              ProdKind_;
   fon9::PackBcd<1>  DecimalLocator_;
   fon9::PackBcd<1>  StrikePriceDecimalLocator_;
   fon9::PackBcd<8>  BeginDateYYYYMMDD_;
   fon9::PackBcd<8>  EndDateYYYYMMDD_;
   fon9::PackBcd<2>  FlowGroup_;
   fon9::PackBcd<8>  DeliveryDateYYYYMMDD_;
   /// 適用動態價格穩定:Y, 不適用動態價格穩定:N;
   char              DynamicBandingYN_;
};
struct ExgMdBasicInfo_V8 {
   ExgMdProdId10     ProdId_;
   ExgMdPrice        ReferencePrice_;
};
//--------------------------------------------------------------------------//
/// 逐筆行情 商品漲跌幅及基本資料訊息: Fut:(Tx='1'; Mg='1'); Opt:(Tx='4'; Mg='1'); Ver=7;
struct ExgMcI010_V7 : public ExgMcHead
                    , public ExgMdBasicInfo_V7
                    , public ExgMdBasicInfo_C7 {
};
/// 間隔行情 商品漲跌幅及基本資料訊息: Fut:(Tx='1'; Mg='1'); Opt:(Tx='4'; Mg='1'); Ver=7;
struct ExgMiI010_V7 : public ExgMiHead
                    , public ExgMdBasicInfo_V7
                    , public ExgMdBasicInfo_C7 {
};
//--------------------------------------------------------------------------//
struct ExgMcI010_V8 : public ExgMcHead
                    , public ExgMdBasicInfo_V8
                    , public ExgMdBasicInfo_C7 {
};
struct ExgMiI010_V8 : public ExgMiHead
                    , public ExgMdBasicInfo_V8
                    , public ExgMdBasicInfo_C7 {
};
//-////////////////////////////////////////////////////////////////////////-//
struct ExgMdLmtInfo {
   ExgMdProdId10     ProdId_;

   //
   // NO-RAISE-LIMIT-LEVELS      9(2) 1 漲停總階數 PACK BCD      LimitDef.Count_;
   // RAISE-LIMIT-LIST  OCCURS   1-99 6-594 依漲停總階數解析      LimitDef.Limits_[];
   //       LIMIT-LEVEL          9(2) 1 階數 PACK BCD
   //       RAISE-LIMIT-PRICE    9(9) 5 漲停價格 PACK BCD
   //
   // NO-FALL-LIMIT-LEVELS       9(2) 1 跌停總階數 PACK BCD
   // FALL-LIMIT-LIST   OCCURS   1-99 6-594 依跌停總階數解析
   //       LIMIT-LEVEL          9(2) 1 階數 PACK BCD
   //       FALL-LIMIT-PRICE     9(9) 5 跌停價格 PACK BCD
   //
   using LevelNo = fon9::PackBcd<2>;
   struct LvLimit {
      LevelNo     Level_;
      ExgMdPrice  Price_;
   };
   struct LimitDef {
      LevelNo     Count_;
      LvLimit     Limits_[1];
   };
   LimitDef       Raise_;
};
struct ExgMcI012 : public ExgMcHead
                 , public ExgMdLmtInfo {
};
struct ExgMiI012 : public ExgMiHead
                 , public ExgMdLmtInfo {
};

fon9_PACK_POP;
} // namespaces
#endif//__f9twf_ExgMdFmtBasicInfo_hpp__
