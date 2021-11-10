// \file f9twf/ExgMcToMiConv.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMcToMiConv.hpp"
#include "f9twf/ExgMdPkReceiver.hpp"
#include "f9twf/ExgMdFmtMatch.hpp"
#include "f9twf/ExgMdFmtHL.hpp"
#include "f9twf/ExgMdFmtBS.hpp"
#include "fon9/FileReadAll.hpp"

namespace f9twf {
using namespace fon9;

ExgMcToMiConvHandler::~ExgMcToMiConvHandler() {
}
//--------------------------------------------------------------------------//
struct ExgMcToMiConv::HandlerFns {
   static void ConvI024(ExgMcToMiConv& conv, const ExgMcMessage& e) {
      const ExgMcI024&    pk = *static_cast<const ExgMcI024*>(&e.Pk_);
      fon9::FwdBufferList buf{0};
      const unsigned      miPkSz = static_cast<unsigned>(e.PkSize_ - sizeof(ExgMcI024Head)
                                                         + sizeof(ExgMiI020Head)
                                                         + sizeof(ExgMdMatchStatusCode));
      ExgMiI020*          miPk = reinterpret_cast<ExgMiI020*>(buf.AllocBuffer(miPkSz));
      *static_cast<ExgMdHead0*>(miPk) = e.Pk_;
      fon9::ToPackBcd(miPk->BodyLength_, miPkSz - sizeof(ExgMiNoBody));
      /// 試撮價格註記, '0':成交揭示訊息; '1':試撮價格訊息.
      if (pk.CalculatedFlag_ == '1') {
         // 間隔行情: ExgMiI022 試撮成交價量: Fut:(Tx='2'; Mg='7'); Opt:(Tx='5'; Mg='7'); Ver=2;
         miPk->MessageKind_ = '7';
         fon9::ToPackBcd(miPk->VersionNo_, 2u);
      }
      else {
         // 間隔行情: ExgMiI020 成交價量: Fut:(Tx='2'; Mg='1'); Opt:(Tx='5'; Mg='1'); Ver=4;
         miPk->MessageKind_ = '1';
         fon9::ToPackBcd(miPk->VersionNo_, 4u);
      }
      miPk->ProdId_ = pk.ProdId_;
      const auto datsz = reinterpret_cast<uintptr_t>(&pk) + e.PkSize_ - sizeof(ExgMdTail)
                       - reinterpret_cast<uintptr_t>(static_cast<const ExgMdMatchHead*>(&pk));
      char*      datptr = reinterpret_cast<char*>(static_cast<ExgMdMatchHead*>(miPk));
      memcpy(datptr, static_cast<const ExgMdMatchHead*>(&pk), datsz);
      fon9::ToPackBcd(*reinterpret_cast<ExgMdMatchStatusCode*>(datptr + datsz), 0u);
      conv.FinalMiMessageSeq(*miPk, miPkSz, buf.MoveOut());
   }
   static void ConvI025(ExgMcToMiConv& conv, const ExgMcMessage& e) {
      const ExgMcI025&    pk = *static_cast<const ExgMcI025*>(&e.Pk_);
      fon9::FwdBufferList buf{0};
      const unsigned      miPkSz = static_cast<unsigned>(e.PkSize_ - sizeof(ExgMcI025Head)
                                                         + sizeof(ExgMiI021Head));
      ExgMiI021*          miPk = reinterpret_cast<ExgMiI021*>(buf.AllocBuffer(miPkSz));
      *static_cast<ExgMdHead0*>(miPk) = e.Pk_;
      miPk->MessageKind_ = '5';
      fon9::ToPackBcd(miPk->VersionNo_, 3u);
      fon9::ToPackBcd(miPk->BodyLength_, miPkSz - sizeof(ExgMiNoBody));
      miPk->ProdId_ = pk.ProdId_;
      *static_cast<ExgMdDayHighLow*>(miPk) = pk;
      conv.FinalMiMessageSeq(*miPk, miPkSz, buf.MoveOut());
   }
   static void ConvI081(ExgMcToMiConv& conv, const ExgMcMessage& e) {
      MakeI080(conv, e, '2', 2u);
   }
   static void ConvI083(ExgMcToMiConv& conv, const ExgMcMessage& e) {
      if(static_cast<const ExgMcI083*>(&e.Pk_)->CalculatedFlag_ == '1')
         MakeI080(conv, e, '8', 1u);
      else
         MakeI080(conv, e, '2', 2u);
   }
   static void MakeI080(ExgMcToMiConv& conv, const ExgMcMessage& e, char mg, unsigned ver) {
      if (e.Symb_ == nullptr)
         return;
      const bool          hasDerived = (e.Symb_->BS_.Data_.DerivedBuy_.Qty_ || e.Symb_->BS_.Data_.DerivedSell_.Qty_);
      fon9::FwdBufferList buf{0};
      const unsigned      miPkSz = static_cast<unsigned>(sizeof(ExgMiI080) + sizeof(ExgMdTail)
                                                         - (hasDerived ? 0 : sizeof(ExgMiDerivedBS)));
      ExgMiI080*          miPk = reinterpret_cast<ExgMiI080*>(buf.AllocBuffer(miPkSz));
      *static_cast<ExgMdHead0*>(miPk) = e.Pk_;
      miPk->MessageKind_ = mg;
      fon9::ToPackBcd(miPk->VersionNo_, ver);
      fon9::ToPackBcd(miPk->BodyLength_, miPkSz - sizeof(ExgMiNoBody));
      miPk->ProdId_ = static_cast<const ExgMcI081*>(&e.Pk_)->ProdId_;
      //-----
      static_assert(fon9::fmkt::SymbBSData::kBSCount == sizeof(miPk->BuyOrderBook_) / sizeof(miPk->BuyOrderBook_[0]),
                    "BSCount not equal.");
      AssignBS(miPk->BuyOrderBook_, e.Symb_->BS_.Data_.Buys_, e.Symb_->PriceOrigDiv_);
      AssignBS(miPk->SellOrderBook_, e.Symb_->BS_.Data_.Sells_, e.Symb_->PriceOrigDiv_);
      if (!hasDerived)
         fon9::ToPackBcd(miPk->DerivedFlag_, 0u);
      else {
         fon9::ToPackBcd(miPk->DerivedFlag_, 1u);
         ExgMdPriceFrom(miPk->FirstDerived_.BuyPrice_, e.Symb_->BS_.Data_.DerivedBuy_.Pri_, e.Symb_->PriceOrigDiv_);
         ExgMdPriceFrom(miPk->FirstDerived_.SellPrice_, e.Symb_->BS_.Data_.DerivedSell_.Pri_, e.Symb_->PriceOrigDiv_);
         fon9::ToPackBcd(miPk->FirstDerived_.BuyQty_, e.Symb_->BS_.Data_.DerivedBuy_.Qty_);
         fon9::ToPackBcd(miPk->FirstDerived_.SellQty_, e.Symb_->BS_.Data_.DerivedSell_.Qty_);
      }
      //-----
      conv.FinalMiMessageSeq(*miPk, miPkSz, buf.MoveOut());
   }
   static void AssignBS(f9twf::ExgMdOrderPQ* dst, const fon9::fmkt::PriQty* src, uint32_t origDiv) {
      for (unsigned L = 0; L < fon9::fmkt::SymbBSData::kBSCount; ++L) {
         dst->Price_.AssignFrom(src->Pri_, origDiv);
         fon9::ToPackBcd(dst->Qty_, src->Qty_);
         ++src;
         ++dst;
      }
   }
   // 底下這些直接排除, 不轉成間隔行情格式.
   static void ConvI084(ExgMcToMiConv&, const ExgMcMessage&) {}
   static void ConvI001(ExgMcToMiConv&, const ExgMcMessage&) {}
   static void ConvI002(ExgMcToMiConv&, const ExgMcMessage&) {}
};
//--------------------------------------------------------------------------//
ExgMcToMiConv::ExgMcToMiConv(ExgMcChannelMgrSP channelMgr, std::string name)
   : base{std::move(name)}
   , ChannelMgr_{std::move(channelMgr)} {
   memset(MsgMap_, 0, sizeof(MsgMap_));
   this->GetMsgHandler('2', 'A').FnHandler_ = &HandlerFns::ConvI081;
   this->GetMsgHandler('2', 'B').FnHandler_ = &HandlerFns::ConvI083;
   this->GetMsgHandler('2', 'C').FnHandler_ = &HandlerFns::ConvI084;
   this->GetMsgHandler('2', 'D').FnHandler_ = &HandlerFns::ConvI024;
   this->GetMsgHandler('2', 'E').FnHandler_ = &HandlerFns::ConvI025;

   this->GetMsgHandler('5', 'A').FnHandler_ = &HandlerFns::ConvI081;
   this->GetMsgHandler('5', 'B').FnHandler_ = &HandlerFns::ConvI083;
   this->GetMsgHandler('5', 'C').FnHandler_ = &HandlerFns::ConvI084;
   this->GetMsgHandler('5', 'D').FnHandler_ = &HandlerFns::ConvI024;
   this->GetMsgHandler('5', 'E').FnHandler_ = &HandlerFns::ConvI025;

   this->GetMsgHandler('0', '1').FnHandler_ = &HandlerFns::ConvI001;
   this->GetMsgHandler('0', '2').FnHandler_ = &HandlerFns::ConvI002;
}
ExgMcToMiConv::~ExgMcToMiConv() {
   for (unsigned L = 0; L < ExgMcChannelMgr::kChannelCount; ++L) {
      if (auto* channel = this->ChannelMgr_->GetChannel(static_cast<ExgMrChannelId_t>(L)))
         channel->UnsubscribeConsumer(*this);
   }
}
void ExgMcToMiConv::OnStartupMcGroup(ExgMcGroup&, const std::string& logPath) {
   MsgHandler* msgh = &this->MsgMap_[0][0];
   const auto  kMsgCount = sizeof(this->MsgMap_) / sizeof(*msgh);
   for (size_t L = 0; L < kMsgCount; ++L) {
      msgh->LastSeq_ = 0;
      ++msgh;
   }
   // logfn = logPath + "_SeedName.bin"
   //       => "./logs/yyyymmdd/TwfMd_MdDay_ToMiConvF.bin"
   std::string logfn = logPath + this->Name_ + ".bin";
   this->PkLog_ = fon9::AsyncFileAppender::Make();
   auto res = this->PkLog_->OpenImmediately(logfn, fon9::FileMode::CreatePath | fon9::FileMode::Read | fon9::FileMode::Append);
   if (res.IsError()) {
      fon9_LOG_FATAL("ExgMcToMiConv.OnStartupMcGroup|fn=", logfn, '|', res);
      this->PkLog_.reset();
      return;
   }
   struct MiReloader : public ExgMiPkReceiver {
      fon9_NON_COPY_NON_MOVE(MiReloader);
      ExgMcToMiConv& Owner_;
      MiReloader(ExgMcToMiConv& owner) : Owner_(owner) {
      }
      ~MiReloader() {
      }
      bool operator()(fon9::DcQueue& rdbuf, fon9::File::Result&) {
         this->FeedBuffer(rdbuf);
         return true;
      }
      bool OnPkReceived(const void* pk, unsigned pksz) override {
         (void)pksz;
         auto& msgh = this->Owner_.GetMsgHandler(*static_cast<const ExgMiHead*>(pk));
         // +100 = 重啟時讓序號不連續.
         msgh.LastSeq_ = fon9::PackBcdTo<SeqT>(static_cast<const ExgMiHead*>(pk)->InformationSeq_) + 100;
         return true;
      }
   };
   MiReloader           reloader{*this};
   fon9::File::SizeType fpos = 0;
   res = fon9::FileReadAll(*this->PkLog_, fpos, reloader);
   if (res.IsError())
      fon9_LOG_FATAL("ExgMcToMiConv.OnStartupMcGroup.Read|fn=", logfn, "|pos=", fpos, '|', res);
}
void ExgMcToMiConv::OnExgMcMessage(const ExgMcMessage& e) {
   MsgHandler& msgh = this->GetMsgHandler(e.Pk_);
   if (msgh.FnHandler_) {
      msgh.FnHandler_(*this, e);
      return;
   }
   // 除了一些特殊訊息, 一般只要轉 Head 即可.
   fon9::FwdBufferList buf{0};
   const unsigned      miPkSz = static_cast<unsigned>(e.PkSize_ - sizeof(ExgMcHead) + sizeof(ExgMiHead));
   ExgMiHead*          miHead = reinterpret_cast<ExgMiHead*>(buf.AllocBuffer(miPkSz));
   *static_cast<ExgMdHead0*>(miHead) = e.Pk_;
   *static_cast<ExgMdHeadVerLen*>(miHead) = e.Pk_;
   fon9::ToPackBcd(miHead->InformationSeq_, ++msgh.LastSeq_);
   memcpy(miHead + 1, &e.Pk_ + 1, miPkSz - sizeof(ExgMiNoBody));
   FinalMiMessage(*miHead, miPkSz, buf.MoveOut());
}
void ExgMcToMiConv::FinalMiMessage(ExgMiHead& pk, unsigned pksz, fon9::BufferList&& buf) {
   // 重算 CheckSum & 0x0d, 0x0a.
   *(reinterpret_cast<char*>(&pk) + pksz - 3) = fon9::PkReceiver::CalcCheckSum(reinterpret_cast<char*>(&pk), pksz);
   *(reinterpret_cast<char*>(&pk) + pksz - 2) = '\xd';
   *(reinterpret_cast<char*>(&pk) + pksz - 1) = '\xa';
   // 通知, 例: ExgMcToMiSender;
   struct Combiner {
      bool operator()(ExgMcToMiConvHandler* subr, ExgMcToMiConv& sender, const ExgMiHead& pk, unsigned pksz) {
         subr->OnExgMiMessage(sender, pk, pksz);
         return true;
      }
   } combiner;
   this->Handlers_.Combine(combiner, *this, pk, pksz);
   // 寫入 PkLog_;
   if (this->PkLog_)
      this->PkLog_->Append(std::move(buf));
}
//--------------------------------------------------------------------------//
class ExgMcToMiSender : public fon9::io::Session, public ExgMcToMiConvHandler {
   fon9_NON_COPY_NON_MOVE(ExgMcToMiSender);
   using base = fon9::io::Session;
   fon9::io::Device* Device_{};
   uint64_t          SentCount_{};
public:
   const ExgMcToMiConvSP   Conv_;

