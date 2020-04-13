// \file f9twf/ExgMdFmtParsers.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdFmtParsers.hpp"
#include "f9twf/ExgMdFmtBasicInfo.hpp"
#include "f9twf/ExgMdFmtBS.hpp"
#include "f9twf/ExgMcFmtSS.hpp"
#include "f9twf/ExgMdFmtBaseContract.hpp"

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
   symb.PriceOrigDiv_ = ToPriceOrigDiv(pk);
   symb.StrikePriceDiv_ = ToStrikePriceDiv(pk);
   ExgMdPriceTo(symb.Ref_.Data_.PriRef_, pk.ReferencePrice_, symb.PriceOrigDiv_);
   ExgMdPriceTo(symb.Ref_.Data_.PriUpLmt_, pk.RiseLimitPrice1_, symb.PriceOrigDiv_);
   ExgMdPriceTo(symb.Ref_.Data_.PriDnLmt_, pk.FallLimitPrice1_, symb.PriceOrigDiv_);
   return true;
}
f9twf_API void I010BasicInfoParser(ExgMcMessage& e) {
   I010BasicInfoLockedParser(e, ExgMdLocker{e, static_cast<const ExgMcI010*>(&e.Pk_)->ProdId_});
}
//--------------------------------------------------------------------------//
f9twf_API void I011ContractParser(ExgMcMessage& e) {
   const ExgMcI011& i011 = *static_cast<const ExgMcI011*>(&e.Pk_);
   ExgMdSymbs&      symbs{*e.Channel_.GetChannelMgr()->Symbs_};
   auto             symbsLk{symbs.SymbMap_.Lock()};
   ExgMdContract&   con = symbs.FetchContract(i011.ContractId_);
   con.AcceptQuote_ = (i011.AcceptQuoteFlagYN_ == 'Y' ? fon9::EnabledYN::Yes : fon9::EnabledYN{});
   con.ContractSize_.Assign<4>(fon9::PackBcdTo<uint64_t>(i011.ContractSizeV4_));
   con.StkNo_ = i011.StkNo_;
   con.SetBaseValues(ToPriceOrigDiv(i011), ToStrikePriceDiv(i011));
   //puts(RevPrintTo<std::string>(
   //   '[', i011.ContractId_, "]"
   //   "[", i011.StkNo_, "]"
   //   "[.", fon9::PackBcdTo<uint8_t>(i011.DecimalLocator_), "]"
   //   "[", StrView_all(i011.NameBig5_), "]"
   //   ).c_str());
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
