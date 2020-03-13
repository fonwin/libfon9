// \file fon9/rc/RcFuncConn.hpp
//
// 提供 f9rc 的 Connection, SASL 功能
//
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcFuncConn_hpp__
#define __fon9_rc_RcFuncConn_hpp__
#include "fon9/rc/Rc.hpp"

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

} } // namespaces
#endif//__fon9_rc_RcFuncConn_hpp__
