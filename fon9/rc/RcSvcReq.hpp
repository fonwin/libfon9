// \file fon9/rc/RcSvcReq.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSvcReq_hpp__
#define __fon9_rc_RcSvcReq_hpp__
#include "fon9/rc/RcSvc.hpp"

namespace fon9 { namespace rc {

class RcClientSession;
class RcSeedVisitorClientNote;

constexpr f9sv_Result ToResultC(seed::OpResult res) {
   return static_cast<f9sv_Result>(cast_to_underlying(res));
}

/// 若 IsSubscribeStream(tabNameAndStreamArgs) 則表示為訂閱 Stream 使用的 TabName 格式:
/// - "$TabName:StreamDecoderName:Args";
///   返回後 tabNameAndStreamArgs = "StreamDecoderName:Args";
/// - 否則為一般的 TabName;
///   返回後 tabNameAndStreamArgs.IsNull();
seed::Tab* RcSvcGetTabStreamArgs(seed::Layout& layout, StrView& tabNameAndStreamArgs);
StrView RcSvcGetStreamDecoderName(StrView tabNameAndStreamArgs);

class RcSvcReq {
   fon9_NON_COPY_NON_MOVE(RcSvcReq);
public:
   f9sv_ReportHandler       const   Handler_;
   RcSeedVisitorClientNote* const   Note_;

   RcClientSession&  Session_;
   const CharVector  OrigTreePath_;
   const CharVector  OrigTabName_;
   const CharVector  OrigSeedKey_;
   const SvFuncCode  OrigSvFunc_;

   // -----
   /// 一般功能:非訂閱類的(Query,GridView,Remove...).
   /// - 檢查 Note_, Session_ 的狀態;
   /// - 正規化 OrigTreePath_;
   /// - 如果有 Layout: 檢查 Seed or SubrRec 是否允許此要求;
   /// - 如果沒 Layout: 將 req 放入 TreeRec::PendingReqs_; 若 req 為第一筆, 則送出.
   static f9sv_Result Run(RcSvcReqSP req);

   /// 取得了 layout 之後, tree.PendingReqs_[0] 填入 pod(seed), 然後送出剩餘的要求.
   static void RunPendingReqs(const svc::TreeLocker& maplk, svc::TreeRec& tree, bool isTabNotFound);

   // -----
   RcSvcReq(RcClientSession& ses,
            f9rc_ClientLogFlag logFlag,
            StrView funcName,
            SvFuncCode fc,
            const f9sv_SeedName& seedName,
            f9sv_ReportHandler handler);

   /// 解構時, 如果有需要, 則在解構時記錄 log.
   virtual ~RcSvcReq();

   /// 寫入 log, 並阻擋之後的寫入要求.
   void FlushLogFinal() {
      this->FlushLogKeep();
      this->IsNeedsLogInfo_ = false;
   }
   void SetExLogMsg(const StrView& msg) {
      this->ExLogMsg_ = msg;
   }

   /// 寫入 log, 並保留之後的寫入要求.
   /// 通常在放入 PendingRecs 之前使用.
   void FlushLogKeep();

   bool IsSameSeed(const RcSvcReq& preq) const {
      return (this->TabIndex_ == preq.TabIndex_
              && this->OrigTabName_ == preq.OrigTabName_
              && this->OrigSeedKey_ == preq.OrigSeedKey_);
   }

   virtual void RejectPending(const svc::TreeLocker& maplk);

   f9sv_Result ReportResult(f9sv_Result st);

   void AssignRptTabSeed(f9sv_ClientReport& rpt) {
      rpt.UserData_ = this->Handler_.UserData_;
      rpt.SeedKey_  = ToStrView(this->OrigSeedKey_).ToCStrView();
      const_cast<f9sv_Tab*>(rpt.Tab_)->Named_.Name_  = ToStrView(this->OrigTabName_).ToCStrView();
      const_cast<f9sv_Tab*>(rpt.Tab_)->Named_.Index_ = static_cast<int>(this->TabIndex_);
   }

protected:
   virtual void AddLogMsg(RevBufferList& rbuf);

   /// 當首次收到 Layout 時, PendingReqs.front() 的回報.
   /// - pod == nullptr: 表示 not_found_tab; 此時應直接回報 Handler.
   /// - pod != nullptr: 表示正確回報, 此時應將 Handler 填入 seed or subr;
   virtual void OnReportPending(const svc::TreeLocker& maplk, const svc::PodRec* pod) = 0;

