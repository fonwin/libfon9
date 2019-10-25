/// \file fon9/SchTask.cpp
/// \author fonwinz@gmail.com
#include "fon9/SchTask.hpp"
#include "fon9/buffer/RevBufferList.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 {

static constexpr DayTimeSec   kNoEndTime{25 * 60 * 60};
#define kCSTR_SplitField          "|"
#define kCSTR_SplitTagValue       "="

//--------------------------------------------------------------------------//

void SchConfig::SetAlwaysInSch() {
   this->Weekdays_.set();
   this->TimeZoneName_.assign("L");
   this->TimeZoneAdj_ = GetLocalTimeZoneOffset();
   this->StartTime_.Seconds_ = 0;
   this->EndTime_ = kNoEndTime;
}
void SchConfig::SetAlwaysOutSch() {
   this->Weekdays_.reset();
}

void SchConfig::Parse(StrView cfgstr) {
   this->SetAlwaysInSch();
   StrView tag, value;
   while (!cfgstr.empty()) {
      StrFetchTagValue(cfgstr, tag, value, *kCSTR_SplitField, *kCSTR_SplitTagValue);
      if (tag == "Weekdays") {
         this->Weekdays_.reset();
         for (char ch : value) {
            size_t idx = static_cast<size_t>(ch - '0');
            if (idx < kWeekdayCount)
               this->Weekdays_.set(idx);
         }
      }
      else if (tag == "Start") {
         this->StartTime_ = StrTo(value, DayTimeSec{});
      }
      else if (tag == "End") {
         this->EndTime_ = StrTo(value, kNoEndTime);
      }
      else if (tag == "TZ") {
         this->TimeZoneName_.assign(value);
         this->TimeZoneAdj_ = StrTo(value, TimeZoneOffset{});
      }
   }
}

fon9_API void RevPrint(RevBuffer& rbuf, const SchConfig& schcfg) {
   RevPrint(rbuf, kCSTR_SplitField "TZ" kCSTR_SplitTagValue, schcfg.TimeZoneName_);
   if (schcfg.EndTime_ < kNoEndTime)
      RevPrint(rbuf, kCSTR_SplitField "End" kCSTR_SplitTagValue, schcfg.EndTime_);
   RevPrint(rbuf, kCSTR_SplitField "Start" kCSTR_SplitTagValue, schcfg.StartTime_);
   if (schcfg.Weekdays_.any()) {
      for (size_t L = kWeekdayCount; L > 0; ) {
         if (schcfg.Weekdays_.test(--L))
            RevPutChar(rbuf, static_cast<char>(L + '0'));
      }
      RevPrint(rbuf, "Weekdays" kCSTR_SplitTagValue);
   }
   else
      RevPrint(rbuf, "Weekdays" kCSTR_SplitTagValue "OUT");
}

//--------------------------------------------------------------------------//

TimeStamp SchConfig::GetNextSchIn(TimeStamp now) const {
   CheckResult res = this->Check(now);
   if (res.SchSt_ == SchSt::OutSch) // 如果現在是 SchOut.
      return res.NextCheckTime_;    // 則 NextCheckTime_ 就是 SchIn 的時間.
   // 如果現在是 SchIn, 則 NextCheckTime_ 是 SchOut 的時間.
   if (res.NextCheckTime_.GetOrigValue() == 0)
      return res.NextCheckTime_;
   now.SetOrigValue(res.NextCheckTime_.GetOrigValue() + 1);
   res = this->Check(now);
   return(res.SchSt_ == SchSt::InSch ? now : res.NextCheckTime_);
}
SchConfig::CheckResult SchConfig::Check(TimeStamp now) const {
   now += this->TimeZoneAdj_;
   CheckResult       res;
   const struct tm&  tm = GetDateTimeInfo(now);
   DayTimeSec        dnow{static_cast<uint32_t>((tm.tm_hour * 60 + tm.tm_min) * 60 + tm.tm_sec)};
   DayTimeSec        next = kNoEndTime;
   size_t            wday = static_cast<size_t>(tm.tm_wday);
   if (this->StartTime_ < this->EndTime_) { // 開始-結束: 在同一天.
      res.SchSt_ = (this->Weekdays_.test(wday) && (this->StartTime_ <= dnow && dnow <= this->EndTime_))
                  ? SchSt::InSch : SchSt::OutSch;
      if (res.SchSt_ == SchSt::InSch) // 排程時間內, 下次計時器=結束時間.
         next.Seconds_ = this->EndTime_.Seconds_ + 1;
      else if (this->Weekdays_.any()) { // 排程時間外, 今日尚未啟動: 下次計時器=(今日or次日)的開始時間.
         if (this->EndTime_ < dnow) // 排程時間外, 今日已結束: 下次計時器=次日開始時間.
            goto __CHECK_TOMORROW;
__CHECK_TODAY:
         while (!this->Weekdays_.test(wday)) {
__CHECK_TOMORROW:
            now += TimeInterval_Day(1);
            wday = (wday + 1) % 7;
         }
         next = this->StartTime_;
      }
   } else { // 結束日 = 隔日.
      if (this->StartTime_ <= dnow) { // 起始時間已到, 尚未到達隔日.
         res.SchSt_ = this->Weekdays_.test(wday) ? SchSt::InSch : SchSt::OutSch;
         if (res.SchSt_ == SchSt::InSch) {
            now += TimeInterval_Day(1);
            next.Seconds_ = this->EndTime_.Seconds_ + 1;
         } else if (this->Weekdays_.any())
            goto __CHECK_TOMORROW;
      }
      else { // 起始時間已到, 且已到達隔日.
         res.SchSt_ = (dnow <= this->EndTime_) ? SchSt::InSch : SchSt::OutSch;
         if (res.SchSt_ == SchSt::InSch) { // 尚未超過隔日結束時間.
            res.SchSt_ = this->Weekdays_.test(wday ? (wday - 1) : 6) // 看看昨日是否需要啟動.
                         ? SchSt::InSch : SchSt::OutSch;
            if (res.SchSt_ == SchSt::InSch)
               next.Seconds_ = this->EndTime_.Seconds_ + 1;
         }
         if (res.SchSt_ != SchSt::InSch && this->Weekdays_.any())
            goto __CHECK_TODAY;
      }
   }
   if (next < kNoEndTime) { // 有下次計時時間.
      res.NextCheckTime_ = TimeStampResetHHMMSS(now);
      res.NextCheckTime_ += TimeInterval_Second(next.Seconds_) - this->TimeZoneAdj_.ToTimeInterval();
   }
   return res;
}

