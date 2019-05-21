/// \file fon9/fmkt/Trading.cpp
/// \author fonwinz@gmail.com
#include "fon9/fmkt/Trading.hpp"

namespace fon9 { namespace fmkt {

TradingRequest::~TradingRequest() {
}
void TradingRequest::FreeThis() {
   delete this;
}
//--------------------------------------------------------------------------//
TradingLine::~TradingLine() {
}
//--------------------------------------------------------------------------//
TradingLineManager::~TradingLineManager() {
   this->FlowControlTimer_.StopAndWait();
   this->ClearReqQueue(TradingSvr::Locker{this->TradingSvr_}, "TradingLineManager dtor.");
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
      r->NoReadyLineReject(cause);
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
   this->OnNewTradingLineReady(&src, tsvr);
}
void TradingLineManager::FlowControlTimer::EmitOnTimer(TimeStamp now) {
   (void)now;
   TradingLineManager&  rmgr = ContainerOf(*this, &TradingLineManager::FlowControlTimer_);
   TradingSvr::Locker   tsvr{rmgr.TradingSvr_};
   if (!tsvr->Lines_.empty())
      rmgr.OnNewTradingLineReady(nullptr, tsvr);
}
void TradingLineManager::OnNewTradingLineReady(TradingLine* src, const Locker& tsvr) {
   (void)src;
   while (!tsvr->ReqQueue_.empty()) {
      auto res = this->SendRequest(*tsvr->ReqQueue_.front(), tsvr);
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
//--------------------------------------------------------------------------//
SendRequestResult TradingLineManager::SendRequest(TradingRequest& req, const Locker& tsvr) {
   if (size_t sz = tsvr->Lines_.size()) {
      if (fon9_LIKELY(tsvr->ReqQueue_.empty() || tsvr->ReqQueue_.front().get() == &req)) {
         using LineSendResult = TradingLine::SendResult;
         LineSendResult resFlowControl{LineSendResult::Busy};
         for (size_t L = 0; L < sz; ++L) {
            ++tsvr->LineIndex_;
         __RETRY_SAME_INDEX:
            if (tsvr->LineIndex_ >= sz)
               tsvr->LineIndex_ = 0;
            LineSendResult res = tsvr->Lines_[tsvr->LineIndex_]->SendRequest(req);
            if (fon9_LIKELY(res == LineSendResult::Sent))
               return SendRequestResult::Sent;
            if (fon9_LIKELY(res >= LineSendResult::FlowControl)) {
               if (resFlowControl < LineSendResult::FlowControl || res < resFlowControl)
                  resFlowControl = res;
            }
            else if (fon9_LIKELY(res == LineSendResult::RejectRequest)) {
               return SendRequestResult::RejectRequest;
            }
            else if (fon9_LIKELY(res != LineSendResult::Busy)) {
               assert(res == LineSendResult::Broken);
               tsvr->Lines_.erase(tsvr->Lines_.begin() + tsvr->LineIndex_);
               if (L < --sz)
                  goto __RETRY_SAME_INDEX;
               if (sz == 0)
                  goto __SendRequestResult_NoReadyLine;
               break;
            }
         }
         if (resFlowControl >= LineSendResult::FlowControl)
            this->FlowControlTimer_.RunAfter(ToFlowControlInterval(resFlowControl));
      }
      if (tsvr->ReqQueue_.empty() || tsvr->ReqQueue_.front().get() != &req)
         tsvr->ReqQueue_.push_back(&req);
      return SendRequestResult::Queuing;
   }
__SendRequestResult_NoReadyLine:
   req.NoReadyLineReject("No ready line.");
   return SendRequestResult::NoReadyLine;
}

} } // namespaces
