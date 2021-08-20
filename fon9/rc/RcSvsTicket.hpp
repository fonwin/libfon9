// \file fon9/rc/RcSvsTicket.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSvsTicket_hpp__
#define __fon9_rc_RcSvsTicket_hpp__
#include "fon9/rc/RcSession.hpp"
#include "fon9/rc/RcSeedVisitor.hpp"
#include "fon9/seed/SeedVisitor.hpp"
#include "fon9/io/Device.hpp"

namespace fon9 { namespace rc {

class fon9_API RcSeedVisitorServerNote;

//--------------------------------------------------------------------------//
struct RcSvsSubr : public intrusive_ref_counter<RcSvsSubr> {
   fon9_NON_COPY_NON_MOVE(RcSvsSubr);

   RcSvsSubr(f9sv_SubrIndex subrIndex, StrView seedKey, io::DeviceSP dev)
      : SubrIndex_{subrIndex}
      , SeedKey_{seedKey}
      , Device_{std::move(dev)} {
   }
   seed::OpResult Unsubscribe(seed::SubscribableOp* opSubr, seed::Tab& tab);

   const f9sv_SubrIndex SubrIndex_;
   f9sv_TabSize         TabIndex_;
   SvFuncCode           SvFunc_{SvFuncCode::Subscribe};
   bool                 IsStream_{false};
   /// 當收到 NotifyKind = ParentSeedClear, 或 訂閱 seed 收到 PodRemoved, SeedRemoved 時;
   bool                 IsSubjectClosed_{false};
   char                 Padding___[1];
   seed::TreeSP         Tree_;
   SubConn              SubConn_{};
   const CharVector     SeedKey_;
   const io::DeviceSP   Device_;
};
using RcSvsSubrSP = intrusive_ptr<RcSvsSubr>;
//--------------------------------------------------------------------------//
struct RcSvsUnsubRunnerSP : public RcSvsSubrSP {
   using base = RcSvsSubrSP;
   RcSvsUnsubRunnerSP(RcSvsSubrSP preg) : base{std::move(preg)} {
   }
   void operator()(const seed::PodOpResult& res, seed::PodOp* pod) {
      this->Unsubscribe(*res.Sender_, res.OpResult_, pod);
   }
   void operator()(const seed::TreeOpResult& res, seed::TreeOp* opTree);
   void Unsubscribe(seed::Tree& tree, seed::OpResult opres, seed::SubscribableOp* opSubr);
};
//--------------------------------------------------------------------------//
/// RcSvc 送來的要求處理基底.
/// - 回報格式,前端共同部分:
///   - Tree not found: OpResult(必定 < OpResult.no_error);
///   - Tab  not found: [AckLayout] + fon9_BitvV_NumberNull;
///   - 各種Req自訂回報: [AckLayout] + 開頭必須為[數字>=0 or 非數字] + 後續內容...
struct RcSvsReq {
   fon9_NON_COPY_NON_MOVE(RcSvsReq);
   RcSvsReq() = default;

   io::DeviceSP   Device_; // 為了確保在 Query 執行期間, device/session 不會死亡.
   seed::LayoutSP AckLayout_;
   SvFunc         SvFunc_;
   bool           IsAllTabs_;
   bool           IsTreeFound_{false};
   char           Padding__[1];
   int32_t        AfterAckLayoutS32_{-1};
   RevBufferList  AckBuffer_{256};

   RcSeedVisitorServerNote& SessionNote() const;

