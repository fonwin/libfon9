// \file f9tws/ExgMdFmt23Handler.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdFmt23Handler_hpp__
#define __f9tws_ExgMdFmt23Handler_hpp__
#include "f9tws/ExgMdSystem.hpp"

namespace f9tws {

class f9tws_API ExgMdFmt23Handler : public ExgMdHandlerPkCont {
   fon9_NON_COPY_NON_MOVE(ExgMdFmt23Handler);
   void PkContOnReceived(const void* pk, unsigned pksz, SeqT seq) override;
public:
   using ExgMdHandlerPkCont::ExgMdHandlerPkCont;
   virtual ~ExgMdFmt23Handler();
};

} // namespaces
#endif//__f9tws_ExgMdFmt23Handler_hpp__
