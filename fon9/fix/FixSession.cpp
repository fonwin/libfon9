// \file fon9/fix/FixSession.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixSession.hpp"
#include "fon9/fix/FixRecorder.hpp"
#include "fon9/fix/FixAdminMsg.hpp"

namespace fon9 { namespace fix {

FixSession::FixSession(const FixConfig& cfg)
   : RxArgs_{FixParser_} {
   this->RxArgs_.FixSession_  = this;
   this->RxArgs_.FixReceiver_ = this;
   this->RxArgs_.FixConfig_   = &cfg;
   this->RxArgs_.FixSender_   = nullptr;
}
FixSession::~FixSession() {
}
//--------------------------------------------------------------------------//
void FixSession::ClearFixSession(FixSessionSt st) {
   {
      StLocker stLocker = this->RunSt_.Lock();
      stLocker->FixSt_ = st;
      stLocker->MsgReceivedCount_ = stLocker->HbTestCount_ = 0;
   }
   this->ClearFeedBuffer();
   this->ClearRecvKeeper();
}
FixSenderSP FixSession::OnFixSessionDisconnected(const StrView& info) {
   this->FixSessionTimerStopNoWait();
   this->ClearFixSession(FixSessionSt::Disconnected);
   if (this->FixSender_) {
      this->FixSender_->GetFixRecorder().Write(f9fix_kCSTR_HdrInfo, "OnFixSessionDisconnected: ", info);
      this->RxArgs_.FixSender_ = nullptr;
   }
   return std::move(this->FixSender_);
}
void FixSession::OnFixSessionConnected() {
   this->ClearFixSession(FixSessionSt::Connected);
   this->FixSessionTimerRunAfter(this->RxArgs_.FixConfig_->TiWaitForLogon_);
}
void FixSession::OnFixMessageParsed(StrView fixmsg) {
   FixSessionSt curst;
   {
      StLocker stLocker = this->RunSt_.Lock();
      ++stLocker->MsgReceivedCount_;
      this->RxArgs_.SetOrigMsgStr(fixmsg);
      curst = stLocker->FixSt_;
   }
   if (fon9_LIKELY(curst == FixSessionSt::ApReady)) {
      // 為了加快 Ap 層的處以速度, ApReady 狀態下的訊息, 直接處理.
      this->DispatchFixMessage(this->RxArgs_);
      return;
   }
   switch (curst) {
   case FixSessionSt::Disconnected:
      return;
   case FixSessionSt::Connected: // this = Acceptor; fixmsg = Logon Request.
      assert(this->FixSender_.get() == nullptr);
      if (this->Check1stMustLogon())
         this->OnRecvLogonRequest(this->RxArgs_);
      return;
   case FixSessionSt::LogonSent: // this = Initiator; fixmsg = Logon Response.
      if (!this->Check1stMustLogon())
         return;
      break;
   case FixSessionSt::LogonRecovering:
   case FixSessionSt::LogonTest:
   case FixSessionSt::ApReady: // 已在此函式一開始處理過了.
   case FixSessionSt::LogoutPending:
      break;
   }
   this->DispatchFixMessage(this->RxArgs_);
}
bool FixSession::Check1stMustLogon() {
   const FixParser::FixField* fldMsgType = this->FixParser_.GetField(f9fix_kTAG_MsgType);
   if (fldMsgType == nullptr) {
      this->CloseFixSession("Unknown 1st msg.");
      return false;
   }
   static_assert(sizeof(f9fix_kMSGTYPE_Logon) - 1 == 1, "Logon.MsgType error?!");
   if (fon9_LIKELY(fldMsgType->Value_.size() == 1)) {
      const char msgType = *fldMsgType->Value_.begin();
      if (fon9_LIKELY(msgType == *f9fix_kMSGTYPE_Logon))
          return true;
      if (msgType == *f9fix_kMSGTYPE_Logout) {
         std::string errmsg{"1st msg is Logout:"};
         if (const FixParser::FixField* fld = this->FixParser_.GetField(f9fix_kTAG_Text))
            fld->Value_.AppendTo(errmsg);
         this->CloseFixSession(std::move(errmsg));
         return false;
      }
   }
   this->CloseFixSession("1st msg is not Logon.");
   return false;
}
FixParser::Result FixSession::OnFixMessageError(FixParser::Result res, StrView& fixmsgStream, const char* perr) {
   (void)fixmsgStream; (void)perr;
   RevBufferList rbuf{64};
   RevPrint(rbuf, "FixSession.OnFixMessageError|err=", res);
   this->CloseFixSession(BufferTo<std::string>(rbuf.MoveOut()));
   return res;
}
//--------------------------------------------------------------------------//
static void CopyFixField(RevBuffer& msgbuf, const StrView& fldTo, const FixParser::FixField* fldFrom) {
   if (fldFrom)
      RevPrint(msgbuf, fldTo, fldFrom->Value_);
}
void FixSession::SendLogout(FixBuilder&& fixb, FixRecorder* fixr) {
   StLocker stLocker = this->RunSt_.Lock();
   if (FixSender* fixout = this->FixSender_.get()) {
      if (stLocker->FixSt_ != FixSessionSt::LogoutPending) {
         stLocker->FixSt_ = FixSessionSt::LogoutPending;
         stLocker.unlock();
         this->FixSessionTimerRunAfter(this->RxArgs_.FixConfig_->TiLogoutPending_);
         fixout->Send(f9fix_SPLFLDMSGTYPE(Logout), std::move(fixb));
      }
      return;
   }
   stLocker->FixSt_ = FixSessionSt::Disconnected;
   stLocker.unlock();
   this->FixSessionTimerStopNoWait();
   RevBuffer&   msgbuf = fixb.GetBuffer();
   CopyFixField(msgbuf, f9fix_SPLTAGEQ(TargetCompID), this->FixParser_.GetField(f9fix_kTAG_SenderCompID));
   CopyFixField(msgbuf, f9fix_SPLTAGEQ(TargetSubID),  this->FixParser_.GetField(f9fix_kTAG_SenderSubID));
   CopyFixField(msgbuf, f9fix_SPLTAGEQ(SenderCompID), this->FixParser_.GetField(f9fix_kTAG_TargetCompID));
   CopyFixField(msgbuf, f9fix_SPLTAGEQ(SenderSubID),  this->FixParser_.GetField(f9fix_kTAG_TargetSubID));

   RevPut_TimeFIXMS(msgbuf, UtcNow());
   RevPrint(msgbuf, f9fix_SPLFLDMSGTYPE(Logout) f9fix_SPLTAGEQ(MsgSeqNum) "0" f9fix_SPLTAGEQ(SendingTime));

   BufferList  fixmsg{fixb.Final(ToStrView(this->FixParser_.GetExpectHeader()))};
   std::string cause = BufferTo<std::string>(fixmsg);
   this->OnFixSessionForceSend(std::move(fixmsg));
   if (fixr)
      fixr->Write(f9fix_kCSTR_HdrError, cause);
   StrView  sp{&cause};
   this->FixParser_.Parse(sp);
   if (const FixParser::FixField* fldText = this->FixParser_.GetField(f9fix_kTAG_Text))
      cause = fldText->Value_.ToString("ForceLogout:");
   this->CloseFixSession(std::move(cause));
}
//--------------------------------------------------------------------------//
void FixSession::SendLogon(FixSenderSP fixout, uint32_t heartBtInt, FixBuilder&& fixb) {
   {
      StLocker stLocker = this->RunSt_.Lock();
      assert(stLocker->FixSt_ < FixSessionSt::LogonSent);
      stLocker->FixSt_ = FixSessionSt::LogonSent;
      stLocker->HeartBtInt_ = heartBtInt;
   }
   if (this->ResetNextSendSeq_ > 0) {
      fixout->ResetNextSendSeq(this->ResetNextSendSeq_);
      this->ResetNextSendSeq_ = 0;
   }
   if (this->ResetNextRecvSeq_ > 0) {
      fixout->GetFixRecorder().ForceResetRecvSeq("Reset RECV seqNum on SendLogon()", this->ResetNextRecvSeq_);
      this->ResetNextRecvSeq_ = 0;
   }
   RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(HeartBtInt), heartBtInt);
   fixout->Send(f9fix_SPLFLDMSGTYPE(Logon), std::move(fixb));
   assert(this->FixSender_.get() == nullptr && (this->RxArgs_.FixSender_ == nullptr || this->RxArgs_.FixSender_ == fixout.get()));
   this->RxArgs_.FixSender_ = fixout.get();
   this->FixSender_ = std::move(fixout);
}
void FixSession::SendLogonResponse(FixSenderSP fixout, uint32_t heartBtInt, FixBuilder&& fixb, const FixRecvEvArgs& rxargs) {
   this->SendLogon(std::move(fixout), heartBtInt, std::move(fixb));
   this->OnLogonResponsed(rxargs, FixReceiver::GapSkipRecord);
}
void FixSession::OnLogonResponsed(const FixRecvEvArgs& rxargs, FixReceiver::GapFlags gapf) {
   StLocker stLocker = this->RunSt_.Lock();
   if (rxargs.SeqSt_ == FixSeqSt::Conform) {
      // - Logon Request/Response 訊息沒有 gap, 不用回補
      // - 接下來要等對方要求我方補齊(可能會接著收到對方的 ResendRequest)
      this->SendLogonTestRequest(std::move(stLocker));
   }
   else {
      stLocker->FixSt_ = FixSessionSt::LogonRecovering;
      stLocker->MsgReceivedCount_ = 0;
      stLocker.unlock();
      this->FixSessionTimerRunAfter(this->RxArgs_.FixConfig_->TiLogonRecover_);
      this->OnMsgSeqNumNotExpected(rxargs, gapf);
   }
}
void FixSession::SendLogonTestRequest(StLocker&& stLocker) {
   if (FixSessionSt::LogonSent <= stLocker->FixSt_ && stLocker->FixSt_ <= FixSessionSt::LogonTest) {
      stLocker->FixSt_ = FixSessionSt::LogonTest;
      stLocker.unlock();
      this->FixSessionTimerRunAfter(this->RxArgs_.FixConfig_->TiLogonTest_);
      FixBuilder fixb;
      RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(TestReqID) "LogonTest");
      this->FixSender_->Send(f9fix_SPLFLDMSGTYPE(TestRequest), std::move(fixb));
   }
}
void FixSession::SetApReadyStImpl(StLocker&& stLocker) {
   assert (stLocker->FixSt_ < FixSessionSt::ApReady);
   if (FixSender* fixout = this->FixSender_.get()) {
      stLocker->HbTestCount_ = 0;
      stLocker->FixSt_ = FixSessionSt::ApReady;
      this->FixSessionTimerRunAfter(TimeInterval_Second(stLocker->HeartBtInt_));
      stLocker.unlock();
      fixout->GetFixRecorder().Write(f9fix_kCSTR_HdrInfo, "ApReady");
      this->OnFixSessionApReady();
   }
   else {
      stLocker.unlock();
      this->CloseFixSession("SetApReadySt|err=No FixSender.");
   }
}
void FixSession::SendSessionReject(FixSeqNum refSeqNum, FixTag refTagID, const StrView& refMsgType, FixBuilder&& fixb) {
   if (FixSender* fixout = this->FixSender_.get())
      fix::SendSessionReject(*fixout, refSeqNum, refTagID, refMsgType, std::move(fixb));
}
//--------------------------------------------------------------------------//
// 來自 FixReceiver 的事件.
void FixSession::OnRecoverDone(const FixRecvEvArgs& rxargs) {
   baseFixReceiver::OnRecoverDone(rxargs);
   this->SendLogonTestRequest(this->RunSt_.Lock());
}
void FixSession::OnLogoutRequired(const FixRecvEvArgs&, FixBuilder&& fixb) {
   this->SendLogout(std::move(fixb));
}
//--------------------------------------------------------------------------//
void FixSession::FixSessionOnTimer() {
   const char* strClostReason = nullptr;
   {
      StLocker stLocker = this->RunSt_.Lock();
      switch (stLocker->FixSt_) {
      case FixSessionSt::Disconnected:
         break;
      case FixSessionSt::Connected:
      case FixSessionSt::LogonSent:
         strClostReason = "Logon timeout.";
         break;
      case FixSessionSt::LogonRecovering:
         if (stLocker->MsgReceivedCount_ == 0)
            strClostReason = "LogonRecover timeout.";
         else {
            stLocker->MsgReceivedCount_ = 0;
            this->FixSessionTimerRunAfter(this->RxArgs_.FixConfig_->TiLogonRecover_);
         }
         break;
      case FixSessionSt::LogonTest:
         if (stLocker->MsgReceivedCount_ == 0) {
            ++stLocker->HbTestCount_;
            if (stLocker->HbTestCount_ >= this->RxArgs_.FixConfig_->MaxLogonTestCount_) {
               strClostReason = "LogonTest|err=Remote no response";
               break;
            }
         }
         else {
            stLocker->HbTestCount_ = 0;
            stLocker->MsgReceivedCount_ = 0;
         }
         this->SendLogonTestRequest(std::move(stLocker));
         return;
      case FixSessionSt::LogoutPending:
         strClostReason = "Logout timeout.";
         break;
      case FixSessionSt::ApReady:
         TimeInterval hbTi = UtcNow() - this->FixSender_->GetLastSentTime();
         hbTi = TimeInterval_Second(stLocker->HeartBtInt_) - hbTi;
         if (hbTi > TimeInterval_Second(1)) { // 距離上次送出資料, 還需要再等一會兒, 才到達需要送 Hb 的時間.
            this->FixSessionTimerRunAfter(hbTi);
            break;
         }
         this->FixSessionTimerRunAfter(TimeInterval_Second(stLocker->HeartBtInt_));
         if (stLocker->MsgReceivedCount_ == 0) {
            ++stLocker->HbTestCount_;
            const uint32_t hbTestRequestCount = this->RxArgs_.FixConfig_->HbTestRequestCount_;
            if (stLocker->HbTestCount_ >= hbTestRequestCount + 1) {
               strClostReason = "Remote no response";
               break;
            }
            if (stLocker->HbTestCount_ >= hbTestRequestCount) {
               stLocker.unlock();
               FixBuilder fixb;
               RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(TestReqID) "AliveTest");
               this->FixSender_->Send(f9fix_SPLFLDMSGTYPE(TestRequest), std::move(fixb));
               return;
            }
         }
         else {
            stLocker->HbTestCount_ = 0;
            stLocker->MsgReceivedCount_ = 0;
         }
         stLocker.unlock();
         SendHeartbeat(*this->FixSender_, nullptr);
         return;
      }
   } // auto stLocker unlock();
   if (strClostReason) {
      this->CloseFixSession(strClostReason);
   }
}
//--------------------------------------------------------------------------//
std::string FixSession::ResetNextSendSeq(StrView strNum) {
   FixSeqNum nextSendSeq = StrTo(&StrTrim(&strNum), FixSeqNum{0});
   if (!strNum.empty())
      return "Invalid next SEND seqNum string.";
   std::string res = RevPrintTo<std::string>("New next SEND seqNum = ", nextSendSeq, '\n');
   if (nextSendSeq <= 0) {
      this->ResetNextSendSeq_ = 0;
      return res + "Cleared\n";
   }
   if (this->RunSt_.Lock()->FixSt_ == FixSessionSt::ApReady) {
      this->ResetNextSendSeq_ = 0;
      assert(this->FixSender_.get() != nullptr);
      this->FixSender_->SequenceReset(nextSendSeq);
      res.append("SequenceReset sent\n");
   }
   else {
      this->ResetNextSendSeq_ = nextSendSeq;
      res.append("Use it when next Logon.\n");
   }
   return res;
}
std::string FixSession::ResetNextRecvSeq(StrView strNum) {
   FixSeqNum nextRecvSeq = StrTo(&StrTrim(&strNum), FixSeqNum{0});
   if (!strNum.empty())
      return "Invalid next RECV seqNum string.";
   std::string res = RevPrintTo<std::string>("New next RECV seqNum = ", nextRecvSeq, '\n');
   if (nextRecvSeq <= 0) {
      this->ResetNextRecvSeq_ = 0;
      return res + "Cleared\n";
   }
   if (FixSender* fixout = this->FixSender_.get()) {
      this->ResetNextRecvSeq_ = 0;
      fixout->GetFixRecorder().ForceResetRecvSeq("FixSession.ResetNextRecvSeq", nextRecvSeq);
      res.append("Resetted\n");
   }
   else {
      this->ResetNextRecvSeq_ = nextRecvSeq;
      res.append("Use it when next Logon.\n");
   }
   return res;
}
//--------------------------------------------------------------------------//
void FixSession::InitFixConfig(FixConfig& fixcfg) {
   FixReceiver::InitFixConfig(fixcfg);
   fixcfg.Fetch(f9fix_kMSGTYPE_Heartbeat).FixMsgHandler_ = &FixSession::OnRecvHeartbeat;
   fixcfg.Fetch(f9fix_kMSGTYPE_TestRequest).FixMsgHandler_ = &FixSession::OnRecvTestRequest;

   auto mcfg = &fixcfg.Fetch(f9fix_kMSGTYPE_Logout);
   mcfg->FixSeqAllow_ = FixSeqSt::AllowAnySeq;
   mcfg->FixMsgHandler_ = &FixSession::OnRecvLogout;

   // f9fix_kMSGTYPE_Logon:
   // 這裡只有處理 Logon Response, 並進行回補.
   // Logon Request 在 FixSession::OnFixMessageParsed() 裡面處理.
   mcfg = &fixcfg.Fetch(f9fix_kMSGTYPE_Logon);
   mcfg->FixSeqAllow_ = FixSeqSt::TooHigh;
   mcfg->FixMsgHandler_ = &FixSession::OnRecvLogonResponse;
}
void FixSession::OnRecvHeartbeat(const FixRecvEvArgs& rxargs) {
   rxargs.FixSession_->SetApReadySt();
}
void FixSession::OnRecvTestRequest(const FixRecvEvArgs& rxargs) {
   SendHeartbeat(*rxargs.FixSender_, rxargs.Msg_.GetField(f9fix_kTAG_TestReqID));
   rxargs.FixSession_->SetApReadySt();
}
void FixSession::OnRecvLogonResponse(const FixRecvEvArgs& rxargs) {
   if (rxargs.FixSession_->RunSt_.Lock()->FixSt_ == FixSessionSt::LogonSent)
      rxargs.FixSession_->OnLogonResponsed(rxargs, FixReceiver::GapDontKeep);
   else
      SendInvalidMsgType(*rxargs.FixSender_, rxargs.Msg_.GetMsgSeqNum(), f9fix_kMSGTYPE_Logon);
}
void FixSession::OnRecvLogout(const FixRecvEvArgs& rxargs) {
   FixSession* fixses = rxargs.FixSession_;
   if (rxargs.SeqSt_ == FixSeqSt::TooLow) {
      fixses->OnMsgSeqNumNotExpected(rxargs, FixReceiver::GapDontKeep);
      return;
   }
   if (rxargs.SeqSt_ != FixSeqSt::Conform)
      rxargs.FixSender_->GetFixRecorder().Write(f9fix_kCSTR_HdrIgnoreRecv, rxargs.OrigMsgStr());
   if (fixses->RunSt_.Lock()->FixSt_ == FixSessionSt::ApReady)
      fixses->SendLogout(StrView{"Logout response"});
   if (rxargs.SeqSt_ == FixSeqSt::TooHigh)
      fixses->OnMsgSeqNumNotExpected(rxargs, FixReceiver::GapDontKeep);
}

} } // namespaces
