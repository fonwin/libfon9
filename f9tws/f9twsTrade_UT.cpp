// \file f9tws/f9twsTrade_UT.cpp
// \author fonwinz@gmail.com
//--------------------------------------------------------------------------//
//
// 當專案愈趨龐大時, 應將每個 #include 分隔的區塊, 獨立到單獨的 hpp/cpp 裡面;
//
//--------------------------------------------------------------------------//
#include "fon9/seed/Plugins.hpp"
extern "C" fon9_API fon9::seed::PluginsDesc f9p_NamedIoManager;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpServer;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpClient;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_Dgram;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_HttpSession; // & f9p_WsSeedVisitor & f9p_HttpStaticDispatcher;
#ifdef F9CARD
extern "C"          fon9::seed::PluginsDesc f9p_F9Card;
#endif
void* ForceLinkSomething() {
   static const void* forceLinkList[]{
      &f9p_NamedIoManager, &f9p_TcpServer, &f9p_TcpClient, &f9p_Dgram,
      &f9p_HttpSession,
   #ifdef F9CARD
      &f9p_F9Card,
   #endif
   };
   return forceLinkList;
}
//--------------------------------------------------------------------------//
#include "fon9/fmkt/TradingRequest.hpp"
#include "fon9/fix/FixBase.hpp"
class TwsTradingRequest : public fon9::fmkt::TradingRequest {
   fon9_NON_COPY_NON_MOVE(TwsTradingRequest);
   static std::string NormalizeToFixMsg(std::string&& fixFields) {
      std::replace(fixFields.begin(), fixFields.end(), '|', f9fix_kCHAR_SPL);
      if (*fixFields.c_str() != f9fix_kCHAR_SPL)
         fixFields.insert(0, 1, f9fix_kCHAR_SPL);
      return std::move(fixFields);
   }
public:
   // fix header: "|TargetSubID#57=0(盤中整股)|SenderSubID#50=下單帳號所屬券商"
   const std::string FixHeaderFields_;
   const std::string FixBodyFields_;
   TwsTradingRequest(f9fmkt_RxKind rxKind, std::string&& fixHeaderFields, std::string&& fixBodyFields)
      : FixHeaderFields_(NormalizeToFixMsg(std::move(fixHeaderFields)))
      , FixBodyFields_(NormalizeToFixMsg(std::move(fixBodyFields))) {
      this->RxKind_ = rxKind;
   }
   // 支援: 流量管制時, 檢查排隊中的其他下單要求.
   // 例如: this 為刪單要求, 是否允許檢查[排隊中的新單]要求, 在送出前就直接先刪單?
   // bool PreOpQueuingRequest(fon9::fmkt::TradingLineManager& from) const override {
   //    return this->RxKind() != f9fmkt_RxKind_RequestNew;
   // }
   // OpQueuingRequestResult OpQueuingRequest(fon9::fmkt::TradingLineManager& from, TradingRequest& queuingRequest) override {
   //    (void)from; (void)queuingRequest;
   //    return Op_AllDone;    // this 及 queuingRequest 都已完成, 不用送給交易所.
   //    return Op_TargetDone; // queuingRequest 已完成不用送交易所, 但 this 仍要送給交易所.
   //    return Op_ThisDone;   // queuingRequest 仍要送交易所, 但 this 已完成不用送給交易所.
   //    return Op_NotTarget;  // queuingRequest 不是 this 要處理的對象, 繼續檢查下一個排隊中的要求.
   // }
};
using TwsTradingRequestSP = fon9::intrusive_ptr<TwsTradingRequest>;
TwsTradingRequestSP MakeTwsTradingRequest(fon9::StrView cmdln) {
   fon9::StrView cmdop = fon9::StrFetchTrim(cmdln, &fon9::isspace);
   f9fmkt_RxKind rxKind;
   if (cmdop == "new")
      rxKind = f9fmkt_RxKind_RequestNew;
   else if (cmdop == "del")
      rxKind = f9fmkt_RxKind_RequestDelete;
   else if (cmdop == "chg")
      rxKind = f9fmkt_RxKind_RequestChgQty;
   else
      return TwsTradingRequestSP{};
   fon9::StrView header = fon9::StrFetchTrim(cmdln, &fon9::isspace);
   fon9::StrTrim(&cmdln);
   return TwsTradingRequestSP{new TwsTradingRequest{rxKind, header.ToString(), cmdln.ToString()}};
}
//--------------------------------------------------------------------------//
#include "f9tws/ExgTradingLineFixFactory.hpp"
#include "fon9/fix/FixApDef.hpp"
static void OnFixReject(const fon9::fix::FixRecvEvArgs& rxargs, const fon9::fix::FixOrigArgs& orig) {
   // SessionReject or BusinessReject: 不該發生這些事.
   fon9_LOG_INFO("OnFixReject|orig=", orig.MsgStr_, "|rx=", rxargs.OrigMsgStr());
}
static void OnFixReport(const fon9::fix::FixRecvEvArgs& rxargs) {
   fon9_LOG_INFO("OnFixReport|rx=", rxargs.OrigMsgStr());
   // 處理回報內容: 解析 FIX 欄位.
   const fon9::fix::FixParser::FixField*  fixfld;
   fon9::RevBufferFixedSize<128>          rbuf;
   rbuf.RewindEOS();
   //-----
   #define OUT_FIX_FIELD_VALUE(tagName)                                 \
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_##tagName)) != nullptr)\
      fon9::RevPrint(rbuf, fixfld->Value_);                             \
   fon9::RevPrint(rbuf, "|" #tagName "=")
   //-----
   OUT_FIX_FIELD_VALUE(Text);
   OUT_FIX_FIELD_VALUE(Price);
   OUT_FIX_FIELD_VALUE(OrderQty);
   OUT_FIX_FIELD_VALUE(ClOrdID);
   OUT_FIX_FIELD_VALUE(OrigClOrdID);
   OUT_FIX_FIELD_VALUE(OrderID);
   fon9::RevPrint(rbuf, "\n" "OnFixReport:");
   #undef OUT_FIX_FIELD_VALUE
   // 回報處理也會占用系統資源, 應盡量簡化,
   // 實際應用嚴禁在 console 輸出訊息, 因為 console 輸出, 非常耗費時間!
   puts(rbuf.GetCurrent());
}
static void OnFixExecutionReport(const fon9::fix::FixRecvEvArgs& rxargs) {
   OnFixReport(rxargs);
}
static void OnFixCancelReject(const fon9::fix::FixRecvEvArgs& rxargs) {
   OnFixReport(rxargs);
}
class TwsTradingLineFix : public f9tws::ExgTradingLineFix {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineFix);
   using base = f9tws::ExgTradingLineFix;

public:
   using base::base;
   SendResult SendRequest(fon9::fmkt::TradingRequest& req) override {
      auto now = fon9::UtcNow();
      auto ti = this->FlowCounter_.Fetch(now);
      if (ti.GetOrigValue() > 0) // 流量管制: 須等候的時間, 等時間過了之後才可繼續傳送.
         return fon9::fmkt::ToFlowControlResult(ti);

      const TwsTradingRequest* twsreq = static_cast<TwsTradingRequest*>(&req);
      fon9::fix::FixBuilder fixmsg;
      auto& fixb = fixmsg.GetBuffer();
      fon9::RevPrint(fixb, twsreq->FixBodyFields_);
      // "|TransactTime=yyyymmdd-hh:mm:ss.mmm"
      fixmsg.PutUtcTime(now);
      fon9::RevPrint(fixb, f9fix_SPLTAGEQ(TransactTime));
      fon9::RevPrint(fixb, twsreq->FixHeaderFields_);
      fon9::StrView fldMsgType;
      fon9_WARN_DISABLE_SWITCH;
      switch (twsreq->RxKind()) {
      case f9fmkt_RxKind_RequestDelete:
         fldMsgType = f9fix_SPLFLDMSGTYPE(OrderCancelRequest);
         break;
      case f9fmkt_RxKind_RequestChgQty:
      case f9fmkt_RxKind_RequestChgPri:
         fldMsgType = f9fix_SPLFLDMSGTYPE(OrderReplaceRequest);
         break;
      default:
      case f9fmkt_RxKind_RequestNew:
         fldMsgType = f9fix_SPLFLDMSGTYPE(NewOrderSingle);
         break;
      }
      fon9_WARN_POP;
      this->FixSender_->Send(fldMsgType, std::move(fixmsg));
      return SendResult::Sent;
   }
};
class TwsTradingLineFixFactory : public f9tws::ExgTradingLineFixFactory {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineFixFactory);
   using base = f9tws::ExgTradingLineFixFactory;
