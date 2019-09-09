// \file f9tws/ExgLineFactory.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgLineFactory.hpp"
#include "fon9/FilePath.hpp"

namespace f9tws {

ExgLineFactory::ExgLineFactory(std::string logPathFmt, Named&& name)
   : base{std::move(name)}
   , LogPathFmt_{std::move(logPathFmt)} {
}
fon9::io::SessionServerSP ExgLineFactory::CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
   (void)mgr; (void)cfg;
   errReason = "f9tws::ExgLineFactory|err=Not support Server.";
   return fon9::io::SessionServerSP{};
}
fon9::io::SessionSP ExgLineFactory::CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
   if (ExgTradingLineMgr* twsLineMgr = dynamic_cast<ExgTradingLineMgr*>(&mgr))
      return this->CreateTradingLine(*twsLineMgr, cfg, errReason);
   errReason = "IoManager must be f9tws::ExgTradingLineMgr";
   return fon9::io::SessionSP{};
}
fon9::TimeStamp ExgLineFactory::GetTDay() {
   return fon9::UtcNow() + fon9::GetLocalTimeZoneOffset();
}
std::string ExgLineFactory::MakeLogPath(std::string& res) {
   fon9::TimeStamp tday = this->GetTDay();
   if (tday.IsNullOrZero())
      return "f9tws::ExgLineFactory|err=Unknown TDay.";
   fon9::TimedFileName  logPath(this->LogPathFmt_, fon9::TimedFileName::TimeScale::Day);
   // log 檔名與 TDay 相關, 與 TimeZone 無關,
   // 所以要扣除 logPath.GetTimeChecker().GetTimeZoneOffset();
   logPath.RebuildFileName(tday - logPath.GetTimeChecker().GetTimeZoneOffset());
   res = fon9::FilePath::AppendPathTail(&logPath.GetFileName());
   return std::string{};
}

} // namespaces
