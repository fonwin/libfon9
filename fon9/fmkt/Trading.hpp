/// \file fon9/fmkt/Trading.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_Trading_hpp__
#define __fon9_fmkt_Trading_hpp__
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/buffer/RevBuffer.hpp"
#include "fon9/Timer.hpp"
#include <deque>

namespace fon9 { namespace fmkt {

class fon9_API TradingLineManager;

/// \ingroup fmkt
/// 回報序號, 每台主機自行依序編號.
/// 也就是 HostA.RxSNO=1, 與 HostB.RxSNO=1, 可能是不同的 TradingRxItem.
using TradingRxSNO = uint64_t;

/// \ingroup fmkt
/// 回報項目: 可能為「下單要求(新、刪、改、查...)」、「成交回報」、「委託異動」、「交易系統事件」...
class fon9_API TradingRxItem : public intrusive_ref_counter<TradingRxItem> {
   fon9_NON_COPY_NON_MOVE(TradingRxItem);

protected:
   f9fmkt_RxKind           RxKind_{};
   f9fmkt_TradingMarket    Market_{};
   f9fmkt_TradingSessionId SessionId_{};
   /// RxItemFlags_ 由衍生者自行決定如何使用.
   uint8_t                 RxItemFlags_{};
   TradingRxSNO            RxSNO_{0};

   virtual ~TradingRxItem();

public:
   TradingRxItem() = default;
   TradingRxItem(f9fmkt_RxKind rxKind) : RxKind_{rxKind} {
   }

   /// 必須透過 FreeThis() 來刪除, 預設 delete this;
   /// 若有使用 ObjCarrierTape 則將 this 還給 ObjCarrierTape;
   virtual void FreeThis();
   inline friend void intrusive_ptr_deleter(const TradingRxItem* p) {
      const_cast<TradingRxItem*>(p)->FreeThis();
   }

   f9fmkt_RxKind           RxKind() const { return this->RxKind_; }
   f9fmkt_TradingMarket    Market() const { return this->Market_; }
   f9fmkt_TradingSessionId SessionId() const { return this->SessionId_; }

   TradingRxSNO RxSNO() const {
      return this->RxSNO_;
   }
   /// 只能設定一次, 設定後不應該更改.
   TradingRxSNO SetRxSNO(TradingRxSNO sno) {
      assert(this->RxSNO_ == 0);
      return this->RxSNO_ = sno;
   }

   /// 將內容依照 Fields 的順序輸出到 rbuf;
   /// - 前方須包含 Name, 尾端 **不可加 '\n'**
   /// - 使用 fon9_kCSTR_CELLSPL 分隔欄位;
   /// - 不含 RxSNO();
   /// - 預設: do nothing.
   virtual void RevPrint(RevBuffer& rbuf) const;
   /// 預設傳回 nullptr.
   virtual const TradingRxItem* CastToRequest() const;
   /// 預設傳回 nullptr.
   virtual const TradingRxItem* CastToOrder() const;
   /// 預設傳回 nullptr.
   virtual const TradingRxItem* CastToEvent() const;
};
using TradingRxItemSP = intrusive_ptr<TradingRxItem>;

/// \ingroup fmkt
/// 下單要求基底.
class fon9_API TradingRequest : public TradingRxItem {
   fon9_NON_COPY_NON_MOVE(TradingRequest);
   using base = TradingRxItem;

protected:
   virtual ~TradingRequest();

public:
   using base::base;
   TradingRequest() = default;

   /// - 無可用線路時的拒絕.
   /// - TradingLineManager 解構時, 拒絕還在 Queue 裡面的要求.
   virtual void NoReadyLineReject(StrView cause) = 0;
};
using TradingRequestSP = intrusive_ptr<TradingRequest>;

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
   };
   /// 設計衍生者請注意:
   /// 透過 TradingLineManager 來的下單要求, 必定已經鎖住「可用線路表」,
   /// 因此不可再呼叫 TradingLineManager 的相關函式, 會造成死結!
   virtual SendResult SendRequest(TradingRequest& req) = 0;
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
   /// 返回此訊息前, 必定已經呼叫過 req.NoReadyLineReject();
   NoReadyLine = -2,
   /// 拒絕送出 Request: 通常是 Request 的內容有誤.
   RejectRequest = -3,

   Sent = 0,
   Queuing = 1,
};
inline bool IsSentOrQueuing(SendRequestResult r) {
   return(r >= SendRequestResult::Sent);
}

fon9_WARN_DISABLE_PADDING;
/// \ingroup fmkt
/// 交易連線管理員基底.
/// - 負責尋找可下單的線路送出下單要求.
/// - 負責等候流量管制時間, 時間到解除管制時, 透過 OnNewTradingLineReady() 通知.
class fon9_API TradingLineManager {
   fon9_NON_COPY_NON_MOVE(TradingLineManager);
   /// 可送出下單要求的連線. 流量管制時不移除.
   struct TradingSvrImpl {
      using Lines = std::vector<TradingLine*>;
      using Reqs = std::deque<TradingRequestSP>;
      unsigned LineIndex_{0};
      Lines    Lines_;
      Reqs     ReqQueue_;
   };
   using TradingSvr = MustLock<TradingSvrImpl>;
   TradingSvr  TradingSvr_;

public:
   using Locker = TradingSvr::Locker;

   TradingLineManager() = default;
   virtual ~TradingLineManager();

   /// 當 src 進入可下單狀態時的通知:
   /// - 連線成功後, 進入可下單狀態.
   /// - 從忙碌狀態, 進入可下單狀態.
   void OnTradingLineReady(TradingLine& src);

   /// 當 src 斷線時的通知.
   /// 不包含: 流量管制, 線路忙碌.
   void OnTradingLineBroken(TradingLine& src);

   SendRequestResult SendRequest(TradingRequest& req, const Locker* tsvr = nullptr) {
      if (tsvr)
         return this->SendRequest(req, *tsvr);
      return this->SendRequest(req, Locker{this->TradingSvr_});
   }

protected:
   /// 當有新的可交易線路時的通知, 預設: 送出 queue 的 req.
   /// 若 src == nullptr 表示流量管制解除.
   virtual void OnNewTradingLineReady(TradingLine* src, const Locker& tsvr);

private:
   SendRequestResult SendRequest(TradingRequest& req, const Locker& tsvr);
   void ClearReqQueue(Locker&& tsvr, StrView cause);

   struct FlowControlTimer : public DataMemberTimer {
      fon9_NON_COPY_NON_MOVE(FlowControlTimer);
      FlowControlTimer() = default;
      virtual void EmitOnTimer(TimeStamp now) override;
   };
   FlowControlTimer FlowControlTimer_;
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_fmkt_Trading_hpp__
