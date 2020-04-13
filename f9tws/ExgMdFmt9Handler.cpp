// \file f9tws/ExgMdFmt9Handler.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdFmt9Handler.hpp"
#include "f9tws/ExgMdFmt9.hpp"

namespace f9tws {
namespace f9fmkt = fon9::fmkt;

ExgMdFmt9Handler::~ExgMdFmt9Handler() {
}
void ExgMdFmt9Handler::OnPkReceived(const ExgMdHeader& pkhdr, unsigned pksz) {
   const ExgMdFmt9& fmt9 = *static_cast<const ExgMdFmt9*>(&pkhdr);
   const unsigned   dealHHMMSS = fon9::PackBcdTo<unsigned>(fmt9.DealHHMMSS_);
   if (dealHHMMSS == 999999)
      return;
   const fon9::DayTime  dealTime = fon9::TimeInterval_HHMMSS(dealHHMMSS);

   auto  symblk = this->MdSys_.Symbs_->SymbMap_.Lock();
   auto  symb = fon9::static_pointer_cast<ExgMdSymb>(
      this->MdSys_.Symbs_->FetchSymb(symblk, ToStrView(fmt9.StkNo_)));
   if (dealTime == symb->Deal_.Data_.DealTime_)
      return;

   symb->Deal_.Data_.DealTime_ = symb->Deal_.Data_.InfoTime_ = dealTime;
   symb->Deal_.Data_.Flags_ = f9fmkt::DealFlag::DealTimeChanged;
   fmt9.DealPQ_.AssignTo(symb->Deal_.Data_.Deal_);
   symb->Deal_.Data_.TotalQty_ += symb->Deal_.Data_.Deal_.Qty_;

   symb->High_.CheckHigh(symb->Deal_.Data_.Deal_.Pri_, dealTime);
   symb->Low_.CheckLow(symb->Deal_.Data_.Deal_.Pri_, dealTime);
         
   fon9::RevBufferList rts{pksz};
   fon9::ToBitv(rts, symb->Deal_.Data_.Deal_.Qty_);
   fon9::ToBitv(rts, symb->Deal_.Data_.Deal_.Pri_);
   *rts.AllocPacket<uint8_t>() = 0; // = 1筆成交.
   fon9::RevPutBitv(rts, fon9_BitvV_NumberNull); // DealTime = InfoTime;
   *rts.AllocPacket<uint8_t>() = fon9::cast_to_underlying(symb->Deal_.Data_.Flags_);
   symb->MdRtStream_.Publish(*this->MdSys_.Symbs_, ToStrView(symb->SymbId_),
                             f9fmkt::RtsPackType::DealPack, dealTime, std::move(rts));
}

} // namespaces
