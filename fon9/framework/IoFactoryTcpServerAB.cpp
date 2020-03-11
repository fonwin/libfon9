// \file fon9/framework/IoFactoryTcpServerAB.cpp
//
// 支援 Ip 黑名單、白名單的 TcpServer.
//
// \author fonwinz@gmail.com
#include "fon9/framework/IoManager.hpp"
#include "fon9/auth/PolicySocketFrom.hpp"

#ifdef fon9_WINDOWS
#include "fon9/io/win/IocpTcpServer.hpp"
using fon9_TcpServer = fon9::io::IocpTcpServer;
#else
#include "fon9/io/FdrTcpServer.hpp"
using fon9_TcpServer = fon9::io::FdrTcpServer;
#endif

namespace fon9 {
struct TcpServerAB : public fon9_TcpServer {
   fon9_NON_COPY_NON_MOVE(TcpServerAB);
   const auth::PolicySocketFromAgentSP IpChecker_;

   template <class... ArgsT>
   TcpServerAB(auth::PolicySocketFromAgentSP ipChecker, ArgsT&&... args)
      : fon9_TcpServer(std::forward<ArgsT>(args)...)
      , IpChecker_{std::move(ipChecker)} {
   }
   bool OnBeforeAccept(const io::SocketAddress& addrRemote, io::SocketResult& soRes, CharVector& exlog) override {
      if (this->IpChecker_.get() == nullptr)
         return true;
      return this->IpChecker_->OnBeforeAccept(addrRemote, soRes, exlog);
   }
};
static bool TcpServerAB_Start(seed::PluginsHolder& holder, StrView args) {
   using PolicySocketFromAgentSP = auth::PolicySocketFromAgentSP;
   struct Factory : public DeviceFactory {
      fon9_NON_COPY_NON_MOVE(Factory);
      const PolicySocketFromAgentSP  IpChecker_;
      Factory(std::string name, PolicySocketFromAgentSP ipChecker)
         : DeviceFactory(name)
         , IpChecker_{std::move(ipChecker)} {
      }
      io::DeviceSP CreateDevice(IoManagerSP mgr, SessionFactory& sesFactory, const IoConfigItem& cfg, std::string& errReason) override {
         if (auto ses = sesFactory.CreateSessionServer(*mgr, cfg, errReason))
            return new TcpServerAB(this->IpChecker_, mgr->GetIoService(), std::move(ses), mgr);
         return io::DeviceSP{};
      }
   };
   struct ArgsParser : public DeviceFactoryConfigParser {
      using base = DeviceFactoryConfigParser;
      using base::base;
      PolicySocketFromAgentSP  IpChecker_;
      DeviceFactorySP CreateDeviceFactory() override {
         return new Factory(this->Name_, this->IpChecker_);
      }
      bool OnUnknownTag(seed::PluginsHolder& holder, StrView tag, StrView value) override {
         if (tag == "IpChecker") { // Ip檢查. value = "MaAuth/IpCheckerServiceName"
            if (value.Get1st() == '/')
               value.SetBegin(value.begin() + 1);
            if (auto authMgr = holder.Root_->Get<auth::AuthMgr>(StrFetchTrim(value, '/'))) {
               this->IpChecker_ = authMgr->Agents_->Get<auth::PolicySocketFromAgent>(value);
               if (this->IpChecker_.get() != nullptr)
                  return true;
               this->ErrMsg_.append("|err=Not found Checker: '");
            }
            else
               this->ErrMsg_.append("|err=Not found AuthMgr: '");
            this->ErrMsg_.append(tag.begin(), value.end());
            this->ErrMsg_.push_back('\'');
            return true;
         }
         return base::OnUnknownTag(holder, tag, value);
      }
   };
   return ArgsParser{"TcpServerAB"}.Parse(holder, args);
}
} // namespace fon9

extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpServerAB;
static fon9::seed::PluginsPark f9pAutoPluginsReg{"TcpServerAB", &f9p_TcpServerAB};
fon9::seed::PluginsDesc f9p_TcpServerAB{"", &fon9::TcpServerAB_Start, nullptr, nullptr,};
