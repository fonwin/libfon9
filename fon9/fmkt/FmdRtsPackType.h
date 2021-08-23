// \file fon9/fmkt/FmdRtsPackType.h
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_FmdRtsPackType_h__
#define __fon9_fmkt_FmdRtsPackType_h__
#include "fon9/sys/Config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// \ingroup fmkt
/// 在訂閱 MdRts 時, 可選擇要訂閱哪些種類的訊息.
fon9_ENUM(f9sv_MdRtsKind, uint32_t) {
   /// 基本資料訊息.
   f9sv_MdRtsKind_Base = 0x01,
   /// 參考價訊息(今日參考價、漲跌停價、LvUpLmt、LvDnLmt...)
   f9sv_MdRtsKind_Ref = 0x02,
   /// 成交價量.
   f9sv_MdRtsKind_Deal = 0x04,
   /// 買賣報價.
   f9sv_MdRtsKind_BS = 0x08,

   //// 收盤後資訊, e.g. 結算價.
   f9sv_MdRtsKind_Closing = 0x10,

   /// 詢價.
   f9sv_MdRtsKind_QuoteReq = 0x20,

   /// 動態價格穩定措施.
   f9sv_MdRtsKind_DynBand = 0x40,

   /// 交易時段狀態.
   /// Session, SessionSt, TwsBaseFlag...
   f9sv_MdRtsKind_TradingSession = 0x80,

   /// 沒有 InfoTime 的 f9sv_RtsPackType 打包格式.
   /// 回補時, 若有指定回補起始時間, 則會從封包內容取出 InfoTime,
   /// 如果有此旗標, 表示無法藉此封包判斷回補起始時間.
   f9sv_MdRtsKind_NoInfoTime = 0x100,

   /// 當某訊息需要提供給所有訂閱者, 不論訂閱時 Kind filter.
   /// e.g. 暫停交易(BreakSt).
   f9sv_MdRtsKind_All_NoInfoTime = 0xffffffff,
   f9sv_MdRtsKind_All_AndInfoTime = f9sv_MdRtsKind_All_NoInfoTime - f9sv_MdRtsKind_NoInfoTime,

   /// 當訂閱時沒有指定 Kind filter, 則表示訂閱全部.
   f9sv_MdRtsKind_Full = 0xffffffff,
};

