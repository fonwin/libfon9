// \file fon9/FlowControl_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/FlowControlCalc.hpp"
#include "fon9/FlowCounter.hpp"
#include "fon9/TestTools.hpp"

// 時間必須能被 segInterval 整除, 才能正確控制測試時的「時間分割」位置.
const fon9::TimeStamp   kStartTime;
// 流量 2 秒, 極限 80(一般網路流量應 * 1000),
// 檢測密度 2/10 = 0.2秒; = 8 / 每0.2秒;
const unsigned          fcMax = 80;
const auto              fcInterval = fon9::TimeInterval_Second(2);
const auto              negFcInterval = fon9::TimeInterval() - fcInterval;
const unsigned          segCount = 10;
const auto              segInterval = fcInterval / segCount;
const auto              negSegInterval = fon9::TimeInterval() - segInterval;
const auto              segLmt = fcMax / segCount;
fon9::FlowControlCalc   fcCalc;

void TestCalcUsed(fon9::TimeStamp now, unsigned usedCount,
                  unsigned afUsedCount, fon9::TimeInterval expResult) {
   char strbuf[1024];
   snprintf(strbuf, sizeof(strbuf), "fcCalc|max=%u/%g|seg=%u|Used=%3u|step=%3g|afUsed=%3u|res=% -4g",
           fcMax, fcInterval.To<double>(), segCount, usedCount,
           (now - kStartTime).To<double>(), afUsedCount, expResult.To<double>());
   fon9_CheckTestResult(strbuf, (fcCalc.CalcUsed(now, usedCount) == expResult
                                 && fcCalc.UsedCount() == afUsedCount));
}

int main(int argc, char* argv[]) {
   (void)argc; (void)argv;
   fon9::AutoPrintTestInfo utinfo{"FlowControl"};

   const auto      kUsedCount = (segLmt * 2) / 3;
   fon9::TimeStamp now = kStartTime;
   fcCalc.SetFlowControlArgs(fcMax, fcInterval, segCount);
   TestCalcUsed(now, kUsedCount, kUsedCount, fon9::TimeInterval{});
   // 用量超過一個分割的上限, 應延遲 segInterval;
   const auto  tiStep = segInterval / 2;
   TestCalcUsed(now += tiStep, kUsedCount, kUsedCount * 2, segInterval);
   // 時間超過上個分割, 扣除上個分割的可用量, 剩餘的用量沒超過, 所以不用等候(返回0).
   auto expUsedCount = kUsedCount * 3 - segLmt;
   TestCalcUsed(now += tiStep, kUsedCount, expUsedCount, fon9::TimeInterval{});
   // 累計現在要送的超過極限, 累計失敗, retval<0, 表示必須等候 (-retval) 才能送出此次流量.
   TestCalcUsed(now, fcMax, expUsedCount, negSegInterval);
   // 單筆超過極限, 允許累計, 但累計後需要等候消化.
   expUsedCount += (fcMax + kUsedCount);
   TestCalcUsed(now, fcMax + kUsedCount, expUsedCount, fcInterval + segInterval);
   // 過了1個時間間隔
   expUsedCount -= segLmt;
   TestCalcUsed(now += segInterval, kUsedCount, expUsedCount, negFcInterval + negSegInterval);
   // 又過了1個時間間隔
   expUsedCount -= segLmt;
   TestCalcUsed(now += segInterval, kUsedCount, expUsedCount, negFcInterval);
   // 又再過了1個時間間隔: 可以累計了.
   expUsedCount = expUsedCount - segLmt + kUsedCount;
   TestCalcUsed(now += segInterval, kUsedCount, expUsedCount, fcInterval - segInterval);
   // 過了 fcInterval, 流量應已消化.
   TestCalcUsed(now += fcInterval, kUsedCount, kUsedCount, fon9::TimeInterval{});
}
