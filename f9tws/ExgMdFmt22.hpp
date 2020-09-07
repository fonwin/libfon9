// \file f9tws/ExgMdFmt22.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt22_hpp__
#define __f9tws_ExgMdFmt22_hpp__
#include "f9tws/ExgMdFmt.hpp"
#include "fon9/fmkt/SymbBSData.hpp"

namespace f9tws {
fon9_PACK(1);

/// 盤中零股交易個股基本資料.
struct ExgMdFmt22v1 : public ExgMdHeader {
   StkNo    StkNo_;
   /// 股票中文簡稱(BIG5 編碼);
   char     ChineseName_[16];
   /// 股票筆數註記, 用來識別「股票代號」欄位資料代表意義。
   /// - 開盤前, 每一循環傳送所有個股基本資料,
   ///   循環末筆此欄為 "AL",「股票代號」欄位為「總股票數目」。
   /// - 開盤至收盤間, 每一循環傳送當日新增股票資料,
   ///   循環末筆此欄為 "NE",「股票代號」欄位為「新增之總股票數目」。
   char     StockEntries_[2];
   /// 股票異常代碼: 
   /// 00=正常, 01=注意, 02=處置, 03=注意及處置, 04=再次處置,
   /// 05=注意及再次處置, 06=彈性處置, 07=注意及彈性處置.
   fon9::PackBcd<2>  AnomalyCode_;

   /// 今日參考價 9(5)V9(4).
   fon9::PackBcd<9>  PriRefV4_;
   /// 漲停價 9(5)V9(4).
   fon9::PackBcd<9>  PriUpLmtV4_;
   /// 跌停價 9(5)V9(4).
   fon9::PackBcd<9>  PriDnLmtV4_;

   /// 可現股當沖註記:
   /// A=可先買後賣或先賣後買, B=可先買後賣, SPACE=不可現股當沖。
   char  DayTrading_;

   /// 撮合循環秒數: 集合競價交易之撮合循環秒數, 0=採用逐筆撮合方式交易。
   fon9::PackBcd<6>   MatchingCycleSeconds_;

   /// 交易單位: 每一交易單位所代表之股數(權證為權證單位數, 受益憑證則為受益單位數),
   /// 預設值為 1000。若記錄值為 1000 表示該股票每一交易單位為一千股; 若記錄值為 500 表示該股票每一交易單位為 500 股。
   fon9::PackBcd<5>  TradingUnit_;
};

fon9_PACK_POP;
} // namespaces
#endif//__f9tws_ExgMdFmt22_hpp__
