// \file fon9/rc/RcSvcReqSeed.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSvcReq.hpp"
#include "fon9/rc/RcSeedVisitorClient.hpp"

namespace fon9 { namespace rc {

void RcSvcReqSeed::OnReportPending(const svc::TreeLocker& maplk, const svc::PodRec* pod) {
   (void)maplk;
   if (pod) {
      if (this->TabIndex_ == kTabAll) {
         for (f9sv_TabSize L = this->Tree_->LayoutC_.TabCount_; L > 0;)
            this->AssignWorkStToSeed(*pod->Seeds_[--L]);
         return;
      }
      if (!this->OrigTabName_.empty()) {
         if (auto* tab = this->Tree_->Layout_->GetTab(ToStrView(this->OrigTabName_)))
            this->TabIndex_ = static_cast<f9sv_TabSize>(tab->GetIndex());
         else {
            this->ReportResult(f9sv_Result_NotFoundTab);
            return;
         }
      }
      if (this->TabIndex_ < this->Tree_->LayoutC_.TabCount_) {
         this->AssignWorkStToSeed(*pod->Seeds_[this->TabIndex_]);
         return;
      }
   }
   this->ExLogMsg_ = "|err=Ack:Tab not found";
   this->ReportResult(f9sv_Result_NotFoundTab);
}
f9sv_Result RcSvcReqSeed::CheckSeedWorkingSt(SvFuncCode fcWorking) {
   switch (fcWorking) {
   default:
   case SvFuncCode::Empty:
   case SvFuncCode::Acl:
   case SvFuncCode::Subscribe:
   case SvFuncCode::SubscribeData:
   case SvFuncCode::Unsubscribe:
      return f9sv_Result_NoError;

   #define caseInWorkSt(name) \
   case SvFuncCode::name:    this->ExLogMsg_ = "|err=In" #name;      return f9sv_Result_In##name
      caseInWorkSt(Query);
      caseInWorkSt(Write);
      caseInWorkSt(Remove);
      caseInWorkSt(Command);
      caseInWorkSt(GridView);
   #undef caseInWorkSt
   }
}
f9sv_Result RcSvcReqSeed::CheckFinal(const svc::TreeLocker& maplk, const svc::PodRec* pod) {
   f9sv_Result retval;
   if (pod) {
      if (this->TabIndex_ == kTabAll) {
         for (f9sv_TabSize tabidx = this->Tree_->LayoutC_.TabCount_; tabidx > 0;) {
            if ((retval = this->CheckSeedWorkingSt(*pod, --tabidx)) != f9sv_Result_NoError) {
               this->DupTabIndex_ = tabidx;
               return retval;
            }
         }
      }
      else if ((retval = this->CheckSeedWorkingSt(*pod, this->TabIndex_)) != f9sv_Result_NoError) {
         return retval;
      }
   }
   // -----
   if (this->PendingSt_ != PendingSt::PopRun) {
      const TimeInterval fcWait = this->Note_->FcQryFetch(maplk);
      if (fon9_LIKELY(fcWait.GetOrigValue() > 0)) {
         this->ExLogMsg_ = "|err=Flow control";
         this->InFlowControl_ = true;
         return static_cast<f9sv_Result>(fcWait.ShiftUnit<3>() + 1);
      }
   }
   // -----
   if (pod) {
      if (this->TabIndex_ == kTabAll) {
         for (f9sv_TabSize tabidx = this->Tree_->LayoutC_.TabCount_; tabidx > 0;)
            this->AssignWorkStToSeed(*pod, --tabidx);
      }
      else {
         this->AssignWorkStToSeed(*pod, this->TabIndex_);
      }
   }
   return f9sv_Result_NoError;
}
f9sv_Result RcSvcReqSeed::CheckPendingReqs(const svc::TreeLocker& maplk) {
   (void)maplk;
   const auto iend = this->Tree_->PendingReqs_.end();
   const auto ibeg = this->Tree_->PendingReqs_.begin();
   for (auto i = ibeg; i != iend; ++i) {
      const RcSvcReq& preq = *(i->get());
      if (this->IsSameSeed(preq)) {
         // preq = 一般要求(Query,GridView,Remove...), 上次未完成前, 同一個 seed 不可再執行一般要求.
         f9sv_Result retval = this->CheckSeedWorkingSt(preq.OrigSvFunc_);
         if (retval != f9sv_Result_NoError)
            return retval;
      }
   }
   return f9sv_Result_NoError;
}
//--------------------------------------------------------------------------//
RcSvcReqGridView::~RcSvcReqGridView() {
   this->FlushLogFinal();
}
void RcSvcReqGridView::AddLogMsg(RevBufferList& rbuf) {
   RevPrint(rbuf, "|gvReqMaxRowCount=", this->GridViewReqMaxRowCount_);
}
void RcSvcReqGridView::PackRequest(RevBufferList& rbuf) {
   ToBitv(rbuf, this->GridViewReqMaxRowCount_);
   base::PackRequest(rbuf);
}
//--------------------------------------------------------------------------//
RcSvcReqExStrArg::~RcSvcReqExStrArg() {
   this->FlushLogFinal();
}
void RcSvcReqExStrArg::AddLogMsg(RevBufferList& rbuf) {
   RevPrint(rbuf, "|exStrArg={", this->ExStrArg_, '}');
}
void RcSvcReqExStrArg::PackRequest(RevBufferList& rbuf) {
   ToBitv(rbuf, this->ExStrArg_);
   base::PackRequest(rbuf);
}

} } // namespaces
