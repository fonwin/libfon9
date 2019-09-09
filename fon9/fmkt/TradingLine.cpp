/// \file fon9/fmkt/TradingLine.cpp
/// \author fonwinz@gmail.com
#include "fon9/fmkt/TradingLine.hpp"

namespace fon9 { namespace fmkt {

TradingLine::~TradingLine() {
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
   if (tsvr->LineIndex_ > static_cast<unsigned>(ifind - ibeg))
      --tsvr->LineIndex_;
   tsvr->Lines_.erase(ifind);
   if (tsvr->Lines_.empty())
      this->ClearReqQueue(std::move(tsvr), "No ready line.");
}
void TradingLineManager::ClearReqQueue(Locker&& tsvr, StrView cause) {
   TradingSvrImpl::Reqs reqs = std::move(tsvr->ReqQueue_);
   tsvr.unlock();
   for (const TradingRequestSP& r : reqs)
      this->NoReadyLineReject(*r, cause);
}
//--------------------------------------------------------------------------//
void TradingLineManager::OnTradingLineReady(TradingLine& src) {
   TradingSvr::Locker tsvr{this->TradingSvr_};
   auto ifind = std::find(tsvr->Lines_.begin(), tsvr->Lines_.end(), &src);
   if (ifind == tsvr->Lines_.end()) {
      tsvr->LineIndex_ = static_cast<unsigned>(tsvr->Lines_.size());
      tsvr->Lines_.push_back(&src);
   }
   else {
      tsvr->LineIndex_ = static_cast<unsigned>(ifind - tsvr->Lines_.begin());
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
         /// - 多線路時怎樣使用才是最好? 還要進一步研究.
         ++tsvr->LineIndex_;

      __RETRY_SAME_INDEX:
         if (tsvr->LineIndex_ >= lineCount)
            tsvr->LineIndex_ = 0;
         LineSendResult resSend = tsvr->Lines_[tsvr->LineIndex_]->SendRequest(req);
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
            tsvr->Lines_.erase(tsvr->Lines_.begin() + tsvr->LineIndex_);
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

} } // namespaces
