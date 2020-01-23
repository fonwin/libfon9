// \file fon9/SchTask_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/SchTask.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/RevPrint.hpp"

//--------------------------------------------------------------------------//
#define MAKE_SCH_RES(yyyymmddHHMMSS, SCHST, isChanged) \
   fon9::SchConfig::CheckResult{fon9::YYYYMMDDHHMMSS_ToTimeStamp(yyyymmddHHMMSS), \
                                fon9::SchSt::SCHST, \
                                isChanged}

#define MAKE_SCH_RES_T0(SCHST, isChanged) \
   fon9::SchConfig::CheckResult{fon9::TimeStamp{}, fon9::SchSt::SCHST, isChanged}
//--------------------------------------------------------------------------//
void TestSchConfigCheck(fon9::SchConfig& cfg, fon9::TimeStamp localNow, fon9::SchConfig::CheckResult localExpect) {
   localNow -= cfg.TimeZoneAdj_;
   auto res = cfg.Check(localNow, fon9::SchSt::Unknown);
   localNow += cfg.TimeZoneAdj_;
   fon9::RevBufferFixedSize<1024>   rbuf;
   rbuf.RewindEOS();
   static const fon9::FmtTS fmtts{"K-T"};
   if (res.NextCheckTime_.GetOrigValue() != 0) {
      res.NextCheckTime_ += cfg.TimeZoneAdj_;
      fon9::RevPrint(rbuf, res.NextCheckTime_, fmtts);
   }
   fon9::RevPrint(rbuf, "now=", localNow, fmtts, "|InSch=", res.SchSt_, "|Next=");
   if (res.SchSt_ == localExpect.SchSt_ && res.NextCheckTime_ == localExpect.NextCheckTime_
       && res.IsChanged_ == localExpect.IsChanged_) {
      std::cout << rbuf.GetCurrent() << std::endl;
      return;
   }
   std::cout << rbuf.GetCurrent() << '\n';
   rbuf.RewindEOS();
   fon9::RevPrint(rbuf, "      not match expect|InSch=", localExpect.SchSt_, "|Next=", localExpect.NextCheckTime_, fmtts);
   std::cout << rbuf.GetCurrent() << std::endl;
   abort();
}

void PrintSchConfig(const fon9::SchConfig& cfg) {
   fon9::RevBufferFixedSize<1024> rbuf;
   rbuf.RewindEOS();
   fon9::RevPrint(rbuf, "schcfg={", cfg, "}");
   std::cout << rbuf.GetCurrent() << std::endl;
}

void TestSchConfigCheck_InDay() {
   fon9::SchConfig cfg{"Weekdays=12345|Start=084500|End=135000"};
   PrintSchConfig(cfg);
   // 現在時間在 StartTime 之前:
   fon9::TimeStamp localNow = fon9::YYYYMMDDHHMMSS_ToTimeStamp(20161225000000);// Sunday.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161226: Mon.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161227: Tue.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161227084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161228: Wed.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161228084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161229: Thr.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161229084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161230: Fri.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161230084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161231: Sat.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20170102084500, OutSch, false));
   // 現在時間==StartTime:
   localNow = fon9::YYYYMMDDHHMMSS_ToTimeStamp(20161225084500);
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161226: Mon.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226135001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161227: Tue.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161227135001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161228: Wed.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161228135001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161229: Thr.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161229135001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161230: Fri.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161230135001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161231: Sat.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20170102084500, OutSch, false));
   // 現在時間==EndTime:
   localNow = fon9::YYYYMMDDHHMMSS_ToTimeStamp(20161225135000);
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161226: Mon.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226135001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161227: Tue.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161227135001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161228: Wed.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161228135001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161229: Thr.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161229135001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161230: Fri.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161230135001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161231: Sat.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20170102084500, OutSch, false));
   // 現在時間已超過EndTime:
   localNow = fon9::YYYYMMDDHHMMSS_ToTimeStamp(20161225135001);
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161226: Mon.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161227084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161227: Tue.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161228084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161228: Wed.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161229084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161229: Thr.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161230084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161230: Fri.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20170102084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161231: Sat.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20170102084500, OutSch, false));
}
void TestSchConfigCheck_NoEnd() {
   fon9::SchConfig cfg{"Weekdays=12345|Start=084500"};
   PrintSchConfig(cfg);
   // 現在時間在 StartTime 之前:
   fon9::TimeStamp localNow = fon9::YYYYMMDDHHMMSS_ToTimeStamp(20161225000000);// Sunday.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161226: Mon.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161227: Tue.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161227084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161228: Wed.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161228084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161229: Thr.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161229084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161230: Fri.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161230084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161231: Sat.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20170102084500, OutSch, false));
   // 現在時間==StartTime:
   localNow = fon9::YYYYMMDDHHMMSS_ToTimeStamp(20161225084500);
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161226: Mon.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES_T0(InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161227: Tue.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES_T0(InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161228: Wed.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES_T0(InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161229: Thr.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES_T0(InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161230: Fri.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES_T0(InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161231: Sat.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20170102084500, OutSch, false));
}
void TestSchConfigCheck_CrossDay() {
   fon9::SchConfig cfg{"Weekdays=12345|Start=08:45:00|End=05:30:00"};
   PrintSchConfig(cfg);
   // 現在時間在次日結束之前.
   fon9::TimeStamp localNow = fon9::YYYYMMDDHHMMSS_ToTimeStamp(20161225000000);// Sunday.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161226: Mon.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161227: Tue.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161227053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161228: Wed.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161228053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161229: Thr.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161229053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161230: Fri.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161230053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161231: Sat.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161231053001, InSch, true));
   // 現在時間==StartTime:
   localNow = fon9::YYYYMMDDHHMMSS_ToTimeStamp(20161225084500);
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161226: Mon.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161227053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161227: Tue.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161228053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161228: Wed.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161229053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161229: Thr.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161230053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161230: Fri.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161231053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161231: Sat.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20170102084500, OutSch, false));
   // 現在時間==EndTime:
   localNow = fon9::YYYYMMDDHHMMSS_ToTimeStamp(20161225053000);
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161226: Mon.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161227: Tue.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161227053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161228: Wed.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161228053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161229: Thr.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161229053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161230: Fri.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161230053001, InSch, true));
   localNow += fon9::TimeInterval_Day(1);//20161231: Sat.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161231053001, InSch, true));
   // 現在時間已超過EndTime,尚未到達 StartTime:
   localNow = fon9::YYYYMMDDHHMMSS_ToTimeStamp(20161225060000);
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161226: Mon.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161226084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161227: Tue.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161227084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161228: Wed.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161228084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161229: Thr.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161229084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161230: Fri.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20161230084500, OutSch, false));
   localNow += fon9::TimeInterval_Day(1);//20161231: Sat.
   TestSchConfigCheck(cfg, localNow, MAKE_SCH_RES(20170102084500, OutSch, false));
}

void TestSchTask() {
   TestSchConfigCheck_InDay();
   TestSchConfigCheck_NoEnd();
   TestSchConfigCheck_CrossDay();
}

//--------------------------------------------------------------------------//

int main() {
   fon9::AutoPrintTestInfo utinfo{"SchTask"};
   TestSchTask();
}
