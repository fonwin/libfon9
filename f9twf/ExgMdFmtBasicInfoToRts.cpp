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

   const f9fmkt::SymbRef::Data ref = symb.Ref_.Data_;
   const auto                  strikePriceDiv = symb.StrikePriceDiv_;
   const auto                  flowGroup = symb.FlowGroup_;
   const auto                  tradingMarket = symb.TradingMarket_;

   if (!I010BasicInfoLockedParser(e, lk))
      return;
   if (strikePriceDiv != symb.StrikePriceDiv_
       || flowGroup != symb.FlowGroup_
       || tradingMarket != symb.TradingMarket_
       || ref.PriRef_ != symb.Ref_.Data_.PriRef_
       || ref.PriUpLmt_ != symb.Ref_.Data_.PriUpLmt_
       || ref.PriDnLmt_ != symb.Ref_.Data_.PriDnLmt_) {
      // 因為 symb.PriceOrigDiv_ 客戶端用不到, 所以不提供;
      fon9::RevBufferList rbuf{128};
      fon9::ToBitv(rbuf, symb.Ref_.Data_.PriDnLmt_);
      fon9::ToBitv(rbuf, symb.Ref_.Data_.PriUpLmt_);
      fon9::ToBitv(rbuf, symb.Ref_.Data_.PriRef_);
      *rbuf.AllocPacket<uint8_t>() = fon9::PackBcdTo<uint8_t>(pk.StrikePriceDecimalLocator_);
      *rbuf.AllocPacket<uint8_t>() = symb.FlowGroup_;
      *rbuf.AllocPacket<char>() = symb.TradingMarket_;
      symb.MdRtStream_.Publish(lk.Symbs_, ToStrView(symb.SymbId_),
                               f9fmkt::RtsPackType::BaseInfoTw,
                               e.Pk_.InformationTime_.ToDayTime(),
                               std::move(rbuf));
   }
   symb.Contract_.OnSymbBaseChanged(symb);
}

} // namespaces
