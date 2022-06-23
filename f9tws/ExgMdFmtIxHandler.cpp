// \file f9tws/ExgMdFmtIxHandler.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdFmtIxHandler.hpp"
#include "f9tws/ExgMdFmtIxNoList.hpp"
#include "fon9/Tools.hpp"

namespace f9tws {
namespace f9fmkt = fon9::fmkt;
//#define DEBUG_PRINT_IDX

ExgMdFmt21Handler::~ExgMdFmt21Handler() {
}
void ExgMdFmt21Handler::OnPkReceived(const ExgMdHead& pkhdr, unsigned pksz) {
   const ExgMdFmt21& fmt21 = *static_cast<const ExgMdFmt21*>(&pkhdr);
   {
      ExgMdIndices&  ixs = *TwsMdSys(*this).Indices_;
      auto           ixsLk{ixs.SymbMap_.Lock()};
      ExgMdIndex&    ix = *static_cast<ExgMdIndex*>(ixs.FetchSymb(ixsLk, ToStrView(fmt21.IdxNo_)).get());
      f9fmkt_TradingMarket mkt;
      switch (pkhdr.GetMarket()) {
      case ExgMdMarket_TwTSE: mkt = f9fmkt_TradingMarket_TwSEC; break;
      case ExgMdMarket_TwOTC: mkt = f9fmkt_TradingMarket_TwOTC; break;
      default: return;
      }
      fon9::RevBufferList rbuf{128};
      f9sv_MdRtsKind      mdKind = f9sv_MdRtsKind{};
      fon9::fmkt::Pri pclose{fon9::PackBcdTo<uint64_t>(fmt21.YesterdayClosingIndexV2_), 2};
      if (ix.Ref_.Data_.PriRef_ != pclose) {
         mdKind |= f9sv_MdRtsKind_Ref;
         ix.Ref_.Data_.PriRef_ = pclose;
         fon9::fmkt::MdRtsPackFieldValue(rbuf,
                                         *ixs.LayoutSP_->GetTab(fon9_kCSTR_TabName_Ref)->Fields_.Get("PriRef"),
                                         fon9::seed::SimpleRawRd{ix.Ref_});
      }
      if (ix.TradingMarket_ != mkt || ix.NameUTF8_.Chars_[0] == '\0') {
         mdKind |= f9sv_MdRtsKind_Base;
         fon9::seed::SimpleRawRd rdBase{ix};
         auto* tabBase = ixs.LayoutSP_->GetTab(fon9_kCSTR_TabName_Base);
         if (ix.TradingMarket_ != mkt) {
            ix.TradingMarket_ = mkt;
            fon9::fmkt::MdRtsPackFieldValue(rbuf, *tabBase->Fields_.Get("Market"), rdBase);
         }
         if (ix.NameUTF8_.Chars_[0] == '\0') {
            fon9::Big5ToUtf8EOS(fon9::StrView_eos_or_all(fmt21.NameBig5_, ' '),
                                ix.NameUTF8_.Chars_, sizeof(ix.NameUTF8_));
            fon9::fmkt::MdRtsPackFieldValue(rbuf, *tabBase->Fields_.Get("NameUTF8"), rdBase);
         }
      }
      if (mdKind == f9sv_MdRtsKind{})
         return;
      ix.MdRtStream_.PublishAndSave(ToStrView(ix.SymbId_),
                                    f9sv_RtsPackType_FieldValue_NoInfoTime,
                                    mdKind | f9sv_MdRtsKind_NoInfoTime,
                                    std::move(rbuf));
   #ifdef DEBUG_PRINT_IDX
      puts(fon9::RevPrintTo<std::string>(
         '"', fon9::StrView_all(fmt21.IdxNo_.Chars_), "\": ", fmt21.GetMarket(),
         '/', ix.Ref_.Data_.PriRef_, fon9::FmtDef{8,2},
         '/', fon9::PackBcdTo<uint8_t>(fmt21.FormatCode_),
         '/', ix.NameUTF8_, // fon9::StrView_all(fmt21.NameBig5_),
         '/', fon9::StrView_all(fmt21.NameEng_)
         ).c_str());
   #endif
   } // auto unlock.
   TwsMdSys(*this).BaseInfoPkLog(pkhdr, pksz);
}
//--------------------------------------------------------------------------//
struct ExgMdIndexUpdater {
   fon9_NON_COPY_NON_MOVE(ExgMdIndexUpdater);
   ExgMdIndices&                 Indicas_;
   ExgMdIndices::Locker          IxsLk_;
   const fon9::DayTime           IdxTime_;
   const f9fmkt::MarketDataSeq   MktSeq_;
   char                          Padding____[4];
   ExgMdIndexUpdater(ExgMdSystem& mdsys, const fon9::PackBcd<6>& pIdxHHMMSS, f9fmkt::MarketDataSeq mktseq)
      : Indicas_(*mdsys.Indices_)
      , IxsLk_{Indicas_.SymbMap_.Lock()}
      , IdxTime_{f9fmkt::ExgMdIndexToDayTime(fon9::PackBcdTo<unsigned>(pIdxHHMMSS))}
      , MktSeq_{mktseq} {
   }
   ExgMdIndex* Update(const IdxNo& idxNo, const IdxValueV2& valueV2, fon9::RevBuffer* gbuf = nullptr) {
      ExgMdIndex& ix = *static_cast<ExgMdIndex*>(this->Indicas_.FetchSymb(this->IxsLk_, ToStrView(idxNo)).get());
      if (this->IdxTime_ < ix.Deal_.Data_.Time_)
         return nullptr;
      const auto        idxValueV2  = fon9::PackBcdTo<uint32_t>(valueV2);
      const f9fmkt::Pri idxValuePri = f9fmkt::Pri::Make<2>(idxValueV2);
      if (this->IdxTime_ == ix.Deal_.Data_.Time_ && idxValuePri == ix.Deal_.Data_.Pri_)
         return nullptr;
      ix.Deal_.Data_.Time_ = this->IdxTime_;
      ix.Deal_.Data_.Pri_  = idxValuePri;
      if (this->IdxTime_ != f9fmkt::YesterdayIndexTime())
         ix.CheckOHL(idxValuePri, this->IdxTime_);
      fon9::RevBufferList rts{128};
      fon9::ToBitv(rts, idxValueV2);
      f9fmkt::PackMktSeq(rts, this->Indicas_.CtrlFlags_, ix.Deal_.Data_.MarketSeq_ = this->MktSeq_);
      ix.MdRtStream_.Publish(ToStrView(ix.SymbId_),
                             f9sv_RtsPackType_IndexValueV2,
                             f9sv_MdRtsKind_Deal,
                             this->IdxTime_,
                             std::move(rts));
      if (gbuf) {
         fon9::ToBitv(*gbuf, idxValueV2);
         fon9::ToBitv(*gbuf, ix.SymbId_);
      }
      return &ix;
   }
};
//--------------------------------------------------------------------------//
ExgMdFmtIxHandler::~ExgMdFmtIxHandler() {
}
void ExgMdFmtIxHandler::OnPkReceived(const ExgMdHead& pkhdr, unsigned pksz) {
   (void)pksz;
   const ExgMdFmtIx& fmtIx = *static_cast<const ExgMdFmtIx*>(&pkhdr);
   ExgMdIndexUpdater updater{TwsMdSys(*this), fmtIx.IdxHHMMSS_, fmtIx.GetSeqNo()};
   if (auto* ix = updater.Update(fmtIx.IdxNo_, fmtIx.IdxValueV2_)) {
      (void)ix;
      #ifdef DEBUG_PRINT_IDX
         puts(fon9::RevPrintTo<std::string>(
            "[", fmtIx.GetMarket(),
            "][", fon9::StrView_all(fmtIx.IdxNo_.Chars_),
            "][", ix->Deal_.Data_.Time_,
            "][", ix->Deal_.Data_.Pri_, fon9::FmtDef{8,2}, "]"
            ).c_str());
      #endif
   }
}
//--------------------------------------------------------------------------//
ExgMdHandlerSP ExgMdFmt3Handler::MakeTwse(ExgMdSystem& mdsys) {
   return ExgMdHandlerSP{new ExgMdFmt3Handler{mdsys, TwseIxNoList_}};
}
ExgMdHandlerSP ExgMdFmt3Handler::MakeTpex(ExgMdSystem& mdsys) {
   return ExgMdHandlerSP{new ExgMdFmt3Handler{mdsys, TpexIxNoList_}};
}

ExgMdFmt3Handler::~ExgMdFmt3Handler() {
}
void ExgMdFmt3Handler::Initialize() {
   // 建立 Handler 時, 可能會將 MakeTwse(), MakeTpex() 同時加入,
   // 但實際應用可能會區分 MdSysTwse, MdSysTpex, 如果在此建立 FetchSymb();
   // 則會造成 MdSysTwse 有 Tpex 的指數代號, MdSysTpex 有 Twse 的指數代號;
   // 這樣會造成困惑, 所以在此不要 FetchSymb();
   // ExgMdIndices& ixs = *this->MdSys_.Indices_;
   // auto          ixsLk{ixs.SymbMap_.Lock()};
   // for (auto L = this->IdxNoCount_; L > 0;)
   //    ixs.FetchSymb(ixsLk, ToStrView(this->IdxNoList_[--L]));
}
void ExgMdFmt3Handler::OnPkReceived(const ExgMdHead& pkhdr, unsigned pksz) {
   (void)pksz;
   const ExgMdFmt3&  fmt3 = *static_cast<const ExgMdFmt3*>(&pkhdr);
   ExgMdIndexUpdater updater{TwsMdSys(*this), fmt3.IdxHHMMSS_, fmt3.GetSeqNo()};

   ExgMdIndices::BlockPublish bpub(updater.Indicas_,
                                   static_cast<fon9::BufferNodeSize>(this->IdxNoCount_ * 15));
   fon9::RevBufferList*       gbuf = bpub.GetPublishBuffer();
   bool                       is1stUpdated = true;
   for (size_t L = 0; L < this->IdxNoCount_; ++L) {
      if (this->IdxNoList_[L].Chars_[0] != ' ') {
         if (updater.Update(this->IdxNoList_[L], fmt3.IndicesV2_[L], gbuf))
            is1stUpdated = false;
         else if (is1stUpdated)
            return;
      }
   }

#ifdef DEBUG_PRINT_IDX
   puts(fon9::RevPrintTo<std::string>('[', fmt3.GetMarket(), "] ------  ", updater.IdxTime_).c_str());
   for (size_t L = 0; L < this->IdxNoCount_; ++L) {
      puts(fon9::RevPrintTo<std::string>(
         "[-][", fon9::StrView_all(this->IdxNoList_[L].Chars_), "][",
         fon9::PackBcdTo<uint32_t>(fmt3.IndicesV2_[L]), fon9::FmtDef{8}, "]"
         ).c_str());
   }
#endif

   if (gbuf == nullptr)
      return;
   f9fmkt::PackMktSeq(*gbuf, updater.Indicas_.CtrlFlags_, updater.MktSeq_);
   fon9::ToBitv(*gbuf, updater.IdxTime_);
   *(gbuf->AllocPacket<uint8_t>()) = fon9::cast_to_underlying(f9sv_RtsPackType_IndexValueV2List);
   f9fmkt::MdRtsNotifyArgs  e(updater.Indicas_, fon9::seed::TextBegin(), f9sv_MdRtsKind_Deal, *gbuf);
   bpub.UnsafePublish(f9sv_RtsPackType_IndexValueV2List, e);
}

} // namespaces
