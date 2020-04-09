// \file fon9/fmkt/MdRtsTypes.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_MdRtsTypes_hpp__
#define __fon9_fmkt_MdRtsTypes_hpp__
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 { namespace fmkt {

enum class MdRtsKind : uint32_t {
   All = 0xff,
   /// 基本資料訊息.
   Base = 0x01,
   /// 參考價訊息.
   Ref = 0x02,
   Deal = 0x04,
   BS = 0x08,
   /// 交易時段狀態.
   TradingSessionSt = 0x80,
};
fon9_ENABLE_ENUM_BITWISE_OP(MdRtsKind);

/// \ingroup fmkt
/// - 預期 RtsPackType 應在 0..0x7f 之間.
///   若超過 0x7f, 則應思考編碼端及解碼端應如何處理.
/// - 封包基本格式:
///   - RtsPackType(1 byte)
///   - InfoTime(Bitv:DayTime, 可能為 Null, 表示 InfoTime 與前一個封包相同)
///   - 後續內容由各 RtsPackType 自行定義.
enum class RtsPackType : uint8_t {
   /// 打包成交明細.
   /// - 收到封包者: 一個 DealPack 可能觸發多次「交易明細」異動事件.
   /// - InfoTime(Bitv: Null = MdRtsDecoder.InfoTime)
   /// - f9fmkt::DealFlag(uint8_t): flags
   /// - IsEnumContains(flags, f9fmkt::DealFlag::DealTimeChanged):
   ///   - DealTime(Bitv: Null=InfoTime)
   ///   - 若沒有 f9fmkt::DealFlag::DealTimeChanged 旗標, 表示 DealTime 沒異動.
   /// - IsEnumContains(flags, f9fmkt::DealFlag::TotalQtyLost): BeforeTotalQty(Bitv)
   ///   - 若無此旗標, 則表示 TotalQty 沒問題, 直接使用之前的 TotalQty 累加後續的成交明細即可.
   /// - Count0(uint8_t), 實際筆數=Count0+1;
   /// - DealPQ[Count0+1](Bitv)
   /// - IsEnumContains(flags, f9fmkt::DealFlag::DealBuyCntChanged):  DealBuyCnt(Bitv)
   /// - IsEnumContains(flags, f9fmkt::DealFlag::DealSellCntChanged): DealSellCnt(Bitv)
   DealPack,

   /// 委託簿快照(買賣報價), 不一定會填滿檔位, 接收端必須清除未提供的檔位.
   /// - InfoTime(Bitv: Null = MdRtsDecoder.InfoTime)
   /// - first(1 byte):
   ///   - (first & RtBSType::Mask) : RtBSType::OrderBuy, OrderSell, DerivedBuy, DerivedSell;
   ///   - Count 4 bits = (first & 0x0f) + 1;
   ///   - 例如: first = 0x13: first 之後, 提供「第4檔..第1檔」的賣出報價 Pri(Bitv), Qty(Bitv);
   SnapshotBS,
   /// 試算委託簿快照, 後續內容與 SnapshotBS 相同.
   CalculatedBS,

   /// 委託簿異動.
   /// - InfoTime(Bitv: Null = MdRtsDecoder.InfoTime)
   /// - first(1 byte):
   ///   - MSB 1 bit = (first & 0x80) = Calculated?
   ///   - Count 7 bits = (first & 7f) + 1; 底下的資料重複次數.
   /// - type(1 byte)
   ///   - (type & 0x0f) : Level;
   ///   - (type & RtBSType::Mask) : RtBSType::OrderBuy, OrderSell, DerivedBuy, DerivedSell;
   ///   - (type & RtBSAction::Mask) : RtBSAction::ChangePQ, ChangeQty, Delete, New;
   /// - Action != RtBSAction::Delete, 則接著提供該檔位的 Pri(Bitv), Qty(Bitv);
   /// - Action == RtBSAction::New, 則先將原本檔為往後移動, 然後填入該檔位的 Pri, Qty;
   UpdateBS,

   /// 盤別狀態通知.
   /// - InfoTime(Bitv: Null=清盤 or Unchanged)
   /// - TDayYYYYMMDD(uint32_t BigEndian)
   /// - f9fmkt_TradingSessionId(char)
   /// - f9fmkt_TradingSessionSt(uint8_t)
   TradingSessionSt,

   /// Twf 基本資訊.
   /// - InfoTime(Bitv: Null=Unchanged)
   /// - TradingMarket(char)
   /// - 如果有 Base.FlowGroup: FlowGroup(uint8_t)
   /// - 如果有 Base.StrikePriceDiv: StrikePriceDecimalLocator(uint8_t)
   /// - 如果有 Base.ShUnit: ShUnit(Bitv)
   /// - PriRef(Bitv)
   /// - PriUpLmt(Bitv)
   /// - PriDnLmt(Bitv)
   BaseInfoTw,

   /// DealPack + SnapshotBS(無InfoTime);
   /// 使用 f9fmkt::DealFlag::Calculated 判斷是否為試算.
   DealBS,

   /// 單一商品的全部資料, 通常用在訂閱整棵樹的商品資料回補.
   /// - 底下內容重覆 n 次:
   ///   - SymbId
   ///   - SymbCellsToBitv()
   SnapshotSymbList,

   /// 指數值.
   /// - Bitv(DealTime): YesterdayIndexTime()=昨天收盤指數, ClosedIndexTime()=今日收盤指數, else=指數統計時間.
   /// - Bitv(DealPri * 100): 「* 100」之後,通常為整數值.
   IndexValueV2,
   /// 一次多筆指數.
   /// - DealTime
   /// - 底下內容重覆 n 次:
   ///   - IndexId;
   ///   - Bitv(DealPri * 100);
   IndexValueV2List,

   Count,
};
static_assert(cast_to_underlying(RtsPackType::Count) <= 0x7f, "");

/// static_assert(ClosedIndexTime().GetOrigValue() == 99 * 24 * 60 * 60 * DayTime::Divisor, "");
/// = 8553600000000
constexpr DayTime ClosedIndexTime() {
   return fon9::TimeInterval_Day(99);
}
/// static_assert(YesterdayIndexTime().GetOrigValue() == -1 * 24 * 60 * 60 * DayTime::Divisor, "");
/// = -86400000000
constexpr DayTime YesterdayIndexTime() {
   return fon9::TimeInterval_Day(-1);
}

enum class RtBSType : uint8_t {
   // --xx ----
   Mask = 0x30,
   OrderBuy = 0x00,
   OrderSell = 0x10,
   DerivedBuy = 0x20,
   DerivedSell = 0x30,
};
enum class RtBSAction : uint8_t {
   // xx-- ----
   Mask = 0xc0,
   New = 0x00,
   Delete = 0x40,
   ChangePQ = 0x80,
   ChangeQty = 0xc0,
};

//--------------------------------------------------------------------------//

inline MdRtsKind StrToMdRtsKind(StrView* args) {
   MdRtsKind res = static_cast<MdRtsKind>(HexStrTo(args));
   return(res == MdRtsKind{} ? MdRtsKind::All : res);
}
fon9_API MdRtsKind GetMdRtsKind(RtsPackType pkType);

class SymbBS;
fon9_API void MdRtsPackSnapshotBS(RevBuffer& rbuf, const SymbBS& symbBS);

} } // namespaces
#endif//__fon9_fmkt_MdRtsTypes_hpp__
