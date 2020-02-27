// \file fon9/rc/RcSeedVisitorClientReq.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSeedVisitorClientReq_hpp__
#define __fon9_rc_RcSeedVisitorClientReq_hpp__
#include "fon9/rc/RcSeedVisitorClient.hpp"

namespace fon9 { namespace rc {

constexpr f9sv_Result ToResultC(seed::OpResult res) {
   return static_cast<f9sv_Result>(cast_to_underlying(res));
}

/// 若 IsSubscribeStream(tabNameAndStreamArgs) 則表示為訂閱 Stream 使用的 TabName 格式:
/// - "$TabName:StreamDecoderName:Args";
///   返回後 tabNameAndStreamArgs = "StreamDecoderName:Args";
/// - 否則為一般的 TabName;
///   返回後 tabNameAndStreamArgs.IsNull();
seed::Tab* RcSvGetTabStreamArgs(seed::Layout& layout, StrView& tabNameAndStreamArgs);
StrView RcSvGetStreamDecoderName(StrView tabNameAndStreamArgs);

struct RcSvClientRequest {
   fon9_NON_COPY_NON_MOVE(RcSvClientRequest);
   RcClientSession&                 Session_;
   RcSeedVisitorClientNote* const   Note_;
   seed::AclPathParser              Parser_;
   const StrView                    OrigTreePath_;
   const StrView                    OrigTabName_;
   const StrView                    OrigSeedKey_;
   f9sv_TabSize                     TabIndex_;
   // 當 TabName_ == "*" 若有任一 seed 正在查詢或訂閱, 則返回失敗.
   // 此處記錄正在查訊或訂閱的 seed 的 TabIndex.
   f9sv_TabSize                        DupTabIndex_{kTabAll};
   /// 在 this->Tree_ 有 Layout_ 的情況下, 若 IsSubscribeStream(this->OrigTabName_)
   /// - 則從 OrigTabName_ = "$TabName:StreamDecoderName:Args";
   ///   提取出 this->StreamArgs_ = "StreamDecoderName:Args";
   /// - 否則 this->OrigTabName_ 為一般 TabName,
   ///   此時 this->StreamArgs_.IsNull();
   StrView                    StreamArgs_;
   // for log.
   const StrView              FuncName_;
   bool                       IsNeedsLogInfo_;
   // 若有將 req 放到 TreeRec::PendingReqs_; 則會將此處設為 SvFunc::Empty;
   SvFunc                     SvFunc_;
   char                       Padding___[2];
   f9sv_SubrIndex             SubrIndex_{kSubrIndexNull};
   const f9sv_ReportHandler   Handler_;
   StrView                    ExtMsg_;
   svc::TreeRec*              Tree_{};

   RcSvClientRequest(RcClientSession& ses, bool isNeedsLogInfo,
                     StrView funcName, SvFuncCode fc,
                     const f9sv_SeedName& seedName, f9sv_ReportHandler handler);
   RcSvClientRequest(RcClientSession& ses, bool isNeedsLogInfo,
                     StrView funcName, SvFunc fc,
                     RcSeedVisitorClientNote& note,
                     svc::TreeRec& tree,
                     const svc::PendingReq& req);

   /// 解構時, 如果有需要, 則記錄 log.
   virtual ~RcSvClientRequest();
   void FlushLog();

   using TreeLocker = svc::TreeLocker;
   using PodRec = svc::PodRec;

   /// 檢查 retval 是否允許送出要求, 或直接觸發失敗通知(this->Handler_).
   /// retval 的來源: this->FetchSeedSt(); 或 this->CheckSubrReqSt(); 或 this->CheckReqSt();
   f9sv_Result FinalCheckSendRequest(f9sv_Result retval);

   /// - 檢查 Note_;
   /// - 正規化 OrigTreePath_;
   /// - 檢查 Layout
   ///   - 如果有 Layout: 透過 this->FetchSeedSt(tree) 設定 SeetSt;
   ///   - 如果沒 Layout
   ///     - 則 this 放入 TreeRec::PendingReqs_;
   ///     - 清除 this->SvFunc_; 讓 this->FinalCheckSendRequest(); 可判斷: 已放入 Pending, 不用 Send;
   f9sv_Result CheckReqSt();
   f9sv_Result CheckSubrReqSt();

   /// 在已有 Layout 的情況下.
   /// - 檢查 TabName_, TabIndex_ 是否正確.
   /// - 必要時建立 pod 及 seed;
   /// - 設定 seed 狀態;
   f9sv_Result FetchSeedSt(const TreeLocker& locker);

private:
   /// 送出要求.
   void SendRequest();

   /// 在設定 Seed::SvFunc_ 之前, 或放到 Pending 之前(此時 pod=nullptr): 檢查是否允許.
   /// 例如: 是否流量管制? 是否超過訂閱數量?
   /// - retval != f9sv_Result_NoError 表示不允許此次要求.
   /// - 預設返回 f9sv_Result_NoError;
   virtual f9sv_Result OnBeforeAssignTo(const TreeLocker& locker, const PodRec* pod);

   /// 檢查 pod.Seeds_[tabidx]->SvFunc_ 是否允許查詢或訂閱.
   f9sv_Result CheckSeedSt(const PodRec& pod, unsigned tabidx) {
      return this->CheckCurrSt(pod.Seeds_[tabidx]->SvFunc_);
   }
   f9sv_Result CheckCurrSt(SvFunc currSt);

   /// 設定 Seed 的 SvFunc_ 及 Handler_;
   void AssignToSeed(svc::SeedRec& seed) {
      seed.SvFunc_  = this->SvFunc_;
      seed.Handler_ = this->Handler_;
   }

   f9sv_Result NoSubscribeError();
};

} } // namespaces
#endif//__fon9_rc_RcSeedVisitorClientReq_hpp__
