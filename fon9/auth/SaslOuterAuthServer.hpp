#pragma once
#include "fon9/auth/OuterAuth.hpp"

namespace fon9 { namespace auth {

/// \ingroup auth
/// - 在 authMgr->Agents_ 加上 AA_OUTER_AUTH 的支援.
fon9_API UserMgrSP PlantOuterAuth(AuthMgr& authMgr);


class AuthSession_OuterAuth : public AuthSession {
   fon9_NON_COPY_NON_MOVE(AuthSession_OuterAuth);
   using base = AuthSession;

   const OuterAuthMgrSP OuterAuthMgr_;
   unsigned             Step_{ 0 };
   char                 Padding_[4];
public:

   AuthSession_OuterAuth(AuthMgrSP authMgr, FnOnAuthVerifyCB&& cb, OuterAuthMgrSP outerAuthMgr)
      : base(std::move(authMgr), std::move(cb))
      , OuterAuthMgr_(std::move(outerAuthMgr))
   {
   }

   virtual void AuthVerify(const AuthRequest& req) override;

   /// 當外部驗證完成，可調用此function繼續與client session聯繫.
   void OnVerifyFinish(const fon9::StrView& policy, const fon9::StrView& message);
};

class fon9_API AA_OUTER_AUTH : public AuthAgent {
   fon9_NON_COPY_NON_MOVE(AA_OUTER_AUTH);
   using base = AuthAgent;
public:

   static const char* SeedName;

   OuterAuthMgrSP OuterAuthMgr_;
   AA_OUTER_AUTH(const seed::MaTreeSP& /*maTree*/, std::string name)
      : base{ std::move(name) }
      , OuterAuthMgr_(new OuterAuthMgr()) {
   }
   AuthSessionSP CreateAuthSession(AuthMgrSP authMgr, FnOnAuthVerifyCB cb) override {
      return new AuthSession_OuterAuth(authMgr, std::move(cb), this->OuterAuthMgr_);
   }
};

} } // namespaces
