// \file f9twf/ExgMcChannel.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMcChannel.hpp"
#include "f9twf/ExgMdPkReceiver.hpp"
#include "f9twf/ExgMcFmtSS.hpp"
#include "f9twf/ExgMrRecover.hpp"
#include "fon9/FileReadAll.hpp"
#include "fon9/Log.hpp"

namespace f9twf {
namespace f9fmkt = fon9::fmkt;

ExgMcMessageConsumer::~ExgMcMessageConsumer() {
}
//--------------------------------------------------------------------------//
void ExgMcChannel::StartupChannel(std::string logPath) {
   this->State_ = (IsEnumContainsAny(this->Style_, ExgMcChannelStyle::WaitBasic | ExgMcChannelStyle::WaitSnapshot)
                   ? ExgMcChannelState::Waiting : ExgMcChannelState::Running);
   this->CycleStartSeq_ = 0;
   this->CycleBeforeLostCount_ = 0;
   this->Pk1stPos_ = 0;
   this->Pk1stSeq_ = 0;
   this->Clear();
   this->PkLog_.reset();
   if (IsEnumContainsAny(this->Style_, ExgMcChannelStyle::PkLog | ExgMcChannelStyle::Reload)) {
      fon9::NumOutBuf nbuf;
      logPath.append(fon9::ToStrRev(nbuf.end(), this->ChannelId_, fon9::FmtDef{4, fon9::FmtFlag::IntPad0}),
                     nbuf.end());
      logPath.append(".bin");
      this->PkLog_ = fon9::AsyncFileAppender::Make();
      auto res = this->PkLog_->OpenImmediately(logPath, fon9::FileMode::CreatePath | fon9::FileMode::Read | fon9::FileMode::Append);
      if (res.HasResult()) {
         res = this->PkLog_->GetFileSize();
         if (res.HasResult())
            this->Pk1stPos_ = res.GetResult();
      }
      if (!res.HasResult()) {
         this->PkLog_.reset();
         fon9_LOG_FATAL(this->ChannelMgr_->Name_, ".StartupChannel|channelId=", this->ChannelId_, "|fn=", logPath, '|', res);
      }
   }
   if (this->IsSnapshot()) // 快照更新, 尚未收到 A:Refresh Begin 之前, 不記錄 PkLog.
      this->IsSkipPkLog_ = true;
}
void ExgMcChannel::SetupReload() {
   if (!this->PkLog_ || !IsEnumContains(this->Style_, ExgMcChannelStyle::Reload))
      return;
   struct McReloader : public ExgMcPkReceiver {
      fon9_NON_COPY_NON_MOVE(McReloader);
      ExgMcChannel&           Owner_;
      const ExgMcChannelState BfState_;
      char                    Padding___[7];
      McReloader(ExgMcChannel& owner) : Owner_(owner), BfState_{owner.State_} {
         owner.IsSkipPkLog_ = owner.IsSetupReloading_ = true;
      }
      ~McReloader() {
         this->Owner_.IsSkipPkLog_ = this->Owner_.IsSetupReloading_ = false;
      }
      bool operator()(fon9::DcQueue& rdbuf, fon9::File::Result&) {
         this->FeedBuffer(rdbuf);
         return true;
      }
      bool OnPkReceived(const void* pk, unsigned pksz) override {
         this->Owner_.OnPkReceived(*static_cast<const ExgMcHead*>(pk), pksz);
         return true;
      }
   };
   McReloader           reloader{*this};
   fon9::File::SizeType fpos = 0;
   fon9::File::Result   res = fon9::FileReadAll(*this->PkLog_, fpos, reloader);
   if (res.IsError())
      fon9_LOG_FATAL(this->ChannelMgr_->Name_, ".SetupReload|channelId=", this->ChannelId_, "|pos=", fpos, '|', res);
}
void ExgMcChannel::ReloadDispatch(SeqT fromSeq) {
   if (!this->PkLog_)
      return;
   struct McReloader : public ExgMcPkReceiver {
      fon9_NON_COPY_NON_MOVE(McReloader);
      ExgMcChannel&        Owner_;
      const SeqT           FromSeq_;
      PkPendings::Locker   Locker_;
      McReloader(ExgMcChannel& owner, SeqT fromSeq)
         : Owner_(owner)
         , FromSeq_{fromSeq}
         , Locker_{owner.PkPendings_.Lock()} {
      }
      bool operator()(fon9::DcQueue& rdbuf, fon9::File::Result&) {
         this->FeedBuffer(rdbuf);
         return true;
      }
      bool OnPkReceived(const void* pk, unsigned pksz) override {
         SeqT seq = static_cast<const ExgMcHead*>(pk)->GetChannelSeq();
         if (this->FromSeq_ == 0 || seq >= this->FromSeq_) {
            ExgMcMessage e(*static_cast<const ExgMcHead*>(pk), pksz, this->Owner_, seq);
            this->Owner_.DispatchMcMessage(e);
         }
         return true;
      }
   };
   McReloader           reloader{*this, fromSeq};
   fon9::File::SizeType fpos = this->Pk1stPos_;
   fon9::File::Result   res = fon9::FileReadAll(*this->PkLog_, fpos, reloader);
   if (res.IsError())
      fon9_LOG_FATAL(this->ChannelMgr_->Name_, ".ReloadDispatch|channelId=", this->ChannelId_, "|pos=", fpos, '|', res);
   if (this->State_ < ExgMcChannelState::Cycled)
      this->State_ = ExgMcChannelState::Running;
}
void ExgMcChannel::OnBasicInfoCycled() {
   if (IsEnumContains(this->Style_, ExgMcChannelStyle::WaitBasic))
      this->ReloadDispatch(0);
}
void ExgMcChannel::OnSnapshotDone(SeqT lastRtSeq) {
   if (IsEnumContains(this->Style_, ExgMcChannelStyle::WaitSnapshot)
       && this->State_ == ExgMcChannelState::Waiting)
      this->ReloadDispatch(this->IsRealtime() ? lastRtSeq + 1 : 0);
}
//--------------------------------------------------------------------------//
void ExgMcChannel::AddRecover(ExgMrRecoverSessionSP svr) {
   auto rs = this->Recovers_.Lock();
   for (auto& cur : *rs) {
      if (cur == svr)
         return;
   }
   if (svr->Role_ == ExgMcRecoverRole::Primary)
      rs->push_front(svr);
   else
      rs->push_back(svr);
}
void ExgMcChannel::DelRecover(ExgMrRecoverSessionSP svr) {
   bool isFront;
   {
      auto rs = this->Recovers_.Lock();
      auto ifind = std::find(rs->begin(), rs->end(), svr);
      if (ifind == rs->end())
         return;
      isFront = (ifind == rs->begin());
      rs->erase(ifind);
   }
   if (isFront && !this->PkPendings_.Lock()->empty())
      this->StartPkContTimer();
}
void ExgMcChannel::OnRecoverErr(SeqT beginSeqNo, unsigned recoverNum, ExgMrStatusCode st) {
   bool isNoRecover = false;
   if (st == 5) { // 5: 備援尚未開放回補.
      auto rs = this->Recovers_.Lock();
      isNoRecover = (rs->empty() || rs->front()->Role_ != ExgMcRecoverRole::Primary);
   }
   auto pks = this->PkPendings_.Lock();
   if (!pks->empty()) {
      SeqT endSeq = beginSeqNo + recoverNum;
      if (this->NextSeq_ < endSeq)
         this->NextSeq_ = endSeq;
      if (isNoRecover) {
         this->base::PkContOnTimer(std::move(pks));
         return;
      }
      pks.unlock();
      this->StartPkContTimer();
   }
}
void ExgMcChannel::OnRecoverEnd(ExgMrRecoverSession* svr) {
   fon9_LOG_INFO(this->ChannelMgr_->Name_, ".RecoverEnd"
                 "|lastInfoTime=", this->LastInfoTime_,
                 "|dev=", fon9::ToPtr(svr->GetDevice()),
                 "|recover.SessionId=", svr->SessionId_,
                 "|requestCount=", svr->GetRequestCount());
   // 補完了, 若 channel 的 PkPending 仍有訊息, 則應繼續回補.
   auto pks = this->PkPendings_.Lock();
   if (!pks->empty())
      this->StartPkContTimer();
}
void ExgMcChannel::PkContOnTimer(PkPendings::Locker&& pks) {
   const auto keeps = pks->size();
   if (keeps == 0)
      return;
   // 每次最多回補 1000 筆.
   constexpr ExgMrRecoverNum_t kMaxRecoverNum = 1000;
   // 每個 Session 一天最多可以要求 1000 次(2020/01/15:已無次數限制).
   // 必須等上次要求完成, 才能送出下一個要求.
   constexpr uint32_t kMaxRequestCount = std::numeric_limits<uint32_t>::max();//1000;

   const bool  isKeepsNoGap = (keeps == 1 || (pks->back().Seq_ - pks->begin()->Seq_ + 1 == keeps));
   const SeqT  lostCount = (isKeepsNoGap ? pks->begin()->Seq_ : pks->back().Seq_) - this->NextSeq_;
   const ExgMrRecoverNum_t recoverNum = (lostCount > kMaxRecoverNum
                                         ? kMaxRecoverNum
                                         : static_cast<ExgMrRecoverNum_t>(lostCount));
   ExgMrRecoverSessionSP svr;
   bool hasAnyRecover = false;
   {
      auto rs = this->Recovers_.Lock();
      auto iend = rs->end();
      for (auto ibeg = rs->begin(); ibeg != iend;) {
         auto* cur = ibeg->get();
         // 在有 Primary 的時候, 不可使用 Secondary.
         if (rs->front()->Role_ != cur->Role_)
            break;
         if (fon9_UNLIKELY(cur->GetRequestCount() >= kMaxRequestCount)) {
            fon9_LOG_WARN(this->ChannelMgr_->Name_,
                          "|dev=", fon9::ToPtr(cur->GetDevice()),
                          "|err=request count over limit.");
            ibeg = rs->erase(ibeg);
            continue;
         }
         auto st = cur->RequestMcRecover(this->ChannelId_,
                                         static_cast<ExgMrMsgSeqNum_t>(this->NextSeq_),
                                         recoverNum);
         if (st == ExgMrState::Requesting) { // 成功送出回補要求.
            svr = cur;
            break;
         }
         if (st != ExgMrState::ApReady) // 連線問題.
            ibeg = rs->erase(ibeg);
         else { // 有回補要求正在處理中.
            hasAnyRecover = true;
            ++ibeg;
         }
      }
   }
   if (fon9_UNLIKELY(fon9::LogLevel::Warn >= fon9::LogLevel_)) {
      fon9::RevBufferList rbuf_{fon9::kLogBlockNodeSize};
      fon9::RevPrint(rbuf_, "|keeps=", keeps, '\n');
      if (!isKeepsNoGap)
         fon9::RevPrint(rbuf_, "|back=", pks->back().Seq_);
      fon9::RevPrint(rbuf_, "|channelId=", this->ChannelId_,
                     "|from=", this->NextSeq_, "|to=", pks->begin()->Seq_ - 1,
                     "|lostCount=", lostCount);
      if (svr) {
         fon9::RevPrint(rbuf_, "|dev=", fon9::ToPtr(svr->GetDevice()),
                        "|recover.SessionId=", svr->SessionId_,
                        "|requestCount=", svr->GetRequestCount(),
                        "|recoverNum=", recoverNum);
      }
      fon9::RevPrint(rbuf_, this->ChannelMgr_->Name_, ".PkLost"
                     "|lastInfoTime=", this->LastInfoTime_);
      fon9::LogWrite(fon9::LogLevel::Warn, std::move(rbuf_));
   }
   if (svr)
      return;
   if (hasAnyRecover)
      this->StartPkContTimer();
   else
      base::PkContOnTimer(std::move(pks));
}
// -----
// 收到完整封包後, 會執行到此, 尚未確定是否重複或有遺漏.
ExgMcChannelState ExgMcChannel::OnPkReceived(const ExgMcHead& pk, unsigned pksz) {
   assert(pk.GetChannelId() == this->ChannelId_);
   const SeqT seq = pk.GetChannelSeq();
   // ----- 處理特殊訊息: Hb, SeqReset...
   if (fon9_UNLIKELY(pk.TransmissionCode_ == '0')) {
      switch (pk.MessageKind_) {
      case '1':
         if (this->IsSnapshot()) // 快照更新: 使用 (A:Refresh Begin .. Z:Refresh Complete) 判斷 CycledClose.
            return this->State_;
         // Heartbeat: CHANNEL-SEQ 欄位會為該傳輸群組之末筆序號訊息。
         if (this->NextSeq_ != seq + 1) // 序號不正確的 Hb, 不處理.
            return this->State_;
         this->PkLogAppend(&pk, pksz, seq);
         // 檢查 Channel 是否已符合 Cycled 條件.
         if (seq == 0 // 期交所系統已啟動, 但尚未開始發送輪播訊息.
             || this->CycleStartSeq_ == seq + 1) // 兩次輪播循環之間的 Hb: 不用理會.
            return this->State_;
         if (IsEnumContainsAny(this->Style_, ExgMcChannelStyle::CycledClose | ExgMcChannelStyle::CycledNotify)
             && this->CycleStartSeq_
             && this->State_ < ExgMcChannelState::Cycled // ChannelCycled() 事件只觸發一次.
             && this->LostCount_ - this->CycleBeforeLostCount_ == 0) { // 沒有遺漏, 才算是 Cycled 完成.
            this->State_ = (IsEnumContains(this->Style_, ExgMcChannelStyle::CycledClose)
                            ? ExgMcChannelState::CanBeClosed : ExgMcChannelState::Cycled);
            this->ChannelMgr_->ChannelCycled(*this);
         }
         this->OnCycleStart(seq + 1);
         return this->State_;
      case '2': // I002.SeqReset.
         if (0);//SeqReset 之後, API 商品訂閱者的序號要怎麼處理呢?
         fon9_LOG_WARN(this->ChannelMgr_->Name_, ".I002.SeqReset|channelId=", this->ChannelId_);
         RecoversImpl rs{std::move(*this->Recovers_.Lock())};
         for (auto& cur : rs) {
            cur->GetDevice()->AsyncClose("I002.SeqReset");
            cur->GetDevice()->WaitGetDeviceId(); // 等候 AsyncClose() 完成.
            cur->GetDevice()->AsyncOpen(std::string{});
         }
         {
            auto pks = this->PkPendings_.Lock();
            pks->clear();
            this->NextSeq_ = 0;
         }
         return this->State_;
      }
   }
   // ----- 如果允許任意順序, 則可先直接處理 DispatchMcMessage().
   if (IsEnumContains(this->Style_, ExgMcChannelStyle::AnySeq)) {
      if (this->State_ != ExgMcChannelState::Waiting) {
         ExgMcMessage e(pk, pksz, *this, seq);
         auto         locker = this->PkPendings_.Lock();
         this->DispatchMcMessage(e);
      }
   }
   // ----- 不允許任意順序 or 有其他 Style, 則應透過 [序號連續] 機制處理.
   if ((this->Style_ - ExgMcChannelStyle::AnySeq) != ExgMcChannelStyle{}
       || this->Style_ == ExgMcChannelStyle{})
      this->FeedPacket(&pk, pksz, seq);
   return this->State_;
}
// -----
// 確定有連續, 或確定無法連續, 會執行到此.
// 此時 this->PkPendings_ 是 locked 狀態.
void ExgMcChannel::PkContOnReceived(const void* pk, unsigned pksz, SeqT seq) {
   if (fon9_UNLIKELY(IsEnumContains(this->Style_, ExgMcChannelStyle::AnySeq))) {
      // AnySeq 在收到封包的第一時間, 就已觸發 this->DispatchMcMessage(e);
      // 所以在確定連續時, 不用再處理了!
   }
   else if (fon9_UNLIKELY(this->IsSnapshot())) {
      if (this->State_ == ExgMcChannelState::CanBeClosed) // 已收過一輪, 不用再收.
         return;
      if (static_cast<const ExgMcHead*>(pk)->MessageKind_ != 'C')
         return; // 不認識的快照訊息?!
      switch (static_cast<const ExgMcHead*>(pk)->TransmissionCode_) {
      case '2': // I084.Fut.
      case '5': // I084.Opt.
         break;
      default: // 不認識的快照訊息?!
         return;
      }
      SeqT lastRtSeq;
      switch (static_cast<const ExgMcI084*>(pk)->MessageType_) {
      case 'A':
         lastRtSeq = fon9::PackBcdTo<SeqT>(static_cast<const ExgMcI084*>(pk)->_A_RefreshBegin_.LastSeq_);
         if (this->IsSkipPkLog_) {
            // 檢查是否可以開始接收新的一輪: 判斷即時行情的 Pk1stSeq_ 是否符合要求?
            auto rtseq = this->ChannelMgr_->GetRealtimeChannel(*this).Pk1stSeq_;
            // if (lastRtSeq == 0)  // 即時行情開始之前的快照.
            // if (rtseq == 0)      // 尚未收到即時行情.
            if (rtseq > lastRtSeq)
               return;
            this->IsSkipPkLog_ = false;
            this->OnCycleStart(lastRtSeq);
         }
         else { // 還沒收一輪, 但又收到 'A', 表示前一輪接收失敗(至少有漏 'Z') => 重收.
            this->OnCycleStart(lastRtSeq);
            if (this->PkLog_) {
               auto res = this->PkLog_->GetFileSize();
               if (res.HasResult())
                  this->Pk1stPos_ = res.GetResult();
               this->Pk1stSeq_ = seq;
            }
         }
         break;
      case 'Z':
         if (this->IsSkipPkLog_) { // 尚未開始.
            this->AfterNextSeq_ = 0;
            return;
         }
         lastRtSeq = fon9::PackBcdTo<SeqT>(static_cast<const ExgMcI084*>(pk)->_Z_RefreshComplete_.LastSeq_);
         if (lastRtSeq == this->CycleStartSeq_) {
            auto rtseq = this->ChannelMgr_->GetRealtimeChannel(*this).Pk1stSeq_;
            // 檢查是否有遺漏? 有遺漏則重收.
            // 沒遺漏則觸發 OnSnapshotDone() 事件.
            if ((rtseq == 1 || rtseq <= lastRtSeq) && this->LostCount_ - this->CycleBeforeLostCount_ == 0) {
               this->State_ = ExgMcChannelState::CanBeClosed;
               if (this->ChannelMgr_->IsBasicInfoCycled(*this))
                  this->ChannelMgr_->OnSnapshotDone(*this, lastRtSeq);
            }
         }
         if (this->State_ != ExgMcChannelState::CanBeClosed) {
            // 收到 'Z' 之後, 若下一個循環的 lastRtSeq 沒變, 則 ChannelSeq 會回頭使用 'A' 的 ChannelSeq.
            // 所以在收到 'Z' 之後, 需要重設 NextSeq_; 因為在漏封包時, 必須可以重收下一次的循環.
            this->AfterNextSeq_ = 0;
         }
         break;
      default:
         if (this->IsSkipPkLog_) // 尚未開始.
            return;
         break;
      }
      goto __DISPATCH_AND_PkLog;
   }
   else {
   __DISPATCH_AND_PkLog:
      if (fon9_LIKELY(this->State_ != ExgMcChannelState::Waiting)) {
         ExgMcMessage e(*static_cast<const ExgMcHead*>(pk), pksz, *this, seq);
         this->DispatchMcMessage(e);
      }
   }
   this->LastInfoTime_ = static_cast<const ExgMcHead*>(pk)->InformationTime_.ToDayTime();
   this->PkLogAppend(pk, pksz, seq);
}
void ExgMcChannel::DispatchMcMessage(ExgMcMessage& e) {
   assert(this->PkPendings_.IsLocked());
   assert(e.Pk_.GetChannelId() == this->ChannelId_);
   this->ChannelMgr_->DispatchMcMessage(e);
   this->NotifyConsumersLocked(e);
}
void ExgMcChannel::NotifyConsumersLocked(ExgMcMessage& e) {
   assert(this->PkPendings_.IsLocked());
   if (this->IsSetupReloading_)
      return;
   struct Combiner {
      bool operator()(ExgMcMessageConsumer* subr, ExgMcMessage& e) {
         subr->OnExgMcMessage(e);
         return true;
      }
   } combiner;
   this->UnsafeConsumers_.Combine(combiner, e);
}
//--------------------------------------------------------------------------//
// 即時行情接收流程: (必須區分 Fut or Opt)
// 1. 準備階段:
//    - 接收 [Channel 3.4.基本資料] 完整的一輪後, 可關閉 Receiver.
//      - 收完一輪之後, 才能進行下一階段.
//      - 因為要取得正確的 DECIMAL-LOCATOR;
//    - 此階段 Channel 1,2; 13,14; 都要先暫存不處理, 但仍要 [過濾重複] & [回補遺漏].
//      - Channel 13,14; 需要從 MESSAGE-TYPE == 'A': Refresh Begin 開始儲存.
//        - LAST-SEQ: 此輪 Refresh 訊息，對應即時行情傳輸群組之訊息流水序號。
//        - 待快照更新對應到即時行情訊息之末筆處理序號, 已銜接暫存即時行情序號。
//        - 參考快照更新對應之末筆處理序號，[捨棄小於等於] 末筆處理序號之相關暫存即時行情。
//        - 參考快照更新對應之末筆處理序號，[處理大於] 末筆處理序號之相關暫存即時行情。
//      - 需同時判斷 Channel 1,2; 是否可以接續? 如果不行, 就等下一輪再開始儲存[Channel 13,14].
//      - 收完一輪 Channel 13,14 後, 就可以關閉 Receiver.
// 2. 根據快照(Channel 13,14)更新商品委託簿.
//    - 從[即時行情暫存檔]載入&回放, 通知 Handler & Consumer.
// 3. 持續更新 Channel 1,2 即時行情.
//    - 若有行情漏失, 且已不允許回補, 要如何處理呢? 重啟 Channel 13,14 快照更新?
//    - 目前暫不支援此情況, 若有需要, 則需增加一個 Channel Receiver 註冊機制,
//      當需要收特定 Channel 的訊息時, 透過該機制通知 Receiver 啟動接收.
// ----------------------------------------------------------------------------
// 1. [即時行情 Channel] 預設為 Blocking 狀態.
//    即使在 Blocking 狀態, [即時行情 Channel] 依然必須確保連續、補漏.
// 2. [快照更新 Channel] 必須知道 [即時行情 Channel],
//    因為必須由 [快照更新 Channel] 來決定:
//    - [即時行情 Channel] 是否可以進入發行狀態.
//    - [即時行情 Channel] 要從哪個序號開始發行.
ExgMcChannelMgr::ExgMcChannelMgr(ExgMdSymbsSP symbs, fon9::StrView sysName, fon9::StrView groupName, f9fmkt_TradingSessionId tsesId)
   : TradingSessionId_{tsesId}
   , Name_{sysName.ToString() + "_" + groupName.ToString()}
   , Symbs_{std::move(symbs)} {
   // 即時行情.
   this->Channels_[ 1].Ctor(this,  1, ExgMcChannelStyle::PkLog | ExgMcChannelStyle::WaitSnapshot);
   this->Channels_[ 2].Ctor(this,  2, ExgMcChannelStyle::PkLog | ExgMcChannelStyle::WaitSnapshot);
   // 基本資料.
   this->Channels_[ 3].Ctor(this,  3, ExgMcChannelStyle::BasicInfo);
   this->Channels_[ 4].Ctor(this,  4, ExgMcChannelStyle::BasicInfo);
   // 文字公告. 盤中可能會有新增文字公告?
   this->Channels_[ 5].Ctor(this,  5, ExgMcChannelStyle::AnySeq | ExgMcChannelStyle::PkLogChangeable);
   this->Channels_[ 6].Ctor(this,  6, ExgMcChannelStyle::AnySeq | ExgMcChannelStyle::PkLogChangeable);
   // 統計資訊.
   this->Channels_[ 7].Ctor(this,  7, ExgMcChannelStyle::AnySeq | ExgMcChannelStyle::PkLogChangeable);
   this->Channels_[ 8].Ctor(this,  8, ExgMcChannelStyle::AnySeq | ExgMcChannelStyle::PkLogChangeable);
   // 詢價資訊.
   this->Channels_[ 9].Ctor(this,  9, ExgMcChannelStyle::AnySeq | ExgMcChannelStyle::PkLogChangeable);
   this->Channels_[10].Ctor(this, 10, ExgMcChannelStyle::AnySeq | ExgMcChannelStyle::PkLogChangeable);
   // 現貨資訊
   this->Channels_[11].Ctor(this, 11, ExgMcChannelStyle::PkLogChangeable);
   this->Channels_[12].Ctor(this, 12, ExgMcChannelStyle::PkLogChangeable);
   // 快照更新.
   this->Channels_[13].Ctor(this, 13, ExgMcChannelStyle::Snapshot);
   this->Channels_[14].Ctor(this, 14, ExgMcChannelStyle::Snapshot);
}
ExgMcChannelMgr::~ExgMcChannelMgr() {
}
void ExgMcChannelMgr::StartupChannelMgr(std::string logPath) {
   fon9_LOG_INFO(this->Name_, ".StartupChannelMgr|path=", logPath);
   for (ExgMcChannel& channel : this->Channels_)
      channel.StartupChannel(logPath);
   for (ExgMcChannel& channel : this->Channels_)
      channel.SetupReload();
}
void ExgMcChannelMgr::ChannelCycled(ExgMcChannel& src) {
   fon9_LOG_INFO(this->Name_, ".ChannelCycled|ChannelId=", src.GetChannelId());
   auto iMkt = (src.GetChannelId() % 2);
   if (src.IsBasicInfo()) {
      for (ExgMcChannel& c : this->Channels_) {
         if (c.GetChannelId() % 2 == iMkt)
            c.OnBasicInfoCycled();
      }
      ExgMcChannel& ss = GetSnapshotChannel(src);
      if (ss.GetChannelState() == ExgMcChannelState::CanBeClosed)
         this->OnSnapshotDone(ss, ss.CycleStartSeq_);
   }
}
void ExgMcChannelMgr::OnSnapshotDone(ExgMcChannel& src, uint64_t lastRtSeq) {
   assert(src.IsSnapshot());
   fon9_LOG_INFO(this->Name_, ".SnapshotDone|ChannelId=", src.GetChannelId());
   auto iMkt = (src.GetChannelId() % 2);
   for (ExgMcChannel& c : this->Channels_) {
      if (c.GetChannelId() % 2 == iMkt)
         c.OnSnapshotDone(lastRtSeq);
   }
}

} // namespaces
