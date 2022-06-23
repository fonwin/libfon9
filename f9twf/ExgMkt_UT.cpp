// \file f9twf/ExgMkt_UT.cpp
//
// 測試 台灣期交所 行情格式解析.
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9twf/ExgMdPkReceiver.hpp"
#include "f9twf/ExgMdFmtMatch.hpp"
#include "f9twf/ExgMdFmtBS.hpp"
#include "f9twf/ExgMdFmtHL.hpp"
#include "f9twf/ExgMcFmtSS.hpp"
#include "f9twf/ExgMdSymbs.hpp"
#include "f9extests/ExgMktTester.hpp"

//--------------------------------------------------------------------------//
/// = sizeof(f9twf::ExgMiHead); or sizeof(f9twf::ExgMcHead);
unsigned gPkHeadSize = 0;

struct TwfPkReceiver : public fon9::PkReceiver {
   fon9_NON_COPY_NON_MOVE(TwfPkReceiver);
   using base = fon9::PkReceiver;
   using base::ReceivedCount_;
   using base::ChkSumErrCount_;
   using base::DroppedBytes_;
   uint32_t FmtCount_[0x100][0x100];
   uint32_t LastSeq_[0x100][0x100];
   uint32_t SeqMisCount_[0x100][0x100];

   TwfPkReceiver() : base{gPkHeadSize} {
      this->ClearStatus();
   }
   unsigned GetPkSize(const void* pkptr) override {
      return this->PkHeadSize_ == sizeof(f9twf::ExgMiHead)
         ? f9twf::GetPkSize_ExgMi(*reinterpret_cast<const f9twf::ExgMiHead*>(pkptr))
         : f9twf::GetPkSize_ExgMc(*reinterpret_cast<const f9twf::ExgMcHead*>(pkptr));
   }
   void ClearStatus() {
      base::ClearStatus();
      memset(this->FmtCount_, 0, sizeof(this->FmtCount_));
      memset(this->LastSeq_, 0, sizeof(this->LastSeq_));
      memset(this->SeqMisCount_, 0, sizeof(this->SeqMisCount_));
   }
   bool OnPkReceived(const void* pkptr, unsigned pksz) override {
      (void)pksz;
      const f9twf::ExgMdHead0& pk = *reinterpret_cast<const f9twf::ExgMdHead0*>(pkptr);
      unsigned char  txCode = fon9::unsigned_cast(pk.TransmissionCode_);
      unsigned char  mgKind = fon9::unsigned_cast(pk.MessageKind_);
      ++this->FmtCount_[txCode][mgKind];

      uint32_t  seqNo;
      if (this->PkHeadSize_ == sizeof(f9twf::ExgMiHead))
         seqNo = fon9::PackBcdTo<uint32_t>(reinterpret_cast<const f9twf::ExgMiHead*>(pkptr)->InformationSeq_);
      else {
         bool isHb = (txCode == '0' && mgKind == '1'); // Hb: CHANNEL-SEQ 欄位會為該傳輸群組之末筆序號訊息。
         uint32_t chid = fon9::PackBcdTo<uint32_t>(reinterpret_cast<const f9twf::ExgMcHead*>(pkptr)->ChannelId_);
         txCode = static_cast<unsigned char>(chid >> 8);
         mgKind = static_cast<unsigned char>(chid & 0xff);
         seqNo = static_cast<uint32_t>(
            fon9::PackBcdTo<uint64_t>(reinterpret_cast<const f9twf::ExgMcHead*>(pkptr)->ChannelSeq_));
         if (isHb)
            --this->LastSeq_[txCode][mgKind];
      }
      if (seqNo != this->LastSeq_[txCode][mgKind] + 1)
         ++this->SeqMisCount_[txCode][mgKind];
      this->LastSeq_[txCode][mgKind] = seqNo;
      return true;
   }
};
//--------------------------------------------------------------------------//

/// 買賣報價 & 成交明細.
struct RtParser : public TwfPkReceiver, public fon9::fmkt::SymbTree {
   fon9_NON_COPY_NON_MOVE(RtParser);
   using basePkReceiver = TwfPkReceiver;
   using baseTree = fon9::fmkt::SymbTree;
   using MiParserMap = f9twf::ExgMdParserMap<f9twf::ExgMiHead, RtParser>;
   using McParserMap = f9twf::ExgMdParserMap<f9twf::ExgMcHead, RtParser>;
   MiParserMap MiParserMap_;
   McParserMap McParserMap_;
   size_t      ParsedCount_{};
   size_t      TotalQtyLostCount_{};
   size_t      UnknownValue_{};