   /// 資料正規化之後, 由衍生者接力檢查, 其他資料是否正確?
   /// 此時 this->Tree_ 尚未準備好.
   /// 預設直接返回: f9sv_Result_NoError;
   virtual f9sv_Result CheckOnAfterNormalized();

   /// 確定有 Layout, 且沒有 PendingReqs, 檢查是否允許送出要求?
   /// - 預設:
   ///   - 檢查 TabIndex_, OrigTabName_ 是否正確.
   ///   - 然後呼叫衍生者的 CheckFinal() 最後檢查;
   /// - 若返回 f9sv_Result_NoError, 接著就會解鎖 maplk, 然後呼叫 this->SendRequest();
   f9sv_Result CheckOnBeforeSend(const svc::TreeLocker& maplk);
   virtual f9sv_Result CheckFinal(const svc::TreeLocker& maplk, const svc::PodRec* pod) = 0;

   /// 檢查 this->Tree_->PendingReqs_ 是否允許此次要求?
   virtual f9sv_Result CheckPendingReqs(const svc::TreeLocker& maplk) = 0;

   /// 預設打包: TreePath, SeedKey, TabName or TabIndex;
   virtual void PackRequest(RevBufferList& rbuf);
   void SendRequest();

   /// 若有要求 server 傳送 layout = SvFuncFlag::Layout + this->OrigSvFunc_;
   /// 否則 = this->OrigSvFunc_;
   SvFunc   CurrSvFunc_;
   bool     InFlowControl_{};
   char     Padding3__[3];

   // for log.
   bool  IsNeedsLogInfo_;
   enum class PendingSt : char {
      None = '\0',
      PreCheck = 'c',
      Pushed = 'p',
      PopRun = 'r',
      Reject = 'j',
      AckPod = 'a',
      AckTabNotFound = 'x',
   };
   PendingSt      PendingSt_{};
   const StrView  FuncName_;
   StrView        ExLogMsg_;

   CharVector     NormalizedPath_;
   svc::TreeRec*  Tree_{};

   /// 建構時:
   /// - 若 tabName==nullptr 使用 seedName.TabIndex_;
   /// - 若 tabName==""      使用 0;
   /// - 若 tabName=="*"     使用 kTabAll;
   /// - 否則在送出前設定 = layout->GetTab(tabName);
   f9sv_TabSize   TabIndex_;

   // 當 TabName_ == "*", 若該 pod 內, 有任一 seed 正在查詢或訂閱, 則返回失敗.
   // 此處記錄正在查訊或訂閱的 seed 的 TabIndex.
   f9sv_TabSize   DupTabIndex_{kTabAll};

   /// 在 this->Tree_ 有 Layout_ 的情況下, 若 IsSubscribeStream(this->OrigTabName_)
   /// - 則從 OrigTabName_ = "$TabName:StreamDecoderName:Args";
   ///   提取出 this->SubrStreamArgs_  = "StreamDecoderName:Args";
   /// - 否則 this->OrigTabName_ 為一般 TabName,
   ///   此時 this->SubrStreamArgs_.IsNull();
   StrView        SubrStreamArgs_;
};
using RcSvcReqSP = std::unique_ptr<RcSvcReq>;
//--------------------------------------------------------------------------//
class RcSvcReqSeed : public RcSvcReq {
   fon9_NON_COPY_NON_MOVE(RcSvcReqSeed);
   using base = RcSvcReq;
protected:
   f9sv_Result CheckPendingReqs(const svc::TreeLocker& maplk) override;
   f9sv_Result CheckFinal(const svc::TreeLocker& maplk, const svc::PodRec* pod) override;

   /// 由於每個 seed 只保留一個 Handler, 所以上次要求尚未完成回報前, 要拒絕新的要求!
   f9sv_Result CheckSeedWorkingSt(SvFuncCode fcWorking);
   /// 檢查 pod.Seeds_[tabidx] 是否允許執行此要求.
   f9sv_Result CheckSeedWorkingSt(const svc::PodRec& pod, f9sv_TabSize tabidx) {
      return this->CheckSeedWorkingSt(*pod.Seeds_[tabidx]);
   }
   f9sv_Result CheckSeedWorkingSt(const svc::SeedRec& seed) {
      return this->CheckSeedWorkingSt(seed.WorkingSvFunc_);
   }

