// \file fon9/rc/RcFuncConn.hpp
//
// 提供 f9rc 的 Connection, SASL 功能
//
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcFuncConn_hpp__
#define __fon9_rc_RcFuncConn_hpp__
#include "fon9/rc/Rc.hpp"
#include "fon9/auth/AuthMgr.hpp"

namespace fon9 { namespace rc {

/// \ingroup rc
/// FunctionAgent: Connection.
/// - Client: ProtocolVersion 溝通完畢後, 送出連線訊息: "ApVersion", "Description"
/// - Server: 收到 Client 的連線訊息後, 若同意連線, 則送出 Server 的 "ApVersion", "Description", "SASL mech list"
///   - SASL mech list 使用1個空白分隔.
class fon9_API RcFuncConnection : public RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcFuncConnection);
   using base = RcFunctionAgent;
public:
   const CharVector  ApVersion_;
   const CharVector  Description_;
   RcFuncConnection(RcSessionSt stReq, const StrView& apVer, const StrView& description)
      : base{f9rc_FunctionCode_Connection, stReq}
      , ApVersion_{apVer}
      , Description_{description} {
   }
   ~RcFuncConnection();
};

class fon9_API RcFuncConnClient : public RcFuncConnection {
   fon9_NON_COPY_NON_MOVE(RcFuncConnClient);
   using base = RcFuncConnection;
   /// 送出 ApVersion_ & Description_;
   void SendConnectionMessage(RcSession& ses);

public:
   RcFuncConnClient(const StrView& apVer, const StrView& description)
      : base{RcSessionSt::Connecting, apVer, description} {
   }

   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;

   /// 若 ses 為 RcSessionRole::ProtocolInitiator,
   /// 則在此處(送出 ProtocolVersion 之後)呼叫 SendConnectionMessage();
   void OnSessionLinkReady(RcSession& ses);
   /// 若 ses 為 RcSessionRole::ProtocolAcceptor,
   /// 則在此處(收到 ProtocolVersion 之後)呼叫 SendConnectionMessage();
   void OnSessionProtocolReady(RcSession& ses);
};

class fon9_API RcFuncSaslClient : public RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcFuncSaslClient);
   using base = RcFunctionAgent;
public:
   RcFuncSaslClient() : base{f9rc_FunctionCode_SASL, RcSessionSt::Logoning} {
   }
   void OnSessionConnected(RcSession& ses, StrView saslMechList);
   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;
};

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
   const auth::AuthMgrSP   AuthMgr_;
public:
   RcFuncSaslServer(auth::AuthMgrSP authMgr)
      : base{f9rc_FunctionCode_SASL, RcSessionSt::Connecting}
      , AuthMgr_(std::move(authMgr)) {
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
class fon9_API RcServerNote_SaslAuth : public RcFunctionNote {
   fon9_NON_COPY_NON_MOVE(RcServerNote_SaslAuth);
protected:
   RcSession&          RcSession_;
   auth::AuthRequest   AuthRequest_;
   auth::AuthSessionSP AuthSession_;
   friend class RcFuncSaslServer;
   void OnAuthVerifyCB(auth::AuthR rcode, auth::AuthSessionSP authSession);

   void VerifyRequest() {
      this->AuthSession_->AuthVerify(this->AuthRequest_);
   }
public:
   RcServerNote_SaslAuth(RcSession& ses, auth::AuthMgr& authMgr, StrView mechName, std::string&& firstMessage);
   ~RcServerNote_SaslAuth();

   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;

   const auth::AuthResult& GetAuthResult() const {
      return this->AuthSession_->GetAuthResult();
   }
};

} } // namespaces
#endif//__fon9_rc_RcFuncConn_hpp__
