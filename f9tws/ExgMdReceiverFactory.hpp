// \file f9tws/ExgMdReceiverFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMdReceiverFactory_hpp__
#define __f9tws_ExgMdReceiverFactory_hpp__
#include "f9tws/ExgMdSystem.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace f9tws {

class f9tws_API ExgMdReceiverFactory : public fon9::SessionFactory {
   fon9_NON_COPY_NON_MOVE(ExgMdReceiverFactory);
   using base = fon9::SessionFactory;
public:
   const ExgMdSystemSP  MdDispatcher_;

   template <class... ArgsT>
   ExgMdReceiverFactory(ExgMdSystemSP mdDispatcher, ArgsT&&... args)
      : base(std::forward<ArgsT>(args)...)
      , MdDispatcher_(std::move(mdDispatcher)) {
   }

   fon9::io::SessionSP CreateSession(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
};

} // namespaces
#endif//__f9tws_ExgMdReceiverFactory_hpp__
