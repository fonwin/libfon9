// \file fon9/TsAppend.cpp
// \author fonwinz@gmail.com
#include "fon9/TsAppend.hpp"
#include "fon9/BitvFixedInt.hpp"

namespace fon9 {

fon9_API void TsAppend(AsyncFileAppender& pklog, TimeStamp now,
                       const void* pkptr, uint16_t pksz) {
   size_t         kSaveSize = pksz + sizeof(now) + sizeof(pksz);
   RevBufferNode* rbuf{RevBufferNode::Alloc(kSaveSize)};
   fon9::byte*    pbuf = rbuf->GetDataBegin() - kSaveSize;
   rbuf->SetPrefixUsed(pbuf);
   *pbuf = fon9_BitvT_TimeStamp_Orig7; // 0xe0
   ToBitvFixedRev<sizeof(now) - 1>(reinterpret_cast<char*>(pbuf) + sizeof(now), now.GetOrigValue());
   PutBigEndian(pbuf + sizeof(now), pksz);
   memcpy(pbuf + sizeof(now) + sizeof(pksz), pkptr, pksz);
   pklog.Append(rbuf);
}

} // namespaces
