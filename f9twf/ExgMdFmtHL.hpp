// \file f9twf/ExgMdFmtHL.hpp
//
// 台灣期交所行情格式: 盤中最(高)低價揭示.
// 逐筆行情: I025;
// 間隔行情: I021;
//
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtHL_hpp__
#define __f9twf_ExgMdFmtHL_hpp__
#include "f9twf/ExgMdFmt.hpp"

namespace f9twf {
fon9_PACK(1);

struct ExgMdDayHighLow {
   ExgMdSPrice    DayHighPrice_;
   ExgMdSPrice    DayLowPrice_;
   ExgMdHHMMSSu6  ShowTime_;
};
//--------------------------------------------------------------------------//
/// 逐筆行情 盤中最(高)低價揭示: Fut:(Tx='2'; Mg='E'); Opt:(Tx='5'; Mg='E'); Ver=1;
struct ExgMcI025Head : public ExgMcHead {
   ExgMdProdId20     ProdId_;
   ExgMcProdMsgSeq   ProdMsgSeq_;
};
struct ExgMcI025 : public ExgMcI025Head
                 , public ExgMdDayHighLow
                 , public ExgMdTail {
};
//--------------------------------------------------------------------------//
/// 間隔行情 盤中最(高)低價揭示: Fut:(Tx='2'; Mg='5'); Opt:(Tx='5'; Mg='5'); Ver=3;
struct ExgMiI021Head : public ExgMiHead {
   ExgMdProdId20  ProdId_;
};
struct ExgMiI021 : public ExgMiI021Head
                 , public ExgMdDayHighLow
                 , public ExgMdTail {
};

fon9_PACK_POP;
} // namespaces
#endif//__f9twf_ExgMdFmtHL_hpp__
