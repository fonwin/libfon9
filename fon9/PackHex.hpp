/// \file fon9/PackHex.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_PackHex_hpp__
#define __fon9_PackHex_hpp__
#include "fon9/RevPrint.hpp"

namespace fon9 {

fon9_API extern const char  kSeqToHex[16];// = "0123456789abcdef";
fon9_API extern const char  kSeqToHEX[16];// = "0123456789ABCDEF";

/// hexCount: 每個 byte 2 個 hex, 如果 hexCount 是奇數, 則表示僅輸出最後一個 *phex 的高位;
fon9_API void RevPrintPackHexMap(RevBuffer& rbuf, const uint8_t* phex, unsigned hexCount, const char kSeqMap[16]);

static inline void RevPrintPackHex(RevBuffer& rbuf, const uint8_t* phex, unsigned hexCount) { RevPrintPackHexMap(rbuf, phex, hexCount, kSeqToHex); }
static inline void RevPrintPackHEX(RevBuffer& rbuf, const uint8_t* phex, unsigned hexCount) { RevPrintPackHexMap(rbuf, phex, hexCount, kSeqToHEX); }

template <unsigned kCount>
struct PackHex : public Comparable<PackHex<kCount> > {
   enum { HexCount = kCount };
   uint8_t  Values_[(kCount + 1) / 2];
   bool operator==(const PackHex& rhs) const { return memcmp(this->Values_, rhs.Values_, sizeof(this->Values_)) == 0; }
   bool operator< (const PackHex& rhs) const { return memcmp(this->Values_, rhs.Values_, sizeof(this->Values_)) < 0; }
};

template <unsigned kCount>
struct PackHEX : public PackHex<kCount> {
};

template <unsigned kCount>
static inline void RevPrint(RevBuffer& rbuf, const PackHex<kCount>& hex) {
   RevPrintPackHex(rbuf, hex.Values_, kCount);
}

template <unsigned kCount>
static inline void RevPrint(RevBuffer& rbuf, const PackHEX<kCount>& hex) {
   RevPrintPackHEX(rbuf, hex.Values_, kCount);
}

} // namespace

#endif//__fon9_PackHex_hpp__
