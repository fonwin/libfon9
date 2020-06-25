// \file f9twf/ExgMcReceiverFactory.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMcReceiverFactory.hpp"
#include "f9twf/ExgMcGroup.hpp"
#include "f9twf/ExgMcReceiver.hpp"

namespace f9twf {
using namespace fon9;

io::SessionSP ExgMcReceiverFactory::CreateSession(IoManager& ioMgr, const IoConfigItem& cfg, std::string& errReason) {
   if (auto mgr = dynamic_cast<ExgMcGroupIoMgr*>(&ioMgr)) {
      StrView           tag, value, args = ToStrView(cfg.SessionArgs_);
      ExgMrChannelId_t  channelId = 0;
      TimeInterval      waitInterval{TimeInterval::Null()};
      while (fon9::StrFetchTagValue(args, tag, value)) {
         if (tag == "ChannelId") {
            channelId = StrTo(value, channelId);
            if (channelId >= ExgMcChannelMgr::kChannelCount) {
               errReason = "f9twf.ExgMcReceiverFactory.CreateSession: Unknown ChannelId.";
               return nullptr;
            }
         }
         else if (tag == "WaitInterval")
            waitInterval = StrTo(value, waitInterval);
      }
      if (auto ch = mgr->McGroup_->ChannelMgr_->GetChannel(channelId)) {
         if (!waitInterval.IsNull())
            ch->SetWaitInterval(waitInterval);
         return new ExgMcReceiver(mgr->McGroup_->ChannelMgr_, channelId);
      }
      errReason = "f9twf.ExgMcReceiverFactory.CreateSession: Unknown ChannelId.";
      return nullptr;
   }
   errReason = "f9twf.ExgMcReceiverFactory.CreateSession: Unknown IoMgr.";
   return nullptr;
}
io::SessionServerSP ExgMcReceiverFactory::CreateSessionServer(IoManager& ioMgr, const IoConfigItem& cfg, std::string& errReason) {
   (void)ioMgr; (void)cfg;
   errReason = "f9twf.ExgMcReceiverFactory: Not support server.";
   return nullptr;
}

} // namespaces
