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
      // 盤中零股採用集合競價，每次的 Fmt23 揭示都包含「買賣報價」，
      // 如果市場上確實沒有買賣，則 (this->ItemMask_ & 0x7e) 為 0;
      // 所以 Fmt23 的 HasBS() 總是為 true; 表示有提供買賣報價(有可能檔數為 0);
      // return (this->ItemMask_ & 0x7e) != 0;
      return true;

      // 格式6，採用逐筆撮合，有額外的旗標，用來標示該次 Fmt6 是否包含買賣報價。
      // return (this->ItemMask_ & 0x7e) || (this->ItemMask_ & 1) == 0;
   }
};
fon9_PACK_POP;

} // namespaces
#endif//__f9tws_ExgMdFmt23_hpp__
