// \file f9twf/ExgMcChannel.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMcChannel_hpp__
#define __f9twf_ExgMcChannel_hpp__
#include "f9twf/ExgMdFmt.hpp"
#include "f9twf/ExgMrRecover.hpp"
#include "f9twf/ExgMdSymbs.hpp"
#include "fon9/TsAppend.hpp"
#include "fon9/Subr.hpp"
#include "fon9/PkCont.hpp"
#include <deque>

namespace f9twf {

class f9twf_API ExgMcChannel;

class ExgMrRecoverSession;
using ExgMrRecoverSessionSP = fon9::intrusive_ptr<ExgMrRecoverSession>;

//--------------------------------------------------------------------------//
struct ExgMcMessage {
   fon9_NON_COPY_NON_MOVE(ExgMcMessage);
   const ExgMcHead&  Pk_;
   const unsigned    PkSize_;
   char              Padding____[4];
   ExgMcChannel&     Channel_;
   uint64_t          SeqNo_;
   ExgMdSymb*        Symb_{nullptr};

   ExgMcMessage(const ExgMcHead& pk, unsigned pksz, ExgMcChannel& channel, uint64_t seq)
      : Pk_(pk)
      , PkSize_{pksz}
      , Channel_(channel)
      , SeqNo_(seq) {
      assert(seq == pk.GetChannelSeq());
   }
};

/// 處理 Channel 收到的訊息, 每個 Channel 可以有多個 Consumers;
/// - 在 ExgMcChannelMgr::DispatchMcMessage() 執行完畢後之後才會呼叫 Consumers.
/// - 不通知 SetupReload 的訊息.
struct f9twf_API ExgMcMessageConsumer {
   fon9_NON_COPY_NON_MOVE(ExgMcMessageConsumer);
   ExgMcMessageConsumer() = default;
   virtual ~ExgMcMessageConsumer();
   virtual void OnExgMcMessage(const ExgMcMessage& e) = 0;
};

enum class ExgMcChannelStyle : uint8_t {
   /// 不考慮序號是否連續, 一旦收到資料就通知.
   /// 例: Channel 3.4.基本資料;
   AnySeq = 0x01,
   /// 需要將封包記錄到 PkLog.
   PkLog = 0x02,
   /// 啟動時, 需要從 PkLog 載入.
   /// 例: Channel 3.4.基本資料;
   Reload = 0x04,
   /// 可改變 PkLog 的設定.
   PkLogChangeable = 0x08,

   /// 是否需要等候基本資料?
   /// - 在尚未收到基本資料之前, 依然會確保連續性、排除重複.
   /// - 通常會配合 PkLog, 在等候期間依然會寫入 PkLog, 但不會處理封包內容.
   /// - 等基本資料準備好之後, 從 PkLog 取出回放.
   /// - 例: 7.8.統計資訊; 13.14.快照更新;
   WaitBasic = 0x10,

   /// 等候快照更新(A:Refresh Begin .. Z:Refresh Complete 且沒有遺漏).
   /// - 通常用於: 1.2.即時行情;
   WaitSnapshot = 0x20,

   /// 當收完一個輪播循環後, 就可以關閉 Receiver.
   /// 輪播循環判斷方式:
   /// - Channel 13.14.快照更新(A:Refresh Begin .. Z:Refresh Complete 且沒有遺漏).
   /// - 由於: 盤後交易某些商品資訊必須待一般交易時段結束方可產生，
   ///   盤後交易時段各商品開始傳送 I010 的時間點將不同。
   ///   - 所以: Channel 3.4.基本資料 不適用此旗標.
   /// - 目前除了 Channel 13.14 之外, 沒有其他 Channel 需要此旗標.
   CycledClose = 0x40,
   /// 當收完一個輪播循環後, 觸發 ChannelCycled() 事件.
   /// 若沒有 CycledClose 旗標, 則不會進入 CanBeClosed 狀態,
   /// ChannelCycled() 只會觸發一次.
   /// - Channel 3.4.基本資料: 2個 Hb 之間, 沒有遺漏訊息.
   ///   - 也許應該找一個特定訊息(例:第1次收到的I010, 等下次收到相同內容時, 視為一個循環).
   ///   - 盤後交易某些商品資訊必須待一般交易時段結束方可產生，
   ///     盤後交易時段各商品開始傳送 I010 的時間點將不同。
   ///     => 所以不能用單純使用 Hb 來判斷是否 Cycled.
   CycledNotify = 0x80,

