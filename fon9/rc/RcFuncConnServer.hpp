// \file fon9/rc/RcFuncConnServer.hpp
//
// 提供 f9rc Server 端 Connection, SASL 功能
//
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcFuncConnServer_hpp__
#define __fon9_rc_RcFuncConnServer_hpp__
#include "fon9/rc/RcFuncConn.hpp"
#include "fon9/auth/AuthMgr.hpp"
#include "fon9/auth/PoDupLogon.hpp"

namespace fon9 { namespace rc {

/// \ingroup rc
/// 收到對方的 Connection 要求後, 提供: ApVersion & Description & SASL mech list.
class fon9_API RcFuncConnServer : public RcFuncConnection {
   fon9_NON_COPY_NON_MOVE(RcFuncConnServer);
   using base = RcFuncConnection;
   const auth::AuthMgrSP   AuthMgr_;
public:
   /// 建構時提供 AuthMgr, Client 連線時, 透過 AuthMgr::GetStdSaslMechList(), 取得可用的登入機制列表.
   RcFuncConnServer(const StrView& apVer, const StrView& description, auth::AuthMgrSP authMgr)
      : base{RcSessionSt::ProtocolReady, apVer, description}
      , AuthMgr_(std::move(authMgr)) {
   }
   /// 檢查 SessionSt 狀態:
   /// - > RcSessionSt::Connecting: ses.ForceLogout().
   /// - >=RcSessionSt::ProtocolReady: 送出 ApVersion_ & Description_ & SaslMechList;
   void  OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;
};
/// \ingroup rc
/// 提供 SASL 登入驗證.
class fon9_API RcFuncSaslServer : public RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcFuncSaslServer);
   using base = RcFunctionAgent;
   const auth::AuthMgrSP         AuthMgr_;
   const auth::PoDupLogonAgentSP OnlineMgr_;
public:
   RcFuncSaslServer(auth::AuthMgrSP authMgr, auth::PoDupLogonAgentSP onlineMgr)
      : base{f9rc_FunctionCode_SASL, RcSessionSt::Connecting}
      , AuthMgr_{std::move(authMgr)}
      , OnlineMgr_{std::move(onlineMgr)} {
   }
   void OnSessionLinkBroken(RcSession& ses) override;
   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;
};

/// \ingroup rc
/// 提供 RcSession SASL 登入驗證處理.
/// 可以透過底下方式取得 Policy:
/// \code
/// if (fon9::rc::RcServerNote_SaslAuth* authNote = ses.GetNote<fon9::rc::RcServerNote_SaslAuth>(f9rc_FunctionCode_SASL)) {
///    const fon9::auth::AuthResult& authr = authNote->GetAuthResult();
///    // 透過 authmgr 取得需要的 policy
///    if (auto agent = authr.AuthMgr_->Agents_->Get<AgentType>("PolicyName"))
///       agent->GetPolicy(authr, policy);
/// }
/// \endcode
class fon9_API RcServerNote_SaslAuth : public RcFunctionNote, public auth::PoDupLogonClient {
   fon9_NON_COPY_NON_MOVE(RcServerNote_SaslAuth);
protected:
   unsigned PoDupLogonClient_AddRef() const override;
   unsigned PoDupLogonClient_Release() const override;
   void PoDupLogonClient_ForceLogout(StrView reason) override;

   RcSession&          RcSession_;
   auth::AuthRequest   AuthRequest_;
   auth::AuthSessionSP AuthSession_;
   const auth::PoDupLogonAgentSP OnlineMgr_;
   friend class RcFuncSaslServer;
   void OnAuthVerifyCB(auth::AuthR rcode, auth::AuthSessionSP authSession);

   void VerifyRequest() {
      this->AuthSession_->AuthVerify(this->AuthRequest_);
   }
public:
   RcServerNote_SaslAuth(RcSession& ses, auth::AuthMgr& authMgr, StrView mechName, std::string&& firstMessage,
                         auth::PoDupLogonAgentSP onlineMgr);
   ~RcServerNote_SaslAuth();

   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;

   const auth::AuthResult& GetAuthResult() const {
      return this->AuthSession_->GetAuthResult();
   }
};

} } // namespaces
#endif//__fon9_rc_RcFuncConnServer_hpp__
