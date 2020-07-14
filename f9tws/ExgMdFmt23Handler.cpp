// \file f9tws/ExgMdFmt23Handler.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdFmt23Handler.hpp"
#include "f9tws/ExgMdFmt6Handler.hpp"
#include "f9tws/ExgMdFmt23.hpp"

namespace f9tws {

ExgMdFmt23Handler::~ExgMdFmt23Handler() {
}
//--------------------------------------------------------------------------//
void ExgMdFmt23Handler::PkContOnReceived(const void* pkptr, unsigned pksz, SeqT seq) {
   FmtRt_PkContOnReceived<f9fmkt_TradingSessionId_OddLot>(
      *this, *this->MdSys_.SymbsOdd_,
      *static_cast<const ExgMdFmt23v1*>(pkptr), pksz, seq);
}

} // namespaces
