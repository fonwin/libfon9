// \file f9tws/ExgMdPkReceiver.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdPkReceiver_hpp__
#define __f9tws_ExgMdPkReceiver_hpp__
#include "f9tws/ExgMdFmt.hpp"
#include "fon9/PkReceiver.hpp"

namespace f9tws {

/// 解析台灣證券交易所(及櫃買中心)行情格式框架.
class f9tws_API ExgMdPkReceiver : public fon9::PkReceiver {
   fon9_NON_COPY_NON_MOVE(ExgMdPkReceiver);
   using base = fon9::PkReceiver;
public:
   ExgMdPkReceiver(bool isDgram = false)
      : base{sizeof(ExgMdHeader), isDgram} {
   }
   virtual ~ExgMdPkReceiver();

   unsigned GetPkSize(const void* pkptr) override;
};

} // namespaces
#endif//__f9tws_ExgMdPkReceiver_hpp__
