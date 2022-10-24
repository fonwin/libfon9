/// \file fon9/PassKey.cpp
/// \author fonwinz@gmail.com
#include "fon9/PassKey.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 {

static bool defaultPassKeyToPassword(const StrView& passKey, CharVector& password) {
   password.assign(passKey);
   return false;
}
static FnPassKeyToPassword stFnPassKeyToPassword = defaultPassKeyToPassword;
//--------------------------------------------------------------------------//
fon9_API void SetFnPassKeyToPassword(FnPassKeyToPassword fnPassKeyToPassword) {
   stFnPassKeyToPassword = (fnPassKeyToPassword ? fnPassKeyToPassword : defaultPassKeyToPassword);
}

fon9_API bool PassKeyToPassword(const StrView& passKey, CharVector& password) {
   return stFnPassKeyToPassword(passKey, password);
}

fon9_API unsigned PassKeyToPassNum(const StrView& passKey, unsigned defaultPassNum) {
   CharVector password;
   if (PassKeyToPassword(passKey, password))
      return StrTo(ToStrView(password), defaultPassNum);
   return defaultPassNum;
}

} // namesapce fon9;
