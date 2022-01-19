/// \file fon9/framework/SessionFactoryConfigWithAuthMgr.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/SessionFactoryConfigWithAuthMgr.hpp"

namespace fon9 {

bool SessionFactoryConfigWithAuthMgr::OnUnknownTag(seed::PluginsHolder& holder, StrView tag, StrView value) {
   if (tag == "AuthMgr") {
      this->AuthMgrName_ = value;
      return true;
   }
   return base::OnUnknownTag(holder, tag, value);
}
auth::AuthMgrSP SessionFactoryConfig_GetAuthMgr(IoFactoryConfigParser& parser,
                                                seed::MaTree&          root,
                                                const StrView&         authMgrName) {
   if (auto authMgr = root.Get<auth::AuthMgr>(authMgrName))
      return authMgr;
   parser.ErrMsg_ += "|err=Unknown AuthMgr";
   if (!authMgrName.empty()) {
      parser.ErrMsg_.push_back('=');
      authMgrName.AppendTo(parser.ErrMsg_);
   }
   return nullptr;
}

} // namespaces
