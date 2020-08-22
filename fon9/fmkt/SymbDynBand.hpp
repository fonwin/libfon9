// \file fon9/fmkt/SymbDynBand.hpp
//
// 動態價格穩定措施相關欄位.
//
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbDynBand_hpp__
#define __fon9_fmkt_SymbDynBand_hpp__
#include "fon9/fmkt/Symb.hpp"
#include "fon9/fmkt/FmktTypes.hpp"

/// 「動態價格穩定措施」調整範圍時, 告知調整方向.
/// 此處定義與期交所「I140. FUNCTION-CODE = 40x」的定義相同.
fon9_ENUM(f9fmkt_DynBandSide, uint8_t) {
   /// 上下限同時調整.
   f9fmkt_DynBandSide_All = 0x00,
   /// 期貨調整買方；選擇權調整買權上限、賣權下限.
   f9fmkt_DynBandSide_Long = 0x01,
   /// 期貨調整賣方；選擇權調整買權下限、賣權上限.
   f9fmkt_DynBandSide_Short = 0x02,
   /// 取得當盤最新參數，選擇權調整買權上限、賣權下限.
   f9fmkt_DynBandSide_OptLong = 0x03,
   /// 取得當盤最新參數，選擇權調整買權下限、賣權上限.
   f9fmkt_DynBandSide_OptShort = 0x04,
};

/// 暫停動態價格穩定原因.
/// 此處定義與期交所「I140. FUNCTION-CODE = 40x」的定義相同.
fon9_ENUM(f9fmkt_DynBandStopReason, uint8_t) {
   f9fmkt_DynBandStopReason_None = 0x00,
   /// 因應特殊市況.
   f9fmkt_DynBandStopReason_MarketSt = 0x01,
   /// 動態價格穩定措施資訊異常.
   f9fmkt_DynBandStopReason_InfoErr = 0x02,
   /// 因資訊異常基準價無法計算.
   f9fmkt_DynBandStopReason_BasePri = 0x03,
};

fon9_ENUM(f9fmkt_DynBandSt, uint8_t) {
   /// 不適用「動態價格穩定措施」.
   f9fmkt_DynBandSt_Disabled,
   /// 適用「動態價格穩定措施」.
   f9fmkt_DynBandSt_Enabled,
   /// 暫停「動態價格穩定措施」.
   f9fmkt_DynBandSt_Suspended,
   /// 恢復「動態價格穩定措施」.
   f9fmkt_DynBandSt_Resumed,
   /// 調整「動態價格穩定」範圍.
   f9fmkt_DynBandSt_Ranged,
};

//--------------------------------------------------------------------------//

namespace fon9 { namespace fmkt {

/// 期交所目前定義 9v9, 所以範圍是: 0.0 ~ 9.9;
using SymbDynBandRange = fon9::Decimal<uint16_t, 2>;

struct SymbDynBand_Data {
   uint32_t                   StHHMMSS_;
   f9fmkt_DynBandSt           DynBandSt_;
   f9fmkt_DynBandStopReason   Reason_;
   f9fmkt_DynBandSide         SideType_;
   char                       PaddingBeforeRange___[5];
   /// SideType_ == f9fmkt_DynBandSide_Long;
   /// 期貨調整買方；選擇權調整買權上限、賣權下限.
   SymbDynBandRange           RangeL_;
   /// SideType_ == f9fmkt_DynBandSide_Short;
   /// 期貨調整賣方；選擇權調整買權下限、賣權上限.
   SymbDynBandRange           RangeS_;

   SymbDynBand_Data() {
      this->Clear();
   }
   void Clear() {
      ForceZeroNonTrivial(this);
   }
   bool IsEqualExcludeRange(const SymbDynBand_Data& rhs) const {
      return memcmp(this, &rhs, fon9_OffsetOf(SymbDynBand_Data, PaddingBeforeRange___)) == 0;
   }
   void CopyExcludeRange(const SymbDynBand_Data& rhs) {
      memcpy(this, &rhs, fon9_OffsetOf(SymbDynBand_Data, PaddingBeforeRange___));
   }

   bool IsEqualExcludeSide(const SymbDynBand_Data& rhs) const {
      return memcmp(this, &rhs, fon9_OffsetOf(SymbDynBand_Data, SideType_)) == 0;
   }
   void CopyExcludeSide(const SymbDynBand_Data& rhs) {
      memcpy(this, &rhs, fon9_OffsetOf(SymbDynBand_Data, SideType_));
   }
};

/// \ingroup fmkt
/// 商品資料的擴充: 動態價格穩定措施.
fon9_API_TEMPLATE_CLASS(SymbDynBand, SimpleSymbData, SymbDynBand_Data);

fon9_API seed::Fields SymbDynBand_MakeFields();

} } // namespaces
#endif//__fon9_fmkt_SymbDynBand_hpp__
