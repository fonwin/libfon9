// \file fon9/fmkt/MdRtStream.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/MdRtStream.hpp"
#include "fon9/fmkt/MdSymbs.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace fmkt {

MdRtStream::~MdRtStream() {
}
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
   // args = "MdRts:F:F,RecoverTime"
   // 1st F(hex) is MdRtsKind MdRtSubr.RtFilter_;
   // 2nd F(hex) is MdRtsKind MdRtRecover.Filter_;
   const StrView decoderName = StrFetchTrim(args, ':');
   if (decoderName != "MdRts")
      return seed::OpResult::bad_subscribe_stream_args;

   MdRtSubrSP  psub{new MdRtSubr{std::move(subr), &args}};
   // 必須先加入訂閱, 因為 psub(SubscribeStreamOK{}) 呼叫時, 可能會取消訂閱!
   // 如果在 psub(SubscribeStreamOK{}) 之後才加入訂閱, 這樣的取消就無效了!
   this->UnsafeSubj_.Subscribe(pSubConn, psub);

   // 訂閱成功的通知.
   struct SubscribeStreamOK : public seed::SeedNotifySubscribeOK {
      fon9_NON_COPY_NON_MOVE(SubscribeStreamOK);
      using base = SeedNotifySubscribeOK;
      SymbPodOp& PodOp_;
      SubscribeStreamOK(SymbPodOp& op, seed::Tab& tabRt)
         : base(*op.Sender_, &tabRt, op.KeyText_, nullptr, seed::SeedNotifyKind::SubscribeStreamOK)
         , PodOp_(op) {
         this->StreamDataKind_ = f9sv_MdRtsKind_All;
      }
      BufferList GetGridViewBuffer(const std::string** ppGvStr) const override {
         if (ppGvStr)
            *ppGvStr = nullptr;
         RevBufferList rbuf{256};
         SymbCellsToBitv(rbuf, *this->Tree_.LayoutSP_, *this->PodOp_.Symb_);
         return rbuf.MoveOut();
      }
      void MakeGridView() const override {
         this->CacheGV_ = BufferTo<std::string>(this->GetGridViewBuffer(nullptr));
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
//--------------------------------------------------------------------------//
void MdRtStream::OnSymbDailyClear(SymbTree& tree, const Symb& symb) {
   (void)tree; assert(&tree == &this->InnMgr_.MdSymbs_);
   this->OnSessionChanged(symb);
}
void MdRtStream::OnSymbSessionClear(SymbTree& tree, const Symb& symb) {
   (void)tree; assert(&tree == &this->InnMgr_.MdSymbs_);
   this->OnSessionChanged(symb);
}
void MdRtStream::OnSessionChanged(const Symb& symb) {
   const void*   prevStream = this->RtStorage_.GetStream().get();
   RevBufferList rts{64};
   CharVector    pk;
   *rts.AllocPacket<uint8_t>() = symb.TradingSessionSt_;
   *rts.AllocPacket<char>() = symb.TradingSessionId_;
   PutBigEndian(rts.AllocPacket<uint32_t>(), symb.TDayYYYYMMDD_);
   BufferAppendTo(rts.cfront(), pk);
   this->Publish(ToStrView(symb.SymbId_), f9sv_RtsPackType_TradingSessionId, DayTime::Null(), std::move(rts));
   // 開啟 Storage, 如果 Storage 有變, 則將 SessionSt 寫入新開啟的 Storage.
   // 這樣在從頭回補時, 才能補到 TDay 及盤別狀態.
   this->InnMgr_.RtOpen(this->RtStorage_, symb);
   if (prevStream != this->RtStorage_.GetStream().get()) {
      this->RtStorageSize_ = this->RtStorage_.Size();
      RevPutMem(rts, pk.begin(), pk.size());
      RevPutBitv(rts, fon9_BitvV_NumberNull); // InfoTime = Null.
      *rts.AllocPacket<uint8_t>() = cast_to_underlying(f9sv_RtsPackType_TradingSessionId);
      this->Save(std::move(rts));
   }
}
void MdRtStream::BeforeRemove(SymbTree& tree, Symb& symb) {
   seed::SeedNotifyArgs e(tree, nullptr, ToStrView(symb.SymbId_), nullptr, seed::SeedNotifyKind::PodRemoved);
   this->UnsafeSubj_.Publish(e);
   this->InnMgr_.MdSymbs_.UnsafePublish(f9sv_RtsPackType_Count, e);
}
// -----
void MdRtStream::PublishUpdateBS(const StrView& keyText, SymbBSData& symbBS, RevBufferList&& rts) {
   // 每秒儲存一次 SnapshotBS, 回補時才能正確處理 UpdateBS;
   this->Publish(keyText, f9sv_RtsPackType_UpdateBS, symbBS.InfoTime_, std::move(rts));
   const auto bstm = static_cast<uint32_t>(symbBS.InfoTime_.GetIntPart());
   if (this->LastTimeSnapshotBS_ != bstm) {
      this->LastTimeSnapshotBS_ = bstm;
      rts.MoveOut();
      symbBS.Flags_ |= f9sv_BSFlag_OrderBuy | f9sv_BSFlag_OrderSell | f9sv_BSFlag_DerivedBuy | f9sv_BSFlag_DerivedSell;
      MdRtsPackSnapshotBS(rts, symbBS);
      ToBitv(rts, symbBS.InfoTime_);
      *rts.AllocPacket<uint8_t>() = cast_to_underlying(IsEnumContains(symbBS.Flags_, f9sv_BSFlag_Calculated)
                                                       ? f9sv_RtsPackType_CalculatedBS
                                                       : f9sv_RtsPackType_SnapshotBS);
   }
   this->Save(std::move(rts));
}
void MdRtStream::Publish(const StrView& keyText, f9sv_RtsPackType pkType, const DayTime infoTime, RevBufferList&& rts) {
   const auto rtsKind = GetMdRtsKind(pkType);
   assert(!IsEnumContains(rtsKind, f9sv_MdRtsKind_NoInfoTime));
   if ((this->InfoTimeKind_ == rtsKind && this->InfoTime_ == infoTime) || infoTime.IsNull())
      RevPutBitv(rts, fon9_BitvV_NumberNull);
   else {
      // 訂閱者可用 RtsKind 過濾所需要的封包種類,
      // 所以如果 RtsKind 改變了, 仍需要將 InfoTime 送出.
      this->InfoTimeKind_ = rtsKind;
      this->InfoTime_ = infoTime;
      ToBitv(rts, infoTime);
   }
   *rts.AllocPacket<uint8_t>() = cast_to_underlying(pkType);
   // -----
   MdRtsNotifyArgs e{this->InnMgr_.MdSymbs_, keyText, rtsKind, rts};
   this->UnsafeSubj_.Publish(e);
   this->InnMgr_.MdSymbs_.UnsafePublish(pkType, e);
   // -----
   if (IsEnumContains(rtsKind, f9sv_MdRtsKind_BS)) {
      fon9_WARN_DISABLE_SWITCH;
      switch (pkType) {
      case f9sv_RtsPackType_UpdateBS:
         // 由 this->PublishUpdateBS() 決定如何儲存.
         return;
      case f9sv_RtsPackType_SnapshotBS:
      case f9sv_RtsPackType_CalculatedBS:
         this->LastTimeSnapshotBS_ = static_cast<uint32_t>(infoTime.GetIntPart());
         break;
      }
      fon9_WARN_POP;
   }
   this->Save(std::move(rts));
}
void MdRtStream::PublishAndSave(const StrView& keyText, f9sv_RtsPackType pkType, RevBufferList&& rts) {
   *rts.AllocPacket<uint8_t>() = cast_to_underlying(pkType);

   MdRtsNotifyArgs e(this->InnMgr_.MdSymbs_, keyText, GetMdRtsKind(pkType), rts);
   this->UnsafeSubj_.Publish(e);
   this->InnMgr_.MdSymbs_.UnsafePublish(pkType, e);
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
//--------------------------------------------------------------------------//
void MdRtsNotifyArgs::MakeGridView() const {
   this->CacheGV_ = BufferTo<std::string>(this->RtsForGvStr_);
}

} } // namespaces
