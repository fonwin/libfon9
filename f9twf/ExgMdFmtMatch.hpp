// \file f9twf/ExgMdFmtMatch.hpp
//
// 台灣期交所行情格式: 成交價量.
// 逐筆行情: 成交價量:I024;
// 間隔行情: 成交價量:I020; 試撮成交價量:I022;
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtMatch_hpp__
#define __f9twf_ExgMdFmtMatch_hpp__
#include "f9twf/ExgMdFmt.hpp"

namespace f9twf {
fon9_PACK(1);

struct ExgMdMatchData {
   ExgMdSPrice       MatchPrice_;
   fon9::PackBcd<4>  MatchQty_;
};
struct ExgMdMatchHead {
   ExgMdHHMMSSu6     MatchTime_;
   ExgMdSPrice       FirstMatchPrice_;
   fon9::PackBcd<8>  FirstMatchQty_;
   /// 成交資料揭示項目註記（BIT MAP）.
   /// 成交資料揭示項目註記：以各別 Bit 表示揭示項目（以二進位 BINARY 表示）
   /// Bit 7 ＝ 1 本次揭示第一個封包, 0 延續上一封包.
   /// Bit 6-0 ＝（範圍 0000000-1111111）：揭示成交價、成交量之傳送之個數.
   /// 若為試撮結果(I022)僅會有 1 個試撮成交價量，故為 128（二進位值 10000000），不帶後面價量。
   fon9::byte        MatchDisplayItem_;
   /// 實際數量 = (MatchDisplayItem_ & 0x7f) 筆 MatchData_;
   ExgMdMatchData    MatchData_[1];
};
/// 緊接在 ExgMdMatchData MatchData_[] 之後的欄位.
struct ExgMdMatchCnt {
   /// 累計成交數量.
   fon9::PackBcd<8>  MatchTotalQty_;
   /// 累計買進成交筆數.
   fon9::PackBcd<8>  MatchBuyCnt_;
   /// 累計賣出成交筆數.
   fon9::PackBcd<8>  MatchSellCnt_;
};
//--------------------------------------------------------------------------//
/// 逐筆行情 成交價量: Fut:(Tx='2'; Mg='D'); Opt:(Tx='5'; Mg='D'); Ver=1;
struct ExgMcI024Head : public ExgMcHead {
   ExgMdProdId20     ProdId_;
   ExgMcProdMsgSeq   ProdMsgSeq_;
   /// 試撮價格註記, '0':成交揭示訊息; '1':試撮價格訊息.
   char              CalculatedFlag_;
};
/// I024 使用動態長度, 所以在此不加上 Tail;
struct ExgMcI024 : public ExgMcI024Head
                 , public ExgMdMatchHead {
};

//--------------------------------------------------------------------------//
/// 成交狀況標記: I020, I022, 放在 ExgMdTail 之前.
/// - 00：正常狀況
/// - 01~60：異常狀況時間(分鐘)
/// - 98：異常狀況解除
/// - 99：異常狀況超過 60 分鐘.
using ExgMdMatchStatusCode = fon9::PackBcd<2>;

/// 間隔行情 成交價量: Fut:(Tx='2'; Mg='1'); Opt:(Tx='5'; Mg='1'); Ver=4;
struct ExgMiI020Head : public ExgMiHead {
   ExgMdProdId20  ProdId_;
};
/// I020 使用動態長度, 所以在此不加上 Tail;
struct ExgMiI020 : public ExgMiI020Head
                 , public ExgMdMatchHead {
};
/// 間隔行情: 試撮成交價量: Fut:(Tx='2'; Mg='7'); Opt:(Tx='5'; Mg='7'); Ver=2;
using ExgMiI022 = ExgMiI020;

fon9_PACK_POP;
} // namespaces
#endif//__f9twf_ExgMdFmtMatch_hpp__
