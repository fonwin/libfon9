// \file fon9/rc/RcFuncConnServer.cpp
//
// 提供 f9rc 的 Connection & SASL 認證服務.
//
// \author fonwinz@gmail.com
#include "fon9/rc/RcFuncConn.hpp"
#include "fon9/rc/RcSession.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/framework/SessionFactoryConfigWithAuthMgr.hpp"

namespace fon9 { namespace rc {

void RcFuncConnServer::OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) {
   if (ses.GetSessionSt() > RcSessionSt::Connecting) {
      ses.ForceLogout("Dup FuncConnServer");
      return;
   }
   // 收到 Client 的 ApVersion, Description.
   CharVector apVer, description;
   BitvTo(param.RecvBuffer_, apVer);
   BitvTo(param.RecvBuffer_, description);
   ses.SetRemoteApVer(std::move(apVer));
   // 回覆 Server 的 ApVersion, Description, SaslMechList.
   RevBufferList  rbuf{64 + static_cast<BufferNodeSize>(this->ApVersion_.size() + this->Description_.size())};
   ToBitv(rbuf, this->AuthMgr_->GetSaslMechList());
   ToBitv(rbuf, this->Description_);
   ToBitv(rbuf, this->ApVersion_);
   ses.SendConnecting(std::move(rbuf));
}
//--------------------------------------------------------------------------//
RcServerNote_SaslAuth::RcServerNote_SaslAuth(RcSession& ses, auth::AuthMgr& authMgr, StrView mechName, std::string&& firstMessage)
   : RcSession_(ses) {
   this->AuthRequest_.UserFrom_ = ses.GetRemoteIp().ToString();
   this->AuthRequest_.Response_ = std::move(firstMessage);
   using namespace std::placeholders;
   this->AuthSession_ = authMgr.CreateAuthSession(mechName, std::bind(&RcServerNote_SaslAuth::OnAuthVerifyCB, this, _1, _2));
}
RcServerNote_SaslAuth::~RcServerNote_SaslAuth() {
}
void RcServerNote_SaslAuth::OnAuthVerifyCB(auth::AuthR rcode, auth::AuthSessionSP authSession) {
   RevBufferList  rbuf{static_cast<BufferNodeSize>(64 + rcode.Info_.size())};
   if (rcode.RCode_ == fon9_Auth_Success)
      ToBitv(rbuf, authSession->GetAuthResult().ExtInfo_);
   ToBitv(rbuf, rcode.Info_);
   ToBitv(rbuf, rcode.RCode_);
   this->RcSession_.SendSasl(std::move(rbuf));
   if (rcode.RCode_ == fon9_Auth_NeedsMore)
      return;
   if (rcode.RCode_ == fon9_Auth_Success) {
      rcode.Info_ = authSession->GetAuthResult().ExtInfo_;
      authSession->GetAuthResult().UpdateRoleConfig();
   }
   this->RcSession_.OnSaslDone(rcode, ToStrView(authSession->GetAuthResult().AuthcId_));
}
void RcServerNote_SaslAuth::OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) {
   if (ses.GetSessionSt() > RcSessionSt::Logoning)
      ses.ForceLogout("Dup FuncSaslServer.");
   else {
      BitvTo(param.RecvBuffer_, this->AuthRequest_.Response_);
      this->VerifyRequest();
   }
}
//--------------------------------------------------------------------------//
void RcFuncSaslServer::OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) {
   CharVector  mechName;
   std::string firstMessage;
   BitvTo(param.RecvBuffer_, mechName);
   BitvTo(param.RecvBuffer_, firstMessage);
   RcServerNote_SaslAuth*  note;
   RcFunctionNoteSP noteSP{note = new RcServerNote_SaslAuth{ses, *this->AuthMgr_, ToStrView(mechName), std::move(firstMessage)}};
   if (!note->AuthSession_)
      ses.ForceLogout("SASL mech not found.");
   else {
      ses.ResetNote(RcFunctionCode::SASL, std::move(noteSP));
      note->VerifyRequest();
   }
}
void RcFuncSaslServer::OnSessionLinkBroken(RcSession& ses) {
   ses.SetUserId(StrView{});
   base::OnSessionLinkBroken(ses);
}
//--------------------------------------------------------------------------//
struct RcSessionServer_Factory : public SessionFactory, public RcFunctionMgr {
   fon9_NON_COPY_NON_MOVE(RcSessionServer_Factory);
   auth::AuthMgrSP   AuthMgr_;
   CharVector        NameVer_;
   CharVector        Description_;

