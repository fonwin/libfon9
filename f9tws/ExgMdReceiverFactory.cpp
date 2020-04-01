// \file f9tws/ExgMdReceiverFactory.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMdReceiverFactory.hpp"
#include "f9tws/ExgMdReceiverSession.hpp"

namespace f9tws {
using namespace fon9;

io::SessionSP ExgMdReceiverFactory::CreateSession(IoManager& ioMgr, const IoConfigItem& cfg, std::string& errReason) {
   (void)ioMgr;
   StrView  tag, value, args = ToStrView(cfg.SessionArgs_);
   StrView  pkLogName;
   while (fon9::StrFetchTagValue(args, tag, value)) {
      if (tag == "PkLog")
         pkLogName = value;
      else {
         errReason = "f9tws.ExgMdReceiverFactory.CreateSession: Unknown Tag:" + tag.ToString();
         return nullptr;
      }
   }
   return new ExgMdReceiverSession(this->MdDispatcher_, pkLogName);
}
io::SessionServerSP ExgMdReceiverFactory::CreateSessionServer(IoManager& ioMgr, const IoConfigItem& cfg, std::string& errReason) {
   (void)ioMgr; (void)cfg;
   errReason = "f9tws.ExgMdReceiverFactory: Not support server.";
   return nullptr;
}

} // namespaces
