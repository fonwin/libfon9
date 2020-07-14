// \file f9tws/ExgMdFmt6Handler.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdFmt6Handler.hpp"
#include "f9tws/ExgMdFmt6.hpp"

namespace f9tws {

ExgMdFmt6Handler::~ExgMdFmt6Handler() {
}
//--------------------------------------------------------------------------//
void ExgMdFmt6Handler::PkContOnReceived(const void* pkptr, unsigned pksz, SeqT seq) {
   FmtRt_PkContOnReceived<f9fmkt_TradingSessionId_Normal>(
      *this, *this->MdSys_.Symbs_,
      *static_cast<const ExgMdFmt6v4*>(pkptr), pksz, seq);
}

} // namespaces
