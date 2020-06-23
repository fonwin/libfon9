// \file fon9/fmkt/FmdTypes.h
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_FmdTypes_h__
#define __fon9_fmkt_FmdTypes_h__
#include "fon9/sys/Config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 在 SymbDealData (fon9/fmkt/SymbDealData.hpp) 裡面, 提供最後異動了那些欄位.
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
//--------------------------------------------------------------------------//      
/// 在 SymbBSData (fon9/fmkt/SymbBSData.hpp) 裡面, 提供最後異動的原因.
fon9_ENUM(f9sv_BSFlag, uint8_t) {
   /// 試撮後剩餘委託簿.
   f9sv_BSFlag_Calculated = 0x01,
   /// 委託簿的買方有異動, 或快照有買方資料.
   f9sv_BSFlag_OrderBuy = 0x02,
   /// 委託簿的賣方有異動, 或快照有賣方資料.
   f9sv_BSFlag_OrderSell = 0x04,
   /// 委託簿的衍生買方有異動, 或快照有衍生買方資料.
   f9sv_BSFlag_DerivedBuy = 0x10,
   /// 委託簿的衍生賣方有異動, 或快照有衍生賣方資料.
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

#ifdef __cplusplus
}
#endif
#endif//__fon9_fmkt_FmdTypes_h__
