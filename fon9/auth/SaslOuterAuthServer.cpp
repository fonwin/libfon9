#include "fon9/auth/SaslOuterAuthServer.hpp"
#include "fon9/crypto/Sha1.hpp"
#include "fon9/crypto/Sha256.hpp"
#include <cstring>

namespace fon9 { namespace auth {

static const std::string kNoAuthchannelText = "no authentication channel";

const char* auth::AA_OUTER_AUTH::SeedName = "AA_OUTER_AUTH";

void AuthSession_OuterAuth::AuthVerify(const AuthRequest& req) {
   ++this->Step_;

   switch (this->Step_) {
   case 1:
   {
      EPassMode passMode = this->OuterAuthMgr_->GetPassMode();
      if (passMode == EPassMode::EPassMode_Unset) {
         this->OnVerifyCB_(AuthR(fon9_Auth_EOther, kNoAuthchannelText), this);
         return;
      }

      std::string msg = "m=";
      msg.push_back(static_cast<char>(this->OuterAuthMgr_->GetPassMode()));
      this->OnVerifyCB_(AuthR(fon9_Auth_NeedsMore, msg), this);
      break;
   }
   case 2:
   {
      StrView reqstr{ &req.Response_ };
      if (std::strncmp(reqstr.begin(), "n=", 2) != 0) {
         this->OnVerifyCB_(AuthR(fon9_Auth_EArgs_Format), this);
         return;
      }
      reqstr.SetBegin(reqstr.begin() + 2);
      StrView user(StrFetchNoTrim(reqstr, ','));
      this->AuthResult_.AuthcId_.assign(user);

      if (std::strncmp(reqstr.begin(), "p=", 2) != 0) {
         this->OnVerifyCB_(AuthR(fon9_Auth_EArgs_Format), this);
         return;
      }
      reqstr.SetBegin(reqstr.begin() + 2);

      switch (OuterAuthMgr_->GetPassMode()) {
      case EPassMode_Unset:
      default:
         this->OnVerifyCB_(AuthR(fon9_Auth_EOther, kNoAuthchannelText), this);
         return;
      case EPassMode_sh1:
         if (reqstr.size() != fon9::crypto::Sha1::kOutputSize) {
            this->OnVerifyCB_(AuthR(fon9_Auth_EArgs_Format), this);
            return;
         }
         break;
      case EPassMode_sha256:
         if( reqstr.size() != fon9::crypto::Sha256::kOutputSize ) {
            this->OnVerifyCB_(AuthR(fon9_Auth_EArgs_Format), this);
            return;
         }
         break;
      }

      OuterAuthMgr_->Verify(this, user, reqstr, &req.UserFrom_);
      break;
   }
   default:
      this->OnVerifyCB_(AuthR(fon9_Auth_EOther, "invalid-step"), this);
      break;
   }

   return;
}

void AuthSession_OuterAuth::OnVerifyFinish(const fon9::StrView& policy, const fon9::StrView& message) {
   if (policy.size() == 0 || message.size() > 0) {
      this->OnVerifyCB_(AuthR(fon9_Auth_EOther, message.ToString()), this);
      return;
   }

   this->AuthResult_.RoleId_ = RoleId(policy);
   this->OnVerifyCB_(AuthR(fon9_Auth_Success, "v=ok"), this);
}

fon9_API UserMgrSP PlantOuterAuth(AuthMgr& authMgr) {
   authMgr.Agents_->Plant<AA_OUTER_AUTH>("AA_OUTER_AUTH.Plant", auth::AA_OUTER_AUTH::SeedName);
   return nullptr;
}

} } // namespaces
