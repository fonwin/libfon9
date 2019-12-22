// \file fon9/rc/RcSeedVisitorServer.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSeedVisitorServer.hpp"
#include "fon9/rc/RcSeedVisitor.hpp"
#include "fon9/rc/RcSession.hpp"
#include "fon9/rc/RcFuncConn.hpp"
#include "fon9/auth/PolicyAcl.hpp"
#include "fon9/io/Device.hpp"
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
         RevBufferList       rbuf{256};
         // 最後一行: "|FcQryCount/FcQryMS|MaxSubrCount"
         RevPrint(rbuf, fon9_kCSTR_ROWSPL /* Path="" */ fon9_kCSTR_CELLSPL,
                  aclcfg.FcQuery_.FcCount_, '/', aclcfg.FcQuery_.FcTimeMS_, fon9_kCSTR_CELLSPL,
                  aclcfg.MaxSubrCount_);
         agAcl->MakeGridView(rbuf, aclcfg);
         RevPrint(rbuf, fon9_kCSTR_LEAD_TABLE fon9_kCSTR_PolicyAclAgent_Name fon9_kCSTR_ROWSPL);
         PutBigEndian(rbuf.AllocPacket<SvFuncCode>(), SvFuncCode::Acl);
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
}
//--------------------------------------------------------------------------//
struct RcSeedVisitorServerNote::SeedVisitor : public seed::SeedVisitor {
   fon9_NON_COPY_NON_MOVE(SeedVisitor);
   using base = seed::SeedVisitor;
   // 在此保留 Device, 避免在尚未取消訂閱前, Note 及 Visitor 被刪除.
   io::DeviceSP Device_;
   SeedVisitor(io::DeviceSP dev, const auth::AuthResult& authr, seed::AclConfig&& aclcfg)
      : base(authr.AuthMgr_->MaRoot_, authr.MakeUFrom(ToStrView(dev->WaitGetDeviceId())))
      , Device_{std::move(dev)} {
      this->Fairy_->ResetAclConfig(std::move(aclcfg));
   }
   const seed::AclConfig& GetAclConfig() const {
      return this->Fairy_->GetAclConfig();
   }
   void OnTicketRunnerDone(seed::TicketRunner& runner, DcQueue&& extmsg) override {
      (void)runner; (void)extmsg;
      assert(!"It should not be executed here.");
   }
   void OnTicketRunnerWrite(seed::TicketRunnerWrite& runner, const seed::SeedOpResult& res, const seed::RawWr& wr) override {
      (void)runner; (void)res; (void)wr;
      assert(!"It should not be executed here.");
   }
   void OnTicketRunnerRead(seed::TicketRunnerRead& runner, const seed::SeedOpResult& res, const seed::RawRd& rd) override {
      (void)runner; (void)res; (void)rd;
      assert(!"It should not be executed here.");
   }
   void OnTicketRunnerRemoved(seed::TicketRunnerRemove& runner, const seed::PodRemoveResult& res) override {
      (void)runner; (void)res;
      assert(!"It should not be executed here.");
   }
   void OnTicketRunnerSubscribe(seed::TicketRunnerSubscribe& runner, bool isSubOrUnsub) override {
      (void)runner; (void)isSubOrUnsub;
      assert(!"It should not be executed here.");
   }
   void OnSeedNotify(seed::VisitorSubr& subr, const seed::SeedNotifyArgs& args) override {
      (void)subr; (void)args;
   }
   void OnTicketRunnerBeforeGridView(seed::TicketRunnerGridView& runner, seed::TreeOp& opTree, seed::GridViewRequest& req) override {
      (void)runner; (void)opTree; (void)req;
      assert(!"It should not be executed here.");
      // TODO: 支援 GridView?
   }
   void OnTicketRunnerGridView(seed::TicketRunnerGridView& runner, seed::GridViewResult& res) override {
      (void)runner; (void)res;
      // TODO: 支援 GridView?
      assert(!"It should not be executed here.");
   }
   void OnTicketRunnerCommand(seed::TicketRunnerCommand& runner, const seed::SeedOpResult& res, StrView msg) override {
      (void)res; assert(runner.OpResult_ == res.OpResult_);
      this->OnTicketRunnerDone(runner, DcQueueFixedMem{msg});
   }
   void OnTicketRunnerSetCurrPath(seed::TicketRunnerCommand& runner) override {
      this->OnTicketRunnerDone(runner, DcQueueFixedMem{});
   }
};
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
struct TickRunnerQuery : public seed::TicketRunnerRead {
   fon9_NON_COPY_NON_MOVE(TickRunnerQuery);
   using base = seed::TicketRunnerRead;
   io::DeviceSP   Device_; // 為了確保在 Query 執行期間, device/session 不會死亡.
   SvFuncCode     SvFunc_;
   bool           IsAllTabs_;
   char           Padding__[6];
   RevBufferList  AckBuffer_{256};

