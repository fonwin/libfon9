// \file fon9/fmkt/FmktTypes.h
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_FmktTypes_h__
#define __fon9_fmkt_FmktTypes_h__
#include "fon9/sys/Config.h"
#include <stdint.h>

/// \ingroup fmkt
/// 買賣別. 
enum f9fmkt_Side fon9_ENUM_underlying(char) {
   f9fmkt_Side_Unknown = 0,
   f9fmkt_Side_Buy = 'B',
   f9fmkt_Side_Sell = 'S',
};

/// \ingroup fmkt
/// 商品的交易市場.
/// - 由系統決定市場別, 用於可直接連線的交易市場.
/// - 這裡沒有將所有可能的 market 都定義出來, 當有實際需求時可自行依下述方式增加.
/// - 底下以台灣市場為例:
///   - 如果系統有交易外匯, 則可增加:
///     `constexpr fon9::fmkt::TradingMarket  f9fmkt_TradingMarket_Forex = static_cast<fon9::fmkt::TradingMarket>('x');`
///   - 如果系統可直接連上 SGX, 則可增加:
///     `constexpr fon9::fmkt::TradingMarket  f9fmkt_TradingMarket_SGX = static_cast<fon9::fmkt::TradingMarket>('g');`
///   - 台灣大部分券商的對外(國外)期權, 可視為「一個」市場:
///     `constexpr fon9::fmkt::TradingMarket  f9fmkt_TradingMarket_FF = static_cast<fon9::fmkt::TradingMarket>('f');`
enum f9fmkt_TradingMarket   fon9_ENUM_underlying(char) {
   f9fmkt_TradingMarket_Unknown = '\0',
   /// 台灣上市(集中交易市場)
   f9fmkt_TradingMarket_TwSEC = 'T',
   /// 台灣上櫃(櫃檯買賣中心)
   f9fmkt_TradingMarket_TwOTC = 'O',
   /// 台灣興櫃.
   f9fmkt_TradingMarket_TwEmg = 'E',
   /// 台灣期交所.
   f9fmkt_TradingMarket_TwFex = 'F',
};

/// \ingroup fmkt
/// 交易時段代碼.
/// - FIX 4.x TradingSessionID(#336): 並沒有明確的定義, 由雙邊自訂.
/// - FIX 5.0 TradingSessionID(#336): http://fixwiki.org/fixwiki/TradingSessionID
///   '1'=Day; '2'=HalfDay; '3'=Morning; '4'=Afternoon; '5'=Evening; '6'=AfterHours; '7'=Holiday;
/// - 台灣期交所 FIX 的 Trading Session Status Message 定義 TradingSessionID=流程群組代碼.
/// - 台灣證交所 FIX 沒有 Trading Session Status Message,
///   但要求提供 TargetSubID(#57)=[TMP協定的 AP-CODE]:
///   Trading session: Required for New / Replace / Cancel / Query / Execution
///   '0'=Regular Trading; '7'=FixedPrice; '2'=OddLot.
enum f9fmkt_TradingSessionId   fon9_ENUM_underlying(char) {
   f9fmkt_TradingSessionId_Unknown = 0,
   /// 一般(日盤).
   f9fmkt_TradingSessionId_Normal = 'N',
   /// 零股.
   f9fmkt_TradingSessionId_OddLot = 'O',
   /// 盤後定價.
   f9fmkt_TradingSessionId_FixedPrice = 'F',
   /// 盤後交易(夜盤).
   f9fmkt_TradingSessionId_AfterHour = '6',

   /// 提供給陣列使用, 例如:
   /// using SessionMap = std::array<SessionRec, TradingSessionId_MaxIndex + 1u>;
   f9fmkt_TradingSessionId_MaxIndex = 'z',
};

/// \ingroup fmkt
/// 下單要求狀態.
enum f9fmkt_TradingRequestSt   fon9_ENUM_underlying(uint8_t) {
   /// 建構時的初始值.
   f9fmkt_TradingRequestSt_Initialing = 0,
   /// 下單要求已填妥.
   f9fmkt_TradingRequestSt_Initialed = 1,

   /// - 當檢查方式需要他方協助, 無法立即得到結果, 則在檢查前設定此狀態.
   /// - 如果檢查方式可立即完成:「失敗拒絕下單 or 檢查通過繼續下一步驟」, 則檢查前可以不用設定此狀態.
   f9fmkt_TradingRequestSt_Checking = 0x1c,
   /// 下單要求排隊中.
   f9fmkt_TradingRequestSt_Queuing = 0x20,

   /// 在呼叫 io.Send() 之前設定的狀態.
   /// 您可以自行決定要在 io.Send() 之前 or 之後 or both, 設定送出狀態.
   f9fmkt_TradingRequestSt_Sending = 0x30,
   /// 在呼叫 io.Send() 之後設定的狀態.
   /// 如果要在送出後設定狀態: 則要小心考慮在 io.Send() 之後, 設定 Sent 之前, 就已收到送出的結果.
   f9fmkt_TradingRequestSt_Sent = 0x38,

   /// 交易所部分回報: 有些下單要求交易所會有多次回報.
   /// 針對「某筆下單要求」交易所「最後一次回報」前的狀態.
   /// 此部分回報為成功.
   f9fmkt_TradingRequestSt_PartExchangeAccepted = 0xa2,
   /// 交易所部分回報: 有些下單要求交易所會有多次回報.
   /// 針對「某筆下單要求」交易所「最後一次回報」前的狀態.
   /// 此部分回報為失敗.
   f9fmkt_TradingRequestSt_PartExchangeRejected = 0xae,

