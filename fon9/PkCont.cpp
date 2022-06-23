// \file fon9/PkCont.cpp
// \author fonwinz@gmail.com
#include "fon9/PkCont.hpp"
#include "fon9/Log.hpp"

namespace fon9 {

PkContFeeder::PkContFeeder() {
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
         while (i->Seq_ == this->NextSeq_) {
            this->CallOnReceived(i->data(), static_cast<unsigned>(i->size()), i->Seq_);
            if (++i == iend)
               break;
         }
         if (ibeg != i)
            pks->erase(ibeg, i);
         return;
      }
      if (seq < this->NextSeq_) {
         ++this->DroppedCount_;
         this->PkContOnDropped(pk, pksz, seq);
         return;
      }
      if (this->NextSeq_ == 0 || this->WaitInterval_.GetOrigValue() == 0)
         goto __PK_RECEIVED;
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
