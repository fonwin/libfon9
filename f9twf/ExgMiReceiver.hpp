// \file f9twf/ExgMiReceiver.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMiReceiver_hpp__
#define __f9twf_ExgMiReceiver_hpp__
#include "f9twf/ExgMdFmt.hpp"
#include "f9twf/ExgTypes.hpp"
#include "f9twf/ExgMdPkReceiver.hpp"
#include "fon9/PkCont.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace f9twf {

class f9twf_API ExgMiChannel;
class f9twf_API ExgMiSystemBase;
//--------------------------------------------------------------------------//
fon9_EXPORT_TEMPLATE_CLASS(f9twf_API, ExgMiHandler,       fon9::PkHandlerBase,   ExgMiChannel, ExgMiHead);
fon9_EXPORT_TEMPLATE_CLASS(f9twf_API, ExgMiHandlerAnySeq, fon9::PkHandlerAnySeq, ExgMiHandler);
using ExgMiHandlerSP = std::unique_ptr<ExgMiHandler>;

class f9twf_API ExgMiHandlerPkCont : public fon9::PkHandlerPkCont<ExgMiHandler> {
   fon9_NON_COPY_NON_MOVE(ExgMiHandlerPkCont);
   using base = fon9::PkHandlerPkCont<ExgMiHandler>;
public:
   using base::base;
   virtual ~ExgMiHandlerPkCont();

   void CheckLogLost(const void* pk, SeqT seq) {
      base::CheckLogLost(pk, seq, &OnLogSeqGap);
   }
   /// 在 rbuf 前端加上: "TwfExgMi.Gap|..."
   static void OnLogSeqGap(fon9::PkContFeeder& rthis, fon9::RevBuffer& rbuf, const void* pk);
};
//--------------------------------------------------------------------------//
class f9twf_API ExgMiChannel : public fon9::SessionFactory {
   fon9_NON_COPY_NON_MOVE(ExgMiChannel);
   using base = fon9::SessionFactory;
   using MiHandlers = ExgMdMessageDispatcher<ExgMiHandlerSP>;
   MiHandlers        MiHandlers_;
   // 2 次 Hb 之間, 若沒有任何訊息, 則可能發生斷線.
   uint64_t          ReceivedCountInHb_{0};
   fon9::TimeStamp   HbChkTime_;

   friend class ExgMiSystemBase;
   /// 取得收到的封包數量, 並清除封包數量計數器.
   uint64_t ResetReceivedCount() {
      auto retval = this->ReceivedCountInHb_;
      this->ReceivedCountInHb_ = 0;
      return retval;
   }
   /// 由於 Hb timeout 由 ExgMiSystemBase 處理,
   /// 所以相關狀態變化, 由 ExgMiSystemBase 負責更新.
   void SetMdReceiverSt(fon9::fmkt::MdReceiverSt st, fon9::TimeStamp now) {
      this->HbChkTime_ = now;
      this->CurMdReceiverSt_ = this->EvMdReceiverSt_ = st;
   }

   fon9::fmkt::MdReceiverSt   CurMdReceiverSt_;
   fon9::fmkt::MdReceiverSt   EvMdReceiverSt_;
   char                       Padding____[4];
public:
   const uint8_t              ChannelId_;
   const f9fmkt_TradingMarket Market_;
   ExgMiSystemBase&           MiSystem_;

   template <class... ArgsT>
   ExgMiChannel(ExgMiSystemBase& miSystem, uint8_t channelId, f9fmkt_TradingMarket market, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , ChannelId_{channelId}
      , Market_{market}
      , MiSystem_(miSystem) {
      assert(market == f9fmkt_TradingMarket_TwFUT || market == f9fmkt_TradingMarket_TwOPT);
   }

   virtual ~ExgMiChannel();

   void DailyClear();

   const fon9::fmkt::MdReceiverSt* GetMdReceiverStPtr() const {
      return &this->CurMdReceiverSt_;
   }

   template <char tx, char mg, uint8_t ver>
   void RegMiHandler(ExgMiHandlerSP handler) {
      this->MiHandlers_.Reg<tx, mg, ver>(std::move(handler));
   }

   /// Session 收到封包後, 丟到這裡分派給 handler 處理.
   void OnPkReceived(const ExgMiHead& pk, unsigned pksz) {
      if (ExgMiHandler* handler = this->MiHandlers_.Get(pk).get())
         handler->OnPkReceived(pk, pksz);
      ++this->ReceivedCountInHb_;
      this->CurMdReceiverSt_ = fon9::fmkt::MdReceiverSt::DataIn;
   }

   fon9::io::SessionSP CreateSession(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
};
using ExgMiChannelSP = fon9::intrusive_ptr<ExgMiChannel>;
//--------------------------------------------------------------------------//
/// 台灣期交所 間隔行情 接收 Session.
class ExgMiReceiver : public fon9::io::Session, private f9twf::ExgMiPkReceiver {
   fon9_NON_COPY_NON_MOVE(ExgMiReceiver);
   using base = fon9::io::Session;

   fon9::io::Device* Device_{};
   fon9::TimeStamp   PkLastTime_;
   enum {
      DataInSt_Waiting1st,
      DataInSt_WaitingIn,
      DataInSt_Received,
      DataInSt_NoData,
   }                 DataInSt_{};
   char              Padding____[4];

   void OnDevice_Initialized(fon9::io::Device& dev) override;
   void OnDevice_CommonTimer(fon9::io::Device& dev, fon9::TimeStamp now) override;
   fon9::io::RecvBufferSize OnDevice_LinkReady(fon9::io::Device& dev) override;

   /// 封包的 CheckSum 正確, 來到此處處理.
   bool OnPkReceived(const void* pkptr, unsigned pksz) override;

public:
   const ExgMiChannelSP  MiChannel_;

   ExgMiReceiver(ExgMiChannelSP miChannel)
      : MiChannel_{std::move(miChannel)} {
   }
   ~ExgMiReceiver();

   std::string SessionCommand(fon9::io::Device& dev, fon9::StrView cmdln) override;
   fon9::io::RecvBufferSize OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueue& rxbuf) override;
};

} // namespaces
#endif//__f9twf_ExgMiReceiver_hpp__
