// \file f9twf/ExgMdFmtI060.hpp
//
// 台灣期交所行情格式: I060 現貨標的資訊.
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtI060_hpp__
#define __f9twf_ExgMdFmtI060_hpp__
#include "f9twf/ExgMdFmt.hpp"

namespace f9twf {
fon9_PACK(1);

/// Bit 5: 有無日期?   9(8): YYYYMMDD;
/// Bit 4: 有無時間?   fon9::PackBcd<12>: HHMMSSuuuuuu; 若時間=0, 價格=參考價;
/// Bit 3: 有無定盤價? fon9::PackBcd<12>: 9(7)V99999;
/// Bit 2: 有無賣出價? fon9::PackBcd<12>: 9(7)V99999;
/// Bit 1: 有無買進價? fon9::PackBcd<12>: 9(7)V99999;
/// Bit 0: 有無成交價? fon9::PackBcd<12>: 9(7)V99999;
enum class I060StatusItem1 : uint8_t {
   PriMatch = 0x01,
   PriBuy = 0x02,
   PriSell = 0x04,
   PriFixed = 0x08,
   Time = 0x10,
   Date = 0x20,
};
fon9_ENABLE_ENUM_BITWISE_OP(I060StatusItem1);

struct ExgMdI060Head {
   /// 類別代碼(契約代碼).
   fon9::CharAry<3>  Kind_;
   uint8_t           StatusItem2_;
   I060StatusItem1   StatusItem1_;
   char              Body_[1];
   using HHMMSSuuuuuu = fon9::PackBcd<12>;
   using PriceV5 = fon9::PackBcd<12>;
   using YYYYMMDD = fon9::CharAry<8>;
};

/// I060 使用動態長度, 所以在此不加上 Tail;
struct ExgMiI060 : public ExgMiHead, public ExgMdI060Head {
};
struct ExgMcI060 : public ExgMcHead, public ExgMdI060Head {
};

fon9_PACK_POP;
} // namespaces
#endif//__f9twf_ExgMdFmtI060_hpp__
