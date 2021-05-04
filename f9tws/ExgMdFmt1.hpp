// \file f9tws/ExgMdFmt1.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt1_hpp__
#define __f9tws_ExgMdFmt1_hpp__
#include "f9tws/ExgMdFmt.hpp"

namespace f9tws {
fon9_PACK(1);

struct ExgMdFmt1_C3 : public ExgMdHeader {
   StkNo    StkNo_;
   /// 股票中文簡稱(BIG5 編碼);
   char     ChineseName_[16];
   /// 01:水泥工業, 02:食品工業, 03:塑膠工業...
   /// 其餘請參考: 交易所文件,附錄三.
   char     IndustryCategory_[2];
   /// W1:認購權證、等比例發行(公開發行時原始轉換標的股數為 1000)
   /// W2:認購權證、不等比例發行(公開發行時原始轉換標的股數不為 1000)
   /// 其餘請參考: 交易所文件,附錄四.
   char     StockCategory_[2];
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
};

struct ExgMdFmt1_C3Ref {
   /// 今日參考價 9(5)V9(4).
   fon9::PackBcd<9>  PriRefV4_;
   /// 漲停價 9(5)V9(4).
   fon9::PackBcd<9>  PriUpLmtV4_;
   /// 跌停價 9(5)V9(4).
   fon9::PackBcd<9>  PriDnLmtV4_;
   /// 非十元面額註記: Y=每股金額「非」新台幣十元之股票。
   char  IsNon10NTD_;
   /// 異常推介個股註記: Y=有線電視理財節目異常推介之有價證券。
   char  IsAbnormalRecommendation_;
   /// 特殊異常證券註記: Y=表示為特殊異常有價證券。
   char  IsAbnormalSecurities_;
   /// 可現股當沖註記:
   /// A=可先買後賣或先賣後買, B=可先買後賣, SPACE=不可現股當沖。
   char  DayTrading_;
   /// 平盤下融券賣出: Y=可平盤下融券賣出。
   char  IsMarginSale_BelowPriRef_;
   /// 平盤下借券賣出註記: Y=可平盤下借券賣出證券。
   char  IsSecuritiesLendingSale_BelowPriRef_;
   /// 撮合循環秒數: 集合競價交易之撮合循環秒數, 0=採用逐筆撮合方式交易。
   fon9::PackBcd<6>   MatchingCycleSeconds_;

   /// 權證識別碼: Y=表示有權證資料, SPACE=權證資料欄位皆為 0。
   char  HasWarrantData_;
   /// 履約價格: 最新權證履約價格 9(6)V9(4)。
   fon9::PackBcd<10>  WarrantPriStrikeV4_;
   /// 前一營業日履約數量: 數量單位為一千權證單位。
   fon9::PackBcd<10>  WarrantPrevDayExerciseVolume_;
   /// 前一營業日註銷數量: 數量單位為一千權證單位。
   fon9::PackBcd<10>  WarrantPrevDayCancellationVolume_;
   /// 發行餘額量: 數量單位為一千權證單位。
   fon9::PackBcd<10>  WarrantIssuingBalance_;
   /// 行使比率 9(06)V99: 最新每千單位權證可轉換標的股數。
   /// 若為指數權證, 則紀錄最新每千單位權證可轉換標的指數值。
   /// 例如:
   /// - 權證標的為股票時, 若此欄位值為 1000.00, 則代表每單位權證之行使比率為 1;
   ///   若此欄位值為 300.00，則代表每單位權證之行使比率為 0.3;
   /// - 權證標的為指數時, 若此欄位值為 1000.00，則代表每單位權證之行使比率為 1;
   ///   若此欄位值為 500.00，則代表每單位權證之行使比率為 0.5。
   fon9::PackBcd<8>  WarrantStrikeRatioV2_;
   /// 權證上限價格 9(6)V9(4)
   fon9::PackBcd<10> WarrantPriUpperLimitV4_;
   /// 權證下限價格 9(6)V9(4)
   fon9::PackBcd<10> WarrantPriLowerLimitV4_;
   /// 權證到期日 YYYYMMDD
   fon9::PackBcd<8>  WarrantMaturityDate_;
};

struct ExgMdFmt1_C3Tail {
   /// 交易單位: 每一交易單位所代表之股數(權證為權證單位數, 受益憑證則為受益單位數),
   /// 預設值為 1000。若記錄值為 1000 表示該股票每一交易單位為一千股; 若記錄值為 500 表示該股票每一交易單位為 500 股。
   fon9::PackBcd<5>  TradingUnit_;
   /// 交易幣別代號: SPACES=新台幣, 否則為其它外國幣別:附錄七。
   char TradingCurrencyCode_[3];
   /// 行情線路: 用來識別個股當日的盤中即時行情送出的 IP 線路及格式。
   fon9::PackBcd<2>  InformationLine_;
};
//--------------------------------------------------------------------------//
struct ExgMdFmt1_C3Foreign {
   /// 外國股票識別碼: Y=外國股票。
   char IsForeignStock_;
};
struct ExgMdFmt1_C3CTGCD {
   /// 板別註記:
   /// 以 ASCII 1 BYTE 表示，預設值為"0"。紀錄值為"3"時表示該股為創新板股票
   char StkCTGCD_;
};
struct ExgMdFmt1v8_Twse
   : public ExgMdFmt1_C3
   , public ExgMdFmt1_C3Ref
   , public ExgMdFmt1_C3Foreign
   , public ExgMdFmt1_C3Tail
{
};
struct ExgMdFmt1v9_Twse
   : public ExgMdFmt1_C3
   , public ExgMdFmt1_C3CTGCD
   , public ExgMdFmt1_C3Ref
   , public ExgMdFmt1_C3Foreign
   , public ExgMdFmt1_C3Tail
{
};
//--------------------------------------------------------------------------//
struct ExgMdFmt1_TPEX_317 {
   /// 類股註記: 1=中小企業股票（名稱暫定，認定標準依櫃買中心正式公告為準）。
   char  ClassIndicator_;
};
struct ExgMdFmt1v8_Tpex
   : public ExgMdFmt1_C3
   , public ExgMdFmt1_TPEX_317
   , public ExgMdFmt1_C3Ref
   , public ExgMdFmt1_C3Tail
{
};
struct ExgMdFmt1v9_Tpex
   : public ExgMdFmt1_C3
   , public ExgMdFmt1_TPEX_317
   , public ExgMdFmt1_C3CTGCD
   , public ExgMdFmt1_C3Ref
   , public ExgMdFmt1_C3Tail
{
};

fon9_PACK_POP;
} // namespaces
#endif//__f9tws_ExgMdFmt1_hpp__
