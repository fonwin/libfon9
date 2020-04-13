// \file fon9/fmkt/MdSymbs.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/MdSymbs.hpp"
#include "fon9/FileReadAll.hpp"
#include "fon9/Log.hpp"
#include "fon9/BitvDecode.hpp"
#include "fon9/seed/RawWr.hpp"

namespace fon9 { namespace fmkt {

void fon9_API SymbCellsToBitv(RevBuffer& rbuf, seed::Layout& layout, Symb& symb) {
   size_t tabidx = layout.GetTabCount();
   while (tabidx > 0) {
      auto* tab = layout.GetTab(--tabidx);
      seed::SimpleRawRd rd{*symb.GetSymbData(static_cast<int>(tabidx))};
      auto fldidx = tab->Fields_.size();
      while (fldidx > 0) {
         auto* fld = tab->Fields_.Get(--fldidx);
         fld->CellToBitv(rd, rbuf);
      }
      ToBitv(rbuf, unsigned_cast(tab->GetIndex()));
   }
}
//--------------------------------------------------------------------------//
struct MdSymbsBase::Recover : public TimerEntry {
   fon9_NON_COPY_NON_MOVE(Recover);
   MdSymbsBase&      MdSymbs_;
   const SymbsSubrSP Subr_;
   Recover(MdSymbsBase& mdSymbs, SymbsSubrSP subr)
      : TimerEntry{GetDefaultTimerThread()}
      , MdSymbs_(mdSymbs)
      , Subr_{std::move(subr)} {
   }
   ~Recover() {
   }
   void OnTimer(TimeStamp now) override {
      (void)now;
      if (this->Subr_->IsUnsubscribed())
         return;
      struct NotifyArgs : public seed::SeedNotifyStreamRecoverArgs {
         fon9_NON_COPY_NON_MOVE(NotifyArgs);
         mutable RevBufferList AckBuf_{1024*2};
         using base = seed::SeedNotifyStreamRecoverArgs;
         NotifyArgs(MdSymbsBase& mdSymbs)
            : base(mdSymbs, nullptr/*tab*/,
                   // key: 因為用 RtsPackType::SnapshotSymbList 打包, 所以這裡為 don't care.
                   // 但在 rc 送出時, 因為是 IsSubrTree() 所以會將這裡的 key 傳給 rc.client,
                   // 可參閱: fon9/rc/RcSeedVisitorServer.cpp,
                   // 為了減少資料量, 所以用 TextBegin(), 這樣就只需 1 byte 來打包 key.
                   seed::TextBegin()/*key*/,
                   nullptr/*rd*/,
                   seed::SeedNotifyKind::StreamRecover) {
         }
         BufferList GetGridViewBuffer(const std::string** gvStr) const override {
            if (gvStr)
               *gvStr = nullptr;
            return this->AckBuf_.MoveOut();
         }
      };
      NotifyArgs  nargs(this->MdSymbs_);
      auto        symbsLk = this->MdSymbs_.SymbMap_.ConstLock();
      auto&       symbsRe = this->Subr_->SymbsRecovering_;
      SymbSP      keeps[100];
      unsigned    keepi = 0;
      while (!symbsRe.empty()) {
         Symb& symb = *symbsRe.begin()->second;
         keeps[keepi++].reset(&symb);
         symbsRe.erase(symbsRe.begin());
         SymbCellsToBitv(nargs.AckBuf_, *this->MdSymbs_.LayoutSP_, symb);
         ToBitv(nargs.AckBuf_, symb.SymbId_);
         const auto* cfront = nargs.AckBuf_.cfront();
         if (cfront->GetFrontSpaces() < 256 || cfront->GetNext())
            break;
         if (keepi >= numofele(keeps))
            break;
      }
      *nargs.AckBuf_.AllocPacket<uint8_t>() = cast_to_underlying(RtsPackType::SnapshotSymbList);
      // SeedNotifyKind::StreamRecover, 在 RtsPackType 之前, 必須加上長度:
      // SeedNotifyKind::StreamRecover + [RtsLength + RtsPackType  + RtsPackData] * N
      // SeedNotifyKind::StreamRecover + [RtsLength + SnapshotSymb + (Snapshot * N)] * 1
      ByteArraySizeToBitvT(nargs.AckBuf_, CalcDataSize(nargs.AckBuf_.cfront()));
      // ----- 通知訂閱者.
      if (symbsRe.empty())
         nargs.NotifyKind_ = seed::SeedNotifyKind::StreamRecoverEnd;
      this->Subr_(nargs);
      // ----- 檢查流量.
      if (fon9_LIKELY(nargs.FlowControlWait_.IsNullOrZero())) {
         if (nargs.NotifyKind_ != seed::SeedNotifyKind::StreamRecoverEnd) {
            symbsLk.unlock();
            this->RunAfter(TimeInterval_Millisecond(1));
         }
         return;
      }
      // 有流量管制, 設定下次回補時間.
      if (nargs.FlowControlWait_.GetOrigValue() < 0) {
         // 強迫管制, 資料未送出, 所以要還給 symbsRe;
         while (keepi > 0) {
            Symb* symb = keeps[--keepi].get();
            symbsRe.emplace(ToStrView(symb->SymbId_), symb);
         }
         nargs.FlowControlWait_.SetOrigValue(-nargs.FlowControlWait_.GetOrigValue());
      }
      else { // 有流量管制, 但 nargs 的內容已處理.
         if (nargs.NotifyKind_ == seed::SeedNotifyKind::StreamRecoverEnd)
            return;
      }
      symbsLk.unlock();
      this->RunAfter(nargs.FlowControlWait_);
   }
};
//--------------------------------------------------------------------------//
MdSymbsBase::~MdSymbsBase() {
}
void MdSymbsBase::DailyClear(unsigned tdayYYYYMMDD) {
   auto symbs = this->SymbMap_.Lock();
   this->IsDailyClearing_ = true;
   this->RtInnMgr_.DailyClear(tdayYYYYMMDD);
   this->LockedDailyClear(symbs, tdayYYYYMMDD);
   this->IsDailyClearing_ = false;
   if (this->UnsafeSubj_.IsEmpty())
      return;
   // 發行 dailyClear 訊息;
   RevBufferList rts{64};
   *rts.AllocPacket<uint8_t>() = f9fmkt_TradingSessionSt_Clear;
   *rts.AllocPacket<char>() = f9fmkt_TradingSessionId_Normal;
   PutBigEndian(rts.AllocPacket<uint32_t>(), tdayYYYYMMDD);
   RevPutBitv(rts, fon9_BitvV_NumberNull); // DayTime::Null();
   *rts.AllocPacket<uint8_t>() = cast_to_underlying(RtsPackType::TradingSessionSt);
   MdRtsNotifyArgs  e{*this, fon9_kCSTR_SubrTree, GetMdRtsKind(RtsPackType::TradingSessionSt), rts};
   this->UnsafeSubj_.Publish(e);
}
void MdSymbsBase::UnsafePublish(RtsPackType pkType, seed::SeedNotifyArgs& e) {
   assert(this->SymbMap_.IsLocked());
   // IsDailyClearing_ 時, 有可能 RtsPackType::TradingSessionSt 或 PodRemoved(商品過期被移除)
   // 所以這裡要讓 PodRemoved 送給 tree 的訂閱者,
   // 但 TradingSessionSt 由 MdSymbsBase::DailyClear() 送出一次, 不要每個商品都送一次.
   if (this->IsDailyClearing_ && pkType == RtsPackType::TradingSessionSt)
      return;
   if (this->IsBlockPublish_)
      return;
   // 訂閱時, 若沒有提供 'S' = get all SnapshotSymb;
   // 則不保證底下資料的正確:
   // - InfoTime, DealTime
   // - TotalQty
   //   - RtsPackType::DealBS, RtsPackType::DealPack 一般情況不包含 TotalQty;
   //   - 在沒有 TotalQtyLost 的情況下, TotalQty 由 Client 自行計算.
   // - RtsPackType::UpdateBS
   this->UnsafeSubj_.Publish(e);
}
seed::OpResult MdSymbsBase::SubscribeStream(SubConn* pSubConn, seed::Tab& tab, StrView args, seed::FnSeedSubr&& fnSubr) {
   if (!(this->EnAllows_ & EnAllowSubrTree) || &tab != this->RtTab_)
      return seed::SubscribeStreamUnsupported(pSubConn);
   // args = "MdRts:F,S"
   // F(hex) = MdRtsKind MdRtSubr.RtFilter_;
   // 'S' = get all SnapshotSymb; 建構時必須提供 EnAllowSubrSnapshotSymb 旗標;
   const StrView decoderName = StrFetchTrim(args, ':');
   if (decoderName != "MdRts")
      return seed::OpResult::bad_subscribe_stream_args;
   // -----
   SymbsSubrSP  psub{new SymbsSubr{std::move(fnSubr), &args}};
   if (StrTrimHead(&args).Get1st() == ',')
      StrTrimHead(&args, args.begin() + 1);
   // 解析 args 看看是否需要取得「全部商品」的現在資料? this->EnAllows_ 是否開放?
   bool isSubrGetAll = false;
   if ((this->EnAllows_ & EnAllowSubrSnapshotSymb) == EnAllowSubrSnapshotSymb) {
      isSubrGetAll = (args.Get1st() == 'S');
   }
   else if (!args.empty())
      return seed::OpResult::bad_subscribe_stream_args;
   // -----
   auto  symbslk = this->SymbMap_.ConstLock();
   this->UnsafeSubj_.Subscribe(pSubConn, psub);
   // -----
   // 訂閱成功的通知.
   psub(seed::SeedNotifySubscribeOK{*this, nullptr/*tab*/, seed::TextBegin(), nullptr/*rd*/, seed::SeedNotifyKind::SubscribeStreamOK});
   if (isSubrGetAll) {
      TimeStamp tmbeg = UtcNow();
      psub->SymbsRecovering_ = *symbslk;
      fon9_LOG_INFO("MdSymbs.SubrTree|spend=", UtcNow() - tmbeg, "|symbCount=", symbslk->size());
      intrusive_ptr<Recover>  recover{new Recover(*this, psub)};
      symbslk.unlock();
      recover->RunAfter(TimeInterval{});
   }
   return seed::OpResult::no_error;
}
seed::OpResult MdSymbsBase::UnsubscribeStream(SubConn subConn, seed::Tab& tab) {
   if (&tab != this->RtTab_)
      return seed::OpResult::not_supported_subscribe_stream;
   auto   symbslk = this->SymbMap_.ConstLock();
   return MdRtUnsafeSubj_UnsubscribeStream(this->UnsafeSubj_, subConn);
}
//--------------------------------------------------------------------------//
void MdSymbsBase::SaveTo(std::string fname) {
   File fd;
   auto res = fd.Open(fname, FileMode::CreatePath | FileMode::Trunc | FileMode::Append);
   if (res.IsError()) {
      fon9_LOG_ERROR("MdSymbs.SaveTo|fname=", fname, '|', res);
      return;
   }
   std::string                wrbuf;
   RevBufferFixedSize<2048>   rbuf;
   auto symbs = this->SymbMap_.ConstLock();
   for (const auto& isymb : *symbs) {
      rbuf.Rewind();
      SymbCellsToBitv(rbuf, *this->LayoutSP_, *isymb.second);
      ToBitv(rbuf, isymb.first); // SymbId;
      ByteArraySizeToBitvT(rbuf, rbuf.GetUsedSize());
      wrbuf.append(rbuf.GetCurrent(), rbuf.GetMemEnd());
   }
   fd.Append(ToStrView(wrbuf));
}
void MdSymbsBase::LoadFrom(std::string fname) {
   File fd;
   auto res = fd.Open(fname, FileMode::Read);
   if (res.IsError()) {
      if (res.GetError() != ErrC{std::errc::no_such_file_or_directory})
         fon9_LOG_ERROR("MdSymbs.LoadFrom|fname=", fname, '|', res);
      return;
   }
   auto  symbsLk = this->SymbMap_.Lock();
   try {
      File::PosType fpos = 0;
      CharVector    rdbuf;
      CharVector    keyText;
      FileReadAll(fd, fpos, [&rdbuf, &keyText, this, &symbsLk](DcQueueList& dcq, File::Result&) -> bool {
         size_t barySize;
         while (PopBitvByteArraySize(dcq, barySize)) {
            dcq.Read(rdbuf.alloc(barySize), barySize);
            DcQueueFixedMem rds{rdbuf.begin(), barySize};
            BitvTo(rds, keyText);
            Symb&  symb = *this->FetchSymb(symbsLk, ToStrView(keyText));
            size_t tabidx = 0;
            for (auto tabCount = this->LayoutSP_->GetTabCount(); tabCount > 0; --tabCount) {
               BitvTo(rds, tabidx);
               auto* tab = this->LayoutSP_->GetTab(tabidx);
               seed::SimpleRawWr wr{*symb.GetSymbData(static_cast<int>(tabidx))};
               const auto fldCount = tab->Fields_.size();
               for (size_t fldidx = 0; fldidx < fldCount; ++fldidx)
                  tab->Fields_.Get(fldidx)->BitvToCell(wr, rds);
            }
         }
         return true;
      });
   }
   catch (std::runtime_error& e) {
      fon9_LOG_ERROR("MdSymbs.LoadFrom|fname=", fname, "|Read.err=", e.what());
   }
   this->OnAfterLoadFrom(std::move(symbsLk));
}
void MdSymbsBase::OnAfterLoadFrom(Locker&& symbsLk) {
   (void)symbsLk;
}

} } // namespaces
