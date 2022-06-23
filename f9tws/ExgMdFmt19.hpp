// \file f9tws/ExgMdFmt19.hpp
//
// 格式19: 當日暫停/恢復交易股票資料.
//
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt19_hpp__
#define __f9tws_ExgMdFmt19_hpp__
#include "f9tws/ExgMdFmt.hpp"

namespace f9tws {
fon9_PACK(1);

struct ExgMdFmt19 : public ExgMdHead {
   StkNo             StkNo_;
   /// 當日暫停交易時間.
   fon9::PackBcd<6>  SuspendHHMMSS_;
   /// 當日恢復交易時間 .
   fon9::PackBcd<6>  ResumeHHMMSS_;
};

fon9_PACK_POP;
} // namespaces
#endif//__f9tws_ExgMdFmt19_hpp__
