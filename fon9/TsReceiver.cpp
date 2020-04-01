// \file fon9/TsReceiver.cpp
// \author fonwinz@gmail.com
#include "fon9/TsReceiver.hpp"
#include "fon9/BitvDecode.hpp"

namespace fon9 {

TsReceiver::~TsReceiver() {
}
bool TsReceiver::FeedBuffer(DcQueue& rxbuf) {
   char        peekbuf[0x10000];
   PkszT       pksz;
   TimeStamp   pktm;
   while (auto pkptr = static_cast<const byte*>(rxbuf.Peek(peekbuf, kHeadSize))) {
      if (fon9_UNLIKELY(*pkptr != fon9_BitvT_TimeStamp_Orig7))
         rxbuf.PopUnknownChar(fon9_BitvT_TimeStamp_Orig7);
      else {
         pktm.SetOrigValue(GetPackedBigEndian<TimeStamp::OrigType>(pkptr + 1, sizeof(pktm) - 1));
         pksz = GetBigEndian<PkszT>(pkptr + sizeof(pktm));
         pkptr = static_cast<const byte*>(rxbuf.Peek(peekbuf, pksz + kHeadSize));
         if (fon9_UNLIKELY(pkptr == nullptr)) // rxbuf 資料不足.
            break;
         bool isCont = this->OnPkReceived(pktm, pkptr + kHeadSize, pksz);
         rxbuf.PopConsumed(pksz + kHeadSize);
         if (!isCont)
            return false;
      }
   }
   return true;
}

} // namespaces