public:
   TwsTradingLineFixFactory(std::string fixLogPathFmt, Named&& name)
      : base(std::move(fixLogPathFmt), std::move(name)) {
      this->FixConfig_.Fetch(f9fix_kMSGTYPE_OrderCancelRequest).FixRejectHandler_ = &OnFixReject;
      this->FixConfig_.Fetch(f9fix_kMSGTYPE_ExecutionReport)   .FixMsgHandler_    = &OnFixExecutionReport;
      this->FixConfig_.Fetch(f9fix_kMSGTYPE_OrderCancelReject) .FixMsgHandler_    = &OnFixCancelReject;
   }
   fon9::io::SessionSP CreateTradingLineFix(f9tws::ExgTradingLineMgr&           lineMgr,
                                            const f9tws::ExgTradingLineFixArgs& args,
                                            fon9::fix::IoFixSenderSP            fixSender) {
      return fon9::io::SessionSP{new TwsTradingLineFix(lineMgr, this->FixConfig_, args, std::move(fixSender))};
   }
};
//--------------------------------------------------------------------------//
#include "f9tws/ExgTradingLineMgr.hpp"
class TwsTradingLineMgr : public f9tws::ExgTradingLineMgr {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineMgr);
   using base = f9tws::ExgTradingLineMgr;
