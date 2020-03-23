// \file fon9/FlowControlCalc.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_FlowControlCalc_hpp__
#define __fon9_FlowControlCalc_hpp__
#include "fon9/TimeStamp.hpp"

namespace fon9 {

/// \ingroup Misc.
/// 計算一段時間內的平均流量管制.
/// - 計算方式: 將時間內分成 n 等分, 流量分攤到分割裡面計算.
///   - 例: 1 秒流量限制 100K, 分成 20 等分, 則每 50ms 之內, 不應該超過 5K.
/// - 如果瞬間超過多個分割流量, 則管制時間會拉長,
///   並將超過的部分, 分攤到接下來的分割.
/// - thread unsafe. 如果需要 thread safe, 請配合 MustLock<FlowControlCalc>;
class fon9_API FlowControlCalc {
   /// 從 CalcTimeSeg_ 開始到現在, 已用了多少流量.
   uint64_t       UsedCount_{};
   /// CalcTime_ = time / SegInterval_;
   uint64_t       CalcTimeSeg_;

   /// 每個分割的最大流量.
   uint64_t       SegLmt_{};
   /// 每個分割的時間長度.
   TimeInterval   SegInterval_;

   /// 單位時間內極限流量, 超過則強迫管制.
   uint64_t       FcMax_{};
   /// 極限流量的單位時間.
   TimeInterval   FcInterval_;

public:
   FlowControlCalc() = default;

   FlowControlCalc(uint64_t fcMax, TimeInterval fcInterval, unsigned segCount) {
      this->SetFlowControlArgs(fcMax, fcInterval, segCount);
   }

   void ClearArgs() {
      this->FcMax_ = 0;
   }
   bool HasControl() const {
      return this->FcMax_ > 0;
   }
   uint64_t UsedCount() const {
      return this->UsedCount_;
   }
   uint64_t CalcTimeSeg() const {
      return this->CalcTimeSeg_;
   }

   void SetFlowControlArgs(uint64_t fcMax, TimeInterval fcInterval, unsigned segCount);

   /// \retval =0 使用量累計成功, 沒超過管制.
   /// \retval >0 使用量累計成功, 但超過管制, 建議延遲 retval.
   /// \retval <0 使用量累計失敗, 超過管制, 必須延遲 -retval.
   TimeInterval CalcUsed(TimeStamp now, uint64_t usedCount);

   /// \retval =0 使用量累計成功, 沒超過管制.
   /// \retval >0 使用量累計成功, 但超過管制, 建議延遲 retval.
   /// \retval <0 使用量累計成功, 但超過極限, 必須延遲 -retval.
   TimeInterval ForceUsed(TimeStamp now, uint64_t usedCount) {
      TimeInterval retval = this->CalcUsed(now, usedCount);
      if (retval.GetOrigValue() < 0)
         this->UsedCount_ += usedCount;
      return retval;
   }
};

} // namespace
#endif//__fon9_FlowControlCalc_hpp__
