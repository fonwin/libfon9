// \file fon9/rc/RcSvsTicket.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSvsTicket.hpp"
#include "fon9/rc/RcSeedVisitorServer.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace rc {

static void MakeAckLayout(RevBufferList& rbuf, seed::Layout& layout) {
   const auto  szBefore = CalcDataSize(rbuf.cfront());
   std::string cfgstr;
   for (auto L = layout.GetTabCount(); L > 0;) {
      seed::Tab* tab = layout.GetTab(--L);
      seed::AppendFieldsConfig(cfgstr, tab->Fields_, '|', '\n');
      RevPrint(rbuf, "\n{\n", cfgstr, "}\n");
      RevPrintNamed(rbuf, *tab, '|');
      RevPrint(rbuf, tab->Flags_, '|');
      cfgstr.clear();
   }
   seed::AppendFieldConfig(cfgstr, *layout.KeyField_, '|', '\n');
   RevPrint(rbuf, layout.Flags_, '|', cfgstr);
   ByteArraySizeToBitvT(rbuf, CalcDataSize(rbuf.cfront()) - szBefore);
}
//--------------------------------------------------------------------------//
seed::OpResult RcSvsSubr::Unsubscribe(seed::SubscribableOp* opSubr, seed::Tab& tab) {
   if (opSubr) {
      if (this->IsStream_)
         return opSubr->UnsubscribeStream(&this->SubConn_, tab);
      return opSubr->Unsubscribe(&this->SubConn_, tab);
   }
   return seed::OpResult::no_subscribe;
}
//--------------------------------------------------------------------------//
void RcSvsUnsubRunnerSP::operator()(const seed::TreeOpResult& res, seed::TreeOp* opTree) {
   RcSvsSubr* reg = this->get();
   if (seed::IsSubrTree(reg->SeedKey_.begin()))
      this->Unsubscribe(*res.Sender_, res.OpResult_, opTree);
   else
      opTree->Get(ToStrView(reg->SeedKey_), RcSvsUnsubRunnerSP{*this});
}
void RcSvsUnsubRunnerSP::Unsubscribe(seed::Tree& tree, seed::OpResult opres, seed::SubscribableOp* opSubr) {
   // 必定可以取消註冊, 因為需要取消者, 必定已經訂閱成功.
   // 但是有可能在系統結束時: Tree 已經清除, 但取消訂閱正在執行, 此時 opSubr 可能會是 nullptr;
   // assert(opSubr != nullptr);

   RcSvsSubr* preg = this->get();
   assert(preg->Tree_.get() == &tree);
   seed::Tab* tab = tree.LayoutSP_->GetTab(preg->TabIndex_);

   if (preg->SvFunc_ == SvFuncCode::Empty) { // 因 LinkBroken 的取消註冊, 不用 lock, 不用回覆.
      preg->Unsubscribe(opSubr, *tab);
      return;
   }
   assert(preg->SvFunc_ == SvFuncCode::Unsubscribe);

   RcSession&  ses = *static_cast<RcSession*>(preg->Device_->Session_.get());
   auto&       note = *static_cast<RcSeedVisitorServerNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor));
   // 取消訂閱, 並回覆結果.
   {
      auto   subrs = note.SubrList_.Lock();
      assert(preg->SubrIndex_ < subrs->size() && (*subrs)[preg->SubrIndex_].get() == preg);
      if (preg->SubrIndex_ >= subrs->size())
         return;
      auto& rpSubr = (*subrs)[preg->SubrIndex_];
      if (rpSubr.get() != preg)
         return;
      rpSubr.reset(); // 等同於: (*subrs)[preg->SubrIndex_].reset();
   }
   opres = preg->Unsubscribe(opSubr, *tab);
   (void)opres;
   RevBufferList ackbuf{64};
   ToBitv(ackbuf, preg->SubrIndex_);
   PutBigEndian(ackbuf.AllocBuffer(sizeof(SvFuncCode)), SvFuncCode::Unsubscribe);
   if (!preg->IsSubjectClosed_)
      ses.Send(f9rc_FunctionCode_SeedVisitor, std::move(ackbuf));
}
//--------------------------------------------------------------------------//
RcSeedVisitorServerNote& RcSvsReq::SessionNote() const {
   auto* note = this->Session().GetNote(f9rc_FunctionCode_SeedVisitor);
   return *static_cast<RcSeedVisitorServerNote*>(note);
}
void RcSvsReq::SendRcSvAck(const CharVector& origReqKey) {
   if (this->SvFunc_ == SvFunc::Empty)
      return;
   if (this->AfterAckLayoutS32_ >= 0)
      ToBitv(this->AckBuffer_, static_cast<uint32_t>(this->AfterAckLayoutS32_));
   if (this->AckLayout_)
      MakeAckLayout(this->AckBuffer_, *this->AckLayout_);
   RevPrint(this->AckBuffer_, origReqKey);
   PutBigEndian(this->AckBuffer_.AllocBuffer(sizeof(this->SvFunc_)), this->SvFunc_);
   this->Session().Send(f9rc_FunctionCode_SeedVisitor, std::move(this->AckBuffer_));
   this->SvFunc_ = SvFunc::Empty;
}
void RcSvsReq::PackSeedValues(const seed::Tab& tab, const seed::RawRd& rd) {
   const auto  szBefore = CalcDataSize(this->AckBuffer_.cfront());
   seed::FieldsCellRevPrint0NoSpl(tab.Fields_, rd, this->AckBuffer_);
   ByteArraySizeToBitvT(this->AckBuffer_, CalcDataSize(this->AckBuffer_.cfront()) - szBefore);
}
void RcSvsReq::PackOpError(seed::OpResult res) {
   if (res == seed::OpResult::not_found_tab && this->IsTreeFound_)
      this->PackAckTabNotFound();
   else
      ToBitv(this->AckBuffer_, res);
}
//--------------------------------------------------------------------------//
RcSvsSubscribeRunner::RcSvsSubscribeRunner(seed::SeedVisitor& visitor, RcSvReqKey&& reqKey)
   : base(visitor, ToStrView(reqKey.TreePath_),
          seed::IsSubrTree(reqKey.SeedKey_.begin()) ? seed::AccessRight::SubrTree : seed::AccessRight::Read)
   , ReqKey_(std::move(reqKey)) {
}
void RcSvsSubscribeRunner::OnFoundTree(seed::TreeOp& opTree) {
   this->CheckAckLayoutWhenDtor(opTree.Tree_);
   StrView tabName = ToStrView(this->ReqKey_.TabName_);
   if (seed::IsSubscribeStream(tabName)) {
      // "$TabName:StreamDecoderName:Args";  例如: "$Rt:MdRts:args"
      // SubscribeStream 的額外參數 = "StreamDecoderName:Args";
      this->SubscribeStreamArgs_.Reset(tabName.begin() + 1, tabName.end());
      tabName = StrFetchNoTrim(this->SubscribeStreamArgs_, ':');
   }
   if (seed::Tab* tab = opTree.Tree_.LayoutSP_->GetTabByNameOrFirst(tabName)) {
      if (seed::IsSubrTree(this->ReqKey_.SeedKey_.begin()))
         this->Subscribe(opTree.Tree_, *tab, opTree);
      else {
         opTree.Get(ToStrView(this->ReqKey_.SeedKey_),
                    std::bind(&RcSvsSubscribeRunner::OnLastSeedOp, intrusive_ptr<RcSvsSubscribeRunner>(this),
                              std::placeholders::_1, std::placeholders::_2, std::ref(*tab)));
      }
   }
   else {
      auto  subrs = this->SessionNote().SubrList_.Lock();
      if (subrs->empty()) { // 已因 LinkBroken 清除訂閱, 不用回覆結果.
         assert(this->Session().GetSessionSt() != RcSessionSt::ApReady);
         this->SvFunc_ = SvFunc::Empty;
      }
      else {
         this->PackAckTabNotFound();
         (*subrs)[this->ReqKey_.SubrIndex_].reset();
      }
   }
}
void RcSvsSubscribeRunner::OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab) {
   if (pod)
      this->Subscribe(*resPod.Sender_, tab, *pod);
   else {
      struct ErrPod : public seed::SubscribableOp {
         fon9_NON_COPY_NON_MOVE(ErrPod);
         const seed::OpResult Result_;
         char                 Padding____[4];
         ErrPod(seed::OpResult res) : Result_{res} {
         }
         seed::OpResult Subscribe(SubConn*, seed::Tab&, seed::FnSeedSubr) override {
            return this->Result_;
         }
         seed::OpResult SubscribeStream(SubConn*, seed::Tab&, StrView, seed::FnSeedSubr) override {
            return this->Result_;
         }
      };
      ErrPod errp{resPod.OpResult_};
      this->Subscribe(*resPod.Sender_, tab, errp);
   }
}
void RcSvsSubscribeRunner::OnError(seed::OpResult res) {
   ToBitv(this->AckBuffer_, res);
   auto subrs = this->SessionNote().SubrList_.Lock();
   if (this->ReqKey_.SubrIndex_ < subrs->size())
      (*subrs)[this->ReqKey_.SubrIndex_].reset();
}
void RcSvsSubscribeRunner::Subscribe(seed::Tree& tree, seed::Tab& tab, seed::SubscribableOp& opSubr) {
   auto& note = this->SessionNote();
   auto  subrs = note.SubrList_.Lock();
   if (subrs->empty()) { // 已因 LinkBroken 清除訂閱.
                         // 不用訂閱, 也不用回覆結果.
      assert(this->Session().GetSessionSt() != RcSessionSt::ApReady);
      this->SvFunc_ = SvFunc::Empty;
      return;
   }
   RcSvsSubrSP& rpSubr = (*subrs)[this->ReqKey_.SubrIndex_];
   RcSvsSubr*   psubr = rpSubr.get();
   assert(psubr != nullptr && psubr->Tree_.get() == nullptr);
   if (psubr == nullptr)
      return;
   psubr->Tree_.reset(&tree);
   psubr->TabIndex_ = static_cast<f9sv_TabSize>(tab.GetIndex());
   psubr->IsStream_ = !this->SubscribeStreamArgs_.IsNull();

   seed::OpResult opErrC;
   if (psubr->SvFunc_ == SvFuncCode::Unsubscribe)
      // 尋找 tree 的過程中, 收到 [取消訂閱] 要求.
      // 訂閱要求失敗: 因為取消訂閱.
      opErrC = static_cast<seed::OpResult>(f9sv_Result_Unsubscribed);
   else {
      note.Runner_ = this;
      auto fnSubr = std::bind(&RcSeedVisitorServerNote::OnSubscribeNotify,
                              RcSvsSubrSP{psubr}, std::placeholders::_1);
      if (psubr->IsStream_)
         opErrC = opSubr.SubscribeStream(&psubr->SubConn_, tab, this->SubscribeStreamArgs_, std::move(fnSubr));
      else
         opErrC = opSubr.Subscribe(&psubr->SubConn_, tab, std::move(fnSubr));
      note.Runner_ = nullptr;
      if (opErrC == seed::OpResult::no_error) {
         // 訂閱成功: 會在 RcSeedVisitorServerNote::OnSubscribeNotify() 裡面, NotifyKind = SubscribeOK:
         //          將結果填入 this->AckBuffer_;
         // 所以這裡直接結束.
         return;
      }
   }
   rpSubr.reset(); // 等同於: (*subrs)[this->ReqKey_.SubrIndex_].reset();
                   // 訂閱失敗 & 立即回覆的 tab(seeds) 筆數=0.
   ToBitv(this->AckBuffer_, opErrC);
   RevPutBitv(this->AckBuffer_, fon9_BitvV_Number0);
}
//--------------------------------------------------------------------------//
void RcSvsTicketRunnerQuery::OnLastStep(seed::TreeOp& op, StrView keyText, seed::Tab& tab0) {
   this->CheckAckLayoutWhenDtor(op.Tree_);
   base::OnLastStep(op, keyText, tab0); // => OnLastSeedOp();
}
void RcSvsTicketRunnerQuery::OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab0) {
   (void)tab0;
   if (this->IsAllTabs_) {
      auto tabidx = resPod.Sender_->LayoutSP_->GetTabCount();
      while (auto* ptab = resPod.Sender_->LayoutSP_->GetTab(--tabidx))
         this->ReadSeed(resPod, pod, *ptab);
   }
   else if (auto* ptab = resPod.Sender_->LayoutSP_->GetTabByNameOrFirst(ToStrView(this->TabName_)))
      this->ReadSeed(resPod, pod, *ptab);
   else
      this->PackAckTabNotFound();
}
void RcSvsTicketRunnerQuery::ReadSeed(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab) {
   base::OnLastSeedOp(resPod, pod, tab); // => OnReadOp(); or OnError();
   ToBitv(this->AckBuffer_, static_cast<f9sv_TabSize>(tab.GetIndex()));
}
void RcSvsTicketRunnerQuery::OnReadOp(const seed::SeedOpResult& res, const seed::RawRd* rd) {
   if (rd == nullptr)
      this->OnError(res.OpResult_);
   else
      this->PackSeedValues(*res.Tab_, *rd);
}
//--------------------------------------------------------------------------//
void RcSvsTicketRunnerGridView::OnFoundTree(seed::TreeOp& opTree) {
   this->CheckAckLayoutWhenDtor(opTree.Tree_);
   base::OnFoundTree(opTree);
}
void RcSvsTicketRunnerGridView::OnFoundTab(seed::TreeOp& opTree, seed::GridViewRequest& req) {
   this->AfterAckLayoutS32_ = req.Tab_->GetIndex();
   base::OnFoundTab(opTree, req);
}
void RcSvsTicketRunnerGridView::RowNoToBitv(size_t n) {
   if (n == seed::GridViewResult::kNotSupported)
      RevPutBitv(this->AckBuffer_, fon9_BitvV_NumberNull);
   else
      ToBitv(this->AckBuffer_, n);
}
void RcSvsTicketRunnerGridView::OnGridViewOp(seed::GridViewResult& res) {
   if (res.OpResult_ == seed::OpResult::no_error) {
      ToBitv(this->AckBuffer_, res.GridView_);
      this->RowNoToBitv(res.DistanceEnd_);
      this->RowNoToBitv(res.DistanceBegin_);
      this->RowNoToBitv(res.ContainerSize_);
      ToBitv(this->AckBuffer_, res.RowCount_);
   }
   ToBitv(this->AckBuffer_, res.OpResult_);
}
//--------------------------------------------------------------------------//
void RcSvsTicketRunnerWrite::OnLastStep(seed::TreeOp& op, StrView keyText, seed::Tab& tab) {
   this->CheckAckLayoutWhenDtor(op.Tree_);
   this->AfterAckLayoutS32_ = tab.GetIndex();
   base::OnLastStep(op, keyText, tab);
}
void RcSvsTicketRunnerWrite::OnWriteOp(const seed::SeedOpResult& res, const seed::RawWr* wr) {
   if (wr) {
      const auto  szBefore = CalcDataSize(this->AckBuffer_.cfront());
      this->ParseSetValues(this->AckBuffer_, res, *wr, '|');
      ByteArraySizeToBitvT(this->AckBuffer_, CalcDataSize(this->AckBuffer_.cfront()) - szBefore);
      // -----
      this->PackSeedValues(*res.Tab_, *wr);
   }
   ToBitv(this->AckBuffer_, res.OpResult_);
}
//--------------------------------------------------------------------------//
void RcSvsTicketRunnerRemove::OnBeforeRemove(seed::TreeOp& opTree, StrView keyText, seed::Tab* tab) {
   this->CheckAckLayoutWhenDtor(opTree.Tree_);
   base::OnBeforeRemove(opTree, keyText, tab);
}
void RcSvsTicketRunnerRemove::OnAfterRemove(const seed::PodRemoveResult& res) {
   fon9_WARN_DISABLE_SWITCH;
   switch (res.OpResult_) {
   case seed::OpResult::removed_seed:
      assert(res.Tab_ != nullptr);
      ToBitv(this->AckBuffer_, res.Tab_ ? static_cast<f9sv_TabSize>(res.Tab_->GetIndex()) : 0u);
      /* fall through */ // for OpResult.
   case seed::OpResult::removed_pod:
      ToBitv(this->AckBuffer_, res.OpResult_);
      break;
   default: // 其餘必定已觸發過 this->OnError();
      break;
   }
   fon9_WARN_POP;
   base::OnAfterRemove(res);
}
//--------------------------------------------------------------------------//
void RcSvsTicketRunnerCommand::OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab) {
   this->CheckAckLayoutWhenDtor(*resPod.Sender_);
   this->AfterAckLayoutS32_ = tab.GetIndex();
   base::OnLastSeedOp(resPod, pod, tab);
}
void RcSvsTicketRunnerCommand::OnSeedCommandResult(const seed::SeedOpResult& res, StrView msg) {
   assert(res.Tab_ != nullptr);
   ToBitv(this->AckBuffer_, msg);
   ToBitv(this->AckBuffer_, res.OpResult_);
   if (res.OpResult_ != seed::OpResult::no_error)
      RevPutBitv(this->AckBuffer_, fon9_BitvV_Number0);
   base::OnSeedCommandResult(res, msg);
}

} } // namespaces
