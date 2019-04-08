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
auth::AuthMgrSP SessionFactoryConfigWithAuthMgr::GetAuthMgr() {
   if (auto authMgr = this->PluginsHolder_.Root_->Get<auth::AuthMgr>(this->AuthMgrName_))
      return authMgr;
   this->ErrMsg_ += "|err=Unknown AuthMgr";
   if (!this->AuthMgrName_.empty()) {
      this->ErrMsg_.push_back('=');
      this->AuthMgrName_.AppendTo(this->ErrMsg_);
   }
   return nullptr;
}

} // namespaces
