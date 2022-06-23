// \file f9tws/ExgMdFmt19Handler.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt19Handler_hpp__
#define __f9tws_ExgMdFmt19Handler_hpp__
#include "f9tws/ExgMdSystem.hpp"

namespace f9tws {

class f9tws_API ExgMdFmt19Handler : public ExgMdHandlerAnySeq {
   fon9_NON_COPY_NON_MOVE(ExgMdFmt19Handler);
   void OnPkReceived(const ExgMdHead& pk, unsigned pksz) override;
public:
   using ExgMdHandlerAnySeq::ExgMdHandlerAnySeq;
   virtual ~ExgMdFmt19Handler();
};

} // namespaces
#endif//__f9tws_ExgMdFmt19Handler_hpp__
