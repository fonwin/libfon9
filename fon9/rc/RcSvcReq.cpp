// \file fon9/rc/RcSvcReq.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSvcReq.hpp"
#include "fon9/rc/RcSeedVisitorClient.hpp"

namespace fon9 { namespace rc {

seed::Tab* RcSvcGetTabStreamArgs(seed::Layout& layout, StrView& tabNameAndStreamArgs) {
   StrView tabName = tabNameAndStreamArgs;
   if (!seed::IsSubscribeStream(tabName))
      tabNameAndStreamArgs.Reset(nullptr);
   else {
      tabNameAndStreamArgs.SetBegin(tabName.begin() + 1);
      tabName = StrFetchNoTrim(tabNameAndStreamArgs, ':');
   }
   return layout.GetTabByNameOrFirst(tabName);
}
StrView RcSvcGetStreamDecoderName(StrView tabNameAndStreamArgs) {
   if (!seed::IsSubscribeStream(tabNameAndStreamArgs))
      return nullptr;
   StrFetchNoTrim(tabNameAndStreamArgs, ':'); // 移除 TabName;
   return StrFetchNoTrim(tabNameAndStreamArgs, ':'); // 取出 StreamDecoderName;
}
//--------------------------------------------------------------------------//
RcSvcReq::RcSvcReq(RcClientSession& ses,
                   f9rc_ClientLogFlag logFlag,
                   StrView funcName,
                   SvFuncCode fc,
                   const f9sv_SeedName& seedName,
                   f9sv_ReportHandler handler)
   : Handler_(handler) 
   , Note_{static_cast<RcSeedVisitorClientNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor))}
   , Session_(ses)
   , OrigTreePath_{StrView_cstr(seedName.TreePath_)}
   , OrigTabName_{StrView_cstr(seedName.TabName_)}
   , OrigSeedKey_{StrView_cstr(seedName.SeedKey_)}
   , OrigSvFunc_{fc}
   , CurrSvFunc_{ToSvFunc(fc)}
   , IsNeedsLogInfo_{f9rc_ClientLogFlag{} != (ses.LogFlags_ & logFlag)}
   , FuncName_{funcName}
   , TabIndex_(seedName.TabName_ == nullptr       ? seedName.TabIndex_
              : seed::IsTabAll(seedName.TabName_) ? kTabAll
              :                                     0)
{
}
RcSvcReq::~RcSvcReq() {
   this->FlushLogFinal();
}
void RcSvcReq::AddLogMsg(RevBufferList& rbuf) {
   (void)rbuf;
}
void RcSvcReq::FlushLogKeep() {
   if (fon9_UNLIKELY(this->IsNeedsLogInfo_ && LogLevel::Info >= LogLevel_)) {
      RevBufferList rbuf_{kLogBlockNodeSize};
      RevPrint(rbuf_, this->ExLogMsg_, '\n');
      if (this->PendingSt_ != PendingSt::None)
         RevPrint(rbuf_, "|PendingSt=", this->PendingSt_);
      this->ExLogMsg_.Reset(nullptr);
      this->AddLogMsg(rbuf_);
      if (this->DupTabIndex_ != kTabAll)
         RevPrint(rbuf_, "|dupTabIdx=", this->DupTabIndex_);
      if (!this->NormalizedPath_.empty() && this->OrigTreePath_ != this->NormalizedPath_)
         RevPrint(rbuf_, "|npath=", this->NormalizedPath_);
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
   }
}
//--------------------------------------------------------------------------//
f9sv_Result RcSvcReq::Run(RcSvcReqSP pthisSP) {
   RcSvcReq* const pthis = pthisSP.get();
   assert(pthis->Tree_ == nullptr);
   if (fon9_UNLIKELY(pthis->Note_ == nullptr)) {
      fon9_LOG_ERROR(pthis->FuncName_,
                     "|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&pthis->Session_)),
                     "|err=Must call f9sv_Initialize() first, "
                     "or must apply f9sv_ClientSessionParams when f9rc_CreateClientSession().");
      return f9sv_Result_BadInitParams;
   }
   if (fon9_UNLIKELY(pthis->Session_.GetSessionSt() != RcSessionSt::ApReady)) {
      pthis->ExLogMsg_ = "|err=Session not ready";
      return f9sv_Result_LinkBroken;
   }
   // -----
   seed::AclPathParser  pathParser;
   if (fon9_UNLIKELY(pathParser.NormalizePathStr(ToStrView(pthis->OrigTreePath_)) != nullptr)) {
      pthis->ExLogMsg_ = "|err=TreePath format error";
      return ToResultC(seed::OpResult::path_format_error);
   }
   pthis->NormalizedPath_ = std::move(pathParser.Path_);
   // -----
   f9sv_Result retval;
   if ((retval = pthis->CheckOnAfterNormalized()) != f9sv_Result_NoError)
      return pthis->ReportResult(retval);
   // -----
   { // auto maplk.
      auto  maplk = pthis->Note_->LockTree();
      auto* ptree = pthis->Tree_ = &maplk->fetch(ToStrView(pthis->NormalizedPath_));
      const bool isPendingEmpty = ptree->PendingReqs_.empty();
      if (fon9_LIKELY(ptree->Layout_ && isPendingEmpty)) {
         if ((retval = pthis->CheckOnBeforeSend(maplk)) != f9sv_Result_NoError)
            return pthis->ReportResult(retval);
      }
      else {
         pthis->PendingSt_ = PendingSt::PreCheck;
         if ((retval = pthis->CheckPendingReqs(maplk)) != f9sv_Result_NoError)
            return pthis->ReportResult(retval);
         if ((retval = pthis->CheckFinal(maplk, nullptr)) != f9sv_Result_NoError)
            return pthis->ReportResult(retval);
         // - 每次要求都必須保留該次要求的 handler
         // - 要求的 handler 存放在 SeedRec or SubrRec.
         // - 沒有 layout 無法建立 SeedRec.
         // - 所以: 尚未取得 layout 的要求, 都必須暫存在 tree.PendingReqs_.
         // - 取消訂閱:
         //   - 直接移除 PendingReqs 裡面的訂閱要求.
         //   - 若要取消的是 PendingReqs.front() 已送出的訂閱,
         //     則在此立即送出取消訂閱的要求, 不用等候訂閱結果.
         if (pthis->OrigSvFunc_ == SvFuncCode::Unsubscribe) {
            assert(!isPendingEmpty
                   && ptree->PendingReqs_.front()->OrigSvFunc_ == SvFuncCode::Subscribe
                   && ptree->PendingReqs_.front()->IsSameSeed(*pthis));
         }
         else {
            ptree->PendingReqs_.push_back(std::move(pthisSP));
            pthis->PendingSt_ = PendingSt::Pushed;
            // 只有 pending 的第1筆才需要立即送出, 其餘的只放入 pending 暫時不用送出.
            if (!isPendingEmpty) {
               pthis->ExLogMsg_ = "|info=Pending.";
               pthis->FlushLogKeep();
               return f9sv_Result_NoError;
            }
            // - pthis->Tree_ 沒有 Layout_ 則需要請 Server 提供 layout:
            //   SetSvFuncFlag(&pthis->CurrSvFunc_, SvFuncFlag::Layout);
            // - Layout 只查一次, 所以 Server 端必須先將 Layout 的 Tab 準備好,
            //   不支援查詢 Layout 後新增的 Tab.
            if (!pthis->Tree_->Layout_)
               SetSvFuncFlag(&pthis->CurrSvFunc_, SvFuncFlag::Layout);
            assert(pthis == pthis->Tree_->PendingReqs_.front().get());
         }
      }
   } // maplk.unlock();
   // -----
   if (pthis->PendingSt_ != PendingSt::None)
      pthis->FlushLogKeep();
   pthis->SendRequest();
   return f9sv_Result_NoError;
}
f9sv_Result RcSvcReq::CheckOnAfterNormalized() {
   return f9sv_Result_NoError;
}
f9sv_Result RcSvcReq::CheckOnBeforeSend(const svc::TreeLocker& maplk) {
   if (this->OrigTabName_.empty()) {
      if (this->TabIndex_ >= this->Tree_->LayoutC_.TabCount_) {
         this->ExLogMsg_ = "|err=Bad TabIndex.";
         return f9sv_Result_NotFoundTab;
      }
   }
   else if (this->TabIndex_ != kTabAll) {
      // 建構時指定非 "*" 的 tabName: 透過 tabName 設定 this->TabIndex_;
      this->SubrStreamArgs_ = ToStrView(this->OrigTabName_);
      auto* tab = RcSvcGetTabStreamArgs(*this->Tree_->Layout_, this->SubrStreamArgs_);
      if (tab == nullptr) {
         this->ExLogMsg_ = "|err=TabName not found.";
         return f9sv_Result_NotFoundTab;
      }
      this->TabIndex_ = unsigned_cast(tab->GetIndex());
   }
   return this->CheckFinal(maplk, &this->Tree_->FetchPod(ToStrView(this->OrigSeedKey_)));
}
//--------------------------------------------------------------------------//
void RcSvcReq::RejectPending(const svc::TreeLocker& maplk) {
   (void)maplk;
   assert(this->PendingSt_ != PendingSt::None);
   this->PendingSt_ = PendingSt::Reject;
   this->FlushLogFinal();
}
f9sv_Result RcSvcReq::ReportResult(f9sv_Result st) {
   this->FlushLogFinal();
   if (this->Handler_.FnOnReport_) {
      f9sv_ClientReport rpt;
      ZeroStruct(rpt);
      rpt.ResultCode_ = (this->InFlowControl_ ? f9sv_Result_FlowControl : st);
      rpt.TreePath_ = ToStrView(this->OrigTreePath_).ToCStrView();

      f9sv_Tab   tab;
      ZeroStruct(tab);
      rpt.Tab_ = &tab;

      this->AssignRptTabSeed(rpt);
      this->Handler_.FnOnReport_(&this->Session_, &rpt);
   }
   return st;
}
//--------------------------------------------------------------------------//
void RcSvcReq::PackRequest(RevBufferList& rbuf) {
   // ----- 打包: tabName or tabIndex
   if (HasSvFuncFlag(this->CurrSvFunc_, SvFuncFlag::Layout)) {
      if (this->OrigTabName_.empty())
         ToBitv(rbuf, this->TabIndex_);
      else if (this->TabIndex_ == kTabAll)
         RevPutBitv(rbuf, fon9_BitvV_NumberNull);
      else
         ToBitv(rbuf, this->OrigTabName_);
   }
   else if (this->TabIndex_ == kTabAll)
      RevPutBitv(rbuf, fon9_BitvV_NumberNull);
   else if (this->OrigSvFunc_ == SvFuncCode::Subscribe && !this->SubrStreamArgs_.IsNull())
      ToBitv(rbuf, this->OrigTabName_);
   else
      ToBitv(rbuf, this->TabIndex_);
   // ----- 打包: TreePath & SeedKey
   ToBitv(rbuf, this->OrigSeedKey_);
   ToBitv(rbuf, this->Tree_->TreePath_);
}
void RcSvcReq::SendRequest() {
   assert(SvFuncCode::Acl < this->OrigSvFunc_ && this->OrigSvFunc_ != SvFuncCode::SubscribeData);
   assert(this->OrigSvFunc_ == GetSvFuncCode(this->CurrSvFunc_));
   this->ExLogMsg_ = "|info=Sending.";
   RevBufferList rbuf{static_cast<BufferNodeSize>(128 + this->Tree_->TreePath_.size())};
   this->PackRequest(rbuf);
   PutBigEndian(rbuf.AllocBuffer(sizeof(this->CurrSvFunc_)), this->CurrSvFunc_);
   this->Session_.Send(f9rc_FunctionCode_SeedVisitor, std::move(rbuf));
}
//--------------------------------------------------------------------------//
void RcSvcReq::RunPendingReqs(const svc::TreeLocker& maplk, svc::TreeRec& tree, bool isTabNotFound) {
   if (tree.PendingReqs_.empty())
      return;
   RcSvcReq* preq = tree.PendingReqs_.front().get();
   assert(preq->Tree_ == &tree);
   if (HasSvFuncFlag(preq->CurrSvFunc_, SvFuncFlag::Layout)) {
      preq->PendingSt_ = isTabNotFound ? PendingSt::AckTabNotFound : PendingSt::AckPod;
      preq->OnReportPending(maplk, isTabNotFound ? nullptr : &tree.FetchPod(ToStrView(preq->OrigSeedKey_)));
      tree.PendingReqs_.pop_front();
   }
   while (!tree.PendingReqs_.empty()) {
      preq = tree.PendingReqs_.front().get();
      preq->PendingSt_ = PendingSt::PopRun;
      f9sv_Result res = preq->CheckOnBeforeSend(maplk);
      // 有 preq->InPending_ 判斷, 從 PendingReqs 的送出, 不進行流量管制.
      // => Server 端檢查時, 適度放寬流管參數.
      // if (res > f9sv_Result_NoError) { // 啟動 flow control 計時器.
      //    preq->Note_->StartResendReqTimer();
      //    break;
      // }
      if (res == f9sv_Result_NoError)
         preq->SendRequest();
      else {
         preq->ReportResult(res);
      }
      tree.PendingReqs_.pop_front();
   }
}

} } // namespaces
