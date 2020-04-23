// \file f9twf/ExgMdFmtQuoteReqToRts.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgMdFmtQuoteReqToRts.hpp"
#include "f9twf/ExgMdFmtQuoteReq.hpp"
#include "f9twf/ExgMdFmtParsers.hpp"
#include "fon9/fmkt/SymbTabNames.h"

namespace f9twf {
namespace f9fmkt = fon9::fmkt;

f9twf_API void I100QuoteReqParserToRts(ExgMcMessage& e) {
   const auto& pk = *static_cast<const ExgMcI100*>(&e.Pk_);
   f9fmkt::SymbQuoteReq_Data  quoteReq;
   quoteReq.DisclosureTime_ = pk.DisclosureTime_.ToDayTime();
   quoteReq.DurationSeconds_ = fon9::PackBcdTo<uint16_t>(pk.DurationSeconds_);

   ExgMdLocker lk{e, pk.ProdId_};
   auto&       symb = *e.Symb_;
   if (symb.QuoteReq_.Data_ == quoteReq)
      return;
   symb.QuoteReq_.Data_ = quoteReq;
   fon9::RevBufferList rbuf{128};
   auto* tabQuoteReq = lk.Symbs_.LayoutSP_->GetTab(fon9_kCSTR_TabName_QuoteReq);
   f9fmkt::MdRtsPackTabValues(rbuf, *tabQuoteReq, symb.QuoteReq_);
   symb.MdRtStream_.Publish(ToStrView(symb.SymbId_),
                            f9fmkt::RtsPackType::TabValues_AndInfoTime,
                            e.Pk_.InformationTime_.ToDayTime(),
                            std::move(rbuf));
}

} // namespaces
