// \file f9twf/ExgLineTmpFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgLineTmpFactory_hpp__
#define __f9twf_ExgLineTmpFactory_hpp__
#include "f9twf/ExgTradingLineMgr.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace f9twf {

class f9twf_API ExgLineTmpFactory : public fon9::SessionFactory, public f9fmkt::TradingLineLogPathMaker {
   fon9_NON_COPY_NON_MOVE(ExgLineTmpFactory);
   using baseFactory = fon9::SessionFactory;
   using basePathMaker = f9fmkt::TradingLineLogPathMaker;

protected:
   /// 由衍生者根據 args.ApCode_ 決定要建立哪種 Session.
   virtual fon9::io::SessionSP CreateLineTmp(ExgIoManager&         ioMgr,
                                             const ExgLineTmpArgs& lineArgs,
                                             ExgLineTmpLog&&       log,
                                             std::string&          errReason) = 0;

public:
   ExgLineTmpFactory(std::string logPathFmt, Named&& name);

   /// mgr 必須是 f9twf::ExgIoManager
   fon9::io::SessionSP CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;

   /// 券商端, 不支援 SessionServer.
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
};

} // namespaces
#endif//__f9twf_ExgLineTmpFactory_hpp__