   /// Channel 3.4.基本資料.
   BasicInfo = AnySeq | Reload | PkLog | CycledNotify,
   /// Channel 13.14.快照更新.
   Snapshot = PkLog | CycledClose | WaitBasic,
};
fon9_ENABLE_ENUM_BITWISE_OP(ExgMcChannelStyle);

enum class ExgMcChannelState : uint8_t {
   /// Channel 仍持續執行接收任務.
   Running,
   /// - Waiting BasicInfo.
   /// - Waiting Snapshot.
   Waiting,
   /// 已完成輪播循環.
   /// 但因為不是 ExgMcChannelStyle::CycledClose, 所以不進入 CanBeClosed 狀態.
   Cycled,
   /// Channel Receiver 可以關閉, 例:
   /// - [Channel 3,4 基本資料] 已收了一輪, 且已更新 Symbols.
   /// - [Channel 13,14 快照更新] 已收了一輪, 且可與 [Channel 1,2 即時行情] 銜接.
   CanBeClosed,
};

//--------------------------------------------------------------------------//
/// 台灣期交所逐筆行情: 負責單一 Channel 資料的連續、過濾重複、處理回補.
/// 並不是所有的 Channel 都允許回補, 也不一定需要確保連續, 例如:
/// - Channel 1,2 即時行情: 需要確保連續、過濾重複、處理回補.
/// - Channel 3,4 基本資料: 收到封包直接處理, 不用考慮「連續、重複、回補」的問題.
class f9twf_API ExgMcChannel : private fon9::PkContFeeder {
   fon9_NON_COPY_NON_MOVE(ExgMcChannel);
   using base = fon9::PkContFeeder;
   friend class f9twf_API ExgMcChannelMgr;

   /// 封包消費者, 例: McToMiConv.
   /// 在通知 Consumer 之前, 應先進行 DispatchMcMessage();
   fon9::UnsafeSubject<ExgMcMessageConsumer*> UnsafeConsumers_{8};

   using RecoversImpl = std::deque<ExgMrRecoverSessionSP>;
   using Recovers = fon9::MustLock<RecoversImpl>;
   Recovers Recovers_;
          
   /// 記錄收到的封包.
   /// - 如果是輪播 channel, 則在系統備妥後, 會載入 PkLog_ 用來更新 Symbols 及其他系統狀態.
   fon9::AsyncFileAppenderSP  PkLog_;
   /// 記錄 [PkLog 開啟後] 收到的 [第一筆封包] 的檔案位置.
   fon9::File::SizeType Pk1stPos_;
   /// 記錄 [PkLog 開啟後] 收到的 [第一筆封包] 的序號.
   /// 如果是 Snapshot 則會在收到 [A:Refresh Begin] 時視為 [第一筆封包].
   SeqT                 Pk1stSeq_;
   fon9::DayTime        LastInfoTime_;

   ExgMcChannelMgr*  ChannelMgr_{};
   ExgMrChannelId_t  ChannelId_{};
   ExgMcChannelStyle Style_{};
   ExgMcChannelState State_{ExgMcChannelState::Running};
   /// 暫停將收到的 pk 寫入 this->PkLog_;
   /// - 在 SetupReload() 時;
   /// - 在 [快照更新尚未收到 A:Refresh Begin] 之前.
   bool  IsSkipPkLog_{false};
   bool  IsSetupReloading_{false};
   char  Padding___[2];
   /// 使用 CycleStartSeq_ & CycleBeforeLostCount_ 判斷: 兩個 Hb 之間是否有遺漏.
   /// 如果沒有遺漏, 則視為收了一次完整的輪播, 此時可能會進入 ExgMcChannelState::CanBeClosed 狀態.
   SeqT  CycleStartSeq_{0};
   SeqT  CycleBeforeLostCount_{0};
   void OnCycleStart(SeqT seq) {
      this->CycleStartSeq_ = seq;
      this->CycleBeforeLostCount_ = this->LostCount_;
   }

   // 2 次 Hb 之間, 若沒有任何訊息, 則可能發生斷線.
   SeqT  ReceivedCountInHb_{0};
   static void EmitHbTimer(fon9::TimerEntry* timer, fon9::TimeStamp now);
   fon9::DataMemberEmitOnTimer<&ExgMcChannel::EmitHbTimer> HbTimer_;