   RcSessionServer_Factory(std::string name, auth::AuthMgrSP&& authMgr, const CharVector& nameVer, const CharVector& desp)
      : SessionFactory(std::move(name))
      , AuthMgr_{authMgr}
      , NameVer_{nameVer}
      , Description_{desp} {
      this->Add(RcFunctionAgentSP{new RcFuncConnServer("f9rcServer.0", "fon9 RcServer", authMgr)});
      this->Add(RcFunctionAgentSP{new RcFuncSaslServer{authMgr}});
   }
   unsigned AddRef() override {
      return intrusive_ptr_add_ref(static_cast<SessionFactory*>(this));
   }
   unsigned Release() override {
      return intrusive_ptr_release(static_cast<SessionFactory*>(this));
   }

   io::SessionSP CreateSession(IoManager& iomgr, const IoConfigItem& cfg, std::string& errReason) override {
      (void)iomgr; (void)cfg; (void)errReason;
      return new RcSession(this, RcSessionRole::Host);
   }
   io::SessionServerSP CreateSessionServer(IoManager& iomgr, const IoConfigItem& cfg, std::string& errReason) override {
      (void)iomgr; (void)cfg; (void)errReason;
      return new RcSessionServer(this);
   }

   struct RcSessionServer : public io::SessionServer {
      fon9_NON_COPY_NON_MOVE(RcSessionServer);
      intrusive_ptr<RcSessionServer_Factory> Owner_;
      RcSessionServer(RcSessionServer_Factory* owner)
         : Owner_(owner) {
      }
      io::SessionSP OnDevice_Accepted(io::DeviceServer&) {
         return new RcSession(this->Owner_, RcSessionRole::Host);
      }
   };
};

} } // namespace

static bool RcSessionServer_Start(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
   class ArgsParser : public fon9::SessionFactoryConfigWithAuthMgr {
      fon9_NON_COPY_NON_MOVE(ArgsParser);
      using base = fon9::SessionFactoryConfigWithAuthMgr;
      fon9::CharVector  NameVer_{fon9::StrView{"f9rcServer.0"}};
      fon9::CharVector  Description_{fon9::StrView{"fon9 RcServer"}};
   public:
      using base::base;
      fon9::SessionFactorySP CreateSessionFactory() override {
         if (auto authMgr = this->GetAuthMgr())
            return new fon9::rc::RcSessionServer_Factory(this->Name_, std::move(authMgr), this->NameVer_, this->Description_);
         return nullptr;
      }
      bool OnUnknownTag(fon9::seed::PluginsHolder& holder, fon9::StrView tag, fon9::StrView value) override {
         if (tag == "Ver")
            this->NameVer_.assign(value);
         else if(tag == "Desp")
            this->Description_.assign(value);
         else
            return base::OnUnknownTag(holder, tag, value);
         return true;
      }
   };
   return ArgsParser{holder, "RcSessionServer"}.Parse(holder, args);
}

extern "C" fon9_API fon9::seed::PluginsDesc f9p_RcSessionServer;
static fon9::seed::PluginsPark f9pAutoPluginsReg{"RcSessionServer", &f9p_RcSessionServer};
fon9::seed::PluginsDesc f9p_RcSessionServer{"", &RcSessionServer_Start, nullptr, nullptr, };
