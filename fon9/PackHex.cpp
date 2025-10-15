/// \file fon9/PackHex.cpp
/// \author fonwinz@gmail.com
#include "fon9/PackHex.hpp"

namespace fon9 {

fon9_API const char  kSeqToHex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
fon9_API const char  kSeqToHEX[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

fon9_API void RevPrintPackHexMap(fon9::RevBuffer& rbuf, const uint8_t* phex, unsigned hexCount, const char kSeqMap[16]) {
   char* pbuf = rbuf.AllocBuffer(hexCount);
   for (unsigned L = hexCount; L > 0; --L) {
      uint8_t vhex = *phex++;
      *pbuf++ = kSeqMap[vhex >> 4];
      if (--L == 0)
         break;
      *pbuf++ = kSeqMap[vhex & 0x0f];
   }
}

} // namespace