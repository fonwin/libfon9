// \file f9tws/ExgTradingLineFixFactory.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgTradingLineFixFactory.hpp"
#include "fon9/fix/FixBusinessReject.hpp"

namespace f9tws {

ExgTradingLineFixFactory::ExgTradingLineFixFactory(std::string fixLogPathFmt, Named&& name)
   : base{std::move(name)}
   , FixLogPathFmt_{std::move(fixLogPathFmt)} {
   ExgTradingLineFix::InitFixConfig(this->FixConfig_);
   f9fix::InitRecvRejectMessage(this->FixConfig_);
}

fon9::TimeStamp ExgTradingLineFixFactory::GetTDay() {
   return fon9::UtcNow() + fon9::GetLocalTimeZoneOffset();
}
fon9::io::SessionSP ExgTradingLineFixFactory::CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
   f9tws::ExgTradingLineMgr* twsLineMgr = dynamic_cast<f9tws::ExgTradingLineMgr*>(&mgr);
   if (twsLineMgr == nullptr) {
      errReason = "IoManager must be f9tws::ExgTradingLineMgr";
      return fon9::io::SessionSP{};
   }
   f9tws::ExgTradingLineFixArgs args;
   args.Market_ = twsLineMgr->Market_;
   errReason = f9tws::TwsFixArgParser(args, ToStrView(cfg.SessionArgs_));
   if (!errReason.empty())
      return fon9::io::SessionSP{};

   fon9::TimedFileName  fixLogPath(this->FixLogPathFmt_, fon9::TimedFileName::TimeScale::Day);
   fon9::TimeStamp      tday = this->GetTDay();
   if (tday.GetOrigValue() == 0) {
      errReason = "f9tws::ExgTradingLineFixFactory|err=Unknown TDay.";
      return fon9::io::SessionSP{};
   }
   // FIX log 檔名與 TDay 相關, 與 TimeZone 無關,
   // 所以要扣除 fixLogPath.GetTimeChecker().GetTimeZoneOffset();
   fixLogPath.RebuildFileName(tday - fixLogPath.GetTimeChecker().GetTimeZoneOffset());

   fon9::fix::IoFixSenderSP fixSender;
   errReason = f9tws::MakeExgTradingLineFixSender(args, &fixLogPath.GetFileName(), fixSender);
   if (!errReason.empty())
      return fon9::io::SessionSP{};
   return this->CreateTradingLine(*twsLineMgr, args, std::move(fixSender));
}
fon9::io::SessionServerSP ExgTradingLineFixFactory::CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
   (void)mgr; (void)cfg;
   errReason = "f9tws::ExgTradingLineFixFactory|err=Not support Server.";
   return fon9::io::SessionServerSP{};
}

} // namespaces
