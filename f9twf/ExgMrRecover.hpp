// \file f9twf/ExgMrRecover.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMrRecover_hpp__
#define __f9twf_ExgMrRecover_hpp__
#include "f9twf/ExgMrFmt.hpp"
#include "f9twf/ExgMdPkReceiver.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace f9twf {

class f9twf_API ExgMcChannelMgr;
using ExgMcChannelMgrSP = fon9::intrusive_ptr<ExgMcChannelMgr>;

enum class ExgMcRecoverRole : char {
   Primary = 'P',
   Secondary = 'S',
};

/// 台灣期交所逐筆行情 回補 Session.
/// 收到回補封包後, 直接轉給 ExgMcChannel;
class ExgMrRecoverSession : public fon9::io::Session, private ExgMcPkReceiver {
   fon9_NON_COPY_NON_MOVE(ExgMrRecoverSession);
   using base = fon9::io::Session;
   fon9::io::Device* Device_{};
   ExgMrMsgSeqNum_t  MsgSeqNum_{};
   uint16_t          RequestCount_{0};
   ExgMrRecoverNum_t Recovering_{0};
   char              Padding__[2];

   enum State : uint8_t {
      MrState_Linking,
      MrState_LinkBroken,
      MrState_ApReady,
   };
   State State_{MrState_Linking};

   /// 收到的回補封包, 直接轉給 ChannelMgr_;
   bool OnPkReceived(const void* pk, unsigned pksz) override;

   void OnDevice_Initialized(fon9::io::Device& dev) override;
   fon9::io::RecvBufferSize OnDevice_LinkReady(fon9::io::Device& dev) override;
   void OnDevice_StateChanged(fon9::io::Device& dev, const fon9::io::StateChangedArgs& e) override;
   fon9::io::RecvBufferSize OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueueList& rxbuf) override;

public:
   const ExgMcRecoverRole  Role_;
   const ExgMrSessionId_t  SessionId_;
   const uint16_t          Pass_; // 0001..9999
   const ExgMcChannelMgrSP ChannelMgr_;

   ExgMrRecoverSession(ExgMcChannelMgrSP channelMgr,
                       ExgMcRecoverRole  role,
                       ExgMrSessionId_t  sessionId,
                       uint16_t          pass)
      : Role_{role}
      , SessionId_{sessionId}
      , Pass_{pass}
      , ChannelMgr_{std::move(channelMgr)} {
   }
   ~ExgMrRecoverSession();

   uint16_t GetRequestCount() const {
      return this->RequestCount_;
   }
   bool RequestMcRecover(ExgMrChannelId_t channelId, ExgMrMsgSeqNum_t from, ExgMrRecoverNum_t count);
};

//--------------------------------------------------------------------------//
class f9twf_API ExgMrRecoverFactory : public fon9::SessionFactory {
   fon9_NON_COPY_NON_MOVE(ExgMrRecoverFactory);
   using base = fon9::SessionFactory;
public:
   using base::base;

   fon9::io::SessionSP CreateSession(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
};

} // namespaces
#endif//__f9twf_ExgMrRecover_hpp__
