// \file f9twf/ExgMiSystemBase.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMiSystemBase.hpp"
#include "fon9/framework/IoManagerTree.hpp"
#include "fon9/seed/SysEnv.hpp"

namespace f9twf {

static const fon9::TimeInterval  kMiHbInterval = fon9::TimeInterval_Second(35);
//--------------------------------------------------------------------------//
void ExgMiSystemBase::Ctor() {
   this->Channels_[0] = new ExgMiChannel{*this, 1, f9fmkt_TradingMarket_TwFUT, "MiReceiverCh1"};
   this->Channels_[1] = new ExgMiChannel{*this, 2, f9fmkt_TradingMarket_TwOPT, "MiReceiverCh2"};
}
ExgMiSystemBase::~ExgMiSystemBase() {
   this->HbTimer_.DisposeAndWait();
}
void ExgMiSystemBase::PlantIoMgr(fon9::seed::MaTree& root, fon9::IoManagerArgs& iomArgs) {
   iomArgs.DeviceFactoryPark_ = fon9::seed::FetchNamedPark<fon9::DeviceFactoryPark>(root, "FpDevice");
   iomArgs.SessionFactoryPark_ = fon9::seed::FetchNamedPark<fon9::SessionFactoryPark>(*this->Sapling_, "FpSession");
   for (ExgMiChannelSP ch : this->Channels_) {
      iomArgs.SessionFactoryPark_->Add(std::move(ch));
   }
   iomArgs.Name_ = this->Name_ + "_io";
   iomArgs.CfgFileName_ = fon9::RevPrintTo<std::string>(fon9::seed::SysEnv_GetConfigPath(root), iomArgs.Name_, ".f9gv");
   iomArgs.IoServiceSrc_ = fon9::IoManagerTree::Plant(*this->Sapling_, iomArgs);
}
void ExgMiSystemBase::OnMdSystemStartup(unsigned tdayYYYYMMDD, const std::string& logPath) {
   this->HbTimer_.RunAfter(kMiHbInterval);
   base::OnMdSystemStartup(tdayYYYYMMDD, logPath);
   for (const ExgMiChannelSP& ch : this->Channels_) {
      if (ch.get())
         ch->DailyClear();
   }
}
void ExgMiSystemBase::EmitHbTimer(fon9::TimerEntry* timer, fon9::TimeStamp now) {
   ExgMiSystemBase& rthis = ContainerOf(*static_cast<decltype(ExgMiSystemBase::HbTimer_)*>(timer), &ExgMiSystemBase::HbTimer_);
   const auto       sch = rthis.NoDataEventSch_.Check(now, fon9::SchSt::Unknown);
   if (sch.SchSt_ == fon9::SchSt::InSch) {
      for (const ExgMiChannelSP& ch : rthis.Channels_) {
         if (ch && ch->ResetReceivedCount() == 0) {
            // 2 次 Hb 之間沒有收到任何封包, 行情可能斷線!
            fon9_LOG_WARN(rthis.Name_, '.', ch->Name_, ".Hb"
                          "|interval=", kMiHbInterval,
                          "|info=No data");
            rthis.NoDataEventSubject_.Publish(*ch);
         }
      }
   }
   rthis.HbTimer_.RunAfter(kMiHbInterval);
}

} // namespaces
