// \file fon9/fmkt/MdRtStream.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/MdRtStream.hpp"
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace fmkt {

seed::Fields MdRtStream::MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(MdRtStream, InfoTime_, "InfoTime"));
   return flds;
}
//--------------------------------------------------------------------------//
seed::OpResult MdRtStream::SubscribeStream(SubConn*           pSubConn,
                                           seed::Tab&         tabRt,
                                           SymbPodOp&         op,
                                           StrView            args,
                                           seed::FnSeedSubr&& subr) {
   StrView decoderName = StrFetchTrim(args, ':');
   if (decoderName != "MdRts")
      return seed::OpResult::bad_subscribe_stream_args;

   MdRtSubrSP  psub{new MdRtSubr{std::move(subr)}};

   // args 分成 2 個主要部分,使用 ':' 分隔「即時:回補」
   // - 即時
   //   - MdRtsKind(hex): empty() or 0 表示訂閱全部的即時訊息.
   //   - 訂閱BS快照(+Interval?)? 
   psub->RtFilter_ = StrToMdRtsKind(&args);

   // 必須先加入訂閱, 因為 psub(SubscribeStreamOK{}) 呼叫時, 可能會取消訂閱!
   // 如果在 psub(SubscribeStreamOK{}) 之後才加入訂閱, 這樣的取消就無效了!
   this->Subj_.Subscribe(pSubConn, psub);

   // 訂閱成功的通知.
   struct SubscribeStreamOK : public seed::SeedNotifyArgs {
      fon9_NON_COPY_NON_MOVE(SubscribeStreamOK);
      using base = SeedNotifyArgs;
      SymbPodOp& PodOp_;
      SubscribeStreamOK(SymbPodOp& op, seed::Tab& tabRt)
         : base(*op.Sender_, &tabRt, op.KeyText_, nullptr, seed::SeedNotifyKind::SubscribeStreamOK)
         , PodOp_(op) {
      }
      void MakeGridView() const override {
         RevBufferList rbuf{256};
         size_t tabidx = this->Tree_.LayoutSP_->GetTabCount();
         while (tabidx > 0) {
            auto* tab = this->Tree_.LayoutSP_->GetTab(--tabidx);
            this->PodOp_.BeginRead(*tab, [&rbuf](const seed::SeedOpResult& res, const seed::RawRd* rd) {
               if (rd == nullptr)
                  return;
               auto fldidx = res.Tab_->Fields_.size();
               while (fldidx > 0) {
                  auto* fld = res.Tab_->Fields_.Get(--fldidx);
                  fld->CellToBitv(*rd, rbuf);
               }
               ToBitv(rbuf, unsigned_cast(res.Tab_->GetIndex()));
            });
         }
         this->CacheGV_ = BufferTo<std::string>(rbuf.MoveOut());
      }
   };
   psub(SubscribeStreamOK{op, tabRt});

   // args.empty(): 沒有額外參數: 僅訂閱即時訊息, 沒有回補.
   if (StrTrimHead(&args).Get1st() == ':')
      StrTrimHead(&args, args.begin() + 1);
   if (args.empty() || psub->IsUnsubscribed())
      return seed::OpResult::no_error;

   // -----
   // - 非當日的回補(指定日期回補), 應另建一個 Tab(例:MdHistory) 來處理:
   //   - 僅回補歷史, 則 SubscribeStreamOK 應回啥呢? (empty?)
   //   - 僅回補歷史, 回補完畢後自動取消訂閱, 並回 StreamEnd.
   // -----
   // - 回補: MdRtsKind(hex),DayTime(開始時間,包含)
   //   - 在另一 Thread 回補, 回補與即時可能會交錯送出.
   //   - 送回補訊息前:
   //     - 鎖定 tree;
   //     - 檢查是否已取消訂閱: psub->IsUnsubscribed();
   MdRtRecoverSP recover = MdRtRecover::Make(this->InnMgr_, psub, this->RtStorage_.GetStream());
   if (!recover) {
      // TODO: Stream Recover 失敗訊息? (沒有開啟 reader, 無法回補.)
      seed::SeedNotifyStreamRecoverArgs   e(*op.Sender_, nullptr/*tab*/,
                                            op.KeyText_, nullptr/*rd*/,
                                            seed::SeedNotifyKind::StreamRecoverEnd);
      psub(e);
      return seed::OpResult::no_error;
   }
   recover->EndPos_ = this->RtStorageSize_;
   recover->Filter_ = StrToMdRtsKind(&args);
   if (StrTrimHead(&args).Get1st() == ',')
      StrTrimHead(&args, args.begin() + 1);
   recover->StartInfoTime_ = StrTo(&args, DayTime::Null());
   recover->RunAfter(TimeInterval{});
   return seed::OpResult::no_error;
}
seed::OpResult MdRtStream::UnsubscribeStream(SubConn subConn) {
   MdRtSubrSP psub;
   if (!this->Subj_.MoveOutSubscriber(subConn, psub))
      return seed::OpResult::no_subscribe;
   assert(psub.get() != nullptr && psub->IsUnsubscribed() == false);
   psub->SetUnsubscribed(); // 告知回補程序: 已經取消訂閱, 不用再處理回補了!
   return seed::OpResult::no_error;
}
//--------------------------------------------------------------------------//
void MdRtStream::SessionClear(SymbTree& tree, Symb& symb) {
   const void*   prevStream = this->RtStorage_.GetStream().get();
   RevBufferList rts{64};
   CharVector    pk;
   *rts.AllocPacket<uint8_t>() = symb.TradingSessionSt_;
   *rts.AllocPacket<char>() = symb.TradingSessionId_;
   PutBigEndian(rts.AllocPacket<uint32_t>(), symb.TDayYYYYMMDD_);
   BufferAppendTo(rts.cfront(), pk);
   this->Publish(tree, ToStrView(symb.SymbId_), RtsPackType::TradingSessionSt, DayTime::Null(), std::move(rts));
   // 開啟 Storage, 如果 Storage 有變, 則將 SessionSt 寫入新開啟的 Storage.
   // 這樣在從頭回補時, 才能補到 TDay 及盤別狀態.
   this->InnMgr_.RtOpen(this->RtStorage_, symb);
   if (prevStream != this->RtStorage_.GetStream().get()) {
      this->RtStorageSize_ = this->RtStorage_.Size();
      RevPutMem(rts, pk.begin(), pk.size());
      RevPutBitv(rts, fon9_BitvV_NumberNull); // InfoTime = Null.
      *rts.AllocPacket<uint8_t>() = cast_to_underlying(RtsPackType::TradingSessionSt);
      this->Save(std::move(rts));
   }
}
void MdRtStream::BeforeRemove(SymbTree& tree, Symb& symb) {
   seed::SeedNotifyArgs e(tree, nullptr, ToStrView(symb.SymbId_), nullptr, seed::SeedNotifyKind::PodRemoved);
   this->Subj_.Publish(e);
}
// -----
void MdRtStream::PublishUpdateBS(seed::Tree& tree, const StrView& keyText,
                                 SymbBS& symbBS, RevBufferList&& rts) {
   // 每秒儲存一次 SnapshotBS, 回補時才能正確處理 UpdateBS;
   this->Publish(tree, keyText, RtsPackType::UpdateBS, symbBS.Data_.InfoTime_, std::move(rts));
   const auto bstm = static_cast<uint32_t>(symbBS.Data_.InfoTime_.GetIntPart());
   if (this->LastTimeSnapshotBS_ != bstm) {
      this->LastTimeSnapshotBS_ = bstm;
      rts.MoveOut();
      symbBS.Data_.Flags_ |= BSFlag::OrderBuy | BSFlag::OrderSell | BSFlag::DerivedBuy | BSFlag::DerivedSell;
      MdRtsPackSnapshotBS(rts, symbBS);
      ToBitv(rts, symbBS.Data_.InfoTime_);
      *rts.AllocPacket<uint8_t>() = cast_to_underlying(IsEnumContains(symbBS.Data_.Flags_, BSFlag::Calculated)
                                                       ? RtsPackType::CalculatedBS
                                                       : RtsPackType::SnapshotBS);
   }
   this->Save(std::move(rts));
}
void MdRtStream::Publish(seed::Tree& tree, const StrView& keyText,
                         RtsPackType pkType, DayTime infoTime, RevBufferList&& rts) {
   const auto rtsKind = GetMdRtsKind(pkType);
   if (this->InfoTimeKind_ == rtsKind && this->InfoTime_ == infoTime)
      RevPutBitv(rts, fon9_BitvV_NumberNull);
   else {
      // 訂閱者可用 RtsKind 過濾所需要的封包種類,
      // 所以如果 RtsKind 改變了, 仍需要將 InfoTime 送出.
      this->InfoTimeKind_ = rtsKind;
      this->InfoTime_ = infoTime;
      ToBitv(rts, infoTime);
   }
   *rts.AllocPacket<uint8_t>() = cast_to_underlying(pkType);

   struct NotifyArgs : public seed::SeedNotifyArgs {
      fon9_NON_COPY_NON_MOVE(NotifyArgs);
      using base = seed::SeedNotifyArgs;
      using base::base;

      const BufferNode* Rts_;

      void MakeGridView() const override {
         this->CacheGV_ = BufferTo<std::string>(this->Rts_);
      }
   };
   NotifyArgs e{tree, nullptr/*tab*/, keyText, rtsKind};
   e.Rts_ = rts.cfront();
   this->Subj_.Publish(e);

   if (IsEnumContains(rtsKind, MdRtsKind::BS)) {
      fon9_WARN_DISABLE_SWITCH;
      switch (pkType) {
      case RtsPackType::UpdateBS:
         // 由 this->PublishUpdateBS() 決定如何儲存.
         return;
      case RtsPackType::SnapshotBS:
      case RtsPackType::CalculatedBS:
         this->LastTimeSnapshotBS_ = static_cast<uint32_t>(infoTime.GetIntPart());
         break;
      }
      fon9_WARN_POP;
   }
   this->Save(std::move(rts));
}
// -----
void MdRtStream::Save(RevBufferList&& rts) {
   assert(rts.cfront() != nullptr);
   if (this->RtStorage_.IsReady()) {
      ByteArraySizeToBitvT(rts, CalcDataSize(rts.cfront()));
      // 在封包前端放 4 bytes(uint32_t, big endian) = 實際的位置, 當成格式檢查碼.
      // 當檔案有異常時, 可以用此找到下一個正確的位置.
      PutBigEndian(rts.AllocPacket<uint32_t>(), static_cast<uint32_t>(this->RtStorageSize_));
      this->RtStorageSize_ += CalcDataSize(rts.cfront());
      this->RtStorage_.AppendBuffered(rts.MoveOut());
   }
   else {
      rts.MoveOut();
   }
}

} } // namespaces
