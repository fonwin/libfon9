// \file fon9/rc/RcSeedVisitorClientReq.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSeedVisitorClientReq.hpp"

namespace fon9 { namespace rc {

RcSvClientRequest::RcSvClientRequest(RcClientSession& ses, bool isNeedsLogInfo,
                                     StrView funcName, SvFuncCode fc,
                                     const f9sv_SeedName& seedName, f9sv_ReportHandler handler)
   : Session_(ses)
   , Note_{static_cast<RcSeedVisitorClientNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor))}
   , OrigTreePath_{StrView_cstr(seedName.TreePath_)}
   , OrigTabName_{StrView_cstr(seedName.TabName_)}
   , OrigSeedKey_{StrView_cstr(seedName.SeedKey_)}
   , TabIndex_(seedName.TabName_ == nullptr ? seedName.TabIndex_
               : IsTabAll(seedName.TabName_) ? kTabAll
               : 0) // tabName=="" 使用 0; 或 layout->GetTab(tabName);
   , FuncName_{funcName}
   , IsNeedsLogInfo_{isNeedsLogInfo}
   , SvFunc_{ToSvFunc(fc)}
   , Handler_(handler) {
}
RcSvClientRequest::RcSvClientRequest(RcClientSession& ses, bool isNeedsLogInfo,
                                     StrView funcName, SvFunc fc,
                                     RcSeedVisitorClientNote& note,
                                     RcSeedVisitorClientNote::TreeRec& tree,
                                     const RcSeedVisitorClientNote::PendingReq& preq)
   : Session_(ses)
   , Note_{&note}
   , OrigTreePath_{ToStrView(tree.TreePath_)}
   , OrigTabName_{ToStrView(preq.TabName_)}
   , OrigSeedKey_{ToStrView(preq.SeedKey_)}
   , TabIndex_{preq.TabIndex_}
   , FuncName_{funcName}
   , IsNeedsLogInfo_{isNeedsLogInfo}
   , SvFunc_{ClrSvFuncFlag(&fc, SvFuncFlag::Layout)}
   , Handler_(preq.Handler_)
   , Tree_{&tree} {
}
RcSvClientRequest::~RcSvClientRequest() {
   this->FlushLog();
}
void RcSvClientRequest::FlushLog() {
   if (fon9_UNLIKELY(this->IsNeedsLogInfo_ && LogLevel::Info >= LogLevel_)) {
      RevBufferList rbuf_{kLogBlockNodeSize};
      RevPrint(rbuf_, this->ExtMsg_, '\n');
      if (this->SubrIndex_ != kSubrIndexNull)
         RevPrint(rbuf_, "|subrIndex=", this->SubrIndex_);
      if (this->DupTabIndex_ != kTabAll)
         RevPrint(rbuf_, "|dupTabIdx=", this->DupTabIndex_);
      if (!Parser_.Path_.empty() && this->OrigTreePath_ != ToStrView(this->Parser_.Path_))
         RevPrint(rbuf_, "|npath=", this->Parser_.Path_);
      if (this->OrigTabName_.empty())
         RevPrint(rbuf_, this->TabIndex_);
      else
         RevPrint(rbuf_, this->OrigTabName_);
      RevPrint(rbuf_, this->FuncName_,
               "|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&this->Session_)),
               "|userData=", ToPtr(this->Handler_.UserData_),
               "|opath=", this->OrigTreePath_,
               "|key=", this->OrigSeedKey_,
               "|tab=");
      LogWrite(LogLevel::Info, std::move(rbuf_));
      this->IsNeedsLogInfo_ = false;
   }
}
f9sv_Result RcSvClientRequest::CheckCurrSt(SvFunc currSt) {
   switch (GetSvFuncCode(currSt)) {
   case SvFuncCode::Empty:
      return f9sv_Result_NoError;
   case SvFuncCode::Query:
      this->ExtMsg_ = "|err=InQuery";
      return f9sv_Result_InQuery;
   case SvFuncCode::Subscribe:
      if (GetSvFuncCode(this->SvFunc_) == SvFuncCode::Unsubscribe)
         return f9sv_Result_NoError;
      this->ExtMsg_ = "|err=InSubscribe";
      return f9sv_Result_InSubscribe;
   case SvFuncCode::Unsubscribe:
      this->ExtMsg_ = "|err=InUnsubscribe";
      return f9sv_Result_InUnsubscribe;
   case SvFuncCode::Acl:
   case SvFuncCode::SubscribeData:
      break;
   }
   assert(!"Unknown currSt");
   return f9sv_Result_NoError;
}
f9sv_Result RcSvClientRequest::OnBeforeAssignTo(const TreeLocker& locker, const PodRec* pod) {
   (void)locker; (void)pod;
   return f9sv_Result_NoError;
}
//--------------------------------------------------------------------------//
void RcSvClientRequest::SendRequest() {
   const SvFuncCode fc = GetSvFuncCode(this->SvFunc_);
   assert(fc > SvFuncCode::Acl && fc != SvFuncCode::SubscribeData);
   RevBufferList rbuf{static_cast<BufferNodeSize>(128 + this->Tree_->TreePath_.size())};
   if (fc == SvFuncCode::Subscribe || fc == SvFuncCode::Unsubscribe) {
      ToBitv(rbuf, this->SubrIndex_);
      if (fc == SvFuncCode::Unsubscribe)
         goto __SKIP_SEED_NAME;
   }
   // ----- tabName or tabIndex
   if (HasSvFuncFlag(this->SvFunc_, SvFuncFlag::Layout)) {
      if (this->OrigTabName_.empty())
         ToBitv(rbuf, this->TabIndex_);
      else if (this->TabIndex_ == kTabAll)
         RevPutBitv(rbuf, fon9_BitvV_NumberNull);
      else
         ToBitv(rbuf, this->OrigTabName_);
   }
   else if (this->TabIndex_ == kTabAll)
      RevPutBitv(rbuf, fon9_BitvV_NumberNull);
   else
      ToBitv(rbuf, this->TabIndex_);
   // -----
   ToBitv(rbuf, this->OrigSeedKey_);
   ToBitv(rbuf, this->Tree_->TreePath_);
   // -----
__SKIP_SEED_NAME:;
   PutBigEndian(rbuf.AllocBuffer(sizeof(this->SvFunc_)), this->SvFunc_);
   this->Session_.Send(f9rc_FunctionCode_SeedVisitor, std::move(rbuf));
}
f9sv_Result RcSvClientRequest::FinalCheckSendRequest(f9sv_Result retval) {
   if (retval == f9sv_Result_NoError) {
      // TODO: 指定查詢範圍: key:[from..to)? range:[start,count)?
      if (this->SvFunc_ != SvFunc::Empty) { // 不是 pending 才需要送出.
         this->SendRequest();
         this->ExtMsg_ = "|info=Sending.";
      }
   }
   else if (this->Handler_.FnOnReport_) {
      this->FlushLog();
      f9sv_ClientReport rpt;
      ZeroStruct(rpt);
      rpt.ResultCode_ = retval;
      rpt.UserData_   = this->Handler_.UserData_;
      rpt.TreePath_   = this->OrigTreePath_.ToCStrView();
      rpt.SeedKey_    = this->OrigSeedKey_.ToCStrView();
      f9sv_Tab   tab;
      ZeroStruct(tab);
      rpt.Tab_ = &tab;
      tab.Named_.Name_  = this->OrigTabName_.ToCStrView();
      tab.Named_.Index_ = static_cast<int>(this->TabIndex_);
      this->Handler_.FnOnReport_(&this->Session_, &rpt);
   }
   return retval;
}
//--------------------------------------------------------------------------//
f9sv_Result RcSvClientRequest::CheckReqSt() {
   assert(this->Tree_ == nullptr);
   if (fon9_UNLIKELY(this->Note_ == nullptr)) {
      fon9_LOG_ERROR(this->FuncName_,
                     "|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&this->Session_)),
                     "|err=Must call f9sv_Initialize() first, "
                     "or must apply f9sv_ClientSessionParams when f9rc_CreateClientSession().");
      return f9sv_Result_BadInitParams;
   }
   if (fon9_UNLIKELY(this->Session_.GetSessionSt() != RcSessionSt::ApReady)) {
      this->ExtMsg_ = "|err=Session not ready";
      return f9sv_Result_LinkBroken;
   }
   if (fon9_UNLIKELY(this->Parser_.NormalizePathStr(this->OrigTreePath_) != nullptr)) {
      this->ExtMsg_ = "|err=TreePath format error";
      return ToResultC(seed::OpResult::path_format_error);
   }
   auto  maplk = this->Note_->LockTree();
   this->Tree_ = &maplk->fetch(ToStrView(this->Parser_.Path_));
   if (this->Tree_->Layout_)
      return this->FetchSeedSt(maplk);
   // * this->Tree_ 沒有 Layout_ 則需要請 Server 提供 layout,
   //   並將 req 放到 pending, 等查到 layuout 之後再處理.
   // * Layout 只查一次, 所以 Server 端必須先將 Layout 的 Tab 準備好,
   //   不支援查詢 Layout 後新增的 Tab.
   const auto  origSvFunc = this->SvFunc_;
   f9sv_Result retval;
   if (this->Tree_->PendingReqs_.empty())
      SetSvFuncFlag(&this->SvFunc_, SvFuncFlag::Layout);
   else { // 檢查是否有相同的 seedName 正在 pending.
      const auto iend = this->Tree_->PendingReqs_.end();
      const auto ibeg = this->Tree_->PendingReqs_.begin();
      for (auto i = ibeg; i != iend; ++i) {
         const auto& preq = *i;
         if (this->OrigSeedKey_ != ToStrView(preq.SeedKey_)
             || this->TabIndex_ != preq.TabIndex_
             || this->OrigTabName_ != ToStrView(preq.TabName_))
            continue;
         if (GetSvFuncCode(this->SvFunc_) == SvFuncCode::Unsubscribe
             && GetSvFuncCode(preq.SvFunc_) == SvFuncCode::Subscribe) {
            if (i == ibeg) { // 1st is Subscribing, this is Unsubscribe it.
                             // Wait 1st Subscribe result & process Unsubscribe request.
               this->ExtMsg_ = "|info=Pending.Unsub.";
               goto __UNSUBSCRIBE_1ST_PENDING;
            }
            this->SubrIndex_ = preq.SubrIndex_;
            this->FlushLog();
            if (preq.Handler_.FnOnReport_) {
               f9sv_ClientReport rpt;
               ZeroStruct(rpt);
               rpt.ResultCode_ = f9sv_Result_Unsubscribing;
               rpt.UserData_   = preq.Handler_.UserData_;
               rpt.TreePath_   = ToStrView(this->Tree_->TreePath_).ToCStrView();
               rpt.SeedKey_    = this->OrigSeedKey_.ToCStrView();
               f9sv_Tab   tab;
               ZeroStruct(tab);
               rpt.Tab_ = &tab;
               tab.Named_.Name_  = this->OrigTabName_.ToCStrView();
               tab.Named_.Index_ = static_cast<int>(this->TabIndex_);
               preq.Handler_.FnOnReport_(&this->Session_, &rpt);
            }
            maplk->SubrMap_.RemoveObjPtr(preq.SubrIndex_, nullptr);
            this->Tree_->PendingReqs_.erase(i);
            return f9sv_Result_Unsubscribed;
         }
         assert(preq.SvFunc_ != SvFunc::Empty);
         if ((retval = this->CheckCurrSt(preq.SvFunc_)) != f9sv_Result_NoError)
            return retval;
         this->ExtMsg_ = "|err=Unknown pending st";
         return f9sv_Result_InQuery;
      }
      this->ExtMsg_ = "|info=Pending.";
   }
   // 尚未取得 layout, 所有的要求都在 PendingReqs_, 但沒有發現訂閱要求, 所以此次取消訂閱失敗.
   if (GetSvFuncCode(this->SvFunc_) == SvFuncCode::Unsubscribe)
      return this->NoSubscribeError();
__UNSUBSCRIBE_1ST_PENDING:;
   // 將 req 放到 pending 之前, 也要透過 OnBeforeAssignTo() 檢查是否允許.
   if ((retval = this->OnBeforeAssignTo(maplk, nullptr)) != f9sv_Result_NoError)
      return retval;
   // 只有 pending 的第1筆才需要送出, 其餘的只放入 pending 不用送出.
   if (!this->Tree_->PendingReqs_.empty())
      this->SvFunc_ = SvFunc::Empty;
   // - 每次查詢都必須保留該次查詢的 handler
   // - 查詢的 handler 存放在 seed.
   // - 沒有 layout 無法建立 seed.
   // - 所以: 尚未取得 layout 的查詢, 都必須暫存在 tree.PendingReqs_.
   this->Tree_->PendingReqs_.resize(this->Tree_->PendingReqs_.size() + 1);
   RcSeedVisitorClientNote::PendingReq& pending = this->Tree_->PendingReqs_.back();
   pending.SvFunc_    = origSvFunc;
   pending.SubrIndex_ = this->SubrIndex_;
   pending.Handler_   = this->Handler_;
   pending.TabIndex_  = this->TabIndex_;
   pending.TabName_.assign(this->OrigTabName_);
   pending.SeedKey_.assign(this->OrigSeedKey_);
   return f9sv_Result_NoError;
}
f9sv_Result RcSvClientRequest::FetchSeedSt(const TreeLocker& locker) {
   assert(this->Tree_ && this->Tree_->Layout_.get() != nullptr);
   // 已有 Layout, 則檢查 this->SeedName_.TabIndex_ or this->SeedName_.TabName_ 是否正確.
   if (this->OrigTabName_.empty()) {
      if (this->TabIndex_ >= this->Tree_->LayoutC_.TabCount_) {
         this->ExtMsg_ = "|err=Bad TabIndex.";
         return f9sv_Result_NotFoundTab;
      }
   }
   else if (this->TabIndex_ != kTabAll) {
      auto* tab = this->Tree_->Layout_->GetTab(this->OrigTabName_);
      if (tab == nullptr) {
         this->ExtMsg_ = "|err=TabName not found.";
         return f9sv_Result_NotFoundTab;
      }
      this->TabIndex_ = unsigned_cast(tab->GetIndex());
   }
   bool         isNewPod;
   auto&        pod = this->Tree_->FetchPod(this->OrigSeedKey_, &isNewPod);
   f9sv_TabSize tabidx;
   f9sv_Result  retval;
   if (!isNewPod) {
      if (this->TabIndex_ == kTabAll) {
         for (tabidx = this->Tree_->LayoutC_.TabCount_; tabidx > 0;) {
            if ((retval = this->CheckSeedSt(pod, --tabidx)) != f9sv_Result_NoError) {
               this->DupTabIndex_ = tabidx;
               return retval;
            }
         }
      }
      else if ((retval = this->CheckSeedSt(pod, this->TabIndex_)) != f9sv_Result_NoError) {
         return retval;
      }
   }
   if ((retval = this->OnBeforeAssignTo(locker, &pod)) != f9sv_Result_NoError)
      return retval;
   if (GetSvFuncCode(this->SvFunc_) == SvFuncCode::Unsubscribe) {
      assert(this->TabIndex_ < this->Tree_->LayoutC_.TabCount_);
      auto* seed = pod.Seeds_[this->TabIndex_].get();
      if (GetSvFuncCode(seed->SvFunc_) != SvFuncCode::Subscribe)
         return this->NoSubscribeError();
      this->SubrIndex_ = seed->SubrIndex_;
      this->FlushLog();
      if (seed->Handler_.FnOnReport_) {
         f9sv_ClientReport rpt;
         ZeroStruct(rpt);
         rpt.ResultCode_ = f9sv_Result_Unsubscribing;
         rpt.UserData_   = seed->Handler_.UserData_;
         rpt.TreePath_   = ToStrView(this->Tree_->TreePath_).ToCStrView();
         rpt.SeedKey_    = this->OrigSeedKey_.ToCStrView();
         rpt.Layout_     = &this->Tree_->LayoutC_;
         rpt.Tab_        = &this->Tree_->TabList_[this->TabIndex_];
         seed->Handler_.FnOnReport_(&this->Session_, &rpt);
      }
      auto* subr = locker->SubrMap_.GetObjPtr(this->SubrIndex_);
      assert(subr != nullptr);
      assert(subr->TabIndex_ == this->TabIndex_);
      assert(subr->SvFunc_ == SvFuncCode::Subscribe);
      subr->SvFunc_ = SvFuncCode::Unsubscribe;
   }
   if (this->TabIndex_ == kTabAll) {
      for (tabidx = this->Tree_->LayoutC_.TabCount_; tabidx > 0;)
         this->AssignToSeed(*pod.Seeds_[--tabidx]);
   }
   else {
      this->AssignToSeed(*pod.Seeds_[this->TabIndex_]);
   }
   return f9sv_Result_NoError;
}
f9sv_Result RcSvClientRequest::NoSubscribeError() {
   this->SvFunc_ = SvFunc::Empty;
   this->ExtMsg_ = "|info=NoSubscribe.";
   return f9sv_Result_NoSubscribe;
}

} } // namespaces
