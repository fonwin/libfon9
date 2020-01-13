// \file f9tws/ExgMkt_UT.cpp
//
// 測試 TSEC/OTC 行情格式解析.
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9tws/ExgMktPkReceiver.hpp"
#include "f9tws/ExgMktFmt6.hpp"
#include "f9extests/ExgMktTester.hpp"

struct TwsPkReceiver : public f9tws::ExgMktPkReceiver {
   fon9_NON_COPY_NON_MOVE(TwsPkReceiver);
   using base = f9tws::ExgMktPkReceiver;
   using base::ReceivedCount_;
   using base::ChkSumErrCount_;
   using base::DroppedBytes_;
   uint32_t FmtCount_[f9tws::kExgMktMaxFmtNoSize];
   uint32_t LastSeq_[f9tws::kExgMktMaxFmtNoSize];
   uint32_t SeqMisCount_[f9tws::kExgMktMaxFmtNoSize];

   TwsPkReceiver() {
      this->ClearStatus();
   }
   void ClearStatus() {
      base::ClearStatus();
      memset(this->FmtCount_, 0, sizeof(this->FmtCount_));
      memset(this->LastSeq_, 0, sizeof(this->LastSeq_));
      memset(this->SeqMisCount_, 0, sizeof(this->SeqMisCount_));
   }
   bool OnPkReceived(const void* pkptr, unsigned pksz) override {
      (void)pksz;
      const f9tws::ExgMktHeader& pk = *reinterpret_cast<const f9tws::ExgMktHeader*>(pkptr);
      auto fmtNo = fon9::PackBcdTo<uint32_t>(pk.FmtNo_);
      ++this->FmtCount_[fmtNo];

      auto seqNo = fon9::PackBcdTo<uint32_t>(pk.SeqNo_);
      if (seqNo != this->LastSeq_[fmtNo] + 1)
         ++this->SeqMisCount_[fmtNo];
      this->LastSeq_[fmtNo] = seqNo;
      return true;
   }
};

struct Fmt6Parser : public TwsPkReceiver, public fon9::fmkt::SymbTree {
   fon9_NON_COPY_NON_MOVE(Fmt6Parser);
   using basePkReceiver = TwsPkReceiver;
   using baseTree = fon9::fmkt::SymbTree;
   Fmt6Parser() : baseTree{fon9::seed::LayoutSP{}} {
   }
   fon9::fmkt::SymbSP MakeSymb(const fon9::StrView& symbid) override {
      return new f9extests::SymbIn{symbid};
   }
   size_t GetParsedCount() const {
      return this->FmtCount_[6] + this->FmtCount_[17];
   }
   void PrintInfo() {
   }
   bool OnPkReceived(const void* pkptr, unsigned pksz) override {
      basePkReceiver::OnPkReceived(pkptr, pksz);
      const f9tws::ExgMktHeader& pk = *reinterpret_cast<const f9tws::ExgMktHeader*>(pkptr);
      auto fmtNo = pk.GetFmtNo();
      if (fmtNo != 6 && fmtNo != 17)
         return true;
      const f9tws::ExgMktFmt6v3& fmt6 = *static_cast<const f9tws::ExgMktFmt6v3*>(&pk);
      const f9tws::ExgMktPriQty* pqs  = fmt6.PQs_;
      unsigned                   tmHH = fon9::PackBcdTo<unsigned>(fmt6.Time_.HH_);
      if (fon9_UNLIKELY(tmHH == 99)) // 股票代號"000000"且撮合時間"999999999999",
         return true;                // 表示普通股競價交易末筆即時行情資料已送出.
      uint64_t tmu6 = (((tmHH * 60) + fon9::PackBcdTo<unsigned>(fmt6.Time_.MM_)) * 60
                       + fon9::PackBcdTo<unsigned>(fmt6.Time_.SS_));
      tmu6 = (tmu6 * 1000000) + fon9::PackBcdTo<unsigned>(fmt6.Time_.U6_);
      fon9::StrView        stkno{f9tws::ToStrView(fmt6.StkNo_)};
      SymbMap::Locker      symbs{this->SymbMap_};
      f9extests::SymbIn&   symb = *static_cast<f9extests::SymbIn*>(this->FetchSymb(symbs, stkno).get());
      if (fmt6.ItemMask_ & 0x80) {
         symb.Deal_.Data_.Time_.Assign<6>(tmu6);
         symb.Deal_.Data_.TotalQty_ = fon9::PackBcdTo<fon9::fmkt::Qty>(fmt6.TotalQty_);
         pqs->AssignTo(symb.Deal_.Data_.Deal_);
         ++pqs;
      }
      if ((fmt6.ItemMask_ & 0x7e) || (fmt6.ItemMask_ & 1) == 0) {
         symb.BS_.Data_.Time_.Assign<6>(tmu6);
         pqs = AssignBS(symb.BS_.Data_.Buys_, pqs, (fmt6.ItemMask_ & 0x70) >> 4);
         pqs = AssignBS(symb.BS_.Data_.Sells_, pqs, (fmt6.ItemMask_ & 0x0e) >> 1);
      }
      return true;
   }
   static const f9tws::ExgMktPriQty* AssignBS(fon9::fmkt::PriQty* dst, const f9tws::ExgMktPriQty* pqs, int count) {
      if (count > fon9::fmkt::SymbBS::kBSCount)
         count = fon9::fmkt::SymbBS::kBSCount;
      for (int L = 0; L < count; ++L) {
         pqs->AssignTo(*dst);
         ++dst;
         ++pqs;
      }
      if (count < fon9::fmkt::SymbBS::kBSCount)
         memset(dst, 0, sizeof(*dst) * (fon9::fmkt::SymbBS::kBSCount - count));
      return pqs;
   }
};

int main(int argc, char* argv[]) {
   fon9::AutoPrintTestInfo utinfo{"f9tws ExgMkt"};

   if (argc < 4) {
      std::cout << "Usage: MarketDataFile ReadFrom ReadSize [Steps or ?SymbId]\n"
         "ReadSize=0 for read to EOF.\n"
         "Steps=0 or '?SymbId' for test Match/BS(Fmt6+17) parsing.\n"
         << std::endl;
      return 3;
   }

   f9extests::MktDataFile  mdf{argv};
   if (!mdf.Buffer_)
      return 3;

   TwsPkReceiver pkReceiver;
   double        tmFull = f9extests::ExgMktFeedAll(mdf, pkReceiver);
   for (unsigned L = 0; L < f9tws::kExgMktMaxFmtNoSize; ++L) {
      if (pkReceiver.FmtCount_[L]) {
         std::cout << "FmtNo=" << L
            << "|Count=" << pkReceiver.FmtCount_[L]
            << "|LastSeq=" << pkReceiver.LastSeq_[L]
            << "|MisCount=" << pkReceiver.SeqMisCount_[L]
            << std::endl;
      }
   }
   if (argc >= 5) {
      if (f9extests::CheckExgMktFeederStep(pkReceiver, mdf, argv[4]) == 0)
         f9extests::ExgMktTestParser<Fmt6Parser>("Parse Fmt6+17", mdf, argc - 4, argv + 4, tmFull);
   }
}
