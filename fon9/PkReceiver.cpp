// \file fon9/PkReceiver.cpp
// \author fonwinz@gmail.com
#include "fon9/PkReceiver.hpp"

namespace fon9 {

PkReceiver::~PkReceiver() {
}
bool PkReceiver::FeedBuffer(DcQueue& rxbuf) {
   char  peekbuf[kMaxPacketSize];
   while (const void* pkptr = rxbuf.Peek(peekbuf, this->PkHeadSize_)) {
      if (fon9_LIKELY(*static_cast<const char*>(pkptr) == this->kPkHeadLeader)) {
         const unsigned  pksz = this->GetPkSize(pkptr);
         if (fon9_LIKELY(pksz < kMaxPacketSize)) {
            pkptr = rxbuf.Peek(peekbuf, pksz);
            if (fon9_UNLIKELY(pkptr == nullptr)) // rxbuf 資料不足 Length_
               break;
            // rxbuf 的資料長度足夠一個封包.
            const char* pkL = static_cast<const char*>(pkptr);
            if (fon9_LIKELY(pkL[pksz - 2] == 0x0d && pkL[pksz - 1] == 0x0a)) {
               // 尾碼正確, 檢查 CheckSum.
               if (fon9_LIKELY(CalcCheckSum(pkL, pksz + 1) == 0)) {
                  // CheckSum 正確, 解析封包內容.
                  ++this->ReceivedCount_;
                  if (!this->OnPkReceived(pkptr, pksz)) {
                     rxbuf.PopConsumed(pksz);
                     return false;
                  }
               }
               else {
                  ++this->ChkSumErrCount_;
               }
               rxbuf.PopConsumed(pksz);
               continue;
            } // else: 尾碼不是 0x0d, 0x0a.
         } // else: 長度不合理.
         // 繼續執行: 搜尋下一個 EscCode.
         rxbuf.PopConsumed(1);
         ++this->DroppedBytes_;
      } // else: first code is not this->PkHeadLeader_.
      this->DroppedBytes_ += rxbuf.PopUnknownChar(this->kPkHeadLeader);
   }
   if (this->IsDgram_ && rxbuf.Peek1()) {
      if (size_t remain = rxbuf.CalcSize()) {
         rxbuf.PopConsumed(remain);
         this->DroppedBytes_ += remain;
      }
   }
   return true;
}

} // namespaces
