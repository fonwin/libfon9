// \file f9tws/f9twsTrade_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/framework/Framework.hpp"
#include "fon9/ConsoleIO.h"
#include "fon9/CtrlBreakHandler.h"
//--------------------------------------------------------------------------//
#include "fon9/seed/Plugins.hpp"
extern "C" fon9_API fon9::seed::PluginsDesc f9p_NamedIoManager;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpServer;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpClient;
extern "C" fon9_API fon9::seed::PluginsDesc f9p_HttpSession; // & f9p_WsSeedVisitor & f9p_HttpStaticDispatcher;

#ifdef F9CARD
extern "C"          fon9::seed::PluginsDesc f9p_F9Card;
#endif

void* ForceLinkSomething() {
   static const void* forceLinkList[]{
      &f9p_NamedIoManager, &f9p_TcpServer, &f9p_TcpClient,
      &f9p_HttpSession,
   #ifdef F9CARD
      &f9p_F9Card,
   #endif
   };
   return forceLinkList;
}
//--------------------------------------------------------------------------//
#include "f9tws/ExgTradingLineFixFactory.hpp"
#include "f9tws/ExgTradingLineMgr.hpp"
#include "fon9/fix/FixBusinessReject.hpp"
#include "fon9/fix/FixAdminDef.hpp"
#include "fon9/fix/FixApDef.hpp"
#include "fon9/seed/SysEnv.hpp"

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
//--------------------------------------------------------------------------//
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
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_Text)) != nullptr)
      fon9::RevPrint(rbuf, fixfld->Value_);
   fon9::RevPrint(rbuf, "|Text=");
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_ClOrdID)) != nullptr)
      fon9::RevPrint(rbuf, fixfld->Value_);
   fon9::RevPrint(rbuf, "|ClOrdID=");
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_OrigClOrdID)) != nullptr)
      fon9::RevPrint(rbuf, fixfld->Value_);
   fon9::RevPrint(rbuf, "|OrigClOrdID=");
   if ((fixfld = rxargs.Msg_.GetField(f9fix_kTAG_OrderID)) != nullptr)
      fon9::RevPrint(rbuf, fixfld->Value_);
   fon9::RevPrint(rbuf, "\n" "OnFixReport:OrderID=");
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
   fon9::fmkt::SendRequestResult SendRequest(f9fmkt_RxKind rxKind, fon9::StrView cmdln) {
      fon9::StrView header = fon9::StrFetchTrim(cmdln, &fon9::isspace);
      fon9::StrTrim(&cmdln);
      fon9::intrusive_ptr<TwsTradingRequest> req{new TwsTradingRequest{rxKind, header.ToString(), cmdln.ToString()}};
      return this->TradingLineMgr().SendRequest(*req);
   }
   void OnSeedCommand(fon9::seed::SeedOpResult& res, fon9::StrView cmdln, fon9::seed::FnCommandResultHandler resHandler,
                      fon9::seed::MaTreeBase::Locker&& ulk, fon9::seed::SeedVisitor* visitor) override {
      fon9::StrView cmdop = fon9::StrFetchTrim(cmdln, &fon9::isspace);
      f9fmkt_RxKind rxKind;
      if (cmdop == "new")
         rxKind = f9fmkt_RxKind_RequestNew;
      else if (cmdop == "del")
         rxKind = f9fmkt_RxKind_RequestDelete;
      else if (cmdop == "chg")
         rxKind = f9fmkt_RxKind_RequestChgQty;
      else {
         base::OnSeedCommand(res, fon9::StrView{cmdop.begin(), cmdln.end()}, std::move(resHandler), std::move(ulk), visitor);
         return;
      }
      fon9::StrView rmsg;
      switch (this->SendRequest(rxKind, cmdln)) {
      case fon9::fmkt::SendRequestResult::Sent:          rmsg = "request sent.";             break;
      case fon9::fmkt::SendRequestResult::NoReadyLine:   rmsg = "request no ready line.";    break;
      case fon9::fmkt::SendRequestResult::RejectRequest: rmsg = "request reject.";           break;
      case fon9::fmkt::SendRequestResult::Queuing:       rmsg = "request queuing.";          break;
      case fon9::fmkt::SendRequestResult::RequestEnd:    rmsg = "request done.";             break;
      default:                                           rmsg = "request unknown result.";   break;
      }
      resHandler(res, rmsg);
   }
};
static fon9::intrusive_ptr<TwsTradingLineMgrSeed> PlantTradingLineMgr(fon9::seed::MaTree&        maTree,
                                                                      const fon9::IoManagerArgs& ioargs,
                                                                      f9fmkt_TradingMarket       mkt,
                                                                      fon9::SessionFactorySP     fix44fac) {
   fon9::intrusive_ptr<TwsTradingLineMgrSeed> ioMgr{new TwsTradingLineMgrSeed(ioargs, mkt)};
   ioMgr->TradingLineMgr().SessionFactoryPark_->Add(std::move(fix44fac));
   maTree.Add(ioMgr);
   return ioMgr;
}
//--------------------------------------------------------------------------//
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
   iomArgs.DeviceFactoryPark_ = devfp;
   iomArgs.IoServiceCfgstr_ = ioTse.empty() ? "ThreadCount=2" : ioTse.ToString();
   iomArgs.Name_ = "IoTse";
   iomArgs.CfgFileName_ = cfgpathFH + "_IoTse.f9gv";
   fon9::intrusive_ptr<TwsTradingLineMgrSeed> ioTseMgr = PlantTradingLineMgr(*maTree, iomArgs, f9fmkt_TradingMarket_TwSEC, fix44fac);

   // Otc下單線路, 可與「Tse下單線路:IoTse」共用 iomArgs.IoServiceSrc_;
   if (fon9::iequals(ioOtc, "IoTse"))
      iomArgs.IoServiceSrc_ = &ioTseMgr->TradingLineMgr();
   else {
      iomArgs.IoServiceSrc_.reset();
      iomArgs.IoServiceCfgstr_ = ioOtc.empty() ? "ThreadCount=2" : ioOtc.ToString();
   }
   iomArgs.Name_ = "IoOtc";
   iomArgs.CfgFileName_ = cfgpathFH + "_IoOtc.f9gv";
   PlantTradingLineMgr(*maTree, iomArgs, f9fmkt_TradingMarket_TwOTC, fix44fac);
   return true;
}

extern "C" fon9::seed::PluginsDesc  f9p_f9twsTrade;
static fon9::seed::PluginsPark      f9p_f9twsTrade_reg{"f9twsTrade", &f9p_f9twsTrade};
fon9::seed::PluginsDesc             f9p_f9twsTrade{"", &f9twsTrade_PluginStart, nullptr, nullptr,};
//--------------------------------------------------------------------------//
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

   // PlantScramSha256(), PolicyAclAgent::Plant() 要用 plugins 機制嗎?
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
