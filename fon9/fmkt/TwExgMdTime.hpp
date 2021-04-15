// \file fon9/fmkt/TwExgMdTime.hpp
//
// TaiFex/TSE/OTC 行情的時間格式.
//
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_TwExgMdTime_hpp__
#define __fon9_fmkt_TwExgMdTime_hpp__
#include "fon9/PackBcd.hpp"
#include "fon9/TimeInterval.hpp"

namespace fon9 { namespace fmkt {

fon9_MSC_WARN_DISABLE(4201); // nonstandard extension used: nameless struct/union
fon9_PACK(1);
struct TwExgMdTimeHHMMSS {
   union {
      PackBcd<6>     HHMMSS_;
      struct {
         PackBcd<2>  HH_;
         PackBcd<2>  MM_;
         PackBcd<2>  SS_;
      };
   };

   DayTime ToDayTime() const {
      return TimeInterval_HHMMSS(
         PackBcdTo<uint8_t>(this->HH_),
         PackBcdTo<uint8_t>(this->MM_),
         PackBcdTo<uint8_t>(this->SS_));
   }
};

/// 台灣 (期交所、證交所、櫃買中心) 行情時間格式.
/// HHMMSSuuuuuu 後6位為 us.
struct TwExgMdTimeHHMMSSu6 : public TwExgMdTimeHHMMSS {
   PackBcd<6>  U6_;
   DayTime ToDayTime() const {
      return TwExgMdTimeHHMMSS::ToDayTime()
         + TimeInterval_Microsecond(PackBcdTo<uint32_t>(this->U6_));
   }
   void FromHHMMSSu6(uint64_t hhmmss_u6) {
      ToPackBcd<sizeof(*this) * 2, uint64_t>(this, hhmmss_u6);
   }
};
fon9_PACK_POP;
fon9_MSC_WARN_POP;

} } // namespaces
#endif//__fon9_fmkt_TwExgMdTime_hpp__
