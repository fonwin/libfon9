/// \file fon9/fmkt/TradingLine.cpp
/// \author fonwinz@gmail.com
#include "fon9/fmkt/TradingLine.hpp"
#include "fon9/TimedFileName.hpp"
#include "fon9/FilePath.hpp"

namespace fon9 { namespace fmkt {

TradingLine::~TradingLine() {
}
bool TradingLine::IsOrigSender(const TradingRequest& req) const {
   (void)req;
   return false;
}
//--------------------------------------------------------------------------//
TradingLineManager::~TradingLineManager() {
   this->OnBeforeDestroy();
}
void TradingLineManager::OnBeforeDestroy() {
   this->FlowControlTimer_.DisposeAndWait();
   this->ClearReqQueue(TradingSvr::Locker{this->TradingSvr_}, "TradingLineManager.dtor");
}
void TradingLineManager::OnTradingLineBroken(TradingLine& src) {
   TradingSvr::Locker tsvr{this->TradingSvr_};
   auto ibeg = tsvr->Lines_.begin();
   auto ifind = std::find(tsvr->Lines_.begin(), tsvr->Lines_.end(), &src);
   if (ifind == tsvr->Lines_.end())
      return;
   if (tsvr->LastIndex_ > static_cast<unsigned>(ifind - ibeg))
      --tsvr->LastIndex_;
   tsvr->Lines_.erase(ifind);
   if (tsvr->Lines_.empty())
      this->ClearReqQueue(std::move(tsvr), "No ready line.");
}
void TradingLineManager::ClearReqQueue(Locker&& tsvr, StrView cause) {
   Reqs reqs = std::move(tsvr->ReqQueue_);
   tsvr.unlock();
   for (const TradingRequestSP& r : reqs)
      this->NoReadyLineReject(*r, cause);
}
SendRequestResult TradingLineManager::RequestPushToQueue(TradingRequest& req, const Locker& tsvr) {
   tsvr->ReqQueue_.push_back(&req);
   return SendRequestResult::Queuing;
}
//--------------------------------------------------------------------------//
void TradingLineManager::OnTradingLineReady(TradingLine& src) {
   TradingSvr::Locker tsvr{this->TradingSvr_};
   auto ifind = std::find(tsvr->Lines_.begin(), tsvr->Lines_.end(), &src);
   if (ifind == tsvr->Lines_.end()) {
      tsvr->LastIndex_ = static_cast<unsigned>(tsvr->Lines_.size());
      tsvr->Lines_.push_back(&src);
   }
   else {
      tsvr->LastIndex_ = static_cast<unsigned>(ifind - tsvr->Lines_.begin());
   }
   this->OnNewTradingLineReady(&src, std::move(tsvr));
}
void TradingLineManager::FlowControlTimer::EmitOnTimer(TimeStamp now) {
   (void)now;
   TradingLineManager&  rmgr = ContainerOf(*this, &TradingLineManager::FlowControlTimer_);
   TradingSvr::Locker   tsvr{rmgr.TradingSvr_};
   if (!tsvr->Lines_.empty())
      rmgr.OnNewTradingLineReady(nullptr, std::move(tsvr));
}
void TradingLineManager::SendReqQueue(const Locker& tsvr) {
   while (!tsvr->ReqQueue_.empty()) {
      auto res = this->SendRequestImpl(*tsvr->ReqQueue_.front(), tsvr);
      switch (res) {
      case SendRequestResult::Queuing:
         return;
      default:
      case SendRequestResult::RejectRequest:
      case SendRequestResult::Sent:
      case SendRequestResult::NoReadyLine:
      case SendRequestResult::RequestEnd:
         tsvr->ReqQueue_.pop_front();
         continue;
      }
   }
}
void TradingLineManager::OnNewTradingLineReady(TradingLine* src, Locker&& tsvr) {
   (void)src;
   this->SendReqQueue(tsvr);
}
//--------------------------------------------------------------------------//
SendRequestResult TradingLineManager::CheckOpQueuingRequest(Reqs& reqQueue, TradingRequest& req) {
   // 因為 req 已經準備要進排隊了, 所以這裡沒有速度的要求, 就整個 queue 都掃一遍吧!
   if (!req.PreOpQueuingRequest(*this))
      return SendRequestResult::Queuing;
   const auto iend = reqQueue.end();
   auto i = reqQueue.begin();
   while (i != iend) {
      switch (req.OpQueuingRequest(*this, **i)) {
      default:
      case TradingRequest::Op_NotSupported:                       return SendRequestResult::Queuing;
      case TradingRequest::Op_ThisDone:                           return SendRequestResult::RequestEnd;
      case TradingRequest::Op_AllDone:       reqQueue.erase(i);   return SendRequestResult::RequestEnd;
      case TradingRequest::Op_TargetDone:    reqQueue.erase(i);   return SendRequestResult::Queuing;
      case TradingRequest::Op_NotTarget:                          break; // check next queue item;
      }
      ++i;
   }
   return SendRequestResult::Queuing;
}
void TradingLineManager::SelectPreferNextLine(const TradingRequest& req) {
   Locker tsvr{this->TradingSvr_};
   if (const size_t lineCount = tsvr->Lines_.size()) {
      for (size_t L = 0; L < lineCount; ++L) {
         unsigned nextIndex = static_cast<unsigned>((tsvr->LastIndex_ + 1) % lineCount);
         if (req.IsPreferTradingLine(*tsvr->Lines_[nextIndex]))
            break;
         tsvr->LastIndex_ = nextIndex;
      }
   }
}
SendRequestResult TradingLineManager::SendRequestImpl(TradingRequest& req, const Locker& tsvr) {
   if (size_t lineCount = tsvr->Lines_.size()) {
      using LineSendResult = TradingLine::SendResult;
      LineSendResult resFlowControl{LineSendResult::Busy};
      bool           hasBusyLine = false;
      for (size_t L = 0; L < lineCount; ++L) {

         // 若有多條線路, 目前使用輪流(均分)的方式送出要求.
         // - 但是此法是最快的嗎? => 不一定.
         // - 現代 CPU 有大量的 cache, 盡量固定使用某一條線路, 可讓 CPU cache 發揮最大功效.
         // - 但是某條線路塞到滿之後再換下一條, 也有TCP流量問題要考慮.
         // - 多線路時怎樣使用才是最好? 還要進一步研究.
         ++tsvr->LastIndex_;

      __RETRY_SAME_INDEX:
         if (tsvr->LastIndex_ >= lineCount)
            tsvr->LastIndex_ = 0;
         LineSendResult resSend = tsvr->Lines_[tsvr->LastIndex_]->SendRequest(req);
         if (fon9_LIKELY(resSend == LineSendResult::Sent))
            return SendRequestResult::Sent;

         if (fon9_LIKELY(resSend >= LineSendResult::FlowControl)) {
            if (resFlowControl < LineSendResult::FlowControl || resSend < resFlowControl)
               resFlowControl = resSend;
         }
         else if (fon9_LIKELY(resSend == LineSendResult::RejectRequest))
            return SendRequestResult::RejectRequest;
         else if (fon9_LIKELY(resSend == LineSendResult::NotSupport)) {
            // 等迴圈結束, 再來判斷是否需要 Queue.
         }
         else if (fon9_LIKELY(resSend == LineSendResult::Busy))
            hasBusyLine = true;
         else {
            assert(resSend == LineSendResult::Broken);
            tsvr->Lines_.erase(tsvr->Lines_.begin() + tsvr->LastIndex_);
            if (L < --lineCount)
               goto __RETRY_SAME_INDEX;
            if (lineCount == 0)
               goto __SendRequestResult_NoReadyLine;
            break;
         }
         // FlowControl or Busy: continue check the next line.
      } // for(L < lineCount)
      if (resFlowControl >= LineSendResult::FlowControl)
         this->FlowControlTimer_.RunAfter(ToFlowControlInterval(resFlowControl));
      else if (!hasBusyLine) { // All not support.
         goto __SendRequestResult_NoReadyLine;
      }
      return SendRequestResult::Queuing;
   }
__SendRequestResult_NoReadyLine:
   return this->NoReadyLineReject(req, "No ready line.");
}
SendRequestResult TradingLineManager::NoReadyLineReject(TradingRequest& req, StrView cause) {
   (void)req; (void)cause;
   return SendRequestResult::NoReadyLine;
}
//--------------------------------------------------------------------------//
TradingLineLogPathMaker::~TradingLineLogPathMaker() {
}
TimeStamp TradingLineLogPathMaker::GetTDay() {
   return UtcNow() + GetLocalTimeZoneOffset();
}
std::string TradingLineLogPathMaker::MakeLogPath(std::string& res, TimeStamp* outTDay) {
   TimeStamp tday = this->GetTDay();
   if (outTDay)
      *outTDay = tday;
   if (tday.IsNullOrZero())
      return "|err=Unknown TDay.";
   TimedFileName  logPath(this->LogPathFmt_, TimedFileName::TimeScale::Day);
   // log 檔名與 TDay 相關, 與 TimeZone 無關, 所以要排除 TimeZone;
   logPath.RebuildFileNameExcludeTimeZone(tday);
   res = FilePath::AppendPathTail(&logPath.GetFileName());
   return std::string{};
}

} } // namespaces