   TickRunnerQuery(seed::SeedVisitor& visitor, StrView seed)
      : base(visitor, seed) {
   }
   ~TickRunnerQuery() {
      RevPrint(this->AckBuffer_, this->Bookmark_);
      PutBigEndian(this->AckBuffer_.AllocBuffer(sizeof(this->SvFunc_)), this->SvFunc_);
      static_cast<RcSession*>(this->Device_->Session_.get())->Send(
         f9rc_FunctionCode_SeedVisitor,
         std::move(this->AckBuffer_));
   }
   void OnLastStep(seed::TreeOp& op, StrView keyText, seed::Tab& tab) override {
      if (!this->IsAllTabs_)
         base::OnLastStep(op, keyText, tab);
      else {
         f9sv_TabSize L = 0;
         while (auto* ptab = op.Tree_.LayoutSP_->GetTab(L++))
            base::OnLastStep(op, keyText, *ptab);
      }
      if (IsEnumContains(this->SvFunc_, SvFuncCode::FlagLayout))
         MakeAckLayout(this->AckBuffer_, *op.Tree_.LayoutSP_);
   }
   void OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab) override {
      base::OnLastSeedOp(resPod, pod, tab);
      ToBitv(this->AckBuffer_, tab.GetIndex());
   }
   void OnReadOp(const seed::SeedOpResult& res, const seed::RawRd* rd) override {
      if (rd == nullptr)
         this->OnError(res.OpResult_);
      else {
         const auto  szBefore = CalcDataSize(this->AckBuffer_.cfront());
         seed::FieldsCellRevPrint(res.Tab_->Fields_, *rd, this->AckBuffer_);
         ByteArraySizeToBitvT(this->AckBuffer_, CalcDataSize(this->AckBuffer_.cfront()) - szBefore);
      }
   }
   void OnError(seed::OpResult res) override {
      ToBitv(this->AckBuffer_, res);
   }
};
//--------------------------------------------------------------------------//
RcSeedVisitorServerNote::RcSeedVisitorServerNote(RcSession& ses, const auth::AuthResult& authr, seed::AclConfig&& aclcfg)
   : Visitor_{new SeedVisitor(ses.GetDevice(), authr, std::move(aclcfg))}
   , FcQry_(static_cast<unsigned>(aclcfg.FcQuery_.FcCount_ * 2), // *2: for 緩衝.
            TimeInterval_Millisecond(aclcfg.FcQuery_.FcTimeMS_)) {
}
RcSeedVisitorServerNote::~RcSeedVisitorServerNote() {
}
void RcSeedVisitorServerNote::OnSessionLinkBroken() {
   if (0); this->Visitor_->Device_.reset(); // TODO: 取消訂閱, 然後清除 device.
}
//--------------------------------------------------------------------------//
void RcSeedVisitorServerNote::OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) {
   static_assert(sizeof(SvFuncCode) == 1, "sizeof(SvFuncCode) != 1, 不可使用 Peek1().");
   const void* fcPtr = param.RecvBuffer_.Peek1();
   if (fcPtr == nullptr || param.RemainParamSize_ == 0)
      return;
   const SvFuncCode fc = GetBigEndian<SvFuncCode>(fcPtr);
   param.RecvBuffer_.PopConsumed(sizeof(SvFuncCode));
   param.RemainParamSize_ -= sizeof(SvFuncCode);

   /// 保留 origReqKey, 在查詢結束時當成 key 返回.
   CharVector origReqKey;
   param.RecvBuffer_.Read(origReqKey.alloc(param.RemainParamSize_), param.RemainParamSize_);

   DcQueueFixedMem rxbuf{ToStrView(origReqKey)};
   RcSvReqKey      reqKey;
   reqKey.LoadFrom(rxbuf);

   CharVector seedPath{ToStrView(reqKey.TreePath_)};
   seedPath.push_back('/');
   seedPath.append(ToStrView(reqKey.SeedKey_));
   if (!reqKey.TabName_.empty()) {
      seedPath.push_back('^');
      seedPath.append(ToStrView(reqKey.TabName_));
   }

   fon9_WARN_DISABLE_SWITCH;
   switch (fc & SvFuncCode::FuncMask) {
   case SvFuncCode::Subscribe:
      // - 訂閱? 取消訂閱?
      // - 訂閱通知: 有些資料應提供「自訂打包」, 例:
      //   - 只更新某幾個欄位.
      //   - 資料在欄位間移動(插入或刪除).
      //     - 用欄位群組決定(插入或刪除)的影響範圍.
      //     - 欄位名稱在首個底線前相同, 視為同一群組, 例: BP_1, BP_2...
      break;
   case SvFuncCode::Query:
      // TODO: key range: [from - to] or [from + count].
      if (FcQry_.Fetch(param.RecvTime_).GetOrigValue() > 0) {
         ses.ForceLogout("f9sv_Query flow control.");
         return;
      }
      intrusive_ptr<TickRunnerQuery> query{new TickRunnerQuery{*this->Visitor_, ToStrView(seedPath)}};
      query->Device_ = ses.GetDevice();
      query->SvFunc_ = fc;
      query->IsAllTabs_ = reqKey.IsAllTabs_;
      query->Bookmark_ = std::move(origReqKey);
      query->Run();
      break;
   }
   fon9_WARN_POP;
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

