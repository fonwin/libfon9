// \file fon9/rc/RcMdRtsDecoder.h
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcMdRtsDecoder_h__
#define __fon9_rc_RcMdRtsDecoder_h__
#include "fon9/rc/RcSeedVisitor.h"

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus
   
   /// 將 "MdRts" 加入 StreamDecoder; 之後可以使用: "$TabName:MdRts:Args"; 的格式.
   /// - 在 f9sv_Initialize() 之後, f9sv_Subscribe() 之前, 執行一次即可.
   /// - 行情訂閱: 訂閱參數=? 如何回補? 如何區分日夜盤?
   ///   回補應可到「前一盤」, 不論前一盤是日盤或夜盤.
   f9sv_CAPI_FN(f9sv_Result) f9sv_AddStreamDecoder_MdRts(void);

   fon9_ENUM(f9sv_DealFlag, uint8_t) {
      /// 試撮價格訊息.
      /// 何時可能會有試撮? 開盤前, 收盤前, 瞬間漲跌幅過大暫停交易期間(保險絲熔斷機制)...
      f9sv_DealFlag_Calculated = 0x01,

      /// 成交時間有異動.
      f9sv_DealFlag_DealTimeChanged = 0x02,
      /// DealBuyCnt 有異動.
      f9sv_DealFlag_DealBuyCntChanged = 0x04,
      /// DealSellCnt 有異動.
      f9sv_DealFlag_DealSellCntChanged = 0x08,

      /// TotalQty 不連續, 藉此判斷是否有遺漏成交明細.
      f9sv_DealFlag_TotalQtyLost = 0x10,

      /// LmtFlags 欄位有異動.
      f9sv_DealFlag_LmtFlagsChanged = 0x20,

      /// 暫緩撮合.
      f9sv_DealFlag_HeldMatchMask = 0xc0,
      /// 暫緩撮合, 瞬間價格趨漲.
      f9sv_DealFlag_HeldMatchTrendRise = 0x80,
      /// 暫緩撮合, 瞬間價格趨跌.
      f9sv_DealFlag_HeldMatchTrendFall = 0x40,
   };

   /// 這裡的定義與台灣證交所 Fmt6 的「漲跌停註記」欄位, bit 位置相同.
   fon9_ENUM(f9sv_DealLmtFlag, uint8_t) {
      /// 一般非漲跌停成交.
      f9sv_DealLmtFlag_None = 0x00,
      /// 漲停成交.
      f9sv_DealLmtFlag_UpLmt = 0x80,
      /// 跌停成交.
      f9sv_DealLmtFlag_DnLmt = 0x40,
      /// 暫緩撮合.
      f9sv_DealLmtFlag_HeldMatchMask = 0x03,
      /// 暫緩撮合, 瞬間價格趨漲.
      f9sv_DealLmtFlag_HeldMatchTrendRise = 0x02,
      /// 暫緩撮合, 瞬間價格趨跌.
      f9sv_DealLmtFlag_HeldMatchTrendFall = 0x01,
   };
      
   fon9_ENUM(f9sv_BSFlag, uint8_t) {
      /// 試撮後剩餘委託簿.
      f9sv_BSFlag_Calculated = 0x01,
      /// 委託簿的買方有異動.
      f9sv_BSFlag_OrderBuy = 0x02,
      /// 委託簿的賣方有異動.
      f9sv_BSFlag_OrderSell = 0x04,
      /// 委託簿的衍生買方有異動.
      f9sv_BSFlag_DerivedBuy = 0x10,
      /// 委託簿的衍生賣方有異動.
      f9sv_BSFlag_DerivedSell = 0x20,
   };
   fon9_ENUM(f9sv_BSLmtFlag, uint8_t) {
      /// 最佳一檔買進 = 漲停.
      f9sv_BSLmtFlag_UpLmtBuy = 0x20,
      /// 最佳一檔買進 = 跌停.
      f9sv_BSLmtFlag_DnLmtBuy = 0x10,
      /// 最佳一檔賣出 = 漲停.
      f9sv_BSLmtFlag_UpLmtSell = 0x08,
      /// 最佳一檔賣出 = 跌停.
      f9sv_BSLmtFlag_DnLmtSell = 0x04,
   };

   /// fon9/fmkt/MdRtsTypes.hpp
   /// enum class RtsPackType : uint8_t;
   fon9_ENUM(f9sv_RtsPackType, uint8_t) {
      /// 打包成交明細, 一個 DealPack 可能觸發多次「交易明細」異動事件.
      f9sv_RtsPackType_DealPack,

      /// 委託簿快照(買賣報價).
      f9sv_RtsPackType_SnapshotBS,
      /// 試算委託簿快照.
      f9sv_RtsPackType_CalculatedBS,
      /// 委託簿異動.
      f9sv_RtsPackType_UpdateBS,

      f9sv_RtsPackType_TradingSessionId,

      f9sv_RtsPackType_BaseInfoTw,

      /// DealPack(有InfoTime) + SnapshotBS(無InfoTime);
      /// 使用 f9fmkt::DealFlag::Calculated 判斷是否為試算.
      f9sv_RtsPackType_DealBS,

      f9sv_RtsPackType_SnapshotSymbList_NoInfoTime,
      f9sv_RtsPackType_IndexValueV2,
      f9sv_RtsPackType_IndexValueV2List,
      f9sv_RtsPackType_FieldValue_NoInfoTime,
      f9sv_RtsPackType_FieldValue_AndInfoTime,
      f9sv_RtsPackType_TabValues_NoInfoTime,
      f9sv_RtsPackType_TabValues_AndInfoTime,
   };

#ifdef __cplusplus
}//extern "C"
#endif//__cplusplus
#endif//__fon9_rc_RcMdRtsDecoder_h__
