// \file f9tws/ExgLineTmp.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgLineTmp.hpp"
#include "f9tws/ExgTradingLineMgr.hpp"
#include "fon9/Endian.hpp"

namespace f9tws {

fon9::ConfigParser::Result ExgLineTmpArgs::OnTagValue(fon9::StrView tag, fon9::StrView& value) {
   if (tag == "ApCode") {
      if (value.size() != 1)
         return fon9::ConfigParser::Result::EInvalidValue;
      this->ApCode_ = static_cast<TwsApCode>(*value.begin());
      return fon9::ConfigParser::Result::Success;
   }
   if (tag == "Log") {
      if (value.size() != 1)
         return fon9::ConfigParser::Result::EInvalidValue;
      this->IsNeedsLog_ = (fon9::toupper(value.Get1st()) == 'Y');
      return fon9::ConfigParser::Result::Success;
   }
   return base::OnTagValue(tag, value);
}

f9tws_API std::string ExgLineTmpArgsParser(ExgLineTmpArgs& args, fon9::StrView cfg) {
   return fon9::ParseFullConfig(args, cfg);
}

//--------------------------------------------------------------------------//
using ChAry2 = fon9::CharAryF<2>;
using ChAry6 = fon9::CharAryF<6>;

static ChAry6 GetApCfmLink(const ExgLineTmpArgs& lineargs) {
   unsigned char chApCode = static_cast<unsigned char>(lineargs.ApCode_);
   if (lineargs.ApCode_ == f9tws::TwsApCode::MatchA)
      chApCode = static_cast<unsigned char>(f9tws::TwsApCode::Match3);
   static const char* subSysNames;
   if (lineargs.Market_ == f9fmkt_TradingMarket_TwSEC) {
      subSysNames =
         "300002" // 0:Regular
         "20----" // 1:FileTx
         "400004" // 2:OddLot
         "50----" // 3:Match3 => 由交易所送出("500005"), 券商端回覆.
         "310002" // 4:LeadingAuct 標借
         "700002" // 5:Auction 拍賣
         "410002" // 6:TenderOfferTWSE 標購
         "320002" // 7:FixedPrice
         "------" // 8:
         "------";// 9:
   }
   else if (lineargs.Market_ == f9fmkt_TradingMarket_TwOTC) {
      // AP-CODE = B: 證券商的證金標購委託子系統. "890002"
      subSysNames =
         "930002" // 0:Regular
         "92----" // 1:FileTx
         "940004" // 2:OddLot
         "95----" // 3:Match3 => 由交易所送出("950005"), 券商端回覆.
         "900002" // 4:LeadingAuct 標借
         "------" // 5:Auction 拍賣: 20190906:OTC無此作業?
         "960013" // 6:NegotiationOTC
         "980002" // 7:FixedPrice
         "970002" // 8:TenderOfferOTC 標購
         "------";// 9:
   }
   else {
      return ChAry6{"------"};
   }
   if (fon9::isdigit(chApCode))
      return ChAry6{subSysNames + (chApCode - '0') * sizeof(ChAry6), sizeof(ChAry6)};
   return ChAry6{};
}

ExgTmpIoSession::ExgTmpIoSession(ExgTradingLineMgr& lineMgr, const ExgLineTmpArgs& lineargs, std::string logFileName)
   : Log_{logFileName.empty() ? nullptr : fon9::LogFileAppender::Make()}
   , LineMgr_(lineMgr)
   , LineArgs_(lineargs)
   , LinkSubName_{lineargs.Market_ == f9fmkt_TradingMarket_TwOTC ? ChAry2{"91"} : ChAry2{"10"}}
   , ApCfmLink_{GetApCfmLink(lineargs)} {
   if (this->Log_ && fon9::isalnum(fon9::unsigned_cast(*this->ApCfmLink_.begin()))) {
      auto res = this->Log_->OpenImmediately(logFileName, fon9::FileMode::Append | fon9::FileMode::CreatePath);
      if (res.IsError()) {
         fon9_LOG_WARN("ExgTmpIoSession.OpenLog|fname=", logFileName, '|', res);
         this->Log_.reset();
      }
   }
}

//--------------------------------------------------------------------------//
void ExgTmpIoSession::CheckApBroken(SlmTmpSt st) {
   if (this->St_ == SlmTmpSt::ApReady) {
      this->St_ = st;
      this->OnExgTmp_ApBroken();
   }
}
void ExgTmpIoSession::AsyncClose(std::string cause) {
   this->CheckApBroken(SlmTmpSt::DevClose);
   this->Dev_->AsyncLingerClose(std::move(cause));
}

void ExgTmpIoSession::WriteLog(fon9::StrView hdrinfo, fon9::StrView data) {
   if (fon9_UNLIKELY(this->Log_)) {
      fon9::RevBufferList rbuf{128};
      fon9::RevPrint(rbuf, hdrinfo, data, '\n');
      fon9::RevPut_Date_Time_us(rbuf, fon9::LocalNow());
      this->Log_->Append(rbuf.MoveOut());
   }
}
void ExgTmpIoSession::WriteLogLinkSt(const fon9::io::StateUpdatedArgs& e) {
   if (fon9_UNLIKELY(this->Log_)) {
      fon9::RevBufferList rbuf{128};
      fon9::RevPrint(rbuf, "|LinkSt|", fon9::io::GetStateStr(e.State_), '|', e.Info_, '\n');
      fon9::RevPut_Date_Time_us(rbuf, fon9::LocalNow());
      this->Log_->Append(rbuf.MoveOut());
   }
}
void ExgTmpIoSession::OnDevice_Initialized(fon9::io::Device& dev) {
   this->Dev_ = &dev;
}
void ExgTmpIoSession::OnDevice_StateChanged(fon9::io::Device& dev, const fon9::io::StateChangedArgs& e) {
   (void)dev;
   this->WriteLogLinkSt(e.After_);
   if (e.BeforeState_ == fon9::io::State::LinkReady)
      this->CheckApBroken(SlmTmpSt::ApBroken);
}
void ExgTmpIoSession::OnDevice_StateUpdated(fon9::io::Device& dev, const fon9::io::StateUpdatedArgs& e) {
   (void)dev;
   this->WriteLogLinkSt(e);
}
fon9::io::RecvBufferSize ExgTmpIoSession::OnDevice_LinkReady(fon9::io::Device& dev) {
   // 等候交易所: 成功 SLM-010; 或失敗 SLM-040;
   this->LastRxTime_.Assign0();
   dev.CommonTimerRunAfter(GetHbIntervalSLM());
   this->WriteLog(fon9::ToStrView(dev.WaitGetDeviceInfo()), nullptr);
   return fon9::io::RecvBufferSize::Default;
}

//--------------------------------------------------------------------------//
#define f9tws_kCSTR_SLM_LEAD  "\xfe\xfe"
#define kSLM_HDR_SIZE         6u
#define kSLM_SIZE             (kSLM_HDR_SIZE + f9tws_kSIZE_SLM_TAIL)
struct ExgSlmHead {
   fon9::CharAry<2>  Lead_;   // f9tws_kCSTR_SLM_LEAD
   fon9::CharAry<2>  Code_;   // "00"=TMP; "11"=SLM-030; "10"=SLM-010;
   fon9::CharAry<2>  BodyLenBigEndian_;
   bool IsLeadOK() const {
      return(this->Lead_.Chars_[0] == f9tws_kCSTR_SLM_LEAD[0]
          && this->Lead_.Chars_[1] == f9tws_kCSTR_SLM_LEAD[1]);
   }
   uint16_t GetBodyLen() const {
      return fon9::GetBigEndian<uint16_t>(this->BodyLenBigEndian_.Chars_);
   }
   bool IsCode(char ch0, char ch1) const {
      return(this->Code_.Chars_[0] == ch0 && this->Code_.Chars_[1] == ch1);
   }
   void SetLeadCode(const char (&lcode)[5]) {
      assert(lcode[sizeof(lcode) - 1] == '\0');
      static_assert(sizeof(lcode) - 1 == sizeof(this->Lead_) + sizeof(this->Code_), "lcode size error.");
      memcpy(&this->Lead_, lcode, sizeof(lcode) - 1);
   }
   void SetBodyLen(size_t len) {
      uint16_t u16val = static_cast<uint16_t>(len);
      assert(u16val == len);
      fon9::PutBigEndian<uint16_t>(this->BodyLenBigEndian_.Chars_, u16val);
   }
};
static_assert(sizeof(ExgSlmHead) == kSLM_HDR_SIZE, "struct ExgSlmHead; must pack?");

fon9::io::RecvBufferSize ExgTmpIoSession::OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueueList& rxbuf) {
   (void)dev;
   char  bufpk[64 * 1024 + kSLM_SIZE];
   while (auto* pSlm = reinterpret_cast<const ExgSlmHead*>(rxbuf.Peek(bufpk, kSLM_HDR_SIZE))) {
      if (fon9_LIKELY(pSlm->IsLeadOK())) {
         const auto     apMsgLen = pSlm->GetBodyLen();
         const unsigned pkLen = apMsgLen + kSLM_SIZE;
         pSlm = reinterpret_cast<const ExgSlmHead*>(rxbuf.Peek(bufpk, pkLen));
         if (pSlm == nullptr) // rxbuf 資料不足.
            return fon9::io::RecvBufferSize::Default;
         this->LastRxTime_ = fon9::UtcNow();
         if (fon9_LIKELY(pSlm->IsCode('0', '0'))) {
            // SLM-020: TMP Socket 訊息.
            if (fon9_UNLIKELY(this->Log_))
               this->WriteLog("|Rx|TMP|", fon9::StrView{reinterpret_cast<const char*>(pSlm), pkLen});
            if (fon9_LIKELY(apMsgLen >= f9tws_kSIZE_TMP_HEAD_A))
               this->OnExgTmp_RecvTmp(reinterpret_cast<const char*>(pSlm) + kSLM_HDR_SIZE, apMsgLen);
            else {
               this->AsyncClose("SLM-ERR: TMP size too small.");
               return fon9::io::RecvBufferSize::NoRecvEvent;
            }
         }
         else {
            if (fon9_UNLIKELY(this->Log_))
               this->WriteLog("|Rx|SLM|", fon9::StrView{reinterpret_cast<const char*>(pSlm), pkLen});
            if (fon9_LIKELY(pSlm->IsCode('1', '1'))) {
               // SLM-030: 確定 Socket Session 訊息.
               // do nothing, 僅接設定 this->LastRxTime_;
               // 若太久沒收到任何訊息, 則應斷線.
            }
            else if (fon9_LIKELY(pSlm->IsCode('1', '0'))) {
               // SLM-010: 建立 Socket 完成通知訊息.
               this->OnExgTmp_SlmReady();
            }
            else { // SLM-040: 錯誤訊息: Code = "21", "22"...
               this->AsyncClose(fon9::RevPrintTo<std::string>("SLM-ERR: ", pSlm->Code_));
               return fon9::io::RecvBufferSize::NoRecvEvent;
            }
         }
         rxbuf.PopConsumed(pkLen);
      }
      else { // SLM frame error.
         this->WriteLog("|Rx|SLM-ERR|Frame|", fon9::StrView{reinterpret_cast<const char*>(pSlm), kSLM_HDR_SIZE});
         this->AsyncClose("SLM-ERR: RxFrameErr.");
         return fon9::io::RecvBufferSize::NoRecvEvent;
      }
   }
   return fon9::io::RecvBufferSize::Default;
}

