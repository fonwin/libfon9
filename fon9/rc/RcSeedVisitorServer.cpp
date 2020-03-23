// \file fon9/rc/RcSeedVisitorServer.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSeedVisitorServer.hpp"
#include "fon9/rc/RcSession.hpp"
#include "fon9/rc/RcFuncConnServer.hpp"
#include "fon9/auth/PolicyAcl.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace rc {

RcSeedVisitorServerAgent::~RcSeedVisitorServerAgent() {
}
void RcSeedVisitorServerAgent::OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) {
   (void)param;
   if (auto* authNote = ses.GetNote<RcServerNote_SaslAuth>(f9rc_FunctionCode_SASL)) {
      const auth::AuthResult& authr = authNote->GetAuthResult();
      if (auto agAcl = authr.AuthMgr_->Agents_->Get<auth::PolicyAclAgent>(fon9_kCSTR_PolicyAclAgent_Name)) {
         seed::AclConfig  aclcfg;
         agAcl->GetPolicy(authr, aclcfg);
         RevBufferList  rbuf{256};
         // 最後一行: "|FcQryCount/FcQryMS|MaxSubrCount"
         // 回補流量管制, 暫時不考慮告知 Client, 因為回補流量管制由 Server 處理.
         RevPrint(rbuf, fon9_kCSTR_ROWSPL /* Path="" */ fon9_kCSTR_CELLSPL,
                  aclcfg.FcQuery_.FcCount_, '/', aclcfg.FcQuery_.FcTimeMS_, fon9_kCSTR_CELLSPL,
                  aclcfg.MaxSubrCount_);
         agAcl->MakeGridView(rbuf, aclcfg);
         RevPrint(rbuf, fon9_kCSTR_LEAD_TABLE fon9_kCSTR_PolicyAclAgent_Name fon9_kCSTR_ROWSPL);
         PutBigEndian(rbuf.AllocPacket<SvFunc>(), SvFuncCode::Acl);
         ses.Send(this->FunctionCode_, std::move(rbuf));
         ses.ResetNote(this->FunctionCode_, RcFunctionNoteSP{new RcSeedVisitorServerNote(ses, authr, std::move(aclcfg))});
      }
      else
         ses.ForceLogout("Auth PolicyAclAgent not found.");
   }
   else {
      ses.ForceLogout("Auth SASL note not found.");
   }
}
void RcSeedVisitorServerAgent::OnSessionLinkBroken(RcSession& ses) {
   if (auto note = static_cast<RcSeedVisitorServerNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor))) {
      assert(dynamic_cast<RcSeedVisitorServerNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor)) != nullptr);
      note->OnSessionLinkBroken();
   }
   // 在 LinkBroken 時, 不要刪除 note;
   // - 因為有可能還有 runner 尚未完成, 當 runner 完成時, 會需要用到 note;
   // - Server 端通常使用 tcp server, 在連線中斷且 runner(有包含 DeviceSP) 執行完畢後,
   //   最終會刪除 device, 那時 note 就會被刪除了.
   // base::OnSessionLinkBroken(ses);
}
//--------------------------------------------------------------------------//
struct RcSeedVisitorServerNote::SeedVisitor : public seed::SeedVisitor {
   fon9_NON_COPY_NON_MOVE(SeedVisitor);
   using base = seed::SeedVisitor;
   // 在此保留 Device, 避免在尚未取消訂閱前, Note 及 Visitor 被刪除.
   // 在 RcSeedVisitorServerNote::OnSessionLinkBroken() 時清除.
   io::DeviceSP Device_;
   SeedVisitor(io::DeviceSP dev, const auth::AuthResult& authr, seed::AclConfig&& aclcfg)
      : base(authr.AuthMgr_->MaRoot_, authr.MakeUFrom(ToStrView(dev->WaitGetDeviceId())))
      , Device_{std::move(dev)} {
      this->Fairy_->ResetAclConfig(std::move(aclcfg));
   }
   const seed::AclConfig& GetAclConfig() const {
      return this->Fairy_->GetAclConfig();
   }
};
//--------------------------------------------------------------------------//
RcSeedVisitorServerNote::RcSeedVisitorServerNote(RcSession& ses,
                                                 const auth::AuthResult& authr,
                                                 seed::AclConfig&& aclcfg)
   : Visitor_{new SeedVisitor(ses.GetDevice(), authr, std::move(aclcfg))}
   , FcQry_(static_cast<unsigned>(aclcfg.FcQuery_.FcCount_ * 2), // *2: for 緩衝.
            TimeInterval_Millisecond(aclcfg.FcQuery_.FcTimeMS_))
   , FcRecover_(aclcfg.FcRecover_.FcCount_ * 1000u,
                TimeInterval_Millisecond(aclcfg.FcRecover_.FcTimeMS_),
                aclcfg.FcRecover_.FcTimeMS_ /* 讓時間分割單位=1ms */) {
}
RcSeedVisitorServerNote::~RcSeedVisitorServerNote() {
}
//--------------------------------------------------------------------------//
struct RcSeedVisitorServerNote::UnsubRunnerSP : public intrusive_ptr<SubrReg> {
   using base = intrusive_ptr<SubrReg>;
   UnsubRunnerSP(SubrRegSP preg) : base{std::move(preg)} {
   }
   void operator()(const seed::PodOpResult& res, seed::PodOp* pod) {
      this->Unsubscribe(*res.Sender_, res.OpResult_, pod);
   }
   void operator()(const seed::TreeOpResult& res, seed::TreeOp* opTree) {
      SubrReg* reg = this->get();
      if (IsSubrTree(reg->SeedKey_.begin()))
         this->Unsubscribe(*res.Sender_, res.OpResult_, opTree);
      else
         opTree->Get(ToStrView(reg->SeedKey_), UnsubRunnerSP{*this});
   }
   void Unsubscribe(seed::Tree& tree, seed::OpResult opres, seed::SubscribableOp* opSubr) {
      // 必定可以取消註冊, 因為需要取消者, 必定已經訂閱成功.
      // 但是有可能在系統結束時: Tree 已經清除, 但取消訂閱正在執行, 此時 opSubr 可能會是 nullptr;
      // assert(opSubr != nullptr);

      SubrReg* preg = this->get();
      assert(preg->Tree_.get() == &tree);
      seed::Tab* tab = tree.LayoutSP_->GetTab(preg->TabIndex_);

      if (preg->SvFunc_ == SvFuncCode::Empty) { // 因 LinkBroken 的取消註冊, 不用 lock, 不用回覆.
         preg->Unsubscribe(opSubr, *tab);
         return;
      }
      assert(preg->SvFunc_ == SvFuncCode::Unsubscribe);

      RcSession&  ses = *static_cast<RcSession*>(preg->Device_->Session_.get());
      auto& note = *static_cast<RcSeedVisitorServerNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor));
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
      RevBufferList ackbuf{64};
      ToBitv(ackbuf, preg->SubrIndex_);
      PutBigEndian(ackbuf.AllocBuffer(sizeof(SvFuncCode)), SvFuncCode::Unsubscribe);
      if (!preg->IsSubjectClosed_)
         ses.Send(f9rc_FunctionCode_SeedVisitor, std::move(ackbuf));
   }
};
void RcSeedVisitorServerNote::OnSessionLinkBroken() {
   SubrListImpl  subrs = std::move(*this->SubrList_.Lock());
   for (SubrRegSP& preg : subrs) {
      if (preg) {
         preg->SvFunc_ = SvFuncCode::Empty; // 設定成: 因 LinkBroken 的取消註冊.
         seed::Tree* tree = preg->Tree_.get();
         tree->OnTreeOp(UnsubRunnerSP{std::move(preg)});
      }
   }
   this->Visitor_->Device_.reset();
}
//--------------------------------------------------------------------------//
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