   RcSession& Session() const {
      return *static_cast<RcSession*>(this->Device_->Session_.get());
   }
   void CheckAckLayoutWhenDtor(seed::Tree& tree) {
      this->IsTreeFound_ = true;
      if (HasSvFuncFlag(this->SvFunc_, SvFuncFlag::Layout))
         this->AckLayout_ = tree.LayoutSP_;
   }
   // 若 client 為 PendingReqs: 在收到 layout 後, 處理 PendingReqs 時, 若 tab not found 則會直接觸發事件.
   // 若 client 已有 layout, 則會先判斷 tab 是否存在, 若不存在則不會送出, 會在當場觸發 tab not found. 
   // 所以填入 fon9_BitvV_NumberNull, 然後 client 端在 RcSvcAckBase::Run() 處理.
   void PackAckTabNotFound() {
      RevPutBitv(this->AckBuffer_, fon9_BitvV_NumberNull);
   }
   // 打包 seed 內容: 使用[文字]格式.
   // 解析結果在 RcSeedVisitorClient.cpp: RcSvcNotifySeeds();
   void PackSeedValues(const seed::Tab& tab, const seed::RawRd& rd);
   void PackOpError(seed::OpResult res);
   void SendRcSvAck(const CharVector& origReqKey);
};

template <class SeedTicketRunner>
struct RcSvsTicketRunnerT : public SeedTicketRunner, public RcSvsReq {
   fon9_NON_COPY_NON_MOVE(RcSvsTicketRunnerT);
   using SeedTicketRunner::SeedTicketRunner;
   ~RcSvsTicketRunnerT() {
      this->SendRcSvAck(this->Bookmark_); // Bookmark_ = origReqKey;
   }
   void OnError(seed::OpResult res) override {
      this->PackOpError(res);
   }
};

struct RcSvsTicket {
   RcSvsReq*            Req_;
   seed::TicketRunnerSP Runner_;