public:
   using base::base;
   fon9::fmkt::SendRequestResult NoReadyLineReject(fon9::fmkt::TradingRequest& req, fon9::StrView cause) override {
      printf("NoReadyLineReject: req=[%p] %s\n", &req, cause.ToString().c_str());
      return fon9::fmkt::SendRequestResult::NoReadyLine;
   }
   fon9::fmkt::SendRequestResult RequestPushToQueue(fon9::fmkt::TradingRequest& req, const Locker& tsvr) override {
      // 因流量管制, 所以 req 進入排隊狀態.
      printf("PushQueue: req=[%p]\n", &req);
      return base::RequestPushToQueue(req, tsvr);
   }
};
class TwsTradingLineMgrSeed : public fon9::seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(TwsTradingLineMgrSeed);
   using base = fon9::seed::NamedSapling;
public:
   TwsTradingLineMgrSeed(const fon9::IoManagerArgs& ioargs, f9fmkt_TradingMarket mkt)
      : base(new TwsTradingLineMgr(ioargs, fon9::TimeInterval_Second(1), mkt), ioargs.Name_) {
      this->SetTitle(ioargs.CfgFileName_);
      this->SetDescription(ioargs.Result_);
      // 可將 this->TradingLineMgr() 提供給其他物件參考,
      // 當該物件有需要送出下單訊息時, 可直接使用。
   }
   f9tws::ExgTradingLineMgr& TradingLineMgr() const {
      return *static_cast<f9tws::ExgTradingLineMgr*>(this->Sapling_.get());
   }
   void OnSeedCommand(fon9::seed::SeedOpResult& res, fon9::StrView cmdln, fon9::seed::FnCommandResultHandler resHandler,
                      fon9::seed::MaTreeBase::Locker&& ulk, fon9::seed::SeedVisitor* visitor) override {
      if (auto preq = MakeTwsTradingRequest(cmdln)) {
         fon9::StrView rmsg;
         switch (this->TradingLineMgr().SendRequest(*preq)) {
         case fon9::fmkt::SendRequestResult::Sent:          rmsg = "request sent.";             break;
         case fon9::fmkt::SendRequestResult::NoReadyLine:   rmsg = "request no ready line.";    break;
         case fon9::fmkt::SendRequestResult::RejectRequest: rmsg = "request reject.";           break;
         case fon9::fmkt::SendRequestResult::Queuing:       rmsg = "request queuing.";          break;
         case fon9::fmkt::SendRequestResult::RequestEnd:    rmsg = "request done.";             break;
         default:                                           rmsg = "request unknown result.";   break;
         }
         resHandler(res, rmsg);
      }
      else {
         base::OnSeedCommand(res, cmdln, std::move(resHandler), std::move(ulk), visitor);
      }
   }
};
using TwsTradingLineMgrSeedSP = fon9::intrusive_ptr<TwsTradingLineMgrSeed>;
//--------------------------------------------------------------------------//
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/fmkt/SymbTws.hpp"
#include "fon9/fmkt/SymbDeal.hpp"
#include "fon9/fmkt/SymbBS.hpp"
class SymbMd : public fon9::fmkt::SymbTws {
   fon9_NON_COPY_NON_MOVE(SymbMd);
   using base = fon9::fmkt::SymbTws;
   using PendingReqsImpl = std::vector<TwsTradingRequestSP>;
   using PendingReqs = fon9::MustLock<PendingReqsImpl>;
   PendingReqs PendingReqs_;
   static fon9::fmkt::SymbData* GetSymbMdData(SymbMd* pthis, int tabid) {
      static const int32_t kSymbMdOffset[]{
         0,
         fon9_OffsetOf(SymbMd, LastDeal_),
         fon9_OffsetOf(SymbMd, LastBS_),
      };
      if (static_cast<size_t>(tabid) < fon9::numofele(kSymbMdOffset))
         return reinterpret_cast<fon9::fmkt::SymbData*>(reinterpret_cast<char*>(pthis) + kSymbMdOffset[tabid]);
      return nullptr;
   }
public:
   fon9::fmkt::SymbDeal  LastDeal_;
   fon9::fmkt::SymbTwsBS LastBS_;

