#include "fon9/auth/OuterAuth.hpp"
#include "fon9/auth/SaslOuterAuthServer.hpp"

namespace fon9 { namespace auth {

bool OuterAuthMgr::Verify(AuthSessionSP authSession, const fon9::StrView& user, const fon9::StrView& password, const fon9::StrView& ufrom) {
   for(auto it : Channels_) {
      OuterAuthChannel* channel = dynamic_cast<OuterAuthChannel*>(it.second.get());
      if (channel != nullptr && channel->IsReady()) {
         ReqUID uid = GetReqUID();
         AuthData_.insert(std::pair<ReqUID, BeenAuthData>( uid , BeenAuthData(authSession) ));
         channel->SendVerify(uid, user, password, ufrom);
         return true;
      }
   }
   return false;
}

EPassMode OuterAuthMgr::GetPassMode() {
   if (PassMode_ != EPassMode::EPassMode_Unset)
      return PassMode_;
   // 加密方式需要由外部驗證主機決定.
   for (auto it : Channels_) {
      /// 目前不考慮每條線路有獨立加密模式的情況.
      OuterAuthChannel* channel = dynamic_cast<OuterAuthChannel*>(it.second.get());
      if (channel != nullptr && channel->IsReady()) {
         if (channel->PassMode_ != EPassMode::EPassMode_Unset) {
            return PassMode_ = channel->PassMode_;
         }
      }
   }
   return PassMode_;
}

void OuterAuthMgr::OnVerifyResult(const ReqUID& reqID, const fon9::StrView& user, const fon9::StrView& policy, const fon9::StrView& message) {
   (void)user;
   AuthDataContainer::iterator it = AuthData_.find(reqID);
   if (it != AuthData_.end()) {
      AuthSession_OuterAuth* authSession = dynamic_cast<AuthSession_OuterAuth*> (it->second.AuthSession_.get());
      if (authSession != nullptr) {
         authSession->OnVerifyFinish(policy, message);
         AuthData_.erase(reqID);
         return;
      }
   }
   // 無法取得線路導致無法回覆訊息. 讓Client自行logon timeout.
}

void OuterAuthMgr::AddOuterAuthChannel(fon9::io::SessionSP session) {
   Channels_.insert({ session.get(), session });
}

void OuterAuthMgr::DelOuterAuthChannel(fon9::io::SessionSP session) {
   Channels_.erase(session.get());
   if (Channels_.size() == 0 ) {
      PassMode_ = EPassMode_Unset;
   }
}

} } // // namespaces
