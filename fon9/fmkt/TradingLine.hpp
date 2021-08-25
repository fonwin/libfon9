/// \file fon9/fmkt/TradingLine.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_TradingLine_hpp__
#define __fon9_fmkt_TradingLine_hpp__
#include "fon9/fmkt/TradingRequest.hpp"
#include "fon9/Timer.hpp"
#include <deque>

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 交易連線基底.
class fon9_API TradingLine {
   fon9_NON_COPY_NON_MOVE(TradingLine);
public:
   TradingLine() = default;

   virtual ~TradingLine();

   enum class SendResult {
      Sent = 0,
      /// >= SendRequestResult::FlowControl 表示流量管制.
      /// 可透過 ToFlowControlInterval(res); 取得: 還要等候多久才解除管制.
      FlowControl = 1,

      /// 線路忙碌中, 例如: 每次只能送1筆, 必須等收到結果後才能送下一筆.
      Busy = -1,
      /// 線路已斷線無法再送單.
      Broken = -2,
      /// 拒絕送出 Request: 通常是 Request 的內容有誤.
      RejectRequest = -3,
      /// 例如: 零股線路, 不支援定價交易.
      NotSupport = -4,
   };
   /// 設計衍生者請注意:
   /// 透過 TradingLineManager 來的下單要求, 必定已經鎖住「可用線路表」,
   /// 因此不可再呼叫 TradingLineManager 的相關函式, 會造成死結!
   virtual SendResult SendRequest(TradingRequest& req) = 0;

   /// 判斷 this 是否為 req 的原始傳送者.
   /// 預設傳回 false;
   virtual bool IsOrigSender(const TradingRequest& req) const;
};
inline TimeInterval ToFlowControlInterval(TradingLine::SendResult r) {
   assert(r >= TradingLine::SendResult::FlowControl);
   return TimeInterval::Make<6>(static_cast<TimeInterval::OrigType>(r));
}
inline TradingLine::SendResult ToFlowControlResult(TimeInterval ti) {
   assert(static_cast<TradingLine::SendResult>(ti.GetOrigValue()) >= TradingLine::SendResult::FlowControl);
   return static_cast<TradingLine::SendResult>(ti.GetOrigValue());
}

/// \ingroup fmkt
/// 下單要求傳送的結果.
enum class SendRequestResult : int {
   /// 返回此訊息前, 必定已經呼叫過 TradingLineManager::NoReadyLineReject();
   NoReadyLine = -2,
   /// 拒絕送出 Request: 通常是 Request 的內容有誤.
   RejectRequest = -3,

   Sent = 0,
   Queuing = 1,
   /// 下單要求完成, 既沒送出, 也沒排隊, 例: 內部刪單.
   RequestEnd = 2,
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup fmkt
/// 交易連線管理員基底.
/// - 負責尋找可下單的線路送出下單要求.
/// - 負責處理流量管制:
///   - 有線路但暫時無法送單(例: FIX流量管制, TMP送單中), 將下單要求放到 Queue.
///   - 等候流量管制時間, 時間到解除管制時, 透過 OnNewTradingLineReady() 通知.
class fon9_API TradingLineManager {
   fon9_NON_COPY_NON_MOVE(TradingLineManager);

protected:
   using Reqs = std::deque<TradingRequestSP>;
   /// 可送出下單要求的連線. 流量管制時不移除.
   struct TradingSvrImpl {
      using Lines = std::vector<TradingLine*>;
      unsigned LastIndex_{0};
      Lines    Lines_;
      Reqs     ReqQueue_;
   };
   using TradingSvr = MustLock<TradingSvrImpl>;
   TradingSvr  TradingSvr_;

public:
   using Locker = TradingSvr::Locker;
   using QueueReqs = Reqs;

   TradingLineManager() = default;
   virtual ~TradingLineManager();

   /// 當 src 進入可下單狀態時的通知:
   /// - 連線成功後, 進入可下單狀態.
   /// - 從忙碌狀態, 進入可下單狀態.
   void OnTradingLineReady(TradingLine& src);

