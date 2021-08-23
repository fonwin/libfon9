// \file fon9/rc/RcSeedVisitorServer.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcSeedVisitorServer.hpp"
#include "fon9/rc/RcSession.hpp"
#include "fon9/rc/RcFuncConnServer.hpp"
#include "fon9/auth/PolicyAcl.hpp"
#include "fon9/seed/FieldMaker.hpp"

#ifdef _DEBUG
// 通知 Server 端暫停執行訂閱, 等收到[取消訂閱]後, 才執行上次暫停的[訂閱要求].
// 提供給 RcSv_UT 驗證流程使用.
fon9_API bool gIs_RcSv_UT_Server_ReorderRunOnce;
#endif

namespace fon9 { namespace rc {

RcSeedVisitorServerAgent::~RcSeedVisitorServerAgent() {
}
RcFunctionNoteSP RcSeedVisitorServerAgent::OnCreateRcSeedVisitorServerNote(RcSession& ses,
                                                                           const auth::AuthResult& authr,
                                                                           seed::AclConfig&& aclcfg) {
   return RcFunctionNoteSP{new RcSeedVisitorServerNote(ses, authr, std::move(aclcfg))};
}
void RcSeedVisitorServerAgent::OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) {
   (void)param;
   if (auto* authNote = ses.GetNote<RcServerNote_SaslAuth>(f9rc_FunctionCode_SASL)) {
      const auth::AuthResult& authr = authNote->GetAuthResult();
      if (auto agAcl = authr.AuthMgr_->Agents_->Get<auth::PolicyAclAgent>(fon9_kCSTR_PolicyAclAgent_Name)) {
         seed::AclConfig  aclcfg;
         agAcl->GetPolicy(authr, aclcfg);

         // aclcfg 在呼叫 this->OnCreateRcSeedVisitorServerNote(); 時已用掉,
         // 不能在 this->OnCreateRcSeedVisitorServerNote(); 之後使用,
         // 所以提前建立 rbuf;
         RevBufferList  rbuf{256};
         // 最後一行: "|FcQryCount/FcQryMS|MaxSubrCount"
         // 回補流量管制 aclcfg.FcRecover_, 暫時不考慮告知 Client, 因為回補流量管制由 Server 處理.
         RevPrint(rbuf, fon9_kCSTR_ROWSPL /* Path="" */ fon9_kCSTR_CELLSPL,
                  aclcfg.FcQuery_.FcCount_, '/', aclcfg.FcQuery_.FcTimeMS_, fon9_kCSTR_CELLSPL,
                  aclcfg.MaxSubrCount_);
         agAcl->MakeGridView(rbuf, aclcfg);
         RevPrint(rbuf, fon9_kCSTR_LEAD_TABLE fon9_kCSTR_PolicyAclAgent_Name fon9_kCSTR_ROWSPL);
         PutBigEndian(rbuf.AllocPacket<SvFunc>(), SvFuncCode::Acl);

         if (auto note = this->OnCreateRcSeedVisitorServerNote(ses, authr, std::move(aclcfg))) {
            ses.Send(this->FunctionCode_, std::move(rbuf));
            ses.ResetNote(this->FunctionCode_, std::move(note));
         }
         else {
            // 若衍生者在 OnCreateRcSeedVisitorServerNote() 返回 nullptr,
            // 則應由衍生者自行處理 ses.ForceLogout(原因);
         }
      }
      else {
         ses.ForceLogout("Auth PolicyAclAgent not found.");
      }
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
   /// 必須保留一份 AuthR 給 seed::SeedVisitor 使用, 避免 Session 先死.
   const auth::AuthResult AuthR_;
   // 在此保留 Device, 避免在尚未取消訂閱前, Note 及 Visitor 被刪除.
   // 在 RcSeedVisitorServerNote::OnSessionLinkBroken() 時清除.
   io::DeviceSP Device_;
   SeedVisitor(io::DeviceSP dev, const auth::AuthResult& authr, seed::AclConfig&& aclcfg)
      : base(authr.AuthMgr_->MaRoot_, authr.MakeUFrom(ToStrView(dev->WaitGetDeviceId())), AuthR_)
      , AuthR_{authr}
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
   : FcQry_(static_cast<unsigned>(aclcfg.FcQuery_.FcCount_ * 2), // *2: for 緩衝.
            TimeInterval_Millisecond(aclcfg.FcQuery_.FcTimeMS_))
   , FcRecover_(aclcfg.FcRecover_.FcCount_ * 1000u,
                TimeInterval_Millisecond(aclcfg.FcRecover_.FcTimeMS_),
                aclcfg.FcRecover_.FcTimeMS_ /* 讓時間分割單位=1ms */)
   , Visitor_{new SeedVisitor(ses.GetDevice(), authr, std::move(aclcfg))} {
}
RcSeedVisitorServerNote::~RcSeedVisitorServerNote() {
}
void RcSeedVisitorServerNote::OnSessionLinkBroken() {
   SubrListImpl  subrs = std::move(*this->SubrList_.Lock());
   for (RcSvsSubrSP& preg : subrs) {
      if (preg) {
         preg->SvFunc_ = SvFuncCode::Empty; // 設定成: 因 LinkBroken 的取消註冊.
         if (seed::Tree* tree = preg->Tree_.get())
            tree->OnTreeOp(RcSvsUnsubRunnerSP{std::move(preg)});
      }
   }
   this->Visitor_->Device_.reset();
}
//--------------------------------------------------------------------------//
void RcSeedVisitorServerNote::OnRecvUnsubscribe(RcSession& ses, f9sv_SubrIndex usidx) {
   (void)ses;
   auto  subrs = this->SubrList_.Lock();
   if (usidx >= subrs->size()) {
      // ses.ForceLogout("f9sv_Unsubscribe bad SubrIndex.");
      // return;
   __ACK_UNSUBR_ID:;
      // 不存在的訂閱編號, 有可能是[訂閱失敗]的回覆還在路上, Client 就已送來取消訂閱要求.
      // 這種情況也要回覆 [取消訂閱成功]?
      // => 不用回覆, 由 Client 收到 [訂閱失敗] 時, 自行處理.
      // => 這裡直接忽略此 [取消訂閱] 的要求!
      // RevBufferList ackbuf{64};
      // ToBitv(ackbuf, usidx);
      // PutBigEndian(ackbuf.AllocBuffer(sizeof(SvFuncCode)), SvFuncCode::Unsubscribe);
      // ses.Send(f9rc_FunctionCode_SeedVisitor, std::move(ackbuf));
      return;
   }
   RcSvsSubrSP psubr = (*subrs)[usidx].get();
   if (psubr.get() == nullptr || psubr->SvFunc_ != SvFuncCode::Subscribe) {
      // ses.ForceLogout("f9sv_Unsubscribe no subscribe.");
      // return;
      goto __ACK_UNSUBR_ID;
   }
   psubr->SvFunc_ = SvFuncCode::Unsubscribe;
   if (auto tree = psubr->Tree_.get()) {
      // 已訂閱成功, 執行取消訂閱步驟.
      subrs.unlock(); // 必須先 unlock, 因為 tree->OnTreeOp() 可能會就地呼叫 op.
      tree->OnTreeOp(RcSvsUnsubRunnerSP{psubr});
   }
   else {
      // RcSvsSubscribeRunner 還在尋找 tree 準備訂閱,
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

#ifdef _DEBUG // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
   struct RunnerKeepOnce {
      seed::TicketRunnerSP Runner_;
      seed::OpResult       Result_;
      char                 Padding___[4];
      void FlushRunner() {
         if (this->Runner_) {
            if (this->Result_ == seed::OpResult::no_error)
               this->Runner_->Run();
            else
               this->Runner_->OnError(this->Result_);
            this->Runner_.reset();
            gIs_RcSv_UT_Server_ReorderRunOnce = false;
         }
      }
      bool PushRunner(seed::TicketRunnerSP& runner, seed::OpResult runResult) {
         if (gIs_RcSv_UT_Server_ReorderRunOnce) {
            if (!this->Runner_) {
               this->Runner_ = std::move(runner);
               this->Result_ = runResult;
               return true;
            }
         }
         return false;
      }
   };
   static RunnerKeepOnce   runnerKeep;
#endif // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

   if (fcCode == SvFuncCode::Unsubscribe) {
      // 取消訂閱, 只需要提供 usidx;
      f9sv_SubrIndex usidx{kSubrIndexNull};
      BitvTo(param.RecvBuffer_, usidx);
      this->OnRecvUnsubscribe(ses, usidx);
      #ifdef _DEBUG
         runnerKeep.FlushRunner();
      #endif
      return;
   }
   // 保留 origReqKey 存放在 TicketRunner::Bookmark_, 在回覆結果時當成 key 返回.
   CharVector origReqKey;
   param.RecvBuffer_.Read(origReqKey.alloc(param.RemainParamSize_), param.RemainParamSize_);

   DcQueueFixedMem rxbuf{ToStrView(origReqKey)};
   RcSvReqKey      reqKey;
   reqKey.LoadSeedName(rxbuf);
   // 因為 rxbuf 可能還包含其他非 ReqKey 的資料, 所以在此使用 origReqKeyEnd 紀錄實際 ReqKey 的尾端.
   // 當有其他仍為 key 的資料時(例: reqKey.SubrIndex_), 則需要調整 origReqKeyEnd;
   const char*     origReqKeyEnd = reinterpret_cast<const char*>(rxbuf.Peek1());
   RcSvsTicket     ticket;
   seed::OpResult  runResult = seed::OpResult::no_error;
   if (fcCode == SvFuncCode::Subscribe) {
      BitvTo(rxbuf, reqKey.SubrIndex_);
      origReqKeyEnd = reinterpret_cast<const char*>(rxbuf.Peek1());
      const auto  maxSubrCount = this->Visitor_->GetAclConfig().MaxSubrCount_;
      auto        subrs = this->SubrList_.Lock();
      // 不符合訂閱規範者, 立即斷線: ForceLogout()
      if (maxSubrCount > 0 && reqKey.SubrIndex_ >= maxSubrCount) {
         ses.ForceLogout("f9sv_Subscribe over MaxSubrCount.");
         return;
      }
      if (reqKey.SubrIndex_ > subrs->size() + 100) {
         // 一般而言, reqKey.SubrIndex_ 會依序增加.
         // 但如果 client端取消「內部 pending 訂閱」, 因該訂閱尚未送出, 則有可能造成後續的訂閱跳號.
         // 為了避免發生此情況, 所以在此設定一個允許的「最大跳號 = N」數量: subrs->size() + N;
         // 但這樣仍無法避免「惡意」連線造成的記憶體用盡的問題, 所以一般而言應設定 MaxSubrCount 權限.
         ses.ForceLogout("f9sv_Subscribe bad SubrIndex.");
         return;
      }
      RcSvsSubrSP* ppSubr;
      if (reqKey.SubrIndex_ >= subrs->size()) {
         subrs->resize(reqKey.SubrIndex_ + 1);
         ppSubr = &(*subrs)[reqKey.SubrIndex_];
      }
      else {
         ppSubr = &(*subrs)[reqKey.SubrIndex_];
         if (auto* psubr = ppSubr->get()) {
            if (!psubr->IsSubjectClosed_) {
               ses.ForceLogout("f9sv_Subscribe dup subscribe.");
               return;
            }
         }
      }
      ppSubr->reset(new RcSvsSubr(reqKey.SubrIndex_, ToStrView(reqKey.SeedKey_), ses.GetDevice()));
      if (reqKey.IsAllTabs_)
         runResult = seed::OpResult::not_found_tab;
      ticket.Assign(new RcSvsSubscribeRunner{*this->Visitor_, std::move(reqKey)});
   }
   else {
      const auto fcti = this->FcQry_.Fetch(param.RecvTime_);
      if (fcti.GetOrigValue() > 0) {
         // 當有回覆 layout 時, 表示 client 端可能有 pending, 此時應適度放寬流量.
         // 因為 client 通常只會查詢少數資料表(系統中也不會有太多資料表),
         // 所以在此用「超過次數」來處理「適度放寬」.
         if (++this->FcOverCount_ > 30) {
            ses.ForceLogout(RevPrintTo<std::string>("RcSvs Flow control:fn=", fcCode, "|ti=", fcti));
            return;
         }
      }
      fon9_WARN_DISABLE_SWITCH;
      switch (fcCode) {
      case SvFuncCode::Query: {
            CharVector seedPath = reqKey.SeedPath();
            // TODO: 是否允許使用範圍(key range)查詢? [from - to] or [from + count].
            ticket.Assign(new RcSvsTicketRunnerQuery{*this->Visitor_, ToStrView(seedPath), ToStrView(reqKey.TabName_)});
         }
         break;
      case SvFuncCode::Write: {
            std::string values;
            BitvTo(rxbuf, values);
            // values.empty(); => 取得 seed 內容;
            ticket.Assign(new RcSvsTicketRunnerWrite{*this->Visitor_, ToStrView(reqKey.SeedTabPath()), std::move(values)});
         }
         break;
      case SvFuncCode::Remove:
         ticket.Assign(new RcSvsTicketRunnerRemove{*this->Visitor_, ToStrView(reqKey.SeedTabPath())});
         break;
      case SvFuncCode::Command: {
            std::string cmdln;
            BitvTo(rxbuf, cmdln);
            StrView     svCmdLn = ToStrView(cmdln);
            if (StrTrimHead(&svCmdLn).empty()) // 不支援 cmdln.empty(), 所以改成 "?" = help;
               cmdln.push_back('?');
            ticket.Assign(new RcSvsTicketRunnerCommand{*this->Visitor_, ToStrView(reqKey.SeedTabPath()), std::move(cmdln)});
         }
         break;
      case SvFuncCode::GridView: {
            RcSvsTicketRunnerGridView::ReqMaxRowCountT reqMaxRowCount = 0;
            BitvTo(rxbuf, reqMaxRowCount);
            ticket.Assign(new RcSvsTicketRunnerGridView(*this->Visitor_, ToStrView(reqKey.TreePath_), reqMaxRowCount,
                                                        ToStrView(reqKey.SeedKey_), ToStrView(reqKey.TabName_)));
         }
         break;
      }
   }
   fon9_WARN_POP;

   if (ticket.Runner_) {
      ticket.Req_->Device_    = ses.GetDevice();
      ticket.Req_->SvFunc_    = fc;
      ticket.Req_->IsAllTabs_ = reqKey.IsAllTabs_;
      if (origReqKeyEnd) // 調整實際的 ReqKey 長度.
         origReqKey.resize(static_cast<size_t>(origReqKeyEnd - origReqKey.begin()));
      ticket.Runner_->Bookmark_ = std::move(origReqKey);
      #ifdef _DEBUG
         if (runnerKeep.PushRunner(ticket.Runner_, runResult))
            return;
      #endif
      if (runResult == seed::OpResult::no_error) {
         ticket.Runner_->Run();
      }
      else {
         ticket.Runner_->OnError(runResult);
      }
      #ifdef _DEBUG
         runnerKeep.FlushRunner();
      #endif
   }
   else {
      ses.ForceLogout(RevPrintTo<std::string>("RcSvs unknown fn=", fcCode));
   }
}
//--------------------------------------------------------------------------//
// return gv size;
template <class SeedNotifyArgsT>
static size_t PutGridViewBuffer(RevBufferList& rbuf, SeedNotifyArgsT& e) {
   assert(rbuf.cfront() == nullptr);
   const std::string* gvStr = nullptr;
   rbuf = RevBufferList{64, e.GetGridViewBuffer(&gvStr)};
   if (auto* cbuf = rbuf.cfront()) {
      auto sz = CalcDataSize(cbuf);
      return sz + ByteArraySizeToBitvT(rbuf, sz);
   }
   if (gvStr == nullptr || gvStr->empty())
      return 0;
   auto sz = gvStr->size();
   return sz + ByteArrayToBitv_NoEmpty(rbuf, gvStr->c_str(), sz);
}

void RcSeedVisitorServerNote::OnSubscribeNotify(RcSvsSubrSP preg, const seed::SeedNotifyArgs& e) {
   RevBufferList ackbuf{128};
   RcSession&    ses = *static_cast<RcSession*>(preg->Device_->Session_.get());
   switch (e.NotifyKind_) {
   default: // Unknown NotifyKind;
      return;
   case seed::SeedNotifyKind::SubscribeStreamOK:
   case seed::SeedNotifyKind::SubscribeOK:
      assert(dynamic_cast<const seed::SeedNotifySubscribeOK*>(&e) != nullptr);
      if (auto* note = static_cast<RcSeedVisitorServerNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor))) {
         assert(note->Runner_ != nullptr);
         auto& runnerAckBuf = note->Runner_->AckBuffer_;
         // 訂閱成功 {立即回覆的 tab(seeds) 內容;}
         // 若為非Stream訂閱, 則格式會與 RcSvsReq::PackSeedValues(); 相同.
         const auto gvsz = PutGridViewBuffer(runnerAckBuf, *static_cast<const seed::SeedNotifySubscribeOK*>(&e));
         if (e.NotifyKind_ == seed::SeedNotifyKind::SubscribeStreamOK) {
            if (gvsz == 0)
               RevPutBitv(runnerAckBuf, fon9_BitvV_ByteArrayEmpty);
            goto __ACK_SUCCESS_0;
         }
         if (gvsz == 0) {
         __ACK_SUCCESS_0:;
            // 訂閱成功, 告知有 0 筆立即回覆的 tab(seeds) 內容.
            ToBitv(runnerAckBuf, seed::OpResult::no_error);
            RevPutBitv(runnerAckBuf, fon9_BitvV_Number0);
         }
         else { // 訂閱成功, 告知有1筆立即回覆的 tab(seeds) 內容.
            ToBitv(runnerAckBuf, static_cast<f9sv_TabSize>(e.Tab_->GetIndex()));
            ToBitv(runnerAckBuf, seed::OpResult::no_error);
            ToBitv(runnerAckBuf, 1u);
         }
         note->Runner_->SendRcSvAck(note->Runner_->Bookmark_);
      }
      return;
   case seed::SeedNotifyKind::TableChanged:
      ToBitv(ackbuf, e.GetGridView());
      break;
   case seed::SeedNotifyKind::StreamRecover:
   case seed::SeedNotifyKind::StreamRecoverEnd:
      if (auto* note = static_cast<RcSeedVisitorServerNote*>(ses.GetNote(f9rc_FunctionCode_SeedVisitor))) {
         assert(dynamic_cast<const seed::SeedNotifyStreamRecoverArgs*>(&e) != nullptr);
         const auto gvsz = PutGridViewBuffer(ackbuf, *static_cast<const seed::SeedNotifyStreamRecoverArgs*>(&e));
         TimeInterval fc = note->FcRecover_.CalcUsed(UtcNow(), gvsz + 16);
         if (!fc.IsZero() && e.NotifyKind_ != seed::SeedNotifyKind::StreamRecoverEnd) {
            static_cast<const seed::SeedNotifyStreamRecoverArgs*>(&e)->FlowControlWait_ = fc;
            if (fc.GetOrigValue() < 0)
               return;
         }
         if (gvsz == 0) // gvsz > 0 已在 PutGridViewBuffer() 填好了;
            RevPutBitv(ackbuf, fon9_BitvV_ByteArrayEmpty);
         goto __CHECK_PUT_KEY_FOR_SUBR_TREE;
      }
      return;
   case seed::SeedNotifyKind::StreamEnd:
      preg->IsSubjectClosed_ = true;
      /* fall through */ // 繼續處理 gv 及 key 填入 ackbuf;
   case seed::SeedNotifyKind::SeedChanged:
   case seed::SeedNotifyKind::StreamData:
      ToBitv(ackbuf, e.GetGridView());
      goto __CHECK_PUT_KEY_FOR_SUBR_TREE;
   case seed::SeedNotifyKind::PodRemoved:
   case seed::SeedNotifyKind::SeedRemoved:
      preg->IsSubjectClosed_ = true;
   __CHECK_PUT_KEY_FOR_SUBR_TREE:;
      // 如果訂閱的是 tree, 則回報時, 一律額外提供 seedKey.
      // 在 fon9/rc/RcSvc.cpp 裡面的 RxSubrData::CheckLoadSeedKey(); 會取出此 seedKey;
      // 如果回覆的訊息與 seedKey 無關, 仍須提供此值,
      // 此時可用 seed::TextBegin() 來減少資料量: 只需 1 byte 就可打包 seed::TextBegin().
      if (seed::IsSubrTree(preg->SeedKey_.begin()))
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
