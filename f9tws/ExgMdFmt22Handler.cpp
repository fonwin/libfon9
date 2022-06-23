// \file f9tws/ExgMdFmt22Handler.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdFmt22Handler.hpp"
#include "f9tws/ExgMdFmt1Handler.hpp"
#include "f9tws/ExgMdFmt22.hpp"

namespace f9tws {

ExgMdFmt22Handler::~ExgMdFmt22Handler() {
}
void ExgMdFmt22Handler::OnPkReceived(const ExgMdHead& pkhdr, unsigned pksz) {
   const ExgMdFmt22v1& fmt22 = *static_cast<const ExgMdFmt22v1*>(&pkhdr);
   EdgMdParseBaseInfo<ExgMdBaseInfoParser>(fon9::PackBcdTo<uint8_t>(pkhdr.Market_) == ExgMdMarket_TwTSE
                                           ? f9fmkt_TradingMarket_TwSEC : f9fmkt_TradingMarket_TwOTC,
                                           *this, *TwsMdSys(*this).SymbsOdd_,
                                           fmt22, pksz);
}

} // namespaces
