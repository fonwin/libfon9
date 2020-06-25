// \file f9twf/ExgMcChannelTunnel.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_ExgMcChannelTunnel_hpp__
#define __f9twf_ExgMcChannelTunnel_hpp__
#include "f9twf/ExgMcChannel.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace f9twf {

/// 台灣期交所逐筆行情「Channel訊息轉發程序」工廠.
class f9twf_API ExgMcChannelTunnelFactory : public fon9::SessionFactory {
   fon9_NON_COPY_NON_MOVE(ExgMcChannelTunnelFactory);
   using base = fon9::SessionFactory;
public:
   using base::base;
   fon9::io::SessionSP CreateSession(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& ioMgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
};

} // namespaces
#endif//__f9twf_ExgMcChannelTunnel_hpp__
