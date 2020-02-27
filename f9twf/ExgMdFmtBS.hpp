// \file f9twf/ExgMdFmtBS.hpp
//
// 台灣期交所行情格式: 委託簿揭示訊息.
// 逐筆行情: 委託簿揭示訊息:I081; 委託簿快照訊息:I083;
// 間隔行情: 委託簿揭示訊息:I080; 試撮後剩餘委託簿揭示訊息:I082;
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtBS_hpp__
#define __f9twf_ExgMdFmtBS_hpp__
#include "f9twf/ExgMdFmt.hpp"

namespace f9twf {
fon9_PACK(1);

struct ExgMdOrderPQ {
   ExgMdSPrice       Price_;
   fon9::PackBcd<8>  Qty_;
};
//--------------------------------------------------------------------------//
struct ExgMdEntryType {
   /// 行情種類: '0':買, '1':賣, 'E':衍生買, 'F':衍生賣.
   char  EntryType_;
};
struct ExgMdEntry : public ExgMdEntryType
                  , public ExgMdOrderPQ {
   /// 此價格位於揭示深度中之檔位, 例如：
   /// MdEntryType='0'，且 Level_ 為 1，則代表此價格為買方最佳一檔。
   fon9::PackBcd<2>  Level_;
};
//---------------------------//
struct ExgMcI081UpdEntry {
   /// 行情揭示方式:
   /// '0':新增 New, '1':修改 Change, '2':刪除 Delete, '5':覆蓋 Overlay (適用衍生一檔).
   char  UpdateAction_;
};
struct ExgMcI081Entry : public ExgMcI081UpdEntry
                      , public ExgMdEntry {
};
/// 逐筆 委託簿揭示訊息: Fut:(Tx='2'; Mg='A'); Opt:(Tx='5'; Mg='A'); Ver=1
struct ExgMcI081 : public ExgMcHead {
   ExgMdProdId20     ProdId_;
   ExgMcProdMsgSeq   ProdMsgSeq_;
   fon9::PackBcd<2>  NoMdEntries_;
   /// 根據 NoMdEntries_ 決定有幾筆 MdEntry_[];
   ExgMcI081Entry    MdEntry_[1];
};
//---------------------------//
/// 逐筆行情 委託簿快照訊息: Fut:(Tx='2'; Mg='B'); Opt:(Tx='5'; Mg='B'); Ver=1
/// - 若為試撮後剩餘委託簿訊息：
///   - 最佳價格欄位若為市價時，以特殊值 999999999(買) 或 -999999999(賣)表示。
///   - 試撮階段無衍生委託單。
/// - 動態長度, 所以這裡不加 ExgMdTail.
struct ExgMcI083 : public ExgMcHead {
   ExgMdProdId20     ProdId_;
   ExgMcProdMsgSeq   ProdMsgSeq_;
   /// 試撮價格註記, '0':委託簿揭示訊息; '1':試撮後剩餘委託簿.
   char              CalculatedFlag_;
   fon9::PackBcd<2>  NoMdEntries_;
   /// 根據 NoMdEntries_ 決定有幾筆 MdEntry_[];
   ExgMdEntry        MdEntry_[1];
};

//--------------------------------------------------------------------------//
struct ExgMiDerivedBS {
   /// 衍生委託單買進價格/數量.
   fon9::PackBcd<9>  BuyPrice_;
   fon9::PackBcd<8>  BuyQty_;
   /// 衍生委託單賣出價格/數量.
   fon9::PackBcd<9>  SellPrice_;
   fon9::PackBcd<8>  SellQty_;
};

/// 間隔行情 委託簿揭示訊息: Fut:(Tx='2'; Mg='2'); Opt:(Tx='5'; Mg='2'); Ver=2;
struct ExgMiI080 : public ExgMiHead {
   ExgMdProdId20     ProdId_;
   ExgMdOrderPQ      BuyOrderBook_[5];
   ExgMdOrderPQ      SellOrderBook_[5];
   /// 有無衍生委託單之旗標: 01:解析 FirstDerived_; 00:不帶 FirstDerived_.
   fon9::PackBcd<2>  DerivedFlag_;
   ExgMiDerivedBS    FirstDerived_;
};
/// 間隔行情 試撮後剩餘委託簿揭示訊息, Fut:(Tx='2'; Mg='8'); Opt:(Tx='5'; Mg='8'); Ver=1;
/// 最佳價格欄位若為市價時，以特殊值 999999999(買) 或 -999999999(賣)表示。
using ExgMiI082 = ExgMiI080;

fon9_PACK_POP;
} // namespaces
#endif//__f9twf_ExgMdFmtBS_hpp__
