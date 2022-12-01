// \file f9tws/ExgMdFmt9Handler.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdFmt9Handler.hpp"
#include "f9tws/ExgMdFmt9.hpp"

namespace f9tws {
namespace f9fmkt = fon9::fmkt;

ExgMdFmt9Handler::~ExgMdFmt9Handler() {
}
void ExgMdFmt9Handler::OnPkReceived(const ExgMdHead& pkhdr, unsigned pksz) {
   const ExgMdFmt9& fmt9 = *static_cast<const ExgMdFmt9*>(&pkhdr);
   const unsigned   dealHHMMSS = fon9::PackBcdTo<unsigned>(fmt9.DealHHMMSS_);
   if (dealHHMMSS == 999999)
      return;
   const fon9::DayTime  dealTime = fon9::TimeInterval_HHMMSS(dealHHMMSS);

   auto  symblk = TwsMdSys(*this).Symbs_->SymbMap_.Lock();
   auto  symb = fon9::static_pointer_cast<ExgMdSymb>(
      TwsMdSys(*this).Symbs_->FetchSymb(symblk, ToStrView(fmt9.StkNo_)));
   if (dealTime == symb->Deal_.Data_.DealTime_)
      return;

   symb->Deal_.Data_.DealTime_ = symb->Deal_.Data_.InfoTime_ = dealTime;
   symb->Deal_.Data_.Flags_ = f9sv_DealFlag_DealTimeChanged;
   fmt9.DealPQ_.AssignTo(symb->Deal_.Data_.Deal_);
   symb->Deal_.Data_.TotalQty_ += symb->Deal_.Data_.Deal_.Qty_;
   symb->CheckOHL(symb->Deal_.Data_.Deal_.Pri_, dealTime);

   fon9::RevBufferList rts{pksz};
   fon9::ToBitv(rts, symb->Deal_.Data_.Deal_.Qty_);
   fon9::ToBitv(rts, symb->Deal_.Data_.Deal_.Pri_);
   symb->Deal_.Data_.MarketSeq_ = ++symb->BS_.Data_.MarketSeq_;
   *rts.AllocPacket<uint8_t>() = 0; // = 1筆成交.
   fon9::RevPutBitv(rts, fon9_BitvV_NumberNull); // DealTime = InfoTime;
   *rts.AllocPacket<uint8_t>() = fon9::cast_to_underlying(symb->Deal_.Data_.Flags_);
   if (IsEnumContains(TwsMdSys(*this).Symbs_->CtrlFlags_, f9fmkt::MdSymbsCtrlFlag::HasMarketDataSeq)) {
      /// 定價 Fmt9 的序號獨立編號, 與 Fmt6 無關, 所以必須額外處理.
      ToBitv(rts, symb->BS_.Data_.MarketSeq_);
   }
   symb->MdRtStream_.Publish(ToStrView(symb->SymbId_), f9sv_RtsPackType_DealPack, f9sv_MdRtsKind_Deal,
                             dealTime, std::move(rts));
}

} // namespaces
