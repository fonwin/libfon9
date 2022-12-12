/// \file fon9/fmkt/TradingLineHelper1.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_TradingLineHelper1_hpp__
#define __fon9_fmkt_TradingLineHelper1_hpp__
#include "fon9/fmkt/TradingLineGroup.hpp"
#include "fon9/fmkt/TradingLine.hpp"

namespace fon9 { namespace fmkt {

// 這裡提供一個簡易的 TradingLineManager Helper 範例:
// 使用 Lg 機制, 同一個 LgMgr 裡面, 不同的線路群組之間的備援.
// - Helper: 協助 LineMgr(Asker) 尋找 Offer;
// - Asker:  提出轉送要求的 LineMgr;
// - Offer:  實際提供轉送功能的人;
//   - 可能直接由本地端的 LineMgr 提供協助, 也可能由遠端的 LineMgr 提供協助, 甚至可能由券商系統提供協助;
//--------------------------------------------------------------------------//
struct LocalHelperMaker;

/// 協助 LineMgr(Offer) 支援 LineMgr(Asker) 傳送 Request.
class fon9_API LocalHelpOfferEvHandler : public TradingLineManagerEv {
   fon9_NON_COPY_NON_MOVE(LocalHelpOfferEvHandler);
   using base = TradingLineManagerEv;
   /// 當 this Offer 的狀態有異動, 必須通知 Asker 繼續傳送 or 拒絕排隊中委託.
   CharVector  AskerLgList_;

   friend struct LocalHelperMaker;
   /// 僅允許在系統初始化階段設定 asker.
   bool AddAskerLg(LgOut askerLg) {
      const char chLg = static_cast<char>(askerLg);
      if (memchr(this->AskerLgList_.begin(), chLg, this->AskerLgList_.size()) != nullptr)
         return false;
      this->AskerLgList_.push_back(chLg);
      return true;
   }

protected:
   /// - 預設: 通知全部 asker->SendReqQueue(asker->Lock());
   /// - 同時鎖 tsvrOffer 及 asker 有可能會死結!
   /// - tsvrOffer==nullptr: 從 OnWorkingRequestDone(); 而來.
   virtual void NotifyToAsker(const TLineLocker* tsvrOffer);
   /// 當有特殊情況時, 清除 [工作中(尚未傳送完成)] 的下單要求.
   /// 例如: 有線路斷線時.
   virtual void ClearWorkingRequests() = 0;

public:
   TradingLgMgrBase&    LgMgr_;
   TradingLineManager&  Offer_;
   const CharVector     Name_;

   LocalHelpOfferEvHandler(TradingLgMgrBase& lgMgr, TradingLineManager& offer, const StrView& name)
      : LgMgr_(lgMgr)
      , Offer_(offer)
      , Name_{name} {
   }
   virtual ~LocalHelpOfferEvHandler();

   const CharVector& AskerLgList() const {
      return this->AskerLgList_;
   }
   bool IsAskerEmpty() const {
      return this->AskerLgList_.empty();
   }
   bool IsNoReadyLine() const {
      return this->Offer_.IsNoReadyLine();
   }
   virtual bool IsBusyWorking() const = 0;
   virtual void GetStInfo(RevBuffer& rbuf) const;

   virtual SendRequestResult OnAskFor_SendRequest(TradingRequest& req, const TLineLocker& tsvrAsker);

   void OnNewTradingLineReady(TradingLine* src, const TLineLocker& tsvrFrom) override;
   void OnTradingLineBroken(TradingLine& src, const TLineLocker& tsvrFrom) override;
   void OnSendReqQueueEmpty(const TLineLocker& tsvrFrom) override;
   void OnTradingLineManagerDetach(const TLineLocker& tsvrFrom) override;
};
using LocalHelpOfferEvHandlerSP = intrusive_ptr<LocalHelpOfferEvHandler>;
using HelpOfferList = std::vector<LocalHelpOfferEvHandlerSP>;
//--------------------------------------------------------------------------//
/// 簡易版 Offer, 不論本身有多少 Lines, 不論有多少 Asker, 每次僅支援一筆 Request;
/// 只有當 WorkingRequest_ 完成後, 呼叫 this->OnWorkingRequestDone() 才能繼續支援, 傳送下一筆.
class fon9_API LocalHelpOfferEvHandler1 : public LocalHelpOfferEvHandler {
   fon9_NON_COPY_NON_MOVE(LocalHelpOfferEvHandler1);
   using base = LocalHelpOfferEvHandler;
protected:
   TradingRequestSP  WorkingRequest_;
   /// 當 this->WorkingRequest_ 完成後, 就可以支援下一次的傳送.
   void OnWorkingRequestDone() {
      this->WorkingRequest_.reset();
      this->NotifyToAsker(nullptr);
   }
   void ClearWorkingRequests() override;
public:
   using base::base;
   bool IsBusyWorking() const override;
   SendRequestResult OnAskFor_SendRequest(TradingRequest& req, const TLineLocker& tsvrAsker) override;
};
//--------------------------------------------------------------------------//
/// - 在 OnAskFor_xxx() 事件中:
///   - 如果其他 thread 正在處理 offer->Lock(); 且造成 from.Lock(); ====> 那就死結了!!
///   - 所以 OnAskFor_BeforeQueue() 事件, 必需在特定的管理機制下執行(例: 僅能在某個特定 thread 執行 SendRequest)
class fon9_API LocalHelper : public TradingLineHelper {
   fon9_NON_COPY_NON_MOVE(LocalHelper);
   using base = TradingLineHelper;
public:
   TradingLgMgrBase&     LgMgr_;
   TradingLineManager&   Asker_;
   const HelpOfferList   OfferList_;

   LocalHelper(TradingLgMgrBase& lgMgr, TradingLineManager& asker, HelpOfferList&& offerList)
      : LgMgr_(lgMgr)
      , Asker_(asker)
      , OfferList_{offerList} {
   }
   SendRequestResult OnAskFor_BeforeQueue(TradingRequest& req, const TLineLocker& tsvrAsker) override;
   SendRequestResult OnAskFor_NoReadyLine(TradingRequest& req, const TLineLocker& tsvrAsker) override;
   bool IsHelperReady() const override;
   void GetStInfo(RevBuffer& rbuf) const override;
};
//--------------------------------------------------------------------------//
struct fon9_API LocalHelperMaker {
   virtual ~LocalHelperMaker();

   virtual LocalHelpOfferEvHandlerSP MakeHelpOffer(TradingLgMgrBase& lgMgr, TradingLineManager& lmgrOffer, const StrView& name) = 0;
   /// 預設: new LocalHelper{lgMgr, *lmgr, std::move(askerOfferList)}
   virtual TradingLineHelperSP MakeHelper(TradingLgMgrBase& lgMgr, TradingLineManager& asker, HelpOfferList&& offerList);

   void MakeTradingLgLocalHelper(TradingLgMgrBase& lgMgr);
};

} } // namespaces
#endif//__fon9_fmkt_TradingLineHelper1_hpp__
