/// \file fon9/framework/SessionFactoryConfigWithAuthMgr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_SessionFactoryConfigWithAuthMgr_hpp__
#define __fon9_framework_SessionFactoryConfigWithAuthMgr_hpp__
#include "fon9/framework/IoFactory.hpp"
#include "fon9/auth/AuthMgr.hpp"

namespace fon9 {

fon9_API auth::AuthMgrSP SessionFactoryConfig_GetAuthMgr(
   IoFactoryConfigParser& parser,
   seed::MaTree&          root,
   const StrView&         authMgrName);

class fon9_API SessionFactoryConfigWithAuthMgr : public SessionFactoryConfigParser {
   fon9_NON_COPY_NON_MOVE(SessionFactoryConfigWithAuthMgr);
   using base = SessionFactoryConfigParser;

protected:
   seed::PluginsHolder& PluginsHolder_;
   StrView              AuthMgrName_;

   auth::AuthMgrSP GetAuthMgr() {
      return SessionFactoryConfig_GetAuthMgr(*this, *this->PluginsHolder_.Root_, this->AuthMgrName_);
   }

public:
   template <class... ArgsT>
   SessionFactoryConfigWithAuthMgr(seed::PluginsHolder& pluginsHolder, ArgsT&&... args)
      : base{std::forward<ArgsT>(args)...}
      , PluginsHolder_{pluginsHolder} {
   }
   bool OnUnknownTag(seed::PluginsHolder& holder, StrView tag, StrView value) override;
};

} // namespaces
#endif//__fon9_framework_SessionFactoryConfigWithAuthMgr_hpp__
