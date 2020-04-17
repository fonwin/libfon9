// \file fon9/fmkt/SymbBreakSt.hpp
//
// 商品暫停交易 相關欄位.
//
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbBreakSt_hpp__
#define __fon9_fmkt_SymbBreakSt_hpp__
#include "fon9/fmkt/Symb.hpp"
#include "fon9/fmkt/FmktTypes.hpp"

/// 暫停交易原因.
/// 此處定義與期交所「I140. FUNCTION-CODE = 20x, 30x」的定義相同.
fon9_ENUM(f9fmkt_BreakReason, uint8_t) {
   /// 正常,或沒有提供原因.
   f9fmkt_BreakReason_Normal = 0x00,
   /// 交易所(期交所)因故暫停交易.
   f9fmkt_BreakReason_ByExg = 0x01,
   /// 證券市場異常暫停交易 或 交易所(期交所)系統故障部分商品提早收盤.
   f9fmkt_BreakReason_SystemErr = 0x02,
   /// 個股訊息面暫停交易
   f9fmkt_BreakReason_StockMessage = 0x03,
   /// 盤後商品契約資料尚未完成.
   f9fmkt_BreakReason_NotReady = 0x04,
   /// 前日暫停交易商品恢復交易公告.
   f9fmkt_BreakReason_Resumed = 0x99,
};

namespace fon9 { namespace fmkt {

/// 是否有暫停交易, 應判斷 (BreakHHMMSS_ != RestartHHMMSS_) 則有暫停交易;
struct SymbBreakSt_Data {
   /// 暫停交易起始時間.
   uint32_t    BreakHHMMSS_;
   /// 重新開始收單時間.
   uint32_t    RestartHHMMSS_;
   /// 重新開盤時間.
   uint32_t    ReopenHHMMSS_;

   f9fmkt_BreakReason   Reason_;
   char                 Padding___[3];

   SymbBreakSt_Data() {
      this->Clear();
   }
   void Clear() {
      memset(this, 0, sizeof(*this));
   }
   bool operator==(const SymbBreakSt_Data& rhs) const {
      return memcmp(this, &rhs, sizeof(*this)) == 0;
   }
   bool operator!=(const SymbBreakSt_Data& rhs) const {
      return !this->operator==(rhs);
   }
};

/// \ingroup fmkt
/// 商品資料的擴充: 暫停交易.
fon9_API_TEMPLATE_CLASS(SymbBreakSt, SimpleSymbData, SymbBreakSt_Data);

/// 期貨欄位包含: BreakTime, RestartTime, ReopenTime, Reason;
/// 時間格式為整數 HHMMSS;
fon9_API seed::Fields SymbBreakSt_MakeFieldsTwf();

/// 證券欄位包含: BreakTime, RestartTime;
/// 時間格式為整數 HHMMSS;
fon9_API seed::Fields SymbBreakSt_MakeFieldsTws();

} } // namespaces
#endif//__fon9_fmkt_SymbBreakSt_hpp__
