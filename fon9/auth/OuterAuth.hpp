#pragma once
#include "fon9/sys/Config.hpp"
#include "fon9/auth/UserMgr.hpp"
#include "fon9/io/Session.hpp"
#include <map>

namespace fon9 { namespace auth {

using ReqUID = uint64_t;

enum EPassMode {
   EPassMode_Unset = 0,
   EPassMode_sh1 = 1,
   EPassMode_sha256 = 2,
};

class OuterAuthChannel {
   fon9_NON_COPY_NON_MOVE(OuterAuthChannel);

protected:
   bool IsReady_;
   char Padding_[3];
public:
   EPassMode PassMode_{ EPassMode_Unset };

   OuterAuthChannel() = default;

   inline bool IsReady() const { return IsReady_; }

   virtual void SendVerify(const ReqUID& reqID, const fon9::StrView& user, const fon9::StrView& encryptionPassword, const fon9::StrView& ufrom) = 0;

   // 目前不支援改密碼.
   // virtual void SendChangePassword(const ReqUID& reqID, const fon9::StrView& user, const fon9::StrView& password, const fon9::StrView& newPassword, const fon9::StrView& ufrom) = 0;
};

class fon9_API OuterAuthMgr : public intrusive_ref_counter<OuterAuthMgr> {
   fon9_NON_COPY_NON_MOVE(OuterAuthMgr);

   struct BeenAuthData {
      AuthSessionSP AuthSession_;

      BeenAuthData(AuthSessionSP authSession)
         :AuthSession_(authSession) {
      }
   };
   using ChannelMap = std::map<void*, fon9::io::SessionSP>;
   char Padding_[4];
   ChannelMap Channels_;

   using AuthDataContainer = std::map<ReqUID, BeenAuthData>;
   AuthDataContainer AuthData_;

   ReqUID ReqUID_{ 0 };
   EPassMode PassMode_{ EPassMode_Unset };
   char Padding__[4];

protected:
   inline ReqUID GetReqUID() { return ++ReqUID_; }

public:
   OuterAuthMgr() = default;

   bool Verify(AuthSessionSP authSession, const fon9::StrView& user, const fon9::StrView& password, const fon9::StrView& ufrom);

   void OnVerifyResult(const ReqUID& reqID, const fon9::StrView& user, const fon9::StrView& policy, const fon9::StrView& message);

   void AddOuterAuthChannel(fon9::io::SessionSP session);
   void DelOuterAuthChannel(fon9::io::SessionSP session);

   EPassMode GetPassMode();
};

using OuterAuthMgrSP = intrusive_ptr<OuterAuthMgr>;

} } // // namespaces
