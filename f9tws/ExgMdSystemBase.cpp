// \file f9tws/ExgMdSystemBase.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdSystemBase.hpp"

namespace f9tws {

ExgMdHandlerPkCont::~ExgMdHandlerPkCont() {
}
void ExgMdHandlerPkCont::OnLogSeqGap(fon9::PkContFeeder& rthis, fon9::RevBuffer& rbuf, const void* pk) {
   (void)rthis;
   fon9::RevPrint(rbuf, "TwsExgMd.Gap"
                  "|mkt=", static_cast<const ExgMdHead*>(pk)->GetMarket(),
                  "|fmt=", static_cast<const ExgMdHead*>(pk)->GetFmtNo());
}
//--------------------------------------------------------------------------//
ExgMdSystemBase::~ExgMdSystemBase() {
}
void ExgMdSystemBase::OnMdSystemStartup(const unsigned tdayYYYYMMDD, const std::string& logPath) {
   base::OnMdSystemStartup(tdayYYYYMMDD, logPath);
   for (const ExgMdHandlerSP& pph : this->MdHandlers_) {
      if (pph.get())
         pph->DailyClear();
   }
}

} // namespaces