   /// 設定 Seed 的 SvFunc_ 及 Handler_;
   void AssignWorkStToSeed(const svc::PodRec& pod, f9sv_TabSize tabidx) {
      this->AssignWorkStToSeed(*pod.Seeds_[tabidx]);
   }
   void AssignWorkStToSeed(svc::SeedRec& seed) {
      assert(seed.WorkingSvFunc_ == SvFuncCode::Empty && seed.SeedHandler_.FnOnReport_ == nullptr);
      seed.WorkingSvFunc_ = this->OrigSvFunc_;
      seed.SeedHandler_ = this->Handler_;
   }

   void OnReportPending(const svc::TreeLocker& maplk, const svc::PodRec* pod) override;
public:
   using base::base;
};

class RcSvcReqGridView : public RcSvcReqSeed {
   fon9_NON_COPY_NON_MOVE(RcSvcReqGridView);
   using base = RcSvcReqSeed;
   const int16_t  GridViewReqMaxRowCount_;
   char           Padding___[6];
   void AddLogMsg(RevBufferList& rbuf) override;
   void PackRequest(RevBufferList& rbuf) override;
public:
   template <class... ArgsT>
   RcSvcReqGridView(int16_t gvReqMaxRowCount, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , GridViewReqMaxRowCount_{gvReqMaxRowCount} {
   }
   ~RcSvcReqGridView();
};

/// SvFuncCode::Write:   提供 FieldName=Value|...
/// SvFuncCode::Command: 提供 要執行的指令及參數.
class RcSvcReqExStrArg : public RcSvcReqSeed {
   fon9_NON_COPY_NON_MOVE(RcSvcReqExStrArg);
   using base = RcSvcReqSeed;
   const CharVector  ExStrArg_;
   void AddLogMsg(RevBufferList& rbuf) override;
   void PackRequest(RevBufferList& rbuf) override;
public:
   template <class... ArgsT>
   RcSvcReqExStrArg(StrView exStrArg, ArgsT&&... args)
      : RcSvcReqSeed(std::forward<ArgsT>(args)...)
      , ExStrArg_{StrTrim(&exStrArg)} {
   }
   ~RcSvcReqExStrArg();
};
//--------------------------------------------------------------------------//
class RcSvcReqSubr : public RcSvcReq {
   fon9_NON_COPY_NON_MOVE(RcSvcReqSubr);
   using base = RcSvcReq;
protected:
   /// 訂閱編號, 不論是否有立即送出訂閱要求, 或放到 PendingReqs,
   /// 只要訂閱要求沒問題, 就會制訂編號.
   f9sv_SubrIndex SubrIndex_{kSubrIndexNull};
   char           Padding___[4];

   void AddLogMsg(RevBufferList& rbuf) override;
   void PackRequest(RevBufferList& rbuf) override;
   void OnReportPending(const svc::TreeLocker& maplk, const svc::PodRec* pod) override;
   void OnSubrPodReady(const svc::TreeLocker& maplk, const svc::PodRec& pod);

   /// - 必須指定 this->TabIndex_, 不可為 kTabAll;
   /// - 若有指定 StreamDecoderName, 則 StreamDecoder 必須存在於 RcSvStreamDecoderPark;
   f9sv_Result CheckOnAfterNormalized() override;
   f9sv_Result CheckPendingReqs(const svc::TreeLocker& maplk) override;
   f9sv_Result CheckFinal(const svc::TreeLocker& maplk, const svc::PodRec* pod) override;
public:
   using base::base;
   ~RcSvcReqSubr();

   f9sv_SubrIndex SubrIndex() const {
      return this->SubrIndex_;
   }

   void RejectPending(const svc::TreeLocker& maplk) override;
};

class RcSvcReqUnsubr : public RcSvcReqSubr {
   fon9_NON_COPY_NON_MOVE(RcSvcReqUnsubr);
   using base = RcSvcReqSubr;
protected:
   void PackRequest(RevBufferList& rbuf) override;
   f9sv_Result Error_NoSubscribe();
   f9sv_Result CheckPendingReqs(const svc::TreeLocker& maplk) override;
   f9sv_Result CheckFinal(const svc::TreeLocker& maplk, const svc::PodRec* pod) override;
public:
   using base::base;
};

} } // namespaces
#endif//__fon9_rc_RcSvcReq_hpp__