   RtParser() : baseTree{fon9::seed::LayoutSP{}} {
      // 逐筆: 成交價量:I024;
      this->McParserMap_.Reg<'2', 'D', 1>(&McI024MatchParser);
      this->McParserMap_.Reg<'5', 'D', 1>(&McI024MatchParser);
      // 一般: 成交價量:I020; 試撮成交價量:I022;
      this->MiParserMap_.Reg<'2', '1', 4>(&MiI020MatchParser);
      this->MiParserMap_.Reg<'5', '1', 4>(&MiI020MatchParser);
      this->MiParserMap_.Reg<'2', '7', 2>(&MiI022MatchParser);
      this->MiParserMap_.Reg<'5', '7', 2>(&MiI022MatchParser);
      // 逐筆: 委託簿揭示訊息:I081; 委託簿快照訊息:I083;
      this->McParserMap_.Reg<'2', 'A', 1>(&McI081BSParser);
      this->McParserMap_.Reg<'5', 'A', 1>(&McI081BSParser);
      this->McParserMap_.Reg<'2', 'B', 1>(&McI083BSParser);
      this->McParserMap_.Reg<'5', 'B', 1>(&McI083BSParser);
      // 一般: 委託簿揭示訊息:I080; 試撮後剩餘委託簿揭示訊息:I082;
      this->MiParserMap_.Reg<'2', '2', 2>(&MiI080BSParser);
      this->MiParserMap_.Reg<'5', '2', 2>(&MiI080BSParser);
      this->MiParserMap_.Reg<'2', '8', 1>(&MiI082BSParser);
      this->MiParserMap_.Reg<'5', '8', 1>(&MiI082BSParser);
      // 最高價、最低價.
      this->McParserMap_.Reg<'2', 'E', 1>(&McI025HLParser);
      this->McParserMap_.Reg<'5', 'E', 1>(&McI025HLParser);
      this->MiParserMap_.Reg<'2', '5', 3>(&MiI021HLParser);
      this->MiParserMap_.Reg<'5', '5', 3>(&MiI021HLParser);
      // 快照.
      this->McParserMap_.Reg<'2', 'C', 1>(&McI084SSParser);
      this->McParserMap_.Reg<'5', 'C', 1>(&McI084SSParser);
   }
   fon9::fmkt::SymbSP MakeSymb(const fon9::StrView& symbid) override {
      return new f9extests::SymbIn{symbid};
   }
   size_t GetParsedCount() const {
      return this->ParsedCount_;
   }
   void PrintInfo() {
      if (this->TotalQtyLostCount_ > 0)
         std::cout << "TotalQty lost count=" << this->TotalQtyLostCount_ << std::endl;
      if (this->UnknownValue_ > 0)
         std::cout << "Unknown value count=" << this->UnknownValue_ << std::endl;
   }
   bool OnPkReceived(const void* pkptr, unsigned pksz) override {
      basePkReceiver::OnPkReceived(pkptr, pksz);
      if (this->PkHeadSize_ == sizeof(f9twf::ExgMiHead))
         this->ParsedCount_ += this->MiParserMap_.Call(*reinterpret_cast<const f9twf::ExgMiHead*>(pkptr), pksz, *this);
      else
         this->ParsedCount_ += this->McParserMap_.Call(*reinterpret_cast<const f9twf::ExgMcHead*>(pkptr), pksz, *this);
      return true;
   }
   //-----------------------------------------------------------------------//
   static void McI024MatchParser(const f9twf::ExgMcHead& pk, unsigned pksz, RtParser& dst) {
      if (AssignMatch(dst, *static_cast<const f9twf::ExgMcI024*>(&pk),
                      static_cast<const f9twf::ExgMcI024*>(&pk)->CalculatedFlag_ == '1')
          != pksz - sizeof(f9twf::ExgMdTail))
         fon9_CheckTestResult("McI024MatchParser.pksz", false);
   }
   static void MiI020MatchParser(const f9twf::ExgMiHead& pk, unsigned pksz, RtParser& dst) {
      if (AssignMatch(dst, *static_cast<const f9twf::ExgMiI020*>(&pk), false)
          != pksz - sizeof(f9twf::ExgMdMatchStatusCode) - sizeof(f9twf::ExgMdTail))
         fon9_CheckTestResult("MiI020MatchParser.pksz", false);
   }
   static void MiI022MatchParser(const f9twf::ExgMiHead& pk, unsigned pksz, RtParser& dst) {
      if(AssignMatch(dst, *static_cast<const f9twf::ExgMiI022*>(&pk), true)
         != pksz - sizeof(f9twf::ExgMdMatchStatusCode) - sizeof(f9twf::ExgMdTail))
         fon9_CheckTestResult("MiI022MatchParser.pksz", false);
   }
   // isCalc = true = 試撮價格訊息.
   template <class Pk>
   static uintptr_t AssignMatch(RtParser& dst, const Pk& pk, bool isCalc) {
      return AssignMatch(dst, fon9::StrView_eos_or_all(pk.ProdId_.Chars_, ' '), pk, pk, isCalc)
             - reinterpret_cast<uintptr_t>(&pk);
   }
   static uintptr_t AssignMatch(RtParser& dst, const fon9::StrView& symbid,
                                const f9twf::ExgMdMatchHead& mat, const f9twf::ExgMdHead0& hdr,
                                bool isCalc) {
      SymbMap::Locker      symbs{dst.SymbMap_};
      f9extests::SymbIn&   symb = *static_cast<f9extests::SymbIn*>(dst.FetchSymb(symbs, symbid).get());
      symb.Deal_.Data_.InfoTime_ = hdr.InformationTime_.ToDayTime();
      symb.Deal_.Data_.DealTime_ = mat.MatchTime_.ToDayTime();
      mat.FirstMatchPrice_.AssignTo(symb.Deal_.Data_.Deal_.Pri_, symb.PriceOrigDiv_);
      symb.Deal_.Data_.Deal_.Qty_ = fon9::PackBcdTo<uint32_t>(mat.FirstMatchQty_);
      const f9twf::ExgMdMatchData* pdat = mat.MatchData_;
      auto checkTotalQty = symb.Deal_.Data_.TotalQty_ + symb.Deal_.Data_.Deal_.Qty_;
      if (auto count = (mat.MatchDisplayItem_ & 0x7f)) {
         do {
            pdat->MatchPrice_.AssignTo(symb.Deal_.Data_.Deal_.Pri_, symb.PriceOrigDiv_);
            symb.Deal_.Data_.Deal_.Qty_ = fon9::PackBcdTo<uint32_t>(pdat->MatchQty_);
            checkTotalQty += symb.Deal_.Data_.Deal_.Qty_;
            ++pdat;
         } while (--count > 0);
      }
      const f9twf::ExgMdMatchCnt* pcnt = reinterpret_cast<const f9twf::ExgMdMatchCnt*>(pdat);
      symb.Deal_.Data_.TotalQty_ = fon9::PackBcdTo<uint32_t>(pcnt->MatchTotalQty_);
      if (!isCalc && symb.Deal_.Data_.TotalQty_ != checkTotalQty)
         ++dst.TotalQtyLostCount_;
      return reinterpret_cast<uintptr_t>(pcnt + 1);
   }
   //-----------------------------------------------------------------------//
   static void MiI080BSParser(const f9twf::ExgMiHead& pk, unsigned pksz, RtParser& dst) {
      MiBSParser(*static_cast<const f9twf::ExgMiI080*>(&pk), pksz, dst);
   }
   static void MiI082BSParser(const f9twf::ExgMiHead& pk, unsigned pksz, RtParser& dst) {
      MiBSParser(*static_cast<const f9twf::ExgMiI082*>(&pk), pksz, dst);
   }
   static void MiBSParser(const f9twf::ExgMiI080& pk, unsigned pksz, RtParser& dst) {
      fon9::StrView        symbid = fon9::StrView_eos_or_all(pk.ProdId_.Chars_, ' ');
      SymbMap::Locker      symbs{dst.SymbMap_};
      f9extests::SymbIn&   symb = *static_cast<f9extests::SymbIn*>(dst.FetchSymb(symbs, symbid).get());
      static_assert(fon9::fmkt::SymbBSData::kBSCount == sizeof(pk.BuyOrderBook_)/sizeof(pk.BuyOrderBook_[0]),
                    "fon9::fmkt::SymbBS::kBSCount must equal to numofele(pk.BuyOrderBook_)");
      AssignBS(symb.BS_.Data_.Buys_, pk.BuyOrderBook_, symb.PriceOrigDiv_);
      AssignBS(symb.BS_.Data_.Sells_, pk.SellOrderBook_, symb.PriceOrigDiv_);
      // pk.DerivedFlag_: 有無衍生委託單之旗標: 01:解析 FirstDerived_; 00:不帶 FirstDerived_.
      if (fon9::PackBcdTo<uint8_t>(pk.DerivedFlag_) == 0) {
         if (pksz != sizeof(pk) + sizeof(f9twf::ExgMdTail) - sizeof(pk.FirstDerived_))
            fon9_CheckTestResult("MiBSParser.pksz", false);
      }
      else {
         if (pksz != sizeof(pk) + sizeof(f9twf::ExgMdTail))
            fon9_CheckTestResult("MiBSParser+Derived.pksz", false);
         f9twf::ExgMdPriceTo(symb.BS_.Data_.DerivedBuy_.Pri_, pk.FirstDerived_.BuyPrice_, symb.PriceOrigDiv_);
         f9twf::ExgMdPriceTo(symb.BS_.Data_.DerivedSell_.Pri_, pk.FirstDerived_.SellPrice_, symb.PriceOrigDiv_);
         symb.BS_.Data_.DerivedBuy_.Qty_ = fon9::PackBcdTo<uint32_t>(pk.FirstDerived_.BuyQty_);
         symb.BS_.Data_.DerivedSell_.Qty_ = fon9::PackBcdTo<uint32_t>(pk.FirstDerived_.SellQty_);
      }
   }
   static void AssignBS(fon9::fmkt::PriQty* dst, const f9twf::ExgMdOrderPQ* pqs, uint32_t origDiv) {
      for (unsigned L = 0; L < fon9::fmkt::SymbBSData::kBSCount; ++L) {
         pqs->Price_.AssignTo(dst->Pri_, origDiv);
         dst->Qty_ = fon9::PackBcdTo<uint32_t>(pqs->Qty_);
         ++pqs;
         ++dst;
      }
   }
   //-----------------------------------------------------------------------//
   static void McI081BSParser(const f9twf::ExgMcHead& pk, unsigned pksz, RtParser& rthis) {
      fon9::StrView      symbid = fon9::StrView_eos_or_all(static_cast<const f9twf::ExgMcI081*>(&pk)->ProdId_.Chars_, ' ');
      SymbMap::Locker    symbs{rthis.SymbMap_};
      f9extests::SymbIn& symb = *static_cast<f9extests::SymbIn*>(rthis.FetchSymb(symbs, symbid).get());
      unsigned mdCount = fon9::PackBcdTo<unsigned>(static_cast<const f9twf::ExgMcI081*>(&pk)->NoMdEntries_);
      auto*    mdEntry = static_cast<const f9twf::ExgMcI081*>(&pk)->MdEntry_;
      f9twf::ExgMdToUpdateBS(pk.InformationTime_.ToDayTime(), mdCount, mdEntry, symb,
                             fon9::fmkt::PackBcdToMarketSeq(static_cast<const f9twf::ExgMcI081*>(&pk)->ProdMsgSeq_));
      if (reinterpret_cast<uintptr_t>(mdEntry + mdCount) - reinterpret_cast<uintptr_t>(&pk)
          != pksz - sizeof(f9twf::ExgMdTail))
         fon9_CheckTestResult("McI081BSParser.pksz", false);
   }
   static void McI083BSParser(const f9twf::ExgMcHead& pk, unsigned pksz, RtParser& rthis) {
      fon9::StrView      symbid = fon9::StrView_eos_or_all(static_cast<const f9twf::ExgMcI083*>(&pk)->ProdId_.Chars_, ' ');
      SymbMap::Locker    symbs{rthis.SymbMap_};
      f9extests::SymbIn& symb = *static_cast<f9extests::SymbIn*>(rthis.FetchSymb(symbs, symbid).get());
      symb.BS_.Data_.Clear();
      const void* entryEnd = f9twf::ExgMdToSnapshotBS(pk.InformationTime_.ToDayTime(),
         fon9::PackBcdTo<unsigned>(static_cast<const f9twf::ExgMcI083*>(&pk)->NoMdEntries_),
         static_cast<const f9twf::ExgMcI083*>(&pk)->MdEntry_,
         symb,
         fon9::fmkt::PackBcdToMarketSeq(static_cast<const f9twf::ExgMcI083*>(&pk)->ProdMsgSeq_));
      if (reinterpret_cast<uintptr_t>(entryEnd) - reinterpret_cast<uintptr_t>(&pk)
          != pksz - sizeof(f9twf::ExgMdTail))
         fon9_CheckTestResult("McI083BSParser.pksz", false);
   }
   //-----------------------------------------------------------------------//
   static void McI025HLParser(const f9twf::ExgMcHead& pk, unsigned pksz, RtParser& rthis) {
      AssignHL(rthis, *static_cast<const f9twf::ExgMcI025*>(&pk), pksz);
   }
   static void MiI021HLParser(const f9twf::ExgMiHead& pk, unsigned pksz, RtParser& rthis) {
      AssignHL(rthis, *static_cast<const f9twf::ExgMiI021*>(&pk), pksz);
   }
   template <class Pk>
   static void AssignHL(RtParser& rthis, const Pk& pk, unsigned pksz) {
      if (pksz != sizeof(pk))
         fon9_CheckTestResult("AssignHL.pksz", false);
      AssignHL(rthis, pk);
   }
   static void AssignHL(RtParser& rthis, const f9twf::ExgMdDayHighLow& pk) {
      (void)rthis; (void)pk;
   }
   //-----------------------------------------------------------------------//
   static void McI084SSParser(const f9twf::ExgMcHead& pk, unsigned pksz, RtParser& rthis) {
      switch (static_cast<const f9twf::ExgMcI084*>(&pk)->MessageType_) {
      case 'A':
         McI084SSParserA(static_cast<const f9twf::ExgMcI084*>(&pk)->_A_RefreshBegin_, pksz, rthis);
         break;
      case 'O':
         McI084SSParserO(static_cast<const f9twf::ExgMcI084*>(&pk)->_O_OrderData_, pksz, rthis);
         break;
      case 'S':
         McI084SSParserS(static_cast<const f9twf::ExgMcI084*>(&pk)->_S_ProductStatistics_, pksz, rthis);
         break;
      case 'P':
         McI084SSParserP(static_cast<const f9twf::ExgMcI084*>(&pk)->_P_ProductStatus_, pksz, rthis);
         break;
      case 'Z':
         McI084SSParserZ(static_cast<const f9twf::ExgMcI084*>(&pk)->_Z_RefreshComplete_, pksz, rthis);
         break;
      default:
         ++rthis.UnknownValue_;
         break;
      }
   }
   static void McI084SSParserA(const f9twf::ExgMcI084::RefreshBegin& pk, unsigned pksz, RtParser& rthis) {
      (void)pk; (void)rthis;
      if (pksz != sizeof(pk) + sizeof(const f9twf::ExgMcNoBody) + 1/*MessageType*/)
         fon9_CheckTestResult("McI084SSParserA.pksz", false);
   }
   static void McI084SSParserO(const f9twf::ExgMcI084::OrderData& pk, unsigned pksz, RtParser& rthis) {
      (void)rthis;
      unsigned prodCount = fon9::PackBcdTo<unsigned>(pk.NoEntries_);
      const f9twf::ExgMcI084::OrderDataEntry* prod = pk.Entry_;
      for (unsigned L = 0; L < prodCount; ++L) {
         unsigned mdCount = fon9::PackBcdTo<unsigned>(prod->NoMdEntries_);
         // 處理 prod & md
         prod = reinterpret_cast<const f9twf::ExgMcI084::OrderDataEntry*>(prod->MdEntry_ + mdCount);
      }
      if (pksz != (sizeof(const f9twf::ExgMcNoBody) + 1/*MessageType*/
                   + (reinterpret_cast<uintptr_t>(prod) - reinterpret_cast<uintptr_t>(&pk))))
         fon9_CheckTestResult("McI084SSParserO.pksz", false);
   }
   static void McI084SSParserS(const f9twf::ExgMcI084::ProductStatistics& pk, unsigned pksz, RtParser& rthis) {
      (void)pk; (void)rthis;
      if (pksz != (sizeof(pk) + sizeof(const f9twf::ExgMcNoBody) + 1/*MessageType*/
                   + sizeof(*pk.Entry_) * (fon9::PackBcdTo<unsigned>(pk.NoEntries_) - 1)))
         fon9_CheckTestResult("McI084SSParserS.pksz", false);
   }
   static void McI084SSParserP(const f9twf::ExgMcI084::ProductStatus& pk, unsigned pksz, RtParser& rthis) {
      (void)pk; (void)rthis;
      if (pksz != (sizeof(pk) + sizeof(const f9twf::ExgMcNoBody) + 1/*MessageType*/
                   + sizeof(*pk.Entry_) * (fon9::PackBcdTo<unsigned>(pk.NoEntries_) - 1)))
         fon9_CheckTestResult("McI084SSParserP.pksz", false);
   }
   static void McI084SSParserZ(const f9twf::ExgMcI084::RefreshComplete& pk, unsigned pksz, RtParser& rthis) {
      (void)pk; (void)rthis;
      if (pksz != sizeof(pk) + sizeof(const f9twf::ExgMcNoBody) + 1/*MessageType*/)
         fon9_CheckTestResult("McI084SSParserZ.pksz", false);
   }
};

