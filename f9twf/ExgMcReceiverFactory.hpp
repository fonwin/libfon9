// \file f9twf/ExgMcReceiverFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMcReceiverFactory_hpp__
#define __f9twf_ExgMcReceiverFactory_hpp__
#include "f9twf/Config.h"
#include "fon9/framework/IoFactory.hpp"

namespace f9twf {

class f9twf_API ExgMcReceiverFactory : public fon9::SessionFactory {
   fon9_NON_COPY_NON_MOVE(ExgMcReceiverFactory);
   using base = fon9::SessionFactory;
public:
   using base::base;

   fon9::io::SessionSP CreateSession(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
};

} // namespaces
#endif//__f9twf_ExgMcReceiverFactory_hpp__
