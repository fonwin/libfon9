// \file f9twf/ExgMcFmtSS.hpp
//
// 台灣期交所行情格式:
// 逐筆行情: 快照更新訊息:I084;
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMcFmtSS_hpp__
#define __f9twf_ExgMcFmtSS_hpp__
#include "f9twf/ExgMdFmtBS.hpp"

namespace f9twf {
fon9_PACK(1);

/// 逐筆 快照更新訊息: Fut:(Tx='2'; Mg='C'); Opt:(Tx='5'; Mg='C'); Ver=1;
struct ExgMcI084 : public ExgMcHead {
   /// A: Refresh Begin
   /// O: Order Data
   /// S: Statistics for Order Data
   /// P: Product Status
   /// Z: Refresh Complete
   char  MessageType_;

   //-----------------------------------------------------------------------//
   struct RefreshBegin {
      /// 此輪 Refresh 訊息，對應即時行情傳輸群組之訊息流水序號。
      ExgMcProdMsgSeq   LastSeq_;
   };
   //-----------------------------------------------------------------------//
   struct OrderDataEntry {
      ExgMdProdId20     ProdId_;
      /// 商品行情訊息末筆處理序號.
      ExgMcProdMsgSeq   LastProdMsgSeq_;
      /// 買賣價量變更巢狀迴圈數.
      /// 若為0則代表該商品的委託簿為空，即無最佳5檔及衍生一檔。
      fon9::PackBcd<2>  NoMdEntries_;
      /// 根據 NoMdEntries_ 決定有幾筆 MdEntry_[];
      ExgMdEntry        MdEntry_[1];
   };
   struct OrderData {
      fon9::PackBcd<2>  NoEntries_;
      /// 依據 NoEntries_ 迴圈數依序解析.
      OrderDataEntry    Entry_[1];
   };
   //-----------------------------------------------------------------------//
   struct ProductStatisticsEntry {
      ExgMdProdId20     ProdId_;

      ExgMdHHMMSSu6     LastMatchTime_;
      ExgMdSPrice       LastMatchPrice_;
      fon9::PackBcd<8>  LastMatchQty_;

      fon9::PackBcd<8>  MatchTotalQty_;
      fon9::PackBcd<8>  MatchBuyCnt_;
      fon9::PackBcd<8>  MatchSellCnt_;

      ExgMdHHMMSSu6     FirstMatchTime_;
      ExgMdSPrice       FirstMatchPrice_;
      fon9::PackBcd<8>  FirstMatchQty_;

      ExgMdSPrice       DayHighPrice_;
      ExgMdSPrice       DayLowPrice_;

