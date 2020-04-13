// \file f9twf/ExgMdFmtBasicInfo.hpp
//
// 台灣期交所行情格式: 商品漲跌幅及基本資料訊息(I010)
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtBasicInfo_hpp__
#define __f9twf_ExgMdFmtBasicInfo_hpp__
#include "f9twf/ExgMdFmt.hpp"

namespace f9twf {
fon9_PACK(1);

struct ExgMdBasicInfo {
   ExgMdProdId10     ProdId_;
   ExgMdPrice        RiseLimitPrice1_;
   ExgMdPrice        ReferencePrice_;
   ExgMdPrice        FallLimitPrice1_;
   ExgMdPrice        RiseLimitPrice2_;
   ExgMdPrice        FallLimitPrice2_;
   ExgMdPrice        RiseLimitPrice3_;
   ExgMdPrice        FallLimitPrice3_;
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
//--------------------------------------------------------------------------//
/// 逐筆行情 商品漲跌幅及基本資料訊息: Fut:(Tx='1'; Mg='1'); Opt:(Tx='4'; Mg='1'); Ver=7;
struct ExgMcI010 : public ExgMcHead
                 , public ExgMdBasicInfo {
};
//--------------------------------------------------------------------------//
/// 間隔行情 商品漲跌幅及基本資料訊息: Fut:(Tx='1'; Mg='1'); Opt:(Tx='4'; Mg='1'); Ver=7;
struct ExgMiI010 : public ExgMiHead
                 , public ExgMdBasicInfo {
};

fon9_PACK_POP;
} // namespaces
#endif//__f9twf_ExgMdFmtBasicInfo_hpp__