//--------------------------------------------------------------------------//

void SchTask::Timer::EmitOnTimer(TimeStamp now) {
   SchTask&        sch = ContainerOf(*this, &SchTask::Timer_);
   SchImpl::Locker impl{sch.SchImpl_};
   const SchSt     bfSt = impl->SchSt_;
   switch (bfSt) {
   case SchSt::Unknown:
   case SchSt::Restart:
   case SchSt::InSch:
   case SchSt::Stopped:
   case SchSt::OutSch:
      break;
   case SchSt::Stopping:
   case SchSt::Disposing:
      return;
   }
   auto res = impl->Config_.Check(now);
   if (res.NextCheckTime_.GetOrigValue() != 0)
      sch.Timer_.RunAt(res.NextCheckTime_);
   if ((bfSt == SchSt::InSch) != (res.SchSt_ == SchSt::InSch) || bfSt == SchSt::Restart) {
      impl->SchSt_ = res.SchSt_;
      impl.unlock();
      sch.OnSchTask_StateChanged(res.SchSt_ == SchSt::InSch);
   }
   else
      impl.unlock();
   sch.OnSchTask_NextCheckTime(res);
}

//--------------------------------------------------------------------------//

SchTask::~SchTask() {
   this->Timer_.DisposeAndWait();
}
void SchTask::OnSchTask_NextCheckTime(const SchConfig::CheckResult&) {
}
bool SchTask::SetSchState(SchSt st) {
   SchImpl::Locker impl{this->SchImpl_};
   if (impl->SchSt_ >= SchSt::Disposing)
      return false;
   impl->SchSt_ = st;
   return true;
}
bool SchTask::Restart(TimeInterval ti) {
   SchImpl::Locker impl{this->SchImpl_};
   switch (impl->SchSt_) {
   case SchSt::Unknown:
   case SchSt::Restart:
   case SchSt::InSch:
   case SchSt::Stopped:
   case SchSt::OutSch:
      break;
   case SchSt::Stopping:
   case SchSt::Disposing:
      return false;
   }
   impl->SchSt_ = SchSt::Restart;
   this->Timer_.RunAfter(ti);
   return true;
}
SchSt SchTask::Start(const StrView& cfgstr) {
   TimeStamp       now = UtcNow();
   SchImpl::Locker impl{this->SchImpl_};
   const SchSt     retval = impl->SchSt_;
   impl->Config_.Parse(cfgstr);
   switch (retval) {
   case SchSt::Unknown:
      impl->SchSt_ = SchSt::Stopped;
      break;
   case SchSt::InSch:
   case SchSt::Stopped:
   case SchSt::OutSch:
      break;
   case SchSt::Stopping:
   case SchSt::Disposing:
   case SchSt::Restart:
      return retval;
   }
   auto  res = impl->Config_.Check(now);
   if ((retval == SchSt::InSch) != (res.SchSt_ == SchSt::InSch)) {
      this->Timer_.RunAt(now + TimeInterval_Second(1));
      return(retval == SchSt::Unknown ? SchSt::Stopped : retval);
   }
   if (res.NextCheckTime_.GetOrigValue() == 0) {//sch狀態正確 && 沒有結束時間: 不用啟動計時器.
      this->Timer_.StopNoWait();
      return retval;
   }
   this->Timer_.RunAt(res.NextCheckTime_);
   impl.unlock();
   this->OnSchTask_NextCheckTime(res);
   return retval;
}
void SchTask::StopAndWait() {
   if (!this->SetSchState(SchSt::Stopping))
      return;
   this->Timer_.StopAndWait();
   this->SetSchState(SchSt::Stopped);
}
void SchTask::DisposeAndWait() {
   this->SetSchState(SchSt::Disposing);
   this->Timer_.DisposeAndWait();
}

} // namespaces