   using base::base;
   ~SymbMd() {
      this->RejectAllRequests("SymbMd dtor.");
   }
   void RejectAllRequests(fon9::StrView cause) {
      auto reqs = this->PendingReqs_.Lock();
      for (auto& preq : *reqs) {
         (void)cause; (void)preq;// TODO: 通知 *preq 尚未觸發前, 就取消下單.
      }
      reqs->clear();
   }
   static fon9::seed::LayoutSP MakeLayout() {
      fon9::seed::Fields baseFlds = base::MakeFields();
      fon9::seed::Fields dealFlds = fon9::fmkt::SymbTwsDeal_MakeFields(true);
      fon9::seed::Fields bsFlds   = fon9::fmkt::SymbTwsBS_MakeFields(true);
      return fon9::seed::LayoutSP{new fon9::seed::LayoutN(
         fon9_MakeField(SymbMd, SymbId_, "Id"),
         fon9::seed::TreeFlag::AddableRemovable | fon9::seed::TreeFlag::Unordered,
         fon9::seed::TabSP{new fon9::seed::Tab{fon9::Named{"Base"},     std::move(baseFlds), fon9::seed::TabFlag::NoSapling_NoSeedCommand_Writable}},
         fon9::seed::TabSP{new fon9::seed::Tab{fon9::Named{"LastDeal"}, std::move(dealFlds), fon9::seed::TabFlag::NoSapling_NoSeedCommand_Writable}},
         fon9::seed::TabSP{new fon9::seed::Tab{fon9::Named{"LastBS"},   std::move(bsFlds),   fon9::seed::TabFlag::NoSapling_NoSeedCommand_Writable}}
      )};
   }
   fon9::fmkt::SymbData* GetSymbData(int tabid) override {
      return GetSymbMdData(this, tabid);
   }
   fon9::fmkt::SymbData* FetchSymbData(int tabid) override {
      return GetSymbMdData(this, tabid);
   }
   void AddRequest(TwsTradingRequestSP req) {
      this->PendingReqs_.Lock()->push_back(std::move(req));
   }
   void FireSendRequests(f9tws::ExgTradingLineMgr& lineMgr) {
      auto reqs = this->PendingReqs_.Lock();
      for (auto& preq : *reqs) {
         lineMgr.SendRequest(*preq);
      }
      reqs->clear();
   }
};
class SymbMdTree : public fon9::fmkt::MdSymbTree {
   fon9_NON_COPY_NON_MOVE(SymbMdTree);
   using base = fon9::fmkt::MdSymbTree;
public:
   SymbMdTree() : base{SymbMd::MakeLayout()} {
   }
   fon9::fmkt::SymbSP MakeSymb(const fon9::StrView& symbid) override {
      return fon9::fmkt::SymbSP{new SymbMd(symbid)};
   }
};
class SymbMdMgr : public fon9::seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(SymbMdMgr);
   using base = fon9::seed::NamedSapling;
