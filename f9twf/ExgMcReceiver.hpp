// \file f9twf/ExgMcReceiver.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMcReceiver_hpp__
#define __f9twf_ExgMcReceiver_hpp__
#include "f9twf/ExgMdPkReceiver.hpp"
#include "f9twf/ExgMcChannel.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace f9twf {

/// 台灣期交所逐筆行情 接收 Session.
/// 收到封包後, 直接轉給 ChannelMgr_;
class ExgMcReceiver : public fon9::io::Session, private ExgMcPkReceiver {
   fon9_NON_COPY_NON_MOVE(ExgMcReceiver);
   using base = fon9::io::Session;
   fon9::io::Device* Device_;

   /// 收到封包後, 直接轉發給 this->ChannelMgr_ 的 Channel: 檢查連續、過濾重複、處理回補.
   bool OnPkReceived(const void* pkptr, unsigned pksz) override;

public:
   const ExgMcChannelMgrSP ChannelMgr_;
   const ExgMrChannelId_t  ChannelId_;
   char                    Padding___[6];

   /// 如果有指定 channelId, 則在 OnDevice_BeforeOpen() 會檢查是否需要開啟 Receiver.
   ExgMcReceiver(ExgMcChannelMgrSP channelMgr, ExgMrChannelId_t channelId)
      : ChannelMgr_{std::move(channelMgr)}
      , ChannelId_{channelId} {
   }
   ~ExgMcReceiver();

   bool OnDevice_BeforeOpen(fon9::io::Device& dev, std::string& cfgstr) override;
   void OnDevice_Initialized(fon9::io::Device& dev) override;
   std::string SessionCommand(fon9::io::Device& dev, fon9::StrView cmdln) override;
   fon9::io::RecvBufferSize OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueue& rxbuf) override;
};

} // namespaces
#endif//__f9twf_ExgMcReceiver_hpp__
