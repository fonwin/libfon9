// \file fon9/rc/RcMdRtsDecoder.cpp
// \author fonwinz@gmail.com
#include "fon9/rc/RcMdRtsDecoder.hpp"
#include "fon9/rc/RcMdRtsDecoder.h"
#include "fon9/fmkt/SymbDeal.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/BitvDecode.hpp"

namespace fon9 { namespace rc {

#define STATIC_ASSERT_CHECK_ENUM(EnumTypeName, ValueName) \
   static_assert(cast_to_underlying(f9sv_##EnumTypeName##_##ValueName) == cast_to_underlying(fmkt::EnumTypeName::ValueName), "");
// -----
STATIC_ASSERT_CHECK_ENUM(DealFlag, Calculated);
STATIC_ASSERT_CHECK_ENUM(DealFlag, DealTimeChanged);
STATIC_ASSERT_CHECK_ENUM(DealFlag, DealBuyCntChanged);
STATIC_ASSERT_CHECK_ENUM(DealFlag, DealSellCntChanged);
STATIC_ASSERT_CHECK_ENUM(DealFlag, TotalQtyLost);
STATIC_ASSERT_CHECK_ENUM(BSFlag, Calculated);
STATIC_ASSERT_CHECK_ENUM(BSFlag, OrderBuy);
STATIC_ASSERT_CHECK_ENUM(BSFlag, OrderSell);
STATIC_ASSERT_CHECK_ENUM(BSFlag, DerivedBuy);
STATIC_ASSERT_CHECK_ENUM(BSFlag, DerivedSell);
//--------------------------------------------------------------------------//
/// 讀取 BigEndian 的數值.
/// 若資料不足, 則觸發 exception: Raise<BitvNeedsMore>("RcMdRtsDecoder.Read");
template <typename ValueT>
inline ValueT ReadOrRaise(DcQueue& rxbuf) {
   ValueT res;
   auto*  pres = rxbuf.Peek(&res, sizeof(res));
   if(pres == nullptr)
      Raise<BitvNeedsMore>("RcMdRtsDecoder.Read");
   res = GetBigEndian<ValueT>(pres);
   rxbuf.PopConsumed(sizeof(ValueT));
   return res;
}
inline seed::Tab* GetTabOrRaise(seed::Layout& layout, StrView tabName) {
   if (auto* tab = layout.GetTab(tabName))
      return tab;
   Raise<std::runtime_error>(RevPrintTo<std::string>("MdRts:Not found tab=", tabName));
}
inline const seed::Field* GetFieldOrRaise(seed::Tab& tab, StrView fldName) {
   if (auto* fld = tab.Fields_.Get(fldName))
      return fld;
   Raise<std::runtime_error>(RevPrintTo<std::string>(
      "MdRts:Not found field=", fldName, "|tab=", tab.Name_));
}
/// 如果為 Null 表示: 不變動 dst;
static void BitvToDayTimeOrUnchange(DcQueue& rxbuf, DayTime& dst) {
   DayTime val{DayTime::Null()};
   BitvTo(rxbuf, val);
   if (!val.IsNull())
      dst = val;
}
//--------------------------------------------------------------------------//
struct RcSvStreamDecoderNote_MdRts : public RcSvStreamDecoderNote {
   DayTime  InfoTime_;
};

struct RcSvStreamDecoder_MdRts : public RcSvStreamDecoder {
   fon9_NON_COPY_NON_MOVE(RcSvStreamDecoder_MdRts);
   f9sv_TabSize         TabIdxBS_;
   f9sv_TabSize         TabIdxDeal_;
   f9sv_TabSize         TabIdxBase_;
   f9sv_TabSize         TabIdxRef_;

   const seed::Field*   FldBaseTDay_;
   const seed::Field*   FldBaseSession_;
   const seed::Field*   FldBaseSessionSt_;
   const seed::Field*   FldBaseMarket_;
   const seed::Field*   FldBaseFlowGroup_;
   const seed::Field*   FldBaseStrikePriceDiv_;

   const seed::Field*   FldPriRef_;
   const seed::Field*   FldPriUpLmt_;
   const seed::Field*   FldPriDnLmt_;

   const seed::Field*   FldDealInfoTime_;
   const seed::Field*   FldDealTime_;
   const seed::Field*   FldDealTotalQty_;
   const seed::Field*   FldDealPri_;
   const seed::Field*   FldDealQty_;
   const seed::Field*   FldDealBuyCnt_;
   const seed::Field*   FldDealSellCnt_;
   const seed::Field*   FldDealFlags_;

   struct FieldPQ {
      const seed::Field*   FldPri_;
      const seed::Field*   FldQty_;
   };
   using FieldPQList = std::vector<FieldPQ>;
   FieldPQList          FldOrderBuys_;
   FieldPQList          FldOrderSells_;
   FieldPQList          FldDerivedBuys_;
   FieldPQList          FldDerivedSells_;
   const seed::Field*   FldBSInfoTime_;
   const seed::Field*   FldBSFlags_;

   RcSvStreamDecoder_MdRts(svc::TreeRec& tree) {
      // 配合 MdRts 的編碼, 從 tree 取出必要欄位(tab + field);
      auto* tab = GetTabOrRaise(*tree.Layout_, "Deal");
      this->TabIdxDeal_      = static_cast<f9sv_TabSize>(tab->GetIndex());
      this->FldDealInfoTime_ = GetFieldOrRaise(*tab, "InfoTime");
      this->FldDealTime_     = GetFieldOrRaise(*tab, "DealTime");
      this->FldDealTotalQty_ = GetFieldOrRaise(*tab, "TotalQty");
      this->FldDealPri_      = GetFieldOrRaise(*tab, "DealPri");
      this->FldDealQty_      = GetFieldOrRaise(*tab, "DealQty");
      this->FldDealBuyCnt_   = GetFieldOrRaise(*tab, "DealBuyCnt");
      this->FldDealSellCnt_  = GetFieldOrRaise(*tab, "DealSellCnt");
      this->FldDealFlags_    = GetFieldOrRaise(*tab, "Flags");

      tab = GetTabOrRaise(*tree.Layout_, "BS");
      this->TabIdxBS_      = static_cast<f9sv_TabSize>(tab->GetIndex());
      this->FldBSInfoTime_ = GetFieldOrRaise(*tab, "InfoTime");
      this->FldBSFlags_    = GetFieldOrRaise(*tab, "Flags");
      GetBSFields(this->FldOrderBuys_, tab->Fields_, 'B', '\0');
      GetBSFields(this->FldOrderSells_, tab->Fields_, 'S', '\0');
      GetBSFields(this->FldDerivedBuys_, tab->Fields_, 'D', 'B');
      GetBSFields(this->FldDerivedSells_, tab->Fields_, 'D', 'S');

      tab = GetTabOrRaise(*tree.Layout_, "Base");
      this->TabIdxBase_            = static_cast<f9sv_TabSize>(tab->GetIndex());
      this->FldBaseTDay_           = GetFieldOrRaise(*tab, "TDay");
      this->FldBaseSession_        = GetFieldOrRaise(*tab, "Session");
      this->FldBaseSessionSt_      = GetFieldOrRaise(*tab, "SessionSt");
      this->FldBaseMarket_         = GetFieldOrRaise(*tab, "Market");
      this->FldBaseFlowGroup_      = GetFieldOrRaise(*tab, "FlowGroup");
      this->FldBaseStrikePriceDiv_ = GetFieldOrRaise(*tab, "StrikePriceDiv");

      tab = GetTabOrRaise(*tree.Layout_, "Ref");
      this->TabIdxRef_   = static_cast<f9sv_TabSize>(tab->GetIndex());
      this->FldPriRef_   = GetFieldOrRaise(*tab, "PriRef");
      this->FldPriUpLmt_ = GetFieldOrRaise(*tab, "PriUpLmt");
      this->FldPriDnLmt_ = GetFieldOrRaise(*tab, "PriDnLmt");
   }
   static void GetBSFields(FieldPQList& dst, const seed::Fields& flds, char ch1, char ch2) {
      dst.reserve(fmkt::SymbBS::kBSCount);
      NumOutBuf   nbuf;
      char* const pend = nbuf.end();
      // ch1 + ch2 + N + ('P' or 'Q'); e.g. "B1P", "B1Q"... "DB1P", "DB1Q";
      FieldPQ fld;
      for(unsigned L = 0;;) {
         char* pbeg = ToStrRev(pend - 1, ++L);
         if (ch2)
            *--pbeg = ch2;
         *--pbeg = ch1;
         StrView  fldName{pbeg, pend};
         *(pend - 1) = 'P';
         if ((fld.FldPri_ = flds.Get(fldName)) == nullptr)
            break;
         *(pend - 1) = 'Q';
         if ((fld.FldQty_ = flds.Get(fldName)) == nullptr)
            break;
         dst.push_back(fld);
      }
      dst.shrink_to_fit();
   }

   RcSvStreamDecoderNoteSP CreateDecoderNote() {
      return RcSvStreamDecoderNoteSP{new RcSvStreamDecoderNote_MdRts};
   }
   // -----
   void OnSubscribeStreamOK(svc::SubrRec& subr, StrView ack,
                            f9rc_ClientSession& ses, f9sv_ClientReport& rpt,
                            bool isNeedsLogResult) {
      // Pod snapshot;
      DcQueueFixedMem dcq{ack};
      auto fnOnHandler = subr.Seeds_[subr.TabIndex_]->Handler_.FnOnReport_;
      while (!dcq.empty()) {
         size_t tabidx = kTabAll;
         BitvTo(dcq, tabidx);
         auto* tab = subr.Tree_->Layout_->GetTab(tabidx);
         assert(tab != nullptr);
         if (tab == nullptr) {
            fon9_LOG_ERROR("OnSubscribeStreamOK.Snapshot"
                           "|ses=", ToPtr(&ses), 
                           "|tab=", tabidx,
                           "|err=Not found tab");
            return;
         }
         svc::SeedRec*     seed = subr.Seeds_[tabidx].get();
         seed::SimpleRawWr wr{seed};
         rpt.Seed_ = seed;
         rpt.Tab_ = &subr.Tree_->LayoutC_.TabList_[tabidx];
         size_t fldidx = 0;
         while (auto* fld = tab->Fields_.Get(fldidx++))
            fld->BitvToCell(wr, dcq);
         if (isNeedsLogResult)
            fon9_LOG_INFO("OnSubscribeStreamOK.Snapshot"
                          "|ses=", ToPtr(&ses),
                          "|tab=", tab->Name_);
         if (fnOnHandler)
            fnOnHandler(&ses, &rpt);
      }
   }
   void DecodeStreamData(svc::RxSubrData& rx, f9sv_ClientReport& rpt) {
      DcQueueFixedMem         rxbuf{rx.Gv_};
      const fmkt::RtsPackType pkType = ReadOrRaise<fmkt::RtsPackType>(rxbuf);
      if (fon9_UNLIKELY(rx.IsNeedsLog_))
         RevPrint(rx.LogBuf_, "|mdRts=", pkType);

      switch (pkType) {
      case fmkt::RtsPackType::DealPack:
         return this->DecodeDealPack(rx, rpt, rxbuf);
      case fmkt::RtsPackType::SnapshotBS:
         return this->DecodeSnapshotBS(rx, rpt, rxbuf, fmkt::BSFlag{});
      case fmkt::RtsPackType::CalculatedBS:
         return this->DecodeSnapshotBS(rx, rpt, rxbuf, fmkt::BSFlag::Calculated);
      case fmkt::RtsPackType::UpdateBS:
         return this->DecodeUpdateBS(rx, rpt, rxbuf);
      case fmkt::RtsPackType::TradingSessionSt:
         return this->DecodeTradingSessionSt(rx, rpt, rxbuf);
      case fmkt::RtsPackType::BaseInfoTwf:
         return this->DecodeBaseInfoTwf(rx, rpt, rxbuf);
      }
   }
   // -----
   struct DecoderAux {
      fon9_NON_COPY_NON_MOVE(DecoderAux);
      RcSvStreamDecoderNote_MdRts*  Note_{nullptr};
      svc::SeedRec*                 SeedRec_;
      seed::SimpleRawWr             RawWr_;
      DecoderAux(svc::RxSubrData& rx, f9sv_ClientReport& rpt, f9sv_TabSize tabidx)
         : SeedRec_{rx.SubrRec_->Seeds_[tabidx].get()}
         , RawWr_{*SeedRec_} {
         rpt.Seed_ = this->SeedRec_;
         rpt.Tab_ = &rx.SubrRec_->Tree_->LayoutC_.TabList_[tabidx];
      }
      DecoderAux(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf, f9sv_TabSize tabidx)
         : DecoderAux{rx, rpt, tabidx} {
         assert(dynamic_cast<RcSvStreamDecoderNote_MdRts*>(rx.SeedRec_->StreamDecoderNote_.get()) != nullptr);
         this->Note_ = static_cast<RcSvStreamDecoderNote_MdRts*>(rx.SeedRec_->StreamDecoderNote_.get());
         BitvToDayTimeOrUnchange(rxbuf, this->Note_->InfoTime_);
      }
   };
   // -----
   void DecodeDealPack(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      DecoderAux  dec{rx, rpt, rxbuf, this->TabIdxDeal_};
      DayTime     dealTime = dec.Note_->InfoTime_;
      this->FldDealInfoTime_->PutNumber(dec.RawWr_, dealTime.GetOrigValue(), dealTime.Scale);

      fmkt::DealFlag dealFlags = ReadOrRaise<fmkt::DealFlag>(rxbuf);
      if (IsEnumContains(dealFlags, fmkt::DealFlag::DealTimeChanged)) {
         BitvToDayTimeOrUnchange(rxbuf, dealTime);
         this->FldDealTime_->PutNumber(dec.RawWr_, dealTime.GetOrigValue(), dealTime.Scale);
      }

      // 取得現在的 totalQty, 試算階段不提供 totalQty, 所以將 pTotalQty 設為 nullptr, 表示不更新.
      fmkt::Qty  totalQty = unsigned_cast(this->FldDealTotalQty_->GetNumber(dec.RawWr_, 0, 0));
      fmkt::Qty* pTotalQty = IsEnumContains(dealFlags, fmkt::DealFlag::Calculated) ? nullptr : &totalQty;
      if (IsEnumContains(dealFlags, fmkt::DealFlag::TotalQtyLost)) {
         BitvTo(rxbuf, totalQty);
         if (pTotalQty == nullptr)
            this->FldDealTotalQty_->PutNumber(dec.RawWr_, static_cast<seed::FieldNumberT>(totalQty), 0);
      }

      auto     fnOnReport = rx.SeedRec_->Handler_.FnOnReport_;
      unsigned count = ReadOrRaise<byte>(rxbuf);
      if (rx.IsNeedsLog_) {
         RevPrint(rx.LogBuf_, "|rtDeal=", ToHex(dealFlags), "|count=", count + 1);
         rx.FlushLog();
      }
      if (count > 0) {
         this->FldDealFlags_->PutNumber(dec.RawWr_, static_cast<seed::FieldNumberT>(
            dealFlags - (fmkt::DealFlag::DealBuyCntChanged | fmkt::DealFlag::DealSellCntChanged)), 0);
         this->DecodeDealPQ(rxbuf, dec.RawWr_, pTotalQty);
         if (fnOnReport)
            fnOnReport(&rx.Session_, &rpt);
         dealFlags -= (fmkt::DealFlag::TotalQtyLost | fmkt::DealFlag::DealTimeChanged);
         if (--count > 0) {
            this->FldDealFlags_->PutNumber(dec.RawWr_, static_cast<seed::FieldNumberT>(
               dealFlags - (fmkt::DealFlag::DealBuyCntChanged | fmkt::DealFlag::DealSellCntChanged)), 0);
            do {
               this->DecodeDealPQ(rxbuf, dec.RawWr_, pTotalQty);
               if (fnOnReport)
                  fnOnReport(&rx.Session_, &rpt);
            } while (--count > 0);
         }
      }
      this->FldDealFlags_->PutNumber(dec.RawWr_, static_cast<seed::FieldNumberT>(dealFlags), 0);
      this->DecodeDealPQ(rxbuf, dec.RawWr_, pTotalQty);
      this->DecodeDealCnt(rxbuf, dec.RawWr_, dealFlags);
      assert(rxbuf.empty());
      if (fnOnReport)
         fnOnReport(&rx.Session_, &rpt);
   }
   void DecodeDealPQ(DcQueue& rxbuf, seed::SimpleRawWr& dealwr, fmkt::Qty* pTotalQty) {
      this->FldDealPri_->BitvToCell(dealwr, rxbuf);
      fmkt::Qty dealQty{0};
      BitvTo(rxbuf, dealQty);
      this->FldDealQty_->PutNumber(dealwr, static_cast<seed::FieldNumberT>(dealQty), 0);
      if (pTotalQty) {
         *pTotalQty += dealQty;
         this->FldDealTotalQty_->PutNumber(dealwr, static_cast<seed::FieldNumberT>(*pTotalQty), 0);
      }
   }
   void DecodeDealCnt(DcQueue& rxbuf, seed::SimpleRawWr& dealwr, const fmkt::DealFlag dealFlags) {
      if (IsEnumContains(dealFlags, fmkt::DealFlag::DealBuyCntChanged))
         this->FldDealBuyCnt_->BitvToCell(dealwr, rxbuf);
      if (IsEnumContains(dealFlags, fmkt::DealFlag::DealSellCntChanged))
         this->FldDealSellCnt_->BitvToCell(dealwr, rxbuf);
   }
   // -----
   static void ClearFieldValues(const seed::RawWr& wr, FieldPQList::const_iterator ibeg, FieldPQList::const_iterator iend) {
      for (; ibeg != iend; ++ibeg) {
         ibeg->FldPri_->SetNull(wr);
         ibeg->FldQty_->SetNull(wr);
      }
   }
   static void RevPrintPQ(RevBuffer& rbuf, const FieldPQ& fld, const seed::RawRd& rd) {
      fld.FldQty_->CellRevPrint(rd, nullptr, rbuf);
      RevPrint(rbuf, '*');
      fld.FldPri_->CellRevPrint(rd, nullptr, rbuf);
   }
   void DecodeSnapshotBS(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf, fmkt::BSFlag bsFlags) {
      DecoderAux dec(rx, rpt, rxbuf, this->TabIdxBS_);
      this->FldBSInfoTime_->PutNumber(dec.RawWr_, dec.Note_->InfoTime_.GetOrigValue(), dec.Note_->InfoTime_.Scale);
      const FieldPQList* flds;
      while (!rxbuf.empty()) {
         const uint8_t first = ReadOrRaise<uint8_t>(rxbuf);
         switch (static_cast<fmkt::RtBSType>(first & cast_to_underlying(fmkt::RtBSType::Mask))) {
         #define case_RtBSType(type) case fmkt::RtBSType::type: \
            flds = &this->Fld##type##s_; \
            bsFlags |= fmkt::BSFlag::type; \
            break
         // -----
         case_RtBSType(OrderBuy);
         case_RtBSType(OrderSell);
         case_RtBSType(DerivedBuy);
         case_RtBSType(DerivedSell);
         default:
            assert(!"Unknown RtBSType.");
            return;
         }
         assert(!flds->empty());
         unsigned count = static_cast<unsigned>(first & 0x0f) + 1;
         auto     ibeg = flds->cbegin() + count;
         this->ClearFieldValues(dec.RawWr_, ibeg, flds->cend());
         if (fon9_UNLIKELY(rx.IsNeedsLog_))
            RevPrint(rx.LogBuf_, '}');
         do {
            --ibeg;
            ibeg->FldPri_->BitvToCell(dec.RawWr_, rxbuf);
            ibeg->FldQty_->BitvToCell(dec.RawWr_, rxbuf);
            if (fon9_UNLIKELY(rx.IsNeedsLog_)) {
               RevPrintPQ(rx.LogBuf_, *ibeg, dec.RawWr_);
               if (count > 1)
                  RevPrint(rx.LogBuf_, '|');
            }
         } while (--count > 0);
         if (fon9_UNLIKELY(rx.IsNeedsLog_))
            RevPrint(rx.LogBuf_, "|rtBS.", ToHex(first), "={");
      }
      // -----
      #define check_ClearFieldValues(type) \
      while (!IsEnumContains(bsFlags, fmkt::BSFlag::type)) { \
         this->ClearFieldValues(dec.RawWr_, this->Fld##type##s_.cbegin(), this->Fld##type##s_.cend()); \
         break; \
      }
      // -----
      check_ClearFieldValues(OrderBuy);
      check_ClearFieldValues(OrderSell);
      check_ClearFieldValues(DerivedBuy);
      check_ClearFieldValues(DerivedSell);
      // -----
      this->ReportBS(rx, rpt, dec.RawWr_, bsFlags);
   }
   void ReportBS(svc::RxSubrData& rx, f9sv_ClientReport& rpt, const seed::RawWr& wr, fmkt::BSFlag bsFlags) {
      this->FldBSFlags_->PutNumber(wr, static_cast<seed::FieldNumberT>(bsFlags), 0);
      this->ReportEv(rx, rpt);
   }
   void ReportEv(svc::RxSubrData& rx, f9sv_ClientReport& rpt) {
      if (rx.IsNeedsLog_)
         rx.FlushLog();
      if (auto fnOnReport = rx.SeedRec_->Handler_.FnOnReport_)
         fnOnReport(&rx.Session_, &rpt);
   }
   // -----
   static void CopyPQ(const seed::RawWr& wr, const FieldPQ& dst, const FieldPQ& src) {
      const auto srcPriNull = src.FldPri_->GetNullValue();
      const auto srcPriScale = src.FldPri_->DecScale_;
      dst.FldPri_->PutNumber(wr, src.FldPri_->GetNumber(wr, srcPriScale, srcPriNull), srcPriScale);
      dst.FldQty_->PutNumber(wr, src.FldQty_->GetNumber(wr, 0, 0), 0);
   }
   static void DeletePQ(const seed::RawWr& wr, FieldPQList::const_iterator ibeg, FieldPQList::const_iterator iend) {
      assert(ibeg != iend);
      const FieldPQ* prev = &*ibeg;
      // 刪除 ibeg: ibeg 之後往前移動; 清除 iend-1;
      while (++ibeg != iend) {
         const FieldPQ* curr = &*ibeg;
         CopyPQ(wr, *prev, *curr);
         prev = curr;
      }
      prev->FldPri_->SetNull(wr);
      prev->FldQty_->SetNull(wr);
   }
   static void InsertPQ(const seed::RawWr& wr, FieldPQList::const_iterator ibeg, FieldPQList::const_iterator iend) {
      while (--iend != ibeg)
         CopyPQ(wr, *iend, *(iend - 1));
   }
   void DecodeUpdateBS(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      DecoderAux dec(rx, rpt, rxbuf, this->TabIdxBS_);
      this->FldBSInfoTime_->PutNumber(dec.RawWr_, dec.Note_->InfoTime_.GetOrigValue(), dec.Note_->InfoTime_.Scale);
      const uint8_t first = ReadOrRaise<uint8_t>(rxbuf);
      fmkt::BSFlag  bsFlags{};
      if (first & 0x80)
         bsFlags |= fmkt::BSFlag::Calculated;
      unsigned           count = (first & 0x7fu);
      const FieldPQList* flds;
      for (;;) {
         const uint8_t bsType = ReadOrRaise<uint8_t>(rxbuf);
         switch (static_cast<fmkt::RtBSType>(bsType & cast_to_underlying(fmkt::RtBSType::Mask))) {
         case_RtBSType(OrderBuy);
         case_RtBSType(OrderSell);
         case_RtBSType(DerivedBuy);
         case_RtBSType(DerivedSell);
         default:
            assert(!"Unknown RtBSType.");
            return;
         }
         assert((bsType & 0x0fu) < flds->size());
         if ((bsType & 0x0fu) >= flds->size()) {
            fon9_LOG_ERROR("DecodeUpdateBS|err=Bad level|rtBS=", ToHex(bsType));
            return;
         }
         const auto ibeg = flds->cbegin() + (bsType & 0x0f);
         switch (static_cast<fmkt::RtBSAction>(bsType & cast_to_underlying(fmkt::RtBSAction::Mask))) {
         case fmkt::RtBSAction::New:
            InsertPQ(dec.RawWr_, ibeg, flds->cend());
            // 不用 break; 取出 rxbuf 裡面的 Pri,Qty; 填入 ibeg;
         case fmkt::RtBSAction::ChangePQ:
            ibeg->FldPri_->BitvToCell(dec.RawWr_, rxbuf);
            ibeg->FldQty_->BitvToCell(dec.RawWr_, rxbuf);
            if (fon9_UNLIKELY(rx.IsNeedsLog_))
               RevPrintPQ(rx.LogBuf_, *ibeg, dec.RawWr_);
            break;
         case fmkt::RtBSAction::ChangeQty:
            ibeg->FldQty_->BitvToCell(dec.RawWr_, rxbuf);
            if (fon9_UNLIKELY(rx.IsNeedsLog_))
               ibeg->FldQty_->CellRevPrint(dec.RawWr_, nullptr, rx.LogBuf_);
            break;
         case fmkt::RtBSAction::Delete:
            DeletePQ(dec.RawWr_, ibeg, flds->cend());
            break;
         default:
            assert(!"Unknown RtBSAction.");
            fon9_LOG_ERROR("DecodeUpdateBS|err=Unknown RtBSAction|rtBS=", ToHex(bsType));
            return;
         }
         if (fon9_UNLIKELY(rx.IsNeedsLog_))
            RevPrint(rx.LogBuf_, "|rtBS.", ToHex(bsType), '=');
         if (count <= 0)
            break;
         --count;
      }
      assert(count == 0 && rxbuf.empty());
      // -----
      this->ReportBS(rx, rpt, dec.RawWr_, bsFlags);
   }
   // -----
   void DecodeTradingSessionSt(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      DecoderAux  dec(rx, rpt, rxbuf, this->TabIdxBase_);
      uint32_t    tdayYYYYMMDD = ReadOrRaise<uint32_t>(rxbuf);
      char        sesId = cast_to_underlying(ReadOrRaise<f9fmkt_TradingSessionId>(rxbuf));
      auto        sesSt = ReadOrRaise<f9fmkt_TradingSessionSt>(rxbuf);
      this->FldBaseTDay_->PutNumber(dec.RawWr_, tdayYYYYMMDD, 0);
      this->FldBaseSession_->StrToCell(dec.RawWr_, StrView{&sesId, 1});
      this->FldBaseSessionSt_->PutNumber(dec.RawWr_, sesSt, 0);
      assert(rxbuf.empty());
      this->ReportEv(rx, rpt);
   }
   void DecodeBaseInfoTwf(svc::RxSubrData& rx, f9sv_ClientReport& rpt, DcQueue& rxbuf) {
      DecoderAux  decBase(rx, rpt, rxbuf, this->TabIdxBase_);
      char        chTradingMarket = cast_to_underlying(ReadOrRaise<f9fmkt_TradingMarket>(rxbuf));
      uint8_t     u8FlowGroup     = ReadOrRaise<uint8_t>(rxbuf);
      uint8_t     u8StrikePriceDecimalLocator = ReadOrRaise<uint8_t>(rxbuf);
      this->FldBaseMarket_->StrToCell(decBase.RawWr_, StrView{&chTradingMarket, 1});
      this->FldBaseFlowGroup_->PutNumber(decBase.RawWr_, u8FlowGroup, 0);
      this->FldBaseStrikePriceDiv_->PutNumber(decBase.RawWr_, static_cast<seed::FieldNumberT>(fon9::GetDecDivisor(u8StrikePriceDecimalLocator)), 0);
      this->ReportEv(rx, rpt);

      DecoderAux  decRef(rx, rpt, this->TabIdxRef_);
      this->FldPriRef_->BitvToCell(decRef.RawWr_, rxbuf);
      this->FldPriUpLmt_->BitvToCell(decRef.RawWr_, rxbuf);
      this->FldPriDnLmt_->BitvToCell(decRef.RawWr_, rxbuf);
      assert(rxbuf.empty());
      this->ReportEv(rx, rpt);
   }
};

} } // namespaces
//--------------------------------------------------------------------------//
extern "C" {

f9sv_CAPI_FN(f9sv_Result) f9sv_AddStreamDecoder_MdRts(void) {
   using namespace fon9::rc;
   struct Factory : public RcSvStreamDecoderFactory {
      RcSvStreamDecoderSP CreateStreamDecoder(svc::TreeRec& tree) override {
         return RcSvStreamDecoderSP{new RcSvStreamDecoder_MdRts{tree}};
      }
   };
   static Factory factory;
   RcSvStreamDecoderPark::Register("MdRts", &factory);
   return f9sv_Result_NoError;
}

} // extern "C"
