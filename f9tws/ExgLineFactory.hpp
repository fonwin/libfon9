// \file f9tws/ExgLineFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgLineFactory_hpp__
#define __f9tws_ExgLineFactory_hpp__
#include "f9tws/ExgTradingLineMgr.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace f9tws {

class f9tws_API ExgLineFactory : public fon9::SessionFactory, public f9fmkt::TradingLineLogPathMaker {
   fon9_NON_COPY_NON_MOVE(ExgLineFactory);
   using baseFactory = fon9::SessionFactory;
   using basePathMaker = f9fmkt::TradingLineLogPathMaker;

protected:
   virtual fon9::io::SessionSP CreateTradingLine(ExgTradingLineMgr& lineMgr,
                                                 const fon9::IoConfigItem& cfg,
                                                 std::string& errReason) = 0;

public:
   ExgLineFactory(std::string logPathFmt, Named&& name);

   /// mgr 必須是 f9tws::ExgTradingLineMgr
   fon9::io::SessionSP CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
   /// 券商端, 不支援 SessionServer.
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
};

} // namespaces
#endif//__f9tws_ExgLineFactory_hpp__
