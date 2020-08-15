// \file fon9/rc/RcSeedVisitorClientCAPI.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSeedVisitorClientReq.hpp"

namespace fon9 { namespace rc {

f9sv_Result RcSeedVisitorClientNote::AddSubr(const TreeLocker& locker, RcSvClientRequest& req, const svc::PodRec* pod) {
   if (this->Config_.MaxSubrCount_ > 0) {
      if (locker->SubrMap_.Size() >= this->Config_.MaxSubrCount_) {
         req.ExtMsg_ = "|err=OverMaxSubrCount";
         return f9sv_Result_OverMaxSubrCount;
      }
   }
   req.SubrIndex_ = static_cast<f9sv_SubrIndex>(locker->SubrMap_.Alloc());
   svc::SubrRec*  subr = locker->SubrMap_.GetObjPtr(req.SubrIndex_);
   subr->IsStream_ = seed::IsSubscribeStream(req.OrigTabName_);
   subr->SvFunc_ = GetSvFuncCode(req.SvFunc_);
   assert(subr->SvFunc_ == SvFuncCode::Subscribe);
   subr->Tree_ = req.Tree_;
   subr->TabIndex_ = req.TabIndex_;
   if (pod == nullptr)
      subr->Seeds_ = nullptr;
   else
      subr->OnPodReady(*pod, req.SubrIndex_);
   subr->SeedKey_.assign(req.OrigSeedKey_);
   return f9sv_Result_NoError;
}

} } // namespaces
//--------------------------------------------------------------------------//
extern "C" {

f9sv_CAPI_FN(int)
f9sv_Initialize(const char* logFileFmt) {
   int retval = fon9_Initialize(logFileFmt);
   if (retval == 0 && fon9::rc::RcClientMgr_->Get(f9rc_FunctionCode_SeedVisitor) == nullptr)
      fon9::rc::RcClientMgr_->Add(fon9::rc::RcFunctionAgentSP{new fon9::rc::RcSeedVisitorClientAgent});
   return retval;
}

f9sv_CAPI_FN(f9sv_Result)
f9sv_Query(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler) {
   struct Request : public fon9::rc::RcSvClientRequest {
      fon9_NON_COPY_NON_MOVE(Request);
      using fon9::rc::RcSvClientRequest::RcSvClientRequest;
      f9sv_Result OnBeforeAssignTo(const TreeLocker& locker, const PodRec* pod) override {
         (void)pod;
         fon9::TimeInterval fcWait = this->Note_->FcQryFetch(locker);
         if (fon9_LIKELY(fcWait.GetOrigValue() <= 0))
            return f9sv_Result_NoError;
         this->ExtMsg_ = "|err=Flow control";
         return static_cast<f9sv_Result>(fcWait.ShiftUnit<3>() + 1);
      }
   };
   Request  req(*static_cast<fon9::rc::RcClientSession*>(ses),
                f9rc_ClientLogFlag{} != (ses->LogFlags_ & f9sv_ClientLogFlag_Query),
                "f9sv_Query", fon9::rc::SvFuncCode::Query,
                *seedName, handler);
   return req.FinalCheckSendRequest(req.CheckReqSt());
}

f9sv_CAPI_FN(f9sv_Result)
f9sv_Subscribe(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler) {
   // TODO: 訂閱 seedName->SeedKey_ 是否允許使用 "*" or 其他 filter 規則?
   struct Request : public fon9::rc::RcSvClientRequest {
      fon9_NON_COPY_NON_MOVE(Request);
      using fon9::rc::RcSvClientRequest::RcSvClientRequest;
      f9sv_Result OnBeforeAssignTo(const TreeLocker& locker, const PodRec* pod) override {
         return this->Note_->AddSubr(locker, *this, pod);
      }
   };
   Request  req(*static_cast<fon9::rc::RcClientSession*>(ses),
                f9rc_ClientLogFlag{} != (ses->LogFlags_ & f9sv_ClientLogFlag_Subscribe),
                "f9sv_Subscribe", fon9::rc::SvFuncCode::Subscribe,
                *seedName, handler);
   return req.FinalCheckSendRequest(req.CheckSubrReqSt());
}

f9sv_CAPI_FN(f9sv_Result)
f9sv_Unsubscribe(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler) {
   using Request = fon9::rc::RcSvClientRequest;
   Request  req(*static_cast<fon9::rc::RcClientSession*>(ses),
                f9rc_ClientLogFlag{} != (ses->LogFlags_ & f9sv_ClientLogFlag_Subscribe),
                "f9sv_Unsubscribe", fon9::rc::SvFuncCode::Unsubscribe,
                *seedName, handler);
   return req.FinalCheckSendRequest(req.CheckSubrReqSt());
}

f9sv_CAPI_FN(const char*)
f9sv_GetSvResultMessage(f9sv_Result res) {
   static_assert(f9sv_Result_AccessDenied == fon9::rc::ToResultC(fon9::seed::OpResult::access_denied), "");
   static_assert(f9sv_Result_NotFoundTab == fon9::rc::ToResultC(fon9::seed::OpResult::not_found_tab), "");
   static_assert(f9sv_Result_PathFormatError == fon9::rc::ToResultC(fon9::seed::OpResult::path_format_error), "");

   #define case_return(r)  case f9sv_Result_##r:    return #r;
   switch (res) {
      case_return(NoError);
      case_return(Unsubscribing);
      case_return(Unsubscribed);
      case_return(SubrNotifyPodRemoved);
      case_return(SubrNotifySeedRemoved);
      case_return(SubrNotifyTableChanged);
      case_return(SubrNotifyUnsubscribed);
      case_return(SubrStreamOK);
      case_return(SubrStreamRecover);
      case_return(SubrStreamRecoverEnd);
      case_return(SubrStreamEnd);

      case_return(AccessDenied);
      case_return(NotFoundTab);
      case_return(PathFormatError);

      case_return(BadInitParams);
      case_return(LinkBroken);
      case_return(FlowControl);
      case_return(InQuery);
      case_return(OverMaxSubrCount);
      case_return(InSubscribe);
      case_return(InUnsubscribe);
   }
   return GetOpResultMessage(static_cast<fon9::seed::OpResult>(res));
}

//--------------------------------------------------------------------------//

f9sv_CAPI_FN(const char*)
f9sv_GetField_Str(const struct f9sv_Seed* seed, const f9sv_Field* fld, char* outbuf, uint32_t* bufsz) {
   assert(reinterpret_cast<const fon9::seed::Field*>(fld->InternalOwner_)->GetIndex() == fld->Named_.Index_);
   fon9::RevBufferList rbuf{128};
   reinterpret_cast<const fon9::seed::Field*>(fld->InternalOwner_)
      ->CellRevPrint(fon9::seed::SimpleRawRd{seed}, nullptr, rbuf);
   const size_t outsz = fon9::CalcDataSize(rbuf.cfront());
   if (outbuf && *bufsz > 0) {
      fon9::DcQueueList dcq{rbuf.MoveOut()};
      if (*bufsz > outsz) {
         dcq.Read(outbuf, outsz);
         outbuf[outsz] = '\0';
      }
      else {
         dcq.Read(outbuf, *bufsz - 1);
         outbuf[*bufsz - 1] = '\0';
      }
   }
   *bufsz = static_cast<uint32_t>(outsz + 1);
   return outbuf;
}

} // extern "C"
