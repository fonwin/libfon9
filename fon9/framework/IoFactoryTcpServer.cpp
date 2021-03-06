﻿/// \file fon9/framework/IoFactoryTcpServer.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/IoManager.hpp"

#ifdef fon9_WINDOWS
#include "fon9/io/win/IocpTcpServer.hpp"
using fon9_TcpServer = fon9::io::IocpTcpServer;
#else
#include "fon9/io/FdrTcpServer.hpp"
using fon9_TcpServer = fon9::io::FdrTcpServer;
#endif

namespace fon9 {

fon9_API DeviceFactorySP MakeIoFactoryTcpServer(std::string name) {
   struct Factory : public DeviceFactory {
      fon9_NON_COPY_NON_MOVE(Factory);
      Factory(std::string name) : DeviceFactory(name) {
      }
      io::DeviceSP CreateDevice(IoManagerSP mgr, SessionFactory& sesFactory, const IoConfigItem& cfg, std::string& errReason) override {
         if (auto ses = sesFactory.CreateSessionServer(*mgr, cfg, errReason))
            return new fon9_TcpServer(mgr->GetIoService(), std::move(ses), mgr);
         return io::DeviceSP{};
      }
   };
   return new Factory{name};
}
static bool TcpServer_Start(seed::PluginsHolder& holder, StrView args) {
   struct ArgsParser : public DeviceFactoryConfigParser {
      ArgsParser() : DeviceFactoryConfigParser{"TcpServer"} {}
      DeviceFactorySP CreateDeviceFactory() override {
         return MakeIoFactoryTcpServer(this->Name_);
      }
   };
   return ArgsParser{}.Parse(holder, args);
}

} // namespaces

extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpServer;
static fon9::seed::PluginsPark f9pAutoPluginsReg{"TcpServer", &f9p_TcpServer};

fon9::seed::PluginsDesc f9p_TcpServer{
   "",
   &fon9::TcpServer_Start,
   nullptr,
   nullptr,
};
