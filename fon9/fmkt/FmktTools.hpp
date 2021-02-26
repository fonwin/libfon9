// \file fon9/fmkt/FmktTools.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_FmktTools_hpp__
#define __fon9_fmkt_FmktTools_hpp__
#include "fon9/fmkt/FmktTypes.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 價格升降單位設定.
/// 當價格 < LvTop_ 時, 升降單位為 Step_;
struct LvPriStep {
   Pri   LvTop_;
   Pri   Step_;
};

/// \ingroup fmkt
/// 計算漲跌停價.
/// - 若 pUpLmt != nullptr, 則 *pUpLmt 必須先填入最高價(或 Null), 例如: 台灣證交所應填入 10000,
///   若漲停計算結果 >= *pUpLmt, 則會自動用該價格降低一檔.
/// - 若 pDnLmt != nullptr, *pDnLmt 必須先填入最低價(或 Null), 例如: 台灣證交所應填入 0,
///   若跌停計算結果 <= *pDnLmt, 則會用首個 steps->Step_;
fon9_API void CalcLmt(const LvPriStep* steps, Pri ref, double rate, Pri* pUpLmt, Pri* pDnLmt);

} } // namespaces
#endif//__fon9_fmkt_FmktTools_hpp__
