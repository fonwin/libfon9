// \file f9tws/ExgMdFmt23.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt23_hpp__
#define __f9tws_ExgMdFmt23_hpp__
#include "f9tws/ExgMdFmt6.hpp"

namespace f9tws {
fon9_PACK(1);

using ExgMdOddQty = fon9::PackBcd<12>;
struct ExgMdOddPriQty {
   fon9::PackBcd<9>  PriV4_;
   ExgMdOddQty       Qty_;

   void AssignTo(fon9::fmkt::PriQty& dst) const {
      dst.Pri_.Assign<4>(fon9::PackBcdTo<uint64_t>(this->PriV4_));
      dst.Qty_ = fon9::PackBcdTo<fon9::fmkt::Qty>(this->Qty_);
   }
};

struct ExgMdFmt23v1 : public ExgMdFmtRtBase {
   ExgMdOddQty       TotalQty_;
   ExgMdOddPriQty    PQs_[1];

   bool HasBS() const {
      return (this->ItemMask_ & 0x7e) != 0;
   }
};
fon9_PACK_POP;

} // namespaces
#endif//__f9tws_ExgMdFmt23_hpp__
