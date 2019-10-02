// \file f9twf/ExgTmpTypes.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgTmpTypes.hpp"

namespace f9twf {

f9twf_API TmpCheckSum TmpCalcCheckSum(const TmpHeader& pktmp, size_t pksz) {
   assert(pksz == TmpGetValue<TmpMsgLength_t>(pktmp.MsgLength_) + sizeof(pktmp.MsgLength_) + sizeof(TmpCheckSum));
   const byte* pbeg = reinterpret_cast<const byte*>(&pktmp);
   const byte* pend = pbeg + pksz - sizeof(TmpCheckSum);
   TmpCheckSum chksum = 0;
   while (pbeg < pend)
      chksum = static_cast<TmpCheckSum>(chksum + *pbeg++);
   return chksum;
}

} // namespaces
