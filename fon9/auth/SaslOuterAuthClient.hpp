#pragma once
#include "fon9/auth/SaslClient.hpp"

namespace fon9 { namespace auth {

fon9_API SaslClientSP SaslOuterAuthClientCreator(const StrView& authz, const StrView& authc, const StrView& pass);
fon9_API SaslClientSP SaslOuterAuthClientCreator_ChgPass(const StrView& authz, const StrView& authc, const StrView& oldPass, const StrView& newPass);

} } // namespaces
