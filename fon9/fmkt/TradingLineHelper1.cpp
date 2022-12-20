/// \file fon9/fmkt/TradingLineHelper1.cpp
/// \author fonwinz@gmail.com
#include "fon9/fmkt/TradingLineHelper1.hpp"
#include <array>

namespace fon9 { namespace fmkt {

void LocalHelper::GetStInfo(RevBuffer& rbuf) const {
   if (this->OfferList_.empty()) {
      RevPrint(rbuf, "NoOffer");
   }
   else {
      char chTail = '}';
      for (auto& offer : this->OfferList_) {
         RevPrint(rbuf, chTail);
         offer->GetStInfo(rbuf);
         chTail = '|';
      }
      RevPrint(rbuf, '{');
   }
}
bool LocalHelper::IsHelperReady() const {
   // 只要有一個 Offer 可提供協助, 就是 IsHelperReady 狀態.
   for (auto& offer : this->OfferList_) {
      if (!offer->IsNoReadyLine())
         return true;
   }
   return false;
}
SendRequestResult LocalHelper::OnAskFor_BeforeQueue(TradingRequest& req, const TLineLocker& tsvrAsker) {
   (void)tsvrAsker; assert(&tsvrAsker->GetOwner() == &this->Asker_);
   for (auto& offer : this->OfferList_) {
      switch (const auto retval = offer->OnAskFor_SendRequest(req, tsvrAsker)) {
      case SendRequestResult::Queuing:
      case SendRequestResult::NoReadyLine:
         continue;
      case SendRequestResult::RejectRequest:
      case SendRequestResult::Sent:
      case SendRequestResult::RequestEnd:
         return retval;
      }
   }
   return SendRequestResult::NoReadyLine;
}
SendRequestResult LocalHelper::OnAskFor_NoReadyLine(TradingRequest& req, const TLineLocker& tsvrAsker) {
   (void)tsvrAsker; assert(&tsvrAsker->GetOwner() == &this->Asker_);
   SendRequestResult finalResult = SendRequestResult::NoReadyLine;
   for (auto& offer : this->OfferList_) {
      switch (const auto retval = offer->OnAskFor_SendRequest(req, tsvrAsker)) {
      case SendRequestResult::Queuing: // 當 offer 有空時, 還是可以支援, 所以最終可以返回 Queuing;
         finalResult = SendRequestResult::Queuing;
         continue;
      case SendRequestResult::NoReadyLine:
         continue;
      case SendRequestResult::RejectRequest:
      case SendRequestResult::Sent:
      case SendRequestResult::RequestEnd:
         return retval;
      }
   }
   return finalResult;
}
//--------------------------------------------------------------------------//
LocalHelpOfferEvHandler::~LocalHelpOfferEvHandler() {
}
void LocalHelpOfferEvHandler::NotifyToAsker(const TLineLocker* tsvrOffer) {
   (void)tsvrOffer;
   if (this->IsBusyWorking())
      return;
   for (char chLg : this->AskerLgList_) {
      if (auto* asker = this->LgMgr_.GetLineMgr(static_cast<LgOut>(chLg), this->Offer_)) {
         if (fon9_LIKELY(asker != &this->Offer_)) {
            asker->SendReqQueue(asker->Lock());
            if (this->IsBusyWorking())
               break;
         }
      }
   }
}
void LocalHelpOfferEvHandler::GetStInfo(RevBuffer& rbuf) const {
   if (this->IsBusyWorking())
      RevPrint(rbuf, "/Busy");
   RevPrint(rbuf, this->Name_, ':', this->Offer_.Lock()->CountReadyLines());
}
SendRequestResult LocalHelpOfferEvHandler::OnAskFor_SendRequest(TradingRequest& req, const TLineLocker& tsvrAsker) {
   assert(&this->Offer_ != &tsvrAsker->GetOwner());
   if (&this->Offer_ == &tsvrAsker->GetOwner())
      return SendRequestResult::NoReadyLine;
   auto tsvrOffer = this->Offer_.Lock();
   if (tsvrOffer->IsLinesEmpty())
      return SendRequestResult::NoReadyLine;
   if (this->IsBusyWorking())
      return SendRequestResult::Queuing;
   return this->Offer_.OnAskFor_SendRequest_FromLocal(req, tsvrOffer, tsvrAsker);
}
void LocalHelpOfferEvHandler::OnNewTradingLineReady(TradingLine* src, const TLineLocker& tsvrFrom) {
   (void)src;
   assert(&this->Offer_ == &tsvrFrom->GetOwner() && !tsvrFrom->IsLinesEmpty());
   if (tsvrFrom->IsReqQueueEmpty())
      this->NotifyToAsker(&tsvrFrom);
}
void LocalHelpOfferEvHandler::OnSendReqQueueEmpty(const TLineLocker& tsvrFrom) {
   assert(&this->Offer_ == &tsvrFrom->GetOwner());
   this->NotifyToAsker(&tsvrFrom);
}
void LocalHelpOfferEvHandler::OnTradingLineBroken(TradingLine& src, const TLineLocker& tsvrFrom) {
   (void)src;
   assert(&this->Offer_ == &tsvrFrom->GetOwner());
   // this->WorkingRequest_ 送出後斷線,
   // 可能無法更新 this->WorkingRequest_ 的後續狀態, 這會造成 asker 永遠等不到支援!!!
   // 所以不論現在是否仍有可用線路, 接下來一律: 清除 this->WorkingRequest_, 並通知 Asker;
   this->ClearWorkingRequests();
   this->NotifyToAsker(&tsvrFrom);
}
void LocalHelpOfferEvHandler::OnTradingLineManagerDetach(const TLineLocker& tsvrFrom) {
   assert(&this->Offer_ == &tsvrFrom->GetOwner());
   this->ClearWorkingRequests();
   this->NotifyToAsker(&tsvrFrom);
}
//--------------------------------------------------------------------------//
void LocalHelpOfferEvHandler1::ClearWorkingRequests() {
   this->WorkingRequest_.reset();
}
bool LocalHelpOfferEvHandler1::IsBusyWorking() const {
   return this->WorkingRequest_.get() != nullptr;
}
SendRequestResult LocalHelpOfferEvHandler1::OnAskFor_SendRequest(TradingRequest& req, const TLineLocker& tsvrAsker) {
   const auto retval = base::OnAskFor_SendRequest(req, tsvrAsker);
   switch (retval) {
   case SendRequestResult::Sent:
      this->WorkingRequest_.reset(&req);
      break;
   case SendRequestResult::Queuing:
   case SendRequestResult::NoReadyLine:
   case SendRequestResult::RejectRequest:
   case SendRequestResult::RequestEnd:
      break;
   }
   return retval;
}
//--------------------------------------------------------------------------//
LocalHelperMaker::~LocalHelperMaker() {
}
TradingLineHelperSP LocalHelperMaker::MakeHelper(TradingLgMgrBase& lgMgr, TradingLineManager& asker, HelpOfferList&& offerList) {
   return new LocalHelper{lgMgr, asker, std::move(offerList)};
}
void LocalHelperMaker::MakeTradingLgLocalHelper(TradingLgMgrBase& lgMgr) {
   const auto lmCount = lgMgr.LgLineMgrCount();

   using OfferMap = std::array<LocalHelpOfferEvHandlerSP, kLgOutCount>;
   std::vector<OfferMap>  offerMap;
   offerMap.resize(lmCount);

   using AskerOfferList = std::array<HelpOfferList, kLgOutCount>;
   std::vector<AskerOfferList> askerMap;
   askerMap.resize(lmCount);

   for (LgIndex askerLgIndex = 0; askerLgIndex < kLgOutCount; ++askerLgIndex) {
      auto* askerLg = lgMgr.GetLgItem(askerLgIndex);
      if (askerLg == nullptr)
         continue;
      fon9::StrView cfgstr{&askerLg->GetDescription()};
      fon9::StrView tag, value;
      while (fon9::StrFetchTagValue(cfgstr, tag, value)) {
         if (!fon9::iequals(tag, "Helper"))
            continue;
         // value = offerLgList;
         for (char lgOffer : value) {
            if (!IsValidateLgOut(static_cast<LgOut>(lgOffer)))
               continue;
            const auto offerLgIndex = LgOutToIndex(static_cast<LgOut>(lgOffer));
            if (offerLgIndex == askerLgIndex)
               continue;
            for (LmIndex lmIndex = 0; lmIndex < lmCount; ++lmIndex) {
               auto* lmgrAsker = lgMgr.GetLineMgr(askerLgIndex, lmIndex);
               if (lmgrAsker == nullptr)
                  continue;
               auto* lmgrOffer = lgMgr.GetLineMgr(offerLgIndex, lmIndex);
               if (lmgrOffer == nullptr)
                  continue;
               auto& offer = offerMap[lmIndex][offerLgIndex];
               if (!offer) {
                  char name[3] = {'L', 'g', lgOffer}; // "Lg-";
                  offer = this->MakeHelpOffer(lgMgr, *lmgrOffer, StrView{name,3});
               }
               if (offer->AddAskerLg(LgIndexToOut(askerLgIndex))) {
                  askerMap[lmIndex][askerLgIndex].push_back(offer);
               }
            }
         }
      }
   }
   for (LgIndex lgIndex = 0; lgIndex < kLgOutCount; ++lgIndex) {
      auto* lgItem = lgMgr.GetLgItem(lgIndex);
      if (lgItem == nullptr)
         continue;
      for (LmIndex lmIndex = 0; lmIndex < lmCount; ++lmIndex) {
         auto* lmgr = lgMgr.GetLineMgr(lgIndex, lmIndex);
         if (lmgr == nullptr)
            continue;
         auto& offerHandler = offerMap[lmIndex][lgIndex];
         if (offerHandler && !offerHandler->IsAskerEmpty()) {
            lmgr->SetEvHandler(offerHandler);
         }
         auto& askerOfferList = askerMap[lmIndex][lgIndex];
         if (!askerOfferList.empty()) {
            lmgr->UpdateHelper(this->MakeHelper(lgMgr, *lmgr, std::move(askerOfferList)));
         }
      }
   }
}

} } // namespaces
