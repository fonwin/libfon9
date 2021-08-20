// \file fon9/rc/RcSvcReqSubr.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSvcReq.hpp"
#include "fon9/rc/RcSeedVisitorClient.hpp"

namespace fon9 { namespace rc {

RcSvcReqSubr::~RcSvcReqSubr() {
   this->FlushLogFinal();
}
void RcSvcReqSubr::OnSubrPodReady(const svc::TreeLocker& maplk, const svc::PodRec& pod) {
   svc::SubrRec* subr = maplk->SubrMap_.GetObjPtr(this->SubrIndex_);
   assert(subr != nullptr);
   assert(subr->IsStream_ == seed::IsSubscribeStream(this->OrigTabName_.begin()));
   assert(subr->Tree_     == this->Tree_);
   assert(subr->SeedKey_  == this->OrigSeedKey_);

   // 訂閱前可能還沒有 Layout, 取得 Layout 之後才有正確的 pod 及 TabIndex.
   subr->TabIndex_ = this->TabIndex_;

   // 有可能在取得訂閱結果之前, 就有 [取消訂閱] 的要求, 此時會設定 subr, 因此底下的 assert 可能不成立.
   // assert(subr->SubrSvFunc_ == this->OrigSvFunc_);
   // assert(subr->SubrHandler_.FnOnReport_ == this->Handler_.FnOnReport_);
   // assert(subr->SubrHandler_.UserData_   == this->Handler_.UserData_);

   subr->OnPodReady(pod, this->SubrIndex_);
}
void RcSvcReqSubr::OnReportPending(const svc::TreeLocker& maplk, const svc::PodRec* pod) {
   // auto*  subr = maplk->SubrMap_.GetObjPtr(this->SubrIndex_);
   // assert(subr != nullptr);
   // if (subr->SubrSvFunc_ == SvFuncCode::Unsubscribe) {
   //    在收到訂閱結果之前, 已要求取消訂閱.
   //    若訂閱成功, 則應送出取消訂閱.
   //    若訂閱失敗, 則用 subr->SubrHandler_ 通知結果.
   //    => 在 RcSvcReqUnsubr::CheckPendingReqs() 已強制將 this->Handler_ 換成 RcSvcReqUnsubr 的了!
   //    => 所以底下的回報, 將會直接使用 RcSvcReqUnsubr.Handler_;
   // }
   if (pod) {
      this->SubrStreamArgs_ = ToStrView(this->OrigTabName_);
      if (auto* stab = RcSvcGetTabStreamArgs(*this->Tree_->Layout_, this->SubrStreamArgs_)) {
         this->TabIndex_ = static_cast<f9sv_TabSize>(stab->GetIndex());
         this->OnSubrPodReady(maplk, *pod);
         return;
      }
   }
   this->ExLogMsg_ = "|err=Ack:Tab not found";
   this->ReportResult(f9sv_Result_NotFoundTab);
   maplk->SubrMap_.RemoveObjPtr(this->SubrIndex_, nullptr);
}
void RcSvcReqSubr::AddLogMsg(RevBufferList& rbuf) {
   if (this->SubrIndex_ != kSubrIndexNull)
      RevPrint(rbuf, "|subrIndex=", this->SubrIndex_);
}
void RcSvcReqSubr::PackRequest(RevBufferList& rbuf) {
   ToBitv(rbuf, this->SubrIndex_);
   base::PackRequest(rbuf);
}
f9sv_Result RcSvcReqSubr::CheckOnAfterNormalized() {
   if (this->TabIndex_ == kTabAll)
      return f9sv_Result_NotFoundTab;
   StrView streamDecoderName = RcSvcGetStreamDecoderName(ToStrView(this->OrigTabName_));
   if (!streamDecoderName.IsNull()) {
      if (RcSvStreamDecoderPark::Register(streamDecoderName, nullptr) == nullptr) {
         // StreamDecoder 不存在.
         return ToResultC(seed::OpResult::bad_subscribe_stream_args);
      }
   }
   return f9sv_Result_NoError;
}
f9sv_Result RcSvcReqSubr::CheckPendingReqs(const svc::TreeLocker& maplk) {
   (void)maplk;
   assert (this->OrigSvFunc_ == SvFuncCode::Subscribe);
   const auto iend = this->Tree_->PendingReqs_.end();
   const auto ibeg = this->Tree_->PendingReqs_.begin();
   for (auto i = ibeg; i != iend; ++i) {
      const RcSvcReq& preq = *(i->get());
      // -----
      // preq 只有在 ibeg 為訂閱要求, 且 preq.IsSameSeed(ibeg) 時, preq 才有可能為 Unsubscribe;
      // => 因為非 ibeg 的訂閱, 如果有取消訂閱, 會直接從 PendingReqs 移除.
      // => 所以這裡不用判斷 preq 是否為 Unsubscribe;
      // -----
      // 若已有正在處理的訂閱, 則此次的訂閱要求失敗.
      if (preq.OrigSvFunc_ == SvFuncCode::Subscribe && this->IsSameSeed(preq)) {
         if (i == ibeg) {
            assert(dynamic_cast<const RcSvcReqSubr*>(&preq) != nullptr);
            auto*  subr = maplk->SubrMap_.GetObjPtr(static_cast<const RcSvcReqSubr*>(&preq)->SubrIndex_);
            assert(subr != nullptr);
            if (subr->SubrSvFunc_ == SvFuncCode::Unsubscribe)
               return f9sv_Result_InUnsubscribe;
         }
         return f9sv_Result_InSubscribe;
      }
   }
   return f9sv_Result_NoError;
}
f9sv_Result RcSvcReqSubr::CheckFinal(const svc::TreeLocker& maplk, const svc::PodRec* pod) {
   assert(this->OrigSvFunc_ == SvFuncCode::Subscribe);
   // -----
   if (this->SubrIndex_ != kSubrIndexNull) {
      // 已分配 SubrIndex_: 來自 RcSvcReq::RunPendingReqs();
      assert(pod != nullptr && this->PendingSt_ == PendingSt::PopRun);
      this->OnSubrPodReady(maplk, *pod);
      return f9sv_Result_NoError;
   }
   // ----- 檢查是否正在訂閱 or 正在取消.
   svc::SubrRec*  subr;
   if (pod) {
      assert(this->Tree_ && this->TabIndex_ < this->Tree_->LayoutC_.TabCount_);
      if (this->TabIndex_ >= this->Tree_->LayoutC_.TabCount_)
         return f9sv_Result_NotFoundTab;
      auto* seed = pod->Seeds_[this->TabIndex_].get();
      if (seed->SubrIndex_ != kSubrIndexNull) {
         subr = maplk->SubrMap_.GetObjPtr(seed->SubrIndex_);
         if (subr->SubrSvFunc_ == SvFuncCode::Unsubscribe) {
            this->ExLogMsg_ = "|InUnsubscribe";
            return f9sv_Result_InUnsubscribe;
         }
         assert(subr->SubrSvFunc_ == SvFuncCode::Subscribe);
         this->ExLogMsg_ = "|InUnsubscribe";
         return f9sv_Result_InSubscribe;
      }
   }
   // ----- 檢查並分配訂閱序號.
   auto maxSubrCount = this->Note_->MaxSubrCount();
   if (maxSubrCount > 0) {
      if (maplk->SubrMap_.Size() >= maxSubrCount) {
         this->ExLogMsg_ = "|err=OverMaxSubrCount";
         return f9sv_Result_OverMaxSubrCount;
      }
   }
   this->SubrIndex_ = static_cast<f9sv_SubrIndex>(maplk->SubrMap_.Alloc());
   subr = maplk->SubrMap_.GetObjPtr(this->SubrIndex_);
   subr->IsStream_    = seed::IsSubscribeStream(this->OrigTabName_.begin());
   subr->SubrSvFunc_  = this->OrigSvFunc_;
   subr->Tree_        = this->Tree_;
   subr->TabIndex_    = this->TabIndex_;
   subr->SeedKey_     = this->OrigSeedKey_;
   subr->SubrHandler_ = this->Handler_;
   if (pod == nullptr)
      subr->Seeds_ = nullptr;
   else
      subr->OnPodReady(*pod, this->SubrIndex_);
   return f9sv_Result_NoError;
}
void RcSvcReqSubr::RejectPending(const svc::TreeLocker& maplk) {
   maplk->SubrMap_.RemoveObjPtr(this->SubrIndex_, nullptr);
   base::RejectPending(maplk);
}
//--------------------------------------------------------------------------//
void RcSvcReqUnsubr::PackRequest(RevBufferList& rbuf) {
   // 取消訂閱, Server 端, 只需要 SubrIndex 即可.
   // 所以不用呼叫 base::PackRequest();
   ToBitv(rbuf, this->SubrIndex_);
}
f9sv_Result RcSvcReqUnsubr::Error_NoSubscribe() {
   this->ExLogMsg_ = "|info=NoSubscribe.";
   return ToResultC(seed::OpResult::no_subscribe);
}
f9sv_Result RcSvcReqUnsubr::CheckPendingReqs(const svc::TreeLocker& maplk) {
   assert (this->OrigSvFunc_ == SvFuncCode::Unsubscribe);
   if (this->Tree_->PendingReqs_.empty())
      return this->Error_NoSubscribe();
   const auto  iend = this->Tree_->PendingReqs_.end();
   auto        i = this->Tree_->PendingReqs_.begin();
   RcSvcReq*   preq = i->get();
   ++i;
   if (preq->OrigSvFunc_ == SvFuncCode::Subscribe && this->IsSameSeed(*preq)) {
      // 訂閱要求已送出, 必須等候訂閱完成, 才能送出取消訂閱.
      // 此時:
      //    1. SubrRec 標記取消中.
      //    2. 通知 Unsubscribing;
      assert(dynamic_cast<const RcSvcReqSubr*>(preq));
      this->SubrIndex_ = static_cast<const RcSvcReqSubr*>(preq)->SubrIndex();
      auto* subr = maplk->SubrMap_.GetObjPtr(this->SubrIndex_);
      assert(subr != nullptr);
      if (subr->SubrSvFunc_ == SvFuncCode::Unsubscribe) {
         this->ExLogMsg_ = "|info=InUnsubscribe";
         return f9sv_Result_InUnsubscribe;
      }
      subr->SubrSvFunc_ = SvFuncCode::Unsubscribe;
      subr->SubrHandler_ = this->Handler_;
      preq->SetExLogMsg(this->ExLogMsg_ = "|info=Send.Unsubscribe");
      this->FlushLogFinal();
      preq->ReportResult(f9sv_Result_Unsubscribing);
      *const_cast<f9sv_ReportHandler*>(&preq->Handler_) = this->Handler_;
      return f9sv_Result_NoError;
   }
   for (; i != iend; ++i) {
      preq = i->get();
      if (preq->OrigSvFunc_ == SvFuncCode::Subscribe && this->IsSameSeed(*preq)) {
         // 尚未送出的訂閱要求 => 直接取消.
         assert(dynamic_cast<const RcSvcReqSubr*>(preq));
         this->SubrIndex_ = static_cast<const RcSvcReqSubr*>(preq)->SubrIndex();
         // 先記錄 this 的 log, 才能表現出 req 處理的順序.
         preq->SetExLogMsg(this->ExLogMsg_ = "|info=Internal.Unsubscribed");
         this->FlushLogFinal();
         preq->ReportResult(f9sv_Result_Unsubscribing);
         maplk->SubrMap_.RemoveObjPtr(this->SubrIndex_, nullptr);
         this->Tree_->PendingReqs_.erase(i);
         return f9sv_Result_Unsubscribed;
      }
   }
   return this->Error_NoSubscribe();
}
f9sv_Result RcSvcReqUnsubr::CheckFinal(const svc::TreeLocker& maplk, const svc::PodRec* pod) {
   assert(this->OrigSvFunc_ == SvFuncCode::Unsubscribe);
   if (pod == nullptr)
      return f9sv_Result_NoError;

   assert(this->TabIndex_ < this->Tree_->LayoutC_.TabCount_);
   auto* seed = pod->Seeds_[this->TabIndex_].get();
   if (seed->SubrIndex_ == kSubrIndexNull)
      return this->Error_NoSubscribe();

   auto* subr = maplk->SubrMap_.GetObjPtr(this->SubrIndex_ = seed->SubrIndex_);
   assert(subr != nullptr);
   assert(subr->TabIndex_ == this->TabIndex_);
   if (subr->SubrSvFunc_ == SvFuncCode::Unsubscribe) {
      this->ExLogMsg_ = "|InUnsubscribe";
      return f9sv_Result_InUnsubscribe;
   }
   assert(subr->SubrSvFunc_ == SvFuncCode::Subscribe);

   this->FlushLogFinal();
   subr->SubrSvFunc_ = SvFuncCode::Unsubscribe;
   if (subr->SubrHandler_.FnOnReport_) {
      f9sv_ClientReport rpt;
      ZeroStruct(rpt);
      rpt.ResultCode_ = f9sv_Result_Unsubscribing;
      rpt.UserData_   = subr->SubrHandler_.UserData_;
      rpt.TreePath_   = ToStrView(this->Tree_->TreePath_).ToCStrView();
      rpt.SeedKey_    = ToStrView(this->OrigSeedKey_).ToCStrView();
      rpt.Layout_     = &this->Tree_->LayoutC_;
      rpt.Tab_        = &this->Tree_->LayoutC_.TabArray_[this->TabIndex_];
      subr->SubrHandler_.FnOnReport_(&this->Session_, &rpt);
   }
   subr->SubrHandler_ = this->Handler_;
   return f9sv_Result_NoError;
}

} } // namespaces
