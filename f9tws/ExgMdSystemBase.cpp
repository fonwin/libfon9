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
   if (0);// 針對 [Market + (第一IP:Fmt6 or 第二IP:Fmt17 or 第五IP:Fmt22)] 處理斷線事件.
   // 可從 ExgMdReceiverSession::OnDevice_CommonTimer() 通知 this->MdDispatcher_; 觸發行情斷線事件.
}
void ExgMdSystemBase::OnMdSystemStartup(const unsigned tdayYYYYMMDD, const std::string& logPath) {
   base::OnMdSystemStartup(tdayYYYYMMDD, logPath);
   for (const ExgMdHandlerSP& pph : this->MdHandlers_) {
      if (pph.get())
         pph->DailyClear();
   }
}

} // namespaces
