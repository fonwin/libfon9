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

class fon9_API LvPriStepVect {
   fon9_NON_COPYABLE(LvPriStepVect);
   LvPriStep*  Vect_;
   void FreeThis();
public:
   LvPriStepVect() : Vect_{} {
   }
   LvPriStepVect(LvPriStepVect&& rhs) : Vect_{rhs.Vect_} {
      rhs.Vect_ = nullptr;
   }
   LvPriStepVect& operator=(LvPriStepVect&& rhs) {
      this->FreeThis();
      this->Vect_ = rhs.Vect_;
      rhs.Vect_ = nullptr;
      return *this;
   }
   ~LvPriStepVect() {
      this->FreeThis();
   }
   const LvPriStep* Get() const {
      return this->Vect_;
   }
   void FromStr(fon9::StrView cfgs);
};

/// \ingroup fmkt
/// \retval empty()  steps == nullptr
/// \retval Step     steps->LvTop_ == Pri::max();
/// \retval p1=t1|p2=t2|...|maxtick
fon9_API CharVector LvPriStep_ToStr(const LvPriStep* steps);

/// \ingroup fmkt
/// 計算漲跌停價.
/// - 若 pUpLmt != nullptr, 則 *pUpLmt 必須先填入最高價(或 Null), 例如: 台灣證交所應填入 10000,
///   若漲停計算結果 >= *pUpLmt, 則會自動用該價格降低一檔.
/// - 若 pDnLmt != nullptr, *pDnLmt 必須先填入最低價(或 Null), 例如: 台灣證交所應填入 0,
///   若跌停計算結果 <= *pDnLmt, 則會用首個 steps->Step_;
fon9_API void CalcLmt(const LvPriStep* steps, Pri ref, double rate, Pri* pUpLmt, Pri* pDnLmt);

/// \ingroup fmkt
/// 尋找最接近但不超過的價格檔位, steps 必須有效.
/// assert(pri <= retval->LvTop_);
static inline const LvPriStep* FindPriTickStep(const LvPriStep* steps, Pri pri) {
   while (steps->LvTop_ < pri)
      ++steps;
   return steps;
}
/// \ingroup fmkt
/// 尋找最接近但不超過的價格檔位, 若 steps == nullptr, 則直接返回 pri;
static inline Pri FindPriTickSize(const LvPriStep* steps, Pri pri) {
   return steps ? (FindPriTickStep(steps, pri)->Step_) : pri;
}

/// \ingroup fmkt
/// 檢查 TickSize 是否正確.
static inline bool CheckPriTickSize(const LvPriStep* steps, Pri pri) {
   if (pri.GetOrigValue() < 0)
      pri = Pri{} - pri;
   const auto tickSize = FindPriTickSize(steps, pri);
   return (pri % tickSize).IsZero();
}


/// \ingroup fmkt
/// 價格從 priFrom 往上加 count 檔.
/// 若 priFrom 不符合檔位, 則先往下調整到檔位, 然後再往上升 count 檔;
/// 不考慮 upLmt 是否在符合檔位.
fon9_API Pri MoveTicksUp(const LvPriStep* steps, Pri priFrom, uint16_t count, Pri upLmt);
/// \ingroup fmkt
/// 價格從 priFrom 往下減 count 檔.
/// 若 priFrom 不符合檔位, 則先往上調整到檔位, 然後再往下降 count 檔;
/// 不考慮 dnLmt 是否在符合檔位.
fon9_API Pri MoveTicksDn(const LvPriStep* steps, Pri priFrom, uint16_t count, Pri dnLmt);

} } // namespaces
#endif//__fon9_fmkt_FmktTools_hpp__
