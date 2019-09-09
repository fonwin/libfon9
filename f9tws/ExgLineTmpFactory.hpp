// \file f9tws/ExgLineTmpFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgLineTmpFactory_hpp__
#define __f9tws_ExgLineTmpFactory_hpp__
#include "f9tws/ExgLineFactory.hpp"

namespace f9tws {

class f9tws_API ExgLineTmpArgs;

class f9tws_API ExgLineTmpFactory : public ExgLineFactory {
   fon9_NON_COPY_NON_MOVE(ExgLineTmpFactory);
   using base = ExgLineFactory;
protected:
   fon9::io::SessionSP CreateTradingLine(ExgTradingLineMgr& lineMgr,
                                         const fon9::IoConfigItem& cfg,
                                         std::string& errReason) override;

   /// 由衍生者根據 args.ApCode_ 決定要建立哪種 Session.
   virtual fon9::io::SessionSP CreateLineTmp(ExgTradingLineMgr& lineMgr,
                                             const ExgLineTmpArgs& args,
                                             std::string& errReason,
                                             std::string logFileName) = 0;
public:
   ExgLineTmpFactory(std::string logPathFmt, Named&& name);
};

} // namespaces
#endif//__f9tws_ExgLineTmpFactory_hpp__