   template <class SeedTicketRunner>
   void Assign(RcSvsTicketRunnerT<SeedTicketRunner>* req) {
      this->Req_ = req;
      this->Runner_.reset(req);
   }
};
//--------------------------------------------------------------------------//
/// 處理 RcSvc 送來的: 訂閱要求.
/// 回報格式(共同回報格式之後):
/// - 訂閱失敗: [AckLayout] + fon9_BitvV_Number0 + OpResult.ErrCode(!=0)
/// - 訂閱成功: [AckLayout] + 在 RcSeedVisitorServerNote::OnSubscribeNotify() 打包底下內容:
///   - SubscribeStreamOK:
///     - fon9_BitvV_Number0 + OpResult.no_error(==0) + ByteArray(由Stream自訂格式).
///   - SubscribeOK(底下二擇一):
///     - 無立即資料: fon9_BitvV_Number0 + OpResult.no_error(==0) + 無後續資料.
///     - 有立即資料: 1:tabCount         + OpResult.no_error(==0) + tabidx + PackSeedValues();
struct RcSvsSubscribeRunner : public RcSvsTicketRunnerT<seed::TicketRunnerTree> {
   fon9_NON_COPY_NON_MOVE(RcSvsSubscribeRunner);
   using base = RcSvsTicketRunnerT<seed::TicketRunnerTree>;
   const RcSvReqKey  ReqKey_;
   StrView           SubscribeStreamArgs_{nullptr};
   RcSvsSubscribeRunner(seed::SeedVisitor& visitor, RcSvReqKey&& reqKey);
   void Subscribe(seed::Tree& tree, seed::Tab& tab, seed::SubscribableOp& opSubr);
   void OnFoundTree(seed::TreeOp& opTree) override;
   void OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab) override;
   void OnError(seed::OpResult res) override;
};
//--------------------------------------------------------------------------//
/// 處理 RcSvc 送來的: 查詢要求.
/// 回報格式(共同回報格式之後):
/// - [AckLayout] + {tabidx + [ErrCode or PackSeedValues()]} * N
///   - N = 1:指定Tab; or TabCount:全部Tabs(TabName="*");
struct RcSvsTicketRunnerQuery : public RcSvsTicketRunnerT<seed::TicketRunnerRead> {
   fon9_NON_COPY_NON_MOVE(RcSvsTicketRunnerQuery);
   using base = RcSvsTicketRunnerT<seed::TicketRunnerRead>;
   // 為了在最後的 tab not found 時, 仍能提供 layout, 所以額外提供最後的 tabName;
   // 由 RcSvsTicketRunnerQuery 自行判斷 tab not found.
   CharVector  TabName_;
   RcSvsTicketRunnerQuery(seed::SeedVisitor& visitor, StrView seed, StrView tabName)
      : base{visitor, seed}
      , TabName_{tabName} {
   }
   void OnLastStep(seed::TreeOp& op, StrView keyText, seed::Tab& tab0) override;
   void OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab0) override;
   void ReadSeed(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab);
   void OnReadOp(const seed::SeedOpResult& res, const seed::RawRd* rd) override;
};
//--------------------------------------------------------------------------//
/// 處理 RcSvc 送來的: GridView 要求.
/// 回報格式(共同回報格式之後):
/// - 失敗: [AckLayout] + tabidx + OpResult.ErrCode(!=0);
/// - 成功: [AckLayout] + tabidx + OpResult.no_error(==0)
///                              + rowCount      + containerSize
///                              + distanceBegin + distanceEnd
///                              + GvStr;
struct RcSvsTicketRunnerGridView : public RcSvsTicketRunnerT<seed::TicketRunnerGridView> {
   fon9_NON_COPY_NON_MOVE(RcSvsTicketRunnerGridView);
   using base = RcSvsTicketRunnerT<seed::TicketRunnerGridView>;
   using base::base;
   void OnFoundTree(seed::TreeOp& opTree) override;
   void OnFoundTab(seed::TreeOp& opTree, seed::GridViewRequest& req) override;
   void RowNoToBitv(size_t n);
   void OnGridViewOp(seed::GridViewResult& res) override;
};
//--------------------------------------------------------------------------//
/// 處理 RcSvc 送來的: Write 要求.
/// 回報格式(共同回報格式之後):
/// - [AckLayout] + tabidx + OpResult.ErrCode(!=0)
/// - [AckLayout] + tabidx + OpResult.no_error(==0) + PackSeedValues() + exMsg
struct RcSvsTicketRunnerWrite : public RcSvsTicketRunnerT<seed::TicketRunnerWrite> {
   fon9_NON_COPY_NON_MOVE(RcSvsTicketRunnerWrite);
   using base = RcSvsTicketRunnerT<seed::TicketRunnerWrite>;
   using base::base;
   void OnLastStep(seed::TreeOp& op, StrView keyText, seed::Tab& tab) override;
   void OnWriteOp(const seed::SeedOpResult& res, const seed::RawWr* wr) override;
};
//--------------------------------------------------------------------------//
/// 處理 RcSvc 送來的: Remove 要求.
/// 回報格式(共同回報格式之後):
/// - [AckLayout] + OpResult.removed_seed(==2) + tabidx;
/// - [AckLayout] + OpResult.removed_pod(==1) or OpResult.ErrCode(!=0);
struct RcSvsTicketRunnerRemove : public RcSvsTicketRunnerT<seed::TicketRunnerRemove> {
   fon9_NON_COPY_NON_MOVE(RcSvsTicketRunnerRemove);
   using base = RcSvsTicketRunnerT<seed::TicketRunnerRemove>;
   using base::base;
   void OnBeforeRemove(seed::TreeOp& opTree, StrView keyText, seed::Tab* tab) override;
   void OnAfterRemove(const seed::PodRemoveResult& res) override;
};
//--------------------------------------------------------------------------//
/// 處理 RcSvc 送來的: Command 要求.
/// 回報格式(共同回報格式之後):
/// - 底下二擇一(有執行, 且結果有包含「!= OpResult.no_error」的回覆, 選第二種格式):
///   - [AckLayout] + tabidx + fon9_BitvV_Number0 + 執行結果字串.
///   - [AckLayout] + tabidx + fon9_BitvV_Number0 + OpResult(!=0) + 執行結果字串.
struct RcSvsTicketRunnerCommand : public RcSvsTicketRunnerT<seed::TicketRunnerCommand> {
   fon9_NON_COPY_NON_MOVE(RcSvsTicketRunnerCommand);
   using base = RcSvsTicketRunnerT<seed::TicketRunnerCommand>;
   using base::base;
   void OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab) override;
   void OnSeedCommandResult(const seed::SeedOpResult& res, StrView msg) override;
};

} } // namespaces
#endif//__fon9_rc_RcSvsTicket_hpp__
