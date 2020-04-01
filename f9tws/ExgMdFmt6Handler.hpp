// \file f9tws/ExgMdFmt6Handler.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt6Handler_hpp__
#define __f9tws_ExgMdFmt6Handler_hpp__
#include "f9tws/ExgMdSystem.hpp"

namespace f9tws {

class f9tws_API ExgMdFmt6Handler : public ExgMdHandlerPkCont {
   fon9_NON_COPY_NON_MOVE(ExgMdFmt6Handler);
   void PkContOnReceived(const void* pk, unsigned pksz, SeqT seq) override;
public:
   using ExgMdHandlerPkCont::ExgMdHandlerPkCont;
   virtual ~ExgMdFmt6Handler();
};

} // namespaces
#endif//__f9tws_ExgMdFmt6Handler_hpp__
