// \file f9tws/ExgLineFactory.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgLineFactory.hpp"

namespace f9tws {

ExgLineFactory::ExgLineFactory(std::string logPathFmt, Named&& name)
   : baseFactory{std::move(name)}
   , basePathMaker{std::move(logPathFmt)} {
}
fon9::io::SessionServerSP ExgLineFactory::CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
   (void)mgr; (void)cfg;
   errReason = "f9tws::ExgLineFactory|err=Not support Server.";
   return fon9::io::SessionServerSP{};
}
fon9::io::SessionSP ExgLineFactory::CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
   if (ExgTradingLineMgr* twsLineMgr = dynamic_cast<ExgTradingLineMgr*>(&mgr))
      return this->CreateTradingLine(*twsLineMgr, cfg, errReason);
   errReason = "IoManager must be f9tws::ExgTradingLineMgr";
   return fon9::io::SessionSP{};
}

} // namespaces
