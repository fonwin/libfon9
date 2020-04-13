// \file f9tws/ExgMdFmt9Handler.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt9Handler_hpp__
#define __f9tws_ExgMdFmt9Handler_hpp__
#include "f9tws/ExgMdSystem.hpp"

namespace f9tws {

class f9tws_API ExgMdFmt9Handler : public ExgMdHandlerAnySeq {
   fon9_NON_COPY_NON_MOVE(ExgMdFmt9Handler);
   void OnPkReceived(const ExgMdHeader& pk, unsigned pksz) override;
public:
   using ExgMdHandlerAnySeq::ExgMdHandlerAnySeq;
   virtual ~ExgMdFmt9Handler();
};

} // namespaces
#endif//__f9tws_ExgMdFmt9Handler_hpp__
