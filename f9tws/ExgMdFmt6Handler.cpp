// \file f9tws/ExgMdFmt6Handler.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdFmt6Handler.hpp"
#include "f9tws/ExgMdFmt6.hpp"
#include "fon9/Log.hpp"

namespace f9tws {
namespace f9fmkt = fon9::fmkt;

ExgMdFmt6Handler::~ExgMdFmt6Handler() {
}
//--------------------------------------------------------------------------//
void ExgMdFmt6Handler::PkContOnReceived(const void* pkptr, unsigned pksz, SeqT seq) {
   const ExgMdFmt6v4& fmt6 = *static_cast<const ExgMdFmt6v4*>(pkptr);
   this->CheckLogLost(fmt6, seq);
   if (fmt6.IsLastDealSent())
      return;
   const auto  dealTime = fmt6.Time_.ToDayTime();
   auto        symblk = this->MdSys_.Symbs_->SymbMap_.Lock();
   auto        symb = fon9::static_pointer_cast<ExgMdSymb>(
                        this->MdSys_.Symbs_->FetchSymb(symblk, ToStrView(fmt6.StkNo_)));

// #define DEBUG_FMT6
#ifdef DEBUG_FMT6
   const bool kDebugPrint = (ToStrView(symb->SymbId_) == "2330");
   if (kDebugPrint) {
      printf("\n[%s]%06u/%02x.%02x.%02x/Total=%u/",
             symb->SymbId_.begin(),
             fon9::PackBcdTo<unsigned>(fmt6.Time_.HHMMSS_),
             fmt6.ItemMask_, fmt6.LmtMask_, fmt6.StatusMask_,
             fon9::PackBcdTo<unsigned>(fmt6.TotalQty_));
   }
#endif
   const bool  isCalc = ((fmt6.StatusMask_ & 0x80) != 0);
   const bool  hasDeal = ((fmt6.ItemMask_ & 0x80) != 0);
   const ExgMdPriQty* mdPQs = fmt6.PQs_;
   if (hasDeal) {
      symb->Deal_.Data_.Flags_ = (isCalc ? f9fmkt::DealFlag::Calculated : f9fmkt::DealFlag{});
      if (symb->Deal_.Data_.InfoTime_ != dealTime) {
         symb->Deal_.Data_.DealTime_ = symb->Deal_.Data_.InfoTime_ = dealTime;
         symb->Deal_.Data_.Flags_ |= f9fmkt::DealFlag::DealTimeChanged;
      }
   #ifdef DEBUG_FMT6
      if (kDebugPrint) {
         printf("Deal=%u*%u/",
                static_cast<unsigned>(fon9::PackBcdTo<uint64_t>(mdPQs->PriV4_)),
                fon9::PackBcdTo<unsigned>(mdPQs->Qty_));
      }
   #endif
      mdPQs->AssignTo(symb->Deal_.Data_.Deal_);
      ++mdPQs;
      // fmt6.TotalQty_ 包含此次成交, 試算階段 DealQty = TotalQty, 所以不更新 TotalQty.
      if (!isCalc) {
         // 若有「暫緩撮合」, 則: DealQty=0, 並提供「瞬間價格趨勢」.
         // 此時不應改變 High/Low, 但仍應「發行 & 儲存」此訊息,
         // 訂閱者如果有回補成交明細, 則會收到此訊息(DealQty=0), 處理時要注意。
         if (symb->Deal_.Data_.Deal_.Qty_ > 0)
            symb->CheckOHL(symb->Deal_.Data_.Deal_.Pri_, dealTime);
         const f9fmkt::Qty pkTotalQty = fon9::PackBcdTo<uint32_t>(fmt6.TotalQty_);
         if (symb->Deal_.Data_.TotalQty_ + symb->Deal_.Data_.Deal_.Qty_ != pkTotalQty)
            symb->Deal_.Data_.Flags_ |= f9fmkt::DealFlag::TotalQtyLost;
         symb->Deal_.Data_.TotalQty_ = pkTotalQty;
      }
      // - Bit 1-0 瞬間價格趨勢 00：一般揭示; 11：（保留）
      //   - 01：暫緩撮合且瞬間趨跌; 10：暫緩撮合且瞬間趨漲;
      //     - 此時 DealPri=上次成交價; DealQty=0; TotalQty=不變;
      //     - 然後該股票的 Fmt6 就是大約2分鐘的「試算」揭示, 且沒有再提供「瞬間價格趨勢」.
      switch (fmt6.LmtMask_ & 0x03) {
      case 1: symb->Deal_.Data_.Flags_ |= f9fmkt::DealFlag::HeldMatchTrendFall; break;
      case 2: symb->Deal_.Data_.Flags_ |= f9fmkt::DealFlag::HeldMatchTrendRise; break;
      }
   }
#ifdef DEBUG_FMT6
   if (kDebugPrint)
      printf("%02x/", symb->Deal_.Data_.Flags_);
#endif

   fon9::RevBufferList rts{pksz};
   f9fmkt::RtsPackType rtsPackType;
   if ((fmt6.ItemMask_ & 0x7e) || (fmt6.ItemMask_ & 1) == 0) {
      symb->BS_.Data_.Flags_ = (isCalc ? f9fmkt::BSFlag::Calculated : f9fmkt::BSFlag{});
      symb->BS_.Data_.InfoTime_ = dealTime;
      symb->BS_.Data_.Flags_ |= (f9fmkt::BSFlag::OrderBuy | f9fmkt::BSFlag::OrderSell);
      mdPQs = f9tws::AssignBS(symb->BS_.Data_.Buys_, mdPQs, (fmt6.ItemMask_ & 0x70) >> 4);
      mdPQs = f9tws::AssignBS(symb->BS_.Data_.Sells_, mdPQs, (fmt6.ItemMask_ & 0x0e) >> 1);
      if (hasDeal) {
         rtsPackType = f9fmkt::RtsPackType::DealBS;
         MdRtsPackSnapshotBS(rts, symb->BS_.Data_);
         goto __ASSIGN_DEAL_TO_RTS;
      }
      else {
         rtsPackType = isCalc ? f9fmkt::RtsPackType::CalculatedBS : f9fmkt::RtsPackType::SnapshotBS;
         MdRtsPackSnapshotBS(rts, symb->BS_.Data_);
      }
   }
   else if (hasDeal) {
      rtsPackType = f9fmkt::RtsPackType::DealPack;
   __ASSIGN_DEAL_TO_RTS:;
      fon9::ToBitv(rts, symb->Deal_.Data_.Deal_.Qty_);
      fon9::ToBitv(rts, symb->Deal_.Data_.Deal_.Pri_);
      *rts.AllocPacket<uint8_t>() = 0; // = 1筆成交.
      if (IsEnumContains(symb->Deal_.Data_.Flags_, f9fmkt::DealFlag::TotalQtyLost))
         fon9::ToBitv(rts, symb->Deal_.Data_.TotalQty_ - symb->Deal_.Data_.Deal_.Qty_);
      if (IsEnumContains(symb->Deal_.Data_.Flags_, f9fmkt::DealFlag::DealTimeChanged))
         fon9::RevPutBitv(rts, fon9_BitvV_NumberNull);
      *rts.AllocPacket<uint8_t>() = fon9::cast_to_underlying(symb->Deal_.Data_.Flags_);
   }
   else { // none Deal, none BS?
      return;
   }
   symb->MdRtStream_.Publish(ToStrView(symb->SymbId_), rtsPackType, dealTime, std::move(rts));
}

} // namespaces
