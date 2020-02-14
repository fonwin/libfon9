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
   cfld.Size_ = fld.Size_;
   cfld.Offset_ = fld.Offset_;
   cfld.DecScale_ = fld.DecScale_;
   cfld.Type_ = static_cast<f9sv_FieldType>(cast_to_underlying(fld.Type_));
   #define static_assert_FieldType(tname) \
      static_assert(cast_to_underlying(f9sv_FieldType_##tname) == cast_to_underlying(seed::FieldType::tname), "");
   static_assert_FieldType(Unknown);
   static_assert_FieldType(Bytes);
   static_assert_FieldType(Chars);
   static_assert_FieldType(Integer);
   static_assert_FieldType(Decimal);
   static_assert_FieldType(TimeStamp);
   static_assert_FieldType(TimeInterval);
   static_assert_FieldType(DayTime);
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
//--------------------------------------------------------------------------//
struct RcSeedVisitorClientNote::RejectRequest {
   f9sv_ClientReport rpt;
   f9sv_Tab          tab;
   RejectRequest(f9sv_Result res) {
      ZeroStruct(rpt);
      ZeroStruct(tab);
      rpt.ResultCode_ = res;
   }
   void RejectTree(TreeLocker& maplk, RcClientSession& ses, TreeRec& tree) {
      rpt.TreePath_ = ToStrView(tree.TreePath_).ToCStrView();
      rpt.Layout_   = &tree.LayoutC_;
      rpt.Tab_      = &tab;
      for (auto& preq : tree.PendingReqs_) {
         maplk->SubrMap_.RemoveObjPtr(preq.SubrIndex_, nullptr);
         if (preq.Handler_.FnOnReport_ == nullptr)
            continue;
         rpt.SeedKey_  = ToStrView(preq.SeedKey_).ToCStrView();
         rpt.UserData_ = preq.Handler_.UserData_;
         tab.Named_.Name_  = ToStrView(preq.TabName_).ToCStrView();
         tab.Named_.Index_ = static_cast<int>(preq.TabIndex_);
         preq.Handler_.FnOnReport_(&ses, &rpt);
      }
      for (auto& pod : tree.PodMap_) {
         for (unsigned L = rpt.Layout_->TabCount_; L > 0;) {
            SeedRec* seed = pod.second.Seeds_[--L].get();
            maplk->SubrMap_.RemoveObjPtr(seed->SubrIndex_, nullptr);
            if (seed->Handler_.FnOnReport_ == nullptr)
               continue;
            rpt.SeedKey_ = ToStrView(pod.first).ToCStrView();
            rpt.UserData_ = seed->Handler_.UserData_;
            rpt.Tab_ = &tree.LayoutC_.TabList_[L];
            seed->Handler_.FnOnReport_(&ses, &rpt);
         }
      }
      tree.PendingReqs_.clear();
      tree.PodMap_.clear();
   }
};
void RcSeedVisitorClientNote::ClearRequests(RcClientSession& ses) {
   RejectRequest  rej{f9sv_Result_LinkBroken};
   auto           maplk = this->TreeMap_.Lock();
   for (auto& itree : *maplk) {
      rej.RejectTree(maplk, ses, *itree.second);
   }
   maplk->clear();
}
void RcSeedVisitorClientNote::OnSessionLinkBroken(RcClientSession& ses) {
   // 不保留訂閱設定, 重新連線後, 由使用者自行重新訂閱.
   this->ClearRequests(ses);
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
void RcSeedVisitorClientNote::ParseLayoutAndSendPendings(const TreeLocker& locker,
                                                         RcClientSession&  ses,
                                                         TreeRec&          tree,
                                                         DcQueue&          rxbuf) {
   std::string   strLayout;
   BitvTo(rxbuf, strLayout);
   StrView  cfgstr{&strLayout};
   StrTrimTail(&cfgstr);
   if (ses.LogFlags_ & (f9sv_ClientLogFlag_Query | f9sv_ClientLogFlag_Subscribe | f9sv_ClientLogFlag_QueryData | f9sv_ClientLogFlag_SubscribeData))
      fon9_LOG_INFO("f9sv_Layout|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&ses)),
                    "|path=", tree.TreePath_, "|layout=[", cfgstr, ']');

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
         cfld->Offset_ += static_cast<int32_t>(sizeof(SeedRec)); // = SeedRec::DyMemPos_;
         cfld->InternalOwner_ = fld;
         ++fldidx;
      }
   }
   tree.LayoutC_.TabList_ = &*tree.TabList_.begin();
   tree.Layout_ = std::move(layout);
   // -----
   // 取得了 layout 之後, tree.PendingReqs_[0] 填入 pod(seed), 然後送出剩餘的要求.
   if (tree.PendingReqs_.empty())
      return;
   PendingReq* preq = &tree.PendingReqs_.front();
   PodRec&     pod  = tree.FetchPod(ToStrView(preq->SeedKey_));
   auto*       subr = locker->SubrMap_.GetObjPtr(preq->SubrIndex_);
   if (subr) {
      subr->Seeds_ = pod.Seeds_;
      subr->TabIndex_ = kTabAll;
   }
   if (!preq->TabName_.empty()) {
      if (IsTabAll(preq->TabName_.begin())) {
         for (f9sv_TabSize L = tree.LayoutC_.TabCount_; L > 0;)
            preq->AssignToSeed(*pod.Seeds_[--L]);
         goto __POP_FRONT_AND_SEND_REQS;
      }
      auto* stab = tree.Layout_->GetTab(ToStrView(preq->TabName_));
      preq->TabIndex_ = (stab ? stab->GetIndex() : kTabAll);
   }
   if (subr)
      subr->TabIndex_ = preq->TabIndex_;
   if (preq->TabIndex_ < tree.LayoutC_.TabCount_)
      preq->AssignToSeed(*pod.Seeds_[preq->TabIndex_]);
   else {
      if (ses.LogFlags_ & (subr ? f9sv_ClientLogFlag_Subscribe : f9sv_ClientLogFlag_Query))
         fon9_LOG_ERROR(subr ? StrView{"f9sv_Subscribe.Ack"} : StrView{"f9sv_Query.Ack"},
                        "|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&ses)),
                        "|path=", tree.TreePath_, "|key=", preq->SeedKey_,
                        "|tab=", (preq->TabIndex_ == kTabAll && preq->TabName_.empty()
                                  ? StrView{"*"} : ToStrView(preq->TabName_)),
                        "|err=Tab not found");
      if (subr) {
         subr->ClearSvFunc();
         locker->SubrMap_.RemoveObjPtr(preq->SubrIndex_, subr);
      }
      if (preq->Handler_.FnOnReport_) {
         f9sv_ClientReport rpt;
         f9sv_Tab          tab;
         ZeroStruct(rpt);
         ZeroStruct(tab);
         rpt.ResultCode_ = f9sv_Result_NotFoundTab;
         rpt.UserData_   = preq->Handler_.UserData_;
         rpt.TreePath_   = ToStrView(tree.TreePath_).ToCStrView();
         rpt.SeedKey_    = ToStrView(preq->SeedKey_).ToCStrView();
         rpt.Tab_        = &tab;
         tab.Named_.Name_ = ToStrView(preq->TabName_).ToCStrView();
         tab.Named_.Index_ = static_cast<int>(preq->TabName_.empty() ? preq->TabIndex_ : 0);
         preq->Handler_.FnOnReport_(&ses, &rpt);
      }
   }

__POP_FRONT_AND_SEND_REQS:;
   for (;;) {
      tree.PendingReqs_.pop_front();
      if (tree.PendingReqs_.empty())
         break;
      preq = &tree.PendingReqs_.front();
      SvFuncCode fc = GetSvFuncCode(preq->SvFunc_);
      if (fc == SvFuncCode::Query) {
         RcSvClientRequest creq(ses, f9rc_ClientLogFlag{} != (ses.LogFlags_ & f9sv_ClientLogFlag_Query),
                                "f9sv_Query(p)", preq->SvFunc_,
                                *this, tree, *preq);
         creq.FinalCheckSendRequest(creq.FetchSeedSt(locker));
      }
      else if(fc == SvFuncCode::Subscribe || fc == SvFuncCode::Unsubscribe) {
         RcSvClientRequest creq(ses, f9rc_ClientLogFlag{} != (ses.LogFlags_ & f9sv_ClientLogFlag_Subscribe),
                                fc == SvFuncCode::Subscribe
                                 ? StrView{"f9sv_Subscribe(p)"} : StrView{"f9sv_Unsubscribe(p)"},
                                preq->SvFunc_, *this, tree, *preq);
         creq.SubrIndex_ = preq->SubrIndex_;
         creq.FinalCheckSendRequest(creq.FetchSeedSt(locker));
      }
   }
}

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
static void NotifySeeds(f9sv_FnOnReport fnOnReport, f9rc_ClientSession* ses, f9sv_ClientReport* rpt,
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
RcSeedVisitorClientNote::~RcSeedVisitorClientNote() {
}
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
      break;
   case SvFuncCode::Acl:
      this->OnRecvAclConfig(*static_cast<RcClientSession*>(&ses), param);
      break;
   case SvFuncCode::Query:
      this->OnRecvQrySubrAck(*static_cast<RcClientSession*>(&ses), param.RecvBuffer_, fcAck, "Query");
      break;
   case SvFuncCode::Subscribe:
      this->OnRecvQrySubrAck(*static_cast<RcClientSession*>(&ses), param.RecvBuffer_, fcAck, "Subscribe");
      break;
   case SvFuncCode::SubscribeData:
      this->OnRecvSubrData(*static_cast<RcClientSession*>(&ses), param.RecvBuffer_, fcAck);
      break;
   case SvFuncCode::Unsubscribe:
      this->OnRecvUnsubrAck(*static_cast<RcClientSession*>(&ses), param.RecvBuffer_);
      break;
   }
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
void RcSeedVisitorClientNote::OnRecvQrySubrAck(RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck, StrView fnName) {
   RcSvReqKey reqKey;
   reqKey.LoadFrom(fcAck, rxbuf);
   // -----
   // reqKey 之後, 接下來格式, 依序如下:
   // - if(IsBitvIntegerNeg())
   //       - f9sv_Result for 錯誤碼. tree 首次查詢的失敗, 此時沒有 layout.
   //       - ### 此次的封包到此結束 ###
   // - if(HasSvFuncFlag(fcAck, SvFuncFlag::Layout))
   //       - string for layout.
   // - if(fon9_BitvV_NumberNull)
   //       - not_found_tab;
   //       - 因為此時無法把 Handler 放入對應的 seed, 所以必定已從 pending 回覆失敗了.
   //       - 在已有 layout 時的查詢、訂閱, 若 tab 不存在, 則會立即觸發失敗, 不會來到此處.
   //       - 所以 ### 此次的封包到此結束 ###
   // - if(fcCode == SvFuncCode::Subscribe)
   //       - N(>=0): 訂閱時立即回覆的 tab(seeds) 數量;
   //         避免: 如果這裡失敗, 且沒有 FlagLayout, 則會與 [tree 首次查詢失敗] 造成衝突.
   //       - f9sv_Result_NoError 
   //       - 否則 f9sv_Result for 訂閱失敗原因(此時N必定==0), ### 此次的封包到此結束 ###
   // - if(fcCode == SvFuncCode::Query)
   //       - N = (IsAllTabs ? layout.TabCount : 1);
   // - 重複 N 次(每個 tab 一次):
   //       - TabIndex + [IsBitvIntegerNeg(f9sv_Result) for 錯誤碼] or [string for SeedData]
   // -----
   const auto fcCode = GetSvFuncCode(fcAck);
   const bool isNeedsLogResult =
      f9rc_ClientLogFlag{} != (ses.LogFlags_ & (fcCode == SvFuncCode::Subscribe
                                                ? f9sv_ClientLogFlag_Subscribe
                                                : f9sv_ClientLogFlag_Query));
   auto     maplk = this->TreeMap_.Lock();
   TreeRec& tree = maplk->fetch(ToStrView(reqKey.TreePath_));
   if (IsBitvIntegerNeg(rxbuf)) { // TreePath 查詢失敗, 清除此 tree 全部的要求.
      assert(tree.Layout_.get() == nullptr && tree.PodMap_.empty());
      f9sv_Result res{};
      BitvTo(rxbuf, res);
      if (isNeedsLogResult)
         fon9_LOG_ERROR("f9sv_", fnName, ".Ack|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&ses)),
                        reqKey, "|err=", res, ':', f9sv_GetSvResultMessage(res));
      RejectRequest{res}.RejectTree(maplk, ses, tree);
      return;
   }
   // -----
   if (HasSvFuncFlag(fcAck, SvFuncFlag::Layout))
      this->ParseLayoutAndSendPendings(maplk, ses, tree, rxbuf);
   // -----
   if (const byte* tabi = rxbuf.Peek1()) {
      if (*tabi == fon9_BitvV_NumberNull) { // not_found_tab
         rxbuf.PopConsumed(1);
         return;
      }
   }
   else
      Raise<BitvNeedsMore>("OnRecvQrySubrAck: needs more");
   // -----
   const bool isNeedsLogData =
      f9rc_ClientLogFlag{} != (ses.LogFlags_ & (fcCode == SvFuncCode::Subscribe
                                                ? f9sv_ClientLogFlag_SubscribeData
                                                : f9sv_ClientLogFlag_QueryData));
   f9sv_ClientReport rpt;
   ZeroStruct(rpt);
   rpt.Layout_   = &tree.LayoutC_;
   rpt.TreePath_ = ToStrView(tree.TreePath_).ToCStrView();
   rpt.SeedKey_  = ToStrView(reqKey.SeedKey_).ToCStrView();
   // -----
   auto&        pod = tree.FetchPod(ToStrView(reqKey.SeedKey_));
   SeedRec*     seed;
   f9sv_TabSize tabCount;
   if (fcCode == SvFuncCode::Query)
      tabCount = reqKey.IsAllTabs_ ? tree.LayoutC_.TabCount_ : 1;
   else {
      assert(fcCode == SvFuncCode::Subscribe);
      // {取得訂閱結果}
      // 有多少立即回覆的 tab(seeds) 數量?
      tabCount = 0;
      BitvTo(rxbuf, tabCount);
      rpt.ResultCode_ = f9sv_Result_NoError;
      BitvTo(rxbuf, rpt.ResultCode_);
      if (isNeedsLogResult) {
         const LogLevel lv = (rpt.ResultCode_ == f9sv_Result_NoError ? LogLevel::Info : LogLevel::Error);
         fon9_LOG(lv, "f9sv_", fnName, ".Ack|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&ses)),
                  reqKey, "|res=", rpt.ResultCode_, ':', f9sv_GetSvResultMessage(rpt.ResultCode_));
      }
      auto* subr = maplk->SubrMap_.GetObjPtr(reqKey.SubrIndex_);
      assert(!reqKey.IsAllTabs_ && subr != nullptr && subr->TabIndex_ != kTabAll);
      seed = pod.Seeds_[subr->TabIndex_].get();
      rpt.Tab_ = &tree.TabList_[subr->TabIndex_];
      auto fnOnReport = seed->Handler_.FnOnReport_;
      if (rpt.ResultCode_ != f9sv_Result_NoError) {
         seed->ClearSvFunc();
         subr->ClearSvFunc();
         maplk->SubrMap_.RemoveObjPtr(reqKey.SubrIndex_, subr);
      }
      if (fnOnReport) {
         if (rpt.ResultCode_ != f9sv_Result_NoError // 訂閱失敗, 即使送出取消註冊, 也不會收到回覆, 所以在此必須通知.
             || GetSvFuncCode(seed->SvFunc_) == SvFuncCode::Subscribe) { // 訂閱成功, 則需要仍在訂閱, 才需要通知.
            rpt.Seed_     = nullptr;
            rpt.UserData_ = seed->Handler_.UserData_;
            fnOnReport(&ses, &rpt);
         }
      }
      if (rpt.ResultCode_ != f9sv_Result_NoError)
         return;
   }

   // ----- {讀取 tab(seeds) 結果}
   for (f9sv_TabSize L = 0; L < tabCount; ++L) {
      f9sv_TabSize  tabidx{kTabAll};
      BitvTo(rxbuf, tabidx);
      if (tabidx >= tree.LayoutC_.TabCount_)
         break;
      rpt.Tab_ = &tree.TabList_[tabidx];
      seed = pod.Seeds_[tabidx].get();
      assert(seed != nullptr);
      auto fnOnReport = seed->Handler_.FnOnReport_;
      rpt.UserData_ = seed->Handler_.UserData_;
      if (GetSvFuncCode(seed->SvFunc_) == SvFuncCode::Query) {
         // 查詢只要回覆一次, 所以在回覆前先清除 seed 的狀態.
         seed->ClearSvFunc();
      }
      if (IsBitvIntegerNeg(rxbuf)) {
         // 查詢失敗. (訂閱失敗不會來到 tab(seed) 這裡).
         BitvTo(rxbuf, rpt.ResultCode_);
         if (isNeedsLogResult)
            fon9_LOG_ERROR("f9sv_", fnName, ".Ack|ses=", ToPtr(static_cast<f9rc_ClientSession*>(&ses)),
                           reqKey, "|err=", rpt.ResultCode_, ':', f9sv_GetSvResultMessage(rpt.ResultCode_));
         if (fnOnReport && GetSvFuncCode(seed->SvFunc_) != SvFuncCode::Unsubscribe) {
            rpt.Seed_ = nullptr;
            fnOnReport(&ses, &rpt);
         }
         continue;
      }
      // 查詢成功 or 訂閱成功的立即回覆.
      std::string   fldValues;
      BitvTo(rxbuf, fldValues);
      const auto* tab = tree.Layout_->GetTab(tabidx);
      if (fon9_UNLIKELY(isNeedsLogData && LogLevel::Info >= LogLevel_)) {
         RevBufferList rbuf_{fon9::kLogBlockNodeSize};
         RevPrint(rbuf_, "|gv={", fldValues, "}\n");
         if (reqKey.IsAllTabs_ || reqKey.TabName_.empty())
            RevPrint(rbuf_, ':', tab->Name_, ':', tabidx);
         RevPrint(rbuf_, "f9sv_", fnName, ".Data|", reqKey);
         LogWrite(LogLevel::Info, std::move(rbuf_));
      }
      if (fnOnReport) {
         // 查詢結果: 使用[文字]傳輸 seed 內容:
         // 組裝結果在 RcSeedVisitorServer.cpp: TickRunnerQuery::OnReadOp();
         rpt.Seed_ = seed;
         NotifySeeds(fnOnReport, &ses, &rpt, &fldValues,
                     seed::SimpleRawWr{seed}, tab->Fields_,
                     IsSubrTree(reqKey.SeedKey_.begin()));
      }
   }
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
   if (isNeedsLog) {
      if (subr)
         RevPrint(rbuf_, "|treePath=", subr->Tree_->TreePath_,
                  "|seedKey=", subr->SeedKey_,
                  "|tab=", subr->TabIndex_,
                  ':', StrView{subr->Tree_->LayoutC_.TabList_[subr->TabIndex_].Named_.Name_});
      RevPrint(rbuf_, "f9sv_Unsubscribe.Ack");
      LogWrite(LogLevel::Info, std::move(rbuf_));
   }
   if (subr == nullptr)
      return;
   assert(subr->SvFunc_ == SvFuncCode::Unsubscribe);
   assert(subr->Tree_ && subr->Seeds_);
   assert(subr->TabIndex_ < subr->Tree_->LayoutC_.TabCount_);
   SeedRec* seed = subr->Seeds_[subr->TabIndex_].get();
   assert(seed && seed->SubrIndex_ == usidx);
   const auto        fnOnReport = seed->Handler_.FnOnReport_;
   const auto        subrSeedKey{std::move(subr->SeedKey_)};
   f9sv_ClientReport rpt;
   ZeroStruct(rpt);
   rpt.ResultCode_ = f9sv_Result_Unsubscribed;
   rpt.TreePath_ = ToStrView(subr->Tree_->TreePath_).ToCStrView();
   rpt.Layout_ = &subr->Tree_->LayoutC_;
   rpt.Tab_ = &subr->Tree_->LayoutC_.TabList_[subr->TabIndex_];
   rpt.SeedKey_ = ToStrView(subrSeedKey).ToCStrView();
   rpt.UserData_ = seed->Handler_.UserData_;
   seed->ClearSvFunc();
   subr->ClearSvFunc();
   maplk->SubrMap_.RemoveObjPtr(usidx, subr);
   if (fnOnReport)
      fnOnReport(&ses, &rpt);
}
void RcSeedVisitorClientNote::OnRecvSubrData(RcClientSession& ses, DcQueue& rxbuf, SvFunc fcAck) {
   struct RxSubrData {
      fon9_NON_COPY_NON_MOVE(RxSubrData);
      char                                   Padding___[7];
      bool                                   IsNeedsLog_;
      const seed::SeedNotifyArgs::NotifyType NotifyType_;
      f9sv_SubrIndex                         SubrIndex_{kSubrIndexNull};
      SubrRec*                               SubrRec_{nullptr};
      seed::Tab*                             Tab_{nullptr};
      RevBufferList                          LogBuf_{kLogBlockNodeSize};
      TreeLocker                             Map_;
      CharVector                             SeedKey_;
      std::string                            Gv_;
      RxSubrData(SvFunc fcAck, f9rc_ClientLogFlag logf, RcSeedVisitorClientNote& note)
         : IsNeedsLog_(f9rc_ClientLogFlag{} != (logf & f9sv_ClientLogFlag_SubscribeData)
                       && LogLevel::Info >= LogLevel_)
         , NotifyType_{GetSvFuncSubscribeDataNotifyType(fcAck)}
         , Map_{note.LockTree()} {
         if (this->IsNeedsLog_)
            RevPutChar(this->LogBuf_, '\n');
      }
      ~RxSubrData() {
         this->FlushLog();
      }
      void FlushLog() {
         if (!this->IsNeedsLog_)
            return;
         this->LogSubrRec();
         this->IsNeedsLog_ = false;
         RevPrint(this->LogBuf_, "f9sv_SubscribeData"
                  "|subrIndex=", this->SubrIndex_,
                  "|notifyType=", this->NotifyType_);
         LogWrite(LogLevel::Info, std::move(this->LogBuf_));
      }
      void LogSubrRec() {
         if (!this->IsNeedsLog_ || this->SubrRec_ == nullptr)
            return;
         if (IsSubrTree(this->SubrRec_->SeedKey_.begin()))
            RevPrint(this->LogBuf_, "|rxSeedKey=", this->SeedKey_);
         RevPrint(this->LogBuf_,
                  "|treePath=", this->SubrRec_->Tree_->TreePath_,
                  "|seedKey=", this->SubrRec_->SeedKey_,
                  "|tab=", this->SubrRec_->TabIndex_,
                  ':', StrView{this->Tab_->GetNameStr()});
         this->SubrRec_ = nullptr;
      }
      StrView CheckLoadSeedKey(DcQueue& rxbuf) {
         if (IsSubrTree(this->SubrRec_->SeedKey_.begin()))
            BitvTo(rxbuf, this->SeedKey_);
         else
            this->SeedKey_ = this->SubrRec_->SeedKey_;
         return ToStrView(this->SeedKey_);
      }
      void LoadGv(DcQueue& rxbuf) {
         BitvTo(rxbuf, this->Gv_);
         if (this->IsNeedsLog_)
            RevPrint(this->LogBuf_, "|gv={", this->Gv_, '}');
      }
   };
   RxSubrData  rx{fcAck, ses.LogFlags_, *this};
   BitvTo(rxbuf, rx.SubrIndex_);
   rx.SubrRec_ = rx.Map_->SubrMap_.GetObjPtr(rx.SubrIndex_);
   if (rx.SubrRec_ == nullptr) {
      if (rx.IsNeedsLog_)
         RevPrint(rx.LogBuf_, "|err=Bad subrIndex.");
      return;
   }
   assert(rx.SubrRec_->SvFunc_ != SvFuncCode::Empty);
   assert(rx.SubrRec_->Tree_ && rx.SubrRec_->Seeds_);
   assert(rx.SubrRec_->TabIndex_ < rx.SubrRec_->Tree_->LayoutC_.TabCount_);
   rx.Tab_ = rx.SubrRec_->Tree_->Layout_->GetTab(rx.SubrRec_->TabIndex_);
   SeedRec* seed = rx.SubrRec_->Seeds_[rx.SubrRec_->TabIndex_].get();
   assert(seed && seed->SubrIndex_ == rx.SubrIndex_);

   f9sv_ClientReport rpt;
   ZeroStruct(rpt);
   rpt.TreePath_ = ToStrView(rx.SubrRec_->Tree_->TreePath_).ToCStrView();
   rpt.Layout_   = &rx.SubrRec_->Tree_->LayoutC_;
   rpt.Tab_      = &rx.SubrRec_->Tree_->LayoutC_.TabList_[rx.SubrRec_->TabIndex_];
   rpt.UserData_ = seed->Handler_.UserData_;
   auto fnOnReport = seed->Handler_.FnOnReport_;

   switch (rx.NotifyType_) {
   default: // Unknown NotifyType.
   case seed::SeedNotifyArgs::NotifyType::SubscribeOK:
      ses.ForceLogout("Unknown SubscribeData NotifyType.");
      return;
   case seed::SeedNotifyArgs::NotifyType::ParentSeedClear:
      // 收到此通知之後, 訂閱會被清除, 不會再收到事件.
      rpt.ResultCode_ = f9sv_Result_SubrNotifyUnsubscribed;
      break;
   case seed::SeedNotifyArgs::NotifyType::SeedChanged:
      rpt.SeedKey_ = rx.CheckLoadSeedKey(rxbuf).ToCStrView();
      rx.LoadGv(rxbuf);
      ParseStrValuesToSeed(&rx.Gv_, seed::SimpleRawWr{seed}, rx.Tab_->Fields_);
      rpt.Seed_ = seed;
      break;
   case seed::SeedNotifyArgs::NotifyType::PodRemoved:
   case seed::SeedNotifyArgs::NotifyType::SeedRemoved:
      rpt.SeedKey_ = rx.CheckLoadSeedKey(rxbuf).ToCStrView();
      rpt.ResultCode_ = (IsSubrTree(rx.SubrRec_->SeedKey_.begin())
                         ? (rx.NotifyType_ == seed::SeedNotifyArgs::NotifyType::PodRemoved
                            ? f9sv_Result_SubrNotifyPodRemoved
                            : f9sv_Result_SubrNotifySeedRemoved)
                         : f9sv_Result_SubrNotifyUnsubscribed);
      break;
   case seed::SeedNotifyArgs::NotifyType::TableChanged:
      // 只有訂閱 tree 才會收到此通知, 此時應更新整個 tree 的內容.
      assert(IsSubrTree(rx.SubrRec_->SeedKey_.begin()));
      rpt.SeedKey_ = ToStrView(rx.SubrRec_->SeedKey_).ToCStrView();
      rx.LoadGv(rxbuf);
      rx.FlushLog();
      if (fnOnReport == nullptr)
         return;
      // 先通知 TableChanged.
      rpt.ResultCode_ = f9sv_Result_SubrNotifyTableChanged;
      fnOnReport(&ses, &rpt);
      // 再通知 seed 內容.
      rpt.Seed_ = seed;
      NotifySeeds(fnOnReport, &ses, &rpt, &rx.Gv_, seed::SimpleRawWr{seed}, rx.Tab_->Fields_, true);
      return;
   }
   if (rpt.ResultCode_ == f9sv_Result_SubrNotifyUnsubscribed) {
      auto* subr = rx.SubrRec_;
      rx.LogSubrRec();
      seed->ClearSvFunc();
      subr->ClearSvFunc();
      rx.Map_->SubrMap_.RemoveObjPtr(rx.SubrIndex_, subr);
   }
   rx.FlushLog();
   if (fnOnReport)
      fnOnReport(&ses, &rpt);
}

f9sv_Result RcSeedVisitorClientNote::AddSubr(const TreeLocker& locker, RcSvClientRequest& req, const PodRec* pod) {
   if (this->Config_.MaxSubrCount_ > 0) {
      if (locker->SubrMap_.Size() >= this->Config_.MaxSubrCount_) {
         req.ExtMsg_ = "|err=OverMaxSubrCount";
         return f9sv_Result_OverMaxSubrCount;
      }
   }
   req.SubrIndex_ = static_cast<f9sv_SubrIndex>(locker->SubrMap_.Alloc());
   SubrRec*  subr = locker->SubrMap_.GetObjPtr(req.SubrIndex_);
   subr->SvFunc_   = GetSvFuncCode(req.SvFunc_);
   assert(subr->SvFunc_ == SvFuncCode::Subscribe);
   subr->Tree_     = req.Tree_;
   subr->TabIndex_ = req.TabIndex_;
   if (pod == nullptr)
      subr->Seeds_ = nullptr;
   else {
      subr->Seeds_ = pod->Seeds_;
      pod->Seeds_[req.TabIndex_]->SubrIndex_ = req.SubrIndex_;
   }
   subr->SeedKey_.assign(req.OrigSeedKey_);
   return f9sv_Result_NoError;
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

   #define case_return(r)  case f9sv_Result_##r:    return #r;
   switch (res) {
      case_return(NoError);
      case_return(Unsubscribing);
      case_return(Unsubscribed);
      case_return(SubrNotifyPodRemoved);
      case_return(SubrNotifySeedRemoved);
      case_return(SubrNotifyTableChanged);
      case_return(SubrNotifyUnsubscribed);

      case_return(AccessDenied);
      case_return(NotFoundTab);
      case_return(BadInitParams);
      case_return(LinkBroken);
      case_return(FlowControl);
      case_return(InQuery);
      case_return(OverMaxSubrCount);
      case_return(InSubscribe);
      case_return(InUnsubscribe);
      case_return(NoSubscribe);
   }
   return GetOpResultMessage(static_cast<fon9::seed::OpResult>(res));
}

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
