// \file f9twf/ExgMdFmtQuoteReq.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMdFmtQuoteReq_hpp__
#define __f9twf_ExgMdFmtQuoteReq_hpp__
#include "f9twf/ExgMdFmt.hpp"

namespace f9twf {
fon9_PACK(1);

struct ExgMdQuoteReq {
   ExgMdProdId10     ProdId_;
   ExgMdHHMMSSu6     DisclosureTime_;
   fon9::PackBcd<3>  DurationSeconds_;
};
//--------------------------------------------------------------------------//
/// 逐筆行情 詢價要求揭示: Fut:(Tx='2'; Mg='4'); Opt:(Tx='5'; Mg='4'); Ver=2;
struct ExgMcI100 : public ExgMcHead
                 , public ExgMdQuoteReq
                 , public ExgMdTail {
};
//--------------------------------------------------------------------------//
/// 間隔行情 詢價要求揭示: Fut:(Tx='2'; Mg='4'); Opt:(Tx='5'; Mg='4'); Ver=2;
struct ExgMiI100 : public ExgMiHead
                 , public ExgMdQuoteReq
                 , public ExgMdTail {
};

fon9_PACK_POP;
} // namespaces
#endif//__f9twf_ExgMdFmtQuoteReq_hpp__
