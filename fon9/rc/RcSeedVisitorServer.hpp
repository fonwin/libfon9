// \file fon9/rc/RcSeedVisitorServer.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_rc_RcSeedVisitorServer_hpp__
#define __fon9_rc_RcSeedVisitorServer_hpp__
#include "fon9/rc/Rc.hpp"
#include "fon9/seed/SeedVisitor.hpp"
#include "fon9/auth/AuthMgr.hpp"

namespace fon9 { namespace rc {

/// \ingroup rc
/// FunctionAgent: SeedVisitor Server.
class RcSeedVisitorServerAgent : public RcFunctionAgent {
   fon9_NON_COPY_NON_MOVE(RcSeedVisitorServerAgent);
   using base = RcFunctionAgent;
public:
   RcSeedVisitorServerAgent() : base{f9rc_FunctionCode_SeedVisitor} {
   }
   ~RcSeedVisitorServerAgent();

   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;
   void OnSessionLinkBroken(RcSession& ses) override;
};

class RcSeedVisitorServerNote : public RcFunctionNote {
   struct SeedVisitor;
   using SeedVisitorSP = intrusive_ptr<SeedVisitor>;
   SeedVisitorSP  Visitor_;
   FlowCounter    FcQry_;
public:
   RcSeedVisitorServerNote(RcSession& ses, const auth::AuthResult& authr, seed::AclConfig&& aclcfg);
   ~RcSeedVisitorServerNote();
   void OnRecvFunctionCall(RcSession& ses, RcFunctionParam& param) override;
   void OnSessionLinkBroken();
};

} } // namespaces
#endif//__fon9_rc_RcSeedVisitorServer_hpp__
