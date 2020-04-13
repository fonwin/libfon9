// \file f9twf/ExgMdFmtBaseContract.hpp
//
// 台灣期交所行情格式: 契約基本資料.
// 逐筆行情 I011; 間隔行情 I011;
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtBaseContract_hpp__
#define __f9twf_ExgMdFmtBaseContract_hpp__
#include "f9twf/ExgMdFmt.hpp"
#include "f9twf/ExgTypes.hpp"

namespace f9twf {
fon9_PACK(1);

struct ExgMdContractI011 {
   ContractId        ContractId_;
   char              NameBig5_[30];
   StkNo             StkNo_;
   /// 契約類別: I:指數類 R:利率類 B:債券類 C:商品類 S:股票類 E:匯率類;
   char              SubType_;
   /// 契約乘數. 9(7)V9(4);
   fon9::PackBcd<11> ContractSizeV4_;
   /// 狀態碼 N：正常, P：暫停交易, U：即將上市;
   char              StatusCode_;
   /// 幣別: 台幣(1)、美元(2)、歐元(3)、日幣(4)、英鎊(5)、澳幣(6)、港幣(7)、人民幣(8);
   char              CurrencyType_;
   fon9::PackBcd<1>  DecimalLocator_;
   fon9::PackBcd<1>  StrikePriceDecimalLocator_;
   /// 是否可報價 Y:可報價, N:不可報價.
   char              AcceptQuoteFlagYN_;
   /// 上市日期 日期格式為 YYYYMMDD 僅適用股票類契約.
   char              BeginDateYYYYMMDD_[8];
   /// 是否可鉅額交易 Y:可, N:不可.
   char              BlockTradeFlagYN_;
   /// 到期別 S:標準 W:週.
   char              ExpiryType_;
   /// 現貨類別 E:ETF, S:個股; 僅適用股票類契約.
   char              UnderlyingType_;
   /// 商品收盤時間群組
   fon9::PackBcd<2>  MarketCloseGroup_;
   /// 一般交易時段：0, 盤後交易時段：1;
   char              EndSession_;
};
//--------------------------------------------------------------------------//
/// 逐筆行情, 契約基本資料訊息(I011): Fut:(Tx='1'; Mg='3'); Opt:(Tx='4'; Mg='3'); Ver=4;
struct ExgMcI011 : public ExgMcHead
                 , public ExgMdContractI011
                 , public ExgMdTail {
};
//--------------------------------------------------------------------------//
/// 間隔行情, 契約基本資料訊息(I011): Fut:(Tx='1'; Mg='3'); Opt:(Tx='4'; Mg='3'); Ver=4;
struct ExgMiI011 : public ExgMiHead
                 , public ExgMdContractI011
                 , public ExgMdTail {
};

fon9_PACK_POP;
} // namespaces
#endif//__f9twf_ExgMdFmtBaseContract_hpp__
