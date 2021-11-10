// \file f9tws/ExgMdReceiverSession.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdReceiverSession.hpp"
#include "fon9/io/Device.hpp"
#include "fon9/TsAppend.hpp"

namespace f9tws {
using namespace fon9;
static const TimeInterval  kMdHbInterval = TimeInterval_Second(35);

ExgMdReceiverSession::~ExgMdReceiverSession() {
}
std::string ExgMdReceiverSession::SessionCommand(io::Device& dev, StrView cmdln) {
   (void)dev;
   cmdln = StrFetchTrim(cmdln, &isspace);
   if (cmdln == "info") {
      return RevPrintTo<std::string>(
         UtcNow(),
         "|pkCount=", this->ReceivedCount_,
         "|chkSumErr=", this->ChkSumErrCount_,
         "|dropped=", this->DroppedBytes_);
   }
   return "unknown ExgMdReceiverSession command";
}
io::RecvBufferSize ExgMdReceiverSession::OnDevice_LinkReady(io::Device& dev) {
   dev.CommonTimerRunAfter(kMdHbInterval);
   return io::RecvBufferSize::Default;
}
void ExgMdReceiverSession::OnDevice_CommonTimer(fon9::io::Device& dev, fon9::TimeStamp now) {
   dev.CommonTimerRunAfter(kMdHbInterval);
   if (now - this->LastRecvTime_ < kMdHbInterval)
      return;
   // 太久沒收到任何封包, 記錄行情可能斷線的訊息.
   fon9_LOG_WARN("Tws.ExgMdReceiverSession.Hb"
                 "|dev=", ToPtr(&dev), "|id={", dev.WaitGetDeviceId(), "}"
                 "|interval=", kMdHbInterval,
                 "|info=No data");
}
bool ExgMdReceiverSession::OnDevice_BeforeOpen(io::Device& dev, std::string& cfgstr) {
   (void)dev; (void)cfgstr;
   if (!this->PkLogName_.empty()) {
      this->PkLog_ = AsyncFileAppender::Make();
      std::string fname = this->MdDispatcher_->LogPath() + this->PkLogName_.ToString();
      auto res = this->PkLog_->OpenImmediately(fname, fon9::FileMode::CreatePath | fon9::FileMode::Append);
      if (res.IsError()) {
         this->PkLog_.reset();
         fon9_LOG_ERROR("ExgMdReceiverSession.PkLog|fname=", fname, '|', res);
      }
   }
   return true;
}
io::RecvBufferSize ExgMdReceiverSession::OnDevice_Recv(io::Device& dev, DcQueue& rxbuf) {
   (void)dev;
   this->FeedBuffer(rxbuf);
   return io::RecvBufferSize::Default;
}
bool ExgMdReceiverSession::OnPkReceived(const void* pkptr, unsigned pksz) {
   this->MdDispatcher_->OnPkReceived(*static_cast<const ExgMdHeader*>(pkptr), pksz);
   this->LastRecvTime_ = UtcNow();
   if (this->PkLog_) {
      static_assert(kExgMdMaxPacketSize < 0xfff0, "ExgMdHeader::Length_ cannot use uint16_t.");
      fon9::TsAppend(*this->PkLog_, this->LastRecvTime_, pkptr, static_cast<uint16_t>(pksz));
   }
   return true;
}

} // namespaces