struct RcSvTickRunner {
   fon9_NON_COPY_NON_MOVE(RcSvTickRunner);
   RcSvTickRunner() = default;
   io::DeviceSP   Device_; // 為了確保在 Query 執行期間, device/session 不會死亡.
   seed::LayoutSP Layout_;
   SvFunc         SvFunc_;
   bool           IsAllTabs_;
   char           Padding__[6];
   RevBufferList  AckBuffer_{256};
   RcSession& Session() const {
      return *static_cast<RcSession*>(this->Device_->Session_.get());
   }
   RcSeedVisitorServerNote& SessionNote() const {
      auto* note = this->Session().GetNote(f9rc_FunctionCode_SeedVisitor);
      return *static_cast<RcSeedVisitorServerNote*>(note);
   }
   void CheckAckLayout(seed::TreeOp& opTree) {
      if (HasSvFuncFlag(this->SvFunc_, SvFuncFlag::Layout))
         this->Layout_ = opTree.Tree_.LayoutSP_;
   }
   void SetAckTabNotFound() {
      // 若 client 為 pending req: 在收到 layout 後, 處理 pendings 時, 若 tab not found 則會直接觸發事件.
      // 若 client 已有 layout, 則會先判斷 tab 是否存在, 若不存在則不會送出, 會在當場觸發 tab not found. 
      // 所以填入 fon9_BitvV_NumberNull, 然後 client 端在 RcSeedVisitorClientNote::OnRecvQrySubrAck() 處理.
      RevPutBitv(this->AckBuffer_, fon9_BitvV_NumberNull);
   }
   void SendRcSvAck(const CharVector& origReqKey) {
      if (this->SvFunc_ == SvFunc::Empty)
         return;
      if (this->Layout_)
         MakeAckLayout(this->AckBuffer_, *this->Layout_);
      RevPrint(this->AckBuffer_, origReqKey);
      PutBigEndian(this->AckBuffer_.AllocBuffer(sizeof(this->SvFunc_)), this->SvFunc_);
      this->Session().Send(f9rc_FunctionCode_SeedVisitor, std::move(this->AckBuffer_));
      this->SvFunc_ = SvFunc::Empty;
   }
};
struct TickRunnerQuery : public seed::TicketRunnerRead, public RcSvTickRunner {
   fon9_NON_COPY_NON_MOVE(TickRunnerQuery);
   using base = seed::TicketRunnerRead;
   // 為了在最後的 tab not found 時, 仍能提供 layout, 所以額外提供最後的 tabName;
   // 由 TickRunnerQuery 自行判斷 tab not found.
   CharVector  TabName_;
   TickRunnerQuery(seed::SeedVisitor& visitor, StrView seed, StrView tabName)
      : base{visitor, seed}
      , TabName_{tabName} {
   }
   ~TickRunnerQuery() {
      this->SendRcSvAck(this->Bookmark_); // Bookmark_ = origReqKey;
   }
   void OnLastStep(seed::TreeOp& op, StrView keyText, seed::Tab& tab0) override {
      this->CheckAckLayout(op);
      base::OnLastStep(op, keyText, tab0); // => OnLastSeedOp();
   }
   void OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab0) override {
      (void)tab0;
      if (this->IsAllTabs_) {
         auto tabidx = resPod.Sender_->LayoutSP_->GetTabCount();
         while (auto* ptab = resPod.Sender_->LayoutSP_->GetTab(--tabidx))
            this->ReadSeed(resPod, pod, *ptab);
      }
      else if (auto* ptab = resPod.Sender_->LayoutSP_->GetTabByNameOrFirst(ToStrView(this->TabName_)))
         this->ReadSeed(resPod, pod, *ptab);
      else
         this->SetAckTabNotFound();
   }
   void ReadSeed(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab) {
      base::OnLastSeedOp(resPod, pod, tab); // => OnReadOp(); or OnError();
      ToBitv(this->AckBuffer_, tab.GetIndex());
   }
   void OnReadOp(const seed::SeedOpResult& res, const seed::RawRd* rd) override {
      if (rd == nullptr)
         this->OnError(res.OpResult_);
      else {
         // 查詢結果: 使用[文字]傳輸 seed 內容.
         // 解析結果在 RcSeedVisitorClient.cpp: RcSeedVisitorClientNote::OnRecvQueryAck();
         const auto  szBefore = CalcDataSize(this->AckBuffer_.cfront());
         seed::FieldsCellRevPrint0NoSpl(res.Tab_->Fields_, *rd, this->AckBuffer_);
         ByteArraySizeToBitvT(this->AckBuffer_, CalcDataSize(this->AckBuffer_.cfront()) - szBefore);
      }
   }
   void OnError(seed::OpResult res) override {
      ToBitv(this->AckBuffer_, res);
   }
};
struct RcSeedVisitorServerNote::TicketRunnerSubscribe : public seed::TicketRunnerTree, public RcSvTickRunner {
   fon9_NON_COPY_NON_MOVE(TicketRunnerSubscribe);
   using base = TicketRunnerTree;
   const RcSvReqKey  ReqKey_;
   StrView           SubscribeStreamArgs_{nullptr};
   TicketRunnerSubscribe(seed::SeedVisitor& visitor, RcSvReqKey&& reqKey)
      : base(visitor, ToStrView(reqKey.TreePath_),
             IsSubrTree(reqKey.SeedKey_.begin()) ? seed::AccessRight::SubrTree : seed::AccessRight::Read)
      , ReqKey_(std::move(reqKey)) {
   }
   ~TicketRunnerSubscribe() {
      this->SendRcSvAck(this->Bookmark_); // Bookmark_ = origReqKey;
   }
   void Subscribe(seed::Tree& tree, seed::Tab& tab, seed::SubscribableOp& opSubr) {
      auto& note = this->SessionNote();
      auto  subrs = note.SubrList_.Lock();
      if (subrs->empty()) { // 已因 LinkBroken 清除訂閱.
         // 不用訂閱, 也不用回覆結果.
         assert(this->Session().GetSessionSt() != RcSessionSt::ApReady);
         this->SvFunc_ = SvFunc::Empty;
         return;
      }
      SubrRegSP& rpSubr = (*subrs)[this->ReqKey_.SubrIndex_];
      SubrReg*   psubr = rpSubr.get();
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
         opErrC = static_cast<seed::OpResult>(f9sv_Result_InUnsubscribe);
      else {
         note.Runner_ = this;
         auto fnSubr = std::bind(&RcSeedVisitorServerNote::OnSubscribeNotify,
                                 SubrRegSP{psubr}, std::placeholders::_1);
         if (psubr->IsStream_)
            opErrC = opSubr.SubscribeStream(&psubr->SubConn_, tab, this->SubscribeStreamArgs_, std::move(fnSubr));
         else
            opErrC = opSubr.Subscribe(&psubr->SubConn_, tab, std::move(fnSubr));
         note.Runner_ = nullptr;
         if (opErrC == seed::OpResult::no_error) {
            // 訂閱成功, 已在 NotifyKind = SubscribeOK 回覆結果, 所以這裡直接結束.
            return;
         }
      }
      rpSubr.reset(); // 等同於: (*subrs)[this->ReqKey_.SubrIndex_].reset();
      // 訂閱失敗 & 立即回覆的 tab(seeds) 筆數=0.
      this->OnError(opErrC);
      RevPutBitv(this->AckBuffer_, fon9_BitvV_Number0);
   }
   void OnFoundTree(seed::TreeOp& opTree) override {
      this->CheckAckLayout(opTree);
      StrView tabName = ToStrView(this->ReqKey_.TabName_);
      if (IsSubscribeStream(tabName)) {
         // "$TabName:StreamDecoderName:Args";  例如: "$Rt:MdRts:args"
         // SubscribeStream 的額外參數 = "StreamDecoderName:Args";
         this->SubscribeStreamArgs_.Reset(tabName.begin() + 1, tabName.end());
         tabName = StrFetchNoTrim(this->SubscribeStreamArgs_, ':');
      }
      seed::Tab* tab = opTree.Tree_.LayoutSP_->GetTabByNameOrFirst(tabName);
      if (!tab) {
         auto  subrs = this->SessionNote().SubrList_.Lock();
         if (subrs->empty()) { // 已因 LinkBroken 清除訂閱, 不用回覆結果.
            assert(this->Session().GetSessionSt() != RcSessionSt::ApReady);
            this->SvFunc_ = SvFunc::Empty;
            return;
         }
         this->SetAckTabNotFound();
         (*subrs)[this->ReqKey_.SubrIndex_].reset();
         return;
      }
      if (IsSubrTree(this->ReqKey_.SeedKey_.begin()))
         this->Subscribe(opTree.Tree_, *tab, opTree);
      else {
         opTree.Get(ToStrView(this->ReqKey_.SeedKey_),
                    std::bind(&TicketRunnerSubscribe::OnLastSeedOp, intrusive_ptr<TicketRunnerSubscribe>(this),
                              std::placeholders::_1, std::placeholders::_2, std::ref(*tab)));
      }
   }
   void OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab) override {
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
   void OnError(seed::OpResult res) override {
      ToBitv(this->AckBuffer_, res);
   }
};
void RcSeedVisitorServerNote::OnRecvUnsubscribe(RcSession& ses, f9sv_SubrIndex usidx) {
   auto  subrs = this->SubrList_.Lock();
   if (usidx >= subrs->size()) {
      ses.ForceLogout("f9sv_Unsubscribe bad SubrIndex.");
      return;
   }
   SubrRegSP psubr = (*subrs)[usidx].get();
   if (psubr.get() == nullptr || psubr->SvFunc_ != SvFuncCode::Subscribe) {
      ses.ForceLogout("f9sv_Unsubscribe no subscribe.");
      return;
   }
   psubr->SvFunc_ = SvFuncCode::Unsubscribe;
   if (auto tree = psubr->Tree_.get()) {
      // 已訂閱成功, 執行取消訂閱步驟.
      subrs.unlock(); // 必須先 unlock, 因為 tree->OnTreeOp() 可能會就地呼叫 op.
      tree->OnTreeOp(UnsubRunnerSP{psubr});
   }
   else {
      // TicketRunnerSubscribe 還在尋找 tree 準備訂閱,
      // 因為有設定 psubr->SvFunc_ = SvFuncCode::Unsubscribe;
      // 所以等找到 tree 時, 可以判斷 [不需要訂閱了], 那時再回覆 Unsubscribe 即可.
   }
}
void RcSeedVisitorServerNote::OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) {
   static_assert(sizeof(SvFunc) == 1, "sizeof(SvFunc) != 1, 不可使用 Peek1().");
   const void* fcPtr = param.RecvBuffer_.Peek1();
   if (fcPtr == nullptr || param.RemainParamSize_ == 0)
      return;
   const SvFunc     fc = GetBigEndian<SvFunc>(fcPtr);
   const SvFuncCode fcCode = GetSvFuncCode(fc);
   param.RecvBuffer_.PopConsumed(sizeof(SvFunc));
   param.RemainParamSize_ -= sizeof(SvFunc);

   if (fcCode == SvFuncCode::Unsubscribe) {
      // 取消訂閱, 只需要提供 usidx;
      f9sv_SubrIndex usidx{kSubrIndexNull};
      BitvTo(param.RecvBuffer_, usidx);
      this->OnRecvUnsubscribe(ses, usidx);
      return;
   }
   // 保留 origReqKey 存放在 TicketRunner::Bookmark_, 在回復結果時當成 key 返回.
   CharVector origReqKey;
   param.RecvBuffer_.Read(origReqKey.alloc(param.RemainParamSize_), param.RemainParamSize_);

   DcQueueFixedMem rxbuf{ToStrView(origReqKey)};
   RcSvReqKey      reqKey;
   reqKey.LoadSeedName(rxbuf);

   RcSvTickRunner*      svTicket{nullptr};
   seed::TicketRunnerSP runner;
   seed::OpResult       runResult = seed::OpResult::no_error;
   StrView              msgLogout;
   fon9_WARN_DISABLE_SWITCH;
   switch (fcCode) {
   case SvFuncCode::Query:
      if (FcQry_.Fetch(param.RecvTime_).GetOrigValue() > 0)
         msgLogout = "f9sv_Query flow control.";
      else {
         CharVector seedPath{ToStrView(reqKey.TreePath_)};
         seedPath.push_back('/');
         seedPath.append(ToStrView(reqKey.SeedKey_));
         // TODO: 是否允許使用範圍(key range)查詢? [from - to] or [from + count].
         TickRunnerQuery* req = new TickRunnerQuery{*this->Visitor_, ToStrView(seedPath), ToStrView(reqKey.TabName_)};
         runner.reset(req);
         svTicket = req;
      }
      break;
   case SvFuncCode::Subscribe:
      BitvTo(rxbuf, reqKey.SubrIndex_);
      const auto  maxSubrCount = this->Visitor_->GetAclConfig().MaxSubrCount_;
      auto        subrs = this->SubrList_.Lock();
      // 不符合訂閱規範者, 立即斷線: ForceLogout()
      if (maxSubrCount > 0 && reqKey.SubrIndex_ >= maxSubrCount)
         msgLogout = "f9sv_Subscribe over MaxSubrCount.";
      else if (reqKey.SubrIndex_ > subrs->size() + 100) {
         // 一般而言, reqKey.SubrIndex_ 會依序增加.
         // 但如果 client端取消「內部 pending 訂閱」, 因該訂閱尚未送出, 則有可能造成後續的訂閱跳號.
         // 為了避免發生此情況, 所以在此設定一個允許的「最大跳號 = N」數量: subrs->size() + N;
         // 但這樣仍無法避免「惡意」連線造成的記憶體用盡的問題, 所以一般而言應設定 MaxSubrCount 權限.
         msgLogout = "f9sv_Subscribe bad SubrIndex.";
      }
      else {
         SubrRegSP* ppSubr;
         if (reqKey.SubrIndex_ >= subrs->size()) {
            subrs->resize(reqKey.SubrIndex_ + 1);
            ppSubr = &(*subrs)[reqKey.SubrIndex_];
         }
         else {
            ppSubr = &(*subrs)[reqKey.SubrIndex_];
            if (auto* psubr = ppSubr->get()) {
               if (!psubr->IsSubjectClosed_) {
                  msgLogout = "f9sv_Subscribe dup subscribe.";
                  break;
               }
            }
         }
         ppSubr->reset(new SubrReg(reqKey.SubrIndex_, ToStrView(reqKey.SeedKey_), ses.GetDevice()));
         TicketRunnerSubscribe* req = new TicketRunnerSubscribe{*this->Visitor_, std::move(reqKey)};
         runner.reset(req);
         svTicket = req;
         if (req->ReqKey_.IsAllTabs_)
            runResult = seed::OpResult::not_found_tab;
      }
      break;
   }
   fon9_WARN_POP;

   if (!msgLogout.empty()) {
      assert(svTicket == nullptr);
      ses.ForceLogout(msgLogout.ToString());
   }
   else if (svTicket) {
      svTicket->Device_ = ses.GetDevice();
      svTicket->SvFunc_ = fc;
      svTicket->IsAllTabs_ = reqKey.IsAllTabs_;
      runner->Bookmark_ = std::move(origReqKey);
      if (runResult == seed::OpResult::no_error)
         runner->Run();
      else
         runner->OnError(runResult);
   }
}
void RcSeedVisitorServerNote::OnSubscribeNotify(SubrRegSP preg, const seed::SeedNotifyArgs& e) {
   RevBufferList ackbuf{128};
   RcSession&    ses = *static_cast<RcSession*>(preg->Device_->Session_.get());
   switch (e.NotifyKind_) {
   default: // Unknown NotifyKind;
      return;
   case seed::SeedNotifyKind::SubscribeStreamOK:
   case seed::SeedNotifyKind::SubscribeOK:
      if (auto* note = static_cast<RcSeedVisitorServerNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor))) {
         assert(note->Runner_ != nullptr);
         const std::string& gv = e.GetGridView(); // {立即回覆的 tab(seeds) 內容;}
         if (e.NotifyKind_ == seed::SeedNotifyKind::SubscribeStreamOK) {
            ToBitv(note->Runner_->AckBuffer_, gv);
            goto __ACK_SUCCESS_0;
         }
         if (gv.empty()) {
         __ACK_SUCCESS_0:;
            // 訂閱成功, 告知有 0 筆立即回覆的 tab(seeds) 內容.
            ToBitv(note->Runner_->AckBuffer_, seed::OpResult::no_error);
            RevPutBitv(note->Runner_->AckBuffer_, fon9_BitvV_Number0);
         }
         else { // 訂閱成功, 告知有1筆立即回覆的 tab(seeds) 內容.
            ToBitv(note->Runner_->AckBuffer_, gv);
            ToBitv(note->Runner_->AckBuffer_, static_cast<unsigned>(e.Tab_->GetIndex()));
            ToBitv(note->Runner_->AckBuffer_, seed::OpResult::no_error);
            ToBitv(note->Runner_->AckBuffer_, 1u);
         }
         note->Runner_->SendRcSvAck(note->Runner_->Bookmark_);
      }
      else {
         assert(note != nullptr);
      }
      return;
   case seed::SeedNotifyKind::TableChanged:
      ToBitv(ackbuf, e.GetGridView());
      break;
   case seed::SeedNotifyKind::StreamRecover:
   case seed::SeedNotifyKind::StreamRecoverEnd:
      if (auto* note = static_cast<RcSeedVisitorServerNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor))) {
         const std::string& gv = e.GetGridView();
         TimeInterval fc = note->FcRecover_.CalcUsed(UtcNow(), gv.size() + 16);
         if (!fc.IsZero() && e.NotifyKind_ != seed::SeedNotifyKind::StreamRecoverEnd) {
            static_cast<const seed::SeedNotifyStreamRecoverArgs*>(&e)->FlowControlWait_ = fc;
            if (fc.GetOrigValue() < 0)
               return;
         }
         ToBitv(ackbuf, gv);
         goto __CHECK_PUT_KEY_FOR_SUBR_TREE;
      }
      return;
   case seed::SeedNotifyKind::StreamEnd:
      preg->IsSubjectClosed_ = true;
      // 不用 break; 繼續處理 gv 及 key 填入 ackbuf;
   case seed::SeedNotifyKind::SeedChanged:
   case seed::SeedNotifyKind::StreamData:
      ToBitv(ackbuf, e.GetGridView());
      goto __CHECK_PUT_KEY_FOR_SUBR_TREE;
   case seed::SeedNotifyKind::PodRemoved:
   case seed::SeedNotifyKind::SeedRemoved:
      preg->IsSubjectClosed_ = true;
   __CHECK_PUT_KEY_FOR_SUBR_TREE:;
      if (IsSubrTree(preg->SeedKey_.begin())) // 如果訂閱的是 tree, 則需要額外提供 seedKey.
         ToBitv(ackbuf, e.KeyText_);
      break;
   case seed::SeedNotifyKind::ParentSeedClear:
      preg->IsSubjectClosed_ = true;
      break;
   }
   ToBitv(ackbuf, preg->SubrIndex_);
   PutBigEndian(ackbuf.AllocBuffer(sizeof(SvFunc)), SvFuncSubscribeData(e.NotifyKind_));
   ses.Send(f9rc_FunctionCode_SeedVisitor, std::move(ackbuf));
}

} } // namespaces
//-////////////////////////////////////////////////////////////////////////-//
#include "fon9/seed/Plugins.hpp"
#include "fon9/framework/IoManager.hpp"

static bool RcSvServerAgent_Start(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
   fon9::StrView tag, value;
   while (fon9::SbrFetchTagValue(args, tag, value)) {
      if (tag == "AddTo") {
         if (fon9::rc::RcFunctionMgr* rcFuncMgr = fon9::rc::FindRcFunctionMgr(holder, value))
            rcFuncMgr->Add(fon9::rc::RcFunctionAgentSP{new fon9::rc::RcSeedVisitorServerAgent{}});
         else
            return false;
      }
      else {
         holder.SetPluginsSt(fon9::LogLevel::Error, "Unknown tag=", tag);
         return false;
      }
   }
   return true;
}
extern "C" fon9_API fon9::seed::PluginsDesc f9p_RcSvServerAgent;
static fon9::seed::PluginsPark f9pAutoPluginsReg{"RcSvServerAgent", &f9p_RcSvServerAgent};
fon9::seed::PluginsDesc f9p_RcSvServerAgent{"", &RcSvServerAgent_Start, nullptr, nullptr,};