      /// 買進累計委託筆數.
      fon9::PackBcd<8>  BuyOrder_;
      /// 買進累計委託合約數.
      fon9::PackBcd<8>  BuyQty_;
      fon9::PackBcd<8>  SellOrder_;
      fon9::PackBcd<8>  SellQty_;
   };
   struct ProductStatistics {
      fon9::PackBcd<2>        NoEntries_;
      ProductStatisticsEntry  Entry_[1];
   };
   //-----------------------------------------------------------------------//
   struct ProductStatusEntry {
      ExgMdProdId20     ProdId_;
      /// 放寬漲幅階段: 若商品發生放寬漲跌幅時，此欄位表示該商品「目前」漲跌幅狀態。
      /// 若無，則為預設值 0。
      /// Ver1 = PackBcd<1>; Ver2 = PackBcd<2>; 基本上還是相容.
      fon9::PackBcd<2>  ExpandTypeLevelR_;
      /// 放寬跌幅階段: 若商品發生放寬漲跌幅時，此欄位表示該商品「目前」漲跌幅狀態。
      /// 若無，則為預設值 0。
      /// Ver1 = PackBcd<1>; Ver2 = PackBcd<2>; 基本上還是相容.
      fon9::PackBcd<2>  ExpandTypeLevelF_;
      /// 商品流程狀態陣列，預設值為 "NNNNNN";
      /// 每個字元代表某個時間流程或商品暫停/恢復流程狀態。
      /// 請參考: 「臺灣期貨交易所 逐筆行情資訊傳輸作業手冊」
      char              ProdStatus_[6];
      /// 若商品目前動態價格穩定措施發生暫停時，
      /// 此欄位以 BITMAP 表示該商品目前暫停原因。若無，則為 0。
      /// Bit 0：保留.
      /// Bit 1：因應特殊市況.
      /// Bit 2：動態價格穩定措施資訊異常.
      /// Bit 3：因資訊異常基準價無法計算 BIT MAP.
      fon9::byte  DynamicPriceBandingStatus_;
      /// 若商品若發生多方動態價格穩定範圍倍數調整，
      /// 此欄位顯示該商品動態價格穩定範圍倍數。
      /// 若無，則為預設值 0。
      fon9::PackBcd<2>  DynamicPriceBandingExpandRangeL_;
      /// 若商品若發生空方動態價格穩定範圍倍數調整，
      /// 此欄位顯示該商品動態價格穩定範圍倍數。
      /// 若無，則為預設值 0。
      fon9::PackBcd<2>  DynamicPriceBandingExpandRangeS_;
   };
   struct ProductStatus {
      fon9::PackBcd<2>     NoEntries_;
      ProductStatusEntry   Entry_[1];
   };
   struct ProductStatusEntryV3 {
      ExgMdProdId20     ProdId_;
      /// 放寬漲幅階段: 若商品發生放寬漲跌幅時，此欄位表示該商品「目前」漲跌幅狀態。
      /// 若無，則為預設值 0。
      /// Ver1 = PackBcd<1>; Ver2 = PackBcd<2>; 基本上還是相容.
      fon9::PackBcd<2>  ExpandTypeLevelR_;
      /// 放寬跌幅階段: 若商品發生放寬漲跌幅時，此欄位表示該商品「目前」漲跌幅狀態。
      /// 若無，則為預設值 0。
      /// Ver1 = PackBcd<1>; Ver2 = PackBcd<2>; 基本上還是相容.
      fon9::PackBcd<2>  ExpandTypeLevelF_;
      /// 商品流程狀態陣列，預設值為 "NNNNNN";
      /// 每個字元代表某個時間流程或商品暫停/恢復流程狀態。
      /// 請參考: 「臺灣期貨交易所 逐筆行情資訊傳輸作業手冊」
      char              ProdStatus_[7];
      /// 若商品目前動態價格穩定措施發生暫停時，
      /// 此欄位以 BITMAP 表示該商品目前暫停原因。若無，則為 0。
      /// Bit 0：保留.
      /// Bit 1：因應特殊市況.
      /// Bit 2：動態價格穩定措施資訊異常.
      /// Bit 3：因資訊異常基準價無法計算 BIT MAP.
      fon9::byte  DynamicPriceBandingStatus_;
      /// 若商品若發生多方動態價格穩定範圍倍數調整，
      /// 此欄位顯示該商品動態價格穩定範圍倍數。
      /// 若無，則為預設值 0。
      fon9::PackBcd<2>  DynamicPriceBandingExpandRangeL_;
      /// 若商品若發生空方動態價格穩定範圍倍數調整，
      /// 此欄位顯示該商品動態價格穩定範圍倍數。
      /// 若無，則為預設值 0。
      fon9::PackBcd<2>  DynamicPriceBandingExpandRangeS_;
   };
   struct ProductStatusV3 {
      fon9::PackBcd<2>     NoEntries_;
      ProductStatusEntryV3 Entry_[1];
   };
   //-----------------------------------------------------------------------//
   struct RefreshComplete {
      /// 此輪 Refresh 訊息，對應即時行情傳輸群組之訊息流水序號。
      ExgMcProdMsgSeq   LastSeq_;
   };
   //-----------------------------------------------------------------------//
   union {
      RefreshBegin      _A_RefreshBegin_;
      OrderData         _O_OrderData_;
      ProductStatistics _S_ProductStatistics_;
      ProductStatus     _P_ProductStatus_;
      ProductStatusV3   _P_ProductStatusV3_;
      RefreshComplete   _Z_RefreshComplete_;
   };
};

fon9_PACK_POP;
} // namespaces
#endif//__f9twf_ExgMcFmtSS_hpp__