/// \ingroup fmkt
/// - 預期 f9sv_RtsPackType 應在 0..0x7f 之間.
///   若超過 0x7f, 則應思考編碼端及解碼端應如何處理.
/// - 封包基本格式:
///   - f9sv_RtsPackType(1 byte)
///   - 大部分接著 InfoTime(Bitv:DayTime, 可能為 Null, 表示 InfoTime 與前一個封包相同)
///   - 後續內容由各 f9sv_RtsPackType 自行定義.
fon9_ENUM(f9sv_RtsPackType, uint8_t) {
   /// 打包成交明細.
   /// - 收到封包者: 一個 DealPack 可能觸發多次「交易明細」異動事件.
   /// - InfoTime(Bitv: Null = MdRtsDecoder.InfoTime)
   /// - 若有 BS.MktSeq 則在此提供 MktSeq(Bitv)
   /// - f9sv_DealFlag(uint8_t): flags
   /// - IsEnumContains(flags, f9sv_DealFlag_DealTimeChanged):
   ///   - DealTime(Bitv: Null=InfoTime)
   ///   - 若沒有 f9sv_DealFlag_DealTimeChanged 旗標, 表示 DealTime 沒異動.
   /// - IsEnumContains(flags, f9sv_DealFlag_TotalQtyLost): BeforeTotalQty(Bitv)
   ///   - 若無此旗標, 則表示 TotalQty 沒問題, 直接使用之前的 TotalQty 累加後續的成交明細即可.
   /// - Count0(uint8_t), 實際筆數=Count0+1;
   /// - DealPQ[Count0+1](Bitv)
   /// - IsEnumContains(flags, f9sv_DealFlag_DealBuyCntChanged):  DealBuyCnt(Bitv)
   /// - IsEnumContains(flags, f9sv_DealFlag_DealSellCntChanged): DealSellCnt(Bitv)
   f9sv_RtsPackType_DealPack,

   /// 委託簿快照(買賣報價), 不一定會填滿檔位, 接收端必須清除未提供的檔位.
   /// - InfoTime(Bitv: Null = MdRtsDecoder.InfoTime)
   /// - 若有 BS.MktSeq 則在此提供 MktSeq(Bitv)
   /// - bstype(1 byte):
   ///   - (bstype & RtBSType::Mask) : RtBSType::OrderBuy, OrderSell, DerivedBuy, DerivedSell;
   ///   - Count 4 bits = (bstype & 0x0f) + 1;
   ///   - 例如: bstype = 0x13: bstype 之後, 提供「第4檔..第1檔」的賣出報價 Pri(Bitv), Qty(Bitv);
   f9sv_RtsPackType_SnapshotBS,
   /// 試算委託簿快照, 後續內容與 SnapshotBS 相同.
   f9sv_RtsPackType_CalculatedBS,

   /// 委託簿異動.
   /// - InfoTime(Bitv: Null = MdRtsDecoder.InfoTime)
   /// - 若有 BS.MktSeq 則在此提供 MktSeq(Bitv)
   /// - count(1 byte):
   ///   - MSB 1 bit = (count & 0x80) = Calculated?
   ///   - Count 7 bits = (count & 7f) + 1; 底下的資料重複次數.
   /// - type(1 byte)
   ///   - (type & 0x0f) : Level;
   ///   - (type & RtBSType::Mask) : RtBSType::OrderBuy, OrderSell, DerivedBuy, DerivedSell;
   ///   - (type & RtBSAction::Mask) : RtBSAction::ChangePQ, ChangeQty, Delete, New;
   /// - Action != RtBSAction::Delete, 則接著提供該檔位的 Pri(Bitv), Qty(Bitv);
   /// - Action == RtBSAction::New, 則先將原本檔為往後移動, 然後填入該檔位的 Pri, Qty;
   f9sv_RtsPackType_UpdateBS,

   /// 盤別(日盤、夜盤...)切換通知.
   /// - InfoTime(Bitv: Null=清盤 or Unchanged)
   /// - TDayYYYYMMDD(uint32_t BigEndian)
   /// - f9fmkt_TradingSessionId(char): Normal, OddLot, FixedPrice, AfterHour...
   /// - f9fmkt_TradingSessionSt(uint8_t): Clear, PreOpen, NonCancelPeriod, Open...
   f9sv_RtsPackType_TradingSessionId,

   /// Twf 基本資訊.
   /// - InfoTime(Bitv: Null=Unchanged)
   /// - TradingMarket(char)
   /// - 如果有 Base.FlowGroup 欄位:      FlowGroup(uint8_t)
   /// - 如果有 Base.StrikePriceDiv 欄位: StrikePriceDecimalLocator(uint8_t)
   /// - 如果有 Base.PriceOrigDiv 欄位:   PriceDecimalLocator(uint8_t)
   /// - 如果有 Base.ShUnit 欄位:         ShUnit(Bitv)
   /// - PriRef(Bitv)
   /// - 漲跌停價 0..N 階(直到封包結束):  PriUpLmt(Bitv) + PriDnLmt(Bitv)
   f9sv_RtsPackType_BaseInfoTw,

   /// DealPack(有InfoTime) + SnapshotBS(無InfoTime);
   /// 使用 f9sv_DealFlag_Calculated 判斷是否為試算.
   f9sv_RtsPackType_DealBS,

   /// 單一商品的全部資料, 通常用在訂閱整棵樹的商品資料回補.
   /// - 底下內容重覆 n 次:
   ///   - SymbId
   ///   - SymbCellsToBitv()
   f9sv_RtsPackType_SnapshotSymbList_NoInfoTime,

   /// 指數值.
   /// - DealTime = Bitv(InfoTime)
   /// - DealTime: YesterdayIndexTime()=昨天收盤指數, ClosedIndexTime()=今日收盤指數, else=指數統計時間.
   /// - Bitv(DealPri * 100): 「* 100」之後,通常為整數值.
   f9sv_RtsPackType_IndexValueV2,
   /// 一次多筆指數.
   /// - DealTime = Bitv(InfoTime)
   /// - 底下內容重覆 1..N 次:
   ///   - IndexId;
   ///   - Bitv(DealPri * 100);
   f9sv_RtsPackType_IndexValueV2List,

   /// 更新單一欄位.
   /// - 僅適用於: tabidx=0..15; fldidx=0..15;
   /// - 底下內容重複 1..N 次:
   ///   - uint8_t nTabFld;
   ///     - tabidx = (nTabFld >> 4);
   ///     - fldidx = (nTabFld & 0x0f);
   ///   - fld->BitvToCell();
   f9sv_RtsPackType_FieldValue_NoInfoTime,
   f9sv_RtsPackType_FieldValue_AndInfoTime,

   /// 更新 Tab 的全部欄位.
   /// - 底下內容重複 1..N 次:
   ///   - uint8_t tabidx;
   ///   - tabs[tabidx].(all fields)->BitvToCell();
   f9sv_RtsPackType_TabValues_NoInfoTime,
   f9sv_RtsPackType_TabValues_AndInfoTime,

   /// 漲跌停價異動.
   f9sv_RtsPackType_PriLmts,

   f9sv_RtsPackType_Count,
};

#ifdef __cplusplus
}
#endif
#endif//__fon9_fmkt_FmdRtsPackType_h__
