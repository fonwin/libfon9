// \file f9twf/ExgMdFmtParsers.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdFmtParsers.hpp"
#include "f9twf/ExgMdFmtBasicInfo.hpp"
#include "f9twf/ExgMdFmtBS.hpp"
#include "f9twf/ExgMcFmtSS.hpp"

namespace f9twf {
namespace f9fmkt = fon9::fmkt;

f9twf_API bool I010BasicInfoLockedParser(ExgMcMessage& e, const ExgMdLocker&) {
   auto& pk = *static_cast<const ExgMcI010*>(&e.Pk_);
   auto& symb = *e.Symb_;
   if (!e.Channel_.GetChannelMgr()->CheckSymbTradingSessionId(symb))
      return false;
   symb.TradingMarket_ = (pk.TransmissionCode_ == '1' ? f9fmkt_TradingMarket_TwFUT : f9fmkt_TradingMarket_TwOPT);
   symb.FlowGroup_ = fon9::PackBcdTo<f9fmkt::SymbFlowGroup_t>(pk.FlowGroup_);
   symb.EndYYYYMMDD_ = fon9::PackBcdTo<uint32_t>(pk.EndDateYYYYMMDD_);
   symb.PriceOrigDiv_ = static_cast<uint32_t>(fon9::GetDecDivisor(
      static_cast<uint8_t>(f9fmkt::Pri::Scale - fon9::PackBcdTo<uint8_t>(pk.DecimalLocator_))));
   symb.StrikePriceDiv_ = static_cast<uint32_t>(fon9::GetDecDivisor(
      fon9::PackBcdTo<uint8_t>(pk.StrikePriceDecimalLocator_)));
   ExgMdPriceTo(symb.Ref_.Data_.PriRef_, pk.ReferencePrice_, symb.PriceOrigDiv_);
   ExgMdPriceTo(symb.Ref_.Data_.PriUpLmt_, pk.RiseLimitPrice1_, symb.PriceOrigDiv_);
   ExgMdPriceTo(symb.Ref_.Data_.PriDnLmt_, pk.FallLimitPrice1_, symb.PriceOrigDiv_);
   return true;
}
f9twf_API void I010BasicInfoParser(ExgMcMessage& e) {
   I010BasicInfoLockedParser(e, ExgMdLocker{e, static_cast<const ExgMcI010*>(&e.Pk_)->ProdId_});
}
//--------------------------------------------------------------------------//
f9twf_API void I081BSParser(ExgMcMessage& e) {
   auto&       pk = *static_cast<const ExgMcI081*>(&e.Pk_);
   ExgMdLocker lk{e, pk.ProdId_};
   if (e.Channel_.GetChannelMgr()->CheckSymbTradingSessionId(*e.Symb_)) {
      ExgMdToUpdateBS(e.Pk_.InformationTime_.ToDayTime(),
                      fon9::PackBcdTo<unsigned>(pk.NoMdEntries_),
                      pk.MdEntry_,
                      *e.Symb_);
   }
}
f9twf_API void I083BSParser(ExgMcMessage& e) {
   auto&       pk = *static_cast<const ExgMcI083*>(&e.Pk_);
   ExgMdLocker lk{e, pk.ProdId_};
   if (e.Channel_.GetChannelMgr()->CheckSymbTradingSessionId(*e.Symb_)) {
      ExgMdToSnapshotBS(e.Pk_.InformationTime_.ToDayTime(),
                        fon9::PackBcdTo<unsigned>(pk.NoMdEntries_),
                        pk.MdEntry_,
                        *e.Symb_);
      if (pk.CalculatedFlag_ == '1')
         e.Symb_->BS_.Data_.Flags_ |= f9fmkt::BSFlag::Calculated;
   }
}
//--------------------------------------------------------------------------//
static void McI084SSParserO(ExgMcMessage& e) {
   ExgMcChannel& rtch = e.Channel_.GetChannelMgr()->GetRealtimeChannel(e.Channel_);
   char          bufI083[sizeof(ExgMcI083) + sizeof(ExgMdEntry) * 100];
   ExgMcI083*    pI083 = nullptr;
   if (rtch.IsNeedsNotifyConsumer()) {
      // 轉發 ExgMcI083 訊息.
      // TODO: 也許應改成選項, 選擇: I084._O_ 是否要轉發 I083 訊息?
      // TODO: ToMiConv 可以直接訂閱 Channel 13,14 處理 I084._O_ 然後轉發 I080 封包.
      pI083 = reinterpret_cast<ExgMcI083*>(bufI083);
      *static_cast<ExgMcHead*>(pI083) = e.Pk_;
      pI083->MessageKind_ = 'B';
      fon9::ToPackBcd(pI083->ChannelId_, rtch.GetChannelId());
      // pI083->ChannelSeq_ 設為 0, 讓處理的人可以判斷此時的 I083 是從快照而來.
      fon9::ToPackBcd(pI083->ChannelSeq_, 0u);
      fon9::ToPackBcd(pI083->VersionNo_, 1u);
      // 試撮價格註記, '0':委託簿揭示訊息; '1':試撮後剩餘委託訊息.
      // 2020.01.17: 根據期交所說明, 試撮階段不會有委託簿快照訊息.
      pI083->CalculatedFlag_ = '0';
   }

   auto&    pk = static_cast<const ExgMcI084*>(&e.Pk_)->_O_OrderData_;
   auto     mdTime = e.Pk_.InformationTime_.ToDayTime();
   unsigned prodCount = fon9::PackBcdTo<unsigned>(pk.NoEntries_);
   auto*    prodEntry = pk.Entry_;
   auto*    symbs = e.Channel_.GetChannelMgr()->Symbs_.get();
   auto     locker = symbs->SymbMap_.Lock();
   for (unsigned prodL = 0; prodL < prodCount; ++prodL) {
      auto& symb = *static_cast<ExgMdSymb*>(symbs->FetchSymb(locker, fon9::StrView_eos_or_all(prodEntry->ProdId_.Chars_, ' ')).get());
      auto  mdCount = fon9::PackBcdTo<unsigned>(prodEntry->NoMdEntries_);
      if (e.Channel_.GetChannelMgr()->CheckSymbTradingSessionId(symb)) {
         ExgMdToSnapshotBS(mdTime, mdCount, prodEntry->MdEntry_, symb);
         if (pI083) {
            static_assert(sizeof(pI083->NoMdEntries_) == 1, "");
            *pI083->NoMdEntries_ = *prodEntry->NoMdEntries_;
            memcpy(pI083->MdEntry_, prodEntry->MdEntry_, sizeof(ExgMdEntry) * mdCount);
            pI083->ProdId_ = prodEntry->ProdId_;
            memcpy(pI083->ProdMsgSeq_, prodEntry->LastProdMsgSeq_, sizeof(pI083->ProdMsgSeq_));
            const unsigned pksz = static_cast<unsigned>(sizeof(ExgMcI083) + sizeof(ExgMdTail)
                                                        + (sizeof(pI083->MdEntry_[0]) * mdCount)
                                                        - sizeof(pI083->MdEntry_));
            fon9::ToPackBcd(pI083->BodyLength_, pksz - sizeof(ExgMcNoBody));
            ExgMcMessage e083{*pI083, pksz, rtch, 0};
            e083.Symb_ = &symb;
            rtch.NotifyConsumers(e083);
         }
      }
      prodEntry = reinterpret_cast<const ExgMcI084::OrderDataEntry*>(prodEntry->MdEntry_ + mdCount);
   }
}
f9twf_API void I084SSParser(ExgMcMessage& e) {
   switch (static_cast<const ExgMcI084*>(&e.Pk_)->MessageType_) {
      //case 'A': McI084SSParserA(e); break;
   case 'O': McI084SSParserO(e); break;
      //case 'S': McI084SSParserS(e); break;
      //case 'P': McI084SSParserP(e); break;
      //case 'Z': McI084SSParserZ(e); break;
   }
}

} // namespaces
