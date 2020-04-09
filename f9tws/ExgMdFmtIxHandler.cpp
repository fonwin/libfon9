// \file f9tws/ExgMdFmtIxHandler.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdFmtIxHandler.hpp"
#include "f9tws/ExgMdFmtIxNoList.hpp"

namespace f9tws {
namespace f9fmkt = fon9::fmkt;
//#define DEBUG_PRINT_IDX

ExgMdFmt21Handler::~ExgMdFmt21Handler() {
}
void ExgMdFmt21Handler::OnPkReceived(const ExgMdHeader& pkhdr, unsigned pksz) {
   const ExgMdFmt21& fmt21 = *static_cast<const ExgMdFmt21*>(&pkhdr);
   {
      ExgMdIndices&  ixs = *this->MdSys_.Indices_;
      auto           ixsLk{ixs.SymbMap_.Lock()};
      ExgMdIndex&    ix = *static_cast<ExgMdIndex*>(ixs.FetchSymb(ixsLk, ToStrView(fmt21.IdxNo_)).get());
      f9fmkt_TradingMarket mkt;
      switch (pkhdr.GetMarket()) {
      case ExgMdMarket_TwTSE: mkt = f9fmkt_TradingMarket_TwSEC; break;
      case ExgMdMarket_TwOTC: mkt = f9fmkt_TradingMarket_TwOTC; break;
      default: return;
      }
      if (ix.TradingMarket_ == mkt)
         return;
      ix.TradingMarket_ = mkt;
   } // auto unlock.
   this->MdSys_.BaseInfoPkLog(pkhdr, pksz);

#ifdef DEBUG_PRINT_IDX
   puts(fon9::RevPrintTo<std::string>(
      '"', fon9::StrView_all(fmt21.IdxNo_.Chars_), "\", // ", fmt21.GetMarket(),
      '/', fon9::PackBcdTo<uint8_t>(fmt21.FormatCode_),
      '/', fon9::StrView_all(fmt21.NameBig5_),
      '/', fon9::StrView_all(fmt21.NameEng_)
      ).c_str());
#endif
}
//--------------------------------------------------------------------------//
// 指數時間及傳輸序號皆為 0 表示開盤前傳送昨日收盤指數。
// 指數時間為 999999 表示傳送今日收盤指數。收盤指數資料將以每 5 秒鐘之傳送間隔，繼續傳送約 15 分鐘。
static fon9::DayTime ToIndexTime(const fon9::PackBcd<6>& pIdxHHMMSS) {
   const auto idxHHMMSS = fon9::PackBcdTo<unsigned>(pIdxHHMMSS);
   return (idxHHMMSS == 0 ? f9fmkt::YesterdayIndexTime()
           : idxHHMMSS == 999999 ? f9fmkt::ClosedIndexTime()
           : fon9::TimeInterval_HHMMSS(idxHHMMSS));
}

struct ExgMdIndexUpdater {
   fon9_NON_COPY_NON_MOVE(ExgMdIndexUpdater);
   ExgMdIndices&        Indicas_;
   ExgMdIndices::Locker IxsLk_;
   const fon9::DayTime  IdxTime_;
   ExgMdIndexUpdater(ExgMdSystem& mdsys, const fon9::PackBcd<6>& pIdxHHMMSS)
      : Indicas_(*mdsys.Indices_)
      , IxsLk_{Indicas_.SymbMap_.Lock()}
      , IdxTime_{ToIndexTime(pIdxHHMMSS)} {
   }
   ExgMdIndex* Update(const IdxNo& idxNo, const IdxValueV2& valueV2, fon9::RevBuffer* gbuf = nullptr) {
      ExgMdIndex& ix = *static_cast<ExgMdIndex*>(this->Indicas_.FetchSymb(this->IxsLk_, ToStrView(idxNo)).get());
      if (this->IdxTime_ < ix.Deal_.Data_.DealTime_)
         return nullptr;
      const auto        idxValueV2 = fon9::PackBcdTo<uint32_t>(valueV2);
      const f9fmkt::Pri idxValuePri = f9fmkt::Pri::Make<2>(idxValueV2);
      if (this->IdxTime_ == ix.Deal_.Data_.DealTime_ && idxValuePri == ix.Deal_.Data_.DealPri_)
         return nullptr;
      ix.Deal_.Data_.DealTime_ = this->IdxTime_;
      ix.Deal_.Data_.DealPri_ = idxValuePri;
      if (this->IdxTime_ != f9fmkt::YesterdayIndexTime()) {
         ix.High_.CheckHigh(idxValuePri, this->IdxTime_);
         ix.Low_.CheckLow(idxValuePri, this->IdxTime_);
      }
      fon9::RevBufferList rts{128};
      fon9::ToBitv(rts, idxValueV2);
      ix.MdRtStream_.Publish(this->Indicas_, ToStrView(ix.SymbId_),
                             f9fmkt::RtsPackType::IndexValueV2,
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
void ExgMdFmtIxHandler::OnPkReceived(const ExgMdHeader& pkhdr, unsigned pksz) {
   (void)pksz;
   const ExgMdFmtIx& fmtIx = *static_cast<const ExgMdFmtIx*>(&pkhdr);
   ExgMdIndexUpdater updater{this->MdSys_, fmtIx.IdxHHMMSS_};
   if (auto* ix = updater.Update(fmtIx.IdxNo_, fmtIx.IdxValueV2_)) {
      (void)ix;
      #ifdef DEBUG_PRINT_IDX
         puts(fon9::RevPrintTo<std::string>(
            "[", fmtIx.GetMarket(),
            "][", fon9::StrView_all(fmtIx.IdxNo_.Chars_),
            "][", ix->Deal_.Data_.DealTime_,
            "][", ix->Deal_.Data_.DealPri_, fon9::FmtDef{8,2}, "]"
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
void ExgMdFmt3Handler::OnPkReceived(const ExgMdHeader& pkhdr, unsigned pksz) {
   (void)pksz;
   const ExgMdFmt3&  fmt3 = *static_cast<const ExgMdFmt3*>(&pkhdr);
   ExgMdIndexUpdater updater{this->MdSys_, fmt3.IdxHHMMSS_};

   ExgMdIndices::BlockPublish bpub(updater.Indicas_,
                                   static_cast<fon9::BufferNodeSize>(this->IdxNoCount_ * 15));
   fon9::RevBufferList*       gbuf = bpub.GetPublishBuffer();
   if (updater.Update(this->IdxNoList_[0], fmt3.IndicesV2_[0], gbuf) == nullptr)
      return;
   for (size_t L = 1; L < this->IdxNoCount_; ++L) {
      if (this->IdxNoList_[L].Chars_[0] != ' ')
         updater.Update(this->IdxNoList_[L], fmt3.IndicesV2_[L], gbuf);
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
   fon9::ToBitv(*gbuf, updater.IdxTime_);
   *(gbuf->AllocPacket<uint8_t>()) = fon9::cast_to_underlying(f9fmkt::RtsPackType::IndexValueV2List);
   f9fmkt::MdRtsNotifyArgs  e(updater.Indicas_, fon9::seed::TextBegin(),
                              f9fmkt::GetMdRtsKind(f9fmkt::RtsPackType::IndexValueV2List),
                              *gbuf);
   bpub.UnsafePublish(f9fmkt::RtsPackType::IndexValueV2List, e);
}

} // namespaces
