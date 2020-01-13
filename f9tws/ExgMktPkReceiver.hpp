// \file f9tws/ExgMktPkReceiver.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMktPkReceiver_hpp__
#define __f9tws_ExgMktPkReceiver_hpp__
#include "f9tws/ExgMktFmt.hpp"
#include "fon9/PkReceiver.hpp"

namespace f9tws {

/// 解析台灣證券交易所(及櫃買中心)行情格式框架.
class f9tws_API ExgMktPkReceiver : public fon9::PkReceiver {
   fon9_NON_COPY_NON_MOVE(ExgMktPkReceiver);
   using base = fon9::PkReceiver;
public:
   ExgMktPkReceiver(bool isDgram = false)
      : base{sizeof(ExgMktHeader), isDgram} {
   }
   virtual ~ExgMktPkReceiver();

   unsigned GetPkSize(const void* pkptr) override;
};

} // namespaces
#endif//__f9tws_ExgMktFeeder_hpp__
