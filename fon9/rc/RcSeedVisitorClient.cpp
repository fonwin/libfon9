// \file fon9/rc/RcSeedVisitorClient.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSeedVisitorClientReq.hpp"
#include "fon9/rc/RcSeedVisitor.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 {

inline bool IsBitvIntegerNeg(const DcQueue& buf) {
   if (const byte* p1 = buf.Peek1())
      return ((*p1 & fon9_BitvT_Mask) == fon9_BitvT_IntegerNeg);
   return false;
}
static void NamedToC(f9sv_Named& cNamed, const NamedIx& named) {
   cNamed.Name_ = ToStrView(named.Name_).ToCStrView();
   cNamed.Title_ = ToStrView(named.GetTitle()).ToCStrView();
   cNamed.Description_ = ToStrView(named.GetDescription()).ToCStrView();
   cNamed.Index_ = named.GetIndex();
}
static void FieldToC(f9sv_Field& cfld, const seed::Field& fld) {
   NamedToC(cfld.Named_, fld);
   NumOutBuf nbuf;
   fld.GetTypeId(nbuf).ToString(cfld.TypeId_);
}

//--------------------------------------------------------------------------//
namespace rc {

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
void RcSeedVisitorClientNote::OnSessionLinkBroken(RcClientSession& ses) {
   // TODO: 是否需要保留訂閱設定, 重新連線後自動重新訂閱?
   this->ClearRequests(ses);
}
//--------------------------------------------------------------------------//
struct RcSeedVisitorClientNote::RejectRequest {
   f9sv_ClientReport rpt;
   f9sv_Tab          tab;
   RejectRequest(f9sv_Result res) {
      ZeroStruct(rpt);
      ZeroStruct(tab);
      rpt.ResultCode_ = res;
   }
   void RejectTree(RcClientSession& ses, StrView treePath, TreeRec& tree) {
      rpt.TreePath_ = treePath.ToCStrView();
      rpt.Layout_ = &tree.LayoutC_;
      rpt.Tab_ = &tab;
      for (auto& req : tree.PendingReqs_) {
         if (req.Handler_.FnOnReport_ == nullptr)
            continue;
         rpt.SeedKey_ = ToStrView(req.SeedKey_).ToCStrView();
         rpt.UserData_ = req.Handler_.UserData_;
         tab.Named_.Name_ = ToStrView(req.TabName_).ToCStrView();
         tab.Named_.Index_ = static_cast<int>(req.TabIndex_);
         req.Handler_.FnOnReport_(&ses, &rpt);
      }
      for (auto& pod : tree.PodMap_) {
         for (unsigned L = rpt.Layout_->TabCount_; L > 0;) {
            SeedRec* seed = pod.second.Seeds_[--L].get();
            if (seed->Handler_.FnOnReport_ == nullptr)
               continue;
            rpt.SeedKey_ = ToStrView(pod.first).ToCStrView();
            rpt.UserData_ = seed->Handler_.UserData_;
            rpt.Tab_ = &tree.LayoutC_.TabList_[L];
            seed->Handler_.FnOnReport_(&ses, &rpt);
         }
      }
   }
};
void RcSeedVisitorClientNote::ClearRequests(RcClientSession& ses) {
   RejectRequest  rej{f9sv_Result_LinkBroken};
   auto           maplk = this->TreeMap_.Lock();
   for (auto& itree : *maplk) {
      rej.RejectTree(ses, ToStrView(itree.first), itree.second);
   }
   maplk->clear();
}
//--------------------------------------------------------------------------//
RcSeedVisitorClientNote::PodRec& RcSeedVisitorClientNote::TreeRec::FetchPod(StrView seedKey, bool* isNew) {
   PodRec& pod = this->PodMap_[CharVector::MakeRef(seedKey)];
   if (pod.Seeds_ != nullptr) {
      if (isNew)
         *isNew = false;
   }
   else {
      if (isNew)
         *isNew = true;
      pod.Seeds_ = new SeedSP[this->LayoutC_.TabCount_];
      for (f9sv_TabSize tabidx = this->LayoutC_.TabCount_; tabidx > 0;) {
         --tabidx;
         pod.Seeds_[tabidx].reset(SeedRec::MakeSeedRec(*this->Layout_->GetTab(tabidx)));
      }
   }
   return pod;
}
//--------------------------------------------------------------------------//
void RcSeedVisitorClientNote::ParseLayout(RcClientSession& ses, TreeMapImpl::value_type& itree, DcQueue& rxbuf) {
   std::string   strLayout;
   BitvTo(rxbuf, strLayout);
   StrView  cfgstr{&strLayout};
   StrTrimTail(&cfgstr);
   if (ses.LogFlags_ & (f9sv_ClientLogFlag_Query | f9sv_ClientLogFlag_Subscribe | f9sv_ClientLogFlag_QueryData | f9sv_ClientLogFlag_SubrData))
      fon9_LOG_INFO("f9sv_Layout|path=", itree.first, "|layout=[", cfgstr, ']');

   seed::TreeFlag treeFlags = static_cast<seed::TreeFlag>(HIntStrTo(StrFetchNoTrim(cfgstr, '|')));
   seed::FieldSP  fldKey{seed::MakeField(cfgstr, '|', '\n')};
   std::vector<seed::TabSP> tabs;
   while (!StrTrimHead(&cfgstr).empty()) {
      seed::TabFlag tabFlags = static_cast<seed::TabFlag>(HIntStrTo(StrFetchNoTrim(cfgstr, '|')));
      Named         tabNamed{DeserializeNamed(cfgstr, '|', '\n')};
      StrView       fldcfg = SbrTrimHeadFetchInside(cfgstr);
      seed::Fields  fields;
      seed::MakeFields(fldcfg, '|', '\n', fields);
      tabs.emplace_back(new seed::Tab{tabNamed, std::move(fields), tabFlags});
   }
   seed::LayoutSP layout{new seed::LayoutN{std::move(fldKey), treeFlags, std::move(tabs)}};
   TreeRec&       tree = itree.second;
   FieldToC(tree.LayoutC_.KeyField_, *layout->KeyField_);
   tree.LayoutC_.TabCount_ = static_cast<f9sv_TabSize>(layout->GetTabCount());
   tree.TabList_.resize(tree.LayoutC_.TabCount_);
   f9sv_TabSize tabidx = 0;
   for (TabC& ctab : tree.TabList_) {
      seed::Tab* stab = layout->GetTab(tabidx++);
      NamedToC(ctab.Named_, *stab);
      ctab.FieldCount_ = static_cast<unsigned>(stab->Fields_.size());
      ctab.Fields_.resize(ctab.FieldCount_);
      ctab.FieldArray_ = &*ctab.Fields_.begin();
      unsigned fldidx = 0;
      while (const auto* fld = stab->Fields_.Get(fldidx)) {
         auto* cfld = &ctab.Fields_[fldidx];
         FieldToC(*cfld, *fld);
         cfld->InternalOwner_ = fld;
         ++fldidx;
      }
   }
   tree.LayoutC_.TabList_ = &*tree.TabList_.begin();
   tree.Layout_ = std::move(layout);

   // 取得了 layout 之後, tree.PendingReqs_[0] 填入 pod(seed), 送出剩餘的要求.
   if (tree.PendingReqs_.empty())
      return;
   PendingReq* req = &tree.PendingReqs_.front();
   PodRec&     pod = tree.FetchPod(ToStrView(req->SeedKey_));
   if (!req->TabName_.empty()) {
      if (IsTabAll(req->TabName_.begin())) {
         for (f9sv_TabSize L = tree.LayoutC_.TabCount_; L > 0;)
            req->HandlerToSeed(*pod.Seeds_[--L]);
         goto __POP_FRONT_AND_SEND_REQS;
      }
      auto* stab = tree.Layout_->GetTab(ToStrView(req->TabName_));
      req->TabIndex_ = (stab ? stab->GetIndex() : kTabAll);
   }
   if (req->TabIndex_ < tree.LayoutC_.TabCount_)
      req->HandlerToSeed(*pod.Seeds_[req->TabIndex_]);
   else if (req->Handler_.FnOnReport_) {
      f9sv_ClientReport rpt;
      f9sv_Tab          tab;
      ZeroStruct(rpt);
      ZeroStruct(tab);
      rpt.ResultCode_ = ToResultC(seed::OpResult::not_found_tab);
      rpt.UserData_ = req->Handler_.UserData_;
      rpt.TreePath_ = ToStrView(itree.first).ToCStrView();
      rpt.SeedKey_ = ToStrView(req->SeedKey_).ToCStrView();
      rpt.Tab_ = &tab;
      tab.Named_.Name_  = ToStrView(req->TabName_).ToCStrView();
      tab.Named_.Index_ = static_cast<int>(req->TabName_.empty() ? req->TabIndex_ : 0);
      req->Handler_.FnOnReport_(&ses, &rpt);
   }
__POP_FRONT_AND_SEND_REQS:;
   for (;;) {
      tree.PendingReqs_.pop_front();
      if (tree.PendingReqs_.empty())
         break;
      req = &tree.PendingReqs_.front();
      if (IsEnumContains(req->SvFunc_, SvFuncCode::Query)) {
         RcSvClientRequest creq(ses, "f9sv_Query(p)", SvFuncCode::Query,
                                (ses.LogFlags_ & f9sv_ClientLogFlag_Query) != f9rc_ClientLogFlag{},
                                *this, ToStrView(itree.first), *req);
         creq.FinalCheckSendQuery(creq.FetchSeedSt(itree.second));
      }
      else {
         if (0); // subscribe / unsubscribe
      }
   }
}
//--------------------------------------------------------------------------//
RcSeedVisitorClientNote::~RcSeedVisitorClientNote() {
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
            assert(fldValues[fldPath->Index_].empty());
            StrView fc = fldValues[fldRights->Index_];
            this->Ac_.FcQuery_.OnValue(fc);
            this->Ac_.MaxSubrCount_ = StrTo(fldValues[fldMaxSubrCount->Index_], this->Ac_.MaxSubrCount_);
            seed::AccessControl ac;
            for (size_t L = 0; L < count; ++L) {
               fldValues = tab->RecList_[L];
               ac.Rights_ = static_cast<seed::AccessRight>(HIntStrTo(fldValues[fldRights->Index_]));
               ac.MaxSubrCount_ = StrTo(fldValues[fldMaxSubrCount->Index_], 0u);
               this->Ac_.Acl_.emplace(CharVector{fldValues[fldPath->Index_]}, ac);
            }
         }
      }
   }
   this->Config_.FcQryCount_ = this->Ac_.FcQuery_.FcCount_;
   this->Config_.FcQryMS_ = this->Ac_.FcQuery_.FcTimeMS_;
   this->Config_.MaxSubrCount_ = this->Ac_.MaxSubrCount_;
   if (ses.LogFlags_ & f9sv_ClientLogFlag_Config) {
      fon9_LOG_INFO("f9sv_Config",
                    "|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&ses)),
                    "|cfg={", this->OrigGvTablesStr_, '}');
   }
   if (this->Params_.FnOnConfig_)
      this->Params_.FnOnConfig_(&ses, &this->Config_);
}
void RcSeedVisitorClientNote::OnRecvQueryAck(RcClientSession& ses, DcQueue& rxbuf, SvFuncCode fcAck) {
   RcSvReqKey reqKey;
   reqKey.LoadFrom(rxbuf);
   // -----
   auto     maplk = this->TreeMap_.Lock();
   auto&    itree = maplk->kfetch(reqKey.TreePath_);
   TreeRec& tree = itree.second;
   if (IsBitvIntegerNeg(rxbuf)) { // TreePath 查詢失敗.
      f9sv_Result res{};
      BitvTo(rxbuf, res);
      if (ses.LogFlags_ & f9sv_ClientLogFlag_Query)
         fon9_LOG_ERROR("f9sv_QueryAck|", reqKey, "|err=", res);
      RejectRequest{res}.RejectTree(ses, ToStrView(itree.first), tree);
      return;
   }
   // -----
   if (IsEnumContains(fcAck, SvFuncCode::FlagLayout))
      this->ParseLayout(ses, itree, rxbuf);
   // -----
   f9sv_ClientReport rpt;
   ZeroStruct(rpt);
   rpt.TreePath_ = ToStrView(itree.first).ToCStrView();
   rpt.Layout_ = &tree.LayoutC_;
   rpt.SeedKey_ = ToStrView(reqKey.SeedKey_).ToCStrView();
   // -----
   auto&        pod = tree.FetchPod(ToStrView(reqKey.SeedKey_));
   f9sv_TabSize tabCount = reqKey.IsAllTabs_ ? tree.LayoutC_.TabCount_ : 1;
   for (f9sv_TabSize L = 0; L < tabCount; ++L) {
      f9sv_TabSize  tabidx{};
      BitvTo(rxbuf, tabidx);
      if (tabidx >= tree.LayoutC_.TabCount_)
         continue;
      rpt.Tab_ = &tree.TabList_[tabidx];
      SeedRec* seed = pod.Seeds_[tabidx].get();
      if (IsBitvIntegerNeg(rxbuf)) { // 查詢失敗.
         BitvTo(rxbuf, rpt.ResultCode_);
         if (ses.LogFlags_ & f9sv_ClientLogFlag_Query)
            fon9_LOG_ERROR("f9sv_QueryAck|", reqKey, "|err=", rpt.ResultCode_);
      }
      else {
         rpt.ResultCode_ = f9sv_Result_NoError;
         std::string fldValues;
         BitvTo(rxbuf, fldValues);
         if (0);// TODO: 目前暫時使用[文字]傳輸 seed 內容, 應改成 Bitv 格式.
         StrView vals{&fldValues};
         StrFetchNoTrim(vals, '\x01'); // remove keyText;
         seed::SimpleRawWr   wr{seed};
         const seed::Fields& flds = tree.Layout_->GetTab(tabidx)->Fields_;
         unsigned            fldidx = 0;
         while (!vals.empty()) {
            flds.Get(fldidx++)->StrToCell(wr, StrFetchNoTrim(vals, '\x01'));
         }
         if (ses.LogFlags_ & f9sv_ClientLogFlag_QueryData) {
            if (fon9_UNLIKELY(LogLevel::Info >= LogLevel_)) {
               RevBufferList rbuf_{fon9::kLogBlockNodeSize};
               RevPutChar(rbuf_, '\n');
               seed::FieldsCellRevPrint(flds, wr, rbuf_);
               RevPrint(rbuf_, "f9sv_QueryData|", reqKey, '|');
               LogWrite(LogLevel::Info, std::move(rbuf_));
            }
         }
      }
      assert(seed != nullptr);
      seed->CurrSt_ = SvFuncCode{};
      if (auto fnOnReport = seed->Handler_.FnOnReport_) {
         seed->Handler_.FnOnReport_ = nullptr;
         rpt.UserData_ = seed->Handler_.UserData_;
         rpt.Seed_ = seed;
         fnOnReport(&ses, &rpt);
      }
   }
}
void RcSeedVisitorClientNote::OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) {
   assert(dynamic_cast<RcClientSession*>(&ses) != nullptr);
   static_assert(sizeof(SvFuncCode) == 1, "sizeof(SvFuncCode) != 1, 不可使用 Peek1().");
   const void* fcPtr = param.RecvBuffer_.Peek1();
   if (fcPtr == nullptr || param.RemainParamSize_ < sizeof(SvFuncCode))
      return;
   param.RemainParamSize_ -= sizeof(SvFuncCode);
   const SvFuncCode fcAck = GetBigEndian<SvFuncCode>(fcPtr);
   param.RecvBuffer_.PopConsumed(sizeof(SvFuncCode));

   fon9_WARN_DISABLE_SWITCH;
   switch (fcAck & SvFuncCode::FuncMask) {
   case SvFuncCode::Acl:
      this->OnRecvAclConfig(*static_cast<RcClientSession*>(&ses), param);
      break;
   case SvFuncCode::Subscribe:
      // - 訂閱成功? 取消訂閱成功?
      // - 訊息通知:
      //    ClientSubrId(unsigned); // 訂閱要求時 client 端提供.
      //    Key(訂閱 tree 才會提供, 訂閱 seed 則不提供);
      break;
   case SvFuncCode::Query:
      this->OnRecvQueryAck(*static_cast<RcClientSession*>(&ses), param.RecvBuffer_, fcAck);
      break;
   }
   fon9_WARN_POP;
}

} } // namespaces
//-////////////////////////////////////////////////////////////////////////-//
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
      f9sv_Result OnBeforeSetupSeedSt() override {
         fon9::TimeInterval fcWait = this->Note_->FcQry_.Fetch();
         if (fon9_LIKELY(fcWait.GetOrigValue() <= 0))
            return f9sv_Result_NoError;
         this->ExtMsg_ = "|err=Flow control";
         return static_cast<f9sv_Result>(fcWait.ShiftUnit<3>() + 1);
      }
   };
   Request  req(*static_cast<fon9::rc::RcClientSession*>(ses), "f9sv_Query", fon9::rc::SvFuncCode::Query,
                (ses->LogFlags_ & f9sv_ClientLogFlag_Query) != f9rc_ClientLogFlag{},
                *seedName, handler);
   return req.FinalCheckSendQuery(req.CheckApiSeedName());
}

// f9sv_CAPI_FN(f9sv_Result)
// f9sv_Subscribe(f9rc_ClientSession* ses, const f9sv_SeedName* seedName, f9sv_ReportHandler handler) {
// }
// 
// f9sv_CAPI_FN(void)
// f9sv_Unsubscribe(f9rc_ClientSession* ses, const f9sv_SeedName* seedName) {
// }

//--------------------------------------------------------------------------//

f9sv_CAPI_FN(const char*)
f9sv_GetField_Str(const struct f9sv_Seed* seed, const f9sv_Field* fld, char* outbuf, unsigned* bufsz) {
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
   *bufsz = static_cast<unsigned>(outsz + 1);
   return outbuf;
}

} // extern "C"
