// \file f9tws/ExgLineFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgLineFactory_hpp__
#define __f9tws_ExgLineFactory_hpp__
#include "f9tws/ExgTradingLineMgr.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace f9tws {

class f9tws_API ExgLineFactory : public fon9::SessionFactory {
   fon9_NON_COPY_NON_MOVE(ExgLineFactory);
   using base = fon9::SessionFactory;
protected:
   const std::string LogPathFmt_;

   virtual fon9::io::SessionSP CreateTradingLine(ExgTradingLineMgr& lineMgr,
                                                 const fon9::IoConfigItem& cfg,
                                                 std::string& errReason) = 0;

   /// this->GetTDay() 無效時, 傳回失敗訊息;
   /// retval.empty() 表示結果存放在 res;
   /// 返回時 res 尾端會加上 '/';
   std::string MakeLogPath(std::string& res);

   /// 預設 = 本地時間 = UtcNow() + GetLocalTimeZoneOffset();
   virtual fon9::TimeStamp GetTDay();

public:
   ExgLineFactory(std::string logPathFmt, Named&& name);

   /// mgr 必須是 f9tws::ExgTradingLineMgr
   fon9::io::SessionSP CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
   /// 券商端, 不支援 SessionServer.
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
};

} // namespaces
#endif//__f9tws_ExgLineFactory_hpp__