public:
   const TwsTradingLineMgrSeedSP IoTse_;
   const TwsTradingLineMgrSeedSP IoOtc_;
   SymbMdMgr(TwsTradingLineMgrSeedSP ioTse, TwsTradingLineMgrSeedSP ioOtc, std::string name)
      : base(new SymbMdTree(), std::move(name))
      , IoTse_{std::move(ioTse)}
      , IoOtc_{std::move(ioOtc)} {
   }
   SymbMdTree& GetSymbs() {
      return *static_cast<SymbMdTree*>(this->Sapling_.get());
   }
   void OnSeedCommand(fon9::seed::SeedOpResult& res, fon9::StrView cmdln, fon9::seed::FnCommandResultHandler resHandler,
                      fon9::seed::MaTreeBase::Locker&& ulk, fon9::seed::SeedVisitor* visitor) override {
      fon9::StrView cmdop = fon9::StrFetchTrim(cmdln, &fon9::isspace);
      if (cmdop == "wait") {
         // 簡易條件判斷: 當 [成交價異動] 時送出下單要求.
         // wait symbid new ...
         // wait symbid chg ...
         // wait symbid del ...
         cmdop = fon9::StrFetchTrim(cmdln, &fon9::isspace); // cmdop = symbid;
         auto&  symbs = this->GetSymbs();
         auto   symblk = symbs.SymbMap_.Lock();
         auto   symbmd = fon9::static_pointer_cast<SymbMd>(symbs.FetchSymb(symblk, cmdop));
         assert(symbmd.get() != nullptr);
         fon9::StrView rmsg;
         if (auto preq = MakeTwsTradingRequest(cmdln)) {
            symbmd->AddRequest(preq);
            rmsg = "Request pushed to the waiting list.";
         }
         else {
            rmsg = "Unknown request kind";
         }
         resHandler(res, rmsg);
      }
      else {
         base::OnSeedCommand(res, fon9::StrView{cmdop.begin(), cmdln.end()}, std::move(resHandler), std::move(ulk), visitor);
      }
   }
};
using SymbMdMgrSP = fon9::intrusive_ptr<SymbMdMgr>;
//--------------------------------------------------------------------------//
#include "f9tws/ExgMdFmt6.hpp"
#include "f9tws/ExgMdPkReceiver.hpp"
#include "fon9/PkCont.hpp"
class TwsMdFeeder : public f9tws::ExgMdPkReceiver {
   fon9_NON_COPY_NON_MOVE(TwsMdFeeder);
   using base = f9tws::ExgMdPkReceiver;

   // 如果不考慮「同資訊多來源備援」、「資料亂序」造成的問題:
   // - 則可以不使用 fon9::PkContFeeder，直接解析收到的資料.
   // - 若不使用 fon9::PkContFeeder 處理亂序問題, 則:
   //   SymbMd 需要增加一個 Fmt6 的序號欄位, 用來判斷是否為舊資料.
   class Fmt6Feeder : public fon9::PkContFeeder {
      fon9_NON_COPY_NON_MOVE(Fmt6Feeder);
      using base = fon9::PkContFeeder;
      void PkContOnReceived(const void* pk, unsigned pksz, SeqT seq) override;
   public:
      SymbMdMgrSP SymbMgr_;
      Fmt6Feeder(SymbMdMgrSP symbMgr)
         : SymbMgr_(std::move(symbMgr)) {
      }
      ~Fmt6Feeder() {
         this->Clear();
      }
   };
   Fmt6Feeder  Fmt6Feeder_;
   Fmt6Feeder  Fmt17Feeder_;
   bool OnPkReceived(const void* pkptr, unsigned pksz) override {
      const f9tws::ExgMdHead& pk = *reinterpret_cast<const f9tws::ExgMdHead*>(pkptr);
      switch (*pk.FmtNo_) {
      case 0x06:  this->Fmt6Feeder_ .FeedPacket(&pk, pksz, pk.GetSeqNo());  break;
      case 0x17:  this->Fmt17Feeder_.FeedPacket(&pk, pksz, pk.GetSeqNo()); break;
      }
      return true;
   }
public:
   TwsMdFeeder(SymbMdMgrSP symbMgr) : Fmt6Feeder_(symbMgr), Fmt17Feeder_(symbMgr) {
   }
};
class TwsMdFeederFactory;
using TwsMdFeederFactorySP = fon9::intrusive_ptr<TwsMdFeederFactory>;
class TwsMdSession : public fon9::io::Session {
   fon9_NON_COPY_NON_MOVE(TwsMdSession);
   using base = fon9::io::Session;
   // 某些 dev 可能不支援 FnOnDevice_RecvDirect, 此時使用 OnDevice_Recv() 觸發接收事件;
   fon9::io::RecvBufferSize OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueue& rxbuf) override;
   static fon9::io::RecvBufferSize OnDeviceRecv(fon9::io::RecvDirectArgs& e);