//--------------------------------------------------------------------------//
static const fon9::TimeInterval kHbIntervalErr = fon9::TimeInterval_Second(1);
static const fon9::TimeInterval kApHbInterval = fon9::TimeInterval_Second(60);

void ExgTmpIoSession::OnDevice_CommonTimer(fon9::io::Device& dev, fon9::TimeStamp now) {
   if (now - this->LastRxTime_ > GetHbIntervalSLM() * 2) {
      this->WriteLog("|SLM-Timeout|", nullptr);
      // 應該每 25 秒, 至少會收到 SLM-030; 超過 n 秒沒收到任何訊息, 應視為異常.
      this->AsyncClose("SLM-ERR: RxTimeout.");
      return;
   }

   auto tiNext = GetHbIntervalSLM() - (now - this->LastTxTime_);
   if (tiNext <= kHbIntervalErr)   // 預計現在要送出 SLM-030;
      tiNext = GetHbIntervalSLM(); // 所以下次送出時間是 SLM-HB 間隔.

   this->OnExgTmp_CheckApTimer(now, tiNext);

   tiNext = GetHbIntervalSLM() - (now - this->LastTxTime_);
   if (tiNext > kHbIntervalErr)
      dev.CommonTimerRunAfter(tiNext);
   else { // 最後送出時間 >= GetHbIntervalSLM() - 1秒; 就送個 SML-030: Hb 吧.
      static const char kSLM030[] = f9tws_kCSTR_SLM_LEAD "11" "\0\0" f9tws_kCSTR_SLM_TAIL;
      this->Dev_->Send(kSLM030, sizeof(kSLM030) - 1); // SLM-030: Hb;
      this->LastTxTime_ = now;
      dev.CommonTimerRunAfter(GetHbIntervalSLM());
      this->WriteLog("|Tx|SLM-030|", kSLM030);
   }
}
void ExgTmpIoSession::OnExgTmp_CheckApTimer(fon9::TimeStamp now, fon9::TimeInterval tiNext) {
   if (reinterpret_cast<const ExgTmpHead*>(&this->ApCfmLink_)->FuncCode_.Chars_[0] == '-')
      return;
   if ((now - this->TmpTxTime_) + tiNext <= kApHbInterval)
      return;
   ExgTmpIoRevBuffer rbuf{64};
   char* pout = rbuf.AllocPrefix(f9tws_kSIZE_TMP_HEAD_A) - f9tws_kSIZE_TMP_HEAD_A;
   rbuf.SetPrefixUsed(pout);
   memcpy(pout, this->ApCfmLink_.Chars_, sizeof(this->ApCfmLink_));
   reinterpret_cast<ExgTmpHead*>(pout)->SetUtcTime(now);
   reinterpret_cast<ExgTmpHead*>(pout)->SetStCode("00");
   this->SendTmp(now, std::move(rbuf));
}