int main(int argc, char* argv[]) {
   fon9::AutoPrintTestInfo utinfo{"f9twf ExgMkt"};

   if (argc < 4) {
      std::cout << "Usage: MarketDataFile ReadFrom ReadSize [Steps or ?SymbId]\n"
         "ReadSize=0 for read to EOF.\n"
         "Steps=0 for test Rt(Match & BS) parsing.\n"
         << std::endl;
      return 3;
   }
   f9extests::MktDataFile  mdf{argv};
   if (!mdf.Buffer_)
      return 3;
   // 檢查格式.
   if (const char* pEsc = reinterpret_cast<const char*>(memchr(mdf.Buffer_, 27, mdf.Size_))) {
      const char* pEnd = mdf.Buffer_ + mdf.Size_;
      unsigned pksz = f9twf::GetPkSize_ExgMi(*reinterpret_cast<const f9twf::ExgMiHead*>(pEsc));
      if (pEsc + pksz <= pEnd && pEsc[pksz - 2] == '\x0d' && pEsc[pksz - 1] == '\x0a') {
         gPkHeadSize = sizeof(f9twf::ExgMiHead);
         std::cout << "fmt=Information" << std::endl;
      }
      else {
         pksz = f9twf::GetPkSize_ExgMc(*reinterpret_cast<const f9twf::ExgMcHead*>(pEsc));
         if (pEsc + pksz <= pEnd && pEsc[pksz - 2] == '\x0d' && pEsc[pksz - 1] == '\x0a') {
            gPkHeadSize = sizeof(f9twf::ExgMcHead);
            std::cout << "fmt=Channel:"
               << fon9::PackBcdTo<unsigned>(reinterpret_cast<const f9twf::ExgMcHead*>(pEsc)->ChannelId_)
               << std::endl;
         }
      }
   }
   if (gPkHeadSize == 0) {
      std::cout << "err=Unknown format." << mdf.Size_ << std::endl;
      return 3;
   }
   // [TRANSMISSION-CODE][MESSAGE-KIND]
   static const char* fmtName[0x100][0x100];
   fmtName['0']['0'] = "I000(Hb)";
   fmtName['0']['1'] = "I001(Hb)";
   fmtName['0']['2'] = "I002(Rst)";
   // --- Fut
   fmtName['1']['1'] = "Fut.I010";  fmtName['2']['1'] = "Fut.I020";  fmtName['3']['1'] = "Fut.I070";  fmtName['7']['1'] = "Fut.B020";
   fmtName['1']['2'] = "Fut.I030";  fmtName['2']['2'] = "Fut.I080";  fmtName['3']['2'] = "Fut.I071";  fmtName['7']['2'] = "Fut.B080";
   fmtName['1']['3'] = "Fut.I011";  fmtName['2']['3'] = "Fut.I140";  fmtName['3']['3'] = "Fut.I072";  fmtName['7']['3'] = "Fut.B021";
   fmtName['1']['4'] = "Fut.I050";  fmtName['2']['4'] = "Fut.I100";  fmtName['3']['4'] = "Fut.I073";
   fmtName['1']['5'] = "Fut.I060";  fmtName['2']['5'] = "Fut.I021";
   fmtName['1']['6'] = "Fut.I120";  fmtName['2']['6'] = "Fut.I023";
   fmtName['1']['7'] = "Fut.I130";  fmtName['2']['7'] = "Fut.I022";
   fmtName['1']['8'] = "Fut.I064";  fmtName['2']['8'] = "Fut.I082";
   fmtName['1']['9'] = "Fut.I065";  fmtName['2']['9'] = "Fut.I090";
                                    fmtName['2']['A'] = "Fut.I081";
                                    fmtName['2']['B'] = "Fut.I083";
                                    fmtName['2']['C'] = "Fut.I084";
                                    fmtName['2']['D'] = "Fut.I024";
                                    fmtName['2']['E'] = "Fut.I025";
   // --- Opt
   fmtName['4']['1'] = "Opt.I010";  fmtName['5']['1'] = "Opt.I020";  fmtName['6']['1'] = "Opt.I070";
   fmtName['4']['2'] = "Opt.I030";  fmtName['5']['2'] = "Opt.I080";  fmtName['6']['2'] = "Opt.I071";
   fmtName['4']['3'] = "Opt.I011";  fmtName['5']['3'] = "Opt.I140";  fmtName['6']['3'] = "Opt.I072";
   fmtName['4']['4'] = "Opt.I050";  fmtName['5']['4'] = "Opt.I100";
   fmtName['4']['5'] = "Opt.I060";  fmtName['5']['5'] = "Fut.I021";
   fmtName['4']['6'] = "Opt.I120";  fmtName['5']['6'] = "Fut.I023";
   fmtName['4']['7'] = "Opt.I130";  fmtName['5']['7'] = "Fut.I022";
   fmtName['4']['8'] = "Opt.I064";  fmtName['5']['8'] = "Fut.I082";
                                    fmtName['5']['9'] = "Fut.I090";
                                    fmtName['5']['A'] = "Opt.I081";
                                    fmtName['5']['B'] = "Opt.I083";
                                    fmtName['5']['C'] = "Opt.I084";
                                    fmtName['5']['D'] = "Opt.I024";
                                    fmtName['5']['E'] = "Opt.I025";
   // ---
   TwfPkReceiver  pkReceiver;
   double         tmFull = f9extests::ExgMktFeedAll(mdf, pkReceiver);
   for (unsigned txL = 0; txL < 0x100; ++txL) {
      for (unsigned mgL = 0; mgL < 0x100; ++mgL) {
         if (pkReceiver.FmtCount_[txL][mgL]) {
            std::cout << "Fmt=" << static_cast<char>(txL) << static_cast<char>(mgL)
               << ":" << (fmtName[txL][mgL] ? fmtName[txL][mgL] : "?")
               << "|Count=" << pkReceiver.FmtCount_[txL][mgL];
            if (gPkHeadSize == sizeof(f9twf::ExgMiHead)) {
               std::cout << "|LastSeq=" << pkReceiver.LastSeq_[txL][mgL]
                         << "|MisCount=" << pkReceiver.SeqMisCount_[txL][mgL];
            }
            std::cout << std::endl;
         }
      }
   }
   if (gPkHeadSize == sizeof(f9twf::ExgMcHead)) {
      for (unsigned txL = 0; txL < 0x100; ++txL) {
         for (unsigned mgL = 0; mgL < 0x100; ++mgL) {
            if (pkReceiver.LastSeq_[txL][mgL] || pkReceiver.SeqMisCount_[txL][mgL]) {
               std::cout << "Channel=" << ((txL << 8) + mgL)
                  << "|LastSeq=" << pkReceiver.LastSeq_[txL][mgL]
                  << "|MisCount=" << pkReceiver.SeqMisCount_[txL][mgL]
                  << std::endl;
            }
         }
      }
   }
   if (argc >= 5) {
      if (f9extests::CheckExgMktFeederStep(pkReceiver, mdf, argv[4]) == 0)
         f9extests::ExgMktTestParser<RtParser>("Parse Match+BS", mdf, argc - 4, argv + 4, tmFull,
                                               fon9::FmtDef{7,8});
   }
}
