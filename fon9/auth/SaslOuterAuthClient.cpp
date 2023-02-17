#include "fon9/auth/SaslOuterAuthClient.hpp"
#include "fon9/auth/OuterAuth.hpp"
#include "fon9/crypto/Sha1.hpp"
#include "fon9/crypto/Sha256.hpp"

namespace fon9 { namespace auth {

namespace {

class SaslOuterAuthClient : public SaslClient {
   using base = SaslClient;
   SaslOuterAuthClient() = delete;
   std::string Pass_{};
   std::string NewPass_{};
   std::string User_{};
public:
   using base::base;

   SaslOuterAuthClient(const StrView& authz, const StrView& authc, const StrView& pass)
      : User_(authc.ToString())
   {
      (void)authz;
      if (const char* pspl = pass.Find('\r')) {
         this->Pass_.assign(pass.begin(), pspl);
         this->NewPass_.assign(pspl + 1, pass.end());
      }
      else {
         this->Pass_ = pass.ToString();
      }

      this->ClientFirstMessage_ = "m=o";
   }

   SaslOuterAuthClient(const StrView& authz, const StrView& authc, const StrView& pass, const StrView& newPass)
      :SaslOuterAuthClient(authz, authc, pass)
   {
      this->NewPass_ = newPass.ToString();
   }

   virtual AuthR OnChallenge(StrView message) override {
      if (message.size() > 2) {
         switch (*message.begin()) {
         case 'm':
            {
               if (this->NewPass_.size() > 0) {
                  return AuthR(fon9_Auth_EOther, "Password change is not supported");
               }

               size_t len;
               fon9::byte buffer[crypto::Sha256::kOutputSize];
               switch (static_cast<EPassMode>(*(message.begin() + 2))) {
               case EPassMode::EPassMode_sh1:
                  len = fon9::crypto::Sha1::kOutputSize;
                  fon9::crypto::Sha1::Hash(Pass_.c_str(), Pass_.size(), buffer);
                  break;
               case EPassMode::EPassMode_sha256:
                  len = fon9::crypto::Sha256::kOutputSize;
                  fon9::crypto::Sha256::Hash(Pass_.c_str(), Pass_.size(), buffer);
                  break;
               case EPassMode::EPassMode_Unset:
               default:
                  return AuthR(fon9_Auth_EOther, "unknown PassMode");
               }

               std::string nextMessage = "n=" + User_ + ",p=";
               fon9::StrView(reinterpret_cast<char*>(buffer), len).AppendTo(nextMessage);
               return AuthR(fon9_Auth_NeedsMore, std::move(nextMessage));
            }
         case 'v':
            return AuthR(fon9_Auth_Success);
         }
      }
      return AuthR(fon9_Auth_EOther, "invalid-step");
   }
};

}

fon9_API SaslClientSP SaslOuterAuthClientCreator(const StrView& authz, const StrView& authc, const StrView& pass) {
   return SaslClientSP{ new SaslOuterAuthClient{authz, authc, pass} };
}

fon9_API SaslClientSP SaslOuterAuthClientCreator_ChgPass(const StrView& authz, const StrView& authc, const StrView& oldPass, const StrView& newPass) {
   return SaslClientSP{ new SaslOuterAuthClient{authz, authc, oldPass, newPass} };
}


} } // namespaces
