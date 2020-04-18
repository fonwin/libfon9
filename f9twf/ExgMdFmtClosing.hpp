// \file f9twf/ExgMdFmtClosing.hpp
//
// 收盤行情資料訊息 I070
// 收盤行情 + 結算價 I071
// 收盤行情 + 結算價 + 未平倉合約數 I072
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtClosing_hpp__
#define __f9twf_ExgMdFmtClosing_hpp__
#include "f9twf/ExgMdFmt.hpp"

namespace f9twf {
fon9_PACK(1);

/// I070 的內容.
struct ExgMdClosing070 {
   ExgMdProdId10  ProdId_;
   /// TERM-HIGH-PRICE 該期最高價.
   ExgMdPrice     PriTermHigh_;
   ExgMdPrice     PriTermLow_;
   /// 當盤最高價.
   ExgMdPrice     PriHigh_;
   ExgMdPrice     PriLow_;
   ExgMdPrice     PriOpen_;
   /// BUY-PRICE 最後買價.
   ExgMdPrice     PriBuy_;
   ExgMdPrice     PriSell_;
   ExgMdPrice     PriClose_;
   /// BO-COUNT-TAL 委託買進總筆數.
   fon9::PackBcd<8>  TotalCountBo_;
   /// BO-QTY-TAL 委託買進總口數.
   fon9::PackBcd<8>  TotalQtyBo_;
   fon9::PackBcd<8>  TotalCountSo_;
   fon9::PackBcd<8>  TotalQtySo_;
   /// TOTAL-COUNT 總成交筆數.
   fon9::PackBcd<8>  TotalCount_;
   /// TOTAL-QTY 總成交量.
   fon9::PackBcd<8>  TotalQty_;
   /// COMBINE-BO-COUNTTAL 合併委託買進總筆數.
   fon9::PackBcd<8>  CombineTotalCountBo_;
   /// COMBINE-BO-QTY-TAL 合併委託買進總口數.
   fon9::PackBcd<8>  CombineTotalQtyBo_;
   fon9::PackBcd<8>  CombineTotalCountSo_;
   fon9::PackBcd<8>  CombineTotalQtySo_;
   /// COMBINE-TOTAL-QTY 合併總成交量.
   fon9::PackBcd<8>  CombineTotalQty_;
};
//--------------------------------------------------------------------------//
/// 逐筆行情 收盤行情資料訊息: Fut:(Tx='3'; Mg='1'); Opt:(Tx='6'; Mg='1'); Ver=2;
using ExgMcI070 = ExgMdBodyDefineT<ExgMcHead, ExgMdClosing070>;
/// 間隔行情 收盤行情資料訊息: Fut:(Tx='3'; Mg='1'); Opt:(Tx='6'; Mg='1'); Ver=2;
using ExgMiI070 = ExgMdBodyDefineT<ExgMiHead, ExgMdClosing070>;
//--------------------------------------------------------------------------//
struct ExgMdClosing071 : public ExgMdClosing070 {
   /// SETTLEMENT-PRICE 結算價.
   ExgMdPrice  PriSettlement_;
};
/// 逐筆行情 收盤行情+結算價: Fut:(Tx='3'; Mg='2'); Opt:(Tx='6'; Mg='2'); Ver=2;
using ExgMcI071 = ExgMdBodyDefineT<ExgMcHead, ExgMdClosing071>;
/// 間隔行情 收盤行情+結算價: Fut:(Tx='3'; Mg='2'); Opt:(Tx='6'; Mg='2'); Ver=2;
using ExgMiI071 = ExgMdBodyDefineT<ExgMiHead, ExgMdClosing071>;
//--------------------------------------------------------------------------//
struct ExgMdClosing072 : public ExgMdClosing071 {
   /// OPEN-INTEREST 未平倉合約數.
   fon9::PackBcd<8>  OpenInterest_;
   /// BLOCK_TRADE_QNTY 鉅額交易總成交量.
   fon9::PackBcd<8>  BlockTradeQty_;
};
/// 逐筆行情 收盤行情+結算價+未平倉合約數: Fut:(Tx='3'; Mg='3'); Opt:(Tx='6'; Mg='3'); Ver=3;
using ExgMcI072 = ExgMdBodyDefineT<ExgMcHead, ExgMdClosing072>;
/// 間隔行情 收盤行情+結算價+未平倉合約數: Fut:(Tx='3'; Mg='3'); Opt:(Tx='6'; Mg='3'); Ver=3;
using ExgMiI072 = ExgMdBodyDefineT<ExgMiHead, ExgMdClosing072>;

fon9_PACK_POP;
} // namespaces
#endif//__f9twf_ExgMdFmtClosing_hpp__
