// \file f9tws/ExgLineTmpFactory.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgLineTmpFactory.hpp"
#include "f9tws/ExgLineTmp.hpp"

namespace f9tws {

ExgLineTmpFactory::ExgLineTmpFactory(std::string logPathFmt, Named&& name)
   : base(std::move(logPathFmt), std::move(name)) {
}

fon9::io::SessionSP ExgLineTmpFactory::CreateTradingLine(ExgTradingLineMgr& lineMgr,
                                                                const fon9::IoConfigItem& cfg,
                                                                std::string& errReason) {
   ExgLineTmpArgs args;
   args.Market_ = lineMgr.Market_;
   errReason = ExgLineTmpArgsParser(args, ToStrView(cfg.SessionArgs_));
   if (!errReason.empty())
      return fon9::io::SessionSP{};

   std::string logFileName;
   if (args.IsNeedsLog_) {
      errReason = this->MakeLogPath(logFileName);
      if (!errReason.empty()) {
         errReason += " for TMP log.";
         return fon9::io::SessionSP{};
      }
      logFileName.append("TMP_");
      logFileName.append(args.BrkId_.begin(), args.BrkId_.size());
      logFileName.push_back('_');
      logFileName.append(args.SocketId_.begin(), args.SocketId_.size());
      logFileName.push_back('_');
      logFileName.push_back(static_cast<char>(args.ApCode_));
      logFileName.append(".log");
   }
   return this->CreateLineTmp(lineMgr, args, errReason, std::move(logFileName));
}

} // namespaces
