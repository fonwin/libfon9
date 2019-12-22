// \file fon9/FlowCounter.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_FlowCounter_hpp__
#define __fon9_FlowCounter_hpp__
#include "fon9/TimeStamp.hpp"
#include "fon9/MustLock.hpp"
#include "fon9/ConfigParser.hpp"
#include <vector>

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
/// \ingroup Misc.
/// 流量管制參數.
struct fon9_API FlowCounterArgs {
   /// 流量管制: FcCount_(筆數) / FcTimeMS_(時間單位)
   /// 流量管制筆數(0=不限制).
   uint16_t FcCount_;
   /// 流量管制, 時間單位, 1000=每秒.
   uint16_t FcTimeMS_;

   void Clear() {
      this->FcCount_ = 0;
      this->FcTimeMS_ = 1000;
   }

   /// tag = "Fc"; value = "count/ms"; 或 value = "count" (預設 1000 ms);
   ConfigParser::Result OnTagValue(StrView tag, StrView& value);
   ConfigParser::Result OnValue(StrView& value);
};

/// \ingroup Misc.
/// 流量管制: 最多筆數/單位時間.
class FlowCounter {
   using Times = std::vector<TimeStamp>;
   Times          SentTimes_;
   unsigned       NextIndex_{0};
   TimeInterval   TimeUnit_;
   TimeInterval InternalCheck(TimeStamp now, const size_t maxc) {
      assert(maxc > 0);
      if (this->NextIndex_ >= maxc)
         this->NextIndex_ = 0;
      TimeInterval ti = this->TimeUnit_ - (now - this->SentTimes_[this->NextIndex_]);
      if (fon9_LIKELY(ti.GetOrigValue() <= 0))
         return TimeInterval{};
      return ti;
   }
public:
   FlowCounter() = default;
   FlowCounter(unsigned count, TimeInterval timeUnit) {
      this->Resize(count, timeUnit);
   }
   FlowCounter(const FlowCounterArgs& args) {
      this->Resize(args);
   }
   /// 設定單位時間內可用筆數.
   /// \retval true  需要流量管制.
   /// \retval false 不需流量管制.
   bool Resize(unsigned count, TimeInterval timeUnit) {
      this->SentTimes_.clear();
      if (count > 0 && timeUnit.GetOrigValue() > 0) {
         this->SentTimes_.resize(count);
         this->TimeUnit_ = timeUnit;
         return true;
      }
      return false;
   }
   bool Resize(const FlowCounterArgs& args) {
      return this->Resize(args.FcCount_, TimeInterval_Millisecond(args.FcTimeMS_));
   }
   /// 檢查現在是否需要管制.
   /// 還要等 retval 才解除管制.
   /// retval.GetOrigValue() <= 0 表示不用管制.
   TimeInterval Check(TimeStamp now = UtcNow()) {
      if (const auto maxc = this->SentTimes_.size())
         return this->InternalCheck(now, maxc);
      return TimeInterval{};
   }
   /// - 若現在不用管制, 則 retval.GetOrigValue() <= 0; 並設定使用一筆.
   /// - 若現在需要管制, 則 retval = 解除管制需要等候的時間.
   TimeInterval Fetch(TimeStamp now = UtcNow()) {
      if (const auto maxc = this->SentTimes_.size()) {
         auto ti = this->InternalCheck(now, maxc);
         if (ti.GetOrigValue() > 0)
            return ti;
         this->SentTimes_[this->NextIndex_++] = now;
      }
      return TimeInterval{};
   }
   /// 配合 Check().GetOrigValue() <= 0 使用.
   /// 強制使用一筆流量.
   void ForceUsed(TimeStamp now) {
      if (this->SentTimes_.empty())
         return;
      assert(this->InternalCheck(now, this->SentTimes_.size()).GetOrigValue() <= 0);
      this->SentTimes_[this->NextIndex_++] = now;
   }
};

class FlowCounterThreadSafe : public MustLock<FlowCounter> {
   fon9_NON_COPY_NON_MOVE(FlowCounterThreadSafe);
   using base = MustLock<FlowCounter>;
   bool  IsNeedsLock_{false};
public:
   using base::base;
   FlowCounterThreadSafe() = default;

   bool Resize(unsigned count, TimeInterval timeUnit) {
      Locker lk{*this};
      return this->IsNeedsLock_ = lk->Resize(count, timeUnit);
   }
   TimeInterval Check() {
      return (this->IsNeedsLock_ ? Locker{*this}->Check() : TimeInterval{});
   }
   TimeInterval Fetch() {
      return (this->IsNeedsLock_ ? Locker{*this}->Fetch() : TimeInterval{});
   }
};
fon9_WARN_POP;

} // namespace
#endif//__fon9_FlowCounter_hpp__