   /// 下單要求流程已結束.
   f9fmkt_TradingRequestSt_Done = 0xda,
   /// 不明原因結束, 無法確定此要求是否成功.
   /// 例如: 要求送出後斷線.
   f9fmkt_TradingRequestSt_Broken = 0xdb,
   /// 排隊中的要求被取消, 此筆要求沒送出.
   /// 可能因程式結束, 或新單排隊中但收到刪單要求.
   f9fmkt_TradingRequestSt_QueuingCanceled = 0xdc,

   /// 下單要求因「線路狀況」拒絕. 尚未送給交易所.
   /// 例如: 無可用線路.
   f9fmkt_TradingRequestSt_LineRejected = 0xe3,
   /// 下單要求因「內部其他原因」拒絕. 尚未送給交易所.
   f9fmkt_TradingRequestSt_InternalRejected = 0xe9,
   /// 下單要求因「風控檢查」拒絕. 尚未送給交易所.
   f9fmkt_TradingRequestSt_CheckingRejected = 0xec,
   /// 下單要求被「交易所」拒絕.
   f9fmkt_TradingRequestSt_ExchangeRejected = 0xee,
   /// 刪改要求因「刪改時已無剩餘量,但曾經有此委託」而被「交易所」拒絕刪改.
   /// 請檢查: 是否已全部成交? 或是否已經被刪(交易所刪單、其他系統刪減...)?
   f9fmkt_TradingRequestSt_ExchangeNoLeavesQty  = 0xef,

   /// 新單要求: 尚未收到新單回報, 但先收到成交回報.
   f9fmkt_TradingRequestSt_FilledBeforeNew = 0xf1,
   /// 已收到交易所的確認回報.
   f9fmkt_TradingRequestSt_ExchangeAccepted = 0xf2,

   /// 交易所刪單: IOC, FOK...
   f9fmkt_TradingRequestSt_ExchangeCanceled = 0xfa,
   /// f9fmkt_TradingRequestSt_ExchangeCanceled 之後, 又收到的交易所刪單.
   /// 例: 報價單已收到 Bid 的刪單, 又到 Offer 的刪單.
   f9fmkt_TradingRequestSt_ExchangeCanceled2 = 0xfb,
   /// f9fmkt_TradingRequestSt_ExchangeCanceled之後, 又提供刪單原因時, 則進入此狀態.
   /// 例: 交易所部分成交(成交合併刪單回報), 然後又回報「因超過動態價格穩定標準」刪單.
   f9fmkt_TradingRequestSt_ExchangeCanceled3 = 0xfc,

   /// 下單要求流程已結束, 但後續又有變動.
   /// 類似 FIX 的 ExecType=D(Restated).
   f9fmkt_TradingRequestSt_Restated = 0xfe,
};

/// \ingroup fmkt
/// OrderSt 分成 4 階段:
/// - 新單還在系統裡未送出.
///   - Checking:   風控檢查中.
///   - Queuing:    排隊中.
/// - 新單已送出但尚未收到回報.
///   - Sending:    傳送中.
///   - Sent:       已送出, 但尚未收到交易所回報.
///   - Broken:     狀態不明: 送出後,未收到回報,但是斷線了.
/// - 委託已成立:
///   - Accepted:   交易所已收單.
///   - PartFilled: 部分成交.
/// - 委託已結束:
///   - 進入此階段後, 狀態就不會再變動.
///   - 當本機發現交易所的 WorkQty 變成 0 時.
///     - 刪減後的 AfterQty=0: 使用者刪減、交易所刪減(IOC、FOK、超過限額...)
///     - 收到成交回報後的 LeavesQty=0
///     - 交易所收盤.
///     - 要注意的是: 「成交回報、刪減回報」可能會亂序!
///       - 假設某筆委託交易所異動順序如下:   
///         新10, 減1(Bf=10,Af=9), 成2, 減2(Bf=7,Af=5), 成2, 刪(Bf=3,Af=0)
///       - 若分別在不同的主機上操作, 最多可能有 6*5*4*3*2*1=720 種順序組合!!
///   - Rejected:   委託被拒絕.
///     - BrokerRejected:   券商拒絕.
///     - ExchangeRejected: 交易所拒絕.
///   - Canceled:   刪單成功(已刪單狀態), 刪減後 AfterQty=0
///     - 透過刪減回報的「BeforeQty - AfterQty」來調整 LeavesQty.
///     - 但此時本機的 LeavesQty 不一定為 0, 因為可能有「在途成交」或「其他未收到的減量」.
///     - 若有在途成交, 則收完成交後, **不會變成全部成交**
///     - UserCanceled:     使用者刪單, 或減量成功後的 AfterQty=0;
///     - ExchangeCanceled: 交易所刪單(IOC、FOK...).
///   - FullFilled: 全部成交.
///   - PartCanceled: 部分成交(其實是全部成交)之後, 才收到成交前的減量回報.
///     - 例: 新單10:Accepted, 成交3:PartFilled, 減量7(Bf=10,Af=3):PartCanceled
///   - DoneForDay: 已收盤.
enum f9fmkt_OrderSt   fon9_ENUM_underlying(uint8_t) {
   f9fmkt_OrderSt_Initialing = 0,
   f9fmkt_OrderSt_NewChecking = f9fmkt_TradingRequestSt_Checking,
   f9fmkt_OrderSt_NewQueuing = f9fmkt_TradingRequestSt_Queuing,
   f9fmkt_OrderSt_NewSending = f9fmkt_TradingRequestSt_Sending,
};

#endif//__fon9_fmkt_FmktTypes_h__
