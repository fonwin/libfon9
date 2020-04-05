// \file fon9/fmkt/MdSymbs.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/MdSymbs.hpp"
#include "fon9/Log.hpp"

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
            : base(mdSymbs, nullptr/*tab*/, seed::TextBegin()/*key*/, nullptr/*rd*/,
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
      *nargs.AckBuf_.AllocPacket<uint8_t>() = cast_to_underlying(RtsPackType::SnapshotSymb);
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
   if (0);// 發行 dailyClear 訊息. 給 this->UnsafeSubj_;
   this->IsDailyClearing_ = false;
}
void MdSymbsBase::UnsafePublish(RtsPackType pkType, seed::SeedNotifyArgs& e) {
   assert(this->SymbMap_.IsLocked());
   if (this->IsDailyClearing_ && pkType == RtsPackType::TradingSessionSt)
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
   // struct SubscribeStreamOK : public seed::SeedNotifySubscribeOK {
   //    fon9_NON_COPY_NON_MOVE(SubscribeStreamOK);
   //    using base = seed::SeedNotifySubscribeOK;
   //    const SymbMap::ConstLocker*   SymbsLk_;
   //    SubscribeStreamOK(MdSymbsBase& tree, const SymbMap::ConstLocker* symbslk)
   //       : base{tree, nullptr/*tab*/, seed::TextBegin(), nullptr/*rd*/, seed::SeedNotifyKind::SubscribeStreamOK}
   //       , SymbsLk_(symbslk) {
   //    }
   //    BufferList GetGridViewBuffer(const std::string** ppGvStr) const override {
   //       if (this->SymbsLk_ == nullptr) {
   //          this->CacheGV_.clear();
   //          *ppGvStr = &this->CacheGV_;
   //          return BufferList{};
   //       }
   //       TimeStamp tmbeg = UtcNow();
   //       if (ppGvStr)
   //          *ppGvStr = nullptr;
   //       RevBufferList rbuf{256};
   //       for (const auto& isymb : **this->SymbsLk_) {
   //          SymbCellsToBitv(rbuf, *this->Tree_.LayoutSP_, *isymb.second);
   //          ToBitv(rbuf, isymb.first); // SymbId;
   //       }
   //       auto tmSpend = UtcNow() - tmbeg;
   //       auto szTot = CalcDataSize(rbuf.cfront());
   //       fon9_LOG_INFO("MdSymbs.SubrTree|spend=", tmSpend,
   //                     "|pksz=", szTot,
   //                     "|count=", (*this->SymbsLk_)->size());
   //       return rbuf.MoveOut();
   //    }
   //    void MakeGridView() const override {
   //       this->CacheGV_ = BufferTo<std::string>(this->GetGridViewBuffer(nullptr));
   //    }
   // };
   // psub(SubscribeStreamOK{*this, isSubrGetAll ? &symbslk : nullptr});
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

} } // namespaces
