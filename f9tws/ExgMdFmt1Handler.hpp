// \file f9tws/ExgMdFmt1Handler.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt1Handler_hpp__
#define __f9tws_ExgMdFmt1Handler_hpp__
#include "f9tws/ExgMdSystem.hpp"

namespace f9tws {

class f9tws_API ExgMdFmt1TwseHandler : public ExgMdHandlerAnySeq {
   fon9_NON_COPY_NON_MOVE(ExgMdFmt1TwseHandler);
   void OnPkReceived(const ExgMdHeader& pk, unsigned pksz) override;
public:
   using ExgMdHandlerAnySeq::ExgMdHandlerAnySeq;
   virtual ~ExgMdFmt1TwseHandler();
};

class f9tws_API ExgMdFmt1TpexHandler : public ExgMdHandlerAnySeq {
   fon9_NON_COPY_NON_MOVE(ExgMdFmt1TpexHandler);
   void OnPkReceived(const ExgMdHeader& pk, unsigned pksz) override;
public:
   using ExgMdHandlerAnySeq::ExgMdHandlerAnySeq;
   virtual ~ExgMdFmt1TpexHandler();
};

} // namespaces
#endif//__f9tws_ExgMdFmt1Handler_hpp__
