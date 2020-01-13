// \file f9twf/ExgMcToMiConv.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMcToMiConv_hpp__
#define __f9twf_ExgMcToMiConv_hpp__
#include "f9twf/ExgMcGroup.hpp"

namespace f9twf {

class f9twf_API ExgMcToMiConv;

struct f9twf_API ExgMcToMiConvHandler {
   fon9_NON_COPY_NON_MOVE(ExgMcToMiConvHandler);
   ExgMcToMiConvHandler() = default;
   virtual ~ExgMcToMiConvHandler();
   virtual void OnExgMiMessage(ExgMcToMiConv& sender, const ExgMiHead& pk, unsigned pksz) = 0;
};

/// 台灣期交所「逐筆行情 => 間隔行情」轉換器.
/// - 要訂閱那些 Channels?
/// - 間隔行情輸出檔(log)?
/// - 間隔行情訂閱者?
/// - 每個訊息格式獨自編列序號.
class f9twf_API ExgMcToMiConv : public fon9::seed::NamedSeed
                              , public ExgMcGroupSetupHandler
                              , public ExgMcMessageConsumer {
   fon9_NON_COPY_NON_MOVE(ExgMcToMiConv);
   using base = fon9::seed::NamedSeed;
   fon9::AsyncFileAppenderSP  PkLog_;

   fon9::Subject<ExgMcToMiConvHandler*> Handlers_{8};

   using SeqT = uint64_t;
   struct HandlerFns;
   struct MsgHandler {
      void(*FnHandler_)(ExgMcToMiConv&, const ExgMcMessage& e);
      SeqT  LastSeq_;
   };
   MsgHandler  MsgMap_[kExgMdMaxTx + 1][kExgMdMaxMg + 1];
   MsgHandler& GetMsgHandler(char tx, char mg) {
      return this->MsgMap_[AlNumToIdx(tx)][AlNumToIdx(mg)];
   }
   MsgHandler& GetMsgHandler(const ExgMdHead0& pk) {
      return this->GetMsgHandler(pk.TransmissionCode_, pk.MessageKind_);
   }
   /// - 重算 CheckSum.
   /// - 加上 0x0d, 0x0a.
   /// - 通知 Handlers_;
   /// - 寫入 PkLog_;
   void FinalMiMessage(ExgMiHead& pk, unsigned pksz, fon9::BufferList&& buf);
   void FinalMiMessageSeq(ExgMiHead& pk, unsigned pksz, fon9::BufferList&& buf) {
      auto& msgh = this->GetMsgHandler(pk);
      fon9::ToPackBcd(pk.InformationSeq_, ++msgh.LastSeq_);
      this->FinalMiMessage(pk, pksz, std::move(buf));
   }

public:
   const ExgMcChannelMgrSP ChannelMgr_;

   ExgMcToMiConv(ExgMcChannelMgrSP channelMgr, std::string name);
   ~ExgMcToMiConv();

   void OnStartupMcGroup(ExgMcGroup&, const std::string& logPath) override;
   void OnExgMcMessage(const ExgMcMessage& e) override;

   void Subscribe(ExgMcToMiConvHandler& h) {
      this->Handlers_.Subscribe(&h);
   }
   void Unsubscribe(ExgMcToMiConvHandler& h) {
      this->Handlers_.UnsubscribeAll(&h);
   }
};
using ExgMcToMiConvSP = fon9::intrusive_ptr<ExgMcToMiConv>;

//--------------------------------------------------------------------------//
class f9twf_API ExgMcToMiSenderFactory : public fon9::SessionFactory {
   fon9_NON_COPY_NON_MOVE(ExgMcToMiSenderFactory);
   using base = fon9::SessionFactory;
public:
   using base::base;

   fon9::io::SessionSP CreateSession(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
};

} // namespaces
#endif//__f9twf_ExgMcToMiConv_hpp__
