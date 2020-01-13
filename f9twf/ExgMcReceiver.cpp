// \file f9twf/ExgMcReceiver.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMcReceiver.hpp"

namespace f9twf {
using namespace fon9;

ExgMcReceiver::~ExgMcReceiver() {
}
std::string ExgMcReceiver::SessionCommand(fon9::io::Device& dev, fon9::StrView cmdln) {
   (void)dev;
   cmdln = StrFetchTrim(cmdln, &isspace);
   if (cmdln == "info") {
      return RevPrintTo<std::string>(
         UtcNow(),
         "|channelId=", this->ChannelId_,
         "|pkCount=", this->ReceivedCount_,
         "|chkSumErr=", this->ChkSumErrCount_,
         "|dropped=", this->DroppedBytes_);
   }
   return "unknown ExgMcReceiver command";
}
void ExgMcReceiver::OnDevice_Initialized(fon9::io::Device& dev) {
   this->Device_ = &dev;
}
bool ExgMcReceiver::OnDevice_BeforeOpen(fon9::io::Device& dev, std::string& cfgstr) {
   (void)cfgstr;
   if (auto* channel = this->ChannelMgr_->GetChannel(this->ChannelId_)) {
      if (channel->GetChannelState() != ExgMcChannelState::CanBeClosed)
         return true;
      dev.Manager_->OnSession_StateUpdated(dev, "Channel cycled on BeforeOpen.", fon9::LogLevel::Info);
      dev.AsyncClose("Channel cycled");
   }
   else {
      dev.Manager_->OnSession_StateUpdated(dev, "Unknown ChannelId.", fon9::LogLevel::Error);
   }
   return false;
}
io::RecvBufferSize ExgMcReceiver::OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) {
   (void)dev;
   this->FeedBuffer(rxbuf);
   return io::RecvBufferSize::Default;
}
bool ExgMcReceiver::OnPkReceived(const void* pkptr, unsigned pksz) {
   if (this->ChannelMgr_->OnPkReceived(*static_cast<const ExgMcHead*>(pkptr), pksz) == ExgMcChannelState::CanBeClosed)
      this->Device_->AsyncClose("Channel can be closed.");
   return true;
}

} // namespaces
