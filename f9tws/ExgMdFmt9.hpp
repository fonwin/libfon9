// \file f9tws/ExgMdFmt9.hpp
//
// 定價交易個股成交資料.
//
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt9_hpp__
#define __f9tws_ExgMdFmt9_hpp__
#include "f9tws/ExgMdFmt.hpp"

namespace f9tws {
fon9_PACK(1);

struct ExgMdFmt9 : public ExgMdHeader {
   StkNo             StkNo_;
   fon9::PackBcd<6>  DealHHMMSS_;
   ExgMdPriQty       DealPQ_;
};

fon9_PACK_POP;
} // namespaces
#endif//__f9tws_ExgMdFmt9_hpp__
