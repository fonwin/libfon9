// \file f9twf/ExgMiReceiver.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMiSystemBase.hpp"

namespace f9twf {

ExgMiChannel::~ExgMiChannel() {
}
void ExgMiChannel::DailyClear() {
   for (const ExgMiHandlerSP& pph : this->MiHandlers_) {
      if (pph.get())
         pph->DailyClear();
   }
}
fon9::io::SessionSP ExgMiChannel::CreateSession(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
   (void)ioMgr; (void)cfg; (void)errReason;
   return new ExgMiReceiver{this};
}
fon9::io::SessionServerSP ExgMiChannel::CreateSessionServer(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
   (void)ioMgr; (void)cfg;
   errReason = "f9twf.ExgMiChannel: Not support server.";
   return nullptr;
}
//--------------------------------------------------------------------------//
ExgMiHandlerPkCont::~ExgMiHandlerPkCont() {
}
void ExgMiHandlerPkCont::OnLogSeqGap(fon9::PkContFeeder& rthis, fon9::RevBuffer& rbuf, const void* pk) {
   auto* pthis = static_cast<const ExgMiHandlerPkCont*>(&rthis);
   fon9::RevPrint(rbuf, "TwfExgMi.Gap"
                  "|channelId=", pthis->PkSys_.ChannelId_,
                  "|market=", pthis->PkSys_.Market_,
                  "|tradingSessionId=", pthis->PkSys_.MiSystem_.TradingSessionId_,
                  "|txCode=", static_cast<const ExgMiHead*>(pk)->TransmissionCode_,
                  "|msgKind=", static_cast<const ExgMiHead*>(pk)->MessageKind_);
}
//--------------------------------------------------------------------------//
ExgMiReceiver::~ExgMiReceiver() {
}
std::string ExgMiReceiver::SessionCommand(fon9::io::Device& dev, fon9::StrView cmdln) {
   (void)dev;
   cmdln = StrFetchTrim(cmdln, &fon9::isspace);
   if (cmdln == "info") {
      return fon9::RevPrintTo<std::string>(
         fon9::LocalNow(),
         "|channelId=", this->MiChannel_->ChannelId_,
         "|market=", this->MiChannel_->Market_,
         "|tradingSessionId=", this->MiChannel_->MiSystem_.TradingSessionId_,
         "|pkCount=", this->ReceivedCount_,
         "|chkSumErr=", this->ChkSumErrCount_,
         "|dropped=", this->DroppedBytes_);
   }
   else if (cmdln == "?") {
      return "info" fon9_kCSTR_CELLSPL "Get Session info";// fon9_kCSTR_ROWSPL;
   }
   return "unknown ExgMiReceiver command";
}
void ExgMiReceiver::OnDevice_Initialized(fon9::io::Device& dev) {
   this->Device_ = &dev;
}
fon9::io::RecvBufferSize ExgMiReceiver::OnDevice_LinkReady(fon9::io::Device& dev) {
   this->PkLastTime_ = fon9::TimeStamp::Null();
   this->DataInSt_ = DataInSt_Waiting1st;
   this->ClearStatus();
   dev.CommonTimerRunAfter(fon9::TimeInterval_Second(5));
   return fon9::io::RecvBufferSize::Default;
}
void ExgMiReceiver::OnDevice_CommonTimer(fon9::io::Device& dev, fon9::TimeStamp now) {
   constexpr unsigned kMiHbIntervalSeconds = 35;
   switch (this->DataInSt_) {
   case DataInSt_WaitingIn:
   case DataInSt_NoData:
      break;
   case DataInSt_Waiting1st:
      this->DataInSt_ = DataInSt_WaitingIn;
      if (dev.Manager_)
         dev.Manager_->OnSession_StateUpdated(dev, "Waiting 1st pk", fon9::LogLevel::Info);
      break;
   case DataInSt_Received:
      if ((now - this->PkLastTime_).GetIntPart() > kMiHbIntervalSeconds) {
         this->DataInSt_ = DataInSt_NoData;
         fon9::RevBufferFixedSize<1024> rbuf;
         fon9::RevPrint(rbuf, "No data"
                        "|pkCount=", this->ReceivedCount_,
                        "|pkLast=", this->PkLastTime_ + fon9::GetLocalTimeZoneOffset(), fon9::kFmtYMD_HMS);
         if (dev.Manager_)
            dev.Manager_->OnSession_StateUpdated(dev, ToStrView(rbuf), fon9::LogLevel::Warn);
      }
      break;
   }
   dev.CommonTimerRunAfter(fon9::TimeInterval_Second(kMiHbIntervalSeconds));
}
fon9::io::RecvBufferSize ExgMiReceiver::OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueue& rxbuf) {
   (void)dev;
   this->FeedBuffer(rxbuf);
   return fon9::io::RecvBufferSize::Default;
}
bool ExgMiReceiver::OnPkReceived(const void* pkptr, unsigned pksz) {
   this->MiChannel_->OnPkReceived(*static_cast<const f9twf::ExgMiHead*>(pkptr), pksz);
   if (fon9_UNLIKELY(this->DataInSt_ != DataInSt_Received)) {
      this->DataInSt_ = DataInSt_Received;
      if (this->Device_->Manager_)
         this->Device_->Manager_->OnSession_StateUpdated(*this->Device_, "DataIn", fon9::LogLevel::Info);
   }
   this->PkLastTime_ = fon9::TimeStamp{RemoveDecPart(fon9::UtcNow())};
   return true;
}

} // namespaces
