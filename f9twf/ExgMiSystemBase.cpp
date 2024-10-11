// \file f9twf/ExgMiSystemBase.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMiSystemBase.hpp"
#include "fon9/framework/IoManagerTree.hpp"
#include "fon9/seed/SysEnv.hpp"

namespace f9twf {

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
   base::OnMdSystemStartup(tdayYYYYMMDD, logPath);
   for (const ExgMiChannelSP& ch : this->Channels_) {
      if (ch.get())
         ch->DailyClear();
   }
}
fon9::TimeInterval ExgMiSystemBase::OnMdSystem_HbTimer(fon9::TimeStamp now) {
   for (const ExgMiChannelSP& ch : this->Channels_) {
      if (!ch)
         continue;
      const auto curSt = *ch->GetMdReceiverStPtr();
      const auto oldSt = ch->EvMdReceiverSt_;
      auto       newSt = curSt;
      fon9::StrView warnInfo;
      if (ch->ResetReceivedCount() != 0) {
         // 資料正常接收中.
         assert(curSt == fon9::fmkt::MdReceiverSt::DataIn);
         ch->HbChkTime_ = now;
      }
      else {
         // 一小段時間無資料.
         switch (oldSt) {
         case fon9::fmkt::MdReceiverSt::DailyClear:
            // DailyClear 之後一小段時間, 必定會觸發一次事件: DailyClearNoData or DataIn;
            assert(curSt == fon9::fmkt::MdReceiverSt::DailyClear);
            newSt = fon9::fmkt::MdReceiverSt::DailyClearNoData;
            warnInfo = "No data @ AfterDailyClear";
            break;
         default:
         case fon9::fmkt::MdReceiverSt::DailyClearNoData:
         case fon9::fmkt::MdReceiverSt::DataBroken:
         case fon9::fmkt::MdReceiverSt::DataIn:
            // 無資料事件, 每經過 kMdHbInterval, 就需要觸發一次?
            if (now - ch->HbChkTime_ >= kMdHbInterval) {
               warnInfo = "No data";
               newSt = (oldSt == fon9::fmkt::MdReceiverSt::DataIn
                        ? fon9::fmkt::MdReceiverSt::DataBroken
                        : oldSt);
            }
            break;
         }
      }
      if (oldSt != newSt || !warnInfo.empty()) {
         const auto logLv = warnInfo.empty() ? fon9::LogLevel::Info : fon9::LogLevel::Warn;
         if (fon9_LIKELY(logLv >= fon9::LogLevel_)) {
            fon9::RevBufferList rbuf_{fon9::kLogBlockNodeSize};
            fon9::RevPutChar(rbuf_, '\n');
            if (!warnInfo.empty())
               fon9::RevPrint(rbuf_, "|info=", warnInfo);
            fon9::RevPrint(rbuf_, this->Name_, '.', ch->Name_, ".StEv"
                           "|sch=", this->NoDataEventSch_, "|HbInterval=", kMdHbInterval,
                           "|Bf=", oldSt, "|Af=", newSt);
            fon9::LogWrite(logLv, std::move(rbuf_));
         }
         ch->SetMdReceiverSt(newSt, now);
         this->OnMdReceiverStChanged(*ch, oldSt, newSt);
      }
   }
   return kMdStChkInterval;
}
void ExgMiSystemBase::OnMdReceiverStChanged(ExgMiChannel& ch, fon9::fmkt::MdReceiverSt bf, fon9::fmkt::MdReceiverSt af) {
   this->StChangedEventSubject_.Publish(ch, bf, af);
}

} // namespaces
