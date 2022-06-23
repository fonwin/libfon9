/// \file fon9/SchTask.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_SchTask_hpp__
#define __fon9_SchTask_hpp__
#include "fon9/Timer.hpp"
#include "fon9/buffer/RevBuffer.hpp"
#include <bitset>

namespace fon9 {

fon9_MSC_WARN_DISABLE(4820);
enum class SchSt {
   Unknown,
   Restart,
   Stopped,
   Stopping,
   InSch,
   OutSch,
   Disposing,
};

/// \ingroup Thrs
/// 排程設定.
struct fon9_API SchConfig {
   /// 星期的設定, 以起始時間(StartTime_)的星期幾為準.
   std::bitset<kWeekdayCount>  Weekdays_;
   /// 排程設定(時間、星期)的 TimeZone 調整值.
   TimeZoneOffset TimeZoneAdj_{};
   CharVector     TimeZoneName_;
   /// in sch:
   /// - StartTime_ < EndTime_:  結束日=開始日:   (StartTime_ <= now && now <= EndTime_)
   /// - StartTime_ >= EndTime_: 結束日=開始日+1: (StartTime_ <= now || now <= EndTime_)
   DayTimeSec  StartTime_{};
   DayTimeSec  EndTime_{};

   bool operator==(const SchConfig& rhs) const {
      return this->Weekdays_ == rhs.Weekdays_
         && this->TimeZoneAdj_ == rhs.TimeZoneAdj_
         && this->TimeZoneName_ == rhs.TimeZoneName_
         && this->StartTime_ == rhs.StartTime_
         && this->EndTime_ == rhs.EndTime_;
   }
   bool operator!=(const SchConfig& rhs) const {
      return !(*this == rhs);
   }

   struct CheckResult {
      TimeStamp NextCheckTime_;
      /// Check() 之後的結果只會有 SchSt::InSch 或 SchSt::OutSch 兩種情形;
      SchSt     SchSt_;
      /// this->IsChanged_ = ((beforeSt == SchSt::InSch) != (this->SchSt_ == SchSt::InSch));
      bool      IsChanged_;
   };

   SchConfig() {
      this->SetAlwaysInSch();
   }
   SchConfig(const StrView& cfgstr) {
      this->Parse(cfgstr);
   }

   /// 檢查 now 是否在排程時間內 & 計算下次檢查時間.
   /// 必須知道 beforeSt 的情況:
   /// - 只有 StartTime_, 沒有 EndTime_ 的排程.
   CheckResult Check(TimeStamp now, SchSt beforeSt) const;

   /// 取得下次 SchIn 的時間.
   /// - 如果 beforeSt != SchIn, 且 now 為 SchIn, 則返回 now;
   /// - 如果 beforeSt == SchIn, 且 now 為 SchIn, 則返回下次 SchIn 的時間.
   /// - 如果 beforeSt == SchIn, 且沒有 EndTime_, 則沒有下次 SchIn 的時間(返回 0).
   TimeStamp GetNextSchIn(TimeStamp now, SchSt beforeSt) const;

   /// 設定一直都在時間內 & 沒有結束時間.
   /// - "Weekdays=0123456|Start=00:00:00|End=|TZ=本機localtime offset(GetLocalTimeZoneOffset())"
   void SetAlwaysInSch();
   bool IsAlwaysInSch() const;
   /// 設定一直都在時間外: this->Weekdays_.reset();
   void SetAlwaysOutSch();
   bool IsAlwaysOutSch() const {
      return !this->Weekdays_.any();
   }

   /// 先設定成預設值(永遠在時間內), 然後解析 cfgstr.
   void Parse(StrView cfgstr);
};

fon9_API void RevPrint(RevBuffer& rbuf, const SchConfig& schcfg);

/// \ingroup Thrs
/// 主動排程項目.
/// 當 Start() 或 Restart() 之後, 會在 DefaultTimerThread 觸發排程事件.
class fon9_API SchTask {
   fon9_NON_COPY_NON_MOVE(SchTask);
   struct Timer : public DataMemberTimer {
      fon9_NON_COPY_NON_MOVE(Timer);
      Timer() = default;
      virtual void EmitOnTimer(TimeStamp now) override;
   };
   Timer Timer_;

protected:
   struct SchImplBase {
      SchConfig   Config_;
      SchSt       SchSt_{SchSt::Unknown};
   };
   using SchImpl = MustLock<SchImplBase>;
   SchImpl  SchImpl_;
   bool SetSchState(SchSt st);
   /// 當 [in sch => out sch] or [out sch => in sch] 時觸發事件.
   /// 在 Timer thread 觸發事件, 觸發時沒有鎖住任何東西.
   virtual void OnSchTask_StateChanged(bool isInSch) = 0;
   /// 預設 do nothing, 僅用來通知下次檢查時間.
   virtual void OnSchTask_NextCheckTime(const SchConfig::CheckResult&);

public:
   SchTask() = default;
   virtual ~SchTask();

   /// 啟動 SchTask.
   /// 剛建立(或停止後)的 SchTask 一律在 SchOut 狀態.
   /// - 剛建立(或停止後)的第一次觸發必定是 sch in 事件.
   /// - 返回 Start() 前的狀態:
   ///   - 若為初次剛啟動:
   ///     - 且 this->Start() 檢查結果未到達 SchIn, 則返回 SchSt::Unknown;
   ///     - 若 this->Start() 檢查結果已到達 SchIn, 則返回 SchSt::Stop; 且在 1 sec 後觸發 SchIn 事件.
   ///   - 若為已啟動過, 則返回 Start() 前的狀態.
   SchSt Start(const StrView& cfgstr);

   /// 強迫在 ti 之後, 再次觸發 OnSchTask_StateChanged() 事件.
   /// \retval false 已呼叫過 DisposeAndWait(), 或正在 StopAndWait();
   bool Restart(TimeInterval ti);

   /// 停止排程.
   void StopAndWait();
   void DisposeAndWait();
};
fon9_MSC_WARN_POP;

} // namespaces
#endif//__fon9_SchTask_hpp__
