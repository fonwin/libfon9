// \file f9twf/ExgMdFmtBasicInfoToRts.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdFmtBasicInfoToRts.hpp"
#include "f9twf/ExgMdFmtBasicInfo.hpp"
#include "f9twf/ExgMdFmtParsers.hpp"

namespace f9twf {
namespace f9fmkt = fon9::fmkt;

f9twf_API void I010BasicInfoParserToRts(ExgMcMessage& e) {
   const ExgMcI010& pk = *static_cast<const ExgMcI010*>(&e.Pk_);
   ExgMdLocker      lk{e, pk.ProdId_};
   auto&            symb = *e.Symb_;

   const TwfSymbRef::Data  ref = symb.Ref_.Data_;
   const auto              strikePriceDiv = symb.StrikePriceDiv_;
   const auto              flowGroup = symb.FlowGroup_;
   const auto              tradingMarket = symb.TradingMarket_;

   if (!I010BasicInfoLockedParser(e, lk))
      return;
   if (strikePriceDiv != symb.StrikePriceDiv_
       || flowGroup != symb.FlowGroup_
       || tradingMarket != symb.TradingMarket_
       || !ref.IsEuqalBaseRef(symb.Ref_.Data_)) {
      // 因為 symb.PriceOrigDiv_ 客戶端用不到, 所以不提供;
      fon9::RevBufferList rbuf{128};
      for (size_t L = fon9::numofele(symb.Ref_.Data_.PriLmts_); L > 0;) {
         --L;
         fon9::ToBitv(rbuf, symb.Ref_.Data_.PriLmts_[L].Dn_);
         fon9::ToBitv(rbuf, symb.Ref_.Data_.PriLmts_[L].Up_);
      }
      fon9::ToBitv(rbuf, symb.Ref_.Data_.PriRef_);
      *rbuf.AllocPacket<uint8_t>() = fon9::PackBcdTo<uint8_t>(pk.StrikePriceDecimalLocator_);
      *rbuf.AllocPacket<uint8_t>() = symb.FlowGroup_;
      *rbuf.AllocPacket<char>() = symb.TradingMarket_;
      symb.MdRtStream_.Publish(ToStrView(symb.SymbId_),
                               f9fmkt::RtsPackType::BaseInfoTw,
                               e.Pk_.InformationTime_.ToDayTime(),
                               std::move(rbuf));
   }
   symb.Contract_.OnSymbBaseChanged(symb);
}

} // namespaces
