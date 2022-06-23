// \file f9tws/ExgMdFmtIx.hpp
//
// 指數相關格式:
// - 格式21: 指數基本資料(代號、名稱...)
// - 格式 Twse10, Tpex12: 新編臺灣指數資料
// - 格式3:  指數統計資料(TWSE:43種，TPEX:28種)
//
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmtIx_hpp__
#define __f9tws_ExgMdFmtIx_hpp__
#include "f9tws/ExgMdFmt.hpp"

namespace f9tws {
fon9_PACK(1);

using IdxNo = StkNo;
// 指數值格式 9(5)V99
using IdxValueV2 = fon9::PackBcd<7>;
//--------------------------------------------------------------------------//
struct ExgMdFmt21 : public ExgMdHead {
   IdxNo IdxNo_;
   /// 指數中文名稱.
   char  NameBig5_[44];
   /// 指數英文名稱.
   char  NameEng_[44];
   /// 昨日收盤指數.
   IdxValueV2  YesterdayClosingIndexV2_;
   /// 開盤指數時間.
   fon9::PackBcd<4>  OpeningIndexHHMM_;
   /// 收盤指數時間.
   fon9::PackBcd<4>  ClosingIndexHHMM_;
   /// 指數傳輸格式代碼.
   fon9::PackBcd<2>  FormatCode_;
};
//--------------------------------------------------------------------------//
struct ExgMdFmt3 : public ExgMdHead {
   /// 指數時間.
   fon9::PackBcd<6>  IdxHHMMSS_;
   /// TWSE=43, TPEX=28;
   fon9::PackBcd<2>  AmountIndices_;
   /// 實際數量 = AmountIndices_;
   IdxValueV2        IndicesV2_[1];
};
//--------------------------------------------------------------------------//
/// TWSE 格式10, 版本1;
/// TPEX 格式12, 版本2;
struct ExgMdFmtIx : public ExgMdHead {
   IdxNo IdxNo_;
   /// 指數時間.
   fon9::PackBcd<6>  IdxHHMMSS_;
   IdxValueV2        IdxValueV2_;
};
using ExgMdTwseFmt10 = ExgMdFmtIx;
using ExgMdTpexFmt12 = ExgMdFmtIx;

fon9_PACK_POP;
} // namespaces
#endif//__f9tws_ExgMdFmtIx_hpp__
