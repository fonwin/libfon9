// \file f9twf/ExgMrRecover.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMrRecover.hpp"
#include "f9twf/ExgMcGroup.hpp"
#include "fon9/buffer/FwdBufferList.hpp"

namespace f9twf {
using namespace fon9;

ExgMrRecoverSession::~ExgMrRecoverSession() {
}
void ExgMrRecoverSession::OnDevice_Initialized(io::Device& dev) {
   this->Device_ = &dev;
}
io::RecvBufferSize ExgMrRecoverSession::OnDevice_LinkReady(io::Device& dev) {
   this->Recovering_ = 0;
   this->ApReadyMsg_.clear();
   FwdBufferList     buf{0};
   ExgMrLoginReq020* req = reinterpret_cast<ExgMrLoginReq020*>(buf.AllocBuffer(sizeof(ExgMrLoginReq020)));
   static uint16_t   gMx = 123;
   uint16_t          mx = ++gMx;
   if (mx <= 1)
      mx = static_cast<uint16_t>(mx + 465);
   TmpPutValue(req->MultiplicationOperator_, static_cast<uint16_t>(mx));
   req->CheckCode_ = static_cast<uint8_t>(((static_cast<uint32_t>(mx) * this->Pass_) / 100) % 100);
   TmpPutValue(req->SessionId_, this->SessionId_);
   req->Final(this->MsgSeqNum_ = 1);
   dev.SendASAP(buf.MoveOut());
   return io::RecvBufferSize::Default;
}
void ExgMrRecoverSession::OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs&) {
   if (this->State_ >= ExgMrState::ApReady) {
      this->State_ = ExgMrState::LinkBroken;
      dev.Manager_->OnSession_StateUpdated(dev, "ApBroken", LogLevel::Info);
      for (unsigned L = 0; L < ExgMcChannelMgr::kChannelCount; ++L) {
         if (auto* channel = this->ChannelMgr_->GetChannel(static_cast<ExgMrChannelId_t>(L)))
            channel->DelRecover(this);
      }
   }
}
io::RecvBufferSize ExgMrRecoverSession::OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) {
__FEED_BUFFER:;
   if (this->Recovering_ > 0) {
      if (this->FeedBuffer(rxbuf)) {
         assert(this->Recovering_ > 0 && this->State_ == ExgMrState::Requesting);
         return io::RecvBufferSize::Default;
      }
   }
   char  pkbuf[1024 * 64 + sizeof(ExgMrNoBody)]; // MsgSize_ == uint16_t, 最大 64K;
   while (auto* pkptr = static_cast<const ExgMrMsgHeader*>(rxbuf.Peek(pkbuf, sizeof(ExgMrMsgHeader)))) {
      unsigned pksz = static_cast<const ExgMrMsgHeader*>(pkptr)->GetPacketSize();
      if ((pkptr = static_cast<const ExgMrMsgHeader*>(rxbuf.Peek(pkbuf, pksz))) == nullptr)
         break;
      if (GetExgMrMsgFooter(*pkptr, pksz).CheckSum_ != ExgMrCalcCheckSum(*pkptr, pksz)) {
         dev.AsyncClose("ExgMrRecoverSession.OnDevice_Recv|err=CheckSum");
         return io::RecvBufferSize::CloseRecv;
      }
      switch (TmpGetValueU(pkptr->MsgType_)) {
      case 10: // 錯誤通知訊息(010).
         dev.AsyncClose(fon9::RevPrintTo<std::string>(
            "ExgMrRecoverSession|Rx010St=", static_cast<const ExgMrError010*>(pkptr)->StatusCode_));
         dev.AsyncOpen(std::string{});
         return io::RecvBufferSize::CloseRecv;
      case 30: // 期交所用此訊息回覆資訊接收端登錄結果，如有多個 ChannelID，則 030 會有多筆。
         if (this->State_ < ExgMrState::ApReady)
            this->State_ = ExgMrState::ApReady;
         if (auto* channel = this->ChannelMgr_->GetChannel(TmpGetValueU(static_cast<const ExgMrLoginCfm030*>(pkptr)->ChannelId_))) {
            fon9::NumOutBuf nbuf;
            char*           pout = fon9::ToStrRev(nbuf.end(), channel->GetChannelId());
            if (this->ApReadyMsg_.empty())
               this->ApReadyMsg_.assign(this->Role_ == ExgMcRecoverRole::Primary
                                        ? fon9::StrView{"ApReady.P:"} : fon9::StrView{"ApReady.S:"});
            else
               *--pout = ',';
            ApReadyMsg_.append(pout, nbuf.end());
            channel->AddRecover(this);
         }
         break;
      case 50: // 回補服務開始(050)
         dev.Manager_->OnSession_StateUpdated(dev, ToStrView(this->ApReadyMsg_), LogLevel::Info);
         break;
      case 104: // 確認連線訊息(104)-Heartbeat;
         static_assert(sizeof(ExgMrHeartbeat104) == sizeof(ExgMrHeartbeat105), "");
         static_cast<ExgMrHeartbeat105*>(const_cast<ExgMrMsgHeader*>(pkptr))->Final(105, ++this->MsgSeqNum_);
         dev.SendASAP(pkptr, pksz);
         break;
      case 102: // 請求行情資料回補回覆訊息(102).
         auto pkrec = static_cast<const ExgMrRecoverSt102*>(pkptr);
         if (pkrec->StatusCode_ == 0) {
            this->Recovering_ = TmpGetValueU(pkrec->RecoverNum_);
            rxbuf.PopConsumed(pksz);
            // 回補的訊息可能會緊接在 102 之後, 所以需要立即處理.
            goto __FEED_BUFFER;
         }
         auto channelId = TmpGetValueU(pkrec->ChannelId_);
         auto beginSeqNo = TmpGetValueU(pkrec->BeginSeqNo_);
         auto recoverNum = TmpGetValueU(pkrec->RecoverNum_);
         fon9_LOG_WARN("ExgMrRecoverSession.RecoverErr|dev=", ToPtr(&dev),
                        "|channelId=", channelId, "|st=", pkrec->StatusCode_,
                        "|beginSeqNo=", beginSeqNo, "|recoverNum=", recoverNum);
         this->State_ = ExgMrState::ApReady;
         if (auto* channel = this->ChannelMgr_->GetChannel(channelId))
            channel->OnRecoverErr(beginSeqNo, recoverNum, pkrec->StatusCode_);
         break;
      }
      rxbuf.PopConsumed(pksz);
   }
   return io::RecvBufferSize::Default;
}
bool ExgMrRecoverSession::OnPkReceived(const void* pk, unsigned pksz) {
   assert(this->Recovering_ > 0 && this->State_ == ExgMrState::Requesting);
   auto* channel = this->ChannelMgr_->GetChannel(static_cast<const ExgMcHead*>(pk)->GetChannelId());
   bool  isRecovering = (--this->Recovering_ > 0);
   if (!isRecovering)
      this->State_ = ExgMrState::ApReady;
   if (channel) {
      channel->OnPkReceived(*static_cast<const ExgMcHead*>(pk), pksz);
      if (!isRecovering)
         channel->OnRecoverEnd(this);
   }
   return isRecovering; // 還沒補完, OnDevice_Recv() 繼續呼叫 this->FeedBuffer();
}
ExgMrState ExgMrRecoverSession::RequestMcRecover(ExgMrChannelId_t channelId, ExgMrMsgSeqNum_t from, ExgMrRecoverNum_t count) {
   if (this->State_ < ExgMrState::ApReady)
      return this->State_;
   if (this->State_ == ExgMrState::Requesting)
      return ExgMrState::ApReady;
   this->State_ = ExgMrState::Requesting;
   ++this->RequestCount_;
   FwdBufferList        buf{0};
   ExgMrRecoverReq101*  req = reinterpret_cast<ExgMrRecoverReq101*>(buf.AllocBuffer(sizeof(ExgMrRecoverReq101)));
   TmpPutValue(req->ChannelId_, channelId);
   TmpPutValue(req->BeginSeqNo_, from);
   TmpPutValue(req->RecoverNum_, count);
   req->Final(++this->MsgSeqNum_);
   this->Device_->SendASAP(buf.MoveOut());
   return this->State_;
}
//--------------------------------------------------------------------------//
io::SessionSP ExgMrRecoverFactory::CreateSession(IoManager& ioMgr, const IoConfigItem& cfg, std::string& errReason) {
   if (auto mgr = dynamic_cast<ExgMcGroupIoMgr*>(&ioMgr)) {
      StrView  tag, value, args = ToStrView(cfg.SessionArgs_);
      ExgMcRecoverRole role{};
      ExgMrSessionId_t sessionId{};
      uint16_t         pass{};
      while (StrFetchTagValue(args, tag, value)) {
         if (tag == "Role") { // Role=P or Role=S
            switch (value.Get1st()) {
            case 'P':   role = ExgMcRecoverRole::Primary;   break;
            case 'S':   role = ExgMcRecoverRole::Secondary; break;
            default:
               errReason = "f9twf.ExgMrRecoverFactory.CreateSession: Unknown Role:";
               value.AppendTo(errReason);
               return nullptr;
            }
         }
         else if (tag == "SessionId")
            sessionId = StrTo(value, sessionId);
         else if (tag == "Pass")
            pass = StrTo(value, pass);
         else {
            errReason = "f9twf.ExgMrRecoverFactory.CreateSession: Unknown tag:";
            tag.AppendTo(errReason);
            return nullptr;
         }
      }
      return new ExgMrRecoverSession(mgr->McGroup_->ChannelMgr_, role, sessionId, pass);
   }
   errReason = "f9twf.ExgMrRecoverFactory.CreateSession: Unknown IoMgr.";
   return nullptr;
}
io::SessionServerSP ExgMrRecoverFactory::CreateSessionServer(IoManager& ioMgr, const IoConfigItem& cfg, std::string& errReason) {
   (void)ioMgr; (void)cfg;
   errReason = "f9twf.ExgMrRecoverFactory: Not support server.";
   return nullptr;
}

} // namespaces
