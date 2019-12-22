// \file fon9/rc/RcSeedVisitorClientReq.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSeedVisitorClientReq_hpp__
#define __fon9_rc_RcSeedVisitorClientReq_hpp__
#include "fon9/rc/RcSeedVisitorClient.hpp"

namespace fon9 { namespace rc {

constexpr f9sv_Result ToResultC(seed::OpResult res) {
   return static_cast<f9sv_Result>(cast_to_underlying(res));
}

struct RcSvClientRequest {
   fon9_NON_COPY_NON_MOVE(RcSvClientRequest);
   RcClientSession&                    Session_;
   RcSeedVisitorClientNote* const      Note_;
   seed::AclPathParser                 Parser_;
   const StrView                       OrigTreePath_;
   const StrView                       OrigTabName_;
   const StrView                       OrigSeedKey_;
   f9sv_TabSize                        TabIndex_;
   // 當 TabName_ == "*" 若有任一 seed 正在查詢或訂閱, 則返回失敗.
   // 此處記錄正在查訊或訂閱的 seed 的 TabIndex.
   f9sv_TabSize                        DupTabIndex_{kTabAll};
   // for log.
   const StrView                       FuncName_;
   bool                                IsLogInfo_;
   // 若有將 req 放到 TreeRec::PendingReqs_; 則會將此處設為 SvFuncCode{};
   SvFuncCode                          SvFunc_;
   char                                Padding___[6];
   const f9sv_ReportHandler            Handler_;
   StrView                             ExtMsg_;

   RcSvClientRequest(RcClientSession& ses, StrView funcName, SvFuncCode svFuncCode, bool isLogInfo,
                     const f9sv_SeedName& seedName, f9sv_ReportHandler handler);
   RcSvClientRequest(RcClientSession& ses, StrView funcName, SvFuncCode svFuncCode, bool isLogInfo,
                     RcSeedVisitorClientNote& note, StrView treePath,
                     const RcSeedVisitorClientNote::PendingReq& req);

   /// 解構時, 如果有需要, 則記錄 log.
   virtual ~RcSvClientRequest();

   /// 檢查 pod.Seeds_[tabidx]->CurrSt_ 是否允許查詢或訂閱.
   f9sv_Result CheckSeedSt(RcSeedVisitorClientNote::PodRec& pod, unsigned tabidx);
   /// 設定 Seed 的 CurrSt_ 及 Handler_;
   void SetupSeedSt(RcSeedVisitorClientNote::SeedRec& seed) {
      seed.CurrSt_ = this->SvFunc_;
      seed.Handler_ = this->Handler_;
   }
   /// 在設定 Seed::CurrSt_ 之前, 檢查是否允許, 例如: 是否流量管制?
   /// - retval != f9sv_Result_NoError 表示不允許此次要求.
   /// - 預設返回 f9sv_Result_NoError;
   virtual f9sv_Result OnBeforeSetupSeedSt();

   /// 送出查詢要求.
   void SendQuery();
   /// 檢查 retval 是否允許送出查詢要求, 或直接觸發失敗通知(this->Handler_).
   f9sv_Result FinalCheckSendQuery(f9sv_Result retval);

   /// - 檢查 Note_;
   /// - 正規化 OrigTreePath_;
   /// - 檢查 Layout: 如果沒有 Layout, 則放入 TreeRec::PendingReqs_; 並清除 this->SvFunc_;
   /// - 透過 this->FetchSeedSt(tree) 設定 SeetSt;
   f9sv_Result CheckApiSeedName();
   /// 在已有 Layout 的情況下.
   /// - 檢查 TabName_; TabIndex_;
   /// - 必要時建立 pod 及 seed;
   /// - 設定 seed 狀態;
   f9sv_Result FetchSeedSt(RcSeedVisitorClientNote::TreeRec& tree);
};

} } // namespaces
#endif//__fon9_rc_RcSeedVisitorClientReq_hpp__