   void DispatchMcMessage(ExgMcMessage& e);
   void NotifyConsumersLocked(ExgMcMessage& e);
   void PkContOnReceived(const void* pk, unsigned pksz, SeqT seq) override;
   void PkContOnTimer(PkPendings::Locker&& pks) override;
   void PkLogAppend(const void* pk, unsigned pksz, SeqT seq) {
      if (!this->IsSkipPkLog_ && this->PkLog_) {
         if (this->Pk1stSeq_ == 0)
            this->Pk1stSeq_ = seq;
         fon9::TsAppend(*this->PkLog_, fon9::UtcNow(), pk, static_cast<uint16_t>(pksz));
         static_assert(kExgMdMaxPacketSize < 0xfff0, "ExgMdHeadVerLen::BodyLength_ cannot use uint16_t.");
      }
   }

   void Ctor(ExgMcChannelMgr* mgr, ExgMrChannelId_t channelId, ExgMcChannelStyle style) {
      assert(this->ChannelMgr_ == nullptr && this->ChannelId_ == 0);
      this->ChannelMgr_ = mgr;
      this->ChannelId_ = channelId;
      this->Style_ = style;
   }
   void StartupChannel(std::string logPath);
   void SetupReload();
   void OnBasicInfoCycled();
   void OnSnapshotDone(SeqT lastRtSeq);
   void ReloadDispatch(SeqT fromSeq);

public:
   ExgMcChannel() = default;
   ~ExgMcChannel();
   ExgMcChannelMgr* GetChannelMgr() const {
      return this->ChannelMgr_;
   }
   ExgMrChannelId_t GetChannelId() const {
      return this->ChannelId_;
   }
   bool IsBasicInfo() const {
      return this->ChannelId_ == 3 || this->ChannelId_ == 4;
   }
   bool IsSnapshot() const {
      return this->ChannelId_ == 13 || this->ChannelId_ == 14;
   }
   bool IsRealtime() const {
      return this->ChannelId_ == 1 || this->ChannelId_ == 2;
   }
   bool SetPkLog(bool isEnabled) {
      if (!IsEnumContains(this->Style_, ExgMcChannelStyle::PkLogChangeable))
         return false;
      if (isEnabled)
         this->Style_ |= ExgMcChannelStyle::PkLog;
      else
         this->Style_ -= ExgMcChannelStyle::PkLog;
      return true;
   }

   /// 返回 channel 狀態, 讓 McReceiver 決定是否需要開啟接收程序.
   /// - 部分 channel 的資料使用輪播方式不斷重複, 所以只要收過一輪,
   ///   當盤就可以不用再收, 可降低頻寬負擔.
   ExgMcChannelState GetChannelState() const {
      return this->State_;
   }
   ExgMcChannelState OnPkReceived(const ExgMcHead& pk, unsigned pksz);

   bool IsNeedsNotifyConsumer() const {
      return (!this->IsSetupReloading_ && !this->UnsafeConsumers_.IsEmpty());
   }
   /// - 在 this->ChannelMgr_->DispatchMcMessage(e); 之後, 將收到的封包轉發給訂閱者(例: McToMiConv).
   /// - 可對於部分手動建立的訊息(例: 收到 I084._O_ 轉成 I083), 透過此處轉發給訂閱者.
   void NotifyConsumers(ExgMcMessage& e) {
      auto locker = this->PkPendings_.Lock();
      this->NotifyConsumersLocked(e);
   }
   void SubscribeConsumer(fon9::SubConn* conn, ExgMcMessageConsumer& h) {
      auto locker = this->PkPendings_.Lock();
      this->UnsafeConsumers_.Subscribe(conn, &h);
   }
   void SubscribeConsumer(ExgMcMessageConsumer& h) {
      auto locker = this->PkPendings_.Lock();
      this->UnsafeConsumers_.Subscribe(&h);
   }
   void UnsubscribeConsumer(fon9::SubConn* h) {
      auto locker = this->PkPendings_.Lock();
      this->UnsafeConsumers_.Unsubscribe(h);
   }
   void UnsubscribeConsumer(ExgMcMessageConsumer& h) {
      auto locker = this->PkPendings_.Lock();
      this->UnsafeConsumers_.UnsubscribeAll(&h);
   }