//--------------------------------------------------------------------------//
static const char kCSTR_L010[] = "1000"; // 開機通知訊息.
static const char kCSTR_L020[] = "1001"; // 確認開機通知訊息.
static const char kCSTR_L030[] = "2002"; // 通知登錄訊息.
static const char kCSTR_L040[] = "2003"; // 登錄訊息.
static const char kCSTR_L050[] = "2004"; // 應用子系統啟動訊息.
static const char kCSTR_L060[] = "2005"; // 確認應用子系統啟動訊息.
static const char kCSTR_L070[] = "3006"; // 結束訊息.
static const char kCSTR_L080[] = "3007"; // 確認結束訊息.

void ExgTmpIoSession::OnExgTmp_SlmReady() {
   // 券商送出 L010, 等交易所回覆 L010.
   ExgTmpIoRevBuffer rbuf{64};
   char* pout = rbuf.AllocPrefix(f9tws_kSIZE_TMP_HEAD_A) - f9tws_kSIZE_TMP_HEAD_A;
   rbuf.SetPrefixUsed(pout);
   reinterpret_cast<ExgTmpHead*>(pout)->SubsysName_ = this->LinkSubName_;
   reinterpret_cast<ExgTmpHead*>(pout)->SetFuncMsg(kCSTR_L010);
   reinterpret_cast<ExgTmpHead*>(pout)->SetUtcTime(this->LastRxTime_);
   reinterpret_cast<ExgTmpHead*>(pout)->SetStCode("00");
   this->SendTmp(this->LastRxTime_, std::move(rbuf));
}
void ExgTmpIoSession::OnExgTmp_RecvTmp(const char* pkptr, unsigned pksz) {
   const ExgTmpHead& pktmp = *reinterpret_cast<const ExgTmpHead*>(pkptr);
   if (fon9_LIKELY(pktmp.SubsysName_ == reinterpret_cast<const ExgTmpHead*>(&this->ApCfmLink_)->SubsysName_)) {
      this->OnExgTmp_ApPacket(pktmp, pksz);
      return;
   }
   if (fon9_LIKELY(pktmp.SubsysName_ == this->LinkSubName_)) {
      if (pktmp.StCode_.Chars_[0] != '0' || pktmp.StCode_.Chars_[1] != '0') {
         // LinkSystem 不正確, 先關閉, 等一會兒再連線.
         // device 的設定必須增加: "|ClosedReopen=5" 表示關閉後 5 秒重連.
         this->AsyncClose(
            fon9::RevPrintTo<std::string>("Rx.LinkSystem.Err"
                                          "|fnc=", pktmp.FuncCode_,
                                          "|msg=", pktmp.MsgType_,
                                          "|st=", pktmp.StCode_));
         return;
      }
      this->CheckApBroken(SlmTmpSt::TmpRelink);
      pkptr += this->LinkSubName_.size();
      ExgTmpIoRevBuffer rbuf{64};
      ExgTmpFuncMsgT    cstrFuncMsg;
      char*             pout;
      bool              isApReady = false;
      if (memcmp(pkptr, kCSTR_L010, f9tws_kSIZE_TMP_HEAD_FUNCMSG) == 0) {
         cstrFuncMsg = &kCSTR_L020;
         pkptr = reinterpret_cast<const ExgTmpHead*>(pkptr - this->LinkSubName_.size())->StCode_.Chars_;
      }
      else if (memcmp(pkptr, kCSTR_L030, f9tws_kSIZE_TMP_HEAD_FUNCMSG) == 0) {
         // 取得 APPEND-NO;
         #define kSIZE_APPEND_NO    3
         if (fon9_UNLIKELY(pksz != f9tws_kSIZE_TMP_HEAD_A + kSIZE_APPEND_NO)) {
            this->WriteLog("|TMP-L030|bad pksz|", nullptr);
            return;
         }
         pkptr += f9tws_kSIZE_TMP_HEAD_A - this->LinkSubName_.size();
         unsigned appn = fon9::Pic9StrTo<kSIZE_APPEND_NO, unsigned>(pkptr);
         // 計算 KEY-VALUE = (APPEND-NO * PASSWORD)取千與百二位數字。
         appn = ((appn * this->LineArgs_.PassCode_) / 100) % 100;
         // APPEND-NO   9(3)
         // BROKER-ID   X(4)
         // AP-CODE     X(1)
         // KEY-VALUE   9(2)
         pout = rbuf.AllocPrefix(10) - 10;
         rbuf.SetPrefixUsed(pout);
         memcpy(pout, pkptr, kSIZE_APPEND_NO);
         memcpy(pout += kSIZE_APPEND_NO, this->LineArgs_.BrkId_.Chars_, this->LineArgs_.BrkId_.size());
         *(pout += this->LineArgs_.BrkId_.size()) = static_cast<char>(this->LineArgs_.ApCode_);
         fon9::Pic9ToStrRev<2>(pout + 3, appn);
         cstrFuncMsg = &kCSTR_L040;
      }
      else if (memcmp(pkptr, kCSTR_L050, f9tws_kSIZE_TMP_HEAD_FUNCMSG) == 0) {
         isApReady = true;
         cstrFuncMsg = &kCSTR_L060;
      }
      else {
         this->WriteLog("|TMP|Unknown LinkSystem function|", nullptr);
         return;
      }
      pout = rbuf.AllocPrefix(f9tws_kSIZE_TMP_HEAD_A) - f9tws_kSIZE_TMP_HEAD_A;
      rbuf.SetPrefixUsed(pout);
      reinterpret_cast<ExgTmpHead*>(pout)->SubsysName_ = this->LinkSubName_;
      reinterpret_cast<ExgTmpHead*>(pout)->SetFuncMsg(*cstrFuncMsg);
      reinterpret_cast<ExgTmpHead*>(pout)->SetUtcTime(this->LastRxTime_);
      reinterpret_cast<ExgTmpHead*>(pout)->SetStCode("00");
      this->SendTmp(this->LastRxTime_, std::move(rbuf));
      if (isApReady) {
         // 必須先送出回覆, 然後才能觸發 ApReady 事件.
         // 避免在事件處理過程送出其他 TMP 訊息.
         this->St_ = SlmTmpSt::ApReady;
         this->WriteLog("|TMP|ApReady|", nullptr);
         char msgApReady[] = "-:ApReady";
         msgApReady[0] = static_cast<char>(this->LineArgs_.ApCode_);
         this->LineMgr_.OnSession_StateUpdated(
            *this->Dev_,
            fon9::StrView{msgApReady, sizeof(msgApReady) - 1},
            fon9::LogLevel::Info);
         this->OnExgTmp_ApReady();
      }
      return;
   }
   this->WriteLog("|TMP|Unknown SubsystemName|", nullptr);
}

//--------------------------------------------------------------------------//
void ExgTmpIoSession::SendTmp(fon9::TimeStamp now, ExgTmpIoRevBuffer&& rbuf) {
   char* pout = rbuf.AllocPrefix(kSLM_HDR_SIZE) - kSLM_HDR_SIZE;
   reinterpret_cast<ExgSlmHead*>(pout)->SetLeadCode(f9tws_kCSTR_SLM_LEAD "00");
   reinterpret_cast<ExgSlmHead*>(pout)->SetBodyLen(fon9::CalcDataSize(rbuf.cfront()) - f9tws_kSIZE_SLM_TAIL);
   rbuf.SetPrefixUsed(pout);
   std::string logb;
   if (fon9_UNLIKELY(this->Log_))
      fon9::BufferAppendTo(rbuf.cfront(), logb);
   this->Dev_->Send(rbuf.MoveOut());
   this->LastTxTime_ = this->TmpTxTime_ = now;
   if (fon9_UNLIKELY(this->Log_))
      this->WriteLog("|Tx|TMP|", &logb);
}

} // namespaces
