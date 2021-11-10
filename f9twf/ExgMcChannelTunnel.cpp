// \file f9twf/ExgMcChannelTunnel.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMcChannelTunnel.hpp"
#include "f9twf/ExgMcGroup.hpp"

namespace f9twf {
using namespace fon9;
using namespace fon9::io;

class ExgMcChannelTunnelSession : public Session, private ExgMcMessageConsumer {
   fon9_NON_COPY_NON_MOVE(ExgMcChannelTunnelSession);
   using base = Session;
   Device*        Device_{};
   ExgMcChannel&  McChannel_;
   SubConn        SubConn_{};

   RecvBufferSize OnDevice_LinkReady(Device& dev) override {
      this->Device_ = &dev;
      this->McChannel_.SubscribeConsumer(&this->SubConn_, *this);
      return RecvBufferSize::Default;
   }
   RecvBufferSize OnDevice_Recv(Device& dev, DcQueue& rxbuf) override {
      (void)dev;
      rxbuf.PopConsumedAll();
      return RecvBufferSize::Default;
   }
   void OnDevice_StateChanged(Device& dev, const StateChangedArgs& e) override {
      (void)dev;
      if (e.BeforeState_ == State::LinkReady)
         this->McChannel_.UnsubscribeConsumer(&this->SubConn_);
   }
   void OnDevice_Destructing(Device& dev) override {
      (void)dev;
      this->McChannel_.UnsubscribeConsumer(&this->SubConn_);
      this->Device_ = nullptr;
   }
   void OnExgMcMessage(const ExgMcMessage& e) override {
      if (this->Device_ && this->Device_->OpImpl_GetState() == State::LinkReady)
         this->Device_->SendASAP(&e.Pk_, e.PkSize_);
   }
public:
   ExgMcChannelTunnelSession(ExgMcChannel& channel) : McChannel_(channel) {
   }
};
class ExgMcChannelTunnelServer : public SessionServer {
   fon9_NON_COPY_NON_MOVE(ExgMcChannelTunnelServer);
   using base = SessionServer;
public:
   ExgMcChannel&  McChannel_;
   ExgMcChannelTunnelServer(ExgMcChannel& channel) : McChannel_(channel) {
   }
   SessionSP OnDevice_Accepted(DeviceServer& dev) override {
      (void)dev;
      return new ExgMcChannelTunnelSession{this->McChannel_};
   }
};
//--------------------------------------------------------------------------//
static ExgMcChannel* ParseMcChannelConfig(IoManager& ioMgr, StrView args, std::string& errReason) {
   if (auto mgr = dynamic_cast<ExgMcGroupIoMgr*>(&ioMgr)) {
      StrView  tag, value;
      while (StrFetchTagValue(args, tag, value)) {
         if (tag == "ChannelId") {
            if (ExgMcChannel* retval = mgr->McGroup_->ChannelMgr_->GetChannel(StrTo(value, ExgMrChannelId_t{})))
               return retval;
            errReason = "f9twf.ExgMcChannelTunnelFactory.CreateSession: Unknown Channel:";
            value.AppendTo(errReason);
            return nullptr;
         }
         else {
            errReason = "f9twf.ExgMcChannelTunnelFactory.CreateSession: Unknown tag:";
            tag.AppendTo(errReason);
            return nullptr;
         }
      }
   }
   errReason = "f9twf.ExgMcChannelTunnelFactory.CreateSession: Unknown IoMgr.";
   return nullptr;
}
SessionSP ExgMcChannelTunnelFactory::CreateSession(IoManager& ioMgr, const IoConfigItem& cfg, std::string& errReason) {
   if (ExgMcChannel* ch = ParseMcChannelConfig(ioMgr, ToStrView(cfg.SessionArgs_), errReason))
      return new ExgMcChannelTunnelSession{*ch};
   return nullptr;
}
SessionServerSP ExgMcChannelTunnelFactory::CreateSessionServer(IoManager& ioMgr, const IoConfigItem& cfg, std::string& errReason) {
   if (ExgMcChannel* ch = ParseMcChannelConfig(ioMgr, ToStrView(cfg.SessionArgs_), errReason))
      return new ExgMcChannelTunnelServer{*ch};
   return nullptr;
}

} // namespaces
