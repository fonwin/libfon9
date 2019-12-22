// \file fon9/rc/RcSeedVisitorClientReq.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSeedVisitorClientReq.hpp"

namespace fon9 { namespace rc {

RcSvClientRequest::RcSvClientRequest(RcClientSession& ses, StrView funcName, SvFuncCode svFuncCode, bool isLogInfo,
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
   , IsLogInfo_{isLogInfo}
   , SvFunc_{svFuncCode - SvFuncCode::FlagLayout}
   , Handler_(handler) {
}
RcSvClientRequest::RcSvClientRequest(RcClientSession& ses, StrView funcName, SvFuncCode svFuncCode, bool isLogInfo,
                                     RcSeedVisitorClientNote& note, StrView treePath,
                                     const RcSeedVisitorClientNote::PendingReq& req)
   : Session_(ses)
   , Note_{&note}
   , OrigTreePath_{treePath}
   , OrigTabName_{ToStrView(req.TabName_)}
   , OrigSeedKey_{ToStrView(req.SeedKey_)}
   , TabIndex_{req.TabIndex_}
   , FuncName_{funcName}
   , IsLogInfo_{isLogInfo}
   , SvFunc_{svFuncCode - SvFuncCode::FlagLayout}
   , Handler_(req.Handler_) {
}
RcSvClientRequest::~RcSvClientRequest() {
   if (fon9_UNLIKELY(this->IsLogInfo_ && LogLevel::Info >= LogLevel_)) {
      RevBufferList rbuf_{kLogBlockNodeSize};
      RevPrint(rbuf_, this->ExtMsg_, '\n');
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
               "|ud=", ToPtr(this->Handler_.UserData_),
               "|opath=", this->OrigTreePath_,
               "|key=", this->OrigSeedKey_,
               "|tab=");
      LogWrite(LogLevel::Info, std::move(rbuf_));
   }
}
f9sv_Result RcSvClientRequest::CheckSeedSt(RcSeedVisitorClientNote::PodRec& pod, unsigned tabidx) {
   SvFuncCode currSt = pod.Seeds_[tabidx]->CurrSt_;
   if (fon9_UNLIKELY(IsEnumContains(currSt, SvFuncCode::Query))) {
      this->ExtMsg_ = "|err=Dup Query same seedName";
      return f9sv_Result_DupQuery;
   }
   if (fon9_UNLIKELY(IsEnumContains(currSt, SvFuncCode::Subscribe))) {
      this->ExtMsg_ = "|err=In subscribe";
      return f9sv_Result_InSubscribe;
   }
   if (fon9_UNLIKELY(IsEnumContains(currSt, SvFuncCode::Unsubscribe))) {
      this->ExtMsg_ = "|err=In unsubscribe";
      return f9sv_Result_InUnsubscribe;
   }
   return f9sv_Result_NoError;
}
f9sv_Result RcSvClientRequest::OnBeforeSetupSeedSt() {
   return f9sv_Result_NoError;
}
//--------------------------------------------------------------------------//
void RcSvClientRequest::SendQuery() {
   assert(IsEnumContains(this->SvFunc_, SvFuncCode::Query));
   const StrView treePath = (this->Parser_.Path_.empty() ? this->OrigTreePath_ : ToStrView(this->Parser_.Path_));
   RevBufferList rbuf{static_cast<BufferNodeSize>(128 + treePath.size())};
   // tabName or tabIndex
   if (IsEnumContains(this->SvFunc_, SvFuncCode::FlagLayout)) {
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
   ToBitv(rbuf, treePath);
   // -----
   PutBigEndian(rbuf.AllocBuffer(sizeof(this->SvFunc_)), this->SvFunc_);
   this->Session_.Send(f9rc_FunctionCode_SeedVisitor, std::move(rbuf));
}
f9sv_Result RcSvClientRequest::FinalCheckSendQuery(f9sv_Result retval) {
   if (retval == f9sv_Result_NoError) {
      // TODO: 指定查詢範圍: key:[from..to)? range:[start,count)?
      if (this->SvFunc_ != SvFuncCode{}) { // 不是 pending 才需要送出.
         this->SendQuery();
         this->ExtMsg_ = "|info=Sending";
      }
   }
   else if (this->Handler_.FnOnReport_) {
      f9sv_ClientReport rpt;
      f9sv_Tab          tab;
      ZeroStruct(rpt);
      ZeroStruct(tab);
      rpt.ResultCode_ = retval;
      rpt.UserData_ = this->Handler_.UserData_;
      rpt.TreePath_ = this->OrigTreePath_.ToCStrView();
      rpt.SeedKey_  = this->OrigSeedKey_.ToCStrView();
      rpt.Tab_ = &tab;
      tab.Named_.Name_  = this->OrigTabName_.ToCStrView();
      tab.Named_.Index_ = static_cast<int>(this->TabIndex_);
      this->Handler_.FnOnReport_(&this->Session_, &rpt);
   }
   return retval;
}
//--------------------------------------------------------------------------//
f9sv_Result RcSvClientRequest::CheckApiSeedName() {
   if (fon9_UNLIKELY(this->Note_ == nullptr)) {
      fon9_LOG_ERROR(this->FuncName_,
                     "|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&this->Session_)),
                     "|err=Must call f9sv_Initialize() first, "
                     "or must apply f9sv_ClientSessionParams when f9rc_CreateClientSession().");
      return f9sv_Result_BadInitParams;
   }
   if (fon9_UNLIKELY(this->Parser_.NormalizePathStr(this->OrigTreePath_) != nullptr)) {
      this->ExtMsg_ = "|err=TreePath format error";
      return ToResultC(seed::OpResult::path_format_error);
   }
   auto  maplk = this->Note_->TreeMap_.Lock();
   auto& tree = maplk->kfetch(CharVector::MakeRef(ToStrView(this->Parser_.Path_))).second;
   if (tree.Layout_)
      return this->FetchSeedSt(tree);
   // Layout 只查一次, 不支援查詢 Layout 後新增的 Tab, 所以 Server 端必須先將 Layout 準備好.
   const auto  svFunc = this->SvFunc_;
   if (tree.PendingReqs_.empty())
      this->SvFunc_ |= SvFuncCode::FlagLayout;
   else {
      this->SvFunc_ = SvFuncCode{};
      this->ExtMsg_ = "|info=Pending.";
   }
   // - 每次查詢都必須保留該次查詢的 handler
   // - 查詢的 handler 存放在 seed.
   // - 沒有 layout 無法建立 seed.
   // - 所以: 尚未取得 layout 的查詢, 都必須暫存.
   tree.PendingReqs_.resize(tree.PendingReqs_.size() + 1);
   RcSeedVisitorClientNote::PendingReq& pending = tree.PendingReqs_.back();
   pending.SvFunc_ = svFunc;
   pending.Handler_ = this->Handler_;
   pending.SeedKey_.assign(this->OrigSeedKey_);
   pending.TabIndex_ = this->TabIndex_;
   if (!this->OrigTabName_.empty())
      pending.TabName_.assign(this->OrigTabName_);
   return f9sv_Result_NoError;
}
f9sv_Result RcSvClientRequest::FetchSeedSt(RcSeedVisitorClientNote::TreeRec& tree) {
   assert(tree.Layout_.get() != nullptr);
   // 已有 Layout, 則檢查 this->SeedName_.TabIndex_ or this->SeedName_.TabName_ 是否正確.
   if (this->OrigTabName_.empty()) {
      if (this->TabIndex_ >= tree.LayoutC_.TabCount_) {
         this->ExtMsg_ = "|err=Bad TabIndex.";
         return ToResultC(seed::OpResult::not_found_tab);
      }
   }
   else if (this->TabIndex_ != kTabAll) {
      auto* tab = tree.Layout_->GetTab(this->OrigTabName_);
      if (tab == nullptr) {
         this->ExtMsg_ = "|err=TabName not found.";
         return ToResultC(seed::OpResult::not_found_tab);
      }
      this->TabIndex_ = unsigned_cast(tab->GetIndex());
   }
   bool         isNewPod;
   auto&        pod = tree.FetchPod(this->OrigSeedKey_, &isNewPod);
   f9sv_TabSize tabidx;
   f9sv_Result  retval;
   if (!isNewPod) {
      if (this->TabIndex_ == kTabAll) {
         for (tabidx = tree.LayoutC_.TabCount_; tabidx > 0;) {
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
   if ((retval = this->OnBeforeSetupSeedSt()) != f9sv_Result_NoError)
      return retval;
   if (this->TabIndex_ == kTabAll) {
      for (tabidx = tree.LayoutC_.TabCount_; tabidx > 0;)
         this->SetupSeedSt(*pod.Seeds_[--tabidx]);
   }
   else {
      this->SetupSeedSt(*pod.Seeds_[this->TabIndex_]);
   }
   return f9sv_Result_NoError;
}

} } // namespaces
