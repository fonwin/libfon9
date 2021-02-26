// \file f9tws/ExgTradingLineMgr.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgTradingLineMgr.hpp"

namespace f9tws {

void ExgTradingLineMgr::OnParentSeedClear() {
   this->OnBeforeDestroy();
   baseIo::OnParentSeedClear();
}
void ExgTradingLineMgr::OnFixSessionApReady(f9fix::IoFixSession& fixses) {
   if (auto line = dynamic_cast<ExgTradingLineFix*>(&fixses)) {
      baseFix::OnFixSessionApReady(fixses);
      this->OnTradingLineReady(*line);
   }
   else {
      this->OnSession_StateUpdated(*fixses.GetDevice(), "Unknown FIX.Ready", fon9::LogLevel::Error);
   }
}
void ExgTradingLineMgr::OnFixSessionDisconnected(f9fix::IoFixSession& fixses, f9fix::FixSenderSP&& fixSender) {
   if (auto line = dynamic_cast<ExgTradingLineFix*>(&fixses))
      this->OnTradingLineBroken(*line);
   baseFix::OnFixSessionDisconnected(fixses, std::move(fixSender));
}
bool ExgTradingLineMgr::SetTradingSessionSt(fon9::TimeStamp tday, f9fmkt_TradingSessionId sesId, f9fmkt_TradingSessionSt sesSt) {
   if (tday < this->TDay_)
      return false;
   this->TDay_ = tday;
   this->TradingSessionId_ = sesId;
   this->TradingSessionSt_ = sesSt;
   return true;
}
void ExgTradingLineMgr::OnTDayChanged(fon9::TimeStamp tday, std::string cause) {
   if (tday != this->TDay_) {
      if (!this->SetTradingSessionSt(tday, f9fmkt_TradingSessionId_Normal, f9fmkt_TradingSessionSt_Unknown))
         return;
   }
   else {
      // 交易日相同: 可能先呼叫了 SetTradingSessionSt();
      // 此時不用設定 TradingSessionSt; 直接重新開啟連線.
   }
   this->DisposeAndReopen(std::move(cause));
}

} // namespaces
