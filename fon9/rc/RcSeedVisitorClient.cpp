// \file fon9/rc/RcSeedVisitorClient.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSeedVisitorClient.hpp"
#include "fon9/rc/RcSvcAck.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace rc {
using namespace fon9::rc::svc;

static void ParseStrValuesToSeed(StrView vals, const seed::RawWr& wr, const seed::Fields& flds) {
   unsigned fldidx = 0;
   const seed::Field* fld;
   while (!vals.empty()) {
      if ((fld = flds.Get(fldidx++)) == nullptr)
         return;
      fld->StrToCell(wr, StrFetchNoTrim(vals, '\x01'));
   }
   while ((fld = flds.Get(fldidx++)) != nullptr)
      fld->StrToCell(wr, StrView{});
}
void RcSvcNotifySeeds(f9sv_FnOnReport fnOnReport, f9rc_ClientSession* ses, f9sv_ClientReport* rpt,
                      StrView gv, const seed::RawWr& wr, const seed::Fields& flds, bool hasKeyText) {
   rpt->ResultCode_ = f9sv_Result_NoError;
   while (!gv.empty()) {
      StrView fldValues = StrFetchNoTrim(gv, *fon9_kCSTR_ROWSPL);
      if (hasKeyText) {
         rpt->SeedKey_ = StrFetchNoTrim(fldValues, '\x01').ToCStrView();
         *const_cast<char*>(rpt->SeedKey_.End_) = '\0';
      }
      ParseStrValuesToSeed(fldValues, wr, flds);
      fnOnReport(ses, rpt);
   }
}
//--------------------------------------------------------------------------//
RcSeedVisitorClientAgent::~RcSeedVisitorClientAgent() {
}
void RcSeedVisitorClientAgent::OnSessionCtor(RcClientSession& ses, const f9rc_ClientSessionParams* params) {
   if (f9rc_FunctionNoteParams* p = params->FunctionNoteParams_[f9rc_FunctionCode_SeedVisitor]) {
      f9sv_ClientSessionParams& f9svRcParams = ContainerOf(*p, &f9sv_ClientSessionParams::BaseParams_);
      assert(f9svRcParams.BaseParams_.FunctionCode_ == f9rc_FunctionCode_SeedVisitor
             && f9svRcParams.BaseParams_.ParamSize_ == sizeof(f9svRcParams));
      ses.ResetNote(f9rc_FunctionCode_SeedVisitor, RcFunctionNoteSP{new RcSeedVisitorClientNote{f9svRcParams}});
   }
}
void RcSeedVisitorClientAgent::OnSessionApReady(RcSession& ses) {
   // 送出查詢 f9sv_ClientConfig 訊息.
   if (auto note = static_cast<RcSeedVisitorClientNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor))) {
      assert(dynamic_cast<RcSeedVisitorClientNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor)) != nullptr);
      assert(dynamic_cast<RcClientSession*>(&ses) != nullptr);
      note->ClearRequests(*static_cast<RcClientSession*>(&ses));
      RevBufferList rbuf{16}; // 送一個空訊息, 等候 SvFuncCode::Acl.
      ses.Send(f9rc_FunctionCode_SeedVisitor, std::move(rbuf));
   }
}
void RcSeedVisitorClientAgent::OnSessionLinkBroken(RcSession& ses) {
   if (auto note = static_cast<RcSeedVisitorClientNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor))) {
      assert(dynamic_cast<RcSeedVisitorClientNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor)) != nullptr);
      assert(dynamic_cast<RcClientSession*>(&ses) != nullptr);
      note->OnSessionLinkBroken(*static_cast<RcClientSession*>(&ses));
   }
}
//--------------------------------------------------------------------------//
void RcSeedVisitorClientNote::ClearRequests(RcClientSession& ses) {
   RcSvcTreeRejector rej{f9sv_Result_LinkBroken};
   auto              maplk = this->TreeMap_.Lock();
   for (auto& itree : *maplk) {
      rej.Run(maplk, ses, *itree.second);
   }
   maplk->clear();
}
void RcSeedVisitorClientNote::OnSessionLinkBroken(RcClientSession& ses) {
   // 不保留訂閱設定, 重新連線後, 由使用者自行重新訂閱.
   this->ClearRequests(ses);
}
//--------------------------------------------------------------------------//
RcSeedVisitorClientNote::~RcSeedVisitorClientNote() {
}
//--------------------------------------------------------------------------//
void RcSeedVisitorClientNote::OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) {
   assert(dynamic_cast<RcClientSession*>(&ses) != nullptr);
   static_assert(sizeof(SvFunc) == 1, "sizeof(SvFunc) != 1, 不可使用 Peek1().");
   const void* fcPtr = param.RecvBuffer_.Peek1();
   if (fcPtr == nullptr || param.RemainParamSize_ < sizeof(SvFunc))
      return;
   const SvFunc fcAck = GetBigEndian<SvFunc>(fcPtr);
   param.RecvBuffer_.PopConsumed(sizeof(SvFunc));
   param.RemainParamSize_ -= sizeof(SvFunc);

   switch (GetSvFuncCode(fcAck)) {
   default:
   case SvFuncCode::Empty: // Unknown SvFuncCode;
      param.RemoveRemainParam();
      return;
   case SvFuncCode::Acl:
      this->OnRecvAclConfig(*static_cast<RcClientSession*>(&ses), param);
      return;
   case SvFuncCode::SubscribeData:
      this->OnRecvSubrData(*static_cast<RcClientSession*>(&ses), param.RecvBuffer_, fcAck);
      return;
   case SvFuncCode::Unsubscribe:
      this->OnRecvUnsubrAck(*static_cast<RcClientSession*>(&ses), param.RecvBuffer_);
      return;
   // ----------
   #define caseAckName(name)  case SvFuncCode::name:                                                        \
      RcSvcAck##name{this->TreeMap_, *static_cast<RcClientSession*>(&ses), param.RecvBuffer_, fcAck}.Run(); \
      break
   // ----------
      caseAckName(Query);
      caseAckName(Subscribe);
      caseAckName(Write);
      caseAckName(Remove);
      caseAckName(Command);
      caseAckName(GridView);
   #undef caseAckName
   // ----------
   }
}
void RcSeedVisitorClientNote::OnRecvSubrData(RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck) {
   svc::RxSubrData rx{ses, fcAck, this->LockTree(), rxbuf};
   if (rx.SubrRec_ == nullptr)
      return;

   f9sv_ClientReport rpt;
   ZeroStruct(rpt);
   rpt.TreePath_ = ToStrView(rx.SubrRec_->Tree_->TreePath_).ToCStrView();
   rpt.Layout_   = &rx.SubrRec_->Tree_->LayoutC_;
   rpt.Tab_      = &rx.SubrRec_->Tree_->LayoutC_.TabArray_[rx.SubrRec_->TabIndex_];
   rpt.UserData_ = rx.SubrRec_->SubrHandler_.UserData_;

   auto  fnOnReport = rx.SubrRec_->SubrHandler_.FnOnReport_;
   void (RcSvStreamDecoder::*fnStreamDecoder)(svc::RxSubrData& rx, f9sv_ClientReport& rpt);
   switch (rx.NotifyKind_) {
   case seed::SeedNotifyKind::SubscribeOK:
   case seed::SeedNotifyKind::SubscribeStreamOK:
      // 訂閱成功會在 RcSvcAckBase::Run() 回覆處理, 不會來到此處.
   default: // Unknown NotifyKind.
      ses.ForceLogout("Unknown SubscribeData NotifyKind.");
      return;
      // ------------------------------------
   case seed::SeedNotifyKind::StreamRecover:
      rpt.ResultCode_ = f9sv_Result_SubrStreamRecover;
      fnStreamDecoder = &RcSvStreamDecoder::DecodeStreamRecover;
      goto __DECODE_STREAM;
   case seed::SeedNotifyKind::StreamRecoverEnd:
      rpt.ResultCode_ = f9sv_Result_SubrStreamRecoverEnd;
      fnStreamDecoder = &RcSvStreamDecoder::DecodeStreamRecoverEnd;
      goto __DECODE_STREAM;
   case seed::SeedNotifyKind::StreamEnd:
      rpt.ResultCode_ = f9sv_Result_SubrStreamEnd;
      fnStreamDecoder = &RcSvStreamDecoder::DecodeStreamEnd;
      goto __DECODE_STREAM;
   case seed::SeedNotifyKind::StreamData:
      fnStreamDecoder = &RcSvStreamDecoder::DecodeStreamData;
   __DECODE_STREAM:;
      assert(rx.SeedRec_->StreamDecoderNote_.get() != nullptr);
      rpt.SeedKey_ = rx.CheckLoadSeedKey(rxbuf).ToCStrView();
      BitvTo(rxbuf, rx.Gv_); // 將 stream data 載入, 但不寫 log, (使用 rx.LoadGv() 會自動寫入 log).
      (rx.SubrRec_->Tree_->TabArray_[rx.SubrRec_->TabIndex_].StreamDecoder_.get()
       ->*(fnStreamDecoder))(rx, rpt);
      if (rx.NotifyKind_ == seed::SeedNotifyKind::StreamEnd)
         rx.RemoveSubscribe();
      return;
      // ------------------------------------
   case seed::SeedNotifyKind::TableChanged:
      // 只有訂閱 tree 才會收到此通知, 此時應更新整個 tree 的內容.
      assert(seed::IsSubrTree(rx.SubrRec_->SeedKey_.begin()));
      rpt.SeedKey_ = ToStrView(rx.SubrRec_->SeedKey_).ToCStrView();
      rx.LoadGv(rxbuf);
      rx.FlushLog();
      if (fnOnReport == nullptr)
         return;
      // 先通知 TableChanged.
      rpt.ResultCode_ = f9sv_Result_SubrNotifyTableChanged;
      fnOnReport(&ses, &rpt);
      // TableChanged 之後, 再通知 seed 內容.
      rpt.Seed_ = rx.SeedRec_;
      RcSvcNotifySeeds(fnOnReport, &ses, &rpt, &rx.Gv_, seed::SimpleRawWr{rx.SeedRec_}, rx.SubrTab_->Fields_, true);
      return;
      // ------------------------------------
   case seed::SeedNotifyKind::SeedChanged:
      rpt.SeedKey_ = rx.CheckLoadSeedKey(rxbuf).ToCStrView();
      rx.LoadGv(rxbuf);
      ParseStrValuesToSeed(&rx.Gv_, seed::SimpleRawWr{rx.SeedRec_}, rx.SubrTab_->Fields_);
      rpt.Seed_ = rx.SeedRec_;
      break;
      // ------------------------------------
   case seed::SeedNotifyKind::PodRemoved:
   case seed::SeedNotifyKind::SeedRemoved:
      rpt.SeedKey_ = rx.CheckLoadSeedKey(rxbuf).ToCStrView();
      if (seed::IsSubrTree(rx.SubrRec_->SeedKey_.begin())) {
         rpt.ResultCode_ = (rx.NotifyKind_ == seed::SeedNotifyKind::PodRemoved
                            ? f9sv_Result_SubrNotifyPodRemoved
                            : f9sv_Result_SubrNotifySeedRemoved);
         break;
      }
      // 單一 pod/seed 被移除, 不會再收到事件.
      // 所以不用 break; 繼續處理 [移除訂閱].
      /* fall through */
   case seed::SeedNotifyKind::ParentSeedClear:
      // 收到此通知之後, 訂閱會被清除, 不會再收到事件.
      rpt.ResultCode_ = f9sv_Result_SubrNotifyUnsubscribed;
      rx.RemoveSubscribe();
      break;
   }
   rx.FlushLog();
   if (fnOnReport)
      fnOnReport(&ses, &rpt);
}
void RcSeedVisitorClientNote::OnRecvAclConfig(RcClientSession& ses, RcFunctionParam& param) {
   this->OrigGvTablesStr_.resize(param.RemainParamSize_);
   if (param.RemainParamSize_)
      param.RecvBuffer_.Read(&*this->OrigGvTablesStr_.begin(), param.RemainParamSize_);
   SvParseGvTablesC(this->Config_.RightsTables_, this->ConfigGvTables_, this->ConfigGvTablesStr_, &this->OrigGvTablesStr_);

   this->Ac_.Clear();
   if (const rc::SvGvTable* tab = this->ConfigGvTables_.Get("PoAcl")) {
      if (size_t count = tab->RecList_.size()) {
         const auto* fldPath = tab->Fields_.Get("Path");
         const auto* fldRights = tab->Fields_.Get("Rights");
         const auto* fldMaxSubrCount = tab->Fields_.Get("MaxSubrCount");
         if (fldPath && fldRights && fldMaxSubrCount) {
            // FcQryCount/FcQryMS & MaxSubrCount;
            const StrView* fldValues = tab->RecList_[--count];
            assert(fldValues[fldPath->GetIndex()].empty());
            StrView fc = fldValues[fldRights->GetIndex()];
            this->Ac_.FcQuery_.OnValue(fc);
            this->Ac_.MaxSubrCount_ = StrTo(fldValues[fldMaxSubrCount->GetIndex()], this->Ac_.MaxSubrCount_);
            seed::AccessControl ac;
            for (size_t L = 0; L < count; ++L) {
               fldValues = tab->RecList_[L];
               ac.Rights_ = static_cast<seed::AccessRight>(HIntStrTo(fldValues[fldRights->GetIndex()]));
               ac.MaxSubrCount_ = StrTo(fldValues[fldMaxSubrCount->GetIndex()], 0u);
               this->Ac_.Acl_.emplace(CharVector{fldValues[fldPath->GetIndex()]}, ac);
            }
         }
      }
   }
   this->Config_.FcQryCount_   = this->Ac_.FcQuery_.FcCount_;
   this->Config_.FcQryMS_      = this->Ac_.FcQuery_.FcTimeMS_;
   this->Config_.MaxSubrCount_ = this->Ac_.MaxSubrCount_;
   {  // protect unsafe members.
      auto  maplk = this->TreeMap_.Lock();
      this->FcQry_.Resize(this->Ac_.FcQuery_);
   }  // ----
   if (ses.LogFlags_ & f9sv_ClientLogFlag_Config) {
      fon9_LOG_INFO("f9sv_Config",
                    "|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&ses)),
                    "|cfg={", this->OrigGvTablesStr_, '}');
   }
   if (this->Params_.FnOnConfig_)
      this->Params_.FnOnConfig_(&ses, &this->Config_);
}
void RcSeedVisitorClientNote::OnRecvUnsubrAck(RcClientSession& ses, DcQueue& rxbuf) {
   RevBufferList rbuf_{fon9::kLogBlockNodeSize};
   const bool isNeedsLog = (LogLevel::Info >= LogLevel_
                            && f9rc_ClientLogFlag{} != (ses.LogFlags_ & f9sv_ClientLogFlag_Subscribe));
   f9sv_SubrIndex usidx{kSubrIndexNull};
   BitvTo(rxbuf, usidx);
   if (isNeedsLog)
      RevPrint(rbuf_, "|subrIndex=", usidx, '\n');
   TreeLocker maplk = this->LockTree();
   auto*      subr = maplk->SubrMap_.GetObjPtr(usidx);
   assert(subr != nullptr); // 取消訂閱, 沒有對應的 subr?
   if (isNeedsLog) {
      if (subr)
         RevPrint(rbuf_, "|treePath=", subr->Tree_->TreePath_,
                  "|seedKey=", subr->SeedKey_,
                  "|tab=", subr->TabIndex_,
                  ':', StrView{subr->Tree_->LayoutC_.TabArray_[subr->TabIndex_].Named_.Name_});
      RevPrint(rbuf_, "f9sv_Unsubscribe.Ack");
      LogWrite(LogLevel::Info, std::move(rbuf_));
   }
   if (subr == nullptr)
      return;
   assert(subr->SubrSvFunc_ == SvFuncCode::Unsubscribe);
   assert(subr->Tree_ && subr->Seeds_);
   assert(subr->TabIndex_ < subr->Tree_->LayoutC_.TabCount_);
   SeedRec* seed = subr->Seeds_[subr->TabIndex_].get();
   assert(seed && seed->SubrIndex_ == usidx);
   const auto        fnOnReport = subr->SubrHandler_.FnOnReport_;
   const auto        subrSeedKey{std::move(subr->SeedKey_)};
   f9sv_ClientReport rpt;
   ZeroStruct(rpt);
   rpt.ResultCode_ = f9sv_Result_Unsubscribed;
   rpt.TreePath_ = ToStrView(subr->Tree_->TreePath_).ToCStrView();
   rpt.Layout_ = &subr->Tree_->LayoutC_;
   rpt.Tab_ = &subr->Tree_->LayoutC_.TabArray_[subr->TabIndex_];
   rpt.SeedKey_ = ToStrView(subrSeedKey).ToCStrView();
   rpt.UserData_ = subr->SubrHandler_.UserData_;
   seed->ClearSubscriber();
   subr->ClearSvFuncSubr();
   maplk->SubrMap_.RemoveObjPtr(usidx, subr);
   if (fnOnReport)
      fnOnReport(&ses, &rpt);
}

} } // namespaces
