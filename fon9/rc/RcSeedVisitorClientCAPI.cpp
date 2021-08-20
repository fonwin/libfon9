// \file fon9/rc/RcSeedVisitorClientCAPI.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSvcReq.hpp"
#include "fon9/rc/RcSeedVisitorClient.hpp"
using namespace fon9::rc;
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
   return RcSvcReq::Run(RcSvcReqSP{new RcSvcReqSeed(
      *static_cast<fon9::rc::RcClientSession*>(ses),
      f9sv_ClientLogFlag_Query, "f9sv_Query", fon9::rc::SvFuncCode::Query,
      *seedName, handler
   )});
}

f9sv_CAPI_FN(f9sv_Result)
f9sv_GridView(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler, int16_t reqMaxRowCount) {
   return RcSvcReq::Run(RcSvcReqSP{new RcSvcReqGridView(
      reqMaxRowCount,
      *static_cast<fon9::rc::RcClientSession*>(ses),
      f9sv_ClientLogFlag_GridView, "f9sv_GridView", fon9::rc::SvFuncCode::GridView,
      *seedName, handler
   )});
}

f9sv_CAPI_FN(fon9_CStrView)
f9sv_GridView_Parser(const f9sv_ClientReport* rpt, fon9_CStrView* exResult) {
   if (exResult->Begin_ == exResult->End_)
      return fon9_CStrView{nullptr,nullptr};
   fon9::StrView  rowSeed{*exResult};
   if (const char* rowNext = rowSeed.Find(*fon9_kCSTR_ROWSPL)) {
      exResult->Begin_ = rowNext + 1;
      rowSeed.SetEnd(rowNext);
   }
   else {
      exResult->Begin_ = exResult->End_ = nullptr;
   }
   static thread_local std::string strKey{64, '\0'};
   if (const char* spl = rowSeed.Find(*fon9_kCSTR_CELLSPL)) {
      strKey.assign(rowSeed.begin(), spl);
      rowSeed.SetBegin(spl + 1);
   }
   else {
      strKey.assign(rowSeed.begin(), rowSeed.end());
      rowSeed.Reset(nullptr);
   }

   fon9::seed::SimpleRawWr wr{const_cast<struct f9sv_Seed*>(rpt->Seed_)};
   const f9sv_Tab*         tab = rpt->Tab_;
   const auto              fldCount = tab->FieldCount_;
   for (uint32_t fldid = 0; fldid < fldCount; ++fldid) {
      const f9sv_Field& fld = tab->FieldArray_[fldid];
      assert(reinterpret_cast<const fon9::seed::Field*>(fld.InternalOwner_)->GetIndex() == fld.Named_.Index_);
      reinterpret_cast<const fon9::seed::Field*>(fld.InternalOwner_)->StrToCell(wr, fon9::StrFetchNoTrim(rowSeed, *fon9_kCSTR_CELLSPL));
   }
   return fon9::ToStrView(strKey).ToCStrView();
}

f9sv_CAPI_FN(f9sv_Result)
f9sv_Write(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler, const char* strFieldValues) {
   return RcSvcReq::Run(RcSvcReqSP{new RcSvcReqExStrArg(
      fon9::StrView_cstr(strFieldValues),
      *static_cast<fon9::rc::RcClientSession*>(ses),
      f9sv_ClientLogFlag_Write, "f9sv_Write", fon9::rc::SvFuncCode::Write,
      *seedName, handler
   )});
}

f9sv_CAPI_FN(f9sv_Result)
f9sv_Command(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler, const char* strArgs) {
   return RcSvcReq::Run(RcSvcReqSP{new RcSvcReqExStrArg(
      fon9::StrView_cstr(strArgs),
      *static_cast<fon9::rc::RcClientSession*>(ses),
      f9sv_ClientLogFlag_Command, "f9sv_Command", fon9::rc::SvFuncCode::Command,
      *seedName, handler
   )});
}

f9sv_CAPI_FN(f9sv_Result)
f9sv_Remove(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler) {
   return RcSvcReq::Run(RcSvcReqSP{new RcSvcReqSeed(
      *static_cast<fon9::rc::RcClientSession*>(ses),
      f9sv_ClientLogFlag_Remove, "f9sv_Remove", fon9::rc::SvFuncCode::Remove,
      *seedName, handler
   )});
}

f9sv_CAPI_FN(f9sv_Result)
f9sv_Subscribe(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler) {
   // TODO: 訂閱 seedName->SeedKey_ 是否允許使用 "*" or 其他 filter 規則?
   return RcSvcReq::Run(RcSvcReqSP{new RcSvcReqSubr(
      *static_cast<fon9::rc::RcClientSession*>(ses),
      f9sv_ClientLogFlag_Subscribe, "f9sv_Subscribe", fon9::rc::SvFuncCode::Subscribe,
      *seedName, handler
   )});
}

f9sv_CAPI_FN(f9sv_Result)
f9sv_Unsubscribe(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler) {
   return RcSvcReq::Run(RcSvcReqSP{new RcSvcReqUnsubr(
      *static_cast<fon9::rc::RcClientSession*>(ses),
      f9sv_ClientLogFlag_Subscribe, "f9sv_Unsubscribe", fon9::rc::SvFuncCode::Unsubscribe,
      *seedName, handler
   )});
}

f9sv_CAPI_FN(const char*)
f9sv_GetSvResultMessage(f9sv_Result res) {
   static_assert(f9sv_Result_AccessDenied == fon9::rc::ToResultC(fon9::seed::OpResult::access_denied), "");
   static_assert(f9sv_Result_NotFoundTab == fon9::rc::ToResultC(fon9::seed::OpResult::not_found_tab), "");
   static_assert(f9sv_Result_PathFormatError == fon9::rc::ToResultC(fon9::seed::OpResult::path_format_error), "");

   #define case_return(r)  case f9sv_Result_##r:    return #r;
   switch (res) {
      case f9sv_Result_RemovePod:
      case f9sv_Result_RemoveSeed:
         // 使用 GetOpResultMessage();
         break;
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
      case_return(NotFoundTreeOrKey);
      case_return(PathFormatError);

      case_return(BadInitParams);
      case_return(LinkBroken);
      case_return(FlowControl);
      case_return(InQuery);
      case_return(OverMaxSubrCount);
      case_return(InSubscribe);
      case_return(InUnsubscribe);
      case_return(InWrite);
      case_return(InRemove);
      case_return(InCommand);
      case_return(InGridView);
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