   /// 當 src 斷線時的通知.
   /// 不包含: 流量管制, 線路忙碌.
   void OnTradingLineBroken(TradingLine& src);

   SendRequestResult SendRequest(TradingRequest& req) {
      return this->SendRequest(req, Locker{this->TradingSvr_});
   }
   SendRequestResult SendRequest(TradingRequest& req, const Locker& tsvr) {
      SendRequestResult res;
      if (fon9_LIKELY(tsvr->ReqQueue_.empty())) {
         res = this->SendRequestImpl(req, tsvr);
      }
      else {
         res = this->CheckOpQueuingRequest(tsvr->ReqQueue_, req);
      }
      if (res == SendRequestResult::Queuing)
         tsvr->ReqQueue_.push_back(&req);
      return res;
   }

   /// 透過 req.IsPreferTradingLine(); 來選擇下一個建議的交易線路.
   /// 但不一定會用該線路, 例: 如果該線路忙碌(流量管制), 則會使用其他線路.
   void SelectPreferNextLine(const TradingRequest& req);

protected:
   /// 衍生者在解構時, 應先呼叫此處,
   /// 如此在 Queue 之中的 req 才會通過 this->NoReadyLineReject(req) 通知衍生者.
   virtual void OnBeforeDestroy();

   /// 清除全部的「排隊中下單要求」.
   virtual void ClearReqQueue(Locker&& tsvr, StrView cause);

   /// - 無可用線路時的拒絕.
   /// - TradingLineManager 解構時, 拒絕還在 Queue 裡面的要求.
   /// - 預設 do nothing, 直接返回 SendRequestResult::NoReadyLine;
   virtual SendRequestResult NoReadyLineReject(TradingRequest& req, StrView cause);

   /// 當有新的可交易線路時的通知, 預設: 送出 queue 的 req.
   /// - src == nullptr 表示從計時器來的: 流量管制解除.
   /// - 如果是從 OnTradingLineReady() 來的, 則 tsvr->Lines_[tsvr->LastIndex_] == src.
   virtual void OnNewTradingLineReady(TradingLine* src, Locker&& tsvr);

   /// 檢查 req 要操作的標的是否在 reqQueue 裡面?
   /// 如果是, 也許可以內部[刪改查], 不用送出.
   /// 當 reqQueue.empty() 時, 不會來到此處.
   /// 預設直接返回 SendRequestResult::Queuing:
   /// 表示 req 為一般要求, 呼叫端必定會將 req 放入 reqQueue.
   SendRequestResult CheckOpQueuingRequest(Reqs& reqQueue, TradingRequest& req);

   /// 嘗試送出 Queue 裡面的下單要求.
   void SendReqQueue(const Locker& tsvr);

   SendRequestResult SendRequestImpl(TradingRequest& req, const Locker& tsvr);

private:
   struct FlowControlTimer : public DataMemberTimer {
      fon9_NON_COPY_NON_MOVE(FlowControlTimer);
      FlowControlTimer() = default;
      virtual void EmitOnTimer(TimeStamp now) override;
   };
   FlowControlTimer FlowControlTimer_;
};
fon9_WARN_POP;

class fon9_API TradingLineLogPathMaker {
   fon9_NON_COPY_NON_MOVE(TradingLineLogPathMaker);

public:
   const std::string LogPathFmt_;

   TradingLineLogPathMaker(std::string logPathFmt)
      : LogPathFmt_{std::move(logPathFmt)} {
   }
   virtual ~TradingLineLogPathMaker();

   /// 預設 = 本地時間 = UtcNow() + GetLocalTimeZoneOffset();
   virtual TimeStamp GetTDay();

   /// - *outTDay = this->GetTDay();
   /// - 若 this->GetTDay() 無效時, 傳回失敗訊息(例如: "|err=Unknown TDay.");
   /// - retval.empty() 表示結果存放在 res; 返回時 res 尾端會加上 '/';
   std::string MakeLogPath(std::string& res, TimeStamp* outTDay = nullptr);
};

} } // namespaces
#endif//__fon9_fmkt_TradingLine_hpp__
