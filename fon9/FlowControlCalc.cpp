/// \file fon9/FlowControlCalc.cpp
/// \author fonwinz@gmail.com
#include "fon9/FlowControlCalc.hpp"

namespace fon9 {

TimeInterval FlowControlCalc::CalcUsed(TimeStamp now, uint64_t usedCount) {
   if (fon9_UNLIKELY(!this->HasControl()))
      return TimeInterval{};
   assert(this->FcInterval_.GetOrigValue() > 0
          && this->SegInterval_.GetOrigValue() > 0
          && this->SegLmt_ > 0
          && this->FcMax_ > 0);
   int64_t nowSeg = now.GetOrigValue() / this->SegInterval_.GetOrigValue();
   if (int64_t segCount = signed_cast(nowSeg - this->CalcTimeSeg_)) {
      if (segCount > 0) {
         uint64_t allowCount = segCount * this->SegLmt_;
         if (this->UsedCount_ > allowCount)
            this->UsedCount_ -= allowCount;
         else
            this->UsedCount_ = 0;
      }
      this->CalcTimeSeg_ = unsigned_cast(nowSeg);
   }
   if (fon9_LIKELY(this->UsedCount_ + usedCount <= this->FcMax_ || usedCount > this->FcMax_)) {
      if (fon9_LIKELY((this->UsedCount_ += usedCount) <= this->SegLmt_))
         return TimeInterval{};
      return this->SegInterval_ * signed_cast(this->UsedCount_ / this->SegLmt_);
   }
   // 加上 usedCount 之後, 會超過極限流量,
   // 所以必須等候現有 this->UsedCount_ 消化完畢的時間.
   TimeInterval retval;
   retval.SetOrigValue(
      - (signed_cast(this->UsedCount_ / this->FcMax_) * this->FcInterval_.GetOrigValue())
      - (1 + signed_cast((this->UsedCount_ % this->FcMax_) / this->SegLmt_)) * this->SegInterval_.GetOrigValue());
   return retval;
}
void FlowControlCalc::SetFlowControlArgs(uint64_t fcMax, TimeInterval fcInterval, unsigned segCount) {
   if (fcInterval.GetOrigValue() <= 0) {
      this->ClearArgs();
      return;
   }
   this->CalcTimeSeg_ = 0;
   this->UsedCount_ = 0;
   this->FcMax_ = fcMax;
   this->FcInterval_ = fcInterval;
   if (segCount > 0) {
      this->SegLmt_ = fcMax / segCount;
      this->SegInterval_ = fcInterval / segCount;
      if (this->SegLmt_ > 0 && this->SegInterval_.GetOrigValue() > 0)
         return;
   }
   this->SegLmt_ = fcMax;
   this->SegInterval_ = fcInterval;
}

} // namespace
