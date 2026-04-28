// \file fon9/PkCont.cpp
// \author fonwinz@gmail.com
#include "fon9/PkCont.hpp"
#include "fon9/Log.hpp"

namespace fon9 {

PkContFeeder::PkContFeeder(SeqT maxSeqNo, SeqT firstSeqNoAfterOverflow)
   : MaxSeqNo_(maxSeqNo)
   , FirstSeqNoAfterOverflow_(firstSeqNoAfterOverflow) {
}
PkContFeeder::~PkContFeeder() {
   this->Timer_.DisposeAndWait();
}
void PkContFeeder::Clear() {
   this->Timer_.StopAndWait();
   PkPendings::Locker pks{this->PkPendings_};
   pks->clear();
   this->ReceivedCount_ = 0;
   this->DroppedCount_ = 0;
   this->LostCount_ = 0;
   this->NextSeq_ = 0;
}
void PkContFeeder::EmitOnTimer(TimerEntry* timer, TimeStamp now) {
   (void)now;
   PkContFeeder& rthis = ContainerOf(*static_cast<decltype(PkContFeeder::Timer_)*>(timer), &PkContFeeder::Timer_);
   rthis.PkContOnTimer(rthis.PkPendings_.Lock());
}
void PkContFeeder::PkContOnTimer(PkPendings::Locker&& pks) {
   for (const auto& i : *pks) {
      this->LostCount_ += (i.Seq_ - this->NextSeq_);
      this->CallOnReceived(i.data(), static_cast<unsigned>(i.size()), i.Seq_);
   }
   pks->clear();
}
void PkContFeeder::PkContOnDropped(const void* pk, unsigned pksz, SeqT seq) {
   (void)pk; (void)pksz; (void)seq;
}
void PkContFeeder::FeedPacket(const void* pk, unsigned pksz, SeqT seq) {
   {  // lock this->PkPendings_;
      PkPendings::Locker pks{this->PkPendings_};
      if (fon9_LIKELY(seq == this->NextSeq_)) {
__PK_RECEIVED:
         this->CallOnReceived(pk, pksz, seq);
         if (fon9_LIKELY(pks->empty()))
            return;
         auto const ibeg = pks->begin();
         auto const iend = pks->end();
         auto i = ibeg;
         for (;;) {
            auto iSeq = i->Seq_;
            if (fon9_UNLIKELY(iSeq > this->MaxSeqNo_)) {
               iSeq -= this->MaxSeqNo_ + 1;
            }
            if (iSeq != this->NextSeq_)
               break;
            this->CallOnReceived(i->data(), static_cast<unsigned>(i->size()), iSeq);
            if (++i == iend)
               break;
         }
         if (ibeg != i)
            pks->erase(ibeg, i);
         return;
      }
      const auto kMaxGapAllow = this->MaxSeqNo_ / 2;
      if (seq < this->NextSeq_) {
         if (this->NextSeq_ - seq < kMaxGapAllow) {
            // 序號可能超過 this->MaxSeqNo_ 此時序號會變小!
            seq += this->MaxSeqNo_ + 1;
         }
         else {
         __DROP_PK:;
            ++this->DroppedCount_;
            this->PkContOnDropped(pk, pksz, seq);
            return;
         }
      }
      else { // (seq > this->NextSeq_)
         if (this->NextSeq_ == 0 && this->ReceivedCount_ == 0) // ● 啟動後的第1筆封包:
            goto __PK_RECEIVED;                                // => 不用判斷是否連續, 直接開始處理封包;
         if (seq - this->NextSeq_ > kMaxGapAllow)              // ● 序號間隔過大 = 序號已 overflow:
            goto __DROP_PK;                                    // => 現在收到的是 overflow 之前的重複封包;
         if (this->WaitInterval_.GetOrigValue() == 0)          // ● 沒有設定漏包等候時間:
            goto __PK_RECEIVED;                                // => 允許漏包(且若亂序則遺漏小序號封包);
      }
      bool isNeedsRunAfter = pks->empty();
      auto ires = pks->insert(PkRec{seq});
      if (!ires.second)
         return;
      ires.first->assign(static_cast<const char*>(pk), pksz);
      if (!isNeedsRunAfter)
         return;
   } // auto unlock this->PkPendings_.
   this->Timer_.RunAfter(this->WaitInterval_);
}
void PkContFeeder::LogSeqGap(const void* pk, SeqT seq, SeqT lostCount, FnOnLogSeqGap fnOnLogSeqGap) {
   if (fon9_UNLIKELY(this->NextSeq_ == 0)) {
      this->NextSeq_ = 1;
      if ((lostCount = (seq - this->NextSeq_)) == 0)
         return;
      this->LostCount_ += lostCount;
   }
   constexpr auto lv = LogLevel::Warn;
   if (fon9_UNLIKELY(lv >= LogLevel_)) {
      RevBufferList rbuf_{kLogBlockNodeSize};
      RevPutChar(rbuf_, '\n');
      if (lostCount > 1)
         RevPrint(rbuf_, "..", seq - 1);
      RevPrint(rbuf_, "|lost=", lostCount, ':', this->NextSeq_);
      if (fnOnLogSeqGap)
         fnOnLogSeqGap(*this, rbuf_, pk);
      LogWrite(lv, std::move(rbuf_));
   }
}

} // namespaces