   ExgMcToMiSender(ExgMcToMiConvSP conv) : Conv_{std::move(conv)} {
   }
   ~ExgMcToMiSender() {
      this->Conv_->Unsubscribe(*this);
   }
   fon9::io::RecvBufferSize OnDevice_LinkReady(fon9::io::Device& dev) override {
      this->Device_ = &dev;
      this->Conv_->Subscribe(*this);
      this->SentCount_ = 0;
      return fon9::io::RecvBufferSize::NoRecvEvent;
   }
   void OnDevice_StateChanged(fon9::io::Device& dev, const fon9::io::StateChangedArgs& e) override {
      (void)dev;
      if (e.BeforeState_ == fon9::io::State::LinkReady)
         this->Conv_->Unsubscribe(*this);
   }
   fon9::io::RecvBufferSize OnDevice_Recv(fon9::io::Device&, fon9::DcQueue& rxbuf) override {
      rxbuf.PopConsumedAll();
      return fon9::io::RecvBufferSize::NoRecvEvent;
   }
   void OnExgMiMessage(ExgMcToMiConv&, const ExgMiHead& pk, unsigned pksz) override {
      if (this->Device_) {
         this->Device_->Send(&pk, pksz);
         ++this->SentCount_;
      }
   }
   std::string SessionCommand(fon9::io::Device& dev, fon9::StrView cmdln) {
      (void)dev;
      cmdln = StrFetchTrim(cmdln, &isspace);
      if (cmdln == "info")
         return RevPrintTo<std::string>(UtcNow(), "|sentCount=", this->SentCount_);
      return "unknown ExgMcReceiver command";
   }
};
//--------------------------------------------------------------------------//
class ExgMcToMiSenderServer : public fon9::io::SessionServer {
   fon9_NON_COPY_NON_MOVE(ExgMcToMiSenderServer);
public:
   const ExgMcToMiConvSP Conv_;
   ExgMcToMiSenderServer(ExgMcToMiConvSP conv) : Conv_{std::move(conv)} {
   }
   fon9::io::SessionSP OnDevice_Accepted(fon9::io::DeviceServer&) {
      return new ExgMcToMiSender(this->Conv_);
   }
};
//--------------------------------------------------------------------------//
static ExgMcToMiConvSP ParseExgMcToMiSenderArgs(IoManager& ioMgr, const IoConfigItem& cfg, std::string& errReason) {
   if (auto mgr = dynamic_cast<ExgMcGroupIoMgr*>(&ioMgr)) {
      ExgMcToMiConvSP conv;
      StrView  tag, value, args = ToStrView(cfg.SessionArgs_);
      while (StrFetchTagValue(args, tag, value)) {
         if (tag == "Conv") {
            conv = mgr->McGroup_->Sapling_->Get<ExgMcToMiConv>(value);
            if (!conv) {
               errReason = "f9twf.ExgMcToMiSenderFactory.CreateSession: Not found Conv=";
               value.AppendTo(errReason);
               return nullptr;
            }
         }
      }
      if (conv)
         return conv;
      errReason = "f9twf.ExgMcToMiSenderFactory.CreateSession: Not found Conv";
      return nullptr;
   }
   errReason = "f9twf.ExgMcToMiSenderFactory.CreateSession: Unknown IoMgr.";
   return nullptr;
}
io::SessionSP ExgMcToMiSenderFactory::CreateSession(IoManager& ioMgr, const IoConfigItem& cfg, std::string& errReason) {
   if (ExgMcToMiConvSP conv = ParseExgMcToMiSenderArgs(ioMgr, cfg, errReason))
      return new ExgMcToMiSender(std::move(conv));
   return nullptr;
}
io::SessionServerSP ExgMcToMiSenderFactory::CreateSessionServer(IoManager& ioMgr, const IoConfigItem& cfg, std::string& errReason) {
   if (ExgMcToMiConvSP conv = ParseExgMcToMiSenderArgs(ioMgr, cfg, errReason))
      return new ExgMcToMiSenderServer(std::move(conv));
   return nullptr;
}

} // namespaces