public:
   const TwsMdFeederFactorySP Feeder_;
   TwsMdSession(TwsMdFeederFactorySP feeder)
      : base{&TwsMdSession::OnDeviceRecv}
      , Feeder_{std::move(feeder)} {
   }
};
// 同一個 Factory 建立出來的 ExgMdFeeder Session 共用同一個 ExgMdFeeder.
// 如此可支援多條線路備援, 降低「封包亂序或遺失」造成的延遲.
class TwsMdFeederFactory : public fon9::SessionFactory, public TwsMdFeeder {
   fon9_NON_COPY_NON_MOVE(TwsMdFeederFactory);
   using baseFactory = fon9::SessionFactory;
   using baseFeeder = TwsMdFeeder;
public:
   TwsMdFeederFactory(SymbMdMgrSP symbMgr, fon9::Named&& name)
      : baseFactory{std::move(name)}
      , baseFeeder(std::move(symbMgr)) {
   }
   fon9::io::SessionSP CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) override {
      (void)mgr; (void)cfg; (void)errReason;
      return fon9::io::SessionSP{new TwsMdSession{this}};
   }
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) override {
      (void)mgr; (void)cfg;
      errReason = "TwsMdFeederFactory|err=Not support Server.";
      return fon9::io::SessionServerSP{};
   }
};
fon9::io::RecvBufferSize TwsMdSession::OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueue& rxbuf) {
   (void)dev;
   this->Feeder_->FeedBuffer(rxbuf);
   return fon9::io::RecvBufferSize::Default;
}
fon9::io::RecvBufferSize TwsMdSession::OnDeviceRecv(fon9::io::RecvDirectArgs& e) {
   TwsMdSession* pthis = static_cast<TwsMdSession*>(e.Device_.Session_.get());
   pthis->Feeder_->FeedBuffer(e.RecvBuffer_);
   return fon9::io::RecvBufferSize::Default;
}
// 收到連續(或跳號但逾時)的 FMT6/FMT17 封包時, 來到此處解析內容.
void TwsMdFeeder::Fmt6Feeder::PkContOnReceived(const void* pk, unsigned pksz, SeqT seq) {
   (void)pksz;
   const f9tws::ExgMdFmt6v4& fmt6 = *reinterpret_cast<const f9tws::ExgMdFmt6v4*>(pk);
   assert(fmt6.GetVerNo() == 4);
   this->CheckLogLost(pk, seq, nullptr);
   if (fmt6.IsLastDealSent())
      return;
   auto&  symbs  = this->SymbMgr_->GetSymbs();
   auto   symblk = symbs.SymbMap_.Lock();
   auto   symbmd = fon9::static_pointer_cast<SymbMd>(symbs.GetSymb(symblk, ToStrView(fmt6.StkNo_)));
   if (!symbmd)
      return;
   const auto                 prevDealPri = symbmd->LastDeal_.Data_.Deal_.Pri_;
   const auto                 dealTime = fmt6.Time_.ToDayTime();
   const f9tws::ExgMdPriQty*  pqs = fmt6.PQs_;
   if (fmt6.ItemMask_ & 0x80) {
      auto& data = symbmd->LastDeal_.Data_;
      data.InfoTime_  = dealTime;
      data.TotalQty_  = fon9::PackBcdTo<decltype(data.TotalQty_)>(fmt6.TotalQty_);
      data.MarketSeq_ = static_cast<fon9::fmkt::MarketDataSeq>(seq);
      pqs->AssignTo(data.Deal_);
      ++pqs;
   }
   if (fon9_LIKELY((fmt6.ItemMask_ & 0x7e) || (fmt6.ItemMask_ & 1) == 0)) {
      auto& data = symbmd->LastBS_.Data_;
      data.InfoTime_ = dealTime;
      pqs = f9tws::AssignBS(data.Buys_, pqs, fon9::unsigned_cast((fmt6.ItemMask_ & 0x70) >> 4));
      pqs = f9tws::AssignBS(data.Sells_, pqs, fon9::unsigned_cast((fmt6.ItemMask_ & 0x0e) >> 1));
      data.MarketSeq_ = static_cast<fon9::fmkt::MarketDataSeq>(seq);
   }
   if (prevDealPri != symbmd->LastDeal_.Data_.Deal_.Pri_) {
      // 條件成立, 觸發下單送出.
      /// fmt6.Market_: 0x01=TSE, 0x02=OTC
      symbmd->FireSendRequests((fmt6.Market_[0] == 0x02 ? this->SymbMgr_->IoOtc_ : this->SymbMgr_->IoTse_)->TradingLineMgr());
   }
}
//--------------------------------------------------------------------------//
#include "fon9/seed/SysEnv.hpp"
static bool f9twsTrade_PluginStart(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
   #define kCSTR_DevFpName    "FpDevice"
   auto devfp = fon9::seed::FetchNamedPark<fon9::DeviceFactoryPark>(*holder.Root_, kCSTR_DevFpName);
   if (!devfp) { // 無法取得系統共用的 DeviceFactoryPark.
      holder.SetPluginsSt(fon9::LogLevel::Error, "f9twsTrade|err=DeviceFactoryPark not found: " kCSTR_DevFpName);
      return false;
   }

   std::string    instName;
   fon9::StrView  tag, value;
   fon9::StrView  ioTse, ioOtc;
   while (fon9::SbrFetchTagValue(args, tag, value)) {
      fon9::StrView* dst;
      if (fon9::iequals(tag, "IoTse"))
         dst = &ioTse;
      else if (fon9::iequals(tag, "IoOtc"))
         dst = &ioOtc;
      else if (fon9::iequals(tag, "TryLastLine")) {
         fon9::fmkt::gTradingLineSelect_TryLastLine_YN = StrTo(value, fon9::fmkt::gTradingLineSelect_TryLastLine_YN);
         continue;
      }
      else {
         holder.SetPluginsSt(fon9::LogLevel::Error, "f9twsTrade|err=Unknown tag: ", tag);
         return false;
      }
      if ((*dst = fon9::SbrFetchInsideNoTrim(value)) == nullptr)
         *dst = value;
      fon9::StrTrim(dst);
   }
   if (instName.empty())
      instName = "f9twsTrade";

   const fon9::seed::MaTreeSP maTree{new fon9::seed::MaTree{instName}};
   // 將 f9twsTradeMaTree 加入 root.
   std::string description;
   if (fon9::fmkt::gTradingLineSelect_TryLastLine_YN == fon9::EnabledYN::Yes) {
      description = "TryLastLine=Y";
   }
   if (!holder.Root_->Add(new fon9::seed::NamedSapling(maTree, instName, std::string{}, description))) {
      holder.SetPluginsSt(fon9::LogLevel::Error, "f9twsTrade|err=Dup PluginName: ", instName);
      return false;
   }
   const std::string       logpath = fon9::seed::SysEnv_GetLogFileFmtPath(*holder.Root_);
   const std::string       cfgpathFH = SysEnv_GetConfigPath(*holder.Root_).ToString() + instName;
   fon9::SessionFactorySP  fix44fac{new TwsTradingLineFixFactory(logpath, fon9::Named{"Fix44"})};
   fon9::IoManagerArgs     iomArgs;
   #define kCSTR_SesFpName "FpSession"
   iomArgs.SessionFactoryPark_ = fon9::seed::FetchNamedPark<fon9::SessionFactoryPark>(*maTree, kCSTR_SesFpName);
   iomArgs.SessionFactoryPark_->Add(std::move(fix44fac));
   // ----- 建立 [TSEC] 下單線路管理員.
   iomArgs.DeviceFactoryPark_ = devfp;
   iomArgs.IoServiceCfgstr_ = ioTse.empty() ? "ThreadCount=2" : ioTse.ToString();
   iomArgs.Name_ = "IoTse";
   iomArgs.CfgFileName_ = cfgpathFH + "_IoTse.f9gv";
   TwsTradingLineMgrSeedSP ioTseMgr{new TwsTradingLineMgrSeed(iomArgs, f9fmkt_TradingMarket_TwSEC)};
   maTree->Add(ioTseMgr);
   // ----- 建立 [OTC] 下單線路管理員.
   // Otc下單線路, 可與「Tse下單線路:IoTse」共用 iomArgs.IoServiceSrc_;
   if (fon9::iequals(ioOtc, "IoTse"))
      iomArgs.IoServiceSrc_ = &ioTseMgr->TradingLineMgr();
   else {
      iomArgs.IoServiceSrc_.reset();
      iomArgs.IoServiceCfgstr_ = ioOtc.empty() ? "ThreadCount=2" : ioOtc.ToString();
   }
   iomArgs.Name_ = "IoOtc";
   iomArgs.CfgFileName_ = cfgpathFH + "_IoOtc.f9gv";
   TwsTradingLineMgrSeedSP ioOtcMgr{new TwsTradingLineMgrSeed(iomArgs, f9fmkt_TradingMarket_TwOTC)};
   maTree->Add(ioTseMgr);
   // ----- 建立商品資料表: 提供給條件下單使用.
   SymbMdMgrSP symbMgr{new SymbMdMgr(ioTseMgr, ioOtcMgr, "Symbs")};
   maTree->Add(symbMgr);
   iomArgs.SessionFactoryPark_->Add(new TwsMdFeederFactory(symbMgr, fon9::Named{"TseMd"}));
   iomArgs.SessionFactoryPark_->Add(new TwsMdFeederFactory(symbMgr, fon9::Named{"OtcMd"}));
   return true;
}
extern "C" fon9::seed::PluginsDesc  f9p_f9twsTrade;
static fon9::seed::PluginsPark      f9p_f9twsTrade_reg{"f9twsTrade", &f9p_f9twsTrade};
fon9::seed::PluginsDesc             f9p_f9twsTrade{"", &f9twsTrade_PluginStart, nullptr, nullptr,};
//--------------------------------------------------------------------------//
#include "fon9/framework/Framework.hpp"
#include "fon9/ConsoleIO.h"
#include "fon9/CtrlBreakHandler.h"
int main(int argc, const char* argv[]) {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif
#if defined(NDEBUG)
   fon9::LogLevel_ = fon9::LogLevel::Info;
#endif
   fon9_SetConsoleUTF8();
   fon9_SetupCtrlBreakHandler();

   fon9::Framework   f9system;
   f9system.Initialize(argc, argv);
   fon9::PlantMaAuth_UserAgent(f9system);
   return Fon9SysCoStart(f9system, argc, argv, nullptr);
}
//--------------------------------------------------------------------------//
#ifdef fon9_POSIX
fon9_GCC_WARN_DISABLE("-Wold-style-cast")
fon9_GCC_WARN_DISABLE_NO_PUSH("-Wnonnull")
// 使用 -static 靜態連結執行程式, 使用 std::condition_variable, std::thread 可能造成 crash 的問題.
void fixed_pthread_bug_in_cpp_std() {
   pthread_cond_signal((pthread_cond_t *) nullptr);
   pthread_cond_broadcast((pthread_cond_t *) nullptr);
   pthread_cond_init((pthread_cond_t *) nullptr,
      (const pthread_condattr_t *) nullptr);
   pthread_cond_destroy((pthread_cond_t *) nullptr);
   pthread_cond_timedwait((pthread_cond_t *) nullptr, (pthread_mutex_t *)
                          nullptr, (const struct timespec *) nullptr);
   pthread_cond_wait((pthread_cond_t *) nullptr, (pthread_mutex_t *)(nullptr));

   pthread_detach(pthread_self());
   pthread_join(pthread_self(), NULL);
}
fon9_GCC_WARN_POP
#endif
