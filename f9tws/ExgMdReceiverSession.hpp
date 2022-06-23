// \file f9tws/ExgMdReceiverSession.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdReceiverSession_hpp__
#define __f9tws_ExgMdReceiverSession_hpp__
#include "f9tws/ExgMdSystemBase.hpp"
#include "f9tws/ExgMdPkReceiver.hpp"
#include "fon9/io/Session.hpp"
#include "fon9/FileAppender.hpp"

namespace f9tws {

/// 台灣證交所(櫃買中心)行情接收 Session.
/// - 收到封包後, 直接轉給 ExgMdSystem;
/// - 由 ExgMdSystem 分配給各個格式 ExgMdHandler 去解析;
/// - 由 ExgMdHandler 確保封包連續性;
class ExgMdReceiverSession : public fon9::io::Session, private ExgMdPkReceiver {
   fon9_NON_COPY_NON_MOVE(ExgMdReceiverSession);
   using base = fon9::io::Session;
   fon9::AsyncFileAppenderSP  PkLog_;
   fon9::TimeStamp            LastRecvTime_;

protected:
   /// 收到封包後, 直接轉發給 this->MdDispatcher_: 檢查連續、過濾重複、解析封包.
   bool OnPkReceived(const void* pkptr, unsigned pksz) override;
   bool OnDevice_BeforeOpen(fon9::io::Device& dev, std::string& cfgstr) override;
   std::string SessionCommand(fon9::io::Device& dev, fon9::StrView cmdln) override;
   fon9::io::RecvBufferSize OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueue& rxbuf) override;
   fon9::io::RecvBufferSize OnDevice_LinkReady(fon9::io::Device& dev) override;
   void OnDevice_CommonTimer(fon9::io::Device& dev, fon9::TimeStamp now);

public:
   const ExgMdSystemBaseSP MdDispatcher_;
   const fon9::CharVector  PkLogName_;

   ExgMdReceiverSession(ExgMdSystemBaseSP mdDispatcher, fon9::StrView pkLogName)
      : MdDispatcher_(std::move(mdDispatcher))
      , PkLogName_{pkLogName} {
   }

   ~ExgMdReceiverSession();
};

} // namespaces
#endif//__f9tws_ExgMdReceiverSession_hpp__