   void AddRecover(ExgMrRecoverSessionSP);
   void DelRecover(ExgMrRecoverSessionSP);
   void OnRecoverErr(SeqT beginSeqNo, unsigned recoverNum, ExgMrStatusCode st);
   void OnRecoverEnd(ExgMrRecoverSession* svr);
};

//--------------------------------------------------------------------------//
/// 台灣期交所逐筆行情 Channel 管理員.
class f9twf_API ExgMcChannelMgr : public fon9::intrusive_ref_counter<ExgMcChannelMgr> {
   fon9_NON_COPY_NON_MOVE(ExgMcChannelMgr);
   char           Padding____[3];
public:
   const f9fmkt_TradingSessionId TradingSessionId_;
   /// = sysName + "_" + groupName;
   const std::string    Name_;
   const ExgMdSymbsSP   Symbs_;

   /// 預設沒有任何的 McMessageParser, 您應該在建構之後, 透過 RegMcMessageParser() 註冊需要解析的封包格式.
   ExgMcChannelMgr(ExgMdSymbsSP symbs, fon9::StrView sysName, fon9::StrView groupName, f9fmkt_TradingSessionId tsesId);
   ~ExgMcChannelMgr();

   using FnMcMessageParser = void(*)(ExgMcMessage&);
   /// 應在 ExgMcChannelMgr 建構後, 使用前, 立即註冊訊息處理者.
   /// - McMessageParser 應由 Plugins 處理? 因為不同系統要處理的事項不同, 例:
   ///   - 行情發送系統: 解析行情時,同時編製要發行(儲存)的包封.
   ///   - 快速刪單系統: 解析行情後,直接觸發內部事件.
   void RegMcMessageParser(char tx, char mg, uint8_t ver, FnMcMessageParser parser) {
      this->McDispatcher_.Reg(tx, mg, ver, parser);
   }

   /// 在系統啟動時, 換日時... 會重新啟動 ChannelMgr;
   void StartupChannelMgr(std::string logPath);

   enum {
      kChannelCount = 20
   };

   ExgMcChannel* GetChannel(ExgMrChannelId_t channelId) {
      assert(channelId < kChannelCount);
      return channelId < kChannelCount ? &this->Channels_[channelId] : nullptr;
   }
   ExgMcChannel& GetRealtimeChannel(const ExgMcChannel& channel) {
      return this->Channels_[2 - (channel.GetChannelId() % 2)];
   }
   ExgMcChannel& GetSnapshotChannel(const ExgMcChannel& channel) {
      return this->Channels_[14 - (channel.GetChannelId() % 2)];
   }
   bool IsSnapshotDone(const ExgMcChannel& channel) {
      return GetSnapshotChannel(channel).GetChannelState() == ExgMcChannelState::CanBeClosed;
   }
   bool IsBasicInfoCycled(const ExgMcChannel& channel) {
      return this->Channels_[4 - (channel.GetChannelId() % 2)]
         .GetChannelState() >= ExgMcChannelState::Cycled;
   }

   /// 根據 MessageId(TransmissionCode & MessageKind) 分派給之前註冊好的 parser 處理.
   /// 若與 e.Pk_ 與 symbol 有關, 則會在返回前設定 e.Symb_; 然後 channel 再將 e 送給 Consumers.
   void DispatchMcMessage(ExgMcMessage& e) {
      if (auto fnParser = this->McDispatcher_.Get(e.Pk_))
         fnParser(e);
   }
   ExgMcChannelState OnPkReceived(const ExgMcHead& pk, unsigned pksz) {
      if (auto* channel = this->GetChannel(pk.GetChannelId()))
         return channel->OnPkReceived(pk, pksz);
      return ExgMcChannelState::Running;
   }

   /// channel 已收完一次輪播, 檢查是否允許進入下一階段, 例:
   /// - 基本資料.
   /// - 快照更新 A:Refresh Begin .. Z:Refresh Complete;
   /// - 以上 2 者都完成後, 才可播放(載入&即時)即時行情.
   void ChannelCycled(ExgMcChannel& src);
   void OnSnapshotDone(ExgMcChannel& src, uint64_t lastRtSeq);

   bool CheckSymbTradingSessionId(ExgMdSymb& symb) {
      return symb.CheckSetTradingSessionId(*this->Symbs_, this->TradingSessionId_);
   }

private:
   using McDispatcher = ExgMdMessageDispatcher<FnMcMessageParser>;
   McDispatcher   McDispatcher_;
   ExgMcChannel   Channels_[kChannelCount];
};

} // namespaces
#endif//__f9twf_ExgMcChannel_hpp__